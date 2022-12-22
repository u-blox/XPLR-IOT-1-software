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


#ifndef X_SENS_BATTERY_GAUGE_H__
#define  X_SENS_BATTERY_GAUGE_H__


/** @file
 * @brief This header file defines the API for Battery Gauge sensor
 * of XPLR-IOT-1. Battery Gauge can be BQ27421 of BQ27250 depening on XPLR-IOT-1
 * version. The file contains basic functions to handle the
 * sensor within the context of Sensor Aggregation use case example.
 * Most sensors have a similar API (where XXXX is the name of the sensor)
 * 
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

/** This function initializes/gets the Battery Gauge device in the Zephyr
 * context. Must be used before any other function in this module. It can either
 * initialize Bq27520 or Bq27421 depending on which gauge is installed in
 * XPLR-IOT-1
 *
 * @return        zero on success (X_ERR_SUCCESS) else negative error code.
 */
err_code xSensBatGaugeInit(void);



/** Sets the Update/Sampling Period of the sensor. If the sensor is enabled/running
 * its values will be updated based on this period. If the sensor is disabled/suspended
 * the Update period will take effect when the sensor is enabled again.
 *
 * @param milliseconds  The update period of the sensor in milliseconds.
 * @return        zero on success (X_ERR_SUCCESS) else negative error code.
 */
err_code xSensBatGaugeSetUpdatePeriod(uint32_t milliseconds);



/** Disables sensor measurements by suspending the sensor's sampling thread.
 *
 * @return        zero on success (X_ERR_SUCCESS) else negative error code.
 */
err_code xSensBatGaugeDisable(void);



/** Enables sensor measurements by resuming/starting the sensor's sampling thread.
 *
 * @return        zero on success (X_ERR_SUCCESS) else negative error code.
 */
err_code xSensBatGaugeEnable(void);



/** Returns the status of the sensor. The status includes info about
 * whether the sensor has been initialized properly, if its thread is running,
 * the sampling/update period and if publish to MQTT(SN) is enabled or not.
 *
 * @return        a structure containing the sensor's status.
 */
xSensStatus_t xSensBatGaugeGetStatus(void);



/** Enables/Disables the publish of measurements to MQTT(SN). In order for
 * the measurements to be actually published, an MQTT(SN) connection should be active
 * via the MQTT module.
 * If no connection is active, when a connection is activated the measurements will start 
 * publishing. If while connected the publish is disabled, this sensor won't publish its measurements.
 *
 * @param enable  [true] = enables publish, [false] = disable publish
 * @return        zero on success (X_ERR_SUCCESS) else negative error code.
 */
err_code xSensBatGaugeEnablePublish(bool enable);



/* ----------------------------------------------------------------
 * FUNCTIONS IMPLEMENTING SHELL-COMMANDS
 * -------------------------------------------------------------- */

/* Functions intended to be used only by the shell */


/** This function is intented only to be used as a command executed by the shell.
 * It basically enables/disables the publish of the sensor measurements using the 
 * string parameters "on", "off" respectively
 * Command Example: sensors Battery publish on
 *
 * @param shell  the shell instance from which the command is given.
 * @param argc   the number of parameters given along with the command.
 * @param argv   the array including the parameters themselves.
 */
void xSensBatGaugeEnablePublishCmd(const struct shell *shell, size_t argc, char **argv);



/** This function is intented only to be used as a command executed by the shell.
 * It sets the period of the sensor measurements in milliseconds.
 * Command Example: sensors BAttery set_period 10000
 *
 * @param shell  the shell instance from which the command is given.
 * @param argc   the number of parameters given along with the command.
 * @param argv   the array including the parameters themselves.
 */
void xSensBatGaugeUpdatePeriodCmd(const struct shell *shell, size_t argc, char **argv);



#endif  //X_SENS_BATTERY_GAUGE_H__