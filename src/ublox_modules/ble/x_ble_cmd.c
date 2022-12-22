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
 * @brief This file contains the implementation for the BLE command handler
 * of Sensor Aggregation firmware for XPLR-IOT-1. Data fed to xBleCmdHandle 
 * function are considered as incoming commands from u-blox mobile application.
 * 
 * Normally incoming data are incoming messages from BLE NUS (Nordic Uart Service).
 * Incoming data are interpreted from the xBleCmdHandle command handler, according
 * to the protocol defined in "mobile_app_ble_protocol.h" and put in a command 
 * execution FIFO. Actual execution is performed by the xBleCmdExecThread thread
 * in this module.
 * 
 * This file also contains the implementation of all command execution functions.
 * These commands usually send a response back via BLE. The actual responses are
 * again defined by the communication protocol.
 * 
 * This module works on top of the communication protocol described in
 * "mobile_app_ble_protocol.h"
 */


#include "x_ble_cmd.h"

//Zephyr-SDK related
#include <zephyr.h>
#include <logging/log.h>
#include <bluetooth/services/nus.h>

//Application related
#include "x_logging.h"
#include "x_system_conf.h"
#include "num_array.h"
#include "x_storage.h"


/* ----------------------------------------------------------------
 * STATIC TYPE DEFINITIONS
 * -------------------------------------------------------------- */

/** Structure used to store commands in command execution FIFO and 
 * generally pass command data from function to function.
*/
typedef struct{
    void *fifo_reserved;                             /**< 1st word reserved for use by FIFO*/
    mBleProtocolCmd_t command;                       /**< The actual command enum*/
    uint8_t cmdPayload[ sizeof(xBleCmdPayloads_t) ]; /**< Command payload data without headers etc*/
    uint16_t cmdPayloadLen;                          /**< Command Payload data length*/
}commandWithPayload_t;


/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION -- GENERAL / HELP FUNCTIONS
 * -------------------------------------------------------------- */

/** All recognized incoming commands parsed by xBleCmdHandle are put
 *  into a FIFO for execution by this thread function. This thread is
 *  executed as long as commands are present in the command execution FIFO.
 */
static void xBleCmdExecThread(void);


/** This function is executed when an unknown command is received.
 * It sends an unknown command response to the command sender.
 *
 * @param cmdItem  Commands which send data to the firmware (in this case the
 *                 incoming command which will be echoed to the sender),
 *                 use this structure to get the payload data of the incoming
 *                 command.
 * @return         zero on success else negative error code.
 */
static err_code xBleCmdUnknownCommand( commandWithPayload_t *cmdItem );


/** Helper function which deletes any thingstream domain that migh
 * have been saved in the XPLR-IOT-1 internal memory.
 *
 * @return        zero on success else negative error code.
 */
static err_code xThingDeleteThingstreamDomain();


/** Helper function which saves a thingstream domain name in the internal
 *  memory of XPLR-IOT-1.
 *
 * @param domainStr  String that contains the thingstream domain name to
 *                   be saved.
 * @return           zero on success else negative error code.
 */
err_code xThingSaveThingstreamDomain( char *domainStr );


/** Helper function which saves the Thingstream Client ID credential of a 
 * Thingstream thing in the internal memory of XPLR-IOT-1.
 *
 * @param clientId  String containing the Client ID.
 * @return          zero on success else negative error code.
 */
err_code xThingSaveThingstreamClientId( char *clientId );


/** Helper function which saves the Thingstream Username credential of a 
 * Thignstream thing in the internal memory of XPLR-IOT-1.
 *
 * @param username  String containing the Username.
 * @return          ero on success else negative error code.
 */
err_code xThingSaveThingstreamUsername( char *username );


/** Helper function which saves the Thingstream Password credential of a 
 * Thignstream thing in the internal memory of XPLR-IOT-1.
 *
 * @param password  String containing the Password.
 * @return          zero on success else negative error code.
 */
err_code xThingSaveThingstreamPassword( char *password );


/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION -- SYSTEM COMMANDS EXEC
 * -------------------------------------------------------------- */

/** Implementation of command which Sends the Firmware Version to the
 *  command sender (normally the u-blox mobile application)
 * 
 * @return        zero on success else negative error code.
 */
static err_code xBleCmdReadFwVersion( void );


/** Implementation of command which informs the Firmware about the Version of 
 * the command sender(normally the version of the u-blox mobile application).
 * This command/info is sent externally, the firmware alone cannot request this,
 * the mobile app should send this by itself.
 * 
 * @param cmdItem  Commands which send data to the firmware (in this case the
 *                 Version of the mobile app), use this structure to get the
 *                 payload of the incoming command.
 * @return         zero on success else negative error code.
 */
static err_code xBleCmdGetMobileAppVersion( commandWithPayload_t *cmdItem );


/** Implementation of command which sends the Communication Protocol Version to
 * the command sender (normally the u-blox mobile application). The protocol 
 * version is defined by the "mobile_app_ble_protocol.h" file. There could be
 * updates to the firmware version, but the protocol could remain the same, that
 * is why the protocol version is treated separately.
 *
 * @return        zero on success else negative error code.
 */
static err_code xBleCmdReadProtocolVersion( void );


/** Implementation of command which sends the XPLR-IOT-1 unique identifier to the
 *  command sender (normally the u-blox mobile application). The unique identifier
 *  is the Device. Id of NORA-B1 module in XPLR-IOT-1 as read from
 *  NRF_FICR->INFO.DEVICEID registers.
 * 
 * @return        zero on success else negative error code.
 */
static err_code xBleCmdReadUniqueID( void );



/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION -- WIFI COMMANDS EXEC
 * -------------------------------------------------------------- */

/** Implementation of Command which clears any WIFI configuration 
 * (SSID, password) provided either by "modules NINAW156 provision"
 * command or by BLE commands such as xBleCmdWriteWiFiSsid, 
 * xBleCmdWriteWiFiPassword etc.
 * 
 * @return        zero on success else negative error code.
 */
static err_code xBleCmdClearWiFiConfig( void );


/** Implementation of Command which sends the WISI SSID saved in the
 *  internal memory of XPLR-IOT-1 (the SSID of the network the XPLR-IOT-1
 *  is configured to connect to) to the command sender (normally the u-blox
 *  mobile application).
 * 
 * @return        zero on success else negative error code.
 */
static err_code xBleCmdReadWiFiSsid( void );


/** Implementation of command with which the sender defines/configures the SSID of the
 *  WIFI network the XPLR-IOT-1 is supposed to connect to (provision). 
 * This is saved in internal XPLR-IOT-1 memory.
 * This command should be followed by the command imeplemented by 
 * xBleCmdWriteWiFiSecType which defines the type of network (password protected
 * or not). 
 * @param cmdItem  Commands which send data to the firmware (in this case the
 *                 WiFi network SSID), use this structure to get the
 *                 payload of the incoming command.
 * @return         zero on success else negative error code.
 */
