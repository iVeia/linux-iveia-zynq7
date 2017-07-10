/*
 * IOMEM char driver
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
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include <iveia/iv_device.h>
#include "iomem.h"

#define IV_IOMEM_DEVNO    (MKDEV(IV_IOMEM_DEV_MAJOR, 0))

#define MODNAME "iomem"

struct iomem_dev * iomem_devp;

/*
 * open()
 */
int iomem_open(struct inode *inode, struct file *filp)
{
	struct iomem_dev * dev;
	dev = container_of(inode->i_cdev, struct iomem_dev, cdev);
	filp->private_data = dev;

	return 0;
}


/*
 * close()
 */
int iomem_release(struct inode *inode, struct file *filp)
{
	return 0;
}


/*
 * read()
 */
ssize_t iomem_read(struct file *filp, char __user *buf, size_t count,
				loff_t *f_pos)
{
	int ret;
	char * iomem = NULL;

	iomem = ioremap( *f_pos, count );
	if ( iomem == NULL ) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_to_user(buf, iomem, count)) {
		ret = -EFAULT;
		goto out;
	}

	*f_pos += count;
	ret = count;
out:
	if ( iomem ) {
		iounmap( iomem );
	}

	return ret;
}


/*
 * write()
 */
ssize_t iomem_write(struct file *filp, const char __user *buf, size_t count,
				loff_t *f_pos)
{
	int ret;
	char * iomem = NULL;

	iomem = ioremap( *f_pos, count );
	if ( iomem == NULL ) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(iomem, buf, count)) {
		ret = -EFAULT;
		goto out;
	}

	*f_pos += count;
	ret = count;
out:
	if ( iomem ) {
		iounmap( iomem );
	}

	return ret;
}


loff_t iomem_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t newpos;

	switch(whence) {
	  case 0: /* SEEK_SET */
		newpos = off;
		break;

	  case 1: /* SEEK_CUR */
		newpos = filp->f_pos + off;
		break;

	  default: 
		newpos = -EINVAL;
		goto out;
	}
	if (newpos < 0) {
		newpos = -EINVAL;
		goto out;
	}

	filp->f_pos = newpos;

out:
	return newpos;
}


struct file_operations iomem_fops = {
	.owner =    THIS_MODULE,
	.read =     iomem_read,
	.write =    iomem_write,
	.llseek =   iomem_llseek,
	.open =     iomem_open,
	.release =  iomem_release,
};


/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
void iomem_cleanup_module(void)
{
	/* Get rid of our char dev entries */
	if (iomem_devp) {
		cdev_del(&iomem_devp->cdev);
		kfree(iomem_devp);
	}

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(IV_IOMEM_DEVNO, 1);
}


static int __init iomem_init(void)
{
	int err;

	/*
	 * Get a range of minor numbers to work with, using static major.
	 */
	err = register_chrdev_region(IV_IOMEM_DEVNO, 1, MODNAME);
	if (err < 0) {
		goto fail;
	}

	iomem_devp = kmalloc(sizeof(struct iomem_dev), GFP_KERNEL);
	if (!iomem_devp) {
		err = -ENOMEM;
		goto fail;
	}
	memset(iomem_devp, 0, sizeof(struct iomem_dev));
	
	cdev_init(&iomem_devp->cdev, &iomem_fops);
	iomem_devp->cdev.owner = THIS_MODULE;
	iomem_devp->cdev.ops = &iomem_fops;
	err = cdev_add (&iomem_devp->cdev, IV_IOMEM_DEVNO, 1);
	if (err) {
		printk(KERN_NOTICE MODNAME ": Error %d adding device", err);
		goto fail;
	}

	return 0;

fail:
	iomem_cleanup_module();
	return err;
}
module_init(iomem_init);


static void __exit iomem_exit(void)
{
	iomem_cleanup_module();
}
module_exit(iomem_exit);


MODULE_AUTHOR("iVeia, LLC");
MODULE_DESCRIPTION("iomem char driver");
MODULE_LICENSE("GPL");
