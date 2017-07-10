/*
 * Titan-PPCx GF SPI driver
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
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/dma-mapping.h>
#include <linux/spi/spi.h>
#include <iveia/iv_device.h>
#include "iv_atlas_i_lpe_gfspi.h"

#define IV_GFSPI_DEVNO MKDEV(IV_SPI_DEV_MAJOR, 0)
#define MODNAME "iv_atlas_i_lpe_spi"

#define IV_GFSPI_NUM_CS_LINES 2
#ifdef CONFIG_IVEIA_GFSPI_MUXED_CS
	#define IV_GFSPI_MUXED_CS 1
	#define IV_GFSPI_NUM_DEVS (1 << IV_GFSPI_NUM_CS_LINES)
#else
	#define IV_GFSPI_MUXED_CS 1
	#define IV_GFSPI_NUM_DEVS IV_GFSPI_NUM_CS_LINES
#endif
#define IV_GFSPI_MINOR_CS_MASK      (0x0F)
#define IV_GFSPI_CFG_MINOR_OFFSET   (0x80)
#define IV_GFSPI_NUM_MINORS         (IV_GFSPI_CFG_MINOR_OFFSET + IV_GFSPI_NUM_DEVS)
#define IV_GFSPI_MODE_DEFAULT           (0)
#define IV_GFSPI_MAX_SPEED_HZ_DEFAULT   (1000000)

#define TXRX_BUF_SIZE (16)
struct iv_atlas_i_lpe_spi_dev {
	struct semaphore sem;
	struct cdev cdev;
	struct {
		unsigned char rx_buf[TXRX_BUF_SIZE];
		unsigned int count;
		unsigned long cfg_to_read;
		unsigned long cfg[IV_GFSPI_NUM_CFG_WORDS];
	} chip[IV_GFSPI_NUM_DEVS];
	char * gpio_bank2_base;
};

static struct iv_atlas_i_lpe_spi_dev * iv_atlas_i_lpe_spi_devp = NULL;

/*
 * Bitbanged SPI
 *
 * This bitbanged use of SPI has been hacked in because of issues using the
 * hardware SPI bus.  If those issues are resolved, this can be reverted back
 * to using the hardware SPI bus.
 */
#define GPIO_BANK2_THRU_BANK6_PHYS 0x49050000
#define GPIO_BANK2_THRU_BANK6_LEN 0xA000
#define GPIO_BANK_REG_SPACING 0x2000
#define GPIO_BANK_REG_OE 0x0034
#define GPIO_BANK_REG_DATAIN 0x0038
#define GPIO_BANK_REG_DATAOUT 0x003C

/*
 * GPIO banks are 32 bits.  GPIO bank 2 is GPIO_32 thru 63.  
 */
#define GPIO_REG_BANK_OFFSET(gpio) (GPIO_BANK_REG_SPACING * ( g/32 - 1 ))

#define IVSPI_CLK                               88
#define IVSPI_SIMO                              89
#define IVSPI_SOMI                              90
#define IVSPI_CS0                               91
#define IVSPI_CS1                               92

static void gpio_set_oe(struct iv_atlas_i_lpe_spi_dev * dev, int g, int oe)
{
	unsigned long reg_offset;
	unsigned long * p;

	reg_offset = GPIO_REG_BANK_OFFSET(g) + GPIO_BANK_REG_OE;
	g = g % 32;

	p = (unsigned long *) (dev->gpio_bank2_base + reg_offset);
	// note: OE register bits are inverted (0 is output)
	if (!oe) {
		writel(readl(p) | (1 << g), p);
	} else {
		writel(readl(p) & ~(1 << g), p);
	}
}

static void gpio_set_as_output(struct iv_atlas_i_lpe_spi_dev * dev, int g)
{
	gpio_set_oe(dev, g, 1);
}

static void gpio_set_as_input(struct iv_atlas_i_lpe_spi_dev * dev, int g)
{
	gpio_set_oe(dev, g, 0);
}

static int gpio_read(struct iv_atlas_i_lpe_spi_dev * dev, unsigned long g)
{
	unsigned long reg_offset;
	unsigned long * p;

	reg_offset = GPIO_REG_BANK_OFFSET(g) + GPIO_BANK_REG_DATAIN;
	g = g % 32;

	p = (unsigned long *) (dev->gpio_bank2_base + reg_offset);
	return (readl(p) & (1 << g));
}

