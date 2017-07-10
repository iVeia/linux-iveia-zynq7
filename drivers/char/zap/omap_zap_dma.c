/*
 * ZAP DMA engine
 *
 * (C) Copyright 2008, iVeia, LLC
 */
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
//#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/dma-mapping.h>
#include <plat/dma.h>
#include <asm/outercache.h>
#include <linux/ctype.h>

#include "zap.h"
#include "_zap.h"
#include "omap_zap_dma.h"
#include "dma.h"

#ifndef MODNAME
#define MODNAME "zap"
#endif

#define PHYS_MEM_BOTTOM (0x80000000)

#define ZAP_REG_CSR		(0x00000000/4)
    #define NUM_INTERFACES_SHIFT    (16)
	#define CSR_TX_OOB		(1 << 3)
	#define CSR_RX_OOB		(1 << 2)
	#define CSR_TXEN		(1 << 1)
	#define CSR_RXEN		(1 << 0)

#define ZAP_REG_ICR		(0x00000004/4)
	#define ICR_SET_RX_RDY		(1<<21)
	#define ICR_SET_RX_ERR		(1<<20)
	#define ICR_SET_TX_SIZE_RDY	(1<<19)
	#define ICR_SET_TX_RDY		(1<<18)
	#define ICR_SET_TX_ERR		(1<<17)
	#define ICR_SET_GLBL		(1<<16)

	#define ICR_CLR_RX_RDY		(1<<5)
	#define ICR_CLR_RX_ERR		(1<<4)
	#define ICR_CLR_TX_SIZE_RDY	(1<<3)
	#define ICR_CLR_TX_RDY		(1<<2)
	#define ICR_CLR_TX_ERR		(1<<1)
	#define ICR_CLR_GLBL		(1<<0)

	#define ICR_MSK_RX_RDY		(1<<21)
	#define ICR_MSK_RX_ERR		(1<<20)
	#define ICR_MSK_TX_SIZE_RDY	(1<<19)
	#define ICR_MSK_TX_RDY		(1<<18)
	#define ICR_MSK_TX_ERR		(1<<17)
	#define ICR_MSK_GLBL		(1<<16)

	#define ICR_INT_RX_RDY		(1<<5)
	#define ICR_INT_RX_ERR		(1<<4)
	#define ICR_INT_TX_SIZE_RDY	(1<<3)
	#define ICR_INT_TX_RDY		(1<<2)
	#define ICR_INT_TX_ERR		(1<<1)
	#define ICR_INT_GLBL		(1<<0)

#define ZAP_REG_TX_SIZE	(0x00000008/4)

#define ZAP_REG_RX_SIZE (0x00000008/4)
	#define RX_SIZE_OOB_OVERFLOW_ERR	(0x80000000)
	#define RX_SIZE_DAT_OVERFLOW_ERR	(0x40000000)

#define ZAP_REG_MAX_RX_SIZE		(0x0000000c/4)
	#define MAX_RX_OOB_SIZE_SHIFT	16
	#define MAX_RX_OOB_SIZE_MASK	0xFFFF0000
	#define MAX_RX_DAT_SIZE_SHIFT	0
	#define MAX_RX_DAT_SIZE_MASK	0x0000FFFF

#define ZAP_REG_RX_FIFO_SIZE	(0x00000100/4)
#define ZAP_REG_TX_FIFO_SIZE	(0x00000104/4)

#define ZAP_REG_HIGH_WATER_MARK	(0x00000108/4)
	#define HIGH_WATER_RX_SHIFT 0
	#define HIGH_WATER_TX_SHIFT	16

#define FPGA_VERSION_REG		(0x0000010C/4)
	#define FPGA_VERSION_BOARD_SHIFT	24
	#define FPGA_VERSION_MAJOR_SHIFT	16
	#define FPGA_VERSION_MINOR_SHIFT	8
	#define FPGA_VERSION_RELEASE_SHIFT	0

#define INTERFACE_REG_OFFSET    (0x00000200/4)


#define _ZAP_REG_READ(reg)			\
	pdma_if->zap_reg[reg]
#define _ZAP_REG_WRITE(reg, val)			\
	pdma_if->zap_reg[reg] = val
#define _ZAP_REG_WRITE_MASKED(reg, val, mask)	\
	_ZAP_REG_WRITE( reg, (val & mask) | (_ZAP_REG_READ(reg) & ~mask))