static err_code xBleCmdWriteWiFiSsid( commandWithPayload_t *cmdItem );


/** Implementation of command with which the sender defines/configures the type
 *  of the WIFI network the XPLR-IOT-1 is supposed to connect to (password protected
 *  or not). 
 *  This is saved in internal XPLR-IOT-1 memory.
 *  If the network type is "protected with password" this command should
 *  be followed by the command imeplemented by xBleCmdWriteWiFiPassword which
 *  configures the password of the network. 
 * 
 * @param cmdItem  Commands which send data to the firmware (in this case the
 *                 type of WiFi network), use this structure to get the
 *                 payload of the incoming command.
 * @return         zero on success else negative error code.
 */
static err_code xBleCmdWriteWiFiSecType( commandWithPayload_t *cmdItem ); 


/** Implementation of command with which the sender defines/configures the password
 *  of the WIFI network the XPLR-IOT-1 is supposed to connect to (provision).
 * This is saved in internal XPLR-IOT-1 memory
 * 
 * @param cmdItem  Commands which send data to the firmware (in this case the
 *                 password of the WiFi network), use this structure to get the
 *                 payload of the incoming command.
 * @return         zero on success else negative error code.
 */
static err_code xBleCmdWriteWiFiPassword( commandWithPayload_t *cmdItem ); 


/** Implementation of command which scans for WiFi networks. It then reports,
 * how many networks were found and if all of the results are available (due to
 * memory restrictions etc. depending on implememntation) 
 *
 * @return        zero on success else negative error code.
 */
static err_code xBleCmdScanWiFi( void );

/** Implementation of command reporting the results of a Scan WiFi command.
 * This should be used only after a Scan WiFi command.
 * The results of Scan WiFi command are saved and each time this command is
 * called, the next result of Scan WiFi command is sent as a response.
 * In order for the caller to obtain x results, this command should be used
 * x times. If all results have been already sent, then it responds with a 
 * NOT_FOUND response.
 *
 * @return        zero on success else negative error code.
 */
static err_code xBleCmdGetNextScanWiFiResult( void );


/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION -- THINGSTREAM CONFIG COMMANDS EXEC
 * -------------------------------------------------------------- */

/** Implementation of Command which clears any Thingstream configuration 
 * (domain names, MQTT & MQTTSN anywhere configuration).
 * 
 * Note: MQTTSN - Flex configurations saved are not affected by this command,
 * because the u-blox mobile application does not config MQTT Flex Things
 * 
 * @return        zero on success else negative error code.
 */
static err_code xBleCmdClearThingstreamConfig();  


/** Implementation of Command which sends the Thingstream Domain saved in the
 *  internal memory of XPLR-IOT-1, to the command sender (normally the u-blox
 *  mobile application).
 * 
 * @return        zero on success else negative error code.
 */
static err_code xBleCmdReadThingstreamDomain();


/** Implementation of Command which sends the Thingstream IP Thing Client ID
 * (MQTT client ID/Device ID) saved in the internal memory of XPLR-IOT-1, to 
 * the command sender (normally the u-blox mobile application).
 * 
 * @return        zero on success else negative error code.
 */
static err_code xBleCmdReadThingstreamIpThingClientId();


/** Implementation of Command which sends the Thingstream MQTT Anywhere Thing
 *  Device ID (MQTTSN Device ID) saved in the internal memory of XPLR-IOT-1,
 *  to the command sender (normally the u-blox mobile application).
 * 
 * @return        zero on success else negative error code.
 */
static err_code xBleCmdReadThingstreamSimThingDevId();


/** Implementation of command with which the sender defines/configures the 
 * Thingstream domain his Thing belongs to (IP or SIM Thing).  
 * This is saved in XPLR-IOT-1 internal memory.
 * In order to complete the configuration of a thing, Client/Device ID will 
 * be needed and in case of IP thing, Username and Password are also needed.
 * 
 * @param cmdItem  Commands which send data to the firmware (in this case the
 *                 Thingstream domain), use this structure to get the
 *                 payload of the incoming command.
 * @return         zero on success else negative error code.
 */
static err_code xBleCmdWriteThingstreamDomain( commandWithPayload_t *cmdItem );


/** Implementation of command with which the sender defines/configures the 
 * IP Thing Client ID (MQTT configuration).  
 * This is saved in XPLR-IOT-1 internal memory.
 * In order to complete the configuration of an IP thing: Client/Device ID,
 * Username and Password are needed.
 * 
 * @param cmdItem  Commands which send data to the firmware (in this case the
 *                 IP Thing Client ID), use this structure to get the
 *                 payload of the incoming command.
 * @return         zero on success else negative error code.
 */
static err_code xBleCmdWriteThingstreamIpThingClientId( commandWithPayload_t *cmdItem );


/** Implementation of command with which the sender defines/configures the 
 * IP Thing Username (MQTT configuration).  
 * This is saved in XPLR-IOT-1 internal memory.
 * In order to complete the configuration of an IP thing: Client/Device ID,
 * Username and Password are needed.
 * 
 * @param cmdItem  Commands which send data to the firmware (in this case the
 *                 IP Thing Username), use this structure to get the
 *                 payload of the incoming command.
 * @return         zero on success else negative error code.
 */
static err_code xBleCmdWriteThingstreamIpThingUsername( commandWithPayload_t *cmdItem );


/** Implementation of command with which the sender defines/configures the 
 * IP Thing Password (MQTT configuration).  
 * This is saved in XPLR-IOT-1 internal memory.
 * In order to complete the configuration of an IP thing: Client/Device ID,
 * Username and Password will be needed.
 * 
 * @param cmdItem  Commands which send data to the firmware (in this case the
 *                 IP Thing Password), use this structure to get the
 *                 payload of the incoming command.
 * @return         zero on success else negative error code.
 */
static err_code xBleCmdWriteThingstreamIpThingPassword( commandWithPayload_t *cmdItem );


/** Implementation of command with which the sender defines/configures the 
 * SIM Thing Device ID ( MQTT Anywhere plan - MQTTSN configuration).  
 * This is saved in XPLR-IOT-1 internal memory.
 * 
 * Note: MQTTSN for MQTT Flex things is not currently supported by BLE commands.
 * 
 * @param cmdItem  Commands which send data to the firmware (in this case the
 *                 SIM Thing Device ID), use this structure to get the
 *                 payload of the incoming command.
 * @return         zero on success else negative error code.
 */
static err_code xBleCmdWriteThingstreamSimThingDevId( commandWithPayload_t *cmdItem );


/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

