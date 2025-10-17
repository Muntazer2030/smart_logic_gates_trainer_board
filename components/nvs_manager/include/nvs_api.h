#pragma once
#include <esp_err.h>
/**
 * @brief Initialize NVS
 */
void init_nvs();

/**
 * @brief Initialize device data NVS
 */
void init_device_data_nvs();

/**
 * @brief Write a string to NVS
 * @param namespace The NVS namespace
 * @param key The key to store the string
 * @param value The string value
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t write_str_to_nvs(const char *namespace, const char *key, const char *value);

esp_err_t read_str_from_nvs(const char *namespace, const char *key, char *value, size_t *length);
esp_err_t write_int_to_nvs(const char *namespace, const char *key, const int value);
esp_err_t read_int_from_nvs(const char *namespace, const char *key, int8_t *value);
esp_err_t write_str_to_device_data_nvs(const char *namespace, const char *key, const char *value);
esp_err_t read_str_from_device_data_nvs(const char *namespace, const char *key, char *value, size_t *length);
