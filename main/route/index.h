#pragma once
#include "esp_http_server.h"

/**
 * @brief HTTP handler for the index page (serves /littlefs/index.html)
 * @param req HTTP request pointer
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t route_index_handler(httpd_req_t *req);