/*
 * Driver for MT9P111 CMOS Image Sensor from Micron
 *
 * Copyright (C) iVeia, LLC
 * Author: Brian Silverman <bsilverman@iveia.com>
 *
 * Based on MT9V113 sensor driver
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/v4l2-mediabus.h>

#include <media/v4l2-chip-ident.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/mt9p111.h>

#include "mt9p111_regs.h"

/* Macro's */
#define I2C_RETRY_COUNT                 (5)

#define MT9P111_DEF_WIDTH		640
#define MT9P111_DEF_HEIGHT		480

/* Debug functions */
static int debug = 1;
module_param(debug, bool, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-1)");

/*
 * struct mt9p111 - sensor object
 * @subdev: v4l2_subdev associated data
 * @pad: media entity associated data
 * @format: associated media bus format
 * @rect: configured resolution/window
 * @pdata: Board specific
 * @ver: Chip version
 * @power: Current power state (0: off, 1: on)
 */
struct mt9p111 {
	struct v4l2_subdev subdev;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;
	struct v4l2_rect rect;

	const struct mt9p111_platform_data *pdata;
	unsigned int ver;
	bool power;
};

#define to_mt9p111(sd)	container_of(sd, struct mt9p111, subdev)
/*
 * struct mt9p111_fmt -
 * @mbus_code: associated media bus code
 * @fmt: format descriptor
 */
struct mt9p111_fmt {
	unsigned int mbus_code;
	struct v4l2_fmtdesc fmt;
};
/*
 * List of image formats supported by mt9p111
 * Currently we are using 8 bit and 8x2 bit mode only, but can be
 * extended to 10 bit mode.
 */
static const struct mt9p111_fmt mt9p111_fmt_list[] = {
	{
		.mbus_code = V4L2_MBUS_FMT_UYVY8_2X8,
		.fmt = {
			.index = 0,
			.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
			.flags = 0,
			.description = "8-bit UYVY 4:2:2 Format",
			.pixelformat = V4L2_PIX_FMT_UYVY,
		},
	},
};

/* MT9P111 default register values */
/*
 * This sequence was extracted on the physical I2C bus on a Leopard Board 368
 * (LI-TB02) via an adapter, to a Leopard Imaging Aptina MT9P111 AF HD IP CAMERA
 * BOARD (LI-CAM-SOC5140AF).  The Linux and application software was provided 
 * binary only, without driver source.
 * 
 * Note: All of the registers are annotated with their register name, per the
 * Aptina datasheet.  Some of the registers, as configured by the LI software,
 * are undocumented in the Aptina datasheet.  Those register names are left
 * blank.
 */
