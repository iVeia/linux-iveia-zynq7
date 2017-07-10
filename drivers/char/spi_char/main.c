/*
 * SPI char driver
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
#include <linux/proc_fs.h>
#include <linux/dma-mapping.h>
#include <linux/spi/spi.h>
#include <iveia/iv_device.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include "spi_char.h"

struct spi_char_dev * spi_char_devp;

#define MODNAME "gfspi"


#define TXRX_BUF_SIZE               (16)
#define IV_GFSPI_MAX_DEVS           (16)
#define IV_GFSPI_MINOR_CS_MASK      (0x0F)
#define IV_GFSPI_CFG_MINOR_OFFSET   (0x80)
#define IV_GFSPI_NUM_MINORS         (IV_GFSPI_CFG_MINOR_OFFSET + IV_GFSPI_MAX_DEVS)

#define IV_GFSPI_MODE_DEFAULT           (0)
#define IV_GFSPI_MAX_SPEED_HZ_DEFAULT   (1000000)

struct spi_char_dev {
	struct semaphore sem;
	struct cdev cdev;
	char * app_regs;
	struct spi_device * spi;

	struct {
		unsigned char rx_buf[TXRX_BUF_SIZE];
		unsigned int count;
		unsigned int offset;
		unsigned long cfg_to_read;
		unsigned long cfg[IV_GFSPI_NUM_CFG_WORDS];
	} chip[IV_GFSPI_MAX_DEVS];
};

static int spi_char_probe(struct spi_device *spi);
static int __devexit spi_char_remove(struct spi_device *spi);
static struct spi_driver spi_char_driver = {
	.driver = {
		.name =     "gf_spi",
		.bus =      &spi_bus_type,
		.owner =    THIS_MODULE,
	},
	.probe =    spi_char_probe,
	.remove =   __devexit_p(spi_char_remove),
};


/*
 * open()
 */
static int spi_char_open(struct inode *inode, struct file *filp)
{
	struct spi_char_dev *dev;
	dev = container_of(inode->i_cdev, struct spi_char_dev, cdev);
	filp->private_data = dev;

	return 0;
}


/*
 * close()
 */
static int spi_char_release(struct inode *inode, struct file *filp)
{
	return 0;
}


static ssize_t cfg_read(struct spi_char_dev *dev, int cs, char __user *buf, size_t count)
{
	unsigned long cfg = dev->chip[cs].cfg_to_read;
	
	if ( count < sizeof(unsigned long)) return -EINVAL;
	count = sizeof(unsigned long);

	if (copy_to_user(buf, (char *) &dev->chip[cs].cfg[cfg], count)) return -EFAULT;

	return count;
}


static ssize_t data_read(struct spi_char_dev *dev, int cs, char __user *buf, size_t count)
{
	if (count > dev->chip[cs].count) {
		count = dev->chip[cs].count;
	}

	if (copy_to_user(buf, dev->chip[cs].rx_buf + dev->chip[cs].offset, count)) return -EFAULT;
	dev->chip[cs].count -= count;
	dev->chip[cs].offset += count;

	return count;
}


/*
 * read()
 */
static ssize_t spi_char_read(struct file *filp, char __user *buf, size_t count,
				loff_t *f_pos)
{
	struct spi_char_dev *dev = filp->private_data; 
	unsigned int minor = iminor(filp->f_dentry->d_inode);
	int cs = minor & IV_GFSPI_MINOR_CS_MASK;
	int is_cfg = ( minor >= IV_GFSPI_CFG_MINOR_OFFSET );
	int ret = 0;

	if ( cs >= IV_GFSPI_MAX_DEVS ) return -ENODEV;

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

	if ( is_cfg ) {
		ret = cfg_read(dev, cs, buf, count);
	} else {
		ret = data_read(dev, cs, buf, count);
	}

	up(&dev->sem);
	return ret;
}


static ssize_t cfg_write(struct spi_char_dev *dev, int cs, const char __user *buf, size_t count)
{
	unsigned long data[2];
	
	if ( count != sizeof(unsigned long) && count != 2 * sizeof(unsigned long)) return -EINVAL;

	if (copy_from_user(data, buf, count)) return -EFAULT;

	/*
	 * User has written on word or two.  If one, they're doing a cfg read (and
	 * just writing the reg num), if 2 words, then a cfg write (reg num
	 * followed by val).
	 */
	{
		unsigned long cfg = data[0];
		unsigned long val = data[1];
		if ( cfg >= IV_GFSPI_NUM_CFG_WORDS ) return -EINVAL;

		if ( count == sizeof(unsigned long)) {
			dev->chip[cs].cfg_to_read = cfg;
		} else {
			dev->chip[cs].cfg[cfg] = val;
		}
	}

	return count;
}


