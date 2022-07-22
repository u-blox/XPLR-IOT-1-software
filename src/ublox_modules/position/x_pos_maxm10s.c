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
 * @brief This file contains the function implementation of MaxM10S positioning
 * module used in Sensor Aggregation use case (XPLR-IOT-1) 
 * 
 * -- Operation of MAXM10S module --
 * 
 *  In order to get a position fix the following should be done:
 * 
 *  - xPosMaxM10Init()   -> powers the module, initializes ubxlib and gnss module
 *  - xPosMaxM10Enable() -> starts up the necessary threads (also calls the init function
 *                          mentioned above if necessary)
 * 
 *  How it works:
 *  - Two threads and a callback are used:
 *   1. The start request thread: maxM10PositionRequestStartThread
 *   2. The complete request thread: maxM10PositionRequestCompleteThread
 *   3. The Position Request Callback: gnssPosCallback
 * 
 *  Request Start:
 *  The start request thread is called every time the position is requested
 * (depends on the update period of position). It sends a position request to ubxlib
 *  and then continues execution.
 *  It also sets a maximum time in which the position should be obtained (timeout).
 *  When a position fix is available from ubxlib a Callback is triggered and notifies
 *  the application that a position fix is available.
 * 
 *  Request Callback:
 *  When ubxlib has some results on the position request, this callback is triggered.
 *  In this callback the position is retrieved and we check for errors.
 *  After that the Complete request thread is enabled.
 * 
 *  Complete Request Thread:
 *  This thread types the position and sends data to Thingstream (depending on configuration)
 *  It also stops the ubxlib position request. This is needed to be able to perform a new call later.
 *  
 *  Timeout Conditions:
 *  If the callback is not triggered within timeout, the request fails. Timeout handler calls the
 *  Request complete thread to perform the necessary actions. Then upon the next Start request the
 *  module tries again to get the position. 
 *  
 *  The timeout should be smaller than the position update period. A position Start request should not be attempted,
 *  before the previous request completes. The request completes, either when the callback is triggered or the 
 *  timeout expires.
 * 
 */


#include "x_pos_maxm10s.h"

#include <stdio.h>
#include <stdlib.h>

// Zephyr related includes
#include <zephyr.h>
#include <sys/printk.h>
#include <drivers/uart.h>
#include <drivers/sensor.h> // for channel names
#include <hal/nrf_gpio.h>

// ubxlib related includes
#include "u_cfg_app_platform_specific.h"

#include "u_port.h"
#include "u_port_debug.h"

#include "u_error_common.h"
#include "u_port_uart.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_pwr.h"
#include "u_gnss_pos.h"

// Sensor Aggegation specific includes
#include "x_pin_conf.h"
#include "x_logging.h"
#include "x_led.h"
#include "x_sens_common.h"
#include "x_data_handle.h" //xDataSend
#include "x_module_common.h"
#include "x_system_conf.h"


/* ----------------------------------------------------------------
 * DEFINITION CHECKS
 * -------------------------------------------------------------- */

// The timeout period in this app should not exceed the timeout period defined in ubxlib
#if ( (U_GNSS_POS_TIMEOUT_SECONDS) < (MAXM10S_DEFAULT_TIMEOUT_PERIOD_MS/1000) )
    #error "MAXM10S_DEFAULT_TIMEOUT_PERIOD_MS should be lower than U_GNSS_POS_TIMEOUT_SECONDS"
#endif

// Timeout should be lower than update period
#if ( MAXM10S_DEFAULT_TIMEOUT_PERIOD_MS >= MAXM10S_DEFAULT_UPDATE_PERIOD_MS )
    #error "MAXM10S_DEFAULT_TIMEOUT_PERIOD_MS should be lower than MAXM10S_DEFAULT_UPDATE_PERIOD_MS"
#endif


/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION
 * -------------------------------------------------------------- */

/** This thread controls the Start of every position Request from MaxM10
* Should run every time a new position fix is requested from MaxM10S
*/
void maxM10PositionRequestStartThread(void);


/** Callback controlled by ubxlib. This callback is generated after a position
* request, when MaxM10 has a position fix to report.
*/
static void gnssPosCallback(int32_t networkHandle,
                            int32_t errorCode,
                            int32_t latitudeX1e7,
                            int32_t longitudeX1e7,
                            int32_t altitudeMillimetres,
                            int32_t radiusMillimetres,
                            int32_t speedMillimetresPerSecond,
                            int32_t svs,
                            int64_t timeUtc);

