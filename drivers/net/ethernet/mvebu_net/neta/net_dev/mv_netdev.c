#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include "mvCommon.h"
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/platform_device.h>
#include <linux/skbuff.h>
#include <linux/inetdevice.h>
#include <linux/mv_neta.h>
#include <linux/mbus.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <linux/module.h>
#if defined(MY_ABC_HERE)
#include <linux/mii.h>
#endif  
#include "mvOs.h"
#include "mvDebug.h"
#include "mvEthPhy.h"

#include "gbe/mvNeta.h"
#include "bm/mvBm.h"
#include "pnc/mvPnc.h"
#include "pnc/mvTcam.h"
#include "pmt/mvPmt.h"
#include "mv_mux_netdev.h"

#include "mv_netdev.h"
#include "mv_eth_tool.h"
#include "mv_eth_sysfs.h"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/phy.h>
#endif  

#ifdef CONFIG_MV_NETA_TXDONE_IN_HRTIMER
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#endif

#ifdef CONFIG_ARCH_MVEBU
#include "mvebu-soc-id.h"
#include "mvNetConfig.h"
#else
#include "mvSysEthConfig.h"
#include "ctrlEnv/mvCtrlEnvLib.h"
#endif  

#if defined(CONFIG_NETMAP) || defined(CONFIG_NETMAP_MODULE)
#include <mv_neta_netmap.h>
#endif

#ifdef CONFIG_OF
#if defined(MY_ABC_HERE)
int port_vbase[MV_ETH_MAX_PORTS] = {0};
#else
int port_vbase[MV_ETH_MAX_PORTS];
#endif  
int bm_reg_vbase, pnc_reg_vbase;
static u32 pnc_phyaddr_base;
static u32 pnc_win_size;
static u32 bm_phyaddr_base;
static u32 bm_win_size;
#endif  

static struct mv_mux_eth_ops mux_eth_ops;

#ifdef CONFIG_MV_CPU_PERF_CNTRS
#include "cpu/mvCpuCntrs.h"
MV_CPU_CNTRS_EVENT	*event0 = NULL;
MV_CPU_CNTRS_EVENT	*event1 = NULL;
MV_CPU_CNTRS_EVENT	*event2 = NULL;
MV_CPU_CNTRS_EVENT	*event3 = NULL;
MV_CPU_CNTRS_EVENT	*event4 = NULL;
MV_CPU_CNTRS_EVENT	*event5 = NULL;
#endif  

static struct  platform_device *neta_sysfs;
unsigned int ext_switch_port_mask = 0;

void handle_group_affinity(int port);
void set_rxq_affinity(struct eth_port *pp, MV_U32 rxqAffinity, int group);
static inline int mv_eth_tx_policy(struct eth_port *pp, struct sk_buff *skb);

static int pm_flag;
static int wol_ports_bmp;

#ifdef CONFIG_MV_ETH_PNC
unsigned int mv_eth_pnc_ctrl_en = 1;

int mv_eth_ctrl_pnc(int en)
{
	mv_eth_pnc_ctrl_en = en;
	return 0;
}
#endif  

#ifdef CONFIG_MV_NETA_SKB_RECYCLE
int mv_ctrl_swf_recycle = CONFIG_MV_NETA_SKB_RECYCLE_DEF;
EXPORT_SYMBOL(mv_ctrl_swf_recycle);

int mv_eth_ctrl_swf_recycle(int en)
{
	mv_ctrl_swf_recycle = en;
	return 0;
}
#else

int mv_eth_ctrl_swf_recycle(int en)
{
	pr_info("SWF SKB recycle is not supported\n");
	return 1;
}

#endif  

extern u8 mvMacAddr[CONFIG_MV_ETH_PORTS_NUM][MV_MAC_ADDR_SIZE];
extern u16 mvMtu[CONFIG_MV_ETH_PORTS_NUM];
static const u8 *mac_src[CONFIG_MV_ETH_PORTS_NUM];

extern unsigned int switch_enabled_ports;

struct bm_pool mv_eth_pool[MV_ETH_BM_POOLS];
struct eth_port **mv_eth_ports;

static struct device *neta_global_dev;

int mv_ctrl_txdone = CONFIG_MV_ETH_TXDONE_COAL_PKTS;
EXPORT_SYMBOL(mv_ctrl_txdone);

static int mv_eth_ports_num = 0;

static int mv_eth_initialized = 0;

#ifdef CONFIG_MV_NETA_TXDONE_IN_HRTIMER
static unsigned int mv_eth_tx_done_hrtimer_period_us = CONFIG_MV_NETA_TX_DONE_HIGH_RES_TIMER_PERIOD;
#endif

static void mv_eth_txq_delete(struct eth_port *pp, struct tx_queue *txq_ctrl);
static void mv_eth_tx_timeout(struct net_device *dev);
static int  mv_eth_tx(struct sk_buff *skb, struct net_device *dev);
static void mv_eth_tx_frag_process(struct eth_port *pp, struct sk_buff *skb, struct tx_queue *txq_ctrl, u16 flags);
static int mv_eth_rxq_fill(struct eth_port *pp, int rxq, int num);

static void mv_eth_config_show(void);
static int  mv_eth_priv_init(struct eth_port *pp, int port);
static void mv_eth_priv_cleanup(struct eth_port *pp);
static int  mv_eth_hal_init(struct eth_port *pp);
struct net_device *mv_eth_netdev_init(struct platform_device *pdev);
static void mv_eth_netdev_init_features(struct net_device *dev);

static MV_STATUS mv_eth_pool_create(int pool, int capacity);
static int mv_eth_pool_add(struct eth_port *pp, int pool, int buf_num);
static int mv_eth_pool_free(int pool, int num);
static int mv_eth_pool_destroy(int pool);

#if defined(MY_ABC_HERE)
extern MV_U32 syno_wol_support(struct eth_port *pp);
extern int mv_eth_tool_read_phy_reg(int phy_addr, u16 page, u16 reg, u16 *val);
DEFINE_SPINLOCK(mii_lock);
#endif  

#ifdef CONFIG_MV_ETH_TSO
int mv_eth_tx_tso(struct sk_buff *skb, struct net_device *dev, struct mv_eth_tx_spec *tx_spec,
		struct tx_queue *txq_ctrl);
#endif

static char *port0_config_str = NULL, *port1_config_str = NULL, *port2_config_str = NULL, *port3_config_str = NULL;
int mv_eth_cmdline_port0_config(char *s);
__setup("mv_port0_config=", mv_eth_cmdline_port0_config);
int mv_eth_cmdline_port1_config(char *s);
__setup("mv_port1_config=", mv_eth_cmdline_port1_config);
int mv_eth_cmdline_port2_config(char *s);
__setup("mv_port2_config=", mv_eth_cmdline_port2_config);
int mv_eth_cmdline_port3_config(char *s);
__setup("mv_port3_config=", mv_eth_cmdline_port3_config);

#ifdef CONFIG_MV_NETA_TXDONE_IN_HRTIMER
unsigned int mv_eth_tx_done_hrtimer_period_get(void)
{
	return mv_eth_tx_done_hrtimer_period_us;
}

int mv_eth_tx_done_hrtimer_period_set(unsigned int period)
{
	if ((period < MV_ETH_HRTIMER_PERIOD_MIN) || (period > MV_ETH_HRTIMER_PERIOD_MAX)) {
		pr_info("period should be in [%u, %u]\n", MV_ETH_HRTIMER_PERIOD_MIN, MV_ETH_HRTIMER_PERIOD_MAX);
		return -EINVAL;
	}

	mv_eth_tx_done_hrtimer_period_us = period;
	return 0;
}
#endif

int mv_eth_cmdline_port0_config(char *s)
{
	port0_config_str = s;
	return 1;
}

int mv_eth_cmdline_port1_config(char *s)
{
	port1_config_str = s;
	return 1;
}

int mv_eth_cmdline_port2_config(char *s)
{
	port2_config_str = s;
	return 1;
}

int mv_eth_cmdline_port3_config(char *s)
{
	port3_config_str = s;
	return 1;
}
void mv_eth_stack_print(int port, MV_BOOL isPrintElements)
{
	struct eth_port *pp;

	if (mvNetaPortCheck(port))
		return;

	pp = mv_eth_port_by_id(port);
	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return;
	}

	if (pp->pool_long == NULL) {
		pr_err("%s: Error - long pool is null\n", __func__);
		return;
	}

	pr_info("Long pool (%d) stack\n", pp->pool_long->pool);
	mvStackStatus(pp->pool_long->stack, isPrintElements);

#ifdef CONFIG_MV_ETH_BM_CPU
	if (MV_NETA_BM_CAP()) {
		if (pp->pool_short == NULL) {
			pr_err("%s: Error - short pool is null\n", __func__);
			return;
		}
		pr_info("Short pool (%d) stack\n", pp->pool_short->pool);
		mvStackStatus(pp->pool_short->stack, isPrintElements);
	}
#endif  
}

static void mv_eth_adaptive_rx_update(struct eth_port *pp)
{
	unsigned long period = jiffies - pp->rx_timestamp;

	if (period >= (pp->rate_sample_cfg * HZ)) {
		int i;
		unsigned long rate = pp->rx_rate_pkts * HZ / period;

		if (rate < pp->pkt_rate_low_cfg) {
			if (pp->rate_current != 1) {
				pp->rate_current = 1;
				for (i = 0; i < CONFIG_MV_ETH_RXQ; i++) {
					mv_eth_rx_time_coal_set(pp->port, i, pp->rx_time_low_coal_cfg);
					mv_eth_rx_pkts_coal_set(pp->port, i, pp->rx_pkts_low_coal_cfg);
				}
			}
		} else if (rate > pp->pkt_rate_high_cfg) {
			if (pp->rate_current != 3) {
				pp->rate_current = 3;
				for (i = 0; i < CONFIG_MV_ETH_RXQ; i++) {
					mv_eth_rx_time_coal_set(pp->port, i, pp->rx_time_high_coal_cfg);
					mv_eth_rx_pkts_coal_set(pp->port, i, pp->rx_pkts_high_coal_cfg);
				}
			}
		} else {
			if (pp->rate_current != 2) {
				pp->rate_current = 2;
				for (i = 0; i < CONFIG_MV_ETH_RXQ; i++) {
					mv_eth_rx_time_coal_set(pp->port, i, pp->rx_time_coal_cfg);
					mv_eth_rx_pkts_coal_set(pp->port, i, pp->rx_pkts_coal_cfg);
				}
			}
		}
		pp->rx_rate_pkts = 0;
		pp->rx_timestamp = jiffies;
	}
}

static int mv_eth_tag_type_set(int port, int type)
{
	struct eth_port *pp;

	if (mvNetaPortCheck(port))
		return -EINVAL;

	pp = mv_eth_port_by_id(port);
	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return -EINVAL;
	}

	if ((type == MV_TAG_TYPE_MH) || (type == MV_TAG_TYPE_DSA) || (type == MV_TAG_TYPE_EDSA))
		mvNetaMhSet(port, type);

	pp->tagged = (type == MV_TAG_TYPE_NONE) ? MV_FALSE : MV_TRUE;

	return 0;
}

void set_cpu_affinity(struct eth_port *pp, MV_U32 cpuAffinity, int group)
{
	int cpu;
	struct cpu_ctrl	*cpuCtrl;
	MV_U32 rxqAffinity = 0;

	if (cpuAffinity == 0)
		return;

	for (cpu = 0; cpu < nr_cpu_ids; cpu++) {
		cpuCtrl = pp->cpu_config[cpu];
		if (!(MV_BIT_CHECK(pp->cpu_mask, cpu)))
			continue;
		if (cpuCtrl->napiCpuGroup == group) {
			rxqAffinity = MV_REG_READ(NETA_CPU_MAP_REG(pp->port, cpu)) & 0xff;
			break;
		}
	}
	for (cpu = 0; cpu < nr_cpu_ids; cpu++) {
		if (cpuAffinity & 1) {
			cpuCtrl = pp->cpu_config[cpu];
			cpuCtrl->napi = pp->napiGroup[group];
			cpuCtrl->napiCpuGroup = group;
			cpuCtrl->cpuRxqMask = rxqAffinity;
			 
			mvNetaRxqCpuMaskSet(pp->port, rxqAffinity, cpu);
		}
		cpuAffinity >>= 1;
	}
}

int group_has_cpus(struct eth_port *pp, int group)
{
	int cpu;
	struct cpu_ctrl	*cpuCtrl;

	for (cpu = 0; cpu < nr_cpu_ids; cpu++) {
		if (!(MV_BIT_CHECK(pp->cpu_mask, cpu)))
			continue;

		cpuCtrl = pp->cpu_config[cpu];

		if (cpuCtrl->napiCpuGroup == group)
			return 1;
	}

	return 0;
}

void set_rxq_affinity(struct eth_port *pp, MV_U32 rxqAffinity, int group)
{
	int rxq, cpu;
	MV_U32 regVal;
	MV_U32 tmpRxqAffinity;
	int groupHasCpus;
	int cpuInGroup;
	struct cpu_ctrl	*cpuCtrl;

	if (rxqAffinity == 0)
		return;

	groupHasCpus = group_has_cpus(pp, group);

	if (!groupHasCpus) {
		printk(KERN_ERR "%s: operation not performed; group %d has no cpu \n", __func__, group);
		return;
	}

	for (cpu = 0; cpu < nr_cpu_ids; cpu++) {
		if (!(MV_BIT_CHECK(pp->cpu_mask, cpu)))
			continue;
		tmpRxqAffinity = rxqAffinity;

		regVal = MV_REG_READ(NETA_CPU_MAP_REG(pp->port, cpu));
		cpuCtrl = pp->cpu_config[cpu];

		if (cpuCtrl->napiCpuGroup == group) {
			cpuInGroup = 1;
			 
			regVal = regVal & 0xff00;
		} else {
			cpuInGroup = 0;
		}

		for (rxq = 0; rxq < CONFIG_MV_ETH_RXQ; rxq++) {
			 
			if (tmpRxqAffinity & 1) {
				if (cpuInGroup)
					regVal |= NETA_CPU_RXQ_ACCESS_MASK(rxq);
				else
					regVal &= ~NETA_CPU_RXQ_ACCESS_MASK(rxq);
			}
			tmpRxqAffinity >>= 1;
		}

		MV_REG_WRITE(NETA_CPU_MAP_REG(pp->port, cpu), regVal);
		cpuCtrl->cpuRxqMask = regVal;
	}
}

static int mv_eth_port_config_parse(struct eth_port *pp)
{
	char *str;

	printk(KERN_ERR "\n");
	if (pp == NULL) {
		pr_err("Port %d does not exist\n", pp->port);
		return -1;
	}

	switch (pp->port) {
	case 0:
		str = port0_config_str;
		break;
	case 1:
		str = port1_config_str;
		break;
	case 2:
		str = port2_config_str;
		break;
	case 3:
		str = port3_config_str;
		break;
	default:
		printk(KERN_ERR "  o mv_eth_port_config_parse: got unknown port %d\n", pp->port);
		return -1;
	}

	if (str != NULL) {
		if ((!strcmp(str, "disconnected")) || (!strcmp(str, "Disconnected"))) {
			printk(KERN_ERR "  o Port %d is disconnected from Linux netdevice\n", pp->port);
			clear_bit(MV_ETH_F_CONNECT_LINUX_BIT, &(pp->flags));
			return 0;
		}
	}

	printk(KERN_ERR "  o Port %d is connected to Linux netdevice\n", pp->port);
	set_bit(MV_ETH_F_CONNECT_LINUX_BIT, &(pp->flags));
	return 0;
}

#ifdef ETH_SKB_DEBUG
struct sk_buff *mv_eth_skb_debug[MV_BM_POOL_CAP_MAX * MV_ETH_BM_POOLS];
static spinlock_t skb_debug_lock;

void mv_eth_skb_check(struct sk_buff *skb)
{
	int i;
	struct sk_buff *temp;
	unsigned long flags;

	if (skb == NULL)
		printk(KERN_ERR "mv_eth_skb_check: got NULL SKB\n");

	spin_lock_irqsave(&skb_debug_lock, flags);

	i = *((u32 *)&skb->cb[0]);

	if ((i >= 0) && (i < MV_BM_POOL_CAP_MAX * MV_ETH_BM_POOLS)) {
		temp = mv_eth_skb_debug[i];
		if (mv_eth_skb_debug[i] != skb) {
			printk(KERN_ERR "mv_eth_skb_check: Unexpected skb: %p (%d) != %p (%d)\n",
			       skb, i, temp, *((u32 *)&temp->cb[0]));
		}
		mv_eth_skb_debug[i] = NULL;
	} else {
		printk(KERN_ERR "mv_eth_skb_check: skb->cb=%d is out of range\n", i);
	}

	spin_unlock_irqrestore(&skb_debug_lock, flags);
}

void mv_eth_skb_save(struct sk_buff *skb, const char *s)
{
	int i;
	int saved = 0;
	unsigned long flags;

	spin_lock_irqsave(&skb_debug_lock, flags);

	for (i = 0; i < MV_BM_POOL_CAP_MAX * MV_ETH_BM_POOLS; i++) {
		if (mv_eth_skb_debug[i] == skb) {
			printk(KERN_ERR "%s: mv_eth_skb_debug Duplicate: i=%d, skb=%p\n", s, i, skb);
			mv_eth_skb_print(skb);
		}

		if ((!saved) && (mv_eth_skb_debug[i] == NULL)) {
			mv_eth_skb_debug[i] = skb;
			*((u32 *)&skb->cb[0]) = i;
			saved = 1;
		}
	}

	spin_unlock_irqrestore(&skb_debug_lock, flags);

	if ((i == MV_BM_POOL_CAP_MAX * MV_ETH_BM_POOLS) && (!saved))
		printk(KERN_ERR "mv_eth_skb_debug is FULL, skb=%p\n", skb);
}
#endif  

struct eth_port *mv_eth_port_by_id(unsigned int port)
{
	if (port < mv_eth_ports_num)
		return mv_eth_ports[port];

	return NULL;
}

static inline int mv_eth_skb_mh_add(struct sk_buff *skb, u16 mh)
{
	 
	if (skb_headroom(skb) < MV_ETH_MH_SIZE) {
		printk(KERN_ERR "%s: skb (%p) doesn't have place for MH, head=%p, data=%p\n",
		       __func__, skb, skb->head, skb->data);
		return 1;
	}

	skb->len += MV_ETH_MH_SIZE;
	skb->data -= MV_ETH_MH_SIZE;
	*((u16 *) skb->data) = mh;

	return 0;
}

static inline int mv_eth_mh_skb_skip(struct sk_buff *skb)
{
	__skb_pull(skb, MV_ETH_MH_SIZE);
	return MV_ETH_MH_SIZE;
}

void mv_eth_ctrl_txdone(int num)
{
	mv_ctrl_txdone = num;
}

int mv_eth_ctrl_flag(int port, u32 flag, u32 val)
{
	struct eth_port *pp = mv_eth_port_by_id(port);
	u32 bit_flag = (fls(flag) - 1);

	if (!pp)
		return -ENODEV;

	if (val)
		set_bit(bit_flag, &(pp->flags));
	else
		clear_bit(bit_flag, &(pp->flags));

	if (flag == MV_ETH_F_MH)
		mvNetaMhSet(pp->port, val ? MV_TAG_TYPE_MH : MV_TAG_TYPE_NONE);

	return 0;
}

int mv_eth_ctrl_port_buf_num_set(int port, int long_num, int short_num)
{
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return -EINVAL;
	}

	if (pp->flags & MV_ETH_F_STARTED) {
		printk(KERN_ERR "Port %d must be stopped before\n", port);
		return -EINVAL;
	}
	if (pp->pool_long != NULL) {
		 
		if (pp->pool_long_num > long_num)
			mv_eth_pool_free(pp->pool_long->pool, pp->pool_long_num - long_num);
		else if (long_num > pp->pool_long_num)
			mv_eth_pool_add(pp, pp->pool_long->pool, long_num - pp->pool_long_num);
	}
	pp->pool_long_num = long_num;

#ifdef CONFIG_MV_ETH_BM_CPU
	if (MV_NETA_BM_CAP()) {
#if defined(MY_ABC_HERE)
		if ((pp->pool_short != NULL) && (pp->pool_short != pp->pool_long)) {
#else  
		if (pp->pool_short != NULL) {
#endif  
			 
			if (pp->pool_short_num > short_num)
				mv_eth_pool_free(pp->pool_short->pool, pp->pool_short_num - short_num);
			else if (short_num > pp->pool_short_num)
				mv_eth_pool_add(pp, pp->pool_short->pool, short_num - pp->pool_short_num);
		}
		pp->pool_short_num = short_num;
	}
#endif  

	return 0;
}

#ifdef CONFIG_MV_ETH_BM
 
int mv_eth_ctrl_pool_size_set(int pool, int pkt_size)
{
#ifdef CONFIG_MV_ETH_BM_CPU
	int port;
	struct bm_pool *ppool;
	struct eth_port *pp;

	if (MV_NETA_BM_CAP()) {
		if (mvNetaMaxCheck(pool, MV_ETH_BM_POOLS, "bm_pool"))
			return -EINVAL;

		ppool = &mv_eth_pool[pool];

		for (port = 0; port < mv_eth_ports_num; port++) {
			 
			if (ppool->port_map & (1 << port)) {
				pp = mv_eth_port_by_id(port);

				if (pp->flags & MV_ETH_F_STARTED) {
					pr_err("Port %d use pool #%d and must be stopped before change pkt_size\n",
						port, pool);
					return -EINVAL;
				}
			}
		}
		for (port = 0; port < mv_eth_ports_num; port++) {
			 
			if (ppool->port_map & (1 << port)) {
				pp = mv_eth_port_by_id(port);

				if (ppool == pp->pool_long) {
					mv_eth_pool_free(pool, pp->pool_long_num);
					ppool->port_map &= ~(1 << pp->port);
					pp->pool_long = NULL;
				}
				if (ppool == pp->pool_short) {
					mv_eth_pool_free(pool, pp->pool_short_num);
					ppool->port_map &= ~(1 << pp->port);
					pp->pool_short = NULL;
				}
			}
		}
		ppool->pkt_size = pkt_size;
	}
#endif  

	mv_eth_bm_config_pkt_size_set(pool, pkt_size);
	if (pkt_size == 0)
		mvNetaBmPoolBufferSizeSet(pool, 0);
	else
		mvNetaBmPoolBufferSizeSet(pool, RX_BUF_SIZE(pkt_size));

	return 0;
}
#endif  

int mv_eth_ctrl_set_poll_rx_weight(int port, u32 weight)
{
	struct cpu_ctrl	*cpuCtrl;
	struct eth_port *pp = mv_eth_port_by_id(port);
	int cpu;

	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return -EINVAL;
	}

	if (pp->flags & MV_ETH_F_STARTED) {
		printk(KERN_ERR "Port %d must be stopped before\n", port);
		return -EINVAL;
	}

	if (weight > 255)
		weight = 255;
	pp->weight = weight;

	for_each_possible_cpu(cpu) {
		cpuCtrl = pp->cpu_config[cpu];
		if (cpuCtrl->napi)
			cpuCtrl->napi->weight = pp->weight;
	}

	return 0;
}

int mv_eth_ctrl_rxq_size_set(int port, int rxq, int value)
{
	struct eth_port *pp;
	struct rx_queue	*rxq_ctrl;

	if (mvNetaPortCheck(port))
		return -EINVAL;

	if (mvNetaMaxCheck(rxq, CONFIG_MV_ETH_RXQ, "rxq"))
		return -EINVAL;

	if ((value <= 0) || (value > 0x3FFF)) {
		pr_err("RXQ size %d is out of range\n", value);
		return -EINVAL;
	}

	pp = mv_eth_port_by_id(port);
	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return -EINVAL;
	}

	if (pp->flags & MV_ETH_F_STARTED) {
		pr_err("Port %d must be stopped before\n", port);
		return -EINVAL;
	}
	rxq_ctrl = &pp->rxq_ctrl[rxq];
	if ((rxq_ctrl->q) && (rxq_ctrl->rxq_size != value)) {
		 
		mv_eth_rx_reset(pp->port);

		mvNetaRxqDelete(pp->port, rxq);
		rxq_ctrl->q = NULL;
	}
	pp->rxq_ctrl[rxq].rxq_size = value;

	return 0;
}

int mv_eth_ctrl_txq_size_set(int port, int txp, int txq, int value)
{
	struct tx_queue *txq_ctrl;
	struct eth_port *pp;

	if (mvNetaTxpCheck(port, txp))
		return -EINVAL;

	if (mvNetaMaxCheck(txq, CONFIG_MV_ETH_TXQ, "txq"))
		return -EINVAL;

	if ((value <= 0) || (value > 0x3FFF)) {
		pr_err("TXQ size %d is out of range\n", value);
		return -EINVAL;
	}

	pp = mv_eth_port_by_id(port);
	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return -EINVAL;
	}

	if (pp->flags & MV_ETH_F_STARTED) {
		pr_err("Port %d must be stopped before\n", port);
		return -EINVAL;
	}
	txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];
	if ((txq_ctrl->q) && (txq_ctrl->txq_size != value)) {
		 
		if ((mvNetaTxqNextIndexGet(pp->port, txq_ctrl->txp, txq_ctrl->txq) != 0) ||
			(mvNetaTxqPendDescNumGet(pp->port, txq_ctrl->txp, txq_ctrl->txq) != 0) ||
			(mvNetaTxqSentDescNumGet(pp->port, txq_ctrl->txp, txq_ctrl->txq) != 0)) {
			pr_err("%s: port=%d, txp=%d, txq=%d must be in its initial state\n",
				__func__, port, txq_ctrl->txp, txq_ctrl->txq);
			return -EINVAL;
		}
		mv_eth_txq_delete(pp, txq_ctrl);
	}
	txq_ctrl->txq_size = value;

	return 0;
}

int mv_eth_ctrl_txq_mode_get(int port, int txp, int txq, int *value)
{
	int cpu, mode = MV_ETH_TXQ_FREE, val = 0;
	struct tx_queue *txq_ctrl;
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (pp == NULL)
		return -ENODEV;

	txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];
	for_each_possible_cpu(cpu)
		if (txq_ctrl->cpu_owner[cpu]) {
			mode = MV_ETH_TXQ_CPU;
			val += txq_ctrl->cpu_owner[cpu];
		}
	if ((mode == MV_ETH_TXQ_FREE) && (txq_ctrl->hwf_rxp < (MV_U8) mv_eth_ports_num)) {
		mode = MV_ETH_TXQ_HWF;
		val = txq_ctrl->hwf_rxp;
	}
	if (value)
		*value = val;

	return mode;
}

int mv_eth_ctrl_txq_cpu_own(int port, int txp, int txq, int add, int cpu)
{
	int mode;
	struct tx_queue *txq_ctrl;
	struct cpu_ctrl	*cpuCtrl;
	struct eth_port *pp = mv_eth_port_by_id(port);

	if ((pp == NULL) || (pp->txq_ctrl == NULL))
		return -ENODEV;

	mode = mv_eth_ctrl_txq_mode_get(port, txp, txq, NULL);

	txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];
	cpuCtrl = pp->cpu_config[cpu];

	if (add) {
		if ((mode != MV_ETH_TXQ_CPU) && (mode != MV_ETH_TXQ_FREE))
			return -EINVAL;

		txq_ctrl->cpu_owner[cpu]++;

	} else {
		if (mode != MV_ETH_TXQ_CPU)
			return -EINVAL;

		txq_ctrl->cpu_owner[cpu]--;
	}

	mv_eth_txq_update_shared(txq_ctrl, pp);

	return 0;
}

int mv_eth_ctrl_txq_hwf_own(int port, int txp, int txq, int rxp)
{
	int mode;
	struct tx_queue *txq_ctrl;
	struct eth_port *pp = mv_eth_port_by_id(port);

	if ((pp == NULL) || (pp->txq_ctrl == NULL))
		return -ENODEV;

	mode = mv_eth_ctrl_txq_mode_get(port, txp, txq, NULL);

	txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];

	if (rxp == -1) {
		if (mode != MV_ETH_TXQ_HWF)
			return -EINVAL;
	} else {
		if ((mode != MV_ETH_TXQ_HWF) && (mode != MV_ETH_TXQ_FREE))
			return -EINVAL;
	}

	txq_ctrl->hwf_rxp = (MV_U8) rxp;

	return 0;
}

int mv_eth_shared_set(int port, int txp, int txq, int value)
{
	struct tx_queue *txq_ctrl;
	struct eth_port *pp = mv_eth_port_by_id(port);

	if ((value < 0) || (value > 1)) {
		pr_err("%s: Invalid value %d, should be 0 or 1\n", __func__, value);
		return -EINVAL;
	}

	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return -EINVAL;
	}

	if (pp->flags & MV_ETH_F_STARTED) {
		printk(KERN_ERR "Port %d must be stopped before\n", port);
		return -EINVAL;
	}

	txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];

	if (txq_ctrl == NULL) {
		printk(KERN_ERR "%s: txq_ctrl is null \n", __func__);
		return -EINVAL;
	}

	value ? (txq_ctrl->flags |= MV_ETH_F_TX_SHARED) : (txq_ctrl->flags &= ~MV_ETH_F_TX_SHARED);

	return MV_OK;
}

int mv_eth_ctrl_txq_cpu_def(int port, int txp, int txq, int cpu)
{
	struct cpu_ctrl	*cpuCtrl;
	struct eth_port *pp;

	if ((cpu >= nr_cpu_ids) || (cpu < 0)) {
		pr_err("cpu #%d is out of range: from 0 to %d\n",
			cpu, nr_cpu_ids - 1);
		return -EINVAL;
	}

	if (mvNetaTxpCheck(port, txp))
		return -EINVAL;

	pp = mv_eth_port_by_id(port);
	if ((pp == NULL) || (pp->txq_ctrl == NULL)) {
		pr_err("Port %d does not exist\n", port);
		return -ENODEV;
	}
	cpuCtrl = pp->cpu_config[cpu];

	if (!(MV_BIT_CHECK(cpuCtrl->cpuTxqMask, txq)))	{
		printk(KERN_ERR "Txq #%d can not allocated for cpu #%d\n", txq, cpu);
		return -EINVAL;
	}

	if (test_bit(MV_ETH_F_STARTED_BIT, &(pp->flags))) {
		 
		mv_eth_ctrl_txq_cpu_own(port, pp->txp, cpuCtrl->txq, 0, cpu);

		if (txq != -1) {
			if (mvNetaMaxCheck(txq, CONFIG_MV_ETH_TXQ, "txq"))
				return -EINVAL;

			if (mv_eth_ctrl_txq_cpu_own(port, txp, txq, 1, cpu))
				return -EINVAL;
		}
	}
	pp->txp = txp;
	cpuCtrl->txq = txq;

	return 0;
}

int	mv_eth_cpu_txq_mask_set(int port, int cpu, int txqMask)
{
	struct tx_queue *txq_ctrl;
	int i;
	struct cpu_ctrl	*cpuCtrl;
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (pp->flags & MV_ETH_F_STARTED) {
		printk(KERN_ERR "Port %d must be stopped before\n", port);
		return -EINVAL;
	}

	if ((cpu >= nr_cpu_ids) || (cpu < 0)) {
		printk(KERN_ERR "cpu #%d is out of range: from 0 to %d\n",
			cpu, nr_cpu_ids - 1);
		return -EINVAL;
	}
	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return MV_FAIL;
	}

	if (!(MV_BIT_CHECK(pp->cpu_mask, cpu)))	{
		printk(KERN_ERR "%s:Error- Cpu #%d masked for port  #%d\n", __func__, cpu, port);
		return -EINVAL;
	}

	cpuCtrl = pp->cpu_config[cpu];

	if (!(MV_BIT_CHECK(txqMask, cpuCtrl->txq))) {
		printk(KERN_ERR "Error: port %d default txq %d can not be masked.\n", port, cpuCtrl->txq);
		return -EINVAL;
	}

	for (i = 0; i < 256; i++)
		if (cpuCtrl->txq_tos_map[i] != MV_ETH_TXQ_INVALID)
			if (!(MV_BIT_CHECK(txqMask, cpuCtrl->txq_tos_map[i]))) {
				printk(KERN_WARNING "Warning: port %d tos 0h%x mapped to txq %d ,this rule delete due to new masked value (0X%x).\n",
					port, i, cpuCtrl->txq_tos_map[i], txqMask);
				txq_ctrl = &pp->txq_ctrl[pp->txp * CONFIG_MV_ETH_TXQ + cpuCtrl->txq_tos_map[i]];
				txq_ctrl->cpu_owner[cpu]--;
				mv_eth_txq_update_shared(txq_ctrl, pp);
				cpuCtrl->txq_tos_map[i] = MV_ETH_TXQ_INVALID;
			}

	for (i = 0; i < CONFIG_MV_ETH_TXQ; i++)
		if (!(MV_BIT_CHECK(txqMask, i))) {
			txq_ctrl = &pp->txq_ctrl[pp->txp * CONFIG_MV_ETH_TXQ + i];
			if ((txq_ctrl != NULL) && (txq_ctrl->nfpCounter != 0)) {
				printk(KERN_ERR "Error: port %d txq %d ruled by NFP, can not be masked.\n", port, i);
				return -EINVAL;
			}
		}

	mvNetaTxqCpuMaskSet(port, txqMask, cpu);
	cpuCtrl->cpuTxqMask = txqMask;

	return 0;
}

int mv_eth_ctrl_tx_cmd(int port, u32 tx_cmd)
{
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (!pp)
		return -ENODEV;

	pp->hw_cmd = tx_cmd;

	return 0;
}

int mv_eth_ctrl_tx_mh(int port, u16 mh)
{
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (!pp)
		return -ENODEV;

	pp->tx_mh = mh;

	return 0;
}

#ifdef CONFIG_MV_ETH_TX_SPECIAL
 
void mv_eth_tx_special_check_func(int port,
					int (*func)(int port, struct net_device *dev, struct sk_buff *skb,
								struct mv_eth_tx_spec *tx_spec_out))
{
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (pp)
		pp->tx_special_check = func;
}
#endif  

#ifdef CONFIG_MV_ETH_RX_SPECIAL
 
void mv_eth_rx_special_proc_func(int port, void (*func)(int port, int rxq, struct net_device *dev,
							struct sk_buff *skb, struct neta_rx_desc *rx_desc))
{
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (pp)
		pp->rx_special_proc = func;
}
#endif  

static inline u16 mv_eth_select_txq(struct net_device *dev, struct sk_buff *skb)
{
	struct eth_port *pp = MV_ETH_PRIV(dev);
	return mv_eth_tx_policy(pp, skb);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 25)
