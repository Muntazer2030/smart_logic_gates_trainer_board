#pragma once
#include <time.h>
#include <stdbool.h>
#include "global_variables.h"
void initialize_time();
struct tm get_local_time(void);
int get_time_as_string(char *buffer, size_t buffer_size, const char *format);
int parse_time_range(const char *time_range, int *from_hour, int *from_minute, int *to_hour, int *to_minute);

void schedule_timer_task(void *pvParameters);
void parse_time_string(const char *time_str, time_task_config_t *config);
bool is_within_time_range(time_task_config_t *config);
void indicator_timer_task(void *arg);
void start_timer(void *pvParameters);
void away_mode_task(void *pvParameters);
void start_away_mode();
void stop_away_mode();