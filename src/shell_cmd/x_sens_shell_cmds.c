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
 * @brief This file contains "sensors" shell commands implementation and
 * defines the shell commands structure for the "sensors" root command of the
 * XPLR-IOT-1 Sensor Aggregation Use Case
 */


#include <shell/shell.h>
#include "x_sens_bme280.h"
#include "x_sens_adxl345.h"
#include "x_sens_lis3mdl.h"
#include "x_sens_fxas21002.h"
#include "x_sens_lis2dh12.h"
#include "x_sens_ltr303.h"
#include "x_sens_bq27421.h"

#include "x_pos_maxm10s.h"

#include "x_sens_common.h"
#include "x_sens_common_types.h"

#include "x_data_handle.h" //includes sensor strings names as they appear in the mqtt messages



/* ----------------------------------------------------------------
 * COMMAND FUNCTION IMPLEMENTATION (STATIC)
 * -------------------------------------------------------------- */



/** This function is intented only to be used as a command executed by the shell. 
 * This function gets and types the status of all sensors in XPLR-IOT-1 (except
 * for MAXM10S which is conidered a ublox module and not a sensor). 
 *
 * @param shell  the shell instance from which the command is given.
 * @param argc   the number of parameters given along with the command.
 * @param argv   the array including the parameters themselves.
 */
static void xSensCmdTypeStatus(const struct shell *shell, size_t argc, char **argv)
{
        ARG_UNUSED(argc);
        ARG_UNUSED(argv);
        
        // Those arrays hold the strings that should be typed to indicate the status of the sensor
        // to the user
        char *isEnabledStr[2]= {"Suspended", "Running"};
        char *isPublishEnabledStr[2]={ "Disabled", "Enabled" };
        char *isReadyStr[2] = {"Device Not Ok", "Device Ok"};

        // Common sensor structure that contains information about the status of a sensor
        xSensStatus_t SensorStatus;

        // holds the sensor string name
        char SensorNameStr[ JSON_SENSOR_ID_MAXLEN ];

        shell_print(shell,"\r\n ------------------------ Sensor Status ------------------------ \r\n");
        
        // Cycle through all sensors (up to MaxM10S, without it), get and type their status
        uint8_t sensor_idx=0;
        for(sensor_idx = 0; sensor_idx < max_sensors_num_t; sensor_idx++){
                
                switch(sensor_idx){
                        case bme280_t: SensorStatus = xSensBme280GetStatus();
                                        strcpy(SensorNameStr, JSON_ID_SENSOR_BME280);
                                        break;
                        case adxl345_t: SensorStatus = xSensAdxl345GetStatus();
                                        strcpy(SensorNameStr, JSON_ID_SENSOR_ADXL345);
                                        break;
                        case bq27421_t: SensorStatus = xSensBq27421GetStatus();
                                        strcpy(SensorNameStr, JSON_ID_SENSOR_BQ27421);
                                        break;
                        case lis2dh12_t: SensorStatus = xSensLis2dh12GetStatus();
                                        strcpy(SensorNameStr, JSON_ID_SENSOR_LIS2DH12);
                                        break;
                        case lis3mdl_t: SensorStatus = xSensLis3mdlGetStatus();
                                        strcpy(SensorNameStr, JSON_ID_SENSOR_LIS3MDL);
                                        break;
                        case ltr303_t: SensorStatus = xSensLtr303GetStatus();
                                        strcpy(SensorNameStr, JSON_ID_SENSOR_LTR303);
                                        break;
                        case fxas21002_t: SensorStatus = xSensFxas21002GetStatus();
                                        strcpy(SensorNameStr, JSON_ID_SENSOR_FXAS21002);
                                        break;
                        case maxm10_t : SensorStatus = xPosMaxM10GetSensorStatus();
                                        strcpy(SensorNameStr, JSON_ID_SENSOR_MAXM10);
                                        break;
                        default:  break;
                }

           shell_print(shell, "%*s: %15s | %10s | Update Period: %10d ms | MQTT Publish: %10s",
                JSON_SENSOR_ID_MAXLEN,
                SensorNameStr,
                isReadyStr[ (int) SensorStatus.isReady ],
                isEnabledStr[ (int)SensorStatus.isEnabled ],
                SensorStatus.updatePeriod,
                isPublishEnabledStr[(int)SensorStatus.isPublishEnabled]);     
        }


        shell_print(shell,"\r\n ------------------------ ------------- ------------------------ \r\n\r\n");

        return;
}