static struct mt9p111_reg mt9p111_reg_list[] = {
	{TOK_WRITE16, 0x0010, 0x0336},	// pll_dividers
	{TOK_WRITE16, 0x0012, 0x0070},	// pll_p_dividers
	{TOK_WRITE16, 0x0014, 0x2025},	// pll_control
	{TOK_WRITE16, 0x0022, 0x0020},	// vdd_dis_counter
	{TOK_WRITE16, 0x002A, 0x7FFF},	// pll_p4_p5_p6_dividers
	{TOK_WRITE16, 0x002C, 0x0000},	// pll_p7_divider
	{TOK_WRITE16, 0x002E, 0x0000},	// sensor_clock_divider
	{TOK_WRITE16, 0x001E, 0x0444},	// pad_slew_pad_config
	{TOK_WRITE16, 0x0018, 0x400C},	// standby_control_and_status
	{TOK_DELAY, 0, 10},
	{TOK_WRITE16, 0x098E, 0x6004},	// logical_address_access
	{TOK_WRITE16, 0xE004, 0x0600},	// io_i2c_clk_divider
	{TOK_WRITE16, 0xE002, 0x0108},
	{TOK_WRITE16, 0x0016, 0x0057},	// clocks_control
	{TOK_WRITE16, 0x3B00, 0x80A0},	// txbuffer_data_register_0
	{TOK_WRITE16, 0x3B02, 0x0000},	// txbuffer_data_register_1
	{TOK_WRITE16, 0x3B86, 0x0002},	// txbuffer_total_byte_count
	{TOK_WRITE16, 0x3B82, 0x0007},	// i2c_master_control
	{TOK_DELAY, 0, 100},
	{TOK_WRITE16, 0x0016, 0x0047},	// clocks_control
	{TOK_WRITE16, 0xC83A, 0x000C},	// cam_core_a_y_addr_start
	{TOK_WRITE16, 0xC83C, 0x0018},	// cam_core_a_x_addr_start
	{TOK_WRITE16, 0xC83E, 0x07B1},	// cam_core_a_y_addr_end
	{TOK_WRITE16, 0xC840, 0x0A45},	// cam_core_a_x_addr_end
	{TOK_WRITE16, 0xC842, 0x0001},	// cam_core_a_row_speed
	{TOK_WRITE16, 0xC844, 0x0103},	// cam_core_a_skip_x_core
	{TOK_WRITE16, 0xC846, 0x0103},	// cam_core_a_skip_y_core
	{TOK_WRITE16, 0xC848, 0x0103},	// cam_core_a_skip_x_pipe
	{TOK_WRITE16, 0xC84A, 0x0103},	// cam_core_a_skip_y_pipe
	{TOK_WRITE16, 0xC84C, 0x00F6},	// cam_core_a_power_mode
	{TOK_WRITE16, 0xC84E, 0x0001},	// cam_core_a_bin_mode
	{TOK_WRITE8, 0xC850, 0x0000},	// cam_core_a_orientation
	{TOK_WRITE8, 0xC851, 0x0000},	// cam_core_a_pixel_order
	{TOK_WRITE16, 0xC852, 0x019C},	// cam_core_a_fine_correction
	{TOK_WRITE16, 0xC854, 0x0732},	// cam_core_a_fine_itmin
	{TOK_WRITE16, 0xC858, 0x0000},	// cam_core_a_coarse_itmin
	{TOK_WRITE16, 0xC85A, 0x0001},	// cam_core_a_coarse_itmax_margin
	{TOK_WRITE16, 0xC85C, 0x0423},	// cam_core_a_min_frame_length_lines
	{TOK_WRITE16, 0xC85E, 0xFFFF},	// cam_core_a_max_frame_length_lines
	{TOK_WRITE16, 0xC860, 0x0423},	// cam_core_a_base_frame_length_lines
	{TOK_WRITE16, 0xC862, 0x0F06},	// cam_core_a_min_line_length_pclk
	{TOK_WRITE16, 0xC864, 0xFFFE},	// cam_core_a_max_line_length_pclk
	{TOK_WRITE16, 0xC866, 0x7F7C},	// cam_core_a_p4_5_6_divider
	{TOK_WRITE16, 0xC868, 0x0423},	// cam_core_a_frame_length_lines
	{TOK_WRITE16, 0xC86A, 0x0F06},	// cam_core_a_line_length_pck
	{TOK_WRITE16, 0xC86C, 0x0518},	// cam_core_a_output_size_width
	{TOK_WRITE16, 0xC86E, 0x03D4},	// cam_core_a_output_size_height
	{TOK_WRITE16, 0xC870, 0x0014},	// cam_core_a_rx_fifo_trigger_mark
	{TOK_WRITE16, 0xC858, 0x0002},	// cam_core_a_coarse_itmin
	{TOK_WRITE16, 0xC8B8, 0x0004},	// cam_output_0_jpeg_control
	{TOK_WRITE16, 0xC8AE, 0x0001},	// cam_output_0_output_format
	{TOK_WRITE16, 0xC8AA, 0x0280},	// cam_output_0_image_width
	{TOK_WRITE16, 0xC8AC, 0x01E0},	// cam_output_0_image_height
	{TOK_WRITE16, 0xC872, 0x0010},	// cam_core_b_y_addr_start
	{TOK_WRITE16, 0xC874, 0x001C},	// cam_core_b_x_addr_start
	{TOK_WRITE16, 0xC876, 0x07AF},	// cam_core_b_y_addr_end
	{TOK_WRITE16, 0xC878, 0x0A43},	// cam_core_b_x_addr_end
	{TOK_WRITE16, 0xC87A, 0x0001},	// cam_core_b_row_speed
	{TOK_WRITE16, 0xC87C, 0x0101},	// cam_core_b_skip_x_core
	{TOK_WRITE16, 0xC87E, 0x0101},	// cam_core_b_skip_y_core
	{TOK_WRITE16, 0xC880, 0x0101},	// cam_core_b_skip_x_pipe
	{TOK_WRITE16, 0xC882, 0x0101},	// cam_core_b_skip_y_pipe
	{TOK_WRITE16, 0xC884, 0x00F2},	// cam_core_b_power_mode
	{TOK_WRITE16, 0xC886, 0x0000},	// cam_core_b_bin_mode
	{TOK_WRITE8, 0xC888, 0x0000},	// cam_core_b_orientation
	{TOK_WRITE8, 0xC889, 0x0000},	// cam_core_b_pixel_order
	{TOK_WRITE16, 0xC88A, 0x009C},	// cam_core_b_fine_correction
	{TOK_WRITE16, 0xC88C, 0x034A},	// cam_core_b_fine_itmin
	{TOK_WRITE16, 0xC890, 0x0000},	// cam_core_b_coarse_itmin
	{TOK_WRITE16, 0xC892, 0x0001},	// cam_core_b_coarse_itmax_margin
	{TOK_WRITE16, 0xC894, 0x082F},	// cam_core_b_min_frame_length_lines
	{TOK_WRITE16, 0xC896, 0xFFFF},	// cam_core_b_max_frame_length_lines
	{TOK_WRITE16, 0xC898, 0x082F},	// cam_core_b_base_frame_length_lines
	{TOK_WRITE16, 0xC89A, 0x10CC},	// cam_core_b_min_line_length_pclk
	{TOK_WRITE16, 0xC89C, 0xFFFE},	// cam_core_b_max_line_length_pclk
	{TOK_WRITE16, 0xC89E, 0x7F7C},	// cam_core_b_p4_5_6_divider
	{TOK_WRITE16, 0xC8A0, 0x082F},	// cam_core_b_frame_length_lines
	{TOK_WRITE16, 0xC8A2, 0x10CC},	// cam_core_b_line_length_pck
	{TOK_WRITE16, 0xC8A4, 0x0A28},	// cam_core_b_output_size_width
	{TOK_WRITE16, 0xC8A6, 0x07A0},	// cam_core_b_output_size_height
	{TOK_WRITE16, 0xC8A8, 0x0014},	// cam_core_b_rx_fifo_trigger_mark
	{TOK_WRITE16, 0xC890, 0x0002},	// cam_core_b_coarse_itmin
	{TOK_WRITE16, 0xC8CE, 0x0015},	// cam_output_1_jpeg_control
	{TOK_WRITE16, 0xC8C4, 0x0001},	// cam_output_1_output_format
	{TOK_WRITE16, 0xC8C0, 0x0A20},	// cam_output_1_image_width
	{TOK_WRITE16, 0xC8C2, 0x0798},	// cam_output_1_image_height
	{TOK_WRITE8, 0xA00E, 0x0032},	// fd_max_num_autocor_func_values_to_check
	{TOK_WRITE16, 0xA018, 0x0133},	// fd_expected50hz_flicker_period_in_context_a
	{TOK_WRITE16, 0xA01A, 0x0113},	// fd_expected50hz_flicker_period_in_context_b
	{TOK_WRITE16, 0xA01C, 0x00FF},	// fd_expected60hz_flicker_period_in_context_a
	{TOK_WRITE16, 0xA01E, 0x00E4},	// fd_expected60hz_flicker_period_in_context_b
	{TOK_WRITE16, 0xA010, 0x0129},	// fd_min_expected50hz_flicker_period
	{TOK_WRITE16, 0xA012, 0x013D},	// fd_max_expected50hz_flicker_period
	{TOK_WRITE16, 0xA014, 0x00F5},	// fd_min_expected60hz_flicker_period
	{TOK_WRITE16, 0xA016, 0x0109},	// fd_max_expected60hz_flicker_period
	{TOK_WRITE8, 0xA000, 0x0010},	// fd_status
	{TOK_WRITE8, 0x8417, 0x0004},	// seq_state_cfg_1_fd
	{TOK_DELAY, 0, 10},
	{TOK_WRITE16, 0x0982, 0x0000},	// access_ctl_stat
	{TOK_WRITE16, 0x098A, 0x0000},	// physical_address_access
	{TOK_WRITE16, 0x886C, 0xC0F1},
	{TOK_WRITE16, 0x886E, 0xC5E1},
	{TOK_WRITE16, 0x8870, 0x246A},
	{TOK_WRITE16, 0x8872, 0x1280},
	{TOK_WRITE16, 0x8874, 0xC4E1},
	{TOK_WRITE16, 0x8876, 0xD20F},
	{TOK_WRITE16, 0x8878, 0x2069},
	{TOK_WRITE16, 0x887A, 0x0000},
	{TOK_WRITE16, 0x887C, 0x6A62},
	{TOK_WRITE16, 0x887E, 0x1303},
	{TOK_WRITE16, 0x8880, 0x0084},
	{TOK_WRITE16, 0x8882, 0x1734},
	{TOK_WRITE16, 0x8884, 0x7005},
	{TOK_WRITE16, 0x8886, 0xD801},
	{TOK_WRITE16, 0x8888, 0x8A41},
	{TOK_WRITE16, 0x888A, 0xD900},
	{TOK_WRITE16, 0x888C, 0x0D5A},
	{TOK_WRITE16, 0x888E, 0x0664},
	{TOK_WRITE16, 0x8890, 0x8B61},
	{TOK_WRITE16, 0x8892, 0xE80B},
	{TOK_WRITE16, 0x8894, 0x000D},
	{TOK_WRITE16, 0x8896, 0x0020},
	{TOK_WRITE16, 0x8898, 0xD508},
	{TOK_WRITE16, 0x889A, 0x1504},
	{TOK_WRITE16, 0x889C, 0x1400},
	{TOK_WRITE16, 0x889E, 0x7840},
	{TOK_WRITE16, 0x88A0, 0xD007},
	{TOK_WRITE16, 0x88A2, 0x0DFB},
	{TOK_WRITE16, 0x88A4, 0x9004},
	{TOK_WRITE16, 0x88A6, 0xC4C1},
	{TOK_WRITE16, 0x88A8, 0x2029},
	{TOK_WRITE16, 0x88AA, 0x0300},
	{TOK_WRITE16, 0x88AC, 0x0219},
	{TOK_WRITE16, 0x88AE, 0x06C4},
	{TOK_WRITE16, 0x88B0, 0xFF80},
	{TOK_WRITE16, 0x88B2, 0x08C4},
	{TOK_WRITE16, 0x88B4, 0xFF80},
	{TOK_WRITE16, 0x88B6, 0x086C},
	{TOK_WRITE16, 0x88B8, 0xFF80},
	{TOK_WRITE16, 0x88BA, 0x08C0},
	{TOK_WRITE16, 0x88BC, 0xFF80},
	{TOK_WRITE16, 0x88BE, 0x08C4},
	{TOK_WRITE16, 0x88C0, 0xFF80},
	{TOK_WRITE16, 0x88C2, 0x097C},
	{TOK_WRITE16, 0x88C4, 0x0001},
	{TOK_WRITE16, 0x88C6, 0x0005},
	{TOK_WRITE16, 0x88C8, 0x0000},
	{TOK_WRITE16, 0x88CA, 0x0000},
	{TOK_WRITE16, 0x88CC, 0xC0F1},
	{TOK_WRITE16, 0x88CE, 0x0976},
	{TOK_WRITE16, 0x88D0, 0x06C4},
	{TOK_WRITE16, 0x88D2, 0xD639},
	{TOK_WRITE16, 0x88D4, 0x7708},
	{TOK_WRITE16, 0x88D6, 0x8E01},
	{TOK_WRITE16, 0x88D8, 0x1604},
	{TOK_WRITE16, 0x88DA, 0x1091},
	{TOK_WRITE16, 0x88DC, 0x2046},
	{TOK_WRITE16, 0x88DE, 0x00C1},
	{TOK_WRITE16, 0x88E0, 0x202F},
	{TOK_WRITE16, 0x88E2, 0x2047},
	{TOK_WRITE16, 0x88E4, 0xAE21},
	{TOK_WRITE16, 0x88E6, 0x0F8F},
	{TOK_WRITE16, 0x88E8, 0x1440},
	{TOK_WRITE16, 0x88EA, 0x8EAA},
	{TOK_WRITE16, 0x88EC, 0x8E0B},
	{TOK_WRITE16, 0x88EE, 0x224A},
	{TOK_WRITE16, 0x88F0, 0x2040},
	{TOK_WRITE16, 0x88F2, 0x8E2D},
	{TOK_WRITE16, 0x88F4, 0xBD08},
	{TOK_WRITE16, 0x88F6, 0x7D05},
	{TOK_WRITE16, 0x88F8, 0x8E0C},
	{TOK_WRITE16, 0x88FA, 0xB808},
	{TOK_WRITE16, 0x88FC, 0x7825},
	{TOK_WRITE16, 0x88FE, 0x7510},
	{TOK_WRITE16, 0x8900, 0x22C2},
	{TOK_WRITE16, 0x8902, 0x248C},
	{TOK_WRITE16, 0x8904, 0x081D},
	{TOK_WRITE16, 0x8906, 0x0363},
	{TOK_WRITE16, 0x8908, 0xD9FF},
	{TOK_WRITE16, 0x890A, 0x2502},
	{TOK_WRITE16, 0x890C, 0x1002},
	{TOK_WRITE16, 0x890E, 0x2A05},
	{TOK_WRITE16, 0x8910, 0x03FE},
	{TOK_WRITE16, 0x8912, 0x0A16},
	{TOK_WRITE16, 0x8914, 0x06E4},
	{TOK_WRITE16, 0x8916, 0x702F},
	{TOK_WRITE16, 0x8918, 0x7810},
	{TOK_WRITE16, 0x891A, 0x7D02},
	{TOK_WRITE16, 0x891C, 0x7DB0},
	{TOK_WRITE16, 0x891E, 0xF00B},
	{TOK_WRITE16, 0x8920, 0x78A2},
	{TOK_WRITE16, 0x8922, 0x2805},
	{TOK_WRITE16, 0x8924, 0x03FE},
	{TOK_WRITE16, 0x8926, 0x0A02},
	{TOK_WRITE16, 0x8928, 0x06E4},
	{TOK_WRITE16, 0x892A, 0x702F},
	{TOK_WRITE16, 0x892C, 0x7810},
	{TOK_WRITE16, 0x892E, 0x651D},
	{TOK_WRITE16, 0x8930, 0x7DB0},
	{TOK_WRITE16, 0x8932, 0x7DAF},
	{TOK_WRITE16, 0x8934, 0x8E08},
	{TOK_WRITE16, 0x8936, 0xBD06},
	{TOK_WRITE16, 0x8938, 0xD120},
	{TOK_WRITE16, 0x893A, 0xB8C3},
	{TOK_WRITE16, 0x893C, 0x78A5},
	{TOK_WRITE16, 0x893E, 0xB88F},
	{TOK_WRITE16, 0x8940, 0x1908},
	{TOK_WRITE16, 0x8942, 0x0024},
	{TOK_WRITE16, 0x8944, 0x2841},
	{TOK_WRITE16, 0x8946, 0x0201},
	{TOK_WRITE16, 0x8948, 0x1E26},
	{TOK_WRITE16, 0x894A, 0x1042},
	{TOK_WRITE16, 0x894C, 0x0F15},
	{TOK_WRITE16, 0x894E, 0x1463},
	{TOK_WRITE16, 0x8950, 0x1E27},
	{TOK_WRITE16, 0x8952, 0x1002},
	{TOK_WRITE16, 0x8954, 0x224C},
	{TOK_WRITE16, 0x8956, 0xA000},
	{TOK_WRITE16, 0x8958, 0x224A},
	{TOK_WRITE16, 0x895A, 0x2040},
	{TOK_WRITE16, 0x895C, 0x22C2},
	{TOK_WRITE16, 0x895E, 0x2482},
	{TOK_WRITE16, 0x8960, 0x204F},
	{TOK_WRITE16, 0x8962, 0x2040},
	{TOK_WRITE16, 0x8964, 0x224C},
	{TOK_WRITE16, 0x8966, 0xA000},
	{TOK_WRITE16, 0x8968, 0xB8A2},
	{TOK_WRITE16, 0x896A, 0xF204},
	{TOK_WRITE16, 0x896C, 0x2045},
	{TOK_WRITE16, 0x896E, 0x2180},
	{TOK_WRITE16, 0x8970, 0xAE01},
	{TOK_WRITE16, 0x8972, 0x0D9E},
	{TOK_WRITE16, 0x8974, 0xFFE3},
	{TOK_WRITE16, 0x8976, 0x70E9},
	{TOK_WRITE16, 0x8978, 0x0125},
	{TOK_WRITE16, 0x897A, 0x06C4},
	{TOK_WRITE16, 0x897C, 0xC0F1},
	{TOK_WRITE16, 0x897E, 0xD010},
	{TOK_WRITE16, 0x8980, 0xD110},
	{TOK_WRITE16, 0x8982, 0xD20D},
	{TOK_WRITE16, 0x8984, 0xA020},
	{TOK_WRITE16, 0x8986, 0x8A00},
	{TOK_WRITE16, 0x8988, 0x0809},
	{TOK_WRITE16, 0x898A, 0x01DE},
	{TOK_WRITE16, 0x898C, 0xB8A7},
	{TOK_WRITE16, 0x898E, 0xAA00},
	{TOK_WRITE16, 0x8990, 0xDBFF},
	{TOK_WRITE16, 0x8992, 0x2B41},
	{TOK_WRITE16, 0x8994, 0x0200},
	{TOK_WRITE16, 0x8996, 0xAA0C},
	{TOK_WRITE16, 0x8998, 0x1228},
	{TOK_WRITE16, 0x899A, 0x0080},
	{TOK_WRITE16, 0x899C, 0xAA6D},
	{TOK_WRITE16, 0x899E, 0x0815},
	{TOK_WRITE16, 0x89A0, 0x01DE},
	{TOK_WRITE16, 0x89A2, 0xB8A7},
	{TOK_WRITE16, 0x89A4, 0x1A28},
	{TOK_WRITE16, 0x89A6, 0x0002},
	{TOK_WRITE16, 0x89A8, 0x8123},
	{TOK_WRITE16, 0x89AA, 0x7960},
	{TOK_WRITE16, 0x89AC, 0x1228},
	{TOK_WRITE16, 0x89AE, 0x0080},
	{TOK_WRITE16, 0x89B0, 0xC0D1},
	{TOK_WRITE16, 0x89B2, 0x7EE0},
	{TOK_WRITE16, 0x89B4, 0xFF80},
	{TOK_WRITE16, 0x89B6, 0x0158},
	{TOK_WRITE16, 0x89B8, 0xFF00},
	{TOK_WRITE16, 0x89BA, 0x0618},
	{TOK_WRITE16, 0x89BC, 0x8000},
	{TOK_WRITE16, 0x89BE, 0x0008},
	{TOK_WRITE16, 0x89C0, 0xFF80},
	{TOK_WRITE16, 0x89C2, 0x0A08},
	{TOK_WRITE16, 0x89C4, 0xE280},
	{TOK_WRITE16, 0x89C6, 0x24CA},
	{TOK_WRITE16, 0x89C8, 0x7082},
	{TOK_WRITE16, 0x89CA, 0x78E0},
	{TOK_WRITE16, 0x89CC, 0x20E8},
	{TOK_WRITE16, 0x89CE, 0x01A2},
	{TOK_WRITE16, 0x89D0, 0x1002},
	{TOK_WRITE16, 0x89D2, 0x0D02},
	{TOK_WRITE16, 0x89D4, 0x1902},
	{TOK_WRITE16, 0x89D6, 0x0094},
	{TOK_WRITE16, 0x89D8, 0x7FE0},
	{TOK_WRITE16, 0x89DA, 0x7028},
	{TOK_WRITE16, 0x89DC, 0x7308},
	{TOK_WRITE16, 0x89DE, 0x1000},
	{TOK_WRITE16, 0x89E0, 0x0900},
	{TOK_WRITE16, 0x89E2, 0x7904},
	{TOK_WRITE16, 0x89E4, 0x7947},
	{TOK_WRITE16, 0x89E6, 0x1B00},
	{TOK_WRITE16, 0x89E8, 0x0064},
	{TOK_WRITE16, 0x89EA, 0x7EE0},
	{TOK_WRITE16, 0x89EC, 0xE280},
	{TOK_WRITE16, 0x89EE, 0x24CA},
	{TOK_WRITE16, 0x89F0, 0x7082},
	{TOK_WRITE16, 0x89F2, 0x78E0},
	{TOK_WRITE16, 0x89F4, 0x20E8},
	{TOK_WRITE16, 0x89F6, 0x01A2},
	{TOK_WRITE16, 0x89F8, 0x1102},
	{TOK_WRITE16, 0x89FA, 0x0502},
	{TOK_WRITE16, 0x89FC, 0x1802},
	{TOK_WRITE16, 0x89FE, 0x00B4},
	{TOK_WRITE16, 0x8A00, 0x7FE0},
	{TOK_WRITE16, 0x8A02, 0x7028},
	{TOK_WRITE16, 0x8A04, 0x0000},
	{TOK_WRITE16, 0x8A06, 0x0000},
	{TOK_WRITE16, 0x8A08, 0xFF80},
	{TOK_WRITE16, 0x8A0A, 0x097C},
	{TOK_WRITE16, 0x8A0C, 0xFF80},
	{TOK_WRITE16, 0x8A0E, 0x08CC},
	{TOK_WRITE16, 0x8A10, 0x0000},
	{TOK_WRITE16, 0x8A12, 0x08DC},
	{TOK_WRITE16, 0x8A14, 0x0000},
	{TOK_WRITE16, 0x8A16, 0x0998},
	{TOK_WRITE16, 0x098E, 0x0016},	// logical_address_access
	{TOK_WRITE16, 0x8016, 0x086C},	// mon_address_lo
	{TOK_WRITE16, 0x8002, 0x0001},	// mon_cmd
	{TOK_DELAY, 0, 10},
	{TOK_WRITE16, 0x098E, 0xC40C},	// logical_address_access
	{TOK_WRITE16, 0xC40C, 0x00FF},	// afm_pos_max
	{TOK_WRITE16, 0xC40A, 0x0000},	// afm_pos_min
	{TOK_WRITE16, 0x30D4, 0x9080},	// column_correction
	{TOK_WRITE16, 0x316E, 0xCAFF},	// dac_ecl
	{TOK_WRITE16, 0x305E, 0x10A0},	// global_gain
	{TOK_WRITE16, 0x3E00, 0x0010},	// samp_control
	{TOK_WRITE16, 0x3E02, 0xED02},	// samp_addr_en
	{TOK_WRITE16, 0x3E04, 0xC88C},	// samp_rd1_sig
	{TOK_WRITE16, 0x3E06, 0xC88C},	// samp_rd1_sig_boost
	{TOK_WRITE16, 0x3E08, 0x700A},	// samp_rd1_rst
	{TOK_WRITE16, 0x3E0A, 0x701E},	// samp_rd1_rst_boost
	{TOK_WRITE16, 0x3E0C, 0x00FF},	// samp_rst1_en
	{TOK_WRITE16, 0x3E0E, 0x00FF},	// samp_rst1_boost
	{TOK_WRITE16, 0x3E10, 0x00FF},	// samp_rst1_cloop_sh
	{TOK_WRITE16, 0x3E12, 0x0000},	// samp_rst_boost_seq
	{TOK_WRITE16, 0x3E14, 0xC78C},	// samp_samp1_sig
	{TOK_WRITE16, 0x3E16, 0x6E06},	// samp_samp1_rst
	{TOK_WRITE16, 0x3E18, 0xA58C},	// samp_tx_en
	{TOK_WRITE16, 0x3E1A, 0xA58E},	// samp_tx_boost
	{TOK_WRITE16, 0x3E1C, 0xA58E},	// samp_tx_cloop_sh
	{TOK_WRITE16, 0x3E1E, 0xC0D0},	// samp_tx_boost_seq
	{TOK_WRITE16, 0x3E20, 0xEB00},	// samp_vln_en
	{TOK_WRITE16, 0x3E22, 0x00FF},	// samp_vln_hold
	{TOK_WRITE16, 0x3E24, 0xEB02},	// samp_vcl_en
	{TOK_WRITE16, 0x3E26, 0xEA02},	// samp_colclamp
	{TOK_WRITE16, 0x3E28, 0xEB0A},	// samp_sh_vcl
	{TOK_WRITE16, 0x3E2A, 0xEC01},	// samp_sh_vref
	{TOK_WRITE16, 0x3E2C, 0xEB01},	// samp_sh_vbst
	{TOK_WRITE16, 0x3E2E, 0x00FF},	// samp_spare
	{TOK_WRITE16, 0x3E30, 0x00F3},	// samp_readout
	{TOK_WRITE16, 0x3E32, 0x3DFA},	// samp_reset_done
	{TOK_WRITE16, 0x3E34, 0x00FF},	// samp_vln_clamp
	{TOK_WRITE16, 0x3E36, 0x00F3},	// samp_asc_int
	{TOK_WRITE16, 0x3E38, 0x0000},	// samp_rs_cloop_sh_r
	{TOK_WRITE16, 0x3E3A, 0xF802},	// samp_rs_cloop_sh
	{TOK_WRITE16, 0x3E3C, 0x0FFF},	// samp_rs_boost_seq
	{TOK_WRITE16, 0x3E3E, 0xEA10},	// samp_txlo_gnd
	{TOK_WRITE16, 0x3E40, 0xEB05},	// samp_vln_per_col
	{TOK_WRITE16, 0x3E42, 0xE5C8},	// samp_rd2_sig
	{TOK_WRITE16, 0x3E44, 0xE5C8},	// samp_rd2_sig_boost
	{TOK_WRITE16, 0x3E46, 0x8C70},	// samp_rd2_rst
	{TOK_WRITE16, 0x3E48, 0x8C71},	// samp_rd2_rst_boost
	{TOK_WRITE16, 0x3E4A, 0x00FF},	// samp_rst2_en
	{TOK_WRITE16, 0x3E4C, 0x00FF},	// samp_rst2_boost
	{TOK_WRITE16, 0x3E4E, 0x00FF},	// samp_rst2_cloop_sh
	{TOK_WRITE16, 0x3E50, 0xE38D},	// samp_samp2_sig
	{TOK_WRITE16, 0x3E52, 0x8B0A},	// samp_samp2_rst
	{TOK_WRITE16, 0x3E58, 0xEB0A},	// samp_pix_clamp_en
	{TOK_WRITE16, 0x3E5C, 0x0A00},	// samp_pix_pullup_en
	{TOK_WRITE16, 0x3E5E, 0x00FF},	// samp_pix_pulldown_en_r
	{TOK_WRITE16, 0x3E60, 0x00FF},	// samp_pix_pulldown_en_s
	{TOK_WRITE16, 0x3E90, 0x3C01},	// rst_addr_en
	{TOK_WRITE16, 0x3E92, 0x00FF},	// rst_rst_en
	{TOK_WRITE16, 0x3E94, 0x00FF},	// rst_rst_boost
	{TOK_WRITE16, 0x3E96, 0x3C00},	// rst_tx_en
	{TOK_WRITE16, 0x3E98, 0x3C00},	// rst_tx_boost
	{TOK_WRITE16, 0x3E9A, 0x3C00},	// rst_tx_cloop_sh
	{TOK_WRITE16, 0x3E9C, 0xC0E0},	// rst_tx_boost_seq
	{TOK_WRITE16, 0x3E9E, 0x00FF},	// rst_rst_cloop_sh
	{TOK_WRITE16, 0x3EA0, 0x0000},	// rst_rst_boost_seq
	{TOK_WRITE16, 0x3EA6, 0x3C00},	// rst_pix_pullup_en
	{TOK_WRITE16, 0x3ED8, 0x3057},	// dac_ld_12_13
	{TOK_WRITE16, 0x316C, 0xB44F},	// dac_txlo
	{TOK_WRITE16, 0x316E, 0xCAFF},	// dac_ecl
	{TOK_WRITE16, 0x3ED2, 0xEA0A},	// dac_ld_6_7
	{TOK_WRITE16, 0x3ED4, 0x00A3},	// dac_ld_8_9
	{TOK_WRITE16, 0x3EDC, 0x6020},	// dac_ld_16_17
	{TOK_WRITE16, 0x3EE6, 0xA541},	// dac_ld_26_27
	{TOK_WRITE16, 0x31E0, 0x0000},	// pix_def_id
	{TOK_WRITE16, 0x3ED0, 0x2409},	// dac_ld_4_5
	{TOK_WRITE16, 0x3EDE, 0x0A49},	// dac_ld_18_19
	{TOK_WRITE16, 0x3EE0, 0x4910},	// dac_ld_20_21
	{TOK_WRITE16, 0x3EE2, 0x09D2},	// dac_ld_22_23
	{TOK_WRITE16, 0x30B6, 0x0006},	// autolr_control
	{TOK_WRITE16, 0x337C, 0x0006},	// yuv_ycbcr_control
	{TOK_WRITE8, 0xAC01, 0x00AB},	// awb_mode
	{TOK_WRITE16, 0xAC46, 0x0221},	// awb_left_ccm_0
	{TOK_WRITE16, 0xAC48, 0xFEAE},	// awb_left_ccm_1
	{TOK_WRITE16, 0xAC4A, 0x0032},	// awb_left_ccm_2
	{TOK_WRITE16, 0xAC4C, 0xFFC5},	// awb_left_ccm_3
	{TOK_WRITE16, 0xAC4E, 0x0154},	// awb_left_ccm_4
	{TOK_WRITE16, 0xAC50, 0xFFE7},	// awb_left_ccm_5
	{TOK_WRITE16, 0xAC52, 0xFFB1},	// awb_left_ccm_6
	{TOK_WRITE16, 0xAC54, 0xFEC5},	// awb_left_ccm_7
	{TOK_WRITE16, 0xAC56, 0x028A},	// awb_left_ccm_8
	{TOK_WRITE16, 0xAC58, 0x0130},	// awb_left_ccm_r2b_ratio
	{TOK_WRITE16, 0xAC5C, 0x01CD},	// awb_right_ccm_0
	{TOK_WRITE16, 0xAC5E, 0xFF63},	// awb_right_ccm_1
	{TOK_WRITE16, 0xAC60, 0xFFD0},	// awb_right_ccm_2
	{TOK_WRITE16, 0xAC62, 0xFFCD},	// awb_right_ccm_3
	{TOK_WRITE16, 0xAC64, 0x013B},	// awb_right_ccm_4
	{TOK_WRITE16, 0xAC66, 0xFFF8},	// awb_right_ccm_5
	{TOK_WRITE16, 0xAC68, 0xFFFB},	// awb_right_ccm_6
	{TOK_WRITE16, 0xAC6A, 0xFF78},	// awb_right_ccm_7
	{TOK_WRITE16, 0xAC6C, 0x018D},	// awb_right_ccm_8
	{TOK_WRITE16, 0xAC6E, 0x0055},	// awb_right_ccm_r2b_ratio
	{TOK_WRITE16, 0xB842, 0x0037},	// stat_awb_gray_checker_offset_x
	{TOK_WRITE16, 0xB844, 0x0044},	// stat_awb_gray_checker_offset_y
	{TOK_WRITE16, 0x3240, 0x0024},	// awb_xy_scale
	{TOK_WRITE16, 0x3240, 0x0024},	// awb_xy_scale
	{TOK_WRITE16, 0x3242, 0x0000},	// awb_weight_r0
	{TOK_WRITE16, 0x3244, 0x0000},	// awb_weight_r1
	{TOK_WRITE16, 0x3246, 0x0000},	// awb_weight_r2
	{TOK_WRITE16, 0x3248, 0x7F00},	// awb_weight_r3
	{TOK_WRITE16, 0x324A, 0xA500},	// awb_weight_r4
	{TOK_WRITE16, 0x324C, 0x1540},	// awb_weight_r5
	{TOK_WRITE16, 0x324E, 0x01AC},	// awb_weight_r6
	{TOK_WRITE16, 0x3250, 0x003E},	// awb_weight_r7
	{TOK_DELAY, 0, 10},
	{TOK_WRITE8, 0xAC3C, 0x002E},	// awb_min_accepted_pre_awb_r2g_ratio
	{TOK_WRITE8, 0xAC3D, 0x0084},	// awb_max_accepted_pre_awb_r2g_ratio
	{TOK_WRITE8, 0xAC3E, 0x0011},	// awb_min_accepted_pre_awb_b2g_ratio
	{TOK_WRITE8, 0xAC3F, 0x0063},	// awb_max_accepted_pre_awb_b2g_ratio
	{TOK_WRITE8, 0xACB0, 0x002B},	// awb_rg_min
	{TOK_WRITE8, 0xACB1, 0x0084},	// awb_rg_max
	{TOK_WRITE8, 0xACB4, 0x0011},	// awb_bg_min
	{TOK_WRITE8, 0xACB5, 0x0063},	// awb_bg_max
	{TOK_WRITE8, 0xD80F, 0x0004},	// jpeg_qscale_0
	{TOK_WRITE8, 0xD810, 0x0008},	// jpeg_qscale_1
	{TOK_WRITE8, 0xC8D2, 0x0004},	// cam_output_1_jpeg_qscale_0
	{TOK_WRITE8, 0xC8D3, 0x0008},	// cam_output_1_jpeg_qscale_1
	{TOK_WRITE8, 0xC8BC, 0x0004},	// cam_output_0_jpeg_qscale_0
	{TOK_WRITE8, 0xC8BD, 0x0008},	// cam_output_0_jpeg_qscale_1
	{TOK_WRITE16, 0x301A, 0x10F0},	// reset_register
	{TOK_WRITE16, 0x301E, 0x0000},	// data_pedestal_
	{TOK_WRITE16, 0x301A, 0x10F8},	// reset_register
	{TOK_WRITE8, 0xDC33, 0x0000},	// sys_first_black_level
	{TOK_WRITE8, 0xDC35, 0x0004},	// sys_uv_color_boost
	{TOK_WRITE16, 0x326E, 0x0006},
	{TOK_WRITE8, 0xDC37, 0x0062},	// sys_bright_colorkill
	{TOK_WRITE16, 0x35A4, 0x0596},	// bright_color_kill_controls
	{TOK_WRITE16, 0x35A2, 0x0094},	// dark_color_kill_controls
	{TOK_WRITE8, 0xDC36, 0x0023},	// sys_dark_color_kill
	{TOK_WRITE8, 0xBC18, 0x0000},	// ll_gamma_contrast_curve_0
	{TOK_WRITE8, 0xBC19, 0x0011},	// ll_gamma_contrast_curve_1
	{TOK_WRITE8, 0xBC1A, 0x0023},	// ll_gamma_contrast_curve_2
	{TOK_WRITE8, 0xBC1B, 0x003F},	// ll_gamma_contrast_curve_3
	{TOK_WRITE8, 0xBC1C, 0x0067},	// ll_gamma_contrast_curve_4
	{TOK_WRITE8, 0xBC1D, 0x0085},	// ll_gamma_contrast_curve_5
	{TOK_WRITE8, 0xBC1E, 0x009B},	// ll_gamma_contrast_curve_6
	{TOK_WRITE8, 0xBC1F, 0x00AD},	// ll_gamma_contrast_curve_7
	{TOK_WRITE8, 0xBC20, 0x00BB},	// ll_gamma_contrast_curve_8
	{TOK_WRITE8, 0xBC21, 0x00C7},	// ll_gamma_contrast_curve_9
	{TOK_WRITE8, 0xBC22, 0x00D1},	// ll_gamma_contrast_curve_10
	{TOK_WRITE8, 0xBC23, 0x00DA},	// ll_gamma_contrast_curve_11
	{TOK_WRITE8, 0xBC24, 0x00E1},	// ll_gamma_contrast_curve_12
	{TOK_WRITE8, 0xBC25, 0x00E8},	// ll_gamma_contrast_curve_13
	{TOK_WRITE8, 0xBC26, 0x00EE},	// ll_gamma_contrast_curve_14
	{TOK_WRITE8, 0xBC27, 0x00F3},	// ll_gamma_contrast_curve_15
	{TOK_WRITE8, 0xBC28, 0x00F7},	// ll_gamma_contrast_curve_16
	{TOK_WRITE8, 0xBC29, 0x00FB},	// ll_gamma_contrast_curve_17
	{TOK_WRITE8, 0xBC2A, 0x00FF},	// ll_gamma_contrast_curve_18
	{TOK_WRITE8, 0xBC2B, 0x0000},	// ll_gamma_neutral_curve_0
	{TOK_WRITE8, 0xBC2C, 0x0011},	// ll_gamma_neutral_curve_1
	{TOK_WRITE8, 0xBC2D, 0x0023},	// ll_gamma_neutral_curve_2
	{TOK_WRITE8, 0xBC2E, 0x003F},	// ll_gamma_neutral_curve_3
	{TOK_WRITE8, 0xBC2F, 0x0067},	// ll_gamma_neutral_curve_4
	{TOK_WRITE8, 0xBC30, 0x0085},	// ll_gamma_neutral_curve_5
	{TOK_WRITE8, 0xBC31, 0x009B},	// ll_gamma_neutral_curve_6
	{TOK_WRITE8, 0xBC32, 0x00AD},	// ll_gamma_neutral_curve_7
	{TOK_WRITE8, 0xBC33, 0x00BB},	// ll_gamma_neutral_curve_8
	{TOK_WRITE8, 0xBC34, 0x00C7},	// ll_gamma_neutral_curve_9
	{TOK_WRITE8, 0xBC35, 0x00D1},	// ll_gamma_neutral_curve_10
	{TOK_WRITE8, 0xBC36, 0x00DA},	// ll_gamma_neutral_curve_11
	{TOK_WRITE8, 0xBC37, 0x00E1},	// ll_gamma_neutral_curve_12
	{TOK_WRITE8, 0xBC38, 0x00E8},	// ll_gamma_neutral_curve_13
	{TOK_WRITE8, 0xBC39, 0x00EE},	// ll_gamma_neutral_curve_14
	{TOK_WRITE8, 0xBC3A, 0x00F3},	// ll_gamma_neutral_curve_15
	{TOK_WRITE8, 0xBC3B, 0x00F7},	// ll_gamma_neutral_curve_16
	{TOK_WRITE8, 0xBC3C, 0x00FB},	// ll_gamma_neutral_curve_17
	{TOK_WRITE8, 0xBC3D, 0x00FF},	// ll_gamma_neutral_curve_18
	{TOK_WRITE8, 0xBC3E, 0x0000},	// ll_gamma_nr_curve_0
	{TOK_WRITE8, 0xBC3F, 0x0018},	// ll_gamma_nr_curve_1
	{TOK_WRITE8, 0xBC40, 0x0025},	// ll_gamma_nr_curve_2
	{TOK_WRITE8, 0xBC41, 0x003A},	// ll_gamma_nr_curve_3
	{TOK_WRITE8, 0xBC42, 0x0059},	// ll_gamma_nr_curve_4
	{TOK_WRITE8, 0xBC43, 0x0070},	// ll_gamma_nr_curve_5
	{TOK_WRITE8, 0xBC44, 0x0081},	// ll_gamma_nr_curve_6
	{TOK_WRITE8, 0xBC45, 0x0090},	// ll_gamma_nr_curve_7
	{TOK_WRITE8, 0xBC46, 0x009E},	// ll_gamma_nr_curve_8
	{TOK_WRITE8, 0xBC47, 0x00AB},	// ll_gamma_nr_curve_9
	{TOK_WRITE8, 0xBC48, 0x00B6},	// ll_gamma_nr_curve_10
	{TOK_WRITE8, 0xBC49, 0x00C1},	// ll_gamma_nr_curve_11
	{TOK_WRITE8, 0xBC4A, 0x00CB},	// ll_gamma_nr_curve_12
	{TOK_WRITE8, 0xBC4B, 0x00D5},	// ll_gamma_nr_curve_13
	{TOK_WRITE8, 0xBC4C, 0x00DE},	// ll_gamma_nr_curve_14
	{TOK_WRITE8, 0xBC4D, 0x00E7},	// ll_gamma_nr_curve_15
	{TOK_WRITE8, 0xBC4E, 0x00EF},	// ll_gamma_nr_curve_16
	{TOK_WRITE8, 0xBC4F, 0x00F7},	// ll_gamma_nr_curve_17
	{TOK_WRITE8, 0xBC50, 0x00FF},	// ll_gamma_nr_curve_18
	{TOK_WRITE8, 0xB801, 0x00E0},	// stat_mode
	{TOK_WRITE8, 0xB862, 0x0004},	// stat_bmtracking_speed
	{TOK_WRITE8, 0xB829, 0x0002},	// stat_ll_brightness_metric_divisor
	{TOK_WRITE8, 0xB863, 0x0002},	// stat_bm_mul
	{TOK_WRITE8, 0xB827, 0x000F},	// stat_ae_ev_shift
	{TOK_WRITE8, 0xA409, 0x0037},	// ae_rule_base_target
	{TOK_WRITE16, 0xBC52, 0x00C8},	// ll_start_brightness_metric
	{TOK_WRITE16, 0xBC54, 0x0A28},	// ll_end_brightness_metric
	{TOK_WRITE16, 0xBC58, 0x00C8},	// ll_start_gain_metric
	{TOK_WRITE16, 0xBC5A, 0x12C0},	// ll_end_gain_metric
	{TOK_WRITE16, 0xBC5E, 0x00FA},	// ll_start_aperture_gain_bm
	{TOK_WRITE16, 0xBC60, 0x0258},	// ll_end_aperture_gain_bm
	{TOK_WRITE16, 0xBC66, 0x00FA},	// ll_start_aperture_gm
	{TOK_WRITE16, 0xBC68, 0x0258},	// ll_end_aperture_gm
	{TOK_WRITE16, 0xBC86, 0x00C8},	// ll_start_ffnr_gm
	{TOK_WRITE16, 0xBC88, 0x0640},	// ll_end_ffnr_gm
	{TOK_WRITE16, 0xBCBC, 0x0040},	// ll_sffb_start_gain
	{TOK_WRITE16, 0xBCBE, 0x01FC},	// ll_sffb_end_gain
	{TOK_WRITE16, 0xBCCC, 0x00C8},	// ll_sffb_start_max_gm
	{TOK_WRITE16, 0xBCCE, 0x0640},	// ll_sffb_end_max_gm
	{TOK_WRITE16, 0xBC90, 0x00C8},	// ll_start_grb_gm
	{TOK_WRITE16, 0xBC92, 0x0640},	// ll_end_grb_gm
	{TOK_WRITE16, 0xBC0E, 0x0001},	// ll_gamma_curve_adj_start_pos
	{TOK_WRITE16, 0xBC10, 0x0002},	// ll_gamma_curve_adj_mid_pos
	{TOK_WRITE16, 0xBC12, 0x02BC},	// ll_gamma_curve_adj_end_pos
	{TOK_WRITE16, 0xBCAA, 0x044C},	// ll_cdc_thr_adj_start_pos
	{TOK_WRITE16, 0xBCAC, 0x00AF},	// ll_cdc_thr_adj_mid_pos
	{TOK_WRITE16, 0xBCAE, 0x0009},	// ll_cdc_thr_adj_end_pos
	{TOK_WRITE16, 0xBCD8, 0x00C8},	// ll_pcr_start_bm
	{TOK_WRITE16, 0xBCDA, 0x0A28},	// ll_pcr_end_bm
	{TOK_WRITE16, 0x3380, 0x0504},	// kernel_config
	{TOK_WRITE8, 0xBC94, 0x000C},	// ll_gb_start_threshold_0
	{TOK_WRITE8, 0xBC95, 0x0008},	// ll_gb_start_threshold_1
	{TOK_WRITE8, 0xBC9C, 0x003C},	// ll_gb_end_threshold_0
	{TOK_WRITE8, 0xBC9D, 0x0028},	// ll_gb_end_threshold_1
	{TOK_WRITE16, 0x33B0, 0x2A16},	// ffnr_alpha_beta
	{TOK_WRITE8, 0xBC8A, 0x0002},	// ll_start_ff_mix_thresh_y
	{TOK_WRITE8, 0xBC8B, 0x000F},	// ll_end_ff_mix_thresh_y
	{TOK_WRITE8, 0xBC8C, 0x00FF},	// ll_start_ff_mix_thresh_ygain
	{TOK_WRITE8, 0xBC8D, 0x00FF},	// ll_end_ff_mix_thresh_ygain
	{TOK_WRITE8, 0xBC8E, 0x00FF},	// ll_start_ff_mix_thresh_gain
	{TOK_WRITE8, 0xBC8F, 0x0000},	// ll_end_ff_mix_thresh_gain
	{TOK_WRITE8, 0xBCB2, 0x0020},	// ll_cdc_dark_clus_slope
	{TOK_WRITE8, 0xBCB3, 0x003A},	// ll_cdc_dark_clus_satur
	{TOK_WRITE8, 0xBCB4, 0x0039},
	{TOK_WRITE8, 0xBCB7, 0x0039},
	{TOK_WRITE8, 0xBCB5, 0x0020},
	{TOK_WRITE8, 0xBCB8, 0x003A},
	{TOK_WRITE8, 0xBCB6, 0x0080},
	{TOK_WRITE8, 0xBCB9, 0x0024},
	{TOK_WRITE16, 0xBCAA, 0x03E8},	// ll_cdc_thr_adj_start_pos
	{TOK_WRITE16, 0xBCAC, 0x012C},	// ll_cdc_thr_adj_mid_pos
	{TOK_WRITE16, 0xBCAE, 0x0009},	// ll_cdc_thr_adj_end_pos
	{TOK_WRITE16, 0x33BA, 0x0084},	// apedge_control
	{TOK_WRITE16, 0x33BE, 0x0000},	// ua_knee_l
	{TOK_WRITE16, 0x33C2, 0x8800},	// ua_weights
	{TOK_WRITE16, 0xBC5E, 0x0154},	// ll_start_aperture_gain_bm
	{TOK_WRITE16, 0xBC60, 0x0640},	// ll_end_aperture_gain_bm
	{TOK_WRITE8, 0xBC62, 0x000E},	// ll_start_aperture_kpgain
	{TOK_WRITE8, 0xBC63, 0x0014},	// ll_end_aperture_kpgain
	{TOK_WRITE8, 0xBC64, 0x000E},	// ll_start_aperture_kngain
	{TOK_WRITE8, 0xBC65, 0x0014},	// ll_end_aperture_kngain
	{TOK_WRITE8, 0xBCE2, 0x000A},	// ll_start_pos_knee
	{TOK_WRITE8, 0xBCE3, 0x002B},	// ll_end_pos_knee
	{TOK_WRITE8, 0xBCE4, 0x000A},	// ll_start_neg_knee
	{TOK_WRITE8, 0xBCE5, 0x002B},	// ll_end_neg_knee
	{TOK_WRITE16, 0x3210, 0x01B0},	// color_pipeline_control
	{TOK_WRITE8, 0xBCC0, 0x001F},	// ll_sffb_ramp_start
	{TOK_WRITE8, 0xBCC1, 0x0003},	// ll_sffb_ramp_stop
	{TOK_WRITE8, 0xBCC2, 0x002C},	// ll_sffb_slope_start
	{TOK_WRITE8, 0xBCC3, 0x0010},	// ll_sffb_slope_stop
	{TOK_WRITE8, 0xBCC4, 0x0007},	// ll_sffb_thstart
	{TOK_WRITE8, 0xBCC5, 0x000B},	// ll_sffb_thstop
	{TOK_WRITE16, 0xBCBA, 0x0009},	// ll_sffb_config
	{TOK_WRITE16, 0xBC14, 0xFFFE},	// ll_gamma_fade_to_black_start_pos
	{TOK_WRITE16, 0xBC16, 0xFFFF},	// ll_gamma_fade_to_black_end_pos
	{TOK_WRITE16, 0xBC66, 0x0154},	// ll_start_aperture_gm
	{TOK_WRITE16, 0xBC68, 0x07D0},	// ll_end_aperture_gm
	{TOK_WRITE8, 0xBC6A, 0x0004},	// ll_start_aperture_integer_gain
	{TOK_WRITE8, 0xBC6B, 0x0000},	// ll_end_aperture_integer_gain
	{TOK_WRITE8, 0xBC6C, 0x0000},	// ll_start_aperture_exp_gain
	{TOK_WRITE8, 0xBC6D, 0x0000},	// ll_end_aperture_exp_gain
	{TOK_WRITE16, 0xA81C, 0x0040},	// ae_track_min_again
	{TOK_WRITE16, 0xA820, 0x01FC},	// ae_track_max_again
	{TOK_WRITE16, 0xA822, 0x0080},	// ae_track_min_dgain
	{TOK_WRITE16, 0xA824, 0x0100},	// ae_track_max_dgain
	{TOK_WRITE8, 0xBC56, 0x0064},	// ll_start_ccm_saturation
	{TOK_WRITE8, 0xBC57, 0x001E},	// ll_end_ccm_saturation
	{TOK_WRITE8, 0xBCDE, 0x0003},	// ll_start_sys_threshold
	{TOK_WRITE8, 0xBCDF, 0x0050},	// ll_stop_sys_threshold
	{TOK_WRITE8, 0xBCE0, 0x0008},	// ll_start_sys_gain
	{TOK_WRITE8, 0xBCE1, 0x0003},	// ll_stop_sys_gain
	{TOK_WRITE16, 0xBCD0, 0x000A},	// ll_sffb_sobel_flat_start
	{TOK_WRITE16, 0xBCD2, 0x00FE},	// ll_sffb_sobel_flat_stop
	{TOK_WRITE16, 0xBCD4, 0x001E},	// ll_sffb_sobel_sharp_start
	{TOK_WRITE16, 0xBCD6, 0x00FF},	// ll_sffb_sobel_sharp_stop
	{TOK_WRITE8, 0xBCC6, 0x0000},	// ll_sffb_sharpening_start
	{TOK_WRITE8, 0xBCC7, 0x0000},	// ll_sffb_sharpening_stop
	{TOK_WRITE8, 0xBCC8, 0x0020},	// ll_sffb_flatness_start
	{TOK_WRITE8, 0xBCC9, 0x0040},	// ll_sffb_flatness_stop
	{TOK_WRITE8, 0xBCCA, 0x0004},	// ll_sffb_transition_start
	{TOK_WRITE8, 0xBCCB, 0x0000},	// ll_sffb_transition_stop
	{TOK_WRITE8, 0xBCE6, 0x0003},	// ll_sffb_zero_enable
	{TOK_WRITE8, 0xBCE6, 0x0003},	// ll_sffb_zero_enable
	{TOK_WRITE8, 0xA410, 0x0004},	// ae_rule_target_ae_6
	{TOK_WRITE8, 0xA411, 0x0006},	// ae_rule_target_ae_7
	{TOK_WRITE8, 0xC8BC, 0x0004},	// cam_output_0_jpeg_qscale_0
	{TOK_WRITE8, 0xC8BD, 0x000A},	// cam_output_0_jpeg_qscale_1
	{TOK_WRITE8, 0xC8D2, 0x0004},	// cam_output_1_jpeg_qscale_0
	{TOK_WRITE8, 0xC8D3, 0x000A},	// cam_output_1_jpeg_qscale_1
	{TOK_WRITE8, 0xDC3A, 0x0023},	// sys_sepia_cr
	{TOK_WRITE8, 0xDC3B, 0x00B2},	// sys_sepia_cb
	{TOK_WRITE16, 0x0018, 0x2008},	// standby_control_and_status
	{TOK_WRITE8, 0x843C, 0x0001},	// seq_state_cfg_5_max_frame_cnt
	{TOK_WRITE8, 0x8404, 0x0001},	// seq_cmd
	{TOK_WRITE16, 0x0016, 0x0447},	// clocks_control
	{TOK_WRITE16, 0xC85C, 0x0463},	// cam_core_a_min_frame_length_lines
	{TOK_WRITE16, 0xC860, 0x0463},	// cam_core_a_base_frame_length_lines
	{TOK_WRITE16, 0xC862, 0x0DB0},	// cam_core_a_min_line_length_pclk
	{TOK_WRITE16, 0xC868, 0x0463},	// cam_core_a_frame_length_lines
	{TOK_WRITE16, 0xC86A, 0x0DB0},	// cam_core_a_line_length_pck
	{TOK_WRITE16, 0xC8AA, 0x0500},	// cam_output_0_image_width
	{TOK_WRITE16, 0xC8AC, 0x02D0},	// cam_output_0_image_height
	{TOK_WRITE16, 0xC894, 0x07F0},	// cam_core_b_min_frame_length_lines
	{TOK_WRITE16, 0xC898, 0x07F0},	// cam_core_b_base_frame_length_lines
	{TOK_WRITE16, 0xC89A, 0x0F24},	// cam_core_b_min_line_length_pclk
	{TOK_WRITE16, 0xC8A0, 0x07F0},	// cam_core_b_frame_length_lines
	{TOK_WRITE16, 0xC8A2, 0x0F24},	// cam_core_b_line_length_pck
	{TOK_WRITE16, 0xA010, 0x0147},	// fd_min_expected50hz_flicker_period
	{TOK_WRITE16, 0xA012, 0x015B},	// fd_max_expected50hz_flicker_period
	{TOK_WRITE16, 0xA014, 0x010F},	// fd_min_expected60hz_flicker_period
	{TOK_WRITE16, 0xA016, 0x0123},	// fd_max_expected60hz_flicker_period
	{TOK_WRITE16, 0xA018, 0x0151},	// fd_expected50hz_flicker_period_in_context_a
	{TOK_WRITE16, 0xA01C, 0x0118},	// fd_expected60hz_flicker_period_in_context_a
	{TOK_WRITE16, 0xA01E, 0x00FE},	// fd_expected60hz_flicker_period_in_context_b
	{TOK_WRITE8, 0xDC0A, 0x000E},	// sys_scale_mode
	{TOK_WRITE16, 0xDC1C, 0x3430},	// sys_zoom_ratio
	{TOK_WRITE8, 0x8404, 0x0005},	// seq_cmd
	{TOK_WRITE8, 0x8404, 0x0006},	// seq_cmd
	{TOK_DELAY, 0, 100},
	{TOK_WRITE16, 0x098E, 0xC85C},	// logical_address_access
	{TOK_WRITE16, 0xC85C, 0x0463},	// cam_core_a_min_frame_length_lines
	{TOK_WRITE16, 0xC860, 0x0463},	// cam_core_a_base_frame_length_lines
	{TOK_WRITE16, 0xC868, 0x0463},	// cam_core_a_frame_length_lines
	{TOK_WRITE16, 0xA818, 0x0463},	// ae_track_target_int_time_rows
	{TOK_WRITE16, 0xA81A, 0x0463},	// ae_track_max_int_time_rows
	{TOK_DELAY, 0, 100},
	{TOK_WRITE16, 0x098E, 0x8419},	// logical_address_access
	{TOK_WRITE8, 0x8419, 0x0003},	// seq_state_cfg_1_af
	{TOK_WRITE8, 0x8404, 0x0006},	// seq_cmd
	{TOK_DELAY, 0, 100},
	{TOK_TERM, 0, 0},
};

