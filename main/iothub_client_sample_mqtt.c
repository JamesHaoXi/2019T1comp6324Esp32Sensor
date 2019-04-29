/*
 * iothub_client_sample_mqtt.c
 *
 *  Created on: 25 Jan 2019
 *      Author: developer
 */

// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <stdlib.h>

#include "iothub_client.h"
#include "iothub_device_client_ll.h"
#include "iothub_client_options.h"
#include "iothub_message.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "iothubtransportmqtt.h"
#include "iothub_client_options.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iothub_client_sample_mqtt.h"
#include "errno.h"

#include "data_buffer.h"

#ifdef MBED_BUILD_TIMESTAMP
    #define SET_TRUSTED_CERT_IN_SAMPLES
#endif // MBED_BUILD_TIMESTAMP

#ifdef SET_TRUSTED_CERT_IN_SAMPLES
    #include "certs.h"
#endif // SET_TRUSTED_CERT_IN_SAMPLES
/**
 * The message format for azure IoT hub
 * MSG_TXT = "{\"deviceId\": \"%s\",\"temperature\": %f,\"vdd\": %f,\"status\": %f}"
 * */


/*String containing Hostname, Device Id & Device Key in the format:                         */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessKey=<device_key>"                */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessSignature=<device_sas_token>"    */
/* "HostName=james6342iot.azure-devices.net;DeviceId=james6342IoTGateway;SharedAccessKey=kd6AGzIw9veujIikbTfnrWRp1iR0g6a7/ZtndTRjbOs="*/
#define EXAMPLE_IOTHUB_CONNECTION_STRING "HostName=james6342iot.azure-devices.net;DeviceId=james6342IoTGateway;SharedAccessKey=kd6AGzIw9veujIikbTfnrWRp1iR0g6a7/ZtndTRjbOs="
static const char* connectionString = EXAMPLE_IOTHUB_CONNECTION_STRING;
static const char* test_id = "FF:FF:FF:FF:FF:FF";

static int callbackCounter;
static char msgText[1024];
static char propText[1024];
static bool g_continueRunning;
#define MESSAGE_COUNT 50
#define DOWORK_LOOP_NUM     3

static bool iothub_connection = false;

typedef struct EVENT_INSTANCE_TAG
{
    IOTHUB_MESSAGE_HANDLE messageHandle;
    size_t messageTrackingId;  // For tracking the messages within the user callback.
} EVENT_INSTANCE;

extern struct def_queue_buffer queue_buffer_removeData();
extern char* ud_bd_addr_to_str(esp_bd_addr_t bd);
extern char* local_time_read();
extern void app_emergency_light_test_button(bool on);

static IOTHUBMESSAGE_DISPOSITION_RESULT ReceiveMessageCallback(IOTHUB_MESSAGE_HANDLE message, void* userContextCallback)
{
    int* counter = (int*)userContextCallback;
    const char* buffer;
    size_t size;
    MAP_HANDLE mapProperties;
    const char* messageId;
    const char* correlationId;

    // Message properties
    if ((messageId = IoTHubMessage_GetMessageId(message)) == NULL)
    {
        messageId = "<null>";
    }

    if ((correlationId = IoTHubMessage_GetCorrelationId(message)) == NULL)
    {
        correlationId = "<null>";
    }

    // Message content
    if (IoTHubMessage_GetByteArray(message, (const unsigned char**)&buffer, &size) != IOTHUB_MESSAGE_OK)
    {
        (void)printf("unable to retrieve the message data\r\n");
    }
    else
    {
        (void)printf("Received Message [%d]\r\n Message ID: %s\r\n Correlation ID: %s\r\n Data: <<<%.*s>>> & Size=%d\r\n", *counter, messageId, correlationId, (int)size, buffer, (int)size);
        // If we receive the work 'quit' then we stop running
        if (size == (strlen("quit") * sizeof(char)) && memcmp(buffer, "quit", size) == 0)
        {
 //           g_continueRunning = false;
            (void)printf("*** quit received but we wouldn't quit!\r\n");
        }

        if (size == (strlen("TEST ON") * sizeof(char)) && memcmp(buffer, "TEST ON", size) == 0)
        {
//            g_continueRunning = false;
        	app_emergency_light_test_button(true);
            (void)printf("*** Test on command received!\r\n");
        }
        else if (size == (strlen("TEST OFF") * sizeof(char)) && memcmp(buffer, "TEST OFF", size) == 0)
        {
        	app_emergency_light_test_button(false);
        }

    }

    // Retrieve properties from the message
    mapProperties = IoTHubMessage_Properties(message);
    if (mapProperties != NULL)
    {
        const char*const* keys;
        const char*const* values;
        size_t propertyCount = 0;
        if (Map_GetInternals(mapProperties, &keys, &values, &propertyCount) == MAP_OK)
        {
            if (propertyCount > 0)
            {
                size_t index;

                printf(" Message Properties:\r\n");
                for (index = 0; index < propertyCount; index++)
                {
                    (void)printf("\tKey: %s Value: %s\r\n", keys[index], values[index]);
                }
                (void)printf("\r\n");
            }
        }
    }

    /* Some device specific action code goes here... */
    (*counter)++;
    return IOTHUBMESSAGE_ACCEPTED;
}

