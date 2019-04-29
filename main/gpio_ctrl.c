/*
 * gpio_ctrl.c
 *
 *  Created on: 4 Mar 2019
 *      Author: developer
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

/**
 * Brief:
 * This test code shows how to configure gpio and how to use gpio interrupt.
 *
 * GPIO status:
 * GPIO18: output
 * GPIO19: output
 * GPIO4:  input, pulled up, interrupt from rising edge and falling edge
 * GPIO5:  input, pulled up, interrupt from rising edge.
 *
 * Test:
 * Connect GPIO18 with GPIO4
 * Connect GPIO19 with GPIO5
 * Generate pulses on GPIO18/19, that triggers interrupt on GPIO4/5
 *
 */

#define GPIO_OUTPUT_IO_0    0  //GPIO0
#define GPIO_OUTPUT_IO_1    4  //GPIO4
#define GPIO_OUTPUT_IO_2    16 //GPIO16
#define GPIO_OUTPUT_IO_TEST_BUTTON	2	//GPIO2
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1) | (1ULL<<GPIO_OUTPUT_IO_2) | (1ULL<<GPIO_OUTPUT_IO_TEST_BUTTON))
//#define GPIO_INPUT_IO_0     4
//#define GPIO_INPUT_IO_1     5
//#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1))
//#define ESP_INTR_FLAG_DEFAULT 0
#define TIMER_EL_TEST_TIMEOUT_SEC	(60) //emergency light test time out is 90minutes x60 = 5400s, set it to 1 minute for test

//static xQueueHandle gpio_evt_queue = NULL;
//
//static void IRAM_ATTR gpio_isr_handler(void* arg)
//{
//    uint32_t gpio_num = (uint32_t) arg;
//    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
//}
//
//static void gpio_task_example(void* arg)
//{
//    uint32_t io_num;
//    for(;;) {
//        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
//            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
//        }
//    }
//}

extern bool iothub_check_connection();
extern bool wifi_check_connection();
extern bool ble_check_connection();
extern void eve_timer_trigger(double alarm_time);
extern bool end_of_test_enable_check(void);

void app_el_test_manager();

uint8_t status_el_gi=0;

void gpio_ini()
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

//    //interrupt of rising edge
//    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
//    //bit mask of the pins, use GPIO4/5 here
//    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
//    //set as input mode
//    io_conf.mode = GPIO_MODE_INPUT;
//    //enable pull-up mode
//    io_conf.pull_up_en = 1;
//    gpio_config(&io_conf);

    //change gpio intrrupt type for one pin
//    gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);

//    //create a queue to handle gpio event from isr
//    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
//    //start gpio task
//    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);
//
//    //install gpio isr service
//    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
//    //hook isr handler for specific gpio pin
//    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);
//    //hook isr handler for specific gpio pin
//    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void*) GPIO_INPUT_IO_1);
//
//    //remove isr handler for gpio number.
//    gpio_isr_handler_remove(GPIO_INPUT_IO_0);
//    //hook isr handler for specific gpio pin again
//    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);

//    int cnt = 0;
//    while(cnt<100) {+
//        printf("cnt: %d\n", cnt++);
//        vTaskDelay(1000 / portTICK_RATE_MS);
//        gpio_set_level(GPIO_OUTPUT_IO_0, cnt % 2);
//        gpio_set_level(GPIO_OUTPUT_IO_1, 1-cnt % 2);
//    }
    status_el_gi=0;
}

void led_flash_control(void *pvParameter){
	int cnt=0;
	while(1){
		if (ble_check_connection()&&(cnt%3==0)) gpio_set_level(GPIO_OUTPUT_IO_0, 0);
		if (wifi_check_connection()&&(cnt%3==1)) gpio_set_level(GPIO_OUTPUT_IO_1, 0);
		if ((!iothub_check_connection())&&(cnt%3==2)) gpio_set_level(GPIO_OUTPUT_IO_2, 0);

		vTaskDelay(100 / portTICK_RATE_MS);
		gpio_set_level(GPIO_OUTPUT_IO_0, 1);
		gpio_set_level(GPIO_OUTPUT_IO_1, 1);
		gpio_set_level(GPIO_OUTPUT_IO_2, 1);

		vTaskDelay(900 / portTICK_RATE_MS);
		cnt++;
		app_el_test_manager();
	}
	vTaskDelete(NULL);
}

void app_gpio_led_flash_control(){
//	gpio_ini();
    if ( xTaskCreate(&led_flash_control, "led_ctrl_task", 1024 * 5, NULL, 5, NULL) != pdPASS ) {
        printf("create led_ctrl_task failed\r\n");
    }
}

// emergency light control button
void app_emergency_light_test_button(bool on){
	if (on)
	{
		gpio_set_level(GPIO_OUTPUT_IO_TEST_BUTTON,1);
		status_el_gi=1;
		eve_timer_trigger(TIMER_EL_TEST_TIMEOUT_SEC);//start timer for end of test
	}
	else
	{
		gpio_set_level(GPIO_OUTPUT_IO_TEST_BUTTON,0);
		status_el_gi=0;
	}
}

uint8_t app_emergency_light_read_status(){
	return status_el_gi;
}

/*
 * switch off the test if it is end of test
 * */
void app_el_test_manager()
{
	if (end_of_test_enable_check())
	{
		app_emergency_light_test_button(false);
	}

}
