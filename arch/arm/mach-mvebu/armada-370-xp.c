#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of_address.h>
#if defined(MY_ABC_HERE)
#include <linux/of_platform.h>
#endif  
#include <linux/io.h>
#if defined(MY_ABC_HERE)
#include <linux/clocksource.h>
#else  
#include <linux/time-armada-370-xp.h>
#endif  
#include <linux/clk/mvebu.h>
#include <linux/dma-mapping.h>
#include <linux/mbus.h>
#include <linux/irqchip.h>
#if defined(MY_ABC_HERE)
#include <linux/slab.h>
#endif  
#include <asm/hardware/cache-l2x0.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include "armada-370-xp.h"
#include "common.h"
#include "coherency.h"
#if defined(MY_ABC_HERE)
#include "mvebu-soc-id.h"

static void __init armada_370_xp_map_io(void)
{
	debug_ll_io_init();
}

static void __init armada_370_xp_timer_and_clk_init(void)
{
	pr_notice("\n  LSP version: %s\n\n", LSP_VERSION);

	mvebu_clocks_init();
	clocksource_of_init();
	coherency_init();
	BUG_ON(mvebu_mbus_dt_init(coherency_available()));
#ifdef CONFIG_CACHE_L2X0
	l2x0_of_init(0, ~0UL);
#endif
}

static void __init i2c_quirk(void)
{
	struct device_node *np;
	u32 dev, rev;

	if (mvebu_get_soc_id(&rev, &dev) == 0 && dev > MV78XX0_A0_REV)
		return;

	for_each_compatible_node(np, NULL, "marvell,mv78230-i2c") {
		struct property *new_compat;

		new_compat = kzalloc(sizeof(*new_compat), GFP_KERNEL);

		new_compat->name = kstrdup("compatible", GFP_KERNEL);
		new_compat->length = sizeof("marvell,mv78230-a0-i2c");
		new_compat->value = kstrdup("marvell,mv78230-a0-i2c",
						GFP_KERNEL);

		of_update_property(np, new_compat);
	}
	return;
}

static void __init armada_370_xp_dt_init(void)
{
	if (of_machine_is_compatible("plathome,openblocks-ax3-4"))
		i2c_quirk();
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
}

static const char * const armada_370_dt_compat[] = {
	"marvell,armada370",
	NULL,
};

DT_MACHINE_START(ARMADA_370_DT, "Marvell Armada 370 (Device Tree)")
	.init_machine	= armada_370_xp_dt_init,
	.map_io		= armada_370_xp_map_io,
	.init_irq	= irqchip_init,
	.init_time	= armada_370_xp_timer_and_clk_init,
	.restart	= mvebu_restart,
	.dt_compat	= armada_370_dt_compat,
	.flags          = MACHINE_NEEDS_CPOLICY_WRITEALLOC,
MACHINE_END

static const char * const armada_xp_dt_compat[] = {
	"marvell,armadaxp",
	NULL,
};

DT_MACHINE_START(ARMADA_XP_DT, "Marvell Armada XP (Device Tree)")
	.smp		= smp_ops(armada_xp_smp_ops),
	.init_machine	= armada_370_xp_dt_init,
	.map_io		= armada_370_xp_map_io,
	.init_irq	= irqchip_init,
	.init_time	= armada_370_xp_timer_and_clk_init,
	.restart	= mvebu_restart,
	.dt_compat	= armada_xp_dt_compat,
	.flags          = (MACHINE_NEEDS_CPOLICY_WRITEALLOC |
			   MACHINE_NEEDS_SHAREABLE_PAGES),
MACHINE_END
#else  

static struct map_desc armada_370_xp_io_desc[] __initdata = {
	{
		.virtual	= (unsigned long) ARMADA_370_XP_REGS_VIRT_BASE,
		.pfn		= __phys_to_pfn(ARMADA_370_XP_REGS_PHYS_BASE),
		.length		= ARMADA_370_XP_REGS_SIZE,
		.type		= MT_DEVICE,
	},
};

void __init armada_370_xp_map_io(void)
{
	iotable_init(armada_370_xp_io_desc, ARRAY_SIZE(armada_370_xp_io_desc));
}

void __init armada_370_xp_timer_and_clk_init(void)
{
	mvebu_clocks_init();
	armada_370_xp_timer_init();
}

void __init armada_370_xp_init_early(void)
{
	char *mbus_soc_name;

	if (of_machine_is_compatible("marvell,armada370"))
		mbus_soc_name = "marvell,armada370-mbus";
	else
		mbus_soc_name = "marvell,armadaxp-mbus";

	mvebu_mbus_init(mbus_soc_name,
			ARMADA_370_XP_MBUS_WINS_BASE,
			ARMADA_370_XP_MBUS_WINS_SIZE,
			ARMADA_370_XP_SDRAM_WINS_BASE,
			ARMADA_370_XP_SDRAM_WINS_SIZE,
			coherency_available());

#ifdef CONFIG_CACHE_L2X0
	l2x0_of_init(0, ~0UL);
#endif
}

static void __init armada_370_xp_dt_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
	coherency_init();
}

static const char * const armada_370_xp_dt_compat[] = {
	"marvell,armada-370-xp",
	NULL,
};

DT_MACHINE_START(ARMADA_XP_DT, "Marvell Armada 370/XP (Device Tree)")
	.smp		= smp_ops(armada_xp_smp_ops),
	.init_machine	= armada_370_xp_dt_init,
	.map_io		= armada_370_xp_map_io,
	.init_early	= armada_370_xp_init_early,
	.init_irq	= irqchip_init,
	.init_time	= armada_370_xp_timer_and_clk_init,
	.restart	= mvebu_restart,
	.dt_compat	= armada_370_xp_dt_compat,
MACHINE_END
#endif  
