/*
 * BLE provisioning.
 *
 * A board with no stored Wi-Fi advertises itself over BLE as "LOGIC_<id>".
 * A phone sends the Wi-Fi credentials, plus a small JSON blob on a custom
 * endpoint carrying the lab PC's address:
 *
 *     {"broker":"192.168.1.50","port":1883,"id":"BOARD_003"}
 *
 * Once it has credentials the board connects and stores everything in NVS.
 *
 * A board whose stored Wi-Fi stops working (details left over from older
 * firmware, a changed router password, a new lab) must not be stuck until
 * someone plugs in a cable. With no Wi-Fi 20 s after starting, it restarts
 * into a timed setup window: it advertises over BLE for SETUP_WINDOW_SECONDS,
 * then, if nobody set it up, restarts and tries the stored Wi-Fi again.
 *
 * The old Wi-Fi is never lost to a failed setup. The provisioning library
 * writes new details to flash before trying them, and blanks them after a
 * failed try, so the old ones are kept aside in NVS while a window is open,
 * and only dropped once a network (new or old) actually connects.
 *
 * Requires Bluetooth enabled in menuconfig:
 *     Component config -> Bluetooth -> [*] Bluetooth
 *     (BLE only is enough; NimBLE keeps the binary smaller than Bluedroid.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_attr.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "network_provisioning/manager.h"
#include "network_provisioning/scheme_ble.h"

#include "ble_provisioning.h"
#include "board_manager.h"
#include "lab_discovery.h"
#include "mqtt_manager.h"
#include "nvs_api.h"

#define TAG "BLE_PROV"

/* Proof of possession the phone must supply. Change it for your own lab. */
#define PROV_POP "logic123"

static void (*s_connected_callback)(void) = NULL;
static bool s_network_started = false;

/* Falling back to BLE when the stored Wi-Fi does not work. */
#define CONNECT_DEADLINE_SECONDS 20    /* no Wi-Fi yet since power-on */
#define RECONNECT_DEADLINE_SECONDS 120 /* a working connection dropped: more
                                        * likely a router restart, and a board
                                        * mid-exam should not leave for BLE */
#define SETUP_WINDOW_SECONDS 180

/* Survives esp_restart() but not a power cycle. Once the board has decided
 * it is provisioned, Bluetooth memory is handed back to the heap, so a
 * restart is the only way back to BLE; this tells the next boot to open the
 * setup window despite the stored Wi-Fi. */
#define SETUP_WINDOW_MAGIC 0x5E7B1E01u
static RTC_NOINIT_ATTR uint32_t s_setup_window_request;

static int s_failed_attempts = 0;
static bool s_connected = false;
static bool s_ever_connected = false;
static esp_timer_handle_t s_connect_timer = NULL;
/* True while the provisioning manager drives Wi-Fi (first setup or a
 * fallback window): this file then leaves connecting to it. */
static bool s_provisioning_active = false;
static bool s_credentials_received = false;
static bool s_setup_succeeded = false;
static esp_timer_handle_t s_window_timer = NULL;

static const char *disconnect_hint(uint8_t reason)
{
    switch (reason)
    {
    case WIFI_REASON_NO_AP_FOUND:
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
        return "network not found: out of range, renamed, or 5 GHz only";
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return "rejected: wrong password?";
    default:
        return "could not connect";
    }
}

/* ------------------------------------------------------------------ */
/* Keeping the old Wi-Fi until a new one works                          */
/* ------------------------------------------------------------------ */
static bool read_backup(char *ssid, size_t ssid_size, char *pass, size_t pass_size)
{
    size_t length = ssid_size;
    ssid[0] = '\0';
    pass[0] = '\0';
    if (read_str_from_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_WIFI_BACKUP_SSID, ssid, &length) != ESP_OK ||
        ssid[0] == '\0')
    {
        return false;
    }
    length = pass_size;
    read_str_from_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_WIFI_BACKUP_PASS, pass, &length);
    return true;
}

