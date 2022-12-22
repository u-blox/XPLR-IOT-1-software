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
 * @brief This file contains the actual implementation of the communication protocol
 * defined in "mobile_app_ble_protocol.h". This protocol is used to implement the communication
 * between XPLR-IOT-1 and u-blox mobile application.
 * 
 * The protocol is described in the Readme file that should accompany this file.
 */


#include "mobile_app_ble_protocol.h"
#include <string.h>

// Helper functions
#include "num_array.h"


/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION
 * -------------------------------------------------------------- */

/** Function preparing response for an unknown command. The payload contains the
 * original contents of the command - up to 4 bytes (for reference to the unknown header).
 * This happens so when the user gets an unknown command response he also has the header
 * to which command this unknown command response refers.
 *
 * @param[in]  payload          In: In this case payload refers to the original command contents
 *                              -up to 4 bytes.
 * @param[out] respBuffer       Out: The buffer to which the response will be returned.
 * @param[out] respBufferMaxLen In: The maximum length of respBuffer.
 * @param[out] respBufferLen    Out: The length of the response.
 * @return                      zero on success else negative error code.
 */
static int32_t mBleProtocolUnknownCommandResponse( mBleProtocolPayload_t payload, uint8_t *respBuffer, 
                                  uint16_t respBufferMaxLen, uint16_t *respBufferLen );


/** Function preparing response for a command that has been executed and returned an error.
 * The payload contains the actual error code returned by the execution application.
 *
 * @param[in]  command          In: The command executed, which resulted in an error
 * @param[in]  payload          In: In this case the payload contains the error code returned by the
 *                              application which executed the command. 
 * @param[out] respBuffer       Out: The buffer to which the response will be returned.
 * @param[out] respBufferMaxLen In: The maximum length of respBuffer.
 * @param[out] respBufferLen    Out: The length of the response.
 * @return                      zero on success else negative error code.
 */
static int32_t mBleProtocolErrorResponse( mBleProtocolCmd_t command, mBleProtocolPayload_t payload,
                                  uint8_t *respBuffer, uint16_t respBufferMaxLen, uint16_t *respBufferLen );


/** Function preparing response for a command searching a configuration/file but that was not found.
 * This is different from an error. That is why this case is treated separately.
 *
 * @param[in]  command          In: The command executed, which resulted in a "not found" situation
 * @param[out] respBuffer       Out: The buffer to which the response will be returned.
 * @param[out] respBufferMaxLen In: The maximum length of respBuffer.
 * @param[out] respBufferLen    Out: The length of the response.
 * @return                      zero on success else negative error code.
 */
static int32_t mBleProtocolNotFoundResponse( mBleProtocolCmd_t command, uint8_t *respBuffer, 
                                  uint16_t respBufferMaxLen, uint16_t *respBufferLen );


/** Function preparing a response that does not need any additional payload. 
 *
 * @param[in]  command          In: The command executed, to which this response applies
 * @param[out] respBuffer       Out: The buffer to which the response will be returned.
 * @param[out] respBufferMaxLen In: The maximum length of respBuffer.
 * @param[out] respBufferLen    Out: The length of the response.
 * @return                      zero on success else negative error code.
 */
static int32_t mBleProtocolNoPayloadResponse( mBleProtocolCmd_t command, uint8_t *respBuffer, 
                                  uint16_t respBufferMaxLen, uint16_t *respBufferLen );


/** Function preparing a response that does include additional payload. For example in the "Read SSID"
 * command the response includes in its response the SSID in the payload .
 *
 * @param[in]  command          In: The command executed, to which this response applies
 * @param[in]  payload          In: The payload of the response (the answer to something the command 
 *                              requests)
 * @param[out] respBuffer       Out: The buffer to which the response will be returned.
 * @param[out] respBufferMaxLen In: The maximum length of respBuffer.
 * @param[out] respBufferLen    Out: The length of the response.
 * @return                      zero on success else negative error code.
 */
