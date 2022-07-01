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


#ifndef X_WIFI_NINAW156_H__
#define  X_WIFI_NINAW156_H__

/** @file
 * @brief This file contains the API to handle NINA-W156 WiFi module in
 * XPLR-IOT-1 device. It handles the module by using ubxlib library
 *  
 *   In order to connect to a network you should provide WiFi network credentials
 *   (SSID,Password):
 *   - Use modules NINAW156 provision command
 *   
 *   Then xWifiNinaConnect() is enough to connect to the network.
 *   xWifiNinaDisonnect() disconnects from the network
 * 
 *   To deinitialize use:
 *   - xWifiNinaDeinit() -> does not power off the module
 *   or - xWifiNinaPowerOff() -> also powers off the module
 *   
 *   When connected to a network in order to connect to another network
 *   the module should be deinitialized and then initialized again with
 *   the new network's credentials
 * 
 */


#include <stdint.h>
#include <stdbool.h>
#include "x_errno.h"    //includes error  types
#include <shell/shell.h>   //for shell command functions
#include "x_module_common.h"  // imports ubxStatus_t enum


/* ----------------------------------------------------------------
 * SSID AND PASSWORD DEFINITIONS
 * -------------------------------------------------------------- */

/** Maximmum and minimum lenght of application supported WiFi network
* SSID and Password. These values can be be changed and do not have to do 
* with actual NINA-W156 limitations.
*/
#define WIFI_MAX_SSID_LEN  200
#define WIFI_MAX_PSW_LEN   200
#define WIFI_MIN_SSID_LEN  1
#define WIFI_MIN_PSW_LEN   1


/* ----------------------------------------------------------------
 * TYPE DEFINITIONS
 * -------------------------------------------------------------- */

/** Struct type that describes NINA-W156 status.
 */
typedef struct{
    bool pinsConfigured;        /**< Are NINA-W156 pins configured? */
    bool isPowered;             /**< Is NINA-W156 powered? */
    xSerialCommOption_t com ;  /**< Indicates which NINA-W156 Uart Com is active: UART to USB or UART connected to NORA-B1*/
    ubxStatus_t uStatus;         /**< ubxlib status of the module*/
    bool isConnected;           /**< is connected to a WiFi network?*/
}xWifiNinaStatus_t;



/** Struct type to hold WiFi network credentials.
 */
typedef struct{
    char SSIDstr[ WIFI_MAX_SSID_LEN ];   /**< String containing the WiFi network SSID */
    int32_t sec_type;                    /**< Security Type: 1= Open network (no password required). 2= Password required. All other values are invalid */
    char PSWstr[ WIFI_MAX_PSW_LEN ];     /**< String containing the WiFi network Password (if any) */
}xWifiCredentials_t;


/* ----------------------------------------------------------------
 * FUNCTIONS - HARDWARE CONTROL
 * -------------------------------------------------------------- */

/** Configures NINA-W156 pins connected to NORA-B1 as outputs.
 * Should be used before any other hardware control is used (such as powerOn/Off etc.) 
 */
void xWifiNinaConfigPins(void);

/** Assert Enable Pin of NINA-W156 (provides powers to the module)
 */
void xWifiNinaEnablePinAssert(void);

/** Deassert Enable Pin of NINA-W156 (remove power from the module)
 */
void xWifiNinaEnablePinDeassert(void);

/** Enables NORA - NINA-W156 Uart communication. Disables NINA-W156 UART to USB
 * communication
 */
void xWifiNinaEnableNoraCom(void);

/** Disables NORA - NINA-W156 Uart communication. Enables NINA-W156 UART to USB
 * communication
 */
void xWifiNinaDisableNoraCom(void);

/** Asserts Reset Pin of NINA-W156
 */
void xWifiNinaResetPinAssert(void);

/** Deasserts Reset Pin of NINA-W156
 */
void xWifiNinaResetPinDeassert(void);

/** Just powers the NINA-W156 module by setting the appropriate pins properly
 */
void xWifiNinaPowerOn(void);

/** Calls the Power off thread for NINA-W156 module to properly shut down the module
 * and powers it off (Not strictly a hardware function)
 */
void xWifiNinaPowerOff(void);


/* ----------------------------------------------------------------
 * HANDLING FUNCTIONS
 * -------------------------------------------------------------- */