/** For every Position Request started, this thread should run to complete/close that 
* request (either when position is obtained or not). This thread also reports the 
* results of the request (position or error/timeout etc.)
*/
void maxM10PositionRequestCompleteThread(void);



/** When a position Request has started, there is a timeout Period, within which MAXM10S
* should respond with a position fix and gnssPosCallback is to be triggered. If the timeout
* perid expires, this function is called, and handles this situation. 
*/
void maxM10PositionTimeoutHandler(struct k_timer *dummy);


/** Function to be called, when an error occurs.
*/
static void maxM10ErrorHandle(void);

/** this is a Callback function which is trigerred after a Position Request Start, when the
* MaxM10S responds.
*/
static void gnssPosCallback(int32_t networkHandle,
                            int32_t errorCode,
                            int32_t latitudeX1e7,
                            int32_t longitudeX1e7,
                            int32_t altitudeMillimetres,
                            int32_t radiusMillimetres,
                            int32_t speedMillimetresPerSecond,
                            int32_t svs,
                            int64_t timeUtc);


/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

LOG_MODULE_REGISTER(LOGMOD_NAME_MAXM10S, LOG_LEVEL_DBG);

// Semaphore which controls access to maxM10PositionRequestCompleteThread
K_SEM_DEFINE(RequestCompleteSemaphore, 0, 1);

// This timer implements a timeout on the position Requests
K_TIMER_DEFINE(timeoutTimer, maxM10PositionTimeoutHandler, NULL);

// Start a request for position thread
K_THREAD_DEFINE(maxM10PositionRequestStartThread_id, MAXM10S_STACK_SIZE, maxM10PositionRequestStartThread, NULL, NULL, NULL,
		MAXM10S_PRIORITY, 0, 0);

// Complete a position request thread
K_THREAD_DEFINE(maxM10PositionRequestCompleteThread_id, MAXM10S_STACK_SIZE, maxM10PositionRequestCompleteThread, NULL, NULL, NULL,
		MAXM10S_COMPLETE_POS_PRIORITY, 0, 0);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/** Enum indicating Position Request Status. When no Request is active 
 * status is: REQ_STAT_COMPLETED
 */
static enum{
    
    REQ_STAT_PENDING,    /**< Request pending - waiting for response from MaxM10 module */
    REQ_STAT_OBTAINED,   /**< Position has been obtained from MaxM10 module */
    REQ_STAT_COMPLETED   /**< Position Request has been completed (either obtained or timeout or no request is active) */

}positionRequestStatus_t = REQ_STAT_COMPLETED;


static xPosMaxM10Status_t gMaxStatus = {
    .pinsConfigured = false,
    .isPowered = false,
    .isUbxInit = false,
    .isEnabled = false,
    .isPublishEnabled = false,
    .updatePeriod = MAXM10S_DEFAULT_UPDATE_PERIOD_MS,
    .timeoutPeriod = MAXM10S_DEFAULT_TIMEOUT_PERIOD_MS,
    .com = SERIAL_COMM_USB2UART
};


/** Uart handler for access via ubxlib
 */
int32_t gUartHandle;

/** Gnss handler. Provides access via ubxlib
 */
int32_t gGnssHandle;

/** Hold gnss Latitude, Longitude results from MaxMax10 
 */
int32_t gLatitudeX1e7, gLongitudeX1e7;

/** Flag to indicate whether a gnss position request is active via ubxlib
 */
static bool gubxlibGnssRequestActive = false;


//struct k_sem RequestCompleteSemaphore;


// Packet that holds gnss position request results
xDataPacket_t MaxM10Pack = {
    .error = dataErrOk,
	.sensorType = maxm10_t,
	.name = JSON_ID_SENSOR_MAXM10,
	.measurementsNum = 2,
	//measurements
	.meas ={
		// Position x
		[0].name = JSON_ID_SENSOR_CHAN_POS_DX, 
		[0].type = SENSOR_CHAN_POS_DX,
		[0].dataType = isPosition,
		[0].data.doubleVal = 0,
        // Position y
		[1].name = JSON_ID_SENSOR_CHAN_POS_DY, 
		[1].type = SENSOR_CHAN_POS_DY,
		[1].dataType = isPosition,
		[1].data.int32Val = 0
	}
};


/* ----------------------------------------------------------------
 * CALLBACKS
 * -------------------------------------------------------------- */


