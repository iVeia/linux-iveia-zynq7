/*
 * Bitbanged I2C Driver
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
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/interrupt.h>
#include <iveia/iv_device.h>

#include "iv_bitbanged_i2c.h"

#define MODNAME "iv_bitbanged_i2c"

#define I2C_QUARTER_CLOCK (3)
#define I2C_CLOCK_STRETCH_TIMEOUT_USECS     (25000)

#define I2C_BUS_TYPE_UNUSED             0
#define I2C_BUS_TYPE_GPIO               1
#define I2C_BUS_TYPE_FPGA               2

//
// For FPGA GPIOs, both iostate and iodir may be in the same register, which
// means we have to be careful not to overwrite the iostate when writing the
// iodir.  Also, iostate/iodir may share the same bits (i.e. iostate on read,
// iodir on write) - in that case we use a shadow register to remember the
// state of iodir.
//
#define I2C_FLAG_FPGA_SINGLE_REG        0x00000001
#define I2C_FLAG_FPGA_SAME_BITS         0x00000002

//
// Platform specific config.  
//
// Note: the PLATFORM_* flags are in the same space as the I2C_FLAGS_*, and
// should not conflict with them,
//
#define PLATFORM_TITAN_V5E              0x80000000
#define PLATFORM_TITAN_V5X              0x40000000
#define PLATFORM_TITAN_V5               (PLATFORM_TITAN_V5E | PLATFORM_TITAN_V5X)
#define PLATFORM_TITAN_PPCX             0x20000000

#if defined(CONFIG_IVEIA_TITANV5X)
	#define CURRENT_PLATFORM PLATFORM_TITAN_V5X
#elif defined(CONFIG_IVEIA_TITANV5E)
	#define CURRENT_PLATFORM PLATFORM_TITAN_V5E
#elif defined(CONFIG_IVEIA_TITAN_PPCX)
	#define CURRENT_PLATFORM PLATFORM_TITAN_PPCX
#endif

#if defined(CONFIG_IVEIA_TITANV5)
	#include <platforms/4xx/xparameters/xparameters.h>
	#define TITANV5_I2C_ADDR XPAR_IVEIA_XPS_REGISTERS_PPC_0_INST_1_BASEADDR
	#define TITANV5_I2C_SIZE \
		XPAR_IVEIA_XPS_REGISTERS_PPC_0_INST_1_HIGHADDR \
			- XPAR_IVEIA_XPS_REGISTERS_PPC_0_INST_1_BASEADDR + 1
#elif defined(CONFIG_IVEIA_TITAN_PPCX)
	#define TITANV5_I2C_ADDR 0
	#define TITANV5_I2C_SIZE 0
#endif


const struct {
	int bus_type;
	unsigned long physaddr;
	int size;
	unsigned long reg_index;
	unsigned long iostate_pinmask[2];   // { SCL_PIN, SDA_PIN }
	unsigned long iodir_pinmask[2];   // { SCL_PIN, SDA_PIN }
	unsigned long flags;
} bus_spec[] = {
	// iv_i2c_gfhh1_0: Gigaflex HH1 I2C bus
	{ 
		I2C_BUS_TYPE_FPGA, 
		0x80006000, 
		0x1000, 
		0,
		{ 0x00000001, 0x00000002 }, // SCL_PIN, SDA_PIN (for iostate)
		{ 0x00000001, 0x00000002 }, // SCL_PIN, SDA_PIN (for iodir)
		PLATFORM_TITAN_PPCX,
	}, 
	// iv_i2c_ppcx_0: PPCx local I2C bus
	{ 
		I2C_BUS_TYPE_GPIO, 
		0XFFE0FC00, 
		0x100, 
		0,
		{ 0x00010000, 0x00020000 }, // SCL_PIN, SDA_PIN (for iostate)
		{ 0x00010000, 0x00020000 }, // SCL_PIN, SDA_PIN (for iodir)
		PLATFORM_TITAN_PPCX,
	},  
	// iv_i2c_gfcl2b_0: Gigaflex CL2B I2C bus
	{ 
		I2C_BUS_TYPE_FPGA, 
		TITANV5_I2C_ADDR, 
		TITANV5_I2C_SIZE, 
		0,
		{ 0x00000001, 0x00000002 }, // SCL_PIN, SDA_PIN (for iostate)
		{ 0x00000004, 0x00000008 }, // SCL_PIN, SDA_PIN (for iodir)
		PLATFORM_TITAN_V5 | I2C_FLAG_FPGA_SINGLE_REG,
	},
	// iv_i2c_v5e_0: Titan-v5e local I2C bus
	{ 
		I2C_BUS_TYPE_FPGA, 
		TITANV5_I2C_ADDR, 
		TITANV5_I2C_SIZE, 
		0,
		{ 0x00000400, 0x00000800 }, // SCL_PIN, SDA_PIN (for iostate)
		{ 0x00000400, 0x00000800 }, // SCL_PIN, SDA_PIN (for iodir)
		PLATFORM_TITAN_V5E | I2C_FLAG_FPGA_SINGLE_REG | I2C_FLAG_FPGA_SAME_BITS,
	},
	// iv_i2c_v5x_0: Titan-v5x local I2C bus
	{ 
		I2C_BUS_TYPE_FPGA, 
		TITANV5_I2C_ADDR, 
		TITANV5_I2C_SIZE, 
		1,
		{ 0x00000001, 0x00000002 }, // SCL_PIN, SDA_PIN (for iostate)
		{ 0x00000001, 0x00000002 }, // SCL_PIN, SDA_PIN (for iodir)
		PLATFORM_TITAN_V5X | I2C_FLAG_FPGA_SINGLE_REG | I2C_FLAG_FPGA_SAME_BITS,
	},
	// iv_i2c_v5x_1: Titan-v5e Backplane I2C bus
	{ 
		I2C_BUS_TYPE_FPGA, 
		TITANV5_I2C_ADDR, 
		TITANV5_I2C_SIZE, 
		2,
		{ 0x00000001, 0x00000002 }, // SCL_PIN, SDA_PIN (for iostate)
		{ 0x00000001, 0x00000002 }, // SCL_PIN, SDA_PIN (for iodir)
		PLATFORM_TITAN_V5X | I2C_FLAG_FPGA_SINGLE_REG | I2C_FLAG_FPGA_SAME_BITS,
	},
	// iv_i2c_v5x_2: Titan-v5e daughtercard I2C bus
	{ 
		I2C_BUS_TYPE_FPGA, 
		TITANV5_I2C_ADDR, 
		TITANV5_I2C_SIZE, 
		3,
		{ 0x00000001, 0x00000002 }, // SCL_PIN, SDA_PIN (for iostate)
		{ 0x00000001, 0x00000002 }, // SCL_PIN, SDA_PIN (for iodir)
		PLATFORM_TITAN_V5X | I2C_FLAG_FPGA_SINGLE_REG | I2C_FLAG_FPGA_SAME_BITS,
	},
};

#define SCL_PIN 0
#define SDA_PIN 1

#define I2C_GPIO_POWER_PINMASK 0x00080000

#define NUM_I2C_BUSES (ARRAY_SIZE(bus_spec))

struct iv_bitbanged_i2c_dev
{
	struct semaphore sem;
	struct cdev cdev;
	int registered;
	struct {
		int bus_type;
		unsigned char read_len;
		unsigned char read_offset;
		unsigned char read_bytes[MAX_I2C_PAYLOAD];
		unsigned char * iomap;
		unsigned long * regs;
		struct {
			unsigned long * iostate;
			unsigned long * iodir;
			unsigned long shadow_iodir;
		} fpga_bus;
		struct {
			unsigned long * gpdir;
			unsigned long * gpodr;
			unsigned long * gpdat;
			unsigned long shadow_gpdat;
		} gpio_bus;
	} bus[NUM_I2C_BUSES];
};

#define MSG_HDR_SIZE (offsetof(struct iv_bitbanged_i2c_msg, write_bytes))

#define IV_BITBANGED_I2C_DEVNO MKDEV(IV_BITBANGED_I2C_DEV_MAJOR, 0)

static struct iv_bitbanged_i2c_dev * gdev = NULL;

static inline void i2c_set_pin(struct iv_bitbanged_i2c_dev * dev, int bus, int pin, int state)
{
	if (dev->bus[bus].bus_type == I2C_BUS_TYPE_GPIO) {
		if (state) {
			dev->bus[bus].gpio_bus.shadow_gpdat |= bus_spec[bus].iostate_pinmask[pin];
		} else {
			dev->bus[bus].gpio_bus.shadow_gpdat &= ~bus_spec[bus].iostate_pinmask[pin];
		}
		*(dev->bus[bus].gpio_bus.gpdat) = dev->bus[bus].gpio_bus.shadow_gpdat;

	} else if (dev->bus[bus].bus_type == I2C_BUS_TYPE_FPGA) {
		if ( bus_spec[bus].flags & I2C_FLAG_FPGA_SAME_BITS ) {
			if (state) {
				gdev->bus[bus].fpga_bus.shadow_iodir &= ~bus_spec[bus].iodir_pinmask[pin];
			} else {
				gdev->bus[bus].fpga_bus.shadow_iodir |= bus_spec[bus].iodir_pinmask[pin];
			}
			*(gdev->bus[bus].fpga_bus.iodir) = gdev->bus[bus].fpga_bus.shadow_iodir;

		} else if ( bus_spec[bus].flags & I2C_FLAG_FPGA_SINGLE_REG ) {
			unsigned long current_iodir;
			current_iodir = *(dev->bus[bus].fpga_bus.iodir);

			//
			// Special case - both iostate and iodir are the same reg.  We must
			// keep iostate pins zeroed.
			//
			current_iodir &= ~bus_spec[bus].iostate_pinmask[SCL_PIN];
			current_iodir &= ~bus_spec[bus].iostate_pinmask[SDA_PIN];

			if (state) {
				*(dev->bus[bus].fpga_bus.iodir) = current_iodir & ~bus_spec[bus].iodir_pinmask[pin];
			} else {
				*(dev->bus[bus].fpga_bus.iodir) = current_iodir | bus_spec[bus].iodir_pinmask[pin];
			}

		} else {
			if (state) {
				*(gdev->bus[bus].fpga_bus.iodir) &= ~bus_spec[bus].iodir_pinmask[pin];
			} else {
				*(gdev->bus[bus].fpga_bus.iodir) |= bus_spec[bus].iodir_pinmask[pin];
			}
		}

	} else {
		BUG();
	}
}


static inline int i2c_get_pin(struct iv_bitbanged_i2c_dev * dev, int bus, int pin)
{
	int state;
	if (dev->bus[bus].bus_type == I2C_BUS_TYPE_GPIO) {
		// Set as input before reading.
		*(dev->bus[bus].gpio_bus.gpdir) &= ~bus_spec[bus].iodir_pinmask[pin];
		state = *(dev->bus[bus].gpio_bus.gpdat) & bus_spec[bus].iostate_pinmask[pin] ? 1 : 0;
		*(dev->bus[bus].gpio_bus.gpdir) |= bus_spec[bus].iodir_pinmask[pin];

	} else if (dev->bus[bus].bus_type == I2C_BUS_TYPE_FPGA) {
		state = (*(dev->bus[bus].fpga_bus.iostate) & bus_spec[bus].iostate_pinmask[pin]) ? 1 : 0;

	} else {
		BUG();
	}
	return state;
}


static inline void i2c_set_power(struct iv_bitbanged_i2c_dev * dev, int bus, int on)
{
	if (dev->bus[bus].bus_type == I2C_BUS_TYPE_GPIO) {
		if (!on) {
			dev->bus[bus].gpio_bus.shadow_gpdat |= I2C_GPIO_POWER_PINMASK;
		} else {
			dev->bus[bus].gpio_bus.shadow_gpdat &= ~I2C_GPIO_POWER_PINMASK;
		}
		*(dev->bus[bus].gpio_bus.gpdat) = dev->bus[bus].gpio_bus.shadow_gpdat;
	} else if (dev->bus[bus].bus_type == I2C_BUS_TYPE_FPGA) {
		// Not implemented
		printk(KERN_ERR MODNAME ": Cannot turn power on/off for FPGA I2C bus\n");
	} else {
		BUG();
	}
}

static int i2c_is_idle(struct iv_bitbanged_i2c_dev * dev, int bus)
{
	// bus is idle if both lines are high.
	return i2c_get_pin(dev, bus, SCL_PIN) && i2c_get_pin(dev, bus, SDA_PIN);
}


/*
 * i2c_wait_for_scl_high() - wait for scl high to support slave clock stretching.
 */
