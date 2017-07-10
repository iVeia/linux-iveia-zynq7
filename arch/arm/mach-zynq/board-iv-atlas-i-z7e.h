/*
 *	board-iv-atlas-i-z7e.h
 *
 *	Copyright (C) 2010-2012 iVeia, LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _BOARD_IV_ATLAS_I_Z7E_
#define _BOARD_IV_ATLAS_I_Z7E_

#include <linux/kernel.h>
#include <linux/i2c/twl.h>
#include <linux/gpio.h>

/*
 * Board enums - ensure that they can be or'd together.
 */
typedef unsigned long IV_BOARD_CLASS;
#define IV_BOARD_CLASS_MASK	0x000000FF
#define IV_BOARD_CLASS_NONE	0x00000001
#define IV_BOARD_CLASS_MB	0x00000002
#define IV_BOARD_CLASS_IO	0x00000003
#define IV_BOARD_CLASS_BP	0x00000004


typedef unsigned long IV_BOARD_FIELD;
#define IV_BOARD_FIELD_MASK	0x0000FF00
#define IV_BOARD_FIELD_NONE	0x00000100
#define IV_BOARD_FIELD_PN	0x00000200
#define IV_BOARD_FIELD_SN	0x00000300
#define IV_BOARD_FIELD_NAME	0x00000400

typedef unsigned long IV_BOARD_SUBFIELD;
#define IV_BOARD_SUBFIELD_MASK		0x00FF0000
#define IV_BOARD_SUBFIELD_NONE		0x00010000
#define IV_BOARD_SUBFIELD_PN_ORDINAL	0x00020000
#define IV_BOARD_SUBFIELD_PN_REVISION	0x00030000
#define IV_BOARD_SUBFIELD_PN_VARIANT	0x00040000

struct iv_io_board_info;

struct iv_io_board_info {
	char * name;
	char * board_ord;

	//
	// IO board specific init callbacks.
	//	early_init is called at very beginning of atlas-i-z7e's init_machine()
	//	late_init is called at very end of atlas-i-z7e's init_machine()
	//	really_late_init is called from late_initcall() context.
	//
	void (*early_init)(struct iv_io_board_info *);
	void (*proc_init)(struct iv_io_board_info *);
	void (*late_init)(struct iv_io_board_info *);
	void (*really_late_init)(struct iv_io_board_info *);

	char * lcd_driver_name;
	void (*set_lcd_backlight_level)(unsigned int level);
	int (*power_off)(void);
//	struct omap2_hsmmc_info * mmc2;
	struct matrix_keymap_data * keymap_data;
	struct i2c_board_info * i2c0_boardinfo;
	int i2c0_boardinfo_size;
	int i2c0_speed;
	int * i2c_irq_gpios;
	int i2c_irq_gpios_size;
	struct platform_device ** platform_devices;
	int platform_devices_size;
//	struct omap_board_mux * board_mux;
	int spk_amp_en_gpio;
	int spk_amp_low_gpio;
	int spk_amp_high_gpio;
	int headset_jack_gpio;
	int has_spk;
	int has_spk_with_2bit_gain;
	int has_mic1;
	int has_mic2;
	int has_mic3;
	int has_passthru_mic;
	int has_headset;
};


extern int iv_io_register_board_info(struct iv_io_board_info * info);
extern const struct iv_io_board_info * iv_io_get_board_info(void);

//int is_atlas_master();

#define MAX_KERN_CMDLINE_FIELD_LEN 64
#define DEFAULT_IPMI_STRING "none,none,Unknown"
#define IV_MB_STRING "iv_mb"
#define IV_IO_STRING "iv_io"

#define TWL4030_GPIO_BASE					(OMAP_MAX_GPIO_LINES)

#endif
