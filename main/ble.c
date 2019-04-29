/*
 * ble.c
 *
 *  Created on: 31 Jan 2019
 *      Author: developer
 */

/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/



/****************************************************************************
*
* This file is for gatt client. It can scan ble device, connect multiple devices,
* The gattc_multi_connect demo can connect three ble slaves at the same time.
* Modify the name of gatt_server demo named ESP_GATTS_DEMO_a,
* ESP_GATTS_DEMO_b and ESP_GATTS_DEMO_c,then run three demos,the gattc_multi_connect demo will connect
* the three gatt_server demos, and then exchange data.
* Of course you can also modify the code to connect more devices, we default to connect
* up to 4 devices, more than 4 you need to modify menuconfig.
*
****************************************************************************/

/** test criterial
 *  1- one connect/disconnect and read the data
 *  2- two connect/disconnect and read the data
 *  3- Max connect/disconnect and read the data
 *  Function:
 *  1- ble connection
 *  2- insert new data into buffer
 * */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "data_buffer.h"

#define GATTC_TAG "BLE_MULTICONN"
#define REMOTE_SERVICE_T_UUID        0x2B1D  //0x99DA
#define REMOTE_NOTIFY_CHAR_T_UUID    0x2B1D //0xB65B

#define REMOTE_SERVICE_VDD_UUID        0x99DA//0x00FF
#define REMOTE_NOTIFY_CHAR_VDD_UUID    0xB65B


/* register three profiles, each profile corresponds to one connection,
   which makes it easy to handle each connection event */
//#define PROFILE_NUM 6
//#define PROFILE_A_APP_ID 0
//#define PROFILE_A_APP_VDD_ID 1
//#define PROFILE_B_APP_ID 2
//#define PROFILE_B_APP_VDD_ID 3
//#define PROFILE_C_APP_ID 4
//#define PROFILE_C_APP_VDD_ID 5
#define PROFILE_NUM 3
#define PROFILE_A_APP_ID 0
#define PROFILE_B_APP_ID 1
#define PROFILE_C_APP_ID 2

#define INVALID_HANDLE   0

/* Declare static functions */
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_a_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_b_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_c_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

static esp_bt_uuid_t remote_filter_service_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = REMOTE_SERVICE_T_UUID,},
};

static esp_bt_uuid_t remote_filter_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = REMOTE_NOTIFY_CHAR_T_UUID,},
};

static esp_bt_uuid_t remote_filter_char_vdd_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = REMOTE_NOTIFY_CHAR_VDD_UUID,},
};

static esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};

static bool conn_device_a   = false;
static bool conn_device_b   = false;
static bool conn_device_c   = false;

//static bool get_service[PROFILE_NUM] = {false,false,false,false,false,false};

//static bool get_service_a   = false;
//static bool get_service_b   = false;
//static bool get_service_c   = false;
//
//static bool get_service_a_vdd   = false;
//static bool get_service_b_vdd   = false;
//static bool get_service_c_vdd   = false;

static bool Isconnecting    = false;
static bool stop_scan_done  = false;

static esp_gattc_char_elem_t  *char_elem_result_a   = NULL;
static esp_gattc_char_elem_t  *char_elem_result_a_vdd   = NULL;
static esp_gattc_descr_elem_t *descr_elem_result_a  = NULL;
//static esp_gattc_char_elem_t  *char_elem_result_b   = NULL;
//static esp_gattc_descr_elem_t *descr_elem_result_b  = NULL;
//static esp_gattc_char_elem_t  *char_elem_result_c   = NULL;
//static esp_gattc_descr_elem_t *descr_elem_result_c  = NULL;

static const char remote_device_name[3][7] = {"enware", "enware", "enware"};

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,//BLE_SCAN_FILTER_ALLOW_ONLY_WLST,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_ENABLE//BLE_SCAN_DUPLICATE_DISABLE
};

struct gattc_profile_inst {
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t char_handle;
    esp_bd_addr_t remote_bda;
    uint16_t service_vdd_start_handle;
    uint16_t service_vdd_end_handle;
    uint16_t char_vdd_handle;
    bool get_t_service;
    bool get_vdd_service;
    int temperature;
    int vdd;
    uint8_t status;
};

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gattc_cb = gattc_profile_a_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
		.get_t_service = false,
		.get_vdd_service = false,
    },
    [PROFILE_B_APP_ID] = {
        .gattc_cb = gattc_profile_b_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
		.get_t_service = false,
		.get_vdd_service = false,
    },
    [PROFILE_C_APP_ID] = {
        .gattc_cb = gattc_profile_c_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
		.get_t_service = false,
		.get_vdd_service = false,
    },

};

