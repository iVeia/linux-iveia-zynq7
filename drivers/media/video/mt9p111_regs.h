/*
 * drivers/media/video/mt9p111_regs.h
 *
 * Copyright (C) iVeia, LLC
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
#ifndef _MT9P111_REGS_H
#define _MT9P111_REGS_H

/*
 * MT9P111 registers
 */
#define REG_CHIP_ID				(0x00)

/* Tokens for register write */
#define TOK_WRITE8                      (0)     /* token for 8-bit value write */
#define TOK_WRITE16                     (1)     /* token for 16-bit value write */
#define TOK_TERM                        (2)     /* terminating token */
#define TOK_DELAY                       (3)     /* delay token for reg list */
#define TOK_SKIP                        (4)     /* token to skip a register */

/**
 * struct mt9p111_reg - Structure for TVP5146/47 register initialization values
 * @token - Token: TOK_WRITE, TOK_TERM etc..
 * @reg - Register offset
 * @val - Register Value for TOK_WRITE or delay in ms for TOK_DELAY
 */
struct mt9p111_reg {
	unsigned short token;
	unsigned short reg;
	unsigned short val;
};

#define MT9P111_CHIP_ID			(0x2880)

#endif				/* ifndef _MT9P111_REGS_H */
