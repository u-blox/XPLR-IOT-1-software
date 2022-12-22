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

#ifndef X_CELL_MQTTSN_H__
#define  X_CELL_MQTTSN_H__

/** @file
 * @brief This file contains the API to handle MQTT-SN in
 * XPLR-IOT-1 device. It handles the module by using ubxlib library.
 * 
 * MQTT-SN is used in Sensor Aggregation Use Case only with a Cellular connection.
 * It is used in this implementation to connect to Thingstream portal, using
 * MQTT Anywhere or Flex plan.
 * 
 * Pre-requisites: A Cellular connection should be already established
 * before using these functions.
 *  
 * If however the device is not already connected to a Cellular network,
 * the functions in this module can call the necessary functions internally
 * to connect to a network
 * 
 * Usage:
 * 
 * The application is configured to connect only to Thingstream portal, however
 * it can be tweaked (when using 3rd party SIM card), using MQTTSN_FLEX_BROKER_HOSTNAME and
 * MQTTSN_FLEX_BROKER_PORT definition to connect somewhere else.
 * 
 * After the configuration is set:
 * 
 * xCellMqttSnClientOpen() -> Open the Client
 * xCellMqttSnClientConnect() -> Connect to Broker
 * 
 * At this point you can send the sensor data using the publish on option
 * (see sensors header files for documentation),
 * or send any message using  the xCellMqttSnClientPublish() function or 
 * - "modules MQTTSN send" shell command
 * 
 * To disconnect use:
 * xCellMqttSnClientDisconnect() or
 * xCellMqttSnClientClose() function
 * 
 */


#include <stdint.h>
#include <stdbool.h>
#include <shell/shell.h>

#include "x_errno.h"
#include "x_cell_saraR5.h" //for plan types


/* ----------------------------------------------------------------
 * MQTT-SN MODULE DEFINITIONS
 * -------------------------------------------------------------- */

/** Max String Length of the Broker name string (can be changed according to needs)*/
#define MQTTSN_BROKER_NAME_MAXLEN         300
/** Max String Length of a topic name (can be changed according to needs)*/
#define MMQTTNS_TOPIC_MAXLEN              200
/** Max String Length of a Client ID (can be changed according to needs)*/
#define MQTTSN_CLIENT_ID_MAXLEN           200
/** Max String Length of Connection Duration*/
#define MQTTSN_CON_DURATION_STR_MAXLEN    15  // max 2^32 consists of 10 digits + 3 dots + termination char = 14


/* ----------------------------------------------------------------
 * MQTT ANYWHERE PLAN DEFINITIONS (DEFAULT)
 * -------------------------------------------------------------- */

/** Broker IP set to Thingstream portal */
#define MQTTSN_ANYWHERE_BROKER_IP           "10.7.0.55"

/** Port used for MQTT Anywhere plan in Thingstream portal */
#define MQTTSN_ANYWHERE_BROKER_PORT          2442

/** Connection Duration as defined in MQTT-SN: 
 * WARNING: Please keep in mind to set your sensor/send sampling rate below
 * this duration, otherwise the connection will be droped and the application
 * code does not check for this case in current implementation.
 * 
 * In this implementation the connection is considered to remain active for as
 * long as the sensor(s) need to send data.
 */
#define MQTTSN_ANYWHERE_CONNECTION_DURATION  600  //seconds


/* ----------------------------------------------------------------
 * MQTT FLEX DEFINITIONS
 * -------------------------------------------------------------- */

/** The Filename with which the certificate should be stored in 
 * SARA-R5 for an MQTT-SN client. In this application this only
 * applies when MQTT Flex Plan is used in Thingstream portal.
*/
#define MQTTSN_FLEX_CERTIFICATE_FILENAME    "cert.pem"

/** The Filename with which the key should be stored in 
 * SARA-R5 for an MQTT-SN client. In this application this only
 * applies when MQTT Flex Plan is used in Thingstream portal.
*/
#define MQTTSN_FLEX_KEY_FILENAME            "cert.key"

/** Broker hostname set to Thingstream portal */
#define MQTTSN_FLEX_BROKER_HOSTNAME         "mqtt-flex.thingstream.io"

/** Port used for MQTT Flex plan in Thingstream portal */
#define MQTTSN_FLEX_BROKER_PORT             2443
 

/* ----------------------------------------------------------------
 * TYPE DEFINITIONS
 * -------------------------------------------------------------- */

/** Structure used to hold Mqtt-SN configuration that is saved in
 * the memory. This by no means is a complete Mqtt-Sn configuration,
 * it is only useful within the context of Sensor Agregation use case
 * of XPLR-IOT-1
*/
typedef struct{
    xCellMqttSnPlan_t plan;                  /**< Plan to which this configuration refers*/
    char clientID[MQTTSN_CLIENT_ID_MAXLEN];  /**< Client ID*/
    uint32_t con_duration;                   /**< Connection Duration*/
}xCellMqttSnConfig_t;