/** Initializes/ Prepares NINA-W156 to be used with ubxlib, by calling the 
 * NINA initialization thread.
 * The thread, if necessary powers up NINA-W156, sets the needed pins/coms
 * and initializes ubxlib Port and checks for saved WiFi credentials.
 * 
 * Prepares the module to connect to the saved WiFi network in memory(adds the 
 * network in ubxlib terms).
 * 
 * Prerequisites: 
 *  1.To work properly it needs WiFi credentials saved in the memory (see provision command)
 *  2.Cannot be used while SARA-R5 cellular module is in use by ubxlib (is initialized)
 */
void xWifiNinaInit(void);


/** Deinitializes any ubxlib configuration, so the module cannot be used by ubxlib anymore.
 * It does NOT power down the module.
 * 
 * It's up to the user to decide whether he want to power the module down explicitly by using 
 * xWifiNinaPowerOff function (power off could be used directly without a deinit call).
 * 
 * Side Effects:
 * This function also deinitializes other modules used by ubxlib such as mqtt and MAXM10S.
 * SARA-R5 cannot be used at the same time with NINA-W156, because they share the same
 * uart peripheral ID in NORA-B1, so it won't be affected (same as MQTT-SN).
 */
void xWifiNinaDeinit(void);


/** Enables the NINA connection thread, which connects NINA to the configured WiFi Network.
 *  If not already initialized it also calls xWiFiNinaInit() internally.
 */
void xWifiNinaConnect(void);


/** Disconnects NINA from configured WiFi network.
 */
void xWifiNinaDisconnect(void);


/** Get NINA module current status.
 * @return   xWifiNinaStatus_t struct that describes the module status
 */
xWifiNinaStatus_t xWifiNinaGetModuleStatus(void);


/** Returns the operation result (return or error code) of the last executed operation in 
 * the x_wifi_ninaW156 module. This is about the error codes produced by calls to the API
 * of this header file and not an actual error code returned from a direct command to the
 * NINA-W156 module. 
 * 
 * Error codes returned may include ubxlib error codes and not only codes defined in this
 * application.
 *
 * @return     Result code from last operation performed. 0 is success, negative value is error code
 */
err_code xWifiNinaGetLastOperationResult(void);


/** Should be used after xWifiNinaInit(). Returns a handle to the initialized/added
 * network. This handle can be used with ubxlib functions that require a
 * network handle as a parameter. 
 *
 * @return        A negative value is an error code (should not be used with other ubxlib functions).
 *                Other values are the actual Handle.
 */
int32_t xWifiNinaGetHandle(void);


/** Used by the application to deinitialize a network in ubxlib library.
 * Normally not to be used by the user.
 */
void xWifiNinaNetworkDeinit(void);



/* ----------------------------------------------------------------
 * FUNCTIONS IMPLEMENTING SHELL-COMMANDS
 * -------------------------------------------------------------- */

/** This function is intented only to be used as a command executed by the shell.
 * The command uses two parameters provided by user input while sending the command,
 * to store a WiFi network's SSID and password (if needed). These credentials are then
 * used by xWifiNinaInit() to add this network and prepare the module to connect to it.
 *
 * @param shell  the shell instance from which the command is given.
 * @param argc   the number of parameters given along with the command (should be 2).
 * @param argv   the array including the parameters themselves (SSID, Password).
 */
void xWifiNinaProvisionCmd(const struct shell *shell, size_t argc, char **argv);



/** This function is intented only to be used as a command executed by the shell.
 * The params are only needed because the function types using the shell_print funtion.
 * This command types the saved WiFi credentials provided by using the xWifiNinaProvisionCmd()
 * It types the credentials saved in memory (for the next xWifiNinaInit() ) and the currently 
 * used ones (if the NINA module is connected already to a network, or a network has alredy been 
 * initialized).
 * These two are usually the same except when the user initializes a network with previously
 * saved in memory credentials and then without deinitializing this network he uses a xWifiNinaProvisionCmd
 * to change the credentials. This newly provided credentials will be used in the next call of
 * xWifiNinaInit()
 * 
 * 
 * @param shell  the shell instance from which the command is given (and to which the command types).
 * @param argc   the number of parameters given along with the command (none expected).
 * @param argv   the array including the parameters themselves (none expected).
 */
void xWifiNinaTypeNetworkParamsCmd(const struct shell *shell, size_t argc, char **argv);




#endif  //X_WIFI_NINAW156_H__