
#include "esp_log.h"
#include "mqtt_manager.h"
#include "global_variables.h"
#include "cJSON.h"
#include "commands.h"
#include "json_encode_decode.h"
#include "gpios_manager.h"
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "esp_log.h"
#include "board_manager.h"
#define TAG "WIFI_SWITCH_CONTROLLER"

char *cert = NULL;
char *key = NULL;
char *uuid = "UUID_NOT_SET";

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

void init_mqtt(void)
{
    mqtt_set_message_callback(message_handler);
    mqtt_set_connect_callback(subscribe_to_topics);
    // get_device_data(cert, key, uuid); // reading the saved data from device data partition
    char status_topic[100];
    snprintf(status_topic, sizeof(status_topic), "MTU/%s/device/status", uuid);
    mqtt_app_start(cert, key, uuid, status_topic);
}

void subscribe_to_topics()
{
    char topic[100] = {0};

    snprintf(topic, sizeof(topic), "MTU/%s/command", uuid);
    printf("Subscribing to topic: %s\n", topic);
    mqtt_subscribe(topic, 1);
}
