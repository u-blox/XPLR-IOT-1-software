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

#ifndef X_LED_H__
#define  X_LED_H__

/** @file
 * @brief This file contains the API to control Leds in XPLR_IOT-1.
 * It also contains the Led color/pattern definitions for various optical
 * indications for Sensor Aggregation Use Case example firmware 
 */


#include <stdint.h>
#include <shell/shell.h>

#include "x_errno.h"


/* ----------------------------------------------------------------
 * APPLICATION INDICATION LED COLOR/PATTERN DEFINITIONS
 * -------------------------------------------------------------- */

//define led colour inidications
#define BUTTON_1_PRESS_LEDCOL           LedCyan
#define BUTTON_2_PRESS_LEDCOL           LedGreen

#define WIFI_ACTIVATING_LEDCOL          BUTTON_1_PRESS_LEDCOL
#define CELL_ACTIVATING_LEDCOL          BUTTON_2_PRESS_LEDCOL
#define MQTT_ACTIVATING_LEDCOL          WIFI_ACTIVATING_LEDCOL
#define MQTTSN_ACTIVATING_LEDCOL        CELL_ACTIVATING_LEDCOL       

#define WIFI_DEACTIVATING_LEDCOL        LedWhite
#define CELL_DEACTIVATING_LEDCOL        LedWhite
#define MQTT_DEACTIVATING_LEDCOL        LedWhite
#define MQTTSN_DEACTIVATING_LEDCOL      LedWhite

#define ERROR_LEDCOL                    LedRed

//in case of blink/fade effects in led indications
#define WIFI_ACTIVATING_LED_DELAY_ON          500
#define CELL_ACTIVATING_LED_DELAY_ON          500
#define MQTT_ACTIVATING_LED_DELAY_ON          200
#define MQTTSN_ACTIVATING_LED_DELAY_ON        200

#define WIFI_ACTIVATING_LED_DELAY_OFF          WIFI_ACTIVATING_LED_DELAY_ON
#define CELL_ACTIVATING_LED_DELAY_OFF          CELL_ACTIVATING_LED_DELAY_ON
#define MQTT_ACTIVATING_LED_DELAY_OFF          MQTT_ACTIVATING_LED_DELAY_ON
#define MQTTSN_ACTIVATING_LED_DELAY_OFF        MQTTSN_ACTIVATING_LED_DELAY_ON

#define ERROR_LED_DELAY_ON      100
#define ERROR_LED_DELAY_OFF     100
#define ERROR_LED_BLINKS        3


/* ----------------------------------------------------------------
 * TYPE DEFINITIONS
 * -------------------------------------------------------------- */

/** Supported Led colors
*/
typedef enum {
    LedRed,
    LedGreen,
    LedBlue,
    LedYellow,
    LedPurple,
    LedCyan,
    LedWhite,
    LedOff
}xLedColor_t;


/** Supported Led Modes
*/
typedef enum{
    LedNormal,     /**< Led can be turned on/off */
    LedBlinking,   /**< Led blinks in a certain pattern */
    LedFading      /**< Led fades in/out in a certain pattern */
}xLedMode_t;


/** Structure describing a blinking pattern
*/
typedef struct{
    uint32_t delayOn;        /**< On time */
    uint32_t delayOff;       /**< Off time */
    int32_t remainingBlinks; /**< if negative, blinks indefinitely */
}xLedBlinkState_t;


/** Structure describing a fade in/out pattern and status
*/
typedef struct{
    uint32_t delayIn;       /**< Fade in time */
    uint32_t delayOut;      /**< Fade out time */
    int32_t remainingFades; /**< if negative, fades indefinitely */
    bool directionIn;       /**< Is the pattern in a fade in state now? */
    uint8_t brightness;     /**< Led brightness in a 0-100 scale */
}xLedFadeState_t;


