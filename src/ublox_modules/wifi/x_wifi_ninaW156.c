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
 * @brief This file contains the function implementation of NINA-W156 WiFi
 * module used in Sensor Aggregation use case (XPLR-IOT-1)
 * 
 * The module works as described below:
 * 
 * WiFi credentials should be provided by using the shell command(in the terminal):
 * - modules NINAW156 provision <SSID> <Password>
 * 
 * This needs to be done once since the credentials are saved in NORA-B1 memory.
 * If you need to change the credentials you should execute the same command again.
 * Only one network can be saved.
 * 
 * 
 * Then the flow is as follows:
 * 
 * - xWifiNinaInit()    -> Powers up module, initializes ubxlib, adds network
 * - xWifiNinaConnect() -> Connects to the network added by Init
 * 
 * At this point the user can connect to MQTT
 * 
 * - xWifiNinaDisconnect() -> Disconnect from the network
 * - xWifiNinaDeinit()  -> Deinitialize module (in terms of ubxlib)
 * - xWifiNinaPowerOff() -> Power Down the module
 * 
 * Generally, xWifiNinaConnect() is enough to connect to the network and
 * xWifiNinaPowerOff() is enough to disconnect, deinitialize and power down
 * the module. 
 * Each function can call the previous ones needed to perform its operation.
 * 
 * In order to do that, some functions may need to wait for previous ones to
 * complete their operations. That is why threads are used, to ensure a smooth
 * workflow. Most functions call the corresponding threads eg. xWifiNinaInit()
 * calls xWifiNinaInitThread() using a semaphore.
 * 
 */


#include "x_wifi_ninaW156.h"

#include <zephyr.h>
#include <hal/nrf_gpio.h>
#include <logging/log.h>

#include "u_short_range_module_type.h"
#include "u_network.h"
#include "u_network_config_wifi.h"

#include "x_logging.h"
#include "x_storage.h"
#include "x_system_conf.h"
#include "x_wifi_mqtt.h" // to control automatic disconnections/connections
#include "x_led.h"
#include "x_cell_saraR5.h"
#include "x_pin_conf.h"

#include "x_errno.h"



/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION
 * -------------------------------------------------------------- */

/** Thread called by xWifiNinaInit to configure/initialize the NINA module */
void xWifiNinaInitThread(void);

/** Thread called by xWifiNinaDeinit to deinitialize the NINA module */
void xWifiNinaDeinitThread(void);

/** Thread called by xWifiNinaConnect to connect the NINA module to a network */
void xWifiNinaConnectThread(void);

/** Thread called by xWifiPowerOff to deinitialize and power off the NINA module */
void xWifiNinaPowerOffThread(void);


/** Checks if provided credentials are valid (min/max length, security type).
 *
 * @param creds  The credentials to be checked.
 * @return        Trur if valid, false else.
 */
static bool NinaIsCredentialsValid( xWifiCredentials_t creds );


/** Reset the given credentials structure (render them invalid)
 *
 * @param creds  The credentials to be reset
 */
static void NinaResetCredentials( xWifiCredentials_t creds );



/** Handle error happening in a thread
*/
static void NinaErrorHandle(err_code err_code);


/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */


LOG_MODULE_REGISTER(LOGMOD_NAME_NINAW156, LOG_LEVEL_DBG);

//Define Module Semaphores

K_SEM_DEFINE(xWifiNinaPowerOff_semaphore, 0, 1);
K_SEM_DEFINE(xWifiNinaInit_semaphore, 0, 1);
K_SEM_DEFINE(xWifiNinaDeinit_sempahore, 0, 1);
K_SEM_DEFINE(xWifiNinaConnect_semaphore, 0, 1);

//Define Module Threads

K_THREAD_DEFINE(xWifiNinaPowerOffThreadID, NINAW156_STACK_SIZE, xWifiNinaPowerOffThread, NULL, NULL, NULL,
		NINAW156_CONFIG_PRIORITY, 0, 0);

