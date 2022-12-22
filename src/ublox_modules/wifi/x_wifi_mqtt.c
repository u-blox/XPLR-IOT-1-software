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
 * @brief This file contains the function implementation of MQTT
 * module used in Sensor Aggregation use case (XPLR-IOT-1) 
 */

#include "x_wifi_mqtt.h"
#include <zephyr.h>
#include <logging/log.h>
#include <stdlib.h>  //atoi

#include "ubxlib.h"

#include "x_module_common.h"
#include "x_wifi_ninaW156.h"
#include "x_logging.h"
#include "x_system_conf.h"
#include "x_storage.h"
#include "x_led.h"


/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION
 * -------------------------------------------------------------- */

/** Thread called by xWifiMqttClientOpen(). Performs all necessary operations
 * to open an MQTT Client Session
*/
void xWifiMqttClientOpenThread(void);

/** Thread called by xWifiMqttClientConnect(). Performs all necessary operations
 * to connect to the MQTT Broker
*/
void xWifiMqttClientConnectThread(void);

/** Used by xWifiMqttClientClose(). Disconnects from the Broker. Use only
 * internally to the module
*/
static void xWifiMqttClientDisconnect(void);

/** Handle error happening in a thread
*/
static void mqttErrorHandle( err_code err );

/** MQTT Subription Callback. To be used when messages are received.
 * In this application it does nothing, just for example
*/
static void mqttSubscribeCb(int32_t unreadMsgCount, void *cbParam);

/** Disconnect from MQTT Broker callback. In this implementation it
 * does nothing, just for example
*/
static void mqttDisconnectCb(int32_t errorCode, void *pParam);


/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

LOG_MODULE_REGISTER(LOGMOD_NAME_WIFI_MQTT, LOG_LEVEL_DBG);

//Semaphores definition
K_SEM_DEFINE(xWifiMqttClientOpen_semaphore, 0, 1);
K_SEM_DEFINE(xWifiMqttClientConnect_semaphore, 0, 1);

//Threads definition
K_THREAD_DEFINE(xWifiMqttClientConnectThreadID, MQTT_STACK_SIZE, xWifiMqttClientConnectThread, NULL, NULL, NULL,
		MQTT_PRIORITY, 0, 0);

K_THREAD_DEFINE(xWifiMqttClientOpenThreadID, MQTT_STACK_SIZE, xWifiMqttClientOpenThread, NULL, NULL, NULL,
		MQTT_PRIORITY, 0, 0);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/**MQTT context data for use with ubxlib functions*/
uMqttClientContext_t *gMqttClientCtx;   

/**Holds MQTT client status in terms of Sensor Aggregation application*/
static xClientStatusStruct_t gMqttStatus = {.type = MqttClient, .status = ClientClosed };

/** Holds the result of the last operation performed by this module (refers to operations
 *  that need to report their result to other modules of the application and cannot report
 *  their results directly e.g. a thread operation)
*/
static err_code gLastOperationResult = X_ERR_SUCCESS;

// MQTT client configuration strings
//+1 for string termination character
static char gpMqttClientIdSrt[ MQTT_DEVICE_ID_MAXLEN + 1 ] = {0};
static char gpMqttUserNameStr[ MQTT_USERNAME_MAXLEN + 1 ] = {0};
static char gpMqttPasswordStr[ MQTT_PASSWORD_MAXLEN + 1 ] = {0};

/** A copy of the active (open) MQTT client configuration */
uMqttClientConnection_t gMqttActiveConfigCopy={0};



/* ----------------------------------------------------------------
 * CALLBACKS
 * -------------------------------------------------------------- */

static void mqttSubscribeCb(int32_t unreadMsgCount, void *cbParam){
    //do nothing
}



static void mqttDisconnectCb(int32_t errorCode, void *pParam){
    //do nothing
}


/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


static void mqttErrorHandle( err_code err ){

    gLastOperationResult = err;
    xLedBlink( ERROR_LEDCOL, ERROR_LED_DELAY_ON, ERROR_LED_DELAY_OFF, ERROR_LED_BLINKS);
}



