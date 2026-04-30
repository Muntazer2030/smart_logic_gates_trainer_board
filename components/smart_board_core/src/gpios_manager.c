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


// This function initializes pins meant to READ signals
void init_ports(int port)
{
    gpio_reset_pin(port);
    gpio_set_direction(port, GPIO_MODE_INPUT);

    // GPIO 34-39 do not support internal pull-up/pull-down
    if (port < 34) {
        gpio_set_pull_mode(port, GPIO_PULLDOWN_ONLY);
    } else {
        ESP_LOGW("GPIO", "Pin %d does not support internal pull-down. External resistor required.", port);
    }
}

// This function initializes pins meant to SEND signals (Outputs)
void init_input_port(int port)
{
    gpio_reset_pin(port);
    gpio_set_direction(port, GPIO_MODE_OUTPUT);
    gpio_set_level(port, 0); // Start at 0V
    // Note: Pull-ups/downs are usually disabled for outputs
    gpio_set_pull_mode(port, GPIO_PULLDOWN_ONLY); 
}