static u32 mv_eth_netdev_fix_features(struct net_device *dev, u32 features)
#else
static netdev_features_t mv_eth_netdev_fix_features(struct net_device *dev, netdev_features_t features)
#endif
{
#ifdef CONFIG_MV_ETH_TX_CSUM_OFFLOAD
	struct eth_port *pp = MV_ETH_PRIV(dev);

	if (dev->mtu > pp->plat_data->tx_csum_limit) {
		if (features & (NETIF_F_IP_CSUM | NETIF_F_TSO)) {
			features &= ~(NETIF_F_IP_CSUM | NETIF_F_TSO);
			printk(KERN_ERR "%s: NETIF_F_IP_CSUM and NETIF_F_TSO not supported for mtu larger %d bytes\n",
					dev->name, pp->plat_data->tx_csum_limit);
		}
	}
#endif  
	return features;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 25)
static int mv_eth_netdev_set_features(struct net_device *dev, u32 features)
#else
static int mv_eth_netdev_set_features(struct net_device *dev, netdev_features_t features)
#endif
{
	u32 changed = dev->features ^ features;

	if (changed == 0)
		return 0;

#if defined(MV_ETH_PNC_LB) && defined(CONFIG_MV_ETH_PNC)
	if (MV_NETA_PNC_CAP() && (changed & NETIF_F_RXHASH)) {
		if (features & NETIF_F_RXHASH) {
			dev->features |= NETIF_F_RXHASH;
			mvPncLbModeIp4(LB_2_TUPLE_VALUE);
			mvPncLbModeIp6(LB_2_TUPLE_VALUE);
			mvPncLbModeL4(LB_4_TUPLE_VALUE);
		} else {
			dev->features |= ~NETIF_F_RXHASH;
			mvPncLbModeIp4(LB_DISABLE_VALUE);
			mvPncLbModeIp6(LB_DISABLE_VALUE);
			mvPncLbModeL4(LB_DISABLE_VALUE);
		}
	}
#endif  

	return 0;
}

static const struct net_device_ops mv_eth_netdev_ops = {
	.ndo_open = mv_eth_open,
	.ndo_stop = mv_eth_stop,
	.ndo_start_xmit = mv_eth_tx,
	.ndo_set_rx_mode = mv_eth_set_multicast_list,
	.ndo_set_mac_address = mv_eth_set_mac_addr,
	.ndo_change_mtu = mv_eth_change_mtu,
	.ndo_tx_timeout = mv_eth_tx_timeout,
	.ndo_select_queue = mv_eth_select_txq,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)
	.ndo_fix_features = mv_eth_netdev_fix_features,
	.ndo_set_features = mv_eth_netdev_set_features,
#endif
};

void mv_eth_link_status_print(int port)
{
	MV_ETH_PORT_STATUS link;

	mvNetaLinkStatus(port, &link);
#ifdef CONFIG_MV_PON
	if (MV_PON_PORT(port))
		link.linkup = mv_pon_link_status();
#endif  

#ifdef MY_ABC_HERE
	if (link.linkup) {
		printk(KERN_INFO "link up");
		printk(KERN_INFO ", %s duplex", (link.duplex == MV_ETH_DUPLEX_FULL) ? "full" : "half");
		printk(KERN_INFO ", speed ");

		if (link.speed == MV_ETH_SPEED_1000)
			printk(KERN_INFO "1 Gbps\n");
		else if (link.speed == MV_ETH_SPEED_100)
			printk(KERN_INFO "100 Mbps\n");
		else
			printk(KERN_INFO "10 Mbps\n");
	} else
		printk(KERN_INFO "link down\n");
#else
	if (link.linkup) {
		printk(KERN_CONT "link up");
		printk(KERN_CONT ", %s duplex", (link.duplex == MV_ETH_DUPLEX_FULL) ? "full" : "half");
		printk(KERN_CONT ", speed ");

		if (link.speed == MV_ETH_SPEED_1000)
			printk(KERN_CONT "1 Gbps\n");
		else if (link.speed == MV_ETH_SPEED_100)
			printk(KERN_CONT "100 Mbps\n");
		else
			printk(KERN_CONT "10 Mbps\n");
	} else
		printk(KERN_CONT "link down\n");
#endif  
}

static void mv_eth_rx_error(struct eth_port *pp, struct neta_rx_desc *rx_desc)
{
	STAT_ERR(pp->stats.rx_error++);

	if (pp->dev)
		pp->dev->stats.rx_errors++;

#ifdef CONFIG_MV_NETA_DEBUG_CODE
	if ((pp->flags & MV_ETH_F_DBG_RX) == 0)
		return;

	if (!printk_ratelimit())
		return;

	if ((rx_desc->status & NETA_RX_FL_DESC_MASK) != NETA_RX_FL_DESC_MASK) {
		printk(KERN_ERR "giga #%d: bad rx status %08x (buffer oversize), size=%d\n",
				pp->port, rx_desc->status, rx_desc->dataSize);
		return;
	}

	switch (rx_desc->status & NETA_RX_ERR_CODE_MASK) {
	case NETA_RX_ERR_CRC:
		printk(KERN_ERR "giga #%d: bad rx status %08x (crc error), size=%d\n",
				pp->port, rx_desc->status, rx_desc->dataSize);
		break;
	case NETA_RX_ERR_OVERRUN:
		printk(KERN_ERR "giga #%d: bad rx status %08x (overrun error), size=%d\n",
				pp->port, rx_desc->status, rx_desc->dataSize);
		break;
	case NETA_RX_ERR_LEN:
		printk(KERN_ERR "giga #%d: bad rx status %08x (max frame length error), size=%d\n",
				pp->port, rx_desc->status, rx_desc->dataSize);
		break;
	case NETA_RX_ERR_RESOURCE:
		printk(KERN_ERR "giga #%d: bad rx status %08x (resource error), size=%d\n",
				pp->port, rx_desc->status, rx_desc->dataSize);
		break;
	}
	mv_eth_rx_desc_print(rx_desc);
#endif  
}

void mv_eth_skb_print(struct sk_buff *skb)
{
	printk(KERN_ERR "skb=%p: head=%p, data=%p, tail=%p, end=%p\n", skb, skb->head, skb->data, skb->tail, skb->end);
	printk(KERN_ERR "\t mac=%p, network=%p, transport=%p\n",
			skb->mac_header, skb->network_header, skb->transport_header);
	printk(KERN_ERR "\t truesize=%d, len=%d, data_len=%d, mac_len=%d\n",
		skb->truesize, skb->len, skb->data_len, skb->mac_len);
	printk(KERN_ERR "\t users=%d, dataref=%d, nr_frags=%d, gso_size=%d, gso_segs=%d\n",
	       atomic_read(&skb->users), atomic_read(&skb_shinfo(skb)->dataref),
	       skb_shinfo(skb)->nr_frags, skb_shinfo(skb)->gso_size, skb_shinfo(skb)->gso_segs);
	printk(KERN_ERR "\t proto=%d, ip_summed=%d, priority=%d\n", ntohs(skb->protocol), skb->ip_summed, skb->priority);
}

void mv_eth_rx_desc_print(struct neta_rx_desc *desc)
{
	int i;
	u32 *words = (u32 *) desc;

	printk(KERN_ERR "RX desc - %p: ", desc);
	for (i = 0; i < 8; i++)
		printk(KERN_CONT "%8.8x ", *words++);
	printk(KERN_CONT "\n");

	if (desc->status & NETA_RX_IP4_FRAG_MASK)
		printk(KERN_ERR "Frag, ");

	printk(KERN_CONT "size=%d, L3_offs=%d, IP_hlen=%d, L4_csum=%s, L3=",
	       desc->dataSize,
	       (desc->status & NETA_RX_L3_OFFSET_MASK) >> NETA_RX_L3_OFFSET_OFFS,
	       (desc->status & NETA_RX_IP_HLEN_MASK) >> NETA_RX_IP_HLEN_OFFS,
	       (desc->status & NETA_RX_L4_CSUM_OK_MASK) ? "Ok" : "Bad");

	if (NETA_RX_L3_IS_IP4(desc->status))
		printk(KERN_CONT "IPv4, ");
	else if (NETA_RX_L3_IS_IP4_ERR(desc->status))
		printk(KERN_CONT "IPv4 bad, ");
	else if (NETA_RX_L3_IS_IP6(desc->status))
		printk(KERN_CONT "IPv6, ");
	else
		printk(KERN_CONT "Unknown, ");

	printk(KERN_CONT "L4=");
	if (NETA_RX_L4_IS_TCP(desc->status))
		printk(KERN_CONT "TCP");
	else if (NETA_RX_L4_IS_UDP(desc->status))
		printk(KERN_CONT "UDP");
	else
		printk(KERN_CONT "Unknown");
	printk(KERN_CONT "\n");

#ifdef CONFIG_MV_ETH_PNC
	if (MV_NETA_PNC_CAP()) {
		pr_err("RINFO: ");
		if (desc->pncInfo & NETA_PNC_DA_MC)
			pr_cont("DA_MC, ");
		if (desc->pncInfo & NETA_PNC_DA_BC)
			pr_cont("DA_BC, ");
		if (desc->pncInfo & NETA_PNC_DA_UC)
			pr_cont("DA_UC, ");
		if (desc->pncInfo & NETA_PNC_VLAN)
			pr_cont("VLAN, ");
		if (desc->pncInfo & NETA_PNC_PPPOE)
			pr_cont("PPPOE, ");
		if (desc->pncInfo & NETA_PNC_RX_SPECIAL)
			pr_cont("RX_SPEC, ");
	}
#endif  

	printk(KERN_CONT "\n");
}
EXPORT_SYMBOL(mv_eth_rx_desc_print);

void mv_eth_tx_desc_print(struct neta_tx_desc *desc)
{
	int i;
	u32 *words = (u32 *) desc;

	printk(KERN_ERR "TX desc - %p: ", desc);
	for (i = 0; i < 8; i++)
		printk(KERN_CONT "%8.8x ", *words++);
	printk(KERN_CONT "\n");
}
EXPORT_SYMBOL(mv_eth_tx_desc_print);

void mv_eth_pkt_print(struct eth_port *pp, struct eth_pbuf *pkt)
{
	printk(KERN_ERR "pkt: len=%d off=%d pool=%d "
	       "skb=%p pa=%lx buf=%p\n",
	       pkt->bytes, pkt->offset, pkt->pool,
	       pkt->osInfo, pkt->physAddr, pkt->pBuf);

	mvDebugMemDump(pkt->pBuf + pkt->offset, 64, 1);
	mvOsCacheInvalidate(pp->dev->dev.parent, pkt->pBuf + pkt->offset, 64);
}
EXPORT_SYMBOL(mv_eth_pkt_print);

static inline void mv_eth_rx_csum(struct eth_port *pp, struct neta_rx_desc *rx_desc, struct sk_buff *skb)
{
#if defined(CONFIG_MV_ETH_RX_CSUM_OFFLOAD)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)
	if (pp->dev->features & NETIF_F_RXCSUM) {

		if ((NETA_RX_L3_IS_IP4(rx_desc->status) ||
	      NETA_RX_L3_IS_IP6(rx_desc->status)) && (rx_desc->status & NETA_RX_L4_CSUM_OK_MASK)) {
			skb->csum = 0;
			skb->ip_summed = CHECKSUM_UNNECESSARY;
			STAT_DBG(pp->stats.rx_csum_hw++);
			return;
		}
	}
#endif
#endif  

	skb->ip_summed = CHECKSUM_NONE;
	STAT_DBG(pp->stats.rx_csum_sw++);
}

static inline int mv_eth_tx_done_policy(u32 cause)
{
	return fls(cause >> NETA_CAUSE_TXQ_SENT_DESC_OFFS) - 1;
}

inline int mv_eth_rx_policy(u32 cause)
{
	return fls(cause >> NETA_CAUSE_RXQ_OCCUP_DESC_OFFS) - 1;
}

static inline int mv_eth_txq_tos_map_get(struct eth_port *pp, MV_U8 tos, MV_U8 cpu)
{
	MV_U8 q = pp->cpu_config[cpu]->txq_tos_map[tos];

	if (q == MV_ETH_TXQ_INVALID)
		return pp->cpu_config[smp_processor_id()]->txq;

	return q;
}

static inline int mv_eth_tx_policy(struct eth_port *pp, struct sk_buff *skb)
{
	int txq = pp->cpu_config[smp_processor_id()]->txq;

	if (skb->protocol == htons(ETH_P_IP)) {
		struct iphdr *iph = ip_hdr(skb);

		txq = mv_eth_txq_tos_map_get(pp, iph->tos, smp_processor_id());
	}
	return txq;
}

static struct sk_buff *mv_eth_skb_alloc(struct eth_port *pp, struct bm_pool *pool,
					phys_addr_t *phys_addr, gfp_t gfp_mask)
{
	struct sk_buff *skb;

	skb = __dev_alloc_skb(pool->pkt_size, GFP_DMA | gfp_mask);
	if (!skb) {
		STAT_ERR(pool->stats.skb_alloc_oom++);
		return NULL;
	}
	STAT_DBG(pool->stats.skb_alloc_ok++);

#ifdef ETH_SKB_DEBUG
	mv_eth_skb_save(skb, "alloc");
#endif  

#ifdef CONFIG_MV_ETH_BM_CPU
	 
#if !defined(CONFIG_MV_ETH_BE_WA)
		*((MV_U32 *) skb->head) = MV_32BIT_LE((MV_U32)skb);
#else
		*((MV_U32 *) skb->head) = (MV_U32)skb;
#endif  
	mvOsCacheLineFlush(pp->dev->dev.parent, skb->head);
#endif  

#if defined(MY_ABC_HERE)
	 
	MV_NETA_SKB_MAGIC_BPID_SET(skb, (MV_NETA_SKB_MAGIC(skb) | pool->pool));
#endif  

	if (phys_addr)
		*phys_addr = mvOsCacheInvalidate(pp->dev->dev.parent, skb->head, RX_BUF_SIZE(pool->pkt_size));

	return skb;
}

#ifdef CONFIG_MV_NETA_SKB_RECYCLE
static inline struct bm_pool *mv_eth_skb_recycle_get_pool(struct sk_buff *skb)
{
	if (mv_eth_is_swf_recycle() && MV_NETA_SKB_RECYCLE_MAGIC_IS_OK(skb))
#if defined(MY_ABC_HERE)
		return &mv_eth_pool[MV_NETA_SKB_BPID_GET(skb)];
#else
		return &mv_eth_pool[MV_NETA_SKB_RECYCLE_BPID_GET(skb)];
#endif  
	else
		return NULL;
}
#endif

static inline void mv_eth_txq_buf_free(struct eth_port *pp, u32 shadow)
{
	if (!shadow)
		return;

	if (shadow & MV_ETH_SHADOW_SKB) {
		shadow &= ~MV_ETH_SHADOW_SKB;
#ifndef CONFIG_MV_ETH_BM_CPU
		struct sk_buff *skb = (struct sk_buff *)shadow;
		struct bm_pool *pool = mv_eth_skb_recycle_get_pool(skb);
		 
		if (pool && skb_recycle_check(skb, pool->pkt_size)) {
			 
			if (mv_eth_pool_put(pool, skb))
				STAT_DBG(pp->stats.tx_skb_free++);
			else
				STAT_DBG(pool->stats.skb_recycled_ok++);
		} else {
			dev_kfree_skb_any(skb);
			STAT_DBG(pp->stats.tx_skb_free++);
			if (pool)
				STAT_DBG(pool->stats.skb_recycled_err++);
		}
#else
		dev_kfree_skb_any((struct sk_buff *)shadow);
		STAT_DBG(pp->stats.tx_skb_free++);
#endif  
	} else if (shadow & MV_ETH_SHADOW_EXT) {
		shadow &= ~MV_ETH_SHADOW_EXT;
		mv_eth_extra_pool_put(pp, (void *)shadow);
	} else {
		 
		pr_err("%s: unexpected buffer - not skb and not ext\n", __func__);
	}
}

static inline void mv_eth_txq_cpu_clean(struct eth_port *pp, struct tx_queue *txq_ctrl)
{
	int hw_txq_i, last_txq_i, i, count;
	u32 shadow;

	hw_txq_i = mvNetaTxqNextIndexGet(pp->port, txq_ctrl->txp, txq_ctrl->txq);
	last_txq_i = txq_ctrl->shadow_txq_put_i;

	i = hw_txq_i;
	count = 0;
	while (i != last_txq_i) {
		shadow = txq_ctrl->shadow_txq[i];
		mv_eth_txq_buf_free(pp, shadow);
		txq_ctrl->shadow_txq[i] = (u32)NULL;

		i = MV_NETA_QUEUE_NEXT_DESC(&txq_ctrl->q->queueCtrl, i);
		count++;
	}
	if (count > 0) {
		pr_info("\n%s: port=%d, txp=%d, txq=%d, mode=CPU\n",
			__func__, pp->port, txq_ctrl->txp, txq_ctrl->txq);
		pr_info("Free %d buffers: from desc=%d to desc=%d, tx_count=%d\n",
			count, hw_txq_i, last_txq_i, txq_ctrl->txq_count);
	}
}

#ifdef CONFIG_MV_ETH_HWF
static inline void mv_eth_txq_hwf_clean(struct eth_port *pp, struct tx_queue *txq_ctrl, int rx_port)
{
	int pool, hw_txq_i, last_txq_i, i, count;
	struct neta_tx_desc *tx_desc;

	hw_txq_i = mvNetaTxqNextIndexGet(pp->port, txq_ctrl->txp, txq_ctrl->txq);

	if (mvNetaHwfTxqNextIndexGet(rx_port, pp->port, txq_ctrl->txp, txq_ctrl->txq, &last_txq_i) != MV_OK) {
		printk(KERN_ERR "%s: mvNetaHwfTxqNextIndexGet failed\n", __func__);
		return;
	}

	i = hw_txq_i;
	count = 0;
	while (i != last_txq_i) {
		tx_desc = (struct neta_tx_desc *)MV_NETA_QUEUE_DESC_PTR(&txq_ctrl->q->queueCtrl, i);
		if (mvNetaTxqDescIsValid(tx_desc)) {
			mvNetaTxqDescInv(tx_desc);
			mv_eth_tx_desc_flush(pp, tx_desc);

			pool = (tx_desc->command & NETA_TX_BM_POOL_ID_ALL_MASK) >> NETA_TX_BM_POOL_ID_OFFS;
			mvBmPoolPut(pool, (MV_ULONG)tx_desc->bufPhysAddr);
			count++;
		}
		i = MV_NETA_QUEUE_NEXT_DESC(&txq_ctrl->q->queueCtrl, i);
	}
	if (count > 0) {
		pr_info("\n%s: port=%d, txp=%d, txq=%d, mode=HWF-%d\n",
			__func__, pp->port, txq_ctrl->txp, txq_ctrl->txq, rx_port);
		pr_info("Free %d buffers to BM: from desc=%d to desc=%d\n",
			count, hw_txq_i, last_txq_i);
	}
}
#endif  

int mv_eth_txq_clean(int port, int txp, int txq)
{
	int mode, rx_port;
	struct eth_port *pp;
	struct tx_queue *txq_ctrl;

	if (mvNetaTxpCheck(port, txp))
		return -EINVAL;

	if (mvNetaMaxCheck(txq, CONFIG_MV_ETH_TXQ, "txq"))
		return -EINVAL;

	pp = mv_eth_port_by_id(port);
	if ((pp == NULL) || (pp->txq_ctrl == NULL))
		return -ENODEV;

	txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];

	mode = mv_eth_ctrl_txq_mode_get(pp->port, txq_ctrl->txp, txq_ctrl->txq, &rx_port);
	if (mode == MV_ETH_TXQ_CPU)
		mv_eth_txq_cpu_clean(pp, txq_ctrl);
#ifdef CONFIG_MV_ETH_HWF
	else if (mode == MV_ETH_TXQ_HWF && MV_NETA_HWF_CAP())
		mv_eth_txq_hwf_clean(pp, txq_ctrl, rx_port);
#endif  

	return 0;
}

static inline void mv_eth_txq_bufs_free(struct eth_port *pp, struct tx_queue *txq_ctrl, int num)
{
	u32 shadow;
	int i;

	for (i = 0; i < num; i++) {
		shadow = txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_get_i];
		mv_eth_shadow_inc_get(txq_ctrl);
		mv_eth_txq_buf_free(pp, shadow);
	}
}

inline u32 mv_eth_txq_done(struct eth_port *pp, struct tx_queue *txq_ctrl)
{
	int tx_done;

	tx_done = mvNetaTxqSentDescProc(pp->port, txq_ctrl->txp, txq_ctrl->txq);
	if (!tx_done)
		return tx_done;
 
	if (!mv_eth_txq_bm(txq_ctrl))
		mv_eth_txq_bufs_free(pp, txq_ctrl, tx_done);

	txq_ctrl->txq_count -= tx_done;
	STAT_DBG(txq_ctrl->stats.txq_txdone += tx_done);

	return tx_done;
}
EXPORT_SYMBOL(mv_eth_txq_done);

inline struct sk_buff *mv_eth_pool_get(struct eth_port *pp, struct bm_pool *pool)
{
	struct sk_buff *skb = NULL;
	phys_addr_t pa;
	unsigned long flags = 0;

	MV_ETH_LOCK(&pool->lock, flags);

	if (mvStackIndex(pool->stack) > 0) {
		STAT_DBG(pool->stats.stack_get++);
		skb = (struct sk_buff *)mvStackPop(pool->stack);
	} else
		STAT_ERR(pool->stats.stack_empty++);

	MV_ETH_UNLOCK(&pool->lock, flags);
	if (skb)
		return skb;

	skb = mv_eth_skb_alloc(pp, pool, &pa, GFP_ATOMIC);
	if (!skb)
		return NULL;

	return skb;
}

inline int mv_eth_refill(struct eth_port *pp, int rxq,
				struct bm_pool *pool, struct neta_rx_desc *rx_desc)
{
	struct sk_buff *skb = NULL;
	phys_addr_t phys_addr;
	int pool_in_use = atomic_read(&pool->in_use);

#if defined(MY_ABC_HERE)
	 
	if (mv_eth_pool_bm(pool))
		mvOsCacheLineInv(pp->dev->dev.parent, rx_desc);
#endif  

	if (pool_in_use <= 0)
		return 0;

	if (mv_eth_is_swf_recycle()) {
#if defined(MY_ABC_HERE)
		if (mv_eth_pool_bm(pool) && (pool_in_use < pool->in_use_thresh))
			return 0;
#else  
		if (mv_eth_pool_bm(pool) && (pool_in_use < pool->in_use_thresh)) {
			mvOsCacheLineInv(pp->dev->dev.parent, rx_desc);
			return 0;
		}
#endif  

		skb = mv_eth_pool_get(pp, pool);
		if (!skb)
			return 1;
	}

	if (!skb) {
		skb = mv_eth_skb_alloc(pp, pool, &phys_addr, GFP_ATOMIC);
#if defined(MY_ABC_HERE)
		if (!skb)
			return 1;
#else  
		if (!skb) {
			pool->missed++;
			mv_eth_add_cleanup_timer(pp->cpu_config[smp_processor_id()]);
		}
#endif  
	}

	mv_eth_rxq_refill(pp, rxq, pool, skb, rx_desc);
	atomic_dec(&pool->in_use);

	return 0;
}
EXPORT_SYMBOL(mv_eth_refill);

static inline MV_U32 mv_eth_skb_tx_csum(struct eth_port *pp, struct sk_buff *skb)
{
#ifdef CONFIG_MV_ETH_TX_CSUM_OFFLOAD
	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		int   ip_hdr_len = 0;
		MV_U8 l4_proto;

		if (skb->protocol == htons(ETH_P_IP)) {
			struct iphdr *ip4h = ip_hdr(skb);

			ip_hdr_len = ip4h->ihl;
			l4_proto = ip4h->protocol;
		} else if (skb->protocol == htons(ETH_P_IPV6)) {
			 
			struct ipv6hdr *ip6h = ipv6_hdr(skb);

			if (skb_network_header_len(skb) > 0)
				ip_hdr_len = (skb_network_header_len(skb) >> 2);
			l4_proto = ip6h->nexthdr;
		} else {
			STAT_DBG(pp->stats.tx_csum_sw++);
			return NETA_TX_L4_CSUM_NOT;
		}
		STAT_DBG(pp->stats.tx_csum_hw++);

		return mvNetaTxqDescCsum(skb_network_offset(skb), skb->protocol, ip_hdr_len, l4_proto);
	}
#endif  

	STAT_DBG(pp->stats.tx_csum_sw++);
	return NETA_TX_L4_CSUM_NOT;
}

#ifdef CONFIG_MV_ETH_RX_DESC_PREFETCH
inline struct neta_rx_desc *mv_eth_rx_prefetch(struct eth_port *pp, MV_NETA_RXQ_CTRL *rx_ctrl,
									  int rx_done, int rx_todo)
{
	struct neta_rx_desc	*rx_desc, *next_desc;

	rx_desc = mvNetaRxqNextDescGet(rx_ctrl);
	if (rx_done == 0) {
		 
		mvOsCacheLineInv(pp->dev->dev.parent, rx_desc);
		prefetch(rx_desc);
	}
	if ((rx_done + 1) == rx_todo) {
		 
		return rx_desc;
	}
	 
	next_desc = mvNetaRxqDescGet(rx_ctrl);
	mvOsCacheLineInv(pp->dev->dev.parent, next_desc);
	prefetch(next_desc);

	return rx_desc;
}
#endif  

static inline int mv_eth_rx(struct eth_port *pp, int rx_todo, int rxq, struct napi_struct *napi)
{
	struct net_device *dev;
	MV_NETA_RXQ_CTRL *rx_ctrl = pp->rxq_ctrl[rxq].q;
	int rx_done, rx_filled, err;
	struct neta_rx_desc *rx_desc;
	u32 rx_status;
	int rx_bytes;
	struct sk_buff *skb;
	struct bm_pool *pool;
	int pool_id;
#ifdef CONFIG_NETMAP
	if (pp->flags & MV_ETH_F_IFCAP_NETMAP) {
		int netmap_done;
		if (netmap_rx_irq(pp->dev, 0, &netmap_done))
			return 1;  
	}
#endif  
	 
	rx_done = mvNetaRxqBusyDescNumGet(pp->port, rxq);
	mvOsCacheIoSync(pp->dev->dev.parent);

	if (rx_todo > rx_done)
		rx_todo = rx_done;

	rx_done = 0;
	rx_filled = 0;

	while (rx_done < rx_todo) {

#ifdef CONFIG_MV_ETH_RX_DESC_PREFETCH
		rx_desc = mv_eth_rx_prefetch(pp, rx_ctrl, rx_done, rx_todo);
#else
		rx_desc = mvNetaRxqNextDescGet(rx_ctrl);
		mvOsCacheLineInv(pp->dev->dev.parent, rx_desc);

		prefetch(rx_desc);
#endif  

		rx_done++;
		rx_filled++;

#if defined(MV_CPU_BE)
		mvNetaRxqDescSwap(rx_desc);
#endif  

#ifdef CONFIG_MV_NETA_DEBUG_CODE
		if (pp->flags & MV_ETH_F_DBG_RX) {
			pr_info("\n%s: port=%d, cpu=%d\n", __func__, pp->port, smp_processor_id());
			mv_eth_rx_desc_print(rx_desc);
		}
#endif  

		rx_status = rx_desc->status;
		skb = (struct sk_buff *)rx_desc->bufCookie;
#if defined(MY_ABC_HERE)
		 
		pool_id = (MV_NETA_BM_CAP()) ? (NETA_RX_GET_BPID(rx_desc)) : (MV_NETA_SKB_BPID_GET(skb));
#else
#if !defined(CONFIG_MV_ETH_BM_CPU) && (defined(CONFIG_MV_NETA_SKB_RECYCLE))
		pool_id = MV_NETA_SKB_RECYCLE_BPID_GET(skb);
#else
		pool_id = NETA_RX_GET_BPID(rx_desc);
#endif
#endif  
		pool = &mv_eth_pool[pool_id];

		if (((rx_status & NETA_RX_FL_DESC_MASK) != NETA_RX_FL_DESC_MASK) ||
			(rx_status & NETA_RX_ES_MASK)) {

			mv_eth_rx_error(pp, rx_desc);

			mv_eth_rxq_refill(pp, rxq, pool, skb, rx_desc);
			continue;
		}

		mvOsCacheMultiLineInv(pp->dev->dev.parent, skb->head + NET_SKB_PAD, rx_desc->dataSize);

#ifdef CONFIG_MV_ETH_RX_PKT_PREFETCH
		prefetch(skb->head + NET_SKB_PAD);
		prefetch(skb->head + NET_SKB_PAD + CPU_D_CACHE_LINE_SIZE);
#endif  

		dev = pp->dev;

		atomic_inc(&pool->in_use);
		STAT_DBG(pp->stats.rxq[rxq]++);
		dev->stats.rx_packets++;

		rx_bytes = rx_desc->dataSize - MV_ETH_CRC_SIZE;
		dev->stats.rx_bytes += rx_bytes;

#ifndef CONFIG_MV_ETH_PNC
	 
	if (MV_NETA_PNC_CAP() && NETA_RX_L3_IS_IP4(rx_desc->status)) {
		int ip_offset;

		if ((rx_desc->status & ETH_RX_VLAN_TAGGED_FRAME_MASK))
			ip_offset = MV_ETH_MH_SIZE + sizeof(MV_802_3_HEADER) + MV_VLAN_HLEN;
		else
			ip_offset = MV_ETH_MH_SIZE + sizeof(MV_802_3_HEADER);

		NETA_RX_SET_IPHDR_OFFSET(rx_desc, ip_offset);
		NETA_RX_SET_IPHDR_HDRLEN(rx_desc, 5);
	}
#endif  

#ifdef CONFIG_MV_NETA_DEBUG_CODE
		if (pp->flags & MV_ETH_F_DBG_RX) {
			pr_info("skb=%p, buf=%p, ksize=%d\n", skb, skb->head, ksize(skb->head));
			mvDebugMemDump(skb->head + NET_SKB_PAD, 64, 1);
		}
#endif  

	if (mv_eth_is_swf_recycle())
#if defined(MY_ABC_HERE)
		MV_NETA_SKB_MAGIC_BPID_SET(skb, (MV_NETA_SKB_MAGIC(skb) | pool->pool));
#else
		MV_NETA_SKB_RECYCLE_MAGIC_BPID_SET(skb, (MV_NETA_SKB_RECYCLE_MAGIC(skb) | pool->pool));
#endif  

#if defined(CONFIG_MV_ETH_PNC) && defined(CONFIG_MV_ETH_RX_SPECIAL)
		 
		if (MV_NETA_PNC_CAP() && (rx_desc->pncInfo & NETA_PNC_RX_SPECIAL)) {
			if (pp->rx_special_proc) {
				pp->rx_special_proc(pp->port, rxq, dev, skb, rx_desc);
				STAT_INFO(pp->stats.rx_special++);

				err = mv_eth_refill(pp, rxq, pool, rx_desc);
				if (err) {
					pr_err("Linux processing - Can't refill\n");
#if defined(MY_ABC_HERE)
					atomic_inc(&pp->rxq_ctrl[rxq].missed);
#else  
					pp->rxq_ctrl[rxq].missed++;
#endif  
					rx_filled--;
				}
				continue;
			}
		}
#endif  

		__skb_put(skb, rx_bytes);

#ifdef ETH_SKB_DEBUG
		mv_eth_skb_check(skb);
#endif  

		mv_eth_rx_csum(pp, rx_desc, skb);

		if (pp->tagged) {
			mv_mux_rx(skb, pp->port, napi);
			STAT_DBG(pp->stats.rx_tagged++);
			skb = NULL;
		} else {
			dev->stats.rx_bytes -= mv_eth_mh_skb_skip(skb);
			skb->protocol = eth_type_trans(skb, dev);
		}

#ifdef CONFIG_MV_ETH_GRO
		if (skb && (dev->features & NETIF_F_GRO)) {
			STAT_DBG(pp->stats.rx_gro++);
			STAT_DBG(pp->stats.rx_gro_bytes += skb->len);

			rx_status = napi_gro_receive(pp->cpu_config[smp_processor_id()]->napi, skb);
			skb = NULL;
		}
#endif  

		if (skb) {
			STAT_DBG(pp->stats.rx_netif++);
			rx_status = netif_receive_skb(skb);
			STAT_DBG((rx_status == 0) ? 0 : pp->stats.rx_drop_sw++);
		}

#if defined(MY_ABC_HERE)
		if (mv_eth_pool_bm(pool)) { 
			err = mv_eth_refill(pp, rxq, pool, rx_desc);
			if (err) {
				pr_warn("Linux processing - Can't refill, try to allocate again in cleanup timer\n");
				atomic_inc(&pool->missed);
				 
				mv_eth_add_cleanup_timer(pp->cpu_config[smp_processor_id()]);
			}
		} else { 
			if (!atomic_read(&pp->rxq_ctrl[rxq].refill_stop)) {
				err = mv_eth_refill(pp, rxq, pool, rx_desc);
				if (err) {
					pr_warn("Linux processing - Can't refill, allocate again in cleanup timer\n");
					atomic_inc(&pp->rxq_ctrl[rxq].missed);
					 
					pp->rxq_ctrl[rxq].missed_desc = rx_desc;
					 
					atomic_set(&pp->rxq_ctrl[rxq].refill_stop, 1);
					rx_filled--;
					 
					mv_eth_add_cleanup_timer(pp->cpu_config[smp_processor_id()]);
				}
			} else {
				 
				atomic_inc(&pp->rxq_ctrl[rxq].missed);
				rx_filled--;
			}
		}
#else  
		err = mv_eth_refill(pp, rxq, pool, rx_desc);
		if (err) {
			printk(KERN_ERR "Linux processing - Can't refill\n");
			pp->rxq_ctrl[rxq].missed++;
			mv_eth_add_cleanup_timer(pp->cpu_config[smp_processor_id()]);
			rx_filled--;
		}
#endif  

	}

	mv_neta_wmb();
	mvNetaRxqDescNumUpdate(pp->port, rxq, rx_done, rx_filled);

	return rx_done;
}

