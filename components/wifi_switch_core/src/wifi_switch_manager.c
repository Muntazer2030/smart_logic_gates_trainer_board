
#include "esp_log.h"
#include "mqtt_manager.h"
#include "nvs_manager.h"
#include "nvs_manager_api.h"
#include "global_variables.h"
#include "cJSON.h"
#include "commands.h"
#include "json_encode_decode.h"
#include "gpios_manager.h"
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "time_manager.h"
#include "esp_log.h"
#include "wifi_switch_manager.h"
#define TAG "WIFI_SWITCH_CONTROLLER"

char *cert = NULL;
char *key = NULL;
char *uuid = NULL;
int *switch_gpios = NULL;
int *indicator_gpios = NULL;
int *relay_gpios = NULL;
int *switches_states = NULL;

TaskHandle_t *switch_task_schedule_handlers = NULL;
TaskHandle_t *timer_task_handlers = NULL;

switch_configuration_t *switch_configs = NULL;

void message_handler(const char *topic, const char *payload)
{
    ESP_LOGI(TAG, "Message received: Topic: %s, Payload: %s", topic, payload);
     char topic_and_payload[100] = {0}; // Buffer to hold topic and payload
    snprintf(topic_and_payload, sizeof(topic_and_payload), "%s: %s", topic, payload);
    mqtt_send_message("masseges", topic_and_payload, 1, 0); // Echo the message back
    char command[30] = {0};
    cJSON *content = NULL;

    decode_message(payload, command, &content);

    handle_command(command, content);
}

void initialize_switch_task_handlers(int count)
{
    if (switch_task_schedule_handlers != NULL)
    {
        free(switch_task_schedule_handlers); // Free previous allocation if needed
    }
    switch_task_schedule_handlers = (TaskHandle_t *)calloc(count, sizeof(TaskHandle_t));
    if (!switch_task_schedule_handlers)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for task handlers");
    }
}
void initialize_timer_task_handlers(int count)
{
    if (timer_task_handlers != NULL)
    {
        free(timer_task_handlers); // Free previous allocation if needed
    }
    timer_task_handlers = (TaskHandle_t *)calloc(count, sizeof(TaskHandle_t));
    if (!timer_task_handlers)
    {
        ESP_LOGE("INIT", "Failed to allocate memory for timer task handlers");
    }
}

void load_switches_configs(void)
{
    char switch_config[250]; // No need to initialize here, clearing it inside the loop

    for (size_t i = 0; i < switches_count; i++)
    {
        memset(switch_config, 0, sizeof(switch_config)); // Reset buffer before reading

        if (get_saved_switch_config(i + 1, switch_config) != ESP_OK) // Handle read failure
        {
            ESP_LOGW("CONFIG", "Failed to read switch config for switch %d, using defaults", i + 1);
            memset(&switch_configs[i], 0, sizeof(switch_configuration_t)); // Reset to default config
        }

        decode_switch_configuration(switch_config, &switch_configs[i]);

        // If scheduling is enabled, configure the switch schedule
        if (switch_configs[i].schedule == 1)
        {
            ESP_LOGI(TAG, "End date: %s", switch_configs[i].end_date);
            ESP_LOGI(TAG, "time_from_to: %s", switch_configs[i].time_from_to);
            schedule_switch_state(i + 1, switch_configs[i].time_from_to,
                                  switch_configs[i].end_date, switch_configs[i].days,
                                  switch_configs[i].repeat);
        }
        else
        {
            // Handle start mode
            int initial_state = 0;
            switch (switch_configs[i].start_mode)
            {
            case 0:
                gpio_get_saved_state(i + 1, &initial_state);
                break;
            case 1:
                initial_state = 1;
                break;
            case 2:
                initial_state = 0;
                break;
            default:
                ESP_LOGW("CONFIG", "Invalid start_mode %d for switch %d", switch_configs[i].start_mode, i + 1);
                continue;
            }

            // Apply global settings for the first switch
            if (i == 0)
            {
                set_security_mode(switch_configs[i].security_mode);
                set_indicator_mode(switch_configs[i].indicator_mode);
            }
        }
    }
}

