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


#ifndef X_STORAGE_H__
#define  X_STORAGE_H__


/** @file
 * @brief This file contains the API for using the internal storage of
 *  XPLR-IOT-1 and more specifically the NORA-B1 embedded Flash memory.
 * This API allows saving/reading of WiFi credentials and MQTT(SN) settings
 * in/from the embedded memory of NORA-B1.
 * 
 * Littlefs is used as filesystem to perform those operations
 */


#include <stdint.h>
#include <stdbool.h>
#include <zephyr.h> 

#include "u_mqtt_common.h" 
#include "u_mqtt_client.h" // included for u_mqtt_client to succeed in compilation
#include "x_cell_mqttsn.h"

#include "x_errno.h"


/* ----------------------------------------------------------------
 * DEFINITION OF FILENAMES
 * -------------------------------------------------------------- */

// Filenames for WiFi credentials
#define  wifi_cred_ssid_fname  		"ssid"
#define  wifi_cred_psw_fname 		"pass"
#define  wifi_cred_sec_type_fname  	"sec_type"

// Filenames for MQTT configuration
#define  mqtt_deviceID_fname  		"mqtt_device"
#define  mqtt_username_fname  		"mqtt_user"
#define  mqtt_psw_fname  		    "mqtt_pass"

//Filenames for MQTTSN configuration
#define mqttsn_flex_deviceID_fname		"mqttsn_flex_device"
#define mqttsn_anywhere_deviceID_fname	"mqttsn_anywhere_device"
#define mqttsn_duration_fname			"mqttsn_duration"



/* ----------------------------------------------------------------
 * GENERAL STORAGE FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialize/mount the filesystem.
 *
 * @return   zero on success (X_ERR_SUCCESS) else negative error code.
 */
err_code xStorageInit(void);


/** Deinitialize/unmout the filesystem.
 *
 * @return   zero on success (X_ERR_SUCCESS) else negative error code.
 */
err_code xStorageDeinit(void);


/** Read a file from memory
 *
 * @param data      [Output] Data buffer to which the contents of the file will be saved.
 * @param filename  [Input] The name of the file to read.
 * @param max_size  [input] The data buffer's maximum size.
 * 
 * @return          zero on success (X_ERR_SUCCESS) else negative error code.
 */
err_code xStorageReadFile(void *data, char *filename, uint32_t max_size);


/** Save a file from memory
 *
 * @param data      [Input] Data buffer which holds the contents of the file that will be saved.
 * @param filename  [Input] The name of the file to save.
 * @param data_size [input] The size of data buffer to save in the file.
 * 
 * @return          zero on success else negative error code.
 */
err_code xStorageSaveFile(void *data, char *filename, uint32_t data_size);



/* ----------------------------------------------------------------
 * STORAGE FUNCTIONS SPECIFIC TO APPLICATION
 * -------------------------------------------------------------- */

/** Save wifi credentials. These will be saved in files with names as defined in 
 * - wifi_cred_ssid_fname  		
 * - wifi_cred_psw_fname 		
 * - wifi_cred_sec_type_fname
 *
 * @param ssid          [Input] WiFi network ssid.
 * @param ssid_len      [Input] The length of WiFi network ssid.
 * @param passphrase    [Input] WiFi network password (if any, defined by sec_type).
 * @param pass_len      [Input] The length of WiFi network password.
 * @param sec_type      [Input] Security type ( 1 = open, 2 = password required ).
 * @return              zero on success else negative error code.
 */
err_code xStorageSaveWifiCred(char *ssid, uint8_t ssid_len, char *passphrase, uint8_t pass_len, int32_t sec_type);


/** Read wifi credentials. These will be read from files with names as defined in 
 * - wifi_cred_ssid_fname  		
 * - wifi_cred_psw_fname 		
 * - wifi_cred_sec_type_fname
 *
 * @param ssid          [Output] Buffer holding WiFi network ssid.
 * @param max_ssid_len  [Input] Maximum length of ssid buffer.
 * @param passphrase    [Output] Buffer holding password (if any, defined by sec_type).
 * @param max_pass_len  [Input] Maximum length of password.
 * @param sec_type      [Output]Security type ( 1 = open, 2 = password required ).
 * @return              zero on success else negative error code.
 */
err_code xStorageReadWifiCred(char *ssid, uint8_t max_ssid_len, char *passphrase, uint8_t max_pass_len, int32_t *sec_type);


/** Save MQTT configuration. These will be saved in files with names as defined in 
 * - mqtt_deviceID_fname
 * - mqtt_username_fname
 * - mqtt_psw_fname  		 
 *
 * @param mqttConnection  [Input] MQTT configuration to be saved.
 *                        Only device ID, username and password are saved
 *                        from this struct. 
 * @return                zero on success else negative error code.
 */
err_code xStorageSaveMqttConfig( uMqttClientConnection_t mqttConnection );


/** Read MQTT configuration. These will be read from files with names as defined in 
 * - mqtt_deviceID_fname
 * - mqtt_username_fname
 * - mqtt_psw_fname  		 
 *
 * @param deviceId         [Output] MQTT Configuration Device ID as saved in the file  
 * @param maxDeviceID_len  [Input] deviceId buffer maximum length
 * @param username         [Output] MQTT Configuration username as saved in the file  
 * @param maxUsername_len  [Input] username buffer maximum length
 * @param password         [Output] MQTT Configuration password as saved in the file  
 * @param maxPassword_len  [Input] password buffer maximum length
 * @return                 zero on success else negative error code.
 */
err_code xStorageReadMqttConfig( char *deviceId, uint32_t maxDeviceID_len, 
								char * username, uint32_t maxUsername_len,
								char *password, uint32_t maxPassword_len);


/** Save MQTT-SN configuration. These will be saved in files with names as defined in 
 * - mqttsn_flex_deviceID_fname		
 * - mqttsn_anywhere_deviceID_fname
 * - mqttsn_duration_fname			 		 
 *
 * @param mqttsnConnection  [Input] MQTT-SNconfiguration to be saved.
 *                        Only device ID, and connection duration are saved
 *                        from this struct. 
 * @return                zero on success else negative error code.
 */
err_code xStorageSaveMqttSnConfig(xCellMqttSnConfig_t mqttsnConnection);


/** Read MQTT-SN configuration. These will be read from files with names as defined in 
 * - mqttsn_flex_deviceID_fname		
 * - mqttsn_anywhere_deviceID_fname
 * - mqttsn_duration_fname			 		 
 *
 * @param mqttsnConnection  [Output] Holds the MQTT-SN configuration read from the files.
 * @param plan              [Input] Depending on this parameter MQTT-SN Anywhere or Flex
 *                          configuration files will be read. 
 * @return                  zero on success else negative error code.
 */
err_code xStorageReadMqttSnConfig( xCellMqttSnConfig_t *mqttsnConnection, xCellMqttSnPlan_t plan);



#endif    //X_STORAGE_H__