static int i2c_wait_for_scl_high(struct iv_bitbanged_i2c_dev * dev, int bus)
{
	int timeout = I2C_CLOCK_STRETCH_TIMEOUT_USECS;

	while (! i2c_get_pin(dev, bus, SCL_PIN) && timeout-- > 0) {
		udelay(1);
	}
	udelay(I2C_QUARTER_CLOCK);
	return timeout <= 0 ? -ETIMEDOUT : 0;
}


static int i2c_start(struct iv_bitbanged_i2c_dev * dev, int bus)
{
	int err = 0;
	i2c_set_pin(dev, bus, SDA_PIN, 1);
	udelay(I2C_QUARTER_CLOCK);
	i2c_set_pin(dev, bus, SCL_PIN, 1);
	err = i2c_wait_for_scl_high(dev, bus);
	i2c_set_pin(dev, bus, SDA_PIN, 0);
	udelay(I2C_QUARTER_CLOCK);
	i2c_set_pin(dev, bus, SCL_PIN, 0);
	udelay(I2C_QUARTER_CLOCK);
	return err;
}


static int i2c_stop(struct iv_bitbanged_i2c_dev * dev, int bus)
{
	i2c_set_pin(dev, bus, SDA_PIN, 0);
	udelay(I2C_QUARTER_CLOCK);
	i2c_set_pin(dev, bus, SCL_PIN, 1);
	udelay(I2C_QUARTER_CLOCK);
	i2c_set_pin(dev, bus, SDA_PIN, 1);
	udelay(I2C_QUARTER_CLOCK);
	return 0;
}


