#include "global_variables.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "esp_timer.h"
#include "esp_log.h"

#define TAG "GPIOS_MANAGER"

void set_input_port_state(int input_port, int state)
{
    gpio_set_level(input_port, state);
}

void get_input_port_state(int input_port, int *state)
{
    *state = gpio_get_level(input_port);
}

bool check_output_port(int output_port, int state)
{
    int current_state = gpio_get_level(output_port);
    return current_state == state;
}


void init_ports(int port)
{
    gpio_set_direction(port, GPIO_MODE_INPUT);
}
void init_input_port(int port)
{

    gpio_reset_pin(port);
    gpio_set_direction(port, GPIO_MODE_OUTPUT);
    gpio_set_level(port, 0);
}
