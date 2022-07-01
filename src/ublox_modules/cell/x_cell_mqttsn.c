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
#include "u_at_client.h"
#include "u_cell_sec_tls.h"
#include "u_security_tls.h" //redundant?
#include "u_cell.h" // uCellAtClientHandleGet

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

/** Configure MQTT-SN client to connect to Thingstream platform
 * via MQTT Anywhere plan
*/
static int32_t mqttSnConfigAnywherePlan(void);

/** Configure MQTT-SN client to connect to Thingstream platform
 * via MQTT Flex plan
*/
static int32_t mqttSnConfigFlexPlan(void);

/** When Publishing a message, if normal Topic Type is used the topic
*   should be registered prior to publishing. This function Registers
*   this topic
*/
static int32_t mqttSnRegisterTopic(const char *pTopicNameStr);

/** Callback which handles incoming events from MQTT-SN client,
 *  where events = Unsolicited Response Codes ( URC ) from SARA-R5 module
 * see:SARA-R5_ATCommands_UBX-19047455
*/
void mqttSnClientCb(uAtClientHandle_t  client, void *pUnused);



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

/** Network Handle returned and used by ubxlib functions */
static int32_t gNetHandle;

/** Handle to be used by ubxlib AT Client mocule */
static uAtClientHandle_t gAtHandle;

/** Holds the result of the last operation performed by this module
 *  (refers to operations that need to report their result
 *  to other modules of the application and cannot report
 *  their results directly e.g. a thread operation) */
static err_code gLastOperationResult = X_ERR_SUCCESS;

/**Holds MQTT client status in terms of Sensor Aggregation application*/
static xClientStatusStruct_t gMqttSnStatus = {.type = MqttSNClient, .status = ClientClosed };


/** String representation of uMqttSNTopicType_t enums*/
const char *uMqttSNTopicType_strings[]={
    [MQTTSN_TOPIC_NORMAL] = "normal",
    [MQTTSN_TOPIC_SHORT] = "short",
    [MQTTSN_TOPIC_PREDEFINED] = "pre",
};


/** Callback (Unsolicited Response Code) calls counter */
static int gCbCallsCnt =0;
/** Param X read from an Unsolicited Response Code (callback) */
static int gCbParamX=0;
/** Param Y read from an Unsolicited Response Code (callback) */
static int gCbParamY=0;
/** Param Z read from an Unsolicited Response Code (callback) */
static int gCbParamZ=0;


/** A copy of the active (open) MQTT-SN client configuration */
static xCellMqttSnConfig_t gMqttSnActiveConfigCopy;



/* ----------------------------------------------------------------
 * CALLBACKS
 * -------------------------------------------------------------- */