static void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* userContextCallback)
{
    EVENT_INSTANCE* eventInstance = (EVENT_INSTANCE*)userContextCallback;
    size_t id = eventInstance->messageTrackingId;

    (void)printf("Confirmation[%d] received for message tracking id = %d with result = %s\r\n", callbackCounter, (int)id, ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));
    /* Some device specific action code goes here... */
    callbackCounter++;

    IoTHubMessage_Destroy(eventInstance->messageHandle);
}

void iothub_client_sample_mqtt_run(void)
{
    IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;

    EVENT_INSTANCE messages[MESSAGE_COUNT];
    MAP_HANDLE propMap;

    g_continueRunning = true;
    srand((unsigned int)time(NULL));
//    double avgWindSpeed = 10.0;
//    double minTemperature = 20.0;
//    double minHumidity = 60.0;


    struct def_queue_buffer tmp_data_queue;

    callbackCounter = 0;
    int iterator = 0;
    int receiveContext = 0;
    while (1) {
        xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT|ESPTOUCH_DONE_BIT,
                        false, false, portMAX_DELAY);
        if (platform_init() != 0)
        {
            (void)printf("Failed to initialize the platform.\r\n");
            break;
        }
        else
        {
            if ((iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, MQTT_Protocol)) == NULL)
            {
                (void)printf("ERROR: iotHubClientHandle is NULL!\r\n");
            }
            else
            {
                bool traceOn = true;
                IoTHubClient_LL_SetOption(iotHubClientHandle, OPTION_LOG_TRACE, &traceOn);

                // Setting the Trusted Certificate.  This is only necessary on system with without
                // built in certificate stores.
#ifdef SET_TRUSTED_CERT_IN_SAMPLES
                IoTHubDeviceClient_LL_SetOption(iotHubClientHandle, OPTION_TRUSTED_CERT, certificates);
#endif // SET_TRUSTED_CERT_IN_SAMPLES

                /* Setting Message call back, so we can receive Commands. */
                if (IoTHubClient_LL_SetMessageCallback(iotHubClientHandle, ReceiveMessageCallback, &receiveContext) != IOTHUB_CLIENT_OK)
                {
                    (void)printf("ERROR: IoTHubClient_LL_SetMessageCallback..........FAILED!\r\n");
                }
                else
                {
                    (void)printf("IoTHubClient_LL_SetMessageCallback...successful.\r\n");
                    iothub_connection = true; //iothub connected successfully
                    /* Now that we are ready to receive commands, let's send some messages */

                    double brightness = 20;
                    double vdd = 0;
//                    double humidity = 0;
                    uint8_t status_i = 0;

                    do
                    {
//                        (void)printf("iterator: [%d], callbackCounter: [%d]. \r\n", iterator, callbackCounter);
//                        ThreadAPI_Sleep(100);
//                        if (iterator < MESSAGE_COUNT && (iterator <= callbackCounter))
                        if (!(queue_buffer_isEmpty())&&(iterator <= callbackCounter))
                        {
                        	/*iterator reset if it is greater than MESSAGE_COUNT*/
                        	if (iterator>=MESSAGE_COUNT){
                        		callbackCounter-=iterator;
                        		iterator=0;
                        	}

                        	tmp_data_queue = queue_buffer_peek();

                        	brightness = tmp_data_queue.mv1;//minTemperature + (rand() % 10);
                            vdd = tmp_data_queue.mv2;//3.0 + ((rand()%10)/10.0);
                            status_i = tmp_data_queue.status;

//                            humidity = minHumidity +  (rand() % 20);
                            //"{\"deviceId\": \"%s\",\"temperature\": %f,\"vdd\": %f,\"status\": %f}"
//                            sprintf_s(msgText, sizeof(msgText), "{\"deviceId\":\"myFirstDevice\",\"windSpeed\":%.2f,\"temperature\":%.2f,\"humidity\":%.2f}", avgWindSpeed + (rand() % 4 + 2), temperature, humidity);
                            sprintf_s(msgText, sizeof(msgText), "{\"deviceId\": \"%s\",\"timestamp\": \"%s\",\"brightness\": %.1f,\"vdd\": %.1f,\"status\": %d}", test_id, local_time_read(),brightness, vdd, status_i);
                            if ((messages[iterator].messageHandle = IoTHubMessage_CreateFromByteArray((const unsigned char*)msgText, strlen(msgText))) == NULL)
                            {
                                (void)printf("ERROR: iotHubMessageHandle is NULL!\r\n");
                                iothub_connection = false;
                            }
                            else
                            {
                                messages[iterator].messageTrackingId = iterator;
//                                propMap = IoTHubMessage_Properties(messages[iterator].messageHandle);
//                                (void)sprintf_s(propText, sizeof(propText), brightness > 28 ? "true" : "false");
//                                if (Map_AddOrUpdate(propMap, "brightnessAlert", propText) != MAP_OK)
//                                {
//                                    (void)printf("ERROR: Map_AddOrUpdate Failed!\r\n");
//                                    iothub_connection = false;
//                                }

                                if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle, messages[iterator].messageHandle, SendConfirmationCallback, &messages[iterator]) != IOTHUB_CLIENT_OK)
                                {
                                    (void)printf("ERROR: IoTHubClient_LL_SendEventAsync..........FAILED!\r\n");
                                    iothub_connection = false;
                                }
                                else
                                {
                                	iothub_connection = true;
                                    (void)printf("IoTHubClient_LL_SendEventAsync accepted message [%d] for transmission to IoT Hub.\r\n", (int)iterator);
                                    tmp_data_queue = queue_buffer_removeData();
                                    ThreadAPI_Sleep(1000);//sleep to wait for the cloud platform apps
                                }
                            }
                            iterator++;

                        }
                        IoTHubClient_LL_DoWork(iotHubClientHandle);
                        ThreadAPI_Sleep(10);
                        if (errno == ECONNABORTED || errno == ECONNRESET) {
                        	printf("exit\n");
                            break;
                        }

//                        if (callbackCounter >= MESSAGE_COUNT)
//                        {
//                            printf("exit\n");
//                            break;
//                        }
                    } while (g_continueRunning);

                    if (callbackCounter >= MESSAGE_COUNT) {
                        (void)printf("iothub_client_sample_mqtt has gotten quit message, call DoWork %d more time to complete final sending...\r\n", DOWORK_LOOP_NUM);
                        size_t index = 0;
                        for (index = 0; index < DOWORK_LOOP_NUM; index++)
                        {
                            IoTHubClient_LL_DoWork(iotHubClientHandle);
                            ThreadAPI_Sleep(1);
                        }
                    }
                }

                IoTHubClient_LL_Destroy(iotHubClientHandle);
            }
            platform_deinit();
            if (callbackCounter > 0 && callbackCounter < MESSAGE_COUNT)
            {
                continue;
            }
            else
            {
                break;
            }
        }
    }
}

bool iothub_check_connection(){
	return iothub_connection;
}

//int main(void)
//{
//    iothub_client_sample_mqtt_run();
//    return 0;
//}
