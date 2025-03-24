#include "freertos/FreeRTOS.h"
#include <stdio.h>
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "dht.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "cJSON.h"

#define DHT_PIN GPIO_NUM_4 
static const char *TAG = "DHT22";

#define WIFI_SSID ""
#define WIFI_PASSWORD ""

#define MQTT_BROKER_URI "mqtt://192.168.1.6" 

static esp_mqtt_client_handle_t mqtt_client;

// Wi-Fi Event Handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI("Wi-Fi", "Disconnected, trying to reconnect...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("Wi-Fi", "Connected with IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

// Initialize Wi-Fi
void wifi_init(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// MQTT Event Handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected");
            break;
        default:
            break;
    }
}

// Initialize MQTT
void mqtt_init(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

// DHT Task
void dht_task(void *pvParameter) {
    int temperature = 0, humidity = 0;
    wifi_init();
    vTaskDelay(pdMS_TO_TICKS(5000));  // Delay to allow Wi-Fi & MQTT to stabilize
    
    mqtt_init();
    init_dht(DHT_PIN);

    while (1) {// Before reading data, disable interrupts
        if (dht_read_data(DHT_PIN, &humidity, &temperature) == ESP_OK) {
            ESP_LOGI(TAG, "Temperature: %d°C, Humidity: %d%%", temperature, humidity);

            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "temperature", temperature);
            cJSON_AddNumberToObject(root, "humidity", humidity);

            // Convert JSON object to string
            char *json_string = cJSON_Print(root);
            printf("Generated JSON: %s\n", json_string);

            esp_mqtt_client_publish(mqtt_client, "sensor/data", json_string, 0, 1, 0);

            // Free allocated memory
            cJSON_Delete(root);
            free(json_string);
            
        } else {
            ESP_LOGE(TAG, "Failed to read from DHT22 sensor!");
        }

        vTaskDelay(pdMS_TO_TICKS(2000));  // Read every 2 seconds
    }
}


void app_main() {
    xTaskCreate(dht_task, "dht_task", 4096, NULL, 5, NULL);
}
