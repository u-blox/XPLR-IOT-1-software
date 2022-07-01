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


/** @file
 * @brief File containing the implementation of Data Handling API described in
 * x_data_handling.h
 */


#include "x_data_handle.h"

#include <stdio.h>         //snprintf
#include <logging/log.h>

#include "x_base64.h"
#include "x_wifi_mqtt.h"
#include "x_cell_mqttsn.h"
#include "x_sensor_aggregation_function.h"
#include "x_logging.h"

#include "x_errno.h"


/* ----------------------------------------------------------------
 * DEFINITIONS
 * -------------------------------------------------------------- */

/** Size of temporary string buffers used to create the message
 *  to send */
#define TEMP_STRBUF_SIZE    100

/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION
 * -------------------------------------------------------------- */


/** Function that gets as input an xDataError_t error parameter and returns its 
 * string representation (for typing/sending the error etc).
 *
 * @param err            [Input] Data Error type for which its string description is required.
 * @param string         [Output] String buffer where the error string description is returned.
 * @param string_maxlen  [Input] Maximmum Length of string output parameter
 * @return               zero on success (X_ERR_SUCCESS) else negative error code.
 */
static err_code xDataGetErrStr(xDataError_t err, char *string, uint8_t string_maxlen);


/** Function that prepares the message (in that case a JSON string encoded in Base64)
 * that will be sent via MQTT(SN) in Thingstream portal. This function handles single
 * sensor messages, meaning each sensor has a separate message. The message itself is
 * stored in global string variable pMessage.
 * Also sets the topic to which the message should be published in gpTopicNameStr global
 *
 * @param sensor_data_packet   [Input] Data to be prepared in a xDataPacket_t structure
 * @return                     zero on success (X_ERR_SUCCESS) else negative error code.
 */
static err_code xDataPrepareSingleSensorMsg(xDataPacket_t sensor_data_packet);



/** Function that prepares the message (in that case a JSON string encoded in Base64)
 * that will be sent via MQTT(SN) in Thingstream portal. This function accumulates
 * all sensors data in one message, and the message itself is stored in global string
 * variable pMessage.
 * Also sets the topic to which the message should be published in gpTopicNameStr global
 *
 * @param sensor_data_packet   [Input] Data to be prepared in a xDataPacket_t structure
 * 
 * @return                     negative error code when error happens. 
 *                             zero when data are accumulated successfully in the message,
 *                             but more sensors need to be sampled.
 *                             one when data accumulated succesfully and all sensors have been
 *                             acquired to complete a full sensor data packet. If one is returned
 *                             the message is ready to be sent. 
 */
int32_t xDataPrepareSensorAggregationMsg(xDataPacket_t sensor_data_packet);


/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS
 * -------------------------------------------------------------- */

LOG_MODULE_REGISTER(LOGMOD_NAME_DATA_HANDLE, LOG_LEVEL_DBG);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */


/** Topic to which the message should be published*/
static char gpTopicNameStr[MQTT_MAX_TOPIC_LEN];

/** The message to be published. Created by functions xDataPrepareSingleSensorMsg
 * and xDataPrepareSensorAggregationMsg and reset by function 
 * xDataResetSensorAggregationMsg
*/
static char pMessage[MQTT_MAX_MSG_LEN];


/** Topic alias to which the message should be published (used with MQTT-SN)*/
static char gpTopicAliasStr[10];


/** Used to Flag which sensor's data has been received in order to fill the
 * complete sensor aggregation message with data from all sensors.
 * true is for sensor's whose data has been received.
*/
static bool gSensorsReceivedFlags[max_sensors_num_t] = {0};


/** Contains the string representation of Data Error types. Used by 
 * xDataGetErrStr
 */
const char *const gpSensorErrorStrings[]={
	[dataErrOk]="ok",
	[dataErrNotInit]= "init",
	[dataErrFetchFail] = "fetch",
	[dataErrFetchTimeout]="timeout",
};


/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


