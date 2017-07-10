/*
 * Events driver
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <iveia/iv_device.h>

#include "events.h"
#include <linux/ioctl.h>

#define IV_EVENTS_DEVNO    (MKDEV(IV_EVENTS_DEV_MAJOR, 0))

/*
 * Mapping to vsoc_mem of registers
 */
#define EVENTS_MASK_REG 0
#define EVENTS_PENDING_REG 1
#define EVENTS_MASK_EN_REG 2

struct events_dev {
	struct semaphore sem;
	struct cdev cdev;
	volatile unsigned int * vsoc_mem;
	unsigned long start;
	unsigned long size;
	int opened;
	void * events_board_data;
	struct events_board_ops * ops;
	struct list_head filelist;
};

struct events_file {
	struct list_head list;
	struct events_dev * dev;
	wait_queue_head_t waitq;
	unsigned long pending;
	unsigned long pending_mask;
	int all_events;
};

static struct events_dev * events_devp;

#define ARE_REQUESTED_EVENTS_PENDING(f) \
	( f->all_events && (( f->pending & f->pending_mask ) == f->pending_mask )) \
	|| ( ! f->all_events && ( f->pending & f->pending_mask ))

#define ZYNQ_REGISTER_INT_STS 0xF800700C
#define ZYNQ_DONE_BIT 0x4

static int fpgaIsConfigured(void){

#if defined(CONFIG_MACH_IV_ATLAS_I_Z7E)
	unsigned int ulReg;	
	unsigned long address;
	//checking to make sure fpga is Configured  
	address = ioremap(ZYNQ_REGISTER_INT_STS, 4);

    ulReg = ioread32(address);	
    iounmap( (char *)address);

    ulReg = ulReg & ZYNQ_DONE_BIT;

    if (!ulReg)
        return 0;
    return 1;
    
#else
    return 1;
#endif
}

unsigned long events_get_events(struct events_dev * dev) 
{
	return (unsigned long)(dev->vsoc_mem[EVENTS_PENDING_REG]);
}

/*
 * Called from intterupt context to wake_up all read()ers.
 */
void events_wake_up(struct events_dev * dev, int event)
{
	struct list_head * plist;
	list_for_each(plist, &dev->filelist) {
		struct events_file * pevents_file = list_entry(plist, struct events_file, list);
		set_bit( event, &pevents_file->pending );
		if ( ARE_REQUESTED_EVENTS_PENDING(pevents_file)) {
			wake_up_interruptible( &pevents_file->waitq );
		}
	}
}
	

/*
 * For each pending event, clears it and wakes up read()ers.
 */
void events_foreach_wake_up(struct events_dev * dev)
{
    volatile unsigned int pending = dev->vsoc_mem[EVENTS_PENDING_REG];
	volatile unsigned int events = pending;
	int i;

	for ( i = 0; i < 31; i++ ) {
		if ( events & 1 )
			events_wake_up(dev, i);
		events >>= 1;
	}
    //Clear the events
    dev->vsoc_mem[EVENTS_PENDING_REG] = pending;

    //Disable all events from reporting interrupt
    dev->vsoc_mem[EVENTS_MASK_REG] = 0x00000000;
}


irqreturn_t events_isr(int irq, void *id)
{
	events_foreach_wake_up((struct events_dev *) id);
	return IRQ_HANDLED;
}


static void events_cleanup_open(struct events_dev * dev)
{
	if (dev->ops->free_irq)
		dev->ops->free_irq(dev->events_board_data);
}


/*
 * open()
 */
