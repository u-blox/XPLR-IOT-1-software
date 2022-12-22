/*
 * Copyright (c) 2017, NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <drivers/sensor.h>
#include <drivers/i2c.h>
#include <drivers/gpio.h>

#define ICG20330_REG_OUTXMSB				0x43	// points to ZG_OFFS_USRH for high byte of Z channel...  burst read all gyro info

#define ICG20330_REG_WHOAMI					0x75

#define ICG20330_REG_USER_CTRL				0x6A
#define ICG20330_REG_USER_CTRL_VAL			0x45	// SIG_COND_RST, FIFO_RST, (leave in I2C mode), enable FIFO

#define ICG20330_REG_PWR_MGMT_1				0x6B
#define ICG20330_PWR_MGMT_1_VAL				0x00 	// takes out of sleep, internal 20Mhz oscillator

#define ICG20330_REG_PWR_MGMT_2				0x6C
#define ICG20330_PWR_MGMT_2_GYROS_OFF_VAL	0x00 	// X,Y and Z GYROS disabled
#define ICG20330_PWR_MGMT_2_GYROS_ON_VAL	0x07 	// X,Y and Z GYROS ON

#define ICG20330_REG_SIGNAL_PATH_RESET		0x68
#define ICG20330_TEMP_SIGNAL_PATH_RESET		0x01 	// reset the temperature signal path

#define ICG20330_REG_GYRO_CONFIG			0x1B


#define ICG20330_MAX_NUM_CHANNELS   3
#define ICG20330_BYTES_PER_CHANNEL	2

#define ICG20330_MAX_NUM_BYTES		(ICG20330_BYTES_PER_CHANNEL * \
					 ICG20330_MAX_NUM_CHANNELS)


// Sensitivity scale factors
#define ICG20330_SENS_SCALE_FACTOR_31_25_DPS   1048
#define ICG20330_SENS_SCALE_FACTOR_62_5_DPS    524
#define ICG20330_SENS_SCALE_FACTOR_125_DPS     262
#define ICG20330_SENS_SCALE_FACTOR_250_DPS     131


enum icg20330_channel {
	ICG20330_CHANNEL_GYRO_X	= 0,
	ICG20330_CHANNEL_GYRO_Y,
	ICG20330_CHANNEL_GYRO_Z,
};

enum icg20330_range {
	ICG20330_RANGE_31_25_DPS = 0,
	ICG20330_RANGE_62_5_DPS,
	ICG20330_RANGE_125_DPS,
	ICG20330_RANGE_250_DPS,
};

struct icg20330_config {
	char *i2c_name;
	uint8_t i2c_address;
	uint8_t whoami;
	enum icg20330_range range;
	uint8_t dr;
};

struct icg20330_data {
	const struct device *i2c;
	struct k_sem sem;
	int16_t raw[ICG20330_MAX_NUM_CHANNELS];
};