#define ZAP_POOL_MIN_SIZE  ( 8 * 1024 * 1024 )

#define MAPIT
volatile unsigned long tx_vaddr;
volatile unsigned long rx_vaddr;

///////////////////////////////////////////////////////////////////////////
//
// Globals
//
///////////////////////////////////////////////////////////////////////////

int bInIsr;

int iInWriteBuf;

struct dma_if_interface {
    //int iDevice;
	int rx_on;
	int tx_on;
	int rx_in_dma;
	int rx_dma_count, tx_dma_count;

	char * rx_pbuf;
	char * tx_pbuf;
	unsigned long rx_ulDataLen;
	unsigned long rx_ulOobLen;
	unsigned long rx_ulFlags;
};

struct dma_if {


	struct zap_dev * zap_dev;
	volatile unsigned long * zap_reg;
	int irq;
	char * rx_buffer_vaddr;
	unsigned long rx_buffer_paddr;
	char * tx_buffer_vaddr;
	unsigned long tx_buffer_paddr;
	unsigned long rx_buffer_size;
	unsigned long tx_buffer_size;

	//DMA Parameters

	int rx_dma_ch, tx_dma_ch;
	int rx_dma_id, tx_dma_id;

	dma_addr_t rx_dmabuf, tx_dmabuf;

    //JSM NEW
    struct dma_if_interface interface[ZAP_MAX_DEVICES];
    int rx_dma_device;
    int tx_dma_device;
};

struct dma_if dma_if;
struct dma_if * pdma_if = &dma_if;

#define rx_buffer2phys( vaddr ) \
	((unsigned long) vaddr - (unsigned long) pdma_if->rx_buffer_vaddr + pdma_if->rx_buffer_paddr )
#define phys2rx_buffer( paddr ) \
	((char *) ( paddr - pdma_if->rx_buffer_paddr + (unsigned long) pdma_if->rx_buffer_vaddr ))

#define tx_buffer2phys( vaddr ) \
	((unsigned long) vaddr - (unsigned long) pdma_if->tx_buffer_vaddr + pdma_if->tx_buffer_paddr )
#define phys2tx_buffer( paddr ) \
	((char *) ( paddr - pdma_if->tx_buffer_paddr + (unsigned long) pdma_if->tx_buffer_vaddr ))

static unsigned long ZAP_REG_READ(int iDevice, unsigned long reg) {
	unsigned long ulRetVal;
	ulRetVal = _ZAP_REG_READ((iDevice * INTERFACE_REG_OFFSET) + reg);
	return ulRetVal;
}
static void ZAP_REG_WRITE(int iDevice, unsigned long reg, unsigned long val) {
	_ZAP_REG_WRITE((iDevice * INTERFACE_REG_OFFSET) + reg, val);
}
static void ZAP_REG_WRITE_MASKED(int iDevice, unsigned long reg, unsigned long val, unsigned long mask) {
	_ZAP_REG_WRITE_MASKED((iDevice * INTERFACE_REG_OFFSET) + reg, val, mask);
}


///////////////////////////////////////////////////////////////////////////
//
// Private funcs
//
///////////////////////////////////////////////////////////////////////////

static void
zap_rx_dma_callback(
	int lch,
	u16 ch_status,
	void *data
	)
{
    int iDevice;
	int err;

    iDevice = pdma_if->rx_dma_device;

	//printk(KERN_ERR MODNAME "ZAP_RX_DMA_CALLBACK EXECUTED!\n");
	if ( !(pdma_if->interface[iDevice].rx_on) )
		printk(KERN_ERR MODNAME "ERROR: RX_DMA_CALLBACK CALLED AND RX_ON = 0\n");

	if ( pdma_if->interface[iDevice].rx_on )
	{

		pdma_if->interface[iDevice].rx_dma_count++;

#ifdef MAPIT
		dma_unmap_single( NULL, pdma_if->rx_dmabuf, pdma_if->interface[iDevice].rx_ulDataLen, DMA_FROM_DEVICE );
#else
		//dma_unmap_single( NULL, pdma_if->rx_dmabuf, pdma_if->rx_ulDataLen, DMA_FROM_DEVICE );
#endif

		outer_inv_range( (char *)pdma_if->rx_dmabuf, (char *)pdma_if->rx_dmabuf + pdma_if->interface[iDevice].rx_ulDataLen );

		/* Buffer transfer complete. Move to fifo pool */
		err = pool_enqbuf( &pdma_if->zap_dev->interface[iDevice].rx_pool,
			pdma_if->interface[iDevice].rx_pbuf,
			pdma_if->interface[iDevice].rx_ulDataLen,
			pdma_if->interface[iDevice].rx_ulOobLen,
			pdma_if->interface[iDevice].rx_ulFlags );
		if ( err ) {
			pool_dump( &pdma_if->zap_dev->interface[iDevice].rx_pool, 0 );
			BUG_ON( err );
		}

		ZAP_REG_WRITE(iDevice, ZAP_REG_ICR, ICR_SET_RX_RDY);
		pdma_if->interface[iDevice].rx_in_dma = 0;

	}
}


