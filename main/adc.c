/* ADC1 Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "data_buffer.h"

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling

static esp_adc_cal_characteristics_t *adc_chars;
static esp_adc_cal_characteristics_t *adc_chars1;
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_channel_t channel1 = ADC_CHANNEL_7;
static const adc_atten_t atten = ADC_ATTEN_DB_0; //brightness
static const adc_atten_t atten1 = ADC_ATTEN_DB_11;//VCC
static const adc_unit_t unit = ADC_UNIT_1;

extern uint8_t app_emergency_light_read_status();

static void check_efuse()
{
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

void adc_task(void *pvParameter){
    //Continuously sample ADC1
	struct def_queue_buffer tmp_data_queue;
	uint32_t mv1_new=0;
	uint32_t mv2_new=0;
	uint8_t status_el_new=0;
	uint16_t cnt=0;
    while (1) {
        uint32_t adc_reading = 0;
        uint32_t adc_reading1 = 0;
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            if (unit == ADC_UNIT_1) {
                adc_reading += adc1_get_raw((adc1_channel_t)channel);
                adc_reading1 += adc1_get_raw((adc1_channel_t)channel1);
            } else {
                int raw;
                adc2_get_raw((adc2_channel_t)channel, ADC_WIDTH_BIT_12, &raw);
                adc_reading += raw;
            }
        }
        adc_reading /= NO_OF_SAMPLES;
        adc_reading1 /= NO_OF_SAMPLES;
        //Convert adc_reading to voltage in mV
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
        printf("Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);
        mv1_new = voltage;
//        tmp_data_queue.mv1 = voltage;

        voltage = esp_adc_cal_raw_to_voltage(adc_reading1, adc_chars1);
        printf("Raw: %d\tVoltage1: %dmV\n", adc_reading1, voltage*2);
        mv2_new = voltage*2;

        status_el_new = app_emergency_light_read_status();
//        tmp_data_queue.mv2 = voltage*2;
        if ((abs(tmp_data_queue.mv1-mv1_new)>5)||(abs(tmp_data_queue.mv2-mv2_new)>10)||(abs(tmp_data_queue.status-status_el_new)>0)||(0==cnt%10))
        {
        	tmp_data_queue.mv1 = mv1_new;
        	tmp_data_queue.mv2 = mv2_new;
        	tmp_data_queue.status = status_el_new;
        	cnt=0;
        	queue_buffer_insert(tmp_data_queue, true);
        	printf("Buffer size = %d\n",queue_buffer_size());
        }
        cnt++;
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    vTaskDelete(NULL);
}

void app_adc()
{
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();

    //Configure ADC
    if (unit == ADC_UNIT_1) {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(channel, atten);
        adc1_config_channel_atten(channel1, atten1);

    } else {
        adc2_config_channel_atten((adc2_channel_t)channel, atten);
    }

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

    adc_chars1 = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    val_type = esp_adc_cal_characterize(unit, atten1, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars1);
    print_char_val_type(val_type);

    if ( xTaskCreate(&adc_task, "adc_task", 1024 * 5, NULL, 5, NULL) != pdPASS ) {
        printf("create adc_task failed\r\n");
    }
}
