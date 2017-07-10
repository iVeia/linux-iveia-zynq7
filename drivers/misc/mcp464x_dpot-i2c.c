/*
 * I2C Driver for Microchip MCP464x Digital Potentiometer
 *
 * Copyright (C) 2010 iVeia, LLC.
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


static struct i2c_client *_mcp464x_client;

int mcp464x_write_byte(u8 reg, u8 val)
{
    u8 tmp[2];

    tmp[0] = reg;
    tmp[1] = val;
    return i2c_master_send(_mcp464x_client, tmp, sizeof (tmp));
}
EXPORT_SYMBOL(mcp464x_write_byte);

int mcp464x_read_byte(u8 reg, u8 *data)
{
    u8 tmp;
    int ret;

	ret = i2c_master_recv(_mcp464x_client, &tmp, sizeof (tmp));
	if (ret >= 0) {
        memcpy(data, &tmp, 1);
        return 0;
    }
    return -EIO;
}
EXPORT_SYMBOL(mcp464x_read_byte);

static const struct i2c_device_id mcp464x_id[] = {
	{ "mcp4641t", 0, },  /* 'driver_data' is currently unused */
	{ }
};
MODULE_DEVICE_TABLE(i2c, mcp464x_id);

static int mcp464x_remove(struct i2c_client *client)
{
    kfree(_mcp464x_client);
    return 0;
}

static int __devinit mcp464x_probe(struct i2c_client *client,
                                   const struct i2c_device_id *id)
{
    _mcp464x_client = kzalloc(sizeof (*client), GFP_KERNEL);
    if (!_mcp464x_client)
        return -ENOMEM;
    memcpy(_mcp464x_client, client, sizeof (struct i2c_client));
    return 0;
}

static struct i2c_driver mcp464x_driver = {
	.driver = {
		.name	= "mcp464xx",
	},
	.probe		= mcp464x_probe,
	.remove		= mcp464x_remove,
	.id_table	= mcp464x_id,
};


static int __init mcp464x_init(void)
{
	return i2c_add_driver(&mcp464x_driver);
}
/* register after i2c postcore initcall and before
 * subsys initcalls that may rely on this
 */
subsys_initcall(mcp464x_init);

static void __exit mcp464x_exit(void)
{
	i2c_del_driver(&mcp464x_driver);
}
module_exit(mcp464x_exit);

MODULE_AUTHOR("iVeia, LLC.");
MODULE_DESCRIPTION("I2C Driver for Microchip MCP464x Digital Potentiometer");
MODULE_LICENSE("GPL");
