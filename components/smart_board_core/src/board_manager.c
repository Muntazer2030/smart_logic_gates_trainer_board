#include "esp_log.h"
#include "esp_mac.h"
#include "mqtt_manager.h"
#include "global_variables.h"
#include "cJSON.h"
#include "commands.h"
#include "json_encode_decode.h"
#include "gpios_manager.h"
#include "nvs_api.h"
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <ctype.h>   // isalnum, for bench id validation

#include "board_manager.h"
#define TAG "BOARD_MANAGER"

char *cert = NULL;
char *key = NULL;

/*
 * Every board needs its own identity. It used to be the literal "BOARD_001"
 * compiled into the firmware, which meant a second board published to the
 * first one's topics and the two fought over the same MQTT client id.
 *
 * The id is read from NVS if it was provisioned, otherwise it is derived from
 * the chip's own MAC address, which is unique per device.
 */
static char board_uid[32] = {0};
char *uuid = board_uid;

const char *board_identity(void)
{
    if (board_uid[0] != '\0')
    {
        return board_uid;
    }

    // 1. A name provisioned into NVS wins, so a bench can be labelled.
    char stored[32] = {0};
    size_t length = sizeof(stored);
    if (read_str_from_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_BOARD_UID, stored, &length) == ESP_OK &&
        stored[0] != '\0')
    {
        strncpy(board_uid, stored, sizeof(board_uid) - 1);
        board_uid[sizeof(board_uid) - 1] = '\0';
        ESP_LOGI(TAG, "Board id from NVS: %s", board_uid);
        return board_uid;
    }

    // 2. Otherwise derive a stable unique id from the MAC address.
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK)
    {
        snprintf(board_uid, sizeof(board_uid), "BOARD_%02X%02X%02X", mac[3], mac[4], mac[5]);
    }
    else
    {
        snprintf(board_uid, sizeof(board_uid), "BOARD_001");
    }
    ESP_LOGI(TAG, "Board id derived from MAC: %s", board_uid);
    return board_uid;
}

esp_err_t board_set_identity(const char *new_uid)
{
    // "LOGIC_" + id is the BLE name while advertising, and BLE names stop at
    // 29 characters: a longer id would be advertised cut short.
    if (!new_uid || new_uid[0] == '\0' || strlen(new_uid) > 23)
    {
        return ESP_ERR_INVALID_ARG;
    }
    // The id becomes part of every MQTT topic, where '/', '+', '#' or a
    // space would change the topic's meaning, so only plain characters.
    for (const char *c = new_uid; *c; c++)
    {
        if (!isalnum((unsigned char)*c) && *c != '_' && *c != '-')
        {
            return ESP_ERR_INVALID_ARG;
        }
    }
    esp_err_t err = write_str_to_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_BOARD_UID, new_uid);
    if (err == ESP_OK)
    {
        strncpy(board_uid, new_uid, sizeof(board_uid) - 1);
        board_uid[sizeof(board_uid) - 1] = '\0';
    }
    return err;
}

bool board_broker_is_usable(const char *host)
{
    // A loopback or "any" address means the machine itself. It is right for
    // BoardMaster talking to a lab host on the same PC, and useless to a
    // board, which would just try to reach itself.
    if (!host || host[0] == '\0')
    {
        return false;
    }
    return strncmp(host, "127.", 4) != 0 && strcmp(host, "localhost") != 0 &&
           strcmp(host, "0.0.0.0") != 0 && strcmp(host, "::1") != 0;
}

void message_handler(const char *topic, const char *payload)
{
    ESP_LOGI(TAG, "Message on %s", topic);

    char command[32] = {0};
    char request_id[64] = {0};
    cJSON *content = NULL;

    decode_message(payload, command, sizeof(command), &content,
                   request_id, sizeof(request_id));

    if (command[0] != '\0')
    {
        handle_command(command, content, request_id);
    }

    // decode_message duplicates the content, so it has to be released here.
    if (content)
    {
        cJSON_Delete(content);
    }
}

void init_mqtt(void)
{
    const char *id = board_identity();

    mqtt_set_message_callback(message_handler);
    mqtt_set_connect_callback(subscribe_to_topics);

    char status_topic[96];
    snprintf(status_topic, sizeof(status_topic), "MTU/%s/device/status", id);

    // Broker address comes from provisioning, falling back to the public
    // broker when this board has never been pointed at a lab PC.
    static char broker_host[96];
    size_t host_len = sizeof(broker_host);
    broker_host[0] = '\0';
    read_str_from_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_MQTT_HOST, broker_host, &host_len);
    if (broker_host[0] && !board_broker_is_usable(broker_host))
    {
        // Stored by an older BoardMaster that passed on its own loopback
        // address. The lab host is found again by discovery instead.
        ESP_LOGW(TAG, "Ignoring stored broker '%s': that address means this board itself",
                 broker_host);
        broker_host[0] = '\0';
    }

    // Stored as text: the NVS helpers only handle int8, and 1883 does not fit.
    int broker_port = 1883;
    char port_text[8] = {0};
    size_t port_len = sizeof(port_text);
    if (read_str_from_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_MQTT_PORT, port_text, &port_len) == ESP_OK &&
        atoi(port_text) > 0)
    {
        broker_port = atoi(port_text);
    }

    mqtt_app_start(cert, key, id, status_topic, broker_host, broker_port);
}

void subscribe_to_topics(void)
{
    const char *id = board_identity();
    char topic[96] = {0};

    snprintf(topic, sizeof(topic), "MTU/%s/command", id);
    ESP_LOGI(TAG, "Subscribing to %s", topic);
    mqtt_subscribe(topic, 1);

    // Lab-wide commands (restart everything, self test everything).
    mqtt_subscribe("MTU/ALL/command", 1);
}
