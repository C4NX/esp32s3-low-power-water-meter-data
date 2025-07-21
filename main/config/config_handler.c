#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

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
    char buf[512];
    int ret;

    // Read URL query data
    size_t recv_size = MIN(req->content_len, sizeof(buf)-1);
    ret = httpd_req_recv(req, buf, recv_size);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0'; // Null-terminate

    // Extract values from the form
    char *ssid = get_query_value(buf, "ssid");
    char *password = get_query_value(buf, "password");
    char *mqtt = get_query_value(buf, "mqtt");

    if (!ssid || !password || !mqtt) {
        ESP_LOGE(TAG, "Missing form fields");
        const char *resp = "Error: Missing fields.";
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, resp);
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

    const char *resp = "Configuration saved successfully.";
    httpd_resp_send(req, resp, 200);

cleanup:
    free(ssid);
    free(password);
    free(mqtt);

    return ESP_OK;
}