#pragma once

#include "esp_err.h"

esp_err_t get_saved_switch_config(int switch_number, char *switch_config);
void gpio_get_saved_state(int switch_number, int *state);
void gpio_save_state(int switch_number, int state);
void save_switches_config(int switch_number);
void go_to_factory_app_to_start_update();
void factory_reset(void);