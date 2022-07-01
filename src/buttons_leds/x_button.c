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
 * @brief File containing the button gButtonActionTypes implementation for Sensor
 * Aggregation Use Case Example Firmware for XPLR-IOT-1
 */

#include "x_button.h"

// Zephyr Related
#include <zephyr.h>
#include <drivers/gpio.h>
#include <logging/log.h>

// Application Related
#include "x_logging.h"
#include "x_system_conf.h"
#include "x_sensor_aggregation_function.h"
#include "x_led.h"



/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION
 * -------------------------------------------------------------- */

/** Thread which is called by the button callbacks and executes the gButtonActionType
 * triggered when buttons are pressed enough time
*/
void xButtonActionThread(void);


/** The timer measures how much time the button is pressed. If this time is
 * enough this callback is triggered and the neceddary gButtonActionTypes are performed
 */
void xButtonPressTimerCb(struct k_timer *dummy);


/** Callback triggered when button 1 is pressed or released */
void xButtonPressedCb_btn1(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins);


/** Callback triggered when button 2 is pressed or released */
void xButtonPressedCb_btn2(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins);


/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

LOG_MODULE_REGISTER(LOGMOD_NAME_BUTTON, LOG_LEVEL_DBG);

// Semaphore definition
K_SEM_DEFINE(xButtonAction_semaphore, 0, 1);

// Timer definition (measures button pressed time)
K_TIMER_DEFINE(xButtonPressTimer, xButtonPressTimerCb, NULL);

//Thread definition
K_THREAD_DEFINE(xButtonActionThreadId, BUTTON_ACTION_STACK_SIZE, xButtonActionThread, NULL, NULL, NULL,
		BUTTON_ACTION_PRIORITY, 0, 0);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */


/**Enum holding button lock status*/
static enum{
    unlocked,
    btn1,
    btn2
}gButtonLockStatus = unlocked;


/**Holds which action should be performed */
static enum{
    buttonAction1,
    buttonAction2,
    buttonNoAction
}gButtonActionType = buttonNoAction;


/** Device descriptors for Button 1,2 */
static const struct gpio_dt_spec gButton1 = GPIO_DT_SPEC_GET(DT_NODELABEL(button0), gpios);
static const struct gpio_dt_spec gButton2 = GPIO_DT_SPEC_GET(DT_NODELABEL(button1), gpios);


/** Callback Data */
static struct gpio_callback gButton1CbData;
static struct gpio_callback gButton2CbData;


/** Press and Release Button TImestamps */
static uint32_t gButtonPressTimestamp;
static uint32_t gButtonReleaseTimestamp;


/** When button is pressed, a Led is turned on. If duron the press of a button
* the led was in the middle of an optical indication, this state is saved
* and when the button is released, the led activity continues from where it
* was stopped 
*/
xLedStatus gPreviousLedStatus;


/* ----------------------------------------------------------------
 * CALLBACKS
 * -------------------------------------------------------------- */

// Button pressed enough time, call Thread to execute the action
void xButtonPressTimerCb(struct k_timer *dummy)
{
    if( gButtonLockStatus == btn1 ){
        gButtonActionType = buttonAction1;
        k_sem_give( &xButtonAction_semaphore );
    }
    else if( gButtonLockStatus == btn2 ){
        gButtonActionType = buttonAction2;
        k_sem_give( &xButtonAction_semaphore );
    }
    else{
        //invalid status, do nothing
    }
}