K_THREAD_DEFINE(xWifiNinaInitThreadID, NINAW156_STACK_SIZE, xWifiNinaInitThread, NULL, NULL, NULL,
		NINAW156_CONFIG_PRIORITY, 0, 0);

K_THREAD_DEFINE(xWifiNinaDeinitThreadID, NINAW156_STACK_SIZE, xWifiNinaDeinitThread, NULL, NULL, NULL,
		NINAW156_CONFIG_PRIORITY, 0, 0);        

K_THREAD_DEFINE(xWifiNinaConnectThreadID, NINAW156_STACK_SIZE, xWifiNinaConnectThread, NULL, NULL, NULL,
		NINAW156_CONNECT_PRIORITY, 0, 0);



/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */


/** Holds the WiFi network credentilas provided by provision command or by reading internal memory */
static xWifiCredentials_t gWiFiCredentialsPending = {0};

/** When NINA has been initialized (NetworkAdded to ubxlib) this structure holds the added/initialized Network */
static xWifiCredentials_t gWiFiCredentialsAdded = {0};

/** Holds the result of the last operation performed by this module (refers to operations that need to report their result
 *  to other modules of the application and cannot report their results directly e.g. a thread operation) */
static err_code gLastOperationResult = X_ERR_SUCCESS;


/** Holds NINA status (as regards Sensor Aggregation application)*/
xWifiNinaStatus_t gNinaStatus = {
    .pinsConfigured = false,
    .isPowered = false,
    .com = SERIAL_COMM_USB2UART,
    .isConnected = false,
    .uStatus = uPortNotInitialized
};


/** Network Handle returned and used by ubxlib functions */
static int32_t gNetHandle;



/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


static void NinaErrorHandle(err_code err_code){

    // sets the global Operation Result and blinks red led to indicate error
    gLastOperationResult = err_code;
    xLedBlink( ERROR_LEDCOL, ERROR_LED_DELAY_ON, ERROR_LED_DELAY_OFF, ERROR_LED_BLINKS);
}



static bool NinaIsCredentialsValid( xWifiCredentials_t creds ){
    
    //sec_type valid values are only 1,2
    switch( creds.sec_type ){
        case 1: break; //valid
        case 2: break; // valid
        default: return false; // not valid
    }

    // Minimum and maximum length requirements check
    if( strlen( creds.SSIDstr ) < WIFI_MIN_SSID_LEN ){
        return false;
    }

    if( strlen( creds.PSWstr ) < WIFI_MIN_PSW_LEN ){
        return false;     
    }

    return true;
}



static void NinaResetCredentials( xWifiCredentials_t creds ){

    // These are set to values so that NinaIsCredentialsValid() fails
    // if creds are checked after this function is called
    creds.sec_type = 0;
    memset( creds.SSIDstr, 0 , WIFI_MAX_SSID_LEN );
    memset( creds.PSWstr, 0 , WIFI_MAX_PSW_LEN );
    return;
}



