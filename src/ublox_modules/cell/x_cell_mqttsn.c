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
 * @brief This file contains the function implementation of MQTT-SN
 * module used in Sensor Aggregation use case (XPLR-IOT-1)  
 */

#include "x_cell_mqttsn.h"

//Zephyr-SDK related
#include "zephyr.h"
#include <logging/log.h>
#include <ctype.h>
#include <stdlib.h> //atoi
#include <stdio.h> //sprintf

//Ubxlib related
#include "ubxlib.h"

//Application related
#include "x_system_conf.h"
#include "x_cell_saraR5.h"
#include "x_logging.h"
#include "x_led.h"
#include "x_storage.h"
#include "x_module_common.h"



/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION
 * -------------------------------------------------------------- */

/** Thread called by xCellMqttSnClientOpen(). Performs all necessary operations
*/
void xCellMqttSnClientOpenThread(void);


/** Thread called by xCellMqttSnClientConnect(). Performs all necessary operations
 * to connect to the MQTT Broker
*/
void xCellMqttSnClientConnectThread(void);


/** Handle error happening in a thread
*/
static void mqttSnErrorHandle(err_code err_code);


/** Configure MQTT-SN client TLS security (certificate, key) to connect to
 *  Thingstream platform via MQTT Flex plan 
 * 
 *  Prerequisites: you have to save the certificate and key in SARA module
 *  file system using m-center (or AT commands), and then use function
 *  mqttSnImportCertKeyFiles, or send the following commands before this works
 *  properly:
 *  - AT+USECMNG=1,1,"Some_name","cert.pem" 
 *  - AT+USECMNG=1,2,"Some_other_name", "cert.key"
 * 
 * @return     zero on success, negative error code otherwise
*/
static err_code mqttSnConfigFlexTLSSecurity( void );


/** Used only for MQTT Flex.
 *  Import the client key and certificate to be used, from files saved in cellular
 *  module file system.
 * 
 *  Prerequisites:
 *  In order for this to work, files with the names specified by the following definitions:
 *  - MQTTSN_FLEX_CERTIFICATE_FILENAME 
 *  - MQTTSN_FLEX_KEY_FILENAME
 *  should be saved in the module. These are files downloaded from Thingstream
 *  (or some other broker) and then saved in the Cellular module via m-center.
 *  See instructions on how to Configure MQTT Flex in Readme.
 *
 * @return     zero on success, negative error code otherwise
*/
static err_code mqttSnImportCertKeyFiles(void);


/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */


LOG_MODULE_REGISTER(LOGMOD_NAME_CELL_MQTTSN, LOG_LEVEL_DBG);

// Semaphore definition
K_SEM_DEFINE(xCellMqttSnClientOpen_semaphore, 0, 1);
K_SEM_DEFINE(xCellMqttSnClientConnect_semaphore, 0, 1);

//Threads definition
K_THREAD_DEFINE(xCellMqttSnClientOpenThreadId, MQTTSN_STACK_SIZE, xCellMqttSnClientOpenThread, NULL, NULL, NULL,
		MQTTSN_PRIORITY, 0, 0);

K_THREAD_DEFINE(xCellMqttSnClientConnectThreadId, MQTTSN_STACK_SIZE, xCellMqttSnClientConnectThread, NULL, NULL, NULL,
		MQTTSN_PRIORITY, 0, 0);



/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/**MQTT context data for use with ubxlib functions*/
uMqttClientContext_t *gMqttSnClientCtx;  

// Security Settings for MQTT Flex plan
uSecurityTlsSettings_t tlsSettings = U_SECURITY_TLS_SETTINGS_DEFAULT;

/** Holds the result of the last operation performed by this module
 *  (refers to operations that need to report their result
 *  to other modules of the application and cannot report
 *  their results directly e.g. a thread operation) */
static err_code gLastOperationResult = X_ERR_SUCCESS;

/**Holds MQTT client status in terms of Sensor Aggregation application*/
static xClientStatusStruct_t gMqttSnStatus = {.type = MqttSNClient, .status = ClientClosed };

