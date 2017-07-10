/*
 * Events titan-ppcx driver
 *
 * (C) Copyright 2008-2010, iVeia, LLC
 *
 * IVEIA IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A
 * COURTESY TO YOU. BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
 * ONE POSSIBLE IMPLEMENTATION OF THIS FEATURE, APPLICATION OR STANDARD,
 * IVEIA IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION IS FREE
 * FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE FOR OBTAINING
 * ANY THIRD PARTY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.
 * IVEIA EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
 * THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO ANY
 * WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE FROM
 * CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.
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
#include <linux/interrupt.h>
#include <iveia/iv_titan_ppcx_intr.h>
#include "events.h"

#define VSOC_MEM_BASE       0xD0000000
#define VSOC_MEM_SIZE       0x1000

struct titan_ppcx_board_data {
	void * events_dev;
	int irq;
} titan_ppcx_board_data;

static void titan_ppcx_free_irq(void * board_data)
{
	struct titan_ppcx_board_data * board = board_data;
	if (board->irq >= 0) {
		iv_free_irq(board->irq, NULL);
	}
}

irqreturn_t titan_ppcx_events_isr(int irq, unsigned long bits, void *id)
	return events_isr(irq, id);
}

static int titan_ppcx_request_irq(void * board_data)
{
	struct titan_ppcx_board_data * board = board_data;
	return iv_request_irq(0x02000000, &board->irq, titan_ppcx_events_isr, "iv_events", board->events_dev);
}

static unsigned long titan_ppcx_vsoc_mem_base(void * board_data)
{
	return VSOC_MEM_BASE;
}


static unsigned long titan_ppcx_vsoc_mem_size(void * board_data)
{
	return VSOC_MEM_SIZE;
}


static void * titan_ppcx_init(void * events_dev)
{
	struct titan_ppcx_board_data * board = &titan_ppcx_board_data;
	board->events_dev = events_dev;
	board->irq = -1;
	return board;
}


struct events_board_ops events_board_ops = {
	.vsoc_mem_size	= titan_ppcx_vsoc_mem_size,
	.vsoc_mem_base	= titan_ppcx_vsoc_mem_base,
	.init			= titan_ppcx_init,
	.free_irq		= titan_ppcx_free_irq,
	.request_irq	= titan_ppcx_request_irq,
};
