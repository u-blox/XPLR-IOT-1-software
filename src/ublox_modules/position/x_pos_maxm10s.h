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


#ifndef X_POS_MAXM10S_H__
#define  X_POS_MAXM10S_H__


/** @file
 * @brief This file contains the API to handle MAXM10S GNSS positioning module in
 * XPLR-IOT-1 device. It handles the module by using ubxlib library
 * 
 * Some parts of MaxM10S module can be treated as a sensor (eg. Update Period,
 * sampling threads etc). 
 * Other parts that make use of ubxlib functionality shoulbe be treated as a 
 * ublox module.
 * That is why MaxM10S module lives between two worlds: sensors/modules and lends
 * elements from both, sometimes treated as a sensor and others as a module.
 * 
 *  
 *   In order to get a position fix the following should be done:
 * 
 *  - xPosMaxM10Init()   -> powers the module, initializes ubxlib and gnss module
 *  - xPosMaxM10Enable() -> starts up the necessary threads (also calls the init function
 *                          mentioned above if necessary)
 */


#include <stdint.h>
#include <shell/shell.h>          // needed for shell commands  
#include "x_errno.h"           // for functions that want to return a c210 error code
#include "x_module_common.h" // for xSerialCommOption_t type
#include "x_sens_common_types.h"  // for sensor status structure type


/* ----------------------------------------------------------------
 * TYPE DEFINITIONS
 * -------------------------------------------------------------- */

/** Struct type that describes MaxM10S status.
 */
typedef struct{
    bool pinsConfigured;         /**< Are MaxM10 pins configured? */
    bool isPowered;              /**< Is MaxM10 powered on? */
    bool isUbxInit;              /**< Is MaxM10 ready to be used by ubxlib? Use MaxM10Init function to render it ready  */
    bool isEnabled;              /**< Is the sampling thread running for MaxM10*/
    bool isPublishEnabled;       /**< When true and C210 is connected to MQTT(SN), the position
                                   obtained by MaxM10 are published to Thingstream*/
    uint32_t updatePeriod;       /**< The sampling period of MaxM10*/
    uint32_t timeoutPeriod;      /**< Timeout period for MaxM10 to respond with a position fix, since it is requested
                                    Timeout period should not be higher than updatePeriod*/
    xSerialCommOption_t com;    /**< Indicates which MaxM10 Uart Com is active: UART to USB or UART connected to NORA-B1*/
}xPosMaxM10Status_t;


/* ----------------------------------------------------------------
 * FUNCTIONS - HARDWARE CONTROL OF MAXM10S
 * -------------------------------------------------------------- */

/** Configures the MAXM10S pins connected to NORA-B1 as outputs.
 * Should be used before any other hardware control is used (such as powerOn/Off etc.) 
 */
void xPosMaxM10ConfigPins(void);

/** Just powers the MaxM10 module by setting the appropriate pins
 * SafeBoot disabled, Backup Supply Disabled
 * Cannot be accessed while the Main Sensor Aggregation Function is active
 */
void xPosMaxM10PowerOn(void);

/** Just powers off the MaxM10 module by clearing the appropriate pin
 * Cannot be accessed while the Main Sensor Aggregation Function is active
 */
void xPosMaxM10PowerOff(void);

/** Enables NORA - MAXM10S Uart communication. Disables MaxM10S UART to USB
 * communication
 */
void xPosMaxM10EnableNoraCom(void);

/** Disables NORA - MAXM10S Uart communication. Enables MaxM10S UART to USB
 * communication
 */
void xPosMaxM10DisableNoraCom(void);

/** Assert Enable Pin of MaxM10 (provides powers to the module)
 */
void max10EnablePinAssert(void);

/** Deassert Enable Pin of MaxM10 (remove power from the module)
 */
void max10EnablePinDeassert(void);

/** Enable MaxM10S Safe boot pin
 */
void max10SafeBootPinAssert(void);

/** Disable MaxM10S Safe boot pin
 */
void max10SafeBootPinDeassert(void);

/** Enable MaxM10S Backup Supply pin
 */
void max10BackupSupplyPinAssert(void);

/** Disable MaxM10S Backup Supply pin
 */
void max10BackupSupplyPinDeassert(void);


/* ----------------------------------------------------------------
 * FUNCTIONS - as MODULE CONTROL
 * -------------------------------------------------------------- */

/** Initializes/ Prepares MaxM10S to be used with ubxlib.
 * If necessary powers up MaxM10, sets the needed pins/coms and initializes
 * ubxlib Port and ubxlib Gnss modules.
 *
 * @return        zero on success else negative error code.
 */
err_code xPosMaxM10Init(void);


