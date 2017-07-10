/*
 * Events atlas-i-lpe driver
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
#include <linux/gpio.h>
//#include <mach/gpio.h>
#include "events.h"

#define VSOC_MEM_BASE       0x05000000
#define VSOC_MEM_SIZE       0x1000

#define IV_OCP_EVENTS_IRQ_GPIO  160

struct atlas_i_lpe_board_data {
	void * events_dev;
	int irq;
} atlas_i_lpe_board_data;

static void atlas_i_lpe_free_irq(void * board_data)
{
	struct atlas_i_lpe_board_data * board = board_data;
	if (board->irq >= 0) {
		free_irq(board->irq, board->events_dev);
		board->irq = -1;
	}

	gpio_free(IV_OCP_EVENTS_IRQ_GPIO);
}


static int atlas_i_lpe_request_irq(void * board_data)
{
	struct atlas_i_lpe_board_data * board = board_data;
	int ret;

	ret = gpio_request(IV_OCP_EVENTS_IRQ_GPIO, NULL);
	if (ret == 0) {
		ret = gpio_direction_input(IV_OCP_EVENTS_IRQ_GPIO);
	}
	if (ret < 0) {
		printk(KERN_ERR MODNAME ": Can't get GPIO for IRQ\n");
		return ret;
	}

	board->irq = gpio_to_irq(IV_OCP_EVENTS_IRQ_GPIO);
	ret = request_irq( 
		board->irq, (irq_handler_t) events_isr, IRQF_TRIGGER_RISING, MODNAME, board->events_dev);
	return ret;
}

static void atlas_i_lpe_enable_irq(void * board_data)
{
	struct atlas_i_lpe_board_data * board = board_data;
	enable_irq(board->irq);
}

static void atlas_i_lpe_disable_irq(void * board_data)
{
	struct atlas_i_lpe_board_data * board = board_data;
	disable_irq(board->irq);
}

static unsigned long atlas_i_lpe_vsoc_mem_base(void * board_data)
{
	return VSOC_MEM_BASE;
}


static unsigned long atlas_i_lpe_vsoc_mem_size(void * board_data)
{
	return VSOC_MEM_SIZE;
}


static void * atlas_i_lpe_init(void * events_dev)
{
	struct atlas_i_lpe_board_data * board = &atlas_i_lpe_board_data;
	board->events_dev = events_dev;
	board->irq = -1;
	return board;
}


struct events_board_ops events_board_ops = {
	.vsoc_mem_size	= atlas_i_lpe_vsoc_mem_size,
	.vsoc_mem_base	= atlas_i_lpe_vsoc_mem_base,
	.init			= atlas_i_lpe_init,
	.free_irq		= atlas_i_lpe_free_irq,
	.request_irq	= atlas_i_lpe_request_irq,
	.enable_irq		= atlas_i_lpe_enable_irq,
	.disable_irq		= atlas_i_lpe_disable_irq,
};
