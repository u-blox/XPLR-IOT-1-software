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
 * @brief Implementation of API for ICG20330 sensor of XPLR-IOT-1.
 *  Also implements the thread controlling the sensor's measurements
 */


#include "x_sens_icg20330.h"
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
void icg20330Thread(void);

/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

// Only used to access the I2C address of the device after the setup
#define ICG20330 DT_INST(0, tdk_icg20330)

// In order to use Zephyr's logging module
LOG_MODULE_REGISTER(LOGMOD_NAME_ICG20330, LOG_LEVEL_DBG);


K_THREAD_DEFINE(icg20330_id, ICG20330_STACK_SIZE, icg20330Thread, NULL, NULL, NULL,
		ICG20330_PRIORITY, 0, 0);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/** Device descriptor.
 */
const struct device *gpIcg20330Device;


/** a structure type (common for sensors) to hold info about the status 
 * of the sensor
 */
static xSensStatus_t gSensorStatus = {
	.sensorType= icg20330_t,
	.isReady=false,
	.isPublishEnabled=false, 
	.isEnabled=false, 
	.updatePeriod = ICG20330_DEFAULT_UPDATE_PERIOD_MS
};



/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initializes/Gets the ICG20330 device in the Zephyr context
err_code xSensIcg20330Init(void)
{   
    // Get a device structure from a devicetree node with compatible
    // "tdk,icg20330". (If there are multiple, just pick one.)
    gpIcg20330Device = DEVICE_DT_GET_ANY( tdk_icg20330 );    
    if ( gpIcg20330Device == NULL ) {
        /* No such node, or the node does not have status "okay". */
        LOG_ERR( "\nNo device found.\n" );
        gSensorStatus.isReady = false;
        return X_ERR_DEVICE_NOT_FOUND;
    }    
    if ( !device_is_ready( gpIcg20330Device ) ) {
        LOG_ERR( "\nDevice \"%s\" is not ready; "
               "check the driver initialization logs for errors.\n",
               gpIcg20330Device->name );
        gSensorStatus.isReady = false;
        return X_ERR_DEVICE_NOT_READY;
    }
    //if device is ok
    else{
        LOG_INF( "Found device \"%s\", on I2C address 0x%02x \n", gpIcg20330Device->name, DT_REG_ADDR( ICG20330 ) );
        gSensorStatus.isReady = true;
        return X_ERR_SUCCESS;
    }
}



// Set the update/sampling period of the sensor
err_code xSensIcg20330SetUpdatePeriod( uint32_t milliseconds ){

    if( !xSensIsChangeAllowed() ){
        LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
        return X_ERR_INVALID_STATE; 
    }
    
    gSensorStatus.updatePeriod = milliseconds;
    
    LOG_INF("ICG20330 Update Period Set to %d ms", gSensorStatus.updatePeriod);
    return X_ERR_SUCCESS;
}



// Returns the status of the sensor
xSensStatus_t xSensIcg20330GetStatus(void){
	return gSensorStatus;
}



//Disables ICG20330 measurements by suspending the sensor's sampling thread.
err_code xSensIcg20330Disable(void){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE; 
	}

	k_thread_suspend(icg20330_id);
	LOG_INF("%sICG20330 suspended%s \r\n",LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT);
	gSensorStatus.isEnabled = false;
	
	return X_ERR_SUCCESS;
} 



// Enables ICG20330 measurements by resuming/starting the sensor's sampling thread.
err_code xSensIcg20330Enable(void){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE; 
	}

	k_thread_resume(icg20330_id);
	LOG_INF("%sICG20330 started%s \r\n",LOG_CLRCODE_GREEN, LOG_CLRCODE_DEFAULT);
	gSensorStatus.isEnabled = true;

	return X_ERR_SUCCESS;
}




//Enables/Disables the publish of measurements
err_code xSensIcg20330EnablePublish(bool enable){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE;
	}

	gSensorStatus.isPublishEnabled = enable;
	if( gSensorStatus.isPublishEnabled ){
		LOG_INF("%sICG20330 publish enabled%s \r\n",LOG_CLRCODE_GREEN, LOG_CLRCODE_DEFAULT);
	}
	else{
		LOG_INF("%sICG20330 publish disabled%s \r\n",LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT);
	}

	return X_ERR_SUCCESS;
}



/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


void icg20330Thread(void){

	if ( !gSensorStatus.isReady ) {
        xSensIcg20330Init();
	}

	struct sensor_value gyro[3];
    char str[60];

	xDataPacket_t pack = {
		.error = dataErrOk,
		.sensorType = icg20330_t,
		.name = JSON_ID_SENSOR_ICG20330,
		.measurementsNum = 3,
		//measurements
		.meas ={
			// Gyroscope Axis X
			[0].name = JSON_ID_SENSOR_CHAN_GYRO_X, 
			[0].type = SENSOR_CHAN_GYRO_X,
			[0].dataType = isDouble,
			[0].data.doubleVal = 0,
			// Gyroscope Axis Y
			[1].name = JSON_ID_SENSOR_CHAN_GYRO_Y, 
			[1].type = SENSOR_CHAN_GYRO_Y,
			[1].dataType = isDouble,
			[1].data.doubleVal = 0,
			// Gyroscope Axis Z
			[2].name = JSON_ID_SENSOR_CHAN_GYRO_Z, 
			[2].type = SENSOR_CHAN_GYRO_Z,
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
		else if (sensor_sample_fetch ( gpIcg20330Device) ) {
			LOG_ERR( "Sensor_sample_fetch failed\n" );
			pack.error = dataErrFetchFail;
		}
		
		else{
			sensor_channel_get( gpIcg20330Device, SENSOR_CHAN_GYRO_XYZ, gyro );

			/* Print gyro x,y,z */
		    sprintf(str, "Gyro X=%10.3f Y=%10.3f Z=%10.3f\n",
		       sensor_value_to_double(&gyro[0]),
		       sensor_value_to_double(&gyro[1]),
		       sensor_value_to_double(&gyro[2]));

            LOG_INF("%s",str);		// The LOG_INF without the previous sprintf in this module does not output the values properly

			// prepare data to send
			pack.error = dataErrOk;
			pack.meas[0].data.doubleVal = sensor_value_to_double(&gyro[0]);
			pack.meas[1].data.doubleVal = sensor_value_to_double(&gyro[1]);
			pack.meas[2].data.doubleVal = sensor_value_to_double(&gyro[2]);	
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
void xSensIcg20330EnablePublishCmd(const struct shell *shell, size_t argc, char **argv){

        if(argc!=2){
            shell_print(shell, "Invalid number of parameters. Command example: <publish on>\r\n");  
            return;
        }
	
		// check if the parameter is the "on" string, and enable publish
		if( strcmp(argv[1], "on") == 0 ){
			xSensIcg20330EnablePublish(true);
		}
		
		// check if the parameter is the "off" string, and disable publish
		else if( strcmp(argv[1], "off") == 0 ){
			xSensIcg20330EnablePublish(false);
            return;
        }

        else{
            shell_print(shell, "Invalid parameter (on/off)\r\n");  
            return;
		}

}


// Intended to be called by the shell
void xSensIcg20330UpdatePeriodCmd(const struct shell *shell, size_t argc, char **argv)
{
		// todo: add some sanity checks here on the period***
		uint32_t milliseconds = atoi( argv[1]);
		xSensIcg20330SetUpdatePeriod(milliseconds);

		return;
}

