static int32_t mBleProtocolWithPayloadResponse( mBleProtocolCmd_t command, mBleProtocolPayload_t payload,
                                  uint8_t *respBuffer, uint16_t respBufferMaxLen, uint16_t *respBufferLen );


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/** Helping array that matches the Command enums to their string descriptions*/
const char *const gpCommandStr[]={
	[ READ_FW_VERSION ]           = M_BLE_PROT_CMD_STR_READ_FW_VERSION,
    [ GET_MOBILE_APP_VERSION ]    = M_BLE_PROT_CMD_STR_GET_MOBILE_APP_VERSION,
    [ READ_UNIQUE_ID ]            = M_BLE_PROT_CMD_STR_READ_UNIQUE_ID,
    [ READ_PROTOCOL_VERSION ]     = M_BLE_PROT_CMD_STR_READ_PROTOCOL_VERSION,
    [ CLEAR_WIFI_CONFIG ]         = M_BLE_PROT_CMD_STR_CLEAR_WIFI_CONFIG,
    [ READ_WIFI_SSID ]            = M_BLE_PROT_CMD_STR_READ_WIFI_SSID,
    [ WRITE_WIFI_SSID ]           = M_BLE_PROT_CMD_STR_WRITE_WIFI_SSID,
    [ WRITE_WIFI_TYPE ]           = M_BLE_PROT_CMD_STR_WRITE_WIFI_TYPE,
    [ WRITE_WIFI_PSW ]            = M_BLE_PROT_CMD_STR_WRITE_WIFI_PSW,
    [ SCAN_WIFI ]                 = M_BLE_PROT_CMD_STR_SCAN_WIFI,
    [ GET_NEXT_WIFI_SCAN_RESULT ] = M_BLE_PROT_CMD_STR_GET_NEXT_WIFI_SCAN_RESULT,
    [ CLEAR_THINGSTREAM_CONFIG ]  = M_BLE_PROT_CMD_STR_CLEAR_THINGSTREAM_CONFIG,
    [ READ_THINGSTREAM_DOMAIN  ]  = M_BLE_PROT_CMD_STR_READ_THINGSTREAM_DOMAIN,
    [ READ_IP_THING_CLIENT_ID  ]  = M_BLE_PROT_CMD_STR_READ_IP_THING_CLIENT_ID,
    [ READ_SIM_THING_DEVICE_ID ]  = M_BLE_PROT_CMD_STR_READ_SIM_THING_DEVICE_ID,
    [ WRITE_THINGSTREAM_DOMAIN ]  = M_BLE_PROT_CMD_STR_WRITE_THINGSTREAM_DOMAIN,
    [ WRITE_IP_THING_CLIENT_ID ]  = M_BLE_PROT_CMD_STR_WRITE_IP_THING_CLIENT_ID,
    [ WRITE_IP_THING_USERNAME  ]  = M_BLE_PROT_CMD_STR_WRITE_IP_THING_USERNAME,
    [ WRITE_IP_THING_PSW       ]  = M_BLE_PROT_CMD_STR_WRITE_IP_THING_PSW,
    [ WRITE_SIM_THING_DEVICE_ID ] = M_BLE_PROT_CMD_STR_WRITE_SIM_THING_DEVICE_ID,
	[ UNKNOWN_COMMAND ]           = M_BLE_PROT_CMD_STR_UNKNOWN_COMMAND
};


