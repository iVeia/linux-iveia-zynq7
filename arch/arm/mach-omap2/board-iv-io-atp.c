/*
 * linux/arch/arm/mach-omap2/board-iv-io-atp.c
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

#include <plat/board.h>
#include <plat/common.h>
#include <plat/usb.h>
#include <plat/mcspi.h>
#include <plat/omap_device.h>

#include "mux.h"
#include "hsmmc.h"

#define BOARD_NAME   "Atlas-II ATP Carrier"
#define BOARD_ORD    "00062"

/*
 * I2C device IRQs
 *
 * Note: for the GPIO expanders, the IRQs are really:
 *	- GPIO expander 1 -> NC
 * But, the GPIO expander driver requires the irq be setup for all devices when
 * IRQ support is compiled in.  So, we use unused GPIO pins (NC) for expanders
 * that don't use IRQs.
 */
#define IV_GPIO_EXPANDER_1_IRQ_GPIO 28	// Unused GPIO
static int __initdata iv_io_i2c_irq_gpios[] = {
	IV_GPIO_EXPANDER_1_IRQ_GPIO,
};

/*
 * Carrier GPIOs
 */
#define GPIO_EXPANDER_BASE	(TWL4030_GPIO_BASE + TWL4030_GPIO_MAX)

#define GPIO_GRN_ON_N		(GPIO_EXPANDER_BASE + 0)
#define GPIO_YEL_ON_N		(GPIO_EXPANDER_BASE + 1)
#define GPIO_RED_ON_N		(GPIO_EXPANDER_BASE + 2)
#define GPIO_AUX_ON_N		(GPIO_EXPANDER_BASE + 3)
#define GPIO_READY_WAKE_N	(GPIO_EXPANDER_BASE + 4)
#define GPIO_SLAVE_EN		(GPIO_EXPANDER_BASE + 5)
#define GPIO_SET_USBA_LOW	(GPIO_EXPANDER_BASE + 6)
#define GPIO_NC			(GPIO_EXPANDER_BASE + 7)

/*
 * Carrier I2C bus
 */
static struct pca953x_platform_data iv_io_gpio_expander = {
	.gpio_base	= GPIO_EXPANDER_BASE,
	.irq_base	= OMAP_GPIO_IRQ_BASE,
};
static struct i2c_board_info __initdata iv_io_i2c3_boardinfo[] = {
	{
		I2C_BOARD_INFO("pca9672", 0x22),
		.irq = IV_GPIO_EXPANDER_1_IRQ_GPIO, // Needs to be converted at runtime.
		.platform_data = &iv_io_gpio_expander,
	},
};

extern struct omap2_hsmmc_info iv_io_mmc2;

static struct iv_io_board_info iv_io_board_info = {
	.name				= BOARD_NAME,
	.board_ord			= BOARD_ORD,
	.mmc2				= &iv_io_mmc2,
	.i2c3_boardinfo			= iv_io_i2c3_boardinfo,
	.i2c3_boardinfo_size		= ARRAY_SIZE(iv_io_i2c3_boardinfo),
	.i2c_irq_gpios			= iv_io_i2c_irq_gpios,
	.i2c_irq_gpios_size		= ARRAY_SIZE(iv_io_i2c_irq_gpios),
	.has_mic1			= 1,
	.has_passthru_mic		= 1,
	.has_mic3			= 1,
	.has_headset			= 1,
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
