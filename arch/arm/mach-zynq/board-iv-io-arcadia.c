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
#include <linux/input/matrix_keypad.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/version.h>

#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/i2c/iveia_atmel_mxt_ts.h>
//#include <linux/i2c/atmel_mxt_ts.h>
#include <linux/mmc/host.h>

#include <linux/spi/spi.h>
#include <linux/spi/ads7846.h>
#include <linux/i2c/twl.h>
#include <linux/usb/otg.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>
#include <linux/mmc/host.h>
#include <linux/power_supply.h>
#include <linux/ltc2942_battery.h>

#include <linux/of_platform.h>

#ifdef CONFIG_WILINK_PLATFORM_DATA
#include <linux/wl12xx.h>
#include <linux/regulator/fixed.h>
#include <linux/skbuff.h>
#include <linux/ti_wilink_st.h>
#endif

#include "board-iv-atlas-i-z7e.h"
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#define BOARD_NAME	"Arcadia"
#define BOARD_ORD	"00053"

#include "common.h"

extern struct platform_device *get_first_mmc(void);

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
#define IV_TOUCHSCREEN_IRQ 55
#define IV_GPIO_EXPANDER_1_IRQ_GPIO 28	// Unused GPIO
#define IV_GPIO_EXPANDER_2_IRQ_GPIO 27	// Unused GPIO
static int __initdata iv_io_i2c_irq_gpios[] = {
//	IV_TOUCHSCREEN_IRQ_GPIO,
	IV_GPIO_EXPANDER_1_IRQ_GPIO,
	IV_GPIO_EXPANDER_2_IRQ_GPIO,
};

/*
 * GPIOs
 */
#define GPIO_EXPANDER_BASE_1	(200)
#define GPIO_EXPANDER_SIZE_1	24
#define GPIO_EXPANDER_BASE_2	(GPIO_EXPANDER_BASE_1 + GPIO_EXPANDER_SIZE_1)
#define GPIO_EXPANDER_SIZE_2	24

#define GPIO_USB_PWR		(GPIO_EXPANDER_BASE_1 + 0)
#define GPIO_PWR_INT_N		(GPIO_EXPANDER_BASE_1 + 1)
#define GPIO_JACK_PWR_N		(GPIO_EXPANDER_BASE_1 + 2)
#define GPIO_CHRG_AL_N		(GPIO_EXPANDER_BASE_1 + 3)
#define GPIO_CHRG_N		(GPIO_EXPANDER_BASE_1 + 4)
#define GPIO_FAULT_N		(GPIO_EXPANDER_BASE_1 + 5)
#define GPIO_BAT_LOW_N		(GPIO_EXPANDER_BASE_1 + 6)
#define GPIO_RTC_IRQ_N		(GPIO_EXPANDER_BASE_1 + 7)
#define GPIO_USBA_SW_FAULT_N	(GPIO_EXPANDER_BASE_1 + 8)
#define GPIO_USBB_SW_FAULT_N	(GPIO_EXPANDER_BASE_1 + 9)
#define GPIO_BT_HOST_WAKE	(GPIO_EXPANDER_BASE_1 + 10)
#define GPIO_GPS_GPIO3		(GPIO_EXPANDER_BASE_1 + 11)
#define GPIO_LIGHT_INT_N	(GPIO_EXPANDER_BASE_1 + 12)
#define GPIO_HP_DETECT_N	(GPIO_EXPANDER_BASE_1 + 13)
#define GPIO_EXP_I2C_INT_N	(GPIO_EXPANDER_BASE_1 + 14)
#define GPIO_NC_1_15		(GPIO_EXPANDER_BASE_1 + 15)
#define GPIO_DRDY_M		(GPIO_EXPANDER_BASE_1 + 16)
#define GPIO_INERTIAL_INT2_N	(GPIO_EXPANDER_BASE_1 + 17)
#define GPIO_INERTIAL_INT1_N	(GPIO_EXPANDER_BASE_1 + 18)
#define GPIO_NC_1_19		(GPIO_EXPANDER_BASE_1 + 19)
#define GPIO_NC_1_20		(GPIO_EXPANDER_BASE_1 + 20)
#define GPIO_NC_1_21		(GPIO_EXPANDER_BASE_1 + 21)
#define GPIO_NC_1_22		(GPIO_EXPANDER_BASE_1 + 22)
#define GPIO_NC_1_23		(GPIO_EXPANDER_BASE_1 + 23)

