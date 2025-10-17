#include "time_manager.h"
#include "global_variables.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "esp_random.h"
#include "gpios_manager.h"
#include "sdkconfig.h"
#include "wifi_switch_manager.h"
#include "global_variables.h"
#include "nvs_manager_api.h"
#define TAG "GPIOS TIMER CONTROL"

int stop_timer = 0;

// this is the timer task it handle timer
void schedule_timer_task(void *pvParameters)
{

    schedule_timer_task_config_t *params = (schedule_timer_task_config_t *)pvParameters;

    int daysCount = sizeof(params->days) / sizeof(params->days[0]);
    int days[7] = {0};
    memcpy(days, params->days, sizeof(days));

    int repeat = params->repeat;

    char end_date[11];
    memcpy(end_date, params->end_date, sizeof(end_date));

    int set_state = 0;
    int old_state = 0;
    while (1)
    {
        if (!stop_timer)
        {

            struct tm local_time = get_local_time();
            int current_year = local_time.tm_year + 1900;
            int current_day = local_time.tm_wday;
            int current_hour = local_time.tm_hour;
            int current_minute = local_time.tm_min;

            ESP_LOGI(TAG, "current_year: %d, current_day: %d, current_hour: %d, current_minute: %d", current_year, current_day, current_hour, current_minute);
            ESP_LOGI(TAG, "start_hour: %d, start_min: %d, stop_hour: %d, stop_min: %d", params->start_hour, params->start_min, params->stop_hour, params->stop_min);
            // Check if current day is valid
            bool is_day_valid = false;
            for (int i = 0; i < daysCount; i++)
            {

                if (days[i] == current_day)
                {
                    is_day_valid = true;

                    break;
                }
                else
                {
                    is_day_valid = false;
                }
            }

            bool is_time_valid = false;
            if ((current_hour > params->start_hour || (current_hour == params->start_hour && current_minute >= params->start_min)) &&
                (current_hour < params->stop_hour || (current_hour == params->stop_hour && current_minute < params->stop_min)))
            {

                is_time_valid = true;
            }

            if (is_day_valid && is_time_valid)
            {
                // If the day or time is valid, turn on the indicator
                set_state = 1;
            }
            else
            {
                // If the day or time is not valid, turn off the indicator
                set_state = 0;
            }

            if (repeat == 0)
            {
                int end_year, end_month, end_day;
                if (sscanf(end_date, "%d-%d-%d", &end_year, &end_month, &end_day) == 3)
                {

                    if (current_year > end_year ||
                        (current_year == end_year && local_time.tm_mon + 1 > end_month) ||
                        (current_year == end_year && local_time.tm_mon + 1 == end_month && local_time.tm_mday > end_day))
                    {
                        set_state = 0;

                        if (set_state != old_state)
                        {
                            old_state = set_state;
                            toggle_indicator_and_relay_state(params->switch_number, set_state);
                            send_gangs_property_to_mqtt();
                            
                        }

                        break;
                    }
                }
            }
            if (set_state != old_state)
            {
                old_state = set_state;
                toggle_indicator_and_relay_state(params->switch_number, set_state);

                send_gangs_property_to_mqtt();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    switch_configs[params->switch_number - 1].schedule = 0;

    save_switches_config(params->switch_number);
    vTaskDelete(NULL);
}



bool is_within_time_range(time_task_config_t *config)
{

    struct tm local_time = get_local_time();

    int current_minutes = local_time.tm_hour * 60 + local_time.tm_min;
    int start_minutes = config->start_hour * 60 + config->start_min;
    int stop_minutes = config->stop_hour * 60 + config->stop_min;

    return (current_minutes >= start_minutes && current_minutes < stop_minutes);
}

void indicator_timer_task(void *arg)
{
    time_task_config_t *config = (time_task_config_t *)arg; // Get the passed struct
    while (1)
    {
        if (indicator_mode != 3)
        {
            break;
        }
        if (is_within_time_range(config))
        {
            ESP_LOGI(TAG, "Indicator running within scheduled time: %02d:%02d - %02d:%02d",
                     config->start_hour, config->start_min,
                     config->stop_hour, config->stop_min);
            indicator_mode = 2;
            toggle_indicator_state();
        }
        else
        {
            ESP_LOGI(TAG, "Indicator task is outside the scheduled time.");
            indicator_mode = 1;
            toggle_indicator_state();
        }
        vTaskDelay(pdMS_TO_TICKS(60000)); // Check every minute
    }

    vTaskDelete(NULL);
}
void start_timer(void *pvParameters)
{
    timer_task_config_t *arg = (timer_task_config_t *)pvParameters;

    int new_state = 0;

    // Store the initial state
    int current_state = arg->state;

    // Turn on the switch
    toggle_indicator_and_relay_state(arg->switch_number, current_state);
    printf("Timer started for SWITCH%d, State: %d,timer: %d\n", arg->switch_number, current_state, arg->time);
    // send_gpios_status_to_mqtt();
    //  Wait for the specified time
    vTaskDelay(pdMS_TO_TICKS((arg->time * 1000)));

    // Toggle to the opposite state
    new_state = (current_state == 0) ? 1 : 0;
    toggle_indicator_and_relay_state(arg->switch_number, new_state);
    printf("Timer completed for SWITCH%d, State: %d\n", arg->switch_number, new_state);
    send_gangs_property_to_mqtt();
    timer_task_handlers[arg->switch_number - 1] = NULL;
    vTaskDelete(NULL);
}

static bool away_mode_active = false;
static TaskHandle_t away_mode_task_handle = NULL;

void away_mode_task(void *pvParameters)
{
    while (away_mode_active)
    {
        int random_delay = CONFIG_MIN_DELAY_AWAY_MODE + (esp_random() % (CONFIG_MAX_DELAY_AWAY_MODE - CONFIG_MIN_DELAY_AWAY_MODE));
        int random_switch = (esp_random() % switches_count) + 1;
        toggle_indicator_and_relay_state(random_switch, 1);
        vTaskDelay(pdMS_TO_TICKS(random_delay)); // Keep light ON for a random time

        random_delay = CONFIG_MIN_DELAY_AWAY_MODE + (esp_random() % (CONFIG_MAX_DELAY_AWAY_MODE - CONFIG_MIN_DELAY_AWAY_MODE));
        toggle_indicator_and_relay_state(random_switch, 0);
        vTaskDelay(pdMS_TO_TICKS(random_delay)); // Keep light OFF for a random time
    }

    vTaskDelete(NULL);
}

void start_away_mode()
{
    if (!away_mode_active)
    {
        away_mode_active = true;
        xTaskCreate(away_mode_task, "away_mode_task", 4096, NULL, 5, &away_mode_task_handle);
        ESP_LOGI("SECURITY", "Away mode activated.");
    }
}

void stop_away_mode()
{
    if (away_mode_active)
    {
        away_mode_active = false;
        if (away_mode_task_handle != NULL)
        {
            vTaskDelete(away_mode_task_handle);
            away_mode_task_handle = NULL;
        }

        ESP_LOGI("SECURITY", "Away mode deactivated.");
    }
}