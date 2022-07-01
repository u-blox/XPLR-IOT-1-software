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

#ifndef X_CELL_SARAR5_H__
#define  X_CELL_SARAR5_H__

/** @file
 * @brief This file contains the API to handle SARA-R5 Cellular module in
 * XPLR-IOT-1 device. It handles the module by using ubxlib library 
 *   
 *   - xCellSaraConnect() is enough to connect to the network.
 *   - xCellSaraDeinit() disconnects from the network, deinitializes ubxlib
 *   and powers off the module
 * 
 *  Some additional configuration may be needed when connecting for the first
 *  form a new location. This is especially true when MQTT Flex is used 
 *  (see xCellSaraInit)
 * 
 */


#include <stdbool.h>
#include <stdint.h>
#include <shell/shell.h>

#include "x_errno.h"
#include "x_module_common.h"
#include "x_pin_conf.h"



/* ----------------------------------------------------------------
 * DEFINITIONS
 * -------------------------------------------------------------- */

// Default Thingstream Plan to be used with Cellular.
// Should be an xCellMqttSnPlan_t value
#define MQTTSN_DEFAULT_PLAN  ANYWHERE   

/** Timeout for uNetworkDown() to return (milliseconds) */
#define NETWORK_DOWN_TIMEOUT    20000  



/* ----------------------------------------------------------------
 * TYPE DEFINITIONS
 * -------------------------------------------------------------- */

/** Struct type that describes SARA-R5 status.
 */
typedef struct{
    bool pinsConfigured;   /**< Are SARA-R5 pins configured? */
    bool poweredUp;        /**< Is SARA-R5 powered? */
    ubxStatus_t uStatus;    /**< ubxlib status of the module*/
    bool isReadyToConnect; /**< Flag that shows that the module is configured and ready to connect*/
    bool isRegistered;     /**< Module has been registered to an MNO (mobile network operator)*/
    bool isConnected;      /**< Module has been connected to MNO (mobile network operator)*/
    bool isReadyForMqttSN; /**< Module is ready to be used with an MQTT-SN client*/
}xCellSaraStatus_t;


/** Enum with supported THingstream data plans for cellular.
 * Default is anywhere
 */
typedef enum{
    FLEX,      /**< To be used with 3rd party SIM (additional configuration may be needed) */
    ANYWHERE   /**< To be used with Thingstream SIM card */
}xCellMqttSnPlan_t;



/* ----------------------------------------------------------------
 * FUNCTIONS - HARDWARE CONTROL
 * -------------------------------------------------------------- */

/** Configures SARA-R5 pins connected to NORA-B1 as outputs.
 * Should be used before any other hardware control is used (such as powerOn/Off etc.) 
 */
void xCellSaraConfigPins(void);


/** Just powers the SARA-R5 module by setting the appropriate pins properly
 */
void xCellSaraPowerOn(void);

/** Just powers down SARA-R5 module
 * 
 * Prerequisites: SARA-R5 should not be initialized. Does not
 * deinitialize the module automatically
 */
err_code xCellSaraPowerOff(void);



/* ----------------------------------------------------------------
 * HANDLING FUNCTIONS
 * -------------------------------------------------------------- */


/** Initializes/ Prepares SARA-R5 to be used with ubxlib, by calling the 
 * SARA initialization thread.
 * 
 * The thread, if necessary powers up SARA-R5, initializes ubxlib Port and
 * depending on the xCellMqttSnPlan_t plan used (default is ANYWHERE) configures
 * the cellular module to connect to a Mobile Network Operator. 
 * 
 * For Flex you may need to configure uNetworkConfigurationCell_t flexConfiguration
 * and xCellSaraRegistrationConfig() function to work properly. For ANYWHERE
 * plan it should work ok.
 * 
 * Prerequisites: 
 *  1.Cannot be used while NINA-W156 WiFi module is in use by ubxlib (is initialized)
 * 
 */
void xCellSaraInit(void);


/** Disconnectes the Cellular module and deinitializes any ubxlib configuration,
 *  so the module cannot be used by ubxlib anymore. It also power down the module.
 * 
 * Side Effects:
 * - 1. This function also deinitializes other modules used by ubxlib such as mqtt and MAXM10S.
 * NINA-W156 cannot be used at the same time with SARA-R5, because they share the same
 * uart peripheral ID in NORA-B1, so it won't be affected (same as MQTT).
 * - 2. It powers off the module
 */
void xCellSaraDeinit(void);


/** Enables the SARA connection thread, which connects SARA to the configured Network.
 *  If not already initialized it also calls xCellSaraInit() internally.
 *  It may take some time, especially if firstly used, and type messages in the console.
 *  This is normal
 */
void xCellSaraConnect(void);


/** Should be used after xCellSaraInit(). Returns a handle to the initialized/added
 * network. This handle can be used with ubxlib functions that require a
 * network handle as a parameter. 
 *
 * @return        A negative value is an error code (should not be used with other ubxlib functions).
 *                Other values are the actual Handle.
 */
int32_t xCellSaraGetHandle(void);


/** Get SARA module current status.
 * @return   xCellSaraStatus_t struct that describes the module status
 */
xCellSaraStatus_t xCellSaraGetModuleStatus();


/** Returns the operation result (return or error code) of the last executed operation in 
 * the x_cell_saraR5 module. This is about the error codes produced by calls to the API
 * of this header file and not an actual error code returned from a direct command to the
 * SARA-R5 module. 
 * 
 * Error codes returned may include ubxlib error codes and not only codes defined in this
 * application.
 *
 * @return     Result code from last operation performed. 0 is success, negative value is error code
 */
err_code xCellSaraGetLastOperationResult(void);


/** Set MQTT Flex as current plan. 
 *  Needs to be user before xCellSaraInit to take effect
 */
void xCellSaraActivateMqttFlexPlan(void);

/** Set MQTT Anywhere as current plan. 
 *  Needs to be user before xCellSaraInit to take effect
 */
void xCellSaraActivateMqttAnywherePlan(void);

/** Get active plan that will be/is used with cellular
 */
xCellMqttSnPlan_t xCellSaraGetActiveMqttPlan(void);


/** Used by the application to deinitialize a network in ubxlib library.
 * Normally not to be used by the user.
 */
void xCellSaraNetworkDeinit(void);


/* ----------------------------------------------------------------
 * FUNCTIONS IMPLEMENTING SHELL-COMMANDS
 * -------------------------------------------------------------- */

/** This function is intented only to be used as a command executed by the shell.
 *  It just types the Active Thingstream Plan to be used with Cellular. Can be
 * either Flex or Anywhere
 * 
 * @param shell  the shell instance from which the command is given (and to which the command types).
 * @param argc   Not used
 * @param argv   Not used
 */
void xCellSaraGetActiveMqttPlanCmd(const struct shell *shell, size_t argc, char **argv);



#endif   //X_CELL_SARAR5_H__