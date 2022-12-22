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
 * @brief File containing the main function of the application,
 *  In this application the main function just initializes the necessary modules
 *  The rest of the application is executed via Threads/ commands 
 *  ( shell commands or button press actions )
 */

#include <zephyr.h>
#include <logging/log.h>
#include <logging/log_ctrl.h>

#include "x_sens_bme280.h"
#include "x_sens_lis3mdl.h"
#include "x_sens_icg20330.h"
#include "x_sens_lis2dh12.h"
#include "x_sens_ltr303.h"
#include "x_sens_battery_gauge.h"
#include "x_sens_common.h"

#include "x_pos_maxm10s.h"
#include "x_wifi_ninaW156.h"
#include "x_cell_saraR5.h"
#include "x_nfc.h"
#include "x_ble.h"

#include "x_storage.h"
#include "x_button.h"
#include "x_led.h"

#include "x_logging.h"



// Just initialization processes
void main(void)
{

	// Reclaim pins assigned to Net core for use with App core
	xPinConfReclaimNetCorePins();

	//Initialize sensors
	xSensBme280Init();
	xSensLis3mdlInit();
	xSensIcg20330Init();
	xSensLis2dh12Init();
	xSensLtr303Init();
	xSensBatGaugeInit();

	// Set sensors to not sample until command given
	xSensDisableAll();

	xLedInit();
		
	// Initialize ublox modules
	xCellSaraConfigPins();
	xWifiNinaConfigPins();
	xPosMaxM10ConfigPins();

    // Set GNSS position not to be sampled until command given
	xPosMaxM10Disable();

    // Initializes non-Volatile memory of NORA-B1 for use with littlefs
	//xStorageInit();
	xButtonsConfig();

	// Set the Logger module status to a desired startup config
	// before that process any pending log messages from startup
	// procedures
	while( log_process(false) ){
		;
	}
	xLogStartupConfig();

	// Bluetooth LE (BLE) Functionality
	xBleInit();
	xBleStartAdvertising();

	// MFC Functionality
	xNfcConfig();
	xNfcInit();

	/* From now on the application waits for commands from the user
	 * (either via shell UART terminal or via button presses or BLE commands)
	 * and enables the appropriate threads (or functions) to execute those
	 * commands. You can access shell commands in source code from shell_cmd 
	 * directory.
	*/
}

