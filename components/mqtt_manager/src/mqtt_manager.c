#include "esp_event.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "string.h"
#include "sdkconfig.h"
#include "mqtt_manager.h"
#define TAG "MQTT_API"

#define MQTT_CONNECTED_BIT BIT0
#define MQTT_DISCONNECTED_BIT BIT1

static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;
static EventGroupHandle_t mqtt_event_group = NULL;



static void (*message_callback)(const char *topic, const char *payload) = NULL; // Callback function
static void (*mqtt_connec_callback)() = NULL;        

char *status_topic = NULL; // Status topic for the device
// Callback function
// Function to send MQTT message
void mqtt_send_message(const char *topic, const char *message, int qos, int retain)
{
    if (!mqtt_client || !message)
    {
        ESP_LOGE(TAG, "Invalid arguments to mqtt_send_message");
        return;
    }

    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, message, strlen(message), qos, retain);
    if (msg_id >= 0)
    {
        ESP_LOGI(TAG, "Message published successfully, msg_id=%d", msg_id);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to publish Message.");
    }
}

// Function to set callback for received messages
void mqtt_set_message_callback(void (*callback)(const char *, const char *))
{
    message_callback = callback;
}
void mqtt_set_connect_callback(void (*callback)())
{
    mqtt_connec_callback = callback;
}

void mqtt_subscribe(const char *topic, int qos)
{
    if (!mqtt_client || !topic)
    {
        ESP_LOGE(TAG, "Invalid arguments to mqtt_subscribe");
        return;
    }

    int msg_id = esp_mqtt_client_subscribe(mqtt_client, topic, qos);
    if (msg_id >= 0)
    {
        ESP_LOGI(TAG, "Subscribed successfully, msg_id=%d", msg_id);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to subscribe to topic: %s", topic);
    }
}

// MQTT event handler
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        mqtt_connected = true;
        xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_BIT);
        // esp_mqtt_client_subscribe(mqtt_client, command_topic, 1);
        if (mqtt_connec_callback)
        {
            mqtt_connec_callback();
        }
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);

        mqtt_send_message(status_topic, "{\"status\": \"online\"}", 1, 1);

        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt_connected = false;
        xEventGroupClearBits(mqtt_event_group, MQTT_CONNECTED_BIT);
        xEventGroupSetBits(mqtt_event_group, MQTT_DISCONNECTED_BIT);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\n", event->topic_len, event->topic);
        printf("DATA=%.*s\n", event->data_len, event->data);

        if (message_callback)
        {
            char topic[event->topic_len + 1];
            char payload[event->data_len + 1];

            memcpy(topic, event->topic, event->topic_len);
            topic[event->topic_len] = '\0';

            memcpy(payload, event->data, event->data_len);
            payload[event->data_len] = '\0';

            message_callback(topic, payload);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            ESP_LOGE(TAG, "Transport error: %s", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;

    default:
        ESP_LOGI(TAG, "Other event id: %d", event->event_id);
        break;
    }
}

// Function to start MQTT client
void mqtt_app_start(const char *cert, const char *key, const char *uuid, const char *status_topic_arg)
{
    status_topic = strdup(status_topic_arg);
    printf("Status topic: %s\n", status_topic);
    if (!mqtt_event_group)
    {
        mqtt_event_group = xEventGroupCreate();
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.hostname = "broker.emqx.io",
            .address.port = 1883,
          //  .verification.certificate = (const char *)server_cert_pem_start,
            //.address.transport = MQTT_TRANSPORT_OVER_SSL
            .address.transport = MQTT_TRANSPORT_OVER_TCP
        },

        .credentials = {
            .client_id = "sokeoifkoeskiofkjo",
           // .authentication.certificate = (const char *)cert,
           // .authentication.key = (const char *)key,
        },
        .network.timeout_ms = 5000,
        .network.reconnect_timeout_ms = 4000,
        .session.message_retransmit_timeout = 5000,
        .session.keepalive = 5,
        .session.disable_keepalive = false,
        .session.disable_clean_session = false,

        .session.last_will = {
            .topic = status_topic,
            .msg = "{\"status\": \"offline\"}",
            .msg_len = strlen("{\"status\": \"offline\"}"),

            .qos = 1,
            .retain = 1,
        }

    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client)
    {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return;
    }

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

// Function to stop MQTT client
void mqtt_app_stop()
{
    if (mqtt_client)
    {
        esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
    }
}

// Function to check MQTT connection status
bool mqtt_is_connected()
{
    return mqtt_connected;
}

// Function to wait for MQTT connection
void mqtt_wait_for_connection()
{
    ESP_LOGI(TAG, "Waiting for MQTT connection...");
    xEventGroupWaitBits(mqtt_event_group, MQTT_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "MQTT connected!");
}
