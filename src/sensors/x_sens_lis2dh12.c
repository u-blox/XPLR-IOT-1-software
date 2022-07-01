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
 * @brief Implementation of API for LIS2DH12 sensor of XPLR-IOT-1.
 *  Also implements the thread controlling the sensor's measurements
 */


#include "x_sens_lis2dh12.h"
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
void lis2dh12Thread(void);

/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

// Only used to access the I2C address of the device after the setup
#define LIS2DH12 DT_INST(0, st_lis2dh)

// In order to use Zephyr's logging module
LOG_MODULE_REGISTER(LOGMOD_NAME_LIS2DH12, LOG_LEVEL_DBG);


K_THREAD_DEFINE(lis2dh12_id, LIS2DH12_STACK_SIZE, lis2dh12Thread, NULL, NULL, NULL,
		LIS2DH12_PRIORITY, 0, 0);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/** Device descriptor.
 */
const struct device *gpLis2dh12Device;


/** a structure type (common for sensors) to hold info about the status 
 * of the sensor
 */
static xSensStatus_t gSensorStatus = {
	.sensorType= lis2dh12_t,
	.isReady=false,
	.isPublishEnabled=false, 
	.isEnabled=false, 
	.updatePeriod = LIS2DH12_DEFAULT_UPDATE_PERIOD_MS
};



/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initializes/Gets the LIS2DH12 device in the Zephyr context
err_code xSensLis2dh12Init(void)
{   
    // Get a device structure from a devicetree node with compatible
    // "st,lis2dh12". (If there are multiple, just pick one.)
    gpLis2dh12Device = DEVICE_DT_GET_ANY( st_lis2dh );    
    if ( gpLis2dh12Device == NULL ) {
        /* No such node, or the node does not have status "okay". */
        LOG_ERR( "\nNo device found.\n" );
        gSensorStatus.isReady = false;
        return X_ERR_DEVICE_NOT_FOUND;
    }    
    if ( !device_is_ready( gpLis2dh12Device ) ) {
        LOG_ERR( "\nDevice \"%s\" is not ready; "
               "check the driver initialization logs for errors.\n",
               gpLis2dh12Device->name );
        gSensorStatus.isReady = false;
        return X_ERR_DEVICE_NOT_READY;
    }
    //if device is ok
    else{
        LOG_INF( "Found device \"%s\", on I2C address 0x%02x \n", gpLis2dh12Device->name, DT_REG_ADDR( LIS2DH12 ) );
        gSensorStatus.isReady = true;
        return X_ERR_SUCCESS;
    }
}



// Set the update/sampling period of the sensor
err_code xSensLis2dh12SetUpdatePeriod( uint32_t milliseconds ){

    if( !xSensIsChangeAllowed() ){
        LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
        return X_ERR_INVALID_STATE; 
    }
    
    gSensorStatus.updatePeriod = milliseconds;
    
    LOG_INF("LIS2DH12 Update Period Set to %d ms", gSensorStatus.updatePeriod);
    return X_ERR_SUCCESS;
}



// Returns the status of the sensor
xSensStatus_t xSensLis2dh12GetStatus(void){
	return gSensorStatus;
}



//Disables LIS2DH12 measurements by suspending the sensor's sampling thread.
err_code xSensLis2dh12Disable(void){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE; 
	}

	k_thread_suspend(lis2dh12_id);
	LOG_INF("%sLIS2DH12 suspended%s \r\n",LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT);
	gSensorStatus.isEnabled = false;
	
	return X_ERR_SUCCESS;
} 



// Enables LIS2DH12 measurements by resuming/starting the sensor's sampling thread.
err_code xSensLis2dh12Enable(void){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE; 
	}

	k_thread_resume(lis2dh12_id);
	LOG_INF("%sLIS2DH12 started%s \r\n",LOG_CLRCODE_GREEN, LOG_CLRCODE_DEFAULT);
	gSensorStatus.isEnabled = true;

	return X_ERR_SUCCESS;
}