static void gnssPosCallback(int32_t networkHandle,
                            int32_t errorCode,
                            int32_t latitudeX1e7,
                            int32_t longitudeX1e7,
                            int32_t altitudeMillimetres,
                            int32_t radiusMillimetres,
                            int32_t speedMillimetresPerSecond,
                            int32_t svs,
                            int64_t timeUtc)
{

    //LOG_DBG("errCode: %d", errorCode);
    // if a valid position fix is obtained
    if( (errorCode == 0) && (positionRequestStatus_t == REQ_STAT_PENDING ) ){
        gLatitudeX1e7 = latitudeX1e7;
        gLongitudeX1e7 = longitudeX1e7;
        positionRequestStatus_t = REQ_STAT_OBTAINED;
        k_sem_give( &RequestCompleteSemaphore );
    }
    return;
}



/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */


void xPosMaxM10ConfigPins(void){
    
    nrf_gpio_cfg_output((uint32_t) NORA_EN_MAX_PIN);
    nrf_gpio_cfg_output((uint32_t) MAX_BACKUP_EN_PIN);
    nrf_gpio_cfg_output((uint32_t) NORA_MAX_COM_EN_PIN);
    nrf_gpio_cfg_output((uint32_t) MAX_SAFEBOOT_PIN);
    
    nrf_gpio_pin_clear((uint32_t) MAX_SAFEBOOT_PIN);

    gMaxStatus.pinsConfigured = true;
}



void max10SafeBootPinAssert(void)
{
    if(!gMaxStatus.pinsConfigured){
        xPosMaxM10ConfigPins();
    }
    nrf_gpio_pin_set((uint32_t) MAX_SAFEBOOT_PIN);
}



void max10SafeBootPinDeassert(void)
{
    if(!gMaxStatus.pinsConfigured){
        xPosMaxM10ConfigPins();
    }
    nrf_gpio_pin_clear((uint32_t) MAX_SAFEBOOT_PIN);
}



void max10BackupSupplyPinAssert(void)
{
    if(!gMaxStatus.pinsConfigured){
        xPosMaxM10ConfigPins();
    }
    nrf_gpio_pin_set((uint32_t) MAX_BACKUP_EN_PIN);
}



void max10BackupSupplyPinDeassert(void)
{
    if(!gMaxStatus.pinsConfigured){
        xPosMaxM10ConfigPins();
    }
    nrf_gpio_pin_clear((uint32_t) MAX_BACKUP_EN_PIN);
}



void max10EnablePinAssert(void)
{
    if( !xSensIsChangeAllowed() ){
		LOG_ERR("Cannot change setting when Sensor Aggregation function is active\r\n");
		return;
	}

    if(!gMaxStatus.pinsConfigured){
        xPosMaxM10ConfigPins();
    }
    
    nrf_gpio_pin_set((uint32_t) NORA_EN_MAX_PIN);
}



void max10EnablePinDeassert(void)
{
    if( !xSensIsChangeAllowed() ){
		LOG_ERR("Cannot change setting when Sensor Aggregation function is active\r\n");
		return;
	}

    if(!gMaxStatus.pinsConfigured){
        xPosMaxM10ConfigPins();
    }

    nrf_gpio_pin_clear((uint32_t) NORA_EN_MAX_PIN);
}



void xPosMaxM10EnableNoraCom(void)
{

    if( !xSensIsChangeAllowed() ){
		LOG_ERR("Cannot change setting when Sensor Aggregation function is active\r\n");
		return;
	}

    if(!gMaxStatus.pinsConfigured){
        xPosMaxM10ConfigPins();
    }
    
    nrf_gpio_pin_set((uint32_t) NORA_MAX_COM_EN_PIN);
    gMaxStatus.com = SERIAL_COMM_NORA;
}



void xPosMaxM10DisableNoraCom(void)
{
    if( !xSensIsChangeAllowed() ){
		    LOG_ERR("Cannot change setting when Sensor Aggregation function is active\r\n");
		    return;
	}

    if( gMaxStatus.isUbxInit ){
        xPosMaxM10Deinit();
    }

    nrf_gpio_pin_clear((uint32_t) NORA_MAX_COM_EN_PIN);
    gMaxStatus.com = SERIAL_COMM_USB2UART;
}



