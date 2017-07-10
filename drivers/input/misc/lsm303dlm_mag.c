/*
 * LSM303DLM magnetometer driver
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

#include <linux/i2c/lsm303dlm_mag.h>

#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/input-polldev.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#define LSM303DLM_DEV_NAME	"lsm303dlm_mag"

#define MAX			2048

#define DEFAULT_POLL_INTERVAL	100
#define GAIN_SETTING	2

/*
 * Conversion factors, from datasheet:
 *	LSM303DLM, April 2011, Doc ID 018725 Rev 1
 */
signed long lsb_per_gauss_per_gain_setting_xyz[][3] = {
	{ 0, 0, 0 },		// XYZ settings for GN2-0 = 0 (INVALID SETTING)
	{ 1100, 1100, 980 },	// XYZ settings for GN2-0 = 1
	{ 855, 855, 760 },	// XYZ settings for GN2-0 = 2
	{ 670, 670, 600 },	// XYZ settings for GN2-0 = 3
	{ 450, 450, 400 },	// XYZ settings for GN2-0 = 4
	{ 400, 400, 355 },	// XYZ settings for GN2-0 = 5
	{ 330, 330, 295 },	// XYZ settings for GN2-0 = 6
	{ 230, 230, 205 },	// XYZ settings for GN2-0 = 7
};

#define HZ_TO_MS(hz) ((unsigned long) (1000.0/hz))

/*
 * Registers
 */
#define CRA_REG		0x00
	#define DR_MASK			0x1C
		#define DR_ODR_0_75HZ		0x00
		#define DR_ODR_1_5HZ		0x04
		#define DR_ODR_3HZ		0x08
		#define DR_ODR_7_5HZ		0x0C
		#define DR_ODR_15HZ		0x10
		#define DR_ODR_30HZ		0x14
		#define DR_ODR_75HZ		0x18
		#define DR_ODR_220HZ		0x1C
#define CRB_REG		0x01
	#define GN_SHIFT		5
#define MR_REG		0x02
	#define MD_MASK		0x03
		#define MD_CONTINUOUS_CONV_MODE	0x00
		#define MD_SINGLE_CONV_MODE	0x01
		#define MD_SLEEP_MODE		0x02
#define OUT_X_H		0x03
#define OUT_X_L		0x04
#define OUT_Y_H		0x07
#define OUT_Y_L		0x08
#define OUT_Z_H		0x05
#define OUT_Z_L		0x06
#define SR_REG		0x09
#define IRA_REG		0x0A
#define IRB_REG		0x0B
#define IRC_REG		0x0C
#define WHO_AM_I	0x0F

#define FIRST_DATA_REG		OUT_X_H
#define AUTO_INCREMENT		0x80

#define FUZZ			0
#define FLAT			0

struct lsm303dlm_mag_data {
	atomic_t enabled;
	struct i2c_client *client;
	struct lsm303dlm_mag_platform_data *pdata;
	struct input_polled_dev *input_polled_dev;
};