static int mv_eth_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct eth_port *pp = MV_ETH_PRIV(dev);
	int frags = 0;
	bool tx_spec_ready = false;
	struct mv_eth_tx_spec tx_spec;
	u32 tx_cmd, skb_len = 0;

	struct tx_queue *txq_ctrl = NULL;
	struct neta_tx_desc *tx_desc;
	unsigned long flags = 0;

	if (!test_bit(MV_ETH_F_STARTED_BIT, &(pp->flags))) {
		STAT_INFO(pp->stats.netdev_stop++);

#ifdef CONFIG_MV_NETA_DEBUG_CODE
		if (pp->flags & MV_ETH_F_DBG_TX)
			printk(KERN_ERR "%s: STARTED_BIT = 0, packet is dropped.\n", __func__);
#endif  
		goto out;
	}

	if (!(netif_running(dev))) {
		printk(KERN_ERR "!netif_running() in %s\n", __func__);
		goto out;
	}

#if defined(CONFIG_MV_ETH_TX_SPECIAL)
	if (pp->tx_special_check) {

		if (pp->tx_special_check(pp->port, dev, skb, &tx_spec)) {
			STAT_INFO(pp->stats.tx_special++);
			if (tx_spec.tx_func) {
				tx_spec.tx_func(skb->data, skb->len, &tx_spec);
				goto out;
			} else {
				 
				tx_spec_ready = true;
			}
		}
	}
#endif  

	if (pp->tagged && (!MV_MUX_SKB_IS_TAGGED(skb))) {
#ifdef CONFIG_MV_NETA_DEBUG_CODE
		if (pp->flags & MV_ETH_F_DBG_TX)
			pr_err("%s: port %d is tagged, skb not from MUX interface - packet is dropped.\n",
				__func__, pp->port);
#endif  

		goto out;
	}

	if (tx_spec_ready == false) {
		tx_spec.txp = pp->txp;
		tx_spec.txq = mv_eth_tx_policy(pp, skb);
		tx_spec.hw_cmd = pp->hw_cmd;
		tx_spec.flags = pp->flags;
	}

	txq_ctrl = &pp->txq_ctrl[tx_spec.txp * CONFIG_MV_ETH_TXQ + tx_spec.txq];
	if (txq_ctrl == NULL) {
		printk(KERN_ERR "%s: invalidate txp/txq (%d/%d)\n", __func__, tx_spec.txp, tx_spec.txq);
		goto out;
	}
	mv_eth_lock(txq_ctrl, flags);

#if defined(MY_ABC_HERE)
#ifdef CONFIG_MV_NETA_DEBUG_CODE
	if (pp->flags & MV_ETH_F_DBG_TX) {
		pr_info("\n%s - eth_tx_%lu: cpu=%d, in_intr=0x%lx, port=%d, txp=%d, txq=%d\n",
		       dev->name, dev->stats.tx_packets, smp_processor_id(),
			in_interrupt(), pp->port, tx_spec.txp, tx_spec.txq);
		mv_eth_skb_print(skb);
		mvDebugMemDump(skb->data, 64, 1);
	}
#endif  
#endif  

#ifdef CONFIG_MV_ETH_TSO
	 
	if (skb_is_gso(skb)) {
		frags = mv_eth_tx_tso(skb, dev, &tx_spec, txq_ctrl);
		goto out;
	}
#endif  

	frags = skb_shinfo(skb)->nr_frags + 1;

	if (tx_spec.flags & MV_ETH_F_MH) {
		if (mv_eth_skb_mh_add(skb, pp->tx_mh)) {
			frags = 0;
			goto out;
		}
	}

	tx_desc = mv_eth_tx_desc_get(txq_ctrl, frags);
	if (tx_desc == NULL) {
		frags = 0;
		goto out;
	}
#if defined(MY_ABC_HERE)
	 
#else
	 
#endif  
	tx_cmd = mv_eth_skb_tx_csum(pp, skb);

#ifdef CONFIG_MV_PON
	tx_desc->hw_cmd = tx_spec.hw_cmd;
#endif

#if defined(MY_ABC_HERE)
	 
#else
	 
#endif  
	tx_desc->dataSize = skb_headlen(skb);

#if defined(MY_ABC_HERE)
	 
#else
	tx_desc->bufPhysAddr = mvOsCacheFlush(pp->dev->dev.parent, skb->data, tx_desc->dataSize);
#endif  

	skb_len = skb->len;

	if (frags == 1) {
#if defined(MY_ABC_HERE)
		 
#else
		 
#endif  
#if defined(CONFIG_MV_ETH_BM_CPU) && defined(CONFIG_MV_NETA_SKB_RECYCLE)
		struct bm_pool *pool = mv_eth_skb_recycle_get_pool(skb);
#if defined(MY_ABC_HERE)
		int headroom = skb_headroom(skb);

		if (pool && (headroom < NETA_TX_PKT_OFFSET_MAX) &&
			(atomic_read(&pool->in_use) > 0) && skb_recycle_check(skb, pool->pkt_size)) {
			 
			tx_cmd |= (NETA_TX_BM_ENABLE_MASK |
#else
		if (pool && (atomic_read(&pool->in_use) > 0) && skb_recycle_check(skb, pool->pkt_size)) {
			 
			tx_cmd |= NETA_TX_BM_ENABLE_MASK |
#endif  
				  NETA_TX_BM_POOL_ID_MASK(pool->pool) |
#if defined(MY_ABC_HERE)
				  NETA_TX_PKT_OFFSET_MASK(headroom));
			txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = 0;
			tx_desc->bufPhysAddr = mvOsCacheFlush(pp->dev->dev.parent, skb->head,
								headroom + tx_desc->dataSize);
#else
				  NETA_TX_PKT_OFFSET_MASK(NET_SKB_PAD + MV_ETH_MH_SIZE);
			txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = NULL;
			tx_desc->bufPhysAddr = virt_to_phys(skb->head);
#endif  

			atomic_dec(&pool->in_use);
			STAT_DBG(pool->stats.skb_recycled_ok++);
		} else {
#if defined(MY_ABC_HERE)
			 
			tx_desc->bufPhysAddr = mvOsCacheFlush(pp->dev->dev.parent, skb->data, tx_desc->dataSize);
#endif  
			txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = ((MV_ULONG) skb | MV_ETH_SHADOW_SKB);
			if (pool && (atomic_read(&pool->in_use) > 0))
				STAT_DBG(pool->stats.skb_recycled_err++);
		}
#else
		txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = ((MV_ULONG) skb | MV_ETH_SHADOW_SKB);
#if defined(MY_ABC_HERE)
		tx_desc->bufPhysAddr = mvOsCacheFlush(pp->dev->dev.parent, skb->data, tx_desc->dataSize);
#endif  
#endif  

		if (tx_spec.flags & MV_ETH_F_NO_PAD)
			tx_cmd |= NETA_TX_F_DESC_MASK | NETA_TX_L_DESC_MASK;
		else
			tx_cmd |= NETA_TX_FLZ_DESC_MASK;

		tx_desc->command = tx_cmd;
		mv_eth_tx_desc_flush(pp, tx_desc);

		mv_eth_shadow_inc_put(txq_ctrl);
	} else {
#if defined(MY_ABC_HERE)
		 
		tx_desc->bufPhysAddr = mvOsCacheFlush(pp->dev->dev.parent, skb->data, tx_desc->dataSize);
#endif  

		tx_cmd |= NETA_TX_F_DESC_MASK;

		txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = 0;
		mv_eth_shadow_inc_put(txq_ctrl);

		tx_desc->command = tx_cmd;
		mv_eth_tx_desc_flush(pp, tx_desc);

		mv_eth_tx_frag_process(pp, skb, txq_ctrl, tx_spec.flags);
		STAT_DBG(pp->stats.tx_sg++);
	}
 
	txq_ctrl->txq_count += frags;

#ifdef CONFIG_MV_NETA_DEBUG_CODE
	if (pp->flags & MV_ETH_F_DBG_TX) {
#if defined(MY_ABC_HERE)
		 
#else
		printk(KERN_ERR "\n");
		printk(KERN_ERR "%s - eth_tx_%lu: cpu=%d, in_intr=0x%lx, port=%d, txp=%d, txq=%d\n",
				dev->name, dev->stats.tx_packets, smp_processor_id(),
				in_interrupt(), pp->port, tx_spec.txp, tx_spec.txq);
		pr_info("\t skb=%p, head=%p, data=%p, size=%d\n", skb, skb->head, skb->data, skb_len);
#endif  
		mv_eth_tx_desc_print(tx_desc);
#if defined(MY_ABC_HERE)
		 
#else
		 
		mvDebugMemDump(skb->data, 64, 1);
#endif  
	}
#endif  

#ifdef CONFIG_MV_PON
	if (MV_PON_PORT(pp->port))
		mvNetaPonTxqBytesAdd(pp->port, tx_spec.txp, tx_spec.txq, skb_len);
#endif  

	mv_neta_wmb();
	mvNetaTxqPendDescAdd(pp->port, tx_spec.txp, tx_spec.txq, frags);

	STAT_DBG(txq_ctrl->stats.txq_tx += frags);

out:
	if (frags > 0) {
		dev->stats.tx_packets++;
		dev->stats.tx_bytes += skb->len;
	} else {
		dev->stats.tx_dropped++;
		dev_kfree_skb_any(skb);
	}

#ifndef CONFIG_MV_NETA_TXDONE_ISR
	if (txq_ctrl) {
		if (txq_ctrl->txq_count >= mv_ctrl_txdone) {
			u32 tx_done = mv_eth_txq_done(pp, txq_ctrl);
#if defined(MY_ABC_HERE) && !defined(CONFIG_MV_ETH_STAT_DIST)
			 
#else  
			STAT_DIST((tx_done < pp->dist_stats.tx_done_dist_size) ? pp->dist_stats.tx_done_dist[tx_done]++ : 0);
#endif  
		}
		 
		if ((txq_ctrl->txq_count > 0)  && (txq_ctrl->txq_count <= frags) && (frags > 0)) {
			struct cpu_ctrl *cpuCtrl = pp->cpu_config[smp_processor_id()];

			mv_eth_add_tx_done_timer(cpuCtrl);
		}
	}
#endif  

	if (txq_ctrl)
		mv_eth_unlock(txq_ctrl, flags);

	return NETDEV_TX_OK;
}

#ifdef CONFIG_MV_ETH_TSO
 
static inline int mv_eth_tso_validate(struct sk_buff *skb, struct net_device *dev)
{
	if (!(dev->features & NETIF_F_TSO)) {
		printk(KERN_ERR "error: (skb_is_gso(skb) returns true but features is not NETIF_F_TSO\n");
		return 1;
	}

	if (skb_shinfo(skb)->frag_list != NULL) {
		printk(KERN_ERR "***** ERROR: frag_list is not null\n");
		return 1;
	}

	if (skb_shinfo(skb)->gso_segs == 1) {
		printk(KERN_ERR "***** ERROR: only one TSO segment\n");
		return 1;
	}

	if (skb->len <= skb_shinfo(skb)->gso_size) {
		printk(KERN_ERR "***** ERROR: total_len (%d) less than gso_size (%d)\n", skb->len, skb_shinfo(skb)->gso_size);
		return 1;
	}
	if ((htons(ETH_P_IP) != skb->protocol) || (ip_hdr(skb)->protocol != IPPROTO_TCP) || (tcp_hdr(skb) == NULL)) {
		printk(KERN_ERR "***** ERROR: Protocol is not TCP over IP\n");
		return 1;
	}
	return 0;
}

static inline int mv_eth_tso_build_hdr_desc(struct eth_port *pp, struct neta_tx_desc *tx_desc,
					     struct sk_buff *skb,
					     struct tx_queue *txq_ctrl, u16 *mh, int hdr_len, int size,
					     MV_U32 tcp_seq, MV_U16 ip_id, int left_len)
{
	struct iphdr *iph;
	struct tcphdr *tcph;
	MV_U8 *data, *mac;
	int mac_hdr_len = skb_network_offset(skb);

	data = mv_eth_extra_pool_get(pp);
	if (!data)
		return 0;

	txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = ((MV_ULONG)data | MV_ETH_SHADOW_EXT);

	mac = data + MV_ETH_MH_SIZE;
	iph = (struct iphdr *)(mac + mac_hdr_len);

	memcpy(mac, skb->data, hdr_len);

	if (iph) {
		iph->id = htons(ip_id);
		iph->tot_len = htons(size + hdr_len - mac_hdr_len);
	}

	tcph = (struct tcphdr *)(mac + skb_transport_offset(skb));
	tcph->seq = htonl(tcp_seq);

	if (left_len) {
		 
		tcph->psh = 0;
		tcph->fin = 0;
		tcph->rst = 0;
	}

	if (mh) {
		 
		*((MV_U16 *)data) = *mh;
		 
		mac_hdr_len += MV_ETH_MH_SIZE;
		hdr_len += MV_ETH_MH_SIZE;
	} else {
		 
		data = mac;
	}

	tx_desc->dataSize = hdr_len;
	tx_desc->command = mvNetaTxqDescCsum(mac_hdr_len, skb->protocol, ((u8 *)tcph - (u8 *)iph) >> 2, IPPROTO_TCP);
	tx_desc->command |= NETA_TX_F_DESC_MASK;

	tx_desc->bufPhysAddr = mvOsCacheFlush(pp->dev->dev.parent, data, tx_desc->dataSize);
	mv_eth_shadow_inc_put(txq_ctrl);

	mv_eth_tx_desc_flush(pp, tx_desc);

	return hdr_len;
}

static inline int mv_eth_tso_build_data_desc(struct eth_port *pp, struct neta_tx_desc *tx_desc, struct sk_buff *skb,
#if defined(MY_ABC_HERE)
					     struct tx_queue *txq_ctrl, struct page *frag_page, int page_offset,
#else  
					     struct tx_queue *txq_ctrl, char *frag_ptr,
#endif  
					     int frag_size, int data_left, int total_left)
{
	int size;

	size = MV_MIN(frag_size, data_left);

	tx_desc->dataSize = size;
#if defined(MY_ABC_HERE)
	tx_desc->bufPhysAddr = mvOsCachePageFlush(pp->dev->dev.parent, frag_page, page_offset, size);
#else  
	tx_desc->bufPhysAddr = mvOsCacheFlush(pp->dev->dev.parent, frag_ptr, size);
#endif  
	tx_desc->command = 0;
	txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = 0;

	if (size == data_left) {
		 
		tx_desc->command = NETA_TX_L_DESC_MASK;

		if (total_left == 0) {
			 
			txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = ((MV_ULONG) skb | MV_ETH_SHADOW_SKB);
		}
	}
	mv_eth_shadow_inc_put(txq_ctrl);
	mv_eth_tx_desc_flush(pp, tx_desc);

	return size;
}

int mv_eth_tx_tso(struct sk_buff *skb, struct net_device *dev,
		struct mv_eth_tx_spec *tx_spec, struct tx_queue *txq_ctrl)
{
	int frag = 0;
	int total_len, hdr_len, size, frag_size, data_left;
#if defined(MY_ABC_HERE)
	 
#else  
	char *frag_ptr;
#endif  
	int totalDescNum, totalBytes = 0;
	struct neta_tx_desc *tx_desc;
	MV_U16 ip_id;
	MV_U32 tcp_seq = 0;
#if defined(MY_ABC_HERE)
	struct page *frag_page = NULL;
	MV_U32 frag_offset = 0;
#endif  
	skb_frag_t *skb_frag_ptr;
	const struct tcphdr *th = tcp_hdr(skb);
	struct eth_port *priv = MV_ETH_PRIV(dev);
	MV_U16 *mh = NULL;
	int i;

	STAT_DBG(priv->stats.tx_tso++);
 
	if (mv_eth_tso_validate(skb, dev))
		return 0;

	totalDescNum = skb_shinfo(skb)->gso_segs * 2 + skb_shinfo(skb)->nr_frags;

	if ((txq_ctrl->txq_count + totalDescNum) >= txq_ctrl->txq_size) {
 
		STAT_ERR(txq_ctrl->stats.txq_err++);
		return 0;
	}

	total_len = skb->len;
	hdr_len = (skb_transport_offset(skb) + tcp_hdrlen(skb));

	total_len -= hdr_len;
	ip_id = ntohs(ip_hdr(skb)->id);
	tcp_seq = ntohl(th->seq);

	frag_size = skb_headlen(skb);
#if defined(MY_ABC_HERE)
	frag_page = virt_to_page(skb->data);
	frag_offset = offset_in_page(skb->data);
#else  
	frag_ptr = skb->data;
#endif  

	if (frag_size < hdr_len) {
		printk(KERN_ERR "***** ERROR: frag_size=%d, hdr_len=%d\n", frag_size, hdr_len);
		return 0;
	}

	frag_size -= hdr_len;
#if defined(MY_ABC_HERE)
	frag_offset += hdr_len;
#else  
	frag_ptr += hdr_len;
#endif  
	if (frag_size == 0) {
		skb_frag_ptr = &skb_shinfo(skb)->frags[frag];

		frag_size = skb_frag_ptr->size;
#if defined(MY_ABC_HERE)
		frag_offset = skb_frag_ptr->page_offset;
#endif  
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 1, 10)
#if defined(MY_ABC_HERE)
		frag_page = skb_frag_ptr->page.p;
#else  
		frag_ptr = page_address(skb_frag_ptr->page.p) + skb_frag_ptr->page_offset;
#endif  
#else
#if defined(MY_ABC_HERE)
		frag_page = skb_frag_ptr->page;
#else  
		frag_ptr = page_address(skb_frag_ptr->page) + skb_frag_ptr->page_offset;
#endif  
#endif
		frag++;
	}
	totalDescNum = 0;

	while (total_len > 0) {
		data_left = MV_MIN(skb_shinfo(skb)->gso_size, total_len);

		tx_desc = mv_eth_tx_desc_get(txq_ctrl, 1);
		if (tx_desc == NULL)
			goto outNoTxDesc;

		totalDescNum++;
		total_len -= data_left;
		txq_ctrl->txq_count++;

		if (tx_spec->flags & MV_ETH_F_MH)
			mh = &priv->tx_mh;
		 
		size = mv_eth_tso_build_hdr_desc(priv, tx_desc, skb, txq_ctrl, mh,
					hdr_len, data_left, tcp_seq, ip_id, total_len);
		if (size == 0)
			goto outNoTxDesc;

		totalBytes += size;
 
		ip_id++;

		while (data_left > 0) {
			tx_desc = mv_eth_tx_desc_get(txq_ctrl, 1);
			if (tx_desc == NULL)
				goto outNoTxDesc;

			totalDescNum++;
			txq_ctrl->txq_count++;

			size = mv_eth_tso_build_data_desc(priv, tx_desc, skb, txq_ctrl,
#if defined(MY_ABC_HERE)
							  frag_page, frag_offset, frag_size, data_left, total_len);
#else  
							  frag_ptr, frag_size, data_left, total_len);
#endif  
			totalBytes += size;
 
			data_left -= size;
			tcp_seq += size;

			frag_size -= size;
#if defined(MY_ABC_HERE)
			frag_offset += size;
#else  
			frag_ptr += size;
#endif  

			if ((frag_size == 0) && (frag < skb_shinfo(skb)->nr_frags)) {
				skb_frag_ptr = &skb_shinfo(skb)->frags[frag];

				frag_size = skb_frag_ptr->size;
#if defined(MY_ABC_HERE)
				frag_offset = skb_frag_ptr->page_offset;
#endif  
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 1, 10)
#if defined(MY_ABC_HERE)
				frag_page = skb_frag_ptr->page.p;
#else  
				frag_ptr = page_address(skb_frag_ptr->page.p) + skb_frag_ptr->page_offset;
#endif  
#else
#if defined(MY_ABC_HERE)
				frag_page = skb_frag_ptr->page;
#else  
				frag_ptr = page_address(skb_frag_ptr->page) + skb_frag_ptr->page_offset;
#endif  
#endif
				frag++;
			}
		}		 
	}			 

#ifdef CONFIG_MV_PON
	if (MV_PON_PORT(priv->port))
		mvNetaPonTxqBytesAdd(priv->port, txq_ctrl->txp, txq_ctrl->txq, totalBytes);
#endif  

	STAT_DBG(priv->stats.tx_tso_bytes += totalBytes);
	STAT_DBG(txq_ctrl->stats.txq_tx += totalDescNum);

	mv_neta_wmb();
	mvNetaTxqPendDescAdd(priv->port, txq_ctrl->txp, txq_ctrl->txq, totalDescNum);
 
	return totalDescNum;

outNoTxDesc:
	 
	printk(KERN_ERR "%s: No TX descriptors - rollback %d, txq_count=%d, nr_frags=%d, skb=%p, len=%d, gso_segs=%d\n",
			__func__, totalDescNum, txq_ctrl->txq_count, skb_shinfo(skb)->nr_frags,
			skb, skb->len, skb_shinfo(skb)->gso_segs);

	for (i = 0; i < totalDescNum; i++) {
		txq_ctrl->txq_count--;
		mv_eth_shadow_dec_put(txq_ctrl);
		mvNetaTxqPrevDescGet(txq_ctrl->q);
	}
	return MV_OK;
}
#endif  

static void mv_eth_rxq_drop_pkts(struct eth_port *pp, int rxq)
{
	struct neta_rx_desc *rx_desc;
	struct sk_buff *skb;
	struct bm_pool      *pool;
	int	                rx_done, i, pool_id;
	MV_NETA_RXQ_CTRL    *rx_ctrl = pp->rxq_ctrl[rxq].q;

	if (rx_ctrl == NULL)
		return;

	rx_done = mvNetaRxqBusyDescNumGet(pp->port, rxq);
	mvOsCacheIoSync(pp->dev->dev.parent);

	for (i = 0; i < rx_done; i++) {
		rx_desc = mvNetaRxqNextDescGet(rx_ctrl);
		mvOsCacheLineInv(pp->dev->dev.parent, rx_desc);

#if defined(MV_CPU_BE)
		mvNetaRxqDescSwap(rx_desc);
#endif  

		skb = (struct sk_buff *)rx_desc->bufCookie;
		pool_id = NETA_RX_GET_BPID(rx_desc);
		pool = &mv_eth_pool[pool_id];
		mv_eth_rxq_refill(pp, rxq, pool, skb, rx_desc);
	}
	if (rx_done) {
		mv_neta_wmb();
		mvNetaRxqDescNumUpdate(pp->port, rxq, rx_done, rx_done);
	}
}

static void mv_eth_txq_done_force(struct eth_port *pp, struct tx_queue *txq_ctrl)
{
	int tx_done = txq_ctrl->txq_count;

	mv_eth_txq_bufs_free(pp, txq_ctrl, tx_done);

	txq_ctrl->txq_count -= tx_done;
	STAT_DBG(txq_ctrl->stats.txq_txdone += tx_done);
}

inline u32 mv_eth_tx_done_pon(struct eth_port *pp, int *tx_todo)
{
	int txp, txq;
	struct tx_queue *txq_ctrl;
	unsigned long flags = 0;

	u32 tx_done = 0;

	*tx_todo = 0;

	STAT_INFO(pp->stats.tx_done++);

	txp = pp->txp_num;
	while (txp--) {
		txq = CONFIG_MV_ETH_TXQ;

		while (txq--) {
			txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];
			mv_eth_lock(txq_ctrl, flags);
			if ((txq_ctrl) && (txq_ctrl->txq_count)) {
				tx_done += mv_eth_txq_done(pp, txq_ctrl);
				*tx_todo += txq_ctrl->txq_count;
			}
			mv_eth_unlock(txq_ctrl, flags);
		}
	}

	STAT_DIST((tx_done < pp->dist_stats.tx_done_dist_size) ? pp->dist_stats.tx_done_dist[tx_done]++ : 0);

	return tx_done;
}

inline u32 mv_eth_tx_done_gbe(struct eth_port *pp, u32 cause_tx_done, int *tx_todo)
{
	int txq;
	struct tx_queue *txq_ctrl;
	unsigned long flags = 0;
	u32 tx_done = 0;

	*tx_todo = 0;

	STAT_INFO(pp->stats.tx_done++);

	while (cause_tx_done != 0) {

		txq = mv_eth_tx_done_policy(cause_tx_done);

		if (txq == -1)
			break;

		txq_ctrl = &pp->txq_ctrl[txq];

		if (txq_ctrl == NULL) {
			printk(KERN_ERR "%s: txq_ctrl = NULL, txq=%d\n", __func__, txq);
			return -EINVAL;
		}

		mv_eth_lock(txq_ctrl, flags);

		if ((txq_ctrl) && (txq_ctrl->txq_count)) {
			tx_done += mv_eth_txq_done(pp, txq_ctrl);
			*tx_todo += txq_ctrl->txq_count;
		}
		cause_tx_done &= ~((1 << txq) << NETA_CAUSE_TXQ_SENT_DESC_OFFS);

		mv_eth_unlock(txq_ctrl, flags);
	}

	STAT_DIST((tx_done < pp->dist_stats.tx_done_dist_size) ? pp->dist_stats.tx_done_dist[tx_done]++ : 0);

	return tx_done;
}

static void mv_eth_tx_frag_process(struct eth_port *pp, struct sk_buff *skb, struct tx_queue *txq_ctrl,	u16 flags)
{
	int i;
	struct neta_tx_desc *tx_desc;

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i];

		tx_desc = mvNetaTxqNextDescGet(txq_ctrl->q);

		tx_desc->dataSize = frag->size;
		tx_desc->bufPhysAddr =
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 1, 10)
#if defined(MY_ABC_HERE)
			mvOsCachePageFlush(pp->dev->dev.parent, frag->page.p, frag->page_offset, tx_desc->dataSize);
#else  
			mvOsCacheFlush(pp->dev->dev.parent, page_address(frag->page.p) + frag->page_offset,
			tx_desc->dataSize);
#endif  
#else
#if defined(MY_ABC_HERE)
			mvOsCachePageFlush(pp->dev->dev.parent, frag->page, frag->page_offset, tx_desc->dataSize);
#else  
			mvOsCacheFlush(pp->dev->dev.parent, page_address(frag->page) + frag->page_offset,
			tx_desc->dataSize);
#endif  
#endif
		if (i == (skb_shinfo(skb)->nr_frags - 1)) {
			 
			if (flags & MV_ETH_F_NO_PAD)
				tx_desc->command = NETA_TX_L_DESC_MASK;
			else
				tx_desc->command = (NETA_TX_L_DESC_MASK | NETA_TX_Z_PAD_MASK);

			txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = ((MV_ULONG) skb | MV_ETH_SHADOW_SKB);
			mv_eth_shadow_inc_put(txq_ctrl);
		} else {
			 
			tx_desc->command = 0;

			txq_ctrl->shadow_txq[txq_ctrl->shadow_txq_put_i] = 0;
			mv_eth_shadow_inc_put(txq_ctrl);
		}

		mv_eth_tx_desc_flush(pp, tx_desc);
	}
}

static int mv_eth_port_pools_free(int port)
{
	struct eth_port *pp;

	pp = mv_eth_port_by_id(port);
	if (!pp)
		return MV_OK;

	if (pp->pool_long) {
		mv_eth_pool_free(pp->pool_long->pool, pp->pool_long_num);
#ifndef CONFIG_MV_ETH_BM_CPU
	}
#else
		if (MV_NETA_BM_CAP()) {
			if (pp->pool_long->buf_num == 0)
				mvNetaBmPoolDisable(pp->pool_long->pool);

			if (pp->pool_short && (pp->pool_long->pool != pp->pool_short->pool)) {
				mv_eth_pool_free(pp->pool_short->pool, pp->pool_short_num);
				if (pp->pool_short->buf_num == 0)
					mvNetaBmPoolDisable(pp->pool_short->pool);
			}
		}
	}
#endif  
	return MV_OK;
}

static int mv_eth_pool_free(int pool, int num)
{
	struct sk_buff *skb;
	int i = 0;
	struct bm_pool *ppool = &mv_eth_pool[pool];
	unsigned long flags = 0;
	bool free_all = false;

	MV_ETH_LOCK(&ppool->lock, flags);

	if (num >= ppool->buf_num) {
		 
		free_all = true;
		num = ppool->buf_num;
	}

#ifdef CONFIG_MV_ETH_BM_CPU
	if (mv_eth_pool_bm(ppool)) {

		if (free_all)
			mvNetaBmConfigSet(MV_BM_EMPTY_LIMIT_MASK);

		while (i < num) {
			MV_U32 *va;
			MV_U32 pa = mvBmPoolGet(pool);

			if (pa == 0)
				break;

			va = phys_to_virt(pa);
			skb = (struct sk_buff *)(*va);
#if !defined(CONFIG_MV_ETH_BE_WA)
			skb = (struct sk_buff *)MV_32BIT_LE((MV_U32)skb);
#endif  

			if (skb) {
				dev_kfree_skb_any(skb);
#ifdef ETH_SKB_DEBUG
				mv_eth_skb_check(skb);
#endif  
			}
			i++;
		}
		printk(KERN_ERR "bm pool #%d: pkt_size=%d, buf_size=%d - %d of %d buffers free\n",
			pool, ppool->pkt_size, RX_BUF_SIZE(ppool->pkt_size), i, num);

		if (free_all)
			mvNetaBmConfigClear(MV_BM_EMPTY_LIMIT_MASK);
	}
#endif  

	ppool->buf_num -= num;
#ifdef CONFIG_MV_ETH_BM
	mvNetaBmPoolBufNumUpdate(pool, num, 0);
#endif
	 
	if (free_all)
		num = mvStackIndex(ppool->stack);
	else if (mv_eth_pool_bm(ppool))
		num = 0;

	i = 0;
	while (i < num) {
		 
		if (mvStackIndex(ppool->stack) == 0) {
			printk(KERN_ERR "%s: No more buffers in the stack\n", __func__);
			break;
		}
		skb = (struct sk_buff *)mvStackPop(ppool->stack);
		if (skb) {
			dev_kfree_skb_any(skb);

#ifdef ETH_SKB_DEBUG
			mv_eth_skb_check(skb);
#endif  
		}
		i++;
	}
	if (i > 0)
		printk(KERN_ERR "stack pool #%d: pkt_size=%d, buf_size=%d - %d of %d buffers free\n",
			pool, ppool->pkt_size, RX_BUF_SIZE(ppool->pkt_size), i, num);

	MV_ETH_UNLOCK(&ppool->lock, flags);

	return i;
}

static int mv_eth_pool_destroy(int pool)
{
	int num, status = 0;
	struct bm_pool *ppool = &mv_eth_pool[pool];

	num = mv_eth_pool_free(pool, ppool->buf_num);
	if (num != ppool->buf_num) {
		printk(KERN_ERR "Warning: could not free all buffers in pool %d while destroying pool\n", pool);
		return MV_ERROR;
	}

	status = mvStackDelete(ppool->stack);

#ifdef CONFIG_MV_ETH_BM_CPU
	if (MV_NETA_BM_CAP()) {
		mvNetaBmPoolDisable(pool);

		if (ppool->bm_pool)
			mvOsFree(ppool->bm_pool);
	}
#endif  

	memset(ppool, 0, sizeof(struct bm_pool));

	return status;
}

static int mv_eth_pool_add(struct eth_port *pp, int pool, int buf_num)
{
	struct bm_pool *bm_pool;
	struct sk_buff *skb;
	phys_addr_t pa;
	int i;
	unsigned long flags = 0;

	if ((pool < 0) || (pool >= MV_ETH_BM_POOLS)) {
		printk(KERN_ERR "%s: invalid pool number %d\n", __func__, pool);
		return 0;
	}

	bm_pool = &mv_eth_pool[pool];

	if (bm_pool->pkt_size == 0) {
		printk(KERN_ERR "%s: invalid pool #%d state: pkt_size=%d, buf_size=%d, buf_num=%d\n",
		       __func__, pool, bm_pool->pkt_size, RX_BUF_SIZE(bm_pool->pkt_size), bm_pool->buf_num);
		return 0;
	}

	if ((buf_num < 0) || ((buf_num + bm_pool->buf_num) > (bm_pool->capacity))) {

		printk(KERN_ERR "%s: can't add %d buffers into bm_pool=%d: capacity=%d, buf_num=%d\n",
		       __func__, buf_num, pool, bm_pool->capacity, bm_pool->buf_num);
		return 0;
	}

	MV_ETH_LOCK(&bm_pool->lock, flags);

	for (i = 0; i < buf_num; i++) {
		skb = mv_eth_skb_alloc(pp, bm_pool, &pa, GFP_KERNEL);
		if (!skb)
			break;
 
#ifdef CONFIG_MV_ETH_BM_CPU
		if (MV_NETA_BM_CAP()) {
			 
			mvBmPoolPut(pool, (MV_ULONG)pa);
			STAT_DBG(bm_pool->stats.bm_put++);
		} else {
			mvStackPush(bm_pool->stack, (MV_U32)skb);
			STAT_DBG(bm_pool->stats.stack_put++);
		}
#else
		mvStackPush(bm_pool->stack, (MV_U32)skb);
		STAT_DBG(bm_pool->stats.stack_put++);
#endif  
	}
	bm_pool->buf_num += i;
	bm_pool->in_use_thresh = bm_pool->buf_num / 4;
#ifdef CONFIG_MV_ETH_BM
	mvNetaBmPoolBufNumUpdate(pool, i, 1);
#endif
	printk(KERN_ERR "pool #%d: pkt_size=%d, buf_size=%d - %d of %d buffers added\n",
	       pool, bm_pool->pkt_size, RX_BUF_SIZE(bm_pool->pkt_size), i, buf_num);

	MV_ETH_UNLOCK(&bm_pool->lock, flags);

	return i;
}

#ifdef CONFIG_MV_ETH_BM
void	*mv_eth_bm_pool_create(int pool, int capacity, MV_ULONG *pPhysAddr)
{
		MV_ULONG			physAddr;
		MV_UNIT_WIN_INFO	winInfo;
		void				*pVirt;
		MV_STATUS			status;

#if defined(MY_ABC_HERE)
		pVirt = mvOsIoUncachedMalloc(neta_global_dev->parent, sizeof(MV_U32) * capacity, &physAddr, NULL);
#else  
		pVirt = mvOsIoUncachedMalloc(NULL, sizeof(MV_U32) * capacity, &physAddr, NULL);
#endif  
		if (pVirt == NULL) {
			mvOsPrintf("%s: Can't allocate %d bytes for Long pool #%d\n",
					__func__, MV_BM_POOL_CAP_MAX * sizeof(MV_U32), pool);
			return NULL;
		}

		if (MV_IS_NOT_ALIGN((unsigned)pVirt, MV_BM_POOL_PTR_ALIGN)) {
			mvOsPrintf("memory allocated for BM pool #%d is not %d bytes aligned\n",
						pool, MV_BM_POOL_PTR_ALIGN);
			mvOsIoCachedFree(NULL, sizeof(MV_U32) * capacity, physAddr, pVirt, 0);
			return NULL;
		}
		status = mvNetaBmPoolInit(pool, pVirt, physAddr, capacity);
		if (status != MV_OK) {
			mvOsPrintf("%s: Can't init #%d BM pool. status=%d\n", __func__, pool, status);
			mvOsIoCachedFree(NULL, sizeof(MV_U32) * capacity, physAddr, pVirt, 0);
			return NULL;
		}
#ifdef CONFIG_ARCH_MVEBU
		status = mvebu_mbus_get_addr_win_info(physAddr, &winInfo.targetId, &winInfo.attrib);
#else
		status = mvCtrlAddrWinInfoGet(&winInfo, physAddr);
#endif
		if (status != MV_OK) {
			printk(KERN_ERR "%s: Can't map BM pool #%d. phys_addr=0x%x, status=%d\n",
			       __func__, pool, (unsigned)physAddr, status);
			mvOsIoCachedFree(NULL, sizeof(MV_U32) * capacity, physAddr, pVirt, 0);
			return NULL;
		}
		mvNetaBmPoolTargetSet(pool, winInfo.targetId, winInfo.attrib);
		mvNetaBmPoolEnable(pool);

		if (pPhysAddr != NULL)
			*pPhysAddr = physAddr;

		return pVirt;
}
#endif  