/* Before a setup window: put the stored Wi-Fi aside. An existing backup is
 * kept as it is, since the details now stored may be ones a failed setup
 * left behind (power lost mid-window). */
static void backup_stored_wifi(void)
{
    char ssid[33];
    char pass[65];
    if (read_backup(ssid, sizeof(ssid), pass, sizeof(pass)))
    {
        ESP_LOGI(TAG, "Previous Wi-Fi '%s' is still kept aside", ssid);
        return;
    }

    wifi_config_t stored = {0};
    if (esp_wifi_get_config(WIFI_IF_STA, &stored) != ESP_OK || stored.sta.ssid[0] == 0)
    {
        return;
    }
    // Both fields can fill their arrays with no terminator.
    memcpy(ssid, stored.sta.ssid, sizeof(stored.sta.ssid));
    ssid[sizeof(stored.sta.ssid)] = '\0';
    memcpy(pass, stored.sta.password, sizeof(stored.sta.password));
    pass[sizeof(stored.sta.password)] = '\0';

    write_str_to_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_WIFI_BACKUP_SSID, ssid);
    write_str_to_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_WIFI_BACKUP_PASS, pass);
    ESP_LOGI(TAG, "Keeping Wi-Fi '%s' until a new network actually connects", ssid);
}

/* Puts the kept-aside Wi-Fi back as the stored one. */
static bool restore_backup_wifi(void)
{
    char ssid[33];
    char pass[65];
    if (!read_backup(ssid, sizeof(ssid), pass, sizeof(pass)))
    {
        return false;
    }

    wifi_config_t config = {0};
    strlcpy((char *)config.sta.ssid, ssid, sizeof(config.sta.ssid));
    // A 64-character hex key fills the field exactly; strlcpy would cut one.
    memcpy(config.sta.password, pass, strnlen(pass, sizeof(config.sta.password)));

    esp_wifi_disconnect(); // set_config is refused mid-connect
    esp_wifi_set_storage(WIFI_STORAGE_FLASH);
    if (esp_wifi_set_config(WIFI_IF_STA, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not restore Wi-Fi '%s'", ssid);
        return false;
    }
    ESP_LOGW(TAG, "Restored the previous Wi-Fi '%s'", ssid);
    return true;
}

/* A network connected, so whatever is stored now works: the backup has
 * done its job. Only written when there is one, to spare the flash. */
static void drop_backup(void)
{
    char ssid[33];
    char pass[65];
    if (!read_backup(ssid, sizeof(ssid), pass, sizeof(pass)))
    {
        return;
    }
    write_str_to_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_WIFI_BACKUP_SSID, "");
    write_str_to_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_WIFI_BACKUP_PASS, "");
    ESP_LOGI(TAG, "Connected, so the previous Wi-Fi '%s' is no longer kept aside", ssid);
}

/* ------------------------------------------------------------------ */
/* Falling back to BLE                                                  */
/* ------------------------------------------------------------------ */
static void restart_into_setup_window(void)
{
    ESP_LOGW(TAG, "Restarting to advertise over Bluetooth for %d s so the board can "
                  "be set up again. The stored Wi-Fi is kept.",
             SETUP_WINDOW_SECONDS);
    s_setup_window_request = SETUP_WINDOW_MAGIC;
    esp_restart();
}

static void connect_deadline_cb(void *arg)
{
    if (s_connected || s_provisioning_active)
    {
        return;
    }
    ESP_LOGW(TAG, "No Wi-Fi after %d s.",
             s_ever_connected ? RECONNECT_DEADLINE_SECONDS : CONNECT_DEADLINE_SECONDS);
    restart_into_setup_window();
}