static int i2c_write_bit(struct iv_bitbanged_i2c_dev * dev, int bus, unsigned char bit)
{
	int err = 0;
	i2c_set_pin(dev, bus, SDA_PIN, bit);
	udelay(I2C_QUARTER_CLOCK);
	i2c_set_pin(dev, bus, SCL_PIN, 1);
	err = i2c_wait_for_scl_high(dev, bus);
	// valid data during high clock
	udelay(I2C_QUARTER_CLOCK);
	i2c_set_pin(dev, bus, SCL_PIN, 0);
	udelay(I2C_QUARTER_CLOCK);
	return err;
}


static int i2c_read_bit(struct iv_bitbanged_i2c_dev * dev, int bus, unsigned char * pbit)
{
	int err = 0;
	i2c_set_pin(dev, bus, SDA_PIN, 1);    // Data high allows read (open-drain output).
	udelay(I2C_QUARTER_CLOCK);
	i2c_set_pin(dev, bus, SCL_PIN, 1);
	err = i2c_wait_for_scl_high(dev, bus);
	*pbit = i2c_get_pin(dev, bus, SDA_PIN);
	udelay(I2C_QUARTER_CLOCK);
	i2c_set_pin(dev, bus, SCL_PIN, 0);
	udelay(I2C_QUARTER_CLOCK);
	return err;
}


