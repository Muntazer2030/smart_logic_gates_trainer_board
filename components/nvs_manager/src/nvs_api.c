#include "nvs.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#define TAG "NVS_API"

#define DEVICE_DATA_PARTITION_NAME "device_data"

// Initialize NVS partition
void init_nvs()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}
void init_device_data_nvs()
{
    // Initialize the custom NVS partition

    esp_err_t err = nvs_flash_init_partition(DEVICE_DATA_PARTITION_NAME);

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // Partition is either full or needs reformatting, erase it
        ESP_LOGW(TAG, "Custom NVS partition needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase_partition(DEVICE_DATA_PARTITION_NAME));
        err = nvs_flash_init_partition(DEVICE_DATA_PARTITION_NAME);
    }

    ESP_ERROR_CHECK(err); // Ensure the custom partition is properly initialized
    ESP_LOGI(TAG, "Custom NVS partition initialized successfully");
}
// function to write string to nvs
// used to store the switches configuration
esp_err_t write_str_to_nvs(const char *namespace, const char *key, const char *value)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS handle
    err = nvs_open(namespace, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle (%s)", esp_err_to_name(err));
        return err;
    }

    // Write value to NVS
    err = nvs_set_str(nvs_handle, key, value);
    if (err == ESP_OK)
    {
        // Commit changes
        err = nvs_commit(nvs_handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error committing changes (%s)", esp_err_to_name(err));
        }
    }
    else
    {
        ESP_LOGE(TAG, "Error writing to NVS (%s)", esp_err_to_name(err));
    }

    // Close NVS handle
    nvs_close(nvs_handle);
    return err;
}
// function to write int to nvs

esp_err_t write_int_to_nvs(const char *namespace, const char *key, const int value)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS handle
    err = nvs_open(namespace, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle (%s)", esp_err_to_name(err));
        return err;
    }

    // Write value to NVS
    err = nvs_set_i8(nvs_handle, key, value);
    if (err == ESP_OK)
    {
        // Commit changes
        err = nvs_commit(nvs_handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error committing changes (%s)", esp_err_to_name(err));
        }
    }
    else
    {
        ESP_LOGE(TAG, "Error writing to NVS (%s)", esp_err_to_name(err));
    }

    // Close NVS handle
    nvs_close(nvs_handle);
    return err;
}

// function to read string from nvs

esp_err_t read_str_from_nvs(const char *namespace, const char *key, char *value, size_t *length)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS handle
    err = nvs_open(namespace, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle (%s), namespace: %s", esp_err_to_name(err), namespace);
        return err;
    }

    // Read value from NVS
    err = nvs_get_str(nvs_handle, key, value, length);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading from NVS (%s), to read: %s", esp_err_to_name(err), key);
    }else{
        ESP_LOGI(TAG, "Value read from NVS to read: %s", value);
    }

    // Close NVS handle
    nvs_close(nvs_handle);
    return err;
}

// function to read int from nvs

esp_err_t read_int_from_nvs(const char *namespace, const char *key, int8_t *value)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS handle
    err = nvs_open(namespace, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle (%s)", esp_err_to_name(err));
        return err;
    }

    // Read value from NVS
    err = nvs_get_i8(nvs_handle, key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading from NVS (%s)", esp_err_to_name(err));
    }

    // Close NVS handle
    nvs_close(nvs_handle);
    return err;
}

// function to read string from device data partition
esp_err_t read_str_from_device_data_nvs(const char *namespace, const char *key, char *value, size_t *length)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS handle
    err = nvs_open_from_partition(DEVICE_DATA_PARTITION_NAME, namespace, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening device_data handle (%s)", esp_err_to_name(err));
        return err;
    }

    // Read value from NVS
    err = nvs_get_str(nvs_handle, key, value, length);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading from device_data (%s)", esp_err_to_name(err));
    }

    // Close NVS handle
    nvs_close(nvs_handle);
    return err;
}
esp_err_t write_str_to_device_data_nvs(const char *namespace, const char *key, const char *value)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS handle
    err = nvs_open_from_partition(DEVICE_DATA_PARTITION_NAME, namespace, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening device_data handle (%s)", esp_err_to_name(err));
        return err;
    }

    // Write value to NVS
    err = nvs_set_str(nvs_handle, key, value);
    if (err == ESP_OK)
    {
        // Commit changes
        err = nvs_commit(nvs_handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error committing changes (%s)", esp_err_to_name(err));
        }
    }
    else
    {
        ESP_LOGE(TAG, "Error writing to device_data (%s)", esp_err_to_name(err));
    }

    // Close NVS handle
    nvs_close(nvs_handle);
    return err;
}