void xWifiNinaInitThread(void){

    err_code err;

    // needed to avoid thread overflows when using ubxlib functions within a thread
    k_thread_system_pool_assign(k_current_get());

    while(1){

        //Semaphore given by xWifiNinaInit() function
        k_sem_take( &xWifiNinaInit_semaphore, K_FOREVER );

        LOG_DBG("NINAW156 config request \r\n");

        // The initialization thread ends up in a uNetAdded status
        // if the module status is already there, no need to init again
        if( gNinaStatus.uStatus == uNetAdded ){
            LOG_INF("Already Initialized\r\n");
            gLastOperationResult = X_ERR_SUCCESS;
            continue;
        }

        // SaraR5 cellular should not be initialized when trying to init
        // Nina WiFi module. 
        if( xCommonUPortIsInit() ){
            xCellSaraStatus_t saraStatus = xCellSaraGetModuleStatus();
            if( saraStatus.uStatus >= uPortInitialized ){
                // shoould deinitialize previous uPort to config ninaW156
                LOG_ERR("uPort is used by Cellular\r\n");
                NinaErrorHandle( X_ERR_INVALID_STATE ); 
                continue;
            }
        }

        // indication that configuration starts
        xLedFade(WIFI_ACTIVATING_LEDCOL, WIFI_ACTIVATING_LED_DELAY_ON, WIFI_ACTIVATING_LED_DELAY_OFF, 0);

        // invalid credentials - see if there is a valid configuration saved
        if( ! NinaIsCredentialsValid( gWiFiCredentialsPending ) ){

            // clear previous data
            NinaResetCredentials( gWiFiCredentialsPending );

            int ret_code = xStorageReadWifiCred( gWiFiCredentialsPending.SSIDstr,
                WIFI_MAX_SSID_LEN,
                gWiFiCredentialsPending.PSWstr,
                WIFI_MAX_SSID_LEN,
                &gWiFiCredentialsPending.sec_type );

            if( ret_code < 0 ){
                if( ret_code == ERR_STORAGE_FILE_NOT_FOUND ){
                    LOG_ERR( "Cannot Find WiFI config files, please provide WiFi credentials with provision command \r\n" );    
                }
                else{
                    LOG_ERR( "Error opening WiFI config files, please provide WiFi credentials with provision command \r\n" );
                }

                NinaErrorHandle( ret_code ); //todo provide proper error code (invalid state?missing config)
                continue; 
            }
        }

        // check the configuration just read from file
        if( ! NinaIsCredentialsValid( gWiFiCredentialsPending ) ){
            NinaErrorHandle( X_ERR_INVALID_PARAMETER ); //todo: error code - invalid config
            continue;
        }

        //power on module if necessary
        if( !gNinaStatus.isPowered )
            xWifiNinaPowerOn();

        //enable/assert Nina-Nora uart comm
        xWifiNinaEnableNoraCom();

        //configure Nina-Nora uart comm
        xCommonUartCfg(COMMON_UART_NINA);

        // Connection to Wifi network - if open Network
        static const uNetworkConfigurationWifi_t wifiConfigOpen = {
            .type = U_NETWORK_TYPE_WIFI,
            .module = U_SHORT_RANGE_MODULE_TYPE_NINA_W15,
            .uart = 2,
            .pinTxd = -1,
            .pinRxd = -1,
            .pinCts = -1,
            .pinRts = -1,
            .pSsid = gWiFiCredentialsPending.SSIDstr,
            .authentication = 1, //2 /* WPA/WPA2/WPA3 - see wifi/api/u_wifi_net.h */,
            .pPassPhrase = NULL
        };

        // Connection to Wifi network - if Network requires password
        static const uNetworkConfigurationWifi_t wifiConfig = {
            .type = U_NETWORK_TYPE_WIFI,
            .module = U_SHORT_RANGE_MODULE_TYPE_NINA_W15,
            .uart = 2,
            .pinTxd = -1,
            .pinRxd = -1,
            .pinCts = -1,
            .pinRts = -1,
            .pSsid = gWiFiCredentialsPending.SSIDstr,
            .authentication = 2, //2 /* WPA/WPA2/WPA3 - see wifi/api/u_wifi_net.h */,
            .pPassPhrase = gWiFiCredentialsPending.PSWstr
        };

        // Initialize ubxlib Port for Zephyr
        if( !xCommonUPortIsInit() ){
            if( ( err = xCommonUPortInit() ) != X_ERR_SUCCESS ){
                LOG_ERR("Could not intialize NINA \r\n");
                NinaErrorHandle( err ); 
                continue;
            }
        }

        gNinaStatus.uStatus = uPortInitialized;

        if( ( err = uNetworkInit() ) != X_ERR_SUCCESS ){ 
            LOG_ERR("ninaW156 uNetworkInit failed\n");
            NinaErrorHandle( err ); 
            continue;
        }

        gNinaStatus.uStatus = uNetInitialized;

        // Add netowork and keep a copy of the credentials used
        if(gWiFiCredentialsPending.sec_type == 1){
            gNetHandle = uNetworkAdd(U_NETWORK_TYPE_WIFI, &wifiConfigOpen);
            // keep a copy
            strcpy( gWiFiCredentialsAdded.SSIDstr, wifiConfigOpen.pSsid );
            gWiFiCredentialsAdded.sec_type = 1;
        }
        else{
            gNetHandle = uNetworkAdd(U_NETWORK_TYPE_WIFI, &wifiConfig);
            // keep a copy
            strcpy( gWiFiCredentialsAdded.SSIDstr, wifiConfig.pSsid );
            strcpy( gWiFiCredentialsAdded.PSWstr, wifiConfig.pPassPhrase );
            gWiFiCredentialsAdded.sec_type = 2;
        }


        // If network could not be added
        if( gNetHandle < 0){
            LOG_ERR("Could not add network\r\n");
            // delete the copy since the network has not been added/initialized
            NinaResetCredentials( gWiFiCredentialsAdded );
            //gLastOperationResult = gNetHandle;
            NinaErrorHandle(gNetHandle);
            continue;
        }

        // network succesfully added
        gNinaStatus.uStatus = uNetAdded;
        LOG_INF("Configured. Network Added\r\n");
        gLastOperationResult = X_ERR_SUCCESS;

        //stop configuration Led indication
        xLedOff();
    }
}



