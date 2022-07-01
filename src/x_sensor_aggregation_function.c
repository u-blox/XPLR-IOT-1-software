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
 * @brief This file contains the API implementation to access the main
 * functionality of Sensor Aggregation Use Case for XPLR-IOT-1.
 */

#include "x_sensor_aggregation_function.h"

// Zephyr-SDK related
#include <zephyr.h>
#include <stdlib.h>  //atoi
#include <logging/log.h>

// Application Related
#include "x_wifi_ninaW156.h"
#include "x_cell_saraR5.h"
#include "x_wifi_mqtt.h"
#include "x_cell_mqttsn.h"
#include "x_logging.h"
#include "x_sens_common.h"
#include "x_data_handle.h" // reset sensor aggregation message
#include "x_system_conf.h" // default period of sens aggr functionality
#include "x_led.h"


/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION
 * -------------------------------------------------------------- */

/** Thread called by xSensorAggregationStartWifi to perform the necessary
 * operations */
void xSensorAggregationStartWifiThread(void);

/** Thread called by xSensorAggregationStopWifi to perform the necessary
 * operations */
void xSensorAggregationStopWifiThread(void);

/** Thread called by xSensorAggregationStartCell to perform the necessary
 * operations */
void xSensorAggregationStartCellThread(void);

/** Thread called by xSensorAggregationStopCell to perform the necessary
 * operations */
void xSensorAggregationStopCellThread(void);


/** Handle error happening in a thread */
static void SensorAggregationErrorHandle(err_code err_code);


/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

LOG_MODULE_REGISTER(SENSOR_AGGREGATION_LOGMOD_NAME, LOG_LEVEL_DBG);

// Semaphore definition
K_SEM_DEFINE(xSensorAggregationStartWifi_semaphore, 0, 1);
K_SEM_DEFINE(xSensorAggregationStopWifi_semaphore, 0, 1);
K_SEM_DEFINE(xSensorAggregationStartCell_semaphore, 0, 1);
K_SEM_DEFINE(xSensorAggregationStopCell_semaphore, 0, 1);


// Threads definition
K_THREAD_DEFINE(xSensorAggregationStartWifiThreadId, SENS_AGG_STACK_SIZE, xSensorAggregationStartWifiThread, NULL, NULL, NULL,
		SENS_AGG_PRIORITY, 0, 0);

K_THREAD_DEFINE(xSensorAggregationStopWifiThreadId, SENS_AGG_STACK_SIZE, xSensorAggregationStopWifiThread, NULL, NULL, NULL,
		SENS_AGG_PRIORITY, 0, 0);

K_THREAD_DEFINE(xSensorAggregationStartCellThreadId, SENS_AGG_STACK_SIZE, xSensorAggregationStartCellThread, NULL, NULL, NULL,
		SENS_AGG_PRIORITY, 0, 0);

K_THREAD_DEFINE(xSensorAggregationStopCellThreadId, SENS_AGG_STACK_SIZE, xSensorAggregationStopCellThread, NULL, NULL, NULL,
		SENS_AGG_PRIORITY, 0, 0);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/** Sensor Aggregation Functionality Update (sampling) Prediod 
 * (common for all sensors when mode is enabled)
*/
static uint32_t gUpdatePeriod = SENS_AGG_DEFAULT_UPDATE_PERIOD_MS;

/** Holds Sensor Aggregation Mode - default is disabled*/
xSensorAggregationMode_t gCurrentMode = xSensAggModeDisabled;


/** Holds the result of the last operation performed by this module
 *  (mostly refer to operations performed by threads) */
static err_code gLastOperationResult = X_ERR_SUCCESS;


/** Flag to signal when it is ok to use start or stop functions/commands
 * see: xSensorAggregationIsLocked description
*/
static bool gFunctionIsLocked = false;


/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */

static void SensorAggregationErrorHandle(err_code err_code){
    gLastOperationResult = err_code;
    xLedBlink( ERROR_LEDCOL, ERROR_LED_DELAY_ON, ERROR_LED_DELAY_OFF, ERROR_LED_BLINKS);
}


