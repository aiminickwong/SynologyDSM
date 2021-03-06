#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/circ_buf.h>
#include <linux/interrupt.h>
#include <linux/etherdevice.h>
#include <linux/platform_device.h>
#include <linux/skbuff.h>
#include <linux/ethtool.h>
#include <linux/if.h>
#include <linux/crc32.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>

#define XGMAC_CONTROL		0x00000000	 
#define XGMAC_FRAME_FILTER	0x00000004	 
#define XGMAC_FLOW_CTRL		0x00000018	 
#define XGMAC_VLAN_TAG		0x0000001C	 
#define XGMAC_VERSION		0x00000020	 
#define XGMAC_VLAN_INCL		0x00000024	 
#define XGMAC_LPI_CTRL		0x00000028	 
#define XGMAC_LPI_TIMER		0x0000002C	 
#define XGMAC_TX_PACE		0x00000030	 
#define XGMAC_VLAN_HASH		0x00000034	 
#define XGMAC_DEBUG		0x00000038	 
#define XGMAC_INT_STAT		0x0000003C	 
#define XGMAC_ADDR_HIGH(reg)	(0x00000040 + ((reg) * 8))
#define XGMAC_ADDR_LOW(reg)	(0x00000044 + ((reg) * 8))
#define XGMAC_HASH(n)		(0x00000300 + (n) * 4)  
#define XGMAC_NUM_HASH		16
#define XGMAC_OMR		0x00000400
#define XGMAC_REMOTE_WAKE	0x00000700	 
#define XGMAC_PMT		0x00000704	 
#define XGMAC_MMC_CTRL		0x00000800	 
#define XGMAC_MMC_INTR_RX	0x00000804	 
#define XGMAC_MMC_INTR_TX	0x00000808	 
#define XGMAC_MMC_INTR_MASK_RX	0x0000080c	 
#define XGMAC_MMC_INTR_MASK_TX	0x00000810	 

#define XGMAC_MMC_TXOCTET_GB_LO	0x00000814
#define XGMAC_MMC_TXOCTET_GB_HI	0x00000818
#define XGMAC_MMC_TXFRAME_GB_LO	0x0000081C
#define XGMAC_MMC_TXFRAME_GB_HI	0x00000820
#define XGMAC_MMC_TXBCFRAME_G	0x00000824
#define XGMAC_MMC_TXMCFRAME_G	0x0000082C
#define XGMAC_MMC_TXUCFRAME_GB	0x00000864
#define XGMAC_MMC_TXMCFRAME_GB	0x0000086C
#define XGMAC_MMC_TXBCFRAME_GB	0x00000874
#define XGMAC_MMC_TXUNDERFLOW	0x0000087C
#define XGMAC_MMC_TXOCTET_G_LO	0x00000884
#define XGMAC_MMC_TXOCTET_G_HI	0x00000888
#define XGMAC_MMC_TXFRAME_G_LO	0x0000088C
#define XGMAC_MMC_TXFRAME_G_HI	0x00000890
#define XGMAC_MMC_TXPAUSEFRAME	0x00000894
#define XGMAC_MMC_TXVLANFRAME	0x0000089C

#define XGMAC_MMC_RXFRAME_GB_LO	0x00000900
#define XGMAC_MMC_RXFRAME_GB_HI	0x00000904
#define XGMAC_MMC_RXOCTET_GB_LO	0x00000908
#define XGMAC_MMC_RXOCTET_GB_HI	0x0000090C
#define XGMAC_MMC_RXOCTET_G_LO	0x00000910
#define XGMAC_MMC_RXOCTET_G_HI	0x00000914
#define XGMAC_MMC_RXBCFRAME_G	0x00000918
#define XGMAC_MMC_RXMCFRAME_G	0x00000920
#define XGMAC_MMC_RXCRCERR	0x00000928
#define XGMAC_MMC_RXRUNT	0x00000930
#define XGMAC_MMC_RXJABBER	0x00000934
#define XGMAC_MMC_RXUCFRAME_G	0x00000970
#define XGMAC_MMC_RXLENGTHERR	0x00000978
#define XGMAC_MMC_RXPAUSEFRAME	0x00000988
#define XGMAC_MMC_RXOVERFLOW	0x00000990
#define XGMAC_MMC_RXVLANFRAME	0x00000998
#define XGMAC_MMC_RXWATCHDOG	0x000009a0

#define XGMAC_DMA_BUS_MODE	0x00000f00	 
#define XGMAC_DMA_TX_POLL	0x00000f04	 
#define XGMAC_DMA_RX_POLL	0x00000f08	 
#define XGMAC_DMA_RX_BASE_ADDR	0x00000f0c	 
#define XGMAC_DMA_TX_BASE_ADDR	0x00000f10	 
#define XGMAC_DMA_STATUS	0x00000f14	 
#define XGMAC_DMA_CONTROL	0x00000f18	 
#define XGMAC_DMA_INTR_ENA	0x00000f1c	 
#define XGMAC_DMA_MISS_FRAME_CTR 0x00000f20	 
#define XGMAC_DMA_RI_WDOG_TIMER	0x00000f24	 
#define XGMAC_DMA_AXI_BUS	0x00000f28	 
#define XGMAC_DMA_AXI_STATUS	0x00000f2C	 
#define XGMAC_DMA_HW_FEATURE	0x00000f58	 

#define XGMAC_ADDR_AE		0x80000000
#define XGMAC_MAX_FILTER_ADDR	31

#define XGMAC_PMT_POINTER_RESET	0x80000000
#define XGMAC_PMT_GLBL_UNICAST	0x00000200
#define XGMAC_PMT_WAKEUP_RX_FRM	0x00000040
#define XGMAC_PMT_MAGIC_PKT	0x00000020
#define XGMAC_PMT_WAKEUP_FRM_EN	0x00000004
#define XGMAC_PMT_MAGIC_PKT_EN	0x00000002
#define XGMAC_PMT_POWERDOWN	0x00000001

#define XGMAC_CONTROL_SPD	0x40000000	 
#define XGMAC_CONTROL_SPD_MASK	0x60000000
#define XGMAC_CONTROL_SPD_1G	0x60000000
#define XGMAC_CONTROL_SPD_2_5G	0x40000000
#define XGMAC_CONTROL_SPD_10G	0x00000000
#define XGMAC_CONTROL_SARC	0x10000000	 
#define XGMAC_CONTROL_SARK_MASK	0x18000000
#define XGMAC_CONTROL_CAR	0x04000000	 
#define XGMAC_CONTROL_CAR_MASK	0x06000000
#define XGMAC_CONTROL_DP	0x01000000	 
#define XGMAC_CONTROL_WD	0x00800000	 
#define XGMAC_CONTROL_JD	0x00400000	 
#define XGMAC_CONTROL_JE	0x00100000	 
#define XGMAC_CONTROL_LM	0x00001000	 
#define XGMAC_CONTROL_IPC	0x00000400	 
#define XGMAC_CONTROL_ACS	0x00000080	 
#define XGMAC_CONTROL_DDIC	0x00000010	 
#define XGMAC_CONTROL_TE	0x00000008	 
#define XGMAC_CONTROL_RE	0x00000004	 

#define XGMAC_FRAME_FILTER_PR	0x00000001	 
#define XGMAC_FRAME_FILTER_HUC	0x00000002	 
#define XGMAC_FRAME_FILTER_HMC	0x00000004	 
#define XGMAC_FRAME_FILTER_DAIF	0x00000008	 
#define XGMAC_FRAME_FILTER_PM	0x00000010	 
#define XGMAC_FRAME_FILTER_DBF	0x00000020	 
#define XGMAC_FRAME_FILTER_SAIF	0x00000100	 
#define XGMAC_FRAME_FILTER_SAF	0x00000200	 
#define XGMAC_FRAME_FILTER_HPF	0x00000400	 
#define XGMAC_FRAME_FILTER_VHF	0x00000800	 
#define XGMAC_FRAME_FILTER_VPF	0x00001000	 
#define XGMAC_FRAME_FILTER_RA	0x80000000	 

#define XGMAC_FLOW_CTRL_PT_MASK	0xffff0000	 
#define XGMAC_FLOW_CTRL_PT_SHIFT	16
#define XGMAC_FLOW_CTRL_DZQP	0x00000080	 
#define XGMAC_FLOW_CTRL_PLT	0x00000020	 
#define XGMAC_FLOW_CTRL_PLT_MASK 0x00000030	 
#define XGMAC_FLOW_CTRL_UP	0x00000008	 
#define XGMAC_FLOW_CTRL_RFE	0x00000004	 
#define XGMAC_FLOW_CTRL_TFE	0x00000002	 
#define XGMAC_FLOW_CTRL_FCB_BPA	0x00000001	 

