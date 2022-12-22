/*
 * Copyright (c) 2022, u-blox
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT tdk_icg20330

#include "icg20330.h"
#include <sys/util.h>
#include <sys/__assert.h>
#include <logging/log.h>
#include <math.h>

LOG_MODULE_REGISTER(icg20330, CONFIG_SENSOR_LOG_LEVEL);

// Gyroscope configuration register (little endian layout, bit-fields not portable)	
struct{
	uint8_t FCHOICE_B: 2;
	uint8_t Reserved: 1;
	uint8_t FS_SEL : 2;
	uint8_t ZG_ST : 1;
	uint8_t YG_ST : 1;
	uint8_t XG_ST : 1;
}GyroConfigLittleEndian={0}; 

// Gyroscope configuration register (big endian layout, bit-fields not portable)	
struct{
	uint8_t XG_ST : 1;
	uint8_t YG_ST : 1;
	uint8_t ZG_ST : 1;
	uint8_t FS_SEL : 2;
	uint8_t Reserved: 1;
	uint8_t FCHOICE_B: 2;
}GyroConfigBigEndian={0};



/** Function that tests endianess of machine
 * This is used to test endianness because bitfields are used to describe register bits
 * in this driver. Bit fields are dependent on endianness
 * 
*  @return      True if little endianness, 0 otherwise
*/ 
bool byteOrderIsLittleEndian(void) {
        short int word = 0x0001;
        char *b = (char *)&word;
        return (b[0] ? true : false);
}



static int icg20330_sample_fetch(const struct device *dev,
				  enum sensor_channel chan)
{
	const struct icg20330_config *config = dev->config;
	struct icg20330_data *data = dev->data;
	uint8_t buffer[ICG20330_MAX_NUM_BYTES];
	int16_t *raw;
	int ret = 0;
	int i;

	if (chan != SENSOR_CHAN_ALL) {
		LOG_ERR("Unsupported sensor channel");
		return -ENOTSUP;
	}

	k_sem_take(&data->sem, K_FOREVER);

	/* Read all the channels in one I2C transaction. */
	if (i2c_burst_read(data->i2c, config->i2c_address,
			   ICG20330_REG_OUTXMSB, buffer, sizeof(buffer))) {
		LOG_ERR("Could not fetch sample");
		ret = -EIO;
		goto exit;
	}

	/* Parse the buffer into raw channel data (16-bit integers). To save
	 * RAM, store the data in raw format and wait to convert to the
	 * normalized sensor_value type until later.
	 */
	raw = &data->raw[0];

	for (i = 0; i < sizeof(buffer); i += 2) {
		*raw++ = (buffer[i] << 8) | (buffer[i+1]);
	}

exit:
	k_sem_give(&data->sem);

	return ret;
}

static void icg20330_convert(struct sensor_value *val, int16_t raw,
			      enum icg20330_range range)
{
	double degreesPerSecond;
	int32_t integer;
	double dec;

	int16_t gyroSensitivityFactor;

	switch(range){
		case ICG20330_RANGE_31_25_DPS: gyroSensitivityFactor = ICG20330_SENS_SCALE_FACTOR_31_25_DPS;
									   break;
        case ICG20330_RANGE_62_5_DPS: gyroSensitivityFactor = ICG20330_SENS_SCALE_FACTOR_62_5_DPS;
									  break;
        case ICG20330_RANGE_125_DPS:  gyroSensitivityFactor = ICG20330_SENS_SCALE_FACTOR_125_DPS;
									  break;
        case ICG20330_RANGE_250_DPS:  gyroSensitivityFactor = ICG20330_SENS_SCALE_FACTOR_250_DPS;
									  break;
		default:   val->val1 = 0;
				   val->val2 = 0;
				   return; 
	}
	
	degreesPerSecond = ( (double)raw / (double)gyroSensitivityFactor );
	integer = (int32_t)degreesPerSecond;
	dec = degreesPerSecond - integer;

	val->val1 = (int32_t) integer;
	val->val2 = dec*1000000;

	return;
}

static int icg20330_channel_get(const struct device *dev,
				 enum sensor_channel chan,
				 struct sensor_value *val)
{
	const struct icg20330_config *config = dev->config;
	struct icg20330_data *data = dev->data;
	int start_channel;
	int num_channels;
	int16_t *raw;
	int ret;
	int i;

	k_sem_take(&data->sem, K_FOREVER);

	/* Start with an error return code by default, then clear it if we find
	 * a supported sensor channel.
	 */
	ret = -ENOTSUP;

