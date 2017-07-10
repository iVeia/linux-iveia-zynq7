/*
 * linux/arch/arm/mach-omap2/board-iv-io-common.c
 *
 * Copyright (C) 2012 iVeia, LLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/cpumask.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/spi/spi.h>
#include <linux/spi/eeprom.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/mtd/physmap.h>
#include <linux/spi/flash.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/ctype.h>
#include <linux/random.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>
#include <linux/mmc/host.h>

#include <linux/of.h>
#include <linux/memblock.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <asm/mach/map.h>
#include <asm/mach/time.h>

#include <asm/page.h>
#include <asm/hardware/cache-l2x0.h>
#include "board-iv-atlas-i-z7e.h"

#include <linux/proc_fs.h>

struct platform_device iv_io_wl1271_device = {
	.name = "tiwlan_pm_driver",
	.id   = -1,
	.dev = {
	.platform_data = NULL,
	},
};

/*struct omap2_hsmmc_info iv_io_mmc2 = {
	.mmc		= 2,
	.caps		= MMC_CAP_4_BIT_DATA,
	.gpio_cd	= 26,
	.gpio_wp	= -EINVAL,
	.ext_clock	= 1,
};*/

