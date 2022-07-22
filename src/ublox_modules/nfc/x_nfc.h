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

#ifndef X_NFC_H__
#define  X_NFC_H__

/** @file
 * @brief This function contains the API to control the NFC functionality
 * of the Sensor Aggregation Use Case firmware for XPLR-IOT-1.
 * 
 * The NFC functionality is just used to open a browser (in a mobile phone
 * of tablet) to the XPLR-IOT-1-quick start guide. 
 */ 


#include "x_errno.h"


/* ----------------------------------------------------------------
 * DEFINITIONS
 * -------------------------------------------------------------- */

/** The webpage opened when scanning XPLR-IOT-1 with an NFC device  */
#define NFC_WELCOME_URI     "https://developer.thingstream.io/guides/iot-communication-as-a-service/hardware/xplr-iot-1-quick-start-guide"

// NDEF message max buffer size
#define NDEF_MSG_BUF_SIZE	128


/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Configures NFC functionality to open a browser in the url described
 * in the NFC_WELCOME_URI definition. This function just configures the
 * functionality,  it does not activate NFC frontend. xNfcInit() should
 * be called after that to activate NFC frontend.
 *
 * @return        zero on success else negative error code.
 */
err_code xNfcConfig(void);


/** Activates NFC frontend and starts sensing NFC field.
 *
 * @return        zero on success else negative error code.
 */
err_code xNfcInit(void);



#endif  //X_NFC_H__