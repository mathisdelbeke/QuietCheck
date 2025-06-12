#include <stdio.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#include <esp_adc/adc_oneshot.h>
#include <mqtt_client.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_event.h>

#include "wifi_config.h"

#define BIEPER GPIO_NUM_27

#define WIFI_SSID WIFI_CONFIG_SSID
#define WIFI_PASS WIFI_CONFIG_PASS
#define MQTT_BROKER_URI "mqtt://192.168.0.202"                      // Pi's IP address
#define MQTT_NOISE_READINGS_TOPIC "esp32/noise"                     // Topic for noise readings

#define NUM_NOISE_READINGS 10                                       // Readings for calculating simple moving average
#define TOO_LOUD_LEVEL 50
#define NOISE_READING_DELAY 50
#define MQTT_PUB_READINGS_DELAY 1000

static const char *TAG = "MQTT";
static const uint16_t MICROPHONE_ADC_BIAS = 2048;                   // Bias in middle of reading range (0 - 4095)

static uint16_t noise_readings[NUM_NOISE_READINGS] = {0};           
static uint8_t reading_index = 0;
static uint32_t total_noise = 0;
static uint16_t moving_avg_noise = 0;

static esp_mqtt_client_handle_t mqtt_client;
static adc_oneshot_unit_handle_t adc_handle;

static void wifi_init_sta() {                                              
    esp_netif_init();                                               // Init TCP/IP network intefaces
    esp_event_loop_create_default();                                // Event loop used by MQTT under the hood
    esp_netif_create_default_wifi_sta();                            

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);                                            // Default init configuration

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);                               // Wi-Fi in station (STA) mode = connect to a network
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
}

static void mqtt_event_handler_cb(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            break;
        default:
            break;
    }
}

static void mqtt_app_start() {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler_cb, NULL);     // No user-defined handler_args (NULL)
    esp_mqtt_client_start(mqtt_client);
}

static void gpio_init() {
    gpio_set_direction(BIEPER, GPIO_MODE_OUTPUT);
}

static void adc_init() {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_config, &adc_handle);
    
    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,                                           // 12-bit resolution (0 - 4095)
        .atten = ADC_ATTEN_DB_12,                                                   // ADC translates 0 - 3.3 V range                                           
    };
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_6, &chan_config);            // CHNNL_6: GPIO34 on ESP32
}

static void init_all() {
    nvs_flash_init();
    wifi_init_sta();
    vTaskDelay(5000 / portTICK_PERIOD_MS);                                          // Wait for WiFi
    mqtt_app_start();
    gpio_init();
    adc_init();
}

static void read_noise(uint16_t *noise) {
    int adc_reading = 0;
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL_6, &adc_reading);      // Raw ADC value, GPIO34
    if (ret == ESP_OK) {
        *noise = abs(adc_reading - MICROPHONE_ADC_BIAS);                            // Absolute difference from bias
    }
}

static void update_sma_noise(uint16_t *noise) {
    total_noise -= noise_readings[reading_index];                                   // Update simple moving average
    noise_readings[reading_index] = *noise;
    total_noise += noise_readings[reading_index];
    reading_index = (reading_index + 1) % NUM_NOISE_READINGS;
    moving_avg_noise = total_noise / NUM_NOISE_READINGS; 
}

static void reset_sma_noise() {
    total_noise = 0;
    reading_index = 0;
    moving_avg_noise = 0;
    for (uint8_t i = 0; i < NUM_NOISE_READINGS; i++) {
        noise_readings[i] = 0;
    }  
}

static void check_too_loud() {
    if (moving_avg_noise > TOO_LOUD_LEVEL) {                                                  
        gpio_set_level(BIEPER, 1);
        vTaskDelay(100);
        gpio_set_level(BIEPER, 0);
        reset_sma_noise();                                                          // Because SMA decreases to slow after beep, and thus multiple beeps
    }
}

void noise_reading_task (void *pvParameters) {                                      // No user-defined data (pointer to void parameters = NULL)
    while (1) {
        uint16_t noise = 0;
        read_noise(&noise);
        update_sma_noise(&noise);
        check_too_loud();
        printf("%d\n", moving_avg_noise);
    
        vTaskDelay(pdMS_TO_TICKS(NOISE_READING_DELAY));                             // Yield
    }
}

void mqtt_publish_task(void *pvParameters) {                                        // No user-defined data (pointer to void parameters = NULL)
    while (1) {
        uint8_t mqtt_payload[2];
        mqtt_payload[0] = moving_avg_noise;                                         // LSB
        mqtt_payload[1] = (moving_avg_noise >> 8);                                  // HSB
        esp_mqtt_client_publish(mqtt_client, MQTT_NOISE_READINGS_TOPIC, (const char *)mqtt_payload, 2, 1, 0);
    
        vTaskDelay(pdMS_TO_TICKS(MQTT_PUB_READINGS_DELAY));                         // Yield
    }
}

void app_main() {
    init_all();

    xTaskCreate(noise_reading_task, "noise_reading_task", 2048, NULL, 5, NULL);     // 2048 words = 8192 bytes (8 KB) of memory on stack allocated, priority lvl 5, and no pointer to task handler needed
    xTaskCreate(mqtt_publish_task, "mqtt_publish_task", 4096, NULL, 5, NULL);       // 4096 words = 16384 bytes (16 KB) of memory on stack allocated, priority lvl 5, and no pointer to task handler needed
}