static MV_STATUS mv_eth_pool_create(int pool, int capacity)
{
	struct bm_pool *bm_pool;

	if ((pool < 0) || (pool >= MV_ETH_BM_POOLS)) {
		printk(KERN_ERR "%s: pool=%d is out of range\n", __func__, pool);
		return MV_BAD_VALUE;
	}

	bm_pool = &mv_eth_pool[pool];
	memset(bm_pool, 0, sizeof(struct bm_pool));

#ifdef CONFIG_MV_ETH_BM_CPU
	if (MV_NETA_BM_CAP()) {
		bm_pool->bm_pool = mv_eth_bm_pool_create(pool, capacity, &bm_pool->physAddr);
		if (bm_pool->bm_pool == NULL)
			return MV_FAIL;
	}
#endif  

	bm_pool->stack = mvStackCreate(capacity);

	if (bm_pool->stack == NULL) {
		printk(KERN_ERR "Can't create MV_STACK structure for %d elements\n", capacity);
		return MV_OUT_OF_CPU_MEM;
	}

	bm_pool->pool = pool;
	bm_pool->capacity = capacity;
	bm_pool->pkt_size = 0;
	bm_pool->buf_num = 0;
	atomic_set(&bm_pool->in_use, 0);
	spin_lock_init(&bm_pool->lock);
#if defined(MY_ABC_HERE)
	atomic_set(&bm_pool->missed, 0);
#endif  

	return MV_OK;
}

irqreturn_t mv_eth_isr(int irq, void *dev_id)
{
	struct eth_port *pp = (struct eth_port *)dev_id;
	int cpu = smp_processor_id();
	struct napi_struct *napi = pp->cpu_config[cpu]->napi;
	u32 regVal;

#ifdef CONFIG_MV_NETA_DEBUG_CODE
	if (pp->flags & MV_ETH_F_DBG_ISR) {
		printk(KERN_ERR "%s: port=%d, cpu=%d, mask=0x%x, cause=0x%x\n",
			__func__, pp->port, cpu,
			MV_REG_READ(NETA_INTR_NEW_MASK_REG(pp->port)), MV_REG_READ(NETA_INTR_NEW_CAUSE_REG(pp->port)));
	}
#endif  

	STAT_INFO(pp->stats.irq[cpu]++);

	MV_REG_WRITE(NETA_INTR_NEW_MASK_REG(pp->port), 0);
	 
	if (napi_schedule_prep(napi)) {
		 
		__napi_schedule(napi);
	} else {
		STAT_INFO(pp->stats.irq_err[cpu]++);
#ifdef CONFIG_MV_NETA_DEBUG_CODE
		pr_warning("%s: IRQ=%d, port=%d, cpu=%d - NAPI already scheduled\n",
			__func__, irq, pp->port, cpu);
#endif  
	}

	if ((pp->plat_data->ctrl_model & ~0xFF) == MV_88F68XX)
		regVal = MV_REG_READ(NETA_INTR_NEW_MASK_REG(pp->port));

	return IRQ_HANDLED;
}

int mv_eth_hwf_ctrl(int port, MV_BOOL enable)
{
#ifdef CONFIG_MV_ETH_HWF
	int txp, txq, rx_port, mode;
	struct eth_port *pp;

	if (!MV_NETA_HWF_CAP())
		return 0;

	if (mvNetaPortCheck(port))
		return -EINVAL;

	pp = mv_eth_port_by_id(port);
	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return -ENODEV;
	}

	for (txp = 0; txp < pp->txp_num; txp++) {
		for (txq = 0; txq < CONFIG_MV_ETH_TXQ; txq++) {
			mode = mv_eth_ctrl_txq_mode_get(port, txp, txq, &rx_port);
			if (mode == MV_ETH_TXQ_HWF)
				mvNetaHwfTxqEnable(rx_port, port, txp, txq, enable);
		}
	}
#endif  
	return 0;
}

void mv_eth_link_event(struct eth_port *pp, int print)
{
	struct net_device *dev = pp->dev;
	bool              link_is_up;

	STAT_INFO(pp->stats.link++);

#ifdef CONFIG_MV_PON
	if (MV_PON_PORT(pp->port))
		link_is_up = mv_pon_link_status();
	else
#endif  
		link_is_up = mvNetaLinkIsUp(pp->port);

	if (link_is_up) {
		mvNetaPortUp(pp->port);
		set_bit(MV_ETH_F_LINK_UP_BIT, &(pp->flags));

		if (mv_eth_ctrl_is_tx_enabled(pp)) {
			if (dev) {
				netif_carrier_on(dev);
				netif_tx_wake_all_queues(dev);
			}
		}
		mv_eth_hwf_ctrl(pp->port, MV_TRUE);
	} else {
		mv_eth_hwf_ctrl(pp->port, MV_FALSE);

		if (dev) {
			netif_carrier_off(dev);
			netif_tx_stop_all_queues(dev);
		}
		mvNetaPortDown(pp->port);
		clear_bit(MV_ETH_F_LINK_UP_BIT, &(pp->flags));
	}

	if (print) {
#ifdef MY_ABC_HERE
		if (dev)
			printk(KERN_INFO "%s: ", dev->name);
		else
			printk(KERN_INFO "%s: ", "none");
#else
		if (dev)
			printk(KERN_ERR "%s: ", dev->name);
		else
			printk(KERN_ERR "%s: ", "none");
#endif  

		mv_eth_link_status_print(pp->port);
	}
}

int mv_eth_poll(struct napi_struct *napi, int budget)
{
	int rx_done = 0;
	MV_U32 causeRxTx;
	struct eth_port *pp = MV_ETH_PRIV(napi->dev);
	struct cpu_ctrl *cpuCtrl = pp->cpu_config[smp_processor_id()];

#ifdef CONFIG_MV_NETA_DEBUG_CODE
	if (pp->flags & MV_ETH_F_DBG_POLL) {
		printk(KERN_ERR "%s ENTER: port=%d, cpu=%d, mask=0x%x, cause=0x%x\n",
			__func__, pp->port, smp_processor_id(),
			MV_REG_READ(NETA_INTR_NEW_MASK_REG(pp->port)), MV_REG_READ(NETA_INTR_NEW_CAUSE_REG(pp->port)));
	}
#endif  

	if (!test_bit(MV_ETH_F_STARTED_BIT, &(pp->flags))) {
		STAT_INFO(pp->stats.netdev_stop++);

#ifdef CONFIG_MV_NETA_DEBUG_CODE
		if (pp->flags & MV_ETH_F_DBG_RX)
			printk(KERN_ERR "%s: STARTED_BIT = 0, poll completed.\n", __func__);
#endif  

		napi_complete(napi);
		STAT_INFO(pp->stats.poll_exit[smp_processor_id()]++);
		return rx_done;
	}

	STAT_INFO(pp->stats.poll[smp_processor_id()]++);

	causeRxTx = MV_REG_READ(NETA_INTR_NEW_CAUSE_REG(pp->port)) &
	    (MV_ETH_MISC_SUM_INTR_MASK | MV_ETH_TXDONE_INTR_MASK | MV_ETH_RX_INTR_MASK);

	if (causeRxTx & MV_ETH_MISC_SUM_INTR_MASK) {
		MV_U32 causeMisc;

		causeRxTx &= ~MV_ETH_MISC_SUM_INTR_MASK;
		causeMisc = MV_REG_READ(NETA_INTR_MISC_CAUSE_REG(pp->port));

		if (causeMisc & NETA_CAUSE_LINK_CHANGE_MASK)
			mv_eth_link_event(pp, 1);

		MV_REG_WRITE(NETA_INTR_MISC_CAUSE_REG(pp->port), 0);
	}
	causeRxTx |= cpuCtrl->causeRxTx;

#ifdef CONFIG_MV_NETA_TXDONE_ISR
	if (causeRxTx & MV_ETH_TXDONE_INTR_MASK) {
		int tx_todo = 0;
		 
		if (MV_PON_PORT(pp->port))
			mv_eth_tx_done_pon(pp, &tx_todo);
		else
			mv_eth_tx_done_gbe(pp, (causeRxTx & MV_ETH_TXDONE_INTR_MASK), &tx_todo);

		causeRxTx &= ~MV_ETH_TXDONE_INTR_MASK;
	}
#endif  

#if (CONFIG_MV_ETH_RXQ > 1)
	while ((causeRxTx != 0) && (budget > 0)) {
		int count, rx_queue;

		rx_queue = mv_eth_rx_policy(causeRxTx);
		if (rx_queue == -1)
			break;

		count = mv_eth_rx(pp, budget, rx_queue, napi);
		rx_done += count;
		budget -= count;
		if (budget > 0)
			causeRxTx &= ~((1 << rx_queue) << NETA_CAUSE_RXQ_OCCUP_DESC_OFFS);
	}
#else
	rx_done = mv_eth_rx(pp, budget, CONFIG_MV_ETH_RXQ_DEF, napi);
	budget -= rx_done;
#endif  

	if (pp->rx_adaptive_coal_cfg)
		pp->rx_rate_pkts += rx_done;

	STAT_DIST((rx_done < pp->dist_stats.rx_dist_size) ? pp->dist_stats.rx_dist[rx_done]++ : 0);

#ifdef CONFIG_MV_NETA_DEBUG_CODE
	if (pp->flags & MV_ETH_F_DBG_POLL) {
		printk(KERN_ERR "%s  EXIT: port=%d, cpu=%d, budget=%d, rx_done=%d\n",
			__func__, pp->port, smp_processor_id(), budget, rx_done);
	}
#endif  

	if (budget > 0) {
		unsigned long flags;

		causeRxTx = 0;

		napi_complete(napi);

		STAT_INFO(pp->stats.poll_exit[smp_processor_id()]++);

		if (pp->rx_adaptive_coal_cfg)
			mv_eth_adaptive_rx_update(pp);

		if (!(pp->flags & MV_ETH_F_IFCAP_NETMAP)) {
			local_irq_save(flags);

			mv_neta_wmb();
			MV_REG_WRITE(NETA_INTR_NEW_MASK_REG(pp->port),
			     (MV_ETH_MISC_SUM_INTR_MASK | MV_ETH_TXDONE_INTR_MASK | MV_ETH_RX_INTR_MASK));

			local_irq_restore(flags);
		}
	}
	cpuCtrl->causeRxTx = causeRxTx;
	return rx_done;
}

static void mv_eth_cpu_counters_init(void)
{
#ifdef CONFIG_MV_CPU_PERF_CNTRS

	mvCpuCntrsInitialize();

#ifdef CONFIG_PLAT_ARMADA
	 
	mvCpuCntrsProgram(0, MV_CPU_CNTRS_CYCLES, "Cycles", 13);

	mvCpuCntrsProgram(1, MV_CPU_CNTRS_INSTRUCTIONS, "Instr", 13);
	 
	mvCpuCntrsProgram(2, MV_CPU_CNTRS_ICACHE_READ_MISS, "IcMiss", 0);

	mvCpuCntrsProgram(3, MV_CPU_CNTRS_DCACHE_READ_MISS, "DcRdMiss", 0);

	mvCpuCntrsProgram(4, MV_CPU_CNTRS_DCACHE_WRITE_MISS, "DcWrMiss", 0);

	mvCpuCntrsProgram(5, MV_CPU_CNTRS_DTLB_MISS, "dTlbMiss", 0);

#else  
	 
	mvCpuCntrsProgram(0, MV_CPU_CNTRS_INSTRUCTIONS, "Instr", 16);
	 
	mvCpuCntrsProgram(1, MV_CPU_CNTRS_ICACHE_READ_MISS, "IcMiss", 0);

	mvCpuCntrsProgram(2, MV_CPU_CNTRS_CYCLES, "Cycles", 18);

	mvCpuCntrsProgram(3, MV_CPU_CNTRS_DCACHE_READ_MISS, "DcRdMiss", 0);
	 
#endif  

	event0 = mvCpuCntrsEventCreate("RX_DESC_PREF", 100000);
	event1 = mvCpuCntrsEventCreate("RX_DESC_READ", 100000);
	event2 = mvCpuCntrsEventCreate("RX_BUF_INV", 100000);
	event3 = mvCpuCntrsEventCreate("RX_DESC_FILL", 100000);
	event4 = mvCpuCntrsEventCreate("TX_START", 100000);
	event5 = mvCpuCntrsEventCreate("RX_BUF_INV", 100000);
	if ((event0 == NULL) || (event1 == NULL) || (event2 == NULL) ||
		(event3 == NULL) || (event4 == NULL) || (event5 == NULL))
		printk(KERN_ERR "Can't create cpu counter events\n");
#endif  
}

void mv_eth_port_promisc_set(int port)
{
#ifdef CONFIG_MV_ETH_PNC
	if (MV_NETA_PNC_CAP()) {
		 
		if (mv_eth_pnc_ctrl_en) {
			pnc_mac_me(port, NULL, CONFIG_MV_ETH_RXQ_DEF);
			pnc_mcast_all(port, 1);
		} else {
			pr_err("%s: PNC control is disabled\n", __func__);
		}
	} else {  
		mvNetaRxUnicastPromiscSet(port, MV_TRUE);
		mvNetaSetUcastTable(port, CONFIG_MV_ETH_RXQ_DEF);
		mvNetaSetSpecialMcastTable(port, CONFIG_MV_ETH_RXQ_DEF);
		mvNetaSetOtherMcastTable(port, CONFIG_MV_ETH_RXQ_DEF);
	}
#else  
	mvNetaRxUnicastPromiscSet(port, MV_TRUE);
	mvNetaSetUcastTable(port, CONFIG_MV_ETH_RXQ_DEF);
	mvNetaSetSpecialMcastTable(port, CONFIG_MV_ETH_RXQ_DEF);
	mvNetaSetOtherMcastTable(port, CONFIG_MV_ETH_RXQ_DEF);
#endif  
}

void mv_eth_port_filtering_cleanup(int port)
{
#ifdef CONFIG_MV_ETH_PNC
	static bool is_first = true;

	if (MV_NETA_PNC_CAP()) {
		 
		if (is_first) {
			tcam_hw_init();
			is_first = false;
		}
	} else {
		mvNetaRxUnicastPromiscSet(port, MV_FALSE);
		mvNetaSetUcastTable(port, -1);
		mvNetaSetSpecialMcastTable(port, -1);
		mvNetaSetOtherMcastTable(port, -1);
	}
#else
	mvNetaRxUnicastPromiscSet(port, MV_FALSE);
	mvNetaSetUcastTable(port, -1);
	mvNetaSetSpecialMcastTable(port, -1);
	mvNetaSetOtherMcastTable(port, -1);
#endif  
}

static MV_STATUS mv_eth_bm_pools_init(void)
{
	int i, j;
	MV_STATUS status;

#ifdef CONFIG_MV_ETH_BM
	if (MV_NETA_BM_CAP()) {
		mvNetaBmControl(MV_START);
		mv_eth_bm_config_get();
	}
#endif  

	for (i = 0; i < MV_ETH_BM_POOLS; i++) {
		status = mv_eth_pool_create(i, MV_BM_POOL_CAP_MAX);
		if (status != MV_OK) {
			printk(KERN_ERR "%s: can't create bm_pool=%d - capacity=%d\n", __func__, i, MV_BM_POOL_CAP_MAX);
			for (j = 0; j < i; j++)
				mv_eth_pool_destroy(j);
			return status;
		}
#ifdef CONFIG_MV_ETH_BM_CPU
		if (MV_NETA_BM_CAP()) {
			mv_eth_pool[i].pkt_size = mv_eth_bm_config_pkt_size_get(i);
			if (mv_eth_pool[i].pkt_size == 0)
				mvNetaBmPoolBufferSizeSet(i, 0);
			else
				mvNetaBmPoolBufferSizeSet(i, RX_BUF_SIZE(mv_eth_pool[i].pkt_size));
		} else {
			mv_eth_pool[i].pkt_size = 0;
		}
#else
		mv_eth_pool[i].pkt_size = 0;
#endif  
	}
	return MV_OK;
}

static int mv_eth_port_link_speed_fc(int port, MV_ETH_PORT_SPEED port_speed, int en_force)
{
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (en_force) {
		if (mvNetaSpeedDuplexSet(port, port_speed, MV_ETH_DUPLEX_FULL)) {
			printk(KERN_ERR "mvEthSpeedDuplexSet failed\n");
			return -EIO;
		}
		if (mvNetaFlowCtrlSet(port, MV_ETH_FC_ENABLE)) {
			printk(KERN_ERR "mvEthFlowCtrlSet failed\n");
			return -EIO;
		}
		if (mvNetaForceLinkModeSet(port, 1, 0)) {
			printk(KERN_ERR "mvEthForceLinkModeSet failed\n");
			return -EIO;
		}

		set_bit(MV_ETH_F_FORCE_LINK_BIT, &(pp->flags));

	} else {
		if (mvNetaForceLinkModeSet(port, 0, 0)) {
			printk(KERN_ERR "mvEthForceLinkModeSet failed\n");
			return -EIO;
		}
		if (mvNetaSpeedDuplexSet(port, MV_ETH_SPEED_AN, MV_ETH_DUPLEX_AN)) {
			printk(KERN_ERR "mvEthSpeedDuplexSet failed\n");
			return -EIO;
		}
		if (mvNetaFlowCtrlSet(port, MV_ETH_FC_AN_SYM)) {
			printk(KERN_ERR "mvEthFlowCtrlSet failed\n");
			return -EIO;
		}

		clear_bit(MV_ETH_F_FORCE_LINK_BIT, &(pp->flags));
	}
	return 0;
}

static void mv_eth_get_mac_addr(int port, unsigned char *addr)
{
	u32 mac_addr_l, mac_addr_h;

	mac_addr_l = MV_REG_READ(ETH_MAC_ADDR_LOW_REG(port));
	mac_addr_h = MV_REG_READ(ETH_MAC_ADDR_HIGH_REG(port));
	addr[0] = (mac_addr_h >> 24) & 0xFF;
	addr[1] = (mac_addr_h >> 16) & 0xFF;
	addr[2] = (mac_addr_h >> 8) & 0xFF;
	addr[3] = mac_addr_h & 0xFF;
	addr[4] = (mac_addr_l >> 8) & 0xFF;
	addr[5] = mac_addr_l & 0xFF;
}

static int mv_eth_load_network_interfaces(struct platform_device *pdev)
{
	u32 port;
	struct eth_port *pp;
	int err = 0;
	struct net_device *dev;
	struct mv_neta_pdata *plat_data = (struct mv_neta_pdata *)pdev->dev.platform_data;

	port = pdev->id;
#if defined(MY_ABC_HERE)
	if (plat_data->tx_csum_limit < MV_ETH_TX_CSUM_MIN_SIZE || plat_data->tx_csum_limit > MV_ETH_TX_CSUM_MAX_SIZE) {
		pr_info("WARN: Invalid FIFO size %d on port-%d, minimal size 2K will be used as default\n",
											plat_data->tx_csum_limit, port);
		plat_data->tx_csum_limit = MV_ETH_TX_CSUM_MIN_SIZE;
	}
#else  
	if (plat_data->tx_csum_limit == 0)
		plat_data->tx_csum_limit = MV_ETH_TX_CSUM_MAX_SIZE;
#endif  

	pr_info("  o Loading network interface(s) for port #%d: cpu_mask=0x%x, tx_csum_limit=%d\n",
			port, plat_data->cpu_mask, plat_data->tx_csum_limit);

	dev = mv_eth_netdev_init(pdev);

	if (!dev) {
		printk(KERN_ERR "%s: can't create netdevice\n", __func__);
		mv_eth_priv_cleanup(pp);
		return -EIO;
	}

	pp = (struct eth_port *)netdev_priv(dev);

	mv_eth_ports[port] = pp;

	if (!MV_PON_PORT(pp->port)) {
		 
		switch (plat_data->speed) {
		case SPEED_10:
			err = mv_eth_port_link_speed_fc(pp->port, MV_ETH_SPEED_10, 1);
			break;
		case SPEED_100:
			err = mv_eth_port_link_speed_fc(pp->port, MV_ETH_SPEED_100, 1);
			break;
		case SPEED_1000:
			err = mv_eth_port_link_speed_fc(pp->port, MV_ETH_SPEED_1000, 1);
			break;
		case 0:
			err = mv_eth_port_link_speed_fc(pp->port, MV_ETH_SPEED_AN, 0);
			break;
		default:
			 
			break;
		}
		if (err)
			return err;
	}

	if (mv_eth_hal_init(pp)) {
		printk(KERN_ERR "%s: can't init eth hal\n", __func__);
		mv_eth_priv_cleanup(pp);
		return -EIO;
	}
#ifdef CONFIG_MV_ETH_HWF
	if (MV_NETA_HWF_CAP())
		mvNetaHwfInit(port);
#endif  

#ifdef CONFIG_MV_ETH_PMT
	if (MV_NETA_PMT_CAP()) {
		if (MV_PON_PORT(port))
			mvNetaPmtInit(port, (MV_NETA_PMT *)ioremap(PMT_PON_PHYS_BASE, PMT_MEM_SIZE));
		else
			mvNetaPmtInit(port, (MV_NETA_PMT *)ioremap(PMT_GIGA_PHYS_BASE + port * 0x40000, PMT_MEM_SIZE));
	}
#endif  

	pr_info("\t%s p=%d: mtu=%d, mac=" MV_MACQUAD_FMT " (%s)\n",
		MV_PON_PORT(port) ? "pon" : "giga", port, dev->mtu, MV_MACQUAD(dev->dev_addr), mac_src[port]);

	handle_group_affinity(port);

	mux_eth_ops.set_tag_type = mv_eth_tag_type_set;
	mv_mux_eth_attach(pp->port, pp->dev, &mux_eth_ops);

#ifdef CONFIG_NETMAP
	mv_neta_netmap_attach(pp);
#endif  

	if (!(pp->flags & MV_ETH_F_CONNECT_LINUX))
		mv_eth_open(pp->dev);

	return MV_OK;
}

int mv_eth_resume_network_interfaces(struct eth_port *pp)
{
	int cpu;
	int err = 0;

	if (!MV_PON_PORT(pp->port)) {
		int phyAddr;
		 
		phyAddr = pp->plat_data->phy_addr;
		mvNetaPhyAddrSet(pp->port, phyAddr);
	}
	mvNetaPortDisable(pp->port);
	mvNetaDefaultsSet(pp->port);

	if (pp->flags & MV_ETH_F_MH)
		mvNetaMhSet(pp->port, MV_TAG_TYPE_MH);

	if (pp->tagged) {
		 
		mv_eth_port_promisc_set(pp->port);
	}

	for (cpu = 0; cpu < nr_cpu_ids; cpu++) {
		 
		mvNetaRxqCpuMaskSet(pp->port, pp->cpu_config[cpu]->cpuRxqMask, cpu);
		mvNetaTxqCpuMaskSet(pp->port, pp->cpu_config[cpu]->cpuTxqMask, cpu);
	}

	if (!MV_PON_PORT(pp->port)) {
		 
		switch (pp->plat_data->speed) {
		case SPEED_10:
			err = mv_eth_port_link_speed_fc(pp->port, MV_ETH_SPEED_10, 1);
			break;
		case SPEED_100:
			err = mv_eth_port_link_speed_fc(pp->port, MV_ETH_SPEED_100, 1);
			break;
		case SPEED_1000:
			err = mv_eth_port_link_speed_fc(pp->port, MV_ETH_SPEED_1000, 1);
			break;
		case 0:
			err = mv_eth_port_link_speed_fc(pp->port, MV_ETH_SPEED_AN, 0);
			break;
		default:
			 
			break;
		}
		if (err)
			return err;
	}

	return MV_OK;
}

#ifdef CONFIG_MV_ETH_BM
int     mv_eth_bm_pool_restore(struct bm_pool *bm_pool)
{
		MV_UNIT_WIN_INFO        winInfo;
		MV_STATUS               status;
		int pool = bm_pool->pool;

		mvNetaBmPoolInit(bm_pool->pool, bm_pool->bm_pool, bm_pool->physAddr, bm_pool->capacity);
#ifdef CONFIG_ARCH_MVEBU
		status = mvebu_mbus_get_addr_win_info(bm_pool->physAddr, &winInfo.targetId, &winInfo.attrib);
#else
		status = mvCtrlAddrWinInfoGet(&winInfo, bm_pool->physAddr);
#endif
		if (status != MV_OK) {
			printk(KERN_ERR "%s: Can't map BM pool #%d. phys_addr=0x%x, status=%d\n",
				__func__, bm_pool->pool, (unsigned)bm_pool->physAddr, status);
			mvOsIoCachedFree(NULL, sizeof(MV_U32) *  bm_pool->capacity, bm_pool->physAddr, bm_pool->bm_pool, 0);
			return MV_ERROR;
		}
		mvNetaBmPoolTargetSet(pool, winInfo.targetId, winInfo.attrib);
		mvNetaBmPoolEnable(pool);

		return MV_OK;
}
#endif  

static int mv_eth_resume_port_pools(struct eth_port *pp)
{
	int num;

	if (!pp)
		return -ENODEV;

	if (pp->pool_long) {
		num = mv_eth_pool_add(pp, pp->pool_long->pool, pp->pool_long_num);

		if (num != pp->pool_long_num) {
			printk(KERN_ERR "%s FAILED long: pool=%d, pkt_size=%d, only %d of %d allocated\n",
			       __func__, pp->pool_long->pool, pp->pool_long->pkt_size, num, pp->pool_long_num);
			return MV_ERROR;
		}

#ifndef CONFIG_MV_ETH_BM_CPU
	}  
#else
		if (MV_NETA_BM_CAP())
			mvNetaBmPoolBufSizeSet(pp->port, pp->pool_long->pool, RX_BUF_SIZE(pp->pool_long->pkt_size));
	}

	if (MV_NETA_BM_CAP() && pp->pool_short) {
		if (pp->pool_short->pool != pp->pool_long->pool) {
				 
				num = mv_eth_pool_add(pp, pp->pool_short->pool, pp->pool_short_num);
				if (num != pp->pool_short_num) {
					printk(KERN_ERR "%s FAILED short: pool=%d, pkt_size=%d - %d of %d buffers added\n",
					   __func__, pp->pool_short->pool, pp->pool_short->pkt_size, num, pp->pool_short_num);
					return MV_ERROR;
				}

				mvNetaBmPoolBufSizeSet(pp->port, pp->pool_short->pool, RX_BUF_SIZE(pp->pool_short->pkt_size));

		} else {

			int dummy_short_pool = (pp->pool_short->pool + 1) % MV_BM_POOLS;
			 
			mvNetaBmPoolBufSizeSet(pp->port, dummy_short_pool, NET_SKB_PAD);

		}
	}

#endif  

	return MV_OK;
}

static int mv_eth_resume_rxq_txq(struct eth_port *pp, int mtu)
{
	int rxq, txp, txq = 0;

	if (pp->plat_data->is_sgmii)
		MV_REG_WRITE(SGMII_SERDES_CFG_REG(pp->port), pp->sgmii_serdes);

	for (txp = 0; txp < pp->txp_num; txp++)
		mvNetaTxpReset(pp->port, txp);

	mvNetaRxReset(pp->port);

	for (rxq = 0; rxq < CONFIG_MV_ETH_RXQ; rxq++) {

		if (pp->rxq_ctrl[rxq].q) {
			 
			mvNetaRxqAddrSet(pp->port, rxq,  pp->rxq_ctrl[rxq].rxq_size);

			mvNetaRxqOffsetSet(pp->port, rxq, NET_SKB_PAD);

			mv_eth_rx_pkts_coal_set(pp->port, rxq, pp->rxq_ctrl[rxq].rxq_pkts_coal);
			mv_eth_rx_time_coal_set(pp->port, rxq, pp->rxq_ctrl[rxq].rxq_time_coal);

#if defined(CONFIG_MV_ETH_BM_CPU)
			if (MV_NETA_BM_CAP()) {
				 
				if (pp->pool_long && pp->pool_short) {

					if (pp->pool_short->pool == pp->pool_long->pool) {
						int dummy_short_pool = (pp->pool_short->pool + 1) % MV_BM_POOLS;

						mvNetaRxqBmEnable(pp->port, rxq, dummy_short_pool, pp->pool_long->pool);
					} else
						mvNetaRxqBmEnable(pp->port, rxq, pp->pool_short->pool,
								       pp->pool_long->pool);
				}
			} else {
				 
				mvNetaRxqBufSizeSet(pp->port, rxq, RX_BUF_SIZE(pp->pool_long->pkt_size));
				mvNetaRxqBmDisable(pp->port, rxq);
			}
#else
			 
			mvNetaRxqBufSizeSet(pp->port, rxq, RX_BUF_SIZE(pp->pool_long->pkt_size));
			mvNetaRxqBmDisable(pp->port, rxq);
#endif  
			if (mvNetaRxqFreeDescNumGet(pp->port, rxq) == 0)
				mv_eth_rxq_fill(pp, rxq, pp->rxq_ctrl[rxq].rxq_size);
		}
	}

	for (txp = 0; txp < pp->txp_num; txp++) {
		for (txq = 0; txq < CONFIG_MV_ETH_TXQ; txq++) {
			struct tx_queue *txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];

			if (txq_ctrl->q != NULL) {
				mvNetaTxqAddrSet(pp->port, txq_ctrl->txp, txq_ctrl->txq, txq_ctrl->txq_size);
				mv_eth_tx_done_pkts_coal_set(pp->port, txp, txq,
							pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq].txq_done_pkts_coal);
			}
			mvNetaTxqBandwidthSet(pp->port, txp, txq);
		}
		mvNetaTxpRateMaxSet(pp->port, txp);
		mvNetaTxpMaxTxSizeSet(pp->port, txp, RX_PKT_SIZE(mtu));
	}

	return MV_OK;
}

#ifdef CONFIG_MV_ETH_PNC
static void mv_eth_pnc_resume(void)
{
	 
	if (wol_ports_bmp != 0)
		return;

	tcam_hw_init();

	if (pnc_default_init())
		printk(KERN_ERR "%s: Warning PNC init failed\n", __func__);

}
#endif  

int mv_eth_port_resume(int port)
{
	struct eth_port *pp;
	int cpu;

	pp = mv_eth_port_by_id(port);

	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return  MV_ERROR;
	}

	if (!(pp->flags & MV_ETH_F_SUSPEND)) {
		printk(KERN_ERR "%s: port %d is not suspend.\n", __func__, port);
		return MV_ERROR;
	}
	mvNetaPortPowerUp(port, pp->plat_data->is_sgmii, pp->plat_data->is_rgmii, (pp->plat_data->phy_addr == -1));

	mv_eth_win_init(port);

	mv_eth_resume_network_interfaces(pp);

	if (pm_flag == 0) {
#ifdef CONFIG_MV_ETH_BM
		struct bm_pool *ppool;
		int pool;

		if (MV_NETA_BM_CAP()) {
			mvNetaBmControl(MV_START);

			mvNetaBmRegsInit();

			for (pool = 0; pool < MV_ETH_BM_POOLS; pool++) {
				ppool = &mv_eth_pool[pool];
				if (mv_eth_bm_pool_restore(ppool)) {
					pr_err("%s: port #%d pool #%d resrote failed.\n", __func__, port, pool);
					return MV_ERROR;
				}
			}
		}
#endif  

#ifdef CONFIG_MV_ETH_PNC
		if (MV_NETA_PNC_CAP())
			mv_eth_pnc_resume();
#endif  

		pm_flag = 1;
	}

	if (pp->flags & MV_ETH_F_STARTED_OLD)
		(*pp->dev->netdev_ops->ndo_set_rx_mode)(pp->dev);

	for_each_possible_cpu(cpu)
		pp->cpu_config[cpu]->causeRxTx = 0;

	set_bit(MV_ETH_F_STARTED_BIT, &(pp->flags));

	mv_eth_resume_port_pools(pp);

	mv_eth_resume_rxq_txq(pp, pp->dev->mtu);

	if (pp->flags & MV_ETH_F_STARTED_OLD) {
		mv_eth_resume_internals(pp, pp->dev->mtu);
		clear_bit(MV_ETH_F_STARTED_OLD_BIT, &(pp->flags));
		if (pp->flags & MV_ETH_F_CONNECT_LINUX) {
			on_each_cpu(mv_eth_interrupts_unmask, pp, 1);
		}
	} else
		clear_bit(MV_ETH_F_STARTED_BIT, &(pp->flags));

	clear_bit(MV_ETH_F_SUSPEND_BIT, &(pp->flags));

	printk(KERN_NOTICE "Exit suspend mode on port #%d\n", port);

	return MV_OK;
}