static int mt9p111_read_reg(struct i2c_client *client, unsigned short reg)
{
	int err = 0;
	struct i2c_msg msg[1];
	unsigned char data[2];
	unsigned short val = 0;

	if (!client->adapter)
		return -ENODEV;

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = I2C_TWO_BYTE_TRANSFER;
	msg->buf = data;
	data[0] = (reg & I2C_TXRX_DATA_MASK_UPPER) >> I2C_TXRX_DATA_SHIFT;
	data[1] = (reg & I2C_TXRX_DATA_MASK);
	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0) {
		msg->flags = I2C_M_RD;
		msg->len = I2C_TWO_BYTE_TRANSFER;	/* 2 byte read */
		err = i2c_transfer(client->adapter, msg, 1);
		if (err >= 0) {
			val = ((data[0] & I2C_TXRX_DATA_MASK) <<
				I2C_TXRX_DATA_SHIFT) |
				(data[1] & I2C_TXRX_DATA_MASK);
		}
	}

	v4l_dbg(1, debug, client,
		 "mt9p111_read_reg reg=0x%x, val=0x%x\n", reg, val);

	return (int)(0xffff & val);
}

static int mt9p111_write_reg(struct i2c_client *client, unsigned short reg,
			     unsigned short val, int len)
{
	int err = -EAGAIN; /* To enter below loop, err has to be negative */
	int trycnt = 0;
	struct i2c_msg msg[1];
	unsigned char data[4];

	v4l_dbg(1, debug, client,
			"mt9p111_write_reg reg=0x%x, val=0x%x\n", reg, val);

	if (!client->adapter)
		return -ENODEV;

	while ((err < 0) && (trycnt < I2C_RETRY_COUNT)) {
		trycnt++;
		msg->addr = client->addr;
		msg->flags = 0;
		msg->len = len;
		msg->buf = data;
		data[0] = (reg & I2C_TXRX_DATA_MASK_UPPER) >>
			I2C_TXRX_DATA_SHIFT;
		data[1] = (reg & I2C_TXRX_DATA_MASK);
		if (len == 3) {
			data[2] = (val & I2C_TXRX_DATA_MASK);
		} else if (len == 4) {
			data[2] = (val & I2C_TXRX_DATA_MASK_UPPER) >>
				I2C_TXRX_DATA_SHIFT;
			data[3] = (val & I2C_TXRX_DATA_MASK);
		} else
			break;
		err = i2c_transfer(client->adapter, msg, 1);
	}
	if (err < 0)
		printk(KERN_INFO "\n I2C write failed");

	return err;
}

