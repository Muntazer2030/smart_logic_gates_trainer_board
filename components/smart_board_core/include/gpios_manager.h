#pragma once
#include "stdbool.h"
void set_input_port_state(int input_port, int state);

void get_input_port_state(int input_port, int *state);

bool check_output_port(int output_port, int state);

void init_ports(int port);
void init_input_port(int port);