void    mv_eth_hal_shared_init(struct mv_neta_pdata *plat_data)
{
	MV_NETA_HAL_DATA halData;
	MV_U32 bm_phy_base, bm_size;
	MV_U32 pnc_phy_base, pnc_size;
	int ret;
	unsigned int dev = 0, rev = 0;

	bm_phy_base = 0;
	pnc_phy_base = 0;
	bm_size = 0;
	pnc_size = 0;
	memset(&halData, 0, sizeof(halData));

	halData.maxPort = plat_data->max_port;
	halData.pClk = plat_data->pclk;
	halData.tClk = plat_data->tclk;
	halData.maxCPUs = plat_data->max_cpu;
	halData.cpuMask = plat_data->cpu_mask;
	halData.iocc = arch_is_coherent();
	halData.ctrlModel = plat_data->ctrl_model;
	halData.ctrlRev = plat_data->ctrl_rev;

#ifdef CONFIG_ARCH_MVEBU
	dev = plat_data->ctrl_model;
	rev = plat_data->ctrl_rev;

	if (MV_NETA_BM_CAP()) {
		ret = mvebu_mbus_win_addr_get(MV_BM_WIN_ID, MV_BM_WIN_ATTR, &bm_phy_base, &bm_size);
		if (ret) {
			pr_err("%s: get BM mbus window failed, error: %d.\n", __func__, ret);
			return;
		}
		halData.bmPhysBase = bm_phy_base;
		halData.bmVirtBase = (MV_U8 *)ioremap(bm_phy_base, bm_size);
	}
	if (MV_NETA_PNC_CAP()) {
		 
		if ((dev == MV78230_DEV_ID)
			|| (dev == MV78260_DEV_ID)
			|| (dev == MV78460_DEV_ID)) {
			if (!MV_NETA_BM_CAP()) {
				ret = mvebu_mbus_win_addr_get(MV_BM_WIN_ID, MV_BM_WIN_ATTR, &bm_phy_base, &bm_size);
				if (ret) {
					pr_err("%s: get BM mbus window failed, error: %d.\n", __func__, ret);
					return;
				}
			}
			halData.pncPhysBase = bm_phy_base;
			halData.pncVirtBase = (MV_U8 *)ioremap(bm_phy_base, bm_size);
		} else {
			ret = mvebu_mbus_win_addr_get(MV_PNC_WIN_ID, MV_PNC_WIN_ATTR, &pnc_phy_base, &pnc_size);
			if (ret) {
				pr_err("%s: get PNC mbus window failed, error: %d.\n", __func__, ret);
				return;
			}
			halData.pncPhysBase = pnc_phy_base;
			halData.pncVirtBase = (MV_U8 *)ioremap(pnc_phy_base, pnc_size);
		}
		halData.pncTcamSize = plat_data->pnc_tcam_size;
	}
#else
#ifdef CONFIG_MV_ETH_BM
	halData.bmPhysBase = PNC_BM_PHYS_BASE;
	halData.bmVirtBase = (MV_U8 *)ioremap(PNC_BM_PHYS_BASE, PNC_BM_SIZE);
#endif  

#ifdef CONFIG_MV_ETH_PNC
	halData.pncTcamSize = plat_data->pnc_tcam_size;
	halData.pncPhysBase = PNC_BM_PHYS_BASE;
	halData.pncVirtBase = (MV_U8 *)ioremap(PNC_BM_PHYS_BASE, PNC_BM_SIZE);
#endif  

#endif  

	mvNetaHalInit(&halData);

	return;
}

#ifdef CONFIG_ARCH_MVEBU
int mv_eth_win_set(int port, u32 base, u32 size, u8 trg_id, u8 win_attr, u32 *enable, u32 *protection)
{
	u32 baseReg, sizeReg;
	u32 alignment;
	u8 i, attr;

	for (i = 0; i < ETH_MAX_DECODE_WIN; i++) {
		if (*enable & (1 << i))
			break;
	}
	if (i == ETH_MAX_DECODE_WIN) {
		pr_err("%s: No window is available\n", __func__);
		return MV_ERROR;
	}

	if (MV_IS_NOT_ALIGN(base, size)) {
		pr_err("%s: Error setting eth port%d window%d.\n"
		   "Address 0x%08x is not aligned to size 0x%x.\n",
		   __func__, port, i, base, size);
		return MV_ERROR;
	}

	if (!MV_IS_POWER_OF_2(size)) {
		pr_err("%s:Error setting eth port%d window%d.\n"
			"Window size %u is not a power to 2.\n",
			__func__, port, i, size);
		return MV_ERROR;
	}

	attr = win_attr;

#ifdef CONFIG_MV_SUPPORT_L2_DEPOSIT
	if (trg_id == TARGET_DDR) {
		 
		attr &= ~(0x30);
		attr |= 0x30;
	}
#endif

	baseReg = (base & ETH_WIN_BASE_MASK);
	sizeReg = MV_REG_READ(ETH_WIN_SIZE_REG(port, i));

	alignment = 1 << ETH_WIN_SIZE_OFFS;
	sizeReg &= ~ETH_WIN_SIZE_MASK;
	sizeReg |= (((size / alignment) - 1) << ETH_WIN_SIZE_OFFS);

	baseReg &= ~ETH_WIN_ATTR_MASK;
	baseReg |= attr << ETH_WIN_ATTR_OFFS;

	baseReg &= ~ETH_WIN_TARGET_MASK;
	baseReg |= trg_id << ETH_WIN_TARGET_OFFS;

	MV_REG_WRITE(ETH_WIN_BASE_REG(port, i), baseReg);
	MV_REG_WRITE(ETH_WIN_SIZE_REG(port, i), sizeReg);

	*enable &= ~(1 << i);
	*protection |= (FULL_ACCESS << (i * 2));

	return MV_OK;
}

int mv_eth_pnc_win_get(u32 *phyaddr_base, u32 *win_size)
{
	static bool first_time = true;
	int ret = 0;

	if (first_time) {
		ret = mvebu_mbus_win_addr_get(MV_PNC_WIN_ID, MV_PNC_WIN_ATTR, phyaddr_base, win_size);
		pnc_phyaddr_base = *phyaddr_base;
		pnc_win_size = *win_size;
		first_time = false;
	} else {
		*phyaddr_base = pnc_phyaddr_base;
		*win_size = pnc_win_size;
	}
	return ret;
}

int mv_eth_bm_win_get(u32 *phyaddr_base, u32 *win_size)
{
	static bool first_time = true;
	int ret = 0;

	if (first_time) {
		ret = mvebu_mbus_win_addr_get(MV_BM_WIN_ID, MV_BM_WIN_ATTR, phyaddr_base, win_size);
		bm_phyaddr_base = *phyaddr_base;
		bm_win_size = *win_size;
		first_time = false;
	} else {
		*phyaddr_base = bm_phyaddr_base;
		*win_size = bm_win_size;
	}
	return ret;
}

void	mv_eth_win_init(int port)
{
	const struct mbus_dram_target_info *dram;
	u32 phyaddr_base, win_size;
	int i, ret;
	u32 enable = 0, protect = 0;
	unsigned int dev, rev;

	if (mvebu_get_soc_id(&dev, &rev))
		return;

	enable = (1 << ETH_MAX_DECODE_WIN) - 1;
	MV_REG_WRITE(ETH_BASE_ADDR_ENABLE_REG(port), enable);

	for (i = 0; i < ETH_MAX_DECODE_WIN; i++) {
		MV_REG_WRITE(ETH_WIN_BASE_REG(port, i), 0);
		MV_REG_WRITE(ETH_WIN_SIZE_REG(port, i), 0);

		if (i < ETH_MAX_HIGH_ADDR_REMAP_WIN)
			MV_REG_WRITE(ETH_WIN_REMAP_REG(port, i), 0);
	}

	dram = mv_mbus_dram_info();
	if (!dram) {
		pr_err("%s: No DRAM information\n", __func__);
		return;
	}
	for (i = 0; i < dram->num_cs; i++) {
		const struct mbus_dram_window *cs = dram->cs + i;
		ret = mv_eth_win_set(port, cs->base, cs->size, dram->mbus_dram_target_id, cs->mbus_attr,
				     &enable, &protect);
		if (ret) {
			pr_err("%s: eth window set fail\n", __func__);
			return;
		}
	}

	if (MV_NETA_BM_CAP()) {
		ret = mv_eth_bm_win_get(&phyaddr_base, &win_size);
		if (ret) {
			pr_err("%s: BM window addr info get fail\n", __func__);
			return;
		}
		ret = mv_eth_win_set(port, phyaddr_base, win_size, MV_BM_WIN_ID, MV_BM_WIN_ATTR,
				     &enable, &protect);
		if (ret) {
			pr_err("%s: BM window set fail\n", __func__);
			return;
		}
	}

	if (MV_NETA_PNC_CAP()) {
		 
		if ((dev == MV78230_DEV_ID)
			|| (dev == MV78260_DEV_ID)
			|| (dev == MV78460_DEV_ID)) {
			if (!MV_NETA_BM_CAP()) {
				ret = mv_eth_bm_win_get(&phyaddr_base, &win_size);
				if (ret) {
					pr_err("%s: BM window addr info get fail\n", __func__);
					return;
				}
				ret = mv_eth_win_set(port, phyaddr_base, win_size, MV_BM_WIN_ID, MV_BM_WIN_ATTR,
							 &enable, &protect);
				if (ret) {
					pr_err("%s: BM window set fail\n", __func__);
					return;
				}
			}
		} else {
			ret = mv_eth_pnc_win_get(&phyaddr_base, &win_size);
			if (ret) {
				pr_err("%s: PNC window addr info get fail\n", __func__);
				return;
			}
			ret = mv_eth_win_set(port, phyaddr_base, win_size, MV_PNC_WIN_ID, MV_PNC_WIN_ATTR,
					     &enable, &protect);
			if (ret) {
				pr_err("%s: PNC window set fail\n", __func__);
				return;
			}
		}
	}

	MV_REG_WRITE(ETH_ACCESS_PROTECT_REG(port), protect);
	 
	MV_REG_WRITE(ETH_BASE_ADDR_ENABLE_REG(port), enable);
}

#else  

void 	mv_eth_win_init(int port)
{
	MV_UNIT_WIN_INFO addrWinMap[MAX_TARGETS + 1];
	MV_STATUS status;
	int i;

	status = mvCtrlAddrWinMapBuild(addrWinMap, MAX_TARGETS + 1);
	if (status != MV_OK)
		return;

	for (i = 0; i < MAX_TARGETS; i++) {
		if (addrWinMap[i].enable == MV_FALSE)
			continue;

#ifdef CONFIG_MV_SUPPORT_L2_DEPOSIT
		 
		if (MV_TARGET_IS_DRAM(i)) {
			addrWinMap[i].attrib &= ~(0x30);
			addrWinMap[i].attrib |= 0x30;
		}
#endif
	}
	mvNetaWinInit(port, addrWinMap);
	return;
}
#endif  

int mv_eth_port_suspend(int port)
{
	struct eth_port *pp;
	int txp;

	pp = mv_eth_port_by_id(port);
	if (!pp)
		return MV_OK;

	if (pp->flags & MV_ETH_F_SUSPEND) {
		printk(KERN_ERR "%s: port %d is allready suspend.\n", __func__, port);
		return MV_ERROR;
	}

	if (pp->plat_data->is_sgmii)
		pp->sgmii_serdes = MV_REG_READ(SGMII_SERDES_CFG_REG(port));

	if (pp->flags & MV_ETH_F_STARTED) {
		set_bit(MV_ETH_F_STARTED_OLD_BIT, &(pp->flags));
		clear_bit(MV_ETH_F_STARTED_BIT, &(pp->flags));
		mv_eth_suspend_internals(pp);
	} else
		clear_bit(MV_ETH_F_STARTED_OLD_BIT, &(pp->flags));

#ifdef CONFIG_MV_ETH_HWF
	if (MV_NETA_HWF_CAP()) {
		mvNetaHwfEnable(pp->port, 0);
	} else {
		 
		for (txp = 0; txp < pp->txp_num; txp++)
			mv_eth_txp_reset(pp->port, txp);
	}
#else
	 
	for (txp = 0; txp < pp->txp_num; txp++)
		mv_eth_txp_reset(pp->port, txp);
#endif   

	mv_eth_rx_reset(pp->port);

	mv_eth_port_pools_free(port);

	set_bit(MV_ETH_F_SUSPEND_BIT, &(pp->flags));

	printk(KERN_NOTICE "Enter suspend mode on port #%d\n", port);
	return MV_OK;
}

int	mv_eth_wol_mode_set(int port, int mode)
{
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return -EINVAL;
	}

	if ((mode < 0) || (mode > 1)) {
		printk(KERN_ERR "%s: mode = %d, Invalid value.\n", __func__, mode);
		return -EINVAL;
	}

	if (pp->flags & MV_ETH_F_SUSPEND) {
		printk(KERN_ERR "Port %d must resumed before\n", port);
		return -EINVAL;
	}
	pp->pm_mode = mode;

	if (mode)
#ifdef CONFIG_MV_ETH_PNC_WOL
		wol_ports_bmp |= (1 << port);
#else
		;
#endif
	else
		wol_ports_bmp &= ~(1 << port);

	return MV_OK;
}

static void mv_eth_sysfs_exit(void)
{
	struct device *pd;

	pd = bus_find_device_by_name(&platform_bus_type, NULL, "neta");
	if (!pd) {
		printk(KERN_ERR"%s: cannot find pp2 device\n", __func__);
		return;
	}
#ifdef CONFIG_MV_ETH_L2FW
	mv_neta_l2fw_sysfs_exit(&pd->kobj);
#endif

#ifdef CONFIG_MV_ETH_PNC_WOL
	if (MV_NETA_PNC_CAP())
		mv_neta_wol_sysfs_exit(&pd->kobj);
#endif

#ifdef CONFIG_MV_ETH_PNC
	if (MV_NETA_PNC_CAP())
		mv_neta_pnc_sysfs_exit(&pd->kobj);
#endif

#ifdef CONFIG_MV_ETH_BM
	if (MV_NETA_BM_CAP())
		mv_neta_bm_sysfs_exit(&pd->kobj);
#endif

#ifdef CONFIG_MV_ETH_HWF
	if (MV_NETA_HWF_CAP())
		mv_neta_hwf_sysfs_exit(&pd->kobj);
#endif

#ifdef CONFIG_MV_PON
	mv_neta_pon_sysfs_exit(&pd->kobj);
#endif
	platform_device_unregister(neta_sysfs);
}

static int mv_eth_sysfs_init(void)
{
	struct device *pd;

	pd = bus_find_device_by_name(&platform_bus_type, NULL, "neta");
	if (!pd) {
		neta_sysfs = platform_device_register_simple("neta", -1, NULL, 0);
		pd = bus_find_device_by_name(&platform_bus_type, NULL, "neta");
	}

	if (!pd) {
		printk(KERN_ERR"%s: cannot find neta device\n", __func__);
		return -1;
	}

	mv_neta_gbe_sysfs_init(&pd->kobj);

#ifdef CONFIG_MV_PON
	mv_neta_pon_sysfs_init(&pd->kobj);
#endif

#ifdef CONFIG_MV_ETH_HWF
	if (MV_NETA_HWF_CAP())
		mv_neta_hwf_sysfs_init(&pd->kobj);
#endif

#ifdef CONFIG_MV_ETH_BM
	if (MV_NETA_BM_CAP())
		mv_neta_bm_sysfs_init(&pd->kobj);
#endif

#ifdef CONFIG_MV_ETH_PNC
	if (MV_NETA_PNC_CAP())
		mv_neta_pnc_sysfs_init(&pd->kobj);
#endif

#ifdef CONFIG_MV_ETH_PNC_WOL
	if (MV_NETA_PNC_CAP())
		mv_neta_wol_sysfs_init(&pd->kobj);
#endif

#ifdef CONFIG_MV_ETH_L2FW
	mv_neta_l2fw_sysfs_init(&pd->kobj);
#endif

	return 0;
}

static int	mv_eth_shared_probe(struct mv_neta_pdata *plat_data)
{
	int size;

#ifdef ETH_SKB_DEBUG
	memset(mv_eth_skb_debug, 0, sizeof(mv_eth_skb_debug));
	spin_lock_init(&skb_debug_lock);
#endif

	mv_eth_sysfs_init();

	pr_info("SoC: model = 0x%x, revision = 0x%x\n",
		plat_data->ctrl_model, plat_data->ctrl_rev);

	mv_eth_hal_shared_init(plat_data);

	mv_eth_ports_num = plat_data->max_port;
	if (mv_eth_ports_num > CONFIG_MV_ETH_PORTS_NUM)
		mv_eth_ports_num = CONFIG_MV_ETH_PORTS_NUM;

	mv_eth_config_show();

	size = mv_eth_ports_num * sizeof(struct eth_port *);
	mv_eth_ports = mvOsMalloc(size);
	if (!mv_eth_ports)
		goto oom;

	memset(mv_eth_ports, 0, size);

	if (mv_eth_bm_pools_init())
		goto oom;

#ifdef CONFIG_MV_ETH_PNC
	if (MV_NETA_PNC_CAP()) {
		 
		if (pnc_gbe_port_map_init(plat_data->ctrl_model, plat_data->ctrl_rev)) {
			pr_err("%s: PNC GBE port mapping init failed\n", __func__);
			goto oom;
		}

		if (mv_eth_pnc_ctrl_en) {
			if (pnc_default_init())
				pr_err("%s: Warning PNC init failed\n", __func__);
		} else
			pr_err("%s: PNC control is disabled\n", __func__);
	}
#endif  

#ifdef CONFIG_MV_ETH_L2FW
	mv_l2fw_init();
#endif

	mv_eth_cpu_counters_init();

	mv_eth_initialized = 1;

	return 0;

oom:
	if (mv_eth_ports)
		mvOsFree(mv_eth_ports);

	printk(KERN_ERR "%s: out of memory\n", __func__);
	return -ENOMEM;
}

#ifdef CONFIG_OF
static int pnc_bm_initialize;
static unsigned int pnc_tcam_line_num;
static int mv_eth_port_num_get(struct platform_device *pdev);

static int mv_eth_neta_cap_verify(unsigned int neta_cap_bm)
{
	unsigned int dev, rev;

	if (mvebu_get_soc_id(&dev, &rev))
		return MV_FAIL;

	switch (dev) {
	 
	case MV6710_DEV_ID:
	case MV6707_DEV_ID:
		if (neta_cap_bm == 0)
			return 0;
		else
			goto err;
		break;

	case MV78230_DEV_ID:
	case MV78260_DEV_ID:
	case MV78460_DEV_ID:
	 
	case MV88F6510_DEV_ID:
	case MV88F6530_DEV_ID:
	case MV88F6560_DEV_ID:
	case MV88F6601_DEV_ID:
		if (neta_cap_bm == (MV_ETH_CAP_PNC | MV_ETH_CAP_BM | MV_ETH_CAP_HWF | MV_ETH_CAP_PME) ||
		    neta_cap_bm == (MV_ETH_CAP_PNC | MV_ETH_CAP_BM | MV_ETH_CAP_HWF) ||
		    neta_cap_bm == (MV_ETH_CAP_PNC | MV_ETH_CAP_BM) ||
		    neta_cap_bm == MV_ETH_CAP_PNC ||
		    neta_cap_bm == MV_ETH_CAP_BM ||
		    neta_cap_bm == 0)
			return 0;
		else
			goto err;
		break;

	case MV88F6810_DEV_ID:
	case MV88F6811_DEV_ID:
	case MV88F6820_DEV_ID:
	case MV88F6828_DEV_ID:
	case MV88F6W22_DEV_ID:
	case MV88F6W23_DEV_ID:
		if (((rev == MV88F68xx_Z1_REV) && (neta_cap_bm == MV_ETH_CAP_BM || neta_cap_bm == 0)) ||
		    ((rev == MV88F68xx_A0_REV) && (neta_cap_bm == (MV_ETH_CAP_PNC | MV_ETH_CAP_BM) ||
						neta_cap_bm == MV_ETH_CAP_PNC ||
						neta_cap_bm == MV_ETH_CAP_BM ||
						neta_cap_bm == 0)))
			return 0;
		else
			goto err;
		break;

	default:
		goto err;
		break;
	}

	return 0;

err:
	pr_err("Error: invalid NETA capability 0x%x for SoC with dev_id-0x%x, rev_id-%d\n", neta_cap_bm, dev, rev);
	return -1;
}

static struct of_device_id of_bm_pnc_table[] = {
		{ .compatible = "marvell,neta_bm_pnc" },
};

static int mv_eth_pnc_bm_init(struct mv_neta_pdata *plat_data)
{
	struct device_node *bm_pnc_np;
	struct clk *clk;
	unsigned int dev, rev;

	if (pnc_bm_initialize > 0) {
		if (plat_data)
			plat_data->pnc_tcam_size = pnc_tcam_line_num;
		return MV_OK;
	}

	if (mvebu_get_soc_id(&dev, &rev))
		return MV_FAIL;

	bm_pnc_np = of_find_matching_node(NULL, of_bm_pnc_table);
	if (bm_pnc_np) {
		 
		if (of_property_read_u32(bm_pnc_np, "neta_cap_bm", &neta_cap_bitmap)) {
			pr_err("could not get bitmap of neta capability\n");
			return -1;
		}
		 
		if (mv_eth_neta_cap_verify(neta_cap_bitmap)) {
			pr_err("NETA capability verify not pass\n");
			return -1;
		}

		if (MV_NETA_PNC_CAP() && plat_data != NULL) {
			if (of_property_read_u32(bm_pnc_np, "pnc_tcam_size", &pnc_tcam_line_num)) {
				pr_err("could not get pnc tcam size\n");
				return -1;
			}
			plat_data->pnc_tcam_size = pnc_tcam_line_num;
		}

		if (MV_NETA_BM_CAP()) {
			 
			bm_reg_vbase = (int)of_iomap(bm_pnc_np, 0);

			clk = of_clk_get(bm_pnc_np, 0);

			clk_prepare_enable(clk);
		}

		if (MV_NETA_PNC_CAP()) {
			 
			pnc_reg_vbase = (int)of_iomap(bm_pnc_np, 1);
			 
			if ((dev == MV78230_DEV_ID)
				|| (dev == MV78260_DEV_ID)
				|| (dev == MV78460_DEV_ID)) {
				 
				if (!MV_NETA_BM_CAP()) {
					clk = of_clk_get(bm_pnc_np, 0);
					clk_prepare_enable(clk);
				}
			} else {
				 
				clk = of_clk_get(bm_pnc_np, 1);
				clk_prepare_enable(clk);
			}
		}
	} else {
		bm_reg_vbase = 0;
		pnc_reg_vbase = 0;
		neta_cap_bitmap = 0;
	}

	pnc_bm_initialize++;

	return MV_OK;
}

static struct mv_neta_pdata *mv_plat_data_get(struct platform_device *pdev)
{
	struct mv_neta_pdata *plat_data;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *phy_node;
	void __iomem *base_addr;
	struct resource *res;
	struct clk *clk;
	phy_interface_t phy_mode;
	const char *mac_addr = NULL;
	u32 ctrl_model, ctrl_rev;

	if (of_property_read_u32(np, "eth,port-num", &pdev->id)) {
		pr_err("could not get port number\n");
		return NULL;
	}

	plat_data = kzalloc(sizeof(struct mv_neta_pdata), GFP_KERNEL);
	if (plat_data == NULL) {
		pr_err("could not allocate memory for plat_data\n");
		return NULL;
	}
	memset(plat_data, 0, sizeof(struct mv_neta_pdata));

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		pr_err("could not get resource information\n");
		return NULL;
	}

#if defined(MY_ABC_HERE)
	if (!port_vbase[pdev->id]) {
		base_addr = devm_ioremap(&pdev->dev, res->start, resource_size(res));
		if (!base_addr) {
			pr_err("could not map neta registers\n");
			return NULL;
		}
		port_vbase[pdev->id] = (int)base_addr;
	}

	if ((pdev->id % 2) == 1) {
		struct device_node *eth_previous;
		u32 port_num_previous = 0;

		for_each_node_by_name(eth_previous, "ethernet") {
			if (0 != of_property_read_u32(eth_previous, "eth,port-num", &port_num_previous)) {
				pr_err("%s read eth,port-num failed!!!\n", __func__);
				return NULL;
			}

			if (port_num_previous == (pdev->id - 1))
				break;
		}

		if (port_num_previous != (pdev->id - 1)) {
			pr_err("%s eth%d is not found!!!\n", __func__, pdev->id - 1);
			return NULL;
		}

		if (!port_vbase[port_num_previous])
			port_vbase[port_num_previous] = (int)of_iomap(eth_previous, 0);
	}
#else
	base_addr = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!base_addr) {
		pr_err("could not map neta registers\n");
		return NULL;
	}
	port_vbase[pdev->id] = (int)base_addr;
#endif  

	if (pdev->dev.of_node) {
		plat_data->irq = irq_of_parse_and_map(np, 0);
		if (plat_data->irq == 0) {
			pr_err("could not get IRQ number\n");
			return NULL;
		}
	} else {
		res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
		if (res == NULL) {
			pr_err("could not get IRQ number\n");
			return NULL;
		}
		plat_data->irq = res->start;
	}
	 
	mac_addr = of_get_mac_address(np);
	if (mac_addr != NULL)
		memcpy(plat_data->mac_addr, mac_addr, MV_MAC_ADDR_SIZE);

	phy_node = of_parse_phandle(np, "phy", 0);
	if (!phy_node) {
		pr_err("no associated PHY\n");
		return NULL;
	}
	if (of_property_read_u32(phy_node, "reg", &plat_data->phy_addr)) {
		pr_err("could not PHY SMI address\n");
		return NULL;
	}

	if (plat_data->phy_addr == 999)
		plat_data->phy_addr = -1;

	if (of_property_read_u32(np, "eth,port-mtu", &plat_data->mtu)) {
		pr_err("could not get MTU\n");
		return NULL;
	}
	 
#if defined(MY_ABC_HERE)
	if (of_property_read_u32(np, "tx-csum-limit", &plat_data->tx_csum_limit)) {
		pr_err("Could not get FIFO size, tx-csum-limit\n");
		return NULL;
	}
#else  
	if (of_property_read_u32(np, "tx-csum-limit", &plat_data->tx_csum_limit))
		plat_data->tx_csum_limit = MV_ETH_TX_CSUM_MAX_SIZE;
#endif  

	if (mv_eth_pnc_bm_init(plat_data)) {
		pr_err("pnc and bm init fail\n");
		return NULL;
	}

	phy_mode = of_get_phy_mode(np);
	if (phy_mode < 0) {
		pr_err("unknown PHY mode\n");
		return NULL;
	}
	switch (phy_mode) {
	case PHY_INTERFACE_MODE_SGMII:
		plat_data->is_sgmii = 1;
		plat_data->is_rgmii = 0;
	break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
		plat_data->is_sgmii = 0;
		plat_data->is_rgmii = 1;
	break;
	default:
		pr_err("unsupported PHY mode (%d)\n", phy_mode);
		return NULL;
	}

#if defined(MY_ABC_HERE)
	 
#else
	plat_data->tclk = 166666667;     
#endif  
	plat_data->max_port = mv_eth_port_num_get(pdev);

	plat_data->cpu_mask  = (1 << nr_cpu_ids) - 1;
	plat_data->duplex = DUPLEX_FULL;

	if (plat_data->phy_addr == -1)
		plat_data->speed = SPEED_1000;
	else
		plat_data->speed = 0;

	if (mvebu_get_soc_id(&ctrl_model, &ctrl_rev)) {
		mvOsPrintf("%s: get soc_id failed\n", __func__);
		return NULL;
	}
	plat_data->ctrl_model = ctrl_model;
	plat_data->ctrl_rev = ctrl_rev;

	pdev->dev.platform_data = plat_data;

	clk = devm_clk_get(&pdev->dev, 0);
	clk_prepare_enable(clk);
#if defined(MY_ABC_HERE)
	plat_data->tclk = clk_get_rate(clk);
#endif  

	return plat_data;
}
#endif  

static int mv_eth_probe(struct platform_device *pdev)
{
	int port;

#ifdef CONFIG_OF
	struct mv_neta_pdata *plat_data = mv_plat_data_get(pdev);
	pdev->dev.platform_data = plat_data;
#else
	struct mv_neta_pdata *plat_data = (struct mv_neta_pdata *)pdev->dev.platform_data;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		pr_err("could not get IRQ number\n");
		return -ENODEV;
	}
	plat_data->irq = res->start;

	neta_cap_bitmap = 0x0;

#ifdef CONFIG_MV_ETH_PNC
	neta_cap_bitmap |= MV_ETH_CAP_PNC;
#endif

#if defined(MY_ABC_HERE)
#ifdef CONFIG_MV_ETH_BM_CPU
	neta_cap_bitmap |= MV_ETH_CAP_BM;
#endif
#else
#ifdef CONFIG_MV_ETH_BM
	neta_cap_bitmap |= MV_ETH_CAP_BM;
#endif
#endif  

#ifdef CONFIG_MV_ETH_HWF
	neta_cap_bitmap |= MV_ETH_CAP_HWF;
#endif

#ifdef CONFIG_MV_ETH_PME
	neta_cap_bitmap |= MV_ETH_CAP_PME;
#endif

#endif  

	if (plat_data == NULL)
		return -ENODEV;

	port = pdev->id;
	if (!mv_eth_initialized) {
		neta_global_dev = &pdev->dev;
		if (mv_eth_shared_probe(plat_data))
			return -ENODEV;
	}

#ifdef CONFIG_OF
	 
	mvEthPhySmiAddrSet(ETH_SMI_REG(port));
#endif

	pr_info("port #%d: is_sgmii=%d, is_rgmii=%d, phy_addr=%d\n",
		port, plat_data->is_sgmii, plat_data->is_rgmii, plat_data->phy_addr);
	mvNetaPortPowerUp(port, plat_data->is_sgmii, plat_data->is_rgmii, (plat_data->phy_addr == -1));

	mv_eth_win_init(port);

	if (mv_eth_load_network_interfaces(pdev))
		return -ENODEV;

	printk(KERN_ERR "\n");

	return 0;
}

static void mv_eth_tx_timeout(struct net_device *dev)
{
#ifdef CONFIG_MV_ETH_STAT_ERR
	struct eth_port *pp = MV_ETH_PRIV(dev);

	pp->stats.tx_timeout++;
#endif  

	printk(KERN_INFO "%s: tx timeout\n", dev->name);
}

struct net_device *mv_eth_netdev_init(struct platform_device *pdev)
{
	int cpu, i;
	struct net_device *dev;
	struct eth_port *pp;
	struct cpu_ctrl	*cpuCtrl;
	int port = pdev->id;
	struct mv_neta_pdata *plat_data = (struct mv_neta_pdata *)pdev->dev.platform_data;

	dev = alloc_etherdev_mq(sizeof(struct eth_port), CONFIG_MV_ETH_TXQ);
	if (!dev)
		return NULL;

	pp = (struct eth_port *)netdev_priv(dev);
	if (!pp)
		return NULL;

	memset(pp, 0, sizeof(struct eth_port));
	pp->dev = dev;
	pp->plat_data = plat_data;
	pp->cpu_mask = plat_data->cpu_mask;

	dev->mtu = plat_data->mtu;

	if (!is_valid_ether_addr(plat_data->mac_addr)) {
		mv_eth_get_mac_addr(port, plat_data->mac_addr);
		if (is_valid_ether_addr(plat_data->mac_addr)) {
			memcpy(dev->dev_addr, plat_data->mac_addr, MV_MAC_ADDR_SIZE);
			mac_src[port] = "hw config";
		} else {
#ifdef CONFIG_OF
			eth_hw_addr_random(dev);
			mac_src[port] = "random";
#else
			memset(dev->dev_addr, 0, MV_MAC_ADDR_SIZE);
			mac_src[port] = "invalid";
#endif  
		}
	} else {
		memcpy(dev->dev_addr, plat_data->mac_addr, MV_MAC_ADDR_SIZE);
		mac_src[port] = "platform";
	}
	dev->irq = plat_data->irq;

	dev->tx_queue_len = CONFIG_MV_ETH_TXQ_DESC;
	dev->watchdog_timeo = 5 * HZ;
	dev->netdev_ops = &mv_eth_netdev_ops;

	if (mv_eth_priv_init(pp, port)) {
		mv_eth_priv_cleanup(pp);
		return NULL;
	}

	for (i = 0; i < CONFIG_MV_ETH_NAPI_GROUPS; i++) {
		pp->napiGroup[i] = kmalloc(sizeof(struct napi_struct), GFP_KERNEL);
		memset(pp->napiGroup[i], 0, sizeof(struct napi_struct));
	}

	for (cpu = 0; cpu < nr_cpu_ids; cpu++) {
		cpuCtrl = pp->cpu_config[cpu];
		cpuCtrl->napiCpuGroup = 0;
		cpuCtrl->napi         = NULL;
	}

	if (pp->flags & MV_ETH_F_CONNECT_LINUX) {
		for (i = 0; i < CONFIG_MV_ETH_NAPI_GROUPS; i++)
			netif_napi_add(dev, pp->napiGroup[i], mv_eth_poll, pp->weight);
	}

	SET_ETHTOOL_OPS(dev, &mv_eth_tool_ops);

	SET_NETDEV_DEV(dev, &pdev->dev);

	if (pp->flags & MV_ETH_F_CONNECT_LINUX) {
		mv_eth_netdev_init_features(dev);
		if (register_netdev(dev)) {
			printk(KERN_ERR "failed to register %s\n", dev->name);
			free_netdev(dev);
			return NULL;
		} else {

#ifndef CONFIG_MV_ETH_GRO_DEF
			dev->features &= ~NETIF_F_GRO;
#endif  

			printk(KERN_ERR "    o %s, ifindex = %d, GbE port = %d", dev->name, dev->ifindex, pp->port);

			printk(KERN_CONT "\n");
#if defined(MY_ABC_HERE)
			printk("    o %s, phy chipid = %x, Support WOL = %d\n", dev->name, pp->phy_chip, syno_wol_support(pp));
#endif  
		}
	}
	platform_set_drvdata(pdev, pp->dev);

	return dev;
}

bool mv_eth_netdev_find(unsigned int dev_idx)
{
	int port;

	for (port = 0; port < mv_eth_ports_num; port++) {
		struct eth_port *pp = mv_eth_port_by_id(port);

		if (pp && pp->dev && (pp->dev->ifindex == dev_idx))
			return true;
	}
	return false;
}
EXPORT_SYMBOL(mv_eth_netdev_find);

