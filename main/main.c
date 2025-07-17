#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "esp_event.h"
#include "esp_sleep.h"

#include "sys/param.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mqtt_client.h"

#include "wifi_credentials.h"
#include "main_wifi.h"

#include "esp_http_server.h"	// HTTP server
#include "esp_littlefs.h" // LittleFS

#include <dirent.h>	// tempo


// support IDF 5.x
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#include "esp_camera.h"

#define BOARD_WROVER_KIT 1

// WROVER-KIT PIN Map
#ifdef BOARD_WROVER_KIT

#define TAG "main"

#define CAM_PIN_PWDN -1  //power down is not used
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 10
#define CAM_PIN_SIOD 40
#define CAM_PIN_SIOC 39

#define CAM_PIN_D7 48
#define CAM_PIN_D6 11
#define CAM_PIN_D5 12
#define CAM_PIN_D4 14
#define CAM_PIN_D3 16
#define CAM_PIN_D2 18
#define CAM_PIN_D1 17
#define CAM_PIN_D0 15
#define CAM_PIN_VSYNC 38
#define CAM_PIN_HREF 47
#define CAM_PIN_PCLK 13

#endif

// ESP32Cam (AiThinker) PIN Map
#ifdef BOARD_ESP32CAM_AITHINKER

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

#endif
// ESP32S3 (WROOM) PIN Map
#ifdef BOARD_ESP32S3_WROOM
#define CAM_PIN_PWDN 38
#define CAM_PIN_RESET -1   //software reset will be performed
#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF 7
#define CAM_PIN_PCLK 13
#define CAM_PIN_XCLK 15
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5
#define CAM_PIN_D0 11
#define CAM_PIN_D1 9
#define CAM_PIN_D2 8
#define CAM_PIN_D3 10
#define CAM_PIN_D4 12
#define CAM_PIN_D5 18
#define CAM_PIN_D6 17
#define CAM_PIN_D7 16
#endif
// ESP32S3 (GOOUU TECH)
#ifdef BOARD_ESP32S3_GOOUUU
#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1   //software reset will be performed
#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF 7
#define CAM_PIN_PCLK 13
#define CAM_PIN_XCLK 15
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5
#define CAM_PIN_D0 11
#define CAM_PIN_D1 9
#define CAM_PIN_D2 8
#define CAM_PIN_D3 10
#define CAM_PIN_D4 12
#define CAM_PIN_D5 18
#define CAM_PIN_D6 17
#define CAM_PIN_D7 16
#endif

#define MQTT_BROKER_URI "mqtt://broker.hivemq.com"
#define MQTT_BROKER_TOPIC "IeziPVyPNYnaOW4U/"

#define TAG "main"
#define MAX_MEASUREMENTS 100
#define NVS_NAMESPACE "measurements"
#define NVS_KEY_COUNTER "counter"

int64_t wifi_connect_times[MAX_MEASUREMENTS];
int64_t mqtt_connect_times[MAX_MEASUREMENTS];
int64_t capture_send_times[MAX_MEASUREMENTS];

int measurement_index = 0;

#if ESP_CAMERA_SUPPORTED
static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_QVGA,    //QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.

    .jpeg_quality = 12, //0-63, for OV series camera sensors, lower number means higher quality
    .fb_count = 1,       //When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

static esp_err_t init_camera(void)
{
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}
#endif

static void mqtt_event_handler_cb(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_publish(client, MQTT_BROKER_TOPIC, "Hello from ESP32S3!", 0, 0, 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

static esp_mqtt_client_handle_t mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI, // Public broker for testing
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler_cb, client);
    esp_mqtt_client_start(client);
    
    return client;
}

static void camera_loop(esp_mqtt_client_handle_t client)
{
#if ESP_CAMERA_SUPPORTED
    if(ESP_OK != init_camera()) {
        return;
    }

    while (1)
    {
        ESP_LOGI(TAG, "Taking picture...");
        camera_fb_t *pic = esp_camera_fb_get();

        esp_mqtt_client_publish(client, MQTT_BROKER_TOPIC, (const char *)pic->buf, pic->len, 0, 0);

        ESP_LOGI(TAG, "Picture taken! Its size was: %zu bytes", pic->len);
        esp_camera_fb_return(pic);

        vTaskDelay(5000 / portTICK_RATE_MS);
    }
#else
    ESP_LOGE(TAG, "Camera support is not available for this chip");
    return;
#endif
}