static void gpio(struct iv_atlas_i_lpe_spi_dev * dev, unsigned long g, int set)
{
	unsigned long reg_offset;
	unsigned long * p;

	reg_offset = GPIO_REG_BANK_OFFSET(g) + GPIO_BANK_REG_DATAOUT;
	g = g % 32;

	p = (unsigned long *) (dev->gpio_bank2_base + reg_offset);
	if (set) {
		writel(readl(p) | (1 << g), p);
	} else {
		writel(readl(p) & ~(1 << g), p);
	}
}

static void gpio_low(struct iv_atlas_i_lpe_spi_dev * dev, unsigned long g)
{
	gpio(dev, g, 0);
}

static void gpio_high(struct iv_atlas_i_lpe_spi_dev * dev, unsigned long g)
{
	gpio(dev, g, 1);
}

static void half_clk_delay(struct iv_atlas_i_lpe_spi_dev * dev, int cs)
{
	/*
	 * Max speed is fixed by 1 microsecond delay.  Max is 1/1us = 1MHz.  So,
	 * half_clk_delay max is 500kHz.
	 */
	unsigned long speed = min(dev->chip[cs].cfg[IV_GFSPI_MAX_SPEED_HZ], 500000UL);
	udelay(1000000 / speed / 2);
}

static void gpio_init(struct iv_atlas_i_lpe_spi_dev * dev)
{
	gpio_high(dev, IVSPI_CS0);
	gpio_high(dev, IVSPI_CS1);
	gpio_set_as_output(dev, IVSPI_CLK);
	gpio_set_as_output(dev, IVSPI_SIMO);
	gpio_set_as_input(dev, IVSPI_SOMI);
	gpio_set_as_output(dev, IVSPI_CS0);
	gpio_set_as_output(dev, IVSPI_CS1);
}

/*
 * spi_send() - bitbanged SPI send
 */
static void spi_send(
	struct iv_atlas_i_lpe_spi_dev * dev, int cs, char * dataout, char * datain, int len
	)
{
	int i, j;
	int CPHA, CPOL;

	CPHA = dev->chip[cs].cfg[IV_GFSPI_MODE] & SPI_CPHA;
	CPOL = dev->chip[cs].cfg[IV_GFSPI_MODE] & SPI_CPOL;

	for (i = 0; i < len; i ++) {
		datain[i] = 0;

		/* Shift out the value, most-significant-bit first */
		for ( j = 7; j >= 0; j-- ) {
			if ( CPHA ) {
				gpio(dev, IVSPI_CLK, CPOL);
				half_clk_delay(dev, cs);

				gpio(dev, IVSPI_SIMO, dataout[i] & (1<<j));
				gpio(dev, IVSPI_CLK, ! CPOL);
				half_clk_delay(dev, cs);
				datain[i] <<= 1;
				datain[i] |= gpio_read(dev, IVSPI_SOMI) ? 1 : 0;

			} else {
				gpio(dev, IVSPI_SIMO, dataout[i] & (1<<j));
				gpio(dev, IVSPI_CLK, CPOL);
				half_clk_delay(dev, cs);

				datain[i] <<= 1;
				datain[i] |= gpio_read(dev, IVSPI_SOMI) ? 1 : 0;
				gpio(dev, IVSPI_CLK, ! CPOL);
				half_clk_delay(dev, cs);
			}
		}
	}
	gpio(dev, IVSPI_CLK, CPOL);
}


/*
 * open()
 */
int iv_atlas_i_lpe_spi_open(struct inode *inode, struct file *filp)
{
	struct iv_atlas_i_lpe_spi_dev *dev;
	dev = container_of(inode->i_cdev, struct iv_atlas_i_lpe_spi_dev, cdev);
	filp->private_data = dev;

	return 0;
}


/*
 * close()
 */
int iv_atlas_i_lpe_spi_release(struct inode *inode, struct file *filp)
{
	return 0;
}


static ssize_t cfg_read(struct iv_atlas_i_lpe_spi_dev *dev, int cs, char __user *buf, size_t count)
{
	unsigned long cfg = dev->chip[cs].cfg_to_read;
	
	if ( count < sizeof(unsigned long)) return -EINVAL;
	count = sizeof(unsigned long);

	if (copy_to_user(buf, (char *) &dev->chip[cs].cfg[cfg], count)) return -EFAULT;

	return count;
}