void xSensorAggregationStartWifiThread(void){

    bool lock_function_on_entrance_copy = gFunctionIsLocked;

    while(1){

        //revert lock to its previous state after execution
        gFunctionIsLocked = lock_function_on_entrance_copy;

        // Semaphore given by xSensorAggregationStartWifi()
        k_sem_take( &xSensorAggregationStartWifi_semaphore, K_FOREVER );

        // keep lock previous state
        lock_function_on_entrance_copy = gFunctionIsLocked;
        // lock
        gFunctionIsLocked = true;

        if( gCurrentMode == xSensAggModeWifi ){
            LOG_INF("WiFi Sensor Aggregation is already on with period: %d ms \r\n", gUpdatePeriod);
            continue;
        }

        if( gCurrentMode == xSensAggModeCell ){
            //disable cell mode
            LOG_WRN("Cell Sensor Aggregation is already on with period: %d ms. Disabling now \r\n", gUpdatePeriod);
            xSensorAggregationStopCell();
            while( gCurrentMode == xSensAggModeCell ){
                k_sleep(K_MSEC(1000));
            }
        }

        LOG_INF("WiFi Sensor Aggregation starting with period: %d ms \r\n", gUpdatePeriod);

        xDataResetSensorAggregationMsg();
        xSensDisableAll();
        xSensPublishNone();
        gLastOperationResult = xSensSetUpdatePeriodAll( gUpdatePeriod );
        if( gLastOperationResult != X_ERR_SUCCESS ){
            LOG_ERR("There was an issue with the update Period. Abort Sensor Aggregation startup\r\n");
            SensorAggregationErrorHandle(gLastOperationResult);
            continue;
        }

        xWifiMqttClientConnect();
        xClientStatusStruct_t mqtt_stat = xWifiMqttClientGetStatus();

        // wait for connection to MQTT and check for errors
        while( ( mqtt_stat.status < ClientConnected ) && ( gLastOperationResult == X_ERR_SUCCESS ) ) {
            k_sleep(K_MSEC(1000));
            mqtt_stat = xWifiMqttClientGetStatus();
            gLastOperationResult = xWifiMqttGetLastOperationResult();
        }

        if( gLastOperationResult != X_ERR_SUCCESS ){
            LOG_ERR("Error Code from MQTT Connect Request: %d - aborting sensor aggregation initialization", gLastOperationResult);
            xSensorAggregationStopWifi();
            continue;
        }
                
        gCurrentMode = xSensAggModeWifi;

        xSensPublishAll();
        xSensEnableAll();

        xLedOn(WIFI_ACTIVATING_LEDCOL);

    }
}



void xSensorAggregationStopWifiThread(void){

    bool lock_function_on_entrance_copy = gFunctionIsLocked;

    while(1){

        //revert lock to its previous state after execution
        gFunctionIsLocked = lock_function_on_entrance_copy;

        // Semaphore given by xSensorAggregationStopWifi()
        k_sem_take( &xSensorAggregationStopWifi_semaphore, K_FOREVER );
        
        //keep a copy of current lock status and then lock
        lock_function_on_entrance_copy = gFunctionIsLocked;
        gFunctionIsLocked = true;

        LOG_DBG("WiFi Sensor Aggregation stop request \r\n");

        if( gCurrentMode == xSensAggModeCell ){
            LOG_INF("Cell Sensor Aggregation mode is enabled. Abort Action \r\n");
            continue;
        }

        xWifiNinaDeinit(); //also deinitiliazes ubxlib

        xWifiNinaStatus_t nina_stat = xWifiNinaGetModuleStatus();
        //in deinitialization we do not check for errors
        while( nina_stat.uStatus > uPortNotInitialized ){
            k_sleep(K_MSEC(1000));
            nina_stat = xWifiNinaGetModuleStatus();
        }

        xSensDisableAll();
        xSensPublishNone();
        LOG_INF("WiFi Sensor Aggregation stopped \r\n");
        gCurrentMode = xSensAggModeDisabled;
    }
}



void xSensorAggregationStartCellThread(void){

    bool lock_function_on_entrance_copy = gFunctionIsLocked;

    while(1){
        
        //revert lock to its previous state after execution
        gFunctionIsLocked = lock_function_on_entrance_copy;
        
        //Semaphore given by xSensorAggregationStartCell()
        k_sem_take( &xSensorAggregationStartCell_semaphore, K_FOREVER );
        
        //keep a copy of current lock status and then lock
        lock_function_on_entrance_copy = gFunctionIsLocked;
        gFunctionIsLocked = true;

        if( gCurrentMode == xSensAggModeCell ){
            LOG_INF("Cell Sensor Aggregation is already on with period: %d ms \r\n", gUpdatePeriod);
            continue;
        }

        if( gCurrentMode == xSensAggModeWifi ){
            //disable wifi mode
            LOG_WRN("WiFi Sensor Aggregation is already on with period: %d ms. Disabling now \r\n", gUpdatePeriod);
            xSensorAggregationStopWifi();
            while( gCurrentMode == xSensAggModeWifi ){
                k_sleep(K_MSEC(1000));
            }
        }

        LOG_INF("Cell Sensor Aggregation starting with period: %d ms \r\n", gUpdatePeriod);

        xDataResetSensorAggregationMsg();
        xSensDisableAll();
        xSensPublishNone();
        gLastOperationResult = xSensSetUpdatePeriodAll( gUpdatePeriod );
        if( gLastOperationResult != X_ERR_SUCCESS ){
            LOG_ERR("There was an issue with the update Period. Abort Sensor Aggregation startup\r\n");
            SensorAggregationErrorHandle(gLastOperationResult);
            continue;
        }

        xCellMqttSnClientConnect();
        xClientStatus_t mqttsn_stat = xCellMqttSnClientGetStatus();
        
        //wait for connection and check for errors
        while( ( mqttsn_stat < ClientConnected ) && ( gLastOperationResult == X_ERR_SUCCESS ) ) {
            k_sleep(K_MSEC(1000));
            mqttsn_stat = xCellMqttSnClientGetStatus();
            gLastOperationResult = xCellMqttSnGetLastOperationResult();
        }

        if(gLastOperationResult != X_ERR_SUCCESS ){
                LOG_ERR("Error Code from MQTT-SN Connect Request: %d - aborting sensor aggregation initialization", gLastOperationResult);
                xSensorAggregationStopCell();
                continue;
        }
        
        gCurrentMode = xSensAggModeCell;

        xSensPublishAll();
        xSensEnableAll();

        xLedOn(CELL_ACTIVATING_LEDCOL);

    }
}