static int i2c_write_byte(struct iv_bitbanged_i2c_dev * dev, int bus, unsigned char byte)
{
	int i;
	unsigned char ack;
	int err = 0;
	for (i = 0; i < 8; i++) {
		err = i2c_write_bit(dev, bus, byte & 0x80);
		if (err) return err;
		byte <<= 1;
	}
	err = i2c_read_bit(dev, bus, &ack);
	if (err) return err;
	return ack == 0 ? 0 : -EIO;
}


static int i2c_read_byte(struct iv_bitbanged_i2c_dev * dev, int bus, unsigned char * pbyte, bool nack)
{
	int i;
	unsigned char bit;
	int err = 0;
	*pbyte = 0;
	for (i = 0; i < 8; i++) {
		err = i2c_read_bit(dev, bus, &bit);
		if (err) return err;
		*pbyte = (*pbyte << 1) | bit;
	}
	err = i2c_write_bit(dev, bus, nack ? 1 : 0 );
	if (err) return err;
	return 0;
}


static int 
bitbanged_i2c_readwrite(
	struct iv_bitbanged_i2c_dev * dev, 
	int bus,
	struct iv_bitbanged_i2c_msg * msg
   )
{
	int i;
	int err = 0;

	//
	// Test bus idle. This is not a true bus contention test.  It does not
	// follow the spec.
	//
	if (! i2c_is_idle(dev, bus)) return -EIO;

	if (! err)
		err = i2c_start(dev, bus);

	if (msg->write_len > 0) {
		//
		// Write address (with R/W# = WRITE)
		//
		if (! err)
			err = i2c_write_byte(dev, bus, msg->addr | 0);

		//
		// Write bytes
		//
		if (! err) {
			for (i = 0; i < msg->write_len; i++) {
				err = i2c_write_byte(dev, bus, msg->write_bytes[i]);
				if (err) break;
			}
		}
	}

	if (msg->read_len > 0) {
		//
		// If read follows write, we need to envoke a restart.
		//
		if (! err && msg->write_len > 0) 
			err = i2c_start(dev, bus);

		//
		// Write address (with R/W# = READ)
		//
		if (! err)
			err = i2c_write_byte(dev, bus, msg->addr | 1);

		//
		// Read bytes.  NACK the last byte.
		//
		if (! err) {
			dev->bus[bus].read_len = msg->read_len;
			dev->bus[bus].read_offset = 0;
			for (i = 0; i < msg->read_len; i++) {
				int last_byte = i == (msg->read_len - 1);
				err = i2c_read_byte(dev, bus, &dev->bus[bus].read_bytes[i], last_byte);
				if (err) {
					dev->bus[bus].read_len = 0;
					break;
				}
			}
		}
	}

	if (! err)
		err = i2c_stop(dev, bus);
	else
		i2c_stop(dev, bus);

	return err;
}