int events_open(struct inode *inode, struct file *filp)
{
	int err = 0;
	struct events_dev * dev;
	struct events_file * pevents_file = NULL;

	int success = 0;
	dev = container_of(inode->i_cdev, struct events_dev, cdev);

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

	/*
	 * Also, clear any pending events
	 */

    if (fpgaIsConfigured())
	    dev->vsoc_mem[EVENTS_PENDING_REG] = 0xFFFFFFFF;

    if (dev->opened != 0)
	    dev->ops->disable_irq(dev->events_board_data);

	pevents_file = kmalloc(sizeof(struct events_file), GFP_KERNEL);
	if (!pevents_file) {
		err = -ENOMEM;
		goto out;
	}

	init_waitqueue_head( &pevents_file->waitq );
	pevents_file->dev = dev;
	pevents_file->all_events = 0;
	pevents_file->pending = 0;
	pevents_file->pending_mask = 0;
	list_add(&pevents_file->list, &dev->filelist);
	filp->private_data = pevents_file;


	/*
	 * On first open(), allocate IRQ(s).
	 */
	if (dev->opened == 0) {
		if (dev->ops->request_irq) {
			err = dev->ops->request_irq(dev->events_board_data);
			if ( err ) goto out;
		}
	}else{ //if (dev->opened != 0)
	    dev->ops->enable_irq(dev->events_board_data);
    }

	dev->opened++;
	
	success = 1;

out:
	if ( ! success ) {
		if ( pevents_file ) {
			list_del(&pevents_file->list);
		}
		events_cleanup_open(dev);
	}
	up(&dev->sem);

	return err;
}


/*
 * close()
 */
int events_release(struct inode *inode, struct file *filp)
{
	struct events_file * pevents_file = filp->private_data; 
	struct events_dev * dev = pevents_file->dev;
	int err = 0;

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

	dev->ops->disable_irq(dev->events_board_data);
	list_del(&pevents_file->list);
	dev->ops->enable_irq(dev->events_board_data);

	dev->opened--;
	BUG_ON(dev->opened < 0);
	if (dev->opened == 0) {
		events_cleanup_open(dev);
	}

	up(&dev->sem);
	
	return err;
}

long events_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int err = 0;
	int retval = 0;
	struct events_file * pevents_file = filp->private_data; 
	struct events_dev * dev = pevents_file->dev;
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */

	if (_IOC_TYPE(cmd) != EVENTS_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > EVENTS_IOC_MAXNR) return -ENOTTY;
	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	if (_IOC_DIR(cmd) & _IOC_READ) {
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	} else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}
	if (err) return -EFAULT;

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;
	switch(cmd) {
		case EVENTS_IOC_R_INSTANCE_COUNT :			
			__put_user(dev->opened,(unsigned long __user *)arg);					
			break;		
		default:  /* redundant, as cmd was checked against MAXNR */
			retval = -ENOTTY;
			break;
	}
	up(&dev->sem);
	return retval;

}

/*
 * read()
 */
ssize_t events_read(struct file *filp, char __user *buf, size_t count,
				loff_t *f_pos)
{
	struct events_file * pevents_file = filp->private_data; 
	struct events_dev * dev = pevents_file->dev;
	ssize_t retval = 0;
	unsigned long events;

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

	//
	// Verify count/buf is word aligned, and at least a word big.
	//
	if (
		count < sizeof(unsigned long)
		|| count % sizeof(unsigned long) != 0 
		|| ((unsigned long) buf) % sizeof(unsigned long) != 0 
	) {
		retval = -EFAULT;
		goto out;
	}
	count = sizeof(unsigned long);

	//
	// read() works slightly different for blocking vs nonblocking calls.
	// read() always returns a bitmask of events.  But nonblocking calls return
	// the current events levels, whereas blocking calls return the level of
	// any pending events (and will block until there's at least one pending
	// event).
	//
	if (filp->f_flags & O_NONBLOCK) {
		events = events_get_events(dev);

	} else {
		//
		// Block until pending bits set, then clear them.  
		//
		// We're sending back the pending events to the app, so now we can
		// clear them.  We have to be careful, though to clear them atomically,
		// because we can be resetting them in the ISR.  We don't have to do
		// the whole thing atomically, because we're only clearing the bits
		// we're send back to the user.
		//
		int err;
		int i;
		up(&dev->sem);
		err = wait_event_interruptible(
			pevents_file->waitq, ARE_REQUESTED_EVENTS_PENDING(pevents_file)
			);
		if (err) return -ERESTARTSYS;
		if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

		events = pevents_file->pending & pevents_file->pending_mask;
		for (i = 0; i < 32; i++) {
			if ( events & ( 1 << i )) {
				clear_bit( i, &pevents_file->pending );
			}
		}
	}

	if (copy_to_user(buf, &events, count)) {
		retval = -EFAULT;
		goto out;
	}

	retval = count;

out:
	up(&dev->sem);
	return retval;
}