static void
zap_tx_dma_callback(
	int lch,
	u16 ch_status,
	void *data
	)
{
    int iDevice;
	int err;

    iDevice = pdma_if->tx_dma_device;

	if ( !(pdma_if->interface[iDevice].tx_on) )
		printk(KERN_ERR MODNAME "ERROR: TX_DMA_CALLBACK CALLED AND TX_ON = 0\n");

	if ( pdma_if->interface[iDevice].tx_on ){

		err = pool_freebuf( &pdma_if->zap_dev->interface[iDevice].tx_pool, pdma_if->interface[iDevice].tx_pbuf);
		if ( err ) {
			printk(KERN_ERR MODNAME "TX_DMA_CALLBACK, freebuf returned %d,pbuf=0x%08x\n",err,(unsigned long)pdma_if->interface[iDevice].tx_pbuf);
			pool_dump( &pdma_if->zap_dev->interface[iDevice].tx_pool, 0 );
		}

		ZAP_REG_WRITE(iDevice, ZAP_REG_ICR, ICR_SET_TX_RDY);
		pdma_if->interface[iDevice].tx_dma_count++;
	}

}

static irqreturn_t
zap_isr(
	int irq, 
	void *id
	)
{
	int iRetVal;
	char *pbuf;
	unsigned long ulLen;
	unsigned long ulOobLen;
	unsigned long ulFlags;
	unsigned long ulTemp;
	unsigned long icr;
    int iTxDevice;
    int iRxDevice;
	struct pool_entry * pentry;

	icr = ZAP_REG_READ(0, ZAP_REG_ICR);
    iTxDevice = (icr >> 8)  & 0x000000ff;
    iRxDevice = (icr >> 24) & 0x000000ff;

	//printk(KERN_ERR MODNAME "zap_isr, icr = 0x%08x\n",icr);

	if ( (icr & ICR_INT_RX_ERR) && (icr & ICR_MSK_RX_ERR) ){
		printk(KERN_ERR MODNAME "ZAP_ISR: Received RX_ERR\n");
	}

	if ( (icr & ICR_INT_RX_RDY) && (icr & ICR_MSK_RX_RDY) ){
		//printk(KERN_ERR MODNAME "ZAP_RX_ISR, RX_RDY RECEIVED!\n");
		iRetVal = pool_getbuf_try( &pdma_if->zap_dev->interface[iRxDevice].rx_pool, &pbuf, &ulTemp);
		if (!iRetVal){
			//This happens a lot
			//printk(KERN_ERR MODNAME ": ERROR : RX_ISR Called but no packet is available 0\n");
			ZAP_REG_WRITE(iRxDevice, ZAP_REG_ICR, ICR_CLR_RX_RDY);
			return;
		}

		ZAP_REG_WRITE(iRxDevice, ZAP_REG_ICR, ICR_CLR_RX_RDY);
        pdma_if->rx_dma_device = iRxDevice;

		ulTemp = ZAP_REG_READ(iRxDevice, ZAP_REG_RX_SIZE);
		//printk(KERN_ERR MODNAME "\tZAP_RX_ISR, ultemp = 0x%08x !\n",ulTemp);
		pdma_if->interface[iRxDevice].rx_pbuf = pbuf;
		pdma_if->interface[iRxDevice].rx_ulOobLen = ( (ulTemp >> 16) & 0x00003fff ) << 2;
		pdma_if->interface[iRxDevice].rx_ulDataLen = ((ulTemp & 0x0000ffff) << 2) + pdma_if->interface[iRxDevice].rx_ulOobLen;
		pdma_if->interface[iRxDevice].rx_ulFlags = 0;

		if (ulTemp & RX_SIZE_OOB_OVERFLOW_ERR)
			pdma_if->interface[iRxDevice].rx_ulFlags |= ZAP_DESC_FLAG_OVERFLOW_OOB;

		if (ulTemp & RX_SIZE_DAT_OVERFLOW_ERR)
			pdma_if->interface[iRxDevice].rx_ulFlags |= ZAP_DESC_FLAG_OVERFLOW_DATA;

		omap_set_dma_transfer_params( pdma_if->rx_dma_ch,
			OMAP_DMA_DATA_TYPE_S32,
			pdma_if->interface[iRxDevice].rx_ulDataLen >> 2,
			1,
			OMAP_DMA_SYNC_ELEMENT, 
			0, 0);

#ifdef MAPIT
		pdma_if->rx_dmabuf = dma_map_single(NULL, pbuf, pdma_if->interface[iRxDevice].rx_ulDataLen, DMA_FROM_DEVICE);
#else
		pdma_if->rx_dmabuf = rx_buffer2phys( pbuf );
#endif

		omap_set_dma_dest_params( pdma_if->rx_dma_ch, 
			OMAP_DMA_PORT_EMIFF, 
			OMAP_DMA_AMODE_POST_INC, 
			pdma_if->rx_dmabuf,
			0, 0);

		omap_start_dma( pdma_if->rx_dma_ch );
		pdma_if->interface[iRxDevice].rx_in_dma = 1;
	}

	if ( (icr & ICR_INT_TX_RDY) && (icr & ICR_MSK_TX_RDY) ){

		if (pdma_if->interface[iTxDevice].tx_on == 0)
			printk(KERN_ERR MODNAME "ERROR: ICR_INT_TX_RDY and tx_on = 0\n");

		//DEQBUF
		iRetVal = pool_deqbuf_try(	&pdma_if->zap_dev->interface[iTxDevice].tx_pool, &pbuf, &ulLen, &ulOobLen, &ulFlags );
		if (!iRetVal){
			printk(KERN_ERR MODNAME "ERROR: zap_tx_isr called, no buffer available\n");//THIS HAPPENS
			return -1;//FAIL
		}

		pdma_if->interface[iTxDevice].tx_pbuf = pbuf;

        pdma_if->tx_dma_device = iTxDevice;
		ZAP_REG_WRITE(iTxDevice, ZAP_REG_ICR, ICR_CLR_TX_RDY);

		//issue DMA Transaction
		omap_set_dma_transfer_params( pdma_if->tx_dma_ch,
			OMAP_DMA_DATA_TYPE_S32,
			ulLen >> 2,
			1,
			OMAP_DMA_SYNC_ELEMENT, 
			0, 0);

		omap_set_dma_src_params( pdma_if->tx_dma_ch, 
			OMAP_DMA_PORT_EMIFS,
			OMAP_DMA_AMODE_POST_INC,
			tx_buffer2phys( pbuf ),
			0, 0);

		omap_start_dma( pdma_if->tx_dma_ch );
	}

	if ( (icr & ICR_INT_TX_SIZE_RDY) && (icr & ICR_MSK_TX_SIZE_RDY) ){
		if (list_empty(&pdma_if->zap_dev->interface[iTxDevice].tx_pool.fifolist)){
			//This happens an awful lot (confirmed with smaller packets)
			//printk(KERN_ERR MODNAME "ICR_INT_TX_SIZE_RDY and FIFO LIST IS EMPTY\n");
			ZAP_REG_WRITE(iTxDevice, ZAP_REG_ICR, ICR_CLR_TX_SIZE_RDY);
		}else{
			pentry = list_entry( (&pdma_if->zap_dev->interface[iTxDevice].tx_pool)->fifolist.next, struct pool_entry, list);
			ulLen = (pentry->len) >> 2;
			ulOobLen = (pentry->ooblen) >> 2;
			ulTemp = ((ulOobLen << 16) & 0xffff0000) + ((ulLen) & 0x0000ffff);

			ZAP_REG_WRITE(iTxDevice, ZAP_REG_TX_SIZE,ulTemp);
		}
	}

	return IRQ_HANDLED;
}

