
#include "wifi_manager.h"
#include "board_manager.h"
#include "nvs_manager.h"
#include "nvs_api.h"

char ssid[64] = "office";
char pass[64] = "99990000";


void app_main(void)
{
    init_nvs();
    set_wifi_connected_callback(init_mqtt);
    connect_to_wifi((uint8_t *)ssid, (uint8_t *)pass);
}