int _iv_bitbanged_i2c_open(unsigned int bus)
{
	if ( bus >= NUM_I2C_BUSES ) return -ENODEV;

	if (gdev->bus[bus].bus_type == I2C_BUS_TYPE_FPGA) {
		if ( bus_spec[bus].flags & I2C_FLAG_FPGA_SINGLE_REG ) {
			gdev->bus[bus].fpga_bus.iostate = &gdev->bus[bus].regs[0];
			gdev->bus[bus].fpga_bus.iodir = &gdev->bus[bus].regs[0];
		} else {
			gdev->bus[bus].fpga_bus.iostate = &gdev->bus[bus].regs[0];
			gdev->bus[bus].fpga_bus.iodir = &gdev->bus[bus].regs[1];
		}

		//
		// Init SCL/SDA pins
		//
		// Both SCL and SDA are open-drain drivers with external pullups.
		// If iodir bit is 0, then pin is input.  Pin can be read via iostate.
		// If iodir bit is 1, then pin is output. Pin state can be set via iostate.
		//
		// This driver always leaves the iostate as 0.  Then, to:
		//      - write 1: set iodir bit to 0 (input).
		//      - write 0: set iodir bit to 1 (output).
		//      - read: set iodir bit to 0 (input), and read iostate.
		//
		*(gdev->bus[bus].fpga_bus.iostate) = 0;
		*(gdev->bus[bus].fpga_bus.iodir) = 0;
		gdev->bus[bus].fpga_bus.shadow_iodir = 0;

	} else if (gdev->bus[bus].bus_type == I2C_BUS_TYPE_GPIO) {
		//
		// The shadow register is used because we cannot read the
		// state of the GPIO pins.  And when we're writing, we need to
		// write ALL the GPIO (output) pins at once.  So we keep a
		// shadow register that mainatins the state, and we always write
		// through that.
		//
		// BNS 2/24/2009: Unfortunately, there are some output pins
		// other than used by this driver.  Specifically, there are 3 FPGA
		// programming pins.  These pins are all active low.  They MUST be
		// left inactive.  so, we initalize the shadow register here, with
		// all 1s.  Its a hack, because it has nothing to do with this
		// driver.
		gdev->bus[bus].gpio_bus.shadow_gpdat = 0xFFFFFFFF;

		gdev->bus[bus].gpio_bus.shadow_gpdat |= bus_spec[bus].iostate_pinmask[SCL_PIN];
		gdev->bus[bus].gpio_bus.shadow_gpdat |= bus_spec[bus].iostate_pinmask[SDA_PIN];
		gdev->bus[bus].gpio_bus.shadow_gpdat &= ~I2C_GPIO_POWER_PINMASK;

		gdev->bus[bus].gpio_bus.gpdir = &gdev->bus[bus].regs[0];
		gdev->bus[bus].gpio_bus.gpodr = &gdev->bus[bus].regs[1];
		gdev->bus[bus].gpio_bus.gpdat = &gdev->bus[bus].regs[2];
		*(gdev->bus[bus].gpio_bus.gpdir) |= I2C_GPIO_POWER_PINMASK;
		*(gdev->bus[bus].gpio_bus.gpdir) |= bus_spec[bus].iodir_pinmask[SCL_PIN];
		*(gdev->bus[bus].gpio_bus.gpdir) |= bus_spec[bus].iodir_pinmask[SDA_PIN];
		*(gdev->bus[bus].gpio_bus.gpodr) |= bus_spec[bus].iodir_pinmask[SCL_PIN];
		*(gdev->bus[bus].gpio_bus.gpodr) |= bus_spec[bus].iodir_pinmask[SDA_PIN];
		*(gdev->bus[bus].gpio_bus.gpdat) = gdev->bus[bus].gpio_bus.shadow_gpdat;

	} else {
		printk(KERN_ERR MODNAME ": Unknown bus type in I2C bus spec\n");
		return -EINVAL;
	}

	return 0;
}


