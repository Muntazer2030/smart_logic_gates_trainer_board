
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
char *uuid = "BOARD_001";

void message_handler(const char *topic, const char *payload)
{
    ESP_LOGI(TAG, "Message received: Topic: %s, Payload: %s", topic, payload);
    
    char command[30] = {0};
    cJSON *content = NULL;

    decode_message(payload, command, &content);

    handle_command(command, content);
}

void init_mqtt(void)
{
    mqtt_set_message_callback(message_handler);
    mqtt_set_connect_callback(subscribe_to_topics);
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
