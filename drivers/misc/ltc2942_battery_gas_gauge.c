/*
 * I2C Driver for Linear Tech LTC2942 Battery Gas Gauge
 *
 * Copyright (C) 2012 iVeia, LLC.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/ltc2942_battery.h>

/*
 * See LTC2942 "Battery Gas Gauge with Temperature, Voltage Measurement"
 * datasheet for additional information.
 *
 * ACR = Accumlated Charge Register
 */

#define REG_STATUS		0x00
#define REG_CONTROL		0x01
	#define REG_CONTROL_AUTO_MODE		0xC0
	#define REG_CONTROL_MANUAL_VOLT_MODE	0x80
	#define REG_CONTROL_MANUAL_TEMP_MODE	0x40
	#define REG_CONTROL_SLEEP_MODE		0x00
	#define REG_CONTROL_M_POWER_SHIFT	3
	#define REG_CONTROL_AL_MODE		0x04
	#define REG_CONTROL_CC_MODE		0x02
	#define REG_CONTROL_AL_CC_DISABLED	0x00
	#define REG_CONTROL_SHUTDOWN		0x01
#define REG_ACCUM_CHG_MSB	0x02
#define REG_ACCUM_CHG_LSB	0x03
#define REG_CHG_THRESH_HI_MSB	0x04
#define REG_CHG_THRESH_HI_LSB	0x05
#define REG_CHG_THRESH_LO_MSB	0x06
#define REG_CHG_THRESH_LO_LSB	0x07
#define REG_VOLT_MSB		0x08
#define REG_VOLT_LSB		0x09
#define REG_VOLT_THRESH_HI	0x0A
#define REG_VOLT_THRESH_LO	0x0B
#define REG_TEMP_MSB		0x0C
#define REG_TEMP_LSB		0x0D
#define REG_TEMP_THRESH_HI	0x0E
#define REG_TEMP_THRESH_LO	0x0F

/* 16-bit reg shortcuts */
#define REG_ACCUM_CHG		REG_ACCUM_CHG_MSB
#define REG_CHG_THRESH_HI	REG_CHG_THRESH_HI_MSB
#define REG_CHG_THRESH_LO	REG_CHG_THRESH_LO_MSB
#define REG_VOLT		REG_VOLT_MSB
#define REG_TEMP		REG_TEMP_MSB

#define R_SENSE_MOHMS 10

/*
 * Choose M_POWER (from datasheet, where M = 2^M_POWER).
 *
 * M is the coulumb counter gain.  The choice depends on the range of batteries
 * mAh we plan to use, and its chosen to maximize the use of the 16-bit ACR
 * register.  Our current max battery is 3900mAh.
 *
 * M >= 128 * (Qbat / (65536 * 0.085mAh) * (Rsense / 50mohms )
 *    = 128 * (Qbat / (65536 * 0.085mAh) * (10mohms / 50mohms )
 *    = 128 * (3900mAh / (65536 * 0.085mAh) * (10mohms / 50mohms )
 *    ~ 17.9
 *
 * So, M = 32 would be fine, even if the max battery is almost twice as big
 * (~7000mAh).
 *
 * And, M = 2^M_POWER
 *
 * Also, determine qlsb from datasheet - careful on ordering so not to
 * overflow or round.
 *	QLSB = 0.085mAh * (50mohms / Rsense) * (M / 128)
 */
#define M_POWER		5
#define NANO_QLSB	((85000 * 50 * (1<<M_POWER)) / (R_SENSE_MOHMS * 128))

#define LTC2942_POLL_PERIOD		HZ

struct ltc2942_chip {
	struct i2c_client		*client;
	struct delayed_work		work;
	struct power_supply		battery;
	struct power_supply		usb;
	struct power_supply		ac;
	struct ltc2942_platform_data	*pdata;

	int ac_online;

	int usb_online;

	int status;
	int voltage_now;
	int capacity;
	int health;
	int present;
	int temp;
	int technology;
};

