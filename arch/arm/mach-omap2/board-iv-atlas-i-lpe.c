/*
 * linux/arch/arm/mach-omap2/board-iv-atlas-i-lpe.c
 *
 * Copyright (C) 2010 iVeia, LLC.
 *
 * Modified from mach-omap2/board-omap3evm.c
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
#include <linux/ctype.h>
#include <linux/random.h>

#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/mmc/host.h>

#include <linux/regulator/machine.h>
#include <linux/i2c/twl.h>
#include <linux/i2c/pca953x.h>
#include <linux/usb/otg.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>
#include <linux/mmc/host.h>
#include <linux/spi/spi.h>

#include <mach/hardware.h>
#include <mach/board-iv-atlas-i-lpe.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
#include <video/omapdss.h>
#include <video/omap-panel-generic-dpi.h>
#include "common-board-devices.h"
#else
#include <plat/display.h>
#endif

#include <plat/board.h>
#include <plat/common.h>
#include <plat/gpmc.h>
#include <plat/usb.h>
#include <plat/common.h>
#include <plat/mcspi.h>
#include <plat/omap_device.h>

#include "mux.h"
#include "sdram-micron-mt46h32m32lf-6.h"
#include "hsmmc.h"

#define MODNAME "board-iv-atlas-i-lpe"

#define OPTREX_DISPLAY	"optrex-t55583"

#define SYS_CLKOUT2_RATE 96000000

#define P1_SW_EVENTS_REG 0x10
#define P1_SW_EVENTS_DEVOFF 0x01

#define USB_OTG_VBUS_GPIO 178

#ifdef CONFIG_USB_ANDROID

#include <linux/usb/android_composite.h>

#define GOOGLE_VENDOR_ID		0x18d1
#define GOOGLE_PRODUCT_ID		0x9018
#define GOOGLE_ADB_PRODUCT_ID		0x9015

static char *usb_functions_adb[] = {
	"adb",
};

static char *usb_functions_mass_storage[] = {
	"usb_mass_storage",
};
static char *usb_functions_ums_adb[] = {
	"usb_mass_storage",
	"adb",
};

static char *usb_functions_all[] = {
	"adb", "usb_mass_storage",
};

static struct android_usb_product usb_products[] = {
	{
		.product_id	= GOOGLE_PRODUCT_ID,
		.num_functions	= ARRAY_SIZE(usb_functions_adb),
		.functions	= usb_functions_adb,
	},
	{
		.product_id	= GOOGLE_PRODUCT_ID,
		.num_functions	= ARRAY_SIZE(usb_functions_mass_storage),
		.functions	= usb_functions_mass_storage,
	},
	{
		.product_id	= GOOGLE_PRODUCT_ID,
		.num_functions	= ARRAY_SIZE(usb_functions_ums_adb),
		.functions	= usb_functions_ums_adb,
	},
};

static struct usb_mass_storage_platform_data mass_storage_pdata = {
	.nluns		= 1,
	.vendor		= "rowboat",
	.product	= "rowboat gadget",
	.release	= 0x100,
};

static struct platform_device usb_mass_storage_device = {
	.name	= "usb_mass_storage",
	.id	= -1,
	.dev	= {
		.platform_data = &mass_storage_pdata,
	},
};

static struct android_usb_platform_data android_usb_pdata = {
	.vendor_id	= GOOGLE_VENDOR_ID,
	.product_id	= GOOGLE_PRODUCT_ID,
	.functions	= usb_functions_all,
	.products	= usb_products,
	.num_products	= ARRAY_SIZE(usb_products),
	.version	= 0x0100,
	.product_name	= "rowboat gadget",
	.manufacturer_name	= "rowboat",
	.serial_number	= "20100720",
	.num_functions	= ARRAY_SIZE(usb_functions_all),
};

static struct platform_device androidusb_device = {
	.name	= "android_usb",
	.id	= -1,
	.dev	= {
		.platform_data = &android_usb_pdata,
	},
};

static void omap3evm_android_gadget_init(void)
{
	platform_device_register(&androidusb_device);
}
#endif
#define ATLAS_II_GF_CARRIOR_ORD		"00034"
#define IV_IO_MAX_BOARDS 16
struct iv_io_board_info _iv_io_board_info_default = {};
struct iv_io_board_info * iv_io_board_info_default = &_iv_io_board_info_default;
struct iv_io_board_info * iv_io_board_info;
struct iv_io_board_info * iv_io_board_info_registered[IV_IO_MAX_BOARDS];
int iv_io_register_board_info(struct iv_io_board_info * info)
{
	int i;
	if (! info)
		return -EINVAL;

	for (i = 0; i < IV_IO_MAX_BOARDS; i++) {
		if (! iv_io_board_info_registered[i]) break;
	}
	if (i > IV_IO_MAX_BOARDS)
		return -ENOMEM;

	iv_io_board_info_registered[i] = info;

	return 0;
}
EXPORT_SYMBOL_GPL(iv_io_register_board_info);

static char iv_io[MAX_KERN_CMDLINE_FIELD_LEN];
static int __init iv_io_setup(char *options)
{
	strncpy(iv_io, options, sizeof(iv_io));
	iv_io[sizeof(iv_io) - 1] = '\0';
	return 0;
}
__setup("iv_io=", iv_io_setup);

static char iv_mb[MAX_KERN_CMDLINE_FIELD_LEN];
static int __init iv_mb_setup(char *options)
{
	strncpy(iv_mb, options, sizeof(iv_mb));
	iv_mb[sizeof(iv_mb) - 1] = '\0';
	return 0;
}
__setup("iv_mb=", iv_mb_setup);

static char iv_master[MAX_KERN_CMDLINE_FIELD_LEN];
static int __init iv_master_setup(char *options)
{
	strncpy(iv_master, options, sizeof(iv_master));
	iv_master[sizeof(iv_master) - 1] = '\0';
	return 0;
}
__setup("master=", iv_master_setup);

int is_atlas_master(){
    //return atoi(iv_master);
    return simple_strtol(iv_master,NULL,10);
}

/*
 * iv_board_get_field() - return a static pointer to the given field or subfield.
 *
 * buf must be a buffer of at least MAX_KERN_CMDLINE_FIELD_LEN bytes.
 *
 * Returns field (stored in buf), or NULL on no match.
 */