void xWifiNinaDeinitThread(void){

    while(1){

        // Semaphore given by xWifiNinaDeinit()
        k_sem_take( &xWifiNinaDeinit_sempahore, K_FOREVER );

        LOG_INF("NINA deinit request\r\n");

        xCommonUPortDeinit();
        while( xCommonUPortIsInit() ){
            k_sleep(K_MSEC(500));
        }

        // Invalidate credentials used
        NinaResetCredentials( gWiFiCredentialsAdded );

        LOG_INF("Module Deinitialized\r\n");
        gNinaStatus.uStatus = uPortNotInitialized;
    }
}



void xWifiNinaConnectThread(void){

    err_code ret;

    // needed to avoid thread overflows when using ubxlib functions within a thread
    k_thread_system_pool_assign(k_current_get());

    while(1){

        // Semaphore given by xWifiNinaConnect()
        k_sem_take( &xWifiNinaConnect_semaphore, K_FOREVER );

        LOG_DBG("WiFi Connection request \r\n");

        // if connected abort operation
        // if you want to connect to another network, module should
        // be deinitialized and then initialized again with the credentials
        // of the new network
        if( gNinaStatus.isConnected ){
            LOG_INF("Already connected \r\n");
            gLastOperationResult = X_ERR_SUCCESS;
            continue;
        }

        // if xWifiNinaInit() has not been called prior this thread is called.
        if( gNinaStatus.uStatus < uNetAdded ){
            
            LOG_WRN("No valid network added. Configuring and adding Network now\r\n");
     
            // Configure and add a network
            xWifiNinaInit();
            
            while( ( gNinaStatus.uStatus < uNetAdded ) && ( gLastOperationResult == X_ERR_SUCCESS ) ){
                k_sleep(K_MSEC(1000));
                //ubxlib contains timeouts by itself
            }
            
            //check if the cofiguration was successful in order to proceed
            if( gLastOperationResult != X_ERR_SUCCESS ){
                LOG_ERR("Error Code from NINA W156 configuration request: %d - Abort WiFi connection request\r\n", gLastOperationResult);
                continue;
            }
        } 

        // set up led indication
        xLedFade(WIFI_ACTIVATING_LEDCOL, WIFI_ACTIVATING_LED_DELAY_ON, WIFI_ACTIVATING_LED_DELAY_OFF, 0);

        // Connnect to network
        LOG_INF("Bring-up WiFi\r\n");
        ret = uNetworkUp(gNetHandle);
        if( ret != X_ERR_SUCCESS ){
            LOG_ERR("uNetworkUp failed \r\n");
            gNinaStatus.isConnected = false;    
            NinaErrorHandle( ret );
            continue;
        }


        gNinaStatus.isConnected = true;
        LOG_INF("WiFi Connected\r\n");
        xLedOff();
        gLastOperationResult = X_ERR_SUCCESS;
    }
}



