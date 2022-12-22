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
 * @brief This file contains the function implementation of SARA-R5
 * cellular module used in Sensor Aggregation use case (XPLR-IOT-1) 
 */

#include "x_cell_saraR5.h"

#include "stdio.h"
#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

// Zephyr/SDK related
#include <zephyr.h>
#include <hal/nrf_gpio.h>
#include <logging/log.h>

//ubxlib related
//#include "ubxlib.h" --> included via the header .h file

//Sensor aggregation related
#include "x_logging.h"
#include "x_module_common.h"
#include "x_system_conf.h"
#include "x_module_common.h"
#include "x_cell_mqttsn.h"
#include "x_led.h"
#include "x_wifi_ninaW156.h"


/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION
 * -------------------------------------------------------------- */

/** Thread called by xCellSaraInit to configure/initialize the SARA-R5 module */
void xCellSaraInitThread(void);

/** Thread called by xCellSaraConnect to connect SARA-R5 module to a network*/
void xCellSaraConnectThread(void);

/** Thread called by xCellSaraDeinit to disconect, deinitialize and power off
 *  SARA-R5 module
 */
void xCellSaraDeinitThread(void);

/** Function called to configure the cellular module depending on the 
 * active Thingstream plan (Flex, Anywhere). This is supposed to be tweaked
 * by the user, especially in the case of Flex plan.
 *
 * @return        zero on success else negative error code.
 */
static int32_t xCellSaraRegistrationConfig(void);


/** Handle error happening in a thread
*/
static void saraErrorHandle(err_code err_code);

/** Disconnect from network
*/
static void saraDisconnect(void);

/** Callback triggered on changes of connection status
*/
static void saraConnectCb(bool isConnected, void *pParameter);

/** Callback triggered on changes of registration status
*/
static void saraRegisterCb(uCellNetRegDomain_t domain, uCellNetStatus_t reg_status, void *pParameter);


/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

LOG_MODULE_REGISTER(LOGMOD_NAME_SARAR5, LOG_LEVEL_DBG);

// Semamphores definition
K_SEM_DEFINE(xCellSaraInit_semaphore, 0, 1);
K_SEM_DEFINE(xCellSaraConnect_semaphore, 0, 1);
K_SEM_DEFINE(xCellSaraDeinit_semaphore, 0, 1);


// Threads definition
K_THREAD_DEFINE(xCellSaraInitThreadId, SARAR5_STACK_SIZE, xCellSaraInitThread, NULL, NULL, NULL,
		SARAR5_PRIORITY, 0, 0);

K_THREAD_DEFINE(xCellSaraConnectThreadId, SARAR5_STACK_SIZE, xCellSaraConnectThread, NULL, NULL, NULL,
		SARAR5_PRIORITY, 0, 0);

K_THREAD_DEFINE(xCellSaraDeinitThreadId, SARAR5_STACK_SIZE, xCellSaraDeinitThread, NULL, NULL, NULL,
		SARAR5_PRIORITY, 0, 0);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/** Holds SARA status (as regards Sensor Aggregation application)*/
xCellSaraStatus_t gSaraStatus={
    .pinsConfigured = false,
    .poweredUp = false,
    .uStatus = uPortNotInitialized,
    .isRegistered = false,
    .isConnected = false,
    .isReadyToConnect = false,
    .isReadyForMqttSN = false
};


/** Holds the result of the last operation performed by this module (refers to operations
 *  that need to report their result to other modules of the application and cannot report
 *  their results directly e.g. a thread operation) */
static err_code gLastOperationResult = X_ERR_SUCCESS;


/** This holds which connection plan is active:
 * - MQTT Anywhere: Connect using a Thingstream SIM card
 * - MQTT Flex: Connect using a third party SIM card
 */
xCellMqttSnPlan_t gMqttSnActivePlan = MQTTSN_DEFAULT_PLAN;


/** Device Handle returned and used by ubxlib functions */
static uDeviceHandle_t gDevHandle = NULL;


/* ----------------------------------------------------------------
 * CALLBACKS
 * -------------------------------------------------------------- */

static void saraConnectCb(bool isConnected, void *pParameter){
    gSaraStatus.isConnected = isConnected;
     if(gSaraStatus.isConnected){
            LOG_INF("Cell connected \r\n");
    } 
    else{
            LOG_WRN("Cell disconnected \r\n");
    }
}



