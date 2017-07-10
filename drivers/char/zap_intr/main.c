/*
 * ZAP Interrupt char driver
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

#include <linux/list.h>
#include <linux/sched.h>
#include <asm/bitops.h>


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include <iveia/iv_device.h>
#include <plat/dma.h>

#include <linux/gpio.h>

#include "zap_intr.h"

#define VSOC_MEM_BASE	0x05400000
#define VSOC_MEM_SIZE	0x1000

#define VSOC_DATA_LEN_REG	0x00000104/4
#define VSOC_OOB_LEN_REG	0x0000010c/4

struct zap_intr_dev {
	struct cdev cdev;
	volatile unsigned long * vsoc_mem;
	unsigned long start;
	unsigned long size;
	struct zap_intr_file * file;
	wait_queue_head_t waitq;
	int irq;
	int oobEn;
	unsigned long ulDataLen;
	unsigned long ulOobLen;

};

#define IV_ZAP_INTR_DEVNO	(MKDEV(IV_ZAP_INTR_DEV_MAJOR, 0))

#define MODNAME "zap_intr"

#define IV_ZAP_INT_IRQ_GPIO  161

struct zap_intr_dev * zap_intr_devp;

struct zap_intr_file {
	struct zap_intr_dev * dev;
};

static irqreturn_t events_isr(int irq, void *id)
{
	struct zap_intr_dev * dev = (struct zap_intr_dev *) id;

	wake_up_interruptible( &dev->waitq );

	return IRQ_HANDLED;
}

static void events_cleanup_open(struct zap_intr_dev * dev)
{

	if (dev->irq >= 0) {
		free_irq(dev->irq, dev);
		dev->irq = -1;
	}

	gpio_free(IV_ZAP_INT_IRQ_GPIO);
}
/*
 * open()
 */
int zap_intr_open(struct inode *inode, struct file *filp)
{
	int err = 0;
	struct zap_intr_dev * dev;
	struct zap_intr_file * pevents_file = NULL;
	int success = 0;

	dev = container_of(inode->i_cdev, struct zap_intr_dev, cdev);

	pevents_file = kmalloc(sizeof(struct zap_intr_file), GFP_KERNEL);
	dev->file = pevents_file;
	pevents_file->dev = dev;
	dev->ulDataLen = 0;
	dev->ulOobLen = 0;

	err = gpio_request(IV_ZAP_INT_IRQ_GPIO, NULL);
	if (err == 0) {
		err = gpio_direction_input(IV_ZAP_INT_IRQ_GPIO);
	}
	if (err < 0) {
		printk(KERN_ERR MODNAME ": Can't get GPIO for IRQ\n");
		goto out;
	}

	dev->irq = gpio_to_irq(IV_ZAP_INT_IRQ_GPIO);
	err = request_irq( 
		dev->irq, (irq_handler_t) events_isr, IRQF_TRIGGER_RISING, MODNAME, dev);
	if ( err ) goto out;

	init_waitqueue_head( &dev->waitq );

	filp->private_data = pevents_file;
	success = 1;

out:
	if ( ! success ) {
		events_cleanup_open(dev);
	}

	return err;
}


/*
 * close()
 */
int zap_intr_release(struct inode *inode, struct file *filp)
{
	struct zap_intr_file * pevents_file = filp->private_data; 
	struct zap_intr_dev * dev = pevents_file->dev;

	events_cleanup_open(dev);

	return 0;
}


/*
 * read()
 */
ssize_t zap_intr_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct zap_intr_file * pevents_file = filp->private_data; 
	struct zap_intr_dev * dev = pevents_file->dev;
	ssize_t retval = 0;
	unsigned long ulDataLen;
	unsigned long ulOobLen;

	ulDataLen = dev->ulDataLen;
	ulOobLen = dev->ulOobLen;
	dev->ulDataLen = 0;
	dev->ulOobLen = 0;

	if (copy_to_user(buf, &ulDataLen, sizeof(unsigned long))) {
		retval = -EFAULT;
		goto out;
	}
	if (copy_to_user(buf+sizeof(unsigned long), &ulOobLen, sizeof(unsigned long))) {
		retval = -EFAULT;
		goto out;
	}

	retval = count;