void xButtonPressedCb_btn1(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{

    // If button 2 was pressed first and not released yet
    if(gButtonLockStatus == btn2){
        return;
    }

    //Button pressed
	if ( gpio_pin_get_dt(&gButton1) ){

        // handle led status        
        if( gButtonLockStatus == unlocked ){
            gPreviousLedStatus = xLedGetStatus();
        }
        
        gButtonLockStatus = btn1;
        
        // when button 1 is pressed a led indication is used
        xLedOn(BUTTON_1_PRESS_LEDCOL);
		LOG_DBG("Button1 pressed\r\n");
        gButtonPressTimestamp = k_uptime_get();

        // timer expires if button pressed enough time
        k_timer_start(&xButtonPressTimer, K_MSEC( X_BUTTON_PRESS_TIME_FOR_ACTION_MS ), K_NO_WAIT);
        gButtonActionType = buttonNoAction;
	}

    // Button released
	else{
		gButtonReleaseTimestamp = k_uptime_get();
        xLedResumeStatus(gPreviousLedStatus);
        k_timer_stop(&xButtonPressTimer);
        
        // how much time pressed?
        uint32_t press_duration = gButtonReleaseTimestamp - gButtonPressTimestamp;
		LOG_DBG("Button1 Released after %d ms\r\n", press_duration);

        gButtonLockStatus = unlocked;
	}
}



void xButtonPressedCb_btn2(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{

      // If button 1 was pressed first and not released yet
      if(gButtonLockStatus == btn1){
        return;
      }
    
    // Button Pressed
	if ( gpio_pin_get_dt(&gButton2) ){      

        if( gButtonLockStatus == unlocked ){
            gPreviousLedStatus = xLedGetStatus();
        }

        gButtonLockStatus=btn2;
        
        // when button 2 is pressed a led indication is used
        xLedOn(BUTTON_2_PRESS_LEDCOL);
		LOG_DBG("Button2 pressed\r\n");
		gButtonPressTimestamp = k_uptime_get();

        // timer expires if button pressed enough time
        k_timer_start(&xButtonPressTimer, K_MSEC( X_BUTTON_PRESS_TIME_FOR_ACTION_MS ), K_NO_WAIT);
        gButtonActionType = buttonNoAction;
	}

    // Button Released
	else{
		gButtonReleaseTimestamp = k_uptime_get();
        xLedResumeStatus(gPreviousLedStatus);
        k_timer_stop(&xButtonPressTimer);

        // how much time pressed?
        uint32_t press_duration = gButtonReleaseTimestamp - gButtonPressTimestamp;
		LOG_DBG("Button2 Released after %d ms\r\n", press_duration);

        if( press_duration > X_BUTTON_PRESS_TIME_FOR_ACTION_MS){
            gButtonActionType = buttonAction2;
            k_sem_give( &xButtonAction_semaphore );
        }
        
        gButtonLockStatus = unlocked;
	}
}



/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


void xButtonActionThread(void){

    while(1){

        // Semaphore Given by xButtonPressTimerCb when a button is pressed
        // enough time
        k_sem_take( &xButtonAction_semaphore, K_FOREVER );

        // If an action is ongoing, abort new action
        if( xSensorAggregationIsLocked() ){
            LOG_WRN("Functions ongoing, no action will be performed \r\n");
            xLedBlink(ERROR_LEDCOL,100,100,3);
            k_sleep(K_MSEC( (100+100)*3 ) ); //give time for the blinks
            continue;
        } 

        // button 1 action
        if( gButtonActionType == buttonAction1){

            xLedBlink(BUTTON_1_PRESS_LEDCOL,100,100,3);
            k_sleep(K_MSEC( (100+100)*3 ) ); //give time for the blinks

            //if wifi already enabled stop it
            if( xSensorAggregationGetMode() == xSensAggModeWifi ){
                xSensorAggregationStopWifi();
                continue;
            }

            // if cell is enabled, disable it before enabling wifi
            if( xSensorAggregationGetMode() == xSensAggModeCell ){
                xSensorAggregationStopCell();
                do{
                    k_sleep(K_MSEC(1000));
                }while( xSensorAggregationIsLocked() );
            }   

            // enable sensor aggregation application over wifi
            xSensorAggregationStartWifi();
            continue;
        }

        // button 2 action
        if( gButtonActionType == buttonAction2){
            
            xLedBlink(BUTTON_2_PRESS_LEDCOL,100,100,3);
            k_sleep(K_MSEC( (100+100)*3 ) ); //give time for the blinks

            //if cell already enabled stop it
            if( xSensorAggregationGetMode() == xSensAggModeCell ){
                xSensorAggregationStopCell();
                continue;
            }

            // if wifi is enabled, disable it before enabling cell
            if( xSensorAggregationGetMode() == xSensAggModeWifi ){
                xSensorAggregationStopWifi();
                do{
                    k_sleep(K_MSEC(1000));
                }while( xSensorAggregationIsLocked() );
            }

            // enable sensor aggregation application over cell
            xSensorAggregationStartCell();
            continue;
        }
    }
}


/* ----------------------------------------------------------------
 * PUBLIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


err_code xButtonsConfig(void){

    err_code ret;

    if (!device_is_ready(gButton1.port)) {
		LOG_ERR("Error: button device %s is not ready\n",
		       gButton1.port->name);
		return X_ERR_DEVICE_NOT_READY; 
	}

    if (!device_is_ready(gButton2.port)) {
		LOG_ERR("Error: button device %s is not ready\n",
		       gButton2.port->name);
		return X_ERR_DEVICE_NOT_READY; 
	}

    ret = gpio_pin_configure_dt(&gButton1, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to configure %s pin %d\n",
		       ret, gButton1.port->name, gButton1.pin);
		return ret;
	}

    ret = gpio_pin_configure_dt(&gButton2, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to configure %s pin %d\n",
		       ret, gButton2.port->name, gButton2.pin);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&gButton1, GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, gButton1.port->name, gButton1.pin);
		return ret;
	}

    ret = gpio_pin_interrupt_configure_dt(&gButton2, GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, gButton2.port->name, gButton2.pin);
		return ret;
	}

	gpio_init_callback(&gButton1CbData, xButtonPressedCb_btn1, BIT(gButton1.pin));
    gpio_init_callback(&gButton2CbData, xButtonPressedCb_btn2, BIT(gButton2.pin));

	gpio_add_callback(gButton1.port, &gButton1CbData);
    gpio_add_callback(gButton2.port, &gButton2CbData);

	LOG_INF("Set up c210 button 1 at %s pin %d\n", gButton1.port->name, gButton1.pin);
    LOG_INF("Set up c210 button 2 at %s pin %d\n", gButton2.port->name, gButton2.pin);

    return X_ERR_SUCCESS;
}