/*
 * mt9p111_write_regs : Initializes a list of registers
 *		if token is TOK_TERM, then entire write operation terminates
 *		if token is TOK_DELAY, then a delay of 'val' msec is introduced
 *		if token is TOK_SKIP, then the register write is skipped
 *		if token is TOK_WRITE, then the register write is performed
 *
 * reglist - list of registers to be written
 * Returns zero if successful, or non-zero otherwise.
 */
static int mt9p111_write_regs(struct i2c_client *client,
			      const struct mt9p111_reg reglist[])
{
	int err;
	const struct mt9p111_reg *next = reglist;

	for (; next->token != TOK_TERM; next++) {
		if (next->token == TOK_DELAY) {
			msleep(next->val);
			continue;
		}

		if (next->token == TOK_SKIP)
			continue;

		if (next->token == TOK_WRITE8)
			err = mt9p111_write_reg(client, next->reg, next->val,
						I2C_THREE_BYTE_TRANSFER);
		else if (next->token == TOK_WRITE16)
			err = mt9p111_write_reg(client, next->reg, next->val,
						I2C_FOUR_BYTE_TRANSFER);
		else {
			printk(KERN_WARNING "Bad TOKEN");
			continue;
		}

		if (err < 0) {
			v4l_err(client, "Write failed. Err[%d]\n", err);
			return err;
		}
	}
	return 0;
}