/*
 * write()
 *
 * A write consists of a u32 that is the bitmask of events to wait for.  The
 * high bit is special (and means we can really only use 31 events) - it
 * indicates, if set, to wait for all events, instead of any event.
 */
ssize_t events_write(struct file *filp, const char __user *buf, size_t count,
				loff_t *f_pos)
{
	struct events_file * pevents_file = filp->private_data; 
	struct events_dev * dev = pevents_file->dev;
	ssize_t retval = 0;
	unsigned long pending_mask;

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

	//
	// Verify count/buf is word aligned, and at least a word big.
	//
	if (
		count < sizeof(unsigned long)
		|| count % sizeof(unsigned long) != 0 
		|| ((unsigned long) buf) % sizeof(unsigned long) != 0 
	) {
		retval = -EFAULT;
		goto out;
	}
	count = sizeof(unsigned long);

	if (copy_from_user(&pending_mask, buf, count)) {
		retval = -EFAULT;
		goto out;
	}
	pevents_file->all_events = pending_mask & 0x80000000;
	pevents_file->pending_mask = pending_mask & ( ~ 0x80000000 );

    //Allow these events to cause interrupts
    dev->vsoc_mem[EVENTS_MASK_EN_REG] = (unsigned int)pending_mask;

	retval = count;
out:
	up(&dev->sem);
	return retval;
}


unsigned int events_poll(struct file *filp, poll_table *wait)
{
	struct events_file * pevents_file = filp->private_data; 
	unsigned int mask = 0;

	poll_wait(filp, &pevents_file->waitq, wait);
	if ( ARE_REQUESTED_EVENTS_PENDING(pevents_file)) {
		mask |= POLLIN | POLLRDNORM;
	}

	return mask;
}


struct file_operations events_fops = {
	.owner =    THIS_MODULE,
	.read =     events_read,
	.write =    events_write,
	.unlocked_ioctl = 	events_ioctl,
	.open =     events_open,
	.poll =     events_poll,
	.release =  events_release,
};


/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
void events_cleanup_module(struct events_dev * dev)
{
	if ( dev->vsoc_mem) {
		iounmap( (char *) dev->vsoc_mem );
	}

	/* Get rid of our char dev entries */
	if (dev) {
		cdev_del(&dev->cdev);
		kfree(dev);
	}

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(IV_EVENTS_DEVNO, 1);
}


static int __init events_init(void)
{
	int err;
	struct events_dev * dev = NULL;

	/*
	 * Get a range of minor numbers to work with, using static major.
	 */
	err = register_chrdev_region(IV_EVENTS_DEVNO, 1, MODNAME);
	if (err < 0) {
		goto fail;
	}

	events_devp = dev = kmalloc(sizeof(struct events_dev), GFP_KERNEL);
	if (!dev) {
		err = -ENOMEM;
		goto fail;
	}
	memset(dev, 0, sizeof(struct events_dev));
	
	sema_init(&dev->sem, 1);
	
	/*
	 * Set the event board ops, definied in board specific files.
	 */
	dev->ops = &events_board_ops;
	dev->events_board_data = NULL;
	if (dev->ops->init)
		dev->events_board_data = dev->ops->init(dev);

	cdev_init(&dev->cdev, &events_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &events_fops;
	err = cdev_add (&dev->cdev, IV_EVENTS_DEVNO, 1);
	if (err) {
		printk(KERN_WARNING MODNAME ": Error %d adding device", err);
		goto fail;
	}

	INIT_LIST_HEAD(&dev->filelist);
	dev->opened = 0;

	dev->start = dev->ops->vsoc_mem_base(dev->events_board_data);
	dev->size = dev->ops->vsoc_mem_size(dev->events_board_data);
	dev->vsoc_mem = (unsigned int *) ioremap( dev->start, dev->size );
	if ( dev->vsoc_mem == NULL ) {
		err = -ENOMEM;
		goto fail;
	}

	return 0;

fail:
	events_cleanup_module(dev);
	return err;
}
module_init(events_init);


static void __exit events_exit(void)
{
	events_cleanup_module(events_devp);
}
module_exit(events_exit);


MODULE_AUTHOR("iVeia, LLC");
MODULE_DESCRIPTION("Events char driver");
MODULE_LICENSE("GPL");