#define XGMAC_INT_STAT_PMTIM	0x00800000	 
#define XGMAC_INT_STAT_PMT	0x0080		 
#define XGMAC_INT_STAT_LPI	0x0040		 

#define DMA_BUS_MODE_SFT_RESET	0x00000001	 
#define DMA_BUS_MODE_DSL_MASK	0x0000007c	 
#define DMA_BUS_MODE_DSL_SHIFT	2		 
#define DMA_BUS_MODE_ATDS	0x00000080	 

#define DMA_BUS_MODE_PBL_MASK	0x00003f00	 
#define DMA_BUS_MODE_PBL_SHIFT	8
#define DMA_BUS_MODE_FB		0x00010000	 
#define DMA_BUS_MODE_RPBL_MASK	0x003e0000	 
#define DMA_BUS_MODE_RPBL_SHIFT	17
#define DMA_BUS_MODE_USP	0x00800000
#define DMA_BUS_MODE_8PBL	0x01000000
#define DMA_BUS_MODE_AAL	0x02000000

#define DMA_BUS_PR_RATIO_MASK	0x0000c000	 
#define DMA_BUS_PR_RATIO_SHIFT	14
#define DMA_BUS_FB		0x00010000	 

#define DMA_CONTROL_ST		0x00002000	 
#define DMA_CONTROL_SR		0x00000002	 
#define DMA_CONTROL_DFF		0x01000000	 
#define DMA_CONTROL_OSF		0x00000004	 

#define DMA_INTR_ENA_NIE	0x00010000	 
#define DMA_INTR_ENA_AIE	0x00008000	 
#define DMA_INTR_ENA_ERE	0x00004000	 
#define DMA_INTR_ENA_FBE	0x00002000	 
#define DMA_INTR_ENA_ETE	0x00000400	 
#define DMA_INTR_ENA_RWE	0x00000200	 
#define DMA_INTR_ENA_RSE	0x00000100	 
#define DMA_INTR_ENA_RUE	0x00000080	 
#define DMA_INTR_ENA_RIE	0x00000040	 
#define DMA_INTR_ENA_UNE	0x00000020	 
#define DMA_INTR_ENA_OVE	0x00000010	 
#define DMA_INTR_ENA_TJE	0x00000008	 
#define DMA_INTR_ENA_TUE	0x00000004	 
#define DMA_INTR_ENA_TSE	0x00000002	 
#define DMA_INTR_ENA_TIE	0x00000001	 

#define DMA_INTR_NORMAL		(DMA_INTR_ENA_NIE | DMA_INTR_ENA_RIE | \
				 DMA_INTR_ENA_TUE | DMA_INTR_ENA_TIE)

#define DMA_INTR_ABNORMAL	(DMA_INTR_ENA_AIE | DMA_INTR_ENA_FBE | \
				 DMA_INTR_ENA_RWE | DMA_INTR_ENA_RSE | \
				 DMA_INTR_ENA_RUE | DMA_INTR_ENA_UNE | \
				 DMA_INTR_ENA_OVE | DMA_INTR_ENA_TJE | \
				 DMA_INTR_ENA_TSE)

#define DMA_INTR_DEFAULT_MASK	(DMA_INTR_NORMAL | DMA_INTR_ABNORMAL)

#define DMA_STATUS_GMI		0x08000000	 
#define DMA_STATUS_GLI		0x04000000	 
#define DMA_STATUS_EB_MASK	0x00380000	 
#define DMA_STATUS_EB_TX_ABORT	0x00080000	 
#define DMA_STATUS_EB_RX_ABORT	0x00100000	 
#define DMA_STATUS_TS_MASK	0x00700000	 
#define DMA_STATUS_TS_SHIFT	20
#define DMA_STATUS_RS_MASK	0x000e0000	 
#define DMA_STATUS_RS_SHIFT	17
#define DMA_STATUS_NIS		0x00010000	 
#define DMA_STATUS_AIS		0x00008000	 
#define DMA_STATUS_ERI		0x00004000	 
#define DMA_STATUS_FBI		0x00002000	 
#define DMA_STATUS_ETI		0x00000400	 
#define DMA_STATUS_RWT		0x00000200	 
#define DMA_STATUS_RPS		0x00000100	 
#define DMA_STATUS_RU		0x00000080	 
#define DMA_STATUS_RI		0x00000040	 
#define DMA_STATUS_UNF		0x00000020	 
#define DMA_STATUS_OVF		0x00000010	 
#define DMA_STATUS_TJT		0x00000008	 
#define DMA_STATUS_TU		0x00000004	 
#define DMA_STATUS_TPS		0x00000002	 
#define DMA_STATUS_TI		0x00000001	 

#define MAC_ENABLE_TX		0x00000008	 
#define MAC_ENABLE_RX		0x00000004	 

#define XGMAC_OMR_TSF		0x00200000	 
#define XGMAC_OMR_FTF		0x00100000	 
#define XGMAC_OMR_TTC		0x00020000	 
#define XGMAC_OMR_TTC_MASK	0x00030000
#define XGMAC_OMR_RFD		0x00006000	 
#define XGMAC_OMR_RFD_MASK	0x00007000	 
#define XGMAC_OMR_RFA		0x00000600	 
#define XGMAC_OMR_RFA_MASK	0x00000E00	 
#define XGMAC_OMR_EFC		0x00000100	 
#define XGMAC_OMR_FEF		0x00000080	 
#define XGMAC_OMR_DT		0x00000040	 
#define XGMAC_OMR_RSF		0x00000020	 
#define XGMAC_OMR_RTC_256	0x00000018	 
#define XGMAC_OMR_RTC_MASK	0x00000018	 

#define DMA_HW_FEAT_TXCOESEL	0x00010000	 

#define XGMAC_MMC_CTRL_CNT_FRZ	0x00000008

#define MAX_DESC_BUF_SZ		(0x2000 - 8)

#define RXDESC_EXT_STATUS	0x00000001
#define RXDESC_CRC_ERR		0x00000002
#define RXDESC_RX_ERR		0x00000008
#define RXDESC_RX_WDOG		0x00000010
#define RXDESC_FRAME_TYPE	0x00000020
#define RXDESC_GIANT_FRAME	0x00000080
#define RXDESC_LAST_SEG		0x00000100
#define RXDESC_FIRST_SEG	0x00000200
#define RXDESC_VLAN_FRAME	0x00000400
#define RXDESC_OVERFLOW_ERR	0x00000800
#define RXDESC_LENGTH_ERR	0x00001000
#define RXDESC_SA_FILTER_FAIL	0x00002000
#define RXDESC_DESCRIPTOR_ERR	0x00004000
#define RXDESC_ERROR_SUMMARY	0x00008000
#define RXDESC_FRAME_LEN_OFFSET	16
#define RXDESC_FRAME_LEN_MASK	0x3fff0000
#define RXDESC_DA_FILTER_FAIL	0x40000000

#define RXDESC1_END_RING	0x00008000

#define RXDESC_IP_PAYLOAD_MASK	0x00000003
#define RXDESC_IP_PAYLOAD_UDP	0x00000001
#define RXDESC_IP_PAYLOAD_TCP	0x00000002
#define RXDESC_IP_PAYLOAD_ICMP	0x00000003
#define RXDESC_IP_HEADER_ERR	0x00000008
#define RXDESC_IP_PAYLOAD_ERR	0x00000010
#define RXDESC_IPV4_PACKET	0x00000040
#define RXDESC_IPV6_PACKET	0x00000080
#define TXDESC_UNDERFLOW_ERR	0x00000001
#define TXDESC_JABBER_TIMEOUT	0x00000002
#define TXDESC_LOCAL_FAULT	0x00000004
#define TXDESC_REMOTE_FAULT	0x00000008
#define TXDESC_VLAN_FRAME	0x00000010
#define TXDESC_FRAME_FLUSHED	0x00000020
#define TXDESC_IP_HEADER_ERR	0x00000040
#define TXDESC_PAYLOAD_CSUM_ERR	0x00000080
#define TXDESC_ERROR_SUMMARY	0x00008000
#define TXDESC_SA_CTRL_INSERT	0x00040000
#define TXDESC_SA_CTRL_REPLACE	0x00080000
#define TXDESC_2ND_ADDR_CHAINED	0x00100000
#define TXDESC_END_RING		0x00200000
#define TXDESC_CSUM_IP		0x00400000
#define TXDESC_CSUM_IP_PAYLD	0x00800000
#define TXDESC_CSUM_ALL		0x00C00000
#define TXDESC_CRC_EN_REPLACE	0x01000000
#define TXDESC_CRC_EN_APPEND	0x02000000
#define TXDESC_DISABLE_PAD	0x04000000
#define TXDESC_FIRST_SEG	0x10000000
#define TXDESC_LAST_SEG		0x20000000
#define TXDESC_INTERRUPT	0x40000000

