#pragma once
#include <cJSON.h>

void decode_message(const char *message, char *command, cJSON **content);