void init_mqtt(void)
{
    mqtt_set_message_callback(message_handler);
    mqtt_set_connect_callback(subscribe_to_topics);
    get_device_data(cert, key, uuid); // reading the saved data from device data partition
    const char status_topic[100];
    snprintf(status_topic, sizeof(status_topic), "ASWAR/%s/device/status", uuid);
    mqtt_app_start(cert, key, uuid,status_topic);
    initialize_time();
}
void init_wifi_switches(int switches_count, int switches_pins[], int indicators_pins[], int relays_pins[])
{

    // Allocate memory for global arrays
    switch_gpios = malloc(switches_count * sizeof(int));
    indicator_gpios = malloc(switches_count * sizeof(int));
    relay_gpios = malloc(switches_count * sizeof(int));
    switches_states = malloc(sizeof(int) * switches_count);
    switches_count = switches_count;
    if (switch_gpios == NULL || indicator_gpios == NULL || relay_gpios == NULL)
    {
        ESP_LOGE(TAG, "Memory allocation failed");
        free(switch_gpios);
        free(indicator_gpios);
        free(relay_gpios);
        return;
    }
    for (size_t i = 0; i < switches_count; i++)
    {

        switch_gpios[i] = switches_pins[i];
        indicator_gpios[i] = indicators_pins[i];
        relay_gpios[i] = relays_pins[i];
        switches_states[i] = 0;
        init_gpios(switches_pins[i], indicators_pins[i], relays_pins[i]);
    }
    switch_configs = calloc(switches_count, sizeof(switch_configuration_t));

    initialize_switch_task_handlers(switches_count);
    initialize_timer_task_handlers(switches_count);

    cert = malloc(2048);
    key = malloc(2048);
    uuid = malloc(64);
    if (!cert || !key || !uuid)
    {
        ESP_LOGE(TAG, "Memory allocation failed");
        free(cert);
        free(key);
        free(uuid);

        return;
    }
    init_switches_task();
    load_switches_configs();
}

// Function to send status to MQTT
void send_gangs_property_to_mqtt()
{
    char property_topic[100] = {0};
    snprintf(property_topic, sizeof(property_topic), "ASWAR/%s/property/get/gangX", uuid);

    char *switches_state_json = NULL;
    encode_switches_state(&switches_state_json);

    if (switches_state_json != NULL) {
        mqtt_send_message(switches_state_json, property_topic, 1, 0);
        free(switches_state_json);
    }
}

void subscribe_to_topics()
{
    char topic[100] = {0};

    snprintf(topic, sizeof(topic), "ASWAR/%s/update/ota", uuid);
    mqtt_subscribe(topic, 1); 

    snprintf(topic, sizeof(topic), "ASWAR/%s/property/set/time", uuid);
    mqtt_subscribe(topic, 1);

    snprintf(topic, sizeof(topic), "ASWAR/%s/device/factory_reset", uuid);
    mqtt_subscribe(topic, 1); 

    snprintf(topic, sizeof(topic), "ASWAR/%s/device/restart", uuid);
    mqtt_subscribe(topic, 1);

    // Function topics
    for (int i = 1; i <= 3; i++) {
        snprintf(topic, sizeof(topic), "ASWAR/%s/property/function/set/gang%d", uuid, i);
        mqtt_subscribe(topic, 1);
    }

    // Timer topics
    for (int i = 1; i <= switches_count; i++) {
        snprintf(topic, sizeof(topic), "ASWAR/%s/property/timer/set/gang%d", uuid, i);
        mqtt_subscribe(topic, 1);
    }

    // Schedule topics
    for (int i = 1; i <= switches_count; i++) {
        snprintf(topic, sizeof(topic), "ASWAR/%s/property/schedule/set/gang%d", uuid, i);
        mqtt_subscribe(topic, 1);
    }

    // Action topics
    for (int i = 1; i <= switches_count; i++) {
        snprintf(topic, sizeof(topic), "ASWAR/%s/property/action/set/gang%d", uuid, i);
        mqtt_subscribe(topic, 1);
    }
}
