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



#ifndef X_SENS_COMMON_TYPES_H__
#define  X_SENS_COMMON_TYPES_H__


/** @file
 * @brief This file contains some enum/structure types, commonly used by all
 * sensors in the sensor aggregation use case.
 */


#include <stdbool.h>


/* ----------------------------------------------------------------
 * TYPE DEFINITIONS
 * -------------------------------------------------------------- */


/** Common enum indicating the sensor type in an XPLR-IOT-1 device.
 * 
 * max_sensors_enum_t should be the last element in this list and is used for
 * error checks etc, it does not correspond to an actual sensor.
 * 
 * maxm10_t stands for the MAXM10S GNSS module. Although MAX is a module and
 * is handled differently from a sensor, it is included in this list because its data are
 * sent and manipulated just like the data from other sensors
 */
typedef enum{
    bme280_t = 0,
    battery_gauge_t,
    lis2dh12_t,
    lis3mdl_t,
    ltr303_t,
    icg20330_t,
    maxm10_t,
    max_sensors_num_t
}xSensType_t;


/** Common struct type used by all sensors (except MAXM10S) to describe their status.
 */
typedef struct{
    
    xSensType_t sensorType;   /**< Identifies the sensor */
    bool isReady;             /**< If the device is recognized and initialized without
                                   issues, this should be true */
    
    bool isEnabled;           /**< If the sensor sampling thread is running this is true*/
    uint32_t updatePeriod;    /**< The sampling period of the sensor*/
    bool isPublishEnabled;    /**< When true and C210 is connected to MQTT(SN), the sensor
                                   data are published to Thingstream*/
}xSensStatus_t;


#endif //X_SENS_COMMON_TYPES_H__