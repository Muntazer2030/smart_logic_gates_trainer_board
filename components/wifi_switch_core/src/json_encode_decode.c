#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include "global_variables.h"
#include "esp_log.h"
void decode_message(const char *message, char *command, cJSON **content)
{
    cJSON *root = cJSON_Parse(message);
    if (!root)
    {
        printf("Error parsing JSON\n");
        return;
    }

    cJSON *j_command = cJSON_GetObjectItem(root, "command");
    if (j_command && cJSON_IsString(j_command))
    {
        strcpy(command, j_command->valuestring);
        printf("Command: %s\n", command);
    }

    cJSON *j_content = cJSON_GetObjectItem(root, "content");
    if (j_content)
    {
        *content = cJSON_Duplicate(j_content, 1);
    }

    cJSON_Delete(root);
}

void encode_switches_state(char **switches_state_json)
{

    cJSON *json = cJSON_CreateObject();
    cJSON *contant = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "uuid", uuid);
    cJSON_AddStringToObject(json, "firmware version", CONFIG_FIRMWARE_VERSION);
    cJSON_AddStringToObject(json, "api version", CONFIG_API_VERSION);
    cJSON_AddStringToObject(json, "command", "get_state");
    if (!json)
    {
        ESP_LOGE("JSON", "Failed to create JSON object");
        return;
    }
    for (size_t i = 0; i < switches_count; i++)
    {
        switch_configuration_t *switch_config = &switch_configs[i];
        char switch_name[18];
        snprintf(switch_name, sizeof(switch_name), "SWITCH%d", i + 1);

        cJSON *switch_json = cJSON_CreateObject();

        cJSON_AddNumberToObject(switch_json, "state", switches_states[i]);
        cJSON_AddNumberToObject(switch_json, "start_mode", switch_config->start_mode);
        cJSON_AddNumberToObject(switch_json, "schedule", switch_config->schedule);
        cJSON_AddNumberToObject(switch_json, "repeat", switch_config->repeat);
        cJSON_AddNumberToObject(switch_json, "indicator_mode", switch_config->indicator_mode);
        cJSON_AddNumberToObject(switch_json, "security_mode", switch_config->security_mode);

        cJSON *days = cJSON_CreateIntArray(switch_config->days, sizeof(switch_config->days) / sizeof(switch_config->days[0]));
        if (!days)
        {
            cJSON_Delete(switch_json);
            ESP_LOGE("JSON", "Failed to create 'days' array");
            return;
        }
        cJSON_AddItemToObject(switch_json, "days", days);

        cJSON_AddStringToObject(switch_json, "end_date", switch_config->end_date);
        cJSON_AddStringToObject(switch_json, "time_from_to", switch_config->time_from_to);

       

        cJSON_AddItemToObject(contant, switch_name, switch_json);
        
    }
    cJSON_AddItemToObject(json, "content", contant);
    *switches_state_json = cJSON_Print(json);
    if (!(*switches_state_json))
    {
        ESP_LOGE("JSON", "Failed to print JSON string");
    }

    cJSON_Delete(json);
}