int _iv_bitbanged_i2c_release(unsigned int bus)
{
	return 0;
}


ssize_t _iv_bitbanged_i2c_read(struct iv_bitbanged_i2c_dev *dev, unsigned int bus,
						char __user *buf, size_t count, loff_t *f_pos)
{
	int ret = 0;

	if ( dev == NULL ) 
		dev = gdev;

	if ( bus >= NUM_I2C_BUSES ) return -ENODEV;

	if (count <= 0) return 0;

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

	if (count > dev->bus[bus].read_len) {
		count = dev->bus[bus].read_len;
	}

	if (copy_to_user(buf, dev->bus[bus].read_bytes + dev->bus[bus].read_offset, count)) {
		ret = -EFAULT;
		goto out;
	}

	ret = count;
	dev->bus[bus].read_len -= count;
	dev->bus[bus].read_offset += count;

out:
	up(&dev->sem);
	return ret;
}


ssize_t _iv_bitbanged_i2c_write(struct iv_bitbanged_i2c_dev *dev, unsigned int bus,
								const char __user *buf, size_t count, loff_t *offset)
{
	struct iv_bitbanged_i2c_msg msg;
	int ret = 0;

	if ( dev == NULL ) 
		dev = gdev;

	if ( bus >= NUM_I2C_BUSES ) return -ENODEV;

	if (count < MSG_HDR_SIZE) return -EMSGSIZE;
	if (count > sizeof(msg)) return -EMSGSIZE;

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

	if (copy_from_user(&msg, (void __user *) buf, count)) {
		ret = -EFAULT;
		goto out;
	}

	if (msg.write_len > MAX_I2C_PAYLOAD || msg.read_len > MAX_I2C_PAYLOAD) {
		ret = -EMSGSIZE;
		goto out;
	}

	ret = bitbanged_i2c_readwrite(dev, bus, &msg);
	if (ret < 0) goto out;

	ret = count;

out:
	up(&dev->sem);
	return ret;
}


/*
 * open()
 */
int iv_bitbanged_i2c_open(struct inode *inode, struct file *filp)
{
	struct iv_bitbanged_i2c_dev * dev;
	unsigned int minor = iminor(filp->f_dentry->d_inode);

	dev = container_of(inode->i_cdev, struct iv_bitbanged_i2c_dev, cdev);
	filp->private_data = dev;

	return _iv_bitbanged_i2c_open(minor);
}


/*
 * close()
 */
int iv_bitbanged_i2c_release(struct inode *inode, struct file *filp)
{
	unsigned int minor = iminor(filp->f_dentry->d_inode);
	return _iv_bitbanged_i2c_release(minor);
}