int mv_eth_hal_init(struct eth_port *pp)
{
	int rxq, txp, txq, size, cpu;
	struct tx_queue *txq_ctrl;
	struct rx_queue *rxq_ctrl;

	if (!MV_PON_PORT(pp->port)) {
		int phyAddr;

		phyAddr = pp->plat_data->phy_addr;
		mvNetaPhyAddrSet(pp->port, phyAddr);
	}

	pp->port_ctrl = mvNetaPortInit(pp->port, pp->dev->dev.parent);
	if (!pp->port_ctrl) {
		printk(KERN_ERR "%s: failed to load port=%d\n", __func__, pp->port);
		return -ENODEV;
	}

	size = pp->txp_num * CONFIG_MV_ETH_TXQ * sizeof(struct tx_queue);
	pp->txq_ctrl = mvOsMalloc(size);
	if (!pp->txq_ctrl)
		goto oom;

	memset(pp->txq_ctrl, 0, size);

	for (txp = 0; txp < pp->txp_num; txp++) {
		for (txq = 0; txq < CONFIG_MV_ETH_TXQ; txq++) {
			txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];

			txq_ctrl->q = NULL;
			txq_ctrl->hwf_rxp = 0xFF;
			txq_ctrl->txp = txp;
			txq_ctrl->txq = txq;
			txq_ctrl->txq_size = CONFIG_MV_ETH_TXQ_DESC;
			txq_ctrl->txq_count = 0;
			txq_ctrl->bm_only = MV_FALSE;

			txq_ctrl->shadow_txq_put_i = 0;
			txq_ctrl->shadow_txq_get_i = 0;
			txq_ctrl->txq_done_pkts_coal = mv_ctrl_txdone;
			txq_ctrl->flags = MV_ETH_F_TX_SHARED;
			txq_ctrl->nfpCounter = 0;
			for_each_possible_cpu(cpu)
				txq_ctrl->cpu_owner[cpu] = 0;
		}
	}

	pp->rxq_ctrl = mvOsMalloc(CONFIG_MV_ETH_RXQ * sizeof(struct rx_queue));
	if (!pp->rxq_ctrl)
		goto oom;

	memset(pp->rxq_ctrl, 0, CONFIG_MV_ETH_RXQ * sizeof(struct rx_queue));

	for (rxq = 0; rxq < CONFIG_MV_ETH_RXQ; rxq++) {
		rxq_ctrl = &pp->rxq_ctrl[rxq];
		rxq_ctrl->rxq_size = CONFIG_MV_ETH_RXQ_DESC;
		rxq_ctrl->rxq_pkts_coal = CONFIG_MV_ETH_RX_COAL_PKTS;
		rxq_ctrl->rxq_time_coal = CONFIG_MV_ETH_RX_COAL_USEC;
#if defined(MY_ABC_HERE)
		atomic_set(&rxq_ctrl->missed, 0);
		atomic_set(&rxq_ctrl->refill_stop, 0);
#endif  
	}

	if (pp->flags & MV_ETH_F_MH)
		mvNetaMhSet(pp->port, MV_TAG_TYPE_MH);

	pp->autoneg_cfg  = AUTONEG_ENABLE;
	pp->speed_cfg    = SPEED_1000;
	pp->duplex_cfg  = DUPLEX_FULL;
	pp->advertise_cfg = 0x2f;
	pp->rx_time_coal_cfg = CONFIG_MV_ETH_RX_COAL_USEC;
	pp->rx_pkts_coal_cfg = CONFIG_MV_ETH_RX_COAL_PKTS;
	pp->tx_pkts_coal_cfg = mv_ctrl_txdone;
	pp->rx_time_low_coal_cfg = CONFIG_MV_ETH_RX_COAL_USEC >> 2;
	pp->rx_time_high_coal_cfg = CONFIG_MV_ETH_RX_COAL_USEC << 2;
	pp->rx_pkts_low_coal_cfg = CONFIG_MV_ETH_RX_COAL_PKTS;
	pp->rx_pkts_high_coal_cfg = CONFIG_MV_ETH_RX_COAL_PKTS;
	pp->pkt_rate_low_cfg = 1000;
	pp->pkt_rate_high_cfg = 50000;
	pp->rate_sample_cfg = 5;
	pp->rate_current = 0;  

	return 0;
oom:
	printk(KERN_ERR "%s: port=%d: out of memory\n", __func__, pp->port);
	return -ENODEV;
}

void mv_eth_config_show(void)
{
	 
#if (CONFIG_MV_ETH_PORTS_NUM > MV_ETH_MAX_PORTS)
#   error "CONFIG_MV_ETH_PORTS_NUM is large than MV_ETH_MAX_PORTS"
#endif

#if (CONFIG_MV_ETH_RXQ > MV_ETH_MAX_RXQ)
#   error "CONFIG_MV_ETH_RXQ is large than MV_ETH_MAX_RXQ"
#endif

#if CONFIG_MV_ETH_TXQ > MV_ETH_MAX_TXQ
#   error "CONFIG_MV_ETH_TXQ is large than MV_ETH_MAX_TXQ"
#endif

#if defined(CONFIG_MV_ETH_TSO) && !defined(CONFIG_MV_ETH_TX_CSUM_OFFLOAD)
#   error "If GSO enabled - TX checksum offload must be enabled too"
#endif

	pr_info("  o %d Giga ports supported\n", mv_eth_ports_num);

#ifdef CONFIG_MV_PON
	pr_info("  o Giga PON port is #%d: - %d TCONTs supported\n", MV_PON_PORT_ID, MV_ETH_MAX_TCONT());
#endif

#ifdef CONFIG_MV_NETA_SKB_RECYCLE
	pr_info("  o SKB recycle supported for SWF (%s)\n", mv_ctrl_swf_recycle ? "Enabled" : "Disabled");
#endif

#ifdef CONFIG_MV_ETH_NETA
	pr_info("  o NETA acceleration mode %d\n", mvNetaAccMode());
#endif

#ifdef CONFIG_MV_ETH_BM_CPU
	if (MV_NETA_BM_CAP())
		pr_info("  o BM supported for CPU: %d BM pools\n", MV_ETH_BM_POOLS);
#endif  

#ifdef CONFIG_MV_ETH_PNC
	if (MV_NETA_PNC_CAP())
		pr_info("  o PnC supported (%s)\n", mv_eth_pnc_ctrl_en ? "Enabled" : "Disabled");
#endif

#ifdef CONFIG_MV_ETH_HWF
	if (MV_NETA_HWF_CAP())
		pr_info("  o HWF supported\n");
#endif

#ifdef CONFIG_MV_ETH_PMT
	if (MV_NETA_PMT_CAP())
		pr_info("  o PMT supported\n");
#endif

	pr_info("  o RX Queue support: %d Queues * %d Descriptors\n", CONFIG_MV_ETH_RXQ, CONFIG_MV_ETH_RXQ_DESC);

	pr_info("  o TX Queue support: %d Queues * %d Descriptors\n", CONFIG_MV_ETH_TXQ, CONFIG_MV_ETH_TXQ_DESC);

#if defined(CONFIG_MV_ETH_TSO)
	pr_info("  o GSO supported\n");
#endif  

#if defined(CONFIG_MV_ETH_GRO)
	pr_info("  o GRO supported\n");
#endif  

#if defined(CONFIG_MV_ETH_RX_CSUM_OFFLOAD)
	pr_info("  o Receive checksum offload supported\n");
#endif
#if defined(CONFIG_MV_ETH_TX_CSUM_OFFLOAD)
	pr_info("  o Transmit checksum offload supported\n");
#endif

#if defined(CONFIG_MV_ETH_NFP_HOOKS)
	pr_info("  o NFP Hooks are supported\n");
#endif  

#if defined(CONFIG_MV_ETH_NFP_EXT)
	pr_info("  o NFP External drivers supported: up to %d interfaces\n", NFP_EXT_NUM);
#endif  

#ifdef CONFIG_MV_ETH_STAT_ERR
	pr_info("  o Driver ERROR statistics enabled\n");
#endif

#ifdef CONFIG_MV_ETH_STAT_INF
	pr_info("  o Driver INFO statistics enabled\n");
#endif

#ifdef CONFIG_MV_ETH_STAT_DBG
	pr_info("  o Driver DEBUG statistics enabled\n");
#endif

#ifdef ETH_DEBUG
	pr_info("  o Driver debug messages enabled\n");
#endif

#if defined(CONFIG_MV_ETH_SWITCH)
	pr_info("  o Switch support enabled\n");

#endif  

	pr_info("\n");
}

static void mv_eth_netdev_init_features(struct net_device *dev)
{
	dev->features |= NETIF_F_SG | NETIF_F_LLTX;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)
	dev->hw_features |= NETIF_F_SG;
#endif

#ifdef CONFIG_MV_ETH_PNC_L3_FLOW
	dev->features |= NETIF_F_NTUPLE;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)
	dev->hw_features |= NETIF_F_NTUPLE;
#endif
#endif  

#if defined(MV_ETH_PNC_LB) && defined(CONFIG_MV_ETH_PNC)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)
	if (MV_NETA_PNC_CAP())
		dev->hw_features |= NETIF_F_RXHASH;
#endif
#endif

#ifdef CONFIG_MV_ETH_TX_CSUM_OFFLOAD
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)
	dev->hw_features |= NETIF_F_IP_CSUM;
#endif
#ifdef CONFIG_MV_ETH_TX_CSUM_OFFLOAD_DEF
	dev->features |= NETIF_F_IP_CSUM;
#endif  
#endif  

#ifdef CONFIG_MV_ETH_RX_CSUM_OFFLOAD
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)
	dev->hw_features |= NETIF_F_RXCSUM;
#endif
#ifdef CONFIG_MV_ETH_RX_CSUM_OFFLOAD_DEF
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)
	dev->features |= NETIF_F_RXCSUM;
#endif
#endif  
#endif  

#ifdef CONFIG_MV_ETH_TSO
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)
	dev->hw_features |= NETIF_F_TSO;
#endif
#ifdef CONFIG_MV_ETH_TSO_DEF
	dev->features |= NETIF_F_TSO;
#endif  
#endif  
}

int mv_eth_napi_set_cpu_affinity(int port, int group, int affinity)
{
	struct eth_port *pp = mv_eth_port_by_id(port);
	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return -1;
	}

	if (group >= CONFIG_MV_ETH_NAPI_GROUPS) {
		printk(KERN_ERR "%s: group number is higher than %d\n", __func__, CONFIG_MV_ETH_NAPI_GROUPS-1);
		return -1;
		}
	if (pp->flags & MV_ETH_F_STARTED) {
		printk(KERN_ERR "Port %d must be stopped before\n", port);
		return -EINVAL;
	}
	set_cpu_affinity(pp, affinity, group);
	return 0;

}
void handle_group_affinity(int port)
{
	int group;
	struct eth_port *pp;
	MV_U32 group_cpu_affinity[CONFIG_MV_ETH_NAPI_GROUPS];
	MV_U32 rxq_affinity[CONFIG_MV_ETH_NAPI_GROUPS];

	group_cpu_affinity[0] = CONFIG_MV_ETH_GROUP0_CPU;
	rxq_affinity[0] 	  = CONFIG_MV_ETH_GROUP0_RXQ;

#ifdef CONFIG_MV_ETH_GROUP1_CPU
		group_cpu_affinity[1] = CONFIG_MV_ETH_GROUP1_CPU;
		rxq_affinity[1] 	  = CONFIG_MV_ETH_GROUP1_RXQ;
#endif

#ifdef CONFIG_MV_ETH_GROUP2_CPU
		group_cpu_affinity[2] = CONFIG_MV_ETH_GROUP2_CPU;
		rxq_affinity[2] 	  = CONFIG_MV_ETH_GROUP2_RXQ;
#endif

#ifdef CONFIG_MV_ETH_GROUP3_CPU
		group_cpu_affinity[3] = CONFIG_MV_ETH_GROUP3_CPU;
		rxq_affinity[3] 	  = CONFIG_MV_ETH_GROUP3_RXQ;
#endif

	pp = mv_eth_port_by_id(port);
	if (pp == NULL)
		return;

	for (group = 0; group < CONFIG_MV_ETH_NAPI_GROUPS; group++)
		set_cpu_affinity(pp, group_cpu_affinity[group], group);
	for (group = 0; group < CONFIG_MV_ETH_NAPI_GROUPS; group++)
		set_rxq_affinity(pp, rxq_affinity[group], group);
}

int	mv_eth_napi_set_rxq_affinity(int port, int group, int rxqAffinity)
{
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (pp == NULL) {
		printk(KERN_ERR "%s: pp is null \n", __func__);
		return MV_FAIL;
	}
	if (group >= CONFIG_MV_ETH_NAPI_GROUPS) {
		printk(KERN_ERR "%s: group number is higher than %d\n", __func__, CONFIG_MV_ETH_NAPI_GROUPS-1);
		return -1;
		}
	if (pp->flags & MV_ETH_F_STARTED) {
		printk(KERN_ERR "Port %d must be stopped before\n", port);
		return -EINVAL;
	}

	set_rxq_affinity(pp, rxqAffinity, group);
	return 0;
}

void mv_eth_napi_group_show(int port)
{
	int cpu, group;
	struct cpu_ctrl	*cpuCtrl;
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return;
	}
	for (group = 0; group < CONFIG_MV_ETH_NAPI_GROUPS; group++) {
		printk(KERN_INFO "group=%d:\n", group);
		for (cpu = 0; cpu < nr_cpu_ids; cpu++) {
			cpuCtrl = pp->cpu_config[cpu];
			if (!(MV_BIT_CHECK(pp->cpu_mask, cpu)))
				continue;
			if (cpuCtrl->napiCpuGroup == group) {
				printk(KERN_INFO "   CPU%d ", cpu);
				mvNetaCpuDump(port, cpu, 0);
				printk(KERN_INFO "\n");
			}
		}
		printk(KERN_INFO "\n");
	}
}

void mv_eth_priv_cleanup(struct eth_port *pp)
{
	 
}

#ifdef CONFIG_MV_ETH_BM_CPU
static struct bm_pool *mv_eth_long_pool_get(struct eth_port *pp, int pkt_size)
{
	int             pool, i;
	struct bm_pool	*bm_pool, *temp_pool = NULL;
	unsigned long   flags = 0;

	if (MV_NETA_BM_CAP()) {
		pool = mv_eth_bm_config_long_pool_get(pp->port);
		if (pool != -1)  
			return &mv_eth_pool[pool];

		for (i = 0; i < MV_ETH_BM_POOLS; i++) {
			pool = (pp->port + i) % MV_ETH_BM_POOLS;
			bm_pool = &mv_eth_pool[pool];

			MV_ETH_LOCK(&bm_pool->lock, flags);

			if (bm_pool->pkt_size == 0) {
				 
				MV_ETH_UNLOCK(&bm_pool->lock, flags);
				return bm_pool;
			}
			if (bm_pool->pkt_size >= pkt_size) {
				if (temp_pool == NULL)
					temp_pool = bm_pool;
				else if (bm_pool->pkt_size < temp_pool->pkt_size)
					temp_pool = bm_pool;
			}
			MV_ETH_UNLOCK(&bm_pool->lock, flags);
		}
		return temp_pool;
	} else {
		return &mv_eth_pool[pp->port];
	}
}
#else
static struct bm_pool *mv_eth_long_pool_get(struct eth_port *pp, int pkt_size)
{
	return &mv_eth_pool[pp->port];
}
#endif  

static int mv_eth_no_bm_cpu_rxq_fill(struct eth_port *pp, int rxq, int num)
{
	int i;
	struct sk_buff *skb;
	struct bm_pool *bm_pool;
	MV_NETA_RXQ_CTRL *rx_ctrl;
	struct neta_rx_desc *rx_desc;
	phys_addr_t pa;

	bm_pool = pp->pool_long;

	rx_ctrl = pp->rxq_ctrl[rxq].q;
	if (!rx_ctrl) {
		printk(KERN_ERR "%s: rxq %d is not initialized\n", __func__, rxq);
		return 0;
	}

	for (i = 0; i < num; i++) {
		skb = mv_eth_pool_get(pp, bm_pool);
		if (skb) {
			rx_desc = (struct neta_rx_desc *)MV_NETA_QUEUE_DESC_PTR(&rx_ctrl->queueCtrl, i);
			memset(rx_desc, 0, sizeof(struct neta_rx_desc));
			pa = virt_to_phys(skb->head);
			mvNetaRxDescFill(rx_desc, (MV_U32)pa, (MV_U32)skb);
			mvOsCacheLineFlush(pp->dev->dev.parent, rx_desc);
		} else {
			printk(KERN_ERR "%s: rxq %d, %d of %d buffers are filled\n", __func__, rxq, i, num);
			break;
		}
	}

	return i;
}

static int mv_eth_rxq_fill(struct eth_port *pp, int rxq, int num)
{
	int i;

#ifndef CONFIG_MV_ETH_BM_CPU
	i = mv_eth_no_bm_cpu_rxq_fill(pp, rxq, num);
#else
	if (MV_NETA_BM_CAP())
		i = num;
	else
		i = mv_eth_no_bm_cpu_rxq_fill(pp, rxq, num);
#endif  

	mvNetaRxqNonOccupDescAdd(pp->port, rxq, i);

	return i;
}

static int mv_eth_txq_create(struct eth_port *pp, struct tx_queue *txq_ctrl)
{
	txq_ctrl->q = mvNetaTxqInit(pp->port, txq_ctrl->txp, txq_ctrl->txq, txq_ctrl->txq_size);
	if (txq_ctrl->q == NULL) {
		printk(KERN_ERR "%s: can't create TxQ - port=%d, txp=%d, txq=%d, desc=%d\n",
		       __func__, pp->port, txq_ctrl->txp, txq_ctrl->txp, txq_ctrl->txq_size);
		return -ENODEV;
	}

	txq_ctrl->shadow_txq = mvOsMalloc(txq_ctrl->txq_size * sizeof(MV_ULONG));
	if (txq_ctrl->shadow_txq == NULL)
		goto no_mem;

	txq_ctrl->txq_count = 0;
	txq_ctrl->shadow_txq_put_i = 0;
	txq_ctrl->shadow_txq_get_i = 0;

#ifdef CONFIG_MV_ETH_HWF
	if (MV_NETA_HWF_CAP())
		mvNetaHwfTxqInit(pp->port, txq_ctrl->txp, txq_ctrl->txq);
#endif  

	return 0;

no_mem:
	mv_eth_txq_delete(pp, txq_ctrl);
	return -ENOMEM;
}

static int mv_force_port_link_speed_fc(int port, MV_ETH_PORT_SPEED port_speed, int en_force)
{
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (en_force) {
		if (mvNetaForceLinkModeSet(port, 1, 0)) {
			printk(KERN_ERR "mvNetaForceLinkModeSet failed\n");
			return -EIO;
		}
		if (mvNetaSpeedDuplexSet(port, port_speed, MV_ETH_DUPLEX_FULL)) {
			printk(KERN_ERR "mvNetaSpeedDuplexSet failed\n");
			return -EIO;
		}
		if (mvNetaFlowCtrlSet(port, MV_ETH_FC_ENABLE)) {
			printk(KERN_ERR "mvNetaFlowCtrlSet failed\n");
			return -EIO;
		}
		printk(KERN_ERR "*************FORCE LINK**************\n");
		set_bit(MV_ETH_F_FORCE_LINK_BIT, &(pp->flags));

	} else {
		if (mvNetaForceLinkModeSet(port, 0, 0)) {
			printk(KERN_ERR "mvNetaForceLinkModeSet failed\n");
			return -EIO;
		}
		if (mvNetaSpeedDuplexSet(port, MV_ETH_SPEED_AN, MV_ETH_DUPLEX_AN)) {
			printk(KERN_ERR "mvNetaSpeedDuplexSet failed\n");
			return -EIO;
		}
		if (mvNetaFlowCtrlSet(port, MV_ETH_FC_AN_SYM)) {
			printk(KERN_ERR "mvNetaFlowCtrlSet failed\n");
			return -EIO;
		}
		printk(KERN_ERR "*************CLEAR FORCE LINK**************\n");

		clear_bit(MV_ETH_F_FORCE_LINK_BIT, &(pp->flags));
	}
	return 0;
}

static void mv_eth_txq_delete(struct eth_port *pp, struct tx_queue *txq_ctrl)
{
	if (txq_ctrl->shadow_txq) {
		mvOsFree(txq_ctrl->shadow_txq);
		txq_ctrl->shadow_txq = NULL;
	}

	if (txq_ctrl->q) {
		mvNetaTxqDelete(pp->port, txq_ctrl->txp, txq_ctrl->txq);
		txq_ctrl->q = NULL;
	}
}

int mv_eth_txp_reset(int port, int txp)
{
	struct eth_port *pp;
	int queue;

	if (mvNetaTxpCheck(port, txp))
		return -EINVAL;

	pp = mv_eth_port_by_id(port);
	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return -ENODEV;
	}

	if (pp->flags & MV_ETH_F_STARTED) {
		printk(KERN_ERR "Port %d must be stopped before\n", port);
		return -EINVAL;
	}

	for (queue = 0; queue < CONFIG_MV_ETH_TXQ; queue++) {
		struct tx_queue *txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + queue];

		if (txq_ctrl->q) {
			int mode, rx_port;

			mode = mv_eth_ctrl_txq_mode_get(pp->port, txp, queue, &rx_port);
#if defined(MY_ABC_HERE)
			 
			if (mode != MV_ETH_TXQ_HWF) {
#else
			if (mode == MV_ETH_TXQ_CPU) {
#endif  
				 
				mv_eth_txq_done_force(pp, txq_ctrl);
				 
				txq_ctrl->shadow_txq_put_i = 0;
				txq_ctrl->shadow_txq_get_i = 0;
			}
#ifdef CONFIG_MV_ETH_HWF
#if defined(MY_ABC_HERE)
			else if (MV_NETA_HWF_CAP())
#else
			else if (mode == MV_ETH_TXQ_HWF && MV_NETA_HWF_CAP())
#endif  
				mv_eth_txq_hwf_clean(pp, txq_ctrl, rx_port);
#endif  
#if defined(MY_ABC_HERE)
			 
#else
			else
				printk(KERN_ERR "%s: port=%d, txp=%d, txq=%d is not in use\n",
						__func__, pp->port, txp, queue);
#endif  
		}
	}
	mvNetaTxpReset(port, txp);

	return 0;
}

int mv_eth_rx_reset(int port)
{
	struct eth_port *pp = mv_eth_port_by_id(port);
	int rxq = 0;

	if (pp->flags & MV_ETH_F_STARTED) {
		printk(KERN_ERR "Port %d must be stopped before\n", port);
		return -EINVAL;
	}

#ifndef CONFIG_MV_ETH_BM_CPU
	{
		for (rxq = 0; rxq < CONFIG_MV_ETH_RXQ; rxq++) {
#if defined(MY_ABC_HERE)
			struct sk_buff *skb;
#else
			struct eth_pbuf *pkt;
#endif  
			struct neta_rx_desc *rx_desc;
#if defined(MY_ABC_HERE)
			 
#else
			struct bm_pool *pool;
#endif  
			int i, rx_done;
			MV_NETA_RXQ_CTRL *rx_ctrl = pp->rxq_ctrl[rxq].q;

			if (rx_ctrl == NULL)
				continue;

			rx_done = mvNetaRxqFreeDescNumGet(pp->port, rxq);
			mvOsCacheIoSync(pp->dev->dev.parent);
			for (i = 0; i < rx_done; i++) {
				rx_desc = mvNetaRxqNextDescGet(rx_ctrl);
				mvOsCacheLineInv(pp->dev->dev.parent, rx_desc);

#if defined(MV_CPU_BE)
				mvNetaRxqDescSwap(rx_desc);
#endif  

#if defined(MY_ABC_HERE)
				skb = (struct sk_buff *)rx_desc->bufCookie;
				mv_eth_pool_put(pp->pool_long, skb);
#else
				pkt = (struct eth_pbuf *)rx_desc->bufCookie;
				pool = &mv_eth_pool[pkt->pool];
				mv_eth_pool_put(pool, pkt);
#endif  
			}
		}
	}
#else
	if (!MV_NETA_BM_CAP()) {
		for (rxq = 0; rxq < CONFIG_MV_ETH_RXQ; rxq++) {
#if defined(MY_ABC_HERE)
			struct sk_buff *skb;
#else
			struct eth_pbuf *pkt;
#endif  
			struct neta_rx_desc *rx_desc;
			struct bm_pool *pool;
			int i, rx_done;
			MV_NETA_RXQ_CTRL *rx_ctrl = pp->rxq_ctrl[rxq].q;

			if (rx_ctrl == NULL)
				continue;

			rx_done = mvNetaRxqFreeDescNumGet(pp->port, rxq);
			mvOsCacheIoSync(pp->dev->dev.parent);
			for (i = 0; i < rx_done; i++) {
				rx_desc = mvNetaRxqNextDescGet(rx_ctrl);
				mvOsCacheLineInv(pp->dev->dev.parent, rx_desc);

#if defined(MV_CPU_BE)
				mvNetaRxqDescSwap(rx_desc);
#endif  
#if defined(MY_ABC_HERE)
				skb = (struct sk_buff *)rx_desc->bufCookie;
				if (pp->pool_short && (skb->data_len <= pp->pool_short->pkt_size))
					pool = pp->pool_short;
				else
					pool = pp->pool_long;
				mv_eth_pool_put(pool, skb);
#else
				pkt = (struct eth_pbuf *)rx_desc->bufCookie;
				pool = &mv_eth_pool[pkt->pool];
				mv_eth_pool_put(pool, pkt);
#endif  
			}
		}
	}
#endif  

	mvNetaRxReset(port);
	return 0;
}

MV_STATUS mv_eth_rx_pkts_coal_set(int port, int rxq, MV_U32 value)
{
	MV_STATUS status;
	struct eth_port *pp = mv_eth_port_by_id(port);

	status = mvNetaRxqPktsCoalSet(port, rxq, value);
	if (status == MV_OK)
		pp->rxq_ctrl[rxq].rxq_pkts_coal = value;
	return status;
}

MV_STATUS mv_eth_rx_time_coal_set(int port, int rxq, MV_U32 value)
{
	MV_STATUS status;
	struct eth_port *pp = mv_eth_port_by_id(port);

	status = mvNetaRxqTimeCoalSet(port, rxq, value);
	if (status == MV_OK)
		pp->rxq_ctrl[rxq].rxq_time_coal = value;
	return status;
}

MV_STATUS mv_eth_tx_done_pkts_coal_set(int port, int txp, int txq, MV_U32 value)
{
	MV_STATUS status;
	struct eth_port *pp = mv_eth_port_by_id(port);

	status = mvNetaTxDonePktsCoalSet(port, txp, txq, value);
	if (status == MV_OK)
		pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq].txq_done_pkts_coal = value;
	return status;
}

int mv_eth_start_internals(struct eth_port *pp, int mtu)
{
	unsigned int status;
	struct cpu_ctrl	*cpuCtrl;
	int rxq, txp, txq, num, err = 0;
	int pkt_size = RX_PKT_SIZE(mtu);

	if (test_bit(MV_ETH_F_STARTED_BIT, &(pp->flags))) {
		STAT_ERR(pp->stats.state_err++);
		printk(KERN_ERR "%s: port %d, wrong state: STARTED_BIT = 1\n", __func__, pp->port);
		err = -EINVAL;
		goto out;
	}

	if (mvNetaMaxRxSizeSet(pp->port, RX_PKT_SIZE(mtu))) {
		printk(KERN_ERR "%s: can't set maxRxSize=%d for port=%d, mtu=%d\n",
		       __func__, RX_PKT_SIZE(mtu), pp->port, mtu);
		err = -EINVAL;
		goto out;
	}

	if (mv_eth_ctrl_is_tx_enabled(pp)) {
		int cpu;
		for_each_possible_cpu(cpu) {
			if (!(MV_BIT_CHECK(pp->cpu_mask, cpu)))
				continue;

			cpuCtrl = pp->cpu_config[cpu];

			if (!(MV_BIT_CHECK(cpuCtrl->cpuTxqMask, cpuCtrl->txq))) {
				printk(KERN_ERR "%s: error , port #%d txq #%d is masked for cpu #%d (mask= 0X%x).\n",
					__func__, pp->port, cpuCtrl->txq, cpu, cpuCtrl->cpuTxqMask);
				err = -EINVAL;
				goto out;
			}

			if (mv_eth_ctrl_txq_cpu_own(pp->port, pp->txp, cpuCtrl->txq, 1, cpu) < 0) {
				err = -EINVAL;
				goto out;
			}
		}
	}

	if (pp->pool_long == NULL) {
		struct bm_pool *new_pool;

		new_pool = mv_eth_long_pool_get(pp, pkt_size);
		if (new_pool == NULL) {
			printk(KERN_ERR "%s FAILED: port=%d, Can't find pool for pkt_size=%d\n",
			       __func__, pp->port, pkt_size);
			err = -ENOMEM;
			goto out;
		}
		if (new_pool->pkt_size == 0) {
			new_pool->pkt_size = pkt_size;
#ifdef CONFIG_MV_ETH_BM_CPU
			if (MV_NETA_BM_CAP())
				mvNetaBmPoolBufferSizeSet(new_pool->pool, RX_BUF_SIZE(pkt_size));
#endif  
		}
		if (new_pool->pkt_size < pkt_size) {
			printk(KERN_ERR "%s FAILED: port=%d, long pool #%d, pkt_size=%d less than required %d\n",
					__func__, pp->port, new_pool->pool, new_pool->pkt_size, pkt_size);
			err = -ENOMEM;
			goto out;
		}
		pp->pool_long = new_pool;
		pp->pool_long->port_map |= (1 << pp->port);

		num = mv_eth_pool_add(pp, pp->pool_long->pool, pp->pool_long_num);
		if (num != pp->pool_long_num) {
			printk(KERN_ERR "%s FAILED: mtu=%d, pool=%d, pkt_size=%d, only %d of %d allocated\n",
			       __func__, mtu, pp->pool_long->pool, pp->pool_long->pkt_size, num, pp->pool_long_num);
			err = -ENOMEM;
			goto out;
		}
	}

#ifdef CONFIG_MV_ETH_BM_CPU
	if (MV_NETA_BM_CAP()) {
		mvNetaBmPoolBufSizeSet(pp->port, pp->pool_long->pool, RX_BUF_SIZE(pp->pool_long->pkt_size));

		if (pp->pool_short == NULL) {
			int short_pool = mv_eth_bm_config_short_pool_get(pp->port);

			if (short_pool < 0) {
				err = -EINVAL;
				goto out;
			}
			pp->pool_short = &mv_eth_pool[short_pool];
			pp->pool_short->port_map |= (1 << pp->port);
			if (pp->pool_short->pool != pp->pool_long->pool) {
				num = mv_eth_pool_add(pp, pp->pool_short->pool, pp->pool_short_num);
				if (num != pp->pool_short_num) {
					pr_err("%s FAILED: pool=%d, pkt_size=%d - %d of %d buffers added\n",
						 __func__, short_pool, pp->pool_short->pkt_size, num,
						 pp->pool_short_num);
					err = -ENOMEM;
					goto out;
				}
				mvNetaBmPoolBufSizeSet(pp->port, pp->pool_short->pool,
							 RX_BUF_SIZE(pp->pool_short->pkt_size));
			} else {
				int dummy_short_pool = (pp->pool_short->pool + 1) % MV_BM_POOLS;

				mvNetaBmPoolBufSizeSet(pp->port, dummy_short_pool, NET_SKB_PAD);
			}
		}
	}
#endif  

	for (rxq = 0; rxq < CONFIG_MV_ETH_RXQ; rxq++) {
		if (pp->rxq_ctrl[rxq].q == NULL) {
			pp->rxq_ctrl[rxq].q = mvNetaRxqInit(pp->port, rxq, pp->rxq_ctrl[rxq].rxq_size);
			if (!pp->rxq_ctrl[rxq].q) {
				printk(KERN_ERR "%s: can't create RxQ port=%d, rxq=%d, desc=%d\n",
				       __func__, pp->port, rxq, pp->rxq_ctrl[rxq].rxq_size);
				err = -ENODEV;
				goto out;
			}
		}

		mvNetaRxqOffsetSet(pp->port, rxq, NET_SKB_PAD);

		mv_eth_rx_pkts_coal_set(pp->port, rxq, pp->rxq_ctrl[rxq].rxq_pkts_coal);
		mv_eth_rx_time_coal_set(pp->port, rxq, pp->rxq_ctrl[rxq].rxq_time_coal);

#ifdef CONFIG_MV_ETH_BM_CPU
		if (MV_NETA_BM_CAP()) {
			 
			if (pp->pool_short->pool == pp->pool_long->pool) {
				int dummy_short_pool = (pp->pool_short->pool + 1) % MV_BM_POOLS;

				mvNetaRxqBmEnable(pp->port, rxq, dummy_short_pool, pp->pool_long->pool);
			} else
				mvNetaRxqBmEnable(pp->port, rxq, pp->pool_short->pool, pp->pool_long->pool);
		} else {
			 
			mvNetaRxqBufSizeSet(pp->port, rxq, RX_BUF_SIZE(pkt_size));
			mvNetaRxqBmDisable(pp->port, rxq);
		}
#else
		 
		mvNetaRxqBufSizeSet(pp->port, rxq, RX_BUF_SIZE(pkt_size));
		mvNetaRxqBmDisable(pp->port, rxq);
#endif  

		if (!(pp->flags & MV_ETH_F_IFCAP_NETMAP)) {
			if ((mvNetaRxqFreeDescNumGet(pp->port, rxq) == 0))
				mv_eth_rxq_fill(pp, rxq, pp->rxq_ctrl[rxq].rxq_size);
		} else {
			mvNetaRxqNonOccupDescAdd(pp->port, rxq, pp->rxq_ctrl[rxq].rxq_size);
#ifdef CONFIG_NETMAP
			if (neta_netmap_rxq_init_buffers(pp, rxq))
				return MV_ERROR;
#endif
		}
	}

	for (txp = 0; txp < pp->txp_num; txp++) {
		for (txq = 0; txq < CONFIG_MV_ETH_TXQ; txq++) {
			struct tx_queue *txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];

			if ((txq_ctrl->q == NULL) && (txq_ctrl->txq_size > 0)) {
				err = mv_eth_txq_create(pp, txq_ctrl);
				if (err)
					goto out;
				spin_lock_init(&txq_ctrl->queue_lock);
			}
			mv_eth_tx_done_pkts_coal_set(pp->port, txp, txq,
					pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq].txq_done_pkts_coal);
#ifdef CONFIG_NETMAP
			if (neta_netmap_txq_init_buffers(pp, txp, txq))
				return MV_ERROR;
#endif  

		}
		mvNetaTxpMaxTxSizeSet(pp->port, txp, RX_PKT_SIZE(mtu));
	}

#ifdef CONFIG_MV_ETH_HWF
	if (MV_NETA_HWF_CAP()) {
#ifdef CONFIG_MV_ETH_BM_CPU
		if (MV_NETA_BM_CAP())
			mvNetaHwfBmPoolsSet(pp->port, pp->pool_short->pool, pp->pool_long->pool);
		else
			mv_eth_hwf_bm_create(pp->port, RX_PKT_SIZE(mtu));
#else
		mv_eth_hwf_bm_create(pp->port, RX_PKT_SIZE(mtu));
#endif  

		mvNetaHwfEnable(pp->port, 1);
	}
#endif  

	status = mvNetaPortEnable(pp->port);
	if (status == MV_OK)
		set_bit(MV_ETH_F_LINK_UP_BIT, &(pp->flags));