static char * iv_board_get_field(char * buf, IV_BOARD_CLASS class, IV_BOARD_FIELD field,
					IV_BOARD_SUBFIELD subfield)
{
	char * s = buf;
	char * pfield;
	char * board;

	if (class == IV_BOARD_CLASS_MB)
		board = iv_mb;
	else if (class == IV_BOARD_CLASS_IO)
		board = iv_io;
	else
		return NULL;

	if (!board) {
		s[0] = '\0';
	} else {
		strncpy(s, board, MAX_KERN_CMDLINE_FIELD_LEN);
		s[MAX_KERN_CMDLINE_FIELD_LEN - 1] = '\0';
	}

	if (field == IV_BOARD_FIELD_NONE)
		return s;

	pfield = strsep(&s, ",");
	if (field == IV_BOARD_FIELD_PN) {
		s = pfield;
		if (subfield == IV_BOARD_SUBFIELD_NONE)
			return s;

		pfield = strsep(&s, "-"); // Skip first subfield
		pfield = strsep(&s, "-");
		if (subfield == IV_BOARD_SUBFIELD_PN_ORDINAL)
			return pfield;
		pfield = strsep(&s, "-");
		if (subfield == IV_BOARD_SUBFIELD_PN_VARIANT)
			return pfield;
		pfield = strsep(&s, "-");
		if (subfield == IV_BOARD_SUBFIELD_PN_REVISION)
			return pfield;
	}
	pfield = strsep(&s, ",");
	if (field == IV_BOARD_FIELD_SN)
		return pfield;
	pfield = strsep(&s, ",");
	if (field == IV_BOARD_FIELD_NAME)
		return pfield;

	return NULL;
}


/*
 * iv_board_match() - match board field against string
 *
 * Returns non-zero on match
 */
static int iv_board_match(IV_BOARD_CLASS class, IV_BOARD_FIELD field,
					IV_BOARD_SUBFIELD subfield, char * s)
{
	char buf[MAX_KERN_CMDLINE_FIELD_LEN];
	char * p = iv_board_get_field(buf, class, field, subfield);
	return p && s && (strcmp(p, s) == 0);
}

/*
 * iv_board_ord_match() - match board ordinal against string
 *
 * Returns non-zero on match
 */
static int iv_board_ord_match(char * s)
{
	return iv_board_match(IV_BOARD_CLASS_IO, IV_BOARD_FIELD_PN,
					IV_BOARD_SUBFIELD_PN_ORDINAL, s);
}

/*
 * Return board info struct.  Don't call before board is initialized!
 */
const struct iv_io_board_info * iv_io_get_board_info(void)
{
	BUG_ON(iv_io_board_info == NULL);
	return iv_io_board_info;
}

#define SN_LEN 5