#define GPIO_WDE		(GPIO_EXPANDER_BASE_2 + 0)
#define GPIO_SW_RESET_N		(GPIO_EXPANDER_BASE_2 + 1)
#define GPIO_PWR_KILL		(GPIO_EXPANDER_BASE_2 + 2)
#define GPIO_SW_ILIM0		(GPIO_EXPANDER_BASE_2 + 3)
#define GPIO_SW_ILIM1		(GPIO_EXPANDER_BASE_2 + 4)
#define GPIO_VBUS_1_5A_EN	(GPIO_EXPANDER_BASE_2 + 5)
#define GPIO_5V_EN		(GPIO_EXPANDER_BASE_2 + 6)
#define GPIO_EE_WE_N		(GPIO_EXPANDER_BASE_2 + 7)
#define GPIO_USBA_SW_EN		(GPIO_EXPANDER_BASE_2 + 8)
#define GPIO_USBB_SW_EN		(GPIO_EXPANDER_BASE_2 + 9)
#define GPIO_WL_EN		(GPIO_EXPANDER_BASE_2 + 10)
#define GPIO_BT_EN		(GPIO_EXPANDER_BASE_2 + 11)
#define GPIO_BT_WAKE_UP		(GPIO_EXPANDER_BASE_2 + 12)
#define GPIO_GPS_ON_OFF		(GPIO_EXPANDER_BASE_2 + 13)
#define GPIO_AMP_GAIN_LOW	(GPIO_EXPANDER_BASE_2 + 14)
#define GPIO_AMP_GAIN_HIGH_N	(GPIO_EXPANDER_BASE_2 + 15)
#define GPIO_AMP_EN		(GPIO_EXPANDER_BASE_2 + 16)
#define GPIO_SD2_PWR_N		(GPIO_EXPANDER_BASE_2 + 17)
#define GPIO_LCD_SC		(GPIO_EXPANDER_BASE_2 + 18)
#define GPIO_LCD_DISP		(GPIO_EXPANDER_BASE_2 + 19)
#define GPIO_LCD_BL_OFF_N	(GPIO_EXPANDER_BASE_2 + 20)
#define GPIO_LDAC_N		(GPIO_EXPANDER_BASE_2 + 21)
#define GPIO_NC_2_22		(GPIO_EXPANDER_BASE_2 + 22)
#define GPIO_NC_2_23		(GPIO_EXPANDER_BASE_2 + 23)

static int board_keymap_arcadia[] = {
	KEY(0, 0, KEY_BACK),		// S3
	KEY(1, 0, KEY_MENU),		// S4
	KEY(2, 0, KEY_HOMEPAGE),	// S5
	KEY(3, 0, KEY_SEARCH),		// S6
};

static struct matrix_keymap_data iv_io_keymap_data = {
	.keymap				   = board_keymap_arcadia,
	.keymap_size		   = ARRAY_SIZE(board_keymap_arcadia),
};

static struct mxt_platform_data mxt_info = {
	.irqflags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
};

static int ac_online(void)
{
	return ! gpio_get_value_cansleep(GPIO_JACK_PWR_N);
}

static int usb_online(void)
{
	return gpio_get_value_cansleep(GPIO_USB_PWR);
}

static int battery_status(void)
{
	return gpio_get_value_cansleep(GPIO_CHRG_N) ? 
		POWER_SUPPLY_STATUS_NOT_CHARGING : POWER_SUPPLY_STATUS_CHARGING;
}

static int battery_present(void)
{
	/* Assume always present */
	return 1;
}

static int battery_health(void)
{
	/* Assume always good.  TBD: check for fault */
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

/*
 * Arcadia I2C bus
 *
 * 0x19 = Accelerometer
 * 0x1E = Magnetometer
 * 0x20 = GPIO chip #1
 * 0x21 = GPIO chip #2
 * 0x44 = Ambient light sensor
 * 0x4A = Touchscreen controller (one address bootstrapped, not both)
 * 0x4B = Touchscreen controller (one address bootstrapped, not both)
 * 0x53 = 32Kbit EEPROM
 * 0x64 = Battery gas gauge
 * 0x68 = Real-time clock
 * 0x72 = Quad DAC (bootstrapped address)
 * 0x73 = Quad DAC (global address)
 */
static struct pca953x_platform_data gpio_expander_1 = {
	.gpio_base	= GPIO_EXPANDER_BASE_1,
};
static struct pca953x_platform_data gpio_expander_2 = {
	.gpio_base	= GPIO_EXPANDER_BASE_2,
};
static struct i2c_board_info __initdata iv_io_i2c0_boardinfo[] = {
	{
		I2C_BOARD_INFO("tca6424", 0x22),
	//	.irq = IV_GPIO_EXPANDER_1_IRQ_GPIO, // Needs to be converted at runtime.
		.platform_data = &gpio_expander_1,
	},
	{
		I2C_BOARD_INFO("tca6424", 0x23),
	//	.irq = IV_GPIO_EXPANDER_2_IRQ_GPIO, // Needs to be converted at runtime.
		.platform_data = &gpio_expander_2,
	},
/*	{
		I2C_BOARD_INFO("atmel_mxt_ts", 0x4B),
		.irq = IV_TOUCHSCREEN_IRQ, // Needs to be converted at runtime.
		.platform_data = &mxt_info,
	},
*/	{
		I2C_BOARD_INFO("ltc2635-lz8", 0x73),
	},
	{
		I2C_BOARD_INFO("ltc2942", 0x64),
		.platform_data = &ltc2942_info,
	},
	{
		I2C_BOARD_INFO("m41t62", 0x68),
	},
};
/*
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
	*/ /* 
	level = (level * level)/100;
	if (level > 100)
		level = LCD_BL_I2C_DAC_MAX_VAL;
	else if (level > 0) {
		level = (LCD_BL_I2C_DAC_MAX_VAL * level) / 100;
		if (level < LCD_BL_I2C_DAC_MIN_VAL)
			level = LCD_BL_I2C_DAC_MIN_VAL;
	}

	/* Write all DAC outputs */ /*
	ltc2635_write_word(0x3F, level);
}
*/
static void iv_io_set_lcd_backlight_level(unsigned int level)
{
//	iv_io_set_lcd_backlight_level_i2c_dac(level);
}

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

#ifdef CONFIG_WILINK_PLATFORM_DATA

#define WLAN_PMENA_GPIO       (GPIO_WL_EN)
#define WLAN_IRQ         (89)

static struct regulator_consumer_supply iv_io_vmmc2_supply =
	REGULATOR_SUPPLY("vmmc", "sdhci-zynq.0");

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
//        .init_gpio_late		= 1,
	.init_data              = &iv_io_vmmc2,
};