	/* Convert raw gyroscope data to the normalized sensor_value type. */
	switch (chan) {
	case SENSOR_CHAN_GYRO_X:
		start_channel = ICG20330_CHANNEL_GYRO_X;
		num_channels = 1;
		break;
	case SENSOR_CHAN_GYRO_Y:
		start_channel = ICG20330_CHANNEL_GYRO_Y;
		num_channels = 1;
		break;
	case SENSOR_CHAN_GYRO_Z:
		start_channel = ICG20330_CHANNEL_GYRO_Z;
		num_channels = 1;
		break;
	case SENSOR_CHAN_GYRO_XYZ:
		start_channel = ICG20330_CHANNEL_GYRO_X;
		num_channels = 3;
		break;
	default:
		start_channel = 0;
		num_channels = 0;
		break;
	}

	raw = &data->raw[start_channel];
	for (i = 0; i < num_channels; i++) {
		icg20330_convert(val++, *raw++, config->range);
	}

	if (num_channels > 0) {
		ret = 0;
	}

	if (ret != 0) {
		LOG_ERR("Unsupported sensor channel");
	}

	k_sem_give(&data->sem);

	return ret;
}


static int icg20330_init(const struct device *dev)
{
	const struct icg20330_config *config = dev->config;
	struct icg20330_data *data = dev->data;
	uint8_t whoami;
	

	/* Get the I2C device */
	data->i2c = device_get_binding(config->i2c_name);
	if (data->i2c == NULL) {
		LOG_ERR("Could not find I2C device");
		return -EINVAL;
	}


	// --- preliminary soft reset operation
	i2c_reg_write_byte(data->i2c, config->i2c_address,
			   ICG20330_REG_PWR_MGMT_1, 0x81);
	k_sleep(K_MSEC(100));



	// Device resets in sleep mode, which many of the registers are not accesssible
	// including WHOAMI
	// first, change the Power Management Resgisters

	// take out of sleep so we can set other registers
	i2c_reg_write_byte(data->i2c, config->i2c_address,
			   ICG20330_REG_PWR_MGMT_1, 0x01);


	/* Read the WHOAMI register to make sure we are talking to ICG20330
	 * and not some other type of device that happens to have the same I2C
	 * address.
	 */
	if (i2c_reg_read_byte(data->i2c, config->i2c_address,
			      ICG20330_REG_WHOAMI, &whoami)) {
		LOG_ERR("Could not get WHOAMI value");
		return -EIO;
	}

	if (whoami != config->whoami) {
		LOG_ERR("WHOAMI value received 0x%x, expected 0x%x",
			    whoami, config->whoami);
		return -EIO;
	}

	
	// Gyroscope configuration register
	uint8_t *ConfigRegByte;
	if( byteOrderIsLittleEndian() ){
		// FCHOICE_B is set to 01, so that DLPF_CFG is not taken into account.
	  	GyroConfigLittleEndian.FCHOICE_B=1;
		// set Range/Sensitivity
		GyroConfigLittleEndian.FS_SEL = config->range;
		ConfigRegByte = (uint8_t *)&GyroConfigLittleEndian;
	}
	else{
		// FCHOICE_B is set to 01, so that DLPF_CFG is not taken into account.
	  	GyroConfigBigEndian.FCHOICE_B=1;
		// set Range/Sensitivity
		GyroConfigBigEndian.FS_SEL = config->range;
		ConfigRegByte = (uint8_t *)&GyroConfigBigEndian;
	}
	
	// Set the range via configuration register
    i2c_reg_write_byte(data->i2c, config->i2c_address,
			   ICG20330_REG_GYRO_CONFIG, *ConfigRegByte);

	
	// reset sensor and configure
	i2c_reg_write_byte(data->i2c, config->i2c_address,
			   ICG20330_REG_SIGNAL_PATH_RESET, ICG20330_TEMP_SIGNAL_PATH_RESET);
	i2c_reg_write_byte(data->i2c, config->i2c_address,
			   ICG20330_REG_USER_CTRL, ICG20330_REG_USER_CTRL_VAL);


	k_sem_init(&data->sem, 0, K_SEM_MAX_LIMIT);

	k_sem_give(&data->sem);

	LOG_DBG("Init complete");

	return 0;
}

static const struct sensor_driver_api icg20330_driver_api = {
	.sample_fetch = icg20330_sample_fetch,
	.channel_get = icg20330_channel_get,
};

static const struct icg20330_config icg20330_config = {
	.i2c_name = DT_INST_BUS_LABEL(0),
	.i2c_address = DT_INST_REG_ADDR(0),
	.whoami = CONFIG_ICG20330_WHOAMI,
	.range = CONFIG_ICG20330_RANGE,
};

static struct icg20330_data icg20330_data;

DEVICE_DT_INST_DEFINE(0, icg20330_init, NULL,
		    &icg20330_data, &icg20330_config,
		    POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,
		    &icg20330_driver_api);