static void arm_connect_deadline(int seconds)
{
    if (!s_connect_timer)
    {
        const esp_timer_create_args_t args = {
            .callback = connect_deadline_cb,
            .name = "wifi_deadline",
        };
        if (esp_timer_create(&args, &s_connect_timer) != ESP_OK)
        {
            return;
        }
    }
    esp_timer_stop(s_connect_timer); // fine if it was not running
    esp_timer_start_once(s_connect_timer, (uint64_t)seconds * 1000 * 1000);
}

static void window_timer_cb(void *arg)
{
    if (s_setup_succeeded)
    {
        return;
    }
    if (s_credentials_received)
    {
        // Someone is mid-setup; give them another minute rather than cutting
        // them off.
        s_credentials_received = false;
        esp_timer_start_once(s_window_timer, 60ULL * 1000 * 1000);
        return;
    }
    ESP_LOGW(TAG, "Nobody set the board up. Trying the previous Wi-Fi again.");
    // A failed attempt in this window may have overwritten or blanked it.
    restore_backup_wifi();
    esp_restart();
}

/* Beacons arrive every 2 s; 7 s allows for a couple of lost ones. */
#define DISCOVERY_WINDOW_MS 7000

/* While MQTT is down, listen for the lab host this often. Covers a lab
 * started after the boards, and a PC whose address changed. */
#define REDISCOVERY_INTERVAL_MS 15000
#define REDISCOVERY_LISTEN_MS 5000

/* Runs after the first IP: find the lab PC, start MQTT, then keep an ear
 * out for the lab host whenever MQTT is not connected. Kept off the event
 * loop task because the discovery listen blocks. */
static void network_ready_task(void *arg)
{
    if (!lab_discovery_find(DISCOVERY_WINDOW_MS, NULL, 0, NULL))
    {
        ESP_LOGI(TAG, "Using the stored broker until the lab host is heard");
    }
    if (s_connected_callback)
    {
        s_connected_callback();
    }

    int heard_but_unreachable = 0;
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(REDISCOVERY_INTERVAL_MS));
        if (mqtt_is_connected())
        {
            heard_but_unreachable = 0;
            continue;
        }

        char host[96];
        int port = 0;
        if (!lab_discovery_find(REDISCOVERY_LISTEN_MS, host, sizeof(host), &port))
        {
            continue;
        }
        if (mqtt_app_set_broker(host, port))
        {
            heard_but_unreachable = 0;
            continue;
        }

        // Its broadcasts reach this board, yet MQTT to the same address
        // fails: the lab PC is refusing incoming connections.
        if (++heard_but_unreachable % 4 == 1)
        {
            ESP_LOGW(TAG, "The lab host at %s announces itself, but MQTT cannot connect "
                          "to it. On the lab PC, allow Python through Windows Firewall "
                          "(and set the network to Private).",
                     host);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Custom endpoint: the lab PC's address                               */
/* ------------------------------------------------------------------ */
static esp_err_t lab_config_handler(uint32_t session_id, const uint8_t *inbuf,
                                    ssize_t inlen, uint8_t **outbuf,
                                    ssize_t *outlen, void *priv_data)
{
    const char *reply = "{\"status\":\"ok\"}";

    if (inbuf && inlen > 0)
    {
        char *json = strndup((const char *)inbuf, inlen);
        if (json)
        {
            ESP_LOGI(TAG, "Lab config received: %s", json);
            cJSON *root = cJSON_Parse(json);
            if (root)
            {
                const cJSON *broker = cJSON_GetObjectItemCaseSensitive(root, "broker");
                if (cJSON_IsString(broker) && board_broker_is_usable(broker->valuestring))
                {
                    write_str_to_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_MQTT_HOST,
                                     broker->valuestring);
                }
                else if (cJSON_IsString(broker))
                {
                    ESP_LOGW(TAG, "Ignoring broker '%s' from setup: it means this "
                                  "board itself. Discovery will find the lab host.",
                             broker->valuestring);
                }

                const cJSON *port = cJSON_GetObjectItemCaseSensitive(root, "port");
                if (cJSON_IsNumber(port) && port->valueint > 0 && port->valueint < 65536)
                {
                    char port_text[8];
                    snprintf(port_text, sizeof(port_text), "%d", port->valueint);
                    write_str_to_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_MQTT_PORT, port_text);
                }

                const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "id");
                if (cJSON_IsString(id) && id->valuestring[0] &&
                    board_set_identity(id->valuestring) != ESP_OK)
                {
                    ESP_LOGW(TAG, "Rejected bench id '%s'", id->valuestring);
                    reply = "{\"status\":\"bad_id\"}";
                }

                cJSON_Delete(root);
            }
            else
            {
                ESP_LOGW(TAG, "Lab config was not valid JSON");
                reply = "{\"status\":\"bad_json\"}";
            }
            free(json);
        }
    }

    *outlen = strlen(reply) + 1;
    *outbuf = (uint8_t *)strdup(reply);
    return *outbuf ? ESP_OK : ESP_ERR_NO_MEM;
}

