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
#include "esp_http_server.h"  // HTTP server
#include "esp_littlefs.h"     // LittleFS
#include <dirent.h>           // Directory listing

#include "wifi/ap.h" // Wi-fi Access Point code
#include "wifi/sta.h" // Wi-fi Station code
#include "mqtt/mqtt.h" // MQTT client code

#include "route/config.h"
#include "route/index.h"

// Support for IDF 5.x
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#include "esp_camera.h"

// Board selection
#define BOARD_WROVER_KIT 1

// Logging tag
#define TAG "main"

// Camera configuration
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

// Camera config
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
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}
#endif

httpd_handle_t server = NULL;

static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    ESP_LOGI(TAG, "Starting HTTP server on port 80");

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &(httpd_uri_t){
            .uri = "/",
            .method = HTTP_GET,
            .handler = route_index_handler,
            .user_ctx = NULL
        });
        httpd_register_uri_handler(server, &(httpd_uri_t){
            .uri = "/config",
            .method = HTTP_POST,
            .handler = route_config_handler,
            .user_ctx = NULL
        });
        ESP_LOGI(TAG, "HTTP server started");
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
}

static void main_camera_loop(esp_mqtt_client_handle_t client)
{
    start_webserver();
#if ESP_CAMERA_SUPPORTED
    if(ESP_OK != init_camera()) {
        return;
    }

    while (1)
    {
        ESP_LOGI(TAG, "Taking picture...");
        camera_fb_t *pic = esp_camera_fb_get();

        // esp_mqtt_client_publish(client, MQTT_BROKER_TOPIC, (const char *)pic->buf, pic->len, 0, 0);

        ESP_LOGI(TAG, "Picture taken! Its size was: %zu bytes", pic->len);
        esp_camera_fb_return(pic);

        vTaskDelay(5000 / portTICK_RATE_MS);
    }
#else
    ESP_LOGE(TAG, "Camera support is not available for this chip");
    return;
#endif
}

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
        ESP_LOGE(TAG, "Failed to mount LittleFS (%s)", esp_err_to_name(err));
        return;
    }
    size_t total = 0, used = 0;
    esp_littlefs_info("littlefs", &total, &used);
    ESP_LOGI(TAG, "LittleFS: total=%d, used=%d", total, used);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting...");

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);


    // Initialize LittleFS
    mount_littlefs();
    DIR* dir = opendir("/littlefs");
    if (dir != NULL) {
        ESP_LOGI(TAG, "Contents of /littlefs:");
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL) {
            ESP_LOGI(TAG, "  - %s", ent->d_name);
        }
        closedir(dir);
    }

    // Initialize Event Loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());


    // --- Dual AP+STA Mode Initialization ---
    ESP_LOGI(TAG, "Initializing Wi-Fi in AP+STA mode...");
    app_wifi_init();

    // Initialize AP interface
    if (app_wifi_ap_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize AP Wi-Fi");
        return;
    }

    // Check for Wi-Fi configuration
    ESP_LOGI(TAG, "Checking Wi-Fi configuration...");
    char* ssid = app_config_get_ssid();
    char* password = app_config_get_password();

    // Initialize STA interface if credentials are present
    if (ssid && password) {
        err = app_wifi_sta_init();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize STA Wi-Fi");
            return;
        }

        ESP_LOGI(TAG, "Connecting to configured Wi-Fi SSID: %s", ssid);
        err = app_wifi_sta_connect(ssid, password);

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to connect to STA Wi-Fi");
            ESP_LOGE(TAG, "Error: %s", esp_err_to_name(err));

            app_config_reset();
            ESP_LOGI(TAG, "Configuration reset due to connection failure, restarting device...");
            vTaskDelay(pdMS_TO_TICKS(2000)); // Delay for 2 seconds before restart
            esp_restart();
            return;
        }

        wifi_ap_record_t ap_info;
        err = esp_wifi_sta_get_ap_info(&ap_info);
        if (err == ESP_ERR_WIFI_CONN) {
            ESP_LOGE(TAG, "Wi-Fi station interface not initialized");
        }
        else if (err == ESP_ERR_WIFI_NOT_CONNECT) {
            ESP_LOGE(TAG, "Wi-Fi station is not connected");
        } else {
            ESP_LOGI(TAG, "--- Access Point Information ---");
            ESP_LOG_BUFFER_HEX("MAC Address", ap_info.bssid, sizeof(ap_info.bssid));
            ESP_LOG_BUFFER_CHAR("SSID", ap_info.ssid, sizeof(ap_info.ssid));
            ESP_LOGI(TAG, "Primary Channel: %d", ap_info.primary);
            ESP_LOGI(TAG, "RSSI: %d", ap_info.rssi);

            // Start MQTT client
            esp_mqtt_client_handle_t mqtt_client = app_mqtt_start();
            if (mqtt_client == NULL) {
                ESP_LOGE(TAG, "Failed to start MQTT client");
            } else {
                // Start main camera loop only if AP, STA & MQTT are OK
                main_camera_loop(mqtt_client);
                return;
            }
        }
    } else {
        ESP_LOGI(TAG, "No Wi-Fi configuration found, starting in AP only mode");
    }

    // Start web server
    start_webserver();
}