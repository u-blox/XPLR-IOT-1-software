/*
 * Copyright (c) 2020 Linumiz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_bq27520

#include <drivers/i2c.h>
#include <init.h>
#include <drivers/sensor.h>
#include <pm/device.h>
#include <sys/__assert.h>
#include <string.h>
#include <sys/byteorder.h>
#include <drivers/gpio.h>

#include "bq27520.h"

#define BQ27520_SUBCLASS_DELAY 5 /* subclass 64 & 82 needs 5ms delay */
/* Time to set pin in order to exit shutdown mode */
#define PIN_DELAY_TIME 1U
/* Time it takes device to initialize before doing any configuration */
#define INIT_TIME 100U

static int bq27520_gauge_configure(const struct device *dev);

static uint8_t bq275xx_get_block_offset_location( uint16_t reg_offset_code ){
	if(reg_offset_code <=31 ){
		return 0x00;
	}
	else{
		return 0x01;
	}
}

static int bq27520_command_reg_read(struct bq27520_data *bq27520, uint8_t reg_addr,
				    int16_t *val)
{
	uint8_t i2c_data[2];
	int status;

	status = i2c_burst_read(bq27520->i2c, DT_INST_REG_ADDR(0), reg_addr,
				i2c_data, 2);
	if (status < 0) {
		LOG_ERR("Unable to read register");
		return -EIO;
	}

	*val = (i2c_data[1] << 8) | i2c_data[0];

	return 0;
}

static int bq27520_control_reg_write(struct bq27520_data *bq27520,
				     uint16_t subcommand)
{
	uint8_t i2c_data, reg_addr;
	int status = 0;

	reg_addr = BQ27520_COMMAND_CONTROL_LOW;
	i2c_data = (uint8_t)((subcommand)&0x00FF);

	status = i2c_reg_write_byte(bq27520->i2c, DT_INST_REG_ADDR(0), reg_addr,
				    i2c_data);
	if (status < 0) {
		LOG_ERR("Failed to write into control low register");
		return -EIO;
	}

	k_msleep(BQ27520_SUBCLASS_DELAY);

	reg_addr = BQ27520_COMMAND_CONTROL_HIGH;
	i2c_data = (uint8_t)((subcommand >> 8) & 0x00FF);

	status = i2c_reg_write_byte(bq27520->i2c, DT_INST_REG_ADDR(0), reg_addr,
				    i2c_data);
	if (status < 0) {
		LOG_ERR("Failed to write into control high register");
		return -EIO;
	}

	return 0;
}

static int bq27520_command_reg_write(struct bq27520_data *bq27520, uint8_t command,
				     uint8_t data)
{
	uint8_t i2c_data, reg_addr;
	int status = 0;

	reg_addr = command;
	i2c_data = data;

	status = i2c_reg_write_byte(bq27520->i2c, DT_INST_REG_ADDR(0), reg_addr,
				    i2c_data);
	if (status < 0) {
		LOG_ERR("Failed to write into control register");
		return -EIO;
	}

	return 0;
}

static int bq27520_read_block_data(struct bq27520_data *bq27520, uint8_t offset,
				   uint8_t *data, uint8_t bytes)
{
	uint8_t i2c_data;
	int status = 0;

	i2c_data = BQ27520_EXTENDED_BLOCKDATA_START + (offset % 32);

	status = i2c_burst_read(bq27520->i2c, DT_INST_REG_ADDR(0), i2c_data,
				data, bytes);
	if (status < 0) {
		LOG_ERR("Failed to read block");
		return -EIO;
	}

	k_msleep(BQ27520_SUBCLASS_DELAY);

	return 0;
}

static int bq27520_get_device_type(struct bq27520_data *bq27520, uint16_t *val)
{
	int status;

	status =
		bq27520_control_reg_write(bq27520, BQ27520_CONTROL_DEVICE_TYPE);
	if (status < 0) {
		LOG_ERR("Unable to write control register");
		return -EIO;
	}

	status = bq27520_command_reg_read(bq27520, BQ27520_COMMAND_CONTROL_LOW,
					  val);

	if (status < 0) {
		LOG_ERR("Unable to read register");
		return -EIO;
	}

	return 0;
}


