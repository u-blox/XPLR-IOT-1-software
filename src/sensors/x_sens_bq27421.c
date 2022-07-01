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
 * @brief Implementation of API for BQ27421 sensor of XPLR-IOT-1.
 *  Also implements the thread controlling the sensor's measurements
 */


#include "x_sens_bq27421.h"
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
void bq27421Thread(void);

// Helper function to type a measurement of the Fuel Gauge
static void bq27421ShowValues(const char *type_str, struct sensor_value value);

// Hepler function to get a certain measurement channel from Fuel Gauge
int32_t bq27421GetChannelString( enum sensor_channel type, char *string, uint8_t max_string_len );

// Helper function to get measurement channel
int32_t bq27421ReadValue( enum sensor_channel type, struct sensor_value *value );



/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

// Only used to access the I2C address of the device after the setup
#define BQ27421 DT_INST(0, ti_bq274xx)

// In order to use Zephyr's logging module
LOG_MODULE_REGISTER(LOGMOD_NAME_BQ27421, LOG_LEVEL_DBG);


K_THREAD_DEFINE(bq27421_id, BQ27421_STACK_SIZE, bq27421Thread, NULL, NULL, NULL,
		BQ27421_PRIORITY, 0, 0);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/** Device descriptor.
 */
const struct device *gpBq27421Device;


/** a structure type (common for sensors) to hold info about the status 
 * of the sensor
 */
static xSensStatus_t gSensorStatus = {
	.sensorType= bq27421_t,
	.isReady=false,
	.isPublishEnabled=false, 
	.isEnabled=false, 
	.updatePeriod = BQ27421_DEFAULT_UPDATE_PERIOD_MS
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

// Initializes/Gets the BQ27421 device in the Zephyr context
err_code xSensBq27421Init(void)
{   
    // Get a device structure from a devicetree node with compatible
    // "ti,Bq27421". (If there are multiple, just pick one.)
    gpBq27421Device = DEVICE_DT_GET_ANY( ti_bq274xx );    
    if ( gpBq27421Device == NULL ) {
        /* No such node, or the node does not have status "okay". */
        LOG_ERR( "\nNo device found.\n" );
        gSensorStatus.isReady = false;
        return X_ERR_DEVICE_NOT_FOUND;
    }    
    if ( !device_is_ready( gpBq27421Device ) ) {
        LOG_ERR( "\nDevice \"%s\" is not ready; "
               "check the driver initialization logs for errors.\n",
               gpBq27421Device->name );
        gSensorStatus.isReady = false;
        return X_ERR_DEVICE_NOT_READY;
    }
    //if device is ok
    else{
        LOG_INF( "Found device \"%s\", on I2C address 0x%02x \n", gpBq27421Device->name, DT_REG_ADDR( BQ27421 ) );
        gSensorStatus.isReady = true;
        return X_ERR_SUCCESS;
    }
}



// Set the update/sampling period of the sensor
err_code xSensBq27421SetUpdatePeriod( uint32_t milliseconds ){

    if( !xSensIsChangeAllowed() ){
        LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
        return X_ERR_INVALID_STATE; 
    }
    
    gSensorStatus.updatePeriod = milliseconds;
    
    LOG_INF("BQ27421 Update Period Set to %d ms", gSensorStatus.updatePeriod);
    return X_ERR_SUCCESS;
}



// Returns the status of the sensor
xSensStatus_t xSensBq27421GetStatus(void){
	return gSensorStatus;
}



//Disables BQ27421 measurements by suspending the sensor's sampling thread.
err_code xSensBq27421Disable(void){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE; 
	}

	k_thread_suspend(bq27421_id);
	LOG_INF("%sBQ27421 suspended%s \r\n",LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT);
	gSensorStatus.isEnabled = false;
	
	return X_ERR_SUCCESS;
} 



// Enables BQ27421 measurements by resuming/starting the sensor's sampling thread.
err_code xSensBq27421Enable(void){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE; 
	}

	k_thread_resume(bq27421_id);
	LOG_INF("%sBQ27421 started%s \r\n",LOG_CLRCODE_GREEN, LOG_CLRCODE_DEFAULT);
	gSensorStatus.isEnabled = true;

	return X_ERR_SUCCESS;
}




//Enables/Disables the publish of measurements
err_code xSensBq27421EnablePublish(bool enable){

	if( !xSensIsChangeAllowed() ){
		LOG_WRN("Cannot change setting when Sensor Aggregation function is active\r\n");
		return X_ERR_INVALID_STATE;
	}

	gSensorStatus.isPublishEnabled = enable;
	if( gSensorStatus.isPublishEnabled ){
		LOG_INF("%sBQ27421 publish enabled%s \r\n",LOG_CLRCODE_GREEN, LOG_CLRCODE_DEFAULT);
	}
	else{
		LOG_INF("%sBQ27421 publish disabled%s \r\n",LOG_CLRCODE_RED, LOG_CLRCODE_DEFAULT);
	}

	return X_ERR_SUCCESS;
}