void xWifiMqttClientOpenThread(void){

    while(1){

        // Semaphore given by xWifiMqttClientOpen()
        k_sem_take( &xWifiMqttClientOpen_semaphore, K_FOREVER );

        LOG_DBG("Open MQTT client request \r\n");

        gLastOperationResult = X_ERR_SUCCESS;
        xWifiNinaStatus_t nina_status = xWifiNinaGetModuleStatus();

        // If not connected to WiFi network, try to connect before opening client
        if( nina_status.isConnected == false ){

            LOG_WRN("Not connected to a WiFi Network, trying to connect now\r\n");
            xWifiNinaConnect();

            // wait until connected or error while trying to connect happens
            while( ( nina_status.isConnected == false ) && (gLastOperationResult == X_ERR_SUCCESS ) ){
                    k_sleep(K_MSEC(1000));
                    nina_status = xWifiNinaGetModuleStatus();
                    gLastOperationResult = xWifiNinaGetLastOperationResult();
                    //ubxlib contains timeouts by itself, no need to add one
            }

            //if error happens
            if( gLastOperationResult != X_ERR_SUCCESS ){
                LOG_ERR("Error Code from WiFi Connection Request: %d - Abort MQTT Client open", gLastOperationResult);
                continue;
            }
        }

        //start optical indication for Mqtt opening
        xLedFade(MQTT_ACTIVATING_LEDCOL,MQTT_ACTIVATING_LED_DELAY_ON,MQTT_ACTIVATING_LED_DELAY_OFF,0);


        if( gMqttStatus.status > ClientClosed ){
            LOG_WRN("Client already open. Close it, before opening new one\r\n");
            mqttErrorHandle( X_ERR_INVALID_STATE ); 
            continue;
        }

        // Open Mqtt client and check for errors
        gMqttClientCtx = pUMqttClientOpen( xWifiNinaGetHandle(), NULL);
        if( gMqttClientCtx == NULL ){
            err_code err = uMqttClientOpenResetLastError();
            LOG_ERR("pUMqttClientOpen err: %d", err );
            mqttErrorHandle( err ); 
            continue;
        }

        // Client is open
        xLedOff();
        gMqttStatus.status = ClientOpen;
        LOG_INF("MQTT client opened \r\n");
        gLastOperationResult = X_ERR_SUCCESS;

    }
}



void xWifiMqttClientConnectThread(void){

    // needed to avoid thread overflows when using ubxlib functions within a thread
    k_thread_system_pool_assign(k_current_get());

    while(1){

        // Semaphore given by xWifiMqttClientConnect 
        k_sem_take( &xWifiMqttClientConnect_semaphore, K_FOREVER );

        LOG_DBG("Connect to MQTT client requested\r\n");

        // load saved configuration for MQTT connection
        gLastOperationResult = xStorageReadMqttConfig( gpMqttClientIdSrt, MQTT_DEVICE_ID_MAXLEN,
                                            gpMqttUserNameStr, MQTT_USERNAME_MAXLEN,
                                            gpMqttPasswordStr, MQTT_PASSWORD_MAXLEN );

        // If error happens while loading configuration for MQTT connection
        if( gLastOperationResult !=  X_ERR_SUCCESS ){
            mqttErrorHandle( gLastOperationResult );
            if( gLastOperationResult == ERR_STORAGE_FILE_NOT_FOUND ){
                LOG_WRN("No settings saved. Use \"modules MQTT save\" command to save a setting \r\n");    
            }
            else{
                LOG_ERR("Error reading MQTT setting files \r\n");
            }
            continue;
        }
        

        if( gMqttStatus.status == ClientConnected ){
            LOG_INF("Already connected \r\n");
            gLastOperationResult = X_ERR_SUCCESS;
            continue;
        }

        // If an MQTT client is not open, open one now
        if( gMqttStatus.status < ClientOpen ){
            LOG_WRN("No open MQTT client. Opening MQTT client now\r\n");
            xWifiMqttClientOpen();
            
            // wait for client to be opened and check for errors 
            while( ( gMqttStatus.status < ClientOpen ) && ( gLastOperationResult == X_ERR_SUCCESS ) ){
                k_sleep(K_MSEC(1000));
            }
            
            // if error while opening client
            if(gLastOperationResult!=X_ERR_SUCCESS){
                LOG_ERR("Error Code from MQTT open client request: %d - Aborting MQTT Connection \r\n", gLastOperationResult);
                continue;
            }
        }

        //start optical indication for MQTT connection
        xLedFade(MQTT_ACTIVATING_LEDCOL,MQTT_ACTIVATING_LED_DELAY_ON,MQTT_ACTIVATING_LED_DELAY_OFF,0);

        // set the configuration from previously loaded parameters
        const uMqttClientConnection_t mqttConnection = {
                .pBrokerNameStr = MQTT_BROKER_NAME,
                .localPort = MQTT_PORT,  //1883,
                .pClientIdStr = gpMqttClientIdSrt,
                .pUserNameStr = gpMqttUserNameStr,
                .pPasswordStr = gpMqttPasswordStr
        };

        // keep a copy (for type commands etc)
        gMqttActiveConfigCopy = mqttConnection;

        LOG_INF("MQTT connecting\r\n");

        // Connect to MQTT broker and check for errors
        gLastOperationResult = uMqttClientConnect(gMqttClientCtx, &mqttConnection);
        if( gLastOperationResult != X_ERR_SUCCESS ){
            LOG_ERR("uMqttClientConnect failed\r\n");
            mqttErrorHandle( gLastOperationResult );
            continue;
        } 

        // setup a Subscription callback -- Not used in the application, just for example
        gLastOperationResult = uMqttClientSetMessageCallback(gMqttClientCtx, mqttSubscribeCb, (void *)gMqttClientCtx);
        if( gLastOperationResult != X_ERR_SUCCESS ){
            LOG_ERR("uMqttClientSetMessageCallback failed\r\n");
            mqttErrorHandle( gLastOperationResult );
            continue;
        } 

        // setup an MQTT disconnection callback. Does nothing in this implementation
        gLastOperationResult = uMqttClientSetDisconnectCallback(gMqttClientCtx, mqttDisconnectCb, (void *)gMqttClientCtx);
        if( gLastOperationResult != X_ERR_SUCCESS ){
            LOG_ERR("uMqttClientSetDisconnectCallback failed\r\n");
            mqttErrorHandle( gLastOperationResult );
            continue;
        }

        // wait for connection to be established
        while( gMqttStatus.status != ClientConnected ){
            
            // if connection is established
            if( uMqttClientIsConnected(gMqttClientCtx) ){
                gMqttStatus.status = ClientConnected;
                xLedOff();
            }

            k_sleep(K_MSEC(1000));

            //printk(".");
        }

        LOG_INF("MQTT connected\r\n");

        gLastOperationResult = X_ERR_SUCCESS;
        
    }
}



