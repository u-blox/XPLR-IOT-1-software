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



#ifndef MOBILE_APP_BLE_PROTOCOL_H__
#define  MOBILE_APP_BLE_PROTOCOL_H__


/** @file
 * @brief This file contains the definition and the API implementing the
 * u-blox mobile application communication protocol via BLE. This API is used
 * for the communication between the u-blox mobile application and XPLR-IOT-1
 * Sensor Aggregation firmware. Changes in the protocol itself are depicted in 
 * these files alone (header and c files). 
 * 
 * The protocol files are independent of the rest of the application and could be
 * used by another firmware by just copying the files (the helper functions defined in 
 * num_array.h are needed too).
 * 
 * The way it works is:
 * - The firmware application takes its commands from whatever source
 * - This API parses the commands and returns the result to the application
 * - The application performs the necessary actions and uses this API to report results,
 * errors etc.
 * - This API takes the input from the application and prepares a response, according to
 * the protocol. It sends back to the API the response byte array
 * - The application can then send the response, however it likes.
 * 
 * This file contains the command and other necessary definitions.
 * The API functions are used to parse the incoming commands, prepare responses etc.
 * The actual implementation of the protocol is performed by these functions.
 * 
 * The protocol itself is described in the Readme file that should accompany these files.
 * 
 * Generally it is not good to change these file if they are going to be used with the 
 * u-blox mobile application.
 * 
 * 
 *  * ----------------------------------
 * Usage Example:
 * 
 * // incoming data are stored in pCmdData and their length is cmdDataSize.
 * 
 * mBleProtocolCmd_t command;   <-the parsed command will be returnd here
 * uint8_t CmdPayloadBuf[100];  <-the payload of parsed command will be returned here
 * uint16_t cmdPayloadLen;      <-the payload length will be returned here
 * 
 * // Parse command:
 * mBleProtocolParseCmd( pCmdData, cmdDataSize, &command, gpCmdPayloadBuf, 
 *                          sizeof( gpCmdPayloadBuf ), &cmdPayloadLen);
 * 
 * // Execute command:
 * switch(command){
 *  case READ_FW_VERSION:        error_code = executeReadFwVersionCommand()...
 *  case GET_MOBILE_APP_VERSION: error_code = executeGetMobileAppVersionCommand...
 *  case READ_UNIQUE_ID:         ...
 *  ...
 * }
 * 
 * // Prepare Command Response Code (execution result- error code)
 * mBleProtocolResponseCode_t responseCode;
 * if( error_code == SUCCESS){
 *     responseCode = M_CMD_RSP_OK;
 * }
 * else{ //error has happened
 *     responseCode = M_CMD_RSP_ERROR;
 * } 
 * 
 * // Prepare Response payload (if any)
 * mBleProtocolPayload_t payload;
 * 
 * // In case of error the payload is the actual error code of the application
 * if( error_code != SUCCESS){
 *     payload.length = 1;
 *     payload.errorCode = error_code;
 * }
 * 
 * // If operation is succesful and needs to send ap ayload with the response
 * if( error_code == SUCCESS){
 *    //example response -> firmware version
 *    payload.length = 2;
 *    payload.data.fw_version[0] =  FIRMWARE_VERSION_MAJOR;
 *    payload.data.fw_version[1] =  FIRMWARE_VERSION_MINOR;    
 * }
 * 
 * // if not payload is required in the response (just an ok without any additional data)
 * payload.length = 0;
 * 
 * // Get the actual response from the protocol module
 * uint16_t responseLen;
 * uint8_t ResponseBuffer[100];
 * int32_t ret = mBleProtocolPrepareResponse( command, responseCode, payload, ResponseBuffer,
 *                                            sizeof(ResponseBuffer), &responseLen );
 * 
 * // send the response via BLE
 * bt_nus_send( NULL, ResponseBuffer, responseLen);
 *
 * ---------------------------------- 
 * 
 */

#include <stdint.h>
#include <stdbool.h>


/* ----------------------------------------------------------------
 * PROTOCOL DEFINITIONS -- GENERAL
 * -------------------------------------------------------------- */

// Protocol Version Definition
#define M_BLE_PROT_VERSION_MAJOR    1
#define M_BLE_PROT_VERSION_MINOR    0

// Numbers used in protocol versioning scheme
// E.g. using 2 numbers, such as v1.2
// 3 numbers as in v1.2.3 etc...
#define M_BLE_PROT_MOBILE_APP_VERSIONING_NUMS  2


