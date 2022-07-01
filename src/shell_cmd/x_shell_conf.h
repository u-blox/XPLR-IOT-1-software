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

#ifndef X_SHELL_CONF_H__
#define  X_SHELL_CONF_H__

/** @file
 * @brief This file contains the API to handle shell initialization
 * deiintialization functions performed during runtime (Sensor Aggregation
 * Use Case for XPLR-IOT-1)
 */


#include <stdbool.h>


/** Triggers reinitialization of shell after it has been
 * deinitialized.
 */
void xShellReinitTrigger(void);


/** Deinitialize shell
 */
void xShellDeinit(void);


/** Has the deinitialize procedure completed?
 * 
 * @return        True if completed, false otherwise
 */
bool xShellDeinitIsComplete(void);


#endif    //X_SHELL_CONF_H__