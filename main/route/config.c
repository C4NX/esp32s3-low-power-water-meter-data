#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdlib.h>

#include "config/config.h"

static const char *TAG = "route/config";

#include <ctype.h>

/**
 * @brief Helper function to decode URL-encoded strings
 */
static void url_decode(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (*src == '%' && isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])) {
            char hex[3] = { src[1], src[2], '\0' };
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/**
 * @brief Helper to get a query value from a key (in body of a POST request for example)
 */
char* get_query_value(const char *query, const char *key) {
    const char *start = strstr(query, key);
    if (!start) return NULL;

    start += strlen(key);
    if (*start == '=') {
        start++;
        const char *end = strchr(start, '&');
        int len = end ? (end - start) : strlen(start);
        char *value = malloc(len + 1);
        if (value) {
            strncpy(value, start, len);
            value[len] = '\0';
            url_decode(value);
            return value;
        }
    }
    return NULL;
}

/**
 * @brief Handle /config POST route
 */
esp_err_t route_config_handler(httpd_req_t *req) {
    char *buf = NULL;
    int ret;

    // Check if request is POST
    if (req->method != HTTP_POST) {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Only POST method allowed");
        return ESP_FAIL;
    }

    // Allocate memory for the request content
    size_t content_len = req->content_len;
    if (content_len > 4096) { // limit to prevent abuse
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Request too large");
        return ESP_FAIL;
    }
    
    buf = malloc(content_len + 1);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Read form data (key=value pairs)
    int remaining = content_len;
    int offset = 0;
    while (remaining > 0) {
        ret = httpd_req_recv(req, buf + offset, remaining);
        if (ret <= 0) {
            free(buf);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        remaining -= ret;
        offset += ret;
    }
    buf[content_len] = '\0';  // Null-terminate

    // Extract values from the form
    char *ssid = get_query_value(buf, "ssid");
    char *password = get_query_value(buf, "password");
    char *mqtt = get_query_value(buf, "mqtt");

    // Check all required fields
    if (!ssid || !password || !mqtt) {
        ESP_LOGE(TAG, "Missing form fields");
        const char *resp = "{\"status\":\"error\",\"message\":\"Missing fields.\"}";
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, resp);
        goto cleanup;
    }

    ESP_LOGI(TAG, "Received config:");
    ESP_LOGI(TAG, "SSID: %s", ssid);
    ESP_LOGI(TAG, "Password: %s", password);
    ESP_LOGI(TAG, "MQTT: %s", mqtt);

    // Validate inputs
    if (strlen(ssid) > APP_CONFIG_MAX_SSID_LEN) {
        ESP_LOGE(TAG, "SSID too long");
        const char *resp = "{\"status\":\"error\",\"message\":\"SSID must be <= 32 characters.\"}";
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, resp);
        goto cleanup;
    }

    if (strlen(password) > APP_CONFIG_MAX_PASSWORD_LEN) {
        ESP_LOGE(TAG, "Password too long");
        const char *resp = "{\"status\":\"error\",\"message\":\"Password must be <= 64 characters.\"}";
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, resp);
        goto cleanup;
    }

    if (strlen(mqtt) > APP_CONFIG_MAX_MQTT_LEN) {
        ESP_LOGE(TAG, "MQTT server too long");
        const char *resp = "{\"status\":\"error\",\"message\":\"MQTT server must be <= 128 characters.\"}";
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, resp);
        goto cleanup;
    }

    // Save configuration to NVS
    app_config_set(ssid, password, mqtt);
    ESP_LOGI(TAG, "Configuration saved successfully");

    // Return success response as JSON
    httpd_resp_set_type(req, "application/json");
    const char *resp = "{\"status\":\"success\",\"message\":\"Configuration saved successfully, restarting device...\"}";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    // Restart ESP after successful configuration
    ESP_LOGI(TAG, "Restarting ESP (in 10 seconds...)");
    vTaskDelay(pdMS_TO_TICKS(10000));
    esp_restart();
cleanup:
    if (buf) free(buf);
    if (ssid) free(ssid);
    if (password) free(password);
    if (mqtt) free(mqtt);

    return ESP_OK;
}