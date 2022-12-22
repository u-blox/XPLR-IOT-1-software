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

#ifndef X_MODULE_COMMON_H__
#define  X_MODULE_COMMON_H__


/** @file
 * @brief This file contains the API to handle ublox modules common fuctions. 
 * It also defines some common types used by the application to handle ublox modules.
 * Common functions refer to setting up ubxlib to be used by Zephyr and nRF SDK and
 * setting up the common Uart shared by SARA-R5 and NINA-W156. Only one of those modules
 * can occupy the Uart at a time and this configuration can change at runtime.
 */


#include <stdint.h>
#include <stdbool.h>
#include "x_errno.h"


/* ----------------------------------------------------------------
 * TYPE DEFINITIONS
 * -------------------------------------------------------------- */


/** Enum type that describes the ubxlib status of Cellular or Wifi module.
 *  The order of elements in this enum should not be changed
 */
typedef enum{
    uPortNotInitialized,    /**< Ubxlib port not initilized for Zephyr */
    uPortInitialized,       /**< Ubxlib port intialized for use with Zephyr */
    uDeviceApiInitialized,  /**< Ubxlib Device API initialized (refers to uDeviceInit) */
    uDeviceOpened           /**< Ubxlib Device opened (refers to uDeviceOpen) */
}ubxStatus_t;



/** Enum that describes where the common Uart shared by NINA and SARA is configured.
 * This common uart can be configured to work with one of the modules at a time and
 * can change during runtime.
*/
typedef enum{
    COMMON_UART_SARA,    /**< Common Uart configured to work with SARA*/
    COMMON_UART_NINA,    /**< Common Uart configured to work with NINA*/
    COMMON_UART_NONE     /**< Common Uart not configured yet (default is NINA)*/
}xCommonUart_t;



/** MAXM10S and NINA-W156 modules can have their Serial UART comm connected directly to
 *  NORA-B1 or to a USB to UART bridge so that the user can talk directly to the modules
 *  with a host. This enum specifies which comm option is active.
*/
typedef enum{
    SERIAL_COMM_USB2UART = 0,
    SERIAL_COMM_NORA
}xSerialCommOption_t;


/** Enum used in xClientStatusStruct_t to describe whether the client is mqtt or mqttsn
*/
typedef enum{
    MqttClient,
    MqttSNClient
}xClientType_t;



/** Enum used to described the status of an mqtt client in ubxlib terms
 *  Refers to either Mqtt or Mqtt-Sn client
 * 
 *  Note: The order of enums is important here
*/
typedef enum{
    ClientClosed,    /**< Mqtt(SN) Client session not open*/
    ClientOpen,      /**< Mqtt(SN) Client session open*/
    ClientConnected  /**< Mqtt(SN) Client is connected to broker*/
}xClientStatus_t;



/** Structure describing the status of MQTT(SN) client
*/
typedef struct{
    xClientType_t type;
    xClientStatus_t status;
}xClientStatusStruct_t;



/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Configure Common Uart shared by NINA and SARA. This common uart
 * can be configured to work with one of the modules at a time and
 * can change during runtime.
 *
 * @param type  xCommonUart_t which selects either SARA or NINA. If
 * COMMON_UART_NONE is selected, nothing happens
 */ 
void xCommonUartCfg(xCommonUart_t type);



/** Function to initialize/setup ubxlib to be used with Zephyr. Should
 * be used before any attempt to use ubxlib functions.
 * 
 * Does not work if Port is already initialized (need to deinitilize and then
 * use this function again)
 * 
 * @return        zero on success else negative error code.
*/
err_code xCommonUPortInit(void);


/** Function to detinialize ubxlib. This needs to be done if ubxlib is
 * initialized and its setup needs to be changed (e.g, change from Cellular to WiFi).
 * In this case it is deinitiliazed and then re-initialized. After that ubxlib can be 
 * configured with a different configuration as needed. Executed by calling the 
 * appropriate thead.
 * 
 * Note: This function disconnects from everything and deinitializes every ublox module
 * In case of SARA, also powers it off.
*/
void xCommonUPortDeinit(void);



/** Checks if ubxlib is initialized. 
 * Can be initialized with xCommonUPortInit
 * Can be deinitialized with xCommonUPortDeinit
 * 
 * @return        True if initialized, false otherwise.
*/
bool xCommonUPortIsInit();



#endif   //X_MODULE_COMMON_H__