// Debug Messages Logging Module
LOG_MODULE_REGISTER( LOGMOD_NAME_BLE_CMD, LOG_LEVEL_DBG );

/** BLE Command Execution FIFO. Parsed Commands are pushed in this FIFO
 * and xBleCmdExecThread Thread pops those commands and executes them
*/
K_FIFO_DEFINE( xBleCmdExecFifo );

// Threads definition - command execution thread
K_THREAD_DEFINE(xBleCmdExecThreadId, BLE_CMD_EXEC_STACK_SIZE, xBleCmdExecThread, NULL, NULL, NULL,
		BLE_CMD_EXEC_PRIORITY, 0, 0);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

// These variable are defined as global to avoid some overflow issues
// in xBleCmdHandle function.


/** Buffer holding Incoming Command Payload Data (without headers)*/
uint8_t gpCmdPayloadBuf[ sizeof(xBleCmdPayloads_t) ];


/** Buffer holding command responses to be send*/
uint8_t gpResponseBuf[ X_BLE_CMD_OUT_MAX_SIZE ];


/** Holds scan WiFi command status, because its execution results is
 * shared between two commands:
 * - Scan WiFi networks and
 * - Get (Next) Scan WiFi result 
*/
struct{
    mBleProtocolResponseCode_t responseCode;  /**< The response code of the command*/
    err_code ret;                             /**< Err code returned by WiFI scan function*/
    uint16_t resultsNum;                      /**< Number of Networks found (and can be reported) by WiFi Scan*/
    uint16_t sentResultsCount;                /**< Number of results sent (reported) up to now */
}gScanWiFiStatus = 
    // initialize the global structure
    {
        // at start before any Scan command is sent, 
        // the status is error -> no Scan yet = Invalid State 
        .responseCode = M_CMD_RSP_ERROR,  
        .ret = X_ERR_INVALID_STATE,       
        .resultsNum = 0,
        .sentResultsCount = 0
    };


/* ----------------------------------------------------------------
 * PUBLIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


void xBleCmdHandle(uint8_t *pCmdData, uint16_t cmdDataSize){

    mBleProtocolCmd_t command;
    uint16_t cmdPayloadLen;

    // Parse Incoming Data as command
    mBleProtocolParseCmd( pCmdData, cmdDataSize, &command, gpCmdPayloadBuf, 
                           sizeof( gpCmdPayloadBuf ), &cmdPayloadLen);

    // Type the command received
    uint8_t commandStr[ M_BLE_PROT_COMMAND_STRING_MAX_SIZE ];
    if( mBleProtocolGetCommandString( command, commandStr, sizeof(commandStr) ) == M_BLE_PROT_FUNERR_SUCCESS ){
        LOG_INF("Received BLE Command: %s", commandStr );
    }

    // Prepare the command item and push it to BLE command execution FIFO
    commandWithPayload_t commandItem;
    commandItem.command = command;
    commandItem.cmdPayloadLen = cmdPayloadLen;
    if( cmdPayloadLen > 0 ){
        memcpy( commandItem.cmdPayload, gpCmdPayloadBuf, cmdPayloadLen );
    }
    k_fifo_put(&xBleCmdExecFifo, &commandItem);
    
    return;
}


/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION -- GENERAL / HELP FUNCTIONS
 * -------------------------------------------------------------- */


static void xBleCmdExecThread(void){

    commandWithPayload_t  *cmdItem;

    while (1) {

        // Thread is executed only when commands are present in the
        // BLE Command Execution FIFO
        cmdItem = k_fifo_get(&xBleCmdExecFifo, K_FOREVER);

        // Type which command is going to be executed
        uint8_t commandStr[ M_BLE_PROT_COMMAND_STRING_MAX_SIZE ];
        if( mBleProtocolGetCommandString( cmdItem->command, commandStr, sizeof(commandStr) ) == M_BLE_PROT_FUNERR_SUCCESS ){
             LOG_INF("Executing BLE Command: %s \r\n", commandStr );
        }

        // Command Execution, based on command enums, defined by "mobile_app_protocol.h"
        switch( cmdItem->command ){

            case READ_FW_VERSION: xBleCmdReadFwVersion();
            break;

            case GET_MOBILE_APP_VERSION: xBleCmdGetMobileAppVersion( cmdItem );
            break;
            
            case READ_PROTOCOL_VERSION: xBleCmdReadProtocolVersion();
            break;

            case READ_UNIQUE_ID: xBleCmdReadUniqueID();
            break; 

            case CLEAR_WIFI_CONFIG: xBleCmdClearWiFiConfig();
            break;

            case READ_WIFI_SSID:    xBleCmdReadWiFiSsid();
            break;

            case WRITE_WIFI_SSID:   xBleCmdWriteWiFiSsid( cmdItem );
            break;

            case WRITE_WIFI_TYPE:   xBleCmdWriteWiFiSecType( cmdItem );
            break;

            case WRITE_WIFI_PSW:    xBleCmdWriteWiFiPassword( cmdItem );
            break;    

            case SCAN_WIFI:         xBleCmdScanWiFi();
            break;    

            case GET_NEXT_WIFI_SCAN_RESULT: xBleCmdGetNextScanWiFiResult();
            break;

            case CLEAR_THINGSTREAM_CONFIG:  xBleCmdClearThingstreamConfig();
            break;

            case READ_THINGSTREAM_DOMAIN:   xBleCmdReadThingstreamDomain();
            break; 

            case READ_IP_THING_CLIENT_ID:   xBleCmdReadThingstreamIpThingClientId();
            break; 

            case READ_SIM_THING_DEVICE_ID:  xBleCmdReadThingstreamSimThingDevId();
            break; 

            case WRITE_THINGSTREAM_DOMAIN:  xBleCmdWriteThingstreamDomain( cmdItem );
            break;

            case WRITE_IP_THING_CLIENT_ID:  xBleCmdWriteThingstreamIpThingClientId( cmdItem );
            break; 

            case WRITE_IP_THING_USERNAME:   xBleCmdWriteThingstreamIpThingUsername( cmdItem );
            break; 
            
            case WRITE_IP_THING_PSW:        xBleCmdWriteThingstreamIpThingPassword( cmdItem );
            break; 
            
            case WRITE_SIM_THING_DEVICE_ID: xBleCmdWriteThingstreamSimThingDevId( cmdItem );
            break; 
                                           
            case UNKNOWN_COMMAND: xBleCmdUnknownCommand( cmdItem );
                                  break;

            default: break;
        }  // end of command execution switch
    } // end of while
}



