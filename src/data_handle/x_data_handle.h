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


#ifndef X_DATA_HANDLE_H__
#define  X_DATA_HANDLE_H__


/** @file
 * @brief This header file contains the API for functions that have to do with
 * data handling. Data handling includes preparing the message to be sent and
 * also sending that message.
 * 
 * The functions and definitions in this header file, define the message structure 
 * used to send sensor data to Thingstream portal. This structure should remain unchanged
 * for Node-Red dashboard provided with this example to work properly (this is the
 * structure the dashboard recognizes).
 * 
 * The sensor data is sent in a JSON string. An example of such a JSON string is given below:
 * 
 * {"Dev":"C210","Sensors":[{"ID":"BATTERY","err":"fetch"},{"ID":"BME280","mes":[{"nm":"Tm","vl":32.250},
 * {"nm":"Hm","vl":37.993},{"nm":"Pr","vl":98.905}]},{"ID":"ICG20330","mes":[{"nm":"Gx","vl":-0.015},
 * {"nm":"Gy","vl":0.001},{"nm":"Gz","vl":0.008}]},{"ID":"LIS2DH12","mes":[{"nm":"Ax","vl":1.149},
 * {"nm":"Ay","vl":-1.302},{"nm":"Az","vl":-9.270}]},{"ID":"LIS3MDL","mes":[{"nm":"Mx","vl":-1.587},
 * {"nm":"My","vl":-0.081},{"nm":"Mz","vl":0.432}]},{"ID":"LTR303","mes":[{"nm":"Lt","vl":106}]},
 * {"ID":"MAXM10","err":"timeout"}]}
 * 
 * 
 * Copying and pasting this example message in a JSON parser will reveal its structure in
 * a more readable format.
 * 
 * The above string is sent encoded as a Base64 string. Using the message as an example the exact message
 * sent is: 
 * 
 * eyJEZXYiOiJDMjEwIiwiU2Vuc29ycyI6W3siSUQiOiJCUTI3NDIxIiwiZXJyIjoiZmV0Y2gifSx7IklEIjoiQk1FMjgwIiwibWVz
 * IjpbeyJubSI6IlRtIiwidmwiOjMyLjI1MH0seyJubSI6IkhtIiwidmwiOjM3Ljk5M30seyJubSI6IlByIiwidmwiOjk4LjkwNX1df
 * Sx7IklEIjoiRlhBUzIxMDAyIiwibWVzIjpbeyJubSI6Ikd4IiwidmwiOi0wLjAxNX0seyJubSI6Ikd5IiwidmwiOjAuMDAxfSx7Im5
 * tIjoiR3oiLCJ2bCI6MC4wMDh9XX0seyJJRCI6IkxJUzJESDEyIiwibWVzIjpbeyJubSI6IkF4IiwidmwiOjEuMTQ5fSx7Im5tIjoiQ
 * XkiLCJ2bCI6LTEuMzAyfSx7Im5tIjoiQXoiLCJ2bCI6LTkuMjcwfV19LHsiSUQiOiJMSVMzTURMIiwibWVzIjpbeyJubSI6Ik14Iiw
 * idmwiOi0xLjU4N30seyJubSI6Ik15IiwidmwiOi0wLjA4MX0seyJubSI6Ik16IiwidmwiOjAuNDMyfV19LHsiSUQiOiJMVFIzMDMiL
 * CJtZXMiOlt7Im5tIjoiTHQiLCJ2bCI6MTA2fV19LHsiSUQiOiJBRFhMMzQ1IiwibWVzIjpbeyJubSI6IkF4IiwidmwiOjMxLjAwMH0
 * seyJubSI6IkF5IiwidmwiOi0xNTYuMDAwfSx7Im5tIjoiQXoiLCJ2bCI6LTkwNi4wMDB9XX0seyJJRCI6Ik1BWE0xMCIsImVyciI6I
 * nRpbWVvdXQifV19
 * 
 * The message formating is done by using xDataSend function which encodes
 * the message and sents it when necessary (depending on operation mode -single
 * sensor data transmission or one message that contains all sensor data)
 * 
 * The JSON packet key names are defined in this file.
 * The topic names (where the messages should be published) are also defined in this file. For the 
 * dashboard to work these topic names (and aliases) should also not change.
 * 
 * 
 */


#include <stdint.h>
#include "x_sens_common_types.h"
#include <drivers/sensor.h>     // includes sensor_channel enum


