#ifndef _EVENTS_H_
#define _EVENTS_H_

#include "_events.h"
struct events_board_ops {
	void * (*init) (void * events_dev);
	unsigned int (*vsoc_mem_base) (void * events_board_data);   /* Mandatory */
	unsigned int (*vsoc_mem_size) (void * events_board_data);   /* Mandatory */
	int (*request_irq) (void * events_board_data);
	void (*free_irq) (void * events_board_data);
	void (*enable_irq) (void * events_board_data);
	void (*disable_irq) (void * events_board_data);
};

extern struct events_board_ops events_board_ops;

struct events_dev;

extern unsigned long events_get_events(struct events_dev * dev);
extern void events_wake_up(struct events_dev * dev, int event);
extern irqreturn_t events_isr(int irq, void *id);
extern void events_foreach_wakeup(struct events_dev * dev);

#define MODNAME "iv_events"

#endif
