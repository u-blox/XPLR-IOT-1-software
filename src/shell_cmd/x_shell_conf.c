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
 * @brief This file contains the implementation of the API to handle
 *  shell initialization/deintialization functions used in Sensor 
 *  Aggregation Use Case for XPLR-IOT-1
 */


#include "x_shell_conf.h"
#include <zephyr.h>
#include <shell/shell.h>
#include <device.h>
#include <devicetree.h>
#include <shell/shell_uart.h>  //shell_backend_uart_get_ptr


/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION
 * -------------------------------------------------------------- */

//** Initializes shell. Must be called from a workqueue context */
static void shellInitFromWork(struct k_work *work);

/** Callback to be triggered when the shell is unitialized*/
void shellDeinitCb(const struct shell *shell, int res);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/** Flag to signal if the shellDeinitCb has been triggered */
static bool gShellDeinitCbAsserted = false;



/* ----------------------------------------------------------------
 * STATIC FUNCTION IMEPLEMENTATION
 * -------------------------------------------------------------- */


void shellDeinitCb(const struct shell *shell, int res)
{
    gShellDeinitCbAsserted = true;
}



static void shellInitFromWork(struct k_work *work)
{
	const struct device *dev =
			device_get_binding(CONFIG_UART_SHELL_ON_DEV_NAME);
	bool log_backend = CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL > 0;
	// uint32_t level =
	// 	(CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL > LOG_LEVEL_DBG) ?
	// 	CONFIG_LOG_MAX_LEVEL : CONFIG_SHELL_BACKEND_SERIAL_LOG_LEVEL;

	//shell_init(shell_backend_uart_get_ptr(), dev, true, log_backend, level);
	shell_init(shell_backend_uart_get_ptr(), dev, true, log_backend, 0);
}


/* ----------------------------------------------------------------
 * PUBLIC FUNCTION IMEPLEMENTATION
 * -------------------------------------------------------------- */


void xShellReinitTrigger(void)
{
	static struct k_work shell_init_work;

    gShellDeinitCbAsserted = false;

	k_work_init(&shell_init_work, shellInitFromWork);
	int err = k_work_submit(&shell_init_work);

	(void)err;
	__ASSERT_NO_MSG(err >= 0);
}



void xShellDeinit(void)
{
    const struct shell *shell =	shell_backend_uart_get_ptr();
	shell_uninit(shell, shellDeinitCb);
	return;
}



bool xShellDeinitIsComplete(void){
	return gShellDeinitCbAsserted;
}