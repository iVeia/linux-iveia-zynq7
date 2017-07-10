#ifndef __SPI_CHAR_
#define __SPI_CHAR_

struct spi_char_dev {
	struct semaphore sem;
	struct cdev cdev;
	char * app_regs;
};

/*
 * Prototypes for shared functions
 */
ssize_t scull_read(struct file *filp, char __user *buf, size_t count,
				   loff_t *f_pos);
ssize_t scull_write(struct file *filp, const char __user *buf, size_t count,
					loff_t *f_pos);



#endif
