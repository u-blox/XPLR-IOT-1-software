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

#ifndef X_BUTTON_H__
#define  X_BUTTON_H__

/** @file
 * @brief This file contains the button API for Sensor Aggregation
 *  Use Case Example Firmware in XPLR-IOT-1 
 */

#include "x_errno.h"


/** How much time the button should be pressed in order for
 * its respective action to be trigered
*/
#define X_BUTTON_PRESS_TIME_FOR_ACTION_MS     3000 //ms


/** Function to configure XPLR-IOT-1 buttons.
 * 
 * @return  zero on success (X_ERR_SUCCESS) else
 *          negative error code.
 */
err_code xButtonsConfig(void);


#endif    //X_BUTTON_H__