/* ------------------------------------------------------------------ */
/* Events                                                              */
/* ------------------------------------------------------------------ */
static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == NETWORK_PROV_EVENT)
    {
        switch (id)
        {
        case NETWORK_PROV_START:
            ESP_LOGI(TAG, "BLE provisioning started, waiting for a phone");
            break;
        case NETWORK_PROV_WIFI_CRED_RECV:
            ESP_LOGI(TAG, "Wi-Fi credentials received");
            s_credentials_received = true;
            break;
        case NETWORK_PROV_WIFI_CRED_FAIL:
            ESP_LOGE(TAG, "Provisioning failed: wrong Wi-Fi password, or the "
                          "access point was out of range");
            /* Let the phone try again rather than locking the board out. */
            network_prov_mgr_reset_wifi_sm_state_on_failure();
            break;
        case NETWORK_PROV_WIFI_CRED_SUCCESS:
            ESP_LOGI(TAG, "Provisioning succeeded");
            s_setup_succeeded = true;
            s_provisioning_active = false; // from now on, reconnect as usual
            if (s_window_timer)
            {
                esp_timer_stop(s_window_timer);
            }
            break;
        case NETWORK_PROV_END:
            ESP_LOGI(TAG, "Provisioning finished, shutting BLE down");
            network_prov_mgr_deinit();
            break;
        default:
            break;
        }
    }
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
    {
        // While provisioning, the manager connects by itself once a phone has
        // sent details (and blanks any stored ones in the meantime).
        if (!s_provisioning_active)
        {
            arm_connect_deadline(CONNECT_DEADLINE_SECONDS);
            esp_wifi_connect();
        }
    }
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_provisioning_active)
        {
            return; // the manager reports failures to the phone itself
        }
        const wifi_event_sta_disconnected_t *event = (const wifi_event_sta_disconnected_t *)data;
        s_failed_attempts++;
        ESP_LOGW(TAG, "Wi-Fi '%.*s' %s (reason %d, attempt %d)",
                 (int)event->ssid_len, (const char *)event->ssid,
                 disconnect_hint(event->reason), event->reason, s_failed_attempts);

        if (s_connected)
        {
            // A working connection just dropped: a longer deadline from now.
            s_connected = false;
            arm_connect_deadline(RECONNECT_DEADLINE_SECONDS);
        }
        esp_wifi_connect();
    }
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP " IPSTR, IP2STR(&event->ip_info.ip));
        s_failed_attempts = 0;
        s_connected = true;
        s_ever_connected = true;
        if (s_connect_timer)
        {
            esp_timer_stop(s_connect_timer);
        }
        // Whatever is stored now has connected (new or old), so the old
        // details no longer need keeping aside.
        drop_backup();

        // Only the first time. The MQTT client reconnects by itself after a
        // Wi-Fi drop, and starting a second one under the same client id
        // would have the two knock each other off the broker.
        if (!s_network_started)
        {
            s_network_started = true;
            xTaskCreate(network_ready_task, "network_ready", 4096, NULL, 5, NULL);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Entry points                                                        */
/* ------------------------------------------------------------------ */
void ble_provisioning_set_connected_callback(void (*callback)(void))
{
    s_connected_callback = callback;
}

void ble_provisioning_reset(void)
{
    // esp_wifi_restore() works whether or not the provisioning manager is
    // still running, unlike network_prov_mgr_reset_wifi_provisioning().
    ESP_LOGW(TAG, "Clearing stored Wi-Fi, this board will advertise again");
    esp_wifi_restore();
    // Forgetting on purpose: the kept-aside copy goes too, or the next boot
    // would restore it.
    write_str_to_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_WIFI_BACKUP_SSID, "");
    write_str_to_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_WIFI_BACKUP_PASS, "");
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();
}