static const char *esp_key_type_to_str(esp_ble_key_type_t key_type)
{
   const char *key_str = NULL;
   switch(key_type) {
    case ESP_LE_KEY_NONE:
        key_str = "ESP_LE_KEY_NONE";
        break;
    case ESP_LE_KEY_PENC:
        key_str = "ESP_LE_KEY_PENC";
        break;
    case ESP_LE_KEY_PID:
        key_str = "ESP_LE_KEY_PID";
        break;
    case ESP_LE_KEY_PCSRK:
        key_str = "ESP_LE_KEY_PCSRK";
        break;
    case ESP_LE_KEY_PLK:
        key_str = "ESP_LE_KEY_PLK";
        break;
    case ESP_LE_KEY_LLK:
        key_str = "ESP_LE_KEY_LLK";
        break;
    case ESP_LE_KEY_LENC:
        key_str = "ESP_LE_KEY_LENC";
        break;
    case ESP_LE_KEY_LID:
        key_str = "ESP_LE_KEY_LID";
        break;
    case ESP_LE_KEY_LCSRK:
        key_str = "ESP_LE_KEY_LCSRK";
        break;
    default:
        key_str = "INVALID BLE KEY TYPE";
        break;

    }
     return key_str;
}

static char *esp_auth_req_to_str(esp_ble_auth_req_t auth_req)
{
   char *auth_str = NULL;
   switch(auth_req) {
    case ESP_LE_AUTH_NO_BOND:
        auth_str = "ESP_LE_AUTH_NO_BOND";
        break;
    case ESP_LE_AUTH_BOND:
        auth_str = "ESP_LE_AUTH_BOND";
        break;
    case ESP_LE_AUTH_REQ_MITM:
        auth_str = "ESP_LE_AUTH_REQ_MITM";
        break;
    case ESP_LE_AUTH_REQ_BOND_MITM:
        auth_str = "ESP_LE_AUTH_REQ_BOND_MITM";
        break;
    case ESP_LE_AUTH_REQ_SC_ONLY:
        auth_str = "ESP_LE_AUTH_REQ_SC_ONLY";
        break;
    case ESP_LE_AUTH_REQ_SC_BOND:
        auth_str = "ESP_LE_AUTH_REQ_SC_BOND";
        break;
    case ESP_LE_AUTH_REQ_SC_MITM:
        auth_str = "ESP_LE_AUTH_REQ_SC_MITM";
        break;
    case ESP_LE_AUTH_REQ_SC_MITM_BOND:
        auth_str = "ESP_LE_AUTH_REQ_SC_MITM_BOND";
        break;
    default:
        auth_str = "INVALID BLE AUTH REQ";
        break;
   }

   return auth_str;
}

/* user defined scan list to record the latest n */
extern void ud_gap_scan_list_add(esp_bd_addr_t bd);
extern uint8_t ud_gap_scan_list_check(esp_bd_addr_t bd);
extern uint8_t ud_compare_bd_addr(esp_bd_addr_t bd1,esp_bd_addr_t bd2);
extern char* ud_bd_addr_to_str(esp_bd_addr_t bd);

/* user defined function*/
static void remove_all_bonded_devices(void)
{
    int dev_num = esp_ble_get_bond_device_num();

    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list);
    for (int i = 0; i < dev_num; i++) {
        esp_ble_remove_bond_device(dev_list[i].bd_addr);

    }

    free(dev_list);
}

static void show_all_bonded_devices(void)
{
    int dev_num = esp_ble_get_bond_device_num();
    ESP_LOGI(GATTC_TAG, "Check bond device number = %d",dev_num);
    esp_bd_addr_t bd_addr;

    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list);
    for (int i = 0; i < dev_num; i++) {
//        esp_ble_remove_bond_device(dev_list[i].bd_addr);

	memcpy(bd_addr, dev_list[i].bd_addr, sizeof(esp_bd_addr_t));
	ESP_LOGI(GATTC_TAG, "remote BD_ADDR%d: %08x%04x",i,\
			(bd_addr[0] << 24) + (bd_addr[1] << 16) + (bd_addr[2] << 8) + bd_addr[3],
			(bd_addr[4] << 8) + bd_addr[5]);
    }

    free(dev_list);
}

static bool check_is_bonded_device(esp_bd_addr_t bd_addr)
{
    int dev_num = esp_ble_get_bond_device_num();
    bool check=false;

    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list);
    for (int i = 0; i < dev_num; i++) {
    	if (ud_compare_bd_addr(dev_list[i].bd_addr,bd_addr)){
    		check = true;
    		break;
    	}

    }

    free(dev_list);
    return check;
}

/**
 * convert gatt data to temperature in 0.1 celsius degree 1~0.1 degree
 * return -30 ~ 100 degree
 *
 * */
