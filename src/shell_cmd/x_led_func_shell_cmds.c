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
 * @brief This file contains "functions" and "leds" shell commands 
 * implementation and defines the shell commands structure for the those
 * root commands of the XPLR-IOT-1 Sensor Aggregation Use Case
 */


#include <shell/shell.h>
#include "x_sensor_aggregation_function.h"
#include "x_led.h"
#include "x_system_conf.h"
#include "mobile_app_ble_protocol.h"



/* ----------------------------------------------------------------
 * COMMAND FUNCTION IMPLEMENTATION (STATIC)
 * -------------------------------------------------------------- */


void xFirmwareVersionType(const struct shell *shell, size_t argc, char **argv){

       shell_print(shell, "\r\nFirmware Version: %d.%d", FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR );
       
       // If this is an internal version, type the internal version too
       if( FIRMWARE_VERSION_INTERNAL != 0 ){
           shell_print(shell, "Internal Version: %d", FIRMWARE_VERSION_INTERNAL );
       }

       shell_print(shell, "BLE mobile app communication protocol Version: %d.%d", M_BLE_PROT_VERSION_MAJOR, M_BLE_PROT_VERSION_MINOR );
}

/* ----------------------------------------------------------------
 * DEFINE MODULES SHELL COMMAND MENU
 * -------------------------------------------------------------- */

/* Creating subcommands (level 1 command) array for command "led". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_led,
       SHELL_CMD(off, NULL, "Led off", xLedOff),
       SHELL_CMD(on, NULL, "Led on <color>", xLedOnCmd),
       SHELL_CMD(blink, NULL, "blink <color> <times>", xLedBlinkCmd),
       SHELL_CMD(fade, NULL, "Fade", xLedFadeCmd),
       SHELL_SUBCMD_SET_END
);

/* Creating subcommands (level 1 command) array for command "functions". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_functions,
       SHELL_CMD(wifi_start, NULL, "Start Sensor Aggregation via wifi", xSensorAggregationStartWifi),
       SHELL_CMD(wifi_stop, NULL, "Stop Sensor Aggregation via wifi", xSensorAggregationStopWifi),
       SHELL_CMD(cell_start, NULL, "Start Sensor Aggregation via cellular", xSensorAggregationStartCell),
       SHELL_CMD(cell_stop, NULL, "Stop Sensor Aggregation via cellular", xSensorAggregationStopCell),
       SHELL_CMD(status, NULL, "Get the status of Sensor Aggregation Function", xSensorAggregationTypeStatusCmd),
       SHELL_CMD(set_period, NULL, "Set the sampling period of Sensor Aggregation Function", xSensorAggregationSetUpdatePeriodCmd),
       //SHELL_CMD(NINAW156, &NINAW156, "NINAW156 control", NULL),
       SHELL_SUBCMD_SET_END
);

/* Creating root (level 0) command "functions" without a handler */
SHELL_CMD_REGISTER(functions, &sub_functions, "C210 Sensor Aggragetion Main Functions", NULL);

/* Creating root (level 0) command "led" without a handler */
SHELL_CMD_REGISTER(led, &sub_led, "C210 Led testing", NULL);

/* Creating root (level 0) command "version" */
SHELL_CMD_REGISTER(version, NULL, "Get firmware version", xFirmwareVersionType );