/*
 * iVeia Device Header
 *
 * (C) Copyright 2008, iVeia, LLC
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

#if !defined IV_DEV_NO_H_
#define IV_DEV_NO_H_


enum IV_MAJOR_DEV_NO
{
    IV_SPI_DEV_MAJOR = 3000,
    IV_HH1_PWR_BTN_DEV_MAJOR = 3001,
    IV_AUDIO_DEV_MAJOR = 3002,
    IV_BITBANGED_I2C_DEV_MAJOR = 3003,
    IV_HH1_BUTTONS_DEV_MAJOR = 3004,
    IV_IOMEM_DEV_MAJOR = 3005,
    IV_EVENTS_DEV_MAJOR = 3006,
    IV_OCP_DEV_MAJOR = 3007,
    IV_ZAP_DEV_MAJOR = 3008,
    IV_ZAP_INTR_DEV_MAJOR = 3009,
};

#endif  /* IV_DEV_NO_H_ */