static int DataToTemperature(esp_ble_gattc_cb_param_t *p_data){
	int v = 0;
	uint8_t *value;
	value = p_data->notify.value;

	if (p_data->notify.value_len!=2) return 1000;
	v = (value[1]&0x03)*256;
	v += value[0];
	return v;
}

/** convert gatt data to switch data
 * @input: esp_ble_gattc_cb_param_t *p_data
 * @return: 0- off, 1-on, 2- fault
 *
 * */
static uint8_t DataToSwitch(esp_ble_gattc_cb_param_t *p_data){
	if (p_data->notify.value_len!=2) return 2;
	if (p_data->notify.value[1]&0x04) return 1;
	return 0;
}

/**
 * convert gatt data to vdd in 0.001v
 * return 0 ~ 4.0 degree
 *
 * */
static int DataToVdd(esp_ble_gattc_cb_param_t *p_data){
	int v = 0;
	uint8_t *value;
	value = p_data->notify.value;

	if (p_data->notify.value_len!=2) return 0;
	v = value[1]*256;
	v += value[0];
	return v;
}

/**
 * notifyCallback function receive the notify message from server
 * This function is for
 * */
static void notifyCallback(esp_ble_gattc_cb_param_t *p_data){

    ESP_LOGI(GATTC_TAG, "ESP_GATTC_NOTIFY_EVT, Receive notify value: handle= %d value=%d switch=%d",p_data->notify.handle, DataToTemperature(p_data), DataToSwitch(p_data));
//    ESP_LOGI(GATTC_TAG, "ESP_GATTC_NOTIFY_EVT, Receive notify value:");
    esp_log_buffer_hex(GATTC_TAG, p_data->notify.value, p_data->notify.value_len);
    esp_log_buffer_hex("Address",p_data->notify.remote_bda,ESP_BD_ADDR_LEN);

    ud_bd_addr_to_str(p_data->notify.remote_bda);

    //record new data to buffer
    struct def_queue_buffer tmp_data_queue;
    memcpy(tmp_data_queue.mac_addr, p_data->notify.remote_bda, sizeof(tmp_data_queue));

    uint8_t i;
    for(i=0;i<PROFILE_NUM;i++){
    	/* compare the address*/
    	if (ud_compare_bd_addr(p_data->notify.remote_bda, gl_profile_tab[i].remote_bda)){
    		/* check the handle*/
    		if (p_data->notify.handle==gl_profile_tab[i].char_handle){
    			gl_profile_tab[i].status = DataToSwitch(p_data);
    			gl_profile_tab[i].temperature = DataToTemperature(p_data);
    		}else if (p_data->notify.handle==gl_profile_tab[i].char_vdd_handle){
    			gl_profile_tab[i].vdd = DataToVdd(p_data);
    		}
    		/*assemble the latest data*/
    	    tmp_data_queue.status = gl_profile_tab[i].status;
    	    tmp_data_queue.temperature = gl_profile_tab[i].temperature;
    	    tmp_data_queue.vdd = gl_profile_tab[i].vdd;
    	    /* insert the data to queue*/
    	    queue_buffer_insert(tmp_data_queue, true);

    	    ESP_LOGI(GATTC_TAG,"Buffer size:%d", queue_buffer_size());
    	    tmp_data_queue = queue_buffer_peek();
    	    ESP_LOGI(GATTC_TAG,"Temperature:%d", tmp_data_queue.temperature);
    	    break;
    	}
    }
    uint16_t a;
    esp_ble_gap_get_whitelist_size(&a);
    ESP_LOGI(GATTC_TAG,"whitelist_size=%d", a);
    show_all_bonded_devices();
}

static void start_scan(void)
{
    stop_scan_done = false;
    Isconnecting = false;
    uint32_t duration = 1;
    esp_ble_gap_start_scanning(duration);
}