void xWifiNinaPowerOffThread(void){

    while(1){

        // Semaphore given by xWifiNinaPowerOff
        k_sem_take( &xWifiNinaPowerOff_semaphore, K_FOREVER );

        if( !gNinaStatus.isPowered){
            LOG_INF("Already powered Down \r\n"); 
            return;   
        }

        if(gNinaStatus.uStatus >= uPortInitialized){
            xWifiNinaDeinit();
            while( gNinaStatus.uStatus > uPortNotInitialized ){
                k_sleep(K_MSEC(1000));
            }
        }

        xWifiNinaEnablePinDeassert();

        gNinaStatus.isPowered = false;
        LOG_INF("Module powered Down \r\n");
    }
}


/* ----------------------------------------------------------------
 * PUBLIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


void xWifiNinaConfigPins(void){

    //Configure pins as outputs
    nrf_gpio_cfg_output((uint32_t) NINA_RST_PIN);
    nrf_gpio_cfg_output((uint32_t) NINA_EN_PIN);
    nrf_gpio_cfg_output((uint32_t) NORA_NINA_COM_EN_PIN);

    /* Nina also has two more pins connected:
    *  - NINA_SW1_PIN
    *  - NINA_SW2_PIN
    *  Since these two pins are not used in this application
    *  we do not configure them via this module.
    */

    gNinaStatus.pinsConfigured = true;
}



void xWifiNinaResetPinAssert(void){
    
    if(! gNinaStatus.pinsConfigured){
        xWifiNinaConfigPins();
    }

    nrf_gpio_pin_set((uint32_t) NINA_RST_PIN);
}



void xWifiNinaResetPinDeassert(void){

    if(! gNinaStatus.pinsConfigured){
        xWifiNinaConfigPins();
    }

    nrf_gpio_pin_clear((uint32_t) NINA_RST_PIN);
}



void xWifiNinaEnablePinAssert(void)
{
    if(! gNinaStatus.pinsConfigured){
        xWifiNinaConfigPins();
    }
    // Apply VCC to NINA
    nrf_gpio_pin_set((uint32_t) NINA_EN_PIN);
}



void xWifiNinaEnablePinDeassert(void)
{
    if(! gNinaStatus.pinsConfigured){
        xWifiNinaConfigPins();
    }
    // Disable VCC to NINA
    nrf_gpio_pin_clear((uint32_t) NINA_EN_PIN);

}



void xWifiNinaEnableNoraCom(void)
{
    if(! gNinaStatus.pinsConfigured){
        xWifiNinaConfigPins();
    }
    // Select UARTE Comm, UART-BRIDGE comm de-select
    nrf_gpio_pin_set((uint32_t) NORA_NINA_COM_EN_PIN);
    gNinaStatus.com = SERIAL_COMM_NORA;

    LOG_INF("Serial Com set to NORA\r\n");
}



err_code xWifiNinaDisableNoraCom(void)
{
    if( gNinaStatus.uStatus >= uPortInitialized ){
        LOG_ERR("Cannot Disable Nora Serial Com while ubxlib uses the module. Deinit NINA and try again\r\n");
        return X_ERR_INVALID_STATE;
    }


    if(! gNinaStatus.pinsConfigured){
        xWifiNinaConfigPins();
    }

    // Select UART-BRIDGE, disable UART comm between NORA and NINA
    nrf_gpio_pin_clear((uint32_t) NORA_NINA_COM_EN_PIN);
    gNinaStatus.com = SERIAL_COMM_USB2UART;

    LOG_INF("Serial Com Set to UART to USB\r\n");

    return X_ERR_SUCCESS;
}



