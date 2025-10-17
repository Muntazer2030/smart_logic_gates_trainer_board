#pragma once
#include <stddef.h>

void get_wifi_cred(char *ssid, char *pass);
void set_wifi_cred(char *ssid, char *pass);
void get_device_data(char *cert, char *key, char *uuid);
void set_device_data(const char *cert, const char *key, const char *uuid);

