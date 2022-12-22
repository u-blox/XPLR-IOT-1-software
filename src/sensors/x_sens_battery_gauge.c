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
 * @brief Implementation of API for Battery Gauge sensor of XPLR-IOT-1.
 *  Also implements the thread controlling the sensor's measurements
 */


#include "x_sens_battery_gauge.h"
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

/** This functions initializes/gets the Bq27520 battery gauge device
 *  in the Zephyr context.
 *
 * @return        zero on success (X_ERR_SUCCESS) else negative error code.
 */
static err_code xSensBq27520Init(void);


/** This functions initializes/gets the Bq27421 battery gauge device
 *  in the Zephyr context.
 *
 * @return        zero on success (X_ERR_SUCCESS) else negative error code.
 */
static err_code xSensBq27421Init(void);


// This thread implements and controls the measurements of the sensor
// and their ability to publish or not.
void batGaugeThread(void);


// Helper function to type a measurement of the Fuel Gauge
static void batGaugeShowValues(const char *type_str, struct sensor_value value);


/** Hepler function to get the string representation of a certain measurement channel.
 * 
 * @param type            [Input] Channel fpr which the string representation is asked
 * @param string          [Output] The string representation of the measurement
 * @param max_string_len  [input] The size of the string buffer
 * 
 * @return                zero on success (X_ERR_SUCCESS) else negative error code.
 */
int32_t batGaugeGetChannelString( enum sensor_channel type, char *string, uint8_t max_string_len );


// Helper function to get measurement channel
int32_t batGaugeReadValue( enum sensor_channel type, struct sensor_value *value );



/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

// Only used to access the I2C address of the device after the setup
#define BQ27520 DT_INST(0, ti_bq27520)
#define BQ27421 DT_INST(0, ti_bq274xx)

// In order to use Zephyr's logging module
LOG_MODULE_REGISTER(LOGMOD_NAME_BQ27520, LOG_LEVEL_DBG);


K_THREAD_DEFINE(bat_gauge_id, BAT_GAUGE_STACK_SIZE, batGaugeThread, NULL, NULL, NULL,
		BAT_GAUGE_PRIORITY, 0, 0);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/** Device descriptor.
 */
const struct device *gpBatteryGaugeDevice;


/** a structure type (common for sensors) to hold info about the status 
 * of the sensor
 */
static xSensStatus_t gSensorStatus = {
	.sensorType= battery_gauge_t,
	.isReady=false,
	.isPublishEnabled=false, 
	.isEnabled=false, 
	.updatePeriod = BAT_GAUGE_DEFAULT_UPDATE_PERIOD_MS
};


struct sensor_value gVoltageV,  //Voltage in Volts   
					current,	 
					gSoc,       //state of charge (%)
					full_charge_capacity,
					remaining_charge_capacity, 
					avg_power,
					int_temp,
					current_standby,
					current_max_load,
					state_of_health;



/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initializes/Gets the BQ27520 device in the Zephyr context
err_code xSensBatGaugeInit(void)
{   
    err_code ret;
	ret = xSensBq27520Init();
	if( ret == X_ERR_SUCCESS ){

		return X_ERR_SUCCESS;	
	}

	// xSensBq27520Init was not successful, maybe this device
	// has a Bq27421 battery gauge
	ret = xSensBq27421Init();
	return ret;

}


// Set the update/sampling period of the sensor
err_code xSensBatGaugeSetUpdatePeriod( uint32_t milliseconds ){

    if( !xSensIsChangeAllowed() ){
        LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
        return X_ERR_INVALID_STATE; 
    }
    
    gSensorStatus.updatePeriod = milliseconds;
    
    LOG_INF("Battery Gauge Update Period Set to %d ms", gSensorStatus.updatePeriod);
    return X_ERR_SUCCESS;
}



// Returns the status of the sensor
xSensStatus_t xSensBatGaugeGetStatus(void){
	return gSensorStatus;
}



//Disables Battery Gauge measurements by suspending the sensor's sampling thread.
err_code xSensBatGaugeDisable(void){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE; 
	}

	k_thread_suspend(bat_gauge_id);
	LOG_INF("%sBattery Gauge suspended%s \r\n",LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT);
	gSensorStatus.isEnabled = false;
	
	return X_ERR_SUCCESS;
} 



// Enables Battery Gauge measurements by resuming/starting the sensor's sampling thread.
err_code xSensBatGaugeEnable(void){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE; 
	}

	k_thread_resume(bat_gauge_id);
	LOG_INF("%sBattery Gauge started%s \r\n",LOG_CLRCODE_GREEN, LOG_CLRCODE_DEFAULT);
	gSensorStatus.isEnabled = true;

	return X_ERR_SUCCESS;
}




