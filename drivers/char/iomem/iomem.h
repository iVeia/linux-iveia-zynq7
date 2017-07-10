/*
 * iomem.h
 */

#ifndef _IOMEM_H_
#define _IOMEM_H_

#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>

struct iomem_dev {
	struct cdev cdev;
};

#endif