#define DESC_OWN		0x80000000
#define DESC_BUFFER1_SZ_MASK	0x00001fff
#define DESC_BUFFER2_SZ_MASK	0x1fff0000
#define DESC_BUFFER2_SZ_OFFSET	16

struct xgmac_dma_desc {
	__le32 flags;
	__le32 buf_size;
	__le32 buf1_addr;		 
	__le32 buf2_addr;		 
	__le32 ext_status;
	__le32 res[3];
};

struct xgmac_extra_stats {
	 
	unsigned long tx_jabber;
	unsigned long tx_frame_flushed;
	unsigned long tx_payload_error;
	unsigned long tx_ip_header_error;
	unsigned long tx_local_fault;
	unsigned long tx_remote_fault;
	 
	unsigned long rx_watchdog;
	unsigned long rx_da_filter_fail;
	unsigned long rx_sa_filter_fail;
	unsigned long rx_payload_error;
	unsigned long rx_ip_header_error;
	 
	unsigned long tx_undeflow;
	unsigned long tx_process_stopped;
	unsigned long rx_buf_unav;
	unsigned long rx_process_stopped;
	unsigned long tx_early;
	unsigned long fatal_bus_error;
};

struct xgmac_priv {
	struct xgmac_dma_desc *dma_rx;
	struct sk_buff **rx_skbuff;
	unsigned int rx_tail;
	unsigned int rx_head;

	struct xgmac_dma_desc *dma_tx;
	struct sk_buff **tx_skbuff;
	unsigned int tx_head;
	unsigned int tx_tail;
	int tx_irq_cnt;

	void __iomem *base;
	unsigned int dma_buf_sz;
	dma_addr_t dma_rx_phy;
	dma_addr_t dma_tx_phy;

	struct net_device *dev;
	struct device *device;
	struct napi_struct napi;

	struct xgmac_extra_stats xstats;

	spinlock_t stats_lock;
	int pmt_irq;
	char rx_pause;
	char tx_pause;
	int wolopts;
};

#define MAX_MTU			9000
#define PAUSE_TIME		0x400

#define DMA_RX_RING_SZ		256
#define DMA_TX_RING_SZ		128
 
#define TX_THRESH		(DMA_TX_RING_SZ/4)

#define dma_ring_incr(n, s)	(((n) + 1) & ((s) - 1))
#define dma_ring_space(h, t, s)	CIRC_SPACE(h, t, s)
#define dma_ring_cnt(h, t, s)	CIRC_CNT(h, t, s)

static inline void desc_set_buf_len(struct xgmac_dma_desc *p, u32 buf_sz)
{
	if (buf_sz > MAX_DESC_BUF_SZ)
		p->buf_size = cpu_to_le32(MAX_DESC_BUF_SZ |
			(buf_sz - MAX_DESC_BUF_SZ) << DESC_BUFFER2_SZ_OFFSET);
	else
		p->buf_size = cpu_to_le32(buf_sz);
}

static inline int desc_get_buf_len(struct xgmac_dma_desc *p)
{
	u32 len = cpu_to_le32(p->flags);
	return (len & DESC_BUFFER1_SZ_MASK) +
		((len & DESC_BUFFER2_SZ_MASK) >> DESC_BUFFER2_SZ_OFFSET);
}

static inline void desc_init_rx_desc(struct xgmac_dma_desc *p, int ring_size,
				     int buf_sz)
{
	struct xgmac_dma_desc *end = p + ring_size - 1;

	memset(p, 0, sizeof(*p) * ring_size);

	for (; p <= end; p++)
		desc_set_buf_len(p, buf_sz);

	end->buf_size |= cpu_to_le32(RXDESC1_END_RING);
}

static inline void desc_init_tx_desc(struct xgmac_dma_desc *p, u32 ring_size)
{
	memset(p, 0, sizeof(*p) * ring_size);
	p[ring_size - 1].flags = cpu_to_le32(TXDESC_END_RING);
}

static inline int desc_get_owner(struct xgmac_dma_desc *p)
{
	return le32_to_cpu(p->flags) & DESC_OWN;
}

static inline void desc_set_rx_owner(struct xgmac_dma_desc *p)
{
	 
	p->flags = cpu_to_le32(DESC_OWN);
}

static inline void desc_set_tx_owner(struct xgmac_dma_desc *p, u32 flags)
{
	u32 tmpflags = le32_to_cpu(p->flags);
	tmpflags &= TXDESC_END_RING;
	tmpflags |= flags | DESC_OWN;
	p->flags = cpu_to_le32(tmpflags);
}

static inline int desc_get_tx_ls(struct xgmac_dma_desc *p)
{
	return le32_to_cpu(p->flags) & TXDESC_LAST_SEG;
}

static inline u32 desc_get_buf_addr(struct xgmac_dma_desc *p)
{
	return le32_to_cpu(p->buf1_addr);
}

static inline void desc_set_buf_addr(struct xgmac_dma_desc *p,
				     u32 paddr, int len)
{
	p->buf1_addr = cpu_to_le32(paddr);
	if (len > MAX_DESC_BUF_SZ)
		p->buf2_addr = cpu_to_le32(paddr + MAX_DESC_BUF_SZ);
}

static inline void desc_set_buf_addr_and_size(struct xgmac_dma_desc *p,
					      u32 paddr, int len)
{
	desc_set_buf_len(p, len);
	desc_set_buf_addr(p, paddr, len);
}

static inline int desc_get_rx_frame_len(struct xgmac_dma_desc *p)
{
	u32 data = le32_to_cpu(p->flags);
	u32 len = (data & RXDESC_FRAME_LEN_MASK) >> RXDESC_FRAME_LEN_OFFSET;
	if (data & RXDESC_FRAME_TYPE)
		len -= ETH_FCS_LEN;

	return len;
}

static void xgmac_dma_flush_tx_fifo(void __iomem *ioaddr)
{
	int timeout = 1000;
	u32 reg = readl(ioaddr + XGMAC_OMR);
	writel(reg | XGMAC_OMR_FTF, ioaddr + XGMAC_OMR);

	while ((timeout-- > 0) && readl(ioaddr + XGMAC_OMR) & XGMAC_OMR_FTF)
		udelay(1);
}

static int desc_get_tx_status(struct xgmac_priv *priv, struct xgmac_dma_desc *p)
{
	struct xgmac_extra_stats *x = &priv->xstats;
	u32 status = le32_to_cpu(p->flags);

	if (!(status & TXDESC_ERROR_SUMMARY))
		return 0;

	netdev_dbg(priv->dev, "tx desc error = 0x%08x\n", status);
	if (status & TXDESC_JABBER_TIMEOUT)
		x->tx_jabber++;
	if (status & TXDESC_FRAME_FLUSHED)
		x->tx_frame_flushed++;
	if (status & TXDESC_UNDERFLOW_ERR)
		xgmac_dma_flush_tx_fifo(priv->base);
	if (status & TXDESC_IP_HEADER_ERR)
		x->tx_ip_header_error++;
	if (status & TXDESC_LOCAL_FAULT)
		x->tx_local_fault++;
	if (status & TXDESC_REMOTE_FAULT)
		x->tx_remote_fault++;
	if (status & TXDESC_PAYLOAD_CSUM_ERR)
		x->tx_payload_error++;

	return -1;
}

static int desc_get_rx_status(struct xgmac_priv *priv, struct xgmac_dma_desc *p)
{
	struct xgmac_extra_stats *x = &priv->xstats;
	int ret = CHECKSUM_UNNECESSARY;
	u32 status = le32_to_cpu(p->flags);
	u32 ext_status = le32_to_cpu(p->ext_status);

	if (status & RXDESC_DA_FILTER_FAIL) {
		netdev_dbg(priv->dev, "XGMAC RX : Dest Address filter fail\n");
		x->rx_da_filter_fail++;
		return -1;
	}

	if (!(status & RXDESC_FIRST_SEG) || !(status & RXDESC_LAST_SEG))
		return -1;

	if ((status & RXDESC_FRAME_TYPE) && (status & RXDESC_EXT_STATUS) &&
		!(ext_status & RXDESC_IP_PAYLOAD_MASK))
		ret = CHECKSUM_NONE;

	netdev_dbg(priv->dev, "rx status - frame type=%d, csum = %d, ext stat %08x\n",
		   (status & RXDESC_FRAME_TYPE) ? 1 : 0, ret, ext_status);

	if (!(status & RXDESC_ERROR_SUMMARY))
		return ret;

	if (status & (RXDESC_DESCRIPTOR_ERR | RXDESC_OVERFLOW_ERR |
		RXDESC_GIANT_FRAME | RXDESC_LENGTH_ERR | RXDESC_CRC_ERR))
		return -1;

	if (status & RXDESC_EXT_STATUS) {
		if (ext_status & RXDESC_IP_HEADER_ERR)
			x->rx_ip_header_error++;
		if (ext_status & RXDESC_IP_PAYLOAD_ERR)
			x->rx_payload_error++;
		netdev_dbg(priv->dev, "IP checksum error - stat %08x\n",
			   ext_status);
		return CHECKSUM_NONE;
	}

	return ret;
}