///////////////////////////////////////////////////////////////////////////
//
// Public funcs
//
///////////////////////////////////////////////////////////////////////////

int 
dma_ll_init(//Eventually change name to dma_ll_init
	struct zap_dev * dev,
	unsigned long * prx_pool_paddr,
	char ** prx_pool_vaddr,
	unsigned long * prx_pool_size,
	unsigned long * ptx_pool_paddr,
	char ** ptx_pool_vaddr,
	unsigned long * ptx_pool_size
	)
{
	int err, i;

	unsigned long zap_pool_paddr;
	unsigned long zap_pool_size;
	unsigned long pow2_boundary_pages = 1;
	unsigned long ulTemp;

	//memset( pdma, 0, sizeof(struct dma) );

	// 
	// Create buffer pool at the end of mem, half for tx, half for rx, with a
	// small chunk at the end for DMA buf descriptors..  The size is the rest
	// of RAM not allocated to Linux, up to the next power-of-two boundary.
	//

#if 1
	while (pow2_boundary_pages < num_physpages) pow2_boundary_pages <<= 1;
	zap_pool_paddr = num_physpages * PAGE_SIZE + PHYS_MEM_BOTTOM;
	zap_pool_size =  (pow2_boundary_pages - num_physpages) * PAGE_SIZE;
/*
	if ( zap_pool_size < ZAP_POOL_MIN_SIZE ) {
		err = -ENOMEM;
		printk(KERN_ERR MODNAME ": pool size (%08X) is less than minimum (%08X)\n", 
				(unsigned int) zap_pool_size, 
				ZAP_POOL_MIN_SIZE 
				);
		return -ENOMEM;
	}
*/
	zap_pool_size -= ZAP_DMA_BD_BUF_SIZE;
	zap_pool_size = 8*1024*1024;


#ifdef MAPIT
	pdma_if->rx_buffer_paddr = __pa(rx_vaddr);
	pdma_if->rx_buffer_size = 4*1024*1024;
	pdma_if->tx_buffer_paddr = __pa(tx_vaddr);
	pdma_if->tx_buffer_size = 4*1024*1024;

	pdma_if->rx_buffer_vaddr = (char *)rx_vaddr;
	pdma_if->tx_buffer_vaddr = (char *)tx_vaddr;
#else
	pdma_if->rx_buffer_paddr = zap_pool_paddr;
	pdma_if->rx_buffer_size = zap_pool_size/2;
	pdma_if->tx_buffer_paddr = zap_pool_paddr + zap_pool_size/2;
	pdma_if->tx_buffer_size = zap_pool_size/2;

	pdma_if->rx_buffer_vaddr = ioremap(pdma_if->rx_buffer_paddr, pdma_if->rx_buffer_size);
	if (pdma_if->rx_buffer_vaddr == NULL) 
		return -ENOMEM;

	pdma_if->tx_buffer_vaddr = ioremap(pdma_if->tx_buffer_paddr, pdma_if->tx_buffer_size);
	if (pdma_if->tx_buffer_vaddr == NULL) 
		return -ENOMEM;
#endif
#endif
//END HERE
	*prx_pool_paddr = pdma_if->rx_buffer_paddr;
	*prx_pool_vaddr = pdma_if->rx_buffer_vaddr;
	*prx_pool_size= pdma_if->rx_buffer_size;
	*ptx_pool_paddr = pdma_if->tx_buffer_paddr;
	*ptx_pool_vaddr = pdma_if->tx_buffer_vaddr;
	*ptx_pool_size = pdma_if->tx_buffer_size;

    for (i=0;i<ZAP_MAX_DEVICES;i++){
	    pdma_if->interface[i].rx_on = 0;
	    pdma_if->interface[i].tx_on = 0;
    }

	pdma_if->zap_dev = dev;
	
	pdma_if->irq = -1;

	//
	// Setup the ZAP control memory area
	//

	pdma_if->zap_reg = (unsigned long *) ioremap (VSOC_MEM_BASE, VSOC_MEM_SIZE);
	if ( pdma_if->zap_reg == NULL ) {
		return -ENOMEM;
	}

	//
	// Request a dma channel for RX and TX
	//
	pdma_if->rx_dma_id = OMAP_DMA_NO_DEVICE;
	pdma_if->rx_dma_ch = -1;
	if ( err = omap_request_dma(pdma_if->rx_dma_id, "ZAP RX", 
				 zap_rx_dma_callback, 
				 (void *)pdma_if,
				 &pdma_if->rx_dma_ch) )
	{
		printk( KERN_ERR MODNAME ": ZAP RX request_dma failed\n" );
		pdma_if->rx_dma_id = 0;
		return err;
	}
	
	omap_set_dma_priority( pdma_if->rx_dma_ch, 0 , 1 );
	omap_set_dma_src_burst_mode( pdma_if->rx_dma_ch, OMAP_DMA_DATA_BURST_8 );
	omap_set_dma_src_data_pack( pdma_if->rx_dma_ch, 1 );
	omap_set_dma_dest_burst_mode( pdma_if->rx_dma_ch, OMAP_DMA_DATA_BURST_8 );
	omap_set_dma_dest_data_pack( pdma_if->rx_dma_ch, 1 );

	// NOTE: The source address never changes. Set here rather than before
	// each transaction. Same for the destination address for transmit
	//

	omap_set_dma_src_params( pdma_if->rx_dma_ch, 
		OMAP_DMA_PORT_EMIFS,
		OMAP_DMA_AMODE_POST_INC,
		ZAP_DATA_WINDOW,
		0, 0);

	pdma_if->tx_dma_id = OMAP_DMA_NO_DEVICE;
	pdma_if->tx_dma_ch = -1;
	if ( err = omap_request_dma(pdma_if->tx_dma_id, "ZAP TX", 
				 zap_tx_dma_callback, 
				 (void *)pdma_if, 
				 &pdma_if->tx_dma_ch) )
	{
		printk( KERN_ERR MODNAME ": ZAP TX request_dma failed\n" );
		pdma_if->tx_dma_id = 0;
		return err;
	}
	
	omap_set_dma_priority( pdma_if->tx_dma_ch, 0 , 1 );
	omap_set_dma_src_burst_mode( pdma_if->tx_dma_ch, OMAP_DMA_DATA_BURST_8 );
	omap_set_dma_src_data_pack( pdma_if->tx_dma_ch, 1 );
	omap_set_dma_dest_burst_mode( pdma_if->tx_dma_ch, OMAP_DMA_DATA_BURST_8 );
	omap_set_dma_dest_data_pack( pdma_if->tx_dma_ch, 1 );

	omap_set_dma_dest_params( pdma_if->tx_dma_ch, 
		//OMAP_DMA_PORT_EMIFS,
		OMAP_DMA_PORT_EMIFF,
		OMAP_DMA_AMODE_POST_INC,
		ZAP_DATA_WINDOW,
		0, 0);

    dma_ll_update_fpga_parameters();

	//SETUP IRQ LINE

	err = gpio_request(IV_ZAP_INT_IRQ_GPIO, NULL);
	if (err == 0) {
		err = gpio_direction_input(IV_ZAP_INT_IRQ_GPIO);
	}
	if (err < 0) {
		printk(KERN_ERR MODNAME ": Can't get GPIO for IRQ\n");
		return -EBADR;
	}
	pdma_if->irq = gpio_to_irq(IV_ZAP_INT_IRQ_GPIO);
	if ( pdma_if->irq < 0 ) {
		pdma_if->irq = -1;
		return pdma_if->irq;
	}

	//err = request_irq( pdma_if->irq, (irq_handler_t) zap_isr, IRQF_TRIGGER_RISING, MODNAME, pdma_if);
	err = request_irq( pdma_if->irq, (irq_handler_t) zap_isr, IRQF_TRIGGER_HIGH, MODNAME, pdma_if);

	if ( err ) {
		pdma_if->irq = -1;
		return err;
	}

	return 0;
}


