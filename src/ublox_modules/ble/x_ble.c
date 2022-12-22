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
 * @brief This file contains the API and BLE functionality implementation 
 * in the context of Sensor Aggregation Use Case firmware for XPLR-IOT-1.
 * It basically implements a modified Nordic Uart Service as implemented in
 * Nordic's "ble peripheral uart" example. In this modified example, data
 * received from BLE via this Service are are considered commands from 
 * u-blox mobile application and are passed to a BLE command handler function
 * for processing. The command handler itself is treated as a separate module
 * (x_ble_cmd.h).
 */



#include "x_ble.h"

#include <stdio.h>

// Zephyr/Nordic SDK related
#include <zephyr.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci.h>
#include <bluetooth/services/nus.h>
#include <logging/log.h>

// Used to change BLE address
#include <bluetooth/hci_vs.h>
#include <settings/settings.h>

//Sensor aggregation related
#include "x_logging.h"
#include "x_ble_cmd.h"  // input buffer size X_BLE_CMD_IN_MAX_SIZE

/* ----------------------------------------------------------------
 * DEFINITIONS
 * -------------------------------------------------------------- */

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN	(sizeof(DEVICE_NAME) - 1)



/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION
 * -------------------------------------------------------------- */

/** BLE connected Callback*/
static void xBleConnectedCb(struct bt_conn *conn, uint8_t err);

/** BLE Disconnected Callback*/
static void xBleDisonnectedCb(struct bt_conn *conn, uint8_t reason);

/** BLE Data Received Callback*/
static void xBleReceiveCb(struct bt_conn *conn, const uint8_t *const data,
			  uint16_t len);

/** Set BLE Address. Need to call settings_load() after that for the 
 * change to take effect
*/
static void ble_set_bd_addr(bt_addr_t* addr);

/** If there is a ublox BLE address saved in the module, obtain it.
 * Else return false and set address = 0xFF-FF-FF-FF-FF-FF
 * 
 * @param[out] address 	Ublox Mac address (if exists) or 0xFF-FF-FF-FF-FF-FF
 * @return 				True if ublox address exists, false otherwise
 *   
*/
static bool ble_get_ublox_mac_addr( bt_addr_t *address );


/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

LOG_MODULE_REGISTER(LOGMOD_NAME_BLE, LOG_LEVEL_DBG);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/** Incoming characters buffer */
char gRxBuffer[ X_BLE_CMD_IN_MAX_SIZE ]={0};

/** Current Connection Reference */
static struct bt_conn *gpCurrentConn = NULL;

/** Data to be used in advertisement packets */
static const struct bt_data gBleAdvData[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};


/** Data to be used in Scan Response packets */
static const struct bt_data gBleScanRespData[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};


/** Struct defining the callbacks for BLE Connection Events */
static struct bt_conn_cb gBleConnectionCallbacks = {
	.connected    = xBleConnectedCb,
	.disconnected = xBleDisonnectedCb
};


/** Struct definining the Nordic Uart Service (NUS) Callbacks */
static struct bt_nus_cb gBleNusCallbacks = {
	.received = xBleReceiveCb,
};



/** Holds the status of the BLE module in the context of Sensor Aggragetion
 * Use Case */
xBleStatus_t gBleStatus = xBleNotInitialized;


/* ----------------------------------------------------------------
 * CALLBACKS
 * -------------------------------------------------------------- */


static void xBleConnectedCb(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (err) {
		LOG_INF("Connection failed (err %u)", err);
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Connected %s", log_strdup(addr));

	gpCurrentConn = bt_conn_ref(conn);
	gBleStatus = xBleIsConnected;

}



static void xBleDisonnectedCb(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected: %s (reason %u)", log_strdup(addr), reason);

	bt_conn_unref(gpCurrentConn);
	gpCurrentConn = NULL;
	//not connected means advertising
	gBleStatus = xBleIsAdvertising; 
		
}



static void xBleReceiveCb(struct bt_conn *conn, const uint8_t *const data,
			  uint16_t len)
{
	char addr[BT_ADDR_LE_STR_LEN] = {0};

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, ARRAY_SIZE(addr));
	LOG_DBG("Received data from: %s\r\n", log_strdup(addr));
	
	if( len >= sizeof(gRxBuffer) ){
		LOG_WRN("Receive buffer overflow\r\n");
		return;
	}
	else{
		memset( gRxBuffer, 0, sizeof(gRxBuffer) );
		memcpy( gRxBuffer, data, len);
		//LOG_DBG("Recv BLE Data: %s\r\n", gRxBuffer);
	}

	// Received Data are considered as commands
	// The BLE Command Handler function, handles those incoming
	// commands
	xBleCmdHandle(gRxBuffer,len);

}