/*
 * Configure the mt9p111 with the current register settings
 * Returns zero if successful, or non-zero otherwise.
 */
static int mt9p111_def_config(struct v4l2_subdev *subdev)
{
	struct i2c_client *client = v4l2_get_subdevdata(subdev);

	/* common register initialization */
	return mt9p111_write_regs(client, mt9p111_reg_list);
}

/*
 * Detect if an mt9p111 is present, and if so which revision.
 * A device is considered to be detected if the chip ID (LSB and MSB)
 * registers match the expected values.
 * Any value of the rom version register is accepted.
 * Returns ENODEV error number if no device is detected, or zero
 * if a device is detected.
 */
static int mt9p111_detect(struct v4l2_subdev *subdev)
{
	struct mt9p111 *decoder = to_mt9p111(subdev);
	struct i2c_client *client = v4l2_get_subdevdata(subdev);
	unsigned short val = 0;

	val = mt9p111_read_reg(client, REG_CHIP_ID);
	v4l_dbg(1, debug, client, "chip id detected 0x%x\n", val);

	if (MT9P111_CHIP_ID != val) {
		/* We didn't read the values we expected, so this must not be
		 * MT9P111.
		 */
		v4l_err(client, "chip id mismatch read 0x%x, expecting 0x%x\n",
				val, MT9P111_CHIP_ID);
		return -ENODEV;
	}

	decoder->ver = val;

	v4l_info(client, "%s found at 0x%x (%s)\n", client->name,
			client->addr << 1, client->adapter->name);
	return 0;
}


