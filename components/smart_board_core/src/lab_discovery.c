/*
 * Finds the lab PC on the LAN.
 *
 * The lab host (host/start_host.py) broadcasts on UDP 50505 every 2 seconds:
 *
 *     {"service":"smartlogic-lab","api":"http://192.168.1.50:5000",
 *      "mqtt_host":"192.168.1.50","mqtt_port":1883}
 *
 * Listening for a few seconds after getting an IP means a board only ever
 * needs Wi-Fi credentials, and follows the PC if its DHCP address changes.
 */

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "lwip/sockets.h"

#include "board_manager.h"
#include "lab_discovery.h"
#include "nvs_api.h"

#define TAG "DISCOVERY"
#define DISCOVERY_PORT 50505
#define DISCOVERY_SERVICE "smartlogic-lab"

/* Parses one datagram. Returns true and fills host/port for a lab beacon. */
static bool parse_beacon(const char *text, char *host, size_t host_len, int *port)
{
    bool found = false;
    cJSON *root = cJSON_Parse(text);
    if (!root)
    {
        return false;
    }

    const cJSON *service = cJSON_GetObjectItemCaseSensitive(root, "service");
    const cJSON *mqtt_host = cJSON_GetObjectItemCaseSensitive(root, "mqtt_host");
    const cJSON *mqtt_port = cJSON_GetObjectItemCaseSensitive(root, "mqtt_port");

    if (cJSON_IsString(service) && strcmp(service->valuestring, DISCOVERY_SERVICE) == 0 &&
        cJSON_IsString(mqtt_host) && mqtt_host->valuestring[0])
    {
        strlcpy(host, mqtt_host->valuestring, host_len);
        *port = (cJSON_IsNumber(mqtt_port) && mqtt_port->valueint > 0) ? mqtt_port->valueint : 1883;
        found = true;
    }

    cJSON_Delete(root);
    return found;
}

bool lab_discovery_find(int timeout_ms, char *host_out, size_t host_out_size, int *port_out)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
        ESP_LOGW(TAG, "Could not open a UDP socket");
        return false;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(DISCOVERY_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(sock, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        ESP_LOGW(TAG, "Could not listen on UDP %d", DISCOVERY_PORT);
        close(sock);
        return false;
    }

    /* Short receive timeout, so the overall deadline is honoured even when
     * unrelated traffic keeps arriving. */
    struct timeval tick = {.tv_sec = 0, .tv_usec = 500 * 1000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tick, sizeof(tick));

    ESP_LOGI(TAG, "Listening for the lab host for %d ms", timeout_ms);

    char buffer[256];
    char host[96];
    int port = 1883;
    bool found = false;

    for (int waited = 0; waited < timeout_ms && !found; waited += 500)
    {
        int length = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (length <= 0)
        {
            continue;
        }
        buffer[length] = '\0';
        found = parse_beacon(buffer, host, sizeof(host), &port);
    }
    close(sock);

    if (!found)
    {
        ESP_LOGI(TAG, "No lab host heard within %d ms", timeout_ms);
        return false;
    }
    if (!board_broker_is_usable(host))
    {
        ESP_LOGW(TAG, "Ignoring a beacon advertising '%s'", host);
        return false;
    }

    // Written only when it changed: this also runs every few seconds while
    // MQTT cannot connect, and the flash should not wear for nothing.
    char port_text[8];
    snprintf(port_text, sizeof(port_text), "%d", port);
    char stored_host[96] = {0};
    char stored_port[8] = {0};
    size_t length = sizeof(stored_host);
    read_str_from_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_MQTT_HOST, stored_host, &length);
    length = sizeof(stored_port);
    read_str_from_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_MQTT_PORT, stored_port, &length);
    if (strcmp(stored_host, host) != 0 || strcmp(stored_port, port_text) != 0)
    {
        write_str_to_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_MQTT_HOST, host);
        write_str_to_nvs(NVS_BOARD_NAMESPACE, NVS_KEY_MQTT_PORT, port_text);
    }
    ESP_LOGI(TAG, "Lab host found: MQTT %s:%d", host, port);

    if (host_out)
    {
        strlcpy(host_out, host, host_out_size);
    }
    if (port_out)
    {
        *port_out = port;
    }
    return true;
}