void xSensorAggregationStopCellThread(void){

    bool lock_function_on_entrance_copy = gFunctionIsLocked;

    while(1){
       
        //revert lock to its previous state after execution
        gFunctionIsLocked = lock_function_on_entrance_copy;

         // Semaphore given by xSensorAggregationStopCell()
        k_sem_take( &xSensorAggregationStopCell_semaphore, K_FOREVER );

        // keep current lock status copy and then lock
        lock_function_on_entrance_copy = gFunctionIsLocked;
        gFunctionIsLocked = true;

        LOG_DBG("Cell Sensor Aggregation stop request \r\n");

        if( gCurrentMode == xSensAggModeWifi ){
            LOG_INF("Cell Sensor Aggregation mode is enabled. Abort Action \r\n");
            continue;
        }

        xCellSaraDeinit(); //also deinitiliazes ubxlib

        xCellSaraStatus_t sara_stat = xCellSaraGetModuleStatus();
        //in deinitialization we do not check for errors
        while( sara_stat.uStatus > uPortNotInitialized ){
            k_sleep(K_MSEC(1000));
            sara_stat = xCellSaraGetModuleStatus();
        }

        xSensDisableAll();
        xSensPublishNone();
        LOG_INF("Cell Sensor Aggregation stopped \r\n");
        gCurrentMode = xSensAggModeDisabled;
    }

}


/* ----------------------------------------------------------------
 * PUBLIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


void xSensorAggregationStartWifi(void){
    k_sem_give( &xSensorAggregationStartWifi_semaphore );
}


void xSensorAggregationStopWifi(void){
    k_sem_give( &xSensorAggregationStopWifi_semaphore );
}


void xSensorAggregationStartCell(void){
    k_sem_give( &xSensorAggregationStartCell_semaphore );
}



void xSensorAggregationStopCell(void){
    k_sem_give( &xSensorAggregationStopCell_semaphore );
}



err_code xSensorAggregationSetUpdatePeriod(uint32_t milliseconds){
    
    err_code err;

    gUpdatePeriod = milliseconds;

    // if currrently running, update while active
    if(gCurrentMode != xSensAggModeDisabled){
        err = xSensSetUpdatePeriodAll( milliseconds );
        if( err != X_ERR_SUCCESS ){
            LOG_ERR("Invalid update period requested for Sensor Aggregation function\r\n");
            return err; 
        }
    }
    return X_ERR_SUCCESS;
}



int32_t xSensorAggregationGetUpdatePeriod(void){

    return gUpdatePeriod;
}



xSensorAggregationMode_t xSensorAggregationGetMode(void){
    return gCurrentMode;
}



bool xSensorAggregationIsLocked(void){
    return gFunctionIsLocked;
}


/* ----------------------------------------------------------------
 * SHELL COMMANDS IMPLEMENTATION
 * -------------------------------------------------------------- */


void xSensorAggregationTypeStatusCmd(const struct shell *shell, size_t argc, char **argv){
    
    const char *const xSensorAggregationMode_t_strings[]={
        [xSensAggModeDisabled] = "Disabled",
        [xSensAggModeWifi] = "WiFi mode",
        [xSensAggModeCell] = "Cell mode"
    };

    shell_print(shell, "Sensor Aggregation Function Mode: %s with sampling period: %d ms \r\n",
     xSensorAggregationMode_t_strings[gCurrentMode], gUpdatePeriod);

    return;
}



void xSensorAggregationSetUpdatePeriodCmd(const struct shell *shell, size_t argc, char **argv)
{

		// to do: Can add some sanity checks here on the period***

        if( ( xSensorAggregationSetUpdatePeriod( atoi( argv[1]) ) ) == X_ERR_SUCCESS ){
		    shell_print(shell, "Sensor Aggregation Update Period Set to %d ms", gUpdatePeriod);
        }
        else{
            shell_error(shell, "Sensor Aggregation Could not Update Period");
        }
		return;
}