/**
 * @brief sensor value get
 *
 * @return -ENOTSUP for unsupported channels
 */
static int bq27520_channel_get(const struct device *dev,
			       enum sensor_channel chan,
			       struct sensor_value *val)
{
	struct bq27520_data *bq27520 = dev->data;
	float int_temp;

	switch (chan) {
	case SENSOR_CHAN_GAUGE_VOLTAGE:
		val->val1 = ((bq27520->voltage / 1000));
		val->val2 = ((bq27520->voltage % 1000) * 1000U);
		break;

	case SENSOR_CHAN_GAUGE_AVG_CURRENT:
		val->val1 = ((bq27520->avg_current / 1000));
		val->val2 = ((bq27520->avg_current % 1000) * 1000U);
		break;

	case SENSOR_CHAN_GAUGE_STDBY_CURRENT:
		val->val1 = ((bq27520->stdby_current / 1000));
		val->val2 = ((bq27520->stdby_current % 1000) * 1000U);
		break;

	// case SENSOR_CHAN_GAUGE_MAX_LOAD_CURRENT:
	// 	val->val1 = ((bq27520->max_load_current / 1000));
	// 	val->val2 = ((bq27520->max_load_current % 1000) * 1000U);
	// 	break;

	case SENSOR_CHAN_GAUGE_TEMP:
		int_temp = (bq27520->internal_temperature * 0.1f);
		int_temp = int_temp - 273.15f;
		val->val1 = (int32_t)int_temp;
		val->val2 = (int_temp - (int32_t)int_temp) * 1000000;
		break;

	case SENSOR_CHAN_GAUGE_STATE_OF_CHARGE:
		val->val1 = bq27520->state_of_charge;
		val->val2 = 0;
		break;

	case SENSOR_CHAN_GAUGE_STATE_OF_HEALTH:
		val->val1 = bq27520->state_of_health;
		val->val2 = 0;
		break;

	case SENSOR_CHAN_GAUGE_FULL_CHARGE_CAPACITY:
		val->val1 = (bq27520->full_charge_capacity / 1000);
		val->val2 = ((bq27520->full_charge_capacity % 1000) * 1000U);
		break;

	case SENSOR_CHAN_GAUGE_REMAINING_CHARGE_CAPACITY:
		val->val1 = (bq27520->remaining_charge_capacity / 1000);
		val->val2 =
			((bq27520->remaining_charge_capacity % 1000) * 1000U);
		break;

	case SENSOR_CHAN_GAUGE_NOM_AVAIL_CAPACITY:
		val->val1 = (bq27520->nom_avail_capacity / 1000);
		val->val2 = ((bq27520->nom_avail_capacity % 1000) * 1000U);
		break;

	case SENSOR_CHAN_GAUGE_FULL_AVAIL_CAPACITY:
		val->val1 = (bq27520->full_avail_capacity / 1000);
		val->val2 = ((bq27520->full_avail_capacity % 1000) * 1000U);
		break;

	// case SENSOR_CHAN_GAUGE_AVG_POWER:
	// 	val->val1 = (bq27520->avg_power / 1000);
	// 	val->val2 = ((bq27520->avg_power % 1000) * 1000U);
	// 	break;

	default:
		return -ENOTSUP;
	}

	return 0;
}

static int bq27520_sample_fetch(const struct device *dev,
				enum sensor_channel chan)
{
	struct bq27520_data *bq27520 = dev->data;
	int status = 0;

#ifdef CONFIG_BQ27520_LAZY_CONFIGURE
	if (!bq27520->lazy_loaded) {
		status = bq27520_gauge_configure(dev);

		if (status < 0) {
			return status;
		}
		bq27520->lazy_loaded = true;
	}
#endif