static void gattc_profile_event_handler(bool* conn_device, uint16_t profile_app_id, esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *p_data){
	uint16_t count = 0;
	switch (event) {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(GATTC_TAG, "REG_EVT");
//        esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
//        if (scan_ret){
//            ESP_LOGE(GATTC_TAG, "set scan params error, error code = %x", scan_ret);
//        }

        esp_ble_gap_config_local_privacy(true);//security dev19/02/2019

        break;
    /* one device connect successfully, all profiles callback function will get the ESP_GATTC_CONNECT_EVT,
     so must compare the mac address to check which device is connected, so it is a good choice to use ESP_GATTC_OPEN_EVT. */
    case ESP_GATTC_CONNECT_EVT:
        break;
    case ESP_GATTC_OPEN_EVT:
        if (p_data->open.status != ESP_GATT_OK){
            //open failed, ignore the first device, connect the second device
            ESP_LOGE(GATTC_TAG, "connect device failed, status %d", p_data->open.status);
            *conn_device = false;
            //start_scan();
            break;
        }
        memcpy(gl_profile_tab[profile_app_id].remote_bda, p_data->open.remote_bda, 6);
        gl_profile_tab[profile_app_id].conn_id = p_data->open.conn_id;



        ESP_LOGI(GATTC_TAG, "ESP_GATTC_OPEN_EVT conn_id %d, if %d, status %d, mtu %d", p_data->open.conn_id, gattc_if, p_data->open.status, p_data->open.mtu);
        ESP_LOGI(GATTC_TAG, "REMOTE BDA:");
        esp_log_buffer_hex(GATTC_TAG, p_data->open.remote_bda, sizeof(esp_bd_addr_t));
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->open.conn_id);
        if (mtu_ret){
            ESP_LOGE(GATTC_TAG, "config MTU error, error code = %x", mtu_ret);
        }
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        if (p_data->cfg_mtu.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG,"Config mtu failed");
        }
        ESP_LOGI(GATTC_TAG, "Status %d, MTU %d, conn_id %d", p_data->cfg_mtu.status, p_data->cfg_mtu.mtu, p_data->cfg_mtu.conn_id);
