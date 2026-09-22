#pragma once
#include <cJSON.h>

/**
 * @brief Dispatches one MQTT command.
 * @param request_id Echoed back in the report so the server can match the
 *                   verdict to the request that asked for it.
 */
void handle_command(const char *command, cJSON *content, const char *request_id);

void initialize_all_ports(void);

/** Walks every drive pin once. On request only - it toggles the student's circuit. */
void run_port_self_test(void);