static ssize_t data_read(struct iv_atlas_i_lpe_spi_dev *dev, int cs, char __user *buf, size_t count)
{
	if (count > dev->chip[cs].count) {
		count = dev->chip[cs].count;
	}

	if (copy_to_user(buf, dev->chip[cs].rx_buf, count)) return -EFAULT;
	dev->chip[cs].count -= count;

	return count;
}


/*
 * read()
 */
ssize_t iv_atlas_i_lpe_spi_read(struct file *filp, char __user *buf, size_t count,
				loff_t *f_pos)
{
	struct iv_atlas_i_lpe_spi_dev *dev = filp->private_data; 
	unsigned int minor = iminor(filp->f_dentry->d_inode);
	int cs = minor & IV_GFSPI_MINOR_CS_MASK;
	int is_cfg = ( minor >= IV_GFSPI_CFG_MINOR_OFFSET );
	int ret = 0;

	if ( cs >= IV_GFSPI_NUM_DEVS ) return -ENODEV;

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

	if ( is_cfg ) {
		ret = cfg_read(dev, cs, buf, count);
	} else {
		ret = data_read(dev, cs, buf, count);
	}

	up(&dev->sem);
	return ret;
}


static void enable_spi_chip( struct iv_atlas_i_lpe_spi_dev * dev, int cs )
{
	gpio(dev, IVSPI_CLK, dev->chip[cs].cfg[IV_GFSPI_MODE] & SPI_CPOL);
	half_clk_delay(dev, cs);

	if (IV_GFSPI_MUXED_CS) {
		switch (cs) {
			case 0: gpio_low(dev, IVSPI_CS1);   gpio_low(dev, IVSPI_CS0); break;
			case 1: gpio_high(dev, IVSPI_CS1);  gpio_low(dev, IVSPI_CS0); break;
			case 2: gpio_low(dev, IVSPI_CS1);   gpio_high(dev, IVSPI_CS0); break;
			case 3: gpio_high(dev, IVSPI_CS1);  gpio_high(dev, IVSPI_CS0); break;
		}
	} else {
		switch (cs) {
			case 0: gpio_low(dev, IVSPI_CS0); break;
			case 1: gpio_low(dev, IVSPI_CS1); break;
		}
	}
}


static void disable_spi_chip( struct iv_atlas_i_lpe_spi_dev * dev, int cs )
{

	half_clk_delay(dev, cs);

	/*
	 * Assumption is that CS are all active low when not IV_GFSPI_MUXED_CS,
	 * and that the highest CS is unused if IV_GFSPI_MUXED_CS.  Therefore, in
	 * either case, all CSes high is all means all are diabled.
	 */
	gpio_high(dev, IVSPI_CS0);
	gpio_high(dev, IVSPI_CS1);
}


/*
 * _iv_atlas_i_lpe_spi_xfer() - internal xfer function.  
 *
 * dev->sem must have been acquired before calling.
 */
static int _iv_atlas_i_lpe_spi_xfer(
	struct iv_atlas_i_lpe_spi_dev * dev, int cs, char * tx_buf, char * rx_buf, int count 
	)
{
	if ( cs >= IV_GFSPI_NUM_DEVS ) return -ENODEV;

	enable_spi_chip(dev, cs);
	spi_send(dev, cs, tx_buf, rx_buf, count);
	disable_spi_chip(dev, cs);

	return count;
}


static ssize_t cfg_write(struct iv_atlas_i_lpe_spi_dev *dev, int cs, const char __user *buf, size_t count)
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


static ssize_t data_write(struct iv_atlas_i_lpe_spi_dev *dev, int cs, const char __user *buf, size_t count)
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

	ret = _iv_atlas_i_lpe_spi_xfer( dev, cs, tx_buf, dev->chip[cs].rx_buf, count );
	dev->chip[cs].count = (ret < 0) ? 0 : ret;

	return ret < 0 ? ret : count;
}


/*
 * iv_atlas_i_lpe_spi_write() - char driver write() function.
 */
