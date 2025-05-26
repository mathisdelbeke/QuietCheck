#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "esp_adc/adc_oneshot.h"

#include "mqtt_client.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#define WIFI_SSID "x"
#define WIFI_PASS "x"
#define MQTT_BROKER_URI "mqtt://192.168.0.202"

#define NUM_READINGS 10                                              // Number of readings for simple moving average

static const char *TAG = "MQTT";
static const uint16_t ADC_BIAS_VALUE = 2048;

static uint16_t noice_readings[NUM_READINGS] = {0};
static uint8_t reading_index = 0;
static uint32_t total_noice = 0;
static uint16_t moving_avg_noice = 0;

static esp_mqtt_client_handle_t client;
static adc_oneshot_unit_handle_t adc_handle;

void wifi_init_sta(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
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

void mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler_cb, NULL);
    esp_mqtt_client_start(client);
}

void adc_init(void) {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_config, &adc_handle);
    
    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,                                           // 12-bit resolution
        .atten = ADC_ATTEN_DB_12,                                                   // Set attenuation based on expected input voltage
    };
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_6, &chan_config);            // Example: GPIO34
}

void init_all() {
    adc_init();
    nvs_flash_init();
    wifi_init_sta();
    vTaskDelay(5000 / portTICK_PERIOD_MS);                              // Wait for WiFi
    mqtt_app_start();
}

void read_noice(uint16_t *noice) {
    int adc_reading = 0;
    esp_err_t ret = adc_oneshot_read(adc_handle, ADC_CHANNEL_6, &adc_reading);      // Read raw ADC value (0 - 4095)
    if (ret == ESP_OK) {
        *noice = abs(adc_reading - ADC_BIAS_VALUE);
    }
}

void update_sma_noice(uint16_t *noice) {
    total_noice -= noice_readings[reading_index];                                                   // Update simple moving average
    noice_readings[reading_index] = *noice;
    total_noice += noice_readings[reading_index];
    reading_index = (reading_index + 1) % NUM_READINGS;
    moving_avg_noice = total_noice / NUM_READINGS; 
}

void app_main(void) {
    init_all();

    while (1) {
        uint16_t noice = 0;
        read_noice(&noice);
        update_sma_noice(&noice);
        printf("%d\n", moving_avg_noice);

        uint8_t mqtt_payload[2];
        mqtt_payload[0] = moving_avg_noice & 0xFF;
        mqtt_payload[1] = (moving_avg_noice >> 8) & 0xFF;
        esp_mqtt_client_publish(client, "esp32/noice", (const char *)mqtt_payload, 2, 1, 0);

        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}
