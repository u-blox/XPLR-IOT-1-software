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
 * @brief This file implements the common functions used by all sensors
 * in the sensor aggregation use case of XPLR-IOT-1
 */


#include "x_sens_common.h"

#include "x_sens_bme280.h"
#include "x_sens_battery_gauge.h"
#include "x_sens_icg20330.h"
#include "x_sens_lis3mdl.h"
#include "x_sens_lis2dh12.h"
#include "x_sens_ltr303.h"
#include "x_pos_maxm10s.h"

#include "x_errno.h"

#include "x_sensor_aggregation_function.h"


/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */


void xSensEnableAll(void){

    xSensBme280Enable();
    xSensLis3mdlEnable();
    xSensIcg20330Enable();
    xSensLis2dh12Enable();
    xSensLtr303Enable();
    xSensBatGaugeEnable();
    xPosMaxM10Enable();
    return;
}



void xSensDisableAll(void){

    xSensBme280Disable();
    xSensLis3mdlDisable();
    xSensIcg20330Disable();
    xSensLis2dh12Disable();
    xSensLtr303Disable();
    xSensBatGaugeDisable();
    xPosMaxM10Disable();
    return;
}



err_code xSensSetUpdatePeriodAll(uint32_t milliseconds){

    err_code err;
    err = xPosMaxM10SetUpdatePeriod( milliseconds );
    if( err != X_ERR_SUCCESS ){
        return err;
    }

    xSensBme280SetUpdatePeriod( milliseconds );
    xSensBatGaugeSetUpdatePeriod( milliseconds );
    xSensIcg20330SetUpdatePeriod( milliseconds );
    xSensLis2dh12SetUpdatePeriod( milliseconds );
    xSensLis3mdlSetUpdatePeriod( milliseconds );
    xSensLtr303SetUpdatePeriod( milliseconds );
    
    return X_ERR_SUCCESS;
}



void xSensPublishAll(void){
        
        xSensBme280EnablePublish(true);
        xSensLis3mdlEnablePublish(true);
        xSensIcg20330EnablePublish(true);
        xSensLis2dh12EnablePublish(true);
        xSensLtr303EnablePublish(true);
        xSensBatGaugeEnablePublish(true);
        xPosMaxM10EnablePublish(true);
}



void xSensPublishNone(void){
        
        xSensBme280EnablePublish(false);
        xSensLis3mdlEnablePublish(false);
        xSensIcg20330EnablePublish(false);
        xSensLis2dh12EnablePublish(false);
        xSensLtr303EnablePublish(false);
        xSensBatGaugeEnablePublish(false);
        xPosMaxM10EnablePublish(false);
}



bool xSensIsChangeAllowed(void){
	// if sensor aggragation mode is enabled do not allow changes
	if( xSensorAggregationGetMode() != xSensAggModeDisabled ){
		
		// however if mode is enabled, but its setup is not finished yet, allow changes required by the mode setup itself
		// (this logic is applied, because changing the mode after the setup of the sensors is done may publish
		// messages in separate sensor topics, before the mode is setup completely)
		if( !xSensorAggregationIsLocked() ){	
			return false;
		}
	}
	return true;
}