static err_code xBleCmdUnknownCommand( commandWithPayload_t *cmdItem ){

    if( cmdItem->command != UNKNOWN_COMMAND ){
        return X_ERR_INVALID_PARAMETER;
    }

    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;
    mBleProtocolPayload_t payload;
    payload.length  = cmdItem->cmdPayloadLen;
    payload.data.byteBuffer = cmdItem->cmdPayload;

    mBleProtocolPrepareResponse( cmdItem->command, M_CMD_RSP_OK, payload, gpResponseBuf, 
                                sizeof(gpResponseBuf), &responseLen );

    
    return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);
}



static err_code xThingDeleteThingstreamDomain(){
    
    err_code rc;
    rc = xStorageDeleteFile( thingstream_domain_fname ); 		
    return rc;    
}



err_code xThingSaveThingstreamDomain( char *domainStr ){
    
    uint32_t strLen = strlen(domainStr);
    err_code ret;

    // Check Domain string for appropriate length
    if( strLen > THINGSTREAM_DOMAIN_MAX_LEN ){
        
        LOG_WRN("Domain provided too long. Max accepted is %d chars \r\n",
                 THINGSTREAM_DOMAIN_MAX_LEN);
        return X_ERR_INVALID_PARAMETER;
    }

    if( strLen < THINGSTREAM_DOMAIN_MIN_LEN ){
 
        LOG_WRN("Domain provided too short. Min accepted is %d chars \r\n",
                 THINGSTREAM_DOMAIN_MIN_LEN);
        return X_ERR_INVALID_PARAMETER;
    }

    // Save Domain String
    if( ( ret = xStorageSaveFile( domainStr, thingstream_domain_fname, strLen +1 ) ) < 0 ){
		return ret;
	}

    return X_ERR_SUCCESS;
}



err_code xThingSaveThingstreamClientId( char *clientId ){
    
    uint32_t strLen = strlen(clientId);
    err_code ret;

    // check client ID string for appropriate length
    if( strLen > MQTT_DEVICE_ID_MAXLEN ){
        
        LOG_WRN("Client Id provided too long. Max accepted is %d chars \r\n",
                 MQTT_DEVICE_ID_MAXLEN);
        return X_ERR_INVALID_PARAMETER;
    }

    // min one char
    if( strLen < 1 ){
 
        LOG_WRN("Client Id provided too short. Min accepted is %d chars \r\n",
                 1);
        return X_ERR_INVALID_PARAMETER;
    }

    // Save Client ID
    if( ( ret = xStorageSaveFile( clientId, mqtt_deviceID_fname, strLen +1 ) ) < 0 ){
		return ret;
	}

    return X_ERR_SUCCESS;
}



err_code xThingSaveThingstreamUsername( char *username ){

    
    uint32_t strLen = strlen(username);
    err_code ret;

    // Check usename string for appropriate length
    if( strLen > MQTT_USERNAME_MAXLEN ){
        
        LOG_WRN("Username provided too long. Max accepted is %d chars \r\n",
                 MQTT_USERNAME_MAXLEN);
        return X_ERR_INVALID_PARAMETER;
    }

    // min one char
    if( strLen < 1 ){
 
        LOG_WRN("Username provided too short. Min accepted is %d chars \r\n",
                 1);
        return X_ERR_INVALID_PARAMETER;
    }

    // Save username String
    if( ( ret = xStorageSaveFile( username, mqtt_username_fname, strLen +1 ) ) < 0 ){
		return ret;
	}

    return X_ERR_SUCCESS;

}



err_code xThingSaveThingstreamPassword( char *password ){
    
    uint32_t strLen = strlen(password);
    err_code ret;

    // Check password string for appropriate length
    if( strLen > MQTT_PASSWORD_MAXLEN ){
        
        LOG_WRN("Password provided too long. Max accepted is %d chars \r\n",
                 MQTT_PASSWORD_MAXLEN);
        return X_ERR_INVALID_PARAMETER;
    }

    // min one char
    if( strLen < 1 ){
 
        LOG_WRN("Password provided too short. Min accepted is %d chars \r\n",
                 1);
        return X_ERR_INVALID_PARAMETER;
    }

    // Save password String
    if( ( ret = xStorageSaveFile( password, mqtt_psw_fname, strLen +1 ) ) < 0 ){
		return ret;
	}

    return X_ERR_SUCCESS;
}


/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION -- SYSTEM COMMANDS EXEC
 * -------------------------------------------------------------- */


static err_code xBleCmdReadFwVersion( void ){

    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;

    // Prepare the payload of the response
    mBleProtocolPayload_t payload;
    payload.length = 2;
    payload.data.fw_version[0] =  FIRMWARE_VERSION_MAJOR;
    payload.data.fw_version[1] =  FIRMWARE_VERSION_MINOR;

    // Get the actual response from the protocol module
    int32_t ret = mBleProtocolPrepareResponse( READ_FW_VERSION, M_CMD_RSP_OK, payload, gpResponseBuf,
                                              sizeof(gpResponseBuf), &responseLen );
    
    // Send the response if all is ok, else return error code
    if( ret == 0 ){
        return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);
    }
    else{
        return (err_code) ret;
    }
}



static err_code xBleCmdGetMobileAppVersion( commandWithPayload_t *cmdItem ){    

    if( cmdItem->command != GET_MOBILE_APP_VERSION ){
        return X_ERR_INVALID_PARAMETER;
    }
    
    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;
    mBleProtocolPayload_t responsePayload;
    mBleProtocolResponseCode_t responseCode;

    // Is the Payload received from the command ok?
    if( cmdItem->cmdPayloadLen != M_BLE_PROT_MOBILE_APP_VERSIONING_NUMS ){
        
        LOG_WRN("Mobile App Version Payload is wrong. %d-number versioning is accepted only \r\n",
                 M_BLE_PROT_MOBILE_APP_VERSIONING_NUMS);

        responseCode = M_CMD_RSP_ERROR;
        responsePayload.length = 1;  
        responsePayload.data.errorCode = X_ERR_INVALID_PARAMETER;
    }
    
    // if payload of incoming command is ok
    else{
        responseCode = M_CMD_RSP_OK;
        // no payload is needed in the response
        responsePayload.length  = 0;
        LOG_WRN("Mobile App Version Received: %d.%d \r\n",
                 cmdItem->cmdPayload[0],cmdItem->cmdPayload[1]);
    }

    // Prepare an ok, or error response, according to protocol
    mBleProtocolPrepareResponse( cmdItem->command, responseCode, responsePayload, gpResponseBuf, 
                                sizeof(gpResponseBuf), &responseLen );

    // send the response
    return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);
}



static err_code xBleCmdReadProtocolVersion(void){
    
    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;

    // Populate the response payload with the requested information
    mBleProtocolPayload_t payload;
    payload.length = 2;
    payload.data.fw_version[0] =  M_BLE_PROT_VERSION_MAJOR;
    payload.data.fw_version[1] =  M_BLE_PROT_VERSION_MINOR;

    // Prepare the actual response according to protocol
    int32_t ret = mBleProtocolPrepareResponse( READ_PROTOCOL_VERSION, M_CMD_RSP_OK, payload, gpResponseBuf,
                                              sizeof(gpResponseBuf), &responseLen );
    
    // Send the response
    if( ret == 0 ){
        return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);;
    }
    else{
        return (err_code) ret;
    }
}