static int32_t xDataPrepareSingleSensorMsg( xDataPacket_t sensor_data_packet ){

    uint32_t err;

    // Parameter check
    if( sensor_data_packet.sensorType >= max_sensors_num_t ){
        return X_ERR_INVALID_PARAMETER; 
    }

    // Temporary string buffer
    char str_buf[ TEMP_STRBUF_SIZE ];

    // check if error and prepare error message - eg. {"ID":"BME280","error":"some error description"}
    if( sensor_data_packet.error != dataErrOk ){
        if( ( err = xDataGetErrStr( sensor_data_packet.error, str_buf, sizeof(str_buf) ) ) == X_ERR_SUCCESS ){
            snprintf( pMessage, sizeof(pMessage), "{\"%s\":\"%s\",\"%s\":\"%s\"}",
                        JSON_KEYNAME_SENSOR_ID,
                        sensor_data_packet.name,
                        JSON_KEYNAME_SENSOR_ERROR,
                        str_buf);
        }
        else{
            LOG_ERR("Error while getting sensor error string for %s: %d", sensor_data_packet.name, err);
        }
    }

    // if the sensor does not contain any errors, prepare the data message to send
    else{
        /* prepare sensor data message as JSON object eg: 
        {
         "ID":"BME280",
         "samples":[
            { "name":"Temp", "val":333.333 },
            { "name":"Hum", "val":333.333 },
            { "name":"Pres", "val":333.333 }
          ]
        }
        */

        // Sensor Name and start of samples list:   {"ID":"BME280","samples":[
        snprintf(pMessage,sizeof(pMessage),"{\"%s\":\"%s\",\"%s\":[ ",
            JSON_KEYNAME_SENSOR_ID,
            sensor_data_packet.name,
            JSON_KEYNAME_SENSOR_MEASUREMENTS);
        
        //Add Measurement (samples) Values List 
        for(uint8_t meas_num =0; meas_num < sensor_data_packet.measurementsNum; meas_num++){

            memset( str_buf,0,sizeof(str_buf) );
            // if measurement is double
            if( sensor_data_packet.meas[ meas_num ].dataType == isDouble ){
                snprintf(str_buf, sizeof(str_buf), "{\"%s\":\"%s\",\"%s\":%.3f}", 
                    JSON_KEYNAME_SENSOR_CHAN_ID,
                    sensor_data_packet.meas[ meas_num ].name, 
                    JSON_KEYNAME_SENSOR_CHAN_VALUE,
                    sensor_data_packet.meas[ meas_num ].data.doubleVal);
            }
            // if measurement is position
            else if( sensor_data_packet.meas[ meas_num ].dataType == isPosition ){
                snprintf(str_buf, sizeof(str_buf), "{\"%s\":\"%s\",\"%s\":%3.7f}", 
                    JSON_KEYNAME_SENSOR_CHAN_ID,
                    sensor_data_packet.meas[ meas_num ].name, 
                    JSON_KEYNAME_SENSOR_CHAN_VALUE,
                    sensor_data_packet.meas[ meas_num ].data.doubleVal);
            }

            // if measurement is integer
            else{
                snprintf(str_buf, sizeof(str_buf), "{\"%s\":\"%s\",\"%s\":%d}", 
                    JSON_KEYNAME_SENSOR_CHAN_ID,
                    sensor_data_packet.meas[ meas_num ].name, 
                    JSON_KEYNAME_SENSOR_CHAN_VALUE,
                    sensor_data_packet.meas[ meas_num ].data.int32Val);
            }

            // add measurement string to the message
            strcat(pMessage,str_buf);

            // add comma if necessary to separate from next measurement
            if(meas_num < sensor_data_packet.measurementsNum -1 ){
                strcat(pMessage,",");
            }
            //else if this is the last measurement, close measurements list and sensor object
            else{
                strcat(pMessage,"]}");
            }
        }
    }

    // encode string to Base64 (this resolves some issues when sending characters via cell)
    // characters like double quotes ", used in JSON strings may be affected. Encoding the string
    // resolves this issue

    //LOG_DBG("%s\r\n",pMessage);

    char str[ sizeof( pMessage )];
    strcpy( str, pMessage );
    err_code ret = xBase64Encode( str, pMessage, sizeof(pMessage) );
    if( ret < 0 ){
        LOG_ERR( "Message too big to send via MQTT(SN)\r\n" );
        return ret;
    }
    
    // define topic
    switch(sensor_data_packet.sensorType){

        case bme280_t:  strcpy(gpTopicNameStr,TOPIC_NAME_BME280);
                        strcpy(gpTopicAliasStr,TOPIC_ALIAS_BME280);
                        break;
        
        case adxl345_t: strcpy(gpTopicNameStr,TOPIC_NAME_ADXL345);
                        strcpy(gpTopicAliasStr,TOPIC_ALIAS_ADXL345);
                        break;
        
        case bq27421_t: strcpy(gpTopicNameStr,TOPIC_NAME_BQ27421);
                        strcpy(gpTopicAliasStr,TOPIC_ALIAS_BQ27421);
                        break;
        
         
        case lis2dh12_t:strcpy(gpTopicNameStr,TOPIC_NAME_LIS2DH12);
                        strcpy(gpTopicAliasStr,TOPIC_ALIAS_LIS2DH12);
                        break;
        
        case lis3mdl_t: strcpy(gpTopicNameStr,TOPIC_NAME_LIS3MDL);
                        strcpy(gpTopicAliasStr,TOPIC_ALIAS_LIS3MDL);
                        break;
        
        
        case ltr303_t: strcpy(gpTopicNameStr,TOPIC_NAME_LTR303);
                       strcpy(gpTopicAliasStr,TOPIC_ALIAS_LTR303);
                       break;
        
        
        case fxas21002_t: strcpy(gpTopicNameStr,TOPIC_NAME_FXAS21002);
                          strcpy(gpTopicAliasStr,TOPIC_ALIAS_FXAS21002);
                          break;
        
        case maxm10_t: strcpy(gpTopicNameStr,TOPIC_NAME_MAXM10S);
                       strcpy(gpTopicAliasStr, TOPIC_ALIAS_MAXM10S);
                       break;
        
        default: return X_ERR_INVALID_PARAMETER; //invalid parameters
        break;
    }

    return X_ERR_SUCCESS;
}



