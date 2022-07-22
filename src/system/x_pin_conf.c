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
 * @brief This file contains the implementation of any function declared in
 * x_pin_conf.h header file. Those functions have to do with general pin 
 * configuration. Pin configuration that has to do with specific modules
 *  (e.g. SARA-R5 pins, NINA-W156 pins etc.) is implemented in the respective
 * modules source files. In this file general pin configuration functions
 *  that apply to all modules are declared.
 */

#include "x_pin_conf.h"

#include <hal/nrf_gpio.h>




/* ----------------------------------------------------------------
 * PUBLIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */

void xPinConfReclaimNetCorePins(void){

    // reclaim net core uart0 pins, for use by the app core. 
    nrf_gpio_pin_mcu_select( NORA_EN_SARA_PIN, NRF_GPIO_PIN_MCUSEL_APP  );
    nrf_gpio_pin_mcu_select( ALT_INT_PIN, NRF_GPIO_PIN_MCUSEL_APP  );
    nrf_gpio_pin_mcu_select( SARA_INT_PIN, NRF_GPIO_PIN_MCUSEL_APP  );

    // not really needed in the application, this is the SWO pin from the SWD interface 
    //nrf_gpio_pin_mcu_select( 11, NRF_GPIO_PIN_MCUSEL_APP  );

}