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
 * during runtime).
 * It also contains functions the have to do with general pin configuration.
 * Pin configuration that has to do with specific modules (e.g. SARA-R5 pins,
 * NINA-W156 pins etc.) is implemented in the respective modules source files.
 * In this file general pin configuration functions that apply to all modules 
 * are declared.
 */


// Pins are named as they appear in the XPLR-IOT-1 (C210) schematics

/* ----------------------------------------------------------------
 * MAXM10S
 * -------------------------------------------------------------- */

// NORA Uart peripheral instance to which MAX is connected
#define MAX_UART                3
#define MAX_UART_BAUDRATE       9600

#define NORA_EN_MAX_PIN         4    /** Enables MAX voltage rail */
#define MAX_BACKUP_EN_PIN       37   /** MaxM10S Enable backup pin */
#define MAX_SAFEBOOT_PIN        7    /** MAX_SAFEBOOT/NINA_SW2 pin. (implements NOT logic)! 
                                         during reset puts device in safeboot mode */
#define NORA_MAX_COM_EN_PIN     47   /** Controls whether Uart Routes to NORA or USB 2 UART bridge */

#define MAX_ENABLE_POWER        (-1) // Temporary....




/* ----------------------------------------------------------------
 * SARA-R5
 * -------------------------------------------------------------- */

// NORA Uart peripheral instance to which SARA is connected
#define SARA_UART           2
#define SARA_UART_BAUDRATE  115200

// SARA-R5 uart pins (secondary uart - primary is routed to UART 2 USB bridge)
#define SARA_SEC_UART_RX    40
#define SARA_SEC_UART_TX    36
#define SARA_SEC_UART_CTS   38
#define SARA_SEC_UART_RTS   19

#define SARA_RST_PIN        21         /* Reset pin */
#define SARA_PWR_ON_PIN     9          /* Applies POWER_ON Signal */
#define NORA_EN_SARA_PIN    10         /* Applies voltage rail to  module */
#define SARA_INT_PIN        33         /* SARA_INT/NINA_SW1 pin */

#define SARA_V_INT_PIN      (-1)         // Not connected

/* ----------------------------------------------------------------
 * NINA-W156
 * -------------------------------------------------------------- */

// NORA Uart peripheral instance to which NINA is connected
#define NINA_UART             SARA_UART  // NINA and SARA in this implementation
                                         // share the same UART peripheral, so 
                                         // either one or the other can occupy this uart 
                                         // at a time. 

#define NINA_UART_BAUDRATE    115200

// NINA-W156 uart pins
#define NINA_UART_RX          43
#define NINA_UART_TX          31
#define NINA_UART_CTS         30
#define NINA_UART_RTS         20

#define NINA_RST_PIN          41    /* Reset pin. Active low */
#define NINA_EN_PIN           8     /* Applies voltage rail to NINA module */
#define NORA_NINA_COM_EN_PIN  42    /** Controls whether Uart Routes to NORA or USB 2 UART bridge */

#define NINA_SW1_PIN          SARA_INT_PIN       /* Careful when using this pin: it serves a double purpose */
#define NINA_SW2_PIN          MAX_SAFEBOOT_PIN   /* Careful when using this pin: it serves a double purpose */


/* ----------------------------------------------------------------
 * FUEL GAUGE
 * -------------------------------------------------------------- */

#define GAUGE_OUT_PIN      12          /* GAUGE_OUT pin */


/* ----------------------------------------------------------------
 * SENSOR PINS
 * -------------------------------------------------------------- */

#define ACCEL_INT_PIN      22          /* ACCEL_INT input(int) pin */          
#define ALT_INT_PIN        32          /* ALT_INT input(int) pin */



/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** When net core bootloader (b0n) is compiled and programmed along with the application
 *  or when BLE functionality is used (= net core is used) some pins defined to be used as uart0
 *  in the cpunet device tree:
 *  (...\ncs\v1.7.0\zephyr\boards\arm\nrf5340dk_nrf5340\nrf5340dk_nrf5340_cpunet.dts)
 *  are assigned to be used by the net core during Zephyr initialization/configuration (post-kernel)
 *  and before our main function in the application starts its execution. This can be seen in
 *  file (...\ncs\v1.7.0\zephyr\boards\arm\nrf5340dk_nrf5340\nrf5340_cpunet_reset.c) 
 * 
 * These pins cannot be controlled by the app core unless we change the appropriate registers
 * and assign those pins to be used again by the app core. This function reclaims those pins
 * to be able to be used again by the application core.
 */
void xPinConfReclaimNetCorePins(void);


#endif    //X_PIN_CONF_H__