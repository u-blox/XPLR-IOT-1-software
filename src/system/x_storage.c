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
 * @brief This file contains the imeplementation of the internal storage
 * function described in "x_storage.h" file
 */

#include "x_storage.h"

// Zephyr Related
#include <zephyr.h>
#include <stdio.h>  //snprintf
#include <stdlib.h>  //atoi
#include <device.h>
#include <fs/fs.h>
#include <fs/littlefs.h>
#include <storage/flash_map.h>
#include <string.h>
#include <logging/log.h>

// Application Related
#include "x_logging.h"
#include "x_cell_saraR5.h"


/* ----------------------------------------------------------------
 * DEFINITIONS
 * -------------------------------------------------------------- */

/* Matches LFS_NAME_MAX */
#define MAX_PATH_LEN 50


/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

LOG_MODULE_REGISTER(LOGMOD_NAME_STORAGE, LOG_LEVEL_DBG);

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

static struct fs_mount_t lfs_storage_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &storage,
	.storage_dev = (void *)FLASH_AREA_ID(storage),
	.mnt_point = "/lfs",
};

struct fs_mount_t *mp = &lfs_storage_mnt;

/** Flag indicating that fs is mounted*/
static bool gIsMounted = false;


/* ----------------------------------------------------------------
 * FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */

err_code xStorageInit(){
	
	if( gIsMounted ){
		return X_ERR_SUCCESS;
	}

	LOG_INF("Mounting\r\n");

	struct fs_statvfs sbuf;
	int rc = fs_mount(mp);
	if (rc < 0) {
		//LOG_ERR("FAIL MOUNT!! %d\r\n", rc);
		return rc;
	}

	// stats block size, free blocks etc
	rc = fs_statvfs(mp->mnt_point, &sbuf);
	//if (rc < 0) {
		//printk("FAIL: statvfs: %d\n", rc);
	//}
	
	gIsMounted = true;
	return X_ERR_SUCCESS;
}



err_code xStorageDeinit(void){
	
	if(!gIsMounted){
		return X_ERR_SUCCESS;
	}

	int rc = fs_unmount(mp);
	if (rc < 0) {
		//LOG_ERR("FAIL: mount id %u at %s: %d\n", (unsigned int)mp->storage_dev, mp->mnt_point, rc);
		return rc;
	}
	
	gIsMounted = false;
	return X_ERR_SUCCESS;
}



err_code xStorageReadFile(void *data, char *filename, uint32_t max_size){

	struct fs_file_t file;
	int file_size;
	int rc;
	char fname[MAX_PATH_LEN];

	if( !gIsMounted ){
		rc = xStorageInit();
		if( rc < 0){
			return rc;
		}
	}

	fs_file_t_init(&file);

	snprintf(fname, sizeof(fname), "%s/%s", mp->mnt_point, filename);

	rc = fs_open(&file, fname, FS_O_READ);
	if( rc < 0){
		//if (rc == -2 ) {
			//LOG_WRN("File does not exist. No wifi saved\r\n");
		//}
		LOG_ERR("Error opening file (read): %d\r\n", rc);
		xStorageDeinit();
		return rc;
	}

	// get the size of the file:
	rc = fs_seek(&file, 0L, FS_SEEK_END);

	rc = fs_tell(&file);
	if (rc<0 ){
		fs_close(&file);
		return rc;
	}
	else{
		file_size = rc;
	}

	if( file_size > max_size ){
		//file contents cannot fit to data buffer provided
		fs_close(&file);
		return X_ERR_BUFFER_OVERFLOW; 
	}
	
	rc = fs_seek(&file, 0L, FS_SEEK_SET);
	rc = fs_read( &file, data, file_size );

	fs_close(&file);
	return rc;

}



err_code xStorageSaveFile(void *data, char *filename, uint32_t data_size){

	struct fs_file_t file;
	//int file_size;
	int rc;
	char fname[MAX_PATH_LEN];

	if( !gIsMounted ){
		rc = xStorageInit();
		if( rc<0 ){
			return rc;
		} 
	}

	// littlefs does not need to explicitly call garbage collection,
	// so it is not called in this module

	fs_file_t_init(&file);

	snprintf(fname, sizeof(fname), "%s/%s", mp->mnt_point, filename);

	rc = fs_open(&file, fname, FS_O_CREATE | FS_O_WRITE);
	if( rc < 0 ){
		LOG_ERR("Error opening file (write): %d\r\n", rc);
		xStorageDeinit();
		return rc;
	}
	
	fs_seek(&file, 0L, FS_SEEK_SET);
	rc = fs_write(&file, data, data_size);
	
	fs_close(&file);
	return rc;
}