static err_code xBleCmdReadUniqueID( void ){
 
    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;

    mBleProtocolPayload_t payload;
    payload.length = 8; //64-bit identifier
    uint8_t byteBuf[8];

    // Unique ID is the NORA-B1 device ID registers
    uint32_t uniqueID_MSB = NRF_FICR->INFO.DEVICEID[1];
    uint32_t uniqueID_LSB = NRF_FICR->INFO.DEVICEID[0];
    uint64_t uniqueID;

    // Type the registers
    LOG_DBG("DEVICEID1: %08X\n", NRF_FICR->INFO.DEVICEID[1]);
    LOG_DBG("DEVICEID0: %08X\n", NRF_FICR->INFO.DEVICEID[0]);

    // prepare unique ID byte buffer (MSB register followed by LSB register)
    uint32ToArray( byteBuf, uniqueID_MSB );
    uint32ToArray( byteBuf + sizeof( uint32_t ), uniqueID_LSB );
    uniqueID = arrayToUint64( byteBuf );

    // populate the response payload with the unique id
    payload.data.uniqueDeviceID = uniqueID;

    // get the actual response according to protocol
    int32_t ret = mBleProtocolPrepareResponse( READ_UNIQUE_ID, M_CMD_RSP_OK, payload, gpResponseBuf,
                                              sizeof(gpResponseBuf), &responseLen );
    
    // send response
    if( ret == 0 ){
        return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);;
    }
    else{
        return (err_code) ret;
    }   
}


/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION -- WIFI COMMANDS EXEC
 * -------------------------------------------------------------- */


static err_code xBleCmdClearWiFiConfig( void ){

    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;
    mBleProtocolPayload_t payload;
    mBleProtocolResponseCode_t responseCode = M_CMD_RSP_OK;
    
    // no payload in the response is required if operation is successful
    payload.length=0;

    // execute command
    err_code ret = xWifiNinaDeleteNetworkConfig();
    
    // check errors returned and update response code and payload if necessary
    if( ret < 0 ){
        if( ret == ERR_STORAGE_FILE_NOT_FOUND ){
            responseCode = M_CMD_RSP_NOT_FOUND;
        }
        else{
            // if error is returned, payload contains the
            // error number
            responseCode = M_CMD_RSP_ERROR;
            payload.length=1;
            payload.data.errorCode = ret;
        }
    }

    //prepare response format
    int32_t err = mBleProtocolPrepareResponse( CLEAR_WIFI_CONFIG, responseCode, payload, gpResponseBuf,
                                              sizeof(gpResponseBuf), &responseLen );
    
    // send response 
    if( err == 0 ){
        return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);
    }
    else{
        return (err_code) err;
    }   
}



static err_code xBleCmdReadWiFiSsid(){

    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;
    mBleProtocolResponseCode_t responseCode;
    mBleProtocolPayload_t responsePayload;

    char ssid[ WIFI_MAX_SSID_LEN ];
    char passphrase[ WIFI_MAX_PSW_LEN ];
    int32_t sec_type;

    err_code err = xStorageReadWifiCred(ssid, WIFI_MAX_SSID_LEN, passphrase, WIFI_MAX_PSW_LEN, &sec_type);

    // check errors returned and update response code and payload if necessary
    if( err < 0 ){
        if( err == ERR_STORAGE_FILE_NOT_FOUND ){
            responseCode = M_CMD_RSP_NOT_FOUND;
        }
        else{
            // if error is returned, payload contains the
            // error number
            responseCode = M_CMD_RSP_ERROR;
            responsePayload.length=1;
            responsePayload.data.errorCode = err;
        }
    }
    // if no errors while reading WiFi credentials from memory
    else{
        responseCode = M_CMD_RSP_OK;
        responsePayload.data.stringPtr = ssid;
        responsePayload.length = strlen(ssid);
    }

    int32_t ret = mBleProtocolPrepareResponse( READ_WIFI_SSID, responseCode, responsePayload, gpResponseBuf,
                                              sizeof(gpResponseBuf), &responseLen );


    if( ret == 0 ){
        return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);
    }
    else{
        return (err_code) ret;
    }
}



static err_code xBleCmdWriteWiFiSsid( commandWithPayload_t *cmdItem ){

    if( cmdItem->command != WRITE_WIFI_SSID ){
        return X_ERR_INVALID_PARAMETER;
    }

    err_code ret;
    
    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;
    mBleProtocolPayload_t responsePayload;
    mBleProtocolResponseCode_t responseCode;

    // add string termination to data received
    cmdItem->cmdPayload[cmdItem->cmdPayloadLen]='\0';
    
    // type received string
    LOG_INF( "SSID Received: %s \r\n", cmdItem->cmdPayload );

    // save received string
    if( ( ret = ( xWifiNinaSaveSSID( (char*) cmdItem->cmdPayload ) ) ) < 0 ){
        // if error while storing
        responseCode = M_CMD_RSP_ERROR;  
        responsePayload.data.errorCode = (int32_t) ret;
	}
    else{
        //if storing proccess was ok
        responseCode = M_CMD_RSP_OK;
        responsePayload.length = 0;
    }

    // prepare an ok or an error response
    mBleProtocolPrepareResponse( cmdItem->command, responseCode, responsePayload, gpResponseBuf, 
                                sizeof(gpResponseBuf), &responseLen );

    // send the response
    return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);
} 



static err_code xBleCmdWriteWiFiSecType( commandWithPayload_t *cmdItem ){
    
    if( cmdItem->command != WRITE_WIFI_TYPE ){
        return X_ERR_INVALID_PARAMETER;
    }

    err_code ret;
    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;
    mBleProtocolPayload_t responsePayload;
    mBleProtocolResponseCode_t responseCode;


    if( cmdItem->cmdPayloadLen > 1 ){
        
        LOG_WRN("Security Type Parameter Provided is too long. Should be one byte \r\n");

        responseCode = M_CMD_RSP_ERROR;
        responsePayload.length = 1;  
        responsePayload.data.errorCode = X_ERR_INVALID_PARAMETER;
    }

    // save received string
    int32_t sec = (int32_t) cmdItem->cmdPayload[0];
    if( ( ret = ( xWifiNinaSaveSecType( sec ) ) ) < 0 ){
        // if error while storing
        responseCode = M_CMD_RSP_ERROR;  
        responsePayload.data.errorCode = (int32_t) ret;
	}
    else{
        //if storing proccess was ok
        responseCode = M_CMD_RSP_OK;
        responsePayload.length = 0;
    }

    mBleProtocolPrepareResponse( cmdItem->command, responseCode, responsePayload, gpResponseBuf, 
                                sizeof(gpResponseBuf), &responseLen );

    return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);
} 



