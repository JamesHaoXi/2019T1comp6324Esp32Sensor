/*
 * data_buffer.h
 *
 *  Created on: 30 Jan 2019
 *      Author: developer
 */

#ifndef MAIN_DATA_BUFFER_H_
#define MAIN_DATA_BUFFER_H_

#include "esp_bt_defs.h"
#include "esp_log.h"

struct def_queue_buffer{
	esp_bd_addr_t mac_addr;
	int temperature;
	uint8_t vdd;
	uint8_t status;
	uint32_t	mv1;
	uint32_t 	mv2;
};

struct def_queue_buffer queue_buffer_peek() ;
bool queue_buffer_isEmpty();
bool queue_buffer_isFull();
int queue_buffer_size();
void queue_buffer_insert(struct def_queue_buffer data_queue, bool keep_new);
struct def_queue_buffer queue_buffer_removeData();

#endif /* MAIN_DATA_BUFFER_H_ */
