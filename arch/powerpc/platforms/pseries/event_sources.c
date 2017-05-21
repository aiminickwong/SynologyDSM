#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <asm/prom.h>

#include "pseries.h"

void request_event_sources_irqs(struct device_node *np,
				irq_handler_t handler,
				const char *name)
{
	int i, index, count = 0;
#if defined(MY_ABC_HERE)
	struct of_phandle_args oirq;
#else  
	struct of_irq oirq;
#endif  
	const u32 *opicprop;
	unsigned int opicplen;
	unsigned int virqs[16];

	opicprop = of_get_property(np, "open-pic-interrupt", &opicplen);
	if (opicprop) {
		opicplen /= sizeof(u32);
		for (i = 0; i < opicplen; i++) {
			if (count > 15)
				break;
			virqs[count] = irq_create_mapping(NULL, *(opicprop++));
			if (virqs[count] == NO_IRQ) {
				pr_err("event-sources: Unable to allocate "
				       "interrupt number for %s\n",
				       np->full_name);
				WARN_ON(1);
			}
			else
				count++;

		}
	}
	 
	else {
		 
#if defined(MY_ABC_HERE)
		for (index = 0; of_irq_parse_one(np, index, &oirq) == 0;
#else  
		for (index = 0; of_irq_map_one(np, index, &oirq) == 0;
#endif  
		     index++) {
			if (count > 15)
				break;
#if defined(MY_ABC_HERE)
			virqs[count] = irq_create_of_mapping(&oirq);
#else  
			virqs[count] = irq_create_of_mapping(oirq.controller,
							    oirq.specifier,
							    oirq.size);
#endif  
			if (virqs[count] == NO_IRQ) {
				pr_err("event-sources: Unable to allocate "
				       "interrupt number for %s\n",
				       np->full_name);
				WARN_ON(1);
			}
			else
				count++;
		}
	}

	for (i = 0; i < count; i++) {
		if (request_irq(virqs[i], handler, 0, name, NULL)) {
			pr_err("event-sources: Unable to request interrupt "
			       "%d for %s\n", virqs[i], np->full_name);
			WARN_ON(1);
			return;
		}
	}
}