//        esp_ble_gattc_search_service(gattc_if, p_data->cfg_mtu.conn_id, &remote_filter_service_uuid);
        esp_ble_gattc_search_service(gattc_if, p_data->cfg_mtu.conn_id, NULL);//, &remote_filter_service_uuid);
        break;
    case ESP_GATTC_SEARCH_RES_EVT: {
        ESP_LOGI(GATTC_TAG, "SEARCH RES: conn_id = %x is primary service %d", p_data->search_res.conn_id, p_data->search_res.is_primary);
        ESP_LOGI(GATTC_TAG, "start handle %d end handle %d current handle value %d", p_data->search_res.start_handle, p_data->search_res.end_handle, p_data->search_res.srvc_id.inst_id);
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16){
        	if (p_data->search_res.srvc_id.uuid.uuid.uuid16 == REMOTE_SERVICE_T_UUID) {
				ESP_LOGI(GATTC_TAG, "Temperature UUID16: %x", p_data->search_res.srvc_id.uuid.uuid.uuid16);
				gl_profile_tab[profile_app_id].get_t_service = true;
				gl_profile_tab[profile_app_id].service_start_handle = p_data->search_res.start_handle;
				gl_profile_tab[profile_app_id].service_end_handle = p_data->search_res.end_handle;


        	}else if(p_data->search_res.srvc_id.uuid.uuid.uuid16 == REMOTE_SERVICE_VDD_UUID){
				ESP_LOGI(GATTC_TAG, "VDD UUID16: %x", p_data->search_res.srvc_id.uuid.uuid.uuid16);
				gl_profile_tab[profile_app_id].get_vdd_service = true;
				gl_profile_tab[profile_app_id].service_vdd_start_handle = p_data->search_res.start_handle;
				gl_profile_tab[profile_app_id].service_vdd_end_handle = p_data->search_res.end_handle;

        	}
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (p_data->search_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "search service failed, error status = %x", p_data->search_cmpl.status);
            break;
        }

        if (gl_profile_tab[profile_app_id].get_t_service){
//            uint16_t count = 0;
            esp_gatt_status_t status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                     p_data->search_cmpl.conn_id,
                                                                     ESP_GATT_DB_CHARACTERISTIC,
                                                                     gl_profile_tab[profile_app_id].service_start_handle,
                                                                     gl_profile_tab[profile_app_id].service_end_handle,
                                                                     INVALID_HANDLE,
                                                                     &count);
            if (status != ESP_GATT_OK){
                ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
            }
            if (count > 0) {
            	ESP_LOGI(GATTC_TAG, "The T service's char count = %d",count);

                char_elem_result_a = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result_a){
                    ESP_LOGE(GATTC_TAG, "gattc no mem");
                }else {
                    status = esp_ble_gattc_get_char_by_uuid( gattc_if,
                                                             p_data->search_cmpl.conn_id,
                                                             gl_profile_tab[profile_app_id].service_start_handle,
                                                             gl_profile_tab[profile_app_id].service_end_handle,
                                                             remote_filter_char_uuid,
                                                             char_elem_result_a,
                                                             &count);
                    if (status != ESP_GATT_OK){
                        ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_char_by_uuid error");
                    }

                    /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
                    if (count > 0 && (char_elem_result_a[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)){
                        gl_profile_tab[profile_app_id].char_handle = char_elem_result_a[0].char_handle;
                        esp_ble_gattc_register_for_notify (gattc_if, gl_profile_tab[profile_app_id].remote_bda, char_elem_result_a[0].char_handle);
                    }
                }
                /* free char_elem_result */
                free(char_elem_result_a);
            }else{
            	ESP_LOGE(GATTC_TAG, "no char found");
            }
        }
        if(gl_profile_tab[profile_app_id].get_vdd_service){
//            uint16_t count = 0;
            esp_gatt_status_t status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                     p_data->search_cmpl.conn_id,
                                                                     ESP_GATT_DB_CHARACTERISTIC,
                                                                     gl_profile_tab[profile_app_id].service_vdd_start_handle,
                                                                     gl_profile_tab[profile_app_id].service_vdd_end_handle,
                                                                     INVALID_HANDLE,
                                                                     &count);
            if (status != ESP_GATT_OK){
                ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
            }
            if (count > 0) {
            	ESP_LOGI(GATTC_TAG, "The VDD service's char count = %d",count);
                char_elem_result_a_vdd = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result_a_vdd){
                    ESP_LOGE(GATTC_TAG, "gattc no mem");
                }else {
                    status = esp_ble_gattc_get_char_by_uuid( gattc_if,
                                                             p_data->search_cmpl.conn_id,
                                                             gl_profile_tab[profile_app_id].service_vdd_start_handle,
                                                             gl_profile_tab[profile_app_id].service_vdd_end_handle,
                                                             remote_filter_char_vdd_uuid,
															 char_elem_result_a_vdd,
                                                             &count);
                    if (status != ESP_GATT_OK){
                        ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_char_by_uuid error");
                    }

                    /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
                    if (count > 0 && (char_elem_result_a_vdd[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)){
                        gl_profile_tab[profile_app_id].char_vdd_handle = char_elem_result_a_vdd[0].char_handle;
                        esp_ble_gattc_register_for_notify (gattc_if, gl_profile_tab[profile_app_id].remote_bda, char_elem_result_a_vdd[0].char_handle);
                    }
                }
                /* free char_elem_result */
                free(char_elem_result_a_vdd);
            }else{
            	ESP_LOGE(GATTC_TAG, "no char found");
            }
        }
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_SEARCH_CMPL_EVT, start scan.");
        start_scan();//start scan after stopping scan for connection
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        if (p_data->reg_for_notify.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "reg notify failed, error status =%x", p_data->reg_for_notify.status);
            break;
        }
        uint16_t count = 0;
        uint16_t notify_en = 1;
        esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                     gl_profile_tab[profile_app_id].conn_id,
                                                                     ESP_GATT_DB_DESCRIPTOR,
                                                                     gl_profile_tab[profile_app_id].service_start_handle,
                                                                     gl_profile_tab[profile_app_id].service_end_handle,
                                                                     gl_profile_tab[profile_app_id].char_handle,
                                                                     &count);
        if (ret_status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
        }
        if (count > 0){
            descr_elem_result_a = (esp_gattc_descr_elem_t *)malloc(sizeof(esp_gattc_descr_elem_t) * count);
            if (!descr_elem_result_a){
                ESP_LOGE(GATTC_TAG, "malloc error, gattc no mem");
            }else{
                ret_status = esp_ble_gattc_get_descr_by_char_handle( gattc_if,
                                                                     gl_profile_tab[profile_app_id].conn_id,
                                                                     p_data->reg_for_notify.handle,
                                                                     notify_descr_uuid,
                                                                     descr_elem_result_a,
                                                                     &count);
                if (ret_status != ESP_GATT_OK){
                    ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_descr_by_char_handle error");
                }

                /* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result' */
                if (count > 0 && descr_elem_result_a[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result_a[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG){
//writing cause esp32 reset, don't know the reason yet.
//                    ret_status = esp_ble_gattc_write_char_descr( gattc_if,
//                                                                 gl_profile_tab[profile_app_id].conn_id,
//                                                                 descr_elem_result_a[0].handle,
//                                                                 sizeof(notify_en),
//                                                                 (uint8_t *)&notify_en,
//                                                                 ESP_GATT_WRITE_TYPE_RSP,
//                                                                 ESP_GATT_AUTH_REQ_NONE);
                }

                if (ret_status != ESP_GATT_OK){
                    ESP_LOGE(GATTC_TAG, "esp_ble_gattc_write_char_descr error");
                }

                /* free descr_elem_result */
                free(descr_elem_result_a);
            }
        }
        else{
            ESP_LOGE(GATTC_TAG, "decsr not found");
        }

//        ESP_LOGI(GATTC_TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT, start scan.");
//        start_scan();//start scan after stopping scan for connection
        break;
    }
    case ESP_GATTC_NOTIFY_EVT:
//        ESP_LOGI(GATTC_TAG, "ESP_GATTC_NOTIFY_EVT, Receive notify value:");
//        esp_log_buffer_hex(GATTC_TAG, p_data->notify.value, p_data->notify.value_len);
    	notifyCallback(p_data);
        break;
    case ESP_GATTC_WRITE_DESCR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "write descr failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGI(GATTC_TAG, "write descr success");
        uint8_t write_char_data[35];
        for (int i = 0; i < sizeof(write_char_data); ++i)
        {
            write_char_data[i] = i % 256;
        }
        esp_ble_gattc_write_char( gattc_if,
                                  gl_profile_tab[profile_app_id].conn_id,
                                  gl_profile_tab[profile_app_id].char_handle,
                                  sizeof(write_char_data),
                                  write_char_data,
                                  ESP_GATT_WRITE_TYPE_RSP,
                                  ESP_GATT_AUTH_REQ_NONE);
        break;
    case ESP_GATTC_WRITE_CHAR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(GATTC_TAG, "write char failed, error status = %x", p_data->write.status);
        }else{
            ESP_LOGI(GATTC_TAG, "write char success");
        }
