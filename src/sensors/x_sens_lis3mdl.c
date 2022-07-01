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
 * @brief Implementation of API for LIS3MDL sensor of XPLR-IOT-1.
 *  Also implements the thread controlling the sensor's measurements
 */


#include "x_sens_lis3mdl.h"
#include <stdlib.h>
#include <stdio.h>
#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/sensor.h>


#include "x_system_conf.h"     //get priorities, stack sized and default values for threads
#include "x_logging.h"    //get the name for the logging module
#include "x_data_handle.h" //enables mqtt publish 
#include "x_sens_common.h"  //for the sensor status structure


/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION
 * -------------------------------------------------------------- */

// This thread implements and controls the measurements of the sensor
// and their ability to publish or not.
void lis3mdlThread(void);

/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

// Only used to access the I2C address of the device after the setup
#define LIS3MDL DT_INST(0, st_lis3mdl_magn)

// In order to use Zephyr's logging module
LOG_MODULE_REGISTER(LOGMOD_NAME_LIS3MDL, LOG_LEVEL_DBG);


K_THREAD_DEFINE(lis3mdl_id, LIS3MDL_STACK_SIZE, lis3mdlThread, NULL, NULL, NULL,
		LIS3MDL_PRIORITY, 0, 0);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/** Device descriptor.
 */
const struct device *gpLis3mdlDevice;


/** a structure type (common for sensors) to hold info about the status 
 * of the sensor
 */
static xSensStatus_t gSensorStatus = {
	.sensorType= lis3mdl_t,
	.isReady=false,
	.isPublishEnabled=false, 
	.isEnabled=false, 
	.updatePeriod = LIS3MDL_DEFAULT_UPDATE_PERIOD_MS
};



/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initializes/Gets the LIS3MDL device in the Zephyr context
err_code xSensLis3mdlInit(void)
{   
    // Get a device structure from a devicetree node with compatible
    // "bosch,lis3mdl". (If there are multiple, just pick one.)
    gpLis3mdlDevice = DEVICE_DT_GET_ANY( st_lis3mdl_magn );    
    if ( gpLis3mdlDevice == NULL ) {
        /* No such node, or the node does not have status "okay". */
        LOG_ERR( "\nNo device found.\n" );
        gSensorStatus.isReady = false;
        return X_ERR_DEVICE_NOT_FOUND;
    }    
    if ( !device_is_ready( gpLis3mdlDevice ) ) {
        LOG_ERR( "\nDevice \"%s\" is not ready; "
               "check the driver initialization logs for errors.\n",
               gpLis3mdlDevice->name );
        gSensorStatus.isReady = false;
        return X_ERR_DEVICE_NOT_READY;
    }
    //if device is ok
    else{
        LOG_INF( "Found device \"%s\", on I2C address 0x%02x \n", gpLis3mdlDevice->name, DT_REG_ADDR( LIS3MDL ) );
        gSensorStatus.isReady = true;
        return X_ERR_SUCCESS;
    }
}



// Set the update/sampling period of the sensor
err_code xSensLis3mdlSetUpdatePeriod( uint32_t milliseconds ){

    if( !xSensIsChangeAllowed() ){
        LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
        return X_ERR_INVALID_STATE;
    }
    
    gSensorStatus.updatePeriod = milliseconds;
    
    LOG_INF("LIS3MDL Update Period Set to %d ms", gSensorStatus.updatePeriod);
    return X_ERR_SUCCESS;
}



// Returns the status of the sensor
xSensStatus_t xSensLis3mdlGetStatus(void){
	return gSensorStatus;
}



//Disables LIS3MDL measurements by suspending the sensor's sampling thread.
err_code xSensLis3mdlDisable(void){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE;
	}

	k_thread_suspend(lis3mdl_id);
	LOG_INF("%sLIS3MDL suspended%s \r\n",LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT);
	gSensorStatus.isEnabled = false;
	
	return X_ERR_SUCCESS;
} 



// Enables LIS3MDL measurements by resuming/starting the sensor's sampling thread.
err_code xSensLis3mdlEnable(void){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE; 
	}

	k_thread_resume(lis3mdl_id);
	LOG_INF("%sLIS3MDL started%s \r\n",LOG_CLRCODE_GREEN, LOG_CLRCODE_DEFAULT);
	gSensorStatus.isEnabled = true;

	return X_ERR_SUCCESS;
}




