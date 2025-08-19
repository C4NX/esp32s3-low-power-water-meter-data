#define TAG "mqtt"

#include <stdio.h>
#include <inttypes.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "config/config.h"
#include "mqtt_client.h"
#include "esp_event.h"

#define MQTT_BROKER_TOPIC "IeziPVyPNYnaOW4U/"

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

    // Initialize MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = mqtt_broker_uri,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, app_mqtt_event_handler_cb, client);
    esp_mqtt_client_start(client);
    return client;
}