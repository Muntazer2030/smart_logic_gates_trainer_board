#include <cJSON.h>
#include "string.h"
#include "esp_log.h"
#include "gpios_manager.h"
#include "global_variables.h"
#include "time_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_manager_api.h"
#include "wifi_switch_manager.h"
#include "esp_system.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc.h"
#include "esp_rom_sys.h"
#define TAG "COMMANDS"



// Jump to ROM download mode (UART/USB)
void reboot_to_download_mode(void)
{
    // Tell RTC to boot into download mode instead of normal firmware
    REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);

    // Trigger a software reset
    esp_rom_software_reset_system();

    // Just in case, infinite loop until reset happens
    while (true) {}
}
void schedule_switch_state(int switch_number, char *time_from_to, char *end_date, int *days, int repeat)
{

    if (switch_task_schedule_handlers == NULL || switch_number < 0 || switch_number >= switches_count)
    {
        ESP_LOGE("SCHEDULE", "Invalid switch number: %d", switch_number);
        return;
    }

    // Delete previous task if running
    if (switch_task_schedule_handlers[switch_number - 1] != NULL)
    {
        vTaskDelete(switch_task_schedule_handlers[switch_number - 1]);
        switch_task_schedule_handlers[switch_number - 1] = NULL;
        ESP_LOGI("SCHEDULE", "Previous task for switch %d deleted", switch_number);
    }

    // Allocate memory for task parameters
    schedule_timer_task_config_t *params = (schedule_timer_task_config_t *)malloc(sizeof(schedule_timer_task_config_t));
    if (!params)
    {
        ESP_LOGE("SCHEDULE", "Memory allocation failed!");
        return;
    }

    params->switch_number = switch_number;
    params->repeat = repeat;
    parse_time_range(time_from_to, &params->start_hour, &params->start_min, &params->stop_hour, &params->stop_min);
    strncpy(params->end_date, end_date, sizeof(params->end_date) - 1);
    params->end_date[sizeof(params->end_date) - 1] = '\0';
    memcpy(params->days, days, sizeof(params->days));

    memcpy(switch_configs[switch_number - 1].days, days, sizeof(switch_configs[switch_number - 1].days));
    switch_configs[switch_number - 1].repeat = repeat;
    switch_configs[switch_number - 1].schedule = 1;
    switch_configs[switch_number - 1].time_from_to = malloc(strlen(time_from_to) + 1);
    if (switch_configs[switch_number - 1].time_from_to == NULL)
    {
        printf("Memory allocation failed for time_from_to\n");
        return;
    }
    strncpy(switch_configs[switch_number - 1].time_from_to, time_from_to, strlen(time_from_to) + 1);

    switch_configs[switch_number - 1].end_date = malloc(strlen(end_date) + 1);
    if (switch_configs[switch_number - 1].end_date == NULL)
    {
        printf("Memory allocation failed for end_date\n");
        return;
    }
    strncpy(switch_configs[switch_number - 1].end_date, end_date, strlen(end_date) + 1);

    // Create task and store handler
    if (xTaskCreate(schedule_timer_task, "schedule_timer_task", 4096, params, 5, &switch_task_schedule_handlers[switch_number - 1]) == pdPASS)
    {
        ESP_LOGI("SCHEDULE", "Task created for switch %d", switch_number);
    }
    else
    {
        ESP_LOGE("SCHEDULE", "Failed to create task for switch %d", switch_number);
        free(params);
    }
}

