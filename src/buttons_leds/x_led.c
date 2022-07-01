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
 * @brief This file contains the implementation of the Led API described in 
 * x_led.h file
 */


#include "x_led.h"

//Zephyr related
#include <zephyr.h>
#include <stdlib.h> //atoi
#include <logging/log.h>
#include <drivers/led.h>

// Application related
#include "x_logging.h"
#include "x_errno.h"
#include "x_system_conf.h"

/* ----------------------------------------------------------------
 * DEFINITIONS
 * -------------------------------------------------------------- */

// Max brightness for use with xLedSetBrightness function
#define MAX_BRIGHTNESS	100


/* ----------------------------------------------------------------
 * TYPE DEFINITIONS
 * -------------------------------------------------------------- */

/** Describes the combination of leds needed to turn on to get a
 *  certain color
*/
typedef struct{
    uint8_t ledsNum;              /**< Number of Leds needed */
    xLedColor_t ledsNeeded[3];    /**< Which leds are needed (max 3) */
}ledColorCombination_t;


/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION
 * -------------------------------------------------------------- */

/** Thread which controls the blinking/fading effects of Led */
void xLedThread(void);


/** Toggles currently active (in status) led color */
static bool xLedToggle(void);


/** Sets a certain Led color to a given brightness 
 * (in a scale of 0-100)
 *  
 * @param color         Led color
 * @param brightness    Brightness value: 100 = full on, 0 = off
 */
static void xLedSetBrightness(xLedColor_t color, uint8_t brightness);


/** Get the Led combination needed to achieve a certain Led color
 * 
 * @param color         Color to achieve
 * @return              color conmbination needed
 */
static ledColorCombination_t ledGetColorCombination(xLedColor_t color);



/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

// Related to the Zephyr PWM driver which is used to control
// LED brightness 
#if DT_NODE_HAS_STATUS(DT_INST(0, pwm_leds), okay)
    #define LED_PWM_NODE_ID		DT_INST(0, pwm_leds)
    #define LED_PWM_DEV_NAME	DEVICE_DT_NAME(LED_PWM_NODE_ID)
#else
    #error "No LED PWM device found"
#endif


LOG_MODULE_REGISTER(LOGMOD_NAME_LED, LOG_LEVEL_DBG);

// Thread definition
K_THREAD_DEFINE(xLedThreadId, LED_STACK_SIZE, xLedThread, NULL, NULL, NULL,
		LED_PRIORITY, 0, 0);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/** Device descriptor */
const struct device *gpLedPWM;


/** Holds Led status */
static xLedStatus gLedStatus = {
     .LedIsOn=false,
     .color = LedOff, 
     .mode= LedNormal, 
     .blinkState.remainingBlinks = -1, 
     .fadeState.remainingFades = -1 };
 

/** Holds string representation of supported colors */
const char *const gLedColorStrings[]={
	[LedRed] = "red",
	[LedGreen] = "green",
	[LedBlue] = "blue",
	[LedCyan] = "cyan",
	[LedPurple] = "purple",
	[LedYellow] = "yellow",
	[LedWhite] = "white",
    [LedOff] = "off"
};


/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