static inline void xgmac_mac_enable(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + XGMAC_CONTROL);
	value |= MAC_ENABLE_RX | MAC_ENABLE_TX;
	writel(value, ioaddr + XGMAC_CONTROL);

	value = readl(ioaddr + XGMAC_DMA_CONTROL);
	value |= DMA_CONTROL_ST | DMA_CONTROL_SR;
	writel(value, ioaddr + XGMAC_DMA_CONTROL);
}

static inline void xgmac_mac_disable(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + XGMAC_DMA_CONTROL);
	value &= ~(DMA_CONTROL_ST | DMA_CONTROL_SR);
	writel(value, ioaddr + XGMAC_DMA_CONTROL);

	value = readl(ioaddr + XGMAC_CONTROL);
	value &= ~(MAC_ENABLE_TX | MAC_ENABLE_RX);
	writel(value, ioaddr + XGMAC_CONTROL);
}

static void xgmac_set_mac_addr(void __iomem *ioaddr, unsigned char *addr,
			       int num)
{
	u32 data;

	data = (addr[5] << 8) | addr[4] | (num ? XGMAC_ADDR_AE : 0);
	writel(data, ioaddr + XGMAC_ADDR_HIGH(num));
	data = (addr[3] << 24) | (addr[2] << 16) | (addr[1] << 8) | addr[0];
	writel(data, ioaddr + XGMAC_ADDR_LOW(num));
}

static void xgmac_get_mac_addr(void __iomem *ioaddr, unsigned char *addr,
			       int num)
{
	u32 hi_addr, lo_addr;

	hi_addr = readl(ioaddr + XGMAC_ADDR_HIGH(num));
	lo_addr = readl(ioaddr + XGMAC_ADDR_LOW(num));

	addr[0] = lo_addr & 0xff;
	addr[1] = (lo_addr >> 8) & 0xff;
	addr[2] = (lo_addr >> 16) & 0xff;
	addr[3] = (lo_addr >> 24) & 0xff;
	addr[4] = hi_addr & 0xff;
	addr[5] = (hi_addr >> 8) & 0xff;
}

static int xgmac_set_flow_ctrl(struct xgmac_priv *priv, int rx, int tx)
{
	u32 reg;
	unsigned int flow = 0;

	priv->rx_pause = rx;
	priv->tx_pause = tx;

	if (rx || tx) {
		if (rx)
			flow |= XGMAC_FLOW_CTRL_RFE;
		if (tx)
			flow |= XGMAC_FLOW_CTRL_TFE;

		flow |= XGMAC_FLOW_CTRL_PLT | XGMAC_FLOW_CTRL_UP;
		flow |= (PAUSE_TIME << XGMAC_FLOW_CTRL_PT_SHIFT);

		writel(flow, priv->base + XGMAC_FLOW_CTRL);

		reg = readl(priv->base + XGMAC_OMR);
		reg |= XGMAC_OMR_EFC;
		writel(reg, priv->base + XGMAC_OMR);
	} else {
		writel(0, priv->base + XGMAC_FLOW_CTRL);

		reg = readl(priv->base + XGMAC_OMR);
		reg &= ~XGMAC_OMR_EFC;
		writel(reg, priv->base + XGMAC_OMR);
	}

	return 0;
}

static void xgmac_rx_refill(struct xgmac_priv *priv)
{
	struct xgmac_dma_desc *p;
	dma_addr_t paddr;
	int bufsz = priv->dev->mtu + ETH_HLEN + ETH_FCS_LEN;

	while (dma_ring_space(priv->rx_head, priv->rx_tail, DMA_RX_RING_SZ) > 1) {
		int entry = priv->rx_head;
		struct sk_buff *skb;

		p = priv->dma_rx + entry;

		if (priv->rx_skbuff[entry] == NULL) {
			skb = netdev_alloc_skb_ip_align(priv->dev, bufsz);
			if (unlikely(skb == NULL))
				break;

			priv->rx_skbuff[entry] = skb;
			paddr = dma_map_single(priv->device, skb->data,
					       bufsz, DMA_FROM_DEVICE);
			desc_set_buf_addr(p, paddr, priv->dma_buf_sz);
		}

		netdev_dbg(priv->dev, "rx ring: head %d, tail %d\n",
			priv->rx_head, priv->rx_tail);

		priv->rx_head = dma_ring_incr(priv->rx_head, DMA_RX_RING_SZ);
		desc_set_rx_owner(p);
	}
}

static int xgmac_dma_desc_rings_init(struct net_device *dev)
{
	struct xgmac_priv *priv = netdev_priv(dev);
	unsigned int bfsize;

	bfsize = ALIGN(dev->mtu + ETH_HLEN + ETH_FCS_LEN + NET_IP_ALIGN, 8);

	netdev_dbg(priv->dev, "mtu [%d] bfsize [%d]\n", dev->mtu, bfsize);

	priv->rx_skbuff = kzalloc(sizeof(struct sk_buff *) * DMA_RX_RING_SZ,
				  GFP_KERNEL);
	if (!priv->rx_skbuff)
		return -ENOMEM;

	priv->dma_rx = dma_alloc_coherent(priv->device,
					  DMA_RX_RING_SZ *
					  sizeof(struct xgmac_dma_desc),
					  &priv->dma_rx_phy,
					  GFP_KERNEL);
	if (!priv->dma_rx)
		goto err_dma_rx;

	priv->tx_skbuff = kzalloc(sizeof(struct sk_buff *) * DMA_TX_RING_SZ,
				  GFP_KERNEL);
	if (!priv->tx_skbuff)
		goto err_tx_skb;

	priv->dma_tx = dma_alloc_coherent(priv->device,
					  DMA_TX_RING_SZ *
					  sizeof(struct xgmac_dma_desc),
					  &priv->dma_tx_phy,
					  GFP_KERNEL);
	if (!priv->dma_tx)
		goto err_dma_tx;

	netdev_dbg(priv->dev, "DMA desc rings: virt addr (Rx %p, "
	    "Tx %p)\n\tDMA phy addr (Rx 0x%08x, Tx 0x%08x)\n",
	    priv->dma_rx, priv->dma_tx,
	    (unsigned int)priv->dma_rx_phy, (unsigned int)priv->dma_tx_phy);

	priv->rx_tail = 0;
	priv->rx_head = 0;
	priv->dma_buf_sz = bfsize;
	desc_init_rx_desc(priv->dma_rx, DMA_RX_RING_SZ, priv->dma_buf_sz);
	xgmac_rx_refill(priv);

	priv->tx_tail = 0;
	priv->tx_head = 0;
	desc_init_tx_desc(priv->dma_tx, DMA_TX_RING_SZ);

	writel(priv->dma_tx_phy, priv->base + XGMAC_DMA_TX_BASE_ADDR);
	writel(priv->dma_rx_phy, priv->base + XGMAC_DMA_RX_BASE_ADDR);

	return 0;

err_dma_tx:
	kfree(priv->tx_skbuff);
err_tx_skb:
	dma_free_coherent(priv->device,
			  DMA_RX_RING_SZ * sizeof(struct xgmac_dma_desc),
			  priv->dma_rx, priv->dma_rx_phy);
err_dma_rx:
	kfree(priv->rx_skbuff);
	return -ENOMEM;
}

static void xgmac_free_rx_skbufs(struct xgmac_priv *priv)
{
	int i;
	struct xgmac_dma_desc *p;

	if (!priv->rx_skbuff)
		return;

	for (i = 0; i < DMA_RX_RING_SZ; i++) {
		if (priv->rx_skbuff[i] == NULL)
			continue;

		p = priv->dma_rx + i;
		dma_unmap_single(priv->device, desc_get_buf_addr(p),
				 priv->dma_buf_sz, DMA_FROM_DEVICE);
		dev_kfree_skb_any(priv->rx_skbuff[i]);
		priv->rx_skbuff[i] = NULL;
	}
}