static void saraRegisterCb(uCellNetRegDomain_t domain, uCellNetStatus_t reg_status, void *pParameter){
    
    if( reg_status <= U_CELL_NET_STATUS_NOT_REGISTERED ){
        
        gSaraStatus.isRegistered = false;
        LOG_WRN("Cell not Registered \r\n");
    }
    else{
        gSaraStatus.isRegistered = true;
        LOG_INF("Cell Registered \r\n");
    }
}


/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


static void saraErrorHandle(err_code err_code){
    gLastOperationResult = err_code;
    xLedBlink( ERROR_LEDCOL, ERROR_LED_DELAY_ON, ERROR_LED_DELAY_OFF, ERROR_LED_BLINKS);
}


static int32_t xCellSaraRegistrationConfig(void){

    int32_t ret;

    // if uDeviceOpen has not been called yet, we cannot use AtClient to send AT commands
    if( gSaraStatus.uStatus < uDeviceOpened ){
        LOG_WRN("Configuration prior Connection to MNO: Initialize first (use init command)\r\n");
        return X_ERR_INVALID_STATE; //err code invalid state
    }

    // The setting in this function, cannot be set when the module is connected/register to MNO
    if( gSaraStatus.isRegistered || gSaraStatus.isConnected ){
        LOG_WRN("Configuration prior Connection to MNO: Disconnect(deinit) and initialize again (init)\r\n");
        return X_ERR_INVALID_STATE; //err_code invalid state
    }

    // two situations have been programmed:
    // - One when using a Thingstream card
    // - Another one for using a 1nce SIM card over an NB-IoT newtork. 
    // The user may change this section according to his needs.
    // At this point only the URAT configuration is changed, between the two plans

    if( gMqttSnActivePlan == FLEX){
        
        // just check the URAT configuration (if its NB-IoT)
        uCellNetRat_t rat = uCellCfgGetRat(gDevHandle, 0);
        if( rat == U_CELL_NET_RAT_NB1 ){
            LOG_INF("Urat is ok: %d", rat);
        }
        else{
            LOG_INF("Urat is not ok: %d", rat);
            // will change urat to NB-IoT
            LOG_INF("Setting urat to: %d", U_CELL_NET_RAT_NB1);
            ret = uCellCfgSetRat(gDevHandle, U_CELL_NET_RAT_NB1);

            if( ret == 0 ){
                LOG_INF("Rebooting module \r\n");
                ret = uCellPwrReboot( gDevHandle, NULL );
                if(ret<0){
                    return ret;
                }
                LOG_INF("Module restarted\r\n");
            }
            else if(ret<0){
                return ret;
            }
        }
    }


    if( gMqttSnActivePlan == ANYWHERE ){
        // according to thingstream documentation for MQTT Anywhere URAT should be <LTE Cat M1> or <GPRS>
        // (this is implied by command: AT+URAT=9,7)
        // in SARA-R5 only AT+URAT=9 is NOT supported, so we set it to 7 <LTE Cat M1>
        
        uCellNetRat_t rat = uCellCfgGetRat(gDevHandle, 0);

        if( rat == U_CELL_NET_RAT_CATM1 ){
            LOG_INF("Urat is ok: %d", rat);
        }
        else{
            LOG_INF("Urat is not ok: %d", rat);
            // will change urat to NB-IoT
            LOG_INF("Setting urat to: %d", U_CELL_NET_RAT_CATM1);
            ret = uCellCfgSetRat(gDevHandle, U_CELL_NET_RAT_CATM1);

            if( ret == 0 ){
                LOG_INF("Rebooting module \r\n");
                ret = uCellPwrReboot( gDevHandle, NULL);
                if(ret<0){
                    return ret;
                }
                LOG_INF("Module restarted\r\n");
            }
            else if(ret<0){
                return ret;
            }
        }
    }

    return 0;

}