int 
dma_ll_cleanup(
	void
	)
{

	return 0;
}


int 
dma_ll_start_rx(
	int iDevice
	)
{
	unsigned long ulPayloadWords;
	unsigned long ulOobWords;
	unsigned long ulTemp;

	pdma_if->interface[iDevice].rx_dma_count = 0;
	pdma_if->interface[iDevice].rx_in_dma = 0;
	pdma_if->interface[iDevice].rx_on = 1;

	//ulMaxWords = pdma_if->zap_dev->rx_payload_max_size >> 2;

	//ZAP_REG_WRITE_MASKED(ZAP_REG_CSR, ulMaxWords << CSR_MAX_RX_SIZE_SHIFT, CSR_MAX_RX_SIZE_MASK);

	//ZAP_REG_WRITE_MASKED(ZAP_REG_CSR, (pdma_if->zap_dev->rx_payload_max_size - 4) << CSR_MAX_RX_SIZE_SHIFT, CSR_MAX_RX_SIZE_MASK);
	//ZAP_REG_WRITE_MASKED(ZAP_REG_CSR, ((pdma_if->zap_dev->rx_payload_max_size - 4) >> 2) << CSR_MAX_RX_SIZE_SHIFT, CSR_MAX_RX_SIZE_MASK);

	ulPayloadWords = pdma_if->zap_dev->interface[iDevice].rx_payload_max_size >> 2;
	if (pdma_if->zap_dev->interface[iDevice].rx_header_enable)
		ulOobWords = pdma_if->zap_dev->interface[iDevice].rx_header_size >> 2;
	else
		ulOobWords = 0;

	ulPayloadWords -= ulOobWords;

	ulTemp = 0;
	ulTemp |= ( (ulPayloadWords - 1) << MAX_RX_DAT_SIZE_SHIFT ) & MAX_RX_DAT_SIZE_MASK;
	ulTemp |= ( (ulOobWords - 1) << MAX_RX_OOB_SIZE_SHIFT ) & MAX_RX_OOB_SIZE_MASK;

	ZAP_REG_WRITE(iDevice, ZAP_REG_MAX_RX_SIZE,ulTemp);

	ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, 0, CSR_RXEN);

	udelay(1);
	ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, CSR_RXEN, CSR_RXEN);

	if (pdma_if->zap_dev->interface[iDevice].rx_header_enable)
		ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, CSR_RX_OOB, CSR_RX_OOB);
	else
		ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, 0, CSR_RX_OOB);	

	ZAP_REG_WRITE(iDevice, ZAP_REG_ICR, ICR_SET_GLBL | ICR_SET_RX_RDY);

	//printk(KERN_ERR MODNAME "LEAVING dma_ll_start_rx\n");

	return 0;
}

