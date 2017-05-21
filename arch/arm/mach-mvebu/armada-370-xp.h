#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#ifndef __MACH_ARMADA_370_XP_H
#define __MACH_ARMADA_370_XP_H

#if defined(MY_ABC_HERE)
 
#else  
#define ARMADA_370_XP_REGS_PHYS_BASE	0xd0000000
#define ARMADA_370_XP_REGS_VIRT_BASE	IOMEM(0xfec00000)
#define ARMADA_370_XP_REGS_SIZE		SZ_1M

#define ARMADA_370_XP_MBUS_WINS_BASE    (ARMADA_370_XP_REGS_PHYS_BASE + 0x20000)
#define ARMADA_370_XP_MBUS_WINS_SIZE    0x100
#define ARMADA_370_XP_SDRAM_WINS_BASE   (ARMADA_370_XP_REGS_PHYS_BASE + 0x20180)
#define ARMADA_370_XP_SDRAM_WINS_SIZE   0x20
#endif  

#ifdef CONFIG_SMP
#include <linux/cpumask.h>

#if defined(MY_ABC_HERE)
#define ARMADA_XP_MAX_CPUS 4
#endif  

void armada_mpic_send_doorbell(const struct cpumask *mask, unsigned int irq);
void armada_xp_mpic_smp_cpu_init(void);
#if defined(MY_ABC_HERE)
void armada_xp_secondary_startup(void);
extern struct smp_operations armada_xp_smp_ops;
#endif  
#endif

#endif  