/* --------------------------------------------------------------------------
 * V4L2 subdev core operations
 */
/*
 * mt9p111_g_chip_ident - get chip identifier
 */
static int mt9p111_g_chip_ident(struct v4l2_subdev *subdev,
			       struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(subdev);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_MT9P111, 0);
}

/*
 * mt9p111_dev_init - sensor init, tries to detect the sensor
 * @subdev: pointer to standard V4L2 subdev structure
 */
static int mt9p111_dev_init(struct v4l2_subdev *subdev)
{
	struct mt9p111 *decoder = to_mt9p111(subdev);
	int rval;

	rval = decoder->pdata->s_power(subdev, 1);
	if (rval)
		return rval;

	rval = mt9p111_detect(subdev);

	decoder->pdata->s_power(subdev, 0);

	return rval;
}

/*
 * mt9p111_s_config - set the platform data for future use
 * @subdev: pointer to standard V4L2 subdev structure
 * @irq:
 * @platform_data: sensor platform_data
 */
static int mt9p111_s_config(struct v4l2_subdev *subdev, int irq,
			   void *platform_data)
{
	struct mt9p111 *decoder = to_mt9p111(subdev);
	int rval;

	if (platform_data == NULL)
		return -ENODEV;

	decoder->pdata = platform_data;

	rval = mt9p111_dev_init(subdev);
	if (rval)
		return rval;

	return 0;
}