	switch (chan) {
	case SENSOR_CHAN_GAUGE_VOLTAGE:
		status = bq27520_command_reg_read(
			bq27520, BQ27520_COMMAND_VOLTAGE, &bq27520->voltage);
		if (status < 0) {
			LOG_ERR("Failed to read voltage");
			return -EIO;
		}
		break;

	case SENSOR_CHAN_GAUGE_AVG_CURRENT:
		status = bq27520_command_reg_read(bq27520,
						  BQ27520_COMMAND_AVG_CURRENT,
						  &bq27520->avg_current);
		if (status < 0) {
			LOG_ERR("Failed to read average current ");
			return -EIO;
		}
		break;

	case SENSOR_CHAN_GAUGE_TEMP:
		status = bq27520_command_reg_read(
			bq27520, BQ27520_COMMAND_INT_TEMP,
			&bq27520->internal_temperature);
		if (status < 0) {
			LOG_ERR("Failed to read internal temperature");
			return -EIO;
		}
		break;

	case SENSOR_CHAN_GAUGE_STDBY_CURRENT:
		status = bq27520_command_reg_read(bq27520,
						  BQ27520_COMMAND_STDBY_CURRENT,
						  &bq27520->stdby_current);
		if (status < 0) {
			LOG_ERR("Failed to read standby current");
			return -EIO;
		}
		break;

	// case SENSOR_CHAN_GAUGE_MAX_LOAD_CURRENT:
	// 	status = bq27520_command_reg_read(bq27520,
	// 					  BQ27520_COMMAND_MAX_CURRENT,
	// 					  &bq27520->max_load_current);
	// 	if (status < 0) {
	// 		LOG_ERR("Failed to read maximum current");
	// 		return -EIO;
	// 	}
	// 	break;

	case SENSOR_CHAN_GAUGE_STATE_OF_CHARGE:
		status = bq27520_command_reg_read(bq27520, BQ27520_COMMAND_SOC,
						  &bq27520->state_of_charge);
		if (status < 0) {
			LOG_ERR("Failed to read state of charge");
			return -EIO;
		}
		break;

	case SENSOR_CHAN_GAUGE_FULL_CHARGE_CAPACITY:
		status = bq27520_command_reg_read(
			bq27520, BQ27520_COMMAND_FULL_CAPACITY,
			&bq27520->full_charge_capacity);
		if (status < 0) {
			LOG_ERR("Failed to read full charge capacity");
			return -EIO;
		}
		break;

	case SENSOR_CHAN_GAUGE_REMAINING_CHARGE_CAPACITY:
		status = bq27520_command_reg_read(
			bq27520, BQ27520_COMMAND_REM_CAPACITY,
			&bq27520->remaining_charge_capacity);
		if (status < 0) {
			LOG_ERR("Failed to read remaining charge capacity");
			return -EIO;
		}
		break;

	case SENSOR_CHAN_GAUGE_NOM_AVAIL_CAPACITY:
		status = bq27520_command_reg_read(bq27520,
						  BQ27520_COMMAND_NOM_CAPACITY,
						  &bq27520->nom_avail_capacity);
		if (status < 0) {
			LOG_ERR("Failed to read nominal available capacity");
			return -EIO;
		}
		break;

	case SENSOR_CHAN_GAUGE_FULL_AVAIL_CAPACITY:
		status =
			bq27520_command_reg_read(bq27520,
						 BQ27520_COMMAND_AVAIL_CAPACITY,
						 &bq27520->full_avail_capacity);
		if (status < 0) {
			LOG_ERR("Failed to read full available capacity");
			return -EIO;
		}
		break;

	// case SENSOR_CHAN_GAUGE_AVG_POWER:
	// 	status = bq27520_command_reg_read(bq27520,
	// 					  BQ27520_COMMAND_AVG_POWER,
	// 					  &bq27520->avg_power);
	// 	if (status < 0) {
	// 		LOG_ERR("Failed to read battery average power");
	// 		return -EIO;
	// 	}
	// 	break;

	case SENSOR_CHAN_GAUGE_STATE_OF_HEALTH:
		status = bq27520_command_reg_read(bq27520, BQ27520_COMMAND_SOH,
						  &bq27520->state_of_health);

		bq27520->state_of_health = (bq27520->state_of_health) & 0x00FF;

		if (status < 0) {
			LOG_ERR("Failed to read state of health");
			return -EIO;
		}
		break;

	default:
		return -ENOTSUP;
	}

	return 0;
}

/**
 * @brief initialise the fuel gauge
 *
 * @return 0 for success
 */
