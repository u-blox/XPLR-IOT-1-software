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
 * @brief This file implements the API defined in x_nfc.h header file,
 * that controls NFC functionality in the context of Sensor Aggregation Use
 * Case firmware for XPLR-IOT-1.
 * 
 * It also contains the callback implementation of NFC functionality.
 */


#include "x_nfc.h"

// Zephyr/SDK related
#include <nfc_t2t_lib.h>
#include <nfc/ndef/uri_msg.h>
#include <nfc/ndef/uri_rec.h>
#include <logging/log.h>

//Sensor aggregation related
#include "x_logging.h"

/* ----------------------------------------------------------------
 * STATIC FUNCTION DECLARATION
 * -------------------------------------------------------------- */

/** Callback triggered on NFC events */
static void nfcCb(void *context, enum nfc_t2t_event event,
			 const uint8_t *data, size_t data_length);


/* ----------------------------------------------------------------
 * ZEPHYR RELATED DEFINITIONS/DECLARATIONS
 * -------------------------------------------------------------- */

LOG_MODULE_REGISTER(LOGMOD_NAME_NFC, LOG_LEVEL_DBG);


/* ----------------------------------------------------------------
 * GLOBALS
 * -------------------------------------------------------------- */

/** Contains the URI string (URL the browser should open when device
 *  XPLR-IOT-1 is scanned with NFC device)
*/
uint8_t const gUriData[] = NFC_WELCOME_URI;

/** Length of gUriData */
uint16_t gUriLen = sizeof( gUriData );

/** Buffer used to hold an NFC NDEF message. */
static uint8_t gNdefMsgBuf[NDEF_MSG_BUF_SIZE];



/* ----------------------------------------------------------------
 * CALLBACKS
 * -------------------------------------------------------------- */

static void nfcCb(void *context,
			 enum nfc_t2t_event event,
			 const uint8_t *data,
			 size_t data_length)
{
	ARG_UNUSED(context);
	ARG_UNUSED(data);
	ARG_UNUSED(data_length);

	switch (event) {
	case NFC_T2T_EVENT_FIELD_ON:
		//field on action
		break;
	case NFC_T2T_EVENT_FIELD_OFF:
		//field off action
		break;
	default:
		break;
	}
}



/* ----------------------------------------------------------------
 * PUBLIC FUNCTION IMPLEMENTATION
 * -------------------------------------------------------------- */

err_code xNfcConfig(void){

    int32_t err;
    uint32_t len = sizeof(gNdefMsgBuf);

	LOG_INF("Starting NFC Configuration\n");

	/* Set up NFC */
	if ( ( err = nfc_t2t_setup(nfcCb, NULL) ) < 0) {
		LOG_ERR("Cannot setup NFC T2T library!\n");
		return err;
	}

    // Prepare welcome uri (welcome url directing to XPLR-IOT-1 Quick Start Guide)
	if( ( err = nfc_ndef_uri_msg_encode( NFC_URI_NONE, gUriData, gUriLen, gNdefMsgBuf, &len) ) < 0 ){
		LOG_ERR("Cannot encode message!\n");
        return err;
	}


	/* Set created message as the NFC payload */
	if ( ( err = nfc_t2t_payload_set(gNdefMsgBuf, len) ) < 0) {
		LOG_ERR("Cannot set payload!\n");
		return err;
	}

	LOG_INF("NFC configuration done\n");
    return err;
}



err_code xNfcInit(void){

    int32_t err; 

    /* Start sensing NFC field */
	if ( ( err = nfc_t2t_emulation_start() ) < 0 ) {
		LOG_ERR("Cannot start emulation!\n");
		return err;
	}


	LOG_INF("NFC started\n");
    return err;

}