/** Deinitializes any ubxlib configuration that has to do with MaxM10,
 * so the module cannot be used by ubxlib anymore.
 * Also disables (suspends) the MaxM10 thread/sampling.
 * It does NOT power down MaxM10S.
 * That happens because this function could be used for a switch between cell, wifi
 * that needs temporarily to deinitialize ubxlib configuration. In that case if we
 * power down the GNSS modules it will take some time to obtain a position fix again. If the
 * module is still powered the module won't need so much time to obtain a position fix.
 * 
 * It's up to the user to decide whether he want to power the GNSS module down explicitly by using 
 * xPosMaxM10PowerOff function.
 */
void xPosMaxM10Deinit(void);


/** Returns the status of the module. The status includes info about
 * whether the module is ready to be used by ubxlib, if its thread is running,
 * the sampling/update period and if publish to MQTT(SN) is enabled or not. See
 * xPosMaxM10Status_t
 *
 * @return        a structure containing the module status.
 */
xPosMaxM10Status_t xPosMaxM10GetModuleStatus(void);


/* ----------------------------------------------------------------
 * FUNCTIONS - as SENSOR CONTROL
 * -------------------------------------------------------------- */

/** Sets the Update/Sampling Period of MaxM10S. If MaxM10 is enabled/running
 * its values will be updated based on this period. If the sensor is disabled/suspended
 * the Update period will take effect when the sensor is enabled again.
 *
 * @param milliseconds  The update period of the sensor in milliseconds.
 * @return              zero on success (X_ERR_SUCCESS) else negative error code.
 */
err_code xPosMaxM10SetUpdatePeriod(uint32_t milliseconds);



/** Enables MaxM10 position sampling by resuming/starting the module's sampling thread.
 * Cannot be accessed when the main Sensor Aggregation function is active. 
 * If necessary initializes the module by calling xPosMaxM10Init()
 * 
 * @return        zero on success else negative error code.
 */
err_code xPosMaxM10Enable(void);



/** Disables MaxM10 position sampling by suspending/stoping the module's sampling thread.
 * Cannot be accessed when the main Sensor Aggregation function is active. 
 */
void xPosMaxM10Disable(void);



/** Enables/Disables the publish of position to MQTT(SN). In order for
 * the position to be actually published, an MQTT(SN) connection should be active
 * via the MQTT module.
 * If no connection is active, when a connection is activated the position will start 
 * publishing. If while connected the publish is disabled, the module won't publish the position.
 *
 * @param enable  [true] = enables publish, [false] = disable publish
 * @return        zero on success else negative error code.
 */
void xPosMaxM10EnablePublish(bool enable);



/** Returns the status of the module, but in a sensor status description structure.
 *  Basically its like xPosMaxM10GetModuleStatus function but it returns a xSensStatus_t
 * structure instead of a xPosMaxM10Status_t structure (the latter holds some more information)
 *
 * @return        a sensor type structure containing the module status.
 */
xSensStatus_t xPosMaxM10GetSensorStatus(void);



/* ----------------------------------------------------------------
 * FUNCTIONS IMPLEMENTING SHELL-COMMANDS
 * -------------------------------------------------------------- */



/** This function is intented only to be used as a command executed by the shell.
 * It basically enables/disables the publish of the sensor measurements using the 
 * string parameters "on", "off" respectively
 * Command Example: modules MAXM10S publish on
 *
 * @param shell  the shell instance from which the command is given.
 * @param argc   the number of parameters given along with the command.
 * @param argv   the array including the parameters themselves.
 */
void xPosMaxM10EnablePublishCmd(const struct shell *shell, size_t argc, char **argv);


/** This function is intented only to be used as a command executed by the shell.
 * It sets the period of the position aquisition in milliseconds
 * Shell Command Example: modules MAXM10S set_period 10000
 *
 * @param shell  the shell instance from which the command is given.
 * @param argc   the number of parameters given along with the command.
 * @param argv   the array including the parameters themselves.
 */
void xPosMaxM10UpdatePeriodCmd(const struct shell *shell, size_t argc, char **argv);


/** This function is intented only to be used as a command executed by the shell.
 * It sets the max time to wait for a response with the position aquisition since the
 * request has been sent the module. It is set in milliseconds. The timeout cannot be higher
 * than the Update Period of the module nor the timeout set for GNSS aquisition from
 * ubxlib (U_GNSS_POS_TIMEOUT_SECONDS)
 * Shell Command Example: modules MAXM10S set_timeout 10000
 *
 * @param shell  the shell instance from which the command is given.
 * @param argc   the number of parameters given along with the command.
 * @param argv   the array including the parameters themselves.
 */
void xPosMaxM10TimeoutPeriodCmd(const struct shell *shell, size_t argc, char **argv);




#endif // X_POS_MAXM10S_H__