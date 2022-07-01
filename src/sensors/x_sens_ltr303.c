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
 * @brief Implementation of API for LTR303 sensor of XPLR-IOT-1.
 *  Also implements the thread controlling the sensor's measurements
 */


#include "x_sens_ltr303.h"
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
 * DEFINITIONS
 * -------------------------------------------------------------- */

// Used to convert adc values from the sensor to lux
#define ALS_GAIN 1
#define ALS_INT 2
#define pFfactor .16

/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION
 * -------------------------------------------------------------- */

// This thread implements and controls the measurements of the sensor
// and their ability to publish or not.
void ltr303Thread(void);

//Converts ADC value obtained for the sensor to Lux.
static int32_t ltr303Convert2Lux(struct sensor_value *adc_val);

/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

// Only used to access the I2C address of the device after the setup
#define LTR303 DT_INST(0, ltr_303als)

// In order to use Zephyr's logging module
LOG_MODULE_REGISTER(LOGMOD_NAME_LTR303, LOG_LEVEL_DBG);


K_THREAD_DEFINE(ltr303_id, LTR303_STACK_SIZE, ltr303Thread, NULL, NULL, NULL,
		LTR303_PRIORITY, 0, 0);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/** Device descriptor.
 */
const struct device *gpLtr303Device;


/** a structure type (common for sensors) to hold info about the status 
 * of the sensor
 */
static xSensStatus_t gSensorStatus = {
	.sensorType= ltr303_t,
	.isReady=false,
	.isPublishEnabled=false, 
	.isEnabled=false, 
	.updatePeriod = LTR303_DEFAULT_UPDATE_PERIOD_MS
};



/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initializes/Gets the LTR303 device in the Zephyr context
err_code xSensLtr303Init(void)
{   
    // Get a device structure from a devicetree node with compatible
    // "bosch,ltr303". (If there are multiple, just pick one.)
    gpLtr303Device = DEVICE_DT_GET_ANY( ltr_303als );    
    if ( gpLtr303Device == NULL ) {
        /* No such node, or the node does not have status "okay". */
        LOG_ERR( "\nNo device found.\n" );
        gSensorStatus.isReady = false;
        return X_ERR_DEVICE_NOT_FOUND;
    }    
    if ( !device_is_ready( gpLtr303Device ) ) {
        LOG_ERR( "\nDevice \"%s\" is not ready; "
               "check the driver initialization logs for errors.\n",
               gpLtr303Device->name );
        gSensorStatus.isReady = false;
        return X_ERR_DEVICE_NOT_READY;
    }
    //if device is ok
    else{
        LOG_INF( "Found device \"%s\", on I2C address 0x%02x \n", gpLtr303Device->name, DT_REG_ADDR( LTR303 ) );
        gSensorStatus.isReady = true;
        return X_ERR_SUCCESS;
    }
}



// Set the update/sampling period of the sensor
err_code xSensLtr303SetUpdatePeriod( uint32_t milliseconds ){

    if( !xSensIsChangeAllowed() ){
        LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
        return X_ERR_INVALID_STATE; 
    }
    
    gSensorStatus.updatePeriod = milliseconds;
    
    LOG_INF("LTR303 Update Period Set to %d ms", gSensorStatus.updatePeriod);
    return X_ERR_SUCCESS;
}



// Returns the status of the sensor
xSensStatus_t xSensLtr303GetStatus(void){
	return gSensorStatus;
}



//Disables LTR303 measurements by suspending the sensor's sampling thread.
err_code xSensLtr303Disable(void){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE; 
	}

	k_thread_suspend(ltr303_id);
	LOG_INF("%sLTR303 suspended%s \r\n",LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT);
	gSensorStatus.isEnabled = false;
	
	return X_ERR_SUCCESS;
} 



// Enables LTR303 measurements by resuming/starting the sensor's sampling thread.
err_code xSensLtr303Enable(void){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE; 
	}

	k_thread_resume(ltr303_id);
	LOG_INF("%sLTR303 started%s \r\n",LOG_CLRCODE_GREEN, LOG_CLRCODE_DEFAULT);
	gSensorStatus.isEnabled = true;

	return X_ERR_SUCCESS;
}