/* ----------------------------------------------------------------
 * DEFINE SENSORS SHELL COMMAND MENU
 * -------------------------------------------------------------- */


// sensors BME280 sub-commands (level 2)
SHELL_STATIC_SUBCMD_SET_CREATE(BME280,
        SHELL_CMD(enable, NULL, "Enable BME280 measurements (set status to Running)", xSensBme280Enable),
        SHELL_CMD(disable,NULL, "Disable BME280 measurements (set status to Suspended)", xSensBme280Disable),
        SHELL_CMD(set_period,NULL, "Set BME280 period in ms", xSensBme280SetUpdatePeriod),
        SHELL_CMD(publish,NULL, "Publish BME280 measurements: parameters on/off. Eg: publish on ", xSensBme280EnablePublishCmd),
        SHELL_SUBCMD_SET_END
);

//sensors ADXL345 sub-commands (level 2)
SHELL_STATIC_SUBCMD_SET_CREATE(ADXL345,
        SHELL_CMD(enable, NULL, "Enable ADXL345 measurements (set status to Running)", xSensAdxl345Enable),
        SHELL_CMD(disable,NULL, "Disable ADXL345 measurements (set status to Suspended)", xSensAdxl345Disable),
        SHELL_CMD(set_period,NULL, "Set ADXL345 period in ms", xSensAdxl345UpdatePeriodCmd),
        SHELL_CMD(publish,NULL, "Publish ADXL345 measurements: parameters on/off. Eg: publish on ", xSensAdxl345EnablePublishCmd),
        SHELL_SUBCMD_SET_END
);


//sensors LIS3MDL sub-commands (level 2)
SHELL_STATIC_SUBCMD_SET_CREATE(LIS3MDL,
        SHELL_CMD(enable, NULL, "Enable LIS3MDL measurements (set status to Running)", xSensLis3mdlEnable),
        SHELL_CMD(disable,NULL, "Disable LIS3MDL measurements (set status to Suspended)", xSensLis3mdlDisable),
        SHELL_CMD(set_period,NULL, "Set LIS3MDL period in ms", xSensLis3mdlUpdatePeriodCmd),
        SHELL_CMD(publish,NULL, "Publish LIS3MDL measurements: parameters on/off. Eg: publish on ", xSensLis3mdlEnablePublishCmd),
        SHELL_SUBCMD_SET_END
);

//sensors FXAS21002 sub-commands (level 2)
SHELL_STATIC_SUBCMD_SET_CREATE(FXAS21002,
        SHELL_CMD(enable, NULL, "Enable FXAS21002 measurements (set status to Running)", xSensFxas21002Enable),
        SHELL_CMD(disable,NULL, "Disable FXAS21002 measurements (set status to Suspended)", xSensFxas21002Disable),
        SHELL_CMD(set_period,NULL, "Set FXAS21002 period in ms", xSensFxas21002UpdatePeriodCmd),
        SHELL_CMD(publish,NULL, "Publish FXAS21002 measurements: parameters on/off. Eg: publish on ", xSensFxas21002EnablePublishCmd),
        SHELL_SUBCMD_SET_END
);

//sensors LIS2DH12 sub-commands (level 2)
SHELL_STATIC_SUBCMD_SET_CREATE(LIS2DH12,
        SHELL_CMD(enable, NULL, "Enable LIS2DH12 measurements (set status to Running)", xSensLis2dh12Enable),
        SHELL_CMD(disable,NULL, "Disable LIS2DH12 measurements (set status to Suspended)", xSensLis2dh12Disable),
        SHELL_CMD(set_period,NULL, "Set LIS2DH12 period in ms", xSensLis2dh12UpdatePeriodCmd),
        SHELL_CMD(publish,NULL, "Publish LIS2DH12 measurements: parameters on/off. Eg: publish on ", xSensLis2dh12EnablePublishCmd),
        SHELL_SUBCMD_SET_END
);