/* ----------------------------------------------------------------
 * PUBLIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */

bool xWifiMqttClientConnected(void){

    // the xWifiMqttClientConnect should have been performed
    // succesfully to be in a connected status
    if( gMqttStatus.status == ClientConnected ){

        // check if still connected
        if( uMqttClientIsConnected(gMqttClientCtx) ){
            return true;
        }
        else{
            /* if a disconnection has happened, better close the client
            * and try to reopen and reconnect to the broker later.
            * (Implemented in this way so the user does not need to remember that
            * he needs to close the client to reconnect if this returns false,
            *  he just uses the xWifiMqttClientConnect() function again)
            * 
            * * if the disconnection happened from the broker part, it may not work
            */
            xWifiMqttClientClose(); 
            return false;
        }
    }

    else return false;
}



static void xWifiMqttClientDisconnect(void){

    // Disconnecting optical indication
    xLedFade(MQTT_DEACTIVATING_LEDCOL,MQTT_ACTIVATING_LED_DELAY_ON,MQTT_ACTIVATING_LED_DELAY_OFF,0);

    LOG_INF("MQTT Client Disconnection Request \r\n");

    gLastOperationResult = uMqttClientDisconnect(gMqttClientCtx);
    
    if( gLastOperationResult != X_ERR_SUCCESS ){
        LOG_ERR("uMqttClientDisconnect error code: %d \r\n", gLastOperationResult);    
        // continue even if error occurs, normally this is used in
        // the context of deinitialization anyway, because the client
        // works better if its closed after a disconnection 
    }
    
    if( gMqttStatus.status == ClientConnected ){
        gMqttStatus.status = ClientOpen;
    }
    
    xLedOff();
    LOG_INF("MQTT client disconnection action completed\r\n");
    return;
}



void xWifiMqttClientClose(void){

    LOG_INF("MQTT Client Close Request \r\n");

    // if connected to MQTT Broker disconnect before closing the client
    if(gMqttStatus.status == ClientConnected){
        LOG_WRN("Client Connected. Disconnecting MQTT client now \r\n");
        xWifiMqttClientDisconnect();
    }

    if(gMqttStatus.status == ClientOpen){
        LOG_INF("Closing MQTT client \r\n");
        uMqttClientClose(gMqttClientCtx);
        gMqttStatus.status = ClientClosed;    
    }

    LOG_INF("MQTT Client Closed\r\n");
}



