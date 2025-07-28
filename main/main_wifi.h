#include "esp_err.h"

/**
 * @brief Initialize Wi-Fi in Access Point (AP) mode
 *
 * @return esp_err_t
 */
esp_err_t app_wifi_ap_init(void);

/**
 * @brief Deinitialize and clean up Wi-Fi (AP mode)
 */
void app_wifi_ap_deinit(void);

/**
 * @brief Initialize Wi-Fi in STA mode
 *
 * @return esp_err_t
 */
esp_err_t app_wifi_sta_init(void);


/*
 * @brief Wi-Fi connection STA mode	
 */
esp_err_t app_wifi_sta_connect();

/*
 * @brief Wi-Fi disconnection STA mode	
 */
esp_err_t app_wifi_sta_disconnect();

/**
 * @brief Deinitialize and clean up Wi-Fi (STA mode)
 */
void app_wifi_sta_deinit(void);


