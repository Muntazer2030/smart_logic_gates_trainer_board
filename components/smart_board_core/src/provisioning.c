/*
 * Serial provisioning console, the USB fallback for BLE provisioning.
 *
 * Type these into `idf.py monitor`:
 *
 *     provision wifi <ssid> <password>
 *     provision broker <ip-or-hostname> [port]
 *     provision id BOARD_003
 *     provision show
 *     provision ble           (forget Wi-Fi, advertise over BLE again)
 *     provision save          (restart with the new settings)
 *
 * Wi-Fi goes into the Wi-Fi driver's own storage, which is where the BLE
 * provisioning manager looks, so both routes agree. The broker address and
 * bench id live in the "board" NVS namespace.
 */

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ble_provisioning.h"
#include "board_manager.h"
#include "nvs_api.h"
#include "provisioning.h"

#define TAG "PROVISION"
#define CONSOLE_LINE_MAX 160

static void print_help(void)
{
    printf("\n");
    printf("  provision wifi <ssid> <password>   set the Wi-Fi network\n");
    printf("  provision broker <host> [port]     set the MQTT broker (your PC)\n");
    printf("  provision id <BOARD_00X>           set this bench id\n");
    printf("  provision show                     show what is stored\n");
    printf("  provision ble                      forget Wi-Fi, advertise over BLE\n");
    printf("  provision save                     restart with the new settings\n");
    printf("\n");
}

static void show_settings(void)
{
    char value[96];
    size_t length;

    printf("\n  Board id : %s\n", board_identity());

    wifi_config_t wifi = {0};
    if (esp_wifi_get_config(WIFI_IF_STA, &wifi) == ESP_OK && wifi.sta.ssid[0])
    {
        printf("  Wi-Fi    : %s\n", (const char *)wifi.sta.ssid);
    }
    else
    {
        printf("  Wi-Fi    : (not provisioned, advertising over BLE)\n");
    }

    length = sizeof(value);
    value[0] = '\0';
    read_str_from_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_WIFI_BACKUP_SSID, value, &length);
    if (value[0])
    {
        printf("  Kept     : %s (restored unless a new network connects)\n", value);
    }

    length = sizeof(value);
    value[0] = '\0';
    read_str_from_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_MQTT_HOST, value, &length);
    printf("  Broker   : %s\n", value[0] ? value : "broker.emqx.io (default)");

    // The port is stored as text: the NVS int helpers are int8, and 1883
    // does not fit in one.
    length = sizeof(value);
    value[0] = '\0';
    read_str_from_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_MQTT_PORT, value, &length);
    printf("  Port     : %s\n\n", value[0] ? value : "1883 (default)");
}

/** Splits off the next whitespace-delimited word, advancing *cursor. */
static char *next_word(char **cursor)
{
    char *start = *cursor;
    while (*start == ' ' || *start == '\t')
    {
        start++;
    }
    if (*start == '\0')
    {
        *cursor = start;
        return NULL;
    }
    char *end = start;
    while (*end && *end != ' ' && *end != '\t')
    {
        end++;
    }
    if (*end)
    {
        *end = '\0';
        end++;
    }
    *cursor = end;
    return start;
}

static void set_wifi(char *ssid, char *password)
{
    wifi_config_t wifi = {0};
    strlcpy((char *)wifi.sta.ssid, ssid, sizeof(wifi.sta.ssid));
    strlcpy((char *)wifi.sta.password, password ? password : "",
            sizeof(wifi.sta.password));

    if (esp_wifi_set_config(WIFI_IF_STA, &wifi) == ESP_OK)
    {
        printf("  Wi-Fi set to '%s'. Run `provision save` when you are done.\n", ssid);
    }
    else
    {
        printf("  Wi-Fi is still starting, try again in a second.\n");
    }
}

static void set_broker(char *host, char *port)
{
    if (!board_broker_is_usable(host))
    {
        printf("  '%s' would mean this board itself. Use the lab PC's address,\n"
               "  as shown in the lab window (e.g. 192.168.1.50).\n",
               host);
        return;
    }
    write_str_to_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_MQTT_HOST, host);
    if (port)
    {
        write_str_to_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_MQTT_PORT, port);
    }
    printf("  Broker set to '%s'. Run `provision save` when you are done.\n", host);
}

static void handle_line(char *line)
{
    char *cursor = line;
    char *command = next_word(&cursor);

    if (!command)
    {
        return;
    }
    if (strcmp(command, "help") == 0 || strcmp(command, "?") == 0)
    {
        print_help();
        return;
    }
    if (strcmp(command, "provision") != 0)
    {
        return;
    }

    char *action = next_word(&cursor);
    if (!action)
    {
        print_help();
    }
    else if (strcmp(action, "wifi") == 0)
    {
        char *ssid = next_word(&cursor);
        char *password = next_word(&cursor);
        if (ssid)
        {
            set_wifi(ssid, password);
        }
        else
        {
            printf("  usage: provision wifi <ssid> <password>\n");
        }
    }
    else if (strcmp(action, "broker") == 0)
    {
        char *host = next_word(&cursor);
        char *port = next_word(&cursor);
        if (host)
        {
            set_broker(host, port);
        }
        else
        {
            printf("  usage: provision broker <host> [port]\n");
        }
    }
    else if (strcmp(action, "id") == 0)
    {
        char *id = next_word(&cursor);
        if (!id)
        {
            printf("  usage: provision id BOARD_003\n");
        }
        else if (board_set_identity(id) == ESP_OK)
        {
            printf("  Board id set to '%s'. Run `provision save` when you are done.\n", id);
        }
        else
        {
            printf("  Could not store that board id.\n");
        }
    }
    else if (strcmp(action, "show") == 0)
    {
        show_settings();
    }
    else if (strcmp(action, "ble") == 0)
    {
        printf("  Forgetting Wi-Fi. The board will restart and advertise over BLE.\n");
        ble_provisioning_reset();
    }
    else if (strcmp(action, "save") == 0)
    {
        printf("  Restarting with the new settings...\n");
        vTaskDelay(pdMS_TO_TICKS(400));
        esp_restart();
    }
    else
    {
        print_help();
    }
}

static void console_task(void *arg)
{
    char line[CONSOLE_LINE_MAX];
    int length = 0;

    printf("\nProvisioning console ready. Type `provision show` or `help`.\n");

    while (1)
    {
        int ch = fgetc(stdin);
        if (ch == EOF)
        {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (ch == '\r')
        {
            continue;
        }
        if (ch == '\n')
        {
            line[length] = '\0';
            if (length > 0)
            {
                handle_line(line);
            }
            length = 0;
            continue;
        }
        if (length < CONSOLE_LINE_MAX - 1)
        {
            line[length++] = (char)ch;
        }
    }
}

void provisioning_start(void)
{
    // Unbuffered stdin so characters arrive as they are typed.
    setvbuf(stdin, NULL, _IONBF, 0);
    xTaskCreate(console_task, "provision_console", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "Provisioning console started");
}