static err_code xBleCmdWriteWiFiPassword( commandWithPayload_t *cmdItem ){
   
    if( cmdItem->command != WRITE_WIFI_PSW ){
        return X_ERR_INVALID_PARAMETER;
    }

    err_code ret;
    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;
    mBleProtocolPayload_t responsePayload;
    mBleProtocolResponseCode_t responseCode;

    // add string termination to data received
    cmdItem->cmdPayload[cmdItem->cmdPayloadLen]='\0';
    
    // type received string
    LOG_INF( "Password Received: %s \r\n", cmdItem->cmdPayload );

    // save received string
    if( ( ret = ( xWifiNinaSavePassword( (char*) cmdItem->cmdPayload ) ) ) < 0 ){
        // if error while storing
        responseCode = M_CMD_RSP_ERROR;  
        responsePayload.data.errorCode = (int32_t) ret;
	}
    else{
        //if storing proccess was ok
        responseCode = M_CMD_RSP_OK;
        responsePayload.length = 0;
    }

    mBleProtocolPrepareResponse( cmdItem->command, responseCode, responsePayload, gpResponseBuf, 
                                sizeof(gpResponseBuf), &responseLen );

    return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);
} 


static err_code xBleCmdScanWiFi( void ){

    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;
    mBleProtocolPayload_t payload;
    
    // Was NINA initialized within this command execution?
    bool ninaInitLocally = false;
    // the power state of NINA module, upon entering the scan command execution
    bool ninaOriginalPowerStateOn;

    // return code from prepare response and send functions 
    err_code send_ret;

    // normally the NINA module should be initialized with the init shell command
    // before trying to scan for networks.
    // For better user experience (the user might not have a pc available for connection
    // with a UART) this BLE command also initializes the WIFI module.
    // If not connected to WiFi network, try to connect before opening client
    xWifiNinaStatus_t nina_status = xWifiNinaGetModuleStatus();
    err_code ninaLastOperationResult = X_ERR_SUCCESS;

    ninaOriginalPowerStateOn = nina_status.isPowered;
    if( nina_status.uStatus < uDeviceOpened ){
        LOG_WRN("WiFi device closed, trying to open now\r\n");
        ninaInitLocally = true;
        xWifiNinaInit();
        // wait until initialized or error while trying to initialize happens
        while( ( nina_status.uStatus < uDeviceOpened ) && ( ninaLastOperationResult == X_ERR_SUCCESS ) ){
                k_sleep(K_MSEC(1000));
                nina_status = xWifiNinaGetModuleStatus();
                ninaLastOperationResult = xWifiNinaGetLastOperationResult();
        }
        //if error while initializing NINA happens
        if( ninaLastOperationResult != X_ERR_SUCCESS ){
            LOG_ERR("Error Code from WiFi Init Request: %d", ninaLastOperationResult);
            // we continue executing instead of error so that the scan error is reported via a BLE response
        }
    }
    // -- end of NINA  module initialization code
    
    
    // --  execute Scan command -- 
    
    // results strored in global structure, 
    // xBleCmdGetNextScanWiFiResult needs to access them.
    LOG_INF("Scan Requested\r\n");
    gScanWiFiStatus.ret = xWifiNinaScan( &gScanWiFiStatus.resultsNum );
    LOG_INF("Scan Complete\r\n");

    // reset the counter. Each time this command is issued should start from 0.
    gScanWiFiStatus.sentResultsCount = 0;

    // check errors returned and send error response if necessary
    if( gScanWiFiStatus.ret < 0 ){
        LOG_ERR("Error while scanning \r\n");
        // need to save this in a global. 
        // xBleCmdGetNextScanWiFiResult needs it too.
        gScanWiFiStatus.responseCode = M_CMD_RSP_ERROR;
        // if error is returned, payload contains the
        // error number
        payload.data.errorCode = gScanWiFiStatus.ret;
    }
    // no networks found
    else if( gScanWiFiStatus.resultsNum == 0 ){
        LOG_WRN("No networks found \r\n");
        gScanWiFiStatus.responseCode = M_CMD_RSP_NOT_FOUND;
    }
    else{
        LOG_WRN("Found (%d) networks \r\n", gScanWiFiStatus.resultsNum );
        gScanWiFiStatus.responseCode = M_CMD_RSP_OK;
        payload.data.scanCmdResponse.allResultAvailable = !xWifiNinaIsScanMaxReached();
        payload.data.scanCmdResponse.numOfResults = gScanWiFiStatus.resultsNum;
        // indicate there is payload
        payload.length = 1;
    }

    // Prepare the command response according to protocol
    send_ret = mBleProtocolPrepareResponse( SCAN_WIFI, gScanWiFiStatus.responseCode, payload, gpResponseBuf,
                                                  sizeof(gpResponseBuf), &responseLen );

    // send Scan command response 
    if( send_ret == 0 ){
        send_ret = bt_nus_send( NULL, gpResponseBuf, responseLen);
    }

    // Was NINA initialized just to execute this command? Return module to previous state
    if( ninaInitLocally ){
        //if NINA was not powered before scan command
        if( ! ninaOriginalPowerStateOn ){
            //shut it down again
            xWifiNinaPowerOff();
        }
        else{
            //just deinitialize it again
            xWifiNinaDeinit();
        }
    }

    // return last return code 
    return send_ret;
}



static err_code xBleCmdGetNextScanWiFiResult( void ){

    err_code ret;
    uWifiScanResult_t result;
    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;
    mBleProtocolPayload_t payload;

    payload.data.errorCode = gScanWiFiStatus.ret;
    
    // If Scan WiFi command previously executed was ok
    if( gScanWiFiStatus.responseCode == M_CMD_RSP_OK ){
        // Read one result (+1 is for reporting because in reports counting
        // starts from one, not zero)
        ret = xWifiNinaGetScanResult( gScanWiFiStatus.sentResultsCount+1, &result );
        if( ret == X_ERR_SUCCESS ){  
            payload.length = strlen( result.ssid );
            payload.data.scanResult.resultNum = gScanWiFiStatus.sentResultsCount+1;
            payload.data.scanResult.resultSecType = (uint8_t)result.authSuiteBitmask;
            payload.data.scanResult.ssidStrPtr = result.ssid;
            payload.data.scanResult.rssi = result.rssi;
            LOG_DBG("Sending Scan result (%d)", gScanWiFiStatus.sentResultsCount+1);
        }
        // all results have been read
        else{
            LOG_WRN("All results have been sent, sending NOT FOUND response" );
            gScanWiFiStatus.responseCode = M_CMD_RSP_NOT_FOUND;
        }
    }
    
    ret = mBleProtocolPrepareResponse( GET_NEXT_WIFI_SCAN_RESULT, gScanWiFiStatus.responseCode, payload, gpResponseBuf,
                                              sizeof(gpResponseBuf), &responseLen );
    
    // send result response 
    if( ret == 0 ){
        ret = bt_nus_send( NULL, gpResponseBuf, responseLen);
    }

    if(ret!=0){
        LOG_WRN("Scan Result could not be sent");
        return ret;
    }
    else{
        //if result has been send succesfully, update counter
        gScanWiFiStatus.sentResultsCount++;
    }
    
    return ret;
}


