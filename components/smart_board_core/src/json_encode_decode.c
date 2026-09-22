#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "json_encode_decode.h"

#define TAG "JSON_DECODE"

/**
 * @brief Splits an incoming MQTT message into command, content and request id.
 *
 * Every field is bounds checked: the previous version used strcpy() into a
 * fixed 30 byte buffer straight from the network, which a long command string
 * would have overrun.
 */
void decode_message(const char *message, char *command, size_t command_len,
                    cJSON **content, char *request_id, size_t request_id_len)
{
    if (command && command_len)
    {
        command[0] = '\0';
    }
    if (request_id && request_id_len)
    {
        request_id[0] = '\0';
    }
    if (content)
    {
        *content = NULL;
    }

    if (!message)
    {
        return;
    }

    cJSON *root = cJSON_Parse(message);
    if (!root)
    {
        ESP_LOGE(TAG, "Could not parse the incoming message as JSON.");
        return;
    }

    const cJSON *j_command = cJSON_GetObjectItemCaseSensitive(root, "command");
    if (j_command && cJSON_IsString(j_command) && j_command->valuestring && command)
    {
        strncpy(command, j_command->valuestring, command_len - 1);
        command[command_len - 1] = '\0';
        ESP_LOGI(TAG, "Command: %s", command);
    }

    const cJSON *j_request = cJSON_GetObjectItemCaseSensitive(root, "requestId");
    if (j_request && cJSON_IsString(j_request) && j_request->valuestring && request_id)
    {
        strncpy(request_id, j_request->valuestring, request_id_len - 1);
        request_id[request_id_len - 1] = '\0';
    }

    cJSON *j_content = cJSON_GetObjectItemCaseSensitive(root, "content");
    if (j_content && content)
    {
        *content = cJSON_Duplicate(j_content, 1);
    }

    cJSON_Delete(root);
}
