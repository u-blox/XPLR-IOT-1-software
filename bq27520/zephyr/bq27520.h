/*
 * Copyright(c) 2020 Linumiz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_BATTERY_BQ27520_H_
#define ZEPHYR_DRIVERS_SENSOR_BATTERY_BQ27520_H_

#include <logging/log.h>
#include <drivers/gpio.h>
LOG_MODULE_REGISTER(bq27520, CONFIG_SENSOR_LOG_LEVEL);

/*** General Constant ***/
//#define BQ274XX_UNSEAL_KEY 0x8000 /* Secret code to unseal the BQ27441-G1A */

#define BQ27520_UNSEAL_KEY_1 0x0414
#define BQ27520_UNSEAL_KEY_2 0x3672

#define BQ27520_DEVICE_ID 0x0520 /* Default device ID */

/*** Standard Commands ***/
#define BQ27520_COMMAND_CONTROL_LOW 0x00 /* Control() low register */
#define BQ27520_COMMAND_CONTROL_HIGH 0x01 /* Control() high register */
#define BQ27520_COMMAND_ATRATE 0x02
#define BQ27520_COMMAND_ATRATE_TIME_TO_EMPTY 0x04
#define BQ27520_COMMAND_TEMP 0x06 /* Temperature() */
#define BQ27520_COMMAND_VOLTAGE 0x08 /* Voltage() */
#define BQ27520_COMMAND_FLAGS 0x0A /* Flags() */
#define BQ27520_COMMAND_NOM_CAPACITY 0x0C /* NominalAvailableCapacity() */
#define BQ27520_COMMAND_AVAIL_CAPACITY 0x0E /* FullAvailableCapacity() */
#define BQ27520_COMMAND_REM_CAPACITY 0x10 /* RemainingCapacity() */
#define BQ27520_COMMAND_FULL_CAPACITY 0x12 /* FullChargeCapacity() */
#define BQ27520_COMMAND_AVG_CURRENT 0x14 /* AverageCurrent() */
#define BQ27520_COMMAND_TIME_TO_EMPTY 0x16
#define BQ27520_COMMAND_STDBY_CURRENT 0x18 /* StandbyCurrent() */
#define BQ27520_COMMAND_STDBY_TO_EMPTY 0x1A
#define BQ27520_COMMAND_SOH 0x1C /* StateOfHealth() */
#define BQ27520_COMMAND_CYCLE_COUNT 0x1E
#define BQ27520_COMMAND_SOC 0x20 /* StateOfCharge() */
#define BQ27520_COMMAND_INS_CURRENT 0x22
#define BQ27520_COMMAND_INT_TEMP 0x28 /* InternalTemperature() */
#define BQ27520_COMMAND_RESISTANCE_SCALE 0x2A
#define BQ27520_COMMAND_OP_CONFIG 0x2C /* Operation Confuguration */
#define BQ27520_COMMAND_DESIGN_CAPACITY 0x2E
#define BQ27520_COMMAND_UNFILTERED_RM 0x6C
#define BQ27520_COMMAND_FILTERED_RM 0x6E
#define BQ27520_COMMAND_UNFILTERED_FCC 0x70
#define BQ27520_COMMAND_FILTERED_FCC 0x72
#define BQ27520_COMMAND_TRUE_SOC 0x74

// #define BQ274XX_COMMAND_MAX_CURRENT 0x14 /* MaxLoadCurrent() */
// #define BQ274XX_COMMAND_AVG_POWER 0x18 /* AveragePower() */
// #define BQ274XX_COMMAND_REM_CAP_UNFL 0x28 /* RemainingCapacityUnfiltered() */
// #define BQ274XX_COMMAND_REM_CAP_FIL 0x2A /* RemainingCapacityFiltered() */
// #define BQ274XX_COMMAND_FULL_CAP_UNFL 0x2C /* FullChargeCapacityUnfiltered() */
// #define BQ274XX_COMMAND_FULL_CAP_FIL 0x2E /* FullChargeCapacityFiltered() */
// #define BQ274XX_COMMAND_SOC_UNFL 0x30 /* StateOfChargeUnfiltered() */