int xStorageReadWifiCred(char *ssid, uint8_t max_ssid_len, char *passphrase, uint8_t max_pass_len, int32_t *sec_type){

	int rc;
	if(!gIsMounted){
		if( ( rc = xStorageInit() ) < 0 ){
			return rc;
		}
	}
	
	//char fname[MAX_PATH_LEN];
	// read SSID
	rc = xStorageReadFile( ssid, wifi_cred_ssid_fname, max_ssid_len );
	if(rc<0){
		if( rc == ERR_STORAGE_FILE_NOT_FOUND ){
			LOG_WRN("File does not exist. No wifi saved\r\n");
		}
		else{
			LOG_ERR("Error Reading SSID: %d\r\n", rc);	
		}
		return rc;
	}

	// read security type
	rc = xStorageReadFile( (void*)sec_type, wifi_cred_sec_type_fname, sizeof(int32_t) );
	if(rc<0){
		LOG_ERR("Error Reading Security Type: %d\r\n", rc);
		return rc;
	}

	if(*sec_type == 1){
		return X_ERR_SUCCESS;
	}

	// read passphrase
	rc = xStorageReadFile( passphrase, wifi_cred_psw_fname, max_pass_len );
	if(rc<0){
		LOG_ERR("Error Reading passphrase: %d\r\n", rc);
		return rc;
	}

	return X_ERR_SUCCESS;
}




err_code xStorageSaveWifiCred(char *ssid, uint8_t ssid_len, char *passphrase, uint8_t pass_len, int32_t sec_type){
    // lengths refer to string lengths

	int rc;
	
	if( (sec_type!=1) && (sec_type!=2)  ){
		LOG_ERR("Invalid Sec Type, abort saving: %d \r\n", sec_type );
		return X_ERR_INVALID_PARAMETER; 
	}

	if(!gIsMounted){
		if( ( rc = xStorageInit() ) < 0 ){
			return rc;
		}
	}
	
	// save SSID
	if( ( rc = xStorageSaveFile( ssid, wifi_cred_ssid_fname, ssid_len +1 ) ) < 0 ){
		return rc;
	}
	
	// save security type
	if( ( rc = xStorageSaveFile( (void*)&sec_type, wifi_cred_sec_type_fname, sizeof(sec_type) ) ) < 0 ){
		return rc;
	}

	if(sec_type == 2){
		// save passphrase
		if( ( rc = xStorageSaveFile( passphrase, wifi_cred_psw_fname, pass_len +1 ) ) < 0 ){
			return rc;
		}
	}

	return X_ERR_SUCCESS;
}



int xStorageSaveMqttConfig(uMqttClientConnection_t mqttConnection){

	int rc;
	
	if(!gIsMounted){
		if( ( rc = xStorageInit() ) < 0 ){
			return rc;
		}
	}

	// save device ID
	uint32_t size = strlen(mqttConnection.pClientIdStr);
	if( ( rc = xStorageSaveFile( (void*)&mqttConnection.pClientIdStr[0], mqtt_deviceID_fname, size+1 ) ) < 0 ){
		return rc;
	}

	//save Username
	size = strlen(mqttConnection.pUserNameStr);
	if( ( rc = xStorageSaveFile( (void*)&mqttConnection.pUserNameStr[0], mqtt_username_fname, size+1 ) ) < 0 ){
		return rc;
	}

	//save Password 
	size = strlen(mqttConnection.pPasswordStr);
	if( ( rc = xStorageSaveFile( (void*)&mqttConnection.pPasswordStr[0], mqtt_psw_fname, size+1 ) ) < 0 ){
		return rc;
	}

	return X_ERR_SUCCESS;

}


