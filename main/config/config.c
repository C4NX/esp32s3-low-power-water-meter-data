
#include "config.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"

#define TAG "config"

// Expose config keys as const char*
const char *CONFIG_KEY_SSID = "ssid";
const char *CONFIG_KEY_PASSWORD = "password";
const char *CONFIG_KEY_MQTT = "mqtt";

esp_err_t app_config_set(const char *ssid, const char *password, const char *mqtt)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("app_config", NVS_READWRITE, &handle);
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
    esp_err_t err = nvs_open("app_config", NVS_READONLY, &handle);
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
    esp_err_t err = nvs_open("app_config", NVS_READONLY, &handle);
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
    esp_err_t err = nvs_open("app_config", NVS_READONLY, &handle);
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

esp_err_t app_config_reset(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("app_config", NVS_READWRITE, &handle);
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