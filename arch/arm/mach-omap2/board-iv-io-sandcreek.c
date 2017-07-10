/*
 * linux/arch/arm/mach-omap2/board-iv-io-sandcreek.c
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

#define BOARD_NAME	"Sand Creek"
#define BOARD_ORD	"00035"

/*
 * I2C device IRQs
 *
 * Note: for the GPIO expanders, the IRQs are really:
 *	- GPIO expander 1 -> TBD
 *	- GPIO expander 2 -> TBD
 * But, the GPIO expander driver requires the irq be setup for all devices when
 * IRQ support is compiled in.  So, we use unused GPIO pins (NC) for expanders
 * that don't use IRQs.
 */
#define IV_GPIO_EXPANDER_1_IRQ_GPIO 28	// Unused GPIO
#define IV_GPIO_EXPANDER_2_IRQ_GPIO 27	// Unused GPIO
static int __initdata iv_io_i2c_irq_gpios[] = {
	IV_GPIO_EXPANDER_1_IRQ_GPIO,
	IV_GPIO_EXPANDER_2_IRQ_GPIO,
};

/*
 * Sandcreek Carrier GPIOs
 */
#define GPIO_EXPANDER_BASE_1    (TWL4030_GPIO_BASE + TWL4030_GPIO_MAX)
#define GPIO_EXPANDER_SIZE_1    16
#define GPIO_EXPANDER_BASE_2    ( GPIO_EXPANDER_BASE_1 + GPIO_EXPANDER_SIZE_1)
#define GPIO_EXPANDER_SIZE_2    16

#define GPIO_PWR_JACK           (GPIO_EXPANDER_BASE_1 + 0)
#define GPIO_PWR_INT            (GPIO_EXPANDER_BASE_1 + 1)
#define GPIO_ADC_MEASURE        (GPIO_EXPANDER_BASE_1 + 2)
#define GPIO_3_9V_OFF           (GPIO_EXPANDER_BASE_1 + 3)
#define GPIO_CHRG_OFF           (GPIO_EXPANDER_BASE_1 + 4)
#define GPIO_RTC_IRQ            (GPIO_EXPANDER_BASE_1 + 5)
#define GPIO_NULL_6             (GPIO_EXPANDER_BASE_1 + 6)
#define GPIO_BT_UART_WAKE       (GPIO_EXPANDER_BASE_1 + 7)
#define GPIO_USB_SUSP_IND       (GPIO_EXPANDER_BASE_1 + 8)
#define GPIO_USB_HS_IND         (GPIO_EXPANDER_BASE_1 + 9)
#define GPIO_BT_WAKE_UP         (GPIO_EXPANDER_BASE_1 + 10)
#define GPIO_BT_EN              (GPIO_EXPANDER_BASE_1 + 11)
#define GPIO_WL_EN              (GPIO_EXPANDER_BASE_1 + 12)
#define GPIO_ADC1_RESET         (GPIO_EXPANDER_BASE_1 + 13)
#define GPIO_ADC1_NAPSLP        (GPIO_EXPANDER_BASE_1 + 14)
#define GPIO_PWR_KILL           (GPIO_EXPANDER_BASE_1 + 15)

#define GPIO_WDE                (GPIO_EXPANDER_BASE_2 + 0)
#define GPIO_SW_RESET           (GPIO_EXPANDER_BASE_2 + 1)
#define GPIO_USB_TOP_SENSE      (GPIO_EXPANDER_BASE_2 + 2)
#define GPIO_GPS_RESET          (GPIO_EXPANDER_BASE_2 + 3)
#define GPIO_GPS_PROGSELECT     (GPIO_EXPANDER_BASE_2 + 4)
#define GPIO_EE_WE              (GPIO_EXPANDER_BASE_2 + 5)
#define GPIO_INERTIAL_INT1      (GPIO_EXPANDER_BASE_2 + 6)
#define GPIO_INERTIAL_INT2      (GPIO_EXPANDER_BASE_2 + 7)
#define GPIO_DIO_GPIO_0         (GPIO_EXPANDER_BASE_2 + 8)
#define GPIO_DIO_GPIO_1         (GPIO_EXPANDER_BASE_2 + 9)
#define GPIO_DIO_GPIO_2         (GPIO_EXPANDER_BASE_2 + 10)
#define GPIO_DIO_GPIO_3         (GPIO_EXPANDER_BASE_2 + 11)
#define GPIO_DIO_GPIO_4         (GPIO_EXPANDER_BASE_2 + 12)
#define GPIO_DIO_GPIO_5         (GPIO_EXPANDER_BASE_2 + 13)
#define GPIO_FAULT              (GPIO_EXPANDER_BASE_2 + 14)
#define GPIO_CHRG               (GPIO_EXPANDER_BASE_2 + 15)

