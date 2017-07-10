/*
 * I2C Driver for Linear Tech LTC2635 DAC
 *
 * Copyright (C) 2012 iVeia, LLC.
 * Author: Dan Weekley <dweekley@iveia.com>
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

/*
 * Operation notes:
 *  1. Three-byte I2C message required: one-byte register address
 *     followed by two data bytes.
 *  2. Receive-only (slave) interface; LTC2635 won't acknowledge (NAK)
 *     a read request.
 */

static struct i2c_client *_ltc2635_client;

int ltc2635_write_word(u8 reg, u16 val)
{
    u8 tmp[3];
    int rc;

    tmp[0] = reg;
    tmp[1] = (val & 0xFF00) >> 8;
    tmp[2] = val & 0x00FF;
    rc = i2c_master_send(_ltc2635_client, tmp, sizeof (tmp));
    return rc;
}
EXPORT_SYMBOL(ltc2635_write_word);

static const struct i2c_device_id ltc2635_id[] = {
	{ "ltc2635-lz8", 0, },  /* 'driver_data' is currently unused */
	{ }
};
MODULE_DEVICE_TABLE(i2c, ltc2635_id);

static int ltc2635_remove(struct i2c_client *client)
{
    kfree(_ltc2635_client);
    return 0;
}

static int __devinit ltc2635_probe(struct i2c_client *client,
                                   const struct i2c_device_id *id)
{
    _ltc2635_client = kzalloc(sizeof (*client), GFP_KERNEL);
    if (!_ltc2635_client)
        return -ENOMEM;
    memcpy(_ltc2635_client, client, sizeof (struct i2c_client));
    return 0;
}

static struct i2c_driver ltc2635_driver = {
	.driver = {
		.name	= "ltc2635",
	},
	.probe		= ltc2635_probe,
	.remove		= ltc2635_remove,
	.id_table	= ltc2635_id,
};


static int __init ltc2635_init(void)
{
	return i2c_add_driver(&ltc2635_driver);
}
/* register after i2c postcore initcall and before
 * subsys initcalls that may rely on this
 */
subsys_initcall(ltc2635_init);

static void __exit ltc2635_exit(void)
{
	i2c_del_driver(&ltc2635_driver);
}
module_exit(ltc2635_exit);

MODULE_AUTHOR("iVeia, LLC.");
MODULE_DESCRIPTION("I2C Driver for Linear Technology LTC2635 DAC");
MODULE_LICENSE("GPL");
