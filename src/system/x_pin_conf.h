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


#ifndef X_PIN_CONF_H__
#define  X_PIN_CONF_H__


/** @file
 * @brief This file contains XPLR-IOT-1 pin assignements not declared in
 * the Zephyr device tree (these include pins that need to be configured
 * during runtime)
 */


// Pins are named as they appear in the XPLR-IOT-1 (C210) schematics

/* ----------------------------------------------------------------
 * MAXM10S
 * -------------------------------------------------------------- */

#define NORA_EN_MAX_PIN         4    /** Enables MAX voltage rail */
#define MAX_BACKUP_EN_PIN       37   /** MaxM10S Enable backup pin */
#define MAX_SAFEBOOT_PIN        7    /**(NOT LOGIC) ! during reset puts device in safeboot mode */
#define NORA_MAX_COM_EN_PIN     47   /** Controls whether Uart Routes to NORA or USB 2 UART bridge */


/* ----------------------------------------------------------------
 * NINA-W156
 * -------------------------------------------------------------- */

// NINA-W156 uart pins
#define NINA_UART_RX    43
#define NINA_UART_TX    31
#define NINA_UART_CTS   30
#define NINA_UART_RTS   20

#define NINA_RST_PIN           41    /* Reset pin. Active low */
#define NINA_EN_PIN            8     /* Applies voltage rail to NINA module */
#define NORA_NINA_COM_EN_PIN   42    /** Controls whether Uart Routes to NORA or USB 2 UART bridge */


/* ----------------------------------------------------------------
 * SARA-R5
 * -------------------------------------------------------------- */

// SARA-R5 uart pins (secondary uart - primary is routed to UART 2 USB bridge)
#define SARA_SEC_UART_RX    40
#define SARA_SEC_UART_TX    36
#define SARA_SEC_UART_CTS   38
#define SARA_SEC_UART_RTS   19

#define SARA_RST_PIN        21         /* Reset pin */
#define SARA_PWR_ON_PIN     9          /* Applies POWER_ON Signal */
#define NORA_EN_SARA_PIN    10         /* Applies voltage rail to  module */


#endif    //X_PIN_CONF_H__