static void xgmac_free_tx_skbufs(struct xgmac_priv *priv)
{
	int i, f;
	struct xgmac_dma_desc *p;

	if (!priv->tx_skbuff)
		return;

	for (i = 0; i < DMA_TX_RING_SZ; i++) {
		if (priv->tx_skbuff[i] == NULL)
			continue;

		p = priv->dma_tx + i;
		dma_unmap_single(priv->device, desc_get_buf_addr(p),
				 desc_get_buf_len(p), DMA_TO_DEVICE);

		for (f = 0; f < skb_shinfo(priv->tx_skbuff[i])->nr_frags; f++) {
			p = priv->dma_tx + i++;
			dma_unmap_page(priv->device, desc_get_buf_addr(p),
				       desc_get_buf_len(p), DMA_TO_DEVICE);
		}

		dev_kfree_skb_any(priv->tx_skbuff[i]);
		priv->tx_skbuff[i] = NULL;
	}
}

static void xgmac_free_dma_desc_rings(struct xgmac_priv *priv)
{
	 
	xgmac_free_rx_skbufs(priv);
	xgmac_free_tx_skbufs(priv);

	if (priv->dma_tx) {
		dma_free_coherent(priv->device,
				  DMA_TX_RING_SZ * sizeof(struct xgmac_dma_desc),
				  priv->dma_tx, priv->dma_tx_phy);
		priv->dma_tx = NULL;
	}
	if (priv->dma_rx) {
		dma_free_coherent(priv->device,
				  DMA_RX_RING_SZ * sizeof(struct xgmac_dma_desc),
				  priv->dma_rx, priv->dma_rx_phy);
		priv->dma_rx = NULL;
	}
	kfree(priv->rx_skbuff);
	priv->rx_skbuff = NULL;
	kfree(priv->tx_skbuff);
	priv->tx_skbuff = NULL;
}

static void xgmac_tx_complete(struct xgmac_priv *priv)
{
	int i;

	while (dma_ring_cnt(priv->tx_head, priv->tx_tail, DMA_TX_RING_SZ)) {
		unsigned int entry = priv->tx_tail;
		struct sk_buff *skb = priv->tx_skbuff[entry];
		struct xgmac_dma_desc *p = priv->dma_tx + entry;

		if (desc_get_owner(p))
			break;

		if (desc_get_tx_ls(p))
			desc_get_tx_status(priv, p);

		netdev_dbg(priv->dev, "tx ring: curr %d, dirty %d\n",
			priv->tx_head, priv->tx_tail);

		dma_unmap_single(priv->device, desc_get_buf_addr(p),
				 desc_get_buf_len(p), DMA_TO_DEVICE);

		priv->tx_skbuff[entry] = NULL;
		priv->tx_tail = dma_ring_incr(entry, DMA_TX_RING_SZ);

		if (!skb) {
			continue;
		}

		for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
			entry = priv->tx_tail = dma_ring_incr(priv->tx_tail,
							      DMA_TX_RING_SZ);
			p = priv->dma_tx + priv->tx_tail;

			dma_unmap_page(priv->device, desc_get_buf_addr(p),
				       desc_get_buf_len(p), DMA_TO_DEVICE);
		}

		dev_kfree_skb(skb);
	}

	if (dma_ring_space(priv->tx_head, priv->tx_tail, DMA_TX_RING_SZ) >
	    MAX_SKB_FRAGS)
		netif_wake_queue(priv->dev);
}

static void xgmac_tx_err(struct xgmac_priv *priv)
{
	u32 reg, value, inten;

	netif_stop_queue(priv->dev);

	inten = readl(priv->base + XGMAC_DMA_INTR_ENA);
	writel(0, priv->base + XGMAC_DMA_INTR_ENA);

	reg = readl(priv->base + XGMAC_DMA_CONTROL);
	writel(reg & ~DMA_CONTROL_ST, priv->base + XGMAC_DMA_CONTROL);
	do {
		value = readl(priv->base + XGMAC_DMA_STATUS) & 0x700000;
	} while (value && (value != 0x600000));

	xgmac_free_tx_skbufs(priv);
	desc_init_tx_desc(priv->dma_tx, DMA_TX_RING_SZ);
	priv->tx_tail = 0;
	priv->tx_head = 0;
	writel(priv->dma_tx_phy, priv->base + XGMAC_DMA_TX_BASE_ADDR);
	writel(reg | DMA_CONTROL_ST, priv->base + XGMAC_DMA_CONTROL);

	writel(DMA_STATUS_TU | DMA_STATUS_TPS | DMA_STATUS_NIS | DMA_STATUS_AIS,
		priv->base + XGMAC_DMA_STATUS);
	writel(inten, priv->base + XGMAC_DMA_INTR_ENA);

	netif_wake_queue(priv->dev);
}

static int xgmac_hw_init(struct net_device *dev)
{
	u32 value, ctrl;
	int limit;
	struct xgmac_priv *priv = netdev_priv(dev);
	void __iomem *ioaddr = priv->base;

	ctrl = readl(ioaddr + XGMAC_CONTROL) & XGMAC_CONTROL_SPD_MASK;

	value = DMA_BUS_MODE_SFT_RESET;
	writel(value, ioaddr + XGMAC_DMA_BUS_MODE);
	limit = 15000;
	while (limit-- &&
		(readl(ioaddr + XGMAC_DMA_BUS_MODE) & DMA_BUS_MODE_SFT_RESET))
		cpu_relax();
	if (limit < 0)
		return -EBUSY;

	value = (0x10 << DMA_BUS_MODE_PBL_SHIFT) |
		(0x10 << DMA_BUS_MODE_RPBL_SHIFT) |
		DMA_BUS_MODE_FB | DMA_BUS_MODE_ATDS | DMA_BUS_MODE_AAL;
	writel(value, ioaddr + XGMAC_DMA_BUS_MODE);

	writel(DMA_INTR_DEFAULT_MASK, ioaddr + XGMAC_DMA_STATUS);
	writel(DMA_INTR_DEFAULT_MASK, ioaddr + XGMAC_DMA_INTR_ENA);

	writel(XGMAC_INT_STAT_PMTIM, ioaddr + XGMAC_INT_STAT);

	writel(0x0077000E, ioaddr + XGMAC_DMA_AXI_BUS);

	ctrl |= XGMAC_CONTROL_DDIC | XGMAC_CONTROL_JE | XGMAC_CONTROL_ACS |
		XGMAC_CONTROL_CAR;
	if (dev->features & NETIF_F_RXCSUM)
		ctrl |= XGMAC_CONTROL_IPC;
	writel(ctrl, ioaddr + XGMAC_CONTROL);

	writel(DMA_CONTROL_OSF, ioaddr + XGMAC_DMA_CONTROL);

	writel(XGMAC_OMR_TSF | XGMAC_OMR_RFD | XGMAC_OMR_RFA |
		XGMAC_OMR_RTC_256,
		ioaddr + XGMAC_OMR);

	writel(1, ioaddr + XGMAC_MMC_CTRL);
	return 0;
}

static int xgmac_open(struct net_device *dev)
{
	int ret;
	struct xgmac_priv *priv = netdev_priv(dev);
	void __iomem *ioaddr = priv->base;

	if (!is_valid_ether_addr(dev->dev_addr)) {
		eth_hw_addr_random(dev);
		netdev_dbg(priv->dev, "generated random MAC address %pM\n",
			dev->dev_addr);
	}

	memset(&priv->xstats, 0, sizeof(struct xgmac_extra_stats));

	xgmac_hw_init(dev);
	xgmac_set_mac_addr(ioaddr, dev->dev_addr, 0);
	xgmac_set_flow_ctrl(priv, priv->rx_pause, priv->tx_pause);

	ret = xgmac_dma_desc_rings_init(dev);
	if (ret < 0)
		return ret;

	xgmac_mac_enable(ioaddr);

	napi_enable(&priv->napi);
	netif_start_queue(dev);

	return 0;
}

static int xgmac_stop(struct net_device *dev)
{
	struct xgmac_priv *priv = netdev_priv(dev);

	netif_stop_queue(dev);

	if (readl(priv->base + XGMAC_DMA_INTR_ENA))
		napi_disable(&priv->napi);

	writel(0, priv->base + XGMAC_DMA_INTR_ENA);

	xgmac_mac_disable(priv->base);

	xgmac_free_dma_desc_rings(priv);

	return 0;
}