int 
dma_ll_start_tx(
	int iDevice
	)
{
	pdma_if->interface[iDevice].tx_dma_count = 0;

	ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, 0, CSR_TXEN);

	udelay(1);
	ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, CSR_TXEN, CSR_TXEN);

	if (pdma_if->zap_dev->interface[iDevice].tx_header_enable)
		ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, CSR_TX_OOB, CSR_TX_OOB);
	else
		ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, 0, CSR_TX_OOB);	

	ZAP_REG_WRITE(iDevice, ZAP_REG_ICR, ICR_SET_GLBL | ICR_SET_TX_RDY);

	pdma_if->interface[iDevice].tx_on = 1;

	return 0;

}


int 
dma_ll_stop_rx(
	int iDevice
	)
{

	ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, 0, CSR_RXEN);

	ZAP_REG_WRITE(iDevice, ZAP_REG_ICR, ICR_CLR_RX_ERR | ICR_CLR_RX_RDY);

	pdma_if->interface[iDevice].rx_on = 0;
	return 0;
}

int 
dma_ll_stop_tx(
	int iDevice
	)
{
	int err=0;

	ZAP_REG_WRITE_MASKED(iDevice, ZAP_REG_CSR, 0, CSR_TXEN);

	ZAP_REG_WRITE(iDevice, ZAP_REG_ICR, ICR_CLR_TX_ERR | ICR_CLR_TX_RDY | ICR_CLR_TX_SIZE_RDY);

	pdma_if->interface[iDevice].tx_on = 0;
	return 0;
}