static struct platform_device iv_io_wlan_regulator = {
	.name           = "reg-fixed-voltage",
	.id             = 1,
	.dev = {
		.platform_data  = &iv_io_vwlan,
	},
};
/*
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
*/
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

static void __init iv_io_late_init(struct iv_io_board_info * info)
{
	i2c_register_board_info(0, iv_io_i2c0_boardinfo, ARRAY_SIZE(iv_io_i2c0_boardinfo));
}

static void __init iv_io_really_late_init(struct iv_io_board_info * info)
{
	int ret = 0;

	/*
	 * Enable required outputs.
	 *
	 * Turn on 1.5A enable, and leave everything off.
	 */
	iv_io_set_lcd_backlight_level(0);
	if (ret >= 0)
		ret = gpio_out(GPIO_VBUS_1_5A_EN, 1);
	if (ret >= 0)
		ret = gpio_out(GPIO_LCD_BL_OFF_N, 0);
	if (ret >= 0)
		ret = gpio_out(GPIO_LDAC_N, 1);
	if (ret >= 0)
		ret = gpio_out(GPIO_LCD_DISP, 0);

	/*
	 * Flip LCD 180 deg
	 
	if (ret >= 0)
		ret = gpio_out(GPIO_LCD_SC, 0);
	*/
	/*
	 * Bring up LCD and WLAN slowly.  Inrush current may be too great if
	 * everything comes up at once.
	 */
	if (ret >= 0) {
		gpio_set_value_cansleep(GPIO_LCD_BL_OFF_N, 1);
		msleep(10);
		gpio_set_value_cansleep(GPIO_LDAC_N, 0);
		msleep(10);
		gpio_set_value_cansleep(GPIO_LCD_DISP, 1);
		msleep(10);
		iv_io_set_lcd_backlight_level(100);
	}

	/*
	 * Enable battery monitoring.
	 */
	if (ret >= 0)
		ret = gpio_in(GPIO_USB_PWR);
	if (ret >= 0)
		ret = gpio_in(GPIO_JACK_PWR_N);

	if (ret) {
		printk(KERN_ERR BOARD_NAME ": Can't setup some HW (err %d)\n", ret);
	}
	
	
	wifi_late_init();

}

static int iv_io_power_off(void)
{
	int err;
	int gpio = GPIO_PWR_KILL;

	err = gpio_request(gpio, NULL);
	if (err == 0)
		err = gpio_direction_output(gpio, 1);

	/*
	 * We should normally not get here.
	 */
	printk(KERN_ERR BOARD_NAME ": Power off failed (err %d).\n", err);

	return 0;
}

static struct iv_io_board_info iv_io_board_info = {
	.name				= BOARD_NAME,
	.board_ord			= BOARD_ORD,
	.late_init			= iv_io_late_init,
//	.mmc2				= iv_io_mmc2_wifi,
//	.board_mux			= iv_io_board_mux,
	.really_late_init		= iv_io_really_late_init,
//	.lcd_driver_name		= OPTREX_DISPLAY,
	.set_lcd_backlight_level	= iv_io_set_lcd_backlight_level,
	.power_off			= iv_io_power_off,
	.keymap_data			= &iv_io_keymap_data,
//	.i2c0_boardinfo			= iv_io_i2c0_boardinfo,
//	.i2c0_boardinfo_size		= ARRAY_SIZE(iv_io_i2c0_boardinfo),
//	.i2c_irq_gpios			= iv_io_i2c_irq_gpios,
//	.i2c_irq_gpios_size		= ARRAY_SIZE(iv_io_i2c_irq_gpios),
	.platform_devices		= iv_io_platform_devices,
	.platform_devices_size		= ARRAY_SIZE(iv_io_platform_devices),
/*	.spk_amp_en_gpio		= GPIO_AMP_EN,
	.spk_amp_low_gpio		= GPIO_AMP_GAIN_LOW,
	.spk_amp_high_gpio		= GPIO_AMP_GAIN_HIGH_N,
	.headset_jack_gpio		= GPIO_HP_DETECT_N,
	.has_spk_with_2bit_gain		= 1,
	.has_mic3			= 1,
	.has_headset			= 1,
*/};

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