static netdev_tx_t xgmac_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct xgmac_priv *priv = netdev_priv(dev);
	unsigned int entry;
	int i;
	u32 irq_flag;
	int nfrags = skb_shinfo(skb)->nr_frags;
	struct xgmac_dma_desc *desc, *first;
	unsigned int desc_flags;
	unsigned int len;
	dma_addr_t paddr;

	priv->tx_irq_cnt = (priv->tx_irq_cnt + 1) & (DMA_TX_RING_SZ/4 - 1);
	irq_flag = priv->tx_irq_cnt ? 0 : TXDESC_INTERRUPT;

	desc_flags = (skb->ip_summed == CHECKSUM_PARTIAL) ?
		TXDESC_CSUM_ALL : 0;
	entry = priv->tx_head;
	desc = priv->dma_tx + entry;
	first = desc;

	len = skb_headlen(skb);
	paddr = dma_map_single(priv->device, skb->data, len, DMA_TO_DEVICE);
	if (dma_mapping_error(priv->device, paddr)) {
		dev_kfree_skb(skb);
		return -EIO;
	}
	priv->tx_skbuff[entry] = skb;
	desc_set_buf_addr_and_size(desc, paddr, len);

	for (i = 0; i < nfrags; i++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i];

		len = frag->size;

		paddr = skb_frag_dma_map(priv->device, frag, 0, len,
					 DMA_TO_DEVICE);
		if (dma_mapping_error(priv->device, paddr)) {
			dev_kfree_skb(skb);
			return -EIO;
		}

		entry = dma_ring_incr(entry, DMA_TX_RING_SZ);
		desc = priv->dma_tx + entry;
		priv->tx_skbuff[entry] = NULL;

		desc_set_buf_addr_and_size(desc, paddr, len);
		if (i < (nfrags - 1))
			desc_set_tx_owner(desc, desc_flags);
	}

	if (desc != first)
		desc_set_tx_owner(desc, desc_flags |
			TXDESC_LAST_SEG | irq_flag);
	else
		desc_flags |= TXDESC_LAST_SEG | irq_flag;

	wmb();
	desc_set_tx_owner(first, desc_flags | TXDESC_FIRST_SEG);

	priv->tx_head = dma_ring_incr(entry, DMA_TX_RING_SZ);

	writel(1, priv->base + XGMAC_DMA_TX_POLL);
	if (dma_ring_space(priv->tx_head, priv->tx_tail, DMA_TX_RING_SZ) <
	    MAX_SKB_FRAGS)
		netif_stop_queue(dev);

	return NETDEV_TX_OK;
}

static int xgmac_rx(struct xgmac_priv *priv, int limit)
{
	unsigned int entry;
	unsigned int count = 0;
	struct xgmac_dma_desc *p;

	while (count < limit) {
		int ip_checksum;
		struct sk_buff *skb;
		int frame_len;

		if (!dma_ring_cnt(priv->rx_head, priv->rx_tail, DMA_RX_RING_SZ))
			break;

		entry = priv->rx_tail;
		p = priv->dma_rx + entry;
		if (desc_get_owner(p))
			break;

		count++;
		priv->rx_tail = dma_ring_incr(priv->rx_tail, DMA_RX_RING_SZ);

		ip_checksum = desc_get_rx_status(priv, p);
		if (ip_checksum < 0)
			continue;

		skb = priv->rx_skbuff[entry];
		if (unlikely(!skb)) {
			netdev_err(priv->dev, "Inconsistent Rx descriptor chain\n");
			break;
		}
		priv->rx_skbuff[entry] = NULL;

		frame_len = desc_get_rx_frame_len(p);
		netdev_dbg(priv->dev, "RX frame size %d, COE status: %d\n",
			frame_len, ip_checksum);

		skb_put(skb, frame_len);
		dma_unmap_single(priv->device, desc_get_buf_addr(p),
				 frame_len, DMA_FROM_DEVICE);

		skb->protocol = eth_type_trans(skb, priv->dev);
		skb->ip_summed = ip_checksum;
		if (ip_checksum == CHECKSUM_NONE)
			netif_receive_skb(skb);
		else
			napi_gro_receive(&priv->napi, skb);
	}

	xgmac_rx_refill(priv);

	return count;
}

static int xgmac_poll(struct napi_struct *napi, int budget)
{
	struct xgmac_priv *priv = container_of(napi,
				       struct xgmac_priv, napi);
	int work_done = 0;

	xgmac_tx_complete(priv);
	work_done = xgmac_rx(priv, budget);

	if (work_done < budget) {
		napi_complete(napi);
		__raw_writel(DMA_INTR_DEFAULT_MASK, priv->base + XGMAC_DMA_INTR_ENA);
	}
	return work_done;
}

static void xgmac_tx_timeout(struct net_device *dev)
{
	struct xgmac_priv *priv = netdev_priv(dev);

	xgmac_tx_err(priv);
}

static void xgmac_set_rx_mode(struct net_device *dev)
{
	int i;
	struct xgmac_priv *priv = netdev_priv(dev);
	void __iomem *ioaddr = priv->base;
	unsigned int value = 0;
	u32 hash_filter[XGMAC_NUM_HASH];
	int reg = 1;
	struct netdev_hw_addr *ha;
	bool use_hash = false;

	netdev_dbg(priv->dev, "# mcasts %d, # unicast %d\n",
		 netdev_mc_count(dev), netdev_uc_count(dev));

	if (dev->flags & IFF_PROMISC) {
		writel(XGMAC_FRAME_FILTER_PR, ioaddr + XGMAC_FRAME_FILTER);
		return;
	}

	memset(hash_filter, 0, sizeof(hash_filter));

	if (netdev_uc_count(dev) > XGMAC_MAX_FILTER_ADDR) {
		use_hash = true;
		value |= XGMAC_FRAME_FILTER_HUC | XGMAC_FRAME_FILTER_HPF;
	}
	netdev_for_each_uc_addr(ha, dev) {
		if (use_hash) {
			u32 bit_nr = ~ether_crc(ETH_ALEN, ha->addr) >> 23;

			hash_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
		} else {
			xgmac_set_mac_addr(ioaddr, ha->addr, reg);
			reg++;
		}
	}

	if (dev->flags & IFF_ALLMULTI) {
		value |= XGMAC_FRAME_FILTER_PM;
		goto out;
	}

	if ((netdev_mc_count(dev) + reg - 1) > XGMAC_MAX_FILTER_ADDR) {
		use_hash = true;
		value |= XGMAC_FRAME_FILTER_HMC | XGMAC_FRAME_FILTER_HPF;
	}
	netdev_for_each_mc_addr(ha, dev) {
		if (use_hash) {
			u32 bit_nr = ~ether_crc(ETH_ALEN, ha->addr) >> 23;

			hash_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
		} else {
			xgmac_set_mac_addr(ioaddr, ha->addr, reg);
			reg++;
		}
	}

out:
	for (i = 0; i < XGMAC_NUM_HASH; i++)
		writel(hash_filter[i], ioaddr + XGMAC_HASH(i));

	writel(value, ioaddr + XGMAC_FRAME_FILTER);
}

static int xgmac_change_mtu(struct net_device *dev, int new_mtu)
{
	struct xgmac_priv *priv = netdev_priv(dev);
	int old_mtu;

	if ((new_mtu < 46) || (new_mtu > MAX_MTU)) {
		netdev_err(priv->dev, "invalid MTU, max MTU is: %d\n", MAX_MTU);
		return -EINVAL;
	}

	old_mtu = dev->mtu;
	dev->mtu = new_mtu;

	if (old_mtu <= ETH_DATA_LEN && new_mtu <= ETH_DATA_LEN)
		return 0;
	if (old_mtu == new_mtu)
		return 0;

	if (!netif_running(dev))
		return 0;

	xgmac_stop(dev);
	return xgmac_open(dev);
}

static irqreturn_t xgmac_pmt_interrupt(int irq, void *dev_id)
{
	u32 intr_status;
	struct net_device *dev = (struct net_device *)dev_id;
	struct xgmac_priv *priv = netdev_priv(dev);
	void __iomem *ioaddr = priv->base;

	intr_status = __raw_readl(ioaddr + XGMAC_INT_STAT);
	if (intr_status & XGMAC_INT_STAT_PMT) {
		netdev_dbg(priv->dev, "received Magic frame\n");
		 
		readl(ioaddr + XGMAC_PMT);
	}
	return IRQ_HANDLED;
}