/* ----------------------------------------------------------------
 * PUBLIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


err_code xBleInit(void){

    bt_addr_t address;
	
	bt_conn_cb_register(&gBleConnectionCallbacks);
	
	int err = bt_enable(NULL);
	if (err) {
		LOG_ERR("bt_enable error %d\r\n",err);
		return err;
	}

	LOG_INF("Bluetooth initialized\r\n");

	// BLE address change
	if( ble_get_ublox_mac_addr( &address ) ){
		LOG_INF( "Ublox BLE Address: %02x:%02x:%02x:%02x:%02x:%02x \r\n",
					 address.val[5],
					 address.val[4],
					 address.val[3],
					 address.val[2],
					 address.val[1],
					 address.val[0]);

		ble_set_bd_addr(&address);
	}

	settings_load();

	// Initialize Nordic Uart Service
	err = bt_nus_init(&gBleNusCallbacks);
	if (err) {
		LOG_INF("Failed to initialize UART service (err: %d)\r\n", err);
		return err;
	}
	
	gBleStatus = xBleIsInitialized;

	return X_ERR_SUCCESS;

}



xBleStatus_t xBleGetStatus(void){

	return gBleStatus;	
}



err_code xBleStartAdvertising(void){

	err_code err;

	// BLE stack should be initialized first
	if( gBleStatus == xBleNotInitialized ){
		return X_ERR_INVALID_STATE;  // use xBleInit() first;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN, gBleAdvData, ARRAY_SIZE(gBleAdvData), gBleScanRespData,
			      ARRAY_SIZE(gBleScanRespData));
	if (err) {
		LOG_INF("Advertising failed to start (err %d)\r\n", err);
		return err;
	}

	gBleStatus = xBleIsAdvertising;
	return X_ERR_SUCCESS;
}



/* ----------------------------------------------------------------
 * STATIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */


static void ble_set_bd_addr(bt_addr_t* addr) {
	struct net_buf *buf;
	int err;
	buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_BD_ADDR, sizeof(*addr));
	if(!buf) {
		LOG_INF("No RPC command buffers available\n");
	}
	net_buf_add_mem(buf, addr, sizeof(*addr));
	err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_BD_ADDR, buf, NULL);
	if(err) {
		LOG_INF("Device address cannot be set (err %d)\n", err);
	}
}


static bool ble_get_ublox_mac_addr( bt_addr_t *address ){

	uint8_t pub_addr[6];
	
	// Obtain the address saved in the UICR->OTP registers
	pub_addr[0] = ((NRF_UICR->OTP[1] & 0x0000ff00) >> 8);
 	pub_addr[1] = ((NRF_UICR->OTP[1] & 0x000000ff));
 	pub_addr[2] = ((NRF_UICR->OTP[0] & 0xff000000) >> 24);
 	pub_addr[3] = ((NRF_UICR->OTP[0] & 0x00ff0000) >> 16);
 	pub_addr[4] = ((NRF_UICR->OTP[0] & 0x0000ff00) >> 8);
 	pub_addr[5] = ((NRF_UICR->OTP[0] & 0x000000ff));

	// Check if there was actually an address saved in these registers
	if( pub_addr[0]== 0xFF )
		if( pub_addr[1]== 0xFF )
			if( pub_addr[2]== 0xFF )
				if( pub_addr[3]== 0xFF )
					if( pub_addr[4]== 0xFF )
						if( pub_addr[5]== 0xFF ){
							// No address saved in the registers
							// Both registers were 0xFFFFFFFF
							memset( (void *)address->val,0xFF,6);
							return false;
						}
	
	// Additional step tha could be added: Check if the address saved in a
	// valid u-blox address. Not implemented now.
 
 	bt_addr_t addr = {{pub_addr[0], pub_addr[1], pub_addr[2],
 					   pub_addr[3], pub_addr[4], pub_addr[5]}};

	memcpy( (void *)address->val, (void *)&addr.val, 6 );

	return true;

}


/* ----------------------------------------------------------------
 * FUNCTIONS IMPLEMENTING SHELL-COMMANDS
 * -------------------------------------------------------------- */


void xBleEnableCmd(const struct shell *shell, size_t argc, char **argv){

	// if stack is not initialized, initialize and start advertising
	if( gBleStatus == xBleNotInitialized ){
		shell_print( shell, "Initializing BLE stack");
		xBleInit();
	}

	// is already advertising/connected?
	if( gBleStatus >= xBleIsAdvertising ){
		shell_print( shell, "Already advertising or connected");
		return;
	}

	// start advertising
	int err = xBleStartAdvertising();
	if( err == X_ERR_SUCCESS ){
		shell_print( shell, "BLE Advertising started \r\n" );
	}

	return;
}



void xBleDisableCmd(const struct shell *shell, size_t argc, char **argv){

	int err;

	if( gBleStatus == xBleNotInitialized ){
		shell_error( shell, "BLE has not been initialized properly" );
		return;
	}

	// Disconnect first if necessary
	if( gBleStatus == xBleIsConnected ){
		err = bt_conn_disconnect( gpCurrentConn, BT_HCI_ERR_REMOTE_USER_TERM_CONN );
		if(err ){
			shell_error( shell, "BLE Disconnect error %d\r\n",err );
		}
		// not needed here, should be set by the callback
		//gBleStatus = xBleIsAdvertising;
	}

	// Stop advertising via BLE
	err = bt_le_adv_stop();
	if (err) {
		shell_error( shell, "bt_le_adv_stop error %d\r\n",err );
		return;
	}

	// Initialized means not advertising
	gBleStatus = xBleIsInitialized; 

	shell_print( shell, "BLE Advertising stopped \r\n" );

	return;
}



void xBleDisconnectCmd(const struct shell *shell, size_t argc, char **argv){

	if( gBleStatus == xBleNotInitialized ){
		shell_error( shell, "WiFi Device should be initialized first with the \"init\" command" );
		return;
	}

	int err = bt_conn_disconnect( gpCurrentConn, BT_HCI_ERR_REMOTE_USER_TERM_CONN );
	if (err == -ENOTCONN ) {
		shell_warn( shell, "BLE not connected" );
		return;
	}
	else if(err){
		shell_error( shell, "BLE Disconnect error %d\r\n",err );
		return;
	}
	
	// not needed here, status should be set by the disconnection callback
	//gBleStatus = xBleIsAdvertising;
	shell_print( shell, "Disconnecting...");
	return;
}