void xLedThread(void)
{
    // initialize at a "random" value that makes sense.
    // this won't be used anyway under normal circumstances  
    uint32_t sleep_period_ms = 100;

    while(1){

        if( gLedStatus.mode == LedNormal ){
            k_thread_suspend(xLedThreadId);
        }
        
        // Handle Blinking
        if( gLedStatus.mode == LedBlinking ){
            if( xLedToggle() ){  //blink led on period
                sleep_period_ms = gLedStatus.blinkState.delayOn;
            }
            else{ // blink led off period
                sleep_period_ms = gLedStatus.blinkState.delayOff;
                if( gLedStatus.blinkState.remainingBlinks > 0 ){
                    gLedStatus.blinkState.remainingBlinks--;
                }
                // blinking complete
                if( gLedStatus.blinkState.remainingBlinks == 0 ){
                    gLedStatus.mode = LedNormal;
                    gLedStatus.blinkState.delayOn = 0;
                    gLedStatus.blinkState.delayOff = 0;

                    k_thread_suspend(xLedThreadId);
                }
            }
        }

        // Handle Fading
        if( gLedStatus.mode == LedFading ){

            xLedSetBrightness( gLedStatus.color, gLedStatus.fadeState.brightness );
            
            // stop fading
            if( gLedStatus.fadeState.remainingFades == 0 ){
                gLedStatus.mode = LedNormal;
                gLedStatus.fadeState.delayIn = 0;
                gLedStatus.fadeState.delayOut = 0;
                k_thread_suspend(xLedThreadId);
            }

            //start fading out 
            if( gLedStatus.fadeState.brightness == 100 ){
               gLedStatus.fadeState.directionIn = false; 
               sleep_period_ms = gLedStatus.fadeState.delayOut;
            }

            //start fading in 
            if( gLedStatus.fadeState.brightness == 0 ){
                gLedStatus.fadeState.directionIn = true; 
                sleep_period_ms = gLedStatus.fadeState.delayIn;
                gLedStatus.fadeState.remainingFades--;
            }

            // next step brightness
            if( gLedStatus.fadeState.directionIn ){
                gLedStatus.fadeState.brightness+=1;
            }
            else{
                gLedStatus.fadeState.brightness-=1;
            }
        }

        k_sleep( K_MSEC( sleep_period_ms ) );
    }
    
}



static void xLedSetBrightness(xLedColor_t color, uint8_t brightness){

    ledColorCombination_t comb = ledGetColorCombination( gLedStatus.color );
    for( uint8_t x = 0; x < comb.ledsNum ; x++ ){
        led_set_brightness( gpLedPWM, comb.ledsNeeded[x], brightness );
    }

    gLedStatus.LedIsOn = true;
}



static bool xLedToggle(void){
    
    if( gLedStatus.LedIsOn ){
        // turn off all leds
        led_off( gpLedPWM,LedRed );
        led_off( gpLedPWM,LedGreen );
        led_off( gpLedPWM,LedBlue );
        gLedStatus.LedIsOn = false;
        return false;
    }
    else{
        ledColorCombination_t comb = ledGetColorCombination( gLedStatus.color );
        for( uint8_t x = 0; x < comb.ledsNum ; x++){
            led_on( gpLedPWM, comb.ledsNeeded[x] );
        }
        gLedStatus.LedIsOn = true;
        return true;
    }
}



static ledColorCombination_t ledGetColorCombination(xLedColor_t color){

    ledColorCombination_t comb={0};

    if(color <= LedBlue){
        comb.ledsNum=1;
        comb.ledsNeeded[0] = color;
    }

    else if(color == LedWhite){
        comb.ledsNum=3;
        comb.ledsNeeded[0] = LedRed;
        comb.ledsNeeded[1] = LedGreen;
        comb.ledsNeeded[2] = LedBlue;
    }

    // no need to write that exclusively
    // else if(color == LedOff){
    //     comb.ledsNum=0;
    // }

    // 2 color combinations
    else if( color == LedYellow ){
        comb.ledsNum=2;
        comb.ledsNeeded[0] = LedRed;
        comb.ledsNeeded[1] = LedGreen;
    }

    else if( color == LedPurple ){
        comb.ledsNum=2;
        comb.ledsNeeded[0] = LedRed;
        comb.ledsNeeded[1] = LedBlue;
    }

    else if( color == LedCyan ){
        comb.ledsNum=2;
        comb.ledsNeeded[0] = LedGreen;
        comb.ledsNeeded[1] = LedBlue;
    }

    return comb;
}