static int bq27520_gauge_init(const struct device *dev)
{
	struct bq27520_data *bq27520 = dev->data;
	const struct bq27520_config *const config = dev->config;
	int status = 0;
	uint16_t id;

#ifdef CONFIG_PM_DEVICE
	if (!device_is_ready(config->int_gpios.port)) {
		LOG_ERR("GPIO device pointer is not ready to be used");
		return -ENODEV;
	}
#endif

	bq27520->i2c = device_get_binding(config->i2c_name);
	if (bq27520->i2c == NULL) {
		LOG_ERR("Could not get pointer to %s device.",
			config->i2c_name);
		return -EINVAL;
	}

	status = bq27520_get_device_type(bq27520, &id);
	if (status < 0) {
		LOG_ERR("Unable to get device ID");
		return -EIO;
	}

	if (id != BQ27520_DEVICE_ID) {
		LOG_ERR("Invalid Device");
		return -EINVAL;
	}

#ifdef CONFIG_BQ27520_LAZY_CONFIGURE
	bq27520->lazy_loaded = false;
#else
	status = bq27520_gauge_configure(dev);
#endif

	return status;
}

static int bq27520_gauge_configure(const struct device *dev)
{
	struct bq27520_data *bq27520 = dev->data;
	const struct bq27520_config *const config = dev->config;
	int status = 0;
	uint8_t checksum_new = 0;
	// uint16_t flags = 0, designenergy_mwh = 0, taperrate = 0;
	uint8_t designcap_msb, designcap_lsb;
	uint8_t term_volt_msb, term_volt_lsb;
	//uint8_t designenergy_msb, designenergy_lsb,
	// 	terminatevolt_msb, terminatevolt_lsb, taperrate_msb,
	// 	taperrate_lsb;
	uint8_t block[32];

	// designenergy_mwh = (uint16_t)3.7 * config->design_capacity;
	// // taperrate =
	// // 	(uint16_t)config->design_capacity / (0.1 * config->taper_current);

	/** Unseal the battery control register **/
	status = bq27520_control_reg_write(bq27520, BQ27520_UNSEAL_KEY_1);
	if (status < 0) {
		LOG_ERR("Unable to unseal the battery");
		return -EIO;
	}

	status = bq27520_control_reg_write(bq27520, BQ27520_UNSEAL_KEY_2);
	if (status < 0) {
		LOG_ERR("Unable to unseal the battery");
		return -EIO;
	}


	// enable block data flash control
	status = bq27520_command_reg_write(bq27520,
					   BQ27520_EXTENDED_DATA_CONTROL, 0x00);
	if (status < 0) {
		LOG_ERR("Failed to enable block data memory");
		return -EIO;
	}


	// ----- Design Capacity Configuration ----- //

	/* Access Config ( Config -> Design Capacity ) subclass */
	status = bq27520_command_reg_write(bq27520, BQ27520_EXTENDED_DATA_CLASS,
					   BQ27520_SUBCLASS_DATA);
	if (status < 0) {
		LOG_ERR("Failed to access config subclass");
		return -EIO;
	}

	/* Write the block offset for Design Capacity */
	status = bq27520_command_reg_write(bq27520, BQ27520_EXTENDED_DATA_BLOCK,
					   bq275xx_get_block_offset_location( BQ27520_OFFSET_DESIGN_CAPACITY ) );
	if (status < 0) {
		LOG_ERR("Failed to update block offset");
		return -EIO;
	}

	
	// read old design capacity
	// status = bq27520_read_block_data(bq27520, BQ27520_OFFSET_DESIGN_CAPACITY, old_design_capacity_bytes, 2);
	// if (status < 0) {
	// 	LOG_ERR("Unable to read design capacity");
	// 	return -EIO;
	// }

	/* Read the block checksum */
	// status = i2c_reg_read_byte(bq27520->i2c, DT_INST_REG_ADDR(0),
	// 			   BQ27520_EXTENDED_CHECKSUM, &checksum_old);
	// if (status < 0) {
	// 	LOG_ERR("Unable to read block checksum");
	// 	return -EIO;
	// }


	// Write the design capacity to the block
	designcap_msb = config->design_capacity >> 8;
	designcap_lsb = config->design_capacity & 0x00FF;


	status = i2c_reg_write_byte(bq27520->i2c, DT_INST_REG_ADDR(0),
				    BQ27520_OFFSET_DESIGN_CAPACITY + BQ27520_EXTENDED_BLOCKDATA_START,
				    designcap_msb);
	if (status < 0) {
		LOG_ERR("Failed to write designCAP MSB");
		return -EIO;
	}

	status = i2c_reg_write_byte(bq27520->i2c, DT_INST_REG_ADDR(0),
				    BQ27520_OFFSET_DESIGN_CAPACITY + BQ27520_EXTENDED_BLOCKDATA_START + 1,
				    designcap_lsb);
	if (status < 0) {
		LOG_ERR("Failed to erite designCAP LSB");
		return -EIO;
	}

	//read block as it is now
	memset(block,0,sizeof(block));
	status = bq27520_read_block_data( bq27520, 0x00, block, sizeof(block) );
	if (status < 0) {
		LOG_ERR("Unable to read block data");
		return -EIO;
	}

	//estimate checksum
	checksum_new = 0;
	for (uint8_t i = 0; i < 32; i++) {
		checksum_new += block[i];
	}
	checksum_new = 255 - checksum_new;

	// tmp_checksum = ( 255 - checksum_old - old_design_capacity_bytes[0] - old_design_capacity_bytes[1] ) % 256;
	// checksum_new = 255 - (  tmp_checksum + designcap_msb + designcap_lsb ) % 256;

	
	status = bq27520_command_reg_write(bq27520, BQ27520_EXTENDED_CHECKSUM,
					   checksum_new);
	if (status < 0) {
		LOG_ERR("Failed to update new checksum");
		return -EIO;
	}


	// ----- End of Design Capacity Configuration ----- //

	// ----- Terminate Voltage Configuration ----- //

		/* Access Gas Gauging ( Gas Gauging -> Terminate Voltage ) subclass */
	status = bq27520_command_reg_write(bq27520, BQ27520_EXTENDED_DATA_CLASS,
					   BQ27520_SUBCLASS_IT_CFG);
	if (status < 0) {
		LOG_ERR("Failed to access Data subclass");
		return -EIO;
	}

	/* Write the block offset for Design Capacity */
	status = bq27520_command_reg_write(bq27520, BQ27520_EXTENDED_DATA_BLOCK,
					   bq275xx_get_block_offset_location( BQ27520_OFFSET_TERMINATE_VOLTAGE ) );
	if (status < 0) {
		LOG_ERR("Failed to update block offset");
		return -EIO;
	}


	// Write the design capacity to the block
	term_volt_msb = config->terminate_voltage >> 8;
	term_volt_lsb = config->terminate_voltage & 0x00FF;


	status = i2c_reg_write_byte(bq27520->i2c, DT_INST_REG_ADDR(0),
				    (BQ27520_OFFSET_TERMINATE_VOLTAGE % 32) + BQ27520_EXTENDED_BLOCKDATA_START,
				    term_volt_msb);
	if (status < 0) {
		LOG_ERR("Failed to write terminate voltage MSB");
		return -EIO;
	}

	status = i2c_reg_write_byte(bq27520->i2c, DT_INST_REG_ADDR(0),
				    (BQ27520_OFFSET_TERMINATE_VOLTAGE % 32 ) + BQ27520_EXTENDED_BLOCKDATA_START + 1,
				    term_volt_lsb);
	if (status < 0) {
		LOG_ERR("Failed to write terminate voltage LSB");
		return -EIO;
	}

	//read block as it is now
	memset(block,0,sizeof(block));
	status = bq27520_read_block_data( bq27520, 0x00, block, sizeof(block) );
	if (status < 0) {
		LOG_ERR("Unable to read block data");
		return -EIO;
	}

	//estimate checksum
	checksum_new = 0;
	for (uint8_t i = 0; i < 32; i++) {
		checksum_new += block[i];
	}
	checksum_new = 255 - checksum_new;

	// tmp_checksum = ( 255 - checksum_old - old_design_capacity_bytes[0] - old_design_capacity_bytes[1] ) % 256;
	// checksum_new = 255 - (  tmp_checksum + designcap_msb + designcap_lsb ) % 256;

	
	status = bq27520_command_reg_write(bq27520, BQ27520_EXTENDED_CHECKSUM,
					   checksum_new);
	if (status < 0) {
		LOG_ERR("Failed to update new checksum");
		return -EIO;
	}

	// ----- End of Terminate Voltage Configuration ----- //

	// ----- Final Voltage Configuration ----- //

	/* Access Gas Gauging ( Gas Gauging -> Terminate Voltage ) subclass */
	status = bq27520_command_reg_write(bq27520, BQ27520_EXTENDED_DATA_CLASS,
					   BQ27520_SUBCLASS_DISCHARGE);
	if (status < 0) {
		LOG_ERR("Failed to access Discharge subclass");
		return -EIO;
	}

	/* Write the block offset for Design Capacity */
	status = bq27520_command_reg_write(bq27520, BQ27520_EXTENDED_DATA_BLOCK,
					   bq275xx_get_block_offset_location( BQ27520_OFFSET_FINAL_VOLTAGE ) );
	if (status < 0) {
		LOG_ERR("Failed to update block offset");
		return -EIO;
	}


	// Final Voltage should be equal to terminate voltage
	term_volt_msb = config->terminate_voltage >> 8;
	term_volt_lsb = config->terminate_voltage & 0x00FF;


	status = i2c_reg_write_byte(bq27520->i2c, DT_INST_REG_ADDR(0),
				    BQ27520_OFFSET_FINAL_VOLTAGE + BQ27520_EXTENDED_BLOCKDATA_START,
				    term_volt_msb);
	if (status < 0) {
		LOG_ERR("Failed to write final voltage MSB");
		return -EIO;
	}

	status = i2c_reg_write_byte(bq27520->i2c, DT_INST_REG_ADDR(0),
				    BQ27520_OFFSET_FINAL_VOLTAGE + BQ27520_EXTENDED_BLOCKDATA_START + 1,
				    term_volt_lsb);
	if (status < 0) {
		LOG_ERR("Failed to write final voltage LSB");
		return -EIO;
	}

	//read block as it is now
	memset(block,0,sizeof(block));
	status = bq27520_read_block_data( bq27520, 0x00, block, sizeof(block) );
	if (status < 0) {
		LOG_ERR("Unable to read block data");
		return -EIO;
	}

	//estimate checksum
	checksum_new = 0;
	for (uint8_t i = 0; i < 32; i++) {
		checksum_new += block[i];
	}
	checksum_new = 255 - checksum_new;

	// tmp_checksum = ( 255 - checksum_old - old_design_capacity_bytes[0] - old_design_capacity_bytes[1] ) % 256;
	// checksum_new = 255 - (  tmp_checksum + designcap_msb + designcap_lsb ) % 256;

	
	status = bq27520_command_reg_write(bq27520, BQ27520_EXTENDED_CHECKSUM,
					   checksum_new);
	if (status < 0) {
		LOG_ERR("Failed to update new checksum");
		return -EIO;
	}	



	// ----- End of Final Voltage Configuration ----- //

	// ----- Reset Battery Gauge for changes to take effect ----- //

	status = bq27520_control_reg_write(bq27520, BQ27520_CONTROL_RESET);
	if (status < 0) {
		LOG_ERR("Failed to soft reset the gauge");
		return -EIO;
	}

	// add a small delay
	k_msleep(100);


	/* Seal the gauge */
	status = bq27520_control_reg_write(bq27520, BQ27520_CONTROL_SEALED);
	if (status < 0) {
		LOG_ERR("Failed to seal the gauge");
		return -EIO;
	}

	return 0;
}



static const struct sensor_driver_api bq27520_battery_driver_api = {
	.sample_fetch = bq27520_sample_fetch,
	.channel_get = bq27520_channel_get,
};

static const struct bq27520_config bq27520_config = {
	.i2c_name = DT_INST_BUS_LABEL(0),
	.i2c_address = DT_INST_REG_ADDR(0),
	.design_capacity = CONFIG_BQ27520_DESIGN_CAPACITY,
	//.design_voltage = 3000,
	//.taper_current = 2,
	.terminate_voltage = CONFIG_BQ27520_TERMINATE_VOLTAGE
};

static struct bq27520_data bq27520_data;

DEVICE_DT_INST_DEFINE(0, bq27520_gauge_init, NULL,
		    &bq27520_data, &bq27520_config,
		    POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,
		    &bq27520_battery_driver_api);

