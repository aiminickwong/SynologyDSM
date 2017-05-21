#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#ifndef __ASM_HARDWARE_CORESIGHT_H
#define __ASM_HARDWARE_CORESIGHT_H

#define TRACER_ACCESSED_BIT	0
#define TRACER_RUNNING_BIT	1
#define TRACER_CYCLE_ACC_BIT	2
#if defined(CONFIG_SYNO_LSP_HI3536)
#define TRACER_TRACE_DATA_BIT	3
#define TRACER_TIMESTAMP_BIT	4
#define TRACER_BRANCHOUTPUT_BIT	5
#define TRACER_RETURN_STACK_BIT	6
#endif  
#define TRACER_ACCESSED		BIT(TRACER_ACCESSED_BIT)
#define TRACER_RUNNING		BIT(TRACER_RUNNING_BIT)
#define TRACER_CYCLE_ACC	BIT(TRACER_CYCLE_ACC_BIT)
#if defined(CONFIG_SYNO_LSP_HI3536)
#define TRACER_TRACE_DATA	BIT(TRACER_TRACE_DATA_BIT)
#define TRACER_TIMESTAMP	BIT(TRACER_TIMESTAMP_BIT)
#define TRACER_BRANCHOUTPUT	BIT(TRACER_BRANCHOUTPUT_BIT)
#define TRACER_RETURN_STACK	BIT(TRACER_RETURN_STACK_BIT)
#endif  

#define TRACER_TIMEOUT 10000

#if defined(MY_ABC_HERE)
#define etm_writel(t, v, x) \
	(writel_relaxed((v), (t)->etm_regs + (x)))
#define etm_readl(t, x) (readl_relaxed((t)->etm_regs + (x)))
#elif defined(CONFIG_SYNO_LSP_HI3536)
#define etm_writel(t, id, v, x) \
	(__raw_writel((v), (t)->etm_regs[(id)] + (x)))
#define etm_readl(t, id, x) (__raw_readl((t)->etm_regs[(id)] + (x)))
#else
#define etm_writel(t, v, x) \
	(__raw_writel((v), (t)->etm_regs + (x)))
#define etm_readl(t, x) (__raw_readl((t)->etm_regs + (x)))
#endif

#define CSMR_LOCKACCESS 0xfb0
#define CSMR_LOCKSTATUS 0xfb4
#define CSMR_AUTHSTATUS 0xfb8
#define CSMR_DEVID	0xfc8
#define CSMR_DEVTYPE	0xfcc
 
#define CSCR_CLASS	0xff4

#define CS_LAR_KEY	0xc5acce55

#define ETMR_CTRL		0
#define ETMCTRL_POWERDOWN	1
#define ETMCTRL_PROGRAM		(1 << 10)
#define ETMCTRL_PORTSEL		(1 << 11)
#if defined(CONFIG_SYNO_LSP_HI3536)
#define ETMCTRL_CONTEXTIDSIZE(x) (((x) & 3) << 14)
#else  
#define ETMCTRL_DO_CONTEXTID	(3 << 14)
#endif  
#define ETMCTRL_PORTMASK1	(7 << 4)
#define ETMCTRL_PORTMASK2	(1 << 21)
#define ETMCTRL_PORTMASK	(ETMCTRL_PORTMASK1 | ETMCTRL_PORTMASK2)
#define ETMCTRL_PORTSIZE(x) ((((x) & 7) << 4) | (!!((x) & 8)) << 21)
#define ETMCTRL_DO_CPRT		(1 << 1)
#define ETMCTRL_DATAMASK	(3 << 2)
#define ETMCTRL_DATA_DO_DATA	(1 << 2)
#define ETMCTRL_DATA_DO_ADDR	(1 << 3)
#define ETMCTRL_DATA_DO_BOTH	(ETMCTRL_DATA_DO_DATA | ETMCTRL_DATA_DO_ADDR)
#define ETMCTRL_BRANCH_OUTPUT	(1 << 8)
#define ETMCTRL_CYCLEACCURATE	(1 << 12)
#if defined(CONFIG_SYNO_LSP_HI3536)
#define ETMCTRL_TIMESTAMP_EN	(1 << 28)
#define ETMCTRL_RETURN_STACK_EN	(1 << 29)
#endif  

#define ETMR_CONFCODE		(0x04)
#if defined(CONFIG_SYNO_LSP_HI3536)
#define ETMCCR_ETMIDR_PRESENT	BIT(31)
#endif  

#define ETMR_TRACESSCTRL	(0x18)

#define ETMR_TRIGEVT		(0x08)

#define ETMAAT_IFETCH		0
#define ETMAAT_IEXEC		1
#define ETMAAT_IEXECPASS	2
#define ETMAAT_IEXECFAIL	3
#define ETMAAT_DLOADSTORE	4
#define ETMAAT_DLOAD		5
#define ETMAAT_DSTORE		6
 
#define ETMAAT_JAVA		(0 << 3)
#define ETMAAT_THUMB		(1 << 3)
#define ETMAAT_ARM		(3 << 3)
 
#define ETMAAT_NOVALCMP		(0 << 5)
#define ETMAAT_VALMATCH		(1 << 5)
#define ETMAAT_VALNOMATCH	(3 << 5)
 
