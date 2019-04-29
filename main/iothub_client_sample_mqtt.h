/*
 * iothub_client_sample_mqtt.h
 *
 *  Created on: 25 Jan 2019
 *      Author: developer
 */

#ifndef IOTHUB_CLIENT_SAMPLE_MQTT_H
#define IOTHUB_CLIENT_SAMPLE_MQTT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
extern EventGroupHandle_t s_wifi_event_group;
extern const int CONNECTED_BIT;
extern const int ESPTOUCH_DONE_BIT;

void iothub_client_sample_mqtt_run(void);

#ifdef __cplusplus
}
#endif

#endif /* IOTHUB_CLIENT_SAMPLE_MQTT_H */
