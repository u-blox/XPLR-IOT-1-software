/*
 * Copyright 2022 u-blox Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef X_BLE_CMD_H__
#define  X_BLE_CMD_H__


/** @file
 * @brief This file contains the API for the BLE command handler of Sensor
 * Aggregation firmware for XPLR-IOT-1. The firmware can be used together with
 * a mobile application which connects to XPLR-IOT-1 via BLE. The application
 * can send commands to XPLR-IOT-1 for a variety of tasks, mainly having to do 
 * with the provision of the device (insert Wifi,MQTT credentials etc.)
 * 
 * Incoming data fed to xBleCmdHandle function are considered as incoming commands
 * from u-blox mobile application. Incoming data are interpreted from
 * xBleCmdHandle and executed. More details on how this is done can be found
 * in the file description of the corresponding source file "x_ble_cmd.h"
 * and xBleCmdHandle description.
 * 
 * This module works on top of the communication protocol described in
 * "mobile_app_ble_protocol.h"
 */


#include <stdint.h>
#include "x_errno.h"

#include "mobile_app_ble_protocol.h"  // X_BLE_CMD_PROT_CMD_HEADER_SIZE
#include "x_wifi_ninaW156.h"          // WIFI_MAX_SSID_LEN
#include "x_wifi_mqtt.h"              // MQTT_DEVICE_ID_MAXLEN etc...
#include "x_cell_mqttsn.h"            // MQTTSN_CLIENT_ID_MAXLEN



/* ----------------------------------------------------------------
 * DEFINITIONS FOR THIS MODULE ALONE
 * -------------------------------------------------------------- */

/* Thingstream Domain info is only used in the BLE commands to help in 
 * mobile app functionality, and not on Firmware MQTT(SN) configuration.
 * That is why it is defined in this module.
*/

#define THINGSTREAM_DOMAIN_MAX_LEN      200
#define THINGSTREAM_DOMAIN_MIN_LEN      1


/* ----------------------------------------------------------------
 * TYPE DEFINITIONS
 * -------------------------------------------------------------- */

/** This union is basically used to help us define the maximum size of
 *  expected useful data payloads within received commands or payloads
 *  of useful data in responses to BLE commands.
 *  
 *  The reason why it is defined in the header is that it is used to 
 *  define the Maximum size of incoming/outcoming BLE data buffers, used
 *  in other modules. 
*/
typedef union {

    char wifiSSID[ WIFI_MAX_SSID_LEN ];                      /**< WiFi SSID string. In/Out(Reponse)*/
    uint8_t wifiPSW[ WIFI_MAX_PSW_LEN ];                     /**< WiFi password. Incoming only*/
    uint8_t mqttDeviceId[ MQTT_DEVICE_ID_MAXLEN ];           /**< MQTT (IP Thing) Device Id. In/Out(Reponse)*/ 
    uint8_t mqttUsername[ MQTT_USERNAME_MAXLEN ];            /**< MQTT (IP Thing) Username. Incoming only*/
    uint8_t mqttPassword[ MQTT_PASSWORD_MAXLEN ];            /**< MQTT (IP Thing) Password. Incoming only*/
    uint8_t mqttSnClientId[ MQTTSN_CLIENT_ID_MAXLEN ];       /**< MQTTSN (SIM Thing) Client Id. In/Out(Reponse)*/
    uint8_t ThingstreamDomain[ THINGSTREAM_DOMAIN_MAX_LEN ]; /**< Thingstream Domain. In/Out(Reponse)*/
}xBleCmdPayloads_t;


/* ----------------------------------------------------------------
 * TYPE DEFINITIONS FOR OTHER MODULES
 * -------------------------------------------------------------- */

/** Maximum expected incoming BLE command size, with payload.
 *  Can be used to define incoming data buffer size */
#define X_BLE_CMD_IN_MAX_SIZE ( M_BLE_PROT_CMD_SIZE_MIN + sizeof(xBleCmdPayloads_t) )

/** Maximum expected outcoming BLE response size, with payload.
 *  Can be used to define outcoming data buffer size */
#define X_BLE_CMD_OUT_MAX_SIZE ( M_BLE_PROT_RSP_SIZE_MIN + sizeof(xBleCmdPayloads_t) )


/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Function which receives an incoming data buffer, treats the buffer 
 * as a command, interprets the command, and puts it to a FIFO for execution.
 * Actual execution is performed by the xBleCmdExecThread thread in this module.
 * 
 * Normally the input of this function is incoming data from BLE NUS(Nordic Uart Service).
 * The function interprets commands according to the protocol defined in
 * "mobile_app_ble_protocol.h"
 *
 * @param pCmdData     Buffer which contains the incoming data (command).
 * @param cmdDataSize  The size of incoming data (command).
 */
void xBleCmdHandle(uint8_t *pCmdData, uint16_t cmdDataSize);



#endif  //X_BLE_CMD_H__