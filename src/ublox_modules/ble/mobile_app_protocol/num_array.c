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
 * @brief This file contains the implementatio of the helper functions
 * defined in num_array.h 
 */

#include "num_array.h"


/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION
 * -------------------------------------------------------------- */

// Internal function to convert a number to an array of bytes. The bytes 
// are positioned as read in the code, independent of the endianness of the 
// machine


/** Internal function to convert a number to an array of bytes. The bytes
 * are positioned as read in the code, independent of the endianness of
 * the machine
 *
 * @param[out] array   The output array.
 * @param[in] num_ptr  Pointer to the number to be converted.
 * @param[in] num_size The size in bytes of the number type to be converted
 */
static void numToArray(uint8_t* array, void* num_ptr, uint8_t num_size);


/** Internal function to convert an array of bytes that consitute a number to that
 * number type. The bytes are positioned as read in the code (begin from array[0]),
 * independent of the endianness of the machine
 *
 * @param[in] array     The input array.
 * @param[out] num_ptr  Pointer to the variable that will hold the converted number.
 * @param[in] num_size  The size in bytes of the number type to be converted
 */
static void arrayToNum(uint8_t* array, void* num_ptr, uint8_t num_size);



/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS - NUMBER TYPE TO ARRAY
 * -------------------------------------------------------------- */


void uint16ToArray(uint8_t* array, uint16_t number)
{
    numToArray(array, (void*)&number, sizeof(uint16_t) );
}


void uint32ToArray(uint8_t* array, uint32_t number)
{
    numToArray(array, (void*)&number, sizeof(uint32_t) );
}


void uint64ToArray(uint8_t* array, uint64_t number)
{
    numToArray(array, (void*)&number, sizeof(uint64_t) );
}


void int16ToArray(uint8_t* array, int16_t number)
{
    numToArray(array, (void*)&number, sizeof(int16_t) );
}


void int32ToArray(uint8_t* array, int32_t number)
{
    numToArray(array, (void*)&number, sizeof(int32_t) );
}


void int64ToArray(uint8_t* array, int64_t number)
{
    numToArray(array, (void*)&number, sizeof(int64_t) );
}


void floatToArray(uint8_t* array, float number)
{
    numToArray(array, (void*)&number, sizeof(float) );
}


void doubleToArray(uint8_t* array, double number)
{
    numToArray(array, (void*)&number, sizeof(double) );
}


/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS - ARRAY TO NUMBER TYPE
 * -------------------------------------------------------------- */


uint16_t arrayToUint16(uint8_t* array)
{
    uint16_t number;
    arrayToNum(array, (void*)&number, sizeof(uint16_t) );
    return number;
}


uint32_t arrayToUint32(uint8_t* array)
{
    uint32_t number;
    arrayToNum(array, (void*)&number, sizeof(uint32_t) );
    return number;
}


uint64_t arrayToUint64(uint8_t* array)
{
    uint64_t number;
    arrayToNum(array, (void*)&number, sizeof(uint64_t) );
    return number;
}


int16_t arrayToInt16(uint8_t* array)
{
    int16_t number;
    arrayToNum(array, (void*)&number, sizeof(int16_t) );
    return number;
}


int32_t arrayToInt32(uint8_t* array)
{
    int32_t number;
    arrayToNum(array, (void*)&number, sizeof(int32_t) );
    return number;
}


int64_t arrayToInt64(uint8_t* array)
{
    int64_t number;
    arrayToNum(array, (void*)&number, sizeof(int64_t) );
    return number;
}


float arrayToFloat(uint8_t* array)
{
    float number;
    arrayToNum(array, (void*)&number, sizeof(float) );
    return number;
}


double arrayToDouble(uint8_t* array)
{
    double number;
    arrayToNum(array, (void*)&number, sizeof(double) );
    return number;
}


/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */

static void numToArray(uint8_t* array, void* num_ptr, uint8_t num_size)
{
    static uint16_t Host = 0x0100;

    if(*((uint8_t*)&Host)) //big endianess
    {
        for(uint8_t i = 0; i < num_size; i++)
        {
            *(array + i) = *((uint8_t*)num_ptr + i);
        }
    }
    else //little endianess
    {
        for(uint8_t i = 0; i < num_size; i++)
        {
            *(array + i) = *((uint8_t*)num_ptr + (num_size - 1 - i));
        }
    }
}

static void arrayToNum(uint8_t* array, void* num_ptr, uint8_t num_size)
{
    static uint16_t Host = 0x0100;

    if(*((uint8_t*)&Host)) //big endianess
    {
        for(uint8_t i = 0; i < num_size; i++)
        {
            *((uint8_t*)num_ptr + i) = *(array + i);
        }
    }
    else //little endianess
    {
        for(uint8_t i = 0; i < num_size; i++)
        {
            *((uint8_t*)num_ptr + (num_size - 1 - i)) = *(array + i);
        }
    }
}