void ble_provisioning_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_register(NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID,
                                               &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &event_handler, NULL));

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    network_prov_mgr_config_t prov_cfg = {
        .scheme = network_prov_scheme_ble,
        .scheme_event_handler = NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
    };
    ESP_ERROR_CHECK(network_prov_mgr_init(prov_cfg));

    bool provisioned = false;
    ESP_ERROR_CHECK(network_prov_mgr_is_wifi_provisioned(&provisioned));

    // RTC memory holds garbage after a power cycle, hence the reset-reason
    // check as well as the magic value.
    bool window_requested = esp_reset_reason() == ESP_RST_SW &&
                            s_setup_window_request == SETUP_WINDOW_MAGIC;
    s_setup_window_request = 0; // one window per request

    // Power lost in a setup window after a failed attempt blanked the stored
    // details: the kept-aside ones are still good to try.
    if (!provisioned && restore_backup_wifi())
    {
        provisioned = true;
    }

    if (provisioned && !window_requested)
    {
        wifi_config_t stored = {0};
        esp_wifi_get_config(WIFI_IF_STA, &stored);
        ESP_LOGI(TAG, "Connecting to stored Wi-Fi '%.32s'. If it cannot be joined "
                      "within %d s, the board opens Bluetooth setup and keeps these "
                      "details until a new network connects.",
                 (const char *)stored.sta.ssid, CONNECT_DEADLINE_SECONDS);
        network_prov_mgr_deinit();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        return;
    }

    if (provisioned)
    {
        // Must happen before provisioning starts: from then on the library
        // may overwrite or blank the stored details.
        backup_stored_wifi();
    }

    /* Advertise under a name the instructor can recognise on the bench. */
    char service_name[32];
    snprintf(service_name, sizeof(service_name), "LOGIC_%s", board_identity());

    /* Must be created before provisioning starts, registered after. */
    ESP_ERROR_CHECK(network_prov_mgr_endpoint_create("lab-config"));

    s_provisioning_active = true;
    ESP_LOGI(TAG, "Advertising over BLE as '%s' (PoP: %s)", service_name, PROV_POP);
    ESP_ERROR_CHECK(network_prov_mgr_start_provisioning(
        NETWORK_PROV_SECURITY_1, PROV_POP, service_name, NULL));

    ESP_ERROR_CHECK(
        network_prov_mgr_endpoint_register("lab-config", lab_config_handler, NULL));

    if (provisioned)
    {
        // A fallback window: the stored Wi-Fi is kept, and tried again when
        // the window closes without anyone setting the board up.
        const esp_timer_create_args_t timer_args = {
            .callback = window_timer_cb,
            .name = "setup_window",
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_window_timer));
        ESP_ERROR_CHECK(esp_timer_start_once(s_window_timer,
                                             SETUP_WINDOW_SECONDS * 1000ULL * 1000ULL));
        ESP_LOGW(TAG, "Setup window open for %d s, then the stored Wi-Fi is tried again.",
                 SETUP_WINDOW_SECONDS);
    }
}
