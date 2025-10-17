#pragma once

void init_gpios(int indicator_gpio, int relay_gpio, int switch_gpio);
void toggle_indicator_and_relay_state(int switchNum, int state);
void toggle_indicator_state();
void switch_task();
void init_switches_task();