/** Helping array that matches the Command enums to their Code definitions*/
const uint32_t gpCommandCodes[]={
    [ READ_FW_VERSION ]           = M_BLE_PROT_CMD_CODE_READ_FW_VERSION,
    [ GET_MOBILE_APP_VERSION ]    = M_BLE_PROT_CMD_CODE_GET_MOBILE_APP_VERSION,
    [ READ_UNIQUE_ID ]            = M_BLE_PROT_CMD_CODE_READ_UNIQUE_ID,
    [ READ_PROTOCOL_VERSION ]     = M_BLE_PROT_CMD_CODE_READ_PROTOCOL_VERSION,
    [ CLEAR_WIFI_CONFIG ]         = M_BLE_PROT_CMD_CODE_CLEAR_WIFI_CONFIG,
    [ READ_WIFI_SSID ]            = M_BLE_PROT_CMD_CODE_READ_WIFI_SSID,
    [ WRITE_WIFI_SSID ]           = M_BLE_PROT_CMD_CODE_WRITE_WIFI_SSID,
    [ WRITE_WIFI_TYPE ]           = M_BLE_PROT_CMD_CODE_WRITE_WIFI_TYPE,
    [ WRITE_WIFI_PSW ]            = M_BLE_PROT_CMD_CODE_WRITE_WIFI_PSW,
    [ SCAN_WIFI ]                 = M_BLE_PROT_CMD_CODE_SCAN_WIFI,
    [ GET_NEXT_WIFI_SCAN_RESULT ] = M_BLE_PROT_CMD_CODE_GET_NEXT_WIFI_SCAN_RESULT,
    [ CLEAR_THINGSTREAM_CONFIG ]  = M_BLE_PROT_CMD_CODE_CLEAR_THINGSTREAM_CONFIG,
    [ READ_THINGSTREAM_DOMAIN  ]  = M_BLE_PROT_CMD_CODE_READ_THINGSTREAM_DOMAIN,
    [ READ_IP_THING_CLIENT_ID  ]  = M_BLE_PROT_CMD_CODE_READ_IP_THING_CLIENT_ID,
    [ READ_SIM_THING_DEVICE_ID ]  = M_BLE_PROT_CMD_CODE_READ_SIM_THING_DEVICE_ID,
    [ WRITE_THINGSTREAM_DOMAIN ]  = M_BLE_PROT_CMD_CODE_WRITE_THINGSTREAM_DOMAIN,
    [ WRITE_IP_THING_CLIENT_ID ]  = M_BLE_PROT_CMD_CODE_WRITE_IP_THING_CLIENT_ID,
    [ WRITE_IP_THING_USERNAME  ]  = M_BLE_PROT_CMD_CODE_WRITE_IP_THING_USERNAME,
    [ WRITE_IP_THING_PSW       ]  = M_BLE_PROT_CMD_CODE_WRITE_IP_THING_PSW,
    [ WRITE_SIM_THING_DEVICE_ID ] = M_BLE_PROT_CMD_CODE_WRITE_SIM_THING_DEVICE_ID,
	[ UNKNOWN_COMMAND ]           = M_BLE_PROT_RSP_UNKNOWN_CMD
};


