#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/kernel.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/delay.h>
#if defined(MY_ABC_HERE)
#include <linux/mvebu-pmsu.h>
#include <asm/smp_plat.h>
#endif  

#if defined(MY_ABC_HERE)
#define SYS_CTRL_CLK_DIVIDER_CTRL_OFFSET               0x0
#define   SYS_CTRL_CLK_DIVIDER_CTRL_RESET_ALL          0xff
#define   SYS_CTRL_CLK_DIVIDER_CTRL_RESET_SHIFT        8
#define SYS_CTRL_CLK_DIVIDER_CTRL2_OFFSET              0x8
#define   SYS_CTRL_CLK_DIVIDER_CTRL2_NBCLK_RATIO_SHIFT 16
#define SYS_CTRL_CLK_DIVIDER_MASK                      0x3F

#define SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL0_OFFSET                 0x0
#define SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL0_RELOAD_SMOOTH_MASK     0xff
#define SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL0_RELOAD_SMOOTH_SHIFT    0x8
#define SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL0_RELOAD_SMOOTH_PCLK     0x10
#define SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL1_OFFSET                 0x4
#define SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL1_RESET_MASK_MASK        0xff
#define SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL1_RESET_MASK_SHIFT       0x0
#define SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL1_RESET_MASK_PCLK        0x10
#define SYS_CTRL_ACTIVATE_IF_CTRL_OFFSET                            0x3c
#define SYS_CTRL_ACTIVATE_IF_CTRL_PMU_DFS_OVRD_EN_MASK              0xff
#define SYS_CTRL_ACTIVATE_IF_CTRL_PMU_DFS_OVRD_EN_SHIFT             17
#define SYS_CTRL_ACTIVATE_IF_CTRL_PMU_DFS_OVRD_EN                   0x1

#define PMU_DFS_RATIO_SHIFT 16
#define PMU_DFS_RATIO_MASK  0x3F
#else  
#define SYS_CTRL_CLK_DIVIDER_CTRL_OFFSET    0x0
#define SYS_CTRL_CLK_DIVIDER_VALUE_OFFSET   0xC
#define SYS_CTRL_CLK_DIVIDER_MASK	    0x3F
#endif  

#define MAX_CPU	    4

#if defined(MY_ABC_HERE)
struct cpu_clk_regs {
	u32 clk_divider_value_offset;
};
#endif  

struct cpu_clk {
	struct clk_hw hw;
	int cpu;
	const char *clk_name;
	const char *parent_name;
	void __iomem *reg_base;
#if defined(MY_ABC_HERE)
	void __iomem *pmu_dfs;
	void __iomem *dfx_server_base;
	const struct cpu_clk_regs *clk_regs;
#endif  
};

static struct clk **clks;

static struct clk_onecell_data clk_data;

#define to_cpu_clk(p) container_of(p, struct cpu_clk, hw)