//Enables/Disables the publish of measurements
err_code xSensLis2dh12EnablePublish(bool enable){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE;
	}

	gSensorStatus.isPublishEnabled = enable;
	if( gSensorStatus.isPublishEnabled ){
		LOG_INF("%sLIS2DH12 publish enabled%s \r\n",LOG_CLRCODE_GREEN, LOG_CLRCODE_DEFAULT);
	}
	else{
		LOG_INF("%sLIS2DH12 publish disabled%s \r\n",LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT);
	}

	return X_ERR_SUCCESS;
}



/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */

void lis2dh12Thread(void){

	if ( !gSensorStatus.isReady ) {
        xSensLis2dh12Init();
	}

	struct sensor_value accel[3];
    char str[60];
    //int rc;
    // use when not in poll mode
    //bool overrun = false;

	xDataPacket_t pack = {
		.error = dataErrOk,
		.sensorType = lis2dh12_t,
		.name = JSON_ID_SENSOR_LIS2DH12,
		.measurementsNum = 3,
		//measurements
		.meas ={
			// Accelerometer Axis X
			[0].name = JSON_ID_SENSOR_CHAN_ACCEL_X, 
			[0].type = SENSOR_CHAN_ACCEL_X,
			[0].dataType = isDouble,
			[0].data.doubleVal = 0,
			// Accelerometer Axis Y
			[1].name = JSON_ID_SENSOR_CHAN_ACCEL_Y, 
			[1].type = SENSOR_CHAN_ACCEL_Y,
			[1].dataType = isDouble,
			[1].data.doubleVal = 0,
			// Accelerometer Axis Z
			[2].name = JSON_ID_SENSOR_CHAN_ACCEL_Z, 
			[2].type = SENSOR_CHAN_ACCEL_Z,
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
		else if( sensor_sample_fetch ( gpLis2dh12Device ) < 0 ){
			LOG_ERR( "Sensor_sample_fetch failed\n" );
			// Use overrun check when not in poll mode
        	/*
        	if ( rc == -EBADMSG ) {
				// Sample overrun.  Ignore in polled mode. 
			    if (IS_ENABLED(CONFIG_LIS2DH_TRIGGER)) {
				    overrun = true;
			    }
			}
        	*/	
		}

		// if values were received succesfully
		else{
			sensor_channel_get( gpLis2dh12Device, SENSOR_CHAN_ACCEL_XYZ, accel );
            sprintf( str, "Accel X: %f 	, Y: %f , Z: %f\n",
		       sensor_value_to_double(&accel[0]),
		       sensor_value_to_double(&accel[1]),
		       sensor_value_to_double(&accel[2]));

            LOG_INF("%s",str);  // The LOG_INF without the previous sprintf in this module does not output the values properly

			// prepare data to send
			pack.error = dataErrOk;
			pack.meas[0].data.doubleVal = sensor_value_to_double(&accel[0]);
			pack.meas[1].data.doubleVal = sensor_value_to_double(&accel[1]);
			pack.meas[2].data.doubleVal = sensor_value_to_double(&accel[2]);	

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
void xSensLis2dh12EnablePublishCmd(const struct shell *shell, size_t argc, char **argv){

        if(argc!=2){
            shell_print(shell, "Invalid number of parameters. Command example: <publish on>\r\n");  
            return;
        }
	
		// check if the parameter is the "on" string, and enable publish
		if( strcmp(argv[1], "on") == 0 ){
			xSensLis2dh12EnablePublish(true);
		}
		
		// check if the parameter is the "off" string, and disable publish
		else if( strcmp(argv[1], "off") == 0 ){
			xSensLis2dh12EnablePublish(false);
            return;
        }

        else{
            shell_print(shell, "Invalid parameter (on/off)\r\n");  
            return;
		}

}


// Intended to be called by the shell
void xSensLis2dh12UpdatePeriodCmd(const struct shell *shell, size_t argc, char **argv)
{
		// todo: add some sanity checks here on the period***
		uint32_t milliseconds = atoi( argv[1]);
		xSensLis2dh12SetUpdatePeriod(milliseconds);

		return;
}














