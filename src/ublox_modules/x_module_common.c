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
 * @brief This file contains the implementation of the API to handle ublox
 *  modules common fuctions. 
 */


#include "x_module_common.h"

// Zephyr/SDK Related
#include <drivers/uart.h>
#include <hal/nrf_gpio.h>
#include <hal/nrf_uarte.h>
#include <logging/log.h>
#include <logging/log_ctrl.h> //log_process

// Ubxlib related
#include "u_port.h"

// Application related
#include "x_logging.h"
#include "x_wifi_ninaW156.h"
#include "x_cell_saraR5.h"
#include "x_pos_maxm10s.h"
#include "x_wifi_mqtt.h"
#include "x_cell_mqttsn.h"
#include "x_shell_conf.h"
#include "x_led.h"
#include "x_system_conf.h"
#include "x_pin_conf.h"



/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION
 * -------------------------------------------------------------- */

/** Thread called by xCommonUPortDeinit and deinitializes Zephyr port
*   for ubxlib
*/
void xCommonUPortDeinitThread(void);


/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */


LOG_MODULE_REGISTER( LOGMOD_NAME_UBLMOD_COMMON, LOG_LEVEL_DBG );

// Semaphore definition
K_SEM_DEFINE(xCommonUPortDeinit_semaphore, 0, 1);

// Thread definition
K_THREAD_DEFINE(xCommonUPortDeinitThreadId, C210_UPORT_STACK_SIZE, xCommonUPortDeinitThread, NULL, NULL, NULL,
		C210_UPORT_PRIORITY, 0, 0);



/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/** Flag signaling if ubxlib Port is initialized
*/
static bool gubxlibPortInitialized = false;


/** Signals which module (SARA or NINA) is configured in the common UART
 * peripheral. Default is none 
*/
static xCommonUart_t gCommonActiveUart = COMMON_UART_NONE;


/** Structure holding the pin configuration in case NINA is configured
 * to work with the common UART share by NINA and SARA
*/
UARTE_PSEL_Type NINA ={
    NINA_UART_RTS,
    NINA_UART_TX,
    NINA_UART_CTS,
    NINA_UART_RX
};


/** Structure holding the pin configuration in case SARA is configured
 * to work with the common UART share by NINA and SARA
*/
UARTE_PSEL_Type SARA ={
    SARA_SEC_UART_RTS,
    SARA_SEC_UART_TX,
    SARA_SEC_UART_CTS,
    SARA_SEC_UART_RX
};



/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


