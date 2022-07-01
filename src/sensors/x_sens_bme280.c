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
 * @brief Implementation of API for BME280 sensor of XPLR-IOT-1.
 *  Also implements the thread controlling the sensor's measurements
 */


#include "x_sens_bme280.h"
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
void bme280Thread(void);

/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

// Only used to access the I2C address of the device after the setup
#define BME280 DT_INST(0, bosch_bme280)

// In order to use Zephyr's logging module
LOG_MODULE_REGISTER(LOGMOD_NAME_BME280, LOG_LEVEL_DBG);


K_THREAD_DEFINE(bme280_id, BME280_STACK_SIZE, bme280Thread, NULL, NULL, NULL,
		BME280_PRIORITY, 0, 0);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/** Device descriptor.
 */
const struct device *gpBme280Device;


/** a structure type (common for sensors) to hold info about the status 
 * of the sensor
 */
static xSensStatus_t gSensorStatus = {
	.sensorType= bme280_t,
	.isReady=false,
	.isPublishEnabled=false, 
	.isEnabled=false, 
	.updatePeriod = BME280_DEFAULT_UPDATE_PERIOD_MS
};



/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initializes/Gets the BME280 device in the Zephyr context
err_code xSensBme280Init(void)
{   
    // Get a device structure from a devicetree node with compatible
    // "bosch,bme280". (If there are multiple, just pick one.)
    gpBme280Device = DEVICE_DT_GET_ANY( bosch_bme280 );    
    if ( gpBme280Device == NULL ) {
        /* No such node, or the node does not have status "okay". */
        LOG_ERR( "\nNo device found.\n" );
        gSensorStatus.isReady = false;
        return X_ERR_DEVICE_NOT_FOUND;
    }    
    if ( !device_is_ready( gpBme280Device ) ) {
        LOG_ERR( "\nDevice \"%s\" is not ready; "
               "check the driver initialization logs for errors.\n",
               gpBme280Device->name );
        gSensorStatus.isReady = false;
        return X_ERR_DEVICE_NOT_READY;
    }
    //if device is ok
    else{
        LOG_INF( "Found device \"%s\", on I2C address 0x%02x \n", gpBme280Device->name, DT_REG_ADDR( BME280 ) );
        gSensorStatus.isReady = true;
        return X_ERR_SUCCESS;
    }
}



// Set the update/sampling period of the sensor
err_code xSensBme280SetUpdatePeriod( uint32_t milliseconds ){

    if( !xSensIsChangeAllowed() ){
        LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
        return X_ERR_INVALID_STATE; 
    }
    
    gSensorStatus.updatePeriod = milliseconds;
    
    LOG_INF("BME280 Update Period Set to %d ms", gSensorStatus.updatePeriod);
    return X_ERR_SUCCESS;
}



// Returns the status of the sensor
xSensStatus_t xSensBme280GetStatus(void){
	return gSensorStatus;
}



//Disables BME280 measurements by suspending the sensor's sampling thread.
err_code xSensBme280Disable(void){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE; 
	}

	k_thread_suspend(bme280_id);
	LOG_INF("%sBME280 suspended%s \r\n",LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT);
	gSensorStatus.isEnabled = false;
	
	return X_ERR_SUCCESS;
} 



// Enables BME280 measurements by resuming/starting the sensor's sampling thread.
err_code xSensBme280Enable(void){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE; 
	}

	k_thread_resume(bme280_id);
	LOG_INF("%sBME280 started%s \r\n",LOG_CLRCODE_GREEN, LOG_CLRCODE_DEFAULT);
	gSensorStatus.isEnabled = true;

	return X_ERR_SUCCESS;
}




//Enables/Disables the publish of measurements
err_code xSensBme280EnablePublish(bool enable){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE;
	}

	gSensorStatus.isPublishEnabled = enable;
	if( gSensorStatus.isPublishEnabled ){
		LOG_INF("%sBME280 publish enabled%s \r\n",LOG_CLRCODE_GREEN, LOG_CLRCODE_DEFAULT);
	}
	else{
		LOG_INF("%sBME280 publish disabled%s \r\n",LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT);
	}

	return X_ERR_SUCCESS;
}



/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */

void bme280Thread(void){

	if ( !gSensorStatus.isReady ) {
        xSensBme280Init();
	}

	xDataPacket_t pack = {
		.error = dataErrOk,
		.sensorType = bme280_t,
		.name = JSON_ID_SENSOR_BME280,
		.measurementsNum = 3,
		//measurements
		.meas ={
			// temprerature
			[0].name = JSON_ID_SENSOR_CHAN_AMBIENT_TEMP, 
			[0].type = SENSOR_CHAN_AMBIENT_TEMP,
			[0].dataType = isDouble,
			[0].data.doubleVal = 0,
			// humidity
			[1].name = JSON_ID_SENSOR_CHAN_HUMIDITY, 
			[1].type = SENSOR_CHAN_HUMIDITY,
			[1].dataType = isDouble,
			[1].data.doubleVal = 0,
			// pressure
			[2].name = JSON_ID_SENSOR_CHAN_PRESS, 
			[2].type = SENSOR_CHAN_PRESS,
			[2].dataType = isDouble,
			[2].data.doubleVal = 0
		}
		};

	struct sensor_value temp, press, humidity;
	while (1) {

		// if the device has not been initialized properly
		if( !gSensorStatus.isReady ){
			LOG_ERR( "Device cannot be used\r\n" );
			pack.error = dataErrNotInit;
		}

        // else try to read the sensor
		else if (sensor_sample_fetch ( gpBme280Device) ) {
			LOG_ERR( "Sensor_sample_fetch failed\n" );
			pack.error = dataErrFetchFail;
		}

		// if values were received succesfully
		else{
			sensor_channel_get( gpBme280Device, SENSOR_CHAN_AMBIENT_TEMP, &temp );
			sensor_channel_get( gpBme280Device, SENSOR_CHAN_PRESS, &press );
			sensor_channel_get( gpBme280Device, SENSOR_CHAN_HUMIDITY, &humidity );

			LOG_INF(" Ambient temp: %d.%06d  Press: %d.%06d  Humidity: %d.%06d\r\n",
			      temp.val1, temp.val2, press.val1, press.val2,
			      humidity.val1, humidity.val2);

				  

			// prepare data to send
			pack.error = dataErrOk;
			pack.meas[0].data.doubleVal = sensor_value_to_double(&temp);
			pack.meas[1].data.doubleVal = sensor_value_to_double(&humidity);
			pack.meas[2].data.doubleVal = sensor_value_to_double(&press);	

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
void xSensBme280EnablePublishCmd(const struct shell *shell, size_t argc, char **argv){

        if(argc!=2){
            shell_print(shell, "Invalid number of parameters. Command example: <publish on>\r\n");  
            return;
        }
	
		// check if the parameter is the "on" string, and enable publish
		if( strcmp(argv[1], "on") == 0 ){
			xSensBme280EnablePublish(true);
		}
		
		// check if the parameter is the "off" string, and disable publish
		else if( strcmp(argv[1], "off") == 0 ){
			xSensBme280EnablePublish(false);
            return;
        }

        else{
            shell_print(shell, "Invalid parameter (on/off)\r\n");  
            return;
		}

}


// Intended to be called by the shell
void xSensBme280UpdatePeriodCmd(const struct shell *shell, size_t argc, char **argv)
{
		// todo: add some sanity checks here on the period***
		uint32_t milliseconds = atoi( argv[1]);
		xSensBme280SetUpdatePeriod(milliseconds);

		return;
}

