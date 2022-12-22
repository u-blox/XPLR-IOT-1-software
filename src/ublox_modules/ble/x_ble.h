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


#ifndef X_BLE_H__
#define  X_BLE_H__

/** @file
 * @brief This file defines the API for BLE functionality in the context
 * of Sensor Aggregation Use case firmware for XPLR-IOT-1.
 */


#include "stdint.h"
#include "x_errno.h"
#include <shell/shell.h>     //for shell command functions


/* ----------------------------------------------------------------
 * TYPE DEFINITIONS
 * -------------------------------------------------------------- */

/** Enum that describes NORA-B1 BLE status.
 */
typedef enum{
   xBleNotInitialized = 0,  /**< BLE stack not yet initialized */
   xBleIsInitialized,       /**< BLE stack initialized, not advertising or connected */
   xBleIsAdvertising,       /**< BLE is advertising */
   xBleIsConnected          /**< BLE connected */
}xBleStatus_t;


/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */


/** This function configures BLE functionality for Sensor Aggregation
 * use case and starts BLE advertising. It basically configures a modified
 * Nordic Uart Service as implemented in Nordic's "ble peripheral uart" example.
 * In this modified example, data received from BLE via this Service are considered
 * commands from u-blox mobile application and are passed to a BLE command handler for 
 * processing.
 *
 * @return        zero on success else negative error code.
 */
err_code xBleInit(void);


/** Start advertising via BLE. If already advertising (or connected)
 * this function has no effect. BLE stack should be initialized first
 * by using xBleInit().
 * 
 * * @return 		zero on success else negative error code.
*/
err_code xBleStartAdvertising(void);


/** Get NORA-B1 BLE module status 
 *
 * @return        xBleStatus_t enum describing BLE status
 */
xBleStatus_t xBleGetStatus(void);


/* ----------------------------------------------------------------
 * FUNCTIONS IMPLEMENTING SHELL-COMMANDS
 * -------------------------------------------------------------- */


/** This function is intented only to be used as a command executed by the shell.
 * The params are only needed because the function types using the shell_print funtion.
 * This command essentially starts BLE advertising. If BLE stack needs to initialize, it
 * also initializes BLE stack.
 * 
 * @param shell  the shell instance from which the command is given (and to which the command types).
 * @param argc   the number of parameters given along with the command (none expected).
 * @param argv   the array including the parameters themselves (none expected).
 */
void xBleEnableCmd(const struct shell *shell, size_t argc, char **argv);


/** This function is intented only to be used as a command executed by the shell.
 * The params are only needed because the function types using the shell_print funtion.
 * This command function essentially (disconnects and) disables BLE advertising.
 * 
 * @param shell  the shell instance from which the command is given (and to which the command types).
 * @param argc   the number of parameters given along with the command (none expected).
 * @param argv   the array including the parameters themselves (none expected).
 */
void xBleDisableCmd(const struct shell *shell, size_t argc, char **argv);


/** This function is intented only to be used as a command executed by the shell.
 * The params are only needed because the function types using the shell_print funtion.
 * This command disconnects from any connected BLE device. If not connected anywhere, this 
 * command has no effect
 * 
 * @param shell  the shell instance from which the command is given (and to which the command types).
 * @param argc   the number of parameters given along with the command (none expected).
 * @param argv   the array including the parameters themselves (none expected).
 */
void xBleDisconnectCmd(const struct shell *shell, size_t argc, char **argv);



#endif    //X_BLE_H__