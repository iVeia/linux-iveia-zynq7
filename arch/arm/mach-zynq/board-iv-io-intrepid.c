/*
 * linux/arch/arm/mach-omap2/board-iv-io-intrepid.c
 *
 * Copyright (C) 2017 iVeia, LLC.
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
#include <linux/i2c/at24.h>
#include <linux/spi/spi.h>
#include <linux/spi/eeprom.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/mtd/physmap.h>
#include <linux/spi/flash.h>
#include <linux/xilinx_devices.h>
#include <linux/of_platform.h>
#include <linux/opp.h>
#include <linux/of_irq.h>
#include <linux/ctype.h>
#include <linux/random.h>
#include <linux/i2c/pca953x.h>
#include <linux/i2c/pca954x.h>
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
//#include <asm/hardware/gic.h>
#include <asm/hardware/cache-l2x0.h>

//#include <mach/zynq_soc.h>
//#include <mach/pdev.h>

//#include <mach/clk.h>
//#include <mach/uart.h>

#ifdef CONFIG_WILINK_PLATFORM_DATA
#include <linux/wl12xx.h>
#include <linux/regulator/fixed.h>
#include <linux/skbuff.h>
#include <linux/ti_wilink_st.h>
#endif

#include "board-iv-atlas-i-z7e.h"

#include <linux/proc_fs.h>

#include "common.h"

#define BOARD_NAME   "Intrepid"
#define BOARD_ORD    "00099"


#ifdef CONFIG_WILINK_PLATFORM_DATA

#define GPIO_WL_EN       (54)
#define WLAN_PMENA_GPIO       (GPIO_WL_EN)
#define WLAN_IRQ         (89)

static struct regulator_consumer_supply iv_io_vmmc2_supply =
	REGULATOR_SUPPLY("vmmc", "e0100000.sdhci");

static struct regulator_init_data iv_io_vmmc2 = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies = &iv_io_vmmc2_supply,
};

static struct fixed_voltage_config iv_io_vwlan = {
	.supply_name            = "vwl1271",
	.microvolts             = 1800000,
	.gpio                   = WLAN_PMENA_GPIO,
	.startup_delay          = 70000,
	.enable_high            = 1,
	.enabled_at_boot        = 1,
	.init_data              = &iv_io_vmmc2,
};

static struct platform_device iv_io_wlan_regulator = {
	.name           = "reg-fixed-voltage",
	.id             = 1,
	.dev = {
		.platform_data  = &iv_io_vwlan,
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

static void iv_wl12xx_power (bool enable) {
	int ret;
	if (enable) {
		ret = gpio_out(GPIO_WL_EN, 1);
		if (ret < 0)
			printk (KERN_ERR "No control over GPIO_WL_EN\n");
		printk (KERN_ERR "WLAN12XX TURNING ON\n");
	}
	else {
		ret = gpio_out(GPIO_WL_EN, 0);
		if (ret < 0)
			printk (KERN_ERR "No control over GPIO_WL_EN\n");
		printk (KERN_ERR "WLAN12XX TURNING OFF\n");
	}
}

static struct wl12xx_platform_data iv_io_wlan_data __initdata = {
//	.set_power = &iv_wl12xx_power,
	.irq = 89,
	.board_ref_clock = WL12XX_REFCLOCK_38, /* 38.4 MHz */
};

static void __init wifi_late_init(void)
{
	/* WL12xx WLAN Init */

	if (wl12xx_set_platform_data(&iv_io_wlan_data))
		pr_err("error setting wl12xx data\n");
//	platform_device_register(&iv_io_wlan_regulator);
	
}

#define iv_io_mmc2_wifi &_iv_io_mmc2_wifi
extern struct platform_device iv_io_wl1271_device;

static struct platform_device * iv_io_platform_devices[] __initdata = {
};

#else

#define iv_io_board_mux NULL
#define iv_io_mmc2_wifi NULL
static void __init wifi_late_init(void) {}

extern struct platform_device iv_io_wl1271_device;
static struct platform_device * iv_io_platform_devices[] __initdata = {
	&iv_io_wl1271_device,
};


#endif



static void __init iv_io_late_init(struct iv_io_board_info *info) {

	int err = 0;
#ifdef CONFIG_WILINK_PLATFORM_DATA
	platform_device_register(&iv_io_wlan_regulator);
#endif
//	iv_wl12xx_power(true);
};

static void __init iv_io_really_late_init(struct iv_io_board_info *info) {
	wifi_late_init();
};


static struct iv_io_board_info iv_io_board_info = {
	.name				= BOARD_NAME,
	.board_ord			= BOARD_ORD,
	.late_init			= iv_io_late_init,
	.really_late_init		= iv_io_really_late_init,
//	.mmc2				= &iv_io_mmc2,
//	.i2c0_boardinfo			= iv_io_i2c0_boardinfo,
//	.i2c0_boardinfo_size		= ARRAY_SIZE(iv_io_i2c0_boardinfo),
	.platform_devices		= iv_io_platform_devices,
	.platform_devices_size		= ARRAY_SIZE(iv_io_platform_devices),
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
