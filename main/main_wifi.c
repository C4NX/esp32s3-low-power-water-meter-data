#include "main_wifi.h"

#include <inttypes.h>
#include <string.h>

#include "freertos/event_groups.h"

#define TAG "wifi"

#define WIFI_AUTHMODE WIFI_AUTH_WPA2_PSK

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const int WIFI_RETRY_ATTEMPT = 3;
static int wifi_retry_count = 0;

static esp_netif_t *wifi_netif = NULL;
static esp_event_handler_instance_t ip_event_handler;
static esp_event_handler_instance_t wifi_event_handler;

static EventGroupHandle_t s_wifi_event_group = NULL;

static void ip_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "Handling IP event, event code 0x%" PRIx32, event_id);
    switch (event_id)
    {
    case (IP_EVENT_STA_GOT_IP):
        ip_event_got_ip_t *event_ip = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event_ip->ip_info.ip));
        wifi_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    case (IP_EVENT_STA_LOST_IP):
        ESP_LOGI(TAG, "Lost IP");
        break;
    case (IP_EVENT_GOT_IP6):
        ip_event_got_ip6_t *event_ip6 = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6: " IPV6STR, IPV62STR(event_ip6->ip6_info.ip));
        wifi_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    default:
        ESP_LOGI(TAG, "IP event not handled");
        break;
    }
}

static void wifi_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "Handling Wi-Fi event, event code 0x%" PRIx32, event_id);

    switch (event_id)
    {
    case (WIFI_EVENT_WIFI_READY):
        ESP_LOGI(TAG, "Wi-Fi ready");
        break;
    case (WIFI_EVENT_SCAN_DONE):
        ESP_LOGI(TAG, "Wi-Fi scan done");
        break;
    case (WIFI_EVENT_STA_START):
        ESP_LOGI(TAG, "Wi-Fi started, connecting to AP...");
        esp_wifi_connect();
        break;
    case (WIFI_EVENT_STA_STOP):
        ESP_LOGI(TAG, "Wi-Fi stopped");
        break;
    case (WIFI_EVENT_STA_CONNECTED):
        ESP_LOGI(TAG, "Wi-Fi connected");
        break;
    case (WIFI_EVENT_STA_DISCONNECTED):
        ESP_LOGI(TAG, "Wi-Fi disconnected");
        if (wifi_retry_count < WIFI_RETRY_ATTEMPT) {
            ESP_LOGI(TAG, "Retrying to connect to Wi-Fi network...");
            esp_wifi_connect();
            wifi_retry_count++;
        } else {
            ESP_LOGI(TAG, "Failed to connect to Wi-Fi network");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        break;
    case (WIFI_EVENT_STA_AUTHMODE_CHANGE):
        ESP_LOGI(TAG, "Wi-Fi authmode changed");
        break;
    default:
        ESP_LOGI(TAG, "Wi-Fi event not handled");
        break;
    }
}

esp_err_t app_wifi_init(void)
{
    // Initialiser NVS (stockage non volatile)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    // Créer le groupe d'événements pour suivre la connexion Wi-Fi
    s_wifi_event_group = xEventGroupCreate();

    // Initialisation de la pile TCP/IP et de la boucle d’événements
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Créer les interfaces AP et STA
    esp_netif_t* ap_netif = esp_netif_create_default_wifi_ap();
    wifi_netif = esp_netif_create_default_wifi_sta(); // Pour la STA

    if (!ap_netif || !wifi_netif) {
        ESP_LOGE(TAG, "Failed to create Wi-Fi netifs");
        return ESP_FAIL;
    }

    // Configuration du Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Enregistrer les gestionnaires d'événements
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_cb,
                                                        NULL,
                                                        &wifi_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &ip_event_cb,
                                                        NULL,
                                                        &ip_event_handler));

    // Configurer le point d'accès
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "WaterMeter-Setup",
            .ssid_len = strlen("WaterMeter-Setup"),
            .password = "",
            .max_connection = 2,
            .authmode = WIFI_AUTH_OPEN
        }
    };

    // Activer le mode AP + STA
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    // Démarrer le Wi-Fi (le mode STA sera configuré plus tard dans app_wifi_connect)
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

esp_err_t app_wifi_connect(char* wifi_ssid, char* wifi_password)
{
    wifi_config_t wifi_config = {
        .sta = {
            // this sets the weakest authmode accepted in fast scan mode (default)
            .threshold.authmode = WIFI_AUTHMODE,
        },
    };

    strncpy((char*)wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, wifi_password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE)); // default is WIFI_PS_MIN_MODEM
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM)); // default is WIFI_STORAGE_FLASH

    //ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // TODO : remove
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    ESP_LOGI(TAG, "Connecting to Wi-Fi network: %s", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to Wi-Fi network: %s", wifi_config.sta.ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi network: %s", wifi_config.sta.ssid);
        return ESP_FAIL;
    }

    ESP_LOGE(TAG, "Unexpected Wi-Fi error");
    return ESP_FAIL;
}

esp_err_t app_wifi_disconnect(void)
{
    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
    }

    return esp_wifi_disconnect();
}

esp_err_t app_wifi_deinit(void)
{
    esp_err_t ret = esp_wifi_stop();
    if (ret == ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGE(TAG, "Wi-Fi stack not initialized");
        return ret;
    }

    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(wifi_netif));
    esp_netif_destroy(wifi_netif);

    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler));

    return ESP_OK;
}