/* ----------------------------------------------------------------
 * MQTT(SN) TOPIC NAME DEFINITIONS
 * -------------------------------------------------------------- */

/** SARA-R5 Max topic name length [ UBX-19047455 - R09 page.415 ] */
#define MQTT_MAX_TOPIC_LEN    256 

/** SARA-R5 - The maximum parameter length is 1024 characters 
if <hex_mode>=0 or 512 octets if <hex_mode>=1. [ UBX-19047455 - R09 page.415 ] */
#define MQTT_MAX_MSG_LEN      1024

// The TOPIC names and aliases can be changed, however this will stop dashboard from working 
// properly

//define topics per sensor
#define TOPIC_NAME_BME280        "c210/sensor/environmental"
#define TOPIC_NAME_BQ27520       "c210/sensor/battery"
#define TOPIC_NAME_LIS2DH12      "c210/sensor/accelerometer/lis2dh12"
#define TOPIC_NAME_LIS3MDL       "c210/sensor/magnetometer"
#define TOPIC_NAME_LTR303        "c210/sensor/light"
#define TOPIC_NAME_ICG20330     "c210/sensor/gyroscope"
#define TOPIC_NAME_MAXM10S       "c210/position/nmea"
#define TOPIC_NAME_ALL_SENSORS   "c210/all"

// define topic aliases per sensor (should also be defined in thingstream -
// should be done upon entering the redemption code)
#define TOPIC_ALIAS_BME280      "501"    
#define TOPIC_ALIAS_BQ27520     "502"   
#define TOPIC_ALIAS_LIS2DH12    "504"    
#define TOPIC_ALIAS_LIS3MDL     "505"    
#define TOPIC_ALIAS_LTR303      "506" 
#define TOPIC_ALIAS_ICG20330    "507"    
#define TOPIC_ALIAS_MAXM10S     "508"
#define TOPIC_ALIAS_ALL_SENSORS "500"



/* ----------------------------------------------------------------
 * JSON STRING DEFINITIONS
 * -------------------------------------------------------------- */

/* The measurement data are structured as a JSON string and then sent
 * via MQTT. In this section the parameters of this JSON string are 
 * defined
*/

/** Maximum number of measurements per sensor, per reading 
 * in a JSON packet (e.g. for Acceleromenter: Ax,Ay,Az) 
 */
#define JSON_SENSOR_MAX_MEASUREMENTS  3 

/** Maximum Length of measurement (channel) name in a JSON string
 * e.g. Temperature measurement(channel) name = "Tm"
 */
#define JSON_SENSOR_CHAN_KEYNAME_MAXLEN   5


// Json key names definition
#define JSON_KEYNAME_SENSOR_ERROR          "err"  /**< Error keyname when an error in a sensor occurs. 
                                                       Its actual value contains the error. Eg: "err":"timeout" */
#define JSON_KEYNAME_SENSOR_ID             "ID"   /**< Sensor ID keyname eg: "ID":"BME280" */
#define JSON_KEYNAME_SENSOR_MEASUREMENTS   "mes"  /**< Keyname for the measurements list of a sensor" */
#define JSON_KEYNAME_SENSOR_CHAN_ID        "nm"   /**< Keyname the measurement(chanel) name(id) eg: "nm":"Tm" */
#define JSON_KEYNAME_SENSOR_CHAN_VALUE     "vl"   /**< Keyname for the measurement actual value eg: "vl":45 */



// Max string length describing an error for the JSON_KEYNAME_SENSOR_ERROR
// field
#define JSON_SENSOR_ERROR_STRING_MAXLEN  10

/** Maximum length of sensor ID strings
*/
#define JSON_SENSOR_ID_MAXLEN    11 

// define sensor names (IDs) as they are used in JSON messages etc
// (independent of the sensor labels in the device tree)
#define JSON_ID_SENSOR_BME280    "BME280"  
#define JSON_ID_SENSOR_BATTERY   "BATTERY"
#define JSON_ID_SENSOR_LIS2DH12  "LIS2DH12"
#define JSON_ID_SENSOR_LIS3MDL   "LIS3MDL"
#define JSON_ID_SENSOR_LTR303    "LTR303"
#define JSON_ID_SENSOR_ICG20330  "ICG20330"
#define JSON_ID_SENSOR_MAXM10    "MAXM10"