void xWifiNinaPowerOn(void)
{
    if(gNinaStatus.isPowered){
        LOG_INF("Already powered up\r\n");
        return;
    }

    xWifiNinaEnablePinAssert();

    // Initialize the NINA module, we will hold it in reset until it is powered toggle reset
    xWifiNinaResetPinAssert();
    k_sleep(K_MSEC(10));
    xWifiNinaResetPinDeassert();

    // t_Startup time after release of reset UBX-18006647 - R10 (page 23)
    k_sleep(K_MSEC(2600));

    gNinaStatus.isPowered = true;
    LOG_INF("Module powered Up\r\n");

}



void xWifiNinaNetworkDeinit(void){

    LOG_INF("WiFi network Deinitialize request\r\n");

    if(gNinaStatus.isConnected){
        LOG_WRN("WiFi connected. Disconnecting now\r\n");
        xWifiNinaDisconnect();
    }

    uNetworkDeinit();
    gNinaStatus.uStatus = uPortInitialized;
    LOG_INF("Net Deinitialized \r\n");

}



xWifiNinaStatus_t xWifiNinaGetModuleStatus(void){

    return gNinaStatus;
}



void xWifiNinaDisconnect(void){

    LOG_INF("NINA-W156 Disconnection Request \r\n");

    if( gNinaStatus.isConnected == false ){
        LOG_INF("Already Disconnected\r\n");    
        return;
    }

    // Before Disconnecting check if mqtt needs to disconnect too
    xClientStatusStruct_t mqtt_status = xWifiMqttClientGetStatus();
    if(mqtt_status.status > ClientClosed){
        
        LOG_WRN("MQTT Client open. Closing client first\r\n");    
        xWifiMqttClientClose();
    }
    
    xLedFade(WIFI_DEACTIVATING_LEDCOL, WIFI_ACTIVATING_LED_DELAY_ON, WIFI_ACTIVATING_LED_DELAY_OFF, 0);

    gLastOperationResult = uNetworkDown(gNetHandle);
    if(gLastOperationResult != U_ERROR_COMMON_SUCCESS){
        LOG_ERR("uNetworkDown error: %d \r\n", gLastOperationResult);
    }
    
    xLedOff(); //stop indication
    gNinaStatus.isConnected = false;
    LOG_INF("Disconnected\r\n");
    return;
}



int32_t xWifiNinaGetHandle(void){
    return gNetHandle;
}



err_code xWifiNinaGetLastOperationResult(void){
    return gLastOperationResult;
}



void xWifiNinaInit(void){
    k_sem_give( &xWifiNinaInit_semaphore );
}



void xWifiNinaConnect(void){
    k_sem_give( &xWifiNinaConnect_semaphore );
}



void xWifiNinaDeinit(void){
    k_sem_give( &xWifiNinaDeinit_sempahore );
}



void xWifiNinaPowerOff(void){
    k_sem_give( &xWifiNinaPowerOff_semaphore );
}


/* ----------------------------------------------------------------
 * SHELL COMMANDS IMPLEMENTATION
 * -------------------------------------------------------------- */


void xWifiNinaProvisionCmd(const struct shell *shell, size_t argc, char **argv){
        int rc; 

        if(argc==1){
            // invalid number of parameters
            shell_print(shell, "Please provide SSID and password(if needed) \r\n");  
        }
        else if(argc==2){
            // open network 
            gWiFiCredentialsPending.sec_type = 1;
            strcpy( gWiFiCredentialsPending.SSIDstr, argv[1]);
            shell_print(shell, "Open Network with SSID: %s\r\n", gWiFiCredentialsPending.SSIDstr); 
            rc = xStorageSaveWifiCred( argv[1], strlen(argv[1]), " ", 0, gWiFiCredentialsPending.sec_type );
        }
        else if(argc==3){
            // network with security
            gWiFiCredentialsPending.sec_type = 2;
            strcpy( gWiFiCredentialsPending.SSIDstr, argv[1]);
            strcpy( gWiFiCredentialsPending.PSWstr, argv[2]);
            shell_print(shell, "Network SSID: ""%s""  Password: ""%s"" \r\n", gWiFiCredentialsPending.SSIDstr, gWiFiCredentialsPending.PSWstr);  
            rc = xStorageSaveWifiCred( argv[1], strlen(argv[1]), argv[2], strlen( argv[2] ), gWiFiCredentialsPending.sec_type );
        }
        else{
            shell_print(shell, "Invalid number of parameters\r\n");    
        }

		return;
}