void mqttSnClientCb(uAtClientHandle_t  client, void *pUnused){
        
    // This callback is set to be triggered when an unsolicited 
    // response from MQTT-SN client is received from Sara-R5 (+UUMQTTSNC)
    // In this function we read the parameters of this response code
    // (see UMQTTSNC command in AT commands manual for SARA-R5: 
    // SARA-R5_ATCommands_UBX-19047455)


    gCbParamX = uAtClientReadInt(client);    
    gCbParamY = uAtClientReadInt(client); 

    if( gCbParamX==0 ){
        //+UUMQTTSNC: 0
        // response to mqtt disconnect
        // do nothing more
    } 

    if( gCbParamX==1 ){
        //+UUMQTTSNC: 1
        // response to mqtt connect
        // do nothing more
    }

    if( gCbParamX==2 ){
        //+UUMQTTSNC: 2,<MQTTSN_result>,<topic_id>
        //response to topic register -- read one more parameter
        gCbParamZ = uAtClientReadInt(client);  
    }

    // count the callbacks
    gCbCallsCnt++;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


static void mqttSnErrorHandle(err_code err_code){
    gLastOperationResult = err_code;
    xLedBlink( ERROR_LEDCOL, ERROR_LED_DELAY_ON, ERROR_LED_DELAY_OFF, ERROR_LED_BLINKS);
}


static int32_t mqttSnConfigAnywherePlan(void){

    // Read Saved Configuration for Anywhere Plan
    int rc = xStorageReadMqttSnConfig( &gMqttSnActiveConfigCopy, ANYWHERE);
    if( rc < 0 ){
      if( rc == ERR_STORAGE_FILE_NOT_FOUND ){
          LOG_ERR("No settings saved for MQTT Anywhere. Please use \"modules MQTTSN save\" command \r\n");    
      }
      else{
          LOG_ERR("Error reading files for MQTT Flex \r\n");
      }
      mqttSnErrorHandle( rc );
      return rc;
    }

    //Set the  client ID (copy from Thingstream portal):
    //AT+UMQTTSN=0,"identity:xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
    uAtClientLock(gAtHandle);
    uAtClientCommandStart(gAtHandle, "AT+UMQTTSN=");
    uAtClientWriteInt(gAtHandle, 0);
    uAtClientWriteString(gAtHandle, gMqttSnActiveConfigCopy.clientID,true);
    uAtClientCommandStopReadResponse(gAtHandle);
    if (uAtClientUnlock(gAtHandle) != 0) {
       LOG_ERR("Set Client ID error \r\n");
       mqttSnErrorHandle( X_ERR_AT_CMD );
       return X_ERR_AT_CMD;
    }

    //Set server IP address and port:
    //AT+UMQTTSN=2,"10.7.0.55",2442 
    uAtClientLock(gAtHandle);
    uAtClientCommandStart(gAtHandle, "AT+UMQTTSN=");
    uAtClientWriteInt(gAtHandle, 1);
    uAtClientWriteString(gAtHandle, MQTTSN_ANYWHERE_BROKER_IP,true);
    uAtClientWriteInt(gAtHandle, MQTTSN_ANYWHERE_BROKER_PORT);
    uAtClientCommandStopReadResponse(gAtHandle);
    if (uAtClientUnlock(gAtHandle) != 0) {
       LOG_ERR("Configure MQTT-SN gateway error \r\n");
       mqttSnErrorHandle( X_ERR_AT_CMD );
       return X_ERR_AT_CMD;
    }

    //Set connection duration (seconds):
    //AT+UMQTTSN=8,600
    uAtClientLock(gAtHandle);
    uAtClientCommandStart(gAtHandle, "AT+UMQTTSN=");
    uAtClientWriteInt(gAtHandle, 8);
    uAtClientWriteInt(gAtHandle, gMqttSnActiveConfigCopy.con_duration);
    uAtClientCommandStopReadResponse(gAtHandle);
    if (uAtClientUnlock(gAtHandle) != 0) {
       LOG_ERR("Configure connection duration error \r\n");
       mqttSnErrorHandle( X_ERR_AT_CMD );
       return X_ERR_AT_CMD;
    }

    return X_ERR_SUCCESS;
}


static int32_t mqttSnConfigFlexPlan(void){

    // Read saved Configuration
    int rc = xStorageReadMqttSnConfig( &gMqttSnActiveConfigCopy, FLEX);
    if( rc < 0 ){
      if( rc == ERR_STORAGE_FILE_NOT_FOUND ){
          LOG_ERR("No settings saved for MQTT Flex. Please use \"modules MQTTSN save\" command \r\n");    
      }
      else{
          LOG_ERR("Error reading files for MQTT Flex \r\n");
      }
      mqttSnErrorHandle( rc );
      return rc;
    }

     //Specify the certificate to be used with MQTT Flex
     
        //AT+USECMNG=1,1,"MQTTFlex_cert","cert.pem"            
        uAtClientLock(gAtHandle);
        uAtClientCommandStart(gAtHandle, "AT+USECMNG=");
        uAtClientWriteInt(gAtHandle, 1);
        uAtClientWriteInt(gAtHandle, 1);
        uAtClientWriteString(gAtHandle, "MQTTFlex_cert",true);
        uAtClientWriteString(gAtHandle, MQTTSN_FLEX_CERTIFICATE_FILENAME,true);
        uAtClientCommandStopReadResponse(gAtHandle);
        if (uAtClientUnlock(gAtHandle) != 0) {
           LOG_ERR("Certificate import error \r\n");
           mqttSnErrorHandle( X_ERR_AT_CMD );
           return X_ERR_AT_CMD;
        }

        //Specify the key to be used with MQTT Flex
        //AT+USECMNG=1,2,"MQTTFlex_key", "cert.key"
        uAtClientLock(gAtHandle);
        uAtClientCommandStart(gAtHandle, "AT+USECMNG=");
        uAtClientWriteInt(gAtHandle, 1);
        uAtClientWriteInt(gAtHandle, 2);
        uAtClientWriteString(gAtHandle, "MQTTFlex_key",true);
        uAtClientWriteString(gAtHandle, MQTTSN_FLEX_KEY_FILENAME,true);
        uAtClientCommandStopReadResponse(gAtHandle);
        if (uAtClientUnlock(gAtHandle) != 0) {
           LOG_ERR("Certificate key import error \r\n");
           mqttSnErrorHandle( X_ERR_AT_CMD );
           return X_ERR_AT_CMD;
        }

        //Set the profile ID to use.  This examples uses profile 0 (ubxlib adds a profile starting from 0)
        //AT+USECPRF=0
        uCellSecTlsContext_t *pContext;
        pContext = pUCellSecSecTlsAdd(gNetHandle);

        //Set the certificate validation level:
        //AT+USECPRF=0,0,0
        uAtClientLock(gAtHandle);
        uAtClientCommandStart(gAtHandle, "AT+USECPRF=");
        uAtClientWriteInt(gAtHandle, 0);
        uAtClientWriteInt(gAtHandle, 0);
        uAtClientWriteInt(gAtHandle, 0);
        uAtClientCommandStopReadResponse(gAtHandle);
        if (uAtClientUnlock(gAtHandle) != 0) {
           LOG_ERR("Set the certificate validation level error\r\n");
           mqttSnErrorHandle( X_ERR_AT_CMD );
           return X_ERR_AT_CMD;
        }

        // Set profile 0 to accept TLS v1.2: 
        // AT+USECPRF=0,1,3
        gLastOperationResult = uCellSecTlsVersionSet( pContext, U_SECURITY_TLS_VERSION_1_2);
        if(gLastOperationResult != X_ERR_SUCCESS){
            mqttSnErrorHandle( gLastOperationResult );
            return gLastOperationResult;
        }

        //Set the cipher suite:
        //AT+USECPRF=0,2,100,"c0","30",0
        gLastOperationResult = uCellSecTlsCipherSuiteAdd( pContext, U_SECURITY_TLS_CIPHER_SUITE_ECDHE_RSA_WITH_AES_256_GCM_SHA384 );
        if(gLastOperationResult != X_ERR_SUCCESS){
            mqttSnErrorHandle( gLastOperationResult );
            return gLastOperationResult;
        }

        //Set the  client key and certificate to be used (as saved in the module):
        //AT+USECPRF=0,5,"cert"
        gLastOperationResult = uCellSecTlsClientCertificateNameSet(pContext,"MQTTFlex_cert");
        if(gLastOperationResult != X_ERR_SUCCESS){
            mqttSnErrorHandle( gLastOperationResult );
            return gLastOperationResult;
        }

        //AT+USECPRF=0,6,"key"
        gLastOperationResult = uCellSecTlsClientPrivateKeyNameSet(pContext, "MQTTFlex_key", NULL);
        if(gLastOperationResult != X_ERR_SUCCESS){
            mqttSnErrorHandle( gLastOperationResult );
            return gLastOperationResult;
        }

        //Configure the MQTT-SN  client to use your MQTT Flex thing with the security profile 

        //Set the  client ID (copy from Thingstream portal):
        //AT+UMQTTSN=0,"device:262a43d2-8364-4b02-9ed6-49701bbf4872"
        uAtClientLock(gAtHandle);
        uAtClientCommandStart(gAtHandle, "AT+UMQTTSN=");
        uAtClientWriteInt(gAtHandle, 0);
        uAtClientWriteString(gAtHandle, gMqttSnActiveConfigCopy.clientID,true);
        uAtClientCommandStopReadResponse(gAtHandle);
        if (uAtClientUnlock(gAtHandle) != 0) {
           LOG_ERR("Set Client ID error \r\n");
           mqttSnErrorHandle(X_ERR_AT_CMD );
           return X_ERR_AT_CMD;
        }

        //Configure the MQTT-SN gateway endpoint:
        //AT+UMQTTSN=1,"mqtt-flex.thingstream.io",2443
        uAtClientLock(gAtHandle);
        uAtClientCommandStart(gAtHandle, "AT+UMQTTSN=");
        uAtClientWriteInt(gAtHandle, 1);
        uAtClientWriteString(gAtHandle, MQTTSN_FLEX_BROKER_HOSTNAME,true);
        uAtClientWriteInt(gAtHandle, MQTTSN_FLEX_BROKER_PORT);
        uAtClientCommandStopReadResponse(gAtHandle);
        if (uAtClientUnlock(gAtHandle) != 0) {
           LOG_ERR("Configure MQTT-SN gateway error \r\n");
           mqttSnErrorHandle( X_ERR_AT_CMD );
           return X_ERR_AT_CMD;
        }

        //Set the client to use the required DTLS profile: 
        //AT+UMQTTSN=9,1,0
        uAtClientLock(gAtHandle);
        uAtClientCommandStart(gAtHandle, "AT+UMQTTSN=");
        uAtClientWriteInt(gAtHandle, 9);
        uAtClientWriteInt(gAtHandle, 1);
        uAtClientWriteInt(gAtHandle, 0);
        uAtClientCommandStopReadResponse(gAtHandle);
        if (uAtClientUnlock(gAtHandle) != 0) {
           LOG_ERR("Set client DTLS profile error \r\n");
           mqttSnErrorHandle( X_ERR_AT_CMD );
           return X_ERR_AT_CMD;
        }

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

        // obtain network handle for SARA-R5
        gNetHandle =  xCellSaraGetHandle();

        // this check should not be necessary as if the network handle is not valid
        // this would create an error in the SARA config function
        if( gNetHandle < 0 ){
            LOG_ERR("Cannot obtain network handle from cell module, Error Code: %d\r\n", gNetHandle);
            mqttSnErrorHandle( gNetHandle ); //error code
            continue;
        }

        //optical indication
        xLedFade( MQTTSN_ACTIVATING_LEDCOL, MQTTSN_ACTIVATING_LED_DELAY_ON, MQTTSN_ACTIVATING_LED_DELAY_OFF, 0 );

        // obtain a handle for use with AT client
        uCellAtClientHandleGet( gNetHandle, &gAtHandle );

        // Setup MQTT-SN client for MQTT Flex
        if( xCellSaraGetActiveMqttPlan() == FLEX ){
            if( mqttSnConfigFlexPlan() != X_ERR_SUCCESS ){
                continue;
            }
        }

        // Setup MQTT-SN client for MQTT Anywhere
        else if( xCellSaraGetActiveMqttPlan() == ANYWHERE ){
            if( mqttSnConfigAnywherePlan() != X_ERR_SUCCESS ){
                continue;
            }
        }

        LOG_INF("Client opened \r\n");
        gMqttSnStatus.status = ClientOpen;
        xLedOff();
    }
}