/** Structure describing Led status and current active pattern (if any)
*/
typedef struct{
    bool LedIsOn;                 /**< Is Led currently on? */
    xLedMode_t mode;
    xLedColor_t color;
    xLedBlinkState_t blinkState;
    xLedFadeState_t fadeState;
}xLedStatus;


/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */


/** Initialize Led driver for XPLR-IOT-1 Led. Should be called
 * before any calls to the functions in this file. 
 *
 * @return        zero on success (X_ERR_SUCCESS) else negative error code.
 */
err_code xLedInit(void);


/** Turn the Led on, in the color requested.
 *
 * @param color  The Led color requested
 */
void xLedOn(xLedColor_t color);


/** Turns off the led and disables any pending
 *  blink/fade patterns that might be active
 *
 */
void xLedOff(void);


/** Sets up a blinking pattern for the led and activates it.
 *
 * @param color    Led color
 * @param delayOn  On time in the blinking pattern
 * @param delayOff Off time in the blinking pattern
 * @param times    How many times to blink. If negative, blink indefinitely
 * @return         zero on success (X_ERR_SUCCESS), else negative error code
 * 
 */
err_code xLedBlink(xLedColor_t color, uint32_t delayOn, uint32_t delayOff, uint32_t times);


/** Sets up a blinking pattern for the led and activates it.
 *
 * @param color         Led color
 * @param fade_in_time  Fade in time in the fading pattern
 * @param fade_out_time Fade out time in the fading pattern
 * @param times         How many times to fade. If negative, fade indefinitely
 * @return              zero on success (X_ERR_SUCCESS), else negative error code
 */
err_code xLedFade(xLedColor_t color, uint32_t fade_in_time, uint32_t fade_out_time, uint32_t times);


/** Get Led Status (on/off, if in the middle of blinking pattern etc).
 *
 * @return        Returns the Led status in a xLedStatus struct.
 */
xLedStatus xLedGetStatus(void);


/** Resumes a Led status previously obtained with xLedGetStatus
 *
 * @param status  The led status that should be resumed. Obtained with xLedGetStatus
 * @return        zero on success (X_ERR_SUCCESS), else negative error code

 */
err_code xLedResumeStatus(xLedStatus status);



/* ----------------------------------------------------------------
 * FUNCTIONS IMPLEMENTING SHELL-COMMANDS
 * -------------------------------------------------------------- */


/** This function is intented only to be used as a command executed by the shell.
 * It executes the "led on <color>" shell command and turns on the led with the color
 * defined in the parameters of the command.
 *
 * @param shell  the shell instance from which the command is given.
 * @param argc   the number of parameters given along with the command
 *               (1 extra parameter is expected).
 * @param argv   the array including the parameters themselves (color).
 */
void xLedOnCmd(const struct shell *shell, size_t argc, char **argv);


/** This function is intented only to be used as a command executed by the shell.
 * It executes the "led blink <color> <delay on> <delay off> <optional:blinks>"
 * shell command and starts the blinking patterns according to parameters.
 * If the blinks parameter is not given the led blinks indefinitely. Else this
 * parameter defines the number of blinks to be executed
 *
 * @param shell  the shell instance from which the command is given
 * @param argc   the number of parameters given along with the command
 * @param argv   the array including the parameters themselves
 */
void xLedBlinkCmd(const struct shell *shell, size_t argc, char **argv);


/** This function is intented only to be used as a command executed by the shell.
 * It executes the "led fade <color> <fade in time> <fade out time> <optional:fades number>"
 * shell command and starts the fading pattern according to parameters.
 * If the fades number parameter is not given the led blinks indefinitely. Else the 
 * fades number parameter defines the number of fade ins/outs to be executed.
 *
 * @param shell  the shell instance from which the command is given
 * @param argc   the number of parameters given along with the command
 * @param argv   the array including the parameters themselves
 */
void xLedFadeCmd(const struct shell *shell, size_t argc, char **argv);


#endif    //X_LED_H__