/** String representation of uMqttSnTopicNameType_t enums*/
const char *uMqttSNTopicType_strings[]={
    [ U_MQTT_SN_TOPIC_NAME_TYPE_ID_NORMAL ] = "normal",
    [ U_MQTT_SN_TOPIC_NAME_TYPE_NAME_SHORT ] = "short",
    [ U_MQTT_SN_TOPIC_NAME_TYPE_ID_PREDEFINED ] = "pre",
};

/** A copy of the active (open) MQTT-SN client configuration */
static xCellMqttSnConfig_t gMqttSnActiveConfigCopy;


/* ----------------------------------------------------------------
 * CALLBACKS
 * -------------------------------------------------------------- */


static void disconnectCallback(int32_t errorCode, void *pParam)
{
    LOG_WRN("Connection Dropped! \r\n");
    //gMqttSnStatus.status = ClientOpen;  //??
}


/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


static void mqttSnErrorHandle(err_code err_code){
    gLastOperationResult = err_code;
    xLedBlink( ERROR_LEDCOL, ERROR_LED_DELAY_ON, ERROR_LED_DELAY_OFF, ERROR_LED_BLINKS);
}


static err_code mqttSnImportCertKeyFiles(void){

    // The operations performed in this function are done with AT commands.
    // They are not directly supported by ubxlib functions.

    err_code ret;
    uAtClientHandle_t pAtHandle;

    // get AT client handler, to be able to send AT commands direclty to the module
    ret = uCellAtClientHandleGet( xCellSaraGetHandle(), &pAtHandle );
    if( ret < U_ERROR_COMMON_SUCCESS){
        return ret;
    }

    // Specify the certificate to be used with MQTT Flex
    // This block sends this command to the module:
    // AT+USECMNG=1,1,"MQTTFlex_cert","cert.pem"            
    uAtClientLock(pAtHandle);
    uAtClientCommandStart(pAtHandle, "AT+USECMNG=");
    uAtClientWriteInt(pAtHandle, 1);
    uAtClientWriteInt(pAtHandle, 1);
    // The actual name given is not important, since all certificates are scanned 
    // in mqttSnConfigFlexTLSSecurity() function and the first one found is used.
    uAtClientWriteString(pAtHandle, "MQTTFlex_cert",true);
    uAtClientWriteString(pAtHandle, MQTTSN_FLEX_CERTIFICATE_FILENAME,true);
    uAtClientCommandStopReadResponse(pAtHandle);
    if (uAtClientUnlock(pAtHandle) != 0) {
       LOG_ERR("Certificate import error \r\n");
       mqttSnErrorHandle( X_ERR_AT_CMD );
       return X_ERR_AT_CMD;
    }

    // Specify the key to be used with MQTT Flex
    // This block sends this command to the module:
    // AT+USECMNG=1,2,"MQTTFlex_key", "cert.key"
    uAtClientLock(pAtHandle);
    uAtClientCommandStart(pAtHandle, "AT+USECMNG=");
    uAtClientWriteInt(pAtHandle, 1);
    uAtClientWriteInt(pAtHandle, 2);
    // The actual name given is not important, since all keys are scanned 
    // in mqttSnConfigFlexTLSSecurity() function and the first one found is used.
    uAtClientWriteString(pAtHandle, "MQTTFlex_key",true);
    uAtClientWriteString(pAtHandle, MQTTSN_FLEX_KEY_FILENAME,true);
    uAtClientCommandStopReadResponse(pAtHandle);
    if (uAtClientUnlock(pAtHandle) != 0) {
       LOG_ERR("Certificate key import error \r\n");
       mqttSnErrorHandle( X_ERR_AT_CMD );
       return X_ERR_AT_CMD;
    }

    return X_ERR_SUCCESS;
}

