/*
 * linux/arch/arm/mach-omap2/board-iv-io-maveriq.c
 *
 * Copyright (C) 2013 iVeia, LLC.
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

#define BOARD_NAME   "Sand Creek II"
#define BOARD_ORD    "00058"

#define IV_GPIO_EXPANDER_1_IRQ_GPIO 28	// Unused GPIO
#define IV_GPIO_EXPANDER_2_IRQ_GPIO 27	// Unused GPIO
static int __initdata iv_io_i2c_irq_gpios[] = {
	IV_GPIO_EXPANDER_1_IRQ_GPIO,
	IV_GPIO_EXPANDER_2_IRQ_GPIO,
};

#define GPIO_EXPANDER_BASE_1	(200)          //210
#define GPIO_EXPANDER_SIZE_1	24
#define GPIO_EXPANDER_BASE_2	(GPIO_EXPANDER_BASE_1 + GPIO_EXPANDER_SIZE_1)
#define GPIO_EXPANDER_SIZE_2	24

#define GPIO_PWR_VCCA_N            (GPIO_EXPANDER_BASE_1 + 0)
#define GPIO_PWR_INT_N             (GPIO_EXPANDER_BASE_1 + 1)
#define GPIO_CLK_INT_N             (GPIO_EXPANDER_BASE_1 + 2)
#define GPIO_USB_BOT_SENSE_N       (GPIO_EXPANDER_BASE_1 + 3)
#define GPIO_USB_TOP_SENSE_N       (GPIO_EXPANDER_BASE_1 + 4)
#define GPIO_RTC_IRQ_N             (GPIO_EXPANDER_BASE_1 + 5)
#define GPIO_USB_SUSP_IND          (GPIO_EXPANDER_BASE_1 + 6)
#define GPIO_USB_HS_IND            (GPIO_EXPANDER_BASE_1 + 7)

#define GPIO_AUX_PRESENT_N         (GPIO_EXPANDER_BASE_1 + 8)
#define GPIO_AUX_INT_N             (GPIO_EXPANDER_BASE_1 + 9)
#define GPIO_BT_UART_WAKE          (GPIO_EXPANDER_BASE_1 + 10)
#define GPIO_S_WAKE_1_8_V          (GPIO_EXPANDER_BASE_1 + 11)
#define GPIO_S_WAKE_CTL            (GPIO_EXPANDER_BASE_1 + 12)
#define GPIO_BT_WAKE_UP            (GPIO_EXPANDER_BASE_1 + 13)
#define GPIO_BT_EN                 (GPIO_EXPANDER_BASE_1 + 14)
#define GPIO_WL_EN                 (GPIO_EXPANDER_BASE_1 + 15)
                                                             
#define GPIO_HCI_CTS               (GPIO_EXPANDER_BASE_1 + 16)
#define GPIO_USB_DB_ID_GND         (GPIO_EXPANDER_BASE_1 + 17)
#define GPIO_PWR_KILL              (GPIO_EXPANDER_BASE_1 + 18)
#define GPIO_GPS_PWR               (GPIO_EXPANDER_BASE_1 + 19)
#define GPIO_POWER_ON_N            (GPIO_EXPANDER_BASE_1 + 20)

#define GPIO_M_MINI_GPIO0          (GPIO_EXPANDER_BASE_2 + 0)
#define GPIO_M_MINI_GPIO1          (GPIO_EXPANDER_BASE_2 + 1)
#define GPIO_M_MINI_GPIO2          (GPIO_EXPANDER_BASE_2 + 2)
#define GPIO_M_MINI_GPIO3          (GPIO_EXPANDER_BASE_2 + 3)
#define GPIO_M_MINI_GPIO4          (GPIO_EXPANDER_BASE_2 + 4)
#define GPIO_M_MINI_GPIO5          (GPIO_EXPANDER_BASE_2 + 5)
#define GPIO_M_MINI_GPIO6          (GPIO_EXPANDER_BASE_2 + 6)
#define GPIO_M_MINI_GPIO7          (GPIO_EXPANDER_BASE_2 + 7)

#define GPIO_M_MINI_INT_N          (GPIO_EXPANDER_BASE_2 + 8)
#define GPIO_M_MINI_PRESENT_N      (GPIO_EXPANDER_BASE_2 + 9)
#define GPIO_DRDY_M                (GPIO_EXPANDER_BASE_2 + 10)
#define GPIO_INERTIAL_INT1_N       (GPIO_EXPANDER_BASE_2 + 11)
#define GPIO_INERTIAL_INT2_N       (GPIO_EXPANDER_BASE_2 + 12)
#define GPIO_S_RESET_N             (GPIO_EXPANDER_BASE_2 + 13)
#define GPIO_M_SUSPEND_N           (GPIO_EXPANDER_BASE_2 + 14)
#define GPIO_EE_WE_N               (GPIO_EXPANDER_BASE_2 + 15)

#define GPIO_AUX_SUSPEND_N         (GPIO_EXPANDER_BASE_2 + 16)
#define GPIO_WDE                   (GPIO_EXPANDER_BASE_2 + 17)
#define GPIO_SW_RESET_N            (GPIO_EXPANDER_BASE_2 + 18)
#define GPIO_GPS_WAKE              (GPIO_EXPANDER_BASE_2 + 19)
#define GPIO_GPS_RESET_N           (GPIO_EXPANDER_BASE_2 + 20)
#define GPIO_USB_MUX_S             (GPIO_EXPANDER_BASE_2 + 21)
                                                             
#define GPIO_L1_L2_STATUS          (GPIO_EXPANDER_BASE_2 + 23)

static struct pca953x_platform_data gpio_expander_1 = {
	.gpio_base	= GPIO_EXPANDER_BASE_1,
//	.irq_base	= OMAP_GPIO_IRQ_BASE + 0,
};
static struct pca953x_platform_data gpio_expander_2 = {
	.gpio_base	= GPIO_EXPANDER_BASE_2,
//	.irq_base	= OMAP_GPIO_IRQ_BASE + GPIO_EXPANDER_SIZE_1,
};


//extern struct omap2_hsmmc_info iv_io_mmc2;

static struct pca954x_platform_mode pca954x_modes_mux0[] = {
	{ .adap_id = 4, },
	{ .adap_id = 5, },
	{ .adap_id = 6, },
	{ .adap_id = 7, },
	{ .adap_id = 8, },
	{ .adap_id = 9, },
};

static struct pca954x_platform_data pca9547_data_mux0 = {
	.modes		= pca954x_modes_mux0,
	.num_modes	= ARRAY_SIZE(pca954x_modes_mux0),
};

static struct i2c_board_info __initdata iv_io_i2c0_boardinfo[] = {
    	{
		I2C_BOARD_INFO("pca9547", 0x77),
		.platform_data = &pca9547_data_mux0,
    	},
};

static struct i2c_board_info __initdata iv_io_i2c4_boardinfo[] = {
	{
		I2C_BOARD_INFO("tca6424", 0x22),
		.irq = IV_GPIO_EXPANDER_1_IRQ_GPIO, // Needs to be converted at runtime.
		.platform_data = &gpio_expander_1,
	},
	{
		I2C_BOARD_INFO("tca6424", 0x23),
		.irq = IV_GPIO_EXPANDER_2_IRQ_GPIO, // Needs to be converted at runtime.
		.platform_data = &gpio_expander_2,
	},
    {
	I2C_BOARD_INFO("m41t62", 0x68),
    },
};

static struct i2c_board_info __initdata iv_io_i2c5_boardinfo[] = {
};
static struct i2c_board_info __initdata iv_io_i2c6_boardinfo[] = {
};
static struct i2c_board_info __initdata iv_io_i2c7_boardinfo[] = {
};
static struct i2c_board_info __initdata iv_io_i2c8_boardinfo[] = {
};
static struct i2c_board_info __initdata iv_io_i2c9_boardinfo[] = {
};

#ifdef CONFIG_WILINK_PLATFORM_DATA

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
	.enabled_at_boot        = 0,
	.init_data              = &iv_io_vmmc2,
};

static struct platform_device iv_io_wlan_regulator = {
	.name           = "reg-fixed-voltage",
	.id             = 1,
	.dev = {
		.platform_data  = &iv_io_vwlan,
	},
};

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
	platform_device_register(&iv_io_wlan_regulator);
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

static void __init iv_io_late_init(struct iv_io_board_info *info) {

	int err = 0;

	err = i2c_register_board_info(4, iv_io_i2c4_boardinfo, 
					ARRAY_SIZE(iv_io_i2c4_boardinfo));		
	if (err)
		printk(KERN_ERR BOARD_NAME ": Can't register I2C bus 4 (err %d)\n", err);

	err = i2c_register_board_info(5, iv_io_i2c5_boardinfo, 
					ARRAY_SIZE(iv_io_i2c5_boardinfo));
	if (err)
		printk(KERN_ERR BOARD_NAME ": Can't register I2C bus 5 (err %d)\n", err);

	err = i2c_register_board_info(6, iv_io_i2c6_boardinfo, 
					ARRAY_SIZE(iv_io_i2c5_boardinfo));
	if (err)
		printk(KERN_ERR BOARD_NAME ": Can't register I2C bus 6 (err %d)\n", err);

	err = i2c_register_board_info(7, iv_io_i2c7_boardinfo, 
					ARRAY_SIZE(iv_io_i2c5_boardinfo));
	if (err)
		printk(KERN_ERR BOARD_NAME ": Can't register I2C bus 7 (err %d)\n", err);

	err = i2c_register_board_info(8, iv_io_i2c8_boardinfo, 
					ARRAY_SIZE(iv_io_i2c5_boardinfo));
	if (err)
		printk(KERN_ERR BOARD_NAME ": Can't register I2C bus 8 (err %d)\n", err);

	err = i2c_register_board_info(9, iv_io_i2c9_boardinfo, 
					ARRAY_SIZE(iv_io_i2c5_boardinfo));
	if (err)
		printk(KERN_ERR BOARD_NAME ": Can't register I2C bus 9 (err %d)\n", err);

	iv_wl12xx_power(true);
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
	.i2c0_boardinfo			= iv_io_i2c0_boardinfo,
	.i2c0_boardinfo_size		= ARRAY_SIZE(iv_io_i2c0_boardinfo),
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