#if defined(MY_ABC_HERE)
static unsigned long armada_xp_clk_cpu_recalc_rate(struct clk_hw *hwclk,
#else  
static unsigned long clk_cpu_recalc_rate(struct clk_hw *hwclk,
#endif  
					 unsigned long parent_rate)
{
	struct cpu_clk *cpuclk = to_cpu_clk(hwclk);
	u32 reg, div;

#if defined(MY_ABC_HERE)
	reg = readl(cpuclk->reg_base + cpuclk->clk_regs->clk_divider_value_offset);
#else  
	reg = readl(cpuclk->reg_base + SYS_CTRL_CLK_DIVIDER_VALUE_OFFSET);
#endif  
	div = (reg >> (cpuclk->cpu * 8)) & SYS_CTRL_CLK_DIVIDER_MASK;
	return parent_rate / div;
}

#if defined(MY_ABC_HERE)
static unsigned long armada_380_clk_cpu_recalc_rate(struct clk_hw *hwclk,
					 unsigned long parent_rate)
{
	struct cpu_clk *cpuclk = to_cpu_clk(hwclk);
	u32 reg, div;

	if (__clk_is_enabled(hwclk->clk) == false) {
		 
		return parent_rate;
	}

	reg = readl(cpuclk->reg_base + cpuclk->clk_regs->clk_divider_value_offset);
	div = reg & SYS_CTRL_CLK_DIVIDER_MASK;
	return parent_rate / div;
}
#endif  

static long clk_cpu_round_rate(struct clk_hw *hwclk, unsigned long rate,
			       unsigned long *parent_rate)
{
	 
	u32 div;

	div = *parent_rate / rate;
	if (div == 0)
		div = 1;
	else if (div > 3)
		div = 3;

	return *parent_rate / div;
}

#if defined(MY_ABC_HERE)
static int armada_xp_clk_cpu_off_set_rate(struct clk_hw *hwclk, unsigned long rate,
				unsigned long parent_rate)
#else  
static int clk_cpu_set_rate(struct clk_hw *hwclk, unsigned long rate,
			    unsigned long parent_rate)
#endif  
{
	struct cpu_clk *cpuclk = to_cpu_clk(hwclk);
	u32 reg, div;
	u32 reload_mask;

	div = parent_rate / rate;
#if defined(MY_ABC_HERE)
	reg = (readl(cpuclk->reg_base + cpuclk->clk_regs->clk_divider_value_offset)
#else  
	reg = (readl(cpuclk->reg_base + SYS_CTRL_CLK_DIVIDER_VALUE_OFFSET)
#endif  
		& (~(SYS_CTRL_CLK_DIVIDER_MASK << (cpuclk->cpu * 8))))
		| (div << (cpuclk->cpu * 8));
#if defined(MY_ABC_HERE)
	writel(reg, cpuclk->reg_base + cpuclk->clk_regs->clk_divider_value_offset);
#else  
	writel(reg, cpuclk->reg_base + SYS_CTRL_CLK_DIVIDER_VALUE_OFFSET);
#endif  
	 
	reload_mask = 1 << (20 + cpuclk->cpu);

	reg = readl(cpuclk->reg_base + SYS_CTRL_CLK_DIVIDER_CTRL_OFFSET)
	    | reload_mask;
	writel(reg, cpuclk->reg_base + SYS_CTRL_CLK_DIVIDER_CTRL_OFFSET);

	reg = readl(cpuclk->reg_base + SYS_CTRL_CLK_DIVIDER_CTRL_OFFSET)
	    | 1 << 24;
	writel(reg, cpuclk->reg_base + SYS_CTRL_CLK_DIVIDER_CTRL_OFFSET);

	udelay(1000);
	reg &= ~(reload_mask | 1 << 24);
	writel(reg, cpuclk->reg_base + SYS_CTRL_CLK_DIVIDER_CTRL_OFFSET);
	udelay(1000);

	return 0;
}

#if defined(MY_ABC_HERE)
static int armada_xp_clk_cpu_on_set_rate(struct clk_hw *hwclk, unsigned long rate,
			       unsigned long parent_rate)
{
	u32 reg;
	unsigned long fabric_div, target_div, cur_rate;
	struct cpu_clk *cpuclk = to_cpu_clk(hwclk);

	if (!cpuclk->pmu_dfs)
		return -ENODEV;

	cur_rate = __clk_get_rate(hwclk->clk);

	reg = readl(cpuclk->reg_base + SYS_CTRL_CLK_DIVIDER_CTRL2_OFFSET);
	fabric_div = (reg >> SYS_CTRL_CLK_DIVIDER_CTRL2_NBCLK_RATIO_SHIFT) &
		SYS_CTRL_CLK_DIVIDER_MASK;

	if (rate == 2 * cur_rate)
		target_div = fabric_div / 2;
	 
	else
		target_div = fabric_div;

	if (target_div == 0)
		target_div = 1;

	reg = readl(cpuclk->pmu_dfs);
	reg &= ~(PMU_DFS_RATIO_MASK << PMU_DFS_RATIO_SHIFT);
	reg |= (target_div << PMU_DFS_RATIO_SHIFT);
	writel(reg, cpuclk->pmu_dfs);

	reg = readl(cpuclk->reg_base + SYS_CTRL_CLK_DIVIDER_CTRL_OFFSET);
	reg |= (SYS_CTRL_CLK_DIVIDER_CTRL_RESET_ALL <<
		SYS_CTRL_CLK_DIVIDER_CTRL_RESET_SHIFT);
	writel(reg, cpuclk->reg_base + SYS_CTRL_CLK_DIVIDER_CTRL_OFFSET);

	return mvebu_pmsu_dfs_request(cpuclk->cpu);
}

static int armada_xp_clk_cpu_set_rate(struct clk_hw *hwclk, unsigned long rate,
			    unsigned long parent_rate)
{
	if (__clk_is_enabled(hwclk->clk))
		return armada_xp_clk_cpu_on_set_rate(hwclk, rate, parent_rate);
	else
		return armada_xp_clk_cpu_off_set_rate(hwclk, rate, parent_rate);
}

static int armada_380_clk_cpu_set_rate(struct clk_hw *hwclk, unsigned long rate,
			    unsigned long parent_rate)
{
	u32 reg;
	u32 target_div;
	unsigned long cur_rate;
	struct cpu_clk *cpuclk = to_cpu_clk(hwclk);

	if (!cpuclk->pmu_dfs)
		return -ENODEV;

	cur_rate = __clk_get_rate(hwclk->clk);

	if (rate >= cur_rate)
		target_div = 1;
	 
	else
		target_div = 2;

	reg = readl(cpuclk->dfx_server_base + SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL0_OFFSET);
	reg &= ~(SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL0_RELOAD_SMOOTH_MASK <<
			 SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL0_RELOAD_SMOOTH_SHIFT);
	reg |= (SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL0_RELOAD_SMOOTH_PCLK <<
			SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL0_RELOAD_SMOOTH_SHIFT);
	writel(reg, cpuclk->dfx_server_base + SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL0_OFFSET);

	reg = readl(cpuclk->dfx_server_base + SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL1_OFFSET);
	reg &= ~(SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL1_RESET_MASK_MASK <<
			 SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL1_RESET_MASK_SHIFT);
	reg |= (SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL1_RESET_MASK_PCLK <<
			SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL1_RESET_MASK_SHIFT);
	writel(reg, cpuclk->dfx_server_base + SYS_CTRL_CPU_PLL_CLOCK_DIVIDER_CTRL1_OFFSET);

	reg = readl(cpuclk->pmu_dfs);
	reg &= ~(PMU_DFS_RATIO_MASK << PMU_DFS_RATIO_SHIFT);
	reg |= (target_div << PMU_DFS_RATIO_SHIFT);
	writel(reg, cpuclk->pmu_dfs);

	reg = readl(cpuclk->pmu_dfs + SYS_CTRL_ACTIVATE_IF_CTRL_OFFSET);
	reg &= ~(SYS_CTRL_ACTIVATE_IF_CTRL_PMU_DFS_OVRD_EN_MASK <<
			 SYS_CTRL_ACTIVATE_IF_CTRL_PMU_DFS_OVRD_EN_SHIFT);
	reg |= (SYS_CTRL_ACTIVATE_IF_CTRL_PMU_DFS_OVRD_EN <<
			SYS_CTRL_ACTIVATE_IF_CTRL_PMU_DFS_OVRD_EN_SHIFT);
	writel(reg, cpuclk->pmu_dfs + SYS_CTRL_ACTIVATE_IF_CTRL_OFFSET);

	return mvebu_pmsu_dfs_request(cpuclk->cpu);
}
#endif  

#if defined(MY_ABC_HERE)
static const struct clk_ops armada_xp_cpu_ops = {
	.recalc_rate = armada_xp_clk_cpu_recalc_rate,
	.round_rate = clk_cpu_round_rate,
	.set_rate = armada_xp_clk_cpu_set_rate,
};

static const struct clk_ops armada_380_cpu_ops = {
	.recalc_rate = armada_380_clk_cpu_recalc_rate,
	.round_rate = clk_cpu_round_rate,
	.set_rate = armada_380_clk_cpu_set_rate,
};

static const struct cpu_clk_regs armada_xp_cpu_clk_regs = {
	.clk_divider_value_offset = 0xC,
};

static const struct cpu_clk_regs armada_380_cpu_clk_regs = {
	.clk_divider_value_offset = 0x4,
};
#else  
static const struct clk_ops cpu_ops = {
	.recalc_rate = clk_cpu_recalc_rate,
	.round_rate = clk_cpu_round_rate,
	.set_rate = clk_cpu_set_rate,
};
#endif  

void __init of_cpu_clk_setup(struct device_node *node)
{
	struct cpu_clk *cpuclk;
	void __iomem *clock_complex_base = of_iomap(node, 0);
#if defined(MY_ABC_HERE)
	void __iomem *pmu_dfs_base = of_iomap(node, 1);
	void __iomem *dfx_server_base = of_iomap(node, 2);
#endif  
	int ncpus = 0;
	struct device_node *dn;
#if defined(MY_ABC_HERE)
	const struct clk_ops *cpu_ops = NULL;
	const struct cpu_clk_regs *cpu_regs = NULL;
	bool independent_clocks = true;
#endif  

	if (clock_complex_base == NULL) {
		pr_err("%s: clock-complex base register not set\n",
			__func__);
		return;
	}

#if defined(MY_ABC_HERE)
	if (pmu_dfs_base == NULL)
		pr_warn("%s: pmu-dfs base register not set, dynamic frequency scaling not available\n",
			__func__);

#if defined(MY_ABC_HERE)
	if (of_machine_is_compatible("marvell,armada38x")) {
#else  
	if (of_machine_is_compatible("marvell,armada380")) {
#endif  
		if (dfx_server_base == NULL) {
			pr_err("%s: DFX server base register not set\n",
			__func__);
			return;
		}
		cpu_ops = &armada_380_cpu_ops;
		cpu_regs = &armada_380_cpu_clk_regs;
		independent_clocks = false;
		ncpus = 1;
	} else {
		cpu_ops = &armada_xp_cpu_ops;
		cpu_regs = &armada_xp_cpu_clk_regs;
		for_each_node_by_type(dn, "cpu")
			ncpus++;
	}
#else  
	for_each_node_by_type(dn, "cpu")
		ncpus++;
#endif  

	cpuclk = kzalloc(ncpus * sizeof(*cpuclk), GFP_KERNEL);
	if (WARN_ON(!cpuclk))
		return;

	clks = kzalloc(ncpus * sizeof(*clks), GFP_KERNEL);
	if (WARN_ON(!clks))
		goto clks_out;

	for_each_node_by_type(dn, "cpu") {
		struct clk_init_data init;
		struct clk *clk;
		struct clk *parent_clk;
		char *clk_name = kzalloc(5, GFP_KERNEL);
		int cpu, err;

		if (WARN_ON(!clk_name))
			goto bail_out;

		err = of_property_read_u32(dn, "reg", &cpu);
		if (WARN_ON(err))
			goto bail_out;

		sprintf(clk_name, "cpu%d", cpu);
		parent_clk = of_clk_get(node, 0);

		cpuclk[cpu].parent_name = __clk_get_name(parent_clk);
		cpuclk[cpu].clk_name = clk_name;
		cpuclk[cpu].cpu = cpu;
		cpuclk[cpu].reg_base = clock_complex_base;
#if defined(MY_ABC_HERE)
		if (pmu_dfs_base)
			cpuclk[cpu].pmu_dfs = pmu_dfs_base + 4 * cpu;

		cpuclk[cpu].dfx_server_base = dfx_server_base;
		cpuclk[cpu].hw.init = &init;
		cpuclk[cpu].clk_regs = cpu_regs;

		init.name = cpuclk[cpu].clk_name;
		init.ops = cpu_ops;
#else  
		cpuclk[cpu].hw.init = &init;
		init.name = cpuclk[cpu].clk_name;
		init.ops = &cpu_ops;
#endif  
		init.flags = 0;
		init.parent_names = &cpuclk[cpu].parent_name;
		init.num_parents = 1;

		clk = clk_register(NULL, &cpuclk[cpu].hw);
		if (WARN_ON(IS_ERR(clk)))
			goto bail_out;
		clks[cpu] = clk;

#if defined(MY_ABC_HERE)
		if (independent_clocks == false) {
			 
			break;
		}
#endif  
	}
	clk_data.clk_num = MAX_CPU;
	clk_data.clks = clks;
	of_clk_add_provider(node, of_clk_src_onecell_get, &clk_data);

	return;
bail_out:
	kfree(clks);
	while(ncpus--)
		kfree(cpuclk[ncpus].clk_name);
clks_out:
	kfree(cpuclk);
}

CLK_OF_DECLARE(armada_xp_cpu_clock, "marvell,armada-xp-cpu-clock",
					 of_cpu_clk_setup);