err_code xPosMaxM10Init(void){

    err_code err;

    if( !xCommonUPortIsInit() ){
        LOG_WRN("ubxlib port not initialized. Initializing now \r\n");
        err = xCommonUPortInit();
        if( err!= X_ERR_SUCCESS ){
            maxM10ErrorHandle();
            return err;
        }
    }

    if( ! gMaxStatus.isPowered ){
        xPosMaxM10PowerOn();
    }

    if( gMaxStatus.com != SERIAL_COMM_NORA ){
        xPosMaxM10EnableNoraCom();
    }

    uGnssInit();   

    gUartHandle = uPortUartOpen(U_CFG_APP_GNSS_UART,
                               U_GNSS_UART_BAUD_RATE, NULL,
                               U_GNSS_UART_BUFFER_LENGTH_BYTES,
                               U_CFG_APP_PIN_GNSS_TXD,
                               U_CFG_APP_PIN_GNSS_RXD,
                               U_CFG_APP_PIN_GNSS_CTS,
                               U_CFG_APP_PIN_GNSS_RTS);

    if( gUartHandle < 0 ){
        LOG_ERR("Could not open GNSS Uart port\r\n");
        maxM10ErrorHandle();
        return gUartHandle;
    }

    uGnssTransportHandle_t GnssUartHandle;
    GnssUartHandle.uart = gUartHandle;

    gGnssHandle = uGnssAdd(U_GNSS_MODULE_TYPE_M8,
                          U_GNSS_TRANSPORT_NMEA_UART, GnssUartHandle,
                          U_CFG_APP_PIN_GNSS_ENABLE_POWER, false);

    uGnssSetUbxMessagePrint(gGnssHandle, false);

     if ( (err = uGnssPwrOn(gGnssHandle) ) == 0) {
        LOG_INF("Initialized\r\n");
        gMaxStatus.isUbxInit = true;
         //uGnssPwrOff(gGnssHandle);
    }
    else{
        LOG_ERR("Could not initialize\r\n");
        xPosMaxM10Deinit();
    }

    return err;

}



err_code xPosMaxM10SetUpdatePeriod(uint32_t milliseconds){

    if( !xSensIsChangeAllowed() ){
		    LOG_ERR("Cannot change setting when Sensor Aggregation function is active\r\n");
		    return X_ERR_INVALID_STATE; //todo: assign error code - invalid status
	}

    if( milliseconds <= gMaxStatus.timeoutPeriod ){
        LOG_DBG("Update period smaller than timeout\r\n");
        return X_ERR_INVALID_PARAMETER;
    }

    if(positionRequestStatus_t != REQ_STAT_COMPLETED){
        LOG_INF("Terminate current position request");
        k_sem_give( &RequestCompleteSemaphore );
    }

    gMaxStatus.updatePeriod = milliseconds;
    return X_ERR_SUCCESS;
}



err_code MaxM10S_setTimeoutPeriod(uint32_t milliseconds){

    if( !xSensIsChangeAllowed() ){
		LOG_ERR("Cannot change setting when Sensor Aggregation function is active\r\n");
		return -1; //todo: assign error code - invalid status
	}

    // sanity checks
    if( milliseconds > U_GNSS_POS_TIMEOUT_SECONDS*1000 ){
        LOG_DBG("Invalid timeout parameter"); //todo: add more specific message
        return X_ERR_INVALID_PARAMETER;
    }

    if( milliseconds >= gMaxStatus.updatePeriod ){
        LOG_DBG("Timeout parameter should be smaller than sampling period");
        return X_ERR_TIMEOUT_INVALID;
    }

    // if timeout parameter accepted, terminate current request and set new timeout parameter
    if(positionRequestStatus_t!= REQ_STAT_COMPLETED){
        LOG_INF("Terminate current position request");
        k_sem_give( &RequestCompleteSemaphore );
    }

    gMaxStatus.timeoutPeriod = milliseconds;

    return X_ERR_SUCCESS;
}



xPosMaxM10Status_t xPosMaxM10GetModuleStatus(void){
	return gMaxStatus;
}



void xPosMaxM10Disable(void){

    if( !xSensIsChangeAllowed() ){
		    LOG_ERR("Cannot change setting when Sensor Aggregation function is active\r\n");
		    return; //todo: return error code?
	}

    gMaxStatus.isEnabled = false;

    if( positionRequestStatus_t != REQ_STAT_COMPLETED ){
        //thread suspends itself
        k_sem_give( &RequestCompleteSemaphore );
    }
    
	k_thread_suspend( maxM10PositionRequestStartThread_id );
    LOG_INF( "%sMAXM10 suspended%s \r\n", LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT );
} 



