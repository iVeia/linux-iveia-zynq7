/*
 * linux/arch/arm/mach-omap2/board-iv-io-arcadia.c
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
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
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/version.h>

#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/mmc/host.h>

#include <linux/spi/spi.h>
#include <linux/usb/otg.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>
#include <linux/mmc/host.h>
#include <linux/power_supply.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#include "board-iv-atlas-i-z7e.h"
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#define BOARD_NAME	"Bluepoint"
#define BOARD_ORD	"00080"


extern struct platform_device *get_first_mmc(void);

/*
 * Bluepoint I2C bus
 * 0x68 = Real-time Clock
 */
static struct i2c_board_info __initdata iv_io_i2c0_boardinfo[] = {
	{
		I2C_BOARD_INFO("m41t62", 0x68),
	},
};

static int gpio_out(unsigned gpio, int value)
{
	int ret;
	ret = gpio_request(gpio, NULL);
	if (ret == 0)
		ret = gpio_direction_output(gpio, value);
	return ret;
}

static int gpio_in(unsigned gpio)
{
	int ret;
	ret = gpio_request(gpio, NULL);
	if (ret == 0)
		ret = gpio_direction_input(gpio);
	return ret;
}

static void __init iv_io_late_init(struct iv_io_board_info * info)
{
	printk (KERN_ERR "BLUEPOINT BOARD --- LATE INIT\n");
//	i2c_register_board_info(0, iv_io_i2c0_boardinfo, ARRAY_SIZE(iv_io_i2c0_boardinfo));
}

static void __init iv_io_really_late_init(struct iv_io_board_info * info)
{
	int ret = 0;

	if (ret) {
		printk(KERN_ERR BOARD_NAME ": Can't setup some HW (err %d)\n", ret);
	}
}

static struct iv_io_board_info iv_io_board_info = {
	.name				= BOARD_NAME,
	.board_ord			= BOARD_ORD,
	.late_init			= iv_io_late_init,
	.really_late_init		= iv_io_really_late_init,
	.i2c0_boardinfo			= iv_io_i2c0_boardinfo,
	.i2c0_boardinfo_size		= ARRAY_SIZE(iv_io_i2c0_boardinfo),
//	.platform_devices		= iv_io_platform_devices,
//	.platform_devices_size		= ARRAY_SIZE(iv_io_platform_devices),
/*	.spk_amp_en_gpio		= GPIO_AMP_EN,
	.spk_amp_low_gpio		= GPIO_AMP_GAIN_LOW,
	.spk_amp_high_gpio		= GPIO_AMP_GAIN_HIGH_N,
	.headset_jack_gpio		= GPIO_HP_DETECT_N,
*/	.has_spk			= 1,
	.has_mic3			= 1,
	.has_headset			= 1,
};

static int __init iv_io_board_init(void)
{
	int err = iv_io_register_board_info(&iv_io_board_info);
	if (err < 0) {
		printk(KERN_ERR BOARD_NAME
		       ":Could not register iVeia IO board '%s'.\n", BOARD_NAME);
		return err;
	}
	return 0;
}

postcore_initcall(iv_io_board_init);