void encode_switch_configuration(switch_configuration_t *switch_config, char **switch_config_json)
{
    cJSON *json = cJSON_CreateObject();
    if (!json)
    {
        ESP_LOGE("JSON", "Failed to create JSON object");
        return;
    }

    cJSON_AddNumberToObject(json, "start_mode", switch_config->start_mode);
    cJSON_AddNumberToObject(json, "schedule", switch_config->schedule);
    cJSON_AddNumberToObject(json, "repeat", switch_config->repeat);
    cJSON_AddNumberToObject(json, "indicator_mode", switch_config->indicator_mode);
    cJSON_AddNumberToObject(json, "security_mode", switch_config->security_mode);
    cJSON_AddStringToObject(json, "end_date", switch_config->end_date);
    cJSON_AddStringToObject(json, "time_from_to", switch_config->time_from_to);
    cJSON *days = cJSON_CreateIntArray(switch_config->days, sizeof(switch_config->days) / sizeof(switch_config->days[0]));
    if (!days)
    {
        cJSON_Delete(json);
        ESP_LOGE("JSON", "Failed to create 'days' array");
        return;
    }
    cJSON_AddItemToObject(json, "days", days);

    *switch_config_json = cJSON_PrintUnformatted(json); // Allocates new memory
    if (!(*switch_config_json))
    {
        ESP_LOGE("JSON", "Failed to print JSON string");
    }

    cJSON_Delete(json); // Safe now, because `cJSON_PrintUnformatted` makes a copy
}
void decode_switch_configuration(const char *switch_config_json, switch_configuration_t *switch_config)
{
    cJSON *json = cJSON_Parse(switch_config_json);
    if (!json)
    {
        ESP_LOGE("JSON", "Failed to parse JSON string");
        return;
    }

    cJSON *start_mode = cJSON_GetObjectItem(json, "start_mode");
    cJSON *schedule = cJSON_GetObjectItem(json, "schedule");
    cJSON *repeat = cJSON_GetObjectItem(json, "repeat");
    cJSON *indicator_mode = cJSON_GetObjectItem(json, "indicator_mode");
    cJSON *security_mode = cJSON_GetObjectItem(json, "security_mode");
    cJSON *days = cJSON_GetObjectItem(json, "days");
    cJSON *end_date = cJSON_GetObjectItem(json, "end_date");
    cJSON *time_from_to = cJSON_GetObjectItem(json, "time_from_to");

    switch_config->start_mode = start_mode ? start_mode->valueint : 0;
    switch_config->schedule = schedule ? schedule->valueint : 0;
    switch_config->repeat = repeat ? repeat->valueint : 0;
    switch_config->indicator_mode = indicator_mode ? indicator_mode->valueint : 0;
    switch_config->security_mode = security_mode ? security_mode->valueint : 0;

    // Handle end_date
    if (cJSON_IsString(end_date) && (end_date->valuestring != NULL))
    {
        // Allocate memory and copy the string
        switch_config->end_date = strdup(end_date->valuestring);
        if (!switch_config->end_date)
        {
            ESP_LOGE("JSON", "Failed to allocate memory for end_date");
        }
    }
    else
    {
        switch_config->end_date = strdup("0000:00:00"); // Default value
    }

    // Handle time_from_to
    if (cJSON_IsString(time_from_to) && (time_from_to->valuestring != NULL))
    {
        // Allocate memory and copy the string
        switch_config->time_from_to = strdup(time_from_to->valuestring);
        if (!switch_config->time_from_to)
        {
            ESP_LOGE("JSON", "Failed to allocate memory for time_from_to");
        }
    }
    else
    {
        switch_config->time_from_to = strdup("00:00-00:00"); // Default value
    }

    // Handle days array
    if (days && cJSON_IsArray(days))
    {
        int i = 0;
        cJSON *day = cJSON_GetArrayItem(days, i);
        while (day)
        {
            switch_config->days[i] = day->valueint;
            i++;
            day = cJSON_GetArrayItem(days, i);
        }
    }
    else
    {
        for (int i = 0; i < 7; i++)
        {
            switch_config->days[i] = 0;
        }
    }

    // Log values for debugging
    ESP_LOGI("JSON", "Start mode: %d", switch_config->start_mode);
    ESP_LOGI("JSON", "Schedule: %d", switch_config->schedule);
    ESP_LOGI("JSON", "Repeat: %d", switch_config->repeat);
    ESP_LOGI("JSON", "Indicator mode: %d", switch_config->indicator_mode);
    ESP_LOGI("JSON", "Security mode: %d", switch_config->security_mode);
    ESP_LOGI("JSON", "End date: %s", switch_config->end_date);
    ESP_LOGI("JSON", "Time from to: %s", switch_config->time_from_to);

    cJSON_Delete(json);
}
