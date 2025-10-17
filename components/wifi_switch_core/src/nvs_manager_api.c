#include <stddef.h>
#include <string.h>
#include "nvs_api.h"
#include "esp_log.h"
#include "global_variables.h"
#include "json_encode_decode.h"
#include <esp_partition.h>
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "nvs_flash.h"

#define GPIOS_CONFIGURATION_NAMESPACE "gpios_config"
#define GPIOS_STATE_NAMESPACE "gpios_state"

#define TAG "WIFI_SWITCH_NVS_API"

void gpio_save_state(int switch_number, int state)
{
    char key[20];
    snprintf(key, sizeof(key), "SWITCH%d_STATE", switch_number);

    esp_err_t err = write_int_to_nvs(GPIOS_STATE_NAMESPACE, key, state);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Successfully wrote %s: %d to NVS", key, state);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to write %s to NVS (%s)", key, esp_err_to_name(err));
    }
}
void gpio_get_saved_state(int switch_number, int *state)
{

    char key[20];
    snprintf(key, sizeof(key), "SWITCH%d_STATE", switch_number);

    esp_err_t err = ESP_FAIL;
    int stored_state = 0;

    err = read_int_from_nvs(GPIOS_STATE_NAMESPACE, key, (void *)&stored_state);
    if (err == ESP_OK)
    {
        *state = stored_state;
        ESP_LOGI(TAG, "Value read from NVS for %s: %d", key, *state);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to read %s from NVS (%s)", key, esp_err_to_name(err));
        *state = 0;
        ESP_LOGW(TAG, "%s state defaulted to %d", key, *state);
    }
}

esp_err_t get_saved_switch_config(int switch_number, char *switch_config)
{
    size_t length = 250;
    char key[12];
    snprintf(key, sizeof(key), "SWITCH%d", switch_number);
    
    return read_str_from_nvs(GPIOS_CONFIGURATION_NAMESPACE, key, switch_config, &length);
}

// saving switches configuration to nvsGPIOS_CONFIGURATION_NAMESPACE
void save_switches_config(int switch_number)
{
    if (switch_number < 0 || switch_number >= switches_count)
    {
        ESP_LOGE(TAG, "Invalid switch number: %d", switch_number);
        return;
    }

    char key[18];
    snprintf(key, sizeof(key), "SWITCH%d", switch_number);

    char *switch_config_json = NULL;
    encode_switch_configuration(&switch_configs[switch_number-1], &switch_config_json); // Pass address

    if (switch_config_json == NULL) // Ensure encoding succeeded
    {
        ESP_LOGE(TAG, "Failed to encode switch configuration for switch %d", switch_number);
        return;
    }

    write_str_to_nvs(GPIOS_CONFIGURATION_NAMESPACE, key, switch_config_json);

    ESP_LOGI(TAG, "Successfully wrote switch configuration for switch %d to NVS, key: %s, value: %s", switch_number, key, switch_config_json);

    free(switch_config_json);
}
void go_to_factory_app_to_start_update()
{
    // Find the factory partition
    const esp_partition_t *factory_part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    if (!factory_part)
    {
        ESP_LOGE("factory_boot", "Factory partition not found!");
        return;
    }
    write_str_to_device_data_nvs("device_data", "update", "1");
    // Set boot partition to factory
    esp_err_t err = esp_ota_set_boot_partition(factory_part);
    if (err != ESP_OK)
    {
        ESP_LOGE("factory_boot", "Failed to set boot partition: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI("factory_boot", "Boot partition set to factory, restarting...");

    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}
void factory_reset(void)
{
    esp_err_t err;

    ESP_LOGI(TAG, "Erasing NVS...");
    err = nvs_flash_erase();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Erasing OTA data...");
    const esp_partition_t *ota_data_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);

    if (ota_data_partition != NULL) {
        err = esp_partition_erase_range(ota_data_partition, 0, ota_data_partition->size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase OTA data: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGW(TAG, "OTA data partition not found");
    }

    ESP_LOGI(TAG, "Setting boot partition to factory app...");
    const esp_partition_t *factory_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);

    if (factory_partition != NULL) {
        err = esp_ota_set_boot_partition(factory_partition);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set factory partition: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Factory partition not found");
    }

    ESP_LOGI(TAG, "Restarting device...");
    esp_restart();
}