err_code xPosMaxM10Enable(void){

    if( !xSensIsChangeAllowed() ){
		    LOG_ERR( "Cannot change setting when Sensor Aggregation function is active\r\n" );
		    return X_ERR_INVALID_STATE; 
	}

    if( ! gMaxStatus.isUbxInit ){
        err_code ret = xPosMaxM10Init();
        if( ret != X_ERR_SUCCESS ){
            return ret;
        }
    } 

	k_thread_resume( maxM10PositionRequestStartThread_id );
    k_thread_resume( maxM10PositionRequestCompleteThread_id );
    LOG_INF( "%sMAXM10 started%s \r\n", LOG_CLRCODE_GREEN, LOG_CLRCODE_DEFAULT );
	gMaxStatus.isEnabled = true;

    return X_ERR_SUCCESS;
	
}



void xPosMaxM10Deinit(){

    xPosMaxM10Disable();
    uGnssDeinit();  
    LOG_INF("MaxM10S Deinitialized\r\n");
    uPortUartClose( gUartHandle );
    gMaxStatus.isUbxInit = false;
}



void xPosMaxM10PowerOn(void){

    if( !xSensIsChangeAllowed() ){
		LOG_ERR("Cannot change setting when Sensor Aggregation function is active\r\n");
		return;
	}

    max10EnablePinAssert();
    max10SafeBootPinDeassert();
    max10BackupSupplyPinDeassert();

    LOG_INF("MaxM10S Powered on\r\n");

    gMaxStatus.isPowered = true;

}



void xPosMaxM10PowerOff(void){

    if( !xSensIsChangeAllowed() ){
		LOG_ERR("Cannot change setting when Sensor Aggregation function is active\r\n");
		return;
	}

    if( gMaxStatus.isUbxInit ){
        xPosMaxM10Deinit();
    }
    max10EnablePinDeassert();
    LOG_INF("MaxM10S Powered off\r\n");
    gMaxStatus.isPowered = false;
}



void xPosMaxM10EnablePublish(bool enable){

    if( !xSensIsChangeAllowed() ){
		LOG_ERR("Cannot change setting when Sensor Aggregation function is active\r\n");
		return;
	}

	gMaxStatus.isPublishEnabled = enable;
	if( gMaxStatus.isPublishEnabled ){
		LOG_INF("%sMAXM10 publish enabled%s \r\n",LOG_CLRCODE_GREEN, LOG_CLRCODE_DEFAULT);
	}
	else{
		LOG_INF("%sMAXM10 publish disabled%s \r\n",LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT);
	}
}



xSensStatus_t xPosMaxM10GetSensorStatus(void){

    xSensStatus_t retStatus;
    retStatus.sensorType = maxm10_t;
    retStatus.isReady = gMaxStatus.isUbxInit;
    retStatus.isEnabled = gMaxStatus.isEnabled;
    retStatus.isPublishEnabled = gMaxStatus.isPublishEnabled;
    retStatus.updatePeriod = gMaxStatus.updatePeriod;
    return retStatus;

}


/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


static void maxM10ErrorHandle(void){
    xLedBlink( ERROR_LEDCOL, ERROR_LED_DELAY_ON, ERROR_LED_DELAY_OFF, ERROR_LED_BLINKS );
}



void maxM10PositionRequestCompleteThread(void){

    char str[60];
    
    while(1){
        k_sem_take( &RequestCompleteSemaphore, K_FOREVER );
        k_timer_stop( &timeoutTimer );

        // did not obtain position yet, therefore another request or timeout has happened
        if( positionRequestStatus_t != REQ_STAT_OBTAINED  ){
            LOG_DBG("No Position Obtained");
            MaxM10Pack.error = dataErrFetchTimeout;
        }
        else{ 
            sprintf( str, "GNSS Position: https://maps.google.com/?q=%3.7f,%3.7f\n",
                ((double) gLatitudeX1e7) / 10000000,
               ((double) gLongitudeX1e7) / 10000000); 
            LOG_INF("%s",str);

            // prepare data to send
            MaxM10Pack.error = dataErrOk;
		    MaxM10Pack.meas[0].data.doubleVal = ((double) gLatitudeX1e7) / 10000000;
		    MaxM10Pack.meas[1].data.doubleVal = ((double) gLongitudeX1e7) / 10000000;
        }

        //send data
		if( gMaxStatus.isPublishEnabled ){			
			xDataSend(MaxM10Pack);
		}

        // set the enum before uGnssPosGetStop, otherwise sometimes the start request is called before this is set...
        // leading to a "New Position Request while previous pending: complete previous" situation, 
        // probably because uGnssPosGetStop may take some time and yield
        positionRequestStatus_t = REQ_STAT_COMPLETED; 
        uGnssPosGetStop( gGnssHandle );
        gubxlibGnssRequestActive = false;

        // check if needs to be suspended
        if( gMaxStatus.isEnabled == false){
            k_thread_suspend(maxM10PositionRequestCompleteThread_id);
        }
    }
}



