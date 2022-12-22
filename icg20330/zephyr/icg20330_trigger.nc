/*
 * Copyright (c) 2017, NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/log.h>

#include "icg20330.h"

LOG_MODULE_DECLARE(ICG20330, CONFIG_SENSOR_LOG_LEVEL);

static void icg20330_gpio_callback(const struct device *dev,
				    struct gpio_callback *cb,
				    uint32_t pin_mask)
{
	struct icg20330_data *data =
		CONTAINER_OF(cb, struct icg20330_data, gpio_cb);

	if ((pin_mask & BIT(data->gpio_pin)) == 0U) {
		return;
	}

	gpio_pin_interrupt_configure(data->gpio, data->gpio_pin,
				     GPIO_INT_DISABLE);

#if defined(CONFIG_ICG20330_TRIGGER_OWN_THREAD)
	k_sem_give(&data->trig_sem);
#elif defined(CONFIG_ICG20330_TRIGGER_GLOBAL_THREAD)
	k_work_submit(&data->work);
#endif
}

static int icg20330_handle_drdy_int(const struct device *dev)
{
	struct icg20330_data *data = dev->data;

	struct sensor_trigger drdy_trig = {
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ALL,
	};

	if (data->drdy_handler) {
		data->drdy_handler(dev, &drdy_trig);
	}

	return 0;
}

static void icg20330_handle_int(const struct device *dev)
{
	const struct icg20330_config *config = dev->config;
	struct icg20330_data *data = dev->data;
	uint8_t int_source;

	k_sem_take(&data->sem, K_FOREVER);

	if (i2c_reg_read_byte(data->i2c, config->i2c_address,
			      ICG20330_REG_INT_SOURCE,
			      &int_source)) {
		LOG_ERR("Could not read interrupt source");
		int_source = 0U;
	}

	k_sem_give(&data->sem);

	if (int_source & ICG20330INT_SOURCE_DRDY_MASK) {
		icg20330_handle_drdy_int(dev);
	}

	gpio_pin_interrupt_configure(data->gpio, config->gpio_pin,
				     GPIO_INT_EDGE_TO_ACTIVE);
}

#ifdef CONFIG_ICG20330_TRIGGER_OWN_THREAD
static void icg20330_thread_main(struct icg20330_data *data)
{
	while (true) {
		k_sem_take(&data->trig_sem, K_FOREVER);
		icg20330_handle_int(data->dev);
	}
}
#endif

#ifdef CONFIG_ICG20330_TRIGGER_GLOBAL_THREAD
static void icg20330_work_handler(struct k_work *work)
{
	struct icg20330_data *data =
		CONTAINER_OF(work, struct icg20330_data, work);

	icg20330_handle_int(data->dev);
}
#endif

int icg20330_trigger_set(const struct device *dev,
			  const struct sensor_trigger *trig,
			  sensor_trigger_handler_t handler)
{
	const struct icg20330_config *config = dev->config;
	struct icg20330_data *data = dev->data;
	enum icg20330_power power = ICG20330_POWER_STANDBY;
	uint32_t transition_time;
	uint8_t mask;
	int ret = 0;

	k_sem_take(&data->sem, K_FOREVER);

	switch (trig->type) {
	case SENSOR_TRIG_DATA_READY:
		mask = ICG20330_CTRLREG2_CFG_EN_MASK;
		data->drdy_handler = handler;
		break;
	default:
		LOG_ERR("Unsupported sensor trigger");
		ret = -ENOTSUP;
		goto exit;
	}

	/* Configure the sensor interrupt */
	if (i2c_reg_update_byte(data->i2c, config->i2c_address,
				ICG20330_REG_CTRLREG2,
				mask,
				handler ? mask : 0)) {
		LOG_ERR("Could not configure interrupt");
		ret = -EIO;
		goto exit;
	}

exit:
	k_sem_give(&data->sem);

	return ret;
}

int icg20330_trigger_init(const struct device *dev)
{
	const struct icg20330_config *config = dev->config;
	struct icg20330_data *data = dev->data;
	uint8_t ctrl_reg2;
	int ret;

	data->dev = dev;

#if defined(CONFIG_ICG20330_TRIGGER_OWN_THREAD)
	k_sem_init(&data->trig_sem, 0, K_SEM_MAX_LIMIT);
	k_thread_create(&data->thread, data->thread_stack,
			CONFIG_ICG20330_THREAD_STACK_SIZE,
			(k_thread_entry_t)icg20330_thread_main, data, 0, NULL,
			K_PRIO_COOP(CONFIG_ICG20330_THREAD_PRIORITY),
			0, K_NO_WAIT);
#elif defined(CONFIG_ICG20330_TRIGGER_GLOBAL_THREAD)
	data->work.handler = icg20330_work_handler;
#endif

	/* Route the interrupts to INT1/INT2 pins */
	ctrl_reg2 = 0U;
#if CONFIG_ICG20330_DRDY_INT1
	ctrl_reg2 |= ICG20330_CTRLREG2_CFG_DRDY_MASK;
#endif

	if (i2c_reg_write_byte(data->i2c, config->i2c_address,
			       ICG20330_REG_CTRLREG2, ctrl_reg2)) {
		LOG_ERR("Could not configure interrupt pin routing");
		return -EIO;
	}

	/* Get the GPIO device */
	data->gpio = device_get_binding(config->gpio_name);
	if (data->gpio == NULL) {
		LOG_ERR("Could not find GPIO device");
		return -EINVAL;
	}

	data->gpio_pin = config->gpio_pin;

	ret = gpio_pin_configure(data->gpio, config->gpio_pin,
				 GPIO_INPUT | config->gpio_flags);
	if (ret < 0) {
		return ret;
	}

	gpio_init_callback(&data->gpio_cb, icg20330_gpio_callback,
			   BIT(config->gpio_pin));

	ret = gpio_add_callback(data->gpio, &data->gpio_cb);
	if (ret < 0) {
		return ret;
	}

	ret = gpio_pin_interrupt_configure(data->gpio, config->gpio_pin,
					   GPIO_INT_EDGE_TO_ACTIVE);
	if (ret < 0) {
		return ret;
	}

	return 0;
}