void xCellMqttSnClientConnectThread(void){

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

        // reset cb counter 
        gCbCallsCnt = 0;

        LOG_INF("Connecting to MQTTSN broker \r\n");

        //# Connect to broker
        // AT+UMQTTSNC=1
        uAtClientLock(gAtHandle);
        uAtClientCommandStart(gAtHandle, "AT+UMQTTSNC=");
        uAtClientWriteInt(gAtHandle, 1);
        uAtClientCommandStopReadResponse(gAtHandle);
        if (uAtClientUnlock(gAtHandle) != 0) {
           LOG_ERR("Error Sending Connect to broker Command \r\n");
           mqttSnErrorHandle( X_ERR_AT_CMD );
           continue;
        }

        uAtClientSetUrcHandler(gAtHandle, "+UUMQTTSNC:", mqttSnClientCb, NULL);

        while( gCbCallsCnt == 0 ){
            //printk(".");
            k_sleep(K_MSEC(500));   //no need for timeout? URC response is sent at timeout from SARA module
        }


        if( gCbParamY == 0 ){      // failed to connect
            //read mqtt-sn connection error
           int x,y;
           uAtClientLock(gAtHandle);
           uAtClientCommandStart(gAtHandle, "AT+UMQTTSNER");
           uAtClientCommandStop(gAtHandle);
           uAtClientResponseStart(gAtHandle, "+UMQTTSNER:");
           x = uAtClientReadInt(gAtHandle);              
           y = uAtClientReadInt(gAtHandle); 
           //uAtClientCommandStopReadResponse(gAtHandle);
           uAtClientResponseStop(gAtHandle);
           if (uAtClientUnlock(gAtHandle) != 0) {
                LOG_ERR("Error while sending get error AT command \r\n");
                mqttSnErrorHandle( X_ERR_AT_CMD );
                continue;
           }

           LOG_WRN("MQTT connection error code(see SARA-R5 series AT commands manual): %d, %d\r\n", x,y); 
           mqttSnErrorHandle( X_ERR_MQTTSN_CON ); //special error code
           continue;

        }

        else if( gCbParamY == 1 ){   //successful connection to mqtt-sn

           LOG_INF("MQTT Connected \r\n"); 
           gMqttSnStatus.status = ClientConnected;
        }

        xLedOff();
    }
}


