
#pragma once

#include <stdbool.h>
#include <esp_err.h>

#define APP_CONFIG_MAX_SSID_LEN 32
#define APP_CONFIG_MAX_PASSWORD_LEN 64
#define APP_CONFIG_MAX_MQTT_LEN 128

#define APP_CONFIG_KEY_SSID "ssid"
#define APP_CONFIG_KEY_PASSWORD "password"
#define APP_CONFIG_KEY_MQTT "mqtt"

/**
 * @brief Get the MQTT broker URI from the configuration stored in NVS
 */
char *app_config_get_mqtt(void);

/**
 * @brief Get the SSID from the configuration stored in NVS
 */
char *app_config_get_ssid(void);

/**
 * @brief Get the password from the configuration stored in NVS
 */
char *app_config_get_password(void);

/**
 * @brief Get the capture interval (in seconds) from the configuration stored in NVS
 */
uint32_t app_config_get_capture_interval(void);

/**
 * @brief Set the configuration values (SSID, password, MQTT) in NVS
 * @param ssid SSID to set
 * @param password Password to set
 * @param mqtt MQTT broker address to set
 * @param capture_interval Capture interval in seconds to set
 * @return ESP_OK if all values were written successfully, an error code otherwise
 */
esp_err_t app_config_set(const char *ssid, const char *password, const char *mqtt, const uint32_t capture_interval);

/**
 * @brief Reset the configuration stored in NVS
 * @return ESP_OK if the configuration was reset successfully, an error code otherwise
 */
esp_err_t app_config_reset(void);