static err_code mqttSnConfigFlexTLSSecurity( void ){

    err_code ret;
    
    uSecurityCredential_t buffer;
    static char ClientCertificateName[ sizeof buffer.name ];
    static char ClientPrivateKeyName[ sizeof buffer.name ];

    memset( ClientCertificateName, 0, sizeof(ClientCertificateName) );
    memset( ClientPrivateKeyName, 0, sizeof(ClientPrivateKeyName) );

    // Import certificate and Key from files saved in Cellular File system
    ret = mqttSnImportCertKeyFiles();
    if( ret < X_ERR_SUCCESS ){
        return ret;
    }

    // Cycle through all credentials saved in SARA module memory.
    // Find appropriate certificate and key saved in SARA module memory to use with Flex plan.
    // (you have to save this certificates and key in SARA module memory using m-center,
    // and then use function mqttSnImportCertKeyFiles, or send the commands before this works properly:
    // - AT+USECMNG=1,1,"Some_name","cert.pem" 
    // - AT+USECMNG=1,2,"Some_other_name", "cert.key"
    // In case more than one appropriate certificate/Key are found the first ones are used
    for (int32_t x = uSecurityCredentialListFirst( xCellSaraGetHandle(), &buffer);
         x >= 0;
         x = uSecurityCredentialListNext(xCellSaraGetHandle(), &buffer)) {
        
        // found Client Certificate -> use it
        if( buffer.type == U_SECURITY_CREDENTIAL_CLIENT_X509 ){
            memcpy( ClientCertificateName, buffer.name, sizeof(ClientCertificateName) );
            LOG_INF("Client Certificate Found: %s\r\n", ClientCertificateName);    
        }

        // found Private Key -> use it
        else if( buffer.type == U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE ){
            memcpy( ClientPrivateKeyName, buffer.name, sizeof(ClientPrivateKeyName) );
            LOG_INF("Client Private Key Found: %s\r\n", ClientPrivateKeyName);    
        }

        // clear buffer before next search
        memset( &buffer , 0, sizeof(buffer) );
    }

    // Both Client Certificate and Private key need to be found before proceeding 
    if( ( strlen( ClientCertificateName ) == 0 ) || ( strlen(ClientPrivateKeyName) == 0 ) ){
        LOG_INF("Did not find both Client Certificate and Private Key \r\n");
        return X_ERR_NOT_FOUND;
    }

    // Set up the tls Settings structure
    tlsSettings.tlsVersionMin = U_SECURITY_TLS_VERSION_1_2;
    tlsSettings.pRootCaCertificateName = NULL;
    tlsSettings.pClientCertificateName = ClientCertificateName;
    tlsSettings.pClientPrivateKeyName = ClientPrivateKeyName;
    tlsSettings.certificateCheck = U_SECURITY_TLS_CERTIFICATE_CHECK_NONE;
    tlsSettings.pClientPrivateKeyPassword = NULL;
    tlsSettings.cipherSuites.num = 1;
    tlsSettings.cipherSuites.suite[0] = U_SECURITY_TLS_CIPHER_SUITE_ECDHE_RSA_WITH_AES_256_GCM_SHA384;
    //tlsSettings.psk;
    //tlsSettings.pskId;
    //tlsSettings.pskGeneratedByRoT;
    //tlsSettings.pExpectedServerUrl;
    //tlsSettings.pSni;
    //tlsSettings.enableSessionResumption;
    tlsSettings.useDeviceCertificate = false;
    //tlsSettings.includeCaCertificates;
    
    return X_ERR_SUCCESS;
}


