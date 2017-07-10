/*
 * Bitbanging OCP I2C bus driver using the IOMEM API
 *
 * Copyright (C) 2014 iVeia LLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/i2c-iv_ocp.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>

struct i2c_ocp_private_data {
	struct i2c_adapter adap;
	struct i2c_algo_bit_data bit_data;
	struct i2c_ocp_platform_data pdata;
};

/*
 * Toggle SDA by changing the output value of the pin. This is only
 * valid for pins configured as open drain (i.e. setting the value
 * high effectively turns off the output driver.)
 */
static void i2c_ocp_setsda_val(void *data, int state)
{
	struct i2c_ocp_platform_data *pdata = data;

	u32 prev = ioread32(pdata->base);
	if (state)
	    iowrite32((prev | 0x2), pdata->base);
	else
	    iowrite32((prev & 0x1), pdata->base);
}

/*
 * Toggle SCL by changing the output value of the pin. This is used
 * for pins that are configured as open drain and for output-only
 * pins. The latter case will break the i2c protocol, but it will
 * often work in practice.
 */
static void i2c_ocp_setscl_val(void *data, int state)
{
	struct i2c_ocp_platform_data *pdata = data;

	u32 prev = ioread32(pdata->base);
	if (state)
	    iowrite32((prev | 0x1), pdata->base);
	else
	    iowrite32((prev & 0x2), pdata->base);

}

static int i2c_ocp_getsda(void *data)
{
	struct i2c_ocp_platform_data *pdata = data;

	return (ioread32(pdata->base) & 0x2);
}

static int i2c_ocp_getscl(void *data)
{
	struct i2c_ocp_platform_data *pdata = data;

	return (ioread32(pdata->base) & 0x1);
}

static int of_i2c_ocp_probe(struct device_node *np,
			     struct i2c_ocp_platform_data *pdata)
{
	u32 reg;

	of_property_read_u32(np, "i2c-ocp,delay-us", &pdata->udelay);

	if (!of_property_read_u32(np, "i2c-ocp,timeout-ms", &reg))
		pdata->timeout = msecs_to_jiffies(reg);

	return 0;
}

static int i2c_ocp_probe(struct platform_device *pdev)
{
	struct i2c_ocp_private_data *priv;
	struct i2c_ocp_platform_data *pdata;
	struct i2c_algo_bit_data *bit_data;
	struct i2c_adapter *adap;
	int ret;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res){
		dev_err(&pdev->dev, "Memory resource is missing\n");
		return -ENOENT;
	}
		

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	adap = &priv->adap;
	bit_data = &priv->bit_data;
	pdata = &priv->pdata;

	if (pdev->dev.of_node) {
		ret = of_i2c_ocp_probe(pdev->dev.of_node, pdata);
		if (ret)
			return ret;
	} else {
		if (!pdev->dev.platform_data)
			return -ENXIO;
		memcpy(pdata, pdev->dev.platform_data, sizeof(*pdata));
	}

	if (!request_mem_region(res->start, resource_size(res), pdev->name)) {
		dev_err(&pdev->dev, "Memory region busy\n");
		ret = -EBUSY;
		goto request_mem_failed;
	}

	pdata->base = ioremap(res->start, resource_size(res));
	if (!pdata->base) {
		dev_err(&pdev->dev, "Unable to map OCP registers\n");
		ret = -EIO;
		goto map_failed;
	}


	bit_data->setsda = i2c_ocp_setsda_val;
	bit_data->setscl = i2c_ocp_setscl_val;
	bit_data->getscl = i2c_ocp_getscl;
	bit_data->getsda = i2c_ocp_getsda;

	if (pdata->udelay)
		bit_data->udelay = pdata->udelay;
	else
		bit_data->udelay = 5;			/* 100 kHz */

	if (pdata->timeout)
		bit_data->timeout = pdata->timeout;
	else
		bit_data->timeout = HZ / 10;		/* 100 ms */

	bit_data->data = pdata;

	adap->owner = THIS_MODULE;
	if (pdev->dev.of_node)
		strlcpy(adap->name, dev_name(&pdev->dev), sizeof(adap->name));
	else
		snprintf(adap->name, sizeof(adap->name), "i2c-ocp%d", pdev->id);

	adap->algo_data = bit_data;
	adap->class = I2C_CLASS_HWMON | I2C_CLASS_SPD;
	adap->dev.parent = &pdev->dev;
	adap->dev.of_node = pdev->dev.of_node;

	adap->nr = pdev->id;
	ret = i2c_bit_add_numbered_bus(adap);
	if (ret)
		goto err_add_bus;

//	of_i2c_register_devices(adap);

	platform_set_drvdata(pdev, priv);

	dev_info(&pdev->dev, "using address base: %p\n", pdata->base);

	return 0;

err_add_bus:
	iounmap(pdata->base);
map_failed:
	release_mem_region(res->start, resource_size(res));
request_mem_failed:
	kfree(priv);
	return ret;
}

static int i2c_ocp_remove(struct platform_device *pdev)
{
	struct i2c_ocp_private_data *priv;
	struct i2c_ocp_platform_data *pdata;
	struct i2c_adapter *adap;
	struct resource *res;

	priv = platform_get_drvdata(pdev);
	adap = &priv->adap;
	pdata = &priv->pdata;
	iounmap(pdata->base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(res)
		release_mem_region(res->start, resource_size(res));

	i2c_del_adapter(adap);
	kfree(priv);

	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id i2c_ocp_dt_ids[] = {
	{ .compatible = "iveia,i2c-ocp", },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, i2c_ocp_dt_ids);
#endif

static struct platform_driver i2c_ocp_driver = {
	.driver		= {
		.name	= "i2c-ocp",
		.owner	= THIS_MODULE,
		.of_match_table	= of_match_ptr(i2c_ocp_dt_ids),
	},
	.probe		= i2c_ocp_probe,
	.remove		= i2c_ocp_remove,
};

static int __init i2c_ocp_init(void)
{
	int ret;

	ret = platform_driver_register(&i2c_ocp_driver);
	if (ret)
		printk(KERN_ERR "i2c-iv_ocp: probe failed: %d\n", ret);

	return ret;
}
subsys_initcall(i2c_ocp_init);

static void __exit i2c_ocp_exit(void)
{
	platform_driver_unregister(&i2c_ocp_driver);
}
module_exit(i2c_ocp_exit);

MODULE_AUTHOR("Patrick Cleary (iVeia)");
MODULE_DESCRIPTION("Bitbanging OCP core I2C driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:i2c-ocp");