/* ----------------------------------------------------------------
 * HELP MACROS
 * -------------------------------------------------------------- */

/** Gets the size of a struct member */
#define member_size(type, member) sizeof(((type *)0)->member)

/* ----------------------------------------------------------------
 * TYPE DEFINITIONS
 * -------------------------------------------------------------- */

/** Commands that are described by this protocol.
 * 
 *  The commands are described as seen by the mobile app. 
 *  Eg. Read WIFI SSID means: Read the WiFi SSID saved in XPLR-IOT-1 device.
 *  Eg. Write WIFI SSID means: Save the given WiFi SSID in XPLR-IOT-1 device.
 * 
 * The API Returns this enum to the calling application after parsing the data
 * of incoming commands. The calling application can then use this enum to execute
 * a function according to the enum.
*/
typedef enum{
    READ_FW_VERSION = 0,         /**< Read Firmware Version */
    GET_MOBILE_APP_VERSION,      /**< Get Mobile App Version */
    READ_UNIQUE_ID,              /**< Read Device Unique ID */
    READ_PROTOCOL_VERSION,       /**< Read Communication Protocol Version */
    CLEAR_WIFI_CONFIG,           /**< Clear/Delete WiFi Config */
    READ_WIFI_SSID,              /**< Read Saved WiFi SSID */
    WRITE_WIFI_SSID,             /**< Write WiFi SSID */
    WRITE_WIFI_TYPE,             /**< Write WiFi Security Type */
    WRITE_WIFI_PSW,              /**< Write WiFi Password */
    SCAN_WIFI,                   /**< Scan for WiFi SSIDs. Just performs the scan.*/
    GET_NEXT_WIFI_SCAN_RESULT,   /**< Gets the next result of WiFI Scan. 
                                      SCAN_WIFI command should be issued first.
                                      Get all results by iterating this command */
    CLEAR_THINGSTREAM_CONFIG,    /**< Clear Thingstream Config */
    READ_THINGSTREAM_DOMAIN,     /**< Read Thingstream Domain */
    READ_IP_THING_CLIENT_ID,     /**< Read Thingstream IP Thing Client ID */
    READ_SIM_THING_DEVICE_ID,    /**< Read Thingstream SIM Thing Device ID */
    WRITE_THINGSTREAM_DOMAIN,    /**< Write Thingstream Domain */
    WRITE_IP_THING_CLIENT_ID,    /**< Write Thingstream IP Thing Client ID */
    WRITE_IP_THING_USERNAME,     /**< Write Thingstream IP Thing Username */
    WRITE_IP_THING_PSW,          /**< Write Thingstream IP Thing Password */
    WRITE_SIM_THING_DEVICE_ID,   /**< Write Thingstream SIM Thing Device ID */
    UNKNOWN_COMMAND              /**< Unknown Command */
}mBleProtocolCmd_t;


/** SCAN_WIFI Command response payload type
 * This command does not fetch results. Just returns how many results were found. 
 * Get the results with GET_NEXT_WIFI_SCAN_RESULT
 * There is one allResultAvailable member which is used to notify the user if all
 * results can be obtained with GET_NEXT_WIFI_SCAN_RESULT or some results cannot be
 * obtained
 */
typedef struct{
    bool allResultAvailable;          /**< If yes, all results from WiFi Scan can be obtained */
    uint16_t numOfResults;            /**< WiFi Scan number of results that can be obtained */
}mBleProtocolWifiScanPayload_t;


/** GET_NEXT_WIFI_SCAN_RESULT Command payload type */
typedef struct{
    uint16_t resultNum;               /**< WiFi Scan result number */
    uint8_t  resultSecType;           /**< WiFi Scan Result security type */
    uint8_t *ssidStrPtr;              /**< Pointer to an SSID string */
    int32_t rssi;                    /**< RSSI signal strength */
}mBleProtocolWifiScanGetResultPayload_t; 