/*** Control Sub-Commands ***/
#define BQ27520_CONTROL_STATUS 0x0000
#define BQ27520_CONTROL_DEVICE_TYPE 0x0001
#define BQ27520_CONTROL_FW_VERSION 0x0002
#define BQ27520_CONTROL_PREV_MACWRITE 0x0007
#define BQ27520_CONTROL_CHEM_ID 0x0008
#define BQ27520_CONTROL_OCV_CMD 0x000C
#define BQ27520_CONTROL_BAT_INSERT 0x000D
#define BQ27520_CONTROL_BAT_REMOVE 0x000E
#define BQ27520_CONTROL_SET_HIBERNATE 0x0011
#define BQ27520_CONTROL_CLEAR_HIBERNATE 0x0012
#define BQ27520_CONTROL_SET_SNOOZE 0x0013
#define BQ27520_CONTROL_CLEAR_SNOOZE 0x0014
#define BQ27520_CONTROL_DF_VERSION 0x001F
#define BQ27520_CONTROL_SEALED 0x0020
#define BQ27520_CONTROL_IT_ENABLE 0x0021
#define BQ27520_CONTROL_RESET 0x0041

//#define BQ274XX_CONTROL_DM_CODE 0x0004
// #define BQ274XX_CONTROL_SET_CFGUPDATE 0x0013
// #define BQ274XX_CONTROL_SHUTDOWN_ENABLE 0x001B
// #define BQ274XX_CONTROL_SHUTDOWN 0x001C
// #define BQ274XX_CONTROL_PULSE_SOC_INT 0x0023
// #define BQ274XX_CONTROL_SOFT_RESET 0x0042
// #define BQ274XX_CONTROL_EXIT_CFGUPDATE 0x0043
// #define BQ274XX_CONTROL_EXIT_RESIM 0x0044


/*** Extended Data Commands ***/

//#define BQ274XX_EXTENDED_OPCONFIG 0x3A /* OpConfig() */
//#define BQ274XX_EXTENDED_CAPACITY 0x3C /* DesignCapacity() */

#define BQ27520_EXTENDED_DATA_CLASS 0x3E /* DataFlashClass() */
#define BQ27520_EXTENDED_DATA_BLOCK 0x3F /* DataFlashBlock() */
#define BQ27520_EXTENDED_BLOCKDATA_START 0x40 /* BlockData_start() */
#define BQ27520_EXTENDED_BLOCKDATA_END 0x5F /* BlockData_end() */
#define BQ27520_EXTENDED_CHECKSUM 0x60 /* BlockDataCheckSum() */
#define BQ27520_EXTENDED_DATA_CONTROL 0x61 /* BlockDataControl() */
#define BQ27520_EXTENDED_APP_STATUS 0x6A /* Application Status() */


// ???????????????
// #define BQ274XX_EXTENDED_BLOCKDATA_DESIGN_CAP_HIGH 0x4A /* BlockData */
// #define BQ274XX_EXTENDED_BLOCKDATA_DESIGN_CAP_LOW 0x4B
// #define BQ274XX_EXTENDED_BLOCKDATA_DESIGN_ENR_HIGH 0x4C
// #define BQ274XX_EXTENDED_BLOCKDATA_DESIGN_ENR_LOW 0x4D
// #define BQ274XX_EXTENDED_BLOCKDATA_TERMINATE_VOLT_HIGH 0x50
// #define BQ274XX_EXTENDED_BLOCKDATA_TERMINATE_VOLT_LOW 0x51
// #define BQ274XX_EXTENDED_BLOCKDATA_TAPERRATE_HIGH 0x5B
// #define BQ274XX_EXTENDED_BLOCKDATA_TAPERRATE_LOW 0x5C

//#define BQ274XX_DELAY 1000


#define BQ27520_SUBCLASS_DATA 0x30
#define BQ27520_SUBCLASS_IT_CFG 0x50
#define BQ27520_SUBCLASS_DISCHARGE 0x31

#define BQ27520_OFFSET_DESIGN_CAPACITY   10
#define BQ27520_OFFSET_TERMINATE_VOLTAGE 55
#define BQ27520_OFFSET_FINAL_VOLTAGE     14



struct bq27520_data {
	const struct device *i2c;
#ifdef CONFIG_BQ27520_LAZY_CONFIGURE
	bool lazy_loaded;
#endif
	uint16_t voltage;
	int16_t avg_current;
	int16_t stdby_current;
	//int16_t max_load_current;
	//int16_t avg_power;
	uint16_t state_of_charge;
	int16_t state_of_health;
	uint16_t internal_temperature;
	uint16_t full_charge_capacity;
	uint16_t remaining_charge_capacity;
	uint16_t nom_avail_capacity;
	uint16_t full_avail_capacity;
};

struct bq27520_config {
	//char *bus_name;
	char *i2c_name;
	uint8_t i2c_address;
	//uint16_t design_voltage;
	uint16_t design_capacity;
	//uint16_t taper_current;
	uint16_t terminate_voltage;
};

#endif
