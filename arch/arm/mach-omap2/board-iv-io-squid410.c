/*
 * linux/arch/arm/mach-omap2/board-iv-io-squid410.c
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
#include <linux/power_supply.h>
#include <linux/ltc2942_battery.h>

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

#define BOARD_NAME	"Squid 410"
#define BOARD_ORD	"00039"

/*
 * Squid410 GPIOs
 *
 * Currently the only avaiable GPIO is speaker enable.  If others
 * are used, the code will not compile.  Speaker enable is directly hooked to
 * a OMAP GPIO through the FPGA.
 */
#define IV_SQUID410_GPIO_SPKR_AMP_EN     (143)

#define EMERGING_DISPLAY                        "emerging-et020005dmu"

/*
 * Squid keymap
 */
static uint32_t board_keymap_squid[] = {
	KEY(0, 0, KEY_1),    // S3
	KEY(1, 0, KEY_2),    // S5
	KEY(2, 0, KEY_3),    // S4
	KEY(3, 0, KEY_4),    // S6

	// Note: the L/R/U/D keys are rotated from their schematic orientation.
	KEY(0, 1, KEY_DOWN),    // Joystick LEFT
	KEY(0, 2, KEY_ENTER),   // Joystick SELECT
	KEY(1, 1, KEY_LEFT),    // Joystick UP
	KEY(1, 2, KEY_RIGHT),   // Joystick DOWN
	KEY(2, 2, KEY_UP),      // Joystick RIGHT
};
static struct matrix_keymap_data iv_io_keymap_data = {
   .keymap                 = board_keymap_squid,
   .keymap_size            = ARRAY_SIZE(board_keymap_squid),
};

/*
 * Alternate keymap substituted for 'board_keymap_squid' when
 * the 'iv_sq410_dbg' command line param is non-zero.
 */
static uint32_t board_keymap_squid_debug[] = {
	KEY(0, 0, KEY_HOME),    // S3
	KEY(1, 0, KEY_BACK),    // S5
	KEY(2, 0, KEY_MENU),    // S4
	KEY(3, 0, KEY_ENTER),   // S6

	// Note: the L/R/U/D keys are rotated from their schematic orientation.
	KEY(0, 1, KEY_RIGHT),   // Joystick LEFT
	KEY(0, 2, KEY_ENTER),   // Joystick SELECT
	KEY(1, 1, KEY_DOWN),    // Joystick UP
	KEY(1, 2, KEY_UP),      // Joystick DOWN
	KEY(2, 2, KEY_LEFT),    // Joystick RIGHT
};
static struct matrix_keymap_data board_keymap_data_squid_debug = {
   .keymap                 = board_keymap_squid_debug,
   .keymap_size            = ARRAY_SIZE(board_keymap_squid_debug),
};

static int ac_online(void)
{
	// Assume never present
	return 0;
}

static int usb_online(void)
{
	// Assume never present
	return 0;
}

static int battery_status(void)
{
	return POWER_SUPPLY_STATUS_CHARGING;
}

static int battery_present(void)
{
	// Assume always present 
	return 1;
}

static int battery_health(void)
{
	// Assume always good.  TBD: check for fault 
	return POWER_SUPPLY_HEALTH_GOOD;
}

static int battery_technology(void)
{
	return POWER_SUPPLY_TECHNOLOGY_LION;
}

static struct ltc2942_platform_data ltc2942_info = {
	.ac_online		= ac_online,
	.usb_online		= usb_online,
	.battery_status		= battery_status,
	.battery_present	= battery_present,
	.battery_health		= battery_health,
	.battery_technology	= battery_technology,
};


static struct omap_board_mux iv_io_board_mux[] __initdata = {
	/* BackLight - GPIO 165 */
	OMAP3_MUX(UART3_RX_IRRX, OMAP_MUX_MODE4 | OMAP_PIN_INPUT),
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};


/*
 * Squid410 I2C bus
 */
struct i2c_board_info __initdata iv_io_i2c3_boardinfo[] = {
	{
		I2C_BOARD_INFO("lmv1089", 0x67),
	},
/*	{
		I2C_BOARD_INFO("ltc2942", 0x64),
		.platform_data = &ltc2942_info,
	},
*/	{
		I2C_BOARD_INFO("m41t62", 0x68),
	},
};

extern void iv_io_set_lcd_backlight_level_i2c_dpot(unsigned int level);
static void iv_io_set_lcd_backlight_level(unsigned int level)
{
	iv_io_set_lcd_backlight_level_i2c_dpot(level);
}

extern struct omap2_hsmmc_info iv_io_mmc2;

static struct iv_io_board_info iv_io_board_info = {
	.name				= BOARD_NAME,
	.board_ord			= BOARD_ORD,
	.lcd_driver_name		= EMERGING_DISPLAY,
	.board_mux			= iv_io_board_mux,
//	.set_lcd_backlight_level	= iv_io_set_lcd_backlight_level,
	.mmc2				= &iv_io_mmc2,
	.keymap_data			= &board_keymap_data_squid_debug,
	.i2c3_boardinfo			= iv_io_i2c3_boardinfo,
	.i2c3_boardinfo_size		= ARRAY_SIZE(iv_io_i2c3_boardinfo),
	.spk_amp_en_gpio		= IV_SQUID410_GPIO_SPKR_AMP_EN,
	.has_spk			= 1,
	.has_mic1			= 1,
	.has_mic2			= 1,
	.has_headset			= 1,
};

/* Optional debug flag for SQUID alternate keypad layout */
static int __init iv_squid410_debug_setup(char *options)
{
	if (!strncmp("1", options, 1)) {
		iv_io_board_info.keymap_data = &board_keymap_data_squid_debug;
		printk("Using Squid DEBUG keymap\n");
	}
	return 0;
}
__setup("iv_sq410_dbg=", iv_squid410_debug_setup);

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