/* ----------------------------------------------------------------
 * PUBLIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


err_code xLedInit(void){

    gpLedPWM = device_get_binding(LED_PWM_DEV_NAME);

    if (gpLedPWM) {
		LOG_INF("Found device %s\r\n", LED_PWM_DEV_NAME);
        xLedOff();
        return X_ERR_SUCCESS;
	} else {
		LOG_ERR("Device %s not found\r\n", LED_PWM_DEV_NAME);
		return X_ERR_DEVICE_NOT_FOUND; //add error code (init fail)
    }
}



void xLedOn(xLedColor_t color){

    // define which leds are needed to get the requested color
    ledColorCombination_t comb = ledGetColorCombination(color);

    //close previously open leds
    led_off(gpLedPWM,LedRed);
    led_off(gpLedPWM,LedGreen);
    led_off(gpLedPWM,LedBlue);

    // open the necessary leds to get the requested color
    for( uint8_t x = 0; x < comb.ledsNum ; x++){
        led_on(gpLedPWM, comb.ledsNeeded[x] );
    }

    gLedStatus.LedIsOn = true;
    gLedStatus.color = color;
    gLedStatus.mode = LedNormal;

    return;
}



void xLedOff(void){

    led_off(gpLedPWM,LedRed);
    led_off(gpLedPWM,LedGreen);
    led_off(gpLedPWM,LedBlue);
    
    gLedStatus.LedIsOn = false;
    gLedStatus.color = LedOff;
    gLedStatus.mode = LedNormal;
    k_thread_suspend(xLedThreadId);
    
    return;
}



xLedStatus xLedGetStatus(void){
    return gLedStatus;
}



err_code xLedBlink(xLedColor_t color, uint32_t delayOn, uint32_t delayOff, uint32_t blinks){

    if( blinks == 0 ){
        LOG_DBG("Blink: %s, on:%d, off:%d \r\n", gLedColorStrings[color], delayOn, delayOff);
    } 
    else{
        LOG_DBG("Blink: %s %d times, on:%d, off:%d \r\n", gLedColorStrings[color], blinks, delayOn, delayOff);    
    }

    // if both delay on and off are zero the xLedThread won't yield
    if( ( !delayOn ) || ( !delayOff) ){
        LOG_ERR("Delay on or off in blinking cannot be 0 \r\n");
        return X_ERR_INVALID_PARAMETER;
    }

    //reset Led state
    xLedOff();

    gLedStatus.mode = LedBlinking;
    gLedStatus.color = color;
    gLedStatus.blinkState.delayOn = delayOn;
    gLedStatus.blinkState.delayOff = delayOff;
    if( blinks != 0 ){
        gLedStatus.blinkState.remainingBlinks = blinks;
    }
    else{
        gLedStatus.blinkState.remainingBlinks = -1;  //blink indefinitely
    }

    // we assign a thread for blinking, because PWM has some
    // timing restrictions when the on/off times are big
    k_thread_resume(xLedThreadId);

    return X_ERR_SUCCESS;

}



err_code xLedFade(xLedColor_t color, uint32_t fade_in_time, uint32_t fade_out_time, uint32_t times){

    if( times == 0 ){
        LOG_DBG("Fade: %s, on:%d, off:%d \r\n", gLedColorStrings[color], fade_in_time, fade_out_time);
    } 
    else{
        LOG_DBG("Blink: %s %d times, on:%d, off:%d \r\n", gLedColorStrings[color], times, fade_in_time, fade_out_time);    
    }

    // if both delay on and off are zero the xLedThread won't yield
    if( ( !fade_in_time ) && ( !fade_out_time) ){
        LOG_ERR("Fade in and out in fading cannot be both 0 \r\n");
        return X_ERR_INVALID_PARAMETER; 
    }

    if( ( fade_in_time < 100 ) || ( fade_out_time < 100 ) ){
        LOG_ERR("Fade in or out cannot be less than 100 \r\n");
        return X_ERR_INVALID_PARAMETER; 
    }

    //reset Led state
    xLedOff();

    //calculate fade_in, fade_out step delays
    gLedStatus.fadeState.delayIn = (fade_in_time/MAX_BRIGHTNESS);
    gLedStatus.fadeState.delayOut = (fade_out_time/MAX_BRIGHTNESS);

    gLedStatus.mode = LedFading;
    gLedStatus.color = color;
    gLedStatus.fadeState.brightness = 0;
    gLedStatus.fadeState.directionIn = true;
    
    if( times != 0 ){
        //+1 because we subtract 1 every time brightness is 0. We start with brightness 0, so the first 
        // time at fading startup we miss one time
        gLedStatus.fadeState.remainingFades = times + 1; 
    }
    else{
        gLedStatus.fadeState.remainingFades = -1;  //fade indefinitely
    }

    // we assign a thread for blinking, because PWM has some timing
    // restrictions when the on/off times are big
    k_thread_resume(xLedThreadId);

    return X_ERR_SUCCESS;

}



err_code xLedResumeStatus(xLedStatus status){
    
    if( ( !status.LedIsOn ) && ( status.mode == LedNormal ) ){
        xLedOff();

    }

    if( status.color > LedOff ){
        return X_ERR_INVALID_PARAMETER; 
    }

    //actions
    switch(status.mode){
        case LedNormal: xLedOn(status.color); 
                        break;

        case LedBlinking: xLedBlink( status.color, 
                                        status.blinkState.delayOn,
                                        status.blinkState.delayOff,
                                        status.blinkState.remainingBlinks );
                            break;

        case LedFading: xLedFade( status.color, 
                                        (status.fadeState.delayIn)*MAX_BRIGHTNESS,
                                        (status.fadeState.delayOut)*MAX_BRIGHTNESS,
                                        status.fadeState.remainingFades );
                            break;

        default: return X_ERR_INVALID_PARAMETER;
    }

    return X_ERR_SUCCESS;

}


/* ----------------------------------------------------------------
 * SHELL COMMANDS IMPLEMENTATION
 * -------------------------------------------------------------- */