/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION -- THINGSTREAM CONFIG COMMANDS EXEC
 * -------------------------------------------------------------- */


static err_code xBleCmdClearThingstreamConfig(){

    err_code rc;
    uint8_t notFoundAdder = 0;
    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;
    mBleProtocolPayload_t payload;
    mBleProtocolResponseCode_t responseCode = M_CMD_RSP_OK;
    
    // no payload in the response is required if operation is successful
    payload.length=0;

    // execute command, and check only for the not found error

    //delete domain
    rc = xThingDeleteThingstreamDomain();
    if( rc == ERR_STORAGE_FILE_NOT_FOUND ){
        notFoundAdder++;
    }

    //delete IP thing
    rc = xWifiMqttDeleteConfig();
    if( rc == ERR_STORAGE_FILE_NOT_FOUND ){
        notFoundAdder++;
    }

    //delete SIM thing
    rc = xCellMqttSnDeleteAnywhereConfig();
    if( rc == ERR_STORAGE_FILE_NOT_FOUND ){
        notFoundAdder++;
    }

    // did not find a proper configuration for both SIM and IP thing nor a Domain saved
    // respond with a NOT FOUND
    if( notFoundAdder == 3 ){
        responseCode = M_CMD_RSP_NOT_FOUND;
    }

    // other errors are not checked in this function in the response

    //prepare response format
    int32_t err = mBleProtocolPrepareResponse( CLEAR_THINGSTREAM_CONFIG, responseCode, payload, gpResponseBuf,
                                              sizeof(gpResponseBuf), &responseLen );
    
    // send response 
    if( err == 0 ){
        return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);
    }
    else{
        return (err_code) err;
    }  
}



static err_code xBleCmdWriteThingstreamDomain( commandWithPayload_t *cmdItem ){

    if( cmdItem->command != WRITE_THINGSTREAM_DOMAIN ){
        return X_ERR_INVALID_PARAMETER;
    }

    err_code ret;
    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;
    mBleProtocolPayload_t responsePayload;
    mBleProtocolResponseCode_t responseCode;

    // add string termination to data received
    cmdItem->cmdPayload[cmdItem->cmdPayloadLen]='\0';
    
    // type received string
    LOG_INF( "Thingstream Domain Received: %s \r\n", cmdItem->cmdPayload );

    // save received string
    if( ( ret = ( xThingSaveThingstreamDomain( (char*) cmdItem->cmdPayload ) ) ) < 0 ){
        // if error while storing
        responseCode = M_CMD_RSP_ERROR;  
        responsePayload.data.errorCode = (int32_t) ret;
	}
    else{
        //if storing proccess was ok
        responseCode = M_CMD_RSP_OK;
        responsePayload.length = 0;
    }

    mBleProtocolPrepareResponse( cmdItem->command, responseCode, responsePayload, gpResponseBuf, 
                                sizeof(gpResponseBuf), &responseLen );

    return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);
}



static err_code xBleCmdReadThingstreamDomain(){

    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;
    mBleProtocolResponseCode_t responseCode;
    mBleProtocolPayload_t responsePayload;

    char domain[ THINGSTREAM_DOMAIN_MAX_LEN ];
    
    //err_code err = xStorageReadWifiCred(ssid, WIFI_MAX_SSID_LEN, passphrase, WIFI_MAX_PSW_LEN, &sec_type);
    err_code err = xStorageReadFile( domain, thingstream_domain_fname, THINGSTREAM_DOMAIN_MAX_LEN );

    // check errors returned and update response code and payload if necessary
    if( err < 0 ){
        if( err == ERR_STORAGE_FILE_NOT_FOUND ){
            responseCode = M_CMD_RSP_NOT_FOUND;
        }
        else{
            // if error is returned, payload contains the
            // error number
            responseCode = M_CMD_RSP_ERROR;
            responsePayload.length=1;
            responsePayload.data.errorCode = err;
        }
    }
    // if no errors while reading WiFi credentials from memory
    else{
        responseCode = M_CMD_RSP_OK;
        responsePayload.data.stringPtr = domain;
        responsePayload.length = strlen( domain );
        LOG_INF("Domain Read from memory: %s", domain );
    }

    // Get actual response according to protocol
    int32_t ret = mBleProtocolPrepareResponse( READ_THINGSTREAM_DOMAIN, responseCode, responsePayload, gpResponseBuf,
                                              sizeof(gpResponseBuf), &responseLen );

    // send response
    if( ret == 0 ){
        return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);
    }
    else{
        return (err_code) ret;
    }
}



static err_code xBleCmdReadThingstreamIpThingClientId( void ){

    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;
    mBleProtocolResponseCode_t responseCode;
    mBleProtocolPayload_t responsePayload;

    char deviceId[ MQTT_DEVICE_ID_MAXLEN ];
    char password[ MQTT_PASSWORD_MAXLEN ];
    char username[ MQTT_USERNAME_MAXLEN ];

    err_code err = xStorageReadMqttConfig( deviceId, MQTT_DEVICE_ID_MAXLEN, 
							username, MQTT_USERNAME_MAXLEN,
							password, MQTT_PASSWORD_MAXLEN);

    // check errors returned and update response code and payload if necessary
    if( err < 0 ){
        if( err == ERR_STORAGE_FILE_NOT_FOUND ){
            responseCode = M_CMD_RSP_NOT_FOUND;
        }
        else{
            // if error is returned, payload contains the
            // error number
            responseCode = M_CMD_RSP_ERROR;
            responsePayload.length=1;
            responsePayload.data.errorCode = err;
        }
    }
    // if no errors while reading WiFi credentials from memory
    else{
        responseCode = M_CMD_RSP_OK;
        responsePayload.data.stringPtr = deviceId;
        responsePayload.length = strlen(deviceId);
    }

    // get actual response bytes according to protocol
    int32_t ret = mBleProtocolPrepareResponse( READ_IP_THING_CLIENT_ID, responseCode, responsePayload, gpResponseBuf,
                                              sizeof(gpResponseBuf), &responseLen );

    // send the response
    if( ret == 0 ){
        return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);
    }
    else{
        return (err_code) ret;
    }

}