/** A command or response consists of the header (actual command/response) and
 * depending on the command/response, sometimes it can also contain a (data)payload.
 * This structure describes the command/response payload, following the header(s).
*/
typedef struct{
    uint16_t length;                                       /**< Payload length. 
                                                                In case of wifiScanResult this is
                                                                the SSID string length */
    union{                        
        uint8_t *stringPtr;                                /**< Pointer to a string payload */
        int32_t errorCode;                                 /**< Error code returned from the application after execution*/
        uint8_t *byteBuffer;                               /**< Pointer to byte buffer */
        uint8_t fw_version[2];                             /**< Payload is firmware version */
        uint64_t uniqueDeviceID;                           /**< Payload is unique device id */
        mBleProtocolWifiScanPayload_t scanCmdResponse;     /**< SCAN_WIFI command Response payload */
        mBleProtocolWifiScanGetResultPayload_t scanResult; /**< WiFi Scan Result payload (GET_NEXT_WIFI_SCAN_RESULT) */
    }data;                                                 /**< Payload data contents (union) */
}mBleProtocolPayload_t;


/** Enum describing possible Command Response Codes. These describe the execution result of a command.
 * Actual command response codes are defined in this file by explicit definitions
 * (such as M_BLE_PROT_RSP_CODE_OK)
*/
typedef enum{
    M_CMD_RSP_OK,                /**< OK Response. Command succsesfully executed */
    M_CMD_RSP_NOT_FOUND,         /**< Not Found Response. Usually refers to files */
    M_CMD_RSP_ERROR             /**< Error Response. Should be followed by error code */
}mBleProtocolResponseCode_t;


/* ----------------------------------------------------------------
 * PROTOCOL DEFINITIONS -- RESPONSE DEFINITIONS
 * -------------------------------------------------------------- */

// -- Standard Responses -- //

// Unknown command Response
#define M_BLE_PROT_RSP_UNKNOWN_CMD  (0xDEADC0DE)

// Device is busy, cannot accept new command response
#define M_BLE_PROT_RSP_BUSY         (0xFFFFFFFF)


// -- Response Format Definition -- //

// Command response code header - Header that precedes the Command Response Code
// The command response code informs about the outcome of the command (Fail, Ok etc.)
#define M_BLE_PROT_RSP_CODE_HEADER      (uint16_t)(0xC0DE)

// Size of Command response code header
#define M_BLE_PROT_RSP_CODE_HEADER_SIZE sizeof( M_BLE_PROT_RSP_CODE_HEADER )


// -- Command Response Codes -- //

// Response Codes size in bytes
#define M_BLE_PROT_RSP_CODE_SIZE        2
#define M_BLE_PROT_RSP_CODE_OK          (0x0000)  /**< OK Response. Command succsesfully executed */
#define M_BLE_PROT_RSP_CODE_NOT_FOUND   (0x0001)  /**< Not Found Response. Usually refers to files */
#define M_BLE_PROT_RSP_CODE_ERROR       (0x0002)  /**< Error Response. Should be followed by error code */

// Size of actual error code following the M_BLE_PROT_RSP_CODE_ERROR Command Response Code
#define M_BLE_PROT_RSP_ERR_CODE_SIZE    4


/* ----------------------------------------------------------------
 * PROTOCOL DEFINITIONS -- COMMAND DEFINITIONS
 * -------------------------------------------------------------- */

// The commands are described as seen by the mobile app. 
// Eg. Read WIFI SSID means: Read the WiFi SSID saved in XPLR-IOT-1 device.
// Eg. Write WIFI SSID means: Save the given WiFi SSID in XPLR-IOT-1 device.

// Command Size (without payload)
#define M_BLE_PROT_CMD_HEADER_SIZE                    4

// -- System Commmands -- //

#define M_BLE_PROT_CMD_CODE_READ_FW_VERSION           (0x00000001)   /**< Read Firmware Version */
#define M_BLE_PROT_CMD_CODE_GET_MOBILE_APP_VERSION    (0x00000002)   /**< Get Mobile App Version */
#define M_BLE_PROT_CMD_CODE_READ_UNIQUE_ID            (0x00000003)   /**< Read Device Unique ID */
#define M_BLE_PROT_CMD_CODE_READ_PROTOCOL_VERSION     (0x00000004)   /**< Read Communication Protocol Version */

// -- WiFi Commands -- //
   
