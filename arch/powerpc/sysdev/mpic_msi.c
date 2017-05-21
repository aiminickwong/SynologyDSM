#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/irq.h>
#include <linux/bitmap.h>
#include <linux/msi.h>
#include <asm/mpic.h>
#include <asm/prom.h>
#include <asm/hw_irq.h>
#include <asm/ppc-pci.h>
#include <asm/msi_bitmap.h>

#include <sysdev/mpic.h>

void mpic_msi_reserve_hwirq(struct mpic *mpic, irq_hw_number_t hwirq)
{
	 
	if (!mpic->msi_bitmap.bitmap)
		return;

	msi_bitmap_reserve_hwirq(&mpic->msi_bitmap, hwirq);
}

#ifdef CONFIG_MPIC_U3_HT_IRQS
static int mpic_msi_reserve_u3_hwirqs(struct mpic *mpic)
{
	irq_hw_number_t hwirq;
	const struct irq_domain_ops *ops = mpic->irqhost->ops;
	struct device_node *np;
	int flags, index, i;
#if defined(MY_ABC_HERE)
	struct of_phandle_args oirq;
#else  
	struct of_irq oirq;
#endif  

	pr_debug("mpic: found U3, guessing msi allocator setup\n");

	for (i = 0;   i < 8;   i++)
		msi_bitmap_reserve_hwirq(&mpic->msi_bitmap, i);

	for (i = 42;  i < 46;  i++)
		msi_bitmap_reserve_hwirq(&mpic->msi_bitmap, i);

	for (i = 100; i < 105; i++)
		msi_bitmap_reserve_hwirq(&mpic->msi_bitmap, i);

	for (i = 124; i < mpic->num_sources; i++)
		msi_bitmap_reserve_hwirq(&mpic->msi_bitmap, i);

	np = NULL;
	while ((np = of_find_all_nodes(np))) {
		pr_debug("mpic: mapping hwirqs for %s\n", np->full_name);

		index = 0;
#if defined(MY_ABC_HERE)
		while (of_irq_parse_one(np, index++, &oirq) == 0) {
			ops->xlate(mpic->irqhost, NULL, oirq.args,
						oirq.args_count, &hwirq, &flags);
#else  
		while (of_irq_map_one(np, index++, &oirq) == 0) {
			ops->xlate(mpic->irqhost, NULL, oirq.specifier,
						oirq.size, &hwirq, &flags);
#endif  
			msi_bitmap_reserve_hwirq(&mpic->msi_bitmap, hwirq);
		}
	}

	return 0;
}
#else
static int mpic_msi_reserve_u3_hwirqs(struct mpic *mpic)
{
	return -1;
}
#endif

int mpic_msi_init_allocator(struct mpic *mpic)
{
	int rc;

	rc = msi_bitmap_alloc(&mpic->msi_bitmap, mpic->num_sources,
			      mpic->irqhost->of_node);
	if (rc)
		return rc;

	rc = msi_bitmap_reserve_dt_hwirqs(&mpic->msi_bitmap);
	if (rc > 0) {
		if (mpic->flags & MPIC_U3_HT_IRQS)
			rc = mpic_msi_reserve_u3_hwirqs(mpic);

		if (rc) {
			msi_bitmap_free(&mpic->msi_bitmap);
			return rc;
		}
	}

	return 0;
}