int 
dma_ll_rx_is_on(
	int iDevice
	)
{
	return pdma_if->interface[iDevice].rx_on;
}

int 
dma_ll_tx_is_on(
	int iDevice
	)
{
	return pdma_if->interface[iDevice].tx_on;
}

void
dma_ll_rx_free_buf(
    int iDevice
)
{
    disable_irq(pdma_if->irq);

	if (! pdma_if->interface[iDevice].rx_in_dma)
		ZAP_REG_WRITE(iDevice, ZAP_REG_ICR, ICR_SET_RX_RDY);

    enable_irq(pdma_if->irq);
}

void
dma_ll_tx_write_buf(
    int iDevice
)
{
	ZAP_REG_WRITE(iDevice, ZAP_REG_ICR, ICR_SET_TX_SIZE_RDY);
}

void
dma_ll_update_high_water_marks(int iDevice){
	unsigned long ulTemp;

	ulTemp = ZAP_REG_READ(iDevice, ZAP_REG_HIGH_WATER_MARK);
	pdma_if->zap_dev->interface[iDevice].rx_highwater = 4 * ((ulTemp >> HIGH_WATER_RX_SHIFT) & 0x0000ffff);
	pdma_if->zap_dev->interface[iDevice].tx_highwater = 4 * ((ulTemp >> HIGH_WATER_TX_SHIFT) & 0x0000ffff);
}