static irqreturn_t xgmac_interrupt(int irq, void *dev_id)
{
	u32 intr_status;
	bool tx_err = false;
	struct net_device *dev = (struct net_device *)dev_id;
	struct xgmac_priv *priv = netdev_priv(dev);
	struct xgmac_extra_stats *x = &priv->xstats;

	intr_status = __raw_readl(priv->base + XGMAC_DMA_STATUS);
	intr_status &= __raw_readl(priv->base + XGMAC_DMA_INTR_ENA);
	__raw_writel(intr_status, priv->base + XGMAC_DMA_STATUS);

	if (unlikely(intr_status & DMA_STATUS_AIS)) {
		if (intr_status & DMA_STATUS_TJT) {
			netdev_err(priv->dev, "transmit jabber\n");
			x->tx_jabber++;
		}
		if (intr_status & DMA_STATUS_RU)
			x->rx_buf_unav++;
		if (intr_status & DMA_STATUS_RPS) {
			netdev_err(priv->dev, "receive process stopped\n");
			x->rx_process_stopped++;
		}
		if (intr_status & DMA_STATUS_ETI) {
			netdev_err(priv->dev, "transmit early interrupt\n");
			x->tx_early++;
		}
		if (intr_status & DMA_STATUS_TPS) {
			netdev_err(priv->dev, "transmit process stopped\n");
			x->tx_process_stopped++;
			tx_err = true;
		}
		if (intr_status & DMA_STATUS_FBI) {
			netdev_err(priv->dev, "fatal bus error\n");
			x->fatal_bus_error++;
			tx_err = true;
		}

		if (tx_err)
			xgmac_tx_err(priv);
	}

	if (intr_status & (DMA_STATUS_RI | DMA_STATUS_TU | DMA_STATUS_TI)) {
		__raw_writel(DMA_INTR_ABNORMAL, priv->base + XGMAC_DMA_INTR_ENA);
		napi_schedule(&priv->napi);
	}

	return IRQ_HANDLED;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
 
static void xgmac_poll_controller(struct net_device *dev)
{
	disable_irq(dev->irq);
	xgmac_interrupt(dev->irq, dev);
	enable_irq(dev->irq);
}
#endif

static struct rtnl_link_stats64 *
xgmac_get_stats64(struct net_device *dev,
		       struct rtnl_link_stats64 *storage)
{
	struct xgmac_priv *priv = netdev_priv(dev);
	void __iomem *base = priv->base;
	u32 count;

	spin_lock_bh(&priv->stats_lock);
	writel(XGMAC_MMC_CTRL_CNT_FRZ, base + XGMAC_MMC_CTRL);

	storage->rx_bytes = readl(base + XGMAC_MMC_RXOCTET_G_LO);
	storage->rx_bytes |= (u64)(readl(base + XGMAC_MMC_RXOCTET_G_HI)) << 32;

	storage->rx_packets = readl(base + XGMAC_MMC_RXFRAME_GB_LO);
	storage->multicast = readl(base + XGMAC_MMC_RXMCFRAME_G);
	storage->rx_crc_errors = readl(base + XGMAC_MMC_RXCRCERR);
	storage->rx_length_errors = readl(base + XGMAC_MMC_RXLENGTHERR);
	storage->rx_missed_errors = readl(base + XGMAC_MMC_RXOVERFLOW);

	storage->tx_bytes = readl(base + XGMAC_MMC_TXOCTET_G_LO);
	storage->tx_bytes |= (u64)(readl(base + XGMAC_MMC_TXOCTET_G_HI)) << 32;

	count = readl(base + XGMAC_MMC_TXFRAME_GB_LO);
	storage->tx_errors = count - readl(base + XGMAC_MMC_TXFRAME_G_LO);
	storage->tx_packets = count;
	storage->tx_fifo_errors = readl(base + XGMAC_MMC_TXUNDERFLOW);

	writel(0, base + XGMAC_MMC_CTRL);
	spin_unlock_bh(&priv->stats_lock);
	return storage;
}

static int xgmac_set_mac_address(struct net_device *dev, void *p)
{
	struct xgmac_priv *priv = netdev_priv(dev);
	void __iomem *ioaddr = priv->base;
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

	xgmac_set_mac_addr(ioaddr, dev->dev_addr, 0);

	return 0;
}

static int xgmac_set_features(struct net_device *dev, netdev_features_t features)
{
	u32 ctrl;
	struct xgmac_priv *priv = netdev_priv(dev);
	void __iomem *ioaddr = priv->base;
	netdev_features_t changed = dev->features ^ features;

	if (!(changed & NETIF_F_RXCSUM))
		return 0;

	ctrl = readl(ioaddr + XGMAC_CONTROL);
	if (features & NETIF_F_RXCSUM)
		ctrl |= XGMAC_CONTROL_IPC;
	else
		ctrl &= ~XGMAC_CONTROL_IPC;
	writel(ctrl, ioaddr + XGMAC_CONTROL);

	return 0;
}

static const struct net_device_ops xgmac_netdev_ops = {
	.ndo_open = xgmac_open,
	.ndo_start_xmit = xgmac_xmit,
	.ndo_stop = xgmac_stop,
	.ndo_change_mtu = xgmac_change_mtu,
	.ndo_set_rx_mode = xgmac_set_rx_mode,
	.ndo_tx_timeout = xgmac_tx_timeout,
	.ndo_get_stats64 = xgmac_get_stats64,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller = xgmac_poll_controller,
#endif
	.ndo_set_mac_address = xgmac_set_mac_address,
	.ndo_set_features = xgmac_set_features,
};

static int xgmac_ethtool_getsettings(struct net_device *dev,
					  struct ethtool_cmd *cmd)
{
	cmd->autoneg = 0;
	cmd->duplex = DUPLEX_FULL;
	ethtool_cmd_speed_set(cmd, 10000);
	cmd->supported = 0;
	cmd->advertising = 0;
	cmd->transceiver = XCVR_INTERNAL;
	return 0;
}

static void xgmac_get_pauseparam(struct net_device *netdev,
				      struct ethtool_pauseparam *pause)
{
	struct xgmac_priv *priv = netdev_priv(netdev);

	pause->rx_pause = priv->rx_pause;
	pause->tx_pause = priv->tx_pause;
}

static int xgmac_set_pauseparam(struct net_device *netdev,
				     struct ethtool_pauseparam *pause)
{
	struct xgmac_priv *priv = netdev_priv(netdev);

	if (pause->autoneg)
		return -EINVAL;

	return xgmac_set_flow_ctrl(priv, pause->rx_pause, pause->tx_pause);
}

struct xgmac_stats {
	char stat_string[ETH_GSTRING_LEN];
	int stat_offset;
	bool is_reg;
};

#define XGMAC_STAT(m)	\
	{ #m, offsetof(struct xgmac_priv, xstats.m), false }
#define XGMAC_HW_STAT(m, reg_offset)	\
	{ #m, reg_offset, true }

static const struct xgmac_stats xgmac_gstrings_stats[] = {
	XGMAC_STAT(tx_frame_flushed),
	XGMAC_STAT(tx_payload_error),
	XGMAC_STAT(tx_ip_header_error),
	XGMAC_STAT(tx_local_fault),
	XGMAC_STAT(tx_remote_fault),
	XGMAC_STAT(tx_early),
	XGMAC_STAT(tx_process_stopped),
	XGMAC_STAT(tx_jabber),
	XGMAC_STAT(rx_buf_unav),
	XGMAC_STAT(rx_process_stopped),
	XGMAC_STAT(rx_payload_error),
	XGMAC_STAT(rx_ip_header_error),
	XGMAC_STAT(rx_da_filter_fail),
	XGMAC_STAT(rx_sa_filter_fail),
	XGMAC_STAT(fatal_bus_error),
	XGMAC_HW_STAT(rx_watchdog, XGMAC_MMC_RXWATCHDOG),
	XGMAC_HW_STAT(tx_vlan, XGMAC_MMC_TXVLANFRAME),
	XGMAC_HW_STAT(rx_vlan, XGMAC_MMC_RXVLANFRAME),
	XGMAC_HW_STAT(tx_pause, XGMAC_MMC_TXPAUSEFRAME),
	XGMAC_HW_STAT(rx_pause, XGMAC_MMC_RXPAUSEFRAME),
};
#define XGMAC_STATS_LEN ARRAY_SIZE(xgmac_gstrings_stats)

static void xgmac_get_ethtool_stats(struct net_device *dev,
					 struct ethtool_stats *dummy,
					 u64 *data)
{
	struct xgmac_priv *priv = netdev_priv(dev);
	void *p = priv;
	int i;

	for (i = 0; i < XGMAC_STATS_LEN; i++) {
		if (xgmac_gstrings_stats[i].is_reg)
			*data++ = readl(priv->base +
				xgmac_gstrings_stats[i].stat_offset);
		else
			*data++ = *(u32 *)(p +
				xgmac_gstrings_stats[i].stat_offset);
	}
}

static int xgmac_get_sset_count(struct net_device *netdev, int sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return XGMAC_STATS_LEN;
	default:
		return -EINVAL;
	}
}

static void xgmac_get_strings(struct net_device *dev, u32 stringset,
				   u8 *data)
{
	int i;
	u8 *p = data;

	switch (stringset) {
	case ETH_SS_STATS:
		for (i = 0; i < XGMAC_STATS_LEN; i++) {
			memcpy(p, xgmac_gstrings_stats[i].stat_string,
			       ETH_GSTRING_LEN);
			p += ETH_GSTRING_LEN;
		}
		break;
	default:
		WARN_ON(1);
		break;
	}
}

static void xgmac_get_wol(struct net_device *dev,
			       struct ethtool_wolinfo *wol)
{
	struct xgmac_priv *priv = netdev_priv(dev);

	if (device_can_wakeup(priv->device)) {
		wol->supported = WAKE_MAGIC | WAKE_UCAST;
		wol->wolopts = priv->wolopts;
	}
}

static int xgmac_set_wol(struct net_device *dev,
			      struct ethtool_wolinfo *wol)
{
	struct xgmac_priv *priv = netdev_priv(dev);
	u32 support = WAKE_MAGIC | WAKE_UCAST;

	if (!device_can_wakeup(priv->device))
		return -ENOTSUPP;

	if (wol->wolopts & ~support)
		return -EINVAL;

	priv->wolopts = wol->wolopts;

	if (wol->wolopts) {
		device_set_wakeup_enable(priv->device, 1);
		enable_irq_wake(dev->irq);
	} else {
		device_set_wakeup_enable(priv->device, 0);
		disable_irq_wake(dev->irq);
	}

	return 0;
}

static const struct ethtool_ops xgmac_ethtool_ops = {
	.get_settings = xgmac_ethtool_getsettings,
	.get_link = ethtool_op_get_link,
	.get_pauseparam = xgmac_get_pauseparam,
	.set_pauseparam = xgmac_set_pauseparam,
	.get_ethtool_stats = xgmac_get_ethtool_stats,
	.get_strings = xgmac_get_strings,
	.get_wol = xgmac_get_wol,
	.set_wol = xgmac_set_wol,
	.get_sset_count = xgmac_get_sset_count,
};

static int xgmac_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct net_device *ndev = NULL;
	struct xgmac_priv *priv = NULL;
	u32 uid;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	if (!request_mem_region(res->start, resource_size(res), pdev->name))
		return -EBUSY;

	ndev = alloc_etherdev(sizeof(struct xgmac_priv));
	if (!ndev) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	SET_NETDEV_DEV(ndev, &pdev->dev);
	priv = netdev_priv(ndev);
	platform_set_drvdata(pdev, ndev);
	ether_setup(ndev);
	ndev->netdev_ops = &xgmac_netdev_ops;
	SET_ETHTOOL_OPS(ndev, &xgmac_ethtool_ops);
	spin_lock_init(&priv->stats_lock);

	priv->device = &pdev->dev;
	priv->dev = ndev;
	priv->rx_pause = 1;
	priv->tx_pause = 1;

	priv->base = ioremap(res->start, resource_size(res));
	if (!priv->base) {
		netdev_err(ndev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err_io;
	}

	uid = readl(priv->base + XGMAC_VERSION);
	netdev_info(ndev, "h/w version is 0x%x\n", uid);

	writel(0, priv->base + XGMAC_DMA_INTR_ENA);
	ndev->irq = platform_get_irq(pdev, 0);
	if (ndev->irq == -ENXIO) {
		netdev_err(ndev, "No irq resource\n");
		ret = ndev->irq;
		goto err_irq;
	}

	ret = request_irq(ndev->irq, xgmac_interrupt, 0,
			  dev_name(&pdev->dev), ndev);
	if (ret < 0) {
		netdev_err(ndev, "Could not request irq %d - ret %d)\n",
			ndev->irq, ret);
		goto err_irq;
	}

	priv->pmt_irq = platform_get_irq(pdev, 1);
	if (priv->pmt_irq == -ENXIO) {
		netdev_err(ndev, "No pmt irq resource\n");
		ret = priv->pmt_irq;
		goto err_pmt_irq;
	}

	ret = request_irq(priv->pmt_irq, xgmac_pmt_interrupt, 0,
			  dev_name(&pdev->dev), ndev);
	if (ret < 0) {
		netdev_err(ndev, "Could not request irq %d - ret %d)\n",
			priv->pmt_irq, ret);
		goto err_pmt_irq;
	}

	device_set_wakeup_capable(&pdev->dev, 1);
	if (device_can_wakeup(priv->device))
		priv->wolopts = WAKE_MAGIC;	 

	ndev->hw_features = NETIF_F_SG | NETIF_F_FRAGLIST | NETIF_F_HIGHDMA;
	if (readl(priv->base + XGMAC_DMA_HW_FEATURE) & DMA_HW_FEAT_TXCOESEL)
		ndev->hw_features |= NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM |
				     NETIF_F_RXCSUM;
	ndev->features |= ndev->hw_features;
	ndev->priv_flags |= IFF_UNICAST_FLT;

	xgmac_get_mac_addr(priv->base, ndev->dev_addr, 0);
	if (!is_valid_ether_addr(ndev->dev_addr))
		netdev_warn(ndev, "MAC address %pM not valid",
			 ndev->dev_addr);

	netif_napi_add(ndev, &priv->napi, xgmac_poll, 64);
	ret = register_netdev(ndev);
	if (ret)
		goto err_reg;

	return 0;

err_reg:
	netif_napi_del(&priv->napi);
	free_irq(priv->pmt_irq, ndev);
err_pmt_irq:
	free_irq(ndev->irq, ndev);
err_irq:
	iounmap(priv->base);
err_io:
	free_netdev(ndev);
err_alloc:
	release_mem_region(res->start, resource_size(res));
#if defined (MY_DEF_HERE)
#else  
	platform_set_drvdata(pdev, NULL);
#endif  
	return ret;
}

static int xgmac_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct xgmac_priv *priv = netdev_priv(ndev);
	struct resource *res;

	xgmac_mac_disable(priv->base);

	free_irq(ndev->irq, ndev);
	free_irq(priv->pmt_irq, ndev);

#if defined (MY_DEF_HERE)
#else  
	platform_set_drvdata(pdev, NULL);
#endif  
	unregister_netdev(ndev);
	netif_napi_del(&priv->napi);

	iounmap(priv->base);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, resource_size(res));

	free_netdev(ndev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static void xgmac_pmt(void __iomem *ioaddr, unsigned long mode)
{
	unsigned int pmt = 0;

	if (mode & WAKE_MAGIC)
		pmt |= XGMAC_PMT_POWERDOWN | XGMAC_PMT_MAGIC_PKT_EN;
	if (mode & WAKE_UCAST)
		pmt |= XGMAC_PMT_POWERDOWN | XGMAC_PMT_GLBL_UNICAST;

	writel(pmt, ioaddr + XGMAC_PMT);
}

static int xgmac_suspend(struct device *dev)
{
	struct net_device *ndev = platform_get_drvdata(to_platform_device(dev));
	struct xgmac_priv *priv = netdev_priv(ndev);
	u32 value;

	if (!ndev || !netif_running(ndev))
		return 0;

	netif_device_detach(ndev);
	napi_disable(&priv->napi);
	writel(0, priv->base + XGMAC_DMA_INTR_ENA);

	if (device_may_wakeup(priv->device)) {
		 
		value = readl(priv->base + XGMAC_DMA_CONTROL);
		value &= ~(DMA_CONTROL_ST | DMA_CONTROL_SR);
		writel(value, priv->base + XGMAC_DMA_CONTROL);

		xgmac_pmt(priv->base, priv->wolopts);
	} else
		xgmac_mac_disable(priv->base);

	return 0;
}

static int xgmac_resume(struct device *dev)
{
	struct net_device *ndev = platform_get_drvdata(to_platform_device(dev));
	struct xgmac_priv *priv = netdev_priv(ndev);
	void __iomem *ioaddr = priv->base;

	if (!netif_running(ndev))
		return 0;

	xgmac_pmt(ioaddr, 0);

	xgmac_mac_enable(ioaddr);
	writel(DMA_INTR_DEFAULT_MASK, ioaddr + XGMAC_DMA_STATUS);
	writel(DMA_INTR_DEFAULT_MASK, ioaddr + XGMAC_DMA_INTR_ENA);

	netif_device_attach(ndev);
	napi_enable(&priv->napi);

	return 0;
}
#endif  

static SIMPLE_DEV_PM_OPS(xgmac_pm_ops, xgmac_suspend, xgmac_resume);

static const struct of_device_id xgmac_of_match[] = {
	{ .compatible = "calxeda,hb-xgmac", },
	{},
};
MODULE_DEVICE_TABLE(of, xgmac_of_match);

static struct platform_driver xgmac_driver = {
	.driver = {
		.name = "calxedaxgmac",
		.of_match_table = xgmac_of_match,
	},
	.probe = xgmac_probe,
	.remove = xgmac_remove,
	.driver.pm = &xgmac_pm_ops,
};

module_platform_driver(xgmac_driver);

MODULE_AUTHOR("Calxeda, Inc.");
MODULE_DESCRIPTION("Calxeda 10G XGMAC driver");
MODULE_LICENSE("GPL v2");