static int ltc2942_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct ltc2942_chip *chip = container_of(psy,
				struct ltc2942_chip, battery);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->status;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chip->voltage_now;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = chip->capacity;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chip->health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->present;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = chip->temp;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = chip->technology;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int ltc2942_ac_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct ltc2942_chip *chip = container_of(psy,
				struct ltc2942_chip, ac);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->ac_online;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int ltc2942_usb_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct ltc2942_chip *chip = container_of(psy,
				struct ltc2942_chip, usb);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->usb_online;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int ltc2942_write_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int ltc2942_read_reg(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

int ltc2942_read_reg16(struct i2c_client *client, int reg)
{
	u8 msb;
	u8 lsb;

	msb = ltc2942_read_reg(client, reg);
	if (msb < 0)
		return msb;
	lsb = ltc2942_read_reg(client, reg + 1);
	if (lsb < 0)
		return lsb;

	return (msb << 8) | lsb;
}

static void ltc2942_work(struct work_struct *work)
{
	struct ltc2942_chip *chip;
	int raw_charge;
	int raw_voltage;
	int raw_temp;
	struct ltc2942_platform_data * p;
	unsigned long battery_capacity_mah;
	unsigned long battery_capacity_lsbs;
	unsigned long zero_percent_mark;

	chip = container_of(work, struct ltc2942_chip, work.work);
	p = chip->pdata;

	raw_charge = ltc2942_read_reg16(chip->client, REG_ACCUM_CHG);
	raw_temp = ltc2942_read_reg16(chip->client, REG_TEMP);
	raw_voltage = ltc2942_read_reg16(chip->client, REG_VOLT);

	/* Convert raw to tenths K then tenths deg C */
	if (raw_temp >= 0)
		chip->temp = ((6000 * raw_temp) / 65535) - 2730;
	else
		chip->temp = -1;

	/* Convert raw to milliV then microV */
	if (raw_voltage >= 0)
		chip->voltage_now =  ((6 * 1000 * raw_voltage) / 65535) * 1000;
	else
		chip->voltage_now = -1;

	/* 
	 * Convert raw milliAH to percentage 
	 *
	 * We assume that the max ACR reading (0xFFFF) means 100%.  This should
	 * be true as long as the user allows the battery to fully charge to
	 * 100% at least once.
	 *
	 * Careful not to overflow!  Battery capacity in nanoAh may overflow 
	 * 32-bits.
	 */
	battery_capacity_mah = 3900;
	battery_capacity_lsbs = ((battery_capacity_mah * 100000) / NANO_QLSB) * 10;
	zero_percent_mark = 0xFFFF - battery_capacity_lsbs;
	if (raw_charge >= 0)
		if (raw_charge < zero_percent_mark)
			chip->capacity = 0;
		else
			chip->capacity = 
				(100 * (raw_charge - zero_percent_mark)) / battery_capacity_lsbs;
	else
		chip->capacity = -1;

	chip->ac_online = p->ac_online ? p->ac_online() : 0;
	chip->usb_online = p->usb_online ? p->usb_online() : 0;
	chip->status = p->battery_status ? 
		p->battery_status() : POWER_SUPPLY_STATUS_UNKNOWN;
	chip->present = p->battery_present ? p->battery_present() : 0;
	chip->health = p->battery_health ? 
		p->battery_health() : POWER_SUPPLY_HEALTH_UNKNOWN;
	chip->technology = p->battery_technology ? 
		p->battery_technology() : POWER_SUPPLY_TECHNOLOGY_UNKNOWN;

	power_supply_changed(&chip->ac);
	power_supply_changed(&chip->usb);
	power_supply_changed(&chip->battery);

	schedule_delayed_work(&chip->work, LTC2942_POLL_PERIOD);
}

static enum power_supply_property ltc2942_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

static enum power_supply_property ltc2942_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property ltc2942_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int __devinit ltc2942_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct ltc2942_chip *chip;
	int cur_val, new_val;
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->pdata = client->dev.platform_data;

	i2c_set_clientdata(client, chip);

	chip->battery.name		= "battery";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= ltc2942_battery_get_property;
	chip->battery.properties	= ltc2942_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(ltc2942_battery_props);

	chip->ac.name			= "ac";
	chip->ac.type			= POWER_SUPPLY_TYPE_MAINS;
	chip->ac.get_property		= ltc2942_ac_get_property;
	chip->ac.properties		= ltc2942_ac_props;
	chip->ac.num_properties		= ARRAY_SIZE(ltc2942_ac_props);

	chip->usb.name			= "usb";
	chip->usb.type			= POWER_SUPPLY_TYPE_USB;
	chip->usb.get_property		= ltc2942_usb_get_property;
	chip->usb.properties		= ltc2942_usb_props;
	chip->usb.num_properties	= ARRAY_SIZE(ltc2942_usb_props);

	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: battery power supply register\n");
		goto fail;
	}
	ret = power_supply_register(&client->dev, &chip->ac);
	if (ret) {
		dev_err(&client->dev, "failed: AC power supply register\n");
		goto fail;
	}
	ret = power_supply_register(&client->dev, &chip->usb);
	if (ret) {
		dev_err(&client->dev, "failed: USB power supply register\n");
		goto fail;
	}

	cur_val = ltc2942_read_reg(chip->client, REG_CONTROL);
	new_val = REG_CONTROL_AUTO_MODE 
		| (M_POWER << REG_CONTROL_M_POWER_SHIFT)
		| REG_CONTROL_AL_CC_DISABLED;
	if (cur_val != new_val) {
		/* 
		 * Part has not been configured.  This is first boot since
		 * battery was connected.  Shutdown analog section (required)
		 * to change ACR to max, then configure device to auto mode.
		 */
		ltc2942_write_reg(chip->client, REG_CONTROL, 0x01);
		ltc2942_write_reg(chip->client, REG_ACCUM_CHG_MSB, 0xFF);
		ltc2942_write_reg(chip->client, REG_ACCUM_CHG_LSB, 0xFF);
		ltc2942_write_reg(chip->client, REG_CONTROL, new_val);
	}

	INIT_DELAYED_WORK_DEFERRABLE(&chip->work, ltc2942_work);
	schedule_delayed_work(&chip->work, LTC2942_POLL_PERIOD);

	return 0;