static int enable_spi_chip( struct spi_char_dev * dev, int cs )
{
	int err = 0;

	dev->spi->mode = (u8) dev->chip[cs].cfg[IV_GFSPI_MODE];
	dev->spi->max_speed_hz = (u32) dev->chip[cs].cfg[IV_GFSPI_MAX_SPEED_HZ];
	dev->spi->chip_select = cs;
	dev->spi->bits_per_word = 8;

	err = spi_setup( dev->spi );
	if (err < 0) return err;

	return 0;
}


static void disable_spi_chip( struct spi_char_dev * dev )
{
}


/*
 * _iv_hh1_spi_xfer() - internal xfer function.  
 *
 * dev->sem must have been acquired before calling.
 */
static int _iv_hh1_spi_xfer(
	struct spi_char_dev * dev, int cs, char * tx_buf, char * rx_buf, int count 
	)
{
	struct spi_transfer t;
	struct spi_message m;
	int err;

	if ( cs >= IV_GFSPI_MAX_DEVS ) return -ENODEV;

	memset(&t, 0, sizeof(t));
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	t.len = count;

	err = enable_spi_chip(dev, cs);
	if ( err < 0 ) return -EIO;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	err = spi_sync(dev->spi, &m);
	if (err < 0) return -EIO;

	disable_spi_chip(dev);

	return count;
}


static ssize_t data_write(struct spi_char_dev *dev, int cs, const char __user *buf, size_t count)
{
	unsigned char tx_buf[TXRX_BUF_SIZE];
	int ret;

	/* 
	 * Verify there's enough space to send the data AND read it back.
	 */
	if (count > TXRX_BUF_SIZE ) return -ENOSPC;
	if (copy_from_user(tx_buf, buf, count)) return -EFAULT;

	/*
	 * Flush any previous responses
	 */
	dev->chip[cs].count = 0;
	dev->chip[cs].offset = 0;

	ret = _iv_hh1_spi_xfer( dev, cs, tx_buf, dev->chip[cs].rx_buf, count );
	dev->chip[cs].count = (ret < 0) ? 0 : ret;

	return ret < 0 ? ret : count;
}


/*
 * write()
 */
static ssize_t spi_char_write(struct file *filp, const char __user *buf, size_t count,
				loff_t *f_pos)
{
	struct spi_char_dev *dev = filp->private_data; 
	unsigned int minor = iminor(filp->f_dentry->d_inode);
	int cs = minor & IV_GFSPI_MINOR_CS_MASK;
	int is_cfg = ( minor >= IV_GFSPI_CFG_MINOR_OFFSET );
	int ret = 0;

	if ( cs >= IV_GFSPI_MAX_DEVS ) return -ENODEV;

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

	if ( is_cfg ) {
		ret = cfg_write(dev, cs, buf, count);
	} else {
		ret = data_write(dev, cs, buf, count);
	}

	up(&dev->sem);
	return ret;
}


/*
 * ioctl() 
 *
 * Deprecated.  Used for legacy access to GF220 devices.  New devices should
 * use read/write() interface.
 */
static int spi_char_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, 
							unsigned long arg)
{

	int err = 0;
	int retval = 0;
	struct reg_spec reg;
	struct spi_char_dev * dev = filp->private_data;
	
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != GFSPI_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > GFSPI_IOC_MAXNR) return -ENOTTY;

	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;

	switch(cmd) {
		case GFSPI_IOCREADREG:
			if (copy_from_user(&reg, (void __user *) arg, sizeof(reg))) {
				retval = -EFAULT;
				break;
			}
			/*
			 * The proper way to do ioremap()ed read/writes is via
			 * ioread32()/iowrite32().  Nevertheless, its not working on the
			 * app_reg space.  FIXME?
			 *
			 * FIXME: need bounds checking on offset.
			 */
			//reg.val = ioread32(dev->app_regs[reg_set] + offset);
			reg.val = *((unsigned long *) (dev->app_regs + reg.offset));
			if (copy_to_user((void __user *) arg, &reg, sizeof(reg))) {
				retval = -EFAULT;
				break;
			}
			break;

		case GFSPI_IOCWRITEREG:
			if (copy_from_user(&reg, (void __user *) arg, sizeof(reg))) {
				retval = -EFAULT;
				break;
			}
			/* 
			 * See ioread32()/iowrite32() comment above.
			 */
			//iowrite32(reg.val, dev->app_regs + offset);
			*((unsigned long *) (dev->app_regs + reg.offset)) = reg.val;
			break;

		default:  /* redundant, as cmd was checked against MAXNR */
			return -ENOTTY;
	}
	return retval;

}


