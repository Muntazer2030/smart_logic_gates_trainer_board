#pragma once
#include <cJSON.h>

void decode_message(const char *message, char *command, cJSON **content);
void encode_switch_configuration(switch_configuration_t *switch_config, char **switch_config_json);
void decode_switch_configuration(const char *switch_config_json, switch_configuration_t *switch_config);
void encode_switches_state(char **switches_state_json);