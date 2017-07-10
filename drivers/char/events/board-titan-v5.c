/*
 * Events titan-v5 driver
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
#include <platforms/4xx/xparameters/xparameters.h>
#include "events.h"

#define NUM_IRQS 4
#define LAST_IRQ		XPAR_DCR_INTC_0_SYSTEM_I_EVENTS_0_INTR
#define FIRST_IRQ		(LAST_IRQ - NUM_IRQS + 1)

#define VSOC_MEM_BASE   XPAR_IVEIA_XPS_REGISTERS_PPC_0_INST_0_BASEADDR
#define VSOC_MEM_HIGH   XPAR_IVEIA_XPS_REGISTERS_PPC_0_INST_0_HIGHADDR
#define VSOC_MEM_SIZE   (VSOC_MEM_HIGH - VSOC_MEM_BASE + 1)

#error The Events driver for Titan-V5 has no been tested since the split to board* files.

struct titan_v5_board_data {
	void * events_dev;
	int irqs[NUM_IRQS];
	unsigned long reenable_irq_bitmask;
	struct delayed_work reenable_irq_work;
} titan_v5_board_data;

static void reenable_irq(struct work_struct * work)
{
	struct titan_v5_board_data * board;
	int irq;

	board = container_of(work, struct titan_v5_board_data, reenable_irq_work.work);

	for ( irq = FIRST_IRQ; irq <= LAST_IRQ; irq++ ) {
		if (test_and_clear_bit(irq, &board->reenable_irq_bitmask)) {
			enable_irq(irq);
		}
	}
}

static irqreturn_t events_titan_v5_isr(int irq, void * dev_id)
{
	int event;
	struct titan_v5_board_data * board = (struct titan_v5_board_data *) dev_id;
	
	event = LAST_IRQ - irq;

	/*
	 * Restart events
	 */
	if (events_get_events(board->events_dev) & (1 << event)) {
		disable_irq_nosync(irq);
		schedule_delayed_work( &board->reenable_irq_work, 1 );
		set_bit(irq, &board->reenable_irq_bitmask);
	}

	events_wake_up(board->events_dev, event);

	return IRQ_HANDLED;
}


static void titan_v5_free_irq(void * board_data)
{
	struct titan_v5_board_data * board = board_data;
	int i;

	for ( i = 0; i < NUM_IRQS; i++ ) {
		if (board->irqs[i] >= 0) {
			free_irq( board->irqs[i], (void *) board );
			board->irqs[i] = -1;
		}
	}
}


static int titan_v5_request_irq(void * board_data)
{
	struct titan_v5_board_data * board = board_data;
	int i;
	int err = 0;

	for ( i = 0; i < NUM_IRQS; i++ ) {
		err = request_irq( 
			FIRST_IRQ + i, events_titan_v5_isr, 0, "iv_events", (void *) board_data );
		if ( err ) goto out;
		board->irqs[i] = FIRST_IRQ + i;
	}

out:
	titan_v5_free_irq(board_data);
	return err;
}



static unsigned long titan_v5_vsoc_mem_base(void * board_data)
{
	return VSOC_MEM_BASE;
}


static unsigned long titan_v5_vsoc_mem_size(void * board_data)
{
	return VSOC_MEM_SIZE;
}


static void * titan_v5_init(void * events_dev)
{
	struct titan_v5_board_data * board = &titan_v5_board_data;
	int i;

	board->events_dev = events_dev;

	INIT_DELAYED_WORK(&board->reenable_irq_work, reenable_irq);

	for ( i = 0; i < NUM_IRQS; i++ ) {
		board->irqs[i] = -1;
	}

	return board;
}


struct events_board_ops events_board_ops = {
	.vsoc_mem_size	= titan_v5_vsoc_mem_size,
	.vsoc_mem_base	= titan_v5_vsoc_mem_base,
	.init			= titan_v5_init,
	.free_irq		= titan_v5_free_irq,
	.request_irq	= titan_v5_request_irq,
};