/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Sets up the MQTT-SN client according to the active plan 
 * (Anywhere or Flex) by calling the appropriate thread
 * 
 * Prerequisites: 
 * 
 * - Cellular Connection should have been established.
 * If not, the function tries to establish this connection using internal
 * calls to the necessary functions. 
 * 
 * - MQTTSN configuration should have been provided using "modules MQTTSN save"
 * shell command (terminal) at least one time (this command also saves
 * configuration in memory). xStorageReadMqttSnConfig() function can also
 * be used (same effect as the shell command)
 * 
 * If this operation is successful the xCellSaraStatus_t from
 *  xCellMqttSnClientGetStatus() should be "ClientOpen". (wait for the status
 * to change while checking for errors with xWifiMqttGetLastOperationResult()).
 * 
 * Possible errors can be seen by using xCellMqttSnGetLastOperationResult()
 * after this function is called. 
 */
void xCellMqttSnClientOpen(void);  


/** Disconnect and Close MQTT-SN Client.
 *  Use this when you want to disconnect from the MQTT broker
 * 
 * @return     zero on success, negative error code otherwise
 *             even with negative err code returned the client is closed
*/
err_code xCellMqttSnClientClose(void);

/** Connect to MQTT broker (calls the thread which performs the operation). 
 * 
 * Prerequisites: 
 * - xWifiMqttClientOpen() should have been called. If not it is called with
 * an internal call within this function. Then the prerequisites for 
 * xWifiMqttClientOpen() apply.
 * 
 *  If this operation is successful the xCellSaraStatus_t from
 *  xCellMqttSnClientGetStatus() should be "ClientConnected" (wait for the status
 * to change while checking for errors with xWifiMqttGetLastOperationResult()).
 * 
 * The operation result can be seen by using xWifiMqttGetLastOperationResult()
 * after this function is called.
 */
void xCellMqttSnClientConnect(void);


/** Disconnect from MQTT-SN Broker.
 * 
 * @return     zero on success, negative error code otherwise
*/
err_code xCellMqttSnClientDisconnect(void);


/** Get the current status of the MQTT-SN client.
 * 
 * @return        The Status in xClientStatusStruct_t.
 */
xClientStatus_t xCellMqttSnClientGetStatus(void);


/** Get the result of the Last operation performed.
 * Refers to xCellMqttSnClientOpen() and
 * xCellMqttSnClientConnect() operations
 * 
 * @return        zero on success else negative error code.
 */
err_code xCellMqttSnGetLastOperationResult(void);


/** Publish a Message to MQTT-SN Broker.
 * Prerequisites: Need to Connect the Client to the Broker. Won't
 * be done automatically with internal calls.
 * 
 * @param topic_type       topic type according to MQTT-SN protocol(see also xCellMqttSnSendCmd).
 * @param pTopicNameStr    String that contains the Topic to which the
 *                         message will be published (see also xCellMqttSnSendCmd).
 * @param pMessage         String that contains the message itself.
 * @param messageSizeBytes Message Size in Bytes
 * @param qos              Quality of service (0,1,2)
 * @param retain           if true the message will be kept by the broker
 *                         across MQTT disconnects/ connects, else it will be cleared
 * 
 * @return        zero on success else negative error code.
 */
err_code xCellMqttSnClientPublish( const uMqttSnTopicName_t *pTopicName,
                                   const char *pMessage,
                                   size_t messageSizeBytes,
                                   uMqttQos_t qos, bool retain);

/** Delete any MQTT-Anywhere Configuration (MqttSn)
 *
 * @return        zero on success else negative error code.
 */
err_code xCellMqttSnDeleteAnywhereConfig( void );


/* ----------------------------------------------------------------
 * FUNCTIONS IMPLEMENTING SHELL-COMMANDS
 * -------------------------------------------------------------- */

/** This function is intented only to be used as a command executed by the shell.
 * It allows he user to enter the configuration (Plan, Device ID and Connection
 * Duration in seconds if anywhere plan is selected)
 *
 * @param shell  the shell instance from which the command is given.
 * @param argc   the number of parameters given along with the command (should be 2 or 3).
 * @param argv   the array including the parameters themselves.
 */
void xCellMqttSnSaveConfigCmd(const struct shell *shell, size_t argc, char **argv);

/** This function is intented only to be used as a command executed by the shell.
 * Types the saved MQTT-SN Client configuration(s)
 *
 * @param shell  the shell instance from which the command is given.
 * @param argc   Not used
 * @param argv   Not used
 */
void xCellMqttSnTypeConfigCmd(const struct shell *shell, size_t argc, char **argv);

/** This function is intented only to be used as a command executed by the shell.
 * Types the MQTT-SN Client Status (Open, Closed, Connected)
 *
 * @param shell  the shell instance from which the command is given.
 * @param argc   Not used
 * @param argv   Not used
 */
void xCellMqttSnClientStatusCmd(const struct shell *shell, size_t argc, char **argv);

/** This function is intented only to be used as a command executed by the shell.
 * Sends a message to MQTT-SN broker, utilizing xCellMqttSnClientPublish(). 
 * 
 * Command parameters are <topic_type> <topic> <message> <QOS>  
 * Where <topic_type> can be: 
 *  -normal
 *  -short
 *  -pre
 * 
 * And QoS can be: 0,1,2 or 3
 * 
 * Examples: "modules MQTTSN send pre 2 hello 0"
 *           "modules MQTTSN send short AB hello2 1"
 * 
 * @param shell  the shell instance from which the command is given.
 * @param argc   the number of parameters given along with the command (should be 4).
 * @param argv   the array including the parameters themselves.
 */
void xCellMqttSnSendCmd(const struct shell *shell, size_t argc, char **argv);




#endif    //X_CELL_MQTTSN_H__