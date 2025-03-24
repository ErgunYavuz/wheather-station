#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "dht.h"
#include "esp_intr_alloc.h"



#define DHT_TIMEOUT 1100
static const char *TAG = "DHT22";
// DHT22 sheet : https://cdn.sparkfun.com/assets/f/7/d/9/c/DHT22.pdf

int dht_read_data(gpio_num_t dht_pin, int *humidity, int *temperature) {
    // DATA = 8 bit integral RH data+8 bit decimal RH data+8 bit integral T data+8 bit decimal T data+8 bit check-sum
    uint8_t data[5] = {0, 0, 0, 0, 0};
    int bit_index = 0, byte_index = 0;
    
    // Setting the data-bus voltage level from high to low level signals the sensor to read 
    gpio_set_level(dht_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(20)); //20ms is reliable from testing
    gpio_set_level(dht_pin, 1);
    
    // ESP will wait 20-40us for DHT22's response if it's more we can assume timeout 
    int64_t start_time = esp_timer_get_time();
    while (gpio_get_level(dht_pin) == 1) {
        if ((esp_timer_get_time() - start_time) > DHT_TIMEOUT) return ESP_FAIL;
    }

    // DHT22 sends out low-voltage-level signal lasts 80us 
    // We wait in loops for the signal changes
    start_time = esp_timer_get_time();
    while (gpio_get_level(dht_pin) == 0);  // Wait for LOW pulse
    while (gpio_get_level(dht_pin) == 1);  // Wait for HIGH pulse
    
    // Read 40-bit Data Stream
    for (int i = 0; i < 40; i++) {
        // Wait for LOW-to-HIGH transition
        while (gpio_get_level(dht_pin) == 0);
        start_time = esp_timer_get_time();
        
        // Wait while HIGH, measure duration
        while (gpio_get_level(dht_pin) == 1);
        int64_t pulse_time = esp_timer_get_time() - start_time;
        
        // If pulse is longer than 50µs, it is a 1
        if (pulse_time > 50) {
            data[byte_index] |= (1 << (7 - bit_index));
        }
        
        // Move to next bit
        bit_index++;
        if (bit_index == 8) {
            bit_index = 0;
            byte_index++;
        }
    }

    // Verify Checksum
    if ((data[0] + data[1] + data[2] + data[3]) != data[4]) {
        ESP_LOGE(TAG, "Checksum failed!");
        return ESP_FAIL;
    }

    // Convert Data
    *humidity = ((data[0] << 8) | data[1]) / 10;
    *temperature = ((data[2] << 8) | data[3]) / 10;
    return ESP_OK;
}

void init_dht(gpio_num_t dht_pin){
    //Configure GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << dht_pin),      // Selects which GPIO pin(s) to configure
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,      // Open-Drain Mode
        .pull_up_en = GPIO_PULLUP_ENABLE,       // Enable pull-up resistor
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  // Disable internal pull-down resistor
        .intr_type = GPIO_INTR_DISABLE          // Disable interrupts
    };
    gpio_config(&io_conf);
}
