/*
 * i2c-iv_ocp interface to platform code
 *
 * Copyright (C) 2014 iVeia LLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _LINUX_I2C_OCP_H
#define _LINUX_I2C_OCP_H

/**
 * struct i2c_ocp_platform_data - Platform-dependent data for i2c-iv_ocp
 * @base: I2C OCP core memory base
 * @udelay: signal toggle delay. SCL frequency is (500 / udelay) kHz
 * @timeout: clock stretching timeout in jiffies. If the slave keeps
 *	SCL low for longer than this, the transfer will time out.
 */
struct i2c_ocp_platform_data {
	void __iomem	*base;
	int		udelay;
	int		timeout;
};

#endif /* _LINUX_I2C_OCP_H */
