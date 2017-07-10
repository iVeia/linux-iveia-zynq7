/*
 * LSM303DLM accelerometer driver
 *
 * Copyright (C) 2013 iVeia, LLC
 * Author: Brian Silverman <bsilverman@iveia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/i2c/lsm303dlm_accel.h>

#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/input-polldev.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#define LSM303DLM_DEV_NAME	"lsm303dlm_accel"

/** Maximum polled-device-reported g value */
#define G_MAX			8000

#define DEFAULT_POLL_INTERVAL	100

/*
 * Registers
 */
#define CTRL_REG1		0x20
	#define PM_MASK			0xE0
		#define PM_POWER_DOWN		0x00
		#define PM_NORMAL		0x20
		#define PM_LOW_POWER_ODR_0_5	0x40
		#define PM_LOW_POWER_ODR_1	0x60
		#define PM_LOW_POWER_ODR_2	0x80
		#define PM_LOW_POWER_ODR_5	0xA0
		#define PM_LOW_POWER_ODR_10	0xC0
	#define DR_MASK			0x18
		#define DR_ODR_50HZ		0x00
		#define DR_ODR_100HZ		0x08
		#define DR_ODR_400HZ		0x10
		#define DR_ODR_1000HZ		0x18
	#define ZEN_FLAG		0x04
	#define YEN_FLAG		0x02
	#define XEN_FLAG		0x01
#define CTRL_REG2		0x21
#define CTRL_REG3		0x22
#define CTRL_REG4		0x23
#define CTRL_REG5		0x24
#define HP_FILTER_RESET		0x25
#define REFERENCE		0x26
#define STATUS_REG		0x27
#define OUT_X_L			0x28
#define OUT_X_H			0x29
#define OUT_Y_L			0x2a
#define OUT_Y_H			0x2b
#define OUT_Z_L			0x2c
#define OUT_Z_H			0x2d
#define INT1_CFG		0x30
#define INT1_SOURCE		0x31
#define INT1_THS		0x32
#define INT1_DURATION		0x33
#define INT2_CFG		0x34
#define INT2_SOURCE		0x35
#define INT2_THS		0x36
#define INT2_DURATION		0x37

#define FIRST_DATA_REG		OUT_X_L
#define AUTO_INCREMENT		0x80

#define FUZZ			0
#define FLAT			0

struct lsm303dlm_accel_data {
	atomic_t enabled;
	struct i2c_client *client;
	struct lsm303dlm_accel_platform_data *pdata;
	struct input_polled_dev *input_polled_dev;
};