void xLedOnCmd(const struct shell *shell, size_t argc, char **argv){

    if(argc == 1){
            // invalid number of parameters
            shell_warn(shell, "Please provide also the color \r\n");  
            return;
    }
    if(argc > 2){
            // invalid number of parameters
            shell_warn(shell, "invalid number of parameters \r\n");  
            return;
    }
    
    for(uint8_t x=0; x < LedOff; x++){

        if( strcmp( argv[1], gLedColorStrings[x] ) == 0 ){
            xLedOn(x);
            return;
        }
    }

    // if function reaches this point no valid color string was given as a parameter
    shell_error(shell,"No valid color string provided \r\n");
    return;
}



void xLedBlinkCmd(const struct shell *shell, size_t argc, char **argv){

    if( ( argc < 4 ) || (argc > 5) ){
            // invalid number of parameters
            shell_warn(shell, "invalid number of parameters: should be <color> <delay on> <delay off> <optional:blinks> \r\n");  
            return;
    }

    uint8_t x;
    for( x=0; x < LedOff; x++ ){
        if( strcmp( argv[1], gLedColorStrings[x] ) == 0 ){
            break;
        }
    }

    if( x == LedOff ){
        shell_error(shell,"No valid color string provided \r\n");
        return;
    }

    // just blink indefinitely
    if( argc == 4 ){
        xLedBlink(x,atoi(argv[2]),atoi(argv[3]), 0 );
    }
    // blink the specified number of times
    if( argc == 5 ){
        xLedBlink(x,atoi(argv[2]),atoi(argv[3]),atoi(argv[4]));
    } 

    return;

}



void xLedFadeCmd(const struct shell *shell, size_t argc, char **argv){

    if( ( argc < 4 ) || (argc > 5) ){
            // invalid number of parameters
            shell_warn(shell, "invalid number of parameters: should be <color> <delay on> <delay off> <optional:blinks> \r\n");  
            return;
    }

    uint8_t x;
    for( x=0; x < LedOff; x++ ){
        if( strcmp( argv[1], gLedColorStrings[x] ) == 0 ){
            break;
        }
    }

    if( x == LedOff ){
        shell_error(shell,"No valid color string provided \r\n");
        return;
    }

    // just fade indefinitely
    if( argc == 4 ){
        xLedFade(x,atoi(argv[2]),atoi(argv[3]), 0 );
    }
    // fade the specified number of times
    if( argc == 5 ){
        xLedFade(x,atoi(argv[2]),atoi(argv[3]),atoi(argv[4]));
    } 

    return;
}