err_code xStorageReadMqttConfig( char *deviceId, uint32_t maxDeviceID_len, 
								char * username, uint32_t maxUsername_len,
								char *password, uint32_t maxPassword_len)
{

	int rc;
	if(!gIsMounted){
		if( ( rc = xStorageInit() ) < 0 ){
			return rc;
		}
	}

	// read device id
	rc = xStorageReadFile( deviceId, mqtt_deviceID_fname, maxDeviceID_len );
	if(rc<0){
		if( rc== ERR_STORAGE_FILE_NOT_FOUND ){
			LOG_WRN("File does not exist. No mqtt port saved\r\n");
		}
		else{
			LOG_ERR("Error Reading mqtt port: %d\r\n", rc);	
		}
		return rc;
	}

	// read username
	rc = xStorageReadFile( username, mqtt_username_fname, maxUsername_len );
	if(rc<0){
		if( rc== ERR_STORAGE_FILE_NOT_FOUND ){
			LOG_WRN("File does not exist. No mqtt port saved\r\n");
		}
		else{
			LOG_ERR("Error Reading mqtt port: %d\r\n", rc);	
		}
		return rc;
	}

	// read password
	rc = xStorageReadFile( password, mqtt_psw_fname, maxPassword_len );
	if(rc<0){
		if( rc== ERR_STORAGE_FILE_NOT_FOUND ){
			LOG_WRN("File does not exist. No mqtt port saved\r\n");
		}
		else{
			LOG_ERR("Error Reading mqtt port: %d\r\n", rc);	
		}
		return rc;
	}

	return X_ERR_SUCCESS;
}



err_code xStorageSaveMqttSnConfig(xCellMqttSnConfig_t mqttsnConnection){

	int rc;
	
	if(!gIsMounted){
		if( ( rc = xStorageInit() ) < 0 ){
			return rc;
		}
	}

	uint32_t size;
	
	if( mqttsnConnection.plan == FLEX ){

		size = strlen(mqttsnConnection.clientID);
		if( ( rc = xStorageSaveFile( (void*)&mqttsnConnection.clientID[0], mqttsn_flex_deviceID_fname, size+1 ) ) < 0 ){
			return rc;
		}
	}

	if( mqttsnConnection.plan == ANYWHERE ){

		size = strlen(mqttsnConnection.clientID);
		if( ( rc = xStorageSaveFile( (void*)&mqttsnConnection.clientID[0], mqttsn_anywhere_deviceID_fname, size+1 ) ) < 0 ){
			return rc;
		}

		// save connection duration setting as a string
		char duration_str[MQTTSN_CON_DURATION_STR_MAXLEN];
		sprintf(duration_str,"%d",mqttsnConnection.con_duration);
		size = strlen(duration_str);
		if( ( rc = xStorageSaveFile( (void*)duration_str, mqttsn_duration_fname, size+1 ) ) < 0 ){
			return rc;
		}
	}

	return X_ERR_SUCCESS;

}



int xStorageReadMqttSnConfig( xCellMqttSnConfig_t *mqttsnConnection, xCellMqttSnPlan_t plan){

	int rc = 0;
	if(!gIsMounted){
		if( ( rc = xStorageInit() ) < 0 ){
			return rc;
		}
	}

	mqttsnConnection->plan = plan;

	// read device id
	if( plan == ANYWHERE ){
		rc = xStorageReadFile( mqttsnConnection->clientID, mqttsn_anywhere_deviceID_fname, MQTTSN_CLIENT_ID_MAXLEN );
	}
	else if( plan == FLEX ){
		rc = xStorageReadFile( mqttsnConnection->clientID, mqttsn_flex_deviceID_fname, MQTTSN_CLIENT_ID_MAXLEN );
	}
	else{
		return X_ERR_INVALID_PARAMETER; 
	}

	if(rc<0){
		if( rc == ERR_STORAGE_FILE_NOT_FOUND ){
			LOG_WRN("File does not exist. No mqttsn port saved\r\n");
		}
		else{
			LOG_ERR("Error Reading mqttsn port: %d\r\n", rc);	
		}
		return rc;
	}

	// read connection duration
	if( plan == ANYWHERE ){
		char str[MQTTSN_CON_DURATION_STR_MAXLEN];
		rc = xStorageReadFile( str, mqttsn_duration_fname, MQTTSN_CON_DURATION_STR_MAXLEN );
		if(rc<0){
			if( rc== ERR_STORAGE_FILE_NOT_FOUND ){
				LOG_WRN("File does not exist. No mqttsn port saved\r\n");
			}
			else{
				LOG_ERR("Error Reading mqttsn port: %d\r\n", rc);	
			}
			return rc;
		}
		mqttsnConnection->con_duration = atoi(str);
	}

	return X_ERR_SUCCESS;

}


