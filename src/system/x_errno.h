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

#ifndef X_ERRNO_H__
#define  X_ERRNO_H__


/** @file
 * @brief This file contains the error code definitions used in 
 * Sensor Aggregation Use Case example for XPLR-IOT-1.
 * 
 * These codes should not conflict with Zephyr or ubxlib error codes.
 * Success code of ubxlib is the same as success code of the application.
 */


#include "u_error_common.h"     //ubxlib common error codes
#include <stdint.h>

/**Error Code Type definition*/
typedef int32_t err_code;  


/* ----------------------------------------------------------------
 * ERROR CODE DEFINITIONS
 * -------------------------------------------------------------- */


#define X_ERR_MAX      (-8191)          /**(U_ERROR_SHORT_RANGE_MIN) = -8191 -> Smallest
                                             * error code used by ubxlib,
                                             * used as starting point for c210 errors
                                             * (which are smaller than this base)*/ 

#define X_ERR_MIN      (-9192)          /** the minimum error code allowed for c210 errors */
#define X_ERR_BASE     X_ERR_MAX



#define X_ERR_SUCCESS              U_ERROR_COMMON_SUCCESS  // = 0
#define X_ERR_UNKNOWN              ( X_ERR_BASE -1 )
#define X_ERR_DEVICE_NOT_FOUND     ( X_ERR_BASE -2 )
#define X_ERR_DEVICE_NOT_READY     ( X_ERR_BASE -3 )
#define X_ERR_INVALID_PARAMETER    ( X_ERR_BASE -4 )  
#define X_ERR_TIMEOUT_INVALID      ( X_ERR_BASE -5 )
#define X_ERR_BUFFER_OVERFLOW      ( X_ERR_BASE -6 )
#define X_ERR_INVALID_STATE        ( X_ERR_BASE -7 )
#define X_ERR_AT_CMD               ( X_ERR_BASE -8 )
#define X_ERR_MQTTSN_CON           ( X_ERR_BASE -9 )


// Special error code definitions -- these are error codes
// used by 3rd party libraries, and are used in the code.
// They are defined here just to avoid using them as numerical
// values in the code

#define ERR_STORAGE_FILE_NOT_FOUND     (-2) 


#endif    //X_ERRNO_H__