// Mount LittleFS
void mount_littlefs(void)
{
    const esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "littlefs",
        .format_if_mount_failed = true,
        .dont_mount = false
    };

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE("LITTLEFS", "Failed to mount LittleFS (%s)", esp_err_to_name(err));
        return;
    }

    size_t total = 0, used = 0;
    esp_littlefs_info("littlefs", &total, &used);
    ESP_LOGI("LITTLEFS", "Partition size: total=%d, used=%d", total, used);
}

// Display the content via HTTP
esp_err_t index_handler(httpd_req_t *req)
{
    FILE* f = fopen("/littlefs/index.html", "r");
    if (!f) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        httpd_resp_sendstr_chunk(req, line);
    }
    fclose(f);
    httpd_resp_sendstr_chunk(req, NULL); // End response
    return ESP_OK;
}


void app_main(void)
{
	int64_t wifi_start_time, wifi_end_time;

    ESP_LOGI(TAG, "Starting...");

	// test et Initialiser LittleFS
	esp_vfs_littlefs_conf_t conf = {
		.base_path = "/littlefs",
		.partition_label = "littlefs",
		.format_if_mount_failed = true,
		.dont_mount = false,
	};

	esp_err_t err = esp_vfs_littlefs_register(&conf);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to mount LittleFS (%s)", esp_err_to_name(err));
	} else {
		ESP_LOGI(TAG, "LittleFS mounted at /littlefs");
		// code tempo
		DIR* dir = opendir("/littlefs");
		if (dir == NULL) {
			ESP_LOGE(TAG, "Erreur : le dossier /littlefs n'existe pas");
		} else {
			ESP_LOGI(TAG, "Contenu de /littlefs :");
			struct dirent* ent;
			while ((ent = readdir(dir)) != NULL) {
				ESP_LOGI(TAG, "  - %s", ent->d_name);
			}
			closedir(dir);
		}
		// end code tempo

		// Exemple de lecture de fichier HTML
		FILE* f = fopen("/littlefs/index.html", "r");
		if (f == NULL) {
			ESP_LOGE(TAG, "Failed to open /littlefs/index.html");
		} else {
			char line[128];
			while (fgets(line, sizeof(line), f)) {
				ESP_LOGI(TAG, "%s", line);
			}
			fclose(f);
		}
	}

	// endtest
    
	wifi_start_time = esp_timer_get_time();

	ESP_LOGI(TAG, "WiFi connection established in : %lld milliseconds", wifi_start_time/1000);

    ESP_ERROR_CHECK(app_wifi_init());

    esp_err_t ret = app_wifi_connect(WIFI_SSID, WIFI_PASSWORD);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi network");
    }

	// Variante
	wifi_end_time = esp_timer_get_time();
    wifi_connect_times[measurement_index] = wifi_end_time - wifi_start_time;
	ESP_LOGI(TAG, "WiFi connected in %.2f ms", (float) wifi_connect_times[measurement_index]/1000);

    wifi_ap_record_t ap_info;
    ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_ERR_WIFI_CONN) {
        ESP_LOGE(TAG, "Wi-Fi station interface not initialized");
    }
    else if (ret == ESP_ERR_WIFI_NOT_CONNECT) {
        ESP_LOGE(TAG, "Wi-Fi station is not connected");
    } else {
        ESP_LOGI(TAG, "--- Access Point Information ---");
        ESP_LOG_BUFFER_HEX("MAC Address", ap_info.bssid, sizeof(ap_info.bssid));
        ESP_LOG_BUFFER_CHAR("SSID", ap_info.ssid, sizeof(ap_info.ssid));
        ESP_LOGI(TAG, "Primary Channel: %d", ap_info.primary);
        ESP_LOGI(TAG, "RSSI: %d", ap_info.rssi);

        camera_loop(mqtt_app_start());
    }

    ESP_ERROR_CHECK(app_wifi_disconnect());

    ESP_ERROR_CHECK(app_wifi_deinit());

    ESP_LOGI(TAG, "End of program");
}