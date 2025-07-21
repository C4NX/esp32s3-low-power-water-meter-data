#include "esp_err.h"

/**
 * @brief Initialize Wi-Fi in Access Point (AP) mode
 *
 * @return esp_err_t
 */
esp_err_t app_wifi_init(void);

/**
 * @brief Deinitialize and clean up Wi-Fi (AP mode)
 */
void app_wifi_deinit(void);