void maxM10PositionRequestStartThread(void)
{
    int32_t err;
    
	while(1){

        if( ! gMaxStatus.isUbxInit ){
            LOG_WRN("Max not initialized \r\n");
            MaxM10Pack.error = dataErrNotInit;
            //xPosMaxM10Disable();
            //todo: Handling?
        }

        if( positionRequestStatus_t != REQ_STAT_COMPLETED ){
            
            LOG_WRN("New Position Request while previous pending: complete previous\r\n");
            // force previous request to complete
            k_sem_give( &RequestCompleteSemaphore );
        }

        //wait for previous request to close
        while( gubxlibGnssRequestActive ){
            // no active request should exist at this point, however sometimes we may need to wait a bit
            // for the request to close
            k_yield();
        }

        err = uGnssPosGetStart( gGnssHandle, gnssPosCallback);
        if ( err == 0 ) {

            LOG_DBG("Position Start Request\r\n");
            positionRequestStatus_t = REQ_STAT_PENDING;
            gubxlibGnssRequestActive = true;
            
            // set a timeout period for this request
            if( gMaxStatus.timeoutPeriod >0 ){
                k_timer_start(&timeoutTimer, K_MSEC(gMaxStatus.timeoutPeriod), K_NO_WAIT );
            }
        }
        else{
            LOG_ERR("Position Start Request Error: %d  Abort this request\r\n", err);
            MaxM10Pack.error = dataErrFetchFail;
            if( gMaxStatus.isPublishEnabled ){
                xDataSend(MaxM10Pack);
            }
            // try again at next sampling period
        }
    
        k_sleep(K_MSEC(gMaxStatus.updatePeriod));
	}

}



void maxM10PositionTimeoutHandler(struct k_timer *dummy){

    LOG_WRN("Position Request Timeout");
    // force previous request to complete
    k_sem_give( &RequestCompleteSemaphore );

}


/* ----------------------------------------------------------------
 * SHELL COMMANDS IMPLEMENTATION
 * -------------------------------------------------------------- */

void xPosMaxM10EnablePublishCmd(const struct shell *shell, size_t argc, char **argv){

        if(argc!=2){
            shell_print(shell, "Invalid number of parameters\r\n");  
            return;
        }
        
		if( strcmp(argv[1], "on") == 0 ){
			xPosMaxM10EnablePublish(true);
		}
		
		else if( strcmp(argv[1], "off") == 0 ){
			xPosMaxM10EnablePublish(false);
            return;
        }

        else{
            shell_print(shell, "Invalid parameter (on/off)\r\n");  
            return;
		}
}



void xPosMaxM10UpdatePeriodCmd(const struct shell *shell, size_t argc, char **argv){

        err_code err = xPosMaxM10SetUpdatePeriod( atoi(argv[1]) );

        if(err == X_ERR_INVALID_PARAMETER){
            shell_print(shell, "%sRequested Update Period lower than timeout: Try lowering or cancelling timeout first %s \r\n",LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT );
        }

        else{
		    shell_print(shell, "MaxM10S Update Period Set to %d ms", gMaxStatus.updatePeriod);
        }

		return;
}



void xPosMaxM10TimeoutPeriodCmd(const struct shell *shell, size_t argc, char **argv){
        err_code err = MaxM10S_setTimeoutPeriod( atoi(argv[1]) );

        if(err == X_ERR_INVALID_PARAMETER){
            shell_print(shell, "%sRequested Timeout Period exceeds Max allowed: %d seconds%s\r\n",LOG_CLRCODE_RED, U_GNSS_POS_TIMEOUT_SECONDS, LOG_CLRCODE_DEFAULT );
        }

        else if(err == X_ERR_TIMEOUT_INVALID){
            shell_print(shell, "%sRequested Timeout Period should be smaller than update Period: %d ms%s\r\n",LOG_CLRCODE_RED, gMaxStatus.updatePeriod, LOG_CLRCODE_DEFAULT );
        }
        
        else{
		    shell_print(shell, "MaxM10S Timeout Period Set to %d ms \r\n", gMaxStatus.timeoutPeriod);
        }


		return;
}