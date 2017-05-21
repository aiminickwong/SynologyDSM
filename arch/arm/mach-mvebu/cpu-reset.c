#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/reset-controller.h>
#if defined(MY_ABC_HERE)
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/serial_reg.h>

#define INTER_REGS_PHYS_BASE        0xF1000000
#define PORT1_BASE                  (INTER_REGS_PHYS_BASE + 0x12100)
#define UART1_WRITE(val, base, reg) {iowrite32(val, base + (UART_##reg << 2));}
#define SET8N1                      0x3
#define SOFTWARE_SHUTDOWN           0x31
#define SOFTWARE_REBOOT             0x43

extern void (*arm_pm_restart)(char str, const char *cmd);

static void __iomem *uart1_base;

static void synology_power_off(void)
{
	void *base_addr = ioremap(PORT1_BASE, 32);
	UART1_WRITE(SET8N1, base_addr, LCR);
	UART1_WRITE(SOFTWARE_SHUTDOWN, base_addr, TX);
}

static void synology_restart(char mode, const char *cmd)
{
	UART1_WRITE(SET8N1, uart1_base, LCR);
	UART1_WRITE(SOFTWARE_REBOOT, uart1_base, TX);

	mdelay(1000);
	 
	printk("Reboot failed -- System halted\n");
	local_irq_disable();
	while (1);
}
#endif  

static struct of_device_id of_cpu_reset_table[] = {
	{.compatible = "marvell,armada-370-cpu-reset", .data = (void*) 1 },
	{.compatible = "marvell,armada-xp-cpu-reset",  .data = (void*) 4 },
	{.compatible = "marvell,armada-375-cpu-reset", .data = (void*) 2 },
	{.compatible = "marvell,armada-380-cpu-reset", .data = (void*) 2 },
	{   },
};

static void __iomem *cpu_reset_base;

#define CPU_RESET_OFFSET(cpu) (cpu * 0x8)
#define CPU_RESET_ASSERT      BIT(0)

static int mvebu_cpu_reset_assert(struct reset_controller_dev *rcdev,
				  unsigned long idx)
{
	u32 reg;

	reg = readl(cpu_reset_base + CPU_RESET_OFFSET(idx));
	reg |= CPU_RESET_ASSERT;
	writel(reg, cpu_reset_base + CPU_RESET_OFFSET(idx));

	return 0;
}

static int mvebu_cpu_reset_deassert(struct reset_controller_dev *rcdev,
				    unsigned long idx)
{
	u32 reg;

	reg = readl(cpu_reset_base + CPU_RESET_OFFSET(idx));
	reg &= ~CPU_RESET_ASSERT;
	writel(reg, cpu_reset_base + CPU_RESET_OFFSET(idx));

	return 0;
}

static struct reset_control_ops mvebu_cpu_reset_ops = {
	.assert = mvebu_cpu_reset_assert,
	.deassert = mvebu_cpu_reset_deassert,
};

static struct reset_controller_dev mvebu_cpu_reset_dev = {
	.ops = &mvebu_cpu_reset_ops,
};

int __init mvebu_cpu_reset_init(void)
{
	struct device_node *np;
	const struct of_device_id *match;

	np = of_find_matching_node_and_match(NULL, of_cpu_reset_table,
					     &match);
	if (np) {
		pr_info("Initializing CPU Reset module\n");
		cpu_reset_base = of_iomap(np, 0);
		mvebu_cpu_reset_dev.of_node = np;
		mvebu_cpu_reset_dev.nr_resets =
			(unsigned int) match->data;
		reset_controller_register(&mvebu_cpu_reset_dev);
	}
#if defined(MY_ABC_HERE)
	uart1_base = ioremap(PORT1_BASE, 32);
	pm_power_off = synology_power_off;
	arm_pm_restart = synology_restart;
#endif  

	return 0;
}

early_initcall(mvebu_cpu_reset_init);