#define M_BLE_PROT_CMD_CODE_CLEAR_WIFI_CONFIG         (0x00000100)   /**< Clear/Delete WiFi Config */
#define M_BLE_PROT_CMD_CODE_READ_WIFI_SSID            (0x00000101)   /**< Read Saved WiFi SSID */
#define M_BLE_PROT_CMD_CODE_WRITE_WIFI_SSID           (0x00000102)   /**< Write WiFi SSID */
#define M_BLE_PROT_CMD_CODE_WRITE_WIFI_TYPE           (0x00000103)   /**< Write WiFi Security Type */
#define M_BLE_PROT_CMD_CODE_WRITE_WIFI_PSW            (0x00000104)   /**< Write WiFi Password */
#define M_BLE_PROT_CMD_CODE_SCAN_WIFI                 (0x00000105)   /**< Scan for WiFi SSIDs */
#define M_BLE_PROT_CMD_CODE_GET_NEXT_WIFI_SCAN_RESULT (0x00000106)   /**< Get Result from Scan WiFi command  */

// -- MQTT(SN) Thingstream Commands -- //

#define M_BLE_PROT_CMD_CODE_CLEAR_THINGSTREAM_CONFIG  (0x00000200)   /**< Clear Thingstream Config */
#define M_BLE_PROT_CMD_CODE_READ_THINGSTREAM_DOMAIN   (0x00000201)   /**< Read Thingstream Domain */
#define M_BLE_PROT_CMD_CODE_READ_IP_THING_CLIENT_ID   (0x00000202)   /**< Read Thingstream IP Thing Client ID */
#define M_BLE_PROT_CMD_CODE_READ_SIM_THING_DEVICE_ID  (0x00000203)   /**< Read Thingstream SIM Thing Device ID */
#define M_BLE_PROT_CMD_CODE_WRITE_THINGSTREAM_DOMAIN  (0x00000204)   /**< Write Thingstream Domain */
#define M_BLE_PROT_CMD_CODE_WRITE_IP_THING_CLIENT_ID  (0x00000205)   /**< Write Thingstream IP Thing Client ID */
#define M_BLE_PROT_CMD_CODE_WRITE_IP_THING_USERNAME   (0x00000206)   /**< Write Thingstream IP Thing Username */
#define M_BLE_PROT_CMD_CODE_WRITE_IP_THING_PSW        (0x00000207)   /**< Write Thingstream IP Thing Password */
#define M_BLE_PROT_CMD_CODE_WRITE_SIM_THING_DEVICE_ID (0x00000208)   /**< Write Thingstream SIM Thing Device ID */


// -- Readable String Representation of commands -- //

// Max String Length for command Description
#define M_BLE_PROT_COMMAND_STRING_MAX_SIZE            100

#define M_BLE_PROT_CMD_STR_READ_FW_VERSION            "Read Firmware Version"
#define M_BLE_PROT_CMD_STR_GET_MOBILE_APP_VERSION     "Get Mobile App Version"
#define M_BLE_PROT_CMD_STR_READ_UNIQUE_ID             "Read Device Unique ID"
#define M_BLE_PROT_CMD_STR_READ_PROTOCOL_VERSION      "Read Com Protocol Version" 
#define M_BLE_PROT_CMD_STR_CLEAR_WIFI_CONFIG          "Clear/Delete WiFi Config"
#define M_BLE_PROT_CMD_STR_READ_WIFI_SSID             "Read Saved WiFi SSID"
#define M_BLE_PROT_CMD_STR_WRITE_WIFI_SSID            "Write WiFi SSID Config"
#define M_BLE_PROT_CMD_STR_WRITE_WIFI_TYPE            "Write WiFi Security Type"
#define M_BLE_PROT_CMD_STR_WRITE_WIFI_PSW             "Write WiFi Password Config"
#define M_BLE_PROT_CMD_STR_SCAN_WIFI                  "Scan for WiFi SSIDs"
#define M_BLE_PROT_CMD_STR_GET_NEXT_WIFI_SCAN_RESULT "Get Next Result of Scan WiFi"
#define M_BLE_PROT_CMD_STR_UNKNOWN_COMMAND            "Unknown Command"
#define M_BLE_PROT_CMD_STR_CLEAR_THINGSTREAM_CONFIG   "Clear MQTT(SN) Thingstream Config"
#define M_BLE_PROT_CMD_STR_READ_THINGSTREAM_DOMAIN    "Read Thingstream Domain"
#define M_BLE_PROT_CMD_STR_READ_IP_THING_CLIENT_ID    "Read Thingstream IP Thing Client ID"
#define M_BLE_PROT_CMD_STR_READ_SIM_THING_DEVICE_ID   "Read Thingstream SIM Thing Device ID"
#define M_BLE_PROT_CMD_STR_WRITE_THINGSTREAM_DOMAIN   "Write Thingstream Domain"
#define M_BLE_PROT_CMD_STR_WRITE_IP_THING_CLIENT_ID   "Write Thingstream IP Thing Client ID"
#define M_BLE_PROT_CMD_STR_WRITE_IP_THING_USERNAME    "Write Thingstream IP Thing Username"
#define M_BLE_PROT_CMD_STR_WRITE_IP_THING_PSW         "Write Thingstream IP Thing Password" 
#define M_BLE_PROT_CMD_STR_WRITE_SIM_THING_DEVICE_ID  "Write Thingstream SIM Thing Device ID"