#define NEC_DISPLAY                             "nec-nl4864hl11"

/*
 * Keymap for the P3 faceplate when attached to the Sandcreek board
 */
static int board_keymap_sandcreek_p3_faceplate[] = {
	KEY(0, 0, KEY_HOME),   // 8
	KEY(1, 0, KEY_ENTER),  // 5
	KEY(2, 0, KEY_LEFT),   // 1
	KEY(3, 0, KEY_Z),      //

	KEY(0, 1, KEY_RIGHT),  // 7
	KEY(0, 2, KEY_MENU),   // 6
	KEY(1, 1, KEY_DOWN),   // 4
	KEY(1, 2, KEY_UP),     // 3
	KEY(2, 2, KEY_Z),      //

	KEY(2, 1, KEY_BACK),   // 2
};

static struct matrix_keymap_data iv_io_keymap_data = {
	.keymap                 = board_keymap_sandcreek_p3_faceplate,
	.keymap_size            = ARRAY_SIZE(board_keymap_sandcreek_p3_faceplate),
};

/*
 * GF Sandcreek I2C bus
 */
static struct pca953x_platform_data iv_gf_sandcreek_gpio_expander1_8v = {
	.gpio_base	= GPIO_EXPANDER_BASE_1,
	.irq_base	= OMAP_GPIO_IRQ_BASE + 0,
};
static struct pca953x_platform_data iv_gf_sandcreek_gpio_expander3_1v = {
	.gpio_base	= GPIO_EXPANDER_BASE_2,
	.irq_base	= OMAP_GPIO_IRQ_BASE + GPIO_EXPANDER_SIZE_1,
};
static struct i2c_board_info __initdata iv_io_i2c3_boardinfo[] = {
	{
		I2C_BOARD_INFO("tca6416", 0x20),
		.irq = IV_GPIO_EXPANDER_1_IRQ_GPIO, // Needs to be converted at runtime.
		.platform_data = &iv_gf_sandcreek_gpio_expander1_8v,
	},
	{
		I2C_BOARD_INFO("tca6416", 0x21),
		.irq = IV_GPIO_EXPANDER_2_IRQ_GPIO, // Needs to be converted at runtime.
		.platform_data = &iv_gf_sandcreek_gpio_expander3_1v,
	},
	{
		I2C_BOARD_INFO("m41t80", 0x68),
	},
	{
		I2C_BOARD_INFO("mcp4641t", 0x2E),
	},
};

extern void iv_io_set_lcd_backlight_level_i2c_dpot(unsigned int level);
static void iv_io_set_lcd_backlight_level(unsigned int level)
{
	iv_io_set_lcd_backlight_level_i2c_dpot(level);
}

extern struct platform_device iv_io_wl1271_device;
static struct platform_device * iv_io_platform_devices[] __initdata = {
	&iv_io_wl1271_device,
};

static struct iv_io_board_info iv_io_board_info = {
	.name				= BOARD_NAME,
	.board_ord			= BOARD_ORD,
	.lcd_driver_name		= NEC_DISPLAY,
	.set_lcd_backlight_level	= iv_io_set_lcd_backlight_level,
	.keymap_data			= &iv_io_keymap_data,
	.i2c3_boardinfo			= iv_io_i2c3_boardinfo,
	.i2c3_boardinfo_size		= ARRAY_SIZE(iv_io_i2c3_boardinfo),
	.i2c_irq_gpios			= iv_io_i2c_irq_gpios,
	.i2c_irq_gpios_size		= ARRAY_SIZE(iv_io_i2c_irq_gpios),
	.platform_devices		= iv_io_platform_devices,
	.platform_devices_size		= ARRAY_SIZE(iv_io_platform_devices),
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
