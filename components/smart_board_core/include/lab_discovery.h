#pragma once

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Listens for the lab host's UDP beacon for up to @p timeout_ms.
 *
 * When one is heard, its MQTT address is saved to NVS (only if it changed),
 * where init_mqtt() picks it up, and copied to @p host_out / @p port_out
 * (either may be NULL). Returns false, leaving NVS untouched, when nothing
 * is heard. Blocks, so call it from a task rather than an event handler.
 */
bool lab_discovery_find(int timeout_ms, char *host_out, size_t host_out_size, int *port_out);