void set_switch_timer(int switch_number, int time, int state)
{
    if (timer_task_handlers == NULL || switch_number < 0 || switch_number > switches_count)
    {
        ESP_LOGE("TIMER", "Invalid switch number: %d", switch_number);
        return;
    }

    // Delete previous timer task if running
    if (timer_task_handlers[switch_number - 1] != NULL)
    {
        vTaskDelete(timer_task_handlers[switch_number - 1]);
        timer_task_handlers[switch_number - 1] = NULL;
        ESP_LOGI("TIMER", "Previous timer task for switch %d deleted", switch_number);
    }

    // Allocate memory for task parameters
    timer_task_config_t *params = (timer_task_config_t *)malloc(sizeof(timer_task_config_t));
    if (!params)
    {
        ESP_LOGE("TIMER", "Memory allocation failed!");
        return;
    }

    params->switch_number = switch_number;
    params->time = time;
    params->state = state;

    // Create new task and store its handler
    if (xTaskCreate(start_timer, "start_timer", 4096, params, 5, &timer_task_handlers[switch_number - 1]) == pdPASS)
    {
        ESP_LOGI("TIMER", "Timer task created for switch %d", switch_number);
    }
    else
    {
        ESP_LOGE("TIMER", "Failed to create timer task for switch %d", switch_number);
        free(params);
    }
}
void set_switch_start_mode(int switch_number, int mode)
{
    switch_configs[switch_number - 1].start_mode = mode;
    save_switches_config(switch_number);
}

void set_indicator_mode(int mode)
{
    if (mode == 1 && indicator_mode != 1)
    {
        indicator_mode = 1;
        toggle_indicator_state();
    }
    else if (mode == 2 && indicator_mode != 2)
    {
        indicator_mode = 2;
        toggle_indicator_state();
    }

    else if (mode == 3 && indicator_mode != 3)
    {
        time_task_config_t *params = malloc(sizeof(time_task_config_t));
        parse_time_string("07-00-18-00", params);
        xTaskCreate(indicator_timer_task, "indicator_timer_task", 4096, params, 5, NULL);
        indicator_mode = 3;
    }

    switch_configs[0].indicator_mode = mode;
}

void set_security_mode(int mode)
{

    if (mode == 1)
    {
        stop_away_mode();
    }
    else if (mode == 2)
    {
        start_away_mode();
    }
    switch_configs[0].indicator_mode = mode;
}
// Function to start OTA update

