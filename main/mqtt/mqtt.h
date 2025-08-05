
#pragma once
#include "mqtt_client.h"

esp_mqtt_client_handle_t app_mqtt_start(void);

char* app_mqtt_get_broker_topic(void);