// Define measurements (channel) string label names
// Definition names are Based on zephyr's sensor_channel enum types
#define JSON_ID_SENSOR_CHAN_ACCEL_X  "Ax"
#define JSON_ID_SENSOR_CHAN_ACCEL_Y  "Ay"
#define JSON_ID_SENSOR_CHAN_ACCEL_Z  "Az"

#define JSON_ID_SENSOR_CHAN_GYRO_X   "Gx"
#define JSON_ID_SENSOR_CHAN_GYRO_Y   "Gy"
#define JSON_ID_SENSOR_CHAN_GYRO_Z   "Gz"

#define JSON_ID_SENSOR_CHAN_MAGN_X   "Mx"
#define JSON_ID_SENSOR_CHAN_MAGN_Y   "My"
#define JSON_ID_SENSOR_CHAN_MAGN_Z   "Mz"

#define JSON_ID_SENSOR_CHAN_POS_DX   "Px" 
#define JSON_ID_SENSOR_CHAN_POS_DY   "Py"

#define JSON_ID_SENSOR_CHAN_AMBIENT_TEMP "Tm"
#define JSON_ID_SENSOR_CHAN_PRESS        "Pr"
#define JSON_ID_SENSOR_CHAN_HUMIDITY     "Hm"

#define JSON_ID_SENSOR_CHAN_GAUGE_VOLTAGE "Volt"
#define JSON_ID_SENSOR_CHAN_GAUGE_STATE_OF_CHARGE  "SoC"

#define JSON_ID_SENSOR_CHAN_LIGHT    "Lt"



/* ----------------------------------------------------------------
 * DATA TYPE DEFINITIONS
 * -------------------------------------------------------------- */


/** Enum to define what type of data a measurement holds
*/
typedef enum{
    isDouble,
    isPosition,
    isInt
}xDataType_t;



/** Enum to define sensor error types that can be reported via 
 *  the JSON packet
*/
typedef enum{
    dataErrOk,             /**< No error, data ok */
    dataErrNotInit,        /**< Sensor not initialized properly */
    dataErrFetchFail,      /**< Fethcing data from sensor failed (sensor_sample_fetch function fail) */
    dataErrFetchTimeout,   /**< Timeout error */
    dataErrMaxNum          /**< Always at the end of this enum list, only used for sanity checks */
}xDataError_t;



/** Structure describing a measurement from a sensor
*/
struct xDataMeasurement_t{
    enum sensor_channel type;                      /**< Measurement (channel) Type eg:Temp,Pressure etc */
    char name[ JSON_SENSOR_CHAN_KEYNAME_MAXLEN ];  /**< String name of measurement (channel) */
    xDataType_t dataType;                         /**< Type of data used in this measurement */
    union{
        double doubleVal;
        int32_t int32Val;
    }data;                                         /**< Union holding the actual measurement */
};



/** Structure holding and describing a measurement from a sensor
*/
typedef struct{
    xDataError_t error;                   /**< Holds the error (if any) while trying to get sensor Data */
    char name[ JSON_SENSOR_ID_MAXLEN ];   /**< Sensor string name (id) */
    xSensType_t sensorType;               /**< Sensor type */
    struct xDataMeasurement_t meas[ JSON_SENSOR_MAX_MEASUREMENTS ];  /**< measurements from sensor */
    uint8_t measurementsNum;              /**< How many measurement this structure holds */
}xDataPacket_t;



/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** This function handles data transmission in all modes:
 * - Send each sensor's data separately
 * - Send all sensors data in one message, when they are all sampled (wifi or cell)
 * 
 * Depending on the device configuration the function performs the necessary
 * operations
 * 
 * If XPLR-IOT1 is not connected to Cell or WiFi it cannot send data
 * When data are available from a sensor this function should be called
 * if they need to be sent (regardless of mode)
 * 
 * * @param sensor_data_packet  Data to be sent in a xDataPacket_t structure
 * 
*/
void xDataSend(xDataPacket_t sensor_data_packet);


/** This function is to be used when sensor aggregation mode is active (all sensor
 * data sent over one message). When xDataSend is used in this mode the data are
 * accumulated and then sent when all sensors are sampled. 
 * 
 * If something goes wrong in the meantime and the message should/could not be sent, this
 * function should be called to erase all previous data from the buffer used by xDataSend
 * to accumulate the measurements, before a new cycle of measurements from all sensors begins.
 * 
*/
void xDataResetSensorAggregationMsg(void);




#endif    //X_DATA_HANDLE_H__