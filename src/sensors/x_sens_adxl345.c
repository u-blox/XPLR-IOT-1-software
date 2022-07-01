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
 * @brief Implementation of API for ADXL345 sensor of XPLR-IOT-1.
 *  Also implements the thread controlling the sensor's measurements
 */


#include "x_sens_adxl345.h"
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
void adxl345Thread(void);

/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

// Only used to access the I2C address of the device after the setup
#define ADXL345 DT_INST(0, adi_adxl345)

// In order to use Zephyr's logging module
LOG_MODULE_REGISTER(LOGMOD_NAME_ADXL345, LOG_LEVEL_DBG);


K_THREAD_DEFINE(adxl345_id, ADXL345_STACK_SIZE, adxl345Thread, NULL, NULL, NULL,
		ADXL345_PRIORITY, 0, 0);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/** Device descriptor.
 */
const struct device *gpAdxl345Device;


/** a structure type (common for sensors) to hold info about the status 
 * of the sensor
 */
static xSensStatus_t gSensorStatus = {
	.sensorType = adxl345_t,
	.isReady=false,
	.isPublishEnabled=false, 
	.isEnabled=false, 
	.updatePeriod = ADXL345_DEFAULT_UPDATE_PERIOD_MS
};



/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initializes/Gets the ADXL345 device in the Zephyr context
err_code xSensAdxl345Init(void)
{   
    // Get a device structure from a devicetree node with compatible
    // "adi,adxl345". (If there are multiple, just pick one.)
    gpAdxl345Device = DEVICE_DT_GET_ANY( adi_adxl345 );    
    if ( gpAdxl345Device == NULL ) {
        /* No such node, or the node does not have status "okay". */
        LOG_ERR( "\nNo device found.\n" );
        gSensorStatus.isReady = false;
        return X_ERR_DEVICE_NOT_FOUND;
    }    
    if ( !device_is_ready( gpAdxl345Device ) ) {
        LOG_ERR( "\nDevice \"%s\" is not ready; "
               "check the driver initialization logs for errors.\n",
               gpAdxl345Device->name );
        gSensorStatus.isReady = false;
        return X_ERR_DEVICE_NOT_READY;
    }
    //if device is ok
    else{
        LOG_INF( "Found device \"%s\", on I2C address 0x%02x \n", gpAdxl345Device->name, DT_REG_ADDR( ADXL345 ) );
        gSensorStatus.isReady = true;
        return X_ERR_SUCCESS;
    }
}



// Set the update/sampling period of the sensor
err_code xSensAdxl345SetUpdatePeriod( uint32_t milliseconds ){

    if( !xSensIsChangeAllowed() ){
        LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
        return X_ERR_INVALID_STATE; //invalid state
    }
    
    gSensorStatus.updatePeriod = milliseconds;
    
    LOG_INF("ADXL345 Update Period Set to %d ms", gSensorStatus.updatePeriod);
    return X_ERR_SUCCESS;
}



// Returns the status of the sensor
xSensStatus_t xSensAdxl345GetStatus(void){
	return gSensorStatus;
}



//Disables ADXL345 measurements by suspending the sensor's sampling thread.
err_code xSensAdxl345Disable(void){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE; 
	}

	k_thread_suspend(adxl345_id);
	LOG_INF("%sADXL345 suspended%s \r\n",LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT);
	gSensorStatus.isEnabled = false;
	
	return X_ERR_SUCCESS;
} 



// Enables ADXL345 measurements by resuming/starting the sensor's sampling thread.
int32_t xSensAdxl345Enable(void){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE; 
	}

	k_thread_resume(adxl345_id);
	LOG_INF("%sADXL345 started%s \r\n",LOG_CLRCODE_GREEN, LOG_CLRCODE_DEFAULT);
	gSensorStatus.isEnabled = true;

	return X_ERR_SUCCESS;
}




//Enables/Disables the publish of measurements
err_code xSensAdxl345EnablePublish(bool enable){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE;
	}

	gSensorStatus.isPublishEnabled = enable;
	if( gSensorStatus.isPublishEnabled ){
		LOG_INF("%sADXL345 publish enabled%s \r\n",LOG_CLRCODE_GREEN, LOG_CLRCODE_DEFAULT);
	}
	else{
		LOG_INF("%sADXL345 publish disabled%s \r\n",LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT);
	}

	return X_ERR_SUCCESS;
}



/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */

void adxl345Thread(void){

	int ret = 0;
    struct sensor_value accel[3];
	//used to hold strings which are then typed
	char str[60];
	
	// if device has not been initialized
	if ( !gSensorStatus.isReady ) {
        xSensAdxl345Init();
	}

	/* Initialization of the sensor packet. This packet helps
	* in using the publish to MQTT function. 
	* All values of the packet are kept constant except for the
	* actual measurements (initialized at 0)
	*/
	xDataPacket_t pack = {
		.error = dataErrOk,
		.sensorType = adxl345_t,
		.name = JSON_ID_SENSOR_ADXL345,
		.measurementsNum = 3,
		//measurements
		.meas ={
			// Acceleration X axis
			[0].name = JSON_ID_SENSOR_CHAN_ACCEL_X, 
			[0].type = SENSOR_CHAN_ACCEL_X,
			[0].dataType = isDouble,
			[0].data.doubleVal = 0,
			// Acceleration Y axis
			[1].name = JSON_ID_SENSOR_CHAN_ACCEL_Y, 
			[1].type = SENSOR_CHAN_ACCEL_Y,
			[1].dataType = isDouble,
			[1].data.doubleVal = 0,
			// Acceleration Z axis
			[2].name = JSON_ID_SENSOR_CHAN_ACCEL_Z, 
			[2].type = SENSOR_CHAN_ACCEL_Z,
			[2].dataType = isDouble,
			[2].data.doubleVal = 0
		}
	};

	while (1) {

		// if the device has not been initialized properly
		if( !gSensorStatus.isReady ){
			LOG_ERR("Device cannot be used\r\n");
			if( gSensorStatus.isPublishEnabled ){
				pack.error = dataErrNotInit;
			}
		}

		// else try to read the sensor
		else if( ( ret = sensor_sample_fetch ( gpAdxl345Device ) ) < 0 ){
			LOG_WRN( "Sensor_sample_fetch failed, Errno: %d\n", ret );  
			pack.error = dataErrFetchFail;
		}
		
		// if the reading of the sensor is successful, get the values
		else{

		    if( ( ret = sensor_channel_get( gpAdxl345Device, SENSOR_CHAN_ACCEL_XYZ, accel ) ) < 0 ){
				LOG_ERR("Sensor_channel_get failed, error: %d\n", ret);
				pack.error = dataErrFetchFail;
			}
			// if values were received succesfully
			else{
				// just print to terminal
				sprintf(str,"Accel X=%10.2f Y=%10.2f Z=%10.2f (m/s^2)\n",
					sensor_value_to_double(&accel[0]),
					sensor_value_to_double(&accel[1]),
					sensor_value_to_double(&accel[2]));

				LOG_INF("%s",str); // The LOG_INF without the previous sprintf in this module does not output the values properly

				// prepare data to send
				pack.error = dataErrOk;
				pack.meas[0].data.doubleVal = sensor_value_to_double(&accel[0]);
				pack.meas[1].data.doubleVal = sensor_value_to_double(&accel[1]);
				pack.meas[2].data.doubleVal = sensor_value_to_double(&accel[2]);	
			}
		}

		// publish/send (even if data not written correclty, send error)
		if( gSensorStatus.isPublishEnabled ){
			xDataSend(pack);
		}

		// essentially implements the sampling period
		k_sleep( K_MSEC( gSensorStatus.updatePeriod ) );
	}
}



/* ----------------------------------------------------------------
 * SHELL COMMANDS IMPLEMENTATION
 * -------------------------------------------------------------- */

// Intended to be called by the shell
void xSensAdxl345EnablePublishCmd(const struct shell *shell, size_t argc, char **argv){

        if(argc!=2){
            shell_print(shell, "Invalid number of parameters. Command example: <publish on>\r\n");  
            return;
        }
	
		// check if the parameter is the "on" string, and enable publish
		if( strcmp(argv[1], "on") == 0 ){
			xSensAdxl345EnablePublish(true);
		}
		
		// check if the parameter is the "off" string, and disable publish
		else if( strcmp(argv[1], "off") == 0 ){
			xSensAdxl345EnablePublish(false);
            return;
        }

        else{
            shell_print(shell, "Invalid parameter (on/off)\r\n");  
            return;
		}

}


// Intended to be called by the shell
void xSensAdxl345UpdatePeriodCmd(const struct shell *shell, size_t argc, char **argv)
{
		// todo: add some sanity checks here on the period***
		uint32_t milliseconds = atoi( argv[1]);
		xSensAdxl345SetUpdatePeriod(milliseconds);

		return;
}

