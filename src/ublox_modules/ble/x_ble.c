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
 * received from BLE via this Service are typed immediately in the uart 
 * console. The user cannot send data from XPLR-IOT-1 to the connected BLE
 * device.
 */



#include "x_ble.h"

#include <stdio.h>

// Zephyr/gBleScanRespDataK related
#include <zephyr.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci.h>
#include <bluetooth/services/nus.h>
#include <logging/log.h>

//Sensor aggregation related
#include "x_logging.h"


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

/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

LOG_MODULE_REGISTER(LOGMOD_NAME_BLE, LOG_LEVEL_DBG);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/** Incoming chars buffer */
char gRxBuffer[128]={0};

/** Current Connection Reference */
static struct bt_conn *gpCurrentConn;

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

}



static void xBleDisonnectedCb(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected: %s (reason %u)", log_strdup(addr), reason);

	bt_conn_unref(gpCurrentConn);
	gpCurrentConn = NULL;
		
}



static void xBleReceiveCb(struct bt_conn *conn, const uint8_t *const data,
			  uint16_t len)
{
	char addr[BT_ADDR_LE_STR_LEN] = {0};

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, ARRAY_SIZE(addr));

	LOG_INF("Received data from: %s\r\n", log_strdup(addr));
	
	if( len >= sizeof(gRxBuffer) ){
		LOG_INF("Receive buffer overflow\r\n");
		return;
	}

	memset( gRxBuffer, 0, sizeof(gRxBuffer) );
	memcpy( gRxBuffer, data, len);

	printf("%s\r\n",gRxBuffer);

}


/* ----------------------------------------------------------------
 * PUBLIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */

err_code xBleInit(void){

    bt_conn_cb_register(&gBleConnectionCallbacks);
	
	int err = bt_enable(NULL);
	if (err) {
		LOG_ERR("bt_enable error %d\r\n",err);
		return err;
	}

	LOG_INF("Bluetooth initialized\r\n");

	err = bt_nus_init(&gBleNusCallbacks);
	if (err) {
		LOG_INF("Failed to initialize UART service (err: %d)\r\n", err);
		return err;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN, gBleAdvData, ARRAY_SIZE(gBleAdvData), gBleScanRespData,
			      ARRAY_SIZE(gBleScanRespData));
	if (err) {
		LOG_INF("Advertising failed to start (err %d)\r\n", err);
		return err;
	}

	return err;

}