//sensors LTR303 sub-commands (level 2)
SHELL_STATIC_SUBCMD_SET_CREATE(LTR303,
        SHELL_CMD(enable, NULL, "Enable LTR303 measurements (set status to Running)", xSensLtr303Enable),
        SHELL_CMD(disable,NULL, "Disable LTR303 measurements (set status to Suspended)", xSensLtr303Disable),
        SHELL_CMD(set_period,NULL, "Set LTR303 period in ms", xSensLtr303UpdatePeriodCmd),
        SHELL_CMD(publish,NULL, "Publish LTR303 measurements: parameters on/off. Eg: publish on ", xSensLtr303EnablePublishCmd),
        SHELL_SUBCMD_SET_END
);

//sensors BQ27421 sub-commands (level 2)
SHELL_STATIC_SUBCMD_SET_CREATE(BQ27421,
        SHELL_CMD(enable, NULL, "Enable BQ27421 measurements  (set status to Running)", xSensBq27421Enable),
        SHELL_CMD(disable,NULL, "Disable BQ27421 measurements  (set status to Suspended)", xSensBq27421Disable),
        SHELL_CMD(set_period,NULL, "Set BQ27421 period in ms", xSensBq27421UpdatePeriodCmd),
        SHELL_CMD(publish,NULL, "Publish BQ27421 measurements: parameters on/off. Eg: publish on ", xSensBq27421EnablePublishCmd),
        SHELL_SUBCMD_SET_END
);

//sensors enable sub-commands (level 2)
SHELL_STATIC_SUBCMD_SET_CREATE(enable,
        SHELL_CMD(all, NULL, "Enable all sensor measurements", xSensEnableAll),
        SHELL_CMD(none,NULL, "Disable all sensor measurements", xSensDisableAll),
        SHELL_SUBCMD_SET_END
);

//sensors publish sub-commands (level 2)
SHELL_STATIC_SUBCMD_SET_CREATE(publish,
        SHELL_CMD(all, NULL, "Publish all enabled sensor measurements", xSensPublishAll),
        SHELL_CMD(none,NULL, "Do not publish any sensor measurements", xSensPublishNone),
        SHELL_SUBCMD_SET_END
);

/* Creating subcommands (level 1 command) array for command "sensors". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_sensors,
        SHELL_CMD(BME280, &BME280, "BME280 environmental sensor control.", NULL),
        SHELL_CMD(ADXL345, &ADXL345, "ADXL345 accelerometer sensor control", NULL),
        SHELL_CMD(LIS3MDL, &LIS3MDL, "LIS3MDL magnetometer sensor control", NULL),
        SHELL_CMD(FXAS21002, &FXAS21002, "FXAS21002 gyro sensor control", NULL),
        SHELL_CMD(LIS2DH12, &LIS2DH12, "LIS2DH12 accelerometer sensor control", NULL),
        SHELL_CMD(LTR303, &LTR303, "LTR303 light sensor control", NULL),
        SHELL_CMD(BQ27421, &BQ27421, "BQ27421 fuel gauge control", NULL),
        SHELL_CMD(status,   NULL, "Get sensors current status", xSensCmdTypeStatus),
        SHELL_CMD(enable,   &enable, "Enable/Disable all sensors: <enable all>, <enable none>", NULL),
        SHELL_CMD(publish,   &publish, "Enable/Disable publish of all sensors: <publish all>, <publish none>", NULL),
        SHELL_SUBCMD_SET_END
);


/* Creating root (level 0) command "sensors" without a handler */
SHELL_CMD_REGISTER(sensors, &sub_sensors, "Sensor Control Commands", NULL);
