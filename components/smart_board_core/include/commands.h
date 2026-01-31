#pragma once
#include <cJSON.h>

void handle_command(const char *command, cJSON *content);
void initialize_all_ports();