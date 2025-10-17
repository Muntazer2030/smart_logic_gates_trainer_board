#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern char *cert;
extern char *key;
extern char *uuid;

extern int *switch_gpios;
extern int *indicator_gpios;
extern int *relay_gpios;

extern int *switches_states;

extern int indicator_mode;

extern int switches_count;

extern int time_initialized;
extern int stop_timer;







typedef struct
{

    int start_mode;
    int indicator_mode;
    int security_mode;
    int schedule;
    char *time_from_to;
    char *end_date;
    int days[7];
    int repeat;

} switch_configuration_t;

typedef struct
{
    int switch_number;
    int start_hour;
    int start_min;
    int stop_hour;
    int stop_min;
    int repeat;
    int days[7];
    char end_date[11];

} schedule_timer_task_config_t;

typedef struct
{
    int start_hour;
    int start_min;
    int stop_hour;
    int stop_min;
} time_task_config_t;

typedef struct
{
    int switch_number;
    int state;
    int time;
} timer_task_config_t;


extern switch_configuration_t *switch_configs;

extern TaskHandle_t *switch_task_schedule_handlers;
extern TaskHandle_t *timer_task_handlers;