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


#ifndef X_WIFI_MQTT_H__
#define  X_WIFI_MQTT_H__

/** @file
 * @brief This file contains the API to handle MQTT in
 * XPLR-IOT-1 device. It handles the module by using ubxlib library.
 * 
 * MQTT is used in Sensor Aggregation Use Case only with a WiFi connection.
 * It is used in this implementation to connect to Thingstream portal, using
 * an "MQTT Now" plan (IP Thing).
 * 
 * Pre-requisites: A WiFi connection should be already established
 * before using these functions.
 *  
 * If however the device is not already connected to a WiFi network,
 * the functions in this module can call the necessary functions internally
 * to connect to a network (provided the necessary WiFi network credentials
 * have already been provided to the device by the user, see: x_wifi_ninaW156.h)
 * 
 * Usage:
 * 
 * Before Using provide the MQTT configuration using the shell command (terminal):
 * -modules MQTT save <DeviceId> <Username> <Password>
 * 
 * if you want to write code to provide this configuration you can use 
 * xStorageSaveMqttConfig() function, the result will be the same.
 * 
 * The application is configured to connect only to Thingstream portal, however
 * it can be tweaked, using MQTT_BROKER_NAME and MQTT_PORT definition to 
 * connect somewhere else.
 * 
 * After the configuration is set:
 * 
 * xWifiMqttClientOpen() -> Open the Client
 * xWifiMqttClientConnect() -> Connect to Broker
 * 
 * At this point you can send the sensor data using the publish on option
 * (see sensors header files for documentation),
 * or send any message using  the xWifiMqttClientPublish() function or 
 * - "modules MQTT send" shell command
 * 
 * To disconnect use:
 * xWifiMqttClientClose() function
 * 
 */


#include <stdbool.h>
#include <stdint.h>
#include <shell/shell.h> //for shell command functions

#include "x_errno.h"  //includes error types
#include "x_module_common.h"


/* ----------------------------------------------------------------
 * MQTT DEFINITIONS
 * -------------------------------------------------------------- */

#define MQTT_DEVICE_ID_MAXLEN      100
#define MQTT_USERNAME_MAXLEN       100
#define MQTT_PASSWORD_MAXLEN       100

// Default Broker Name to connect to Thingstream platform
#define MQTT_BROKER_NAME    "mqtt.thingstream.io"

// Default Port to connect to Thingstream platform via MQTT Now
#define MQTT_PORT           1883  


/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Open an MQTT Client Session. Basically utilizes pUMqttClientOpen
 * ubxlib function (see related documentation) in the context of 
 * Sensor Aggregation Use Case.
 * 
 * Prerequisites: Connection to WiFi should have been established.
 * If not, the function tries to establish this connection using internal
 * calls to the necessary functions. WiFi network credentials needed
 * in this case.
 * 
 * The operation result can be seen by using xWifiMqttGetLastOperationResult()
 * after this function is called.
 */
void xWifiMqttClientOpen(void);


/** Connect to MQTT broker. 
 * 
 * Prerequisites: 
 * - MQTT configuration should have been provided using "modules MQTT save"
 * shell command (terminal) at least one time (this command also saves
 * configuration in memory). xStorageSaveMqttConfig() function can also
 * be used (same effect as the shell command)
 * - xWifiMqttClientOpen() should have been called. If not it is called with
 * an internal call within this function
 * 
 * The operation result can be seen by using xWifiMqttGetLastOperationResult()
 * after this function is called.
 */
void xWifiMqttClientConnect(void);


/** Is the active MQTT client connected to broker.
 * 
 * Side-Effects:
 * If not connected, it also closes the client. A simple 
 * xWifiMqttClientConnect() call later can open and reconnect
 * the client again. 
 *
 * (Implemented in this way so the user does not need to remember that
 * he needs to close the client to reconnect if this returns false, he just
 * uses the xWifiMqttClientConnect() function again) 
 *
 * @return        True = Connected, else False.
 */
bool xWifiMqttClientConnected(void);


/** Disconnect and Close MQTT Client.
 *  Use this when you want to disconnect from the MQTT broker
 */
void xWifiMqttClientClose(void);


/** Get the current status of the MQTT client.
 * 
 * @return        The Status in xClientStatusStruct_t.
 */
xClientStatusStruct_t xWifiMqttClientGetStatus(void);


/** Publish a Message to MQTT Broker.
 * Prerequisites: Need to Connect the Client to the Broker. Won't
 * be done automatically with internal calls.
 *
 * @param pTopicNameStr    String that contains the Topic to which the
 *                         message will be published.
 * @param pMessage         String that contains the message itself.
 * @param messageSizeBytes Message Size in Bytes
 * @param qos              Quality of service (0,1,2)
 * @param retain           if true the message will be kept by the broker
 *                         across MQTT disconnects/ connects, else it will be cleared
 * 
 * @return        zero on success else negative error code.
 */
err_code xWifiMqttClientPublish(const char *pTopicNameStr, const char *pMessage, size_t messageSizeBytes, uint8_t qos, bool retain);


/** Get the result of the Last operation performed.
 * Refers to xWifiMqttClientOpen() and
 * xWifiMqttClientConnect() operations
 * 
 * @return        zero on success else negative error code.
 */
err_code xWifiMqttGetLastOperationResult(void);


/** Delete any saved Mqtt configuration
 * 
 * @return        zero on success else negative error code.
 */
err_code xWifiMqttDeleteConfig( void );


/* ----------------------------------------------------------------
 * FUNCTIONS IMPLEMENTING SHELL-COMMANDS
 * -------------------------------------------------------------- */


/** This function is intented only to be used as a command executed by the shell.
 * It allows he user to enter the configuration (DeviceID, Username,Password) to
 * connect to an IP Thing in Thingstream portal
 *
 * @param shell  the shell instance from which the command is given.
 * @param argc   the number of parameters given along with the command (should be 3).
 * @param argv   the array including the parameters themselves (DeviceID, Username,Password).
 */
void xWifiMqttSaveConfigCmd(const struct shell *shell, size_t argc, char **argv);


/** This function is intented only to be used as a command executed by the shell.
 * Types the saved MQTT Client configuration 
 *
 * @param shell  the shell instance from which the command is given.
 * @param argc   Not used
 * @param argv   Not used
 */
void xWifiMqttTypeConfigCmd(const struct shell *shell, size_t argc, char **argv);


/** This function is intented only to be used as a command executed by the shell.
 * Types the MQTT Client Status (Open, Closed, Connected)
 *
 * @param shell  the shell instance from which the command is given.
 * @param argc   Not used
 * @param argv   Not used
 */
void xWifiMqttClientStatusCmd(const struct shell *shell, size_t argc, char **argv);


/** This function is intented only to be used as a command executed by the shell.
 * Sends a message to MQTT broker, utilizing xWifiMqttClientPublish(). Retain option
 * is not used in this command implementation. 
 *
 * @param shell  the shell instance from which the command is given.
 * @param argc   the number of parameters given along with the command (should be 3).
 * @param argv   the array including the parameters themselves (Topic Name, Message, QOS).
 */
void xWifiMqttSendCmd(const struct shell *shell, size_t argc, char **argv);






#endif   //X_WIFI_MQTT_H__