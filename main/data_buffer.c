/*
 * data_buffer.c
 *
 *  Created on: 30 Jan 2019
 *      Author: developer
 *
 *  data_buffer is a FIFO data queue and interface for ble and azure
 *  data: MAC address as uuid, temperature in 0.1 degree, vdd in 0.1v, status,
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "data_buffer.h"

#define QUEUE_BUFFER_MAX 6

static struct def_queue_buffer queue_buffer[QUEUE_BUFFER_MAX];

static uint8_t head = 0;
static uint8_t tail = 0;
static uint8_t itemCount = 0;

struct def_queue_buffer queue_buffer_peek() {
	struct def_queue_buffer a_queue;
	memcpy(&a_queue, &queue_buffer[head], sizeof(a_queue));
//	ESP_LOGI("DEBUG", "queue_buffer_peek: T=%d",queue_buffer[head].temperature);

	return a_queue;
}

bool queue_buffer_isEmpty() {
//   return itemCount == 0;
	return head==tail;
}

bool queue_buffer_isFull() {
	uint8_t tail_i;

	tail_i = tail + 1 ;
	if (tail_i>=QUEUE_BUFFER_MAX) tail_i=0;

	if (head==tail_i) return true;

//   return itemCount == QUEUE_BUFFER_MAX;
	return false;
}

int queue_buffer_size() {

	if (tail>head) return tail-head;

   return tail+QUEUE_BUFFER_MAX-head;
}

/**
 * Insert the new data into queue
 * input: 1-data_queue: new datas , 2-keep_new: keep the latest data always or ignore the new data if the queue is full
 *
 * */
void queue_buffer_insert(struct def_queue_buffer data_queue, bool keep_new) {

	if (queue_buffer_isFull() && keep_new){
		queue_buffer_removeData();
	}

	if(!queue_buffer_isFull()) {

	  memcpy(&queue_buffer[tail++], &data_queue, sizeof(data_queue));

	  if(tail >= QUEUE_BUFFER_MAX) {
		 tail = 0;
	  }
//	  if (tail>0) ESP_LOGI("DEBUG", "buffer insert: T=%d",queue_buffer[tail-1].temperature);
//	  else ESP_LOGI("DEBUG", "buffer insert: T=%d",queue_buffer[QUEUE_BUFFER_MAX-1].temperature);

	  itemCount++;
	}
}

struct def_queue_buffer queue_buffer_removeData() {
	struct def_queue_buffer a_queue;

	if (queue_buffer_isEmpty()) return a_queue;

	memcpy(&a_queue, &queue_buffer[head++], sizeof(a_queue));

	if(head >= QUEUE_BUFFER_MAX) {
		head = 0;
	}

	itemCount--;
	return a_queue;
}

