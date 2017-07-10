/*
 * linux/arch/arm/mach-omap2/board-iv-io-atlas-ii-gf-carrier.c
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

#define BOARD_NAME   "Atlas-II GigaFlex"
#define BOARD_ORD    "00034"

/*
 * I2C device IRQs
 *
 * Note: for the GPIO expanders, the IRQs are really:
 *	- GPIO expander 1 -> TBD
 * But, the GPIO expander driver requires the irq be setup for all devices when
 * IRQ support is compiled in.  So, we use unused GPIO pins (NC) for expanders
 * that don't use IRQs.
 */
#define IV_GPIO_EXPANDER_1_IRQ_GPIO 28	// Unused GPIO
static int __initdata iv_io_i2c_irq_gpios[] = {
	IV_GPIO_EXPANDER_1_IRQ_GPIO,
};

/*
 * Atlas-II GF Carrier GPIOs
 */
#define IV_ATLAS_II_GF_CARRIER_GPIO_EXPANDER_BASE   (TWL4030_GPIO_BASE + TWL4030_GPIO_MAX)

#define IV_ATLAS_II_GF_CARRIER_GPIO_PLUG_DETECT     (IV_ATLAS_II_GF_CARRIER_GPIO_EXPANDER_BASE + 0)
#define IV_ATLAS_II_GF_CARRIER_GPIO_BAT_BI          (IV_ATLAS_II_GF_CARRIER_GPIO_EXPANDER_BASE + 1)
#define IV_ATLAS_II_GF_CARRIER_GPIO_BAT_LOW         (IV_ATLAS_II_GF_CARRIER_GPIO_EXPANDER_BASE + 2)
#define IV_ATLAS_II_GF_CARRIER_GPIO_BAT_GD_N        (IV_ATLAS_II_GF_CARRIER_GPIO_EXPANDER_BASE + 3)
#define IV_ATLAS_II_GF_CARRIER_GPIO_CHRG_N          (IV_ATLAS_II_GF_CARRIER_GPIO_EXPANDER_BASE + 4)
#define IV_ATLAS_II_GF_CARRIER_GPIO_DC_DETECT_N     (IV_ATLAS_II_GF_CARRIER_GPIO_EXPANDER_BASE + 5)
#define IV_ATLAS_II_GF_CARRIER_GPIO_ETH_INT_N       (IV_ATLAS_II_GF_CARRIER_GPIO_EXPANDER_BASE + 6)
#define IV_ATLAS_II_GF_CARRIER_GPIO_NC_IO0_7        (IV_ATLAS_II_GF_CARRIER_GPIO_EXPANDER_BASE + 7)
#define IV_ATLAS_II_GF_CARRIER_GPIO_GPIO3_ALT       (IV_ATLAS_II_GF_CARRIER_GPIO_EXPANDER_BASE + 8)
#define IV_ATLAS_II_GF_CARRIER_GPIO_SD_PWR_N        (IV_ATLAS_II_GF_CARRIER_GPIO_EXPANDER_BASE + 9)
#define IV_ATLAS_II_GF_CARRIER_GPIO_SPKR_AMP_EN     (IV_ATLAS_II_GF_CARRIER_GPIO_EXPANDER_BASE + 10)
#define IV_ATLAS_II_GF_CARRIER_GPIO_MIC_CAL         (IV_ATLAS_II_GF_CARRIER_GPIO_EXPANDER_BASE + 11)
#define IV_ATLAS_II_GF_CARRIER_GPIO_MIC_T7          (IV_ATLAS_II_GF_CARRIER_GPIO_EXPANDER_BASE + 12)
#define IV_ATLAS_II_GF_CARRIER_GPIO_MIC_PE          (IV_ATLAS_II_GF_CARRIER_GPIO_EXPANDER_BASE + 13)
#define IV_ATLAS_II_GF_CARRIER_GPIO_NC_IO1_6        (IV_ATLAS_II_GF_CARRIER_GPIO_EXPANDER_BASE + 14)
#define IV_ATLAS_II_GF_CARRIER_GPIO_EE_WE_N         (IV_ATLAS_II_GF_CARRIER_GPIO_EXPANDER_BASE + 15)

/*
 * Atlas-II GF Carrier keymap
 */
static uint32_t board_keymap_atlas_ii_gf[] = {
	KEY(0, 0, KEY_HOME),    // S3
	KEY(1, 0, KEY_BACK),    // S5
	KEY(2, 0, KEY_MENU),    // S4
	KEY(3, 0, KEY_ENTER),   // S6

	// Note: the L/R/U/D keys are rotated from their schematic orientation.
	KEY(0, 1, KEY_LEFT),    // Joystick LEFT
	KEY(0, 2, KEY_REPLY),   // Joystick SELECT
	KEY(1, 1, KEY_UP),      // Joystick UP
	KEY(1, 2, KEY_DOWN),    // Joystick DOWN
	KEY(2, 2, KEY_RIGHT),   // Joystick RIGHT
};
static struct matrix_keymap_data iv_io_keymap_data= {
	.keymap                 = board_keymap_atlas_ii_gf,
	.keymap_size            = ARRAY_SIZE(board_keymap_atlas_ii_gf),
};

/*
 * Atlas-II GF Carrier I2C bus
 */
static struct pca953x_platform_data iv_atlas_ii_gf_gpio_expander = {
	.gpio_base	= IV_ATLAS_II_GF_CARRIER_GPIO_EXPANDER_BASE,
	.irq_base	= OMAP_GPIO_IRQ_BASE,
};
static struct i2c_board_info __initdata iv_io_i2c3_boardinfo[] = {
	{
		I2C_BOARD_INFO("pca9539", 0x76),
		.irq = IV_GPIO_EXPANDER_1_IRQ_GPIO, // Needs to be converted at runtime.
		.platform_data = &iv_atlas_ii_gf_gpio_expander,
	},
	{
		I2C_BOARD_INFO("lmv1089", 0x67),
	},
};

extern struct omap2_hsmmc_info iv_io_mmc2;

static struct iv_io_board_info iv_io_board_info = {
	.name				= BOARD_NAME,
	.board_ord			= BOARD_ORD,
	.mmc2				= &iv_io_mmc2,
	.keymap_data			= &iv_io_keymap_data,
	.i2c3_boardinfo			= iv_io_i2c3_boardinfo,
	.i2c3_boardinfo_size		= ARRAY_SIZE(iv_io_i2c3_boardinfo),
	.i2c_irq_gpios			= iv_io_i2c_irq_gpios,
	.i2c_irq_gpios_size		= ARRAY_SIZE(iv_io_i2c_irq_gpios),
	.spk_amp_en_gpio		= IV_ATLAS_II_GF_CARRIER_GPIO_SPKR_AMP_EN,
	.has_spk			= 1,
	.has_mic1			= 1,
	.has_mic2			= 1,
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