/*
 * mt9p111_s_power - Set power function
 * @subdev: pointer to standard V4L2 subdev structure
 * @on: power state to which device is to be set
 *
 * Sets devices power state to requested state, if possible.
 */
static int mt9p111_s_power(struct v4l2_subdev *subdev, int on)
{
	struct mt9p111 *decoder = to_mt9p111(subdev);
	struct i2c_client *client = v4l2_get_subdevdata(subdev);
	int rval;

	if (on) {
		rval = decoder->pdata->s_power(subdev, 1);
		if (rval)
			goto out;
		rval = mt9p111_def_config(subdev);
		if (rval) {
			decoder->pdata->s_power(subdev, 0);
			goto out;
		}
	} else {
		rval = decoder->pdata->s_power(subdev, 0);
		if (rval)
			goto out;
	}

	decoder->power = on;
out:
	if (rval < 0)
		v4l_err(client, "Unable to set target power state\n");

	return rval;
}

/* --------------------------------------------------------------------------
 * V4L2 subdev file operations
 */
static int mt9p111_open(struct v4l2_subdev *subdev, struct v4l2_subdev_fh *fh)
{
	struct v4l2_mbus_framefmt *format;
	struct v4l2_rect *crop;

	/*
	 * Default configuration -
	 *	Resolution: VGA
	 *	Format: UYVY
	 *	crop = window
	 */
	crop = v4l2_subdev_get_try_crop(fh, 0);
	crop->left = 0;
	crop->top = 0;
	crop->width = MT9P111_DEF_WIDTH;
	crop->height = MT9P111_DEF_HEIGHT;

	format = v4l2_subdev_get_try_format(fh, 0);
	format->code = V4L2_MBUS_FMT_UYVY8_2X8;
	format->width = MT9P111_DEF_WIDTH;
	format->height = MT9P111_DEF_HEIGHT;
	format->field = V4L2_FIELD_NONE;
	format->colorspace = V4L2_COLORSPACE_JPEG;

	return 0;
}

