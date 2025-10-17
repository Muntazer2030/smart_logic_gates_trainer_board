#pragma once

void init_gpios(int indicator_gpio, int relay_gpio, int switch_gpio);

void init_wifi_switches(int switches_count, int switches_pins[], int indecators_pins[], int relays_pin[]);

void send_gangs_property_to_mqtt();
void init_mqtt(void);

void subscribe_to_topics();