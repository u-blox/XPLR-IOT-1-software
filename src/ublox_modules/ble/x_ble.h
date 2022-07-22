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


#ifndef X_BLE_H__
#define  X_BLE_H__

/** @file
 * @brief This file defines the API for BLE functionality in the context
 * of Sensor Aggregation Use case firmware for XPLR-IOT-1.
 */


#include "stdint.h"
#include "x_errno.h"


/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */


/** This function configures BLE functionality for Sensor Aggregation
 * use case and starts BLE advertising. It basically configures a modified
 * Nordic Uart Service as implemented in Nordic's "ble peripheral uart" example.
 * In this modified example, data received from BLE via this Service are typed 
 * immediately in the uart console. The user cannot send data from XPLR-IOT-1
 * to the connected BLE device.
 *
 * @return        zero on success else negative error code.
 */
err_code xBleInit(void);




#endif    //X_BLE_H__