static const char sn_char_map[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

/* iVeia's Organizational Unique Identifier (OUI) */
static const unsigned char iv_oui[3] = {0x00, 0x21, 0x68};

/*
 * Get the base MAC address assigned to the given board class.
 *
 * Each iVeia board has two MAC addresses allocated.  These are
 * based off the board's serial number.  This routine will derive
 * both addresses, and return the 6 byte result in "mac".
 *
 * This function will ALWAYS return an iVeia MAC address.  If the board SN is
 * invalid or not available, this func will use a random iVeia MAC addr, and
 * printk a warning.
 *
 * This function is not reentrant, and because it can return different numbers
 * on each call (if a random MAC is used) it should only be called once per
 * class.
 *
 * Format of returned MAC address data in a buffer is:
 *  MAC address = 00:21:68:AB:CD:EF
 *  mac = {0x00, 0x21, 0x68, 0xAB, 0xCD, 0xEF}
 */
static void get_base_macaddr_call_only_once(char * mac, IV_BOARD_CLASS class)
{
	unsigned long numeric_sn = 0;
	char buf[MAX_KERN_CMDLINE_FIELD_LEN];
	char * alpha_sn;
	char * classname;
	int i;
	
	alpha_sn = iv_board_get_field(buf, class, IV_BOARD_FIELD_SN, IV_BOARD_SUBFIELD_NONE);
	if (!alpha_sn)
		goto process_numeric_sn;

	if (strlen(alpha_sn) != SN_LEN)
		goto process_numeric_sn;

	/*
	 * An SN is a 5 character string that converts to a 25-bit number,
	 * where each char represents 5-bits, according to the sn_char_map.
	 */
	for (i = 0; i < SN_LEN; i++) {
		unsigned long val_5bit;
		char * p;
		p = strchr(sn_char_map, toupper(alpha_sn[i]));
		if (!p) {
			numeric_sn = 0;
			goto process_numeric_sn;
		}
		val_5bit = p - sn_char_map;
		numeric_sn = (numeric_sn << 5) | val_5bit;
	}

process_numeric_sn:
	switch (class){
		case IV_BOARD_CLASS_IO: classname = "IO board"; break;
		case IV_BOARD_CLASS_MB: classname = "mainboard "; break;
		case IV_BOARD_CLASS_BP: classname = "backplane"; break;
		default: classname = "unknown board type"; break;
	}

	if (!numeric_sn) {
		printk(KERN_WARNING MODNAME 
			": Invalid or non-existent %s SN (%s).  Using random MAC addr.\n", 
			classname, alpha_sn);
		numeric_sn = get_random_int();
	}

	numeric_sn <<= 1;
	memcpy(mac, iv_oui, 3);
	mac[3] = (numeric_sn >> 16) & 0xFF;
	mac[4] = (numeric_sn >> 8) & 0xFF;
	mac[5] = (numeric_sn >> 0) & 0xFE;

	printk(KERN_INFO MODNAME 
		": %s SN (%s) is using base MAC addr %02X:%02X:%02X:%02X:%02X:%02X.\n", 
		classname, alpha_sn, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}


static int iv_atlas_i_lpe_pm_power_off(void)
{
	int err;

	/*
	 * Atlas uses 65930's SYSEN output to turn control power supplies.	Tell
	 * 65930 to go to the WAIT-ON state, which will deassert SYSEN.	 Power
	 * button can turn device back on.
	 */
	err = twl_i2c_write_u8(TWL4030_MODULE_PM_MASTER,
				   P1_SW_EVENTS_DEVOFF,
				   P1_SW_EVENTS_REG);

	/*
	 * We should normally not get here.
	 */
	printk(KERN_ERR MODNAME ": Power off failed (err %d).\n", err);
	return 0;
}

static char * get_base_macaddr(IV_BOARD_CLASS class)
{
	static char iv_io_mac[6];
	static char iv_mb_mac[6];
	static char iv_bp_mac[6];
	/* bad_mac, just in case - should never be used */
	static char bad_mac[] = "\x00\x21\x68\xFF\xFF\xFE"; 
	static atomic_t firsttime = ATOMIC_INIT(-1);

	if (atomic_inc_and_test(&firsttime)) {
		get_base_macaddr_call_only_once(iv_io_mac, IV_BOARD_CLASS_IO);
		get_base_macaddr_call_only_once(iv_mb_mac, IV_BOARD_CLASS_MB);
		get_base_macaddr_call_only_once(iv_bp_mac, IV_BOARD_CLASS_BP);
	}

	switch (class){
		case IV_BOARD_CLASS_IO: return iv_io_mac;
		case IV_BOARD_CLASS_MB: return iv_mb_mac;
		case IV_BOARD_CLASS_BP: return iv_bp_mac;
		default: return bad_mac;
	}
}

static int iv_atlas_i_lpe_fpga_init(void)
{
	struct clk * sys_clkout2 = NULL;
	struct clk * clkout2_src_ck = NULL;
	struct clk * cm_96m_fck = NULL;
	int err;

	clkout2_src_ck = clk_get(NULL, "clkout2_src_ck");
	err = PTR_ERR(clkout2_src_ck);
	if (IS_ERR(clkout2_src_ck)) {
		printk(KERN_ERR MODNAME ": Could not get clkout2_src_ck (%d)\n", err);
		goto out;
	}

	sys_clkout2 = clk_get(NULL, "sys_clkout2");
	err = PTR_ERR(sys_clkout2);
	if (IS_ERR(sys_clkout2)) {
		printk(KERN_ERR MODNAME ": Could not get sys_clkout2 (%d)\n", err);
		goto out;
	}

	cm_96m_fck = clk_get(NULL, "cm_96m_fck");
	err = PTR_ERR(cm_96m_fck);
	if (IS_ERR(cm_96m_fck)) {
		printk(KERN_ERR MODNAME ": Could not get cm_96m_fck (%d)\n", err);
		goto out;
	}

	err = clk_set_parent(clkout2_src_ck, cm_96m_fck);
	if (err) {
		printk(KERN_ERR MODNAME ": Could not set parent for sys_clkout2 (%d)\n", err);
		goto out;
	}

	err = clk_set_rate(sys_clkout2, SYS_CLKOUT2_RATE);
	if (err) {
		printk(KERN_ERR MODNAME ": Could not set rate for sys_clkout2 (%d)\n", err);
		goto out;
	}

	err = clk_enable(sys_clkout2);
	if (err) {
		printk(KERN_ERR MODNAME ": Could not enable sys_clkout2 (%d)\n", err);
		goto out;
	}

	return 0;

out:
	if (! IS_ERR(clkout2_src_ck))
		clk_put(clkout2_src_ck);
	if (! IS_ERR(sys_clkout2))
		clk_put(sys_clkout2);
	if (! IS_ERR(cm_96m_fck))
		clk_put(cm_96m_fck);
	return err;
}


/*
 * Enable/Disable LCD functions turn the LCD backlight on/off.
 * Backlight control is dependent on the I/O board.	 The actual LCD
 * on/off control is handled by the panel code for the LCD.
 */
static unsigned int g_lcd_backlight_level = 50;

static void enable_lcd_backlight(int enable) 
{
	static unsigned int last_level = -1;

	if (iv_io_board_info->set_lcd_backlight_level) {
		if (! enable) {
			iv_io_board_info->set_lcd_backlight_level(0);
			last_level = -1;
		} else if (last_level != g_lcd_backlight_level) {
			iv_io_board_info->set_lcd_backlight_level(g_lcd_backlight_level);
			last_level = g_lcd_backlight_level;
		}
	}
}

static int iv_atlas_i_lpe_enable_lcd(struct omap_dss_device *dssdev)
{
	enable_lcd_backlight(1);
	return 0;
}

static void iv_atlas_i_lpe_disable_lcd(struct omap_dss_device *dssdev)
{
	enable_lcd_backlight(0);
}

static struct omap_dss_device iv_atlas_i_lpe_lcd_device = {
	.name			= "lcd",
	// .driver_name specified at runtime
	.type			= OMAP_DISPLAY_TYPE_DPI,
	.phy.dpi.data_lines	= 18,
	.reset_gpio = -EINVAL,
	.platform_enable	= iv_atlas_i_lpe_enable_lcd,
	.platform_disable	= iv_atlas_i_lpe_disable_lcd,
};

static struct omap_dss_device *iv_atlas_i_lpe_dss_devices[] = {
	&iv_atlas_i_lpe_lcd_device,
};

static struct omap_dss_board_info iv_atlas_i_lpe_dss_data = {
	.num_devices	= ARRAY_SIZE(iv_atlas_i_lpe_dss_devices),
	.devices	= iv_atlas_i_lpe_dss_devices,
	.default_device	= &iv_atlas_i_lpe_lcd_device,
};

static struct regulator_consumer_supply iv_atlas_i_lpe_vmmc1_supply = {
	.supply			= "vmmc",
};

static struct regulator_consumer_supply iv_atlas_i_lpe_vmmc2_supply = {
	.supply			= "vmmc",
};

static struct regulator_consumer_supply iv_atlas_i_lpe_vsim_supply = {
	.supply			= "vmmc_aux",
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
static struct regulator_consumer_supply iv_atlas_i_lpe_vdac_supply =
	REGULATOR_SUPPLY("vdda_dac", "omapdss_venc");

static struct regulator_consumer_supply iv_atlas_i_lpe_vpll2_supplies[] = {
	REGULATOR_SUPPLY("vdds_dsi", "omapdss"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dsi1"),
};

static struct regulator_init_data iv_atlas_i_lpe_vpll2 = {
	.constraints = {
		.name			= "VDVI",
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(iv_atlas_i_lpe_vpll2_supplies),
	.consumer_supplies	= iv_atlas_i_lpe_vpll2_supplies,

};

#else
static struct platform_device iv_atlas_i_lpe_dss_device = {
	.name          = "omapdss",
	.id            = -1,
	.dev            = {
		.platform_data = &iv_atlas_i_lpe_dss_data,
	},
};

static struct regulator_consumer_supply iv_atlas_i_lpe_vdac_supply =
	REGULATOR_SUPPLY("vdda_dac", "omapdss");

/* VPLL2 for digital video outputs */
static struct regulator_consumer_supply iv_atlas_i_lpe_vpll2_supply =
	REGULATOR_SUPPLY("vdds_dsi", "omapdss");

static struct regulator_init_data iv_atlas_i_lpe_vpll2 = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &iv_atlas_i_lpe_vpll2_supply,
};
#endif

/* VMMC1 for MMC1 pins CMD, CLK, DAT0..DAT3 (20 mA, plus card == max 220 mA) */
static struct regulator_init_data iv_atlas_i_lpe_vmmc1 = {
	.constraints = {
		.min_uV			= 1850000,
		.max_uV			= 3150000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &iv_atlas_i_lpe_vmmc1_supply,
};

/* VMMC2 for MMC2 pins CMD, CLK, DAT0..DAT3 (max 100 mA) */
static struct regulator_init_data iv_atlas_i_lpe_vmmc2 = {
	.constraints = {
		.min_uV			= 1850000,
		.max_uV			= 3150000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &iv_atlas_i_lpe_vmmc2_supply,
};

/* VSIM for MMC1 pins DAT4..DAT7 (2 mA, plus card == max 50 mA) */
static struct regulator_init_data iv_atlas_i_lpe_vsim = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 3000000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &iv_atlas_i_lpe_vsim_supply,
};

#define MAX_MMC_PORTS 2

static struct omap2_hsmmc_info mmc[MAX_MMC_PORTS + 1] = {
	{
		.mmc		= 1,
		.caps		= MMC_CAP_4_BIT_DATA,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
	},
};

static int iv_atlas_i_lpe_twl_gpio_setup(struct device *dev,
		unsigned gpio, unsigned ngpio)
{
	/*
	 * Init MMCs.  Second MMC is optional.
	 */
	if (iv_io_board_info->mmc2)
		memcpy(&mmc[1], iv_io_board_info->mmc2, sizeof(struct omap2_hsmmc_info));
	else
		memset(&mmc[1], 0, sizeof(mmc[1]));
	omap2_hsmmc_init(mmc);

	/* link regulators to MMC adapters */
	iv_atlas_i_lpe_vmmc1_supply.dev = mmc[0].dev;
	if (iv_io_board_info->mmc2)
		iv_atlas_i_lpe_vmmc2_supply.dev = mmc[1].dev;
	iv_atlas_i_lpe_vsim_supply.dev = mmc[0].dev;

	return 0;
}

static struct twl4030_gpio_platform_data iv_atlas_i_lpe_gpio_data = {
	.gpio_base	= TWL4030_GPIO_BASE,
	.irq_base	= TWL4030_GPIO_IRQ_BASE,
	.irq_end	= TWL4030_GPIO_IRQ_END,
	.setup		= iv_atlas_i_lpe_twl_gpio_setup,
};

static struct twl4030_usb_data iv_atlas_i_lpe_usb_data = {
	.usb_mode	= T2_USB_MODE_ULPI,
};


static struct twl4030_keypad_data iv_atlas_i_lpe_kp_data = {
	.keymap_data	= NULL, // IO Board-specific keymap.  Filled in at run-time.
	.rows		= 4,
	.cols		= 4,
	.rep		= 0,
};

static struct twl4030_codec_audio_data iv_atlas_i_lpe_audio_data;

static struct twl4030_codec_data iv_atlas_i_lpe_codec_data = {
	.audio_mclk = 26000000,
	.audio = &iv_atlas_i_lpe_audio_data,
};

/* VDAC for DSS driving S-Video */
static struct regulator_init_data iv_atlas_i_lpe_vdac = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &iv_atlas_i_lpe_vdac_supply,
};

/*
 * TWL4030 configuration
 */
static struct twl4030_platform_data iv_atlas_i_lpe_twldata = {
	.irq_base	= TWL4030_IRQ_BASE,
	.irq_end	= TWL4030_IRQ_END,

	/* platform_data for children goes here */
	.keypad		= &iv_atlas_i_lpe_kp_data,
	.usb		= &iv_atlas_i_lpe_usb_data,
	.gpio		= &iv_atlas_i_lpe_gpio_data,
	.codec		= &iv_atlas_i_lpe_codec_data,
	.vdac		= &iv_atlas_i_lpe_vdac,
	.vpll2		= &iv_atlas_i_lpe_vpll2,
	.vmmc1		= &iv_atlas_i_lpe_vmmc1,
	.vmmc2		= &iv_atlas_i_lpe_vmmc2,
	.vsim		= &iv_atlas_i_lpe_vsim,
};

static struct i2c_board_info __initdata iv_atlas_i_lpe_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("twl4030", 0x48),
		.flags = I2C_CLIENT_WAKE,
		.irq = INT_34XX_SYS_NIRQ,
		.platform_data = &iv_atlas_i_lpe_twldata,
	},
};

static int __init iv_atlas_i_lpe_i2c_init(void)
{
	struct i2c_board_info * i2c3_boardinfo;
	unsigned int i2c3_boardinfo_size;

	if (iv_io_board_info->keymap_data)
		iv_atlas_i_lpe_twldata.keypad->keymap_data = iv_io_board_info->keymap_data;
	else
		iv_atlas_i_lpe_twldata.keypad = NULL;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
	omap3_pmic_init("twl4030", &iv_atlas_i_lpe_twldata);
#else
	omap_register_i2c_bus(1, 2600, iv_atlas_i_lpe_i2c_boardinfo,
			ARRAY_SIZE(iv_atlas_i_lpe_i2c_boardinfo));
#endif
	omap_register_i2c_bus(2, 100, NULL, 0);

	i2c3_boardinfo = iv_io_board_info->i2c3_boardinfo;
	i2c3_boardinfo_size = iv_io_board_info->i2c3_boardinfo_size;
	if (i2c3_boardinfo && iv_io_board_info->i2c_irq_gpios) {
		int err;
		int i, j;

		/*
		 * Some I2C parts require GPIO IRQs.  Because they are GPIO
		 * IRQs, their IRQ number is not known until runtime via
		 * gpio_to_irq().  
		 *
		 * So, for any I2C parts requiring IRQs, we'll setup and
		 * correct their IRQ number here.
		 */
		for (i = 0; i < iv_io_board_info->i2c_irq_gpios_size; i++) {
			int irq_gpio = iv_io_board_info->i2c_irq_gpios[i];
			for (j = 0; j < i2c3_boardinfo_size; j++) {
				if (i2c3_boardinfo[j].irq != irq_gpio)
					continue;

				err = gpio_request(irq_gpio, NULL);
				if (err == 0) {
					err = gpio_direction_input(irq_gpio);
				}
				if (err < 0) {
					printk(KERN_ERR MODNAME 
						": Can't get GPIO for IRQ\n");
				}

				i2c3_boardinfo[j].irq = gpio_to_irq(irq_gpio);
			}
		}
	}

	if (i2c3_boardinfo) {
		int speed = iv_io_board_info->i2c3_speed ? iv_io_board_info->i2c3_speed : 100;
		omap_register_i2c_bus(3, speed, i2c3_boardinfo, i2c3_boardinfo_size);
	}

	return 0;
}

/*
 * /proc filesystem support
 */
#define MACADDR_INDEX_MASK	0x000000FF
#define MACADDR_0		0x00000000
#define MACADDR_1		0x00000001
#define MACADDR_IV_IO		0x00000100
#define MACADDR_IV_MB		0x00000200
#define MACADDR_IV_BP		0x00000400
#define MACADDR_AS_VAR		0x00010000

#define MACADDR_IV_IO_0		(MACADDR_IV_IO | MACADDR_0)
#define MACADDR_AS_VAR_IV_IO_0	(MACADDR_IV_IO | MACADDR_0 | MACADDR_AS_VAR)
#define MACADDR_IV_IO_1		(MACADDR_IV_IO | MACADDR_1)
#define MACADDR_AS_VAR_IV_IO_1	(MACADDR_IV_IO | MACADDR_1 | MACADDR_AS_VAR)
#define MACADDR_IV_MB_0		(MACADDR_IV_MB | MACADDR_0)
#define MACADDR_AS_VAR_IV_MB_0	(MACADDR_IV_MB | MACADDR_0 | MACADDR_AS_VAR)
#define MACADDR_IV_MB_1		(MACADDR_IV_MB | MACADDR_1)
#define MACADDR_AS_VAR_IV_MB_1	(MACADDR_IV_MB | MACADDR_1 | MACADDR_AS_VAR)
#define MACADDR_IV_BP_0		(MACADDR_IV_BP | MACADDR_0)
#define MACADDR_AS_VAR_IV_BP_0	(MACADDR_IV_BP | MACADDR_0 | MACADDR_AS_VAR)
#define MACADDR_IV_BP_1		(MACADDR_IV_BP | MACADDR_1)
#define MACADDR_AS_VAR_IV_BP_1	(MACADDR_IV_BP | MACADDR_1 | MACADDR_AS_VAR)

#define BOARD_PN		0x00000001
#define BOARD_ORD		0x00000002
#define BOARD_VARIANT		0x00000004
#define BOARD_REV		0x00000008
#define BOARD_NAME		0x00000010
#define BOARD_IV_MB		0x00000020
#define BOARD_IV_IO		0x00000040
#define BOARD_IV_BP		0x00000080

#define PROC_PERM	(S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

#define SENSORS_ENABLED_TAG     0x01
#define SENSORS_WAKE_TAG        0x02
#define SENSORS_DELAY_TAG       0x03

static unsigned int sensors_enabled_data = SENSORS_ENABLED_TAG;
static unsigned int sensors_wake_data = SENSORS_WAKE_TAG;
static unsigned int sensors_delay_data = SENSORS_DELAY_TAG;

/*
 * data backing for /proc/iveia/sensors/ entries
 */
static unsigned int sensor_enable_mask = 0x00;
static unsigned int sensor_wake = 0x00;
static unsigned int sensor_delay = 0x00;

DEFINE_SEMAPHORE(proc_sensors_lock);


static int proc_read_backlight(char *page, char **start,
			off_t off, int count, int *eof, void *data)
{
	return sprintf(page, "%d\n", g_lcd_backlight_level);
}

static int proc_write_backlight(struct file *file, const char __user *buffer,
			unsigned long count, void *data)
{
	char s[16];
	unsigned int ui = 0;

	if (count > sizeof(s) - 1)
		return -EINVAL;
	if (copy_from_user(s, buffer, count))
		return -EFAULT;
	s[count] = '\0';

	sscanf(s, "%u", &ui);
	if (100 < ui) ui = 100;

	g_lcd_backlight_level = ui;
	if (iv_io_board_info->set_lcd_backlight_level)
		iv_io_board_info->set_lcd_backlight_level(ui);  /* 0 - 100 scale */

	return count;
}

static int proc_read_board(char *page, char **start,
			off_t off, int count, int *eof, void *data)
{
	unsigned long flags = (unsigned long) data;
	IV_BOARD_CLASS class = flags & IV_BOARD_CLASS_MASK;
	IV_BOARD_FIELD field = flags & IV_BOARD_FIELD_MASK;
	IV_BOARD_SUBFIELD subfield = flags & IV_BOARD_SUBFIELD_MASK;

	char buf[MAX_KERN_CMDLINE_FIELD_LEN];
	char * p = iv_board_get_field(buf, class, field, subfield);

	return p ? sprintf(page, "%s\n", p) : 0;
}

static int proc_read_macaddr(char *page, char **start,
			off_t off, int count, int *eof, void *data)
{
	unsigned long flags = (unsigned long) data;
	unsigned int last_mac_byte;
	char * mac;
	IV_BOARD_CLASS class;

	if (flags & MACADDR_IV_IO) {
		class = IV_BOARD_CLASS_IO;
	} else if (flags & MACADDR_IV_MB) {
		class = IV_BOARD_CLASS_MB;
	} else if (flags & MACADDR_IV_BP) {
		class = IV_BOARD_CLASS_BP;
	} else {
		printk(KERN_ERR MODNAME ": Invalid iVeia class (flags=%08X)\n",
			(unsigned int) flags);
		class = IV_BOARD_CLASS_NONE;
	}
	mac = get_base_macaddr(class);

	last_mac_byte = mac[5] + ((flags & MACADDR_INDEX_MASK) & 1);

	return sprintf(page, "%s%02X%02X%02X%02X%02X%02X\n", 
		flags & MACADDR_AS_VAR ? "macaddr=" : "",
		mac[0], mac[1], mac[2], mac[3], mac[4], last_mac_byte);
}

static unsigned int *get_sensor_param(unsigned int data, char *op)
{
	switch (data) {
		case SENSORS_ENABLED_TAG:
			return &sensor_enable_mask;
			break;
		case SENSORS_WAKE_TAG:
			return &sensor_wake;
			break;
		case SENSORS_DELAY_TAG:
			return &sensor_delay;
			break;
		default:
			printk(KERN_ERR MODNAME ": undefined proc %s data, 0x%08X\n", op, data);
			return NULL;
			break;
	}
	return NULL;  /* should never get here */
}

static int proc_read_sensor(char *page, char **start,
			off_t off, int count, int *eof, void *data)
{
	unsigned int *sensor_src;
	unsigned int tag = *((unsigned int *)data);
	size_t data_size = sizeof (unsigned int);

	if (down_interruptible(&proc_sensors_lock)) {
		return -ERESTARTSYS;
	}

	sensor_src = get_sensor_param(tag, "read");
	if (sensor_src == NULL) {
		printk(KERN_ERR MODNAME ": couldn't get sensor pointer");
		up(&proc_sensors_lock);
		return -ENODATA;
	}

	memcpy(page, sensor_src, data_size);
	up(&proc_sensors_lock);
	return data_size;
}

static int proc_write_sensor(struct file *file, const char __user *buffer,
			unsigned long count, void *data)
{
	unsigned int *sensor_dest;
	unsigned int tag = *((unsigned int *)data);
	size_t data_size = sizeof (unsigned int);

	if (down_interruptible(&proc_sensors_lock)) {
		return -ERESTARTSYS;
	}

	sensor_dest = get_sensor_param(tag, "write");
	if (sensor_dest == NULL) {
		up(&proc_sensors_lock);
		return -ENODATA;
	}

	memcpy(sensor_dest, buffer, data_size);
	up(&proc_sensors_lock);
	return count;
}

struct proc_func_entry {
	struct proc_func_entry * proc_dir;
	int len;
	char * name;
	read_proc_t * read_proc;
	write_proc_t * write_proc;
	void * data;
};

static struct proc_func_entry proc_macaddr_iv_io_dir[] = {
	{ NULL, 0, "0", proc_read_macaddr, NULL, (void *) MACADDR_IV_IO_0 },
	{ NULL, 0, "0_as_macaddr_var", proc_read_macaddr, NULL, (void *) MACADDR_AS_VAR_IV_IO_0 },
	{ NULL, 0, "1", proc_read_macaddr, NULL, (void *) MACADDR_IV_IO_1 },
	{ NULL, 0, "1_as_macaddr_var", proc_read_macaddr, NULL, (void *) MACADDR_AS_VAR_IV_IO_1 },
};

static struct proc_func_entry proc_macaddr_iv_mb_dir[] = {
	{ NULL, 0, "0", proc_read_macaddr, NULL, (void *) MACADDR_IV_MB_0 },
	{ NULL, 0, "0_as_macaddr_var", proc_read_macaddr, NULL, (void *) MACADDR_AS_VAR_IV_MB_0 },
	{ NULL, 0, "1", proc_read_macaddr, NULL, (void *) MACADDR_IV_MB_1 },
	{ NULL, 0, "1_as_macaddr_var", proc_read_macaddr, NULL, (void *) MACADDR_AS_VAR_IV_MB_1 },
};

static struct proc_func_entry proc_macaddr_iv_bp_dir[] = {
	{ NULL, 0, "0", proc_read_macaddr, NULL, (void *) MACADDR_IV_BP_0 },
	{ NULL, 0, "0_as_macaddr_var", proc_read_macaddr, NULL, (void *) MACADDR_AS_VAR_IV_BP_0 },
	{ NULL, 0, "1", proc_read_macaddr, NULL, (void *) MACADDR_IV_BP_1 },
	{ NULL, 0, "1_as_macaddr_var", proc_read_macaddr, NULL, (void *) MACADDR_AS_VAR_IV_BP_1 },
};

static struct proc_func_entry proc_macaddr_dir[] = {
	{ proc_macaddr_iv_io_dir, ARRAY_SIZE(proc_macaddr_iv_io_dir), "iv_io" },
	{ proc_macaddr_iv_mb_dir, ARRAY_SIZE(proc_macaddr_iv_mb_dir), "iv_mb" },
	{ proc_macaddr_iv_bp_dir, ARRAY_SIZE(proc_macaddr_iv_bp_dir), "iv_bp" },
};

#define INFO(c,f,sf) ((void *) (IV_BOARD_CLASS_##c | IV_BOARD_FIELD_##f | IV_BOARD_SUBFIELD_##sf))
static struct proc_func_entry proc_board_iv_io_dir[] = {
	{ NULL, 0, "pn",	proc_read_board, NULL, INFO(IO, PN, NONE) },
	{ NULL, 0, "ord",	proc_read_board, NULL, INFO(IO, PN, PN_ORDINAL) },
	{ NULL, 0, "variant",	proc_read_board, NULL, INFO(IO, PN, PN_VARIANT) },
	{ NULL, 0, "rev",	proc_read_board, NULL, INFO(IO, PN, PN_REVISION) },
	{ NULL, 0, "sn",	proc_read_board, NULL, INFO(IO, SN, NONE) },
	{ NULL, 0, "name",	proc_read_board, NULL, INFO(IO, NAME, NONE) },
};

static struct proc_func_entry proc_board_iv_mb_dir[] = {
	{ NULL, 0, "pn",	proc_read_board, NULL, INFO(MB, PN, NONE) },
	{ NULL, 0, "ord",	proc_read_board, NULL, INFO(MB, PN, PN_ORDINAL) },
	{ NULL, 0, "variant",	proc_read_board, NULL, INFO(MB, PN, PN_VARIANT) },
	{ NULL, 0, "rev",	proc_read_board, NULL, INFO(MB, PN, PN_REVISION) },
	{ NULL, 0, "sn",	proc_read_board, NULL, INFO(MB, SN, NONE) },
	{ NULL, 0, "name",	proc_read_board, NULL, INFO(MB, NAME, NONE) },
};

static struct proc_func_entry proc_board_iv_bp_dir[] = {
	{ NULL, 0, "pn",	proc_read_board, NULL, INFO(BP, PN, NONE) },
	{ NULL, 0, "ord",	proc_read_board, NULL, INFO(BP, PN, PN_ORDINAL) },
	{ NULL, 0, "variant",	proc_read_board, NULL, INFO(BP, PN, PN_VARIANT) },
	{ NULL, 0, "rev",	proc_read_board, NULL, INFO(BP, PN, PN_REVISION) },
	{ NULL, 0, "sn",	proc_read_board, NULL, INFO(BP, SN, NONE) },
	{ NULL, 0, "name",	proc_read_board, NULL, INFO(BP, NAME, NONE) },
};

static struct proc_func_entry proc_board_dir[] = {
	{ proc_board_iv_io_dir, ARRAY_SIZE(proc_board_iv_io_dir), "iv_io" },
	{ proc_board_iv_mb_dir, ARRAY_SIZE(proc_board_iv_mb_dir), "iv_mb" },
	{ proc_board_iv_bp_dir, ARRAY_SIZE(proc_board_iv_bp_dir), "iv_bp" },
};

static struct proc_func_entry proc_sensors_dir[] = {
	{ NULL, 0, "enabled", proc_read_sensor, proc_write_sensor, &sensors_enabled_data },
	{ NULL, 0, "wake", proc_read_sensor, proc_write_sensor, &sensors_wake_data },
	{ NULL, 0, "delay", proc_read_sensor, proc_write_sensor, &sensors_delay_data },
};

static struct proc_func_entry proc_iveia_dir[] = {
	{ NULL, 0, "backlight", proc_read_backlight, proc_write_backlight, NULL },
	{ proc_sensors_dir, ARRAY_SIZE(proc_sensors_dir), "sensors" },
	{ proc_macaddr_dir, ARRAY_SIZE(proc_macaddr_dir), "macaddr" },
	{ proc_board_dir, ARRAY_SIZE(proc_board_dir), "board" },
};

static struct proc_func_entry proc_root = {
	proc_iveia_dir,	ARRAY_SIZE(proc_iveia_dir), "iveia"
};

/*
 * Recursive function to create a proc entry or dir.  Should be given the proc
 * dir to create the entry in (or NULL if the top (/proc)).  The entry can
 * either be a single proc entry or a dir (that points to an array of
 * proc_func_entrys)
 * 
 * If its a dir, you must define entry's fields proc_dir and len.
 * If its an entry, you must define entry's fields read_proc, write_proc, and data.
 * And, entry->name must always be defined.
 */
static void create_proc_dir_or_entry(
	struct proc_dir_entry * dir, struct proc_func_entry * entry)
{
	int i;

	if (entry->proc_dir) {
		dir = create_proc_entry(entry->name, S_IFDIR, dir);
		if (!dir)
			return;
		for (i = 0; i < entry->len; i++) {
			create_proc_dir_or_entry(
				dir, &entry->proc_dir[i]);
		}

	} else {
		struct proc_dir_entry * p;
		p = create_proc_entry(entry->name, PROC_PERM, dir);
		if (p) {
			p->read_proc = entry->read_proc;
			p->write_proc = entry->write_proc;
			p->data = entry->data;
		} else {
			printk(KERN_ERR MODNAME 
				": Can't create proc entry '%s'\n", 
				entry->name);
		}
	}
}

static void iv_atlas_i_lpe_proc_entries_init(void)
{
	create_proc_dir_or_entry(NULL, &proc_root);

	/*
	 * First time only get_base_macaddr() is called, it gets the MAC
	 * addresses, and does some info printk()s.  Do it now, just so the
	 * printks print now.
	 */
	get_base_macaddr(IV_BOARD_CLASS_NONE);
}

static void __init omap3_iv_atlas_i_lpe_init_early(void)
{
	omap2_init_common_infrastructure();
	omap2_init_common_devices(mt46h32m32lf6_sdrc_params, NULL);
}

static void __init iv_atlas_i_lpe_init_irq(void)
{
	omap3_iv_atlas_i_lpe_init_early();
	omap_init_irq();
}

static struct omap_board_config_kernel iv_atlas_i_lpe_config[] __initdata = {
};

static struct platform_device *iv_atlas_i_lpe_devices[] __initdata = {
#ifdef CONFIG_USB_ANDROID
	&usb_mass_storage_device,
#endif
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
static const struct usbhs_omap_board_data usbhs_bdata __initconst = {

	.port_mode[0] = OMAP_EHCI_PORT_MODE_PHY,
	.port_mode[1] = OMAP_USBHS_PORT_MODE_UNUSED,
	.port_mode[2] = OMAP_USBHS_PORT_MODE_UNUSED,

	.phy_reset	= true,
	/* PHY reset GPIO will be runtime programmed based on EVM version */
	.reset_gpio_port[0]	 = -EINVAL,
	.reset_gpio_port[1]	 = -EINVAL,
	.reset_gpio_port[2]	 = -EINVAL
};
#else
static const struct ehci_hcd_omap_platform_data ehci_pdata __initconst = {

	.port_mode[0] = EHCI_HCD_OMAP_MODE_PHY,
	.port_mode[1] = EHCI_HCD_OMAP_MODE_UNKNOWN,
	.port_mode[2] = EHCI_HCD_OMAP_MODE_UNKNOWN,

	.phy_reset  = true,
	.reset_gpio_port[0]  = -EINVAL,
	.reset_gpio_port[1]  = -EINVAL,
	.reset_gpio_port[2]  = -EINVAL
};
#endif

static struct omap_board_mux board_mux[256] __initdata = {
	OMAP3_MUX(SYS_NIRQ, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP |
				OMAP_PIN_OFF_INPUT_PULLUP | OMAP_PIN_OFF_OUTPUT_LOW |
				OMAP_PIN_OFF_WAKEUPENABLE),
	OMAP3_MUX(SDMMC2_DAT7, OMAP_MUX_MODE4 | OMAP_PIN_INPUT_PULLUP |
				OMAP_WAKEUP_EN | OMAP_PIN_OFF_NONE),
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};

static void iv_atlas_i_lpe_usb_init(void)
{
	int ret = gpio_request(USB_OTG_VBUS_GPIO, NULL);
	if (ret == 0) {
		ret = gpio_direction_output(USB_OTG_VBUS_GPIO, 0);
	}
	if (ret < 0) {
		printk(KERN_ERR MODNAME ": Can't setup USB OTG VBUS GPIO\n");
	}
}

static void iv_atlas_i_lpe_set_vbus(void *musb, int is_on)
{
	gpio_set_value(USB_OTG_VBUS_GPIO, is_on);
}

int musb_board_set_mode(void *musb, u8 musb_mode)
{
	switch (musb_mode) {
		case MUSB_HOST:
			iv_atlas_i_lpe_set_vbus(musb, 1);
			break;
		case MUSB_PERIPHERAL:
		case MUSB_OTG:
			iv_atlas_i_lpe_set_vbus(musb, 0);
			break;
	}
	return 1;
}

static struct omap_musb_board_data musb_board_data = {
	.power			= 500,
	.interface_type		= MUSB_INTERFACE_ULPI,
#if defined(CONFIG_USB_MUSB_OTG)
	.mode		= MUSB_OTG,
#elif defined(CONFIG_USB_MUSB_PERIPHERAL)
	.mode		= MUSB_PERIPHERAL,
#else /* defined(CONFIG_USB_MUSB_HOST) */
	.mode		= MUSB_HOST,
#endif
};

static int iv_atlas_i_lpe_ioboard_init(void)
{
	struct iv_io_board_info * info = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(iv_io_board_info_registered); i++) {
		info = iv_io_board_info_registered[i];
		if (info && iv_board_ord_match(info->board_ord)) {
			printk(KERN_INFO MODNAME ": %s IO board.\n", info->name);
			break;
		}
	}
	if (i >= ARRAY_SIZE(iv_io_board_info_registered)) {
		//_iv_io_board_info_default.board_ord = kmalloc(5);
		_iv_io_board_info_default.board_ord = ATLAS_II_GF_CARRIOR_ORD;
		printk(KERN_INFO MODNAME ": Unrecognized IO board, using defaults.\n");
		iv_io_board_info = iv_io_board_info_default;
	} else {
		iv_io_board_info = info;
	}

	return 0;
}

static void __init iv_atlas_i_lpe_init(void)
{
	iv_atlas_i_lpe_ioboard_init();

	if (iv_io_board_info->power_off)
		pm_power_off = (void *) iv_io_board_info->power_off;
	else
		pm_power_off = (void *) iv_atlas_i_lpe_pm_power_off;

	if (iv_io_board_info->early_init)
		iv_io_board_info->early_init(iv_io_board_info);

	/*
	 * If there's an io board board_mux combine with the main board.
	 */
	if (iv_io_board_info->board_mux) {
		struct omap_board_mux * mainboard_mux = board_mux;
		struct omap_board_mux * mainboard_mux_end = 
					&board_mux[ARRAY_SIZE(board_mux) - 1];
		struct omap_board_mux * ioboard_mux = 
					iv_io_board_info->board_mux;

		while (mainboard_mux->reg_offset != OMAP_MUX_TERMINATOR)
			mainboard_mux++;
		while (ioboard_mux->reg_offset != OMAP_MUX_TERMINATOR) {
			if (mainboard_mux == mainboard_mux_end)
				break;
			*mainboard_mux = *ioboard_mux;
			mainboard_mux++;
			ioboard_mux++;
		}
		if (mainboard_mux == mainboard_mux_end)
			printk(KERN_ERR MODNAME ": Board mux too small to "
				"configure all pins.\n");
		mainboard_mux->reg_offset = OMAP_MUX_TERMINATOR;
	}
	omap3_mux_init(board_mux, OMAP_PACKAGE_CUS);

	omap_board_config = iv_atlas_i_lpe_config;
	omap_board_config_size = ARRAY_SIZE(iv_atlas_i_lpe_config);

	iv_atlas_i_lpe_fpga_init();
	iv_atlas_i_lpe_i2c_init();
	if (ARRAY_SIZE(iv_atlas_i_lpe_devices))
		platform_add_devices(iv_atlas_i_lpe_devices, 
					ARRAY_SIZE(iv_atlas_i_lpe_devices));
	omap_serial_init();

	if (iv_io_board_info->lcd_driver_name) {
		iv_atlas_i_lpe_dss_data.default_device->driver_name = 
			iv_io_board_info->lcd_driver_name;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
		omap_display_init(&iv_atlas_i_lpe_dss_data);
#else
		platform_device_register(&iv_atlas_i_lpe_dss_device);
#endif
	}

	if (iv_io_board_info->platform_devices)
		platform_add_devices(iv_io_board_info->platform_devices,
					 iv_io_board_info->platform_devices_size);

	iv_atlas_i_lpe_proc_entries_init();

	iv_atlas_i_lpe_usb_init();
	usb_musb_init(&musb_board_data);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
	usbhs_init(&usbhs_bdata);
#else
	usb_ehci_init(&ehci_pdata);
#endif

	if (iv_io_board_info->proc_init)
		iv_io_board_info->proc_init(iv_io_board_info);

	if (iv_io_board_info->late_init)
		iv_io_board_info->late_init(iv_io_board_info);
#ifdef CONFIG_USB_ANDROID
	omap3evm_android_gadget_init();
#endif
}

static int __init iv_io_late_initcall(void)
{
	if (iv_board_ord_match(iv_io_board_info->board_ord))
		if (iv_io_board_info->really_late_init)
			iv_io_board_info->really_late_init(iv_io_board_info);
	return 0;
}
late_initcall(iv_io_late_initcall);

MACHINE_START(IV_ATLAS_I_LPE, "iVeia Atlas-I LPe")
	.boot_params	= 0x80000100,
	.reserve	= omap_reserve,
	.map_io		= omap3_map_io,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
	.init_early	= omap3_iv_atlas_i_lpe_init_early,
	.init_irq	= omap_init_irq,
#else
	.init_irq	= iv_atlas_i_lpe_init_irq,
#endif
	.init_machine	= iv_atlas_i_lpe_init,
	.timer		= &omap_timer,
MACHINE_END