/* ----------------------------------------------------------------
 * MINIMUM COMMAND/RESPONSE SIZES
 * -------------------------------------------------------------- */

// Minimum command size, without any payload
#define M_BLE_PROT_CMD_SIZE_MIN    M_BLE_PROT_CMD_HEADER_SIZE

// Minimum Response Size without payload
#define M_BLE_PROT_RSP_SIZE_MIN    (M_BLE_PROT_CMD_SIZE_MIN + M_BLE_PROT_RSP_CODE_HEADER_SIZE + M_BLE_PROT_RSP_CODE_SIZE)


/* ----------------------------------------------------------------
 * API FUNCTION ERROR CODES
 * -------------------------------------------------------------- */

// The API Functions in this module can return the following error codes
// THESE ARE NOT RESPONSE CODES COMMUNICATED THROUGH THE PROTOCOL
// They are error codes returned by functions at a firmware-application level.

#define M_BLE_PROT_FUNERR_SUCCESS        (0)
#define M_BLE_PROT_FUNERR_BUF_OVERFLOW   (-1)
#define M_BLE_PROT_FUNERR_INVALID_PARAM  (-2)


/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */


/** This function takes as input the whole byte buffer as received from
 * BLE and parses its contents. The result is the command and a byte buffer
 * containing the payload (if any) of the command. 
 *
 * @param[in]  inBuffer             In: Data as received by BLE.
 * @param[in]  inBufferLen          In: data size - inBuffer length.
 * @param[out] command              Ou: Parsed command enum.
 * @param[out] cmdDataBuffer        Out: Buffer containining the payload of the parsed command.
 * @param[in]  cmdDataBufferMaxLen  In: Maximum size of cmdDataBuffer.
 * @param[out] cmdDataLen           Out: Payload length of parsed command.
 * @return                          zero on success else negative error code.
 */
void mBleProtocolParseCmd( uint8_t *inBuffer, uint16_t inBufferLen, 
                           mBleProtocolCmd_t *command, uint8_t *cmdDataBuffer, 
                           uint16_t cmdDataBufferMaxLen, uint16_t *cmdDataLen);


/** This function returns the actual response that should be send as answer to a particular
 * command. It takes as input:
 * - The command to which it should respond
 * - The Response Code, which is actually the result of the result of the command execution
 * (ok, fail etc...)
 * - The payload if the command needs to send some info (eg. In read SSID command the response
 * payload is the SSID)
 * 
 * The function then puts the exact response, according to protocol, to the response Buffer
 * provided.
 *
 * @param[in]  command           In: Command for which the response is asked.
 * @param[in]  respCode          In: Result of the command execution as a Command Response Code enum.
 * @param[in]  payload           In: Payload of response - In case of error it holds the error code.
 * @param[out] respBuffer        Out: Buffer to which the response will be placed.
 * @param[in]  respBufferMaxLen  In: Maximum size of respBuffer.
 * @param[out] respBufferLen     Out: Response length.
 * @return                       zero on success else negative error code.
 */
int32_t mBleProtocolPrepareResponse( mBleProtocolCmd_t command, mBleProtocolResponseCode_t respCode,
                                     mBleProtocolPayload_t payload, uint8_t *respBuffer, 
                                     uint16_t respBufferMaxLen, uint16_t *respBufferLen );


/** Function that returns the string representation/description of a command (string terminated by zero).
 *
 * @param[in]  command              In: Command enum for which the string is requested.
 * @param[out] commandString        Out: The string representation/description of the command.
 * @param[in]  commandStringMaxLen  In: The maximum size of commandString.
 * @return                          zero on success else negative error code.
 */
int32_t mBleProtocolGetCommandString( mBleProtocolCmd_t command, uint8_t *commandString,
                                      uint16_t commandStringMaxLen);




#endif  //MOBILE_APP_BLE_PROTOCOL_H__