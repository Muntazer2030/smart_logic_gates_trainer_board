
#include "nvs_api.h"    
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#define TAG "MAIN NVS"

// to get wifi credentials from nvs
void get_wifi_cred(char *ssid, char *pass)
{
    size_t len = 64;
    esp_err_t err = read_str_from_nvs(CONFIG_WIFI_NAMESPACE, "ssid", ssid, &len);
    if (err == ESP_OK)
    {
        ssid[63] = '\0'; // Ensure null-termination
        ESP_LOGI(TAG, "Value read from NVS for ssid: %s", ssid);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to read ssid from NVS (%s)", esp_err_to_name(err));
    }
    len = 64;
    err = read_str_from_nvs(CONFIG_WIFI_NAMESPACE, "password", pass, &len);
    if (err == ESP_OK)
    {
        pass[63] = '\0'; // Ensure null-termination
        ESP_LOGI(TAG, "Value read from NVS for password: %s", pass);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to read password from NVS (%s)", esp_err_to_name(err));
    }
}

void set_wifi_cred(const char *ssid, const char *password)
{
    write_str_to_nvs(CONFIG_WIFI_NAMESPACE, "ssid", ssid);
    write_str_to_nvs(CONFIG_WIFI_NAMESPACE, "password", password);
}

// function to get the uuid and private key and certificat from device data partition
void get_device_data(char *cert, char *key, char *uuid)
{
    size_t len = 2048;
    esp_err_t err = read_str_from_device_data_nvs(CONFIG_DEVICE_DATA_NAMESPACE, "cert", cert, &len);
    if (err == ESP_OK)
    {

        ESP_LOGI(TAG, "Value read from NVS for cert: %s", cert);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to read cert from NVS (%s)", esp_err_to_name(err));
    }
    len = 2048;
    err = read_str_from_device_data_nvs(CONFIG_DEVICE_DATA_NAMESPACE, "key", key, &len);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Value read from NVS for key: %s", key);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to read key from NVS (%s)", esp_err_to_name(err));
    }
    len = 64;
    err = read_str_from_device_data_nvs(CONFIG_DEVICE_DATA_NAMESPACE, "uuid", uuid, &len);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Value read from NVS for uuid: %s", uuid);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to read uuid from NVS (%s)", esp_err_to_name(err));
    }
}

void set_device_data(const char *cert, const char *key, const char *uuid)
{

    write_str_to_device_data_nvs(CONFIG_DEVICE_DATA_NAMESPACE, "cert", cert);
    write_str_to_device_data_nvs(CONFIG_DEVICE_DATA_NAMESPACE, "key", key);
    write_str_to_device_data_nvs(CONFIG_DEVICE_DATA_NAMESPACE, "uuid", uuid);
}
