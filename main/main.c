#include "ble_provisioning.h"
#include "board_manager.h"
#include "commands.h"
#include "esp_log.h"
#include "nvs_api.h"   // init_nvs
#include "nvs_manager.h"
#include "provisioning.h"

#define TAG "MAIN"

/*
 * Start-up order matters here:
 *
 *  1. NVS, because the board id, Wi-Fi and broker address all live there.
 *  2. Ports to a known state, before anything can drive the student's circuit.
 *  3. The serial console, so a board can always be rescued over USB.
 *  4. Wi-Fi via BLE provisioning: a fresh board advertises as LOGIC_<id> and
 *     waits for a phone; a provisioned one just connects and starts MQTT.
 */
void app_main(void)
{
    init_nvs();

    ESP_LOGI(TAG, "Smart Logic Gates trainer board starting, id %s", board_identity());

    // The old build walked all 16 pins on every boot, which drove whatever
    // circuit the student had wired before anyone asked for a test. Run
    // "self_test" from the teacher app when you actually want that.
    initialize_all_ports();

    // Always available over USB, even if BLE or Wi-Fi provisioning went wrong.
    provisioning_start();

    ble_provisioning_set_connected_callback(init_mqtt);
    ble_provisioning_start();
}