int32_t xDataPrepareSensorAggregationMsg(xDataPacket_t sensor_data_packet){

    uint32_t err;

    gSensorsReceivedFlags[max_sensors_num_t] = 0; // always ensure this is 0

    //check parameters
    if(sensor_data_packet.sensorType >= max_sensors_num_t){
        return X_ERR_INVALID_PARAMETER; //invalid param
    }

    // Temporary string buffer
    char str_buf[ TEMP_STRBUF_SIZE ];

    // Start of JSON packet?
    // Is this the first sensor packet received during this sampling session?
    uint8_t x;
    for(x=0; x < sizeof(gSensorsReceivedFlags); x++){
        if(gSensorsReceivedFlags[x]){
            break;
        }
    }

    if( x == max_sensors_num_t ){
        //No sensors included in the message to sent yet. Start JSON packet
        snprintf(pMessage,sizeof(pMessage),"{\"Dev\":\"C210\",\"Sensors\":[");
    }
    else{
        // add one more sensor
        strcat(pMessage,",");
    }
    
    
    // if sensor data are ok, no error
    if( sensor_data_packet.error == dataErrOk ){

        /* prepare sensor data message as JSON object eg: 
        {
         "ID":"BME280",
         "samples":[
            { "name":"Temp", "val":333.333 },
            { "name":"Hum", "val":333.333 },
            { "name":"Pres", "val":333.333 }
          ]
        }
        */
        
        // Start Object Description - Sensor Name
        //eg:  {"ID":"BME280","samples":[
        snprintf(str_buf,sizeof(str_buf),"{\"%s\":\"%s\",\"%s\":[",
            JSON_KEYNAME_SENSOR_ID,
            sensor_data_packet.name,
            JSON_KEYNAME_SENSOR_MEASUREMENTS);

        strcat (pMessage,str_buf);

        // Add Measurement Values List
        // each measurement is similar to this: {"name":"Temp","val":333.333}
        for(uint8_t meas_num =0; meas_num < sensor_data_packet.measurementsNum; meas_num++){

            memset( str_buf,0,sizeof(str_buf) );
            // if measurement is double
            if( sensor_data_packet.meas[ meas_num ].dataType == isDouble ){
                snprintf(str_buf, sizeof(str_buf), "{\"%s\":\"%s\",\"%s\":%.3f}",
                    JSON_KEYNAME_SENSOR_CHAN_ID,
                    sensor_data_packet.meas[ meas_num ].name,
                    JSON_KEYNAME_SENSOR_CHAN_VALUE,
                    sensor_data_packet.meas[ meas_num ].data.doubleVal);
            }
            // if measurement is position
            else if( sensor_data_packet.meas[ meas_num ].dataType == isPosition ){
                snprintf(str_buf, sizeof(str_buf), "{\"%s\":\"%s\",\"%s\":%3.7f}",
                    JSON_KEYNAME_SENSOR_CHAN_ID,
                    sensor_data_packet.meas[ meas_num ].name,
                    JSON_KEYNAME_SENSOR_CHAN_VALUE,
                    sensor_data_packet.meas[ meas_num ].data.doubleVal);
            }

            // if measurement is integer
            else{
                snprintf(str_buf, sizeof(str_buf), "{\"%s\":\"%s\",\"%s\":%d}",
                    JSON_KEYNAME_SENSOR_CHAN_ID,
                    sensor_data_packet.meas[ meas_num ].name,
                    JSON_KEYNAME_SENSOR_CHAN_VALUE,
                    sensor_data_packet.meas[ meas_num ].data.int32Val);
            }

            // add measurement string to the message
            strcat(pMessage,str_buf);

            // add comma if necessary to separate from next measurement
            if(meas_num < sensor_data_packet.measurementsNum -1 ){
                strcat(pMessage,",");
            }
            //if this is the last measurement, close measurements list and sensor object
            else{
                strcat(pMessage,"]}");
            }
        }
    }

    // the sensor contains some error
    // prepare error message - eg. {"ID":"BME280","error":"some error description"}
    else{
        // known error
        char err_str_buf[JSON_SENSOR_ERROR_STRING_MAXLEN];
        if( ( err = xDataGetErrStr( sensor_data_packet.error, err_str_buf, sizeof(err_str_buf)) ) == X_ERR_SUCCESS ){
            snprintf(str_buf,sizeof(str_buf),"{\"%s\":\"%s\",\"%s\":\"%s\"}",
                JSON_KEYNAME_SENSOR_ID,
                sensor_data_packet.name,
                JSON_KEYNAME_SENSOR_ERROR,
                err_str_buf);
        }
        // unknown error
        else{
            snprintf( str_buf, sizeof( str_buf ),"{\"%s\":\"%s\",\"%s\":\"%s\"}",
                JSON_KEYNAME_SENSOR_ID,
                sensor_data_packet.name,
                JSON_KEYNAME_SENSOR_ERROR,
                "unknown error");
        }

        strcat (pMessage,str_buf);
    }

    // flag sensor received and check if all needed sensors have been received
    gSensorsReceivedFlags[ sensor_data_packet.sensorType ] = true;

    for( x=0; x < sizeof( gSensorsReceivedFlags ); x++ ){
        if( !gSensorsReceivedFlags[x] ){
            break;
        }
    }

    // all sensors needed received
    if( x == max_sensors_num_t ){
        // close sensors list and JSON packet
        strcat(pMessage,"]}");
        strcpy(gpTopicAliasStr,TOPIC_ALIAS_ALL_SENSORS);
        strcpy(gpTopicNameStr,TOPIC_NAME_ALL_SENSORS);
        
        // encode string to Base64 (this resolves some issues when sending characters via cell)
        // characters like double quotes ", used in JSON strings may be affected. Encoding the string
        // resolves this issue
        char str[ sizeof( pMessage )];
        strcpy(str,pMessage);
        int32_t ret = xBase64Encode( str, pMessage, sizeof(pMessage) );
        if( ret < 0 ){
            LOG_ERR("Message too big to send via MQTT(SN)\r\n");
            return ret;
        }

        return 0; //signal json packet complete
    }

    else{ //more sensors required
        return 1;
    }
}