//Enables/Disables the publish of measurements
err_code xSensLtr303EnablePublish(bool enable){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE;
	}

	gSensorStatus.isPublishEnabled = enable;
	if( gSensorStatus.isPublishEnabled ){
		LOG_INF("%sLTR303 publish enabled%s \r\n",LOG_CLRCODE_GREEN, LOG_CLRCODE_DEFAULT);
	}
	else{
		LOG_INF("%sLTR303 publish disabled%s \r\n",LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT);
	}

	return X_ERR_SUCCESS;
}



/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */

static int32_t ltr303Convert2Lux(struct sensor_value *adc_val)
{
    int32_t newval, ch0, ch1; 
    int32_t r1, r2, r3;

    ch0 = adc_val->val1;
    ch1 = adc_val->val2;

    // return lux conversion in adc.val1
    r1 = (ch1*100);
    r2 = (ch0+ch1);
    if ((r1 != 0) || (r2 != 0) ) {
        r3 = r1/r2;
    } else {
        r3 = 0;
    }
    
    if (r3 < 45) {
        newval = (1.7743 * ch0 + 1.1059 * ch1)/ALS_GAIN/ALS_INT/pFfactor;
    } else if  ((r3 < 64) && (r3 >= 45) ) {
        newval = (4.2785 * ch0 - 1.9548 * ch1)/ALS_GAIN/ALS_INT/pFfactor;
    } else if  ((r3 < 85) && (r3 >= 64) ) {
       newval = (.5926 * ch0 + .1185 * ch1)/ALS_GAIN/ALS_INT/pFfactor;
    } else {
       newval = 0;
    }
    //adc_val->val1 = newval;
	return newval;
}



void ltr303Thread(void){

	if (gpLtr303Device == NULL) {
        xSensLtr303Init();
	}

	struct sensor_value adc;
	int32_t lightLux;
	char str[60];

	xDataPacket_t pack = {
		.error = dataErrOk,
		.sensorType = ltr303_t,
		.name = JSON_ID_SENSOR_LTR303,
		.measurementsNum = 1,
		//measurements
		.meas ={
			// Light
			[0].name = JSON_ID_SENSOR_CHAN_LIGHT, 
			[0].type = SENSOR_CHAN_LIGHT,
			[0].dataType = isInt,
			[0].data.int32Val = 0
		}
	};

	while (1) {

		if( !gSensorStatus.isReady ){
			LOG_ERR("Device cannot be used\r\n");
			pack.error = dataErrNotInit;
		}

		else if (sensor_sample_fetch ( gpLtr303Device) ) {
			LOG_ERR("Sensor_sample_fetch failed\n");
			pack.error = dataErrFetchFail;
		}

		else{
			sensor_channel_get(gpLtr303Device, SENSOR_CHAN_LIGHT, &adc);
            lightLux = ltr303Convert2Lux(&adc);

			sprintf(str,"Light Sensor Lux: %d \r\n", lightLux);

			LOG_INF("%s",str);		// The LOG_INF without the previous sprintf in this module does not output the values properly

			// prepare data to send
			pack.error = dataErrOk;
			pack.meas[0].data.int32Val = lightLux;


		}

		if( gSensorStatus.isPublishEnabled ){
			xDataSend(pack);
		}
	
        // todo: check yield methods
		k_sleep(K_MSEC(gSensorStatus.updatePeriod));
	}
}



/* ----------------------------------------------------------------
 * SHELL COMMANDS IMPLEMENTATION
 * -------------------------------------------------------------- */

// Intended to be called by the shell
void xSensLtr303EnablePublishCmd(const struct shell *shell, size_t argc, char **argv){

        if(argc!=2){
            shell_print(shell, "Invalid number of parameters. Command example: <publish on>\r\n");  
            return;
        }
	
		// check if the parameter is the "on" string, and enable publish
		if( strcmp(argv[1], "on") == 0 ){
			xSensLtr303EnablePublish(true);
		}
		
		// check if the parameter is the "off" string, and disable publish
		else if( strcmp(argv[1], "off") == 0 ){
			xSensLtr303EnablePublish(false);
            return;
        }

        else{
            shell_print(shell, "Invalid parameter (on/off)\r\n");  
            return;
		}

}


// Intended to be called by the shell
void xSensLtr303UpdatePeriodCmd(const struct shell *shell, size_t argc, char **argv)
{
		// todo: add some sanity checks here on the period***
		uint32_t milliseconds = atoi( argv[1]);
		xSensLtr303SetUpdatePeriod(milliseconds);

		return;
}

