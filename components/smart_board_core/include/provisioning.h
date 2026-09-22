#pragma once

/**
 * @brief Starts the serial provisioning console.
 *
 * Lets Wi-Fi, the MQTT broker address and the bench id be changed over USB
 * without rebuilding the firmware. See provisioning.c for the commands.
 */
void provisioning_start(void);