xClientStatusStruct_t xWifiMqttClientGetStatus(void){
    return gMqttStatus;
}



err_code xWifiMqttClientPublish(const char *pTopicNameStr, const char *pMessage, size_t messageSizeBytes, uint8_t qos, bool retain){
    
    // Should be connected to publish
    if( gMqttStatus.status < ClientConnected){
            LOG_WRN("MQTT not connected\r\n");  
            return X_ERR_INVALID_STATE;  
    }

    uMqttQos_t qos_var = qos;
    return uMqttClientPublish(gMqttClientCtx, pTopicNameStr, pMessage, messageSizeBytes, qos_var, retain);
}



void xWifiMqttClientConnect(void){

    k_sem_give( &xWifiMqttClientConnect_semaphore );
}



void xWifiMqttClientOpen(void){

    k_sem_give( &xWifiMqttClientOpen_semaphore );
}



err_code xWifiMqttGetLastOperationResult(void){

    return gLastOperationResult;
}


/* ----------------------------------------------------------------
 * SHELL COMMANDS IMPLEMENTATION
 * -------------------------------------------------------------- */

void xWifiMqttSaveConfigCmd(const struct shell *shell, size_t argc, char **argv){

        int ret_code; 

        // check for valid number of parameters
        if( argc == 4 ){

            bool invalid = false;

            // checks parameters validity
            if( strlen( argv[1] ) >= MQTT_DEVICE_ID_MAXLEN ){
                shell_error( shell,"DeviceID length cannot be greater than %d\r\n", MQTT_DEVICE_ID_MAXLEN );    
                invalid = true;
            }
            if( strlen( argv[2] )  >= MQTT_USERNAME_MAXLEN ){
                shell_error( shell,"Username length cannot be greater than %d\r\n", MQTT_USERNAME_MAXLEN );    
                invalid = true;
            }
            if( strlen( argv[3] ) >= MQTT_PASSWORD_MAXLEN ){
                shell_error( shell,"Password length cannot be greater than %d\r\n", MQTT_PASSWORD_MAXLEN );    
                invalid = true;
            }

            if( invalid ){
                return;
            }

            //valid paraeters given

            uMqttClientConnection_t mqttConfig = {.pClientIdStr = argv[1], .pUserNameStr = argv[2], .pPasswordStr = argv[3]};

            shell_print(shell, "MQTT DeviceID: ""%s"" Username: ""%s"" Password: ""%s"" \r\n", 
                                mqttConfig.pClientIdStr, mqttConfig.pUserNameStr, mqttConfig.pPasswordStr); 

            ret_code = xStorageSaveMqttConfig( mqttConfig );
            
            if( ret_code < 0 ){
                shell_error( shell, "Error while saving: %d\r\n", ret_code ); 
                return;
            }
        }

        // invalid number of parameters given
        else{
            shell_print(shell, "Please provide <Device ID> <Username> <Password> \r\n");  
        }

		return;
}



void xWifiMqttTypeConfigCmd(const struct shell *shell, size_t argc, char **argv){

        //  ----------- Type Active (Connected) Client Configuration ----
    
    if(gMqttStatus.status == ClientConnected){
        shell_print(shell, "------ Currently Added/Active MQTT Setting --------\r\n");
        shell_print(shell, "Broker Name: %s \r\nPort: %d \r\nDevice ID: %s \r\nUsername: %s \r\nPassword: %s \r\n",
                            MQTT_BROKER_NAME,MQTT_PORT,
                            gMqttActiveConfigCopy.pClientIdStr,
                            gMqttActiveConfigCopy.pUserNameStr,
                            gMqttActiveConfigCopy.pPasswordStr );
    }

    //  ----------- Type Saved/Next MQTT Client Configuration ----

    shell_print(shell, "------ MQTT Setting saved in memory --------\r\n");

    char deviceid[ MQTT_DEVICE_ID_MAXLEN + 1 ];
    char username[ MQTT_USERNAME_MAXLEN + 1 ];
    char password[ MQTT_PASSWORD_MAXLEN + 1 ];
    
    int ret_code = xStorageReadMqttConfig( 
        deviceid, MQTT_DEVICE_ID_MAXLEN, 
        username, MQTT_USERNAME_MAXLEN,
        password, MQTT_PASSWORD_MAXLEN );

    if( ret_code < 0 ){
        if( ret_code == ERR_STORAGE_FILE_NOT_FOUND ){
            shell_print( shell,"No settings saved \r\n" );    
        }
        else{
            shell_print( shell,"Error reading files \r\n" );
        }
         return;
    }
    
    shell_print( shell, "Broker Name: %s \r\nPort: %d \r\nDevice ID: %s \r\nUsername: %s \r\nPassword: %s \r\n",
                            MQTT_BROKER_NAME,MQTT_PORT,
                            deviceid,
                            username,
                            password );

    return;
}



