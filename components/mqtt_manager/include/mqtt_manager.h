#pragma once

#include <stdbool.h>
// Functions
void mqtt_app_start(const char *cert, const char *key, const char *uuid, const char *status_topic);
void mqtt_app_stop();
void mqtt_send_message(const char *topic, const char *message, int qos, int retain);
void mqtt_set_connect_callback(void (*callback)());
void mqtt_set_message_callback(void (*callback)(const char *, const char *));

bool mqtt_is_connected();
void mqtt_wait_for_connection();


void mqtt_subscribe(const char *topic, int qos);


extern const uint8_t server_cert_pem_start[] asm("_binary_rootca_crt_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_rootca_crt_end");