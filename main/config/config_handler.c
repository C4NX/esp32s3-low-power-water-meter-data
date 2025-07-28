#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "config_handler";

/**
 * @brief Helper to get a query value from a key
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
            return value;
        }
    }
    return NULL;
}

/**
 * @brief Handle /config POST route
 */
esp_err_t config_handler(httpd_req_t *req) {
    char *buf = NULL;
    int ret;

    // Check if request is POST
    if (req->method != HTTP_POST) {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Only POST method allowed");
        return ESP_FAIL;
    }

    // Allocate memory for the request content
    size_t content_len = req->content_len;
    if (content_len > 4096) {  // Set a reasonable limit
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Request too large");
        return ESP_FAIL;
    }
    
    buf = malloc(content_len + 1);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Read form data
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

    if (!ssid || !password || !mqtt) {
        ESP_LOGE(TAG, "Missing form fields");
        const char *resp = "Error: Missing fields.";
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, resp);
        free(buf);
        goto cleanup;
    }

    ESP_LOGI(TAG, "Received config:");
    ESP_LOGI(TAG, "SSID: %s", ssid);
    ESP_LOGI(TAG, "Password: %s", password);
    ESP_LOGI(TAG, "MQTT: %s", mqtt);

    // Validate input lengths
    #define MAX_SSID_LEN 32
    #define MAX_PASSWORD_LEN 64
    #define MAX_MQTT_LEN 128

    if (strlen(ssid) > MAX_SSID_LEN) {
        ESP_LOGE(TAG, "SSID too long");
        const char *resp = "Error: SSID must be <= 32 characters.";
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, resp);
        goto cleanup;
    }

    if (strlen(password) > MAX_PASSWORD_LEN) {
        ESP_LOGE(TAG, "Password too long");
        const char *resp = "Error: Password must be <= 64 characters.";
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, resp);
        goto cleanup;
    }

    if (strlen(mqtt) > MAX_MQTT_LEN) {
        ESP_LOGE(TAG, "MQTT server too long");
        const char *resp = "Error: MQTT server must be <= 128 characters.";
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, resp);
        goto cleanup;
    }

    // Open NVS
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle");
        httpd_resp_send_500(req);
        goto cleanup;
    }

    // Save values to NVS
    nvs_set_str(my_handle, "ssid", ssid);
    nvs_set_str(my_handle, "password", password);
    nvs_set_str(my_handle, "mqtt", mqtt);

    nvs_commit(my_handle);
    nvs_close(my_handle);

    const char *resp = "Configuration saved successfully. \0";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

cleanup:
    if (buf) free(buf);
    if (ssid) free(ssid);
    if (password) free(password);
    if (mqtt) free(mqtt);

    return ESP_OK;
}