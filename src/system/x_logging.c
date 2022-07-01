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
 * @brief This file contains the implementation of the functions
 * described in "x_logging.h" file of the sensor aggregatio use
 * case example firmware for XPLR-IOT-1
 */

#include "x_logging.h"

#include <logging/log.h>
#include <logging/log_ctrl.h>
#include <shell/shell.h>
#include <shell/shell_uart.h>  //shell_backend_uart_get_ptr


/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION
 * -------------------------------------------------------------- */


/** Get the source (module or instance) ID of a log instance/module
 *  Usage example:
 *  log_source_id_get( STRINGIFY( LOGMOD_NAME_BME280 ) );
 * 
 * @param name  String containing the name of the log module
 * @return      The source (module or instance) ID. 
*/
static int16_t log_source_id_get(const char *name);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */


//Holds the status of the logging modules of the application, in case
// the status needs to be restored
uint8_t gLogStatusBuffer[ LOG_STATUS_BUF_MAXLEN ];

// Flag whether the Log backend is active
static bool gLogBackendActive;


/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


static int16_t log_source_id_get(const char *name)
{

	for (int16_t i = 0; i < log_src_cnt_get(CONFIG_LOG_DOMAIN_ID); i++) {
		if (strcmp(log_source_name_get(CONFIG_LOG_DOMAIN_ID, i), name)
		    == 0) {
			return i;
		}
	}

	return -1;
}


/* ----------------------------------------------------------------
 * PUBLIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


void xLogDisable( const char *sensor_log_name ){
	
	printk("Disabling logging in the %s module\n",sensor_log_name);
	log_filter_set(NULL, 0, log_source_id_get( sensor_log_name ), LOG_LEVEL_NONE);	// consider setting the level to error(?)				
	return;
}



void log_set_sensor_lvl( const char *sensor_log_name, uint32_t level ){

	const struct shell *sh = shell_backend_uart_get_ptr();
	
	printk("Restoring logging lvl in the %s module to level: %d\n",sensor_log_name, level);
	log_filter_set(sh->log_backend->backend, 0, log_source_id_get( sensor_log_name ), level);	// consider setting the level to error(?)				
	return;
}



void xLogStartupConfig(void){

	// Disable logs for button and led at startup, customize
	// this as needed, so you don't have to send log setup commands
	// every time the device resets
	xLogDisable( STRINGIFY(LOGMOD_NAME_LED) );
	xLogDisable( STRINGIFY(LOGMOD_NAME_BUTTON) );

	//xLogDisable( STRINGIFY(LOGMOD_NAME_STORAGE) );
}



void xLogSaveState(void){

	uint32_t modules_cnt = log_sources_count();
	
	if( modules_cnt > LOG_STATUS_BUF_MAXLEN ){
		printk("Log modules number exceeds the maximum number of modules that can be saved: All log modules will be disabled\r\n");
		return;
	} 

	const struct shell *sh = shell_backend_uart_get_ptr();
	const struct log_backend *backend = sh->log_backend->backend;
	
	gLogBackendActive = log_backend_is_active(backend);
	
	for (int16_t i = 0U; i < modules_cnt; i++) {
		gLogStatusBuffer[i] = (uint8_t) log_filter_get(backend, CONFIG_LOG_DOMAIN_ID, i, true);

	}
	return;
}



void xLogRestoreState(void){

	uint32_t modules_cnt = log_sources_count();
	
	if( modules_cnt > LOG_STATUS_BUF_MAXLEN ){
		printk("Log modules number exceeds the maximum number of modules that can be saved: All log modules will be disabled\r\n");
		return;
	} 

	const struct shell *sh = shell_backend_uart_get_ptr();
	const struct log_backend *backend = sh->log_backend->backend;

    // Set the Log backend to its previous state (active/not active)
	if( !gLogBackendActive ){
		log_backend_deactivate (backend);
	}
	

	for (int16_t i = 0U; i < modules_cnt; i++) {
		log_filter_set(backend, CONFIG_LOG_DOMAIN_ID, i,  (uint32_t)gLogStatusBuffer[i] );
	}
	return;
}