void xCellMqttSnClientOpenThread(void){

    while(1){

        //Semaphore given by MqttSnClientOpen()
        k_sem_take( &xCellMqttSnClientOpen_semaphore, K_FOREVER );

        LOG_DBG("MQTT-SN Open Client Request\r\n");
        
        // Check if client already open
        if( gMqttSnStatus.status >= ClientOpen ){
            LOG_INF("Client Already open \r\n");
            gLastOperationResult = X_ERR_SUCCESS; //error code?
            continue;
        }

        // if SARA has not been instructed to connect/register to an MNO yet,
        // do it now
        gLastOperationResult = X_ERR_SUCCESS;
        xCellSaraStatus_t sara_stat = xCellSaraGetModuleStatus();

        if( sara_stat.isReadyForMqttSN == false ){

            LOG_WRN("Cell not been instructed to connect to MNO yet. Trying that now\r\n");
            xCellSaraConnect();

            while( ( !sara_stat.isReadyForMqttSN ) && ( gLastOperationResult == X_ERR_SUCCESS ) ){
                k_sleep(K_MSEC(1000));
                sara_stat = xCellSaraGetModuleStatus();
                gLastOperationResult = xCellSaraGetLastOperationResult();
            }
            if(gLastOperationResult != X_ERR_SUCCESS){
                LOG_ERR("Error Code from SARA connect request: %d - Aborting MQTT-SN client open \r\n", gLastOperationResult);
                continue;
            }
        }

        //optical indication
        xLedFade( MQTTSN_ACTIVATING_LEDCOL, MQTTSN_ACTIVATING_LED_DELAY_ON, MQTTSN_ACTIVATING_LED_DELAY_OFF, 0 );

        // Open an MQTTSN client for MQTT Anywhere plan
        if( xCellSaraGetActiveMqttPlan() == ANYWHERE ){ 

            gMqttSnClientCtx = pUMqttClientOpen( xCellSaraGetHandle(), NULL);
        }

        // Open an MQTTSN client for MQTT Flex plan
        else if( xCellSaraGetActiveMqttPlan() == FLEX ){

            gLastOperationResult = mqttSnConfigFlexTLSSecurity();
            if( gLastOperationResult < X_ERR_SUCCESS ){
                continue;
            }
            
            gMqttSnClientCtx = pUMqttClientOpen( xCellSaraGetHandle(), &tlsSettings);
        }

        if( gMqttSnClientCtx == NULL ){
            err_code err = uMqttClientOpenResetLastError();
            LOG_ERR("pUMqttClientOpen err: %d", err );
            mqttSnErrorHandle( err ); 
            continue;
        }

        if( !uMqttClientSnIsSupported(gMqttSnClientCtx) ){
            LOG_ERR("MQTTSN Not Supported \r\n");
            mqttSnErrorHandle( X_ERR_UNKNOWN ); 
            continue;
        }

        LOG_INF("Client opened \r\n");
        gMqttSnStatus.status = ClientOpen;
        xLedOff();
    }
}


void xCellMqttSnClientConnectThread(void){

    char brokerNameStr[ MQTTSN_BROKER_NAME_MAXLEN ];
    char portStr[10];

    k_thread_system_pool_assign(k_current_get());

    while(1){

        // Semaphore given by xCellMqttSnClientConnect()
        k_sem_take( &xCellMqttSnClientConnect_semaphore, K_FOREVER );

        LOG_DBG("MQTT-SN Connect Request\r\n");

        if( gMqttSnStatus.status == ClientConnected ){
            LOG_INF("MQTT-SN status already Connected\r\n");
            continue;
        }

        // If client is not open yet, open it now
        if( gMqttSnStatus.status < ClientOpen){
            LOG_WRN("MQTTSN client not open/configured yet. Trying that now \r\n");
            gLastOperationResult = X_ERR_SUCCESS;
            xCellMqttSnClientOpen();
            while( ( gMqttSnStatus.status < ClientOpen ) && ( gLastOperationResult == X_ERR_SUCCESS ) ){
                k_sleep(K_MSEC(1000));
                // gLastOperationResult is updated through the xCellMqttSnClientConnectThread
                // which belongs to this module
            }

            if(gMqttSnStatus.status < ClientOpen){
                LOG_ERR("Error Code from MQTT-SN Open Request: %d - Abort MQTT-SN Connection", gLastOperationResult);
                continue;
            }
        }

        // optical indication
        xLedFade(MQTTSN_ACTIVATING_LEDCOL,MQTTSN_ACTIVATING_LED_DELAY_ON,MQTTSN_ACTIVATING_LED_DELAY_OFF,0);

        // Read saved Configuration for Connection
        xCellMqttSnPlan_t activePlan = xCellSaraGetActiveMqttPlan();
        int rc = xStorageReadMqttSnConfig( &gMqttSnActiveConfigCopy, activePlan );
        if( rc < 0 ){
            if( rc == ERR_STORAGE_FILE_NOT_FOUND ){
                LOG_ERR("No settings saved for MQTTSN plan. Please use \"modules MQTTSN save\" command \r\n");    
            }
        else{
            LOG_ERR("Error reading files for MQTTSN plan\r\n");
        }
            mqttSnErrorHandle( rc );
            continue;
        }

        // Prepare Broker Name string
        // add port at the end of the broker string with a semicolon
        // e.g. "mqtt-flex.thingstream.io:2443", where 2443 is the port
        if( activePlan == FLEX ){
            memset( brokerNameStr, 0, sizeof(brokerNameStr) );
            memset( portStr,0 , sizeof(portStr) );
            strcpy( brokerNameStr, MQTTSN_FLEX_BROKER_HOSTNAME );
            sprintf( portStr, ":%d", MQTTSN_FLEX_BROKER_PORT );
            strcat( brokerNameStr, portStr);
        }
        else if( activePlan == ANYWHERE ){
            memset( brokerNameStr, 0, sizeof(brokerNameStr) );\
            memset( portStr,0 , sizeof(portStr) );
            strcpy( brokerNameStr, MQTTSN_ANYWHERE_BROKER_IP );
            sprintf( portStr, ":%d", MQTTSN_ANYWHERE_BROKER_PORT );
            strcat( brokerNameStr, portStr);
        }

        const uMqttClientConnection_t mqttSnConnection = {
            .pBrokerNameStr = brokerNameStr,
            .localPort = -1, 
            .pClientIdStr = gMqttSnActiveConfigCopy.clientID,
            .inactivityTimeoutSeconds = 600,
            .mqttSn = true
        };

        uMqttClientSetDisconnectCallback(gMqttSnClientCtx, disconnectCallback, NULL);
        gLastOperationResult = uMqttClientConnect(gMqttSnClientCtx, &mqttSnConnection);

        if( gLastOperationResult != X_ERR_SUCCESS ){
            LOG_ERR("uMqttSnClientConnect failed: %d\r\n", gLastOperationResult );
            mqttSnErrorHandle( gLastOperationResult );
            continue;
        } 

        LOG_INF("MQTTSN Connected \r\n"); 
        gMqttSnStatus.status = ClientConnected;
        
        xLedOff();
    }
}