static int32_t mqttSnRegisterTopic(const char *pTopicNameStr){

    int32_t  op_code, MQTTSN_result, topic_id;
    gCbCallsCnt=0;
    
    // Register a topic
    // Before you can publish to a  normal topic, you need to register with with the broker and get back a topic ID. 
    //AT+UMQTTSNC=2,"my/publish/topic"
    uAtClientLock(gAtHandle);
    uAtClientCommandStart(gAtHandle, "AT+UMQTTSNC=");
    uAtClientWriteInt(gAtHandle, 2);
    uAtClientWriteString(gAtHandle, pTopicNameStr,true);
    uAtClientCommandStop(gAtHandle);
    uAtClientCommandStopReadResponse(gAtHandle);
    if ( uAtClientUnlock(gAtHandle) != 0 ) {
        LOG_ERR("Register topic command error\r\n");
        mqttSnErrorHandle( X_ERR_AT_CMD ); //?
        return X_ERR_AT_CMD;
    }
   

      // Expect +UUMQTTSNC: 2,1,1 as a response to confirm the registration.  
   // The final argument in the response is the topic ID to use when you publish.
   // the response is handled by the uAtClientSetUrcHandler which handles "+UUMQTTSNC:" responses
   // and was set during the connect to mqtt function call

    while( gCbCallsCnt == 0 ){
        //printk(".");
        k_sleep(K_MSEC(500));
    }

    op_code = gCbParamX;   
    MQTTSN_result = gCbParamY;
    topic_id = gCbParamZ;

    // if the URC received has operation code (op_code) 2, then it refers
    // to topic registration (and not another URC code)
    if( ( op_code == 2 ) && ( MQTTSN_result == 1 ) ){
        //success
        return topic_id;
    }

    else{
        
        LOG_ERR("Register Topic failure\r\n");
        
        int x,y;
        uAtClientLock(gAtHandle);
        uAtClientCommandStart(gAtHandle, "AT+UMQTTSNER");
        uAtClientCommandStop(gAtHandle);
        uAtClientResponseStart(gAtHandle, "+UMQTTSNER:");
        x = uAtClientReadInt(gAtHandle);              
        y = uAtClientReadInt(gAtHandle); 
        //uAtClientCommandStopReadResponse(gAtHandle);
        uAtClientResponseStop(gAtHandle);
        if (uAtClientUnlock(gAtHandle) != 0) {
             LOG_ERR("Error while sending get error AT command \r\n");
             mqttSnErrorHandle( X_ERR_AT_CMD );
             return X_ERR_AT_CMD;
        }

        LOG_WRN("MQTT Register topic error: %d, %d\r\n", x,y);
        mqttSnErrorHandle( X_ERR_UNKNOWN );
        return X_ERR_UNKNOWN;
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

    err_code ret = X_ERR_SUCCESS;
    if( gMqttSnStatus.status == ClientConnected ){
        LOG_WRN("Client Connected. Trying to disconnect now \r\n");
        ret = xCellMqttSnClientDisconnect();
    }

    LOG_INF("MQTTSN client closed \r\n");
    gMqttSnStatus.status = ClientClosed; //no need to deconfigure something
    return ret;
}



err_code xCellMqttSnClientDisconnect(void){

    // Set optical indication
    xLedFade(MQTTSN_DEACTIVATING_LEDCOL,MQTTSN_ACTIVATING_LED_DELAY_ON,MQTTSN_ACTIVATING_LED_DELAY_OFF,0);

   // reset cb counter 
    gCbCallsCnt = 0;

   // Send disconnect command
   uAtClientLock(gAtHandle);
   uAtClientCommandStart(gAtHandle, "AT+UMQTTSNC=");
   uAtClientWriteInt(gAtHandle, 0);
   uAtClientCommandStopReadResponse(gAtHandle);
   if ( uAtClientUnlock(gAtHandle) != 0 ) {
       LOG_ERR("MQTT SN Disconnection command error \r\n");
       mqttSnErrorHandle( X_ERR_AT_CMD );
       return X_ERR_AT_CMD;
   }
   
   //uAtClientCommandStop(gAtHandle);
    while( gCbCallsCnt == 0 ){
        //printk(".");
        //k_sleep(K_MSEC(500)); //no need for timeout?URC response is sent at timeout from SARA module
    }

    if( gCbParamY == 0 ){      // failed to disconnect
        //read mqtt-sn connection error
       int x,y;
       uAtClientLock(gAtHandle);
       uAtClientCommandStart(gAtHandle, "AT+UMQTTSNER");
       uAtClientCommandStop(gAtHandle);
       uAtClientResponseStart(gAtHandle, "+UMQTTSNER:");
       x = uAtClientReadInt(gAtHandle);              
       y = uAtClientReadInt(gAtHandle); 
       //uAtClientCommandStopReadResponse(gAtHandle);
       uAtClientResponseStop(gAtHandle);
       if (uAtClientUnlock(gAtHandle) != 0) {
             LOG_ERR("Error while sending get error AT command \r\n");
             mqttSnErrorHandle( X_ERR_AT_CMD );
             return X_ERR_AT_CMD;
       }

       LOG_WRN("MQTT-SN disconnection error code(see SARA-R5 series AT commands manual): %d, %d\r\n", x,y); 
       return y;
    }

   LOG_INF("MQTT-SN Disconnected\r\n"); 

   gMqttSnStatus.status = ClientOpen;
   xLedOff();
   return X_ERR_SUCCESS;
   
}



err_code xCellMqttSnClientPublish( uMqttSNTopicType_t topic_type,
    const char *pTopicNameStr,
    const char *pMessage,
    size_t messageSizeBytes,
    uMqttSNQos_t qos,
    bool retain){

    int32_t topic_id;
    int32_t  op_code, MQTTSN_result;
    char topic_id_str[MMQTTNS_TOPIC_MAXLEN];

    if( ( topic_type >= MQTTSN_TOPIC_MAX_NUM ) || ( topic_type < 0 ) ){
        // invalid parameter error
        mqttSnErrorHandle( X_ERR_INVALID_PARAMETER );
        return X_ERR_INVALID_PARAMETER;
    }

    // cannot publish in normal topic type with QOS=3
    if( topic_type == MQTTSN_TOPIC_NORMAL ){
        if( qos == U_MQTTSN_QOS_SPECIAL ){
            //invalid parameters error
            mqttSnErrorHandle( X_ERR_INVALID_PARAMETER );
            return X_ERR_INVALID_PARAMETER;
        }
    }

    // short topic type supports a topic name up to 2 characters long
    if( topic_type == MQTTSN_TOPIC_SHORT ){
        if( strlen(pTopicNameStr) > 2 ){
            mqttSnErrorHandle( X_ERR_INVALID_PARAMETER );
            return X_ERR_INVALID_PARAMETER; 
        }
    }

    // pTopicNameStr should be a number string when predefined topic type is used
    if( topic_type == MQTTSN_TOPIC_PREDEFINED ){
        int len = strlen(pTopicNameStr);
        if(len==0){
            mqttSnErrorHandle( X_ERR_INVALID_PARAMETER );
            return X_ERR_INVALID_PARAMETER; 
        }
        for(int x=0; x<len; x++){
            if( !isdigit( pTopicNameStr[x] ) ){
                mqttSnErrorHandle( X_ERR_INVALID_PARAMETER );
                return X_ERR_INVALID_PARAMETER; 
            }
        }
    }

   // When normal Topic Type is used the topic should be registered prior to publishing
   if( topic_type == MQTTSN_TOPIC_NORMAL){
        topic_id = mqttSnRegisterTopic(pTopicNameStr);
        if(topic_id >= 0 ){
            memset(topic_id_str,0,sizeof(topic_id_str));
            sprintf(topic_id_str,"%d",topic_id);
            //LOG_INF("Topic Id: %s\r\n",topic_id_str);
        }
        else{
            mqttSnErrorHandle( X_ERR_UNKNOWN );
            return X_ERR_UNKNOWN; //failure
        } 
    }
    else{
        memset(topic_id_str,0,sizeof(topic_id_str));
        strcpy(topic_id_str,pTopicNameStr);
        //LOG_INF("Topic Id: %s\r\n",topic_id_str);
    }

    // Publish the message
    gCbCallsCnt =0;
    uAtClientLock(gAtHandle);
    uAtClientCommandStart(gAtHandle, "AT+UMQTTSNC=");
    uAtClientWriteInt(gAtHandle, 4);
    uAtClientWriteInt(gAtHandle, qos);
    uAtClientWriteInt(gAtHandle, (int32_t)retain ); 
    uAtClientWriteInt(gAtHandle, 0); //only string publish supported in this function
    uAtClientWriteInt(gAtHandle, (int)topic_type); //1= predefined? 0 = normal, 2 = short
    uAtClientWriteString(gAtHandle, topic_id_str,true);
    uAtClientWriteString(gAtHandle, pMessage,true);

    uAtClientCommandStopReadResponse(gAtHandle);
    if (uAtClientUnlock(gAtHandle) != 0) {
       LOG_ERR("Send Message error \r\n");
       mqttSnErrorHandle( X_ERR_AT_CMD );
       return X_ERR_AT_CMD; //failure
    }

    //wait for publish
    while( gCbCallsCnt == 0 ){
        //printk(".");
        k_sleep(K_MSEC(100));
    }

    op_code = gCbParamX;   
    MQTTSN_result = gCbParamY;
    
    // URC for publish:   +UUMQTTSNC: 4,<MQTTSN_result> 
    //(see AT command manual for SARA-R5: SARA-R5_ATCommands_UBX-19047455)
    if( ( op_code == 4 ) && ( MQTTSN_result == 1 ) ){
        //success
        return X_ERR_SUCCESS;
    }

    else{
        LOG_ERR("Publish failed\r\n");

        int x,y;
        uAtClientLock(gAtHandle);
        uAtClientCommandStart(gAtHandle, "AT+UMQTTSNER");
        uAtClientCommandStop(gAtHandle);
        uAtClientResponseStart(gAtHandle, "+UMQTTSNER:");
        x = uAtClientReadInt(gAtHandle);              
        y = uAtClientReadInt(gAtHandle); 
        //uAtClientCommandStopReadResponse(gAtHandle);
        uAtClientResponseStop(gAtHandle);
        if (uAtClientUnlock(gAtHandle) != 0) {
             LOG_ERR("Error while sending get error AT command \r\n");
             mqttSnErrorHandle( X_ERR_AT_CMD );
             return X_ERR_AT_CMD; 
        }

        LOG_WRN("MQTT publish error: %d, %d\r\n", x,y); 
        mqttSnErrorHandle( X_ERR_UNKNOWN );
        return X_ERR_UNKNOWN;  //error code
    }
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
            if( (qos > U_MQTTSN_QOS_MAX_NUM) || (qos < 0) ){
                shell_print(shell, "Invalid QOS\r\n");  
                return;
            }

            //check connection if needed
            if( qos < U_MQTTSN_QOS_SPECIAL ){
                // a connection is required
                if( gMqttSnStatus.status < ClientConnected){
                    shell_print(shell, "MQTT-SN not connected, abort action (only QOS=3 can be used without connection)\r\n");  
                    return;
                }
            }

            // check topic type string and get enum
            uint8_t topic_type_enum_idx=0;
            bool topic_type_found = false;
            for( topic_type_enum_idx = 0; topic_type_enum_idx < MQTTSN_TOPIC_MAX_NUM; topic_type_enum_idx++ ){
                if( strcmp(argv[1],uMqttSNTopicType_strings[ topic_type_enum_idx ] ) == 0 ){
                    topic_type_found = true;
                    break;
                }
            }
            
            if( !topic_type_found ){
                shell_print(shell, "Invalid topic type\r\n");  
                return;
            }

            shell_print(shell, "Sending ""%s"" to topic ""%s"" with QoS: %d\r\n", argv[3], argv[2], (uint8_t)qos );

            int32_t err = xCellMqttSnClientPublish(topic_type_enum_idx, argv[2], argv[3], strlen(argv[3]), qos, 0);  //0 is the retain
            if(err != 0){
                shell_print(shell, "nuMqttClientPublish failed: %d\r\n", err );
            }
            else{
                shell_print(shell, "Published\r\n" );
            }
        }

		return;
}