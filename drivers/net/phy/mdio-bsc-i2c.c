/*
 *  BSC over I2C for Broadcom PHYs
 *
 *  Copyright (C) 2005 Ben Gardner <bgardner@wabtec.com>
 *  Copyright (C) 2007 Marvell International Ltd.
 *
 *  Derived from drivers/i2c/chips/pca9539.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#ifdef CONFIG_OF_GPIO
#include <linux/of_platform.h>
#include <linux/of_mdio.h>
#endif


struct mdio_bsc_i2c_data {
    int irqs[PHY_MAX_ADDR];
	struct i2c_client *client;
    struct mii_bus *mdio_bus;
    struct mutex i2c_lock;
};

static const struct i2c_device_id mdio_bsc_i2c_id[] = {
	{ "dummy", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mdio_bsc_i2c_id);

static int mdio_bsc_i2c_read(struct mii_bus *bus, int phy, int reg)
{
    int iRetVal;
    struct mdio_bsc_i2c_data *pdata = bus->priv;
    struct i2c_client *client = pdata->client;

    u8 uReg     = reg;
    u8 buf[2];

	struct i2c_msg msgs[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= &uReg,
		},
		{
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= 2,
			.buf	= buf,
		},
	};

	if (i2c_transfer(client->adapter, msgs, 2) < 0) {
		dev_err(&client->dev, "read error\n");
		return -EIO;
	}

    iRetVal = 0;
    iRetVal = (buf[0] << 8) & 0x0000ff00;
    iRetVal |= (buf[1] & 0x000000ff);

    return iRetVal;
}

static int mdio_bsc_i2c_write(struct mii_bus *bus, int phy, int reg, u16 val)
{
    struct mdio_bsc_i2c_data *pdata = bus->priv;
    struct i2c_client *client = pdata->client;
    u8 buf[3];
	struct i2c_msg msgs[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 3,
			.buf	= buf,
		},
	};

    buf[0] = reg;
    buf[1] = (val >> 8);
    buf[2] = (val & 0x000000ff);

	if (i2c_transfer(client->adapter, msgs, 1) < 0) {
		dev_err(&client->dev, "write error\n");
		return -EIO;
	}

    return 0;
}

static int mdio_bsc_i2c_reset(struct mii_bus *bus)
{
	return 0;
}

static int mdio_bsc_i2c_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
    struct mdio_bsc_i2c_data *pdata;
    struct mii_bus *new_bus;
    int i,ret;

//I2C Setup
    pdata = devm_kzalloc(&client->dev,sizeof(struct mdio_bsc_i2c_data), GFP_KERNEL);
	if (pdata == NULL)
		return -ENOMEM;

    pdata->client = client;
    mutex_init(&pdata->i2c_lock);

	i2c_set_clientdata(client, pdata);

//MDIO Setup
    new_bus=mdiobus_alloc();
    if (!new_bus)
        return -ENODEV;

    new_bus->read = mdio_bsc_i2c_read;
    new_bus->write = mdio_bsc_i2c_write;
    new_bus->reset = mdio_bsc_i2c_reset;

    new_bus->priv = pdata;

    new_bus->name = "iVeia BSC I2C MDIO";
    new_bus->phy_mask = 0;
    new_bus->parent = &client->dev;

    new_bus->irq = pdata->irqs;

	for (i = 0; i < PHY_MAX_ADDR; i++)
		if (!new_bus->irq[i])
			new_bus->irq[i] = PHY_POLL;

    snprintf(new_bus->id, MII_BUS_ID_SIZE, "bsc-i2c-%x", client->dev.id);//Not sure if this is ok

	if (client->dev.of_node){
		ret = of_mdiobus_register(new_bus, client->dev.of_node);//Takes this path
	}else{
		ret = mdiobus_register(new_bus);
    }

    pdata->mdio_bus = new_bus;

	return ret;
}

static int mdio_bsc_i2c_remove(struct i2c_client *client)
{
    struct mdio_bsc_i2c_data *clientdata = i2c_get_clientdata(client);

    kfree(clientdata);

	return 0;
}

static const struct of_device_id mdio_bsc_i2c_dt_ids[] = {
	{ .compatible = "iveia,mdio-bsc-i2c", },
	{ }
};

MODULE_DEVICE_TABLE(of, mdio_bsc_i2c_dt_ids);

static struct i2c_driver mdio_bsc_i2c_driver = {
	.driver = {
		.name	= "iVeia MDC-I2C Driver",
		.of_match_table = mdio_bsc_i2c_dt_ids,
	},
	.probe		= mdio_bsc_i2c_probe,
	.remove		= mdio_bsc_i2c_remove,
    .id_table	= mdio_bsc_i2c_id,
};

static int __init mdio_bsc_i2c_init(void)
{
	return i2c_add_driver(&mdio_bsc_i2c_driver);
}
/* register after i2c postcore initcall and before
 * subsys initcalls that may rely on these GPIOs
 */
subsys_initcall(mdio_bsc_i2c_init);

static void __exit mdio_bsc_i2c_exit(void)
{
	i2c_del_driver(&mdio_bsc_i2c_driver);
}
module_exit(mdio_bsc_i2c_exit);

MODULE_AUTHOR("Jimmy Mumper <jmumper@iveia.com>");
MODULE_DESCRIPTION("Broadcom BSC over I2C Driver");
MODULE_LICENSE("GPL");
