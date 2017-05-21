#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#ifndef MV_XOR_H
#define MV_XOR_H

#include <linux/types.h>
#include <linux/io.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>

#define USE_TIMER
#if defined(MY_ABC_HERE)
#define MV_XOR_SLOT_SIZE		64
 
#define MV_XOR_POOL_SIZE		(MV_XOR_SLOT_SIZE*3072)
#define MV_XOR_THRESHOLD		1
#define MV_XOR_MAX_CHANNELS             2

#define XOR_OPERATION_MODE_XOR		0
#define XOR_OPERATION_MODE_CRC32C	1
#define XOR_OPERATION_MODE_MEMCPY	2
#define XOR_OPERATION_MODE_IN_DESC	7
#define XOR_DESCRIPTOR_SWAP		BIT(14)

#define XOR_DESC_OPERATION_XOR            (0 << 24)
#define XOR_DESC_OPERATION_CRC32C         (1 << 24)
#define XOR_DESC_OPERATION_MEMCPY         (2 << 24)
#define XOR_DESC_OPERATION_PQ             (5 << 24)
#else  
#define MV_XOR_POOL_SIZE		PAGE_SIZE
#define MV_XOR_SLOT_SIZE		64
#define MV_XOR_THRESHOLD		1
#define MV_XOR_MAX_CHANNELS             2

#define XOR_OPERATION_MODE_XOR		0
#define XOR_OPERATION_MODE_MEMCPY	2
#define XOR_OPERATION_MODE_MEMSET	4
#endif  
#define XOR_DESC_SUCCESS		0x40000000

#define XOR_CURR_DESC(chan)	(chan->mmr_base + 0x210 + (chan->idx * 4))
#define XOR_NEXT_DESC(chan)	(chan->mmr_base + 0x200 + (chan->idx * 4))
#define XOR_BYTE_COUNT(chan)	(chan->mmr_base + 0x220 + (chan->idx * 4))
#define XOR_DEST_POINTER(chan)	(chan->mmr_base + 0x2B0 + (chan->idx * 4))
#define XOR_BLOCK_SIZE(chan)	(chan->mmr_base + 0x2C0 + (chan->idx * 4))
#define XOR_INIT_VALUE_LOW(chan)	(chan->mmr_base + 0x2E0)
#define XOR_INIT_VALUE_HIGH(chan)	(chan->mmr_base + 0x2E4)

#define XOR_CONFIG(chan)	(chan->mmr_base + 0x10 + (chan->idx * 4))
#define XOR_ACTIVATION(chan)	(chan->mmr_base + 0x20 + (chan->idx * 4))
#define XOR_INTR_CAUSE(chan)	(chan->mmr_base + 0x30)
#define XOR_INTR_MASK(chan)	(chan->mmr_base + 0x40)
#define XOR_ERROR_CAUSE(chan)	(chan->mmr_base + 0x50)
#define XOR_ERROR_ADDR(chan)	(chan->mmr_base + 0x60)
#if defined(MY_ABC_HERE)
#define XOR_INTR_MASK_VALUE	0x3F7
#else  
#define XOR_INTR_MASK_VALUE	0x3F5
#endif  

#define WINDOW_BASE(w)		(0x250 + ((w) << 2))
#define WINDOW_SIZE(w)		(0x270 + ((w) << 2))
#define WINDOW_REMAP_HIGH(w)	(0x290 + ((w) << 2))
#define WINDOW_BAR_ENABLE(chan)	(0x240 + ((chan) << 2))
#define WINDOW_OVERRIDE_CTRL(chan)	(0x2A0 + ((chan) << 2))

struct mv_xor_device {
	void __iomem	     *xor_base;
	void __iomem	     *xor_high_base;
	struct clk	     *clk;
	struct mv_xor_chan   *channels[MV_XOR_MAX_CHANNELS];
};

#if defined(MY_ABC_HERE)
 
struct mv_xor_suspend_regs {
	int config;
	int int_mask;
};
#endif  

#if defined(MY_ABC_HERE)
 
#else  
 
