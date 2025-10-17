#pragma once
#include <esp_event_base.h>


void connect_to_wifi(uint8_t *ssid, uint8_t *pass);
void set_wifi_connected_callback(void (*callback));
void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);