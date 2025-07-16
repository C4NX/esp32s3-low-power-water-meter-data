#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "esp_event.h"
#include "esp_sleep.h"
#include "nvs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mqtt_client.h"

#include "wifi_credentials.h"
#include "main_wifi.h"

// Support IDF 5.x
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#include "esp_camera.h"

#define BOARD_WROVER_KIT 1

// WROVER-KIT PIN Map
#ifdef BOARD_WROVER_KIT

#define CAM_PIN_PWDN -1  // power down is not used
#define CAM_PIN_RESET -1 // software reset will be performed
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

#define MQTT_BROKER_URI "mqtt://broker.hivemq.com"
// #define MQTT_BROKER_TOPIC_TIME "IeziPVyPNYnaOW4U/time"
#define MQTT_BROKER_TOPIC_IMAGE "IeziPVyPNYnaOW4U/image"
#define MQTT_BROKER_TOPIC_AVERAGE "IeziPVyPNYnaOW4U/average"

#define TAG "main"
#define TIMING_SAMPLES 100

typedef struct {
    int64_t wifi_time;
    int64_t mqtt_connect_time;
    int64_t mqtt_publish_time;
    int64_t capture_time;
} timing_data_t;

static int64_t mqtt_connect_start_time = 0;
static int64_t mqtt_connect_end_time = 0;
static int32_t sample_count = 0;

// Function declarations
void save_timing_to_nvs(const timing_data_t *data, int index);
void load_timing_from_nvs(timing_data_t *data, int index);
void reset_nvs_samples(void);

#if ESP_CAMERA_SUPPORTED
static esp_err_t init_camera(void)
{
    camera_config_t config = {
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

        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_QVGA,
        .jpeg_quality = 12,
        .fb_count = 1,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
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

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            mqtt_connect_end_time = esp_timer_get_time();
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
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
        .broker.address.uri = MQTT_BROKER_URI,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler_cb, NULL);
    esp_mqtt_client_start(client);
    return client;
}

void app_main(void)
{
    int64_t wifi_start_time = 0, wifi_end_time = 0;
    int64_t mqtt_start_time = 0, mqtt_end_time = 0;
    int64_t capture_start_time = 0, capture_end_time = 0;
    esp_mqtt_client_handle_t client = NULL;

    bool sent_successfully = false;
    timing_data_t current_timing = {0};

    ESP_LOGI(TAG, "Starting...");

    // Initialize NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    // Load current sample count from NVS
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open("timing", NVS_READWRITE, &nvs));
    
    esp_err_t err = nvs_open("timing", NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        err = nvs_get_i32(nvs, "sample_count", &sample_count);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "sample_count not found in NVS, initializing to 0");
            sample_count = 0;
        }
        nvs_close(nvs);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS handle, initializing sample_count to 0");
        sample_count = 0;
    }

    nvs_close(nvs);

    if (sample_count >= TIMING_SAMPLES) {
        ESP_LOGI(TAG, "Already collected 100 samples. Resetting...");
        reset_nvs_samples();
        sample_count = 0;
    }

    // Initialize Wi-Fi
    ESP_ERROR_CHECK(app_wifi_init());

    wifi_start_time = esp_timer_get_time();

    // Try connecting to Wi-Fi
    if (app_wifi_connect(WIFI_SSID, WIFI_PASSWORD) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi.");
        goto cleanup;
    }

    wifi_end_time = esp_timer_get_time();
    ESP_LOGI(TAG, "WiFi connected.");

    
    // Start MQTT client
    mqtt_connect_start_time = esp_timer_get_time();
    client = mqtt_app_start();