/* --------------------------------------------------------------------------
 * V4L2 subdev video operations
 */
static int mt9p111_s_stream(struct v4l2_subdev *subdev, int streaming)
{
	/*
	 * FIXME: We should put here the specific reg setting to turn on
	 * streaming in sensor.
	 */
	return 0;
}

/* --------------------------------------------------------------------------
 * V4L2 subdev pad operations
 */
static int mt9p111_enum_mbus_code(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	struct mt9p111 *mt9p111 = to_mt9p111(subdev);

	if (code->index >= ARRAY_SIZE(mt9p111_fmt_list))
		return -EINVAL;

	code->code = mt9p111->format.code;

	return 0;
}

static int mt9p111_enum_frame_size(struct v4l2_subdev *subdev,
				   struct v4l2_subdev_fh *fh,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	int i;

	/* Is requested media-bus format/pixelformat not found on sensor? */
	for (i = 0; i < ARRAY_SIZE(mt9p111_fmt_list); i++) {
		if (fse->code == mt9p111_fmt_list[i].mbus_code )
			goto fmt_found;
	}
	if (i >= ARRAY_SIZE(mt9p111_fmt_list))
		return -EINVAL;

fmt_found:
	/*
	 * Currently only supports VGA resolution
	 */
	fse->min_width = fse->max_width = MT9P111_DEF_WIDTH;
	fse->min_height = fse->max_height = MT9P111_DEF_HEIGHT;

	return 0;
}

static int mt9p111_get_pad_format(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_format *fmt)
{
	struct mt9p111 *mt9p111 = to_mt9p111(subdev);

	fmt->format = mt9p111->format;
	return 0;
}

static int mt9p111_set_pad_format(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_format *fmt)
{
	int i;
	struct mt9p111 *mt9p111 = to_mt9p111(subdev);

	for (i = 0; i < ARRAY_SIZE(mt9p111_fmt_list); i++) {
		if (fmt->format.code == mt9p111_fmt_list[i].mbus_code )
			goto fmt_found;
	}
	if (i >= ARRAY_SIZE(mt9p111_fmt_list))
		return -EINVAL;

fmt_found:
	/*
	 * Only VGA resolution supported
	 */
	fmt->format.width = MT9P111_DEF_WIDTH;
	fmt->format.height = MT9P111_DEF_HEIGHT;
	fmt->format.field = V4L2_FIELD_NONE;
	fmt->format.colorspace = V4L2_COLORSPACE_JPEG;

	mt9p111->format = fmt->format;

	return 0;
}

static int mt9p111_get_crop(struct v4l2_subdev *subdev,
		struct v4l2_subdev_fh *fh, struct v4l2_subdev_crop *crop)
{
	struct mt9p111 *mt9p111 = to_mt9p111(subdev);

	crop->rect = mt9p111->rect;
	return 0;
}

static int mt9p111_set_crop(struct v4l2_subdev *subdev,
		struct v4l2_subdev_fh *fh, struct v4l2_subdev_crop *crop)
{
	struct mt9p111 *mt9p111 = to_mt9p111(subdev);
	struct v4l2_rect rect;

	/*
	 * Only VGA resolution/window is supported
	 */
	rect.left = 0;
	rect.top = 0;
	rect.width = MT9P111_DEF_WIDTH;
	rect.height = MT9P111_DEF_HEIGHT;

	mt9p111->rect = rect;
	crop->rect = rect;

	return 0;
}

static const struct v4l2_subdev_core_ops mt9p111_core_ops = {
	.g_chip_ident = mt9p111_g_chip_ident,
	.s_config = mt9p111_s_config,
	.s_power = mt9p111_s_power,
};

static struct v4l2_subdev_file_ops mt9p111_subdev_file_ops = {
	.open		= mt9p111_open,
};

static const struct v4l2_subdev_video_ops mt9p111_video_ops = {
	.s_stream = mt9p111_s_stream,
};

static const struct v4l2_subdev_pad_ops mt9p111_pad_ops = {
	.enum_mbus_code	= mt9p111_enum_mbus_code,
	.enum_frame_size= mt9p111_enum_frame_size,
	.get_fmt	= mt9p111_get_pad_format,
	.set_fmt	= mt9p111_set_pad_format,
	.get_crop	= mt9p111_get_crop,
	.set_crop	= mt9p111_set_crop,
};

static const struct v4l2_subdev_ops mt9p111_ops = {
	.core	= &mt9p111_core_ops,
	.file	= &mt9p111_subdev_file_ops,
	.video	= &mt9p111_video_ops,
	.pad	= &mt9p111_pad_ops,
};


/*
 * mt9p111_probe - sensor driver i2c probe handler
 * @client: i2c driver client device structure
 *
 * Register sensor as an i2c client device and V4L2
 * sub-device.
 */
static int mt9p111_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct mt9p111 *mt9p111;
	int i, ret;

	/* Check if the adapter supports the needed features */
	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		v4l_err(client, "mt9p111: I2C Adapter doesn't support" \
				" I2C_FUNC_SMBUS_WORD\n");
		return -EIO;
	}
	if (!client->dev.platform_data) {
		v4l_err(client, "No platform data!!\n");
		return -ENODEV;
	}

	mt9p111 = kzalloc(sizeof(*mt9p111), GFP_KERNEL);
	if (mt9p111 == NULL) {
		v4l_err(client, "Could not able to alocate memory!!\n");
		return -ENOMEM;
	}

	mt9p111->pdata = client->dev.platform_data;

	/*
	 * Default configuration -
	 *	Resolution: VGA
	 *	Format: UYVY
	 *	crop = window
	 */
	mt9p111->rect.left = 0;
	mt9p111->rect.top = 0;
	mt9p111->rect.width = MT9P111_DEF_WIDTH;
	mt9p111->rect.height = MT9P111_DEF_HEIGHT;

	mt9p111->format.code = V4L2_MBUS_FMT_UYVY8_2X8;
	mt9p111->format.width = MT9P111_DEF_WIDTH;
	mt9p111->format.height = MT9P111_DEF_HEIGHT;
	mt9p111->format.field = V4L2_FIELD_NONE;
	mt9p111->format.colorspace = V4L2_COLORSPACE_JPEG;

	/*
	 * Register as a subdev
	 */
	v4l2_i2c_subdev_init(&mt9p111->subdev, client, &mt9p111_ops);
	mt9p111->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	/*
	 * Register as media entity
	 */
	mt9p111->pad.flags = MEDIA_PAD_FLAG_OUTPUT;
	ret = media_entity_init(&mt9p111->subdev.entity, 1, &mt9p111->pad, 0);
	if (ret < 0) {
		v4l_err(client, "failed to register as a media entity\n");
		kfree(mt9p111);
	}

	return ret;
}

/*
 * mt9p111_remove - Sensor driver i2c remove handler
 * @client: i2c driver client device structure
 *
 * Unregister sensor as an i2c client device and V4L2
 * sub-device.
 */
static int __exit mt9p111_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct mt9p111 *mt9p111 = to_mt9p111(subdev);

	v4l2_device_unregister_subdev(subdev);
	media_entity_cleanup(&subdev->entity);
	kfree(mt9p111);

	return 0;
}

static const struct i2c_device_id mt9p111_id[] = {
	{ MT9P111_MODULE_NAME, 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, mt9p111_id);

static struct i2c_driver mt9p111_i2c_driver = {
	.driver = {
		.name = MT9P111_MODULE_NAME,
		.owner = THIS_MODULE,
	},
	.probe = mt9p111_probe,
	.remove = __exit_p(mt9p111_remove),
	.id_table = mt9p111_id,
};

static int __init mt9p111_init(void)
{
	return i2c_add_driver(&mt9p111_i2c_driver);
}

static void __exit mt9p111_cleanup(void)
{
	i2c_del_driver(&mt9p111_i2c_driver);
}

module_init(mt9p111_init);
module_exit(mt9p111_cleanup);

MODULE_AUTHOR("iVeia, LLC");
MODULE_DESCRIPTION("MT9P111 camera sensor driver");
MODULE_LICENSE("GPL");