/* ----------------------------------------------------------------
 * PUBLIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


err_code xCellMqttSnGetLastOperationResult(void){
    return gLastOperationResult;
}



void xCellMqttSnClientOpen(void){
    k_sem_give( &xCellMqttSnClientOpen_semaphore );
}



void xCellMqttSnClientConnect(void){
    k_sem_give( &xCellMqttSnClientConnect_semaphore );
}



xClientStatus_t xCellMqttSnClientGetStatus(void){
    return gMqttSnStatus.status;
}



err_code xCellMqttSnClientClose(void){

    LOG_INF("MQTT-SN Client Close Request \r\n");

    // if connected to MQTT Broker disconnect before closing the client
    if( gMqttSnStatus.status == ClientConnected ){
        LOG_WRN("Client Connected. Trying to disconnect now \r\n");
        // return code from xCellMqttSnClientDisconnect is not used in this
        // function, we continue closing the client anyway
        xCellMqttSnClientDisconnect();
    }

    if(gMqttSnStatus.status == ClientOpen){
        LOG_INF("Closing MQTT client \r\n");
        uMqttClientClose(gMqttSnClientCtx);
        gMqttSnStatus.status = ClientClosed;    
    }

    LOG_INF("MQTT-SN Client Closed\r\n");
    
    return X_ERR_SUCCESS;
}


err_code xCellMqttSnClientDisconnect(void){

    // Set optical indication
    xLedFade(MQTTSN_DEACTIVATING_LEDCOL,MQTTSN_ACTIVATING_LED_DELAY_ON,MQTTSN_ACTIVATING_LED_DELAY_OFF,0);

    LOG_INF("MQTT-SN Client Disconnection Request \r\n");

    gLastOperationResult = uMqttClientDisconnect(gMqttSnClientCtx);
    
    if( gLastOperationResult != X_ERR_SUCCESS ){
        LOG_ERR("Mqtt Sn Client Disconnect error code: %d \r\n", gLastOperationResult);    
        // continue even if error occurs, normally this is used in
        // the context of deinitialization anyway 
    }
    
    if( gMqttSnStatus.status == ClientConnected ){
        gMqttSnStatus.status = ClientOpen;
    }

   LOG_INF("MQTT-SN client disconnection action completed\r\n");
   xLedOff();
   return X_ERR_SUCCESS;
   
}


err_code xCellMqttSnClientPublish( const uMqttSnTopicName_t *pTopicName,
                                    const char *pMessage,
                                    size_t messageSizeBytes,
                                    uMqttQos_t qos, bool retain){

    /* Should be connected to publish
    *
    * Although QOS=3 does not need a connection to the Broker:
    * MQTT-SN publish of messages using QOS = 3 in a connectionless way is not currently directly
    * supported by ubxlib v1.0, because the connection parameters (broker name, Port and Client ID)
    * are passed to the library with the use of uMqttClientConnect() function, which connects the 
    * device to the broker. Update to allow connectionless use of QOS=3 is expected soon.
    */
    if( gMqttSnStatus.status < ClientConnected ){
            LOG_WRN("MQTT-SN not connected\r\n");  
            return X_ERR_INVALID_STATE;  
    }                                    
    
    err_code ret = uMqttClientSnPublish( gMqttSnClientCtx, pTopicName, pMessage, messageSizeBytes, qos, retain);
    if( ret!= X_ERR_SUCCESS ){
        LOG_ERR(" Publish error: %d \r\n", ret );
    }

    return ret;

}