//Enables/Disables the publish of measurements
err_code xSensBatGaugeEnablePublish(bool enable){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE;
	}

	gSensorStatus.isPublishEnabled = enable;
	if( gSensorStatus.isPublishEnabled ){
		LOG_INF("%sBattery Gauge publish enabled%s \r\n",LOG_CLRCODE_GREEN, LOG_CLRCODE_DEFAULT);
	}
	else{
		LOG_INF("%sBattery Gauge publish disabled%s \r\n",LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT);
	}

	return X_ERR_SUCCESS;
}



/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


// Initializes/Gets the BQ27520 device in the Zephyr context
static err_code xSensBq27520Init(void)
{   
    // Get a device structure from a devicetree node with compatible
    // "ti,Bq27520". (If there are multiple, just pick one.)
    gpBatteryGaugeDevice = DEVICE_DT_GET_ANY( ti_bq27520 );    
    if ( gpBatteryGaugeDevice == NULL ) {
        /* No such node, or the node does not have status "okay". */
        LOG_ERR( "\nNo device found.\n" );
        gSensorStatus.isReady = false;
        return X_ERR_DEVICE_NOT_FOUND;
    }    
    if ( !device_is_ready( gpBatteryGaugeDevice ) ) {
        LOG_ERR( "\nDevice \"%s\" is not ready; "
               "check the driver initialization logs for errors.\n",
               gpBatteryGaugeDevice->name );
        gSensorStatus.isReady = false;
        return X_ERR_DEVICE_NOT_READY;
    }
    //if device is ok
    else{
        LOG_INF( "Found device \"%s\", on I2C address 0x%02x \n", gpBatteryGaugeDevice->name, DT_REG_ADDR( BQ27520 ) );
        gSensorStatus.isReady = true;
        return X_ERR_SUCCESS;
    }
}


// Initializes/Gets the BQ27421 device in the Zephyr context
static err_code xSensBq27421Init(void)
{   
    // Get a device structure from a devicetree node with compatible
    // "ti,Bq27421". (If there are multiple, just pick one.)
    gpBatteryGaugeDevice = DEVICE_DT_GET_ANY( ti_bq274xx );    
    if ( gpBatteryGaugeDevice == NULL ) {
        /* No such node, or the node does not have status "okay". */
        LOG_ERR( "\nNo device found.\n" );
        gSensorStatus.isReady = false;
        return X_ERR_DEVICE_NOT_FOUND;
    }    
    if ( !device_is_ready( gpBatteryGaugeDevice ) ) {
        LOG_ERR( "\nDevice \"%s\" is not ready; "
               "check the driver initialization logs for errors.\n",
               gpBatteryGaugeDevice->name );
        gSensorStatus.isReady = false;
        return X_ERR_DEVICE_NOT_READY;
    }
    //if device is ok
    else{
        LOG_INF( "Found device \"%s\", on I2C address 0x%02x \n", gpBatteryGaugeDevice->name, DT_REG_ADDR( BQ27421 ) );
        gSensorStatus.isReady = true;
        return X_ERR_SUCCESS;
    }
}



void batGaugeThread(void){

	if (!gSensorStatus.isReady) {
        xSensBatGaugeInit();
	}

	xDataPacket_t pack = {
		.error = dataErrOk,
		.sensorType = battery_gauge_t,
		.name = JSON_ID_SENSOR_BATTERY,
		.measurementsNum = 2,
		//measurements
		.meas ={
			// Voltage
			[0].name = JSON_ID_SENSOR_CHAN_GAUGE_VOLTAGE, 
			[0].type = SENSOR_CHAN_GAUGE_VOLTAGE,
			[0].dataType = isDouble,
			[0].data.doubleVal = 0,
			// SoC
			[1].name = JSON_ID_SENSOR_CHAN_GAUGE_STATE_OF_CHARGE, 
			[1].type = SENSOR_CHAN_GAUGE_STATE_OF_CHARGE,
			[1].dataType = isDouble,
			[1].data.doubleVal = 0,
		}
	};

	while (1) {

		if( !gSensorStatus.isReady ){
			LOG_ERR("Device cannot be used\r\n");
			pack.error = dataErrNotInit;
		}


		if( batGaugeReadValue( SENSOR_CHAN_GAUGE_VOLTAGE, &gVoltageV ) == 0 ){
			batGaugeShowValues( "Voltage: ", gVoltageV );
		} 
		else{
			pack.error = dataErrFetchFail;
		}


		if( batGaugeReadValue( SENSOR_CHAN_GAUGE_STATE_OF_CHARGE, &gSoc ) == 0 ){
			batGaugeShowValues( "State of Charge (%): ", gSoc );
		}
		else{
			pack.error = dataErrFetchFail;
		}

		// prepare data to send
		pack.meas[0].data.doubleVal = sensor_value_to_double( &gVoltageV );
		pack.meas[1].data.doubleVal = sensor_value_to_double( &gSoc );

		//send
		if( gSensorStatus.isPublishEnabled ){
			xDataSend( pack );
		}
		

        // todo: check yield methods
		k_sleep( K_MSEC( gSensorStatus.updatePeriod ) );
	}

}




