
#pragma once

#include "esp_err.h"

/**
 * @brief Initialize Wi-Fi in Station (STA) mode
 *
 * @return ESP_OK on success, or an error code from esp_err_t
 */
esp_err_t app_wifi_sta_init(void);

/**
 * @brief Connect to a Wi-Fi network in STA mode
 *
 * @param wifi_ssid SSID of the Wi-Fi network
 * @param wifi_password Password of the Wi-Fi network
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t app_wifi_sta_connect(char* wifi_ssid, char* wifi_password);

/**
 * @brief Disconnect from the Wi-Fi network in STA mode
 *
 * @return ESP_OK on success, or an error code from esp_err_t
 */
esp_err_t app_wifi_sta_disconnect(void);

/**
 * @brief Deinitialize Wi-Fi STA mode and release resources
 *
 * @return ESP_OK on success, or an error code from esp_err_t
 */
esp_err_t app_wifi_sta_deinit(void);