void xCommonUPortDeinitThread(void){

    // needed to avoid thread overflows when using ubxlib functions within a thread
    k_thread_system_pool_assign(k_current_get());

    while(1){

        // Semaphore given by xCommonUPortDeinit
        k_sem_take( &xCommonUPortDeinit_semaphore, K_FOREVER );

        LOG_INF("uPort Deinitialize request \r\n");

        // no need to do something
        if( !gubxlibPortInitialized ){
            //operation_result =  success
            continue;     
        } 

        // get Max module status and close 
        xPosMaxM10Status_t maxM10status = xPosMaxM10GetModuleStatus();
        if( maxM10status.isUbxInit){
            xPosMaxM10Deinit();
        }

        // get WiFi network initialization status and close if necessary
        // also handle mqtt client
        xWifiNinaStatus_t ninaStatus = xWifiNinaGetModuleStatus();

        if( ninaStatus.uStatus >= uNetInitialized ){
            xWifiNinaNetworkDeinit();
        }

        // get Cellular network initialization status and close (SARA)
        // if necessary. Also handles MQTT-SN client
        xCellSaraStatus_t saraStatus = xCellSaraGetModuleStatus();

        if( saraStatus.uStatus >= uNetInitialized ){
            xCellSaraNetworkDeinit();

            while( ( saraStatus.uStatus >= uNetInitialized ) ){   
                k_sleep(K_MSEC(1000));
                saraStatus = xCellSaraGetModuleStatus();
                //we do not check errors here
                //operation_result = xCellSaraGetLastOperationResult();
            }
        }

        // port deinit optical indication
        //  both cell and wifi have the same color in deactivation
        xLedFade(WIFI_DEACTIVATING_LEDCOL,WIFI_ACTIVATING_LED_DELAY_ON, WIFI_ACTIVATING_LED_DELAY_ON, 0);

        //give time for pending logging messages (if any)
        k_sleep(K_MSEC(1000));

        // Process any pending logs from sensor modules prior to disabling
        // (this could cause problems in very fast sampling rates? < 100ms )
	    while( log_process(false) ){
	    	;
	    }

        // save logger state (in order to restore it after shell is deinitialized
        // and then reinitialized). If is not done, logger state resets to default settings
        xLogSaveState();

        // shell is uninitialized before uPort deinitialization. If uPortDeinit
        // is called with shell enabled, the console uart input crashes.
	    xShellDeinit();
    
        // wait for shell to deinitialize

        //this does not work...
        //while( !xShellDeinitIsComplete() ){
            //wait
        //}
        // a wait is used instead
        k_sleep(K_MSEC(1000));

        // finally close uPort
        uPortDeinit();
        
        // reinitilize shell and restore logger settings
        xShellReinitTrigger();
	    k_sleep(K_MSEC(1000));
	    xLogRestoreState();

        LOG_INF("Port DeInitialized\r\n");
        xLedOff();
        gubxlibPortInitialized = false;
        
        //operation_result =  SUCCESS
    }
}



/* ----------------------------------------------------------------
 * PUBLIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */

bool xCommonUPortIsInit(){
    return gubxlibPortInitialized;
}



void xCommonUPortDeinit(void){
    k_sem_give( &xCommonUPortDeinit_semaphore );
}



void xCommonUartCfg(xCommonUart_t type){

    UARTE_PSEL_Type pins;

    if( (type == COMMON_UART_SARA) && (gCommonActiveUart != COMMON_UART_SARA) ){
        pins = SARA;
    }
    else if( (type == COMMON_UART_NINA) && (gCommonActiveUart != COMMON_UART_NINA) ){
        pins = NINA;
    }
    else{
        return;
    }

    nrf_uarte_disable(NRF_UARTE2_S);
    
    // Set up TX and RX pins.
    nrf_gpio_pin_set(pins.TXD);
    nrf_gpio_cfg_output(pins.TXD);
    nrf_gpio_cfg_input(pins.RXD, NRF_GPIO_PIN_NOPULL);
    nrf_uarte_txrx_pins_set(NRF_UARTE2_S, pins.TXD, pins.RXD);

    // Set up CTS and RTS pins.
    nrf_gpio_cfg_input(pins.CTS, NRF_GPIO_PIN_NOPULL);
    nrf_gpio_pin_set(pins.RTS);
    nrf_gpio_cfg_output(pins.RTS);
    nrf_uarte_hwfc_pins_set(NRF_UARTE2_S, pins.RTS, pins.CTS);

    // enable UARTE
    nrf_uarte_enable(NRF_UARTE2_S);
    // kick-start Rx
    nrf_uarte_task_trigger(NRF_UARTE2_S, NRF_UARTE_TASK_STARTRX);

    gCommonActiveUart = type;

}



err_code xCommonUPortInit(void){

    err_code err;

    
    if(gubxlibPortInitialized){
        // de-initialize previous ubxlib configuration, before initializing a new one
        LOG_INF("uPort Already Initialized \r\n");
        return X_ERR_INVALID_STATE;
    }

    if( ( err = uPortInit() ) == 0){
        gubxlibPortInitialized = true;
        LOG_INF("uPort Initialized \r\n");
    }
    else{
        LOG_ERR("uPortInit failed \r\n");
    }

    return err;

}



