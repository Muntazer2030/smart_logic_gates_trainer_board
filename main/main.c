
#include "wifi_manager.h"
#include "board_manager.h"
#include "nvs_manager.h"
#include "nvs_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "commands.h"
char ssid[64] = "office";
char pass[64] = "99990000";

const int all_pins[] = {15, 2, 0, 4, 16, 17, 5, 18, 12, 14, 27, 26, 25, 33, 13,32};
void test_all_pins()
{
    printf("Initializing Pins...\n");

    // Initialize every pin one by one
    for (int i = 0; i < 16; i++)
    {
        gpio_reset_pin(all_pins[i]);
        gpio_set_direction(all_pins[i], GPIO_MODE_OUTPUT);
        // Turn them all OFF initially
        gpio_set_level(all_pins[i], 0);
    }

    for (int i = 0; i < 16; i++)
    {
        printf("Testing Pin: %d\n", all_pins[i]);

        gpio_set_level(all_pins[i], 1); // Turn ON
        vTaskDelay(pdMS_TO_TICKS(100)); 

        gpio_set_level(all_pins[i], 0); // Turn OFF
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    printf("Cycle complete. Restarting...\n");
}

void app_main(void)
{
    test_all_pins();
    init_nvs();
    initialize_all_ports();
    set_wifi_connected_callback(init_mqtt);
    connect_to_wifi((uint8_t *)ssid, (uint8_t *)pass);
}