/* ----------------------------------------------------------------
 * PUBLIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


void mBleProtocolParseCmd( uint8_t *inBuffer, uint16_t inBufferLen, 
                           mBleProtocolCmd_t *command, uint8_t *cmdDataBuffer, 
                           uint16_t cmdDataBufferMaxLen, uint16_t *cmdDataLen){

    // A command in the protocol needs at least a M_BLE_PROT_CMD_HEADER_SIZE-byte
    // header to recognize the command, or else it cannot be recognized. In this case 
    // the command Data contains the original command contents (for reference in the response).
    if( inBufferLen < M_BLE_PROT_CMD_HEADER_SIZE ){
        *command = UNKNOWN_COMMAND;
        memcpy( cmdDataBuffer, inBuffer, inBufferLen );
        *cmdDataLen = inBufferLen;
        return;
    }

    // Assign the 4-byte command header to a uint32. It also takes care of endianness
    uint32_t cmdHeaderCode = arrayToUint32( inBuffer );

    // Parses commands that do not have extra payload (command data after the command header)
    if( inBufferLen == M_BLE_PROT_CMD_HEADER_SIZE ){
        
        memset( cmdDataBuffer, 0, cmdDataBufferMaxLen );
        *cmdDataLen = 0;

        switch( cmdHeaderCode ){

            case M_BLE_PROT_CMD_CODE_READ_FW_VERSION: *command = READ_FW_VERSION;
            break;

            case M_BLE_PROT_CMD_CODE_READ_PROTOCOL_VERSION: *command = READ_PROTOCOL_VERSION;
            break;

            case M_BLE_PROT_CMD_CODE_READ_UNIQUE_ID: *command = READ_UNIQUE_ID;
            break;

            case M_BLE_PROT_CMD_CODE_CLEAR_WIFI_CONFIG: *command = CLEAR_WIFI_CONFIG;
            break; 

            case M_BLE_PROT_CMD_CODE_READ_WIFI_SSID: *command = READ_WIFI_SSID;
            break;  

            case M_BLE_PROT_CMD_CODE_SCAN_WIFI: *command = SCAN_WIFI;
            break;  

            case M_BLE_PROT_CMD_CODE_GET_NEXT_WIFI_SCAN_RESULT: *command = GET_NEXT_WIFI_SCAN_RESULT;
            break;  

            case M_BLE_PROT_CMD_CODE_CLEAR_THINGSTREAM_CONFIG: *command = CLEAR_THINGSTREAM_CONFIG;
            break;

            case M_BLE_PROT_CMD_CODE_READ_THINGSTREAM_DOMAIN: *command = READ_THINGSTREAM_DOMAIN;
            break;

            case M_BLE_PROT_CMD_CODE_READ_IP_THING_CLIENT_ID: *command = READ_IP_THING_CLIENT_ID;
            break;

            case M_BLE_PROT_CMD_CODE_READ_SIM_THING_DEVICE_ID: *command = READ_SIM_THING_DEVICE_ID;
            break;   

            default: *command = UNKNOWN_COMMAND;
                     // In this case the command Data contains the original
                     // command contents (for reference).
                     memcpy( cmdDataBuffer, inBuffer, inBufferLen );
                     *cmdDataLen = inBufferLen;
        }

    }  // end:if that parses commands without payloads


    // Parses commands that have payload (command data after the command header)
    // ( inBufferLen > M_BLE_PROT_CMD_HEADER_SIZE )
    else{ 

        switch( cmdHeaderCode ){   

            case M_BLE_PROT_CMD_CODE_GET_MOBILE_APP_VERSION: *command = GET_MOBILE_APP_VERSION;
            break;
 
            case M_BLE_PROT_CMD_CODE_WRITE_WIFI_SSID: *command = WRITE_WIFI_SSID;
            break;

            case M_BLE_PROT_CMD_CODE_WRITE_WIFI_TYPE: *command = WRITE_WIFI_TYPE;
            break;

            case M_BLE_PROT_CMD_CODE_WRITE_WIFI_PSW: *command = WRITE_WIFI_PSW;
            break;

            case M_BLE_PROT_CMD_CODE_WRITE_THINGSTREAM_DOMAIN: *command = WRITE_THINGSTREAM_DOMAIN;
            break;

            case M_BLE_PROT_CMD_CODE_WRITE_IP_THING_CLIENT_ID: *command = WRITE_IP_THING_CLIENT_ID;
            break;

            case M_BLE_PROT_CMD_CODE_WRITE_IP_THING_USERNAME: *command = WRITE_IP_THING_USERNAME;
            break;

            case M_BLE_PROT_CMD_CODE_WRITE_IP_THING_PSW: *command = WRITE_IP_THING_PSW;
            break;

            case M_BLE_PROT_CMD_CODE_WRITE_SIM_THING_DEVICE_ID: *command = WRITE_SIM_THING_DEVICE_ID;
            break;

            default: *command = UNKNOWN_COMMAND;         
        }   

        if ( *command == UNKNOWN_COMMAND ){
            // in this case the command Data contains the header of the unknown
            // command (for reference)
            memcpy( cmdDataBuffer, inBuffer, M_BLE_PROT_CMD_HEADER_SIZE );
            *cmdDataLen = M_BLE_PROT_CMD_HEADER_SIZE;
        }
        else{
            memcpy( cmdDataBuffer, (void *)(inBuffer + M_BLE_PROT_CMD_HEADER_SIZE), inBufferLen - M_BLE_PROT_CMD_HEADER_SIZE );
            *cmdDataLen = inBufferLen - M_BLE_PROT_CMD_HEADER_SIZE;
        }

    } // end: else that parses commands with payloads

    return;
}


int32_t mBleProtocolPrepareResponse( mBleProtocolCmd_t command, mBleProtocolResponseCode_t respCode,
                                  mBleProtocolPayload_t payload, uint8_t *respBuffer, 
                                  uint16_t respBufferMaxLen, uint16_t *respBufferLen ){

    // make sure the response buffer is clean
    memset(respBuffer,0,respBufferMaxLen); 

    // Unknown Command Response
    if (command == UNKNOWN_COMMAND ){
        return mBleProtocolUnknownCommandResponse( payload, respBuffer, respBufferMaxLen, respBufferLen );
    }

    // Error/Fail Response
    else if( respCode == M_CMD_RSP_ERROR ){
        return mBleProtocolErrorResponse( command, payload, respBuffer, respBufferMaxLen, respBufferLen );
    }

    // Not Found Response (still success - just no available data)
    else if( respCode == M_CMD_RSP_NOT_FOUND ){
        return mBleProtocolNotFoundResponse( command, respBuffer, respBufferMaxLen, respBufferLen );
    }

    // Success Response without Payload (successful execution of e.g. write/set commands)
    else if( ( respCode == M_CMD_RSP_OK ) && ( payload.length == 0 ) ){
        return mBleProtocolNoPayloadResponse( command, respBuffer, respBufferMaxLen, respBufferLen );
    }

    // Success Response with Payload (successful execution of e.g. read/get commands)
    else if( ( respCode == M_CMD_RSP_OK ) && ( payload.length > 0 ) ){
        return mBleProtocolWithPayloadResponse( command, payload, respBuffer, respBufferMaxLen, respBufferLen );
    }

    else{
        return M_BLE_PROT_FUNERR_INVALID_PARAM;
    }

}


int32_t mBleProtocolGetCommandString( mBleProtocolCmd_t command, uint8_t *commandString, uint16_t commandStringMaxLen){

    // check input parameters
    if( command > UNKNOWN_COMMAND ){
        return M_BLE_PROT_FUNERR_INVALID_PARAM;
    }

    if ( strlen( gpCommandStr[command] ) > commandStringMaxLen ){
        return M_BLE_PROT_FUNERR_BUF_OVERFLOW;
    }

    // Get command's string description
    strcpy( commandString, gpCommandStr[command] );

    return M_BLE_PROT_FUNERR_SUCCESS;
}


/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */

static int32_t mBleProtocolUnknownCommandResponse( mBleProtocolPayload_t payload, uint8_t *respBuffer, 
                                  uint16_t respBufferMaxLen, uint16_t *respBufferLen ){

    // Unknown Command response is:
    // Original Command Header (up to 4-bytes) + M_BLE_PROT_RSP_UNKNOWN_CMD

    uint32_t unknownCommandCode = M_BLE_PROT_RSP_UNKNOWN_CMD;
    // Define size of response
    *respBufferLen = payload.length + sizeof(unknownCommandCode);
    
    // check if respBuffer is big enough
    if ( *respBufferLen > respBufferMaxLen ){
        return M_BLE_PROT_FUNERR_BUF_OVERFLOW;
    }

    // check if payload is valid
    if( payload.length > M_BLE_PROT_CMD_HEADER_SIZE ){
        return M_BLE_PROT_FUNERR_INVALID_PARAM;
    }

    //prepare response
    memcpy( respBuffer, payload.data.byteBuffer, payload.length );  //header
    uint32ToArray( ( respBuffer + payload.length ), unknownCommandCode );
    return M_BLE_PROT_FUNERR_SUCCESS;
}


static int32_t mBleProtocolErrorResponse( mBleProtocolCmd_t command, mBleProtocolPayload_t payload,
                                  uint8_t *respBuffer, uint16_t respBufferMaxLen, uint16_t *respBufferLen ){

    // Error Response is:
    // Original Cmd Header + M_BLE_PROT_RSP_CODE_HEADER + M_BLE_PROT_RSP_CODE_ERROR + Actual 4-byte error code

    // check if respBuffer is big enough to hold the error response
    uint16_t responseSize = M_BLE_PROT_CMD_HEADER_SIZE + M_BLE_PROT_RSP_CODE_SIZE + M_BLE_PROT_RSP_CODE_SIZE + 
                            sizeof(payload.data.errorCode);

    // check parameters
    if( responseSize > respBufferMaxLen ){
        return M_BLE_PROT_FUNERR_BUF_OVERFLOW;      
    }

    if( command > UNKNOWN_COMMAND ){
        return M_BLE_PROT_FUNERR_INVALID_PARAM;
    }

    uint8_t index = 0;
    // Command Header
    uint32ToArray( respBuffer, gpCommandCodes[command] );
    // Response Code Header
    index += M_BLE_PROT_CMD_HEADER_SIZE;
    uint16ToArray( respBuffer + index, M_BLE_PROT_RSP_CODE_HEADER );
    // Response Code
    index += M_BLE_PROT_RSP_CODE_SIZE;
    uint16ToArray( respBuffer + index, M_BLE_PROT_RSP_CODE_ERROR );
    // Error Code as payload
    index += M_BLE_PROT_RSP_CODE_SIZE;
    int32ToArray( respBuffer + index, payload.data.errorCode );

    *respBufferLen = responseSize;

    return M_BLE_PROT_FUNERR_SUCCESS;
}


