/*
 * LSM303DLM magnetometer driver
 *
 * Copyright (C) 2013 iVeia, LLC
 * Author: Brian Silverman <bsilverman@iveia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LSM303DLM_MAG_H__
#define __LSM303DLM_MAG_H__

#include <linux/types.h>

struct lsm303dlm_mag_platform_data {

	int poll_interval;

	u8 negate_x;
	u8 negate_y;
	u8 negate_z;

	u8 swap_xy;

	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(void);
	int (*power_off)(void);
};

#endif 


