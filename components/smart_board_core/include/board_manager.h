#pragma once
#include <esp_err.h>
#include <stdbool.h>

/** Namespace and keys used for board provisioning in NVS. */
#define NVS_BOARD_NAMESPACE "board"
#define NVS_KEY_BOARD_UID   "uid"
#define NVS_KEY_WIFI_SSID   "ssid"
#define NVS_KEY_WIFI_PASS   "pass"
#define NVS_KEY_MQTT_HOST   "mqtt_host"
#define NVS_KEY_MQTT_PORT   "mqtt_port"
/* The previous Wi-Fi, kept while a BLE setup window is open (ble_provisioning.c) */
#define NVS_KEY_WIFI_BACKUP_SSID "bk_ssid"
#define NVS_KEY_WIFI_BACKUP_PASS "bk_pass"

/**
 * @brief This board's unique id, e.g. "BOARD_001".
 *
 * Read from NVS when provisioned, otherwise derived from the chip's MAC so
 * two boards never share an identity.
 */
const char *board_identity(void);

/** Stores a new board id in NVS (used for bench labelling). */
esp_err_t board_set_identity(const char *new_uid);

/** False for addresses that mean "this machine" (127.x, localhost, ...):
 *  meaningful on the lab PC, useless as a broker address for a board. */
bool board_broker_is_usable(const char *host);

void init_mqtt(void);
void subscribe_to_topics(void);