static int32_t mBleProtocolNotFoundResponse( mBleProtocolCmd_t command, uint8_t *respBuffer, 
                                  uint16_t respBufferMaxLen, uint16_t *respBufferLen ){

    // Not found Response is:
    // Original Command Header + M_BLE_PROT_RSP_CODE_HEADER + M_BLE_PROT_RSP_CODE_NOT_FOUND

    // check if respBuffer is big enough to hold the error response
    uint16_t responseSize = M_BLE_PROT_CMD_HEADER_SIZE + M_BLE_PROT_RSP_CODE_SIZE + M_BLE_PROT_RSP_CODE_SIZE;

    // check parameters
    if( responseSize > respBufferMaxLen ){
        return M_BLE_PROT_FUNERR_BUF_OVERFLOW;      
    }

    if( command > UNKNOWN_COMMAND ){
        return M_BLE_PROT_FUNERR_INVALID_PARAM;
    }

    uint8_t index = 0;
    // Command Header
    uint32ToArray( respBuffer, gpCommandCodes[command] );
    // Response Code Header
    index += M_BLE_PROT_CMD_HEADER_SIZE;
    uint16ToArray( respBuffer + index, M_BLE_PROT_RSP_CODE_HEADER );
    // Response Code
    index += M_BLE_PROT_RSP_CODE_SIZE;
    uint16ToArray( respBuffer + index, M_BLE_PROT_RSP_CODE_NOT_FOUND );

    *respBufferLen = responseSize;

    return M_BLE_PROT_FUNERR_SUCCESS;
}