int32_t batGaugeGetChannelString( enum sensor_channel type, char *string, uint8_t max_string_len ){

	char type_str[40];
	
	switch(type){
		case SENSOR_CHAN_GAUGE_VOLTAGE:  strcpy(type_str, "Voltage"); break;
		case SENSOR_CHAN_GAUGE_AVG_CURRENT:	strcpy(type_str, "Average Current"); break;
		case SENSOR_CHAN_GAUGE_STDBY_CURRENT: strcpy(type_str, "Standby Current"); break;
		case SENSOR_CHAN_GAUGE_MAX_LOAD_CURRENT: strcpy(type_str, "Max Load Current"); break;
		case SENSOR_CHAN_GAUGE_STATE_OF_CHARGE: strcpy(type_str, "State of Charge"); break;
		case SENSOR_CHAN_GAUGE_STATE_OF_HEALTH: strcpy(type_str, "State of Health Current"); break;
		case SENSOR_CHAN_GAUGE_AVG_POWER: strcpy(type_str, "Average Power"); break;
		case SENSOR_CHAN_GAUGE_FULL_CHARGE_CAPACITY: strcpy(type_str, "Full Charge Capacity"); break;
		case SENSOR_CHAN_GAUGE_REMAINING_CHARGE_CAPACITY: strcpy(type_str, "Remaining Charge Capacity"); break;
		default:	return X_ERR_INVALID_PARAMETER; 
	}

	if(strlen(type_str)>=max_string_len){
		return X_ERR_BUFFER_OVERFLOW;
	}

	strcpy(string,type_str);
	return X_ERR_SUCCESS;

}





static void batGaugeShowValues(const char *type_str, struct sensor_value value)
{
    if ((value.val2 < 0) && (value.val1 >= 0)) {
        value.val2 = -(value.val2);
        LOG_INF("%s: -%d.%06d\n", type_str, value.val1, value.val2);
    } else if ((value.val2 > 0) && (value.val1 < 0)) {
        LOG_INF("%s: %d.%06d\n", type_str, value.val1, value.val2);
    } else if ((value.val2 < 0) && (value.val1 < 0)) {
        value.val2 = -(value.val2);
        LOG_INF("%s: %d.%06d\n", type_str, value.val1, value.val2);
    } else {
        LOG_INF("%s: %d.%06d\n", type_str, value.val1, value.val2);
    }

	return;

}



int32_t batGaugeReadValue( enum sensor_channel type, struct sensor_value *value ){

	char type_string[40];
	struct sensor_value val;

	int32_t err = batGaugeGetChannelString( type,type_string,sizeof(type_string) );

	if( err < 0 ){
		LOG_ERR("Error in get_channel_string: %d", err );
		return err;
	}

    if( ( err = sensor_sample_fetch_chan( gpBatteryGaugeDevice, type) ) < 0 ){
		LOG_ERR("Problem in channel fetch: %s  error: %d", type_string, err );
		return err;
    }

	if( (err = sensor_channel_get( gpBatteryGaugeDevice, type, &val) ) < 0 ){
		LOG_ERR("Unable to get value for: %s  error:%d", type_string, err );
		return err;
	}

	*value = val;

	return err; // return success at this point (0) 

}



/* ----------------------------------------------------------------
 * SHELL COMMANDS IMPLEMENTATION
 * -------------------------------------------------------------- */

// Intended to be called by the shell
void xSensBatGaugeEnablePublishCmd(const struct shell *shell, size_t argc, char **argv){

        if(argc!=2){
            shell_print(shell, "Invalid number of parameters. Command example: <publish on>\r\n");  
            return;
        }
	
		// check if the parameter is the "on" string, and enable publish
		if( strcmp(argv[1], "on") == 0 ){
			xSensBatGaugeEnablePublish(true);
		}
		
		// check if the parameter is the "off" string, and disable publish
		else if( strcmp(argv[1], "off") == 0 ){
			xSensBatGaugeEnablePublish(false);
            return;
        }

        else{
            shell_print(shell, "Invalid parameter (on/off)\r\n");  
            return;
		}

}


// Intended to be called by the shell
void xSensBatGaugeUpdatePeriodCmd(const struct shell *shell, size_t argc, char **argv)
{
		// todo: add some sanity checks here on the period***
		uint32_t milliseconds = atoi( argv[1]);
		xSensBatGaugeSetUpdatePeriod(milliseconds);

		return;
}



