void xCellSaraInitThread(void){

    err_code err;

    // Cellular device configuration to work with ubxlib
    static const uDeviceCfg_t deviceCfg = {
        .deviceType = U_DEVICE_TYPE_CELL,
        .deviceCfg = {
            .cfgSho = {
                .moduleType = U_CELL_MODULE_TYPE_SARA_R5
            }
        },
        .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
        .transportCfg = {
            .cfgUart = {
                .uart = SARA_UART,
                .baudRate = SARA_UART_BAUDRATE,
                .pinTxd = -1,  
                .pinRxd = -1,
                .pinCts = -1,
                .pinRts = -1
            }
        }
    };
    

    // needed to avoid thread overflows when using ubxlib functions within a thread
    k_thread_system_pool_assign(k_current_get());

    while(1){
        // Semaphore given by xCellSaraInit()
        k_sem_take( &xCellSaraInit_semaphore, K_FOREVER );

        LOG_DBG("SARAR5 device init request \r\n");

        // The initialization thread ends up in a uDeviceOpened status
        // if the module status is already there, no need to init again
        if( gSaraStatus.uStatus == uDeviceOpened ){
            LOG_INF("Already Initialized\r\n");
            gLastOperationResult = X_ERR_SUCCESS;
            continue;
        }

        // if WiFi is using ubxlib, abort 
        //( Wifi should be deinitiliazed fisrt )
        if( xCommonUPortIsInit() ){
            xWifiNinaStatus_t ninaStatus = xWifiNinaGetModuleStatus();
            if( ninaStatus.uStatus >= uPortInitialized ){
                // deinitialize previous uPort to config SARA
                LOG_ERR("uPort is used by WiFi\r\n");
                saraErrorHandle( X_ERR_INVALID_STATE ); 
                continue;
            }
        }

        //optical indication
        xLedFade(CELL_ACTIVATING_LEDCOL, CELL_ACTIVATING_LED_DELAY_ON, CELL_ACTIVATING_LED_DELAY_OFF, 0);

        // Power On SARA if necessary
        if(!gSaraStatus.poweredUp){
            xCellSaraPowerOn();
        }

        // Configure uart
        xCommonUartCfg(COMMON_UART_SARA);

        // Initialize ubxlib for use with Zephyr
        if( !xCommonUPortIsInit() ){
            gLastOperationResult = xCommonUPortInit();
            if( gLastOperationResult != X_ERR_SUCCESS ){
                LOG_ERR("Could not initialize SARA \r\n");
                saraErrorHandle( gLastOperationResult );
                continue;
            }
        }

        gSaraStatus.uStatus = uPortInitialized;

        // Initialize Device API in ubxlib
        if( ( err = uDeviceInit() ) != X_ERR_SUCCESS ){ 
            LOG_ERR("SARAR5 uDeviceInit failed\n");
            saraErrorHandle( err ); 
            continue;
        }

        gSaraStatus.uStatus = uDeviceApiInitialized;

        // Open Device and keep a copy of the credentials used
        err = uDeviceOpen( &deviceCfg, &gDevHandle);

        // If device could not be opened
        if( err < 0){
            LOG_ERR("Could not Open Cellular Device\r\n");
            gDevHandle = NULL;
            saraErrorHandle( err );
            continue;
        }

        // Device succesfully opened
        gSaraStatus.uStatus = uDeviceOpened;
        LOG_INF("Cellular device and opened\r\n");

        gSaraStatus.isReadyToConnect = true;
        gLastOperationResult = X_ERR_SUCCESS;
        xLedOff();         
    }
}



void xCellSaraDeinitThread(void){
    
    while(1){
        // Semaphore given by xCellSaraDeinit()
        k_sem_take( &xCellSaraDeinit_semaphore, K_FOREVER );

        LOG_INF("SARA deinit request\r\n");

        xCommonUPortDeinit();
        
        // Wait for deinitialization to complete
        while( xCommonUPortIsInit() ){
            k_sleep(K_MSEC(500));
        }

        LOG_INF("Module Deinitialized\r\n");            
        gSaraStatus.uStatus = uPortNotInitialized;

        // already off at this point (powered down by ubxlib)
        xCellSaraPowerOff();
    }
}