static int32_t mBleProtocolNoPayloadResponse( mBleProtocolCmd_t command, uint8_t *respBuffer, 
                                  uint16_t respBufferMaxLen, uint16_t *respBufferLen ){

    // Not Payload Response is:
    // Original Command Header + M_BLE_PROT_RSP_CODE_HEADER + M_BLE_PROT_RSP_CODE_OK
    
    // check if respBuffer is big enough to hold the error response
    uint16_t responseSize = M_BLE_PROT_CMD_HEADER_SIZE + M_BLE_PROT_RSP_CODE_SIZE + M_BLE_PROT_RSP_CODE_SIZE;

    if( responseSize > respBufferMaxLen ){
        return M_BLE_PROT_FUNERR_BUF_OVERFLOW;      
    }

    if( command > UNKNOWN_COMMAND ){
        return M_BLE_PROT_FUNERR_INVALID_PARAM;
    }

    uint8_t index = 0;
    // Command Header
    uint32ToArray( respBuffer, gpCommandCodes[command] );
    // Response Code Header
    index += M_BLE_PROT_CMD_HEADER_SIZE;
    uint16ToArray( respBuffer + index, M_BLE_PROT_RSP_CODE_HEADER );
    // Response Code
    index += M_BLE_PROT_RSP_CODE_SIZE;
    uint16ToArray( respBuffer + index, M_BLE_PROT_RSP_CODE_OK );

    *respBufferLen = responseSize;

    return M_BLE_PROT_FUNERR_SUCCESS;
}


