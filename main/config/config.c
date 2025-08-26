
#include "config.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"

#define TAG "config"

#define CONFIG_NVS_NAMESPACE "app_config"

#define CONFIG_KEY_SSID "ssid"
#define CONFIG_KEY_PASSWORD "password"
#define CONFIG_KEY_MQTT "mqtt"
#define CONFIG_KEY_CAPTURE_INTERVAL "capt_intr"

#define CONFIG_DEFAULT_CAPTURE_INTERVAL 10 // 10 seconds

esp_err_t app_config_set(const char *ssid, const char *password, const char *mqtt, uint32_t capture_interval)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // Set SSID
    err = nvs_set_str(handle, CONFIG_KEY_SSID, ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set SSID: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // Set Password
    err = nvs_set_str(handle, CONFIG_KEY_PASSWORD, password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Password: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // Set MQTT Broker URI
    err = nvs_set_str(handle, CONFIG_KEY_MQTT, mqtt);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set MQTT: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // Set Interval
    err = nvs_set_u32(handle, CONFIG_KEY_CAPTURE_INTERVAL, capture_interval);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Capture Interval: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // Commit changes
    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit changes: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    nvs_close(handle);
    return ESP_OK;
}

char* app_config_get_mqtt(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return NULL;
    }

    size_t mqtt_len = 0;
    err = nvs_get_str(handle, CONFIG_KEY_MQTT, NULL, &mqtt_len);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to get MQTT length: %s", esp_err_to_name(err));
        nvs_close(handle);
        return NULL;
    }

    char *mqtt = malloc(mqtt_len + 1);
    if (!mqtt) {
        ESP_LOGE(TAG, "Failed to allocate memory for MQTT");
        nvs_close(handle);
        return NULL;
    }

    err = nvs_get_str(handle, CONFIG_KEY_MQTT, mqtt, &mqtt_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get MQTT: %s", esp_err_to_name(err));
        free(mqtt);
        nvs_close(handle);
        return NULL;
    }

    nvs_close(handle);
    return mqtt;
}

char* app_config_get_ssid(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return NULL;
    }

    size_t ssid_len = 0;
    err = nvs_get_str(handle, CONFIG_KEY_SSID, NULL, &ssid_len);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to get SSID length: %s", esp_err_to_name(err));
        nvs_close(handle);
        return NULL;
    }

    char *ssid = malloc(ssid_len + 1);
    if (!ssid) {
        ESP_LOGE(TAG, "Failed to allocate memory for SSID");
        nvs_close(handle);
        return NULL;
    }

    err = nvs_get_str(handle, CONFIG_KEY_SSID, ssid, &ssid_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SSID: %s", esp_err_to_name(err));
        free(ssid);
        nvs_close(handle);
        return NULL;
    }

    nvs_close(handle);
    return ssid;
}

char* app_config_get_password(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return NULL;
    }

    size_t password_len = 0;
    err = nvs_get_str(handle, CONFIG_KEY_PASSWORD, NULL, &password_len);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to get Password length: %s", esp_err_to_name(err));
        nvs_close(handle);
        return NULL;
    }

    char *password = malloc(password_len + 1);
    if (!password) {
        ESP_LOGE(TAG, "Failed to allocate memory for Password");
        nvs_close(handle);
        return NULL;
    }

    err = nvs_get_str(handle, CONFIG_KEY_PASSWORD, password, &password_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get Password: %s", esp_err_to_name(err));
        free(password);
        nvs_close(handle);
        return NULL;
    }

    nvs_close(handle);
    return password;
}

uint32_t app_config_get_capture_interval(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return CONFIG_DEFAULT_CAPTURE_INTERVAL;
    }

    uint32_t capture_interval = 0;
    err = nvs_get_u32(handle, CONFIG_KEY_CAPTURE_INTERVAL, &capture_interval);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get Capture Interval: %s", esp_err_to_name(err));
        nvs_close(handle);
        return CONFIG_DEFAULT_CAPTURE_INTERVAL;
    }

    nvs_close(handle);
    return capture_interval;
}

esp_err_t app_config_reset(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // Erase all keys in the namespace
    err = nvs_erase_all(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    // Commit changes
    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit changes: %s", esp_err_to_name(err));
    }

    nvs_close(handle);
    return err;
}