#include "esp_http_server.h"
#include "config/config.h"

/**
 * @brief Helper to get a query value from a key (in body of a POST request for example)
 * @param query The query string (e.g., "ssid=foo&password=bar")
 * @param key The key to search for (e.g., "ssid")
 * @return Dynamically allocated string containing the value, or NULL if not found. Caller must free.
 */
char* get_query_value(const char *query, const char *key);

/**
 * @brief Handle configuration route (POST)
 * @param req HTTP request pointer
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t route_config_handler(httpd_req_t *req);