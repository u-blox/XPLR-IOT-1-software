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


#ifndef X_SENSOR_AGGREGATION_FUNCTION_H__
#define  X_SENSOR_AGGREGATION_FUNCTION_H__


/** @file
 * @brief This file contains the API to access the main
 * functionality of Sensor Aggregation Use Case for XPLR-IOT-1.
 * This functionality is to sample all sensors with the same sampling
 * period and then send those data to thingstream portal in a single 
 * message (one message per sampling period)
 */


#include <stdint.h>
#include <stdbool.h>
#include <shell/shell.h>

#include "x_errno.h"


/* ----------------------------------------------------------------
 * TYPE DEFINITIONS
 * -------------------------------------------------------------- */

/** When Sensor Aggregation Functionality is enabled it can operate in
 * two modes:
 * - Send data over WiFi (MQTT)
 * - Send data over Cellular (MQTT-SN)
 * 
 * This enum is used to give information about the current mode of the 
 * device
*/
typedef enum{
    xSensAggModeDisabled,     /**< Sensor Aggregatio mode disabled. In this mode
                                   you can still enable sensors and send data
                                   using shell commands but each sensor will send
                                   separate messages at separate topics */
    xSensAggModeWifi,         /**< Mode enabled, send data over WiFi*/
    xSensAggModeCell          /**< Mode enabled, send data over Cellular*/
}xSensorAggregationMode_t;


/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */


/** Start Sensor Aggregation Functionality over WiFi. Performs all
 * necessary operations (sets up WiFi network, connects to MQTT 
 * enables sensors etc)
 * Does so, by calling the appropriate thread
 * 
 * If Sensor Aggregation functionality over Cellular is already enabled,
 * Cellular is disabled before enabling WiFi.
 * 
 * Possible error Messages are typed in uart console
*/
void xSensorAggregationStartWifi(void);


/** Stops Sensor Aggregation Functionality over WiFi. Performs all
 * necessary operations (disconnects WiFi network, disconnects MQTT 
 * disables sensors etc)
 * Does so, by calling the appropriate thread
 * 
 * Possible error Messages are typed in uart console
 * 
 * This function deinitializes any ubxlib port open and disables all
 * sensors (even if they have been setup as enabled separately by the user)
*/
void xSensorAggregationStopWifi(void);


/** Start Sensor Aggregation Functionality over Cellluar. Performs all
 * necessary operations (connects to Cell, connects to MQTT-SN 
 * enables sensors etc)
 * Does so, by calling the appropriate thread
 * 
 * If Sensor Aggregation functionality over WiFi is already enabled,
 * WiFi is disabled before enabling Cellular.
 * 
 * Possible error Messages are typed in uart console
*/
void xSensorAggregationStartCell(void);


/** Stops Sensor Aggregation Functionality over Cellular. Performs all
 * necessary operations (disconnects Cell, disconnects MQTT-SN 
 * disables sensors etc)
 * Does so, by calling the appropriate thread
 * 
 * Possible error Messages are typed in uart console
 * 
 * This function deinitializes any ubxlib port open and disables all
 * sensors (even if they have been setup as enabled separately by the user)
*/
void xSensorAggregationStopCell(void);


/** When Sensor Aggregatio mode is enabled all sensors are sampled with
 * the same period. There is a default period defined in 
 * SENS_AGG_DEFAULT_UPDATE_PERIOD_MS, however the user can change this period
 * by using this function.
 * 
 * Prerequisites: Sensor Aggregation mode should be disabled when this 
 * function is called
 * 
 * Possible errors in the period parameter are shown in the uart console
 *
 * @param milliseconds  The update period in milliseconds. Some restrictions may apply,
 *                      see xPosMaxM10TimeoutPeriodCmd() description
 * @return              zero on success else negative error code.
 */
err_code xSensorAggregationSetUpdatePeriod(uint32_t milliseconds);


/** Get currently set update period of Sensor Aggregation Functionality.
 *
 * @return        The sampling (update) period in ms.
 */
int32_t xSensorAggregationGetUpdatePeriod(void);


/** Get Sensor Aggregation Functionality current mode.
 *
 * @return        Current Sensor Aggregation Mode as 
 *                xSensorAggregationMode_t
 */
xSensorAggregationMode_t xSensorAggregationGetMode(void);


/** When Sensor Aggregation mode starts, it needs to perform some
 * configuration, connect to network etc. That might take some time.
 * In the meantime no other command to stop Sensor Aggregation mode or
 * switch mode (Cell-> Wifi or vice versa) should be issued.
 * If that happens, it may lead to unexpected results.
 * 
 * This function informs whether the Sensor Aggregation functionality
 * is locked (in the middle of config, connect etc) and commands like
 * stop or switch mode should not be issued.
 *
 * @return        True = functionality locked. Do not send new stop 
 *                or start commands.
 *                False = unlocked. The user can send any command
 */
bool xSensorAggregationIsLocked(void);


/* ----------------------------------------------------------------
 * FUNCTIONS IMPLEMENTING SHELL-COMMANDS
 * -------------------------------------------------------------- */

/** This function is intented only to be used as a command executed by the shell.
 * It basically types the current xSensorAggregationMode_t mode of the device and
 * the sampling period set for sensor aggregation functionality
 *
 * @param shell  the shell instance from which the command is given.
 * @param argc   Not used.
 * @param argv   Not used.
 */
void xSensorAggregationTypeStatusCmd(const struct shell *shell, size_t argc, char **argv);


/** This function is intented only to be used as a command executed by the shell.
 * It implements the shell command: "functions set_period <period_ms>" 
 * by calling xSensorAggregationSetUpdatePeriod()
 * 
 * @param shell  the shell instance from which the command is given.
 * @param argc   the number of parameters given along with the command (period).
 * @param argv   the array including the parameters themselves.
 */
void xSensorAggregationSetUpdatePeriodCmd(const struct shell *shell, size_t argc, char **argv);


#endif    //X_SENSOR_AGGREGATION_FUNCTION_H__