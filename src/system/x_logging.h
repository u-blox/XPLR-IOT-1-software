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


#ifndef X_LOGGING_H__
#define  X_LOGGING_H__

/** @file
 * @brief This file contains the log module name definitions
 *  and the application's logger API to handle Zephyr Logger
 *  in the context of Sensor Aggregation Use Case Example
 *  Firmware for XPLR-IOT-1
 */


#include <stdint.h>

/* ----------------------------------------------------------------
 * DEFINITIONS
 * -------------------------------------------------------------- */

// Color code definitions, use with LOG_INF only to avoid unexpected results.
// These strings modify the color of the printed text in the uart console
#define LOG_CLRCODE_DEFAULT "\x1B[0m"
#define LOG_CLRCODE_RED     "\x1B[1;31m"
#define LOG_CLRCODE_GREEN   "\x1B[1;32m"
#define LOG_CLRCODE_YELLOW  "\x1B[1;33m"


/** maximum number of log modules for which the "xLogSaveState" function can hold
 *  their current status */
#define LOG_STATUS_BUF_MAXLEN       80


// Logging module names for the sensor apps
#define LOGMOD_NAME_BME280      bme280_app  
#define LOGMOD_NAME_ICG20330    icg20330_app  
#define LOGMOD_NAME_LIS2DH12    lis2dh12_app  
#define LOGMOD_NAME_LIS3MDL     lis3mdl_app
#define LOGMOD_NAME_LTR303      ltr303_app
#define LOGMOD_NAME_BQ27520     battery_gauge_app

// Logging module names for the ublox module apps
#define LOGMOD_NAME_UBLMOD_COMMON   ubloxMod_common
#define LOGMOD_NAME_MAXM10S         maxm10s_app
#define LOGMOD_NAME_NINAW156        ninaW156_app
#define LOGMOD_NAME_SARAR5          saraR5_app
#define LOGMOD_NAME_WIFI_MQTT       mqtt_app
#define LOGMOD_NAME_CELL_MQTTSN     mqttSN_app
#define LOGMOD_NAME_BLE             ble_app
#define LOGMOD_NAME_NFC             nfc_app

#define LOGMOD_NAME_BLE_CMD         ble_cmd_app

//Logging module names of other modules
#define LOGMOD_NAME_STORAGE         storage_app
#define LOGMOD_NAME_DATA_HANDLE     mqtt_handle_app
#define LOGMOD_NAME_BUTTON          button_app
#define LOGMOD_NAME_LED             led_app

#define SENSOR_AGGREGATION_LOGMOD_NAME  sensAgg_app


/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Disable logging for a specific sensor
 *  Usage example: xLogDisable( STRINGIFY(LOGMOD_NAME_BME280) );
 *  
 * @param sensor_log_name  The log module that needs to be disabled
 */
void xLogDisable( const char *sensor_log_name );


/** This function is meant to be used at startup to enable/disable
 *  and configure the Logger at appropriate levels according to needs.
 *  Customize this function as needed so you don't have to send log
 *  commands to enable/disable modules and set their levels at each reset.
 */
void xLogStartupConfig(void);

/** The logging system can be configured at runtime by the user
 * (enable/disable logging levels for different modules)
 * During runtime the logger could/should be turned off/reset
 * at certain points. In this case the configuration of the logger will be lost.
 * This function saves the current configuration internally in the module,
 * and then this configuration can be restored using xLogRestoreState
 * function after a logger module reset
*/
void xLogSaveState(void);


/** Resets the logging module to the last logger configuration/state saved by
 * xLogSaveState
*/
void xLogRestoreState(void);




#endif    //X_LOGGING_H__