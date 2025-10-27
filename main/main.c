
#include "wifi_manager.h"
#include "board_manager.h"
#include "nvs_manager.h"
#include "nvs_api.h"

char ssid[64] = "A2025";
char pass[64] = "A000000A";

int switches_count = 3;
int switches_pins[] = {0, 5, 6}, indecators_pins[] = {3, 18, 19}, relays_pin[] = {4, 10, 1};

void app_main(void)
{
    init_nvs();
    //init_device_data_nvs();

    //init_wifi_switches(switches_count, switches_pins, indecators_pins, relays_pin);

    //get_wifi_cred(ssid, pass);
    set_wifi_connected_callback(init_mqtt);
    connect_to_wifi((uint8_t *)ssid, (uint8_t *)pass);
}