static int lsm303dlm_accel_i2c_read(struct lsm303dlm_accel_data *acc,
				  u8 *buf, int len)
{
	int err;

	struct i2c_msg msgs[] = {
		{
		 .addr = acc->client->addr,
		 .flags = acc->client->flags,
		 .len = 1,
		 .buf = buf,
		 },
		{
		 .addr = acc->client->addr,
		 .flags = acc->client->flags | I2C_M_RD,
		 .len = len,
		 .buf = buf,
		 },
	};

	err = i2c_transfer(acc->client->adapter, msgs, 2);

	if (err != 2) {
		dev_err(&acc->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lsm303dlm_accel_i2c_write(struct lsm303dlm_accel_data *acc,
				   u8 *buf, int len)
{
	int err;

	struct i2c_msg msgs[] = {
		{
		 .addr = acc->client->addr,
		 .flags = acc->client->flags,
		 .len = len,
		 .buf = buf,
		 },
	};

	err = i2c_transfer(acc->client->adapter, msgs, 1);

	if (err != 1) {
		dev_err(&acc->client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lsm303dlm_accel_hw_init(struct lsm303dlm_accel_data *acc)
{
	int err = 0;
	u8 acc_data[2];

        acc_data[0] = CTRL_REG1;
        acc_data[1] = (PM_MASK & PM_NORMAL)
			| XEN_FLAG | YEN_FLAG | ZEN_FLAG;
        err = lsm303dlm_accel_i2c_write(acc, acc_data, 2);
	if (err < 0)
		return err;

	return 0;
}

static int lsm303dlm_accel_hw_shutdown(struct lsm303dlm_accel_data *acc)
{
	int err = 0;
	u8 acc_data[2];

        acc_data[0] = CTRL_REG1;
        acc_data[1] = (PM_MASK & PM_POWER_DOWN);
        err = lsm303dlm_accel_i2c_write(acc, acc_data, 2);
	if (err < 0)
		return err;

	return 0;
}

int lsm303dlm_accel_update_odr(struct lsm303dlm_accel_data *acc, int poll_interval)
{
	int err = 0;
	u8 acc_data[2];
	unsigned char odr;
	unsigned char ctrl_reg1;

	if (poll_interval >= 20) {
		odr = DR_ODR_50HZ;
	} else if (poll_interval >= 10) {
		odr = DR_ODR_100HZ;
	} else if (poll_interval >= 2) {
		odr = DR_ODR_400HZ;
	} else if (poll_interval >= 1) {
		odr = DR_ODR_1000HZ;
	} else {
		odr = DR_ODR_50HZ;
	}

        acc_data[0] = CTRL_REG1;
        err = lsm303dlm_accel_i2c_read(acc, acc_data, 1);
	if (err < 0)
		return err;
	ctrl_reg1 = acc_data[0] & ~ DR_MASK;

        acc_data[0] = CTRL_REG1;
        acc_data[1] = ctrl_reg1 | (DR_MASK & odr) ;
        err = lsm303dlm_accel_i2c_write(acc, acc_data, 2);
	if (err < 0)
		return err;

	return 0;
}

static int lsm303dlm_accel_enable(struct lsm303dlm_accel_data *acc)
{
	int err;

	if (!atomic_cmpxchg(&acc->enabled, 0, 1)) {
		int err;

		if (acc->pdata->power_on) {
			err = acc->pdata->power_on();
			if (err < 0)
				goto err_power_on;
		}

		err = lsm303dlm_accel_hw_init(acc);
		if (err < 0)
			goto err_lsm303dlm_accel_hw_init;
	}

	return 0;

err_lsm303dlm_accel_hw_init:
	lsm303dlm_accel_hw_shutdown(acc);
err_power_on:
	atomic_set(&acc->enabled, 0);
	return err;
}

static int lsm303dlm_accel_disable(struct lsm303dlm_accel_data *acc)
{
	int err = 0;
	if (atomic_cmpxchg(&acc->enabled, 1, 0)) {
		err = lsm303dlm_accel_hw_shutdown(acc);
		if (err < 0) {
			dev_err(&acc->input_polled_dev->input->dev, 
				"Could not shutdown device\n");
		}

		if (acc->pdata->power_off)
			acc->pdata->power_off();
	}

	return err;
}

static ssize_t attr_get_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct lsm303dlm_accel_data *acc = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", acc->pdata->poll_interval);
}

static ssize_t attr_set_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct lsm303dlm_accel_data *acc = dev_get_drvdata(dev);
	unsigned long interval_ms;

	if (strict_strtoul(buf, 10, &interval_ms))
		return -EINVAL;
	if (!interval_ms)
		return -EINVAL;
	acc->pdata->poll_interval = interval_ms;
	acc->input_polled_dev->poll_interval = interval_ms;
	lsm303dlm_accel_update_odr(acc, interval_ms);
	return size;
}

static ssize_t attr_get_enable(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct lsm303dlm_accel_data *acc = dev_get_drvdata(dev);
	int val = atomic_read(&acc->enabled);
	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_enable(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct lsm303dlm_accel_data *acc = dev_get_drvdata(dev);
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val)
		lsm303dlm_accel_enable(acc);
	else
		lsm303dlm_accel_disable(acc);

	return size;
}

static struct device_attribute attributes[] = {
	__ATTR(pollrate_ms, 0666, attr_get_polling_rate, attr_set_polling_rate),
	__ATTR(enable, 0666, attr_get_enable, attr_set_enable),
};

static int create_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(dev, attributes + i))
			goto error;
	return 0;

error:
	for ( ; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "%s:Unable to create interface\n", __func__);
	return -1;
}

static int remove_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
	return 0;
}


static void lsm303dlm_accel_input_poll(struct input_polled_dev *polled_dev)
{
	struct lsm303dlm_accel_data *acc = polled_dev->private;
	int err;
	u8 acc_data[6];
	signed short x, y, z;

        acc_data[0] = FIRST_DATA_REG | AUTO_INCREMENT;
        err = lsm303dlm_accel_i2c_read(acc, acc_data, 6);
	if (err < 0)
		return;

	x = ((short) ((acc_data[1] << 8) | acc_data[0])) >> 4;
	y = ((short) ((acc_data[3] << 8) | acc_data[2])) >> 4;
	z = ((short) ((acc_data[5] << 8) | acc_data[4])) >> 4;

	if (acc->pdata->negate_x)
		x = -x;
	if (acc->pdata->negate_y)
		y = -y;
	if (acc->pdata->negate_z)
		z = -z;

	if (acc->pdata->swap_xy) {
		signed short tmp = x;
		x = y;
		y = tmp;
	}

	input_report_abs(polled_dev->input, ABS_X, x);
	input_report_abs(polled_dev->input, ABS_Y, y);
	input_report_abs(polled_dev->input, ABS_Z, z);
	input_sync(polled_dev->input);
}

static int lsm303dlm_accel_input_register(struct lsm303dlm_accel_data *acc)
{
	int err;
	struct input_polled_dev *polled_dev;

	acc->input_polled_dev = input_allocate_polled_device();
	if (!acc->input_polled_dev) {
		err = -ENOMEM;
		dev_err(&acc->client->dev, "input device allocate failed\n");
		goto out;
	}

	polled_dev = acc->input_polled_dev;

	polled_dev->poll = lsm303dlm_accel_input_poll;
	polled_dev->poll_interval = 
		acc->pdata->poll_interval ? acc->pdata->poll_interval : DEFAULT_POLL_INTERVAL;
	polled_dev->private = acc;
	polled_dev->input->name = LSM303DLM_DEV_NAME;
	polled_dev->input->phys = LSM303DLM_DEV_NAME "/input";
	polled_dev->input->id.bustype = BUS_HOST;
	polled_dev->input->dev.parent = &acc->client->dev;

	set_bit(EV_ABS, polled_dev->input->evbit);

	input_set_abs_params(polled_dev->input, ABS_X, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(polled_dev->input, ABS_Y, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(polled_dev->input, ABS_Z, -G_MAX, G_MAX, FUZZ, FLAT);

	err = input_register_polled_device(acc->input_polled_dev);
	if (err) {
		dev_err(&acc->client->dev,
			"unable to register input device %s\n",
			polled_dev->input->name);
		goto out;
	}

	return 0;

out:
	if (acc->input_polled_dev)
		input_free_polled_device(acc->input_polled_dev);
	acc->input_polled_dev = NULL;
	return err;
}

static void lsm303dlm_accel_input_unregister(struct lsm303dlm_accel_data *acc)
{
	input_unregister_polled_device(acc->input_polled_dev);
	input_free_polled_device(acc->input_polled_dev);
}

static int lsm303dlm_accel_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct lsm303dlm_accel_data *acc = NULL;
	int err = -1;

	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		err = -ENODEV;
		goto err_platform_data;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		err = -ENODEV;
		goto err_i2c_check_functionality;
	}

	acc = kzalloc(sizeof(*acc), GFP_KERNEL);
	if (acc == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		err = -ENOMEM;
		goto err_alloc_dev;
	}
	atomic_set(&acc->enabled, 0);

	acc->client = client;

	acc->pdata = kmalloc(sizeof(*acc->pdata), GFP_KERNEL);
	if (acc->pdata == NULL)
		goto err_alloc_pdata;

	memcpy(acc->pdata, client->dev.platform_data, sizeof(*acc->pdata));

	i2c_set_clientdata(client, acc);

	if (acc->pdata->init) {
		err = acc->pdata->init();
		if (err < 0)
			goto err_pdata_init;
	}

	err = lsm303dlm_accel_enable(acc);
	if (err < 0)
		goto err_lsm303dlm_accel_enable;

	err = lsm303dlm_accel_update_odr(acc, acc->pdata->poll_interval);
	if (err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto err_lsm303dlm_accel_update_odr;
	}

	err = lsm303dlm_accel_input_register(acc);
	if (err < 0)
		goto err_lsm303dlm_accel_input_init;

	err = create_sysfs_interfaces(&client->dev);
	if (err < 0) {
		dev_err(&client->dev, "lsm_acc_device register failed\n");
		goto err_create_sysfs_interfaces;
	}

	return 0;

err_create_sysfs_interfaces:
	lsm303dlm_accel_input_unregister(acc);
err_lsm303dlm_accel_input_init:
err_lsm303dlm_accel_update_odr:
	lsm303dlm_accel_disable(acc);
err_lsm303dlm_accel_enable:
	if (acc->pdata->exit)
		acc->pdata->exit();
err_pdata_init:
	kfree(acc->pdata);
err_alloc_pdata:
	kfree(acc);
err_alloc_dev:
err_i2c_check_functionality:
err_platform_data:
	return err;
}

static int __devexit lsm303dlm_accel_remove(struct i2c_client *client)
{
	struct lsm303dlm_accel_data *acc = i2c_get_clientdata(client);
	remove_sysfs_interfaces(&client->dev);
	lsm303dlm_accel_input_unregister(acc);
	lsm303dlm_accel_disable(acc);
	if (acc->pdata->exit)
		acc->pdata->exit();
	kfree(acc->pdata);
	kfree(acc);

	return 0;
}

static const struct i2c_device_id lsm303dlm_accel_id[] = {
	{LSM303DLM_DEV_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, lsm303dlm_accel_id);

static struct i2c_driver lsm303dlm_accel_driver = {
	.driver = {
		.name = LSM303DLM_DEV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = lsm303dlm_accel_probe,
	.remove = __devexit_p(lsm303dlm_accel_remove),
	.id_table = lsm303dlm_accel_id,
};


static int __init lsm303dlm_accel_init(void)
{
	pr_debug("LSM303DLM driver for the accelerometer part\n");
	printk (KERN_INFO "LSM303DLM driver for the accelerometer part\n");
	return i2c_add_driver(&lsm303dlm_accel_driver);
}

static void __exit lsm303dlm_accel_exit(void)
{
	i2c_del_driver(&lsm303dlm_accel_driver);
	return;
}

module_init(lsm303dlm_accel_init);
module_exit(lsm303dlm_accel_exit);

MODULE_DESCRIPTION(LSM303DLM_DEV_NAME " accelerometer driver");
MODULE_AUTHOR("Brian Silverman <bsilverman@iveia.com>");
MODULE_LICENSE("GPL");

