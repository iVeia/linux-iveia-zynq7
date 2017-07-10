/*
 * linux/arch/arm/mach-omap2/board-iv-io-triumph.c
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

#include <linux/of.h>
#include <linux/memblock.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <asm/mach/map.h>
#include <asm/mach/time.h>

#include <asm/page.h>
//#include <asm/hardware/gic.h>
#include <asm/hardware/cache-l2x0.h>

//#include <mach/zynq_soc.h>
//#include <mach/pdev.h>

//#include <mach/clk.h>
//#include <mach/uart.h>

#include "board-iv-atlas-i-z7e.h"

#include <linux/proc_fs.h>

#include "common.h"

#define BOARD_NAME   "Triumph"
#define BOARD_ORD    "00043"

//extern struct omap2_hsmmc_info iv_io_mmc2;

static struct i2c_board_info __initdata iv_io_i2c0_boardinfo[] = {
    {
        I2C_BOARD_INFO("m41t62", 0x68),  // rtc
    },
};

static struct iv_io_board_info iv_io_board_info = {
	.name				= BOARD_NAME,
	.board_ord			= BOARD_ORD,
//	.mmc2				= &iv_io_mmc2,
	.i2c0_boardinfo			= iv_io_i2c0_boardinfo,
	.i2c0_boardinfo_size		= ARRAY_SIZE(iv_io_i2c0_boardinfo),
	.has_mic1			= 1,
	.has_headset			= 1,
    .has_passthru_mic   = 1,
};

static int __init iv_io_board_init(void)
{

	int err = iv_io_register_board_info(&iv_io_board_info);
	if (err < 0) {
		printk(KERN_ERR "Could not register iVeia IO board '%s'.\n", BOARD_NAME);
		return err;
	}
	return 0;
}

postcore_initcall(iv_io_board_init);
