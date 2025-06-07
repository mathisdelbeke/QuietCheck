#include <stdio.h>
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

#define WIFI_SSID WIFI_CONFIG_SSID
#define WIFI_PASS WIFI_CONFIG_PASS
#define MQTT_BROKER_URI "mqtt://192.168.0.202"                      // Pi's IP address
#define MQTT_MEASUREMENTS_TOPIC "esp32/noice"                       // Topic for noice readings

#define NUM_NOICE_READINGS 10                                       // Readings for calculating simple moving average

static const char *TAG = "MQTT";
static const uint16_t MICROPHONE_ADC_BIAS = 2048;                   // Bias in middle of reading range (0 - 4095)

static uint16_t noice_readings[NUM_NOICE_READINGS] = {0};           
static uint8_t reading_index = 0;
static uint32_t total_noice = 0;
static uint16_t moving_avg_noice = 0;

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
    adc_init();
}

static void read_noice(uint16_t *noice) {
    int adc_reading = 0;
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL_6, &adc_reading);      // Raw ADC value, GPIO34
    if (ret == ESP_OK) {
        *noice = abs(adc_reading - MICROPHONE_ADC_BIAS);                            // Absolute difference from bias
    }
}

static void update_sma_noice(uint16_t *noice) {
    total_noice -= noice_readings[reading_index];                                   // Update simple moving average
    noice_readings[reading_index] = *noice;
    total_noice += noice_readings[reading_index];
    reading_index = (reading_index + 1) % NUM_NOICE_READINGS;
    moving_avg_noice = total_noice / NUM_NOICE_READINGS; 
}

void app_main() {
    init_all();

    while (1) {
        uint16_t noice = 0;
        read_noice(&noice);
        update_sma_noice(&noice);
        printf("%d\n", moving_avg_noice);

        uint8_t mqtt_payload[2];
        mqtt_payload[0] = moving_avg_noice;
        mqtt_payload[1] = (moving_avg_noice >> 8);
        esp_mqtt_client_publish(mqtt_client, MQTT_MEASUREMENTS_TOPIC, (const char *)mqtt_payload, 2, 1, 0);

        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}
