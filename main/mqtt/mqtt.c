#define TAG "mqtt"

#include <stdio.h>
#include <inttypes.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "config/config.h"
#include "mqtt_client.h"
#include "esp_event.h"


#include "esp_system.h"
#include "esp_mac.h"


static char *MQTT_BROKER_TOPIC;

static void app_mqtt_event_handler_cb(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            esp_mqtt_client_publish(client, MQTT_BROKER_TOPIC, "Hello from ESP32S3!", 0, 0, 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        default:
            ESP_LOGI(TAG, "Other MQTT event id:%d", event->event_id);
            break;
    }
}


// get an unique topic for the device
// This topic can be used to identify the device uniquely in the MQTT broker
char* app_mqtt_get_unique_topic(void)
{
    uint8_t mac[6];
    char* topic = NULL;

    // Get the default MAC address (station mode)
    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK) {
        ESP_LOGE("MQTT", "Failed to read MAC address: %s", esp_err_to_name(err));
        return NULL;
    }

    // Allocate memory for topic string (max: "device-XX:XX:XX:XX:XX:XX\0" = 32 chars)
    topic = (char*)malloc(32);
    if (topic == NULL) {
        ESP_LOGE("MQTT", "Memory allocation failed for topic");
        return NULL;
    }

    // Create topic using full MAC for uniqueness: e.g., device-a0:b1:c2:d3:e4:f5
    int len = snprintf(topic, 32, "device-%02x:%02x:%02x:%02x:%02x:%02x",
                       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if (len < 0 || len >= 32) {
        ESP_LOGE("MQTT", "Topic formatting failed");
        free(topic);
        return NULL;
    }

    return topic;
}

char* app_mqtt_get_broker_topic(void)
{
    if (MQTT_BROKER_TOPIC == NULL) {
        ESP_LOGE(TAG, "MQTT broker topic is not set");
        return NULL;
    }
    return MQTT_BROKER_TOPIC;
}

esp_mqtt_client_handle_t app_mqtt_start(void)
{
    ESP_LOGI(TAG, "Starting MQTT client...");

    // Retrive MQTT broker URI from configuration
    char *mqtt_broker_uri = app_config_get_mqtt();

    // Check if MQTT broker URI is configured
    if(mqtt_broker_uri == NULL || strlen(mqtt_broker_uri) == 0) {
        ESP_LOGE(TAG, "MQTT broker URI is not configured");
        return NULL;
    }

    // Get a unique topic for the device
    MQTT_BROKER_TOPIC = app_mqtt_get_unique_topic();
    if (MQTT_BROKER_TOPIC == NULL) {
        ESP_LOGE(TAG, "Failed to get unique MQTT topic");
        return NULL;
    }else {
        ESP_LOGI(TAG, "Using MQTT topic: %s", MQTT_BROKER_TOPIC);
    }

    // Initialize MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = mqtt_broker_uri,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, app_mqtt_event_handler_cb, client);
    esp_mqtt_client_start(client);
    return client;
}