void xCellSaraConnectThread(void){

    // Structure that contains the connection parameters/configuration
    // when user wants to connect to MQTT Anywhere using a Thingstream SIM
    // card
    static const uNetworkCfgCell_t gNetworkCfgAnywhere = {
            .type = U_NETWORK_TYPE_CELL,
            .pApn = CELL_APN_ANYWHERE,
            .timeoutSeconds = CELL_CONNECTION_TIMEOUT
    };

    // Structure that contains the connection parameters/configuration
    // when user wants to connect to MQTT Flex using a 3rd party SIM
    // card
    static const uNetworkCfgCell_t gNetworkCfgFlex = {
            .type = U_NETWORK_TYPE_CELL,
            .pApn = CELL_APN_FLEX,
            .timeoutSeconds = CELL_CONNECTION_TIMEOUT
    };


    while(1){

        // Semaphore given by xCellSaraConnect
        k_sem_take( &xCellSaraConnect_semaphore, K_FOREVER );

        // if not ready to connect, prepare module for connection request (open cell device
        // in ubxlib)
        gLastOperationResult = X_ERR_SUCCESS;
        if( !gSaraStatus.isReadyToConnect ){
            xCellSaraInit();
            while( ( !gSaraStatus.isReadyToConnect ) && ( gLastOperationResult == X_ERR_SUCCESS ) ){
                k_sleep(K_MSEC(1000));
            }
            if( gLastOperationResult != X_ERR_SUCCESS ){
                continue;
            }
        }

        // optical indication
        xLedFade(CELL_ACTIVATING_LEDCOL, CELL_ACTIVATING_LED_DELAY_ON, CELL_ACTIVATING_LED_DELAY_OFF, 0);

        // Cellular Configuration that might be needed before connecting to a
        // mobile network operator --> such as setting URAT configuration...
        gLastOperationResult = xCellSaraRegistrationConfig();
        if( gLastOperationResult != X_ERR_SUCCESS ){
            saraErrorHandle( gLastOperationResult );
            continue;
        }
        
        //register callbacks
        uCellNetSetRegistrationStatusCallback(gDevHandle,saraRegisterCb,NULL);
        uCellNetSetBaseStationConnectionStatusCallback(gDevHandle, saraConnectCb,NULL);

        // Bring up the network layer
        LOG_INF("Connecting to operator/network \r\n");
        
        if( gMqttSnActivePlan == FLEX ){
            gLastOperationResult = uNetworkInterfaceUp( gDevHandle, U_NETWORK_TYPE_CELL, &gNetworkCfgFlex );
        } 
        else if( gMqttSnActivePlan == ANYWHERE ){
            gLastOperationResult = uNetworkInterfaceUp( gDevHandle, U_NETWORK_TYPE_CELL, &gNetworkCfgAnywhere );
        }
        
        // Check for errors
        if( gLastOperationResult != X_ERR_SUCCESS ){
            LOG_WRN("Connect to network err code: %d \r\n", gLastOperationResult );    
            saraErrorHandle( gLastOperationResult );
            continue;
        }
        
        LOG_INF("Connected to network \r\n");
        gSaraStatus.isReadyForMqttSN = true;

        xLedOff();
    }
}



static void saraDisconnect(void){

    LOG_INF("Cell Network Down request\r\n");

    // optical indication
    xLedFade(CELL_DEACTIVATING_LEDCOL, CELL_ACTIVATING_LED_DELAY_ON, CELL_ACTIVATING_LED_DELAY_OFF, 0);

    gLastOperationResult = uNetworkInterfaceDown( gDevHandle, U_NETWORK_TYPE_CELL );
    if(gLastOperationResult != U_ERROR_COMMON_SUCCESS){
        LOG_ERR("uNetworkInterfaceDown error: %d \r\n", gLastOperationResult);
    }

    LOG_INF("Cell Network Down\r\n" );
    gSaraStatus.isConnected = false;
    gSaraStatus.isRegistered = false;
    gSaraStatus.isReadyForMqttSN = false;

    xLedOff();
}



