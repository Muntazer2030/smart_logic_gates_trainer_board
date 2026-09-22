#pragma once
#include <cJSON.h>

/**
 * @brief Splits an incoming MQTT message into its command, content and request id.
 * @param command    buffer of at least 32 bytes
 * @param request_id buffer of at least 64 bytes (may be NULL)
 * @param content    receives a duplicated cJSON object the caller must delete
 */
void decode_message(const char *message, char *command, size_t command_len,
                    cJSON **content, char *request_id, size_t request_id_len);