void xWifiNinaTypeNetworkParamsCmd(const struct shell *shell, size_t argc, char **argv){

    int ret_code;

    //  ----------- Type Active Config ----
    if( ! NinaIsCredentialsValid( gWiFiCredentialsAdded ) ){
        shell_print(shell, "------ No valid Network Added/Active --------\r\n");      
    }
    else if( gWiFiCredentialsAdded.sec_type==1 ){
        shell_print(shell, "------ Currently Added/Active Network --------\r\n");
        shell_print(shell, "SSID: ""%s"",  Open network \r\n", gWiFiCredentialsAdded.SSIDstr);
    }
    else if( gWiFiCredentialsAdded.sec_type==2 ){
        shell_print(shell, "------ Currently Added/Active Network --------\r\n");
        shell_print(shell, "SSID: ""%s"",  Passphrase: ""%s"" \r\n", gWiFiCredentialsAdded.SSIDstr, gWiFiCredentialsAdded.PSWstr );
    }

    //  ----------- Type Next Initialization Config ----
    if( ! NinaIsCredentialsValid( gWiFiCredentialsPending ) ){
        shell_print(shell, "------ Readind network saved in file --------\r\n");

        ret_code = xStorageReadWifiCred( gWiFiCredentialsPending.SSIDstr,
            WIFI_MAX_SSID_LEN,
            gWiFiCredentialsPending.PSWstr,
            WIFI_MAX_SSID_LEN,
            &gWiFiCredentialsPending.sec_type );

        if( ret_code < 0 ){
            if( ret_code == ERR_STORAGE_FILE_NOT_FOUND ){
                shell_print(shell,"No network saved \r\n");    
            }
            else{
                shell_print(shell,"Error reading files \r\n");
            }

             // reset invalid config to a known state
             NinaResetCredentials( gWiFiCredentialsPending );
             return;
        }

        // check if read config is valid
        if( ! NinaIsCredentialsValid( gWiFiCredentialsPending ) ){
            shell_print(shell,"Read config is not valid\r\n");
            NinaResetCredentials( gWiFiCredentialsPending );
            return;
        }
    }

    // at this point the config should have been checked and is valid
    if( gWiFiCredentialsPending.sec_type == 1 ){
        shell_print(shell, "------ Network Config for next initialization --------\r\n");
        shell_print(shell, "SSID: ""%s"",  Not secure \r\n", gWiFiCredentialsPending.SSIDstr);
    }
    else if( gWiFiCredentialsPending.sec_type == 2 ){
        shell_print(shell, "------ Network Config for next initialization --------\r\n");
        shell_print(shell, "SSID: ""%s"",  Passphrase: ""%s"" \r\n\r\n", gWiFiCredentialsPending.SSIDstr, gWiFiCredentialsPending.PSWstr );
    }

    //  ----------- Type Saved Config in Memory ----
    shell_print(shell, "------ Network saved in memory --------\r\n");

    xWifiCredentials_t read_cred={0};
    ret_code = xStorageReadWifiCred( read_cred.SSIDstr,
        WIFI_MAX_SSID_LEN,
        read_cred.PSWstr,
        WIFI_MAX_SSID_LEN,
        &read_cred.sec_type );
        
    if( ret_code < 0 ){
        if( ret_code == ERR_STORAGE_FILE_NOT_FOUND ){
            shell_print(shell,"No network saved \r\n");    
        }
        else{
            shell_print(shell,"Error reading files \r\n");
        }
         return;
    }
    
    shell_print(shell, "SSID: ""%s"",  Passphrase: ""%s"", Sec Type: %d \r\n\r\n", read_cred.SSIDstr, read_cred.PSWstr, read_cred.sec_type );

    return;
}