void xWifiMqttClientStatusCmd(const struct shell *shell, size_t argc, char **argv){

    shell_print(shell, "\r\nMQTT --------------------------------\r\n");
    
    // if in connected status, also type the confuguration
    if( gMqttStatus.status == ClientConnected ){
        if( xWifiMqttClientConnected() ){
            shell_print(shell, "Client is connected\r\n");
            shell_print(shell, "Broker Name: %s \r\nPort: %d \r\nDevice ID: %s \r\nUsername: %s \r\nPassword: %s \r\n",
                            MQTT_BROKER_NAME,MQTT_PORT,
                            gMqttActiveConfigCopy.pClientIdStr,
                            gMqttActiveConfigCopy.pUserNameStr,
                            gMqttActiveConfigCopy.pPasswordStr );
        }
        else{
            shell_print(shell, "Client has been disconnected and closed\r\n");
        }
    }
    
    // else only type the status
    else if(gMqttStatus.status == ClientClosed){
        shell_print(shell, "Client is closed\r\n");
    }

    else if(gMqttStatus.status >= ClientOpen){
        shell_print(shell, "Client open\r\n");
    }
    
    return;
}



void xWifiMqttSendCmd(const struct shell *shell, size_t argc, char **argv){

        // cannot send if the function to connect to an MQTT broker
        // has not been called
        if( gMqttStatus.status < ClientConnected){
            shell_print(shell, "MQTT not connected, abort action\r\n");  
            return;
        }

        // Given Parameters Check
        if(argc==1){
            shell_print(shell, "Please provide topic, message and QOS\r\n");  
            return;
        }

        else if(argc==2){
            shell_print(shell, "Please also provide message and QOS\r\n");  
            return;
        }

        else if(argc==3){
            shell_print(shell, "Please also provide QOS\r\n");  
            return;
        }

        // correct number of parameters
        else if(argc==4){

            // Get Quality of service
            uMqttQos_t qos = atoi(argv[3]);
            if( qos >= U_MQTT_QOS_MAX_NUM ){
                shell_print(shell, "Invalid QOS\r\n");  
                return;
            }

            shell_print(shell, "Sending ""%s"" to topic ""%s"" with QoS: %d\r\n", argv[2], argv[1], (uint8_t)qos );

            // Check if actually connected, regardless of MQTT module status
            if( !xWifiMqttClientConnected() ){
                shell_error(shell, "MQTT client has been disconnected, abort action\r\n" );
            }
    
            // Publish (send) the message
            int32_t err = uMqttClientPublish(gMqttClientCtx, argv[1], argv[2], strlen(argv[2]), qos, 0);
            if(err != 0){
                shell_error(shell, "uMqttClientPublish failed: %d\r\n", err );
            }
            else{
                shell_print(shell, "Published\r\n" );
            }
        }

		return;
}




err_code xWifiMqttDeleteConfig( void ){

    err_code rc;
    err_code ret = 0;
    bool notFound = false;

    rc = xStorageDeleteFile( mqtt_deviceID_fname ); 		
    if( rc == ERR_STORAGE_FILE_NOT_FOUND ){
        notFound = true;
    }
    else if( rc < 0 ){
        ret = rc;
    }
    
    rc = xStorageDeleteFile( mqtt_username_fname );
    if( rc == ERR_STORAGE_FILE_NOT_FOUND ){
        notFound = true;
    }
    else if( rc < 0 ){
        ret = rc;
    }

    rc = xStorageDeleteFile( mqtt_psw_fname );  	
    if( rc == ERR_STORAGE_FILE_NOT_FOUND ){
        notFound = true;
    }
    else if( rc < 0 ){
        ret = rc;
    }

    // If even one of the delete functions returns ERR_STORAGE_FILE_NOT_FOUND
    // then this error is returned (a full configuration for mqtt was not saved)
    if( notFound ){
        return ERR_STORAGE_FILE_NOT_FOUND;
    }

    // if one of the delete functions returned an error code, return this code
    // if more than one delete functions returned an error code, then return the last one
    return ret;
}