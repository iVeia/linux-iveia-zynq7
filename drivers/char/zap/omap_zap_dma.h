#ifndef __OMAP_ZAP_DMA_H__
#define __OMAP_ZAP_DMA_H__

#define VSOC_MEM_BASE			(0x05400000)
#define VSOC_MEM_SIZE			(0x1000)

#define VSOC_TX_DATA_LEN_REG	(0x00000004/4)
#define VSOC_TX_OOB_LEN_REG		(0x0000000c/4)
#define VSOC_RX_DATA_LEN_REG	(0x00000104/4)
#define VSOC_RX_OOB_LEN_REG		(0x0000010c/4)
#define VSOC_RX_MAX_SIZE		(0x0000020c/4)
#define VSOC_DMA_ON				(0x0000030c/4)
#define VSOC_INTC_SET           (0x00000204/4)        
#define VSOC_INTC_CLEAR         (0x00000208/4)
#define VSOC_TX_DAT_NEEDED      (0x00000300/4)
#define VSOC_STAT_REG           (0x00000308/4)
#define VSOC_TX_DAT_AVAILABLE   (0x00000300/4)

#define STAT_TX_RDY_MASK        (0x00000002)
#define STAT_RX_RDY_MASK        (0x00000001)

#define INTC_RX_FREE_BUFF_AVAIL_BIT     0
#define INTC_RX_DMA_IN_PROGRESS_BIT     1
#define INTC_RX_DMA_ENABLED_BIT         2

#define INTC_TX_DMA_IN_PROGRESS_BIT     8
#define INTC_TX_SW_BUFF_AVAIL_BIT       9
#define INTC_TX_DMA_ENABLED_BIT         10

#define CTRL_RX_EN_BIT                  0
#define CTRL_TX_EN_BIT                  1
#define CTRL_RX_OOB_EN_BIT              2
#define CTRL_TX_OOB_EN_BIT              3

#define ZAP_DATA_WINDOW			(0x08000000)
#define ZAP_DATA_WINDOW_SIZE	(0x01000000)
#define ZAP_OOB_WINDOW			(0x08000000)
#define ZAP_OOB_WINDOW_SIZE		(0x01000000)

#define IV_ZAP_INT_IRQ_GPIO		(161)

#define ZAP_DMA_BD_BUF_SIZE (0)
#define DMA_BD_RX_NUM ( 0xFFFFFFFF )
#define DMA_BD_TX_NUM ( 0xFFFFFFFF )


///////////////////////////////////////////////////////////////////////////
//
// Public funcs
//
///////////////////////////////////////////////////////////////////////////
  
void
dma_rx_set_max_size(
	unsigned long ulSize
	);
  
void
dma_tx_set_max_size(
	unsigned long ulSize
	);
  
int
dma_dump_string(
	char *buf
	);
#endif