static int32_t mBleProtocolWithPayloadResponse( mBleProtocolCmd_t command, mBleProtocolPayload_t payload,
                                  uint8_t *respBuffer, uint16_t respBufferMaxLen, uint16_t *respBufferLen ){

    // Response with Payload is:
    // Original Command Header + M_BLE_PROT_RSP_CODE_HEADER + M_BLE_PROT_RSP_CODE_OK + Payload
    
    if( command > UNKNOWN_COMMAND ){
        return M_BLE_PROT_FUNERR_INVALID_PARAM;
    }

    volatile uint16_t responseSize;
    
    uint8_t index = 0;
    uint32_t commandCode = gpCommandCodes[command];
    // Command Header
    uint32ToArray( respBuffer, commandCode );
    // Response Code Header
    index += M_BLE_PROT_CMD_HEADER_SIZE;
    uint16ToArray( respBuffer + index, M_BLE_PROT_RSP_CODE_HEADER );
    // Response Code
    index += M_BLE_PROT_RSP_CODE_SIZE;
    uint16ToArray( respBuffer + index, M_BLE_PROT_RSP_CODE_OK );

    //Payload Depends on Command
    index += M_BLE_PROT_RSP_CODE_SIZE;

    switch( commandCode ){

        case M_BLE_PROT_CMD_CODE_READ_PROTOCOL_VERSION:  //same as M_BLE_PROT_CMD_CODE_READ_FW_VERSION
                                                  responseSize = index + sizeof(payload.data.fw_version);
                                                  if( responseSize > respBufferMaxLen ){
                                                    return M_BLE_PROT_FUNERR_BUF_OVERFLOW; 
                                                  }
                                                  respBuffer[index] = payload.data.fw_version[0];
                                                  respBuffer[++index] = payload.data.fw_version[1];
                                                  break;

        case M_BLE_PROT_CMD_CODE_READ_FW_VERSION: responseSize = index + sizeof(payload.data.fw_version);
                                                  if( responseSize > respBufferMaxLen ){
                                                    return M_BLE_PROT_FUNERR_BUF_OVERFLOW; 
                                                  }
                                                  respBuffer[index] = payload.data.fw_version[0];
                                                  respBuffer[++index] = payload.data.fw_version[1];
                                                  break;

        case M_BLE_PROT_CMD_CODE_READ_UNIQUE_ID:  responseSize = index + sizeof(payload.data.uniqueDeviceID);
                                                  if( responseSize > respBufferMaxLen ){
                                                    return M_BLE_PROT_FUNERR_BUF_OVERFLOW; 
                                                  }
                                                  uint64ToArray( respBuffer + index , payload.data.uniqueDeviceID );
                                                  break;

        case M_BLE_PROT_CMD_CODE_READ_WIFI_SSID:  responseSize = index + payload.length;
                                                  if( responseSize > respBufferMaxLen ){
                                                    return M_BLE_PROT_FUNERR_BUF_OVERFLOW; 
                                                  }
                                                  memcpy( respBuffer + index, payload.data.stringPtr, payload.length );  
                                                  break; 

        case M_BLE_PROT_CMD_CODE_READ_THINGSTREAM_DOMAIN: responseSize = index + payload.length;
                                                          if( responseSize > respBufferMaxLen ){
                                                            return M_BLE_PROT_FUNERR_BUF_OVERFLOW; 
                                                          }
                                                          memcpy( respBuffer + index, payload.data.stringPtr, payload.length );  
                                                          break;  

        case M_BLE_PROT_CMD_CODE_READ_IP_THING_CLIENT_ID: responseSize = index + payload.length;
                                                          if( responseSize > respBufferMaxLen ){
                                                            return M_BLE_PROT_FUNERR_BUF_OVERFLOW; 
                                                          }
                                                          memcpy( respBuffer + index, payload.data.stringPtr, payload.length );  
                                                          break;

        case M_BLE_PROT_CMD_CODE_READ_SIM_THING_DEVICE_ID: responseSize = index + payload.length;
                                                           if( responseSize > respBufferMaxLen ){
                                                             return M_BLE_PROT_FUNERR_BUF_OVERFLOW; 
                                                           }
                                                           memcpy( respBuffer + index, payload.data.stringPtr, payload.length );  
                                                           break;    

        case M_BLE_PROT_CMD_CODE_SCAN_WIFI:   responseSize = index + 
                                                             member_size( mBleProtocolWifiScanPayload_t, allResultAvailable )+
                                                             member_size( mBleProtocolWifiScanPayload_t, numOfResults );

                                              if( responseSize > respBufferMaxLen ){
                                                return M_BLE_PROT_FUNERR_BUF_OVERFLOW; 
                                              }

                                              // all Results available flag
                                              respBuffer[index] = payload.data.scanCmdResponse.allResultAvailable;
                                              index += member_size( mBleProtocolWifiScanPayload_t, allResultAvailable );
                                              // number of results
                                              uint16ToArray( respBuffer + index, payload.data.scanCmdResponse.numOfResults );
                                              break;

        case M_BLE_PROT_CMD_CODE_GET_NEXT_WIFI_SCAN_RESULT:   responseSize = index + 
                                                              member_size( mBleProtocolWifiScanGetResultPayload_t, resultNum ) +
                                                              member_size( mBleProtocolWifiScanGetResultPayload_t, resultSecType ) +
                                                              member_size( mBleProtocolWifiScanGetResultPayload_t, rssi ) +
                                                              payload.length;  //payload length is the SSID string length
                                              
                                              if( responseSize > respBufferMaxLen ){
                                                return M_BLE_PROT_FUNERR_BUF_OVERFLOW; 
                                              }
                                              // result number
                                              uint16ToArray( respBuffer + index, payload.data.scanResult.resultNum );
                                              index += member_size( mBleProtocolWifiScanGetResultPayload_t, resultNum );
                                              // security Type
                                              respBuffer[index] = payload.data.scanResult.resultSecType;
                                              index += member_size( mBleProtocolWifiScanGetResultPayload_t, resultSecType );
                                              // RSSI
                                              int32ToArray( respBuffer + index, payload.data.scanResult.rssi );
                                              index += member_size( mBleProtocolWifiScanGetResultPayload_t, rssi );
                                              // SSID
                                              memcpy( respBuffer + index, payload.data.scanResult.ssidStrPtr, payload.length );  
                                              break;                              
        
        default: return M_BLE_PROT_FUNERR_INVALID_PARAM;
    }

    *respBufferLen = responseSize;

    return M_BLE_PROT_FUNERR_SUCCESS;
}
