#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"

#define NUM_SAMPLES 10                                              // Number of readings for moving average

void app_main(void)
{
    const uint16_t BIAS_VALUE = 2048;
    uint16_t readings[NUM_SAMPLES] = {0};
    uint8_t index = 0;
    uint32_t total = 0;
    uint16_t avg_sound_diff = 0;

    uint16_t adc_reading = 0;
    uint16_t sound_diff = 0;
    
    adc1_config_width(ADC_WIDTH_BIT_12);                            // Configure ADC1 channel 6 (GPIO34) 12-bit resolution
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);     // Attenuation to support full voltage range

    while (1) {
        adc_reading = adc1_get_raw(ADC1_CHANNEL_6);                 // Read raw ADC value (0 - 4095)
        sound_diff = abs(adc_reading - BIAS_VALUE);

        total -= readings[index];                                   // Update moving average
        readings[index] = sound_diff;
        total += readings[index];
        index = (index + 1) % NUM_SAMPLES;

        avg_sound_diff = total / NUM_SAMPLES;
        printf("%d\n", avg_sound_diff);

        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}
