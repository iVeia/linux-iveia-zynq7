/*
 * spi_char.h -- definitions for the char module
 */

#ifndef _SPI_CHAR_H_
#define _SPI_CHAR_H_

#include <linux/ioctl.h>

typedef enum {
	IV_GFSPI_MAX_SPEED_HZ,
	IV_GFSPI_MODE,
	IV_GFSPI_NUM_CFG_WORDS,
} IV_GFSPI_CFG_WORDS;

/*
 * Ioctl definitions.  Deprecated - new interfaces use gfspicfg via read/write().
 */
struct reg_spec {
	unsigned long offset;
	unsigned long val;
};

#define GFSPI_IOC_MAGIC  'g'

#define GFSPI_IOCREADREG              _IOR(GFSPI_IOC_MAGIC,  0, struct reg_spec)
#define GFSPI_IOCWRITEREG             _IOW(GFSPI_IOC_MAGIC,  1, struct reg_spec)

#define GFSPI_IOC_MAXNR 2


#endif