out:
	return retval;
}


/*
 * write()
 */
ssize_t zap_intr_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct zap_intr_file * pevents_file = filp->private_data; 
	struct zap_intr_dev * dev = pevents_file->dev;
	unsigned long ulValue;

	if (copy_from_user(&ulValue, buf, count))
		return -EFAULT;

	dev->oobEn = (ulValue & ZAP_INT_OOB_EN);

	return count;
}


unsigned int zap_intr_poll(struct file *filp, poll_table *wait)
{
	//assumes it will wait forever
	struct zap_intr_file * pevents_file = filp->private_data; 
	struct zap_intr_dev * dev = pevents_file->dev;
	unsigned int mask = 0;
	int bDataAvailable;

	poll_wait(filp, &dev->waitq, wait);

	if (dev->oobEn){
		if (dev->ulOobLen == 0)
			dev->ulOobLen = dev->vsoc_mem[VSOC_OOB_LEN_REG];
		if (dev->ulDataLen == 0)
			dev->ulDataLen = dev->vsoc_mem[VSOC_DATA_LEN_REG];
		bDataAvailable = (dev->ulDataLen != 0 && dev->ulOobLen != 0);
	}else{
		if (dev->ulDataLen == 0)
			dev->ulDataLen = dev->vsoc_mem[VSOC_DATA_LEN_REG];
		bDataAvailable = (dev->ulDataLen != 0);
	}

	if (bDataAvailable)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

struct file_operations zap_intr_fops = {
	.owner =	THIS_MODULE,
	.read =	 zap_intr_read,
	.write =	zap_intr_write,
	.poll =	 zap_intr_poll,
	.open =	 zap_intr_open,
	.release =  zap_intr_release,
};

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
void zap_intr_cleanup_module(struct zap_intr_dev * dev)
{

	if ( dev->vsoc_mem) {
		iounmap( (char *) dev->vsoc_mem );
	}

	/* Get rid of our char dev entries */
	if (zap_intr_devp) {
		cdev_del(&zap_intr_devp->cdev);
		kfree(zap_intr_devp);
	}

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(IV_ZAP_INTR_DEVNO, 1);
}

static int __init zap_intr_init(void)
{
	int err;
	struct zap_intr_dev * dev = NULL;

	/*
	 * Get a range of minor numbers to work with, using static major.
	 */
	err = register_chrdev_region(IV_ZAP_INTR_DEVNO, 1, MODNAME);
	if (err < 0) {
		goto fail;
	}

	zap_intr_devp = dev = kmalloc(sizeof(struct zap_intr_dev), GFP_KERNEL);
	if (!dev) {
		err = -ENOMEM;
		goto fail;
	}
	memset(dev, 0, sizeof(struct zap_intr_dev));

	cdev_init(&dev->cdev, &zap_intr_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &zap_intr_fops;
	err = cdev_add (&dev->cdev, IV_ZAP_INTR_DEVNO, 1);
	if (err) {
		printk(KERN_WARNING MODNAME ": Error %d adding device", err);
		goto fail;
	}
	dev->start = VSOC_MEM_BASE;
	dev->size = VSOC_MEM_SIZE;
	dev->vsoc_mem = (unsigned long *) ioremap( dev->start, dev->size );
	if ( dev->vsoc_mem == NULL ) {
		err = -ENOMEM;
		goto fail;
	}

	return 0;

fail:
	zap_intr_cleanup_module(dev);
	return err;
}
module_init(zap_intr_init);


static void __exit zap_intr_exit(void)
{
	zap_intr_cleanup_module(zap_intr_devp);
}
module_exit(zap_intr_exit);


MODULE_AUTHOR("iVeia, LLC");
MODULE_DESCRIPTION("ZAP interrupt char driver");
MODULE_LICENSE("GPL");
