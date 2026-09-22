#pragma once

/**
 * @brief Brings up Wi-Fi, provisioning over BLE first if nothing is stored.
 *
 * The board advertises as "LOGIC_<board id>". A phone supplies the Wi-Fi
 * credentials and, on the "lab-config" endpoint, a JSON blob with the lab PC's
 * address: {"broker":"192.168.1.50","port":1883,"id":"BOARD_003"}
 */
void ble_provisioning_start(void);

/** Called once the board has an IP address. */
void ble_provisioning_set_connected_callback(void (*callback)(void));

/** Forgets the stored Wi-Fi and restarts, so the board advertises again. */
void ble_provisioning_reset(void);
