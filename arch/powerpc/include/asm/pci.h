#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
#ifndef __ASM_POWERPC_PCI_H
#define __ASM_POWERPC_PCI_H
#ifdef __KERNEL__

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>

#include <asm/machdep.h>
#include <asm/scatterlist.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/pci-bridge.h>

#include <asm-generic/pci-dma-compat.h>

#define PCI_PROBE_NONE		-1	 
#define PCI_PROBE_NORMAL	0	 
#define PCI_PROBE_DEVTREE	1	 

#define PCIBIOS_MIN_IO		0x1000
#define PCIBIOS_MIN_MEM		0x10000000

struct pci_dev;

#define IOBASE_BRIDGE_NUMBER	0
#define IOBASE_MEMORY		1
#define IOBASE_IO		2
#define IOBASE_ISA_IO		3
#define IOBASE_ISA_MEM		4

#define pcibios_assign_all_busses() \
	(pci_has_flag(PCI_REASSIGN_ALL_BUS))

static inline void pcibios_penalize_isa_irq(int irq, int active)
{
	 
}

#define HAVE_ARCH_PCI_GET_LEGACY_IDE_IRQ
static inline int pci_get_legacy_ide_irq(struct pci_dev *dev, int channel)
{
	if (ppc_md.pci_get_legacy_ide_irq)
		return ppc_md.pci_get_legacy_ide_irq(dev, channel);
	return channel ? 15 : 14;
}

#ifdef CONFIG_PCI
extern void set_pci_dma_ops(struct dma_map_ops *dma_ops);
extern struct dma_map_ops *get_pci_dma_ops(void);
#else	 
#define set_pci_dma_ops(d)
#define get_pci_dma_ops()	NULL
#endif

#ifdef CONFIG_PPC64

#define PCI_DISABLE_MWI

#ifdef CONFIG_PCI
static inline void pci_dma_burst_advice(struct pci_dev *pdev,
					enum pci_dma_burst_strategy *strat,
					unsigned long *strategy_parameter)
{
	unsigned long cacheline_size;
	u8 byte;

	pci_read_config_byte(pdev, PCI_CACHE_LINE_SIZE, &byte);
	if (byte == 0)
		cacheline_size = 1024;
	else
		cacheline_size = (int) byte * 4;

	*strat = PCI_DMA_BURST_MULTIPLE;
	*strategy_parameter = cacheline_size;
}
#endif

#else  

#ifdef CONFIG_PCI
static inline void pci_dma_burst_advice(struct pci_dev *pdev,
					enum pci_dma_burst_strategy *strat,
					unsigned long *strategy_parameter)
{
	*strat = PCI_DMA_BURST_INFINITY;
	*strategy_parameter = ~0UL;
}
#endif
#endif  

extern int pci_domain_nr(struct pci_bus *bus);

extern int pci_proc_domain(struct pci_bus *bus);

#if defined(MY_ABC_HERE)
 
#else  
 
#define arch_setup_msi_irqs arch_setup_msi_irqs
#define arch_teardown_msi_irqs arch_teardown_msi_irqs
#define arch_msi_check_device arch_msi_check_device
#endif  

struct vm_area_struct;
 
int pci_mmap_page_range(struct pci_dev *pdev, struct vm_area_struct *vma,
			enum pci_mmap_state mmap_state, int write_combine);

#define HAVE_PCI_MMAP	1

extern int pci_legacy_read(struct pci_bus *bus, loff_t port, u32 *val,
			   size_t count);
extern int pci_legacy_write(struct pci_bus *bus, loff_t port, u32 val,
			   size_t count);
extern int pci_mmap_legacy_page_range(struct pci_bus *bus,
				      struct vm_area_struct *vma,
				      enum pci_mmap_state mmap_state);

#define HAVE_PCI_LEGACY	1

#ifdef CONFIG_PPC64

#define PCI_DMA_BUS_IS_PHYS	(0)

#else  

#define PCI_DMA_BUS_IS_PHYS     (1)

#endif  

extern void pcibios_claim_one_bus(struct pci_bus *b);

extern void pcibios_finish_adding_to_bus(struct pci_bus *bus);

extern void pcibios_resource_survey(void);

extern struct pci_controller *init_phb_dynamic(struct device_node *dn);
extern int remove_phb_dynamic(struct pci_controller *phb);

extern struct pci_dev *of_create_pci_dev(struct device_node *node,
					struct pci_bus *bus, int devfn);

extern void of_scan_pci_bridge(struct pci_dev *dev);

extern void of_scan_bus(struct device_node *node, struct pci_bus *bus);
extern void of_rescan_bus(struct device_node *node, struct pci_bus *bus);

struct file;
extern pgprot_t	pci_phys_mem_access_prot(struct file *file,
					 unsigned long pfn,
					 unsigned long size,
					 pgprot_t prot);

#define HAVE_ARCH_PCI_RESOURCE_TO_USER
extern void pci_resource_to_user(const struct pci_dev *dev, int bar,
				 const struct resource *rsrc,
				 resource_size_t *start, resource_size_t *end);

extern resource_size_t pcibios_io_space_offset(struct pci_controller *hose);
extern void pcibios_setup_bus_devices(struct pci_bus *bus);
extern void pcibios_setup_bus_self(struct pci_bus *bus);
extern void pcibios_setup_phb_io_space(struct pci_controller *hose);
extern void pcibios_scan_phb(struct pci_controller *hose);

#endif	 
#endif  