//Enables/Disables the publish of measurements
err_code xSensLis3mdlEnablePublish(bool enable){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE;
	}

	gSensorStatus.isPublishEnabled = enable;
	if( gSensorStatus.isPublishEnabled ){
		LOG_INF("%sLIS3MDL publish enabled%s \r\n",LOG_CLRCODE_GREEN, LOG_CLRCODE_DEFAULT);
	}
	else{
		LOG_INF("%sLIS3MDL publish disabled%s \r\n",LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT);
	}

	return X_ERR_SUCCESS;
}



/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */

void lis3mdlThread(void){

	if ( gpLis3mdlDevice == NULL ) {
        xSensLis3mdlInit();
	}

	struct sensor_value mag[3];
	char str[60];

	xDataPacket_t pack = {
		.error = dataErrOk,
		.sensorType = lis3mdl_t,
		.name = JSON_ID_SENSOR_LIS3MDL,
		.measurementsNum = 3,
		//measurements
		.meas ={
			// Magnetometer Axis X
			[0].name = JSON_ID_SENSOR_CHAN_MAGN_X, 
			[0].type = SENSOR_CHAN_MAGN_X,
			[0].dataType = isDouble,
			[0].data.doubleVal = 0,
			// Magnetometer Axis Y
			[1].name = JSON_ID_SENSOR_CHAN_MAGN_Y, 
			[1].type = SENSOR_CHAN_MAGN_Y,
			[1].dataType = isDouble,
			[1].data.doubleVal = 0,
			// Magnetometer Axis Z
			[2].name = JSON_ID_SENSOR_CHAN_MAGN_Z, 
			[2].type = SENSOR_CHAN_MAGN_Z,
			[2].dataType = isDouble,
			[2].data.doubleVal = 0
		}
	};

	while (1) {

        // if the device has not been initialized properly
		if( !gSensorStatus.isReady ){
			LOG_ERR( "Device cannot be used\r\n" );
			pack.error = dataErrNotInit;
		}

        // else try to read the sensor
		else if( sensor_sample_fetch ( gpLis3mdlDevice ) ) {
			LOG_ERR( "Sensor_sample_fetch failed\n" );
			pack.error = dataErrFetchFail;
		}

        // if values were received succesfully
		else{
			sensor_channel_get( gpLis3mdlDevice, SENSOR_CHAN_MAGN_XYZ, mag );

			sprintf(str,"Mag X=%10.2f Y=%10.2f Z=%10.2f\n",
			    sensor_value_to_double(&mag[0]),
			    sensor_value_to_double(&mag[1]),
			    sensor_value_to_double(&mag[2]));

			LOG_INF("%s",str);		// The LOG_INF without the previous sprintf in this module does not output the values properly

			// prepare data to send
			pack.error = dataErrOk;
			pack.meas[0].data.doubleVal = sensor_value_to_double(&mag[0]);
			pack.meas[1].data.doubleVal = sensor_value_to_double(&mag[1]);
			pack.meas[2].data.doubleVal = sensor_value_to_double(&mag[2]);	

		}

		// publish/send (even if data not written correclty, send error)
		if( gSensorStatus.isPublishEnabled ){				
			xDataSend( pack );
		}

        // essentially implements the sampling period
		k_sleep( K_MSEC( gSensorStatus.updatePeriod ) );
	}
}



/* ----------------------------------------------------------------
 * SHELL COMMANDS IMPLEMENTATION
 * -------------------------------------------------------------- */

// Intended to be called by the shell
void xSensLis3mdlEnablePublishCmd(const struct shell *shell, size_t argc, char **argv){

        if(argc!=2){
            shell_print(shell, "Invalid number of parameters. Command example: <publish on>\r\n");  
            return;
        }
	
		// check if the parameter is the "on" string, and enable publish
		if( strcmp(argv[1], "on") == 0 ){
			xSensLis3mdlEnablePublish(true);
		}
		
		// check if the parameter is the "off" string, and disable publish
		else if( strcmp(argv[1], "off") == 0 ){
			xSensLis3mdlEnablePublish(false);
            return;
        }

        else{
            shell_print(shell, "Invalid parameter (on/off)\r\n");  
            return;
		}

}


// Intended to be called by the shell
void xSensLis3mdlUpdatePeriodCmd(const struct shell *shell, size_t argc, char **argv)
{
		// todo: add some sanity checks here on the period***
		uint32_t milliseconds = atoi( argv[1]);
		xSensLis3mdlSetUpdatePeriod(milliseconds);

		return;
}












