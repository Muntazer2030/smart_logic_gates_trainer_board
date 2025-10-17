#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "esp_sntp.h"
#include "esp_log.h"
#include <portmacro.h>
#include "global_variables.h"

static const char *TAG = "TIME_MANAGER";
int time_initialized = 0;

void initialize_time()
{
    if (time_initialized == 0)
    {
        ESP_LOGI(TAG, "Initializing SNTP...");
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_init();

        // Wait for time synchronization
        time_t now = 0;
        struct tm timeinfo = {0};

        time(&now);
        localtime_r(&now, &timeinfo);
        while (timeinfo.tm_year < (2023 - 1900))
        {
            ESP_LOGI(TAG, "Waiting for system time to be set...");

            vTaskDelay(1000 / portTICK_PERIOD_MS);
            time(&now);
            localtime_r(&now, &timeinfo);
        }

        if (timeinfo.tm_year < (2023 - 1900))
        {
            ESP_LOGE(TAG, "Failed to synchronize time");
        }
        else
        {
            // Set timezone (adjust as needed for your application)
            setenv("TZ", "UTC-3", 1);
            tzset();
            ESP_LOGI(TAG, "Time synchronized");
            time_initialized = true;
        }
    }
}
struct tm get_local_time(void)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo;
}

int get_time_as_string(char *buffer, size_t buffer_size, const char *format)
{
    struct tm timeinfo = get_local_time();

    if (strftime(buffer, buffer_size, format, &timeinfo) == 0)
    {
        return 0;
    }
    return 1;
}
int parse_time_range(const char *time_range, int *from_hour, int *from_minute, int *to_hour, int *to_minute)
{
    printf(time_range);
    if (sscanf(time_range, "%d:%d-%d:%d", from_hour, from_minute, to_hour, to_minute) != 4)
    {
        ESP_LOGE(TAG, "Invalid time range format. Expected 'HH:MM-HH:MM'.");
        return -1; // Indicating error
    }

    return 0; // Success
}
void parse_time_string(const char *time_str, time_task_config_t *config)
{
    if (sscanf(time_str, "%d-%d-%d-%d",
               &config->start_hour, &config->start_min,
               &config->stop_hour, &config->stop_min) == 4)
    {
        printf("Parsed Time - Start: %02d:%02d, Stop: %02d:%02d\n",
               config->start_hour, config->start_min,
               config->stop_hour, config->stop_min);
    }
    else
    {
        printf("Error: Invalid time format\n");
    }
}