/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */

void bq27421Thread(void){

	if (!gSensorStatus.isReady) {
        xSensBq27421Init();
	}

	xDataPacket_t pack = {
		.error = dataErrOk,
		.sensorType = bq27421_t,
		.name = JSON_ID_SENSOR_BQ27421,
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


		if( bq27421ReadValue( SENSOR_CHAN_GAUGE_VOLTAGE, &gVoltageV ) == 0 ){
			bq27421ShowValues( "Voltage: ", gVoltageV );
		} 
		else{
			pack.error = dataErrFetchFail;
		}

		// if( bq27421ReadValue( SENSOR_CHAN_GAUGE_AVG_CURRENT, &current ) == 0 ){
		// 	bq27421ShowValues( "Avg Current (Amps): ", current );
		// }

		// if( bq27421ReadValue( SENSOR_CHAN_GAUGE_STDBY_CURRENT, &current_standby ) == 0 ){
		// 	bq27421ShowValues( "Standby Current (Amps): ", current_standby );
		// }

		// if( bq27421ReadValue( SENSOR_CHAN_GAUGE_MAX_LOAD_CURRENT, &current_max_load ) == 0 ){
		// 	bq27421ShowValues( "Max Load Current (Amps): ", current_max_load );
		// }

		if( bq27421ReadValue( SENSOR_CHAN_GAUGE_STATE_OF_CHARGE, &gSoc ) == 0 ){
			bq27421ShowValues( "State of Charge (%): ", gSoc );
		}
		else{
			pack.error = dataErrFetchFail;
		}

		// if( bq27421ReadValue( SENSOR_CHAN_GAUGE_STATE_OF_HEALTH, &state_of_health ) == 0 ){
		// 	bq27421ShowValues( "State of Health (%%): ", state_of_health );
		// }

		// if( bq27421ReadValue( SENSOR_CHAN_GAUGE_AVG_POWER, &avg_power ) == 0 ){
		// 	bq27421ShowValues( "Average Power (Watt): ", avg_power );
		// }

		// if( bq27421ReadValue( SENSOR_CHAN_GAUGE_FULL_CHARGE_CAPACITY, &full_charge_capacity ) == 0 ){
		// 	bq27421ShowValues( "Full charge capacity (Ah): ", full_charge_capacity );
		// }

		// if( bq27421ReadValue( SENSOR_CHAN_GAUGE_REMAINING_CHARGE_CAPACITY, &remaining_charge_capacity ) == 0 ){
		// 	bq27421ShowValues( "Remaining charge capacity (Ah): ", remaining_charge_capacity );
		//}

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




int32_t bq27421GetChannelString( enum sensor_channel type, char *string, uint8_t max_string_len ){

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
		default:	return -1; 
	}

	if(strlen(type_str)>=max_string_len){
		return -1;
	}

	strcpy(string,type_str);
	return 0;

}





static void bq27421ShowValues(const char *type_str, struct sensor_value value)
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



int32_t bq27421ReadValue( enum sensor_channel type, struct sensor_value *value ){

	char type_string[40];
	struct sensor_value val;

	int32_t err = bq27421GetChannelString( type,type_string,sizeof(type_string) );

	if( err < 0 ){
		LOG_ERR("Error in get_channel_string: %d", err );
		return err;
	}

    if( ( err = sensor_sample_fetch_chan( gpBq27421Device, type) ) < 0 ){
		LOG_ERR("Problem in channel fetch: %s  error: %d", type_string, err );
		return err;
    }

	if( (err = sensor_channel_get( gpBq27421Device, type, &val) ) < 0 ){
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
void xSensBq27421EnablePublishCmd(const struct shell *shell, size_t argc, char **argv){

        if(argc!=2){
            shell_print(shell, "Invalid number of parameters. Command example: <publish on>\r\n");  
            return;
        }
	
		// check if the parameter is the "on" string, and enable publish
		if( strcmp(argv[1], "on") == 0 ){
			xSensBq27421EnablePublish(true);
		}
		
		// check if the parameter is the "off" string, and disable publish
		else if( strcmp(argv[1], "off") == 0 ){
			xSensBq27421EnablePublish(false);
            return;
        }

        else{
            shell_print(shell, "Invalid parameter (on/off)\r\n");  
            return;
		}

}


// Intended to be called by the shell
void xSensBq27421UpdatePeriodCmd(const struct shell *shell, size_t argc, char **argv)
{
		// todo: add some sanity checks here on the period***
		uint32_t milliseconds = atoi( argv[1]);
		xSensBq27421SetUpdatePeriod(milliseconds);

		return;
}



















