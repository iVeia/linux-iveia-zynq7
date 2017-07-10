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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/opp.h>
#include <linux/input/matrix_keypad.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/version.h>

#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/mmc/host.h>

#include <linux/spi/spi.h>
#include <linux/spi/ads7846.h>
#include <linux/i2c/twl.h>
#include <linux/i2c/pca953x.h>
#include <linux/usb/otg.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>
#include <linux/mmc/host.h>

#include <mach/hardware.h>
#include <mach/board-iv-atlas-i-lpe.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
#include <video/omapdss.h>
#include <video/omap-panel-generic-dpi.h>
#else
#include <plat/display.h>
#endif

#include <plat/board.h>
#include <plat/common.h>
#include <plat/usb.h>
#include <plat/mcspi.h>
#include <plat/omap_device.h>

#include "mux.h"
#include "hsmmc.h"

struct platform_device iv_io_wl1271_device = {
	.name = "tiwlan_pm_driver",
	.id   = -1,
	.dev = {
	.platform_data = NULL,
	},
};

struct omap2_hsmmc_info iv_io_mmc2 = {
	.mmc		= 2,
	.caps		= MMC_CAP_4_BIT_DATA,
	.gpio_cd	= 26,
	.gpio_wp	= -EINVAL,
	.ext_clock	= 1,
};

#define LCD_BL_I2C_DPOT_MAX_VAL                 0x00FF
extern int mcp464x_write_byte(u8 reg, u8 val);
void iv_io_set_lcd_backlight_level_i2c_dpot(unsigned int level)
{
	/* Write wiper 1 register (0x10) */
	mcp464x_write_byte(0x10, (LCD_BL_I2C_DPOT_MAX_VAL * level) / 100);
}

#define LCD_BL_I2C_DAC_MIN_VAL                  0x0C00
#define LCD_BL_I2C_DAC_MAX_VAL                  0xFFFF
extern int ltc2635_write_word(u8 reg, u16 val);
void iv_io_set_lcd_backlight_level_i2c_dac(unsigned int level)
{
	/*
	 * The level to DAC conversion is non-linear.  We'll use a very rough
	 * quadratic to get make the level appear linear.  The quadratic was
	 * determined experimentally:
	 *	y = 0.01 * x^2;
	 * Note that at MIN and below, the backlight appears completely off, so
	 * we'll enforce a MIN value unless the level is exactly 0.
	 *
	 * TODO: This quadratic may make this code Arcadia specific - but
	 * currently, its only used on Arcadia.  If so, needs to move to
	 * Arcadia board file.
	 */
	level = (level * level)/100;
	if (level > 100)
		level = LCD_BL_I2C_DAC_MAX_VAL;
	else if (level > 0) {
		level = (LCD_BL_I2C_DAC_MAX_VAL * level) / 100;
		if (level < LCD_BL_I2C_DAC_MIN_VAL)
			level = LCD_BL_I2C_DAC_MIN_VAL;
	}

	/* Write all DAC outputs */
	ltc2635_write_word(0x3F, level);
}


