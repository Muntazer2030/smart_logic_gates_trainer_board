#include <esp_wifi.h>
#include "esp_log.h"
#include "string.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "wifi_manager.h"
#define TAG "WIFI_MANAGER"



// Global pointer to the callback function
void (*wifi_connected_callback)(void) = NULL;

// Wi-Fi event handler to handle events related to Wi-Fi connection
void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        // Connect to Wi-Fi when it's initialized
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        // If disconnected from Wi-Fi, retry
        ESP_LOGI(TAG, "Disconnected, retrying...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        // On Wi-Fi connected (IP received)
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        // Notify other component if callback is set
        if (wifi_connected_callback)
        {
            wifi_connected_callback();  // Call the user-defined callback
        }
    }
}

// Function to set the callback from another component
void set_wifi_connected_callback(void (*callback))
{
    wifi_connected_callback = callback;
}

// Initialize Wi-Fi and connect to the provided SSID and password
void connect_to_wifi(uint8_t *ssid, uint8_t *pass)
{
    // Initialize Wi-Fi and set up event handlers
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, (char *)ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, (char *)pass, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