fail:
	power_supply_unregister(&chip->usb);
	power_supply_unregister(&chip->ac);
	power_supply_unregister(&chip->battery);
	kfree(chip);
	return ret ? ret : -EINVAL;
}

MODULE_DEVICE_TABLE(i2c, ltc2942_id);

static int ltc2942_remove(struct i2c_client *client)
{
	struct ltc2942_chip *chip = i2c_get_clientdata(client);

	power_supply_unregister(&chip->usb);
	power_supply_unregister(&chip->ac);
	power_supply_unregister(&chip->battery);
	cancel_delayed_work(&chip->work);
	kfree(chip);
	return 0;
}

static const struct i2c_device_id ltc2942_id[] = {
	{ "ltc2942", 0 },
	{ }
};

static struct i2c_driver ltc2942_i2c_driver = {
	.driver = {
		.name	= "ltc2942",
	},
	.probe		= ltc2942_probe,
	.remove		= ltc2942_remove,
	.id_table	= ltc2942_id,
};

static int __init ltc2942_init(void)
{
	return i2c_add_driver(&ltc2942_i2c_driver);
}
module_init(ltc2942_init);

static void __exit ltc2942_exit(void)
{
	i2c_del_driver(&ltc2942_i2c_driver);
}
module_exit(ltc2942_exit);

MODULE_AUTHOR("iVeia, LLC.");
MODULE_DESCRIPTION("I2C Driver for Linear Technology LTC2942 Battery Gas Gauge");
MODULE_LICENSE("GPL");
