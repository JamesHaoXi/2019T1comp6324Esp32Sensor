/*
 * scan_list.c
 *
 *  Created on: 24 Jan 2019
 *      Author: developer
 */

#include <stdio.h>
#include <string.h>
#include "esp_bt_defs.h"
#include "esp_log.h"

#define	SCAN_LIST_LENGTH	20

struct ud_gap_scan_list{
	uint16_t	n_start;
	uint16_t	n_end;
	esp_bd_addr_t ud_scan_list[SCAN_LIST_LENGTH];
};

static struct ud_gap_scan_list udud_gap_scan_list_uuid=
{
	.n_start = 0,
	.n_end = 0,
};

void ud_gap_scan_list_add(esp_bd_addr_t bd)
{
	memcpy(udud_gap_scan_list_uuid.ud_scan_list[udud_gap_scan_list_uuid.n_end], bd, sizeof(esp_bd_addr_t));
	udud_gap_scan_list_uuid.n_end++;
	if (udud_gap_scan_list_uuid.n_end>=SCAN_LIST_LENGTH){
		udud_gap_scan_list_uuid.n_end = 0;
	}
}

/** compare two bluetooth addresses
 *  input: mac address 1, mac address 2
 *  return: 0- not matched, 1- matched
 * */
uint8_t ud_compare_bd_addr(esp_bd_addr_t bd1,esp_bd_addr_t bd2)
{

	uint16_t i,n_i;
	n_i = sizeof(esp_bd_addr_t);

	for(i=0;i<n_i;i++)
	{
		if (bd1[i]!=bd2[i]) return 0;
	}

	return 1;

}
/*check if the device in the list*/
uint8_t ud_gap_scan_list_check(esp_bd_addr_t bd)
{
	uint8_t i;

	for(i=0;i<SCAN_LIST_LENGTH;i++)
	{
		if (ud_compare_bd_addr(bd,udud_gap_scan_list_uuid.ud_scan_list[i])==1) return 1;
	}

	return 0;

}

/** char_to_hex
 * */
static char* ud_buffer_to_hex(uint8_t in){
	static char out[3];

	out[0]=in/16;
	if (out[0]<10) out[0]+=0x30;
	else out[0]+=0x60-0x9;

	out[1]=in%16;
	if (out[1]<10) out[1]+=0x30;
	else out[1]+=0x60-0x9;

	out[2]=0;
	return out;
}

/* address convertor
 * convert the esp_bd_addr_t to mac string "XX:XX:XX:XX:XX:XX"
 * */
char* ud_bd_addr_to_str(esp_bd_addr_t bd){
	char addr_char[ESP_BD_ADDR_LEN];
	static char addr_str[ESP_BD_ADDR_LEN*3];
	memcpy(addr_char, bd, ESP_BD_ADDR_LEN);

	for(int i=0;i<ESP_BD_ADDR_LEN;i++){
		if (i+1==ESP_BD_ADDR_LEN) sprintf(addr_str+3*i,"%02x",addr_char[i]);
		else sprintf(addr_str+3*i,"%02x:",addr_char[i]);
	}

//	esp_log_buffer_hex("Address DEBUG",bd,ESP_BD_ADDR_LEN);
//	ESP_LOGI("Address Debug","%s",addr_str);

	return addr_str;
}
