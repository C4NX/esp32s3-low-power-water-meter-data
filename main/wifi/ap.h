#pragma once
#include "esp_err.h"

/**
 * @brief Initialize Wi-Fi with default configuration
 */
void app_wifi_init(void);

/**
 * @brief Initialize Wi-Fi in Access Point (AP) mode
 *
 * @return ESP_OK on success, or an error code from esp_err_t
 */
esp_err_t app_wifi_ap_init(void);

/**
 * @brief Deinitialize Wi-Fi AP mode and release resources
 */
void app_wifi_ap_deinit(void);
