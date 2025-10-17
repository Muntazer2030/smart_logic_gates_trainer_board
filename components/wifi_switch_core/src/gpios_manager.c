#include "global_variables.h"
#include "driver/gpio.h"
#include "nvs_manager_api.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "esp_timer.h"
#include "time_manager.h"
#include "wifi_switch_manager.h"
#include "nvs_manager_api.h"
#include "esp_log.h"
#include "nvs_manager_api.h"
#define TAG "GPIOS_MANAGER"

int indicator_mode = 0;

void toggle_indicator_and_relay_state(int switch_number, int state)
{
    int currentRelayState = switches_states[switch_number - 1]; // Get current relay state
    if (currentRelayState == state)
    {

        return;
    }

    int indicatorState_level = (indicator_mode == 2) ? !state : state;
    switches_states[switch_number - 1] = state;
    gpio_set_level(indicator_gpios[switch_number - 1], indicatorState_level);
    gpio_set_level(relay_gpios[switch_number - 1], state);
    gpio_save_state(switch_number, state);

    ESP_LOGI("TOGGLE", "Switch %d changed to state %d.", switch_number, state);
}

void toggle_indicator_state()
{
    for (size_t i = 0; i < switches_count; i++)
    {
        int indicator_state = switches_states[i];
        if (indicator_mode == 2)
        {
            indicator_state = !switches_states[i];
        }
        gpio_set_level(indicator_gpios[i], indicator_state);
    }
}

// switch task to handle the toggling of the switches (physically)
void switch_task()
{
    bool switch_pressed[switches_count];
    bool long_press_detected[switches_count];
    uint64_t press_time[switches_count];

    memset(switch_pressed, false, switches_count * sizeof(bool));
    memset(long_press_detected, false, switches_count * sizeof(bool));
    memset(press_time, 0, switches_count * sizeof(uint64_t));

    ESP_LOGI("SWITCH_TASK", "Initialized switch task with %d switches", switches_count);

    while (1)
    {
        for (int i = 0; i < switches_count; i++)
        {
            int level = gpio_get_level(switch_gpios[i]);

            if (level == 0 && !switch_pressed[i])
            {
                switch_pressed[i] = true;
                press_time[i] = esp_timer_get_time();
                ESP_LOGI("SWITCH_TASK", "Switch %d pressed at %llu us", i, press_time[i]);
            }

            if (switch_pressed[i]) // Check for long press while holding the button
            {
                uint64_t hold_time = (esp_timer_get_time() - press_time[i]) / 1000; // Convert to ms

                if (i == (CONFIG_REST_SWITCH - 1) && hold_time >= (CONFIG_TIME_FOR_REST_SWITCH * 1000) && !long_press_detected[i])
                {
                    ESP_LOGW("SWITCH_TASK", "Long press detected on switch %d! Restarting ESP...", i);
                    long_press_detected[i] = true;
                    factory_reset(); // esp_restart(); // Restart immediately, no need to wait for release
                }
            }

            if (level == 1 && switch_pressed[i]) // Normal press handling when switch is released
            {
                uint64_t hold_time = (esp_timer_get_time() - press_time[i]) / 1000; // Convert to ms
                ESP_LOGI("SWITCH_TASK", "Switch %d released after %llu ms", i, hold_time);

                if (i != (CONFIG_REST_SWITCH - 1) || hold_time < (CONFIG_TIME_FOR_REST_SWITCH * 1000))
                {
                    // switches_states[i] = !switches_states[i];
                    // stop_timer = !switches_states[i];

                    ESP_LOGI("SWITCH_TASK", "Switch %d state changed to %d", i, !switches_states[i]);
                    toggle_indicator_and_relay_state(i + 1, !switches_states[i]);

                    send_gangs_property_to_mqtt(); // Send the updated state to MQTT

                    ESP_LOGI("SWITCH_TASK", "MQTT update sent successfully for switch %d", i);
                }

                switch_pressed[i] = false;
                long_press_detected[i] = false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void init_gpios(int switch_gpio, int indicator_gpio, int relay_gpio)
{

    gpio_reset_pin(indicator_gpio);
    gpio_reset_pin(relay_gpio);
    gpio_set_direction(indicator_gpio, GPIO_MODE_OUTPUT);
    gpio_set_direction(relay_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(relay_gpio, 0);
    gpio_set_level(indicator_gpio, 0);
    gpio_set_direction(switch_gpio, GPIO_MODE_INPUT);
}
void init_switches_task()
{
    xTaskCreate(switch_task, "switch_task", 4096, NULL, 5, NULL); // a task to handle the switches when toggling on/off (physically)
}