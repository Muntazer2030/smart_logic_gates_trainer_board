#pragma once

void init_gpios(int indicator_gpio, int relay_gpio, int switch_gpio);


void init_mqtt(void);

void subscribe_to_topics();