#endif  
struct mv_xor_chan {
	int			pending;
	spinlock_t		lock;  
	void __iomem		*mmr_base;
	unsigned int		idx;
	int                     irq;
	enum dma_transaction_type	current_type;
#if defined(MY_ABC_HERE)
	struct mv_xor_suspend_regs	suspend_regs;
#endif  
	struct list_head	chain;
#if defined(MY_ABC_HERE)
	struct list_head	free_slots;
	struct list_head	allocated_slots;
#endif  
	struct list_head	completed_slots;
	dma_addr_t		dma_desc_pool;
	void			*dma_desc_pool_virt;
	size_t                  pool_size;
	struct dma_device	dmadev;
	struct dma_chan		dmachan;
#if defined(MY_ABC_HERE)
	 
#else  
	struct mv_xor_desc_slot	*last_used;
	struct list_head	all_slots;
#endif  
	int			slots_allocated;
	struct tasklet_struct	irq_tasklet;
#if defined(MY_ABC_HERE)
	int			op_in_desc;
#endif  
#ifdef USE_TIMER
	unsigned long		cleanup_time;
	u32			current_on_last_cleanup;
#endif
};

#if defined(MY_ABC_HERE)
 
#else  
 
#endif  
 
struct mv_xor_desc_slot {
#if defined(MY_ABC_HERE)
	struct list_head	node;
	enum dma_transaction_type	type;
	void			*hw_desc;
	u16			idx;
	u16			unmap_src_cnt;
	u32			value;
	size_t			unmap_len;
#else  
	struct list_head	slot_node;
	struct list_head	chain_node;
	struct list_head	completed_node;
	enum dma_transaction_type	type;
	void			*hw_desc;
	struct mv_xor_desc_slot	*group_head;
	u16			slot_cnt;
	u16			slots_per_op;
	u16			idx;
	u16			unmap_src_cnt;
	u32			value;
	size_t			unmap_len;
	struct list_head	tx_list;
#endif  
	struct dma_async_tx_descriptor	async_tx;
	union {
		u32		*xor_check_result;
		u32		*crc32_result;
	};
#ifdef USE_TIMER
	unsigned long		arrival_time;
	struct timer_list	timeout;
#endif
};

#if defined(MY_ABC_HERE)
 
#if defined(__LITTLE_ENDIAN)
struct mv_xor_desc {
	u32 status;		 
	u32 crc32_result;	 
	u32 desc_command;	 
	u32 phy_next_desc;	 
	u32 byte_count;		 
	u32 phy_dest_addr;	 
	u32 phy_src_addr[8];	 
	u32 phy_q_dest_addr;
	u32 reserved1;
};
#define mv_phy_src_idx(src_idx) (src_idx)
#else
struct mv_xor_desc {
	u32 crc32_result;	 
	u32 status;		 
	u32 phy_next_desc;	 
	u32 desc_command;	 
	u32 phy_dest_addr;	 
	u32 byte_count;		 
	u32 phy_src_addr[8];	 
	u32 reserved1;
	u32 phy_q_dest_addr;
};
#define mv_phy_src_idx(src_idx) (src_idx ^ 1)
#endif
#else  
 
struct mv_xor_desc {
	u32 status;		 
	u32 crc32_result;	 
	u32 desc_command;	 
	u32 phy_next_desc;	 
	u32 byte_count;		 
	u32 phy_dest_addr;	 
	u32 phy_src_addr[8];	 
	u32 reserved0;
	u32 reserved1;
};
#endif  

#define to_mv_sw_desc(addr_hw_desc)		\
	container_of(addr_hw_desc, struct mv_xor_desc_slot, hw_desc)

#define mv_hw_desc_slot_idx(hw_desc, idx)	\
	((void *)(((unsigned long)hw_desc) + ((idx) << 5)))

#define MV_XOR_MIN_BYTE_COUNT	(128)
#define XOR_MAX_BYTE_COUNT	((16 * 1024 * 1024) - 1)
#define MV_XOR_MAX_BYTE_COUNT	XOR_MAX_BYTE_COUNT

#endif