/* ----------------------------------------------------------------
 * PUBLIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


void xCellSaraActivateMqttFlexPlan(void){
    if( gSaraStatus.uStatus > uPortNotInitialized ){
        LOG_ERR("Cannot Change Plan when module is initialized. SARAR5 must be deinitialized fisrt \r\n");
        return;
    }
    gMqttSnActivePlan = FLEX;
}



void xCellSaraActivateMqttAnywherePlan(void){
    if( gSaraStatus.uStatus > uPortNotInitialized ){
        LOG_ERR("Cannot Change Plan when module is initialized. SARAR5 must be deinitialized fisrt \r\n");
        return;
    }
    gMqttSnActivePlan = ANYWHERE;
}



xCellMqttSnPlan_t xCellSaraGetActiveMqttPlan(void){
    return gMqttSnActivePlan;
}



xCellSaraStatus_t xCellSaraGetModuleStatus(){
    return  gSaraStatus;
}



void xCellSaraConfigPins(void){

    nrf_gpio_cfg_output((uint32_t) NORA_EN_SARA_PIN);
    nrf_gpio_cfg_output((uint32_t) SARA_PWR_ON_PIN);
    nrf_gpio_cfg_output((uint32_t) SARA_RST_PIN);
    nrf_gpio_cfg_output((uint32_t) SARA_INT_PIN);

    nrf_gpio_pin_clear((uint32_t) SARA_INT_PIN);

    gSaraStatus.pinsConfigured = true;
    LOG_INF("ouput pins configured \r\n");

}



void xCellSaraPowerOn(void)
{
    if(gSaraStatus.poweredUp){
        LOG_INF("Already powered up\r\n");
        return; //success
    }

    if( !gSaraStatus.pinsConfigured ){
        xCellSaraConfigPins();
    }
    
    // Power on sequence
    // Apply power to the module and wait a little time
    nrf_gpio_pin_set((uint32_t) NORA_EN_SARA_PIN);
    k_sleep(K_MSEC(100));
    
    //assert POWER_ON pin for a valid time
    nrf_gpio_pin_set((uint32_t) SARA_PWR_ON_PIN);
    k_sleep(K_MSEC(1200));
    //deassert POWER_ON pin
    nrf_gpio_pin_clear((uint32_t) SARA_PWR_ON_PIN);

    gSaraStatus.poweredUp = true;
    LOG_INF("Module powered up\r\n");
    
}



err_code xCellSaraPowerOff(void){

    if(!gSaraStatus.pinsConfigured){
        xCellSaraConfigPins();
    }

    if(!gSaraStatus.poweredUp){
        LOG_INF("Already powered down\r\n");
        return X_ERR_SUCCESS;
    }

    if( gSaraStatus.uStatus > uPortNotInitialized ){
        LOG_ERR("Module network is initialized. Cannot power down. Use deinit command\r\n");
        return X_ERR_INVALID_STATE; 
    }
    
    nrf_gpio_pin_clear((uint32_t) SARA_PWR_ON_PIN);
    nrf_gpio_pin_clear((uint32_t) NORA_EN_SARA_PIN);
    LOG_INF("Module powered down\r\n");
    
    gSaraStatus.poweredUp = false;
    return X_ERR_SUCCESS;
}



void xCellSaraInit(void){
    k_sem_give( &xCellSaraInit_semaphore );
}



void xCellSaraConnect(void){
    k_sem_give( &xCellSaraConnect_semaphore );
}


void xCellSaraDeinit(void){
    k_sem_give( &xCellSaraDeinit_semaphore );
}


void xCellSaraDeviceClose(void){

    LOG_INF("SARA Device close request\r\n");

    // check Mqtt-SN status and close if necessary prior to
    // deinitializing the network
    xClientStatus_t mqttsn_stat = xCellMqttSnClientGetStatus();
    if( mqttsn_stat > ClientClosed ){
        
        xCellMqttSnClientClose();
        // wait for Mqtt-SN to close
        while( mqttsn_stat > ClientClosed ){
            k_sleep( K_MSEC( 1000 ) );
            mqttsn_stat = xCellMqttSnClientGetStatus();
            // we do not check for errors here, continue anyway
        }
    }

    // if network is up (connected)
    if( gSaraStatus.isReadyForMqttSN ){    
        LOG_WRN("SARA Network is up. Brigning Network Down now\r\n");
        saraDisconnect();
    }

    while( gSaraStatus.isReadyForMqttSN ){
        //wait for disconnection    
    }

    // Send command to gracefully power off SARA module. Need to call this
    // before closing the Device with uDeviceClose()
    if( uCellPwrOff(gDevHandle,NULL) < U_ERROR_COMMON_SUCCESS ){
        LOG_WRN("Error in uCellPwrOff: Continue anyway \r\n");
    }

    // Shut-down the cellular device
    uDeviceClose( gDevHandle, false );
    uDeviceDeinit();

    xLedFade(CELL_DEACTIVATING_LEDCOL, CELL_ACTIVATING_LED_DELAY_ON, CELL_ACTIVATING_LED_DELAY_OFF, 0);

    gSaraStatus.isReadyToConnect = false;
    gSaraStatus.uStatus = uPortInitialized;
    LOG_INF("Cell Device Closed \r\n");

    xLedOff();

}



uDeviceHandle_t xCellSaraGetHandle( void ){
    
    if( gSaraStatus.uStatus < uDeviceOpened ){
        LOG_WRN("GetDeviceHandle error: Device not Added yet");
        return NULL;
    }

    else{
        return gDevHandle;
    }
}



err_code xCellSaraGetLastOperationResult( void ){
    return gLastOperationResult;
}



/* ----------------------------------------------------------------
 * SHELL COMMANDS IMPLEMENTATION
 * -------------------------------------------------------------- */

void xCellSaraGetActiveMqttPlanCmd( const struct shell *shell, size_t argc, char **argv ){
    
    if( gMqttSnActivePlan == FLEX )
        shell_print( shell, "Active Plan is MQTT Flex \r\n" );
    
    else if( gMqttSnActivePlan == ANYWHERE )
        shell_print( shell, "Active Plan is MQTT Anywhere \r\n" );
    
    else{
        shell_error( shell, "Active Plan error \r\n" );
    }
}