err_code xDataGetErrStr(xDataError_t err, char *string, uint8_t string_maxlen){
    
    if( string_maxlen < JSON_SENSOR_ERROR_STRING_MAXLEN ){
        return X_ERR_INVALID_PARAMETER; //invalid param, string buffer not big enough
    }

    if( err >= dataErrMaxNum ){
        return X_ERR_INVALID_PARAMETER; //invalid param
    }

    strcpy( string, gpSensorErrorStrings[err] );

    return X_ERR_SUCCESS; 
}



/* ----------------------------------------------------------------
 * PUBLIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


void xDataResetSensorAggregationMsg(void){

    memset(gSensorsReceivedFlags, 0, sizeof(gSensorsReceivedFlags));
    memset(pMessage, 0, sizeof(pMessage));
}



void xDataSend(xDataPacket_t sensor_data_packet){

    
    xSensorAggregationMode_t mode = xSensorAggregationGetMode();

    // if sensor aggregation mode is not enabled, sensors
    // are sent in separate messages
    if( mode == xSensAggModeDisabled){
        if( xDataPrepareSingleSensorMsg(sensor_data_packet) < 0 ){
            xDataResetSensorAggregationMsg();
            return;
        }
    }

    // sensor aggregation mode enabled, send one message containing data 
    // from all sensors
    else{
        int32_t ret;
        ret = xDataPrepareSensorAggregationMsg(sensor_data_packet);
        if( ret < 0 ){
            // some processing error happened, reset message
            xDataResetSensorAggregationMsg();
            return;
        }
        else if( ret == 1 ){
            //json packet not ready, wait for all sensors to be sampled   
            return;
        }

        //else -> all sensors sampled, message is ready to send
    }

    //LOG_DBG("%s\r\n",pMessage);
    LOG_DBG("Send Message\r\n");

    //Set quality of service and retain
    uint8_t qos = 0;
    bool retain = false;

    // Check if mqtt or mqttsn are connected and publish
    xClientStatusStruct_t mqtt_status = xWifiMqttClientGetStatus();
    xClientStatus_t mqttsn_status = xCellMqttSnClientGetStatus();

    if( mqtt_status.status == ClientConnected ){ 
        xWifiMqttClientPublish(gpTopicNameStr, pMessage, strlen(pMessage), qos, retain);
    }
    else if(mqttsn_status == ClientConnected){
        int32_t ret = xCellMqttSnClientPublish( MQTTSN_TOPIC_PREDEFINED,
            gpTopicAliasStr, pMessage, strlen(pMessage), qos, retain);

        if( ret != 0 ){
            LOG_ERR("Publish error %d \r\n", ret);
        }
    }
    // no client is connected, cannot send data
    else{
        LOG_ERR("Could not send data\r\n");
    }
    xDataResetSensorAggregationMsg();
}