void handle_command(const char *command, cJSON *content)
{
    if (!command || !content)
    {
        ESP_LOGE(TAG, "handle_command: Null command or content");
        return;
    }
    if(strcmp(command, "reboot_to_download_mode") == 0)
    {
        reboot_to_download_mode();
    }

    if (strcmp(command, "switch_state") == 0)
    {
        cJSON *switch_number = cJSON_GetObjectItem(content, "switch_number");
        cJSON *state = cJSON_GetObjectItem(content, "state");

        if (!switch_number || !cJSON_IsNumber(switch_number) ||
            !state || !cJSON_IsNumber(state))
        {
            ESP_LOGE(TAG, "Invalid JSON format for switch_state");
            return;
        }

        toggle_indicator_and_relay_state(switch_number->valueint, state->valueint);
    }
    if (strcmp(command, "turn_switches_state") == 0)
    {

        cJSON *state = cJSON_GetObjectItem(content, "state");

        if (!state || !cJSON_IsNumber(state))
        {
            ESP_LOGE(TAG, "Invalid JSON format for switch_state");
            return;
        }
        for (size_t i = 0; i < switches_count; i++)
        {
            toggle_indicator_and_relay_state(i + 1, state->valueint);
        }
    }

    else if (strcmp(command, "schedule_state") == 0)
    {
        cJSON *switch_number = cJSON_GetObjectItem(content, "switch_number");
        cJSON *is_on = cJSON_GetObjectItem(content, "is_on");
        cJSON *time_from_to = cJSON_GetObjectItem(content, "time_from_to");
        cJSON *end_date = cJSON_GetObjectItem(content, "end_date");
        cJSON *days = cJSON_GetObjectItem(content, "days");
        cJSON *repeat = cJSON_GetObjectItem(content, "repeat");

        if (!switch_number || !cJSON_IsNumber(switch_number) ||
            !time_from_to || !cJSON_IsString(time_from_to) ||
            !end_date || !cJSON_IsString(end_date) ||
            !days || !cJSON_IsArray(days) ||
            !repeat || !cJSON_IsNumber(repeat) ||
            !is_on || !cJSON_IsNumber(is_on))
        {
            ESP_LOGE(TAG, "Invalid JSON format for schedule_state");
            return;
        }

        int days_count = cJSON_GetArraySize(days);
        if (days_count <= 0)
        {
            ESP_LOGE(TAG, "schedule_state: Empty or invalid days array");
            return;
        }

        int *days_array = malloc(days_count * sizeof(int));
        if (!days_array)
        {
            ESP_LOGE(TAG, "Memory allocation failed for days array");
            return;
        }

        for (int i = 0; i < days_count; i++)
        {
            cJSON *day_item = cJSON_GetArrayItem(days, i);
            if (!cJSON_IsNumber(day_item))
            {
                ESP_LOGE(TAG, "Invalid day value in JSON array");
                free(days_array);
                return;
            }
            days_array[i] = day_item->valueint;
        }

        schedule_switch_state(
            switch_number->valueint,
            time_from_to->valuestring,
            end_date->valuestring,
            days_array,
            repeat->valueint);

        free(days_array);
        save_switches_config(switch_number->valueint);
    }

    else if (strcmp(command, "set_timer") == 0)
    {
        cJSON *switch_number = cJSON_GetObjectItem(content, "switch_number");
        cJSON *time = cJSON_GetObjectItem(content, "time");
        cJSON *state = cJSON_GetObjectItem(content, "state");

        if (!switch_number || !cJSON_IsNumber(switch_number) ||
            !time || !cJSON_IsNumber(time) ||
            !state || !cJSON_IsNumber(state))
        {
            ESP_LOGE(TAG, "Invalid JSON format for set_timer");
            return;
        }

        set_switch_timer(switch_number->valueint, time->valueint, state->valueint);
    }

    else if (strcmp(command, "switch_start_mode") == 0)
    {
        cJSON *switch_number = cJSON_GetObjectItem(content, "switch_number");
        cJSON *mode = cJSON_GetObjectItem(content, "mode");

        if (!switch_number || !cJSON_IsNumber(switch_number) ||
            !mode || !cJSON_IsNumber(mode))
        {
            ESP_LOGE(TAG, "Invalid JSON format for switch_start_mode");
            return;
        }

        // Explanation:
        // Mode 0 → onLastState = true, onStart = false
        // Mode 1 → onLastState = false, onStart = true
        // Mode 2 → onLastState = false, onStart = false

        set_switch_start_mode(switch_number->valueint, mode->valueint);
        save_switches_config(switch_number->valueint);
    }

    else if (strcmp(command, "set_indicator_mode") == 0)
    {
        cJSON *mode = cJSON_GetObjectItem(content, "mode");

        if (!mode || !cJSON_IsNumber(mode))
        {
            ESP_LOGE(TAG, "Invalid JSON format for set_indicator_mode");
            return;
        }

        set_indicator_mode(mode->valueint);
        save_switches_config(1);
    }

    else if (strcmp(command, "set_security_mode") == 0)
    {
        cJSON *mode = cJSON_GetObjectItem(content, "mode");

        if (!mode || !cJSON_IsNumber(mode))
        {
            ESP_LOGE(TAG, "Invalid JSON format for set_security_mode");
            return;
        }

        set_security_mode(mode->valueint);
        save_switches_config(1);
    }
    else if (strcmp(command, "get_state") == 0)
    {
        send_gangs_property_to_mqtt();
    }
    else if (strcmp(command, "ota") == 0)
    {
        go_to_factory_app_to_start_update();
    }
    else if (strcmp(command, "factory_reset") == 0)
    {
        factory_reset();
    }
    else if (strcmp(command, "restart") == 0)
    {
        esp_restart();
    }

    else
    {
        ESP_LOGW(TAG, "Unknown command: %s", command);
    }
}