/* ----------------------------------------------------------------
 * SHELL COMMANDS IMPLEMENTATION
 * -------------------------------------------------------------- */


void xCellMqttSnSaveConfigCmd(const struct shell *shell, size_t argc, char **argv){

        int ret_code; 

        // valid number of parameters
        if( ( argc >= 3 ) && ( argc <= 4 ) ){

            xCellMqttSnConfig_t mqttsnTemp;
            
            //check Plan parameter
            if( strcmp( argv[1], "flex") == 0 ) {
                mqttsnTemp.plan = FLEX;
            }
            else if( strcmp( argv[1], "anywhere") == 0 ){
                mqttsnTemp.plan = ANYWHERE;
            }
            else{
                shell_error( shell,"Plan parameter should be: \"flex\" or \"anywhere\" \r\n" );
                return;    
            }

            // check device id parameter
            if( strlen( argv[2] ) >= MQTTSN_CLIENT_ID_MAXLEN ){
                shell_error( shell,"DeviceID length cannot be greater than %d\r\n", MQTTSN_CLIENT_ID_MAXLEN );    
                return;
            }
            else{
                strcpy( mqttsnTemp.clientID, argv[2] );
            }

            if( mqttsnTemp.plan == ANYWHERE ){
                if( argc == 4 ){
                    mqttsnTemp.con_duration = atoi( argv[3] );
                }
                else{
                    mqttsnTemp.con_duration = MQTTSN_ANYWHERE_CONNECTION_DURATION;
                }
            }

            ret_code = xStorageSaveMqttSnConfig( mqttsnTemp );
            if( ret_code < 0 ){
                shell_error( shell,"Error while savnig %d\r\n", ret_code );
                return;    
            }
        }

        else{
            // invalid number of parameters
            shell_print( shell, "Please provide <Plan> <Device ID> <Connection Duration in seconds: if anywhere plan is selected >\r\n" );  
        }

		return;
}