static int lsm303dlm_mag_i2c_read(struct lsm303dlm_mag_data *mag,
				  u8 *buf, int len)
{
	int err;

	struct i2c_msg msgs[] = {
		{
		 .addr = mag->client->addr,
		 .flags = mag->client->flags,
		 .len = 1,
		 .buf = buf,
		 },
		{
		 .addr = mag->client->addr,
		 .flags = mag->client->flags | I2C_M_RD,
		 .len = len,
		 .buf = buf,
		 },
	};

	err = i2c_transfer(mag->client->adapter, msgs, 2);

	if (err != 2) {
		dev_err(&mag->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lsm303dlm_mag_i2c_write(struct lsm303dlm_mag_data *mag,
				   u8 *buf, int len)
{
	int err;

	struct i2c_msg msgs[] = {
		{
		 .addr = mag->client->addr,
		 .flags = mag->client->flags,
		 .len = len,
		 .buf = buf,
		 },
	};

	err = i2c_transfer(mag->client->adapter, msgs, 1);

	if (err != 1) {
		dev_err(&mag->client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lsm303dlm_mag_hw_init(struct lsm303dlm_mag_data *mag)
{
	int err = 0;
	u8 mag_data[2];

        mag_data[0] = CRB_REG;
        mag_data[1] = GAIN_SETTING < GN_SHIFT;
        err = lsm303dlm_mag_i2c_write(mag, mag_data, 2);
	if (err < 0)
		return err;

        mag_data[0] = MR_REG;
        mag_data[1] = MD_CONTINUOUS_CONV_MODE & MD_MASK;
        err = lsm303dlm_mag_i2c_write(mag, mag_data, 2);
	if (err < 0)
		return err;

	return 0;
}

static int lsm303dlm_mag_hw_shutdown(struct lsm303dlm_mag_data *mag)
{
	int err = 0;
	u8 mag_data[2];

        mag_data[0] = MR_REG;
        mag_data[1] = MD_SLEEP_MODE & MD_MASK;
        err = lsm303dlm_mag_i2c_write(mag, mag_data, 2);
	if (err < 0)
		return err;

	return 0;
}

static int lsm303dlm_mag_update_odr(struct lsm303dlm_mag_data *mag, int poll_interval)
{
	int err = 0;
	u8 mag_data[2];
	unsigned char odr;
	unsigned char reg;

	if (poll_interval >= HZ_TO_MS(0.75)) {
		odr = DR_ODR_0_75HZ;
	} else if (poll_interval >= HZ_TO_MS(1.5)) {
		odr = DR_ODR_1_5HZ;
	} else if (poll_interval >= HZ_TO_MS(3.0)) {
		odr = DR_ODR_3HZ;
	} else if (poll_interval >= HZ_TO_MS(7.5)) {
		odr = DR_ODR_7_5HZ;
	} else if (poll_interval >= HZ_TO_MS(15.0)) {
		odr = DR_ODR_15HZ;
	} else if (poll_interval >= HZ_TO_MS(30.0)) {
		odr = DR_ODR_30HZ;
	} else if (poll_interval >= HZ_TO_MS(75.0)) {
		odr = DR_ODR_75HZ;
	} else if (poll_interval >= HZ_TO_MS(220.0)) {
		odr = DR_ODR_220HZ;
	} else {
		odr = DR_ODR_0_75HZ;
	}

        mag_data[0] = CRA_REG;
        err = lsm303dlm_mag_i2c_read(mag, mag_data, 1);
	if (err < 0)
		return err;
	reg = mag_data[0] & ~ DR_MASK;

        mag_data[0] = CRA_REG;
        mag_data[1] = reg | (DR_MASK & odr) ;
        err = lsm303dlm_mag_i2c_write(mag, mag_data, 2);
	if (err < 0)
		return err;

	return 0;
}

static int lsm303dlm_mag_enable(struct lsm303dlm_mag_data *mag)
{
	int err;

	if (!atomic_cmpxchg(&mag->enabled, 0, 1)) {
		int err;

		if (mag->pdata->power_on) {
			err = mag->pdata->power_on();
			if (err < 0)
				goto err_power_on;
		}

		err = lsm303dlm_mag_hw_init(mag);
		if (err < 0)
			goto err_lsm303dlm_mag_hw_init;
	}

	return 0;

err_lsm303dlm_mag_hw_init:
	lsm303dlm_mag_hw_shutdown(mag);
err_power_on:
	atomic_set(&mag->enabled, 0);
	return err;
}

static int lsm303dlm_mag_disable(struct lsm303dlm_mag_data *mag)
{
	int err = 0;
	if (atomic_cmpxchg(&mag->enabled, 1, 0)) {
		err = lsm303dlm_mag_hw_shutdown(mag);
		if (err < 0) {
			dev_err(&mag->input_polled_dev->input->dev, 
				"Could not shutdown device\n");
		}

		if (mag->pdata->power_off)
			mag->pdata->power_off();
	}

	return err;
}

static ssize_t attr_get_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct lsm303dlm_mag_data *mag = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", mag->pdata->poll_interval);
}

static ssize_t attr_set_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct lsm303dlm_mag_data *mag = dev_get_drvdata(dev);
	unsigned long interval_ms;

	if (strict_strtoul(buf, 10, &interval_ms))
		return -EINVAL;
	if (!interval_ms)
		return -EINVAL;
	mag->pdata->poll_interval = interval_ms;
	mag->input_polled_dev->poll_interval = interval_ms;
	lsm303dlm_mag_update_odr(mag, interval_ms);
	return size;
}

static ssize_t attr_get_enable(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct lsm303dlm_mag_data *mag = dev_get_drvdata(dev);
	int val = atomic_read(&mag->enabled);
	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_enable(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct lsm303dlm_mag_data *mag = dev_get_drvdata(dev);
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val)
		lsm303dlm_mag_enable(mag);
	else
		lsm303dlm_mag_disable(mag);

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


static void lsm303dlm_mag_input_poll(struct input_polled_dev *polled_dev)
{
	struct lsm303dlm_mag_data *mag = polled_dev->private;
	int err;
	u8 mag_data[6];
	signed long xlsb, ylsb, zlsb;
	signed long x, y, z;

        mag_data[0] = FIRST_DATA_REG | AUTO_INCREMENT;
        err = lsm303dlm_mag_i2c_read(mag, mag_data, 6);
	if (err < 0)
		return;

	xlsb = ((short) ((mag_data[0] << 8) | mag_data[1]));
	ylsb = ((short) ((mag_data[4] << 8) | mag_data[5]));
	zlsb = ((short) ((mag_data[2] << 8) | mag_data[3]));

	/* 
	 * Convert LSBs to nanoteslas.  1 gauss = 100000 nanoteslas.
	 */
	x = (xlsb * 100000) / lsb_per_gauss_per_gain_setting_xyz[GAIN_SETTING][0];
	y = (ylsb * 100000) / lsb_per_gauss_per_gain_setting_xyz[GAIN_SETTING][1];
	z = (zlsb * 100000) / lsb_per_gauss_per_gain_setting_xyz[GAIN_SETTING][2];

	if (mag->pdata->negate_x)
		x = -x;
	if (mag->pdata->negate_y)
		y = -y;
	if (mag->pdata->negate_z)
		z = -z;

	if (mag->pdata->swap_xy) {
		signed short tmp = x;
		x = y;
		y = tmp;
	}

	input_report_abs(polled_dev->input, ABS_X, x);
	input_report_abs(polled_dev->input, ABS_Y, y);
	input_report_abs(polled_dev->input, ABS_Z, z);
	input_sync(polled_dev->input);
}

static int lsm303dlm_mag_input_register(struct lsm303dlm_mag_data *mag)
{
	int err;
	struct input_polled_dev *polled_dev;

	mag->input_polled_dev = input_allocate_polled_device();
	if (!mag->input_polled_dev) {
		err = -ENOMEM;
		dev_err(&mag->client->dev, "input device allocate failed\n");
		goto out;
	}

	polled_dev = mag->input_polled_dev;

	polled_dev->poll = lsm303dlm_mag_input_poll;
	polled_dev->poll_interval = 
		mag->pdata->poll_interval ? mag->pdata->poll_interval : DEFAULT_POLL_INTERVAL;
	polled_dev->private = mag;
	polled_dev->input->name = LSM303DLM_DEV_NAME;
	polled_dev->input->phys = LSM303DLM_DEV_NAME "/input";
	polled_dev->input->id.bustype = BUS_HOST;
	polled_dev->input->dev.parent = &mag->client->dev;

	set_bit(EV_ABS, polled_dev->input->evbit);

	input_set_abs_params(polled_dev->input, ABS_X, -MAX, MAX, FUZZ, FLAT);
	input_set_abs_params(polled_dev->input, ABS_Y, -MAX, MAX, FUZZ, FLAT);
	input_set_abs_params(polled_dev->input, ABS_Z, -MAX, MAX, FUZZ, FLAT);

	err = input_register_polled_device(mag->input_polled_dev);
	if (err) {
		dev_err(&mag->client->dev,
			"unable to register input device %s\n",
			polled_dev->input->name);
		goto out;
	}

	return 0;

out:
	if (mag->input_polled_dev)
		input_free_polled_device(mag->input_polled_dev);
	mag->input_polled_dev = NULL;
	return err;
}

static void lsm303dlm_mag_input_unregister(struct lsm303dlm_mag_data *mag)
{
	input_unregister_polled_device(mag->input_polled_dev);
	input_free_polled_device(mag->input_polled_dev);
}

static int lsm303dlm_mag_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct lsm303dlm_mag_data *mag = NULL;
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

	mag = kzalloc(sizeof(*mag), GFP_KERNEL);
	if (mag == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		err = -ENOMEM;
		goto err_alloc_dev;
	}
	atomic_set(&mag->enabled, 0);

	mag->client = client;

	mag->pdata = kmalloc(sizeof(*mag->pdata), GFP_KERNEL);
	if (mag->pdata == NULL)
		goto err_alloc_pdata;

	memcpy(mag->pdata, client->dev.platform_data, sizeof(*mag->pdata));

	i2c_set_clientdata(client, mag);

	if (mag->pdata->init) {
		err = mag->pdata->init();
		if (err < 0)
			goto err_pdata_init;
	}

	err = lsm303dlm_mag_enable(mag);
	if (err < 0)
		goto err_lsm303dlm_mag_enable;

	err = lsm303dlm_mag_update_odr(mag, mag->pdata->poll_interval);
	if (err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto err_lsm303dlm_mag_update_odr;
	}

	err = lsm303dlm_mag_input_register(mag);
	if (err < 0)
		goto err_lsm303dlm_mag_input_init;

	err = create_sysfs_interfaces(&client->dev);
	if (err < 0) {
		dev_err(&client->dev, "lsm_mag_device register failed\n");
		goto err_create_sysfs_interfaces;
	}

	return 0;

err_create_sysfs_interfaces:
	lsm303dlm_mag_input_unregister(mag);
err_lsm303dlm_mag_input_init:
err_lsm303dlm_mag_update_odr:
	lsm303dlm_mag_disable(mag);
err_lsm303dlm_mag_enable:
	if (mag->pdata->exit)
		mag->pdata->exit();
err_pdata_init:
	kfree(mag->pdata);
err_alloc_pdata:
	kfree(mag);
err_alloc_dev:
err_i2c_check_functionality:
err_platform_data:
	return err;
}

static int __devexit lsm303dlm_mag_remove(struct i2c_client *client)
{
	struct lsm303dlm_mag_data *mag = i2c_get_clientdata(client);
	remove_sysfs_interfaces(&client->dev);
	lsm303dlm_mag_input_unregister(mag);
	lsm303dlm_mag_disable(mag);
	if (mag->pdata->exit)
		mag->pdata->exit();
	kfree(mag->pdata);
	kfree(mag);

	return 0;
}

static const struct i2c_device_id lsm303dlm_mag_id[] = {
	{LSM303DLM_DEV_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, lsm303dlm_mag_id);

static struct i2c_driver lsm303dlm_mag_driver = {
	.driver = {
		.name = LSM303DLM_DEV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = lsm303dlm_mag_probe,
	.remove = __devexit_p(lsm303dlm_mag_remove),
	.id_table = lsm303dlm_mag_id,
};


static int __init lsm303dlm_mag_init(void)
{
	pr_debug("LSM303DLM driver for the magnetometer part\n");
	printk (KERN_INFO "LSM303DLM driver for the magnetometer part\n");
	return i2c_add_driver(&lsm303dlm_mag_driver);
}

static void __exit lsm303dlm_mag_exit(void)
{
	i2c_del_driver(&lsm303dlm_mag_driver);
	return;
}

module_init(lsm303dlm_mag_init);
module_exit(lsm303dlm_mag_exit);

MODULE_DESCRIPTION(LSM303DLM_DEV_NAME " magnetometer driver");
MODULE_AUTHOR("Brian Silverman <bsilverman@iveia.com>");
MODULE_LICENSE("GPL");