ssize_t iv_atlas_i_lpe_spi_write(struct file *filp, const char __user *buf, size_t count,
				loff_t *f_pos)
{
	struct iv_atlas_i_lpe_spi_dev *dev = filp->private_data; 
	unsigned int minor = iminor(filp->f_dentry->d_inode);
	int cs = minor & IV_GFSPI_MINOR_CS_MASK;
	int is_cfg = ( minor >= IV_GFSPI_CFG_MINOR_OFFSET );
	int ret = 0;

	if ( cs >= IV_GFSPI_NUM_DEVS ) return -ENODEV;

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

	if ( is_cfg ) {
		ret = cfg_write(dev, cs, buf, count);
	} else {
		ret = data_write(dev, cs, buf, count);
	}

	up(&dev->sem);
	return ret;
}


struct file_operations iv_atlas_i_lpe_spi_fops = {
	.owner =    THIS_MODULE,
	.read =     iv_atlas_i_lpe_spi_read,
	.write =    iv_atlas_i_lpe_spi_write,
	.open =     iv_atlas_i_lpe_spi_open,
	.release =  iv_atlas_i_lpe_spi_release,
};


/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
void iv_atlas_i_lpe_spi_cleanup_module(void)
{
	/* Get rid of our char dev entries */
	if (iv_atlas_i_lpe_spi_devp) {
		cdev_del(&iv_atlas_i_lpe_spi_devp->cdev);
		kfree(iv_atlas_i_lpe_spi_devp);
		iv_atlas_i_lpe_spi_devp = NULL;
	}

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(IV_GFSPI_DEVNO, IV_GFSPI_NUM_DEVS);
}


static int __init iv_atlas_i_lpe_spi_init(void)
{
	int i;
	int err;
	struct iv_atlas_i_lpe_spi_dev * dev;

	/*
	 * Register major/minor devno
	 */
	err = register_chrdev_region(IV_GFSPI_DEVNO, IV_GFSPI_NUM_MINORS, MODNAME);
	if (err < 0) {
		printk(KERN_WARNING MODNAME ": can't get major %d\n", IV_GFSPI_DEVNO);
		goto fail;
	}

	dev = kmalloc(sizeof(struct iv_atlas_i_lpe_spi_dev), GFP_KERNEL);
	if (!dev) {
		err = -ENOMEM;
		goto fail;
	}
	iv_atlas_i_lpe_spi_devp = dev;
	memset(dev, 0, sizeof(struct iv_atlas_i_lpe_spi_dev));
	{
		int i;
		for ( i = 0; i < IV_GFSPI_NUM_DEVS; i++ ) {
			dev->chip[i].cfg[IV_GFSPI_MAX_SPEED_HZ] = IV_GFSPI_MAX_SPEED_HZ_DEFAULT;
			dev->chip[i].cfg[IV_GFSPI_MODE] = IV_GFSPI_MODE_DEFAULT;
		}
	}
	sema_init(&dev->sem, 1);

	dev->gpio_bank2_base = ioremap( GPIO_BANK2_THRU_BANK6_PHYS, GPIO_BANK2_THRU_BANK6_LEN );
	if ( dev->gpio_bank2_base == NULL ) {
		printk(KERN_WARNING MODNAME ": Could not ioremap() GPIOs" );
		goto fail;
	}
	gpio_init(dev);

	cdev_init(&dev->cdev, &iv_atlas_i_lpe_spi_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &iv_atlas_i_lpe_spi_fops;
	err = cdev_add (&dev->cdev, IV_GFSPI_DEVNO, IV_GFSPI_NUM_MINORS);
	if (err) {
		printk(KERN_WARNING MODNAME ": Error %d adding driver", err);
		goto fail;
	}

	for ( i = 0; i < IV_GFSPI_NUM_DEVS; i++ ) {
		dev->chip[i].count = 0;
	}

	return 0;

fail:
	iv_atlas_i_lpe_spi_cleanup_module();
	return err;
}
fs_initcall(iv_atlas_i_lpe_spi_init);


static void __exit iv_atlas_i_lpe_spi_exit(void)
{
	iv_atlas_i_lpe_spi_cleanup_module();
}
module_exit(iv_atlas_i_lpe_spi_exit);

MODULE_AUTHOR("iVeia, LLC");
MODULE_DESCRIPTION("iVeia Titan-PPCx GigaFlex SPI driver");
MODULE_LICENSE("GPL");