/*
 * read()
 */
ssize_t iv_bitbanged_i2c_read(struct file *filp, char __user *buf,
					  size_t count, loff_t *f_pos)
{
	struct iv_bitbanged_i2c_dev *dev = filp->private_data;
	unsigned int minor = iminor(filp->f_dentry->d_inode);

	return _iv_bitbanged_i2c_read(dev, minor, buf, count, f_pos);
}


/*
 * write()
 */
ssize_t iv_bitbanged_i2c_write(struct file *filp, const char __user *buf,
							 size_t count, loff_t *offset)
{
	struct iv_bitbanged_i2c_dev *dev = filp->private_data;
	unsigned int minor = iminor(filp->f_dentry->d_inode);

	return _iv_bitbanged_i2c_write(dev, minor, buf, count, offset);
}


struct file_operations iv_bitbanged_i2c_fops =
{
	.owner =   THIS_MODULE,
	.read =    iv_bitbanged_i2c_read,
	.write =   iv_bitbanged_i2c_write,
	.open =    iv_bitbanged_i2c_open,
	.release = iv_bitbanged_i2c_release,
};


/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized.
 */
void iv_bitbanged_i2c_cleanup_module(void)
{
	int i;

	for (i = 0; i < NUM_I2C_BUSES; i++) {
		if (gdev->bus[i].regs != NULL) {
			iounmap(gdev->bus[i].regs);
		}
	}

	if (gdev->registered) {
		unregister_chrdev_region(IV_BITBANGED_I2C_DEVNO, 1);
	}

	if (NULL != gdev) {
		cdev_del(&(gdev->cdev));
		kfree(gdev);
		gdev = NULL;
	}

}


/*
 * init
 */
static int __init iv_bitbanged_i2c_init(void)
{
	int err = 0;

	gdev = kmalloc(sizeof (struct iv_bitbanged_i2c_dev), GFP_KERNEL);
	if (gdev == NULL) {
		err = -ENOMEM;
		goto out;
	}
	memset(gdev, 0, sizeof (struct iv_bitbanged_i2c_dev));

	err = register_chrdev_region(IV_BITBANGED_I2C_DEVNO, NUM_I2C_BUSES, MODNAME);
	if (err < 0) goto out;
	gdev->registered = 1;

	sema_init(&(gdev->sem), 1);

	cdev_init(&(gdev->cdev), &iv_bitbanged_i2c_fops);
	gdev->cdev.owner = THIS_MODULE;
	gdev->cdev.ops = &iv_bitbanged_i2c_fops;
	err = cdev_add(&(gdev->cdev), IV_BITBANGED_I2C_DEVNO, NUM_I2C_BUSES);
	if (err < 0) {
		printk(KERN_ERR MODNAME ": error %d adding device\n", err);
		goto out;
	}

	{
		int i;
		for (i = 0; i < NUM_I2C_BUSES; i++) {
			if ( ! ( bus_spec[i].flags & CURRENT_PLATFORM )) continue;
			//
			// TODO: We could check here to see ioremap() was already called
			// for another bus at the same addr.  But its just simpler to remap it
			// again.
			//
			gdev->bus[i].bus_type = bus_spec[i].bus_type;
			gdev->bus[i].iomap = ioremap(bus_spec[i].physaddr, bus_spec[i].size);
			if (gdev->bus[i].iomap == NULL) {
				printk(KERN_ERR MODNAME ": error mapping I2C register address space\n");
				err = -ENOMEM;
				goto out;
			}
			gdev->bus[i].regs = &((unsigned long *) gdev->bus[i].iomap )[ bus_spec[i].reg_index ];
		}
	}

out:
	if (err < 0) {
		iv_bitbanged_i2c_cleanup_module();
	}

	return err;
}


static void __exit iv_bitbanged_i2c_exit(void)
{
	iv_bitbanged_i2c_cleanup_module();
}


// init early, as its needed to get mac addr.
arch_initcall(iv_bitbanged_i2c_init);
module_exit(iv_bitbanged_i2c_exit);


MODULE_AUTHOR("iVeia, LLC");
MODULE_DESCRIPTION("iVeia bitbanged i2c driver");
MODULE_LICENSE("GPL");

