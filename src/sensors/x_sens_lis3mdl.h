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


#ifndef X_SENS_LIS3MDL_H__
#define  X_SENS_LIS3MDL_H__


/** @file
 * @brief This header file defines the API for LIS3MDL sensor
 * of XPLR-IOT-1. It contains basic functions to handle the
 * sensor within the context of Sensor Aggregation use case example.
 * Most sensors have a similar API (where XXXX is the name of the sensor)
 * 
 * (MAXM10S is not considered a sensor but a ublox position module with different API)
 * 
 * Usage:
 * xSensXXXXInit   <- Initialize sensor (only one time)
 * xSensXXXXEnable <- Enable sensor measurements (start its thread)
 * xSensXXXXEnablePublish(true) <- Publish the measurements (if an MQTT(SN) 
 *                                 connection is already established)
 * 
 * \note: Some functions in this module (enable/disable/change period etc) are not
 *  accessible when the main sensor aggregation function is active (samples and sends all
 * sensors in one MQTT message). In this case the functions return an invalid state error
 *  and their action does not take effect. 
 */


#include <stdint.h>
#include <shell/shell.h>
// includes the sensor status structure type
#include "x_sens_common_types.h"
#include "x_errno.h"


/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** This functions initializes/gets the sensor device in the Zephyr
 * context. Must be used before any other function in this module.
 *
 * @return        zero on success (X_ERR_SUCCESS) else negative error code.
 */
err_code xSensLis3mdlInit(void);



/** Sets the Update/Sampling Period of the sensor. If the sensor is enabled/running
 * its values will be updated based on this period. If the sensor is disabled/suspended
 * the Update period will take effect when the sensor is enabled again.
 *
 * @param milliseconds  The update period of the sensor in milliseconds.
 * @return        zero on success (X_ERR_SUCCESS) else negative error code.
 */
err_code xSensLis3mdlSetUpdatePeriod(uint32_t milliseconds);



/** Disables sensor measurements by suspending the sensor's sampling thread.
 *
 * @return        zero on success (X_ERR_SUCCESS) else negative error code.
 */
err_code xSensLis3mdlDisable(void);



/** Enables sensor measurements by resuming/starting the sensor's sampling thread.
 *
 * @return        zero on success (X_ERR_SUCCESS) else negative error code.
 */
err_code xSensLis3mdlEnable(void);



/** Returns the status of the sensor. The status includes info about
 * whether the sensor has been initialized properly, if its thread is running,
 * the sampling/update period and if publish to MQTT(SN) is enabled or not.
 *
 * @return        a structure containing the sensor's status.
 */
xSensStatus_t xSensLis3mdlGetStatus(void);



/** Enables/Disables the publish of measurements to MQTT(SN). In order for
 * the measurements to be actually published, an MQTT(SN) connection should be active
 * via the MQTT module.
 * If no connection is active, when a connection is activated the measurements will start 
 * publishing. If while connected the publish is disabled, this sensor won't publish its measurements.
 *
 * @param enable  [true] = enables publish, [false] = disable publish
 * @return        zero on success (X_ERR_SUCCESS) else negative error code.
 */
err_code xSensLis3mdlEnablePublish(bool enable);



/* ----------------------------------------------------------------
 * FUNCTIONS IMPLEMENTING SHELL-COMMANDS
 * -------------------------------------------------------------- */

/* Functions intended to be used only by the shell */


/** This function is intented only to be used as a command executed by the shell.
 * It basically enables/disables the publish of the sensor measurements using the 
 * string parameters "on", "off" respectively
 * Command Example: sensors LIS3MDL publish on
 *
 * @param shell  the shell instance from which the command is given.
 * @param argc   the number of parameters given along with the command.
 * @param argv   the array including the parameters themselves.
 */
void xSensLis3mdlEnablePublishCmd(const struct shell *shell, size_t argc, char **argv);



/** This function is intented only to be used as a command executed by the shell.
 * It sets the period of the sensor measurements in milliseconds
 * Command Example: sensors LIS3MDL set_period 10000
 *
 * @param shell  the shell instance from which the command is given.
 * @param argc   the number of parameters given along with the command.
 * @param argv   the array including the parameters themselves.
 */
void xSensLis3mdlUpdatePeriodCmd(const struct shell *shell, size_t argc, char **argv);



#endif  //X_SENS_LIS3MDL_H__