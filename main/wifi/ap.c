#define TAG "wifi/ap"

#define AP_DEFAULT_SSID "WaterMeterEsp32"
#define AP_DEFAULT_PASSWORD "WaterMeterPassword"

#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_mac.h"

static void wifi_ap_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        uint8_t *mac = event->mac;
        ESP_LOGI(TAG, "Station %02x:%02x:%02x:%02x:%02x:%02x joined, AID=%d",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        uint8_t *mac = event->mac;
        ESP_LOGI(TAG, "Station %02x:%02x:%02x:%02x:%02x:%02x left, AID=%d",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], event->aid);
    }
}

// Initialize Wi-Fi in AP+STA mode
void app_wifi_init(void) {
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Initialize Wi-Fi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Set Wi-Fi mode to AP+STA
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
}

// Initialize Wi-Fi in AP mode
esp_err_t app_wifi_ap_init(void)
{
    // Create default Wi-Fi AP interface
    esp_netif_create_default_wifi_ap();

    // Register Wi-Fi event handler
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_ap_event_handler, NULL));

    // Configure AP
    wifi_config_t ap_config = {
        .ap = {
            .ssid = AP_DEFAULT_SSID,
            .ssid_len = strlen(AP_DEFAULT_SSID),
            .password = AP_DEFAULT_PASSWORD,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    // Set Wi-Fi mode to AP
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi AP started. SSID: %s", AP_DEFAULT_SSID);
    return ESP_OK;
}

// Deinitialize Wi-Fi (AP mode)
void app_wifi_ap_deinit(void)
{
    esp_err_t err;

    // Stop Wi-Fi
    err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGE(TAG, "Failed to stop Wi-Fi: %s", esp_err_to_name(err));
    }

    // Unregister Wi-Fi event handler
    err = esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_ap_event_handler);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to unregister Wi-Fi event handler: %s", esp_err_to_name(err));
    }

    // Deinitialize Wi-Fi driver
    err = esp_wifi_deinit();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinit Wi-Fi: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Wi-Fi fully deinitialized");
}