#ifdef CONFIG_MV_PON
	else if (MV_PON_PORT(pp->port) && (mv_pon_link_status() == MV_TRUE)) {
		mvNetaPortUp(pp->port);
		set_bit(MV_ETH_F_LINK_UP_BIT, &(pp->flags));
	}
#endif  

	set_bit(MV_ETH_F_STARTED_BIT, &(pp->flags));

 out:
	return err;
}

int mv_eth_resume_internals(struct eth_port *pp, int mtu)
{

	unsigned int status;

	mvNetaMaxRxSizeSet(pp->port, RX_PKT_SIZE(mtu));

#ifdef CONFIG_MV_ETH_HWF
	if (MV_NETA_HWF_CAP()) {
#ifdef CONFIG_MV_ETH_BM_CPU
		if (MV_NETA_BM_CAP()) {
			if (pp->pool_long && pp->pool_short)
				mvNetaHwfBmPoolsSet(pp->port, pp->pool_short->pool, pp->pool_long->pool);
		} else {
			 
			mv_eth_hwf_bm_create(pp->port, RX_PKT_SIZE(mtu));
		}
#else
		 
		mv_eth_hwf_bm_create(pp->port, RX_PKT_SIZE(mtu));
#endif  
		mvNetaHwfEnable(pp->port, 1);
	}

#endif  

	status = mvNetaPortEnable(pp->port);
	if (status == MV_OK)
		set_bit(MV_ETH_F_LINK_UP_BIT, &(pp->flags));

#ifdef CONFIG_MV_PON
	else if (MV_PON_PORT(pp->port) && (mv_pon_link_status() == MV_TRUE)) {
		mvNetaPortUp(pp->port);
		set_bit(MV_ETH_F_LINK_UP_BIT, &(pp->flags));
	}
#endif  

	return MV_OK;

}

int mv_eth_suspend_internals(struct eth_port *pp)
{
	int cpu;

	if (mvNetaPortDisable(pp->port) != MV_OK) {
		printk(KERN_ERR "%s: GbE port %d: mvNetaPortDisable failed\n", __func__, pp->port);
		return MV_ERROR;
	}

	on_each_cpu(mv_eth_interrupts_mask, pp, 1);

	for_each_possible_cpu(cpu) {
		pp->cpu_config[cpu]->causeRxTx = 0;
		if (pp->cpu_config[cpu]->napi)
			 
			if (test_bit(MV_ETH_F_STARTED_OLD_BIT, &(pp->flags)))
				napi_synchronize(pp->cpu_config[cpu]->napi);
	}

	mdelay(10);

	return MV_OK;
}

int mv_eth_stop_internals(struct eth_port *pp)
{

	int txp, queue;
	struct cpu_ctrl	*cpuCtrl;

	if (!test_and_clear_bit(MV_ETH_F_STARTED_BIT, &(pp->flags))) {
		STAT_ERR(pp->stats.state_err++);
		printk(KERN_ERR "%s: port %d, wrong state: STARTED_BIT = 0.\n", __func__, pp->port);
		goto error;
	}

	if (mvNetaPortDisable(pp->port) != MV_OK) {
		printk(KERN_ERR "GbE port %d: ethPortDisable failed\n", pp->port);
		goto error;
	}

	on_each_cpu(mv_eth_interrupts_mask, pp, 1);

	mdelay(10);

#ifdef CONFIG_MV_ETH_HWF
	if (MV_NETA_HWF_CAP())
		mvNetaHwfEnable(pp->port, 0);
#endif  

	for (txp = 0; txp < pp->txp_num; txp++)
		for (queue = 0; queue < CONFIG_MV_ETH_TXQ; queue++)
			mv_eth_txq_clean(pp->port, txp, queue);

	if (mv_eth_ctrl_is_tx_enabled(pp)) {
		int cpu;
		for_each_possible_cpu(cpu) {
			cpuCtrl = pp->cpu_config[cpu];
			if (MV_BIT_CHECK(pp->cpu_mask, cpu))
				if (MV_BIT_CHECK(cpuCtrl->cpuTxqMask, cpuCtrl->txq))
					mv_eth_ctrl_txq_cpu_own(pp->port, pp->txp, cpuCtrl->txq, 0, cpu);
		}
	}

	for (queue = 0; queue < CONFIG_MV_ETH_RXQ; queue++)
		mv_eth_rxq_drop_pkts(pp, queue);

	return 0;

error:
	printk(KERN_ERR "GbE port %d: stop internals failed\n", pp->port);
	return -1;
}

int mv_eth_check_mtu_valid(struct net_device *dev, int mtu)
{
	if (mtu < 68) {
		pr_err("MTU must be at least 68, change mtu failed\n");
		return -EINVAL;
	}
	if (mtu > 9676  ) {
		pr_err("%s: Illegal MTU value %d, ", dev->name, mtu);
		mtu = 9676;
		pr_cont(" rounding MTU to: %d\n", mtu);
	}
	return mtu;
}

int mv_eth_check_mtu_internals(struct net_device *dev, int mtu)
{
	struct eth_port *pp = MV_ETH_PRIV(dev);
	struct bm_pool	*new_pool = NULL;

	new_pool = mv_eth_long_pool_get(pp, RX_PKT_SIZE(mtu));

	if (new_pool == NULL) {
		printk(KERN_ERR "%s: No BM pool available for MTU=%d\n", __func__, mtu);
		return -EPERM;
	}
#ifdef CONFIG_MV_ETH_BM_CPU
	if (MV_NETA_BM_CAP() && new_pool->pkt_size < RX_PKT_SIZE(mtu)) {
		if (mv_eth_bm_config_pkt_size_get(new_pool->pool) != 0) {
			printk(KERN_ERR "%s: BM pool #%d - pkt_size = %d less than required for MTU=%d and can't be changed\n",
						__func__, new_pool->pool, new_pool->pkt_size, mtu);
			return -EPERM;
		}
		 
		if ((new_pool == pp->pool_long) && (pp->pool_long->port_map != (1 << pp->port))) {
			 
			printk(KERN_ERR "%s: bmPool=%d is shared port_map=0x%x. Stop all ports uses this pool before change MTU\n",
						__func__, pp->pool_long->pool, pp->pool_long->port_map);
			return -EPERM;
		}
	}
#endif  
	return 0;
}

int mv_eth_change_mtu_internals(struct net_device *dev, int mtu)
{
	struct bm_pool	*new_pool = NULL;
	struct eth_port *pp = MV_ETH_PRIV(dev);
	int             config_pkt_size;

	if (test_bit(MV_ETH_F_STARTED_BIT, &(pp->flags))) {
		STAT_ERR(pp->stats.state_err++);
		if (pp->flags & MV_ETH_F_DBG_RX)
			printk(KERN_ERR "%s: port %d, STARTED_BIT = 0, Invalid value.\n", __func__, pp->port);
		return MV_ERROR;
	}

	if ((mtu != dev->mtu) && (pp->pool_long)) {
		 
		config_pkt_size = 0;
#ifdef CONFIG_MV_ETH_BM_CPU
		if (MV_NETA_BM_CAP()) {
			new_pool = mv_eth_long_pool_get(pp, RX_PKT_SIZE(mtu));
			if (new_pool != NULL)
				config_pkt_size = mv_eth_bm_config_pkt_size_get(new_pool->pool);
		} else {
			 
			new_pool = NULL;
		}
#else
		 
		new_pool = NULL;
#endif  

		if ((new_pool == NULL) || (new_pool->pkt_size < RX_PKT_SIZE(mtu)) || (pp->pool_long != new_pool) ||
			((new_pool->pkt_size > RX_PKT_SIZE(mtu)) && (config_pkt_size == 0))) {
			mv_eth_rx_reset(pp->port);
			mv_eth_pool_free(pp->pool_long->pool, pp->pool_long_num);

#ifdef CONFIG_MV_ETH_BM_CPU
			if (MV_NETA_BM_CAP()) {
				 
				if (pp->pool_long->buf_num == 0) {
					pp->pool_long->pkt_size = config_pkt_size;

					if (pp->pool_long->pkt_size == 0)
						mvNetaBmPoolBufferSizeSet(pp->pool_long->pool, 0);
					else
						mvNetaBmPoolBufferSizeSet(pp->pool_long->pool,
									  RX_BUF_SIZE(pp->pool_long->pkt_size));
				}
			} else {
				pp->pool_long->pkt_size = config_pkt_size;
			}
#else
			pp->pool_long->pkt_size = config_pkt_size;
#endif  

			pp->pool_long->port_map &= ~(1 << pp->port);
			pp->pool_long = NULL;
		}

	}
	dev->mtu = mtu;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
	netdev_update_features(dev);
#else
	netdev_features_change(dev);
#endif

	return 0;
}

#ifdef CONFIG_MV_NETA_TXDONE_IN_HRTIMER
 
enum hrtimer_restart mv_eth_tx_done_hr_timer_callback(struct hrtimer *timer)
{
	struct cpu_ctrl *cpuCtrl = container_of(timer, struct cpu_ctrl, tx_done_timer);

	tasklet_schedule(&cpuCtrl->tx_done_tasklet);

	return HRTIMER_NORESTART;
}
#endif

#ifndef CONFIG_MV_NETA_TXDONE_ISR
 
static void mv_eth_tx_done_timer_callback(unsigned long data)
{
	struct cpu_ctrl *cpuCtrl = (struct cpu_ctrl *)data;
	struct eth_port *pp = cpuCtrl->pp;
	int tx_done = 0, tx_todo = 0;
	unsigned int txq_mask;

	STAT_INFO(pp->stats.tx_done_timer_event[smp_processor_id()]++);

	clear_bit(MV_ETH_F_TX_DONE_TIMER_BIT, &(cpuCtrl->flags));

	if (!test_bit(MV_ETH_F_STARTED_BIT, &(pp->flags))) {
		STAT_INFO(pp->stats.netdev_stop++);

#ifdef CONFIG_MV_NETA_DEBUG_CODE
		if (pp->flags & MV_ETH_F_DBG_TX)
			printk(KERN_ERR "%s: port #%d is stopped, STARTED_BIT = 0, exit timer.\n", __func__, pp->port);
#endif  

		return;
	}

	if (MV_PON_PORT(pp->port))
		tx_done = mv_eth_tx_done_pon(pp, &tx_todo);
	else {
		 
		txq_mask = ((1 << CONFIG_MV_ETH_TXQ) - 1) & cpuCtrl->cpuTxqOwner;
		tx_done = mv_eth_tx_done_gbe(pp, txq_mask, &tx_todo);
	}

	if (cpuCtrl->cpu != smp_processor_id()) {
		pr_warning("%s: Called on other CPU - %d != %d\n", __func__, cpuCtrl->cpu, smp_processor_id());
		cpuCtrl = pp->cpu_config[smp_processor_id()];
	}
	if (tx_todo > 0)
		mv_eth_add_tx_done_timer(cpuCtrl);
}
#endif  

static void mv_eth_cleanup_timer_callback(unsigned long data)
{
	struct cpu_ctrl *cpuCtrl = (struct cpu_ctrl *)data;
	struct eth_port *pp = cpuCtrl->pp;
#if defined(MY_ABC_HERE)
	struct sk_buff *skb;
	struct bm_pool *pool;
	struct neta_rx_desc *rx_desc;
	phys_addr_t pa;
	int index, refill_ok;
#else  
	struct net_device *dev = pp->dev;
#endif  

	STAT_INFO(pp->stats.cleanup_timer++);

	clear_bit(MV_ETH_F_CLEANUP_TIMER_BIT, &(cpuCtrl->flags));

	if (!test_bit(MV_ETH_F_STARTED_BIT, &(pp->flags)))
		return;

	if (cpuCtrl->cpu != smp_processor_id()) {
		pr_warn("%s: Called on other CPU - %d != %d\n", __func__, cpuCtrl->cpu, smp_processor_id());
		cpuCtrl = pp->cpu_config[smp_processor_id()];
	}

#if defined(MY_ABC_HERE)
	if (MV_NETA_BM_CAP()) { 
		for (index = 0; index < MV_ETH_BM_POOLS; index++) {
			pool = &mv_eth_pool[index];
			if (!mv_eth_pool_bm(pool))
				continue;
			while (atomic_read(&pool->missed)) {
				skb  = mv_eth_skb_alloc(pp, pool, &pa, GFP_ATOMIC);
				if (!skb) {
					 
					mv_eth_add_cleanup_timer(pp->cpu_config[smp_processor_id()]);
					break;
				}
				mv_eth_rxq_refill(pp, index, pool, skb, NULL);
				STAT_INFO(pp->stats.cleanup_timer_skb++);
				atomic_dec(&pool->missed);
			}
			if (atomic_read(&pool->missed))
				break;
		}
	} else { 
		for (index = 0; index < CONFIG_MV_ETH_RXQ; index++) {
			if (!atomic_read(&pp->rxq_ctrl[index].missed))
				continue;
			rx_desc = pp->rxq_ctrl[index].missed_desc;
			refill_ok = 0;
			 
			while (atomic_read(&pp->rxq_ctrl[index].missed)) {
				pool = pp->pool_long;
				skb = mv_eth_skb_alloc(pp, pool, &pa, GFP_ATOMIC);
				if (!skb) {
					 
					pp->rxq_ctrl[index].missed_desc = rx_desc;
					mv_eth_add_cleanup_timer(pp->cpu_config[smp_processor_id()]);
					break;
				}
				mv_eth_rxq_refill(pp, index, pool, skb, rx_desc);
				STAT_INFO(pp->stats.cleanup_timer_skb++);
				atomic_dec(&pp->rxq_ctrl[index].missed);
				 
				rx_desc = mvNetaRxqNextDescPtr(pp->rxq_ctrl[index].q, rx_desc);
				refill_ok++;
			}
			 
			if (!atomic_read(&pp->rxq_ctrl[index].missed))
				atomic_set(&pp->rxq_ctrl[index].refill_stop, 0);

			if (refill_ok) {
				mv_neta_wmb();
				mvNetaRxqDescNumUpdate(pp->port, index, 0, refill_ok);
			}
		}
	}
#endif  
}

void mv_eth_mac_show(int port)
{
	struct eth_port *pp;

	if (mvNetaPortCheck(port))
		return;

	pp = mv_eth_port_by_id(port);
	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return;
	}

#ifdef CONFIG_MV_ETH_PNC
	if (MV_NETA_PNC_CAP()) {
		if (mv_eth_pnc_ctrl_en) {
			mvOsPrintf("PnC MAC Rules - port #%d:\n", port);
			pnc_mac_show();
		} else
			mvOsPrintf("%s: PNC control is disabled\n", __func__);
	} else { 
		mvEthPortUcastShow(port);
		mvEthPortMcastShow(port);
	}
#else  
	mvEthPortUcastShow(port);
	mvEthPortMcastShow(port);
#endif  
}

void mv_eth_vlan_prio_show(int port)
{
	struct eth_port *pp;

	if (mvNetaPortCheck(port))
		return;

	pp = mv_eth_port_by_id(port);
	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return;
	}

#ifdef CONFIG_MV_ETH_PNC
	if (MV_NETA_PNC_CAP()) {
		if (mv_eth_pnc_ctrl_en) {
			mvOsPrintf("PnC VLAN Priority Rules - port #%d:\n", port);
			pnc_vlan_prio_show(port);
		} else
			mvOsPrintf("%s: PNC control is disabled\n", __func__);
	} else {  
		int prio, rxq;

		mvOsPrintf("Legacy VLAN Priority Rules - port #%d:\n", port);
		for (prio = 0; prio <= 0x7; prio++) {

			rxq = mvNetaVprioToRxqGet(port, prio);
			if (rxq > 0)
				pr_info("prio=%d: rxq=%d\n", prio, rxq);
		}
	}
#else  
	{
		int prio, rxq;

		mvOsPrintf("Legacy VLAN Priority Rules - port #%d:\n", port);
		for (prio = 0; prio <= 0x7 ; prio++) {

			rxq = mvNetaVprioToRxqGet(port, prio);
			if (rxq > 0)
				printk(KERN_INFO "prio=%d: rxq=%d\n", prio, rxq);
		}
	}
#endif  
}

void mv_eth_tos_map_show(int port)
{
	int tos, txq, cpu;
	struct cpu_ctrl *cpuCtrl;
	struct eth_port *pp;

	if (mvNetaPortCheck(port))
		return;

	pp = mv_eth_port_by_id(port);
	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return;
	}

#ifdef CONFIG_MV_ETH_PNC
	if (MV_NETA_PNC_CAP()) {
		if (mv_eth_pnc_ctrl_en) {
			mvOsPrintf("PnC    TOS (DSCP) => RXQ Mapping Rules - port #%d:\n", port);
			pnc_ipv4_dscp_show(port);
		} else
			mvOsPrintf("%s: PNC control is disabled\n", __func__);
	} else {
		mvOsPrintf("Legacy TOS (DSCP) => RXQ Mapping Rules - port #%d:\n", port);
		for (tos = 0; tos < 0xFF; tos += 0x4) {
			int rxq;

			rxq = mvNetaTosToRxqGet(port, tos);
			if (rxq > 0)
				pr_err("      0x%02x (0x%02x) => %d\n",
					tos, tos >> 2, rxq);
		}
	}
#else
	mvOsPrintf("Legacy TOS (DSCP) => RXQ Mapping Rules - port #%d:\n", port);
	for (tos = 0; tos < 0xFF; tos += 0x4) {
		int rxq;

		rxq = mvNetaTosToRxqGet(port, tos);
		if (rxq > 0)
			printk(KERN_ERR "      0x%02x (0x%02x) => %d\n",
					tos, tos >> 2, rxq);
	}
#endif  
	for_each_possible_cpu(cpu) {
		printk(KERN_ERR "\n");
		printk(KERN_ERR " TOS => TXQ map for port #%d cpu #%d\n", port, cpu);
		cpuCtrl = pp->cpu_config[cpu];
		for (tos = 0; tos < sizeof(cpuCtrl->txq_tos_map); tos++) {
			txq = cpuCtrl->txq_tos_map[tos];
			if (txq != MV_ETH_TXQ_INVALID)
				printk(KERN_ERR "0x%02x => %d\n", tos, txq);
		}
	}
}

int mv_eth_rxq_tos_map_set(int port, int rxq, unsigned char tos)
{
	int status = -1;
	struct eth_port *pp;

	if (mvNetaPortCheck(port))
		return -EINVAL;

	pp = mv_eth_port_by_id(port);
	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return 1;
	}

#ifdef CONFIG_MV_ETH_PNC
	if (MV_NETA_PNC_CAP()) {
		if (mv_eth_pnc_ctrl_en)
			status = pnc_ip4_dscp(port, tos, 0xFF, rxq);
		else
			mvOsPrintf("%s: PNC control is disabled\n", __func__);
	} else { 
		status = mvNetaTosToRxqSet(port, tos, rxq);
	}
#else  
	status = mvNetaTosToRxqSet(port, tos, rxq);
#endif  

	if (status == 0)
		printk(KERN_ERR "Succeeded\n");
	else if (status == -1)
		printk(KERN_ERR "Not supported\n");
	else
		printk(KERN_ERR "Failed\n");

	return status;
}

int mv_eth_rxq_vlan_prio_set(int port, int rxq, unsigned char prio)
{
	int status = -1;

#ifdef CONFIG_MV_ETH_PNC
	if (MV_NETA_PNC_CAP()) {
		if (mv_eth_pnc_ctrl_en)
			status = pnc_vlan_prio_set(port, prio, rxq);
		else
			mvOsPrintf("%s: PNC control is disabled\n", __func__);
	} else { 
		status = mvNetaVprioToRxqSet(port, prio, rxq);
	}
#else  
	status = mvNetaVprioToRxqSet(port, prio, rxq);
#endif  

	if (status == 0)
		printk(KERN_ERR "Succeeded\n");
	else if (status == -1)
		printk(KERN_ERR "Not supported\n");
	else
		printk(KERN_ERR "Failed\n");

	return status;
}

int mv_eth_txq_tos_map_set(int port, int txq, int cpu, unsigned int tos)
{
	MV_U8 old_txq;
	struct cpu_ctrl	*cpuCtrl;
	struct eth_port *pp = mv_eth_port_by_id(port);

	if (mvNetaPortCheck(port))
		return -EINVAL;

	if ((pp == NULL) || (pp->txq_ctrl == NULL)) {
		pr_err("Port %d does not exist\n", port);
		return -ENODEV;
	}
	if ((cpu >= nr_cpu_ids) || (cpu < 0)) {
		printk(KERN_ERR "cpu #%d is out of range: from 0 to %d\n",
			cpu, nr_cpu_ids - 1);
		return -EINVAL;
	}

	if (!(MV_BIT_CHECK(pp->cpu_mask, cpu))) {
		printk(KERN_ERR "%s: Error, cpu #%d is masked\n", __func__, cpu);
		return -EINVAL;
	}
	if ((tos > 0xFF) || (tos < 0)) {
		printk(KERN_ERR "TOS 0x%x is out of range: from 0 to 0xFF\n", tos);
		return -EINVAL;
	}
	cpuCtrl = pp->cpu_config[cpu];
	old_txq = cpuCtrl->txq_tos_map[tos];

	if (old_txq == (MV_U8) txq)
		return MV_OK;

	if (txq == -1) {
		 
		if (mv_eth_ctrl_txq_cpu_own(port, pp->txp, old_txq, 0, cpu))
			return -EINVAL;

		cpuCtrl->txq_tos_map[tos] = MV_ETH_TXQ_INVALID;
		printk(KERN_ERR "Successfully deleted\n");
		return MV_OK;
	}

	if (mvNetaMaxCheck(txq, CONFIG_MV_ETH_TXQ, "txq"))
		return -EINVAL;

	if (!(MV_BIT_CHECK(cpuCtrl->cpuTxqMask, txq))) {
		printk(KERN_ERR "%s: Error, Txq #%d masked for cpu #%d\n", __func__, txq, cpu);
		return -EINVAL;
	}

	if (mv_eth_ctrl_txq_cpu_own(port, pp->txp, txq, 1, cpu))
		return -EINVAL;

	cpuCtrl->txq_tos_map[tos] = (MV_U8) txq;
	printk(KERN_ERR "Successfully added\n");
	return MV_OK;
}

static int mv_eth_priv_init(struct eth_port *pp, int port)
{
	int cpu, i;
	struct cpu_ctrl	*cpuCtrl;
	u8	*ext_buf;
#if defined(MY_ABC_HERE)
	MV_U16 phy_id_0, phy_id_1;
#endif  

	for (i = 0; i < nr_cpu_ids; i++) {
		pp->cpu_config[i] = kmalloc(sizeof(struct cpu_ctrl), GFP_KERNEL);
		memset(pp->cpu_config[i], 0, sizeof(struct cpu_ctrl));
	}

	pp->port = port;
	pp->txp_num = 1;
	pp->txp = 0;
	pp->pm_mode = 0;
	for_each_possible_cpu(cpu) {
		cpuCtrl = pp->cpu_config[cpu];
		cpuCtrl->txq = CONFIG_MV_ETH_TXQ_DEF;
		cpuCtrl->cpuTxqOwner = (1 << CONFIG_MV_ETH_TXQ_DEF);
		cpuCtrl->cpuTxqMask = 0xFF;
		mvNetaTxqCpuMaskSet(port, 0xFF , cpu);
		cpuCtrl->pp = pp;
		cpuCtrl->cpu = cpu;
	}

	pp->flags = 0;

#ifdef CONFIG_MV_ETH_BM_CPU
	if (MV_NETA_BM_CAP()) {
		pp->pool_long_num = mv_eth_bm_config_long_buf_num_get(port);
		if (pp->pool_long_num > MV_BM_POOL_CAP_MAX)
			pp->pool_long_num = MV_BM_POOL_CAP_MAX;

		pp->pool_short_num = mv_eth_bm_config_short_buf_num_get(port);
		if (pp->pool_short_num > MV_BM_POOL_CAP_MAX)
			pp->pool_short_num = MV_BM_POOL_CAP_MAX;
	} else {
		pp->pool_long_num = CONFIG_MV_ETH_RXQ * CONFIG_MV_ETH_RXQ_DESC * 2;
	}
#else
	pp->pool_long_num = CONFIG_MV_ETH_RXQ * CONFIG_MV_ETH_RXQ_DESC * 2;
#endif  
	for_each_possible_cpu(cpu) {
		cpuCtrl = pp->cpu_config[cpu];
		for (i = 0; i < 256; i++) {
			cpuCtrl->txq_tos_map[i] = MV_ETH_TXQ_INVALID;
#ifdef CONFIG_MV_ETH_TX_SPECIAL
		pp->tx_special_check = NULL;
#endif  
		}
	}

	mv_eth_port_config_parse(pp);

#ifdef CONFIG_MV_PON
	if (MV_PON_PORT(port)) {
		set_bit(MV_ETH_F_MH_BIT, &(pp->flags));
		pp->txp_num = MV_ETH_MAX_TCONT();
		pp->txp = CONFIG_MV_PON_TXP_DEF;
		for_each_possible_cpu(i)
			pp->cpu_config[i]->txq = CONFIG_MV_PON_TXQ_DEF;
	}
#endif  

	for_each_possible_cpu(cpu) {
		cpuCtrl = pp->cpu_config[cpu];
#if defined(CONFIG_MV_NETA_TXDONE_IN_HRTIMER)
		memset(&cpuCtrl->tx_done_timer, 0, sizeof(struct hrtimer));
		hrtimer_init(&cpuCtrl->tx_done_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
		cpuCtrl->tx_done_timer.function = mv_eth_tx_done_hr_timer_callback;
		tasklet_init(&cpuCtrl->tx_done_tasklet, mv_eth_tx_done_timer_callback,
			     (unsigned long) cpuCtrl);
#elif defined(CONFIG_MV_NETA_TXDONE_IN_TIMER)
		memset(&cpuCtrl->tx_done_timer, 0, sizeof(struct timer_list));
		cpuCtrl->tx_done_timer.function = mv_eth_tx_done_timer_callback;
		cpuCtrl->tx_done_timer.data = (unsigned long)cpuCtrl;
		init_timer(&cpuCtrl->tx_done_timer);
#endif
		memset(&cpuCtrl->cleanup_timer, 0, sizeof(struct timer_list));
		cpuCtrl->cleanup_timer.function = mv_eth_cleanup_timer_callback;
		cpuCtrl->cleanup_timer.data = (unsigned long)cpuCtrl;
		init_timer(&cpuCtrl->cleanup_timer);
		clear_bit(MV_ETH_F_TX_DONE_TIMER_BIT, &(cpuCtrl->flags));
		clear_bit(MV_ETH_F_CLEANUP_TIMER_BIT, &(cpuCtrl->flags));
	}

	pp->weight = CONFIG_MV_ETH_RX_POLL_WEIGHT;

	spin_lock_init(&pp->extLock);
	pp->extBufSize = CONFIG_MV_ETH_EXTRA_BUF_SIZE;
	pp->extArrStack = mvStackCreate(CONFIG_MV_ETH_EXTRA_BUF_NUM);
	if (pp->extArrStack == NULL) {
		printk(KERN_ERR "Error: failed create  extArrStack for port #%d\n", port);
		return -ENOMEM;
	}
	for (i = 0; i < CONFIG_MV_ETH_EXTRA_BUF_NUM; i++) {
		ext_buf = mvOsMalloc(CONFIG_MV_ETH_EXTRA_BUF_SIZE);
		if (ext_buf == NULL) {
			printk(KERN_WARNING "%s Warning: %d of %d extra buffers allocated\n",
				__func__, i, CONFIG_MV_ETH_EXTRA_BUF_NUM);
			break;
		}
		mvStackPush(pp->extArrStack, (MV_U32)ext_buf);
	}

#ifdef CONFIG_MV_ETH_STAT_DIST
	pp->dist_stats.rx_dist = mvOsMalloc(sizeof(u32) * (CONFIG_MV_ETH_RXQ * CONFIG_MV_ETH_RXQ_DESC + 1));
	if (pp->dist_stats.rx_dist != NULL) {
		pp->dist_stats.rx_dist_size = CONFIG_MV_ETH_RXQ * CONFIG_MV_ETH_RXQ_DESC + 1;
		memset(pp->dist_stats.rx_dist, 0, sizeof(u32) * pp->dist_stats.rx_dist_size);
	} else
		printk(KERN_ERR "ethPort #%d: Can't allocate %d bytes for rx_dist\n",
		       pp->port, sizeof(u32) * (CONFIG_MV_ETH_RXQ * CONFIG_MV_ETH_RXQ_DESC + 1));

	pp->dist_stats.tx_done_dist =
	    mvOsMalloc(sizeof(u32) * (pp->txp_num * CONFIG_MV_ETH_TXQ * CONFIG_MV_ETH_TXQ_DESC + 1));
	if (pp->dist_stats.tx_done_dist != NULL) {
		pp->dist_stats.tx_done_dist_size = pp->txp_num * CONFIG_MV_ETH_TXQ * CONFIG_MV_ETH_TXQ_DESC + 1;
		memset(pp->dist_stats.tx_done_dist, 0, sizeof(u32) * pp->dist_stats.tx_done_dist_size);
	} else
		printk(KERN_ERR "ethPort #%d: Can't allocate %d bytes for tx_done_dist\n",
		       pp->port, sizeof(u32) * (pp->txp_num * CONFIG_MV_ETH_TXQ * CONFIG_MV_ETH_TXQ_DESC + 1));
#endif  

#if defined(MY_ABC_HERE)
	pp->phy_id =  pp->plat_data->phy_addr;
	pp->wol = 0;
	if (mv_eth_tool_read_phy_reg(pp->phy_id, 0, MII_PHYSID1, &phy_id_0) ||
		mv_eth_tool_read_phy_reg(pp->phy_id, 0, MII_PHYSID2, &phy_id_1)) {
		printk("Synology Warning: Get PHY ID Error!\n");
		pp->phy_chip = 0;
	} else {
		 
		if (MV_PHY_ID_151X == ((phy_id_0 & 0xffff) << 16 | (phy_id_1 & 0xfff0))) {
			pp->phy_chip = (phy_id_0 & 0xffff) << 16 | (phy_id_1 & 0xfff0);
		} else {
			pp->phy_chip = (phy_id_0 & 0xffff) << 16 | (phy_id_1 & 0xffff);
		}
	}
#endif  
	return 0;
}

extern struct Qdisc noop_qdisc;
void mv_eth_set_noqueue(struct net_device *dev, int enable)
{
	struct netdev_queue *txq = netdev_get_tx_queue(dev, 0);

	if (dev->flags & IFF_UP) {
		printk(KERN_ERR "%s: device or resource busy, take it down\n", dev->name);
		return;
	}
	dev->tx_queue_len = enable ? 0 : CONFIG_MV_ETH_TXQ_DESC;

	if (txq)
		txq->qdisc_sleeping = &noop_qdisc;
	else
		printk(KERN_ERR "%s: txq #0 is NULL\n", dev->name);

	printk(KERN_ERR "%s: device tx queue len is %d\n", dev->name, (int)dev->tx_queue_len);

}

void mv_eth_pool_status_print(int pool)
{
	struct bm_pool *bm_pool = &mv_eth_pool[pool];

	printk(KERN_ERR "\nRX Pool #%d: pkt_size=%d, BM-HW support - %s\n",
	       pool, bm_pool->pkt_size, mv_eth_pool_bm(bm_pool) ? "Yes" : "No");

#if defined(MY_ABC_HERE)
	pr_info("bm_pool=%p, stack=%p, capacity=%d, buf_num=%d, port_map=0x%x missed=%d, in_use=%d, in_use_thresh=%u\n",
		bm_pool->bm_pool, bm_pool->stack, bm_pool->capacity, bm_pool->buf_num,
		bm_pool->port_map, atomic_read(&bm_pool->missed),
		atomic_read(&bm_pool->in_use), bm_pool->in_use_thresh);
#else  
	pr_info("bm_pool=%p, stack=%p, capacity=%d, buf_num=%d, port_map=0x%x missed=%d, in_use=%u, in_use_thresh=%u\n",
		bm_pool->bm_pool, bm_pool->stack, bm_pool->capacity, bm_pool->buf_num,
		bm_pool->port_map, bm_pool->missed, bm_pool->in_use, bm_pool->in_use_thresh);
#endif  

#ifdef CONFIG_MV_ETH_STAT_ERR
	printk(KERN_ERR "Errors: skb_alloc_oom=%u, stack_empty=%u, stack_full=%u\n",
	       bm_pool->stats.skb_alloc_oom, bm_pool->stats.stack_empty, bm_pool->stats.stack_full);
#endif  

#ifdef CONFIG_MV_ETH_STAT_DBG
	pr_info("     skb_alloc_ok=%u, bm_put=%u, stack_put=%u, stack_get=%u\n",
	       bm_pool->stats.skb_alloc_ok, bm_pool->stats.bm_put, bm_pool->stats.stack_put, bm_pool->stats.stack_get);

	pr_info("     skb_recycled_ok=%u, skb_recycled_err=%u\n",
	       bm_pool->stats.skb_recycled_ok, bm_pool->stats.skb_recycled_err);
#endif  

	if (bm_pool->stack)
		mvStackStatus(bm_pool->stack, 0);

	memset(&bm_pool->stats, 0, sizeof(bm_pool->stats));
}

void mv_eth_ext_pool_print(struct eth_port *pp)
{
	printk(KERN_ERR "\nExt Pool Stack: bufSize = %u bytes\n", pp->extBufSize);
	mvStackStatus(pp->extArrStack, 0);
}

void mv_eth_netdev_print(struct net_device *dev)
{
	pr_info("%s net_device status:\n\n", dev->name);
	pr_info("ifIdx=%d, mtu=%u, pkt_size=%d, buf_size=%d, MAC=" MV_MACQUAD_FMT "\n",
	       dev->ifindex, dev->mtu, RX_PKT_SIZE(dev->mtu),
		RX_BUF_SIZE(RX_PKT_SIZE(dev->mtu)), MV_MACQUAD(dev->dev_addr));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)
	pr_info("features=0x%x, hw_features=0x%x, wanted_features=0x%x, vlan_features=0x%x\n",
			(unsigned int)(dev->features), (unsigned int)(dev->hw_features),
			(unsigned int)(dev->wanted_features), (unsigned int)(dev->vlan_features));
#else
	pr_info("features=0x%x, vlan_features=0x%x\n",
		 (unsigned int)(dev->features), (unsigned int)(dev->vlan_features));
#endif

	pr_info("flags=0x%x, gflags=0x%x, priv_flags=0x%x: running=%d, oper_up=%d\n",
		(unsigned int)(dev->flags), (unsigned int)(dev->gflags), (unsigned int)(dev->priv_flags),
		netif_running(dev), netif_oper_up(dev));
	pr_info("uc_promisc=%d, promiscuity=%d, allmulti=%d\n", dev->uc_promisc, dev->promiscuity, dev->allmulti);

	if (mv_eth_netdev_find(dev->ifindex)) {
		struct eth_port *pp = MV_ETH_PRIV(dev);
		if (pp->tagged)
			mv_mux_netdev_print_all(pp->port);
	} else {
		 
		if (mv_mux_netdev_find(dev->ifindex) != -1)
			mv_mux_netdev_print(dev);
	}
}