#if ESP_CAMERA_SUPPORTED
    if (init_camera() != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
        goto cleanup;
    }

    ESP_LOGI(TAG, "Taking picture...");
    capture_start_time = esp_timer_get_time();
    camera_fb_t *pic = esp_camera_fb_get();
    capture_end_time = esp_timer_get_time();

    if (!pic) {
        ESP_LOGE(TAG, "Camera capture failed");
        goto cleanup;
    }

    ESP_LOGI(TAG, "Picture taken in %.2f ms", (float)(capture_end_time - capture_start_time) / 1000);

    mqtt_start_time = esp_timer_get_time();
    int msg_id = esp_mqtt_client_publish(client, MQTT_BROKER_TOPIC_IMAGE, (const char *)pic->buf, pic->len, 0, 0);
    mqtt_end_time = esp_timer_get_time();
    ESP_LOGI(TAG, "Image sent with msg_id %d in %.2f ms", msg_id, (float)(mqtt_end_time - mqtt_start_time) / 1000);

    esp_camera_fb_return(pic);

    // Save timing data
    current_timing.wifi_time = wifi_end_time - wifi_start_time;
    current_timing.mqtt_publish_time = mqtt_end_time - mqtt_start_time;
    current_timing.mqtt_connect_time = mqtt_connect_end_time - mqtt_connect_start_time;
    current_timing.capture_time = capture_end_time - capture_start_time;

    save_timing_to_nvs(&current_timing, sample_count);
    sample_count++;

    // Update sample count in NVS
    ESP_ERROR_CHECK(nvs_open("timing", NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_i32(nvs, "sample_count", sample_count));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);

    ESP_LOGI(TAG, "Sample #%" PRId32 " stored.", sample_count);

    // If 100 samples collected, compute average and send
    if (sample_count == TIMING_SAMPLES) {
        int64_t total_wifi = 0, total_mqtt_publish = 0, total_mqtt_connect = 0, total_capture = 0;

        timing_data_t temp;
        for (int i = 0; i < TIMING_SAMPLES; i++) {
            load_timing_from_nvs(&temp, i);
            total_wifi += temp.wifi_time;
            total_mqtt_connect += temp.mqtt_connect_time;
            total_mqtt_publish += temp.mqtt_publish_time;
            total_capture += temp.capture_time;
        }

        float avg_wifi = (float)total_wifi / TIMING_SAMPLES / 1000;
        float avg_mqtt_connect = (float)total_mqtt_connect / TIMING_SAMPLES / 1000;
        float avg_mqtt_publish = (float)total_mqtt_publish / TIMING_SAMPLES / 1000;
        float avg_capture = (float)total_capture / TIMING_SAMPLES / 1000;

        char avg_json[256];
        snprintf(avg_json, sizeof(avg_json),
         "{\"avg_wifi\":%.2f,\"avg_mqtt_connect\":%.2f,\"avg_mqtt_publish\":%.2f,\"avg_capture\":%.2f}",
         avg_wifi, avg_mqtt_connect, avg_mqtt_publish, avg_capture);

        esp_mqtt_client_publish(client, MQTT_BROKER_TOPIC_AVERAGE, avg_json, strlen(avg_json), 0, 0);
        ESP_LOGI(TAG, "Averages published: %s", avg_json);

        // Reset samples
        reset_nvs_samples();
        sample_count = 0;
    }

    sent_successfully = true;

cleanup:
    if (client) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
    }
    ESP_ERROR_CHECK(app_wifi_disconnect());
    ESP_ERROR_CHECK(app_wifi_deinit());
#endif

    if (sent_successfully) {
        ESP_LOGI(TAG, "Operation successful. Going to deep sleep for 10 seconds...");
    } else {
        ESP_LOGE(TAG, "Operation failed. Going to deep sleep for 10 seconds...");
    }

    // Deep sleep for 10 seconds
    esp_deep_sleep(10 * 1000000);
}

// Save individual timing to NVS
void save_timing_to_nvs(const timing_data_t *data, int index)
{
    nvs_handle_t nvs;
    char key[32];

    ESP_ERROR_CHECK(nvs_open("timing", NVS_READWRITE, &nvs));

    snprintf(key, sizeof(key), "wifi_%d", index);
    ESP_ERROR_CHECK(nvs_set_i64(nvs, key, data->wifi_time));

    snprintf(key, sizeof(key), "mqtt_connect_%d", index);
    ESP_ERROR_CHECK(nvs_set_i64(nvs, key, data->mqtt_connect_time));

    snprintf(key, sizeof(key), "mqtt_publish_%d", index);
    ESP_ERROR_CHECK(nvs_set_i64(nvs, key, data->mqtt_publish_time));

    snprintf(key, sizeof(key), "capture_%d", index);
    ESP_ERROR_CHECK(nvs_set_i64(nvs, key, data->capture_time));

    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
}

// Load timing from NVS
void load_timing_from_nvs(timing_data_t *data, int index)
{
    nvs_handle_t nvs;
    char key[32];

    ESP_ERROR_CHECK(nvs_open("timing", NVS_READWRITE, &nvs));

    snprintf(key, sizeof(key), "wifi_%d", index);
    ESP_ERROR_CHECK(nvs_get_i64(nvs, key, &data->wifi_time));

    snprintf(key, sizeof(key), "mqtt_connect_%d", index);
    ESP_ERROR_CHECK(nvs_get_i64(nvs, key, &data->mqtt_connect_time));

    snprintf(key, sizeof(key), "mqtt_publish_%d", index);
    ESP_ERROR_CHECK(nvs_get_i64(nvs, key, &data->mqtt_publish_time));

    snprintf(key, sizeof(key), "capture_%d", index);
    ESP_ERROR_CHECK(nvs_get_i64(nvs, key, &data->capture_time));

    nvs_close(nvs);
}

// Clear all timing data in NVS
void reset_nvs_samples(void)
{
    nvs_handle_t nvs;
    char key[32];

    ESP_ERROR_CHECK(nvs_open("timing", NVS_READWRITE, &nvs));

    for (int i = 0; i < TIMING_SAMPLES; i++) {
        snprintf(key, sizeof(key), "wifi_%d", i);
        nvs_erase_key(nvs, key);

        snprintf(key, sizeof(key), "mqtt_connect_%d", i);
        nvs_erase_key(nvs, key);

        snprintf(key, sizeof(key), "mqtt_publish_%d", i);
        nvs_erase_key(nvs, key);

        snprintf(key, sizeof(key), "capture_%d", i);
        nvs_erase_key(nvs, key);
    }

    nvs_erase_key(nvs, "sample_count");
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
}