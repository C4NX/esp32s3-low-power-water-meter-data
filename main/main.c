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
#include "esp_task_wdt.h"
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
#include "dl/dl_wrapper.h" // ESP-DL (C++) wrapper code
#include "config/config.h" // For app_config_get_mqtt

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

// Camera configuration (moved back to global scope)
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
    .xclk_freq_hz = 5000000, // lower XCLK to reduce capture rate while inference is long
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_QQVGA,
    .jpeg_quality = 12,
    .fb_count = 2,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_LATEST,
};

static esp_err_t init_camera(void) {
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
    }
    return err;
}
#endif // ESP_CAMERA_SUPPORTED

httpd_handle_t server = NULL;

// Start web server on port 80
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
        ESP_LOGE(TAG, "Failed to mount LittleFS (%s)", esp_err_to_name(err));
        return;
    }
    size_t total = 0, used = 0;
    esp_littlefs_info("littlefs", &total, &used);
    ESP_LOGI(TAG, "LittleFS: total=%d, used=%d", total, used);
}

void camera_detect_task(void *arg) {
    esp_mqtt_client_handle_t mqtt_client = (esp_mqtt_client_handle_t)arg;
    ESP_LOGI(TAG, "Camera detect task starting");
#if ESP_CAMERA_SUPPORTED
    static bool camera_initialized = false;
    if (!camera_initialized) {
        if (ESP_OK != init_camera()) {
            ESP_LOGE(TAG, "Camera init failed in task");
            vTaskDelete(NULL);
            return;
        }
        camera_initialized = true;
    }
    // Register this task with WDT
    esp_task_wdt_add(NULL);
    detection_result_t results[8];

    // MQTT publish topic
    static char *publish_topic = NULL; // user requested to use app_config_get_mqtt as topic
    if (mqtt_client && !publish_topic) {
        publish_topic = app_config_get_mqtt(); // may return NULL if not configured
        if (publish_topic) {
            ESP_LOGI(TAG, "MQTT publish topic: %s", publish_topic);
        }
    }

    // Camera capture loop
    while (1) {
        esp_task_wdt_reset();
        camera_fb_t *pic = esp_camera_fb_get();
        if (!pic) {
            ESP_LOGW(TAG, "No frame (camera_fb_get returned NULL)");
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        int64_t t0 = esp_timer_get_time();
        int det = app_run_digit_detection(pic->buf, pic->len, results, 8);
        int64_t t1 = esp_timer_get_time();
        if (det >= 0) {
            ESP_LOGI(TAG, "Detection done: %d objects in %.2f ms", det, (t1 - t0)/1000.0);
        } else {
            ESP_LOGW(TAG, "Detection failed on frame");
        }

        // Publish if MQTT client and topic are available
        if(mqtt_client && publish_topic) {
            // JPEG Buffer
            int msg_id = esp_mqtt_client_publish(mqtt_client, publish_topic, (const char*)pic->buf, (int)pic->len, 0, 0);
            if (msg_id == -1) {
                ESP_LOGW(TAG, "MQTT publish failed");
            } else {
                ESP_LOGI(TAG, "MQTT published frame len=%u msg_id=%d", (unsigned)pic->len, msg_id);
            }

            // Detections as JSON
            char payload[256];
            int offset = 0;
            if(det > 0) {
                offset += snprintf(payload + offset, sizeof(payload) - offset, "{ \"detections\": [");
                for (int i = 0; i < det && i < 8; i++) {
                    if (i > 0) {
                        offset += snprintf(payload + offset, sizeof(payload) - offset, ", ");
                    }
                    offset += snprintf(payload + offset, sizeof(payload) - offset,
                                       "{ \"category\": %d, \"score\": %.2f, \"box\": [%d, %d, %d, %d] }",
                                       results[i].category, results[i].score,
                                       results[i].x1, results[i].y1, results[i].x2, results[i].y2);
                }
                offset += snprintf(payload + offset, sizeof(payload) - offset, "] }");
                int msg_id = esp_mqtt_client_publish(mqtt_client, publish_topic, payload, offset, 0, 0);
                if (msg_id == -1) {
                    ESP_LOGW(TAG, "MQTT publish of detections failed");
                } else {
                    ESP_LOGI(TAG, "MQTT published detections msg_id=%d", msg_id);
                }
            }else { // Send empty detections
                offset += snprintf(payload + offset, sizeof(payload) - offset, "{ \"detections\": [] }");
                int msg_id = esp_mqtt_client_publish(mqtt_client, publish_topic, payload, offset, 0, 0);
                if (msg_id == -1) {
                    ESP_LOGW(TAG, "MQTT publish of empty detections failed");
                } else {
                    ESP_LOGI(TAG, "MQTT published empty detections msg_id=%d", msg_id);
                }
            }
        }
        
        esp_camera_fb_return(pic); // release frame
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
#else
    ESP_LOGE(TAG, "Camera not supported on this build");
#endif
    vTaskDelete(NULL);
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting...");
    
    // NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Watchdog
    ESP_ERROR_CHECK(esp_task_wdt_deinit());
    esp_task_wdt_config_t wdt_config = { .timeout_ms = 60000, .trigger_panic = false };
    ESP_ERROR_CHECK(esp_task_wdt_init(&wdt_config));

    // Filesystem
    mount_littlefs();

    // Event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Wi-Fi (AP + attempt STA)
    app_wifi_init();
    if (app_wifi_ap_init() != ESP_OK) {
        ESP_LOGE(TAG, "AP init failed, unable to start");
        return;
    }
    char *ssid = app_config_get_ssid();
    char *password = app_config_get_password();
    esp_mqtt_client_handle_t mqtt_client = NULL;
    if (ssid && password) {
        if (app_wifi_sta_init() == ESP_OK) {
            if (app_wifi_sta_connect(ssid, password) == ESP_OK) {
                mqtt_client = app_mqtt_start();
                xTaskCreatePinnedToCore(camera_detect_task, "cam_detect", 12288, mqtt_client, 4, NULL, 0);
            } else {
                ESP_LOGW(TAG, "STA connect failed");
            }
        }
    }

    if (ssid) free(ssid);
    if (password) free(password);

    // Start web server, even if no Wi-Fi
    start_webserver();
}