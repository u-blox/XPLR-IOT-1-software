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

#ifndef X_BASE64_H__
#define X_BASE64_H__

/** @file
 * @brief File contains the API for Base 64 operations needed in Sensor
 * Aggregation Use Case (XPLR-IOT-1)
 */

#include <stdlib.h>
#include <stdint.h>

#include "x_errno.h"

/** This function encodes a string buffer in Base64.
 *
 * @param plain        The input buffer.
 * @param cipher       The output buffer containing the encoded string.
 * @param cipher_size  The output buffer's max size
 * @return             zero on success (X_ERR_SUCCESS) else negative error code.
 */
err_code xBase64Encode(char* plain, char *cipher, uint32_t cipher_size);


#endif //X_BASE64_H__