void mv_eth_status_print(void)
{
	pr_info("totals: ports=%d\n", mv_eth_ports_num);

#ifdef CONFIG_MV_NETA_SKB_RECYCLE
	pr_info("SKB recycle = %s\n", mv_ctrl_swf_recycle ? "Enabled" : "Disabled");
#endif  

#ifdef CONFIG_MV_ETH_PNC
	if (MV_NETA_PNC_CAP())
		pr_info("PnC control = %s\n", mv_eth_pnc_ctrl_en ? "Enabled" : "Disabled");
#endif  
}

void mv_eth_port_status_print(unsigned int port)
{
	int txp, q;
	struct eth_port *pp = mv_eth_port_by_id(port);
	struct tx_queue *txq_ctrl;
	struct cpu_ctrl	*cpuCtrl;

	if (!pp)
		return;

	pr_info("\nport=%d, flags=0x%lx, rx_weight=%d, %s\n", port, pp->flags, pp->weight,
			pp->tagged ? "tagged" : "untagged");

	if (pp->flags & MV_ETH_F_CONNECT_LINUX)
		printk(KERN_ERR "%s: ", pp->dev->name);
	else
		printk(KERN_ERR "port %d: ", port);

	mv_eth_link_status_print(port);

	if (pp->pm_mode == 1)
		printk(KERN_CONT "pm - wol\n");
	else
		printk(KERN_CONT "pm - suspend\n");

	printk(KERN_ERR "rxq_coal(pkts)[ q]   = ");
	for (q = 0; q < CONFIG_MV_ETH_RXQ; q++)
		printk(KERN_CONT "%3d ", mvNetaRxqPktsCoalGet(port, q));

	printk(KERN_CONT "\n");
	printk(KERN_ERR "rxq_coal(usec)[ q]   = ");
	for (q = 0; q < CONFIG_MV_ETH_RXQ; q++)
		printk(KERN_CONT "%3d ", mvNetaRxqTimeCoalGet(port, q));

	printk(KERN_CONT "\n");
	printk(KERN_ERR "rxq_desc(num)[ q]    = ");
	for (q = 0; q < CONFIG_MV_ETH_RXQ; q++)
		printk(KERN_CONT "%3d ", pp->rxq_ctrl[q].rxq_size);

	printk(KERN_CONT "\n");
	for (txp = 0; txp < pp->txp_num; txp++) {
		printk(KERN_ERR "txq_coal(pkts)[%2d.q] = ", txp);
		for (q = 0; q < CONFIG_MV_ETH_TXQ; q++)
			printk(KERN_CONT "%3d ", mvNetaTxDonePktsCoalGet(port, txp, q));
		printk(KERN_CONT "\n");

		printk(KERN_ERR "txq_mod(F,C,H)[%2d.q] = ", txp);
		for (q = 0; q < CONFIG_MV_ETH_TXQ; q++) {
			int val, mode;

			mode = mv_eth_ctrl_txq_mode_get(port, txp, q, &val);
			if (mode == MV_ETH_TXQ_CPU)
				printk(KERN_CONT " C%-d ", val);
			else if (mode == MV_ETH_TXQ_HWF)
				printk(KERN_CONT " H%-d ", val);
			else
				printk(KERN_CONT "  F ");
		}
		printk(KERN_CONT "\n");

		printk(KERN_ERR "txq_desc(num) [%2d.q] = ", txp);
		for (q = 0; q < CONFIG_MV_ETH_TXQ; q++) {
			struct tx_queue *txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + q];
			printk(KERN_CONT "%3d ", txq_ctrl->txq_size);
		}
		printk(KERN_CONT "\n");
	}
	printk(KERN_ERR "\n");

#if defined(CONFIG_MV_NETA_TXDONE_ISR)
	printk(KERN_ERR "Do tx_done in NAPI context triggered by ISR\n");
	for (txp = 0; txp < pp->txp_num; txp++) {
		printk(KERN_ERR "txcoal(pkts)[%2d.q] = ", txp);
		for (q = 0; q < CONFIG_MV_ETH_TXQ; q++)
			printk(KERN_CONT "%3d ", mvNetaTxDonePktsCoalGet(port, txp, q));
		printk(KERN_CONT "\n");
	}
	pr_err("\n");
#elif defined(CONFIG_MV_NETA_TXDONE_IN_HRTIMER)
	pr_err("Do tx_done in TX or high-resolution Timer's tasklet: tx_done_threshold=%d timer_interval=%d usec\n",
		mv_ctrl_txdone, mv_eth_tx_done_hrtimer_period_get());
#elif defined(CONFIG_MV_NETA_TXDONE_IN_TIMER)
	pr_err("Do tx_done in TX or regular Timer context: tx_done_threshold=%d timer_interval=%d msec\n",
		mv_ctrl_txdone, CONFIG_MV_NETA_TX_DONE_TIMER_PERIOD);
#endif  

	printk(KERN_ERR "txp=%d, zero_pad=%s, mh_en=%s (0x%04x), tx_cmd=0x%08x\n",
	       pp->txp, (pp->flags & MV_ETH_F_NO_PAD) ? "Disabled" : "Enabled",
	       (pp->flags & MV_ETH_F_MH) ? "Enabled" : "Disabled", pp->tx_mh, pp->hw_cmd);

	printk(KERN_CONT "\n");
#ifdef CONFIG_MV_NETA_TXDONE_ISR
	pr_cont("CPU:  txq  causeRxTx   napi txqMask txqOwner flags\n");
#else
	pr_cont("CPU:  txq  causeRxTx   napi txqMask txqOwner flags  timer\n");
#endif
	{
		int cpu;
		for_each_possible_cpu(cpu) {
			cpuCtrl = pp->cpu_config[cpu];
			if (cpuCtrl != NULL)
#if defined(CONFIG_MV_NETA_TXDONE_ISR)
				pr_err("  %d:   %d   0x%08x   %d    0x%02x    0x%02x    0x%02x\n",
					cpu, cpuCtrl->txq, cpuCtrl->causeRxTx,
					test_bit(NAPI_STATE_SCHED, &cpuCtrl->napi->state),
					cpuCtrl->cpuTxqMask, cpuCtrl->cpuTxqOwner,
					(unsigned)cpuCtrl->flags);
#elif defined(CONFIG_MV_NETA_TXDONE_IN_HRTIMER)
				pr_err("  %d:   %d   0x%08x   %d    0x%02x    0x%02x    0x%02x    %d\n",
					cpu, cpuCtrl->txq, cpuCtrl->causeRxTx,
					test_bit(NAPI_STATE_SCHED, &cpuCtrl->napi->state),
					cpuCtrl->cpuTxqMask, cpuCtrl->cpuTxqOwner,
					(unsigned)cpuCtrl->flags, !(hrtimer_active(&cpuCtrl->tx_done_timer)));
#elif defined(CONFIG_MV_NETA_TXDONE_IN_TIMER)
				pr_err("  %d:   %d   0x%08x   %d    0x%02x    0x%02x    0x%02x    %d\n",
					cpu, cpuCtrl->txq, cpuCtrl->causeRxTx,
					test_bit(NAPI_STATE_SCHED, &cpuCtrl->napi->state),
					cpuCtrl->cpuTxqMask, cpuCtrl->cpuTxqOwner,
					(unsigned)cpuCtrl->flags, timer_pending(&cpuCtrl->tx_done_timer));
#endif
		}
	}

	printk(KERN_CONT "\n");
	printk(KERN_CONT "TXQ: SharedFlag  nfpCounter   cpu_owner\n");

	for (q = 0; q < CONFIG_MV_ETH_TXQ; q++) {
		int cpu;

		txq_ctrl = &pp->txq_ctrl[pp->txp * CONFIG_MV_ETH_TXQ + q];
		if (txq_ctrl != NULL)
			printk(KERN_CONT " %d:     %2lu        %d",
				q, (txq_ctrl->flags & MV_ETH_F_TX_SHARED), txq_ctrl->nfpCounter);

			printk(KERN_CONT "        [");
			for_each_possible_cpu(cpu)
				printk(KERN_CONT "%2d ", txq_ctrl->cpu_owner[cpu]);

			printk(KERN_CONT "]\n");
	}
	printk(KERN_CONT "\n");

	if (pp->dev)
		mv_eth_netdev_print(pp->dev);

	if (pp->tagged)
		mv_mux_netdev_print_all(port);
}

void mv_eth_port_stats_print(unsigned int port)
{
	struct eth_port *pp = mv_eth_port_by_id(port);
	struct port_stats *stat = NULL;
	struct tx_queue *txq_ctrl;
	int txp, queue, cpu = smp_processor_id();
	u32 total_rx_ok, total_rx_fill_ok;

	pr_info("\n====================================================\n");
	pr_info("ethPort_%d: Statistics (running on cpu#%d)", port, cpu);
	pr_info("----------------------------------------------------\n\n");

	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return;
	}
	stat = &(pp->stats);

#ifdef CONFIG_MV_ETH_STAT_ERR
	printk(KERN_ERR "Errors:\n");
	printk(KERN_ERR "rx_error......................%10u\n", stat->rx_error);
	printk(KERN_ERR "tx_timeout....................%10u\n", stat->tx_timeout);
	printk(KERN_ERR "tx_netif_stop.................%10u\n", stat->netif_stop);
	printk(KERN_ERR "netif_wake....................%10u\n", stat->netif_wake);
	printk(KERN_ERR "ext_stack_empty...............%10u\n", stat->ext_stack_empty);
	printk(KERN_ERR "ext_stack_full ...............%10u\n", stat->ext_stack_full);
	printk(KERN_ERR "state_err.....................%10u\n", stat->state_err);
#endif  

#ifdef CONFIG_MV_ETH_STAT_INF
	pr_info("\nEvents:\n");

	pr_info("irq[cpu]            = ");
	for_each_possible_cpu(cpu)
		printk(KERN_CONT "%8d ", stat->irq[cpu]);

	pr_info("irq_none[cpu]       = ");
	for_each_possible_cpu(cpu)
		printk(KERN_CONT "%8d ", stat->irq_err[cpu]);

	pr_info("poll[cpu]           = ");
	for_each_possible_cpu(cpu)
		printk(KERN_CONT "%8d ", stat->poll[cpu]);

	pr_info("poll_exit[cpu]      = ");
	for_each_possible_cpu(cpu)
		printk(KERN_CONT "%8d ", stat->poll_exit[cpu]);

	pr_info("tx_timer_event[cpu] = ");
	for_each_possible_cpu(cpu)
		printk(KERN_CONT "%8d ", stat->tx_done_timer_event[cpu]);

	pr_info("tx_timer_add[cpu]   = ");
	for_each_possible_cpu(cpu)
		printk(KERN_CONT "%8d ", stat->tx_done_timer_add[cpu]);

	pr_info("\n");
	printk(KERN_ERR "tx_done_event.................%10u\n", stat->tx_done);
	printk(KERN_ERR "cleanup_timer_event...........%10u\n", stat->cleanup_timer);
#if defined(MY_ABC_HERE)
	pr_info("skb from cleanup timer........%10u\n", stat->cleanup_timer_skb);
#endif  
	printk(KERN_ERR "link..........................%10u\n", stat->link);
	printk(KERN_ERR "netdev_stop...................%10u\n", stat->netdev_stop);
#ifdef CONFIG_MV_ETH_RX_SPECIAL
	printk(KERN_ERR "rx_special....................%10u\n", stat->rx_special);
#endif  
#ifdef CONFIG_MV_ETH_TX_SPECIAL
	printk(KERN_ERR "tx_special....................%10u\n", stat->tx_special);
#endif  
#endif  

	printk(KERN_ERR "\n");
	total_rx_ok = total_rx_fill_ok = 0;
	printk(KERN_ERR "RXQ:       rx_ok      rx_fill_ok     missed\n\n");
	for (queue = 0; queue < CONFIG_MV_ETH_RXQ; queue++) {
		u32 rxq_ok = 0, rxq_fill = 0;

#ifdef CONFIG_MV_ETH_STAT_DBG
		rxq_ok = stat->rxq[queue];
		rxq_fill = stat->rxq_fill[queue];
#endif  

#if defined(MY_ABC_HERE)
		pr_info("%3d:  %10u    %10u          %d\n",
			queue, rxq_ok, rxq_fill,
			atomic_read(&pp->rxq_ctrl[queue].missed));
#else  
		printk(KERN_ERR "%3d:  %10u    %10u          %d\n",
			queue, rxq_ok, rxq_fill,
			pp->rxq_ctrl[queue].missed);
#endif  
		total_rx_ok += rxq_ok;
		total_rx_fill_ok += rxq_fill;
	}
	printk(KERN_ERR "SUM:  %10u    %10u\n", total_rx_ok, total_rx_fill_ok);

#ifdef CONFIG_MV_ETH_STAT_DBG
	{
		printk(KERN_ERR "\n====================================================\n");
		printk(KERN_ERR "ethPort_%d: Debug statistics", port);
		printk(KERN_CONT "\n-------------------------------\n");

		printk(KERN_ERR "\n");

		printk(KERN_ERR "rx_nfp....................%10u\n", stat->rx_nfp);
		printk(KERN_ERR "rx_nfp_drop...............%10u\n", stat->rx_nfp_drop);

		printk(KERN_ERR "rx_gro....................%10u\n", stat->rx_gro);
		printk(KERN_ERR "rx_gro_bytes .............%10u\n", stat->rx_gro_bytes);

		printk(KERN_ERR "tx_tso....................%10u\n", stat->tx_tso);
		printk(KERN_ERR "tx_tso_bytes .............%10u\n", stat->tx_tso_bytes);

		printk(KERN_ERR "rx_netif..................%10u\n", stat->rx_netif);
		printk(KERN_ERR "rx_drop_sw................%10u\n", stat->rx_drop_sw);
		printk(KERN_ERR "rx_csum_hw................%10u\n", stat->rx_csum_hw);
		printk(KERN_ERR "rx_csum_sw................%10u\n", stat->rx_csum_sw);

		printk(KERN_ERR "tx_skb_free...............%10u\n", stat->tx_skb_free);
		printk(KERN_ERR "tx_sg.....................%10u\n", stat->tx_sg);
		printk(KERN_ERR "tx_csum_hw................%10u\n", stat->tx_csum_hw);
		printk(KERN_ERR "tx_csum_sw................%10u\n", stat->tx_csum_sw);

		printk(KERN_ERR "ext_stack_get.............%10u\n", stat->ext_stack_get);
		printk(KERN_ERR "ext_stack_put ............%10u\n", stat->ext_stack_put);

		printk(KERN_ERR "\n");
	}
#endif  

	printk(KERN_ERR "\n");
	printk(KERN_ERR "TXP-TXQ:  count        send          done      no_resource\n\n");

	for (txp = 0; txp < pp->txp_num; txp++) {
		for (queue = 0; queue < CONFIG_MV_ETH_TXQ; queue++) {
			u32 txq_tx = 0, txq_txdone = 0, txq_err = 0;

			txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + queue];
#ifdef CONFIG_MV_ETH_STAT_DBG
			txq_tx = txq_ctrl->stats.txq_tx;
			txq_txdone =  txq_ctrl->stats.txq_txdone;
#endif  
#ifdef CONFIG_MV_ETH_STAT_ERR
			txq_err = txq_ctrl->stats.txq_err;
#endif  

			printk(KERN_ERR "%d-%d:      %3d    %10u    %10u    %10u\n",
			       txp, queue, txq_ctrl->txq_count, txq_tx,
			       txq_txdone, txq_err);

			memset(&txq_ctrl->stats, 0, sizeof(txq_ctrl->stats));
		}
	}
#if defined(MY_ABC_HERE)
	pr_info("\n");
#else  
	printk(KERN_ERR "\n\n");
#endif  

	memset(stat, 0, sizeof(struct port_stats));

#ifdef CONFIG_MV_ETH_BM_CPU
#if defined(MY_ABC_HERE)
	if (MV_NETA_BM_CAP() && pp->pool_short && (pp->pool_short != pp->pool_long))
#else  
	if (MV_NETA_BM_CAP() && pp->pool_short)
#endif  
		mv_eth_pool_status_print(pp->pool_short->pool);
#endif  

	if (pp->pool_long)
		mv_eth_pool_status_print(pp->pool_long->pool);

	mv_eth_ext_pool_print(pp);

#ifdef CONFIG_MV_ETH_STAT_DIST
	{
		int i;
		struct dist_stats *dist_stats = &(pp->dist_stats);

		if (dist_stats->rx_dist) {
			printk(KERN_ERR "\n      Linux Path RX distribution\n");
			for (i = 0; i < dist_stats->rx_dist_size; i++) {
				if (dist_stats->rx_dist[i] != 0) {
					printk(KERN_ERR "%3d RxPkts - %u times\n", i, dist_stats->rx_dist[i]);
					dist_stats->rx_dist[i] = 0;
				}
			}
		}

		if (dist_stats->tx_done_dist) {
			printk(KERN_ERR "\n      tx-done distribution\n");
			for (i = 0; i < dist_stats->tx_done_dist_size; i++) {
				if (dist_stats->tx_done_dist[i] != 0) {
					printk(KERN_ERR "%3d TxDoneDesc - %u times\n", i, dist_stats->tx_done_dist[i]);
					dist_stats->tx_done_dist[i] = 0;
				}
			}
		}
#ifdef CONFIG_MV_ETH_TSO
		if (dist_stats->tx_tso_dist) {
			printk(KERN_ERR "\n      TSO stats\n");
			for (i = 0; i < dist_stats->tx_tso_dist_size; i++) {
				if (dist_stats->tx_tso_dist[i] != 0) {
					printk(KERN_ERR "%3d KBytes - %u times\n", i, dist_stats->tx_tso_dist[i]);
					dist_stats->tx_tso_dist[i] = 0;
				}
			}
		}
#endif  
	}
#endif  
}

static int mv_eth_port_cleanup(int port)
{
	int txp, txq, rxq, i;
	struct eth_port *pp;
	struct tx_queue *txq_ctrl;
	struct rx_queue *rxq_ctrl;

	pp = mv_eth_port_by_id(port);
	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return -1;
	}

	if (pp->flags & MV_ETH_F_STARTED) {
		printk(KERN_ERR "%s: port %d is started, cannot cleanup\n", __func__, port);
		return -1;
	}

	for (txp = 0; txp < pp->txp_num; txp++) {
		if (mv_eth_txp_reset(port, txp))
			printk(KERN_ERR "Warning: Port %d Tx port %d reset failed\n", port, txp);
	}

	for (txp = 0; txp < pp->txp_num; txp++) {
		for (txq = 0; txq < CONFIG_MV_ETH_TXQ; txq++) {
			txq_ctrl = &pp->txq_ctrl[txp * CONFIG_MV_ETH_TXQ + txq];
			mv_eth_txq_delete(pp, txq_ctrl);
		}
	}

	mvOsFree(pp->txq_ctrl);
	pp->txq_ctrl = NULL;

#ifdef CONFIG_MV_ETH_STAT_DIST
	 
	mvOsFree(pp->dist_stats.tx_done_dist);
#endif

	if (mv_eth_rx_reset(port))
		printk(KERN_ERR "Warning: Rx port %d reset failed\n", port);

	for (rxq = 0; rxq < CONFIG_MV_ETH_RXQ; rxq++) {
		rxq_ctrl = &pp->rxq_ctrl[rxq];
		mvNetaRxqDelete(pp->port, rxq);
		rxq_ctrl->q = NULL;
	}

	mvOsFree(pp->rxq_ctrl);
	pp->rxq_ctrl = NULL;

#ifdef CONFIG_MV_ETH_STAT_DIST
	 
	mvOsFree(pp->dist_stats.rx_dist);
#endif

	if (pp->pool_long) {
		mv_eth_pool_free(pp->pool_long->pool, pp->pool_long_num);
		pp->pool_long->port_map &= ~(1 << pp->port);
		pp->pool_long = NULL;
	}
#ifdef CONFIG_MV_ETH_BM_CPU
	if (MV_NETA_BM_CAP() && pp->pool_short) {
		mv_eth_pool_free(pp->pool_short->pool, pp->pool_short_num);
		pp->pool_short->port_map &= ~(1 << pp->port);
		pp->pool_short = NULL;
	}
#endif  

	mvNetaMhSet(port, MV_TAG_TYPE_NONE);

	mv_force_port_link_speed_fc(port, MV_ETH_SPEED_AN, 0);

	mvNetaPortDestroy(port);

	if (pp->flags & MV_ETH_F_CONNECT_LINUX)
		for (i = 0; i < CONFIG_MV_ETH_NAPI_GROUPS; i++)
			netif_napi_del(pp->napiGroup[i]);

	return 0;
}

int mv_eth_all_ports_cleanup(void)
{
	int port, pool, status = 0;

	for (port = 0; port < mv_eth_ports_num; port++) {
		status = mv_eth_port_cleanup(port);
		if (status != 0) {
			printk(KERN_ERR "Error: mv_eth_port_cleanup failed on port %d, stopping all ports cleanup\n", port);
			return status;
		}
	}

	for (pool = 0; pool < MV_ETH_BM_POOLS; pool++)
		mv_eth_pool_destroy(pool);

	for (port = 0; port < mv_eth_ports_num; port++) {
		if (mv_eth_ports[port])
			mvOsFree(mv_eth_ports[port]);
	}

	memset(mv_eth_ports, 0, (mv_eth_ports_num * sizeof(struct eth_port *)));
	 
	return 0;
}

#ifdef CONFIG_MV_ETH_PNC_WOL

#define DEF_WOL_SIZE	42
MV_U8	wol_data[DEF_WOL_SIZE] = { 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
				   0x00, 0x00, 0x00, 0x00, 0x00, 0x39, 0x08, 0x00,
				   0x45, 0x00, 0x00, 0x4E, 0x00, 0x00, 0x00, 0x00,
				   0x00, 0x11, 0x00, 0x00, 0xc0, 0xa8, 0x01, 0xFA,
				   0x00, 0x00, 0x00, 0x00, 0x00, 0x89, 0x00, 0x89,
				   0x00, 0x3A };

MV_U8	wol_mask[DEF_WOL_SIZE] = { 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
				   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
				   0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
				   0x00, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
				   0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
				   0xff, 0xff };

int mv_eth_wol_pkts_check(int port)
{
	struct eth_port	    *pp = mv_eth_port_by_id(port);
	struct neta_rx_desc *rx_desc;
	struct sk_buff *skb;
	struct bm_pool      *pool;
	int                 rxq, rx_done, i, wakeup, ruleId, pool_id;
	MV_NETA_RXQ_CTRL    *rx_ctrl;

	wakeup = 0;
	for (rxq = 0; rxq < CONFIG_MV_ETH_RXQ; rxq++) {
		rx_ctrl = pp->rxq_ctrl[rxq].q;

		if (rx_ctrl == NULL)
			continue;

		rx_done = mvNetaRxqBusyDescNumGet(pp->port, rxq);

		for (i = 0; i < rx_done; i++) {
			rx_desc = mvNetaRxqNextDescGet(rx_ctrl);
			mvOsCacheLineInv(pp->dev->dev.parent, rx_desc);

#if defined(MV_CPU_BE)
			mvNetaRxqDescSwap(rx_desc);
#endif  

			skb = (struct sk_buff *)rx_desc->bufCookie;
			mvOsCacheInvalidate(pp->dev->dev.parent, skb->head + NET_SKB_PAD, rx_desc->dataSize);

			if (mv_pnc_wol_pkt_match(pp->port, skb->head + NET_SKB_PAD, rx_desc->dataSize, &ruleId))
				wakeup = 1;
			pool_id = NETA_RX_GET_BPID(rx_desc);
			pool = &mv_eth_pool[pool_id];
			mv_eth_rxq_refill(pp, rxq, pool, skb, rx_desc);

			if (wakeup) {
				printk(KERN_INFO "packet match WoL rule=%d found on port=%d, rxq=%d\n",
						ruleId, port, rxq);
				i++;
				break;
			}
		}
		if (i) {
			mvNetaRxqDescNumUpdate(pp->port, rxq, i, i);
			printk(KERN_INFO "port=%d, rxq=%d: %d of %d packets dropped\n", port, rxq, i, rx_done);
		}
		if (wakeup) {
			 
			return 1;
		}
	}
	return 0;
}

void mv_eth_wol_wakeup(int port)
{
	int rxq;

	for (rxq = 0; rxq < CONFIG_MV_ETH_RXQ; rxq++) {
		mvNetaRxqPktsCoalSet(port, rxq, CONFIG_MV_ETH_RX_COAL_PKTS);
		mvNetaRxqTimeCoalSet(port, rxq, CONFIG_MV_ETH_RX_COAL_USEC);
	}

	mv_pnc_wol_wakeup(port);
	printk(KERN_INFO "Exit wakeOnLan mode on port #%d\n", port);

}

int mv_eth_wol_sleep(int port)
{
	int rxq, cpu;
	struct eth_port *pp;

	mv_pnc_wol_sleep(port);

	pp = mv_eth_port_by_id(port);
	if (pp == NULL) {
		pr_err("Port %d does not exist\n", port);
		return 1;
	}

	mv_eth_interrupts_mask(pp);

	for_each_possible_cpu(cpu) {
		if (pp->cpu_config[cpu]->napi)
			napi_synchronize(pp->cpu_config[cpu]->napi);
	}

	if (mv_eth_wol_pkts_check(port)) {
		 
		mv_pnc_wol_wakeup(port);
		printk(KERN_INFO "Failed to enter wakeOnLan mode on port #%d\n", port);
		return 1;
	}
	printk(KERN_INFO "Enter wakeOnLan mode on port #%d\n", port);

	for (rxq = 0; rxq < CONFIG_MV_ETH_RXQ; rxq++) {
		mvNetaRxqPktsCoalSet(port, rxq, 0);
		mvNetaRxqTimeCoalSet(port, rxq, 0);
	}

	mv_eth_interrupts_unmask(pp);

	return 0;
}
#endif  

#ifdef CONFIG_MV_PON
 
PONLINKSTATUSPOLLFUNC pon_link_status_polling_func;

void pon_link_status_notify_func(MV_BOOL link_state)
{
	struct eth_port *pon_port = mv_eth_port_by_id(MV_PON_PORT_ID_GET());

	if ((pon_port->flags & MV_ETH_F_STARTED) == 0) {
		 
#ifdef CONFIG_MV_NETA_DEBUG_CODE
		pr_info("PON port: Link event (%s) when port is down\n",
			link_state ? "Up" : "Down");
#endif  
		return;
	}
	mv_eth_link_event(pon_port, 1);
}

void mv_pon_link_state_register(PONLINKSTATUSPOLLFUNC poll_func, PONLINKSTATUSNOTIFYFUNC *notify_func)
{
	pon_link_status_polling_func = poll_func;
	*notify_func = pon_link_status_notify_func;
}

MV_BOOL mv_pon_link_status(void)
{
	if (pon_link_status_polling_func != NULL)
		return pon_link_status_polling_func();
	printk(KERN_ERR "pon_link_status_polling_func is uninitialized\n");
	return MV_FALSE;
}
#endif  

#ifdef CONFIG_PM

int mv_eth_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct eth_port *pp;
	int port = pdev->id;

	pm_flag = 0;

	pp = mv_eth_port_by_id(port);
	if (!pp)
		return 0;

	if (pp->pm_mode == 0) {
		if (mv_eth_port_suspend(port)) {
			printk(KERN_ERR "%s: port #%d suspend failed.\n", __func__, port);
			return MV_ERROR;
		}

		if ((port != 0) || (wol_ports_bmp == 0)) {
			 
#ifdef CONFIG_ARCH_MVEBU
			{
				 
				struct clk *clk;
				clk = devm_clk_get(&pdev->dev, NULL);
				clk_disable(clk);
			}
#else
			mvCtrlPwrClckSet(ETH_GIG_UNIT_ID, port, 0);
#endif
		}
	} else {

#ifdef CONFIG_MV_ETH_PNC_WOL
		if (pp->flags & MV_ETH_F_STARTED)
			if (mv_eth_wol_sleep(port)) {
				printk(KERN_ERR "%s: port #%d  WOL failed.\n", __func__, port);
				return MV_ERROR;
			}
#else
		printk(KERN_INFO "%s:WARNING port #%d in WOL mode but PNC WOL is not defined.\n", __func__, port);

#endif  
	}

	return MV_OK;
}

int mv_eth_resume(struct platform_device *pdev)
{
	struct eth_port *pp;
	int port = pdev->id;

	pp = mv_eth_port_by_id(port);
	if (!pp)
		return 0;

	if (pp->pm_mode == 0) {
		 
#ifdef CONFIG_ARCH_MVEBU
		{
			struct clk *clk;
			clk = devm_clk_get(&pdev->dev, NULL);
			clk_enable(clk);
		}
#else
		mvCtrlPwrClckSet(ETH_GIG_UNIT_ID, port, 1);
#endif

		mdelay(10);
		if (mv_eth_port_resume(port)) {
			printk(KERN_ERR "%s: port #%d resume failed.\n", __func__, port);
			return MV_ERROR;
		}
	} else
#ifdef CONFIG_MV_ETH_PNC_WOL
		mv_eth_wol_wakeup(port);
#else
		printk(KERN_ERR "%s:WARNING port #%d in WOL mode but PNC WOL is not defined.\n", __func__, port);
#endif  

	return MV_OK;
}

#endif	 

static int mv_eth_shared_remove(void)
{
	mv_eth_sysfs_exit();
	return 0;
}

static int mv_eth_remove(struct platform_device *pdev)
{
	int port = pdev->id;
	struct eth_port *pp = mv_eth_port_by_id(port);

	printk(KERN_INFO "Removing Marvell Ethernet Driver - port #%d\n", port);
	if (pp == NULL)
		pr_err("Port %d does not exist\n", port);

	mv_eth_priv_cleanup(pp);

#ifdef CONFIG_OF
	mv_eth_shared_remove();
#endif  

#ifdef CONFIG_NETMAP
	netmap_detach(pp->dev);
#endif  

	return 0;
}

#if defined(MY_ABC_HERE)
void syno_mv_net_setup_wol(void)
{
	int i = 0, j = 0;
	MV_U16 macTmp[3];
	MV_U16 phyTmp;

	for (i = 0; i < mv_eth_ports_num; i++) {
		struct eth_port *pp = mv_eth_port_by_id(i);

		if (NULL == pp) {
			continue;
		}

		if (!syno_wol_support(pp)) {
			continue;
		}

		if (MV_PHY_ID_151X == pp->phy_chip) {
			 
			mvEthPhyRegWrite(pp->phy_id, 0x16, 0x11);
			mvEthPhyRegWrite(pp->phy_id, 0x10, 0x1000);
			mvEthPhyRegWrite(pp->phy_id, 0x16, 0x0);

			if (pp->wol & WAKE_MAGIC) {
				printk("WOL MAC address: %pM\n", pp->dev->dev_addr);
				for (j = 0; j < 3; ++j) {
					macTmp[j] = (pp->dev->dev_addr[j * 2] & 0xff) | (pp->dev->dev_addr[j * 2 + 1] & 0xff) << 8;
				}

				mvEthPhyRegWrite(pp->phy_id, 0x16, 0x0);
				mvEthPhyRegRead(pp->phy_id, 0x12, &phyTmp);
				mvEthPhyRegWrite(pp->phy_id, 0x12, phyTmp | 0x80);
				mvEthPhyRegWrite(pp->phy_id, 0x16, 0x3);
				mvEthPhyRegRead(pp->phy_id,0x12,  &phyTmp);
				mvEthPhyRegWrite(pp->phy_id, 0x12, (phyTmp & 0x7fff) | 0x4880);
				mvEthPhyRegWrite(pp->phy_id, 0x16, 0x11);
				mvEthPhyRegWrite(pp->phy_id, 0x17, macTmp[2]);
				mvEthPhyRegWrite(pp->phy_id, 0x18, macTmp[1]);
				mvEthPhyRegWrite(pp->phy_id, 0x19, macTmp[0]);
				mvEthPhyRegWrite(pp->phy_id, 0x10, 0x4000);
				mvEthPhyRegWrite(pp->phy_id, 0x16, 0x0);
			}
		}
	}
}
#endif  

static void mv_eth_shutdown(struct platform_device *pdev)
{
	printk(KERN_INFO "Shutting Down Marvell Ethernet Driver\n");
#if defined(MY_ABC_HERE)
	syno_mv_net_setup_wol();
#endif  
}

#ifdef CONFIG_OF
static const struct of_device_id mv_neta_match[] = {
	{ .compatible = "marvell,neta" },
	{ }
};
MODULE_DEVICE_TABLE(of, mv_neta_match);

static int mv_eth_port_num_get(struct platform_device *pdev)
{
	int port_num = 0;
	int tbl_id;
	struct device_node *np = pdev->dev.of_node;

	for (tbl_id = 0; tbl_id < (sizeof(mv_neta_match) / sizeof(struct of_device_id)); tbl_id++) {
		for_each_compatible_node(np, NULL, mv_neta_match[tbl_id].compatible)
			port_num++;
	}

	return port_num;
}
#endif  

static struct platform_driver mv_eth_driver = {
	.probe = mv_eth_probe,
	.remove = mv_eth_remove,
	.shutdown = mv_eth_shutdown,
#ifdef CONFIG_PM
	.suspend = mv_eth_suspend,
	.resume = mv_eth_resume,
#endif  
	.driver = {
		.name = MV_NETA_PORT_NAME,
#ifdef CONFIG_OF
		.of_match_table = mv_neta_match,
#endif  
	},
};

#ifdef CONFIG_OF
module_platform_driver(mv_eth_driver);
#else
static int __init mv_eth_init_module(void)
{
	int err = platform_driver_register(&mv_eth_driver);
#if defined(MY_ABC_HERE)
	spin_lock_init(&mii_lock);
#endif  

	return err;
}
module_init(mv_eth_init_module);

static void __exit mv_eth_exit_module(void)
{
	platform_driver_unregister(&mv_eth_driver);
	mv_eth_shared_remove();
}
module_exit(mv_eth_exit_module);
#endif  

MODULE_DESCRIPTION("Marvell Ethernet Driver - www.marvell.com");
MODULE_AUTHOR("Dmitri Epshtein <dima@marvell.com>");
MODULE_LICENSE("GPL");