struct file_operations spi_char_fops = {
	.owner =    THIS_MODULE,
	.read =     spi_char_read,
	.write =    spi_char_write,
	.ioctl =    spi_char_ioctl,
	.open =     spi_char_open,
	.release =  spi_char_release,
};


/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
static void spi_char_cleanup_module(void)
{
	spi_unregister_driver(&spi_char_driver);

	/* Get rid of our char dev entries */
	if (spi_char_devp) {
		cdev_del(&spi_char_devp->cdev);
		kfree(spi_char_devp);
	}

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(MKDEV(IV_SPI_DEV_MAJOR, 0), 1);
}


static int spi_char_probe(struct spi_device *spi)
{
	int         status;

	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;

	status = spi_setup(spi);
	if (status < 0) {
		printk(KERN_WARNING MODNAME ": spi_setup() failure (%d)\n", status);
		return status;
	}

	if ( spi_char_devp == NULL ) {
		printk(KERN_ERR MODNAME ": spi_char_probe() failure\n" );
		return -EINVAL;
	}

	spi_char_devp->spi = spi;
	
	return 0;
}


static int __devexit spi_char_remove(struct spi_device *spi)
{
	return 0;
}


static int __init spi_char_init(void)
{
	int err;

	/*
	 * Register major/minor devno
	 */
	err = register_chrdev_region(MKDEV(IV_SPI_DEV_MAJOR, 0), IV_GFSPI_NUM_MINORS, MODNAME);
	if (err < 0) {
		printk(KERN_WARNING MODNAME ": can't get major %d\n", IV_SPI_DEV_MAJOR);
		goto fail;
	}

	/*
	 * Alloc dev and initialize
	 */
	spi_char_devp = kmalloc(sizeof(struct spi_char_dev), GFP_KERNEL);
	if (!spi_char_devp) {
		err = -ENOMEM;
		goto fail;
	}
	memset(spi_char_devp, 0, sizeof(struct spi_char_dev));
	{
		int i;
		for ( i = 0; i < IV_GFSPI_MAX_DEVS; i++ ) {
			spi_char_devp->chip[i].cfg[IV_GFSPI_MAX_SPEED_HZ] = IV_GFSPI_MAX_SPEED_HZ_DEFAULT;
			spi_char_devp->chip[i].cfg[IV_GFSPI_MODE] = IV_GFSPI_MODE_DEFAULT;
		}
	}
	sema_init(&spi_char_devp->sem, 1);

	/*
	 * Register char dev
	 */
	cdev_init(&spi_char_devp->cdev, &spi_char_fops);
	spi_char_devp->cdev.owner = THIS_MODULE;
	spi_char_devp->cdev.ops = &spi_char_fops;
	err = cdev_add(&spi_char_devp->cdev, MKDEV(IV_SPI_DEV_MAJOR, 0), IV_GFSPI_NUM_MINORS);
	if (err) {
		printk(KERN_NOTICE MODNAME ": Error %d adding char device", err);
		goto fail;
	}

	spi_char_devp->app_regs = ioremap(
		0x40501000, 0x1000
		);
	if (spi_char_devp->app_regs == NULL) {
		printk(KERN_NOTICE MODNAME ": Error mapping app_regs address space\n");
		goto fail;
	}

	return spi_register_driver(&spi_char_driver);

fail:
	spi_char_cleanup_module();
	return err;
}
module_init(spi_char_init);


static void __exit spi_char_exit(void)
{
	spi_char_cleanup_module();
}
module_exit(spi_char_exit);


MODULE_AUTHOR("iVeia, LLC");
MODULE_DESCRIPTION("SPI char driver");
MODULE_LICENSE("GPL");
