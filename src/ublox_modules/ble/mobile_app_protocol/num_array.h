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


#ifndef NUM_ARRAY_H__
#define NUM_ARRAY_H__

/** @file
 * @brief This file contains helper functions to convert floats, doubles, int32_t
 * etc. to byte arrays and vice versa, not depending on the endianness of the
 * machine.
 * 
 * Example: Number type to array: 0x0201 -> {0x02, 0x01} (independent of endianness)
 *          Array to Number type: {0x02, 0x01} -> 0x0201 (independent of endianness)
 * 
 * These helper functions are needed to prepare byte array responses in 
 * mobile_app_ble_protocol.h, so they are given as part of the mobile app protocol
 * module.
 */


#include "stdint.h"


/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS - NUMBER TYPE TO ARRAY
 * -------------------------------------------------------------- */

/** Convert a uint16_t number to a byte array as is read in the code,
 * independent of the machine endianness. 
 * 0x0102 -> {0x01, 0x02}
 *
 * @param[out] array   Out param -> The output array.
 * @param[in] number   In param -> The uint16_t number to be converted
 */
void uint16ToArray(uint8_t* array, uint16_t number);

/** Convert a uint32_t number to a byte array as is read in the code,
 * independent of the machine endianness. 
 * 0x01020304 -> {0x01, 0x02, 0x03, 0x04}
 *
 * @param[out] array   Out param -> The output array.
 * @param[in] number   In param -> The uint32_t number to be converted
 */
void uint32ToArray(uint8_t* array, uint32_t number);


/** Convert a uint64_t number to a byte array as is read in the code,
 * independent of the machine endianness. 
 * 0x0102030401020304 -> {0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04}
 *
 * @param[out] array   Out param -> The output array.
 * @param[in] number   In param -> The uint64_t number to be converted
 */
void uint64ToArray(uint8_t* array, uint64_t number);

/** Convert an int16_t number to a byte array, independent of the
 * machine endianness. Usually used to send an int16_t via BLE using
 * functions such as bt_nus_send().
 *
 * @param[out] array   Out param -> The output array.
 * @param[in] number   In param -> The int16_t number to be converted
 */
void int16ToArray(uint8_t* array, int16_t number);


/** Convert an int32_t number to a byte array, independent of the
 * machine endianness. Usually used to send an int32_t via BLE using
 * functions such as bt_nus_send().
 *
 * @param[out] array   Out param -> The output array.
 * @param[in] number   In param -> The int32_t number to be converted
 */
void int32ToArray(uint8_t* array, int32_t number);

/** Convert an int64_t number to a byte array, independent of the
 * machine endianness. Usually used to send an int64_t via BLE using
 * functions such as bt_nus_send().
 *
 * @param[out] array   Out param -> The output array.
 * @param[in] number   In param -> The int64_t number to be converted
 */
void int64ToArray(uint8_t* array, int64_t number);


/** Convert a float number to a byte array, independent of the
 * machine endianness. Usually used to send a float via BLE using
 * functions such as bt_nus_send().
 *
 * @param[out] array   Out param -> The output array.
 * @param[in] number   In param -> The float number to be converted
 */
void floatToArray(uint8_t* array, float number);


/** Convert a double number to a byte array, independent of the
 * machine endianness. Usually used to send a double via BLE using
 * functions such as bt_nus_send().
 *
 * @param[out] array   Out param -> The output array.
 * @param[in] number   In param -> The double number to be converted
 */
void doubleToArray(uint8_t* array, double number);


/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS - ARRAY TO NUMBER TYPE
 * -------------------------------------------------------------- */


/** Convert a byte array containing the bytes for a uint16_t number,
 * to a uint16_t number (endianness independent, bytes are used as they
 * read - from array[0] to array[1]).
 *
 * @param[in] array   The byte array containing the uint16_t bytes.
 * @return            The uint16_t represented by the array.
 */
uint16_t arrayToUint16(uint8_t* array);


/** Convert a byte array containing the bytes for a uint32_t number,
 * to a uint32_t number (endianness independent, bytes are used as they
 * read - from array[0] to array[3]).
 *
 * @param[in] array   The byte array containing the uint32_t bytes.
 * @return            The uint32_t represented by the array.
 */
uint32_t arrayToUint32(uint8_t* array);


/** Convert a byte array containing the bytes for a uint64_t number,
 * to a uint64_t number (endianness independent, bytes are used as they
 * read - from array[0] to array[7]).
 *
 * @param[in] array   The byte array containing the uint64_t bytes.
 * @return            The uint64_t represented by the array.
 */
uint64_t arrayToUint64(uint8_t* array);


/** Convert a byte array containing the bytes for an int16_t number,
 * to an int16_t number (endianness independent, bytes are used as they
 * read - from array[0] to array[1]).
 *
 * @param[in] array   The byte array containing the int16_t bytes.
 * @return            The int16_t represented by the array.
 */
int16_t arrayToInt16(uint8_t* array);


/** Convert a byte array containing the bytes for an int32_t number,
 * to an int32_t number (endianness independent, bytes are used as they
 * read - from array[0] to array[3]).
 *
 * @param[in] array   The byte array containing the int32_t bytes.
 * @return            The int32_t represented by the array.
 */
int32_t arrayToInt32(uint8_t* array);


/** Convert a byte array containing the bytes for an int64_t number,
 * to an int64_t number (endianness independent, bytes are used as they
 * read - from array[0] to array[7]).
 *
 * @param[in] array   The byte array containing the int64_t bytes.
 * @return            The int64_t represented by the array.
 */
int64_t arrayToInt64(uint8_t* array);


/** Convert a byte array containing the bytes for a float number,
 * to a float number (endianness independent, bytes are used as they
 *
 * @param[in] array   The byte array containing the float bytes.
 * @return            The float represented by the array.
 */
float arrayToFloat(uint8_t* array);


/** Convert a byte array containing the bytes for a double number,
 * to a double number (endianness independent, bytes are used as they
 *
 * @param[in] array   The byte array containing the double bytes.
 * @return            The double represented by the array.
 */
double arrayToDouble(uint8_t* array);


#endif   // NUM_ARRAY_H__