static err_code xBleCmdReadThingstreamSimThingDevId( void ){

    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;
    mBleProtocolResponseCode_t responseCode;
    mBleProtocolPayload_t responsePayload;

    xCellMqttSnConfig_t mqttsnConnection;

    err_code err = xStorageReadMqttSnConfig( &mqttsnConnection, ANYWHERE);

    // check errors returned and update response code and payload if necessary
    if( err < 0 ){
        if( err == ERR_STORAGE_FILE_NOT_FOUND ){
            responseCode = M_CMD_RSP_NOT_FOUND;
        }
        else{
            // if error is returned, payload contains the
            // error number
            responseCode = M_CMD_RSP_ERROR;
            responsePayload.length=1;
            responsePayload.data.errorCode = err;
        }
    }
    // if no errors while reading WiFi credentials from memory
    else{
        responseCode = M_CMD_RSP_OK;
        responsePayload.data.stringPtr = mqttsnConnection.clientID;
        responsePayload.length = strlen( mqttsnConnection.clientID );
    }

    int32_t ret = mBleProtocolPrepareResponse( READ_SIM_THING_DEVICE_ID, responseCode, responsePayload, gpResponseBuf,
                                              sizeof(gpResponseBuf), &responseLen );


    if( ret == 0 ){
        return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);
    }
    else{
        return (err_code) ret;
    }
}



static err_code xBleCmdWriteThingstreamIpThingClientId( commandWithPayload_t *cmdItem ){

    if( cmdItem->command != WRITE_IP_THING_CLIENT_ID ){
        return X_ERR_INVALID_PARAMETER;
    }

    err_code ret;
    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;
    mBleProtocolPayload_t responsePayload;
    mBleProtocolResponseCode_t responseCode;

    // add string termination to data received
    cmdItem->cmdPayload[cmdItem->cmdPayloadLen]='\0';
    
    // type received string
    LOG_INF( "IP Thing Client Id Received: %s \r\n", cmdItem->cmdPayload );

    // save received string
    if( ( ret = ( xThingSaveThingstreamClientId( (char*) cmdItem->cmdPayload ) ) ) < 0 ){
        // if error while storing
        responseCode = M_CMD_RSP_ERROR;  
        responsePayload.data.errorCode = (int32_t) ret;
	}
    else{
        //if storing proccess was ok
        responseCode = M_CMD_RSP_OK;
        responsePayload.length = 0;
    }

    mBleProtocolPrepareResponse( cmdItem->command, responseCode, responsePayload, gpResponseBuf, 
                                sizeof(gpResponseBuf), &responseLen );

    return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);

}



static err_code xBleCmdWriteThingstreamIpThingUsername( commandWithPayload_t *cmdItem ){
   
    if( cmdItem->command != WRITE_IP_THING_USERNAME ){
        return X_ERR_INVALID_PARAMETER;
    }

    err_code ret;
    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;
    mBleProtocolPayload_t responsePayload;
    mBleProtocolResponseCode_t responseCode;

    // add string termination to data received
    cmdItem->cmdPayload[cmdItem->cmdPayloadLen]='\0';
    
    // type received string
    LOG_INF( "IP Thing Username Received: %s \r\n", cmdItem->cmdPayload );

    // save received string
    if( ( ret = ( xThingSaveThingstreamUsername( (char*) cmdItem->cmdPayload ) ) ) < 0 ){
        // if error while storing
        responseCode = M_CMD_RSP_ERROR;  
        responsePayload.data.errorCode = (int32_t) ret;
	}
    else{
        //if storing proccess was ok
        responseCode = M_CMD_RSP_OK;
        responsePayload.length = 0;
    }

    mBleProtocolPrepareResponse( cmdItem->command, responseCode, responsePayload, gpResponseBuf, 
                                sizeof(gpResponseBuf), &responseLen );

    return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);

}



static err_code xBleCmdWriteThingstreamIpThingPassword( commandWithPayload_t *cmdItem ){

    if( cmdItem->command != WRITE_IP_THING_PSW ){
        return X_ERR_INVALID_PARAMETER;
    }

    err_code ret;
    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;
    mBleProtocolPayload_t responsePayload;
    mBleProtocolResponseCode_t responseCode;

    // add string termination to data received
    cmdItem->cmdPayload[cmdItem->cmdPayloadLen]='\0';
    
    // type received string
    LOG_INF( "IP Thing password Received: %s \r\n", cmdItem->cmdPayload );

    // save received string
    if( ( ret = ( xThingSaveThingstreamPassword( (char*) cmdItem->cmdPayload ) ) ) < 0 ){
        // if error while storing
        responseCode = M_CMD_RSP_ERROR;  
        responsePayload.data.errorCode = (int32_t) ret;
	}
    else{
        //if storing proccess was ok
        responseCode = M_CMD_RSP_OK;
        responsePayload.length = 0;
    }

    mBleProtocolPrepareResponse( cmdItem->command, responseCode, responsePayload, gpResponseBuf, 
                                sizeof(gpResponseBuf), &responseLen );

    return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);

}



static err_code xBleCmdWriteThingstreamSimThingDevId( commandWithPayload_t *cmdItem ){

    if( cmdItem->command != WRITE_SIM_THING_DEVICE_ID ){
        return X_ERR_INVALID_PARAMETER;
    }

    err_code ret;
    // Response length, will be obtained by mBleProtocolPrepareResponse
    uint16_t responseLen;
    mBleProtocolPayload_t responsePayload;
    mBleProtocolResponseCode_t responseCode;

    // add string termination to data received
    cmdItem->cmdPayload[cmdItem->cmdPayloadLen]='\0';
    
    // type received string
    LOG_INF( "SIM Thing Device ID Received: %s \r\n", cmdItem->cmdPayload );

    // save received string
    xCellMqttSnConfig_t mqttsnConnection;
    mqttsnConnection.plan = ANYWHERE;
    mqttsnConnection.con_duration = MQTTSN_ANYWHERE_CONNECTION_DURATION;
    strcpy( mqttsnConnection.clientID, cmdItem->cmdPayload );
    ret = xStorageSaveMqttSnConfig(mqttsnConnection);

    // check errors
    if( ret < 0 ){
        // if error while storing
        responseCode = M_CMD_RSP_ERROR;  
        responsePayload.data.errorCode = (int32_t) ret;
	}
    else{
        //if storing proccess was ok
        responseCode = M_CMD_RSP_OK;
        responsePayload.length = 0;
    }

    mBleProtocolPrepareResponse( cmdItem->command, responseCode, responsePayload, gpResponseBuf, 
                                sizeof(gpResponseBuf), &responseLen );

    return (err_code) bt_nus_send( NULL, gpResponseBuf, responseLen);

}