#define ETMAAT_EXACTMATCH	(1 << 7)
 
#define ETMAAT_IGNCONTEXTID	(0 << 8)
#define ETMAAT_VALUE1		(1 << 8)
#define ETMAAT_VALUE2		(2 << 8)
#define ETMAAT_VALUE3		(3 << 8)
 
#define ETMAAT_IGNSECURITY	(0 << 10)
#define ETMAAT_NSONLY		(1 << 10)
#define ETMAAT_SONLY		(2 << 10)

#define ETMR_COMP_VAL(x)	(0x40 + (x) * 4)
#define ETMR_COMP_ACC_TYPE(x)	(0x80 + (x) * 4)

#define ETMR_STATUS		(0x10)
#define ETMST_OVERFLOW		BIT(0)
#define ETMST_PROGBIT		BIT(1)
#define ETMST_STARTSTOP		BIT(2)
#define ETMST_TRIGGER		BIT(3)

#define etm_progbit(t)		(etm_readl((t), ETMR_STATUS) & ETMST_PROGBIT)
#define etm_started(t)		(etm_readl((t), ETMR_STATUS) & ETMST_STARTSTOP)
#define etm_triggered(t)	(etm_readl((t), ETMR_STATUS) & ETMST_TRIGGER)

#define ETMR_TRACEENCTRL2	0x1c
#define ETMR_TRACEENCTRL	0x24
#define ETMTE_INCLEXCL		BIT(24)
#define ETMR_TRACEENEVT		0x20

#if defined(CONFIG_SYNO_LSP_HI3536)
#define ETMR_VIEWDATAEVT	0x30
#define ETMR_VIEWDATACTRL1	0x34
#define ETMR_VIEWDATACTRL2	0x38
#define ETMR_VIEWDATACTRL3	0x3c
#define ETMVDC3_EXCLONLY	BIT(16)

#define ETMCTRL_OPTS		(ETMCTRL_DO_CPRT)

#define ETMR_ID			0x1e4
#define ETMIDR_VERSION(x)	(((x) >> 4) & 0xff)
#define ETMIDR_VERSION_3_1	0x21
#define ETMIDR_VERSION_PFT_1_0	0x30

#define ETMR_CCE		0x1e8
#define ETMCCER_RETURN_STACK_IMPLEMENTED	BIT(23)
#define ETMCCER_TIMESTAMPING_IMPLEMENTED	BIT(22)

#define ETMR_TRACEIDR		0x200
#else  
#define ETMCTRL_OPTS		(ETMCTRL_DO_CPRT | \
				ETMCTRL_DATA_DO_ADDR | \
				ETMCTRL_BRANCH_OUTPUT | \
				ETMCTRL_DO_CONTEXTID)
#endif  

#define ETMMR_OSLAR	0x300
#define ETMMR_OSLSR	0x304
#define ETMMR_OSSRR	0x308
#define ETMMR_PDSR	0x314

#define ETBR_DEPTH		0x04
#define ETBR_STATUS		0x0c
#define ETBR_READMEM		0x10
#define ETBR_READADDR		0x14
#define ETBR_WRITEADDR		0x18
#define ETBR_TRIGGERCOUNT	0x1c
#define ETBR_CTRL		0x20
#define ETBR_FORMATTERCTRL	0x304
#define ETBFF_ENFTC		1
#define ETBFF_ENFCONT		BIT(1)
#define ETBFF_FONFLIN		BIT(4)
#define ETBFF_MANUAL_FLUSH	BIT(6)
#define ETBFF_TRIGIN		BIT(8)
#define ETBFF_TRIGEVT		BIT(9)
#define ETBFF_TRIGFL		BIT(10)
#if defined(CONFIG_SYNO_LSP_HI3536)
#define ETBFF_STOPFL		BIT(12)
#endif  

#if defined(MY_ABC_HERE)
#define etb_writel(t, v, x) \
	(writel_relaxed((v), (t)->etb_regs + (x)))
#define etb_readl(t, x) (readl_relaxed((t)->etb_regs + (x)))
#else  
#define etb_writel(t, v, x) \
	(__raw_writel((v), (t)->etb_regs + (x)))
#define etb_readl(t, x) (__raw_readl((t)->etb_regs + (x)))
#endif  

#if defined(CONFIG_SYNO_LSP_HI3536)
#define etm_lock(t, id) \
	do { etm_writel((t), (id), 0, CSMR_LOCKACCESS); } while (0)
#define etm_unlock(t, id) \
	do { etm_writel((t), (id), CS_LAR_KEY, CSMR_LOCKACCESS); } while (0)
#else  
#define etm_lock(t) do { etm_writel((t), 0, CSMR_LOCKACCESS); } while (0)
#define etm_unlock(t) \
	do { etm_writel((t), CS_LAR_KEY, CSMR_LOCKACCESS); } while (0)
#endif  

#define etb_lock(t) do { etb_writel((t), 0, CSMR_LOCKACCESS); } while (0)
#define etb_unlock(t) \
	do { etb_writel((t), CS_LAR_KEY, CSMR_LOCKACCESS); } while (0)

#endif  