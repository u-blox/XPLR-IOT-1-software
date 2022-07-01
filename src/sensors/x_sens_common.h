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


#ifndef X_SENS_COMMON_H__
#define  X_SENS_COMMON_H__


/** @file
 * @brief This file contains functions common to all sensors in the C210 device
 */


#include <drivers/sensor.h>
#include "x_errno.h"

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Enable all sensors (including MAXM10S). 
 * Similar to calling xSensXXXXEnable for all sensors
 */
void xSensEnableAll(void);



/** Disable (suspend) all sensors (including MAXM10S).
 * Similar to calling xSensXXXXDisable for all sensors
 */
void xSensDisableAll(void);



/** Enable publish for the measurements of all sensors. This function
 * does not enable the sensors. When the sensors are enabled their data
 * will be published
 * Similar to calling xSensXXXXEnablePublish(true) for all sensors
 */
void xSensPublishAll(void);



/** Disable publish for the measurements of all sensors. This function
 * does not disable the sensors. The sensors can still be sampling
 * Similar to calling xSensXXXXEnablePublish(false) for all sensors
 */
void xSensPublishNone(void);



/** Set a common update period to all sensors
 * Similar to calling xSensXXXXSetUpdatePeriod(milliseconds) with the same milliseconds
 * parameter for all sensors
 *
 * @param milliseconds  The update period to set 
 * @return              zero on success (X_ERROR_CODE) else negative error code.
 */
err_code xSensSetUpdatePeriodAll(uint32_t milliseconds);



/** Checks if changes to sampling period, enable/disable, publish enable/disable of 
 * a sensor is allowed, based on the system's current status
 *
 * @return        true if changes are allowed, false otherwise
 */
bool xSensIsChangeAllowed(void);



#endif  //X_SENS_COMMON_H__