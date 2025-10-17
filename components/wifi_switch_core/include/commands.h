#pragma once
#include <cJSON.h>

void handle_command(const char *command, cJSON *content);
void schedule_switch_state(int switch_number, char *time_from_to, char *end_date, int *days, int repeat);
void set_switch_timer(int switch_number, int time, int state);
void set_switch_start_mode(int switch_number, int mode);
void set_indicator_mode(int mode);
void set_security_mode(int mode);