/*
 * iVeia I2C Bitbanged driver
 *
 * (C) Copyright 2008, iVeia, LLC
 *
 * This header is intended to be included by both the driver and the API library
 * that accesses the driver.  As such, it does not include <ioctl.h>, which
 * means that the driver or library need to include it before including this
 * file.  The included ioctl.h may come from different locations.
 */

#if !defined _IV_BITBANGED_I2C_H_
#define _IV_BITBANGED_I2C_H_

#define MAX_I2C_PAYLOAD (16)

#define IV_I2C_V5E_BUS0 3
#define IV_I2C_V5E_BUS0_UC_ADDR 0x90

#define IV_I2C_V5X_BUS0 4
#define IV_I2C_V5X_BUS0_EE_ADDR 0xA0

struct iv_bitbanged_i2c_msg {
	unsigned char addr;
	unsigned char write_len;
	unsigned char read_len;
	unsigned char flags;
	unsigned char write_bytes[MAX_I2C_PAYLOAD];
};

struct iv_bitbanged_i2c_dev;

int _iv_bitbanged_i2c_open(unsigned int bus);
int _iv_bitbanged_i2c_release(unsigned int bus);
ssize_t _iv_bitbanged_i2c_read(struct iv_bitbanged_i2c_dev *dev, unsigned int bus,
						char __user *buf, size_t count, loff_t *f_pos);
ssize_t _iv_bitbanged_i2c_write(struct iv_bitbanged_i2c_dev *dev, unsigned int bus,
								const char __user *buf, size_t count, loff_t *offset);

#endif
