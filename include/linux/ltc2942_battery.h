/*
 * Copyright (C) 2012 iVeia, LLC.
 * Author: Brian Silverman <bsilverman@iveia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LTC2942_BATTERY_H_
#define _LTC2942_BATTERY_H_

struct ltc2942_platform_data {
	int (*ac_online)(void);
	int (*usb_online)(void);
	int (*battery_status)(void);
	int (*battery_present)(void);
	int (*battery_health)(void);
	int (*battery_technology)(void);
};

#endif