int dma_ll_rx_dma_count(int iDevice){ return pdma_if->interface[iDevice].rx_dma_count; }
int dma_ll_tx_dma_count(int iDevice){ return pdma_if->interface[iDevice].tx_dma_count; }

static int compareVsocVersion(int boardMajor, int boardMinor, char boardRelease, int compMajor, int compMinor, char compRelease){
//Returns -1 if board < comp
//Returns 0 if board = comp
//Returns +1 if board > comp

    if (boardMajor < compMajor)
        return -1;

    if (boardMajor > compMajor)
        return +1;

    if (boardMinor < compMinor)
        return -1;

    if (boardMinor > compMinor)
        return +1;

    if (tolower(boardRelease) < tolower(compRelease))
        return -1;

    if (tolower(boardRelease) > tolower(compRelease))
        return +1;

    return 0;
}

void
dma_ll_update_fpga_parameters()
{
	unsigned long ulTemp;
    int iTemp;

	ulTemp = ZAP_REG_READ(0,FPGA_VERSION_REG);
	pdma_if->zap_dev->fpga_params.fpga_board_code = (ulTemp >> FPGA_VERSION_BOARD_SHIFT) & 0x000000ff;
	pdma_if->zap_dev->fpga_params.fpga_version_major = (ulTemp >> FPGA_VERSION_MAJOR_SHIFT) & 0x000000ff;
	pdma_if->zap_dev->fpga_params.fpga_version_minor = (ulTemp >> FPGA_VERSION_MINOR_SHIFT) & 0x000000ff;
	pdma_if->zap_dev->fpga_params.fpga_version_release = (ulTemp >> FPGA_VERSION_RELEASE_SHIFT) & 0x000000ff;


    //If Velocity SOC is version 2.3.c or earlier, set num_interface = 1
#define VSOC_ONE_PORT_LAST_MAJOR        2
#define VSOC_ONE_PORT_LAST_MINOR        3
#define VSOC_ONE_PORT_LAST_RELEASE      'c'

    iTemp = compareVsocVersion(pdma_if->zap_dev->fpga_params.fpga_version_major, pdma_if->zap_dev->fpga_params.fpga_version_minor, pdma_if->zap_dev->fpga_params.fpga_version_release,
                                VSOC_ONE_PORT_LAST_MAJOR, VSOC_ONE_PORT_LAST_MINOR, VSOC_ONE_PORT_LAST_RELEASE);

    if (iTemp <= 0){
        pdma_if->zap_dev->fpga_params.num_interfaces = 1;
    }else{
	    ulTemp = ZAP_REG_READ(0,ZAP_REG_CSR);
        pdma_if->zap_dev->fpga_params.num_interfaces = (ulTemp >> NUM_INTERFACES_SHIFT) & 0x0000ffff;
    }

	ulTemp = ZAP_REG_READ(0,ZAP_REG_RX_FIFO_SIZE);
	pdma_if->zap_dev->fpga_params.rx_dat_fifo_size = 4 * (ulTemp & 0x0000ffff);
	pdma_if->zap_dev->fpga_params.rx_oob_fifo_size = 4 * ((ulTemp >> 16) & 0x0000ffff);

	ulTemp = ZAP_REG_READ(0,ZAP_REG_TX_FIFO_SIZE);
	pdma_if->zap_dev->fpga_params.tx_dat_fifo_size = 4 * (ulTemp & 0x0000ffff);
	pdma_if->zap_dev->fpga_params.tx_oob_fifo_size = 4 * ((ulTemp >> 16) & 0x0000ffff);
}

