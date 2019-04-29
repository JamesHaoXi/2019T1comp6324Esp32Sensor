/*
 * ble_azure_gateway_main.c
 *
 *  Created on: 31 Jan 2019
 *      Author: developer
 */

#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"

extern void app_ble();
extern void app_wifi_azure();
extern void app_timer();
extern void app_gpio_led_flash_control();
extern void gpio_ini();
extern void app_adc();
extern void app_rtc_time();

void app_main()
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    gpio_ini();
    app_gpio_led_flash_control();

    app_wifi_azure();
    app_adc();
    app_rtc_time();
//	app_ble();
    app_timer();

}
