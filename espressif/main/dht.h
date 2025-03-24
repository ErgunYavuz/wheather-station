#ifndef DHT_H
#define DHT_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

// Function declarations
void init_dht(gpio_num_t dht_pin);
int dht_read_data(gpio_num_t dht_pin, int *humidity, int *temperature);

#endif 