void xCellMqttSnTypeConfigCmd(const struct shell *shell, size_t argc, char **argv){


      //  ----------- Type Active Config ----
    
    if( gMqttSnStatus.status >= ClientOpen ){
        shell_print( shell, "------ Currently Added/Active MQTT-SN Setting --------\r\n" );
        if( gMqttSnActiveConfigCopy.plan == FLEX ){

            shell_print( shell, "Broker Name: %s \r\nPort: %d \r\nDevice ID: %s \r\n\r\n",
                            MQTTSN_FLEX_BROKER_HOSTNAME,MQTTSN_FLEX_BROKER_PORT,
                            gMqttSnActiveConfigCopy.clientID );
        }

        if( gMqttSnActiveConfigCopy.plan == ANYWHERE ){

            shell_print( shell, "Broker IP: %s \r\nPort: %d \r\nDevice ID: %s Connection Duration %d seconds\r\n\r\n",
                            MQTTSN_ANYWHERE_BROKER_IP,MQTTSN_ANYWHERE_BROKER_PORT,
                            gMqttSnActiveConfigCopy.clientID,
                            gMqttSnActiveConfigCopy.con_duration );
        }
    }
    

    //  ----------- Type Saved/Next Initialization Setting ----

    shell_print( shell, "------ MQTT-SN Settings saved in memory --------\r\n" );

    xCellMqttSnConfig_t mqttSnConfig;

    //Read Flex configuration
    int ret_code = xStorageReadMqttSnConfig( &mqttSnConfig, FLEX);

    // Error while getting configuration (or does not exist)
    if( ret_code < 0 ){

        if( ret_code == ERR_STORAGE_FILE_NOT_FOUND ){
            shell_print( shell,"No settings saved for MQTT Flex \r\n" );    
        }

        else{
            shell_print( shell,"Error reading files for MQTT Flex \r\n" );
        }
    }

    else{
        shell_print( shell, "[FLEX] \r\nBroker Name: %s \r\nPort: %d \r\nDevice ID: %s \r\n",
                            MQTTSN_FLEX_BROKER_HOSTNAME, MQTTSN_FLEX_BROKER_PORT, mqttSnConfig.clientID );
    }


    //Read Anywhere configuration
    ret_code = xStorageReadMqttSnConfig( &mqttSnConfig, ANYWHERE );

    // Error while getting configuration (or does not exist)
    if( ret_code < 0 ){

        if( ret_code == ERR_STORAGE_FILE_NOT_FOUND ){
            shell_print(shell,"No settings saved for MQTT Anywhere \r\n");    
        }

        else{
            shell_print(shell,"Error reading files for MQTT Anywhere \r\n");
        }
    }

    else{
        shell_print(shell, "[ANYWHERE] \r\nBroker IP: %s \r\nPort: %d \r\nDevice ID: %s \r\nConnection Duration: %d \r\n",
                            MQTTSN_ANYWHERE_BROKER_IP,MQTTSN_ANYWHERE_BROKER_PORT, mqttSnConfig.clientID, mqttSnConfig.con_duration);
    }

    return;
}


void xCellMqttSnClientStatusCmd(const struct shell *shell, size_t argc, char **argv){

    shell_print(shell, "\r\nMQTT-SN --------------------------------\r\n");
    
    if( gMqttSnStatus.status >= ClientOpen ){
        
        if(gMqttSnStatus.status == ClientConnected)
            shell_print(shell, "Client is connected\r\n");
        else if(gMqttSnStatus.status == ClientOpen)
            shell_print(shell, "Client open\r\n");


        if( gMqttSnActiveConfigCopy.plan == FLEX){
            shell_print(shell, "Plan: Flex \r\nBroker HostName: %s \r\nPort: %d \r\nClient ID: %s \r\n",
                        MQTTSN_FLEX_BROKER_HOSTNAME,MQTTSN_FLEX_BROKER_PORT,
                        gMqttSnActiveConfigCopy.clientID);
        }
        else if(gMqttSnActiveConfigCopy.plan == ANYWHERE){
            shell_print(shell, "Plan: Anywhere \r\nBroker IP: %s \r\nPort: %d \r\nClient ID: %s \r\nAlive Time:%s \r\n",
                        MQTTSN_ANYWHERE_BROKER_IP,MQTTSN_ANYWHERE_BROKER_PORT,
                        gMqttSnActiveConfigCopy.clientID, gMqttSnActiveConfigCopy.con_duration );
        }
    }
    
    else if(gMqttSnStatus.status == ClientClosed){
        shell_print(shell, "Client is closed\r\n");
    }
    
    return;
}


void xCellMqttSnSendCmd(const struct shell *shell, size_t argc, char **argv){

        uMqttSnTopicName_t pTopicName;
        int32_t ret;

        if(argc==1){
            shell_print(shell, "Please provide topic type, topic name, message and QOS\r\n");  
            return;
        }
        else if(argc==2){
            shell_print(shell, "Please also provide topic name, message and QOS\r\n");  
            return;
        }
        else if(argc==3){
            shell_print(shell, "Please also provide message and QOS\r\n");  
            return;
        }
        else if(argc==4){
            shell_print(shell, "Please also provide QOS\r\n");  
            return;
        }
        else if(argc==5){
            //check qos
            int qos = atoi(argv[4]);
            if( (qos > U_MQTT_QOS_SEND_AND_FORGET  ) || (qos < 0) ){
                shell_print(shell, "Invalid QOS\r\n");  
                return;
            }

            // -- Connection is needed always - check why in 
            // xCellMqttSnClientPublish() comments.

            // //check connection if needed --> this will be uncommented when qos==3 is supported
            // //in a connectionless way by ubxlib.
            // if( qos < U_MQTTSN_QOS_SPECIAL ){
            //     // a connection is required
            //     if( gMqttSnStatus.status < ClientConnected){
            //         shell_print(shell, "MQTT-SN not connected, abort action (only QOS=3 can be used without connection)\r\n");  
            //         return;
            //     }
            // }

            // check topic type string and get enum
            uMqttSnTopicNameType_t topic_type_enum_idx = 0;
            bool topic_type_found = false;
            for( topic_type_enum_idx = 0; topic_type_enum_idx < U_MQTT_SN_TOPIC_NAME_TYPE_MAX_NUM; topic_type_enum_idx++ ){
                if( strcmp(argv[1],uMqttSNTopicType_strings[ topic_type_enum_idx ] ) == 0 ){
                    topic_type_found = true;
                    break;
                }
            }
            
            if( !topic_type_found ){
                shell_warn(shell, "Invalid topic type\r\n");  
                return;
            }

            shell_print(shell, "Sending ""%s"" to topic ""%s"" with QoS: %d\r\n", argv[3], argv[2], (uint8_t)qos );


            // prepare topic for use with xCellMqttSnClientPublish function
            switch(topic_type_enum_idx){

                case U_MQTT_SN_TOPIC_NAME_TYPE_ID_NORMAL: 

                    shell_print(shell, "Registering Topic name: %s\r\n", argv[2] );      
                    if( ( ret = uMqttClientSnRegisterNormalTopic( gMqttSnClientCtx, argv[2], &pTopicName) ) < X_ERR_SUCCESS ){
                        shell_warn(shell, "Error while registering topic name: %d\r\n", ret);  
                        return;
                    }
                    break;

                case U_MQTT_SN_TOPIC_NAME_TYPE_ID_PREDEFINED:

                    ;
                    uint16_t topic_id_num = atoi(argv[2]);
                    if( ( ret = uMqttClientSnSetTopicIdPredefined( topic_id_num, &pTopicName ) ) < X_ERR_SUCCESS ){
                        shell_warn(shell, "Error while registering topic name: %d\r\n", ret);  
                        return;
                    }
                    break;

                case U_MQTT_SN_TOPIC_NAME_TYPE_NAME_SHORT:

                    if( ( ret = uMqttClientSnSetTopicNameShort( argv[2], &pTopicName ) ) < X_ERR_SUCCESS ){
                        shell_warn(shell, "Error while registering topic name: %d\r\n", ret);  
                        return;
                    }
                    break;

                case U_MQTT_SN_TOPIC_NAME_TYPE_MAX_NUM: 
                    // steps for this case are performed in the default case that follows
                    // both are invalid states

                default: 
                    shell_warn( shell, "Invalid topic type\r\n");  
                    return; 
            }


            ret = xCellMqttSnClientPublish( &pTopicName, argv[3], strlen(argv[3]), qos, 0);  //0 is the retain
            if(ret != 0){
                shell_warn(shell, "xCellMqttSnClientPublish failed: %d\r\n", ret );
            }
            else{
                shell_print(shell, "Published\r\n" );
            }
        }

		return;
}



err_code xCellMqttSnDeleteAnywhereConfig( void ){

    err_code ret;

    xStorageDeleteFile( mqttsn_duration_fname ); 		
    // in case of error do nothing - it is not important if this file exists or not
   

    ret = xStorageDeleteFile( mqttsn_anywhere_deviceID_fname );
    return ret;
}