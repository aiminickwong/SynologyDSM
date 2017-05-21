#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/io.h>

#include <asm/mach-types.h>
#include <asm/mach/map.h>
#include <asm/mach/pci.h>

static int debug_pci;

static void pcibios_bus_report_status(struct pci_bus *bus, u_int status_mask, int warn)
{
	struct pci_dev *dev;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		u16 status;

		if (dev->bus->number == 0 && dev->devfn == 0)
			continue;

		pci_read_config_word(dev, PCI_STATUS, &status);
		if (status == 0xffff)
			continue;

		if ((status & status_mask) == 0)
			continue;

		pci_write_config_word(dev, PCI_STATUS, status & status_mask);

		if (warn)
			printk("(%s: %04X) ", pci_name(dev), status);
	}

	list_for_each_entry(dev, &bus->devices, bus_list)
		if (dev->subordinate)
			pcibios_bus_report_status(dev->subordinate, status_mask, warn);
}

void pcibios_report_status(u_int status_mask, int warn)
{
	struct list_head *l;

	list_for_each(l, &pci_root_buses) {
		struct pci_bus *bus = pci_bus_b(l);

		pcibios_bus_report_status(bus, status_mask, warn);
	}
}

static void pci_fixup_83c553(struct pci_dev *dev)
{
	 
	pci_write_config_dword(dev, PCI_BASE_ADDRESS_0, PCI_BASE_ADDRESS_SPACE_MEMORY);
	pci_write_config_word(dev, PCI_COMMAND, PCI_COMMAND_IO);

	dev->resource[0].end -= dev->resource[0].start;
	dev->resource[0].start = 0;

	pci_write_config_byte(dev, 0x48, 0xff);

	pci_write_config_byte(dev, 0x42, 0x01);

	pci_write_config_byte(dev, 0x40, 0x22);

	pci_write_config_byte(dev, 0x83, 0x02);

	pci_write_config_byte(dev, 0x80, 0x11);
	pci_write_config_byte(dev, 0x81, 0x00);

	pci_write_config_word(dev, 0x44, 0xb000);
	outb(0x08, 0x4d1);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_WINBOND, PCI_DEVICE_ID_WINBOND_83C553, pci_fixup_83c553);

static void pci_fixup_unassign(struct pci_dev *dev)
{
	dev->resource[0].end -= dev->resource[0].start;
	dev->resource[0].start = 0;
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_WINBOND2, PCI_DEVICE_ID_WINBOND2_89C940F, pci_fixup_unassign);

static void pci_fixup_dec21285(struct pci_dev *dev)
{
	int i;

	if (dev->devfn == 0) {
		dev->class &= 0xff;
		dev->class |= PCI_CLASS_BRIDGE_HOST << 8;
		for (i = 0; i < PCI_NUM_RESOURCES; i++) {
			dev->resource[i].start = 0;
			dev->resource[i].end   = 0;
			dev->resource[i].flags = 0;
		}
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_DEC, PCI_DEVICE_ID_DEC_21285, pci_fixup_dec21285);

static void pci_fixup_ide_bases(struct pci_dev *dev)
{
	struct resource *r;
	int i;

	if ((dev->class >> 8) != PCI_CLASS_STORAGE_IDE)
		return;

	for (i = 0; i < PCI_NUM_RESOURCES; i++) {
		r = dev->resource + i;
		if ((r->start & ~0x80) == 0x374) {
			r->start |= 2;
			r->end = r->start;
		}
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_ANY_ID, PCI_ANY_ID, pci_fixup_ide_bases);

static void pci_fixup_dec21142(struct pci_dev *dev)
{
	pci_write_config_dword(dev, 0x40, 0x80000000);
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_DEC, PCI_DEVICE_ID_DEC_21142, pci_fixup_dec21142);

static void pci_fixup_cy82c693(struct pci_dev *dev)
{
	if ((dev->class >> 8) == PCI_CLASS_STORAGE_IDE) {
		u32 base0, base1;

		if (dev->class & 0x80) {	 
			base0 = 0x1f0;
			base1 = 0x3f4;
		} else {			 
			base0 = 0x170;
			base1 = 0x374;
		}

		pci_write_config_dword(dev, PCI_BASE_ADDRESS_0,
				       base0 | PCI_BASE_ADDRESS_SPACE_IO);
		pci_write_config_dword(dev, PCI_BASE_ADDRESS_1,
				       base1 | PCI_BASE_ADDRESS_SPACE_IO);

		dev->resource[0].start = 0;
		dev->resource[0].end   = 0;
		dev->resource[0].flags = 0;

		dev->resource[1].start = 0;
		dev->resource[1].end   = 0;
		dev->resource[1].flags = 0;
	} else if (PCI_FUNC(dev->devfn) == 0) {
		 
		pci_write_config_byte(dev, 0x4b, 14);
		pci_write_config_byte(dev, 0x4c, 15);

		pci_write_config_byte(dev, 0x4d, 0x41);

		pci_write_config_byte(dev, 0x44, 0x17);

		pci_write_config_byte(dev, 0x45, 0x03);
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_CONTAQ, PCI_DEVICE_ID_CONTAQ_82C693, pci_fixup_cy82c693);

static void pci_fixup_it8152(struct pci_dev *dev)
{
	int i;
	 
	if ((dev->class >> 8) == PCI_CLASS_BRIDGE_HOST ||
	    dev->class == 0x68000 ||
	    dev->class == 0x80103) {
		for (i = 0; i < PCI_NUM_RESOURCES; i++) {
			dev->resource[i].start = 0;
			dev->resource[i].end   = 0;
			dev->resource[i].flags = 0;
		}
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_VENDOR_ID_ITE, PCI_DEVICE_ID_ITE_8152, pci_fixup_it8152);

static inline int pdev_bad_for_parity(struct pci_dev *dev)
{
	return ((dev->vendor == PCI_VENDOR_ID_INTERG &&
		 (dev->device == PCI_DEVICE_ID_INTERG_2000 ||
		  dev->device == PCI_DEVICE_ID_INTERG_2010)) ||
		(dev->vendor == PCI_VENDOR_ID_ITE &&
		 dev->device == PCI_DEVICE_ID_ITE_8152));

}

void pcibios_fixup_bus(struct pci_bus *bus)
{
	struct pci_dev *dev;
	u16 features = PCI_COMMAND_SERR | PCI_COMMAND_PARITY | PCI_COMMAND_FAST_BACK;

	list_for_each_entry(dev, &bus->devices, bus_list) {
		u16 status;

		pci_read_config_word(dev, PCI_STATUS, &status);

		if (!(status & PCI_STATUS_FAST_BACK))
			features &= ~PCI_COMMAND_FAST_BACK;

		if (pdev_bad_for_parity(dev))
			features &= ~(PCI_COMMAND_SERR | PCI_COMMAND_PARITY);

		switch (dev->class >> 8) {
		case PCI_CLASS_BRIDGE_PCI:
			pci_read_config_word(dev, PCI_BRIDGE_CONTROL, &status);
			status |= PCI_BRIDGE_CTL_PARITY|PCI_BRIDGE_CTL_MASTER_ABORT;
			status &= ~(PCI_BRIDGE_CTL_BUS_RESET|PCI_BRIDGE_CTL_FAST_BACK);
			pci_write_config_word(dev, PCI_BRIDGE_CONTROL, status);
			break;

		case PCI_CLASS_BRIDGE_CARDBUS:
			pci_read_config_word(dev, PCI_CB_BRIDGE_CONTROL, &status);
			status |= PCI_CB_BRIDGE_CTL_PARITY|PCI_CB_BRIDGE_CTL_MASTER_ABORT;
			pci_write_config_word(dev, PCI_CB_BRIDGE_CONTROL, status);
			break;
		}
	}

	list_for_each_entry(dev, &bus->devices, bus_list) {
		u16 cmd;

		pci_read_config_word(dev, PCI_COMMAND, &cmd);
		cmd |= features;
		pci_write_config_word(dev, PCI_COMMAND, cmd);

		pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE,
				      L1_CACHE_BYTES >> 2);
	}

	if (bus->self && bus->self->hdr_type == PCI_HEADER_TYPE_BRIDGE) {
		if (features & PCI_COMMAND_FAST_BACK)
			bus->bridge_ctl |= PCI_BRIDGE_CTL_FAST_BACK;
		if (features & PCI_COMMAND_PARITY)
			bus->bridge_ctl |= PCI_BRIDGE_CTL_PARITY;
	}

	printk(KERN_INFO "PCI: bus%d: Fast back to back transfers %sabled\n",
		bus->number, (features & PCI_COMMAND_FAST_BACK) ? "en" : "dis");
}
EXPORT_SYMBOL(pcibios_fixup_bus);

#if defined (MY_DEF_HERE) || defined(MY_ABC_HERE)
void pcibios_add_bus(struct pci_bus *bus)
{
	struct pci_sys_data *sys = bus->sysdata;
	if (sys->add_bus)
		sys->add_bus(bus);
}

void pcibios_remove_bus(struct pci_bus *bus)
{
	struct pci_sys_data *sys = bus->sysdata;
	if (sys->remove_bus)
		sys->remove_bus(bus);
}
#endif  
 
static u8 pcibios_swizzle(struct pci_dev *dev, u8 *pin)
{
	struct pci_sys_data *sys = dev->sysdata;
	int slot, oldpin = *pin;

	if (sys->swizzle)
		slot = sys->swizzle(dev, pin);
	else
		slot = pci_common_swizzle(dev, pin);

	if (debug_pci)
		printk("PCI: %s swizzling pin %d => pin %d slot %d\n",
			pci_name(dev), oldpin, *pin, slot);

	return slot;
}

static int pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	struct pci_sys_data *sys = dev->sysdata;
	int irq = -1;

	if (sys->map_irq)
		irq = sys->map_irq(dev, slot, pin);

	if (debug_pci)
		printk("PCI: %s mapping slot %d pin %d => irq %d\n",
			pci_name(dev), slot, pin, irq);

	return irq;
}

static int pcibios_init_resources(int busnr, struct pci_sys_data *sys)
{
	int ret;
	struct pci_host_bridge_window *window;

	if (list_empty(&sys->resources)) {
		pci_add_resource_offset(&sys->resources,
			 &iomem_resource, sys->mem_offset);
	}

	list_for_each_entry(window, &sys->resources, list) {
		if (resource_type(window->res) == IORESOURCE_IO)
			return 0;
	}

	sys->io_res.start = (busnr * SZ_64K) ?  : pcibios_min_io;
	sys->io_res.end = (busnr + 1) * SZ_64K - 1;
	sys->io_res.flags = IORESOURCE_IO;
	sys->io_res.name = sys->io_res_name;
	sprintf(sys->io_res_name, "PCI%d I/O", busnr);

	ret = request_resource(&ioport_resource, &sys->io_res);
	if (ret) {
		pr_err("PCI: unable to allocate I/O port region (%d)\n", ret);
		return ret;
	}
	pci_add_resource_offset(&sys->resources, &sys->io_res,
				sys->io_offset);

	return 0;
}

#if defined (MY_DEF_HERE)
static void pcibios_init_hw(struct device *parent, struct hw_pci *hw,
			    struct list_head *head)
#else  
static void pcibios_init_hw(struct hw_pci *hw, struct list_head *head)
#endif  
{
	struct pci_sys_data *sys = NULL;
	int ret;
#if defined (MY_DEF_HERE)
	int nr;
	static int busnr;

	for (nr = 0; nr < hw->nr_controllers; nr++) {
#else  
	int nr, busnr;

	for (nr = busnr = 0; nr < hw->nr_controllers; nr++) {
#endif  
		sys = kzalloc(sizeof(struct pci_sys_data), GFP_KERNEL);
		if (!sys)
			panic("PCI: unable to allocate sys data!");

#ifdef CONFIG_PCI_DOMAINS
		sys->domain  = hw->domain;
#endif
		sys->busnr   = busnr;
		sys->swizzle = hw->swizzle;
		sys->map_irq = hw->map_irq;
		sys->align_resource = hw->align_resource;
#if defined (MY_DEF_HERE) || defined(MY_ABC_HERE)
		sys->add_bus = hw->add_bus;
		sys->remove_bus = hw->remove_bus;
#endif  
		INIT_LIST_HEAD(&sys->resources);

		if (hw->private_data)
			sys->private_data = hw->private_data[nr];

		ret = hw->setup(nr, sys);

		if (ret > 0) {
			ret = pcibios_init_resources(nr, sys);
			if (ret)  {
				kfree(sys);
				break;
			}

			if (hw->scan)
				sys->bus = hw->scan(nr, sys);
			else
#if defined (MY_DEF_HERE)
				sys->bus = pci_scan_root_bus(parent, sys->busnr,
						hw->ops, sys, &sys->resources);
#else  
				sys->bus = pci_scan_root_bus(NULL, sys->busnr,
						hw->ops, sys, &sys->resources);
#endif  

			if (!sys->bus)
				panic("PCI: unable to scan bus!");

			busnr = sys->bus->busn_res.end + 1;

			list_add(&sys->node, head);
		} else {
			kfree(sys);
			if (ret < 0)
				break;
		}
	}
}

#if defined (MY_DEF_HERE)
void pci_common_init_dev(struct device *parent, struct hw_pci *hw)
#else  
void pci_common_init(struct hw_pci *hw)
#endif  
{
	struct pci_sys_data *sys;
	LIST_HEAD(head);

	pci_add_flags(PCI_REASSIGN_ALL_RSRC);
	if (hw->preinit)
		hw->preinit();
#if defined (MY_DEF_HERE)
	pcibios_init_hw(parent, hw, &head);
#else  
	pcibios_init_hw(hw, &head);
#endif  
	if (hw->postinit)
		hw->postinit();

	pci_fixup_irqs(pcibios_swizzle, pcibios_map_irq);

	list_for_each_entry(sys, &head, node) {
		struct pci_bus *bus = sys->bus;

		if (!pci_has_flag(PCI_PROBE_ONLY)) {
			 
			pci_bus_size_bridges(bus);

			pci_bus_assign_resources(bus);

			pci_enable_bridges(bus);
		}

		pci_bus_add_devices(bus);
	}
}

#ifndef CONFIG_PCI_HOST_ITE8152
void pcibios_set_master(struct pci_dev *dev)
{
	 
}
#endif

char * __init pcibios_setup(char *str)
{
	if (!strcmp(str, "debug")) {
		debug_pci = 1;
		return NULL;
	} else if (!strcmp(str, "firmware")) {
		pci_add_flags(PCI_PROBE_ONLY);
		return NULL;
	}
	return str;
}

resource_size_t pcibios_align_resource(void *data, const struct resource *res,
				resource_size_t size, resource_size_t align)
{
	struct pci_dev *dev = data;
	struct pci_sys_data *sys = dev->sysdata;
	resource_size_t start = res->start;

	if (res->flags & IORESOURCE_IO && start & 0x300)
		start = (start + 0x3ff) & ~0x3ff;

	start = (start + align - 1) & ~(align - 1);

	if (sys->align_resource)
		return sys->align_resource(dev, res, start, size, align);

	return start;
}

int pcibios_enable_device(struct pci_dev *dev, int mask)
{
	u16 cmd, old_cmd;
	int idx;
	struct resource *r;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	old_cmd = cmd;
	for (idx = 0; idx < 6; idx++) {
		 
		if (!(mask & (1 << idx)))
			continue;

		r = dev->resource + idx;
		if (!r->start && r->end) {
			printk(KERN_ERR "PCI: Device %s not available because"
			       " of resource collisions\n", pci_name(dev));
			return -EINVAL;
		}
		if (r->flags & IORESOURCE_IO)
			cmd |= PCI_COMMAND_IO;
		if (r->flags & IORESOURCE_MEM)
			cmd |= PCI_COMMAND_MEMORY;
	}

	if ((dev->class >> 16) == PCI_BASE_CLASS_BRIDGE)
		cmd |= PCI_COMMAND_IO | PCI_COMMAND_MEMORY;

	if (cmd != old_cmd) {
		printk("PCI: enabling device %s (%04x -> %04x)\n",
		       pci_name(dev), old_cmd, cmd);
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
	return 0;
}

int pci_mmap_page_range(struct pci_dev *dev, struct vm_area_struct *vma,
			enum pci_mmap_state mmap_state, int write_combine)
{
	struct pci_sys_data *root = dev->sysdata;
	unsigned long phys;

	if (mmap_state == pci_mmap_io) {
		return -EINVAL;
	} else {
		phys = vma->vm_pgoff + (root->mem_offset >> PAGE_SHIFT);
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (remap_pfn_range(vma, vma->vm_start, phys,
			     vma->vm_end - vma->vm_start,
			     vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

void __init pci_map_io_early(unsigned long pfn)
{
	struct map_desc pci_io_desc = {
		.virtual	= PCI_IO_VIRT_BASE,
		.type		= MT_DEVICE,
		.length		= SZ_64K,
	};

	pci_io_desc.pfn = pfn;
	iotable_init(&pci_io_desc, 1);
}