//        start_scan();
        break;
    case ESP_GATTC_SRVC_CHG_EVT: {
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(GATTC_TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:%08x%04x",(bda[0] << 24) + (bda[1] << 16) + (bda[2] << 8) + bda[3],
                 (bda[4] << 8) + bda[5]);
        break;
    }
    case ESP_GATTC_DISCONNECT_EVT:
        //Start scanning again
        start_scan();
        if (memcmp(p_data->disconnect.remote_bda, gl_profile_tab[profile_app_id].remote_bda, 6) == 0){
            ESP_LOGI(GATTC_TAG, "device a disconnect");
            *conn_device = false;
            gl_profile_tab[profile_app_id].get_t_service = false;
            gl_profile_tab[profile_app_id].get_vdd_service = false;
        }
        break;
    default:
        break;
    }
}

static void gattc_profile_a_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
	gattc_profile_event_handler(&conn_device_a, PROFILE_A_APP_ID, event, gattc_if, param);

}

static void gattc_profile_b_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
	gattc_profile_event_handler(&conn_device_b, PROFILE_B_APP_ID, event, gattc_if, param);

}

static void gattc_profile_c_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
	gattc_profile_event_handler(&conn_device_c, PROFILE_C_APP_ID, event, gattc_if, param);

}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    switch (event) {
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
         ESP_LOGI(GATTC_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
        break;
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
    	ESP_LOGI(GATTC_TAG, "ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT, Scan start success");
        start_scan();
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        //scan start complete event to indicate scan start successfully or failed
        if (param->scan_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(GATTC_TAG, "Scan start success");
        }else{

            ESP_LOGE(GATTC_TAG, "scan start failed, error status = %x", param->scan_start_cmpl.status);
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
//            esp_log_buffer_hex(GATTC_TAG, scan_result->scan_rst.bda, 6);
//            ESP_LOGI(GATTC_TAG, "Searched Adv Data Len %d, Scan Response Len %d", scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);
//            adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
//                                                ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
//            ESP_LOGI(GATTC_TAG, "Searched Device Name Len %d", adv_name_len);
//            esp_log_buffer_char(GATTC_TAG, adv_name, adv_name_len);
//            ESP_LOGI(GATTC_TAG, "\n");
//            if (Isconnecting){
//                break;
//            }

        	/*user defined 1*/
        	adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
        	if (ud_gap_scan_list_check(scan_result->scan_rst.bda)) {
//        		ESP_LOGI(GATTC_TAG, "Exit in the white list1");
//        		break;
        	}
        	else{
        		ud_gap_scan_list_add(scan_result->scan_rst.bda);
                esp_log_buffer_hex(GATTC_TAG, scan_result->scan_rst.bda, 6);
                ESP_LOGI(GATTC_TAG, "searched Adv Data Len %d, Scan Response Len %d", scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);

                ESP_LOGI(GATTC_TAG, "searched Device Name Len %d", adv_name_len);
                esp_log_buffer_char(GATTC_TAG, adv_name, adv_name_len);
                ESP_LOGI(GATTC_TAG, "\n");
        	}

        	/*user defined 1*/
            if (conn_device_a && conn_device_b && conn_device_c && !stop_scan_done){
                stop_scan_done = true;
                esp_ble_gap_stop_scanning();
                ESP_LOGI(GATTC_TAG, "all devices are connected");
                break;
            }

            if (check_is_bonded_device(scan_result->scan_rst.bda)){

            	ESP_LOGI("TEst", "bonded device");
            	if (conn_device_a == false){
                    conn_device_a = true;
                    ESP_LOGI(GATTC_TAG, "A Searched device %s", remote_device_name[0]);
                    esp_ble_gap_stop_scanning();
                    esp_ble_gattc_open(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                    Isconnecting = true;
            	}else if(conn_device_b == false){
                    conn_device_b = true;
                    ESP_LOGI(GATTC_TAG, "B Searched device %s", remote_device_name[1]);
                    esp_ble_gap_stop_scanning();
                    esp_ble_gattc_open(gl_profile_tab[PROFILE_B_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                    Isconnecting = true;
            	}else if (conn_device_c == false){
                    conn_device_c = true;
                    ESP_LOGI(GATTC_TAG, "C Searched device %s", remote_device_name[2]);
                    esp_ble_gap_stop_scanning();
                    esp_ble_gattc_open(gl_profile_tab[PROFILE_C_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                    Isconnecting = true;
            	}

            }else if (adv_name != NULL) {

                if (strlen(remote_device_name[0]) == adv_name_len && strncmp((char *)adv_name, remote_device_name[0], adv_name_len) == 0 && conn_device_a == false) {
                        conn_device_a = true;
                        ESP_LOGI(GATTC_TAG, "A Searched device %s", remote_device_name[0]);
                        esp_ble_gap_stop_scanning();
                        esp_ble_gattc_open(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                        Isconnecting = true;

                }
                else if (strlen(remote_device_name[1]) == adv_name_len && strncmp((char *)adv_name, remote_device_name[1], adv_name_len) == 0 && conn_device_b == false) {
//                    if (conn_device_b == false) {
                        conn_device_b = true;
                        ESP_LOGI(GATTC_TAG, "B Searched device %s", remote_device_name[1]);
                        esp_ble_gap_stop_scanning();
                        esp_ble_gattc_open(gl_profile_tab[PROFILE_B_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                        Isconnecting = true;

//                    }
                }
                else if (strlen(remote_device_name[2]) == adv_name_len && strncmp((char *)adv_name, remote_device_name[2], adv_name_len) == 0 && conn_device_c == false) {
//                    if (conn_device_c == false) {
                        conn_device_c = true;
                        ESP_LOGI(GATTC_TAG, "C Searched device %s", remote_device_name[2]);
                        esp_ble_gap_stop_scanning();
                        esp_ble_gattc_open(gl_profile_tab[PROFILE_C_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                        Isconnecting = true;
//                    }
//                    break;
                }

            }
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
        	ESP_LOGI(GATTC_TAG, "Scan completed, restarting...");
        	start_scan();
            break;
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(GATTC_TAG, "Scan stop failed");
            break;
        }
        ESP_LOGI(GATTC_TAG, "Stop scan successfully");

        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(GATTC_TAG, "Adv stop failed");
            break;
        }
        ESP_LOGI(GATTC_TAG, "Stop adv successfully");
        break;

    case ESP_GAP_BLE_UPDATE_WHITELIST_COMPLETE_EVT:
    	ESP_LOGI(GATTC_TAG, "ESP_GAP_BLE_UPDATE_WHITELIST_COMPLETE_EVT");
    	break;

    case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT:
        if (param->local_privacy_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(GATTC_TAG, "config local privacy failed, error code =%x", param->local_privacy_cmpl.status);
            break;
        }
        esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
        if (scan_ret){
            ESP_LOGE(GATTC_TAG, "set scan params error, error code = %x", scan_ret);
        }
    	break;

    case ESP_GAP_BLE_PASSKEY_REQ_EVT:                           /* passkey request event */
        /* Call the following function to input the passkey which is displayed on the remote device */
        //esp_ble_passkey_reply(gl_profile_tab[PROFILE_A_APP_ID].remote_bda, true, 0x00);
        ESP_LOGI(GATTC_TAG, "ESP_GAP_BLE_PASSKEY_REQ_EVT");
        break;
    case ESP_GAP_BLE_OOB_REQ_EVT: {
        ESP_LOGI(GATTC_TAG, "ESP_GAP_BLE_OOB_REQ_EVT");
        uint8_t tk[16] = {1}; //If you paired with OOB, both devices need to use the same tk
        esp_ble_oob_req_reply(param->ble_security.ble_req.bd_addr, tk, sizeof(tk));
        break;
    }
    case ESP_GAP_BLE_LOCAL_IR_EVT:                               /* BLE local IR event */
        ESP_LOGI(GATTC_TAG, "ESP_GAP_BLE_LOCAL_IR_EVT");
        break;
    case ESP_GAP_BLE_LOCAL_ER_EVT:                               /* BLE local ER event */
        ESP_LOGI(GATTC_TAG, "ESP_GAP_BLE_LOCAL_ER_EVT");
        break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
        /* send the positive(true) security response to the peer device to accept the security request.
        If not accept the security request, should sent the security response with negative(false) accept value*/
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
    case ESP_GAP_BLE_NC_REQ_EVT:
        /* The app will receive this evt when the IO has DisplayYesNO capability and the peer device IO also has DisplayYesNo capability.
        show the passkey number to the user to confirm it with the number displayed by peer device. */
        esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
        ESP_LOGI(GATTC_TAG, "ESP_GAP_BLE_NC_REQ_EVT, the passkey Notify number:%d", param->ble_security.key_notif.passkey);
        break;
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:  ///the app will receive this evt when the IO  has Output capability and the peer device IO has Input capability.
        ///show the passkey number to the user to input it in the peer deivce.
        ESP_LOGI(GATTC_TAG, "The passkey Notify number:%06d", param->ble_security.key_notif.passkey);
        break;
    case ESP_GAP_BLE_KEY_EVT:
        //shows the ble key info share with peer device to the user.
        ESP_LOGI(GATTC_TAG, "key type = %s", esp_key_type_to_str(param->ble_security.ble_key.key_type));
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
        esp_bd_addr_t bd_addr;
        memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
        ESP_LOGI(GATTC_TAG, "remote BD_ADDR: %08x%04x",\
                (bd_addr[0] << 24) + (bd_addr[1] << 16) + (bd_addr[2] << 8) + bd_addr[3],
                (bd_addr[4] << 8) + bd_addr[5]);
        ESP_LOGI(GATTC_TAG, "address type = %d", param->ble_security.auth_cmpl.addr_type);
        ESP_LOGI(GATTC_TAG, "pair status = %s",param->ble_security.auth_cmpl.success ? "success" : "fail");
        if (!param->ble_security.auth_cmpl.success) {
            ESP_LOGI(GATTC_TAG, "fail reason = 0x%x",param->ble_security.auth_cmpl.fail_reason);
            //remove from whitelist
            esp_ble_gap_update_whitelist(false, bd_addr);
            esp_ble_gap_disconnect(bd_addr);

        } else {
            ESP_LOGI(GATTC_TAG, "auth mode = %s",esp_auth_req_to_str(param->ble_security.auth_cmpl.auth_mode));
            //add to whitelist
            esp_ble_gap_update_whitelist(true, bd_addr);
        }
        break;
    }
    default:
        break;
    }
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    //ESP_LOGI(GATTC_TAG, "EVT %d, gattc if %d, app_id %d", event, gattc_if, param->reg.app_id);

    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        } else {
            ESP_LOGI(GATTC_TAG, "Reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    /* If the gattc_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gattc_if == gl_profile_tab[idx].gattc_if) {
                if (gl_profile_tab[idx].gattc_cb) {
                    gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
                }
            }
        }
    } while (0);
}

void app_ble()
{
//    esp_err_t ret = nvs_flash_init();
//    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//        ESP_ERROR_CHECK(nvs_flash_erase());
//        ret = nvs_flash_init();
//    }
//    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(GATTC_TAG, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    //register the  callback function to the gap module
    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret){
        ESP_LOGE(GATTC_TAG, "gap register error, error code = %x", ret);
        return;
    }

    //register the callback function to the gattc module
    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
    if(ret){
        ESP_LOGE(GATTC_TAG, "gattc register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gattc_app_register(PROFILE_A_APP_ID);
    if (ret){
        ESP_LOGE(GATTC_TAG, "gattc app register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gattc_app_register(PROFILE_B_APP_ID);
    if (ret){
        ESP_LOGE(GATTC_TAG, "gattc app register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gattc_app_register(PROFILE_C_APP_ID);
    if (ret){
        ESP_LOGE(GATTC_TAG, "gattc app register error, error code = %x", ret);
        return;
    }
    ret = esp_ble_gatt_set_local_mtu(200);
    if (ret){
        ESP_LOGE(GATTC_TAG, "set local  MTU failed, error code = %x", ret);
    }

    /* set the security iocap & auth_req & key size & init key response key parameters to the stack*/
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;     //bonding with peer device after authentication
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;           //set the IO capability to No output No input
    uint8_t key_size = 16;      //the key size should be 7~16 bytes
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t oob_support = ESP_BLE_OOB_DISABLE;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_support, sizeof(uint8_t));
    /* If your BLE device act as a Slave, the init_key means you hope which types of key of the master should distribut to you,
    and the response key means which key you can distribut to the Master;
    If your BLE device act as a master, the response key means you hope which types of key of the slave should distribut to you,
    and the init key means which key you can distribut to the slave. */
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

}

bool ble_check_connection(){
	return (conn_device_a||conn_device_b||conn_device_c);
}
