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



#ifndef X_SYSTEM_CONF_H__
#define X_SYSTEM_CONF_H__

/** @file
 * @brief This file holds all the multi-threading configuration 
 * definitions/options needed for Sensor Aggregation Use Case Example for
 * XPLR-IOT-1
 */


#define FIRMWARE_VERSION_MAJOR    0
#define FIRMWARE_VERSION_MINOR    3


// Sensor Threads
#define BQ27421_PRIORITY     7
#define BQ27421_STACK_SIZE   2048
#define BQ27421_DEFAULT_UPDATE_PERIOD_MS   10000  /**< Refers to single sensor sampling,
                                                       not sensor aggregation */

#define LIS2DH12_PRIORITY    7
#define LIS2DH12_STACK_SIZE  2048
#define LIS2DH12_DEFAULT_UPDATE_PERIOD_MS   10000 /**< Refers to single sensor sampling,
                                                       not sensor aggregation */

#define ADXL345_PRIORITY    7
#define ADXL345_STACK_SIZE  2048
#define ADXL345_DEFAULT_UPDATE_PERIOD_MS   10000 /**< Refers to single sensor sampling,
                                                       not sensor aggregation */

#define FXAS21002_PRIORITY    7
#define FXAS21002_STACK_SIZE  2048
#define FXAS21002_DEFAULT_UPDATE_PERIOD_MS   10000 /**< Refers to single sensor sampling,
                                                       not sensor aggregation */

#define BME280_PRIORITY    7
#define BME280_STACK_SIZE  2048
#define BME280_DEFAULT_UPDATE_PERIOD_MS   10000 /**< Refers to single sensor sampling,
                                                       not sensor aggregation */

 #define LTR303_PRIORITY    7
 #define LTR303_STACK_SIZE  2048
 #define LTR303_DEFAULT_UPDATE_PERIOD_MS   10000 /**< Refers to single sensor sampling,
                                                       not sensor aggregation */

 #define LIS3MDL_PRIORITY    7
 #define LIS3MDL_STACK_SIZE  2048
 #define LIS3MDL_DEFAULT_UPDATE_PERIOD_MS   10000 /**< Refers to single sensor sampling,
                                                       not sensor aggregation */


// Ublox Modules Threads
#define C210_UPORT_PRIORITY        7
#define C210_UPORT_STACK_SIZE     1024

#define NINAW156_CONFIG_PRIORITY     7
#define NINAW156_CONNECT_PRIORITY    7
#define NINAW156_STACK_SIZE   2048

#define SARAR5_PRIORITY     7
#define SARAR5_STACK_SIZE   1024

#define MQTT_PRIORITY        7
#define MQTT_STACK_SIZE     1024

#define MQTTSN_PRIORITY        7
#define MQTTSN_STACK_SIZE     1024

#define MAXM10S_PRIORITY                       7
#define MAXM10S_COMPLETE_POS_PRIORITY          7 
#define MAXM10S_STACK_SIZE                     2048
#define MAXM10S_DEFAULT_UPDATE_PERIOD_MS       10000
#define MAXM10S_DEFAULT_TIMEOUT_PERIOD_MS      7000 /**< set to 0 to disable timeout.
                                                         Should be lower than
                                                         MAXM10S_DEFAULT_UPDATE_PERIOD_MS */


// Button and Led Threads
#define  BUTTON_ACTION_PRIORITY        7
#define  BUTTON_ACTION_STACK_SIZE      1024

#define LED_PRIORITY                   7
#define LED_STACK_SIZE                 1024


// Sensor Aggregation Thread
#define SENS_AGG_PRIORITY      7
#define SENS_AGG_STACK_SIZE    1024
#define SENS_AGG_DEFAULT_UPDATE_PERIOD_MS    20000 /**< Refers to sensor aggregation,
                                                        all sensors sampled with the same
                                                        period */


#endif    //X_SYSTEM_CONF_H__
