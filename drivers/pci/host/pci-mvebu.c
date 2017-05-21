#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/mbus.h>
#include <linux/msi.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_pci.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>

#define PCIE_DEV_ID_OFF		0x0000
#define PCIE_CMD_OFF		0x0004
#define PCIE_DEV_REV_OFF	0x0008
#define PCIE_BAR_LO_OFF(n)	(0x0010 + ((n) << 3))
#define PCIE_BAR_HI_OFF(n)	(0x0014 + ((n) << 3))
#define PCIE_HEADER_LOG_4_OFF	0x0128
#define PCIE_BAR_CTRL_OFF(n)	(0x1804 + (((n) - 1) * 4))
#define PCIE_WIN04_CTRL_OFF(n)	(0x1820 + ((n) << 4))
#define PCIE_WIN04_BASE_OFF(n)	(0x1824 + ((n) << 4))
#define PCIE_WIN04_REMAP_OFF(n)	(0x182c + ((n) << 4))
#define PCIE_WIN5_CTRL_OFF	0x1880
#define PCIE_WIN5_BASE_OFF	0x1884
#define PCIE_WIN5_REMAP_OFF	0x188c
#define PCIE_CONF_ADDR_OFF	0x18f8
#define  PCIE_CONF_ADDR_EN		0x80000000
#define  PCIE_CONF_REG(r)		((((r) & 0xf00) << 16) | ((r) & 0xfc))
#define  PCIE_CONF_BUS(b)		(((b) & 0xff) << 16)
#define  PCIE_CONF_DEV(d)		(((d) & 0x1f) << 11)
#define  PCIE_CONF_FUNC(f)		(((f) & 0x7) << 8)
#define  PCIE_CONF_ADDR(bus, devfn, where) \
	(PCIE_CONF_BUS(bus) | PCIE_CONF_DEV(PCI_SLOT(devfn))    | \
	 PCIE_CONF_FUNC(PCI_FUNC(devfn)) | PCIE_CONF_REG(where) | \
	 PCIE_CONF_ADDR_EN)
#define PCIE_CONF_DATA_OFF	0x18fc
#define PCIE_MASK_OFF		0x1910
#define  PCIE_MASK_ENABLE_INTS          0x0f000000
#define PCIE_CTRL_OFF		0x1a00
#define  PCIE_CTRL_X1_MODE		0x0001
#define PCIE_STAT_OFF		0x1a04
#define  PCIE_STAT_BUS                  0xff00
#define  PCIE_STAT_DEV                  0x1f0000
#define  PCIE_STAT_LINK_DOWN		BIT(0)
#define PCIE_DEBUG_CTRL         0x1a60
#define  PCIE_DEBUG_SOFT_RESET		BIT(20)

#define MARVELL_EMULATED_PCI_PCI_BRIDGE_ID 0x7846
#define MVEBU_MAX_PCIE_PORTS	0x4

static u32 nports;
static u32 mbus_pcie_save[MVEBU_MAX_PCIE_PORTS];
struct mvebu_pcie_port *port_bak[MVEBU_MAX_PCIE_PORTS];

struct mvebu_sw_pci_bridge {
	u16 vendor;
	u16 device;
	u16 command;
	u16 class;
	u8 interface;
	u8 revision;
	u8 bist;
	u8 header_type;
	u8 latency_timer;
	u8 cache_line_size;
	u32 bar[2];
	u8 primary_bus;
	u8 secondary_bus;
	u8 subordinate_bus;
	u8 secondary_latency_timer;
	u8 iobase;
	u8 iolimit;
	u16 secondary_status;
	u16 membase;
	u16 memlimit;
	u16 iobaseupper;
	u16 iolimitupper;
	u8 cappointer;
	u8 reserved1;
	u16 reserved2;
	u32 romaddr;
	u8 intline;
	u8 intpin;
	u16 bridgectrl;
};

struct mvebu_pcie_port;

struct mvebu_pcie {
	struct platform_device *pdev;
	struct mvebu_pcie_port *ports;
	struct msi_chip *msi;
	struct resource io;
	char io_name[30];
	struct resource realio;
	char mem_name[30];
	struct resource mem;
	struct resource busn;
	int nports;
};

struct mvebu_pcie_port {
	char *name;
	void __iomem *base;
	spinlock_t conf_lock;
	int haslink;
	u32 port;
	u32 lane;
	int devfn;
	unsigned int mem_target;
	unsigned int mem_attr;
	unsigned int io_target;
	unsigned int io_attr;
	struct clk *clk;
	struct mvebu_sw_pci_bridge bridge;
	struct device_node *dn;
	struct mvebu_pcie *pcie;
	phys_addr_t memwin_base;
	size_t memwin_size;
	phys_addr_t iowin_base;
	size_t iowin_size;
};

static inline bool mvebu_has_ioport(struct mvebu_pcie_port *port)
{
	return port->io_target != -1 && port->io_attr != -1;
}

static bool mvebu_pcie_link_up(struct mvebu_pcie_port *port)
{
	return !(readl(port->base + PCIE_STAT_OFF) & PCIE_STAT_LINK_DOWN);
}

static void mvebu_pcie_set_local_bus_nr(struct mvebu_pcie_port *port, int nr)
{
	u32 stat;

	stat = readl(port->base + PCIE_STAT_OFF);
	stat &= ~PCIE_STAT_BUS;
	stat |= nr << 8;
	writel(stat, port->base + PCIE_STAT_OFF);
}

static void mvebu_pcie_set_local_dev_nr(struct mvebu_pcie_port *port, int nr)
{
	u32 stat;

	stat = readl(port->base + PCIE_STAT_OFF);
	stat &= ~PCIE_STAT_DEV;
	stat |= nr << 16;
	writel(stat, port->base + PCIE_STAT_OFF);
}

static void __init mvebu_pcie_setup_wins(struct mvebu_pcie_port *port)
{
	const struct mbus_dram_target_info *dram;
	u32 size;
	int i;

	dram = mv_mbus_dram_info();

	for (i = 1; i < 3; i++) {
		writel(0, port->base + PCIE_BAR_CTRL_OFF(i));
		writel(0, port->base + PCIE_BAR_LO_OFF(i));
		writel(0, port->base + PCIE_BAR_HI_OFF(i));
	}

	for (i = 0; i < 5; i++) {
		writel(0, port->base + PCIE_WIN04_CTRL_OFF(i));
		writel(0, port->base + PCIE_WIN04_BASE_OFF(i));
		writel(0, port->base + PCIE_WIN04_REMAP_OFF(i));
	}

	writel(0, port->base + PCIE_WIN5_CTRL_OFF);
	writel(0, port->base + PCIE_WIN5_BASE_OFF);
	writel(0, port->base + PCIE_WIN5_REMAP_OFF);

	size = 0;
	for (i = 0; i < dram->num_cs; i++) {
		const struct mbus_dram_window *cs = dram->cs + i;

		writel(cs->base & 0xffff0000,
		       port->base + PCIE_WIN04_BASE_OFF(i));
		writel(0, port->base + PCIE_WIN04_REMAP_OFF(i));
		writel(((cs->size - 1) & 0xffff0000) |
			(cs->mbus_attr << 8) |
			(dram->mbus_dram_target_id << 4) | 1,
		       port->base + PCIE_WIN04_CTRL_OFF(i));

		size += cs->size;
	}

	if ((size & (size - 1)) != 0)
		size = 1 << fls(size);

	writel(dram->cs[0].base, port->base + PCIE_BAR_LO_OFF(1));
	writel(0, port->base + PCIE_BAR_HI_OFF(1));
	writel(((size - 1) & 0xffff0000) | 1,
	       port->base + PCIE_BAR_CTRL_OFF(1));
}

static void mvebu_pcie_setup_hw(struct mvebu_pcie_port *port)
{
	u16 cmd;
	u32 mask;

	mvebu_pcie_setup_wins(port);

	cmd = readw(port->base + PCIE_CMD_OFF);
	cmd |= PCI_COMMAND_IO;
	cmd |= PCI_COMMAND_MEMORY;
	cmd |= PCI_COMMAND_MASTER;
	writew(cmd, port->base + PCIE_CMD_OFF);

	mask = readl(port->base + PCIE_MASK_OFF);
	mask |= PCIE_MASK_ENABLE_INTS;
	writel(mask, port->base + PCIE_MASK_OFF);
}

static int mvebu_pcie_hw_rd_conf(struct mvebu_pcie_port *port,
				 struct pci_bus *bus,
				 u32 devfn, int where, int size, u32 *val)
{
	writel(PCIE_CONF_ADDR(bus->number, devfn, where),
	       port->base + PCIE_CONF_ADDR_OFF);

	*val = readl(port->base + PCIE_CONF_DATA_OFF);

	if (size == 1)
		*val = (*val >> (8 * (where & 3))) & 0xff;
	else if (size == 2)
		*val = (*val >> (8 * (where & 3))) & 0xffff;

	return PCIBIOS_SUCCESSFUL;
}

static int mvebu_pcie_hw_wr_conf(struct mvebu_pcie_port *port,
				 struct pci_bus *bus,
				 u32 devfn, int where, int size, u32 val)
{
	int ret = PCIBIOS_SUCCESSFUL;

	writel(PCIE_CONF_ADDR(bus->number, devfn, where),
	       port->base + PCIE_CONF_ADDR_OFF);

	if (size == 4)
		writel(val, port->base + PCIE_CONF_DATA_OFF);
	else if (size == 2)
		writew(val, port->base + PCIE_CONF_DATA_OFF + (where & 3));
	else if (size == 1)
		writeb(val, port->base + PCIE_CONF_DATA_OFF + (where & 3));
	else
		ret = PCIBIOS_BAD_REGISTER_NUMBER;

	return ret;
}

static void mvebu_pcie_del_windows(struct mvebu_pcie_port *port,
				   phys_addr_t base, size_t size)
{
	while (size) {
		size_t sz = 1 << (fls(size) - 1);

		mvebu_mbus_del_window(base, sz);
		base += sz;
		size -= sz;
	}
}

static void mvebu_pcie_add_windows(struct mvebu_pcie_port *port,
				   unsigned int target, unsigned int attribute,
				   phys_addr_t base, size_t size,
				   phys_addr_t remap)
{
	size_t size_mapped = 0;

	while (size) {
		size_t sz = 1 << (fls(size) - 1);
		int ret;

		ret = mvebu_mbus_add_window_remap_by_id(target, attribute, base,
							sz, remap);
		if (ret) {
			dev_err(&port->pcie->pdev->dev,
				"Could not create MBus window at 0x%x, size 0x%x: %d\n",
				base, sz, ret);
			mvebu_pcie_del_windows(port, base - size_mapped,
					       size_mapped);
			return;
		}

		size -= sz;
		size_mapped += sz;
		base += sz;
		if (remap != MVEBU_MBUS_NO_REMAP)
			remap += sz;
	}
}

static void mvebu_pcie_handle_iobase_change(struct mvebu_pcie_port *port)
{
	phys_addr_t iobase;

	if (port->bridge.iolimit < port->bridge.iobase ||
	    port->bridge.iolimitupper < port->bridge.iobaseupper) {

		if (port->iowin_base) {
			mvebu_pcie_del_windows(port, port->iowin_base,
					       port->iowin_size);
			port->iowin_base = 0;
			port->iowin_size = 0;
		}

		return;
	}

	if (!mvebu_has_ioport(port)) {
		dev_WARN(&port->pcie->pdev->dev,
			 "Attempt to set IO when IO is disabled\n");
		return;
	}

	iobase = ((port->bridge.iobase & 0xF0) << 8) |
		(port->bridge.iobaseupper << 16);
	port->iowin_base = port->pcie->io.start + iobase;
	port->iowin_size = ((0xFFF | ((port->bridge.iolimit & 0xF0) << 8) |
			    (port->bridge.iolimitupper << 16)) -
			    iobase) + 1;

	mvebu_pcie_add_windows(port, port->io_target, port->io_attr,
			       port->iowin_base, port->iowin_size,
			       iobase);

	pci_ioremap_io(iobase, port->iowin_base);
}

static void mvebu_pcie_handle_membase_change(struct mvebu_pcie_port *port)
{
	 
	if (port->bridge.memlimit < port->bridge.membase) {

		if (port->memwin_base) {
			mvebu_pcie_del_windows(port, port->memwin_base,
					       port->memwin_size);
			port->memwin_base = 0;
			port->memwin_size = 0;
		}

		return;
	}

	port->memwin_base  = ((port->bridge.membase & 0xFFF0) << 16);
	port->memwin_size  =
		(((port->bridge.memlimit & 0xFFF0) << 16) | 0xFFFFF) -
		port->memwin_base + 1;

	mvebu_pcie_add_windows(port, port->mem_target, port->mem_attr,
			       port->memwin_base, port->memwin_size,
			       MVEBU_MBUS_NO_REMAP);
}

static void mvebu_sw_pci_bridge_init(struct mvebu_pcie_port *port)
{
	struct mvebu_sw_pci_bridge *bridge = &port->bridge;

	memset(bridge, 0, sizeof(struct mvebu_sw_pci_bridge));

	bridge->class = PCI_CLASS_BRIDGE_PCI;
	bridge->vendor = PCI_VENDOR_ID_MARVELL;
	bridge->device = MARVELL_EMULATED_PCI_PCI_BRIDGE_ID;
	bridge->header_type = PCI_HEADER_TYPE_BRIDGE;
	bridge->cache_line_size = 0x10;

	bridge->iobase = PCI_IO_RANGE_TYPE_32;
	bridge->iolimit = PCI_IO_RANGE_TYPE_32;
}

static int mvebu_sw_pci_bridge_read(struct mvebu_pcie_port *port,
				  unsigned int where, int size, u32 *value)
{
	struct mvebu_sw_pci_bridge *bridge = &port->bridge;

	switch (where & ~3) {
	case PCI_VENDOR_ID:
		*value = bridge->device << 16 | bridge->vendor;
		break;

	case PCI_COMMAND:
		*value = bridge->command;
		break;

	case PCI_CLASS_REVISION:
		*value = bridge->class << 16 | bridge->interface << 8 |
			 bridge->revision;
		break;

	case PCI_CACHE_LINE_SIZE:
		*value = bridge->bist << 24 | bridge->header_type << 16 |
			 bridge->latency_timer << 8 | bridge->cache_line_size;
		break;

	case PCI_BASE_ADDRESS_0 ... PCI_BASE_ADDRESS_1:
		*value = bridge->bar[((where & ~3) - PCI_BASE_ADDRESS_0) / 4];
		break;

	case PCI_PRIMARY_BUS:
		*value = (bridge->secondary_latency_timer << 24 |
			  bridge->subordinate_bus         << 16 |
			  bridge->secondary_bus           <<  8 |
			  bridge->primary_bus);
		break;

	case PCI_IO_BASE:
		if (!mvebu_has_ioport(port))
			*value = bridge->secondary_status << 16;
		else
			*value = (bridge->secondary_status << 16 |
				  bridge->iolimit          <<  8 |
				  bridge->iobase);
		break;

	case PCI_MEMORY_BASE:
		*value = (bridge->memlimit << 16 | bridge->membase);
		break;

	case PCI_PREF_MEMORY_BASE:
		*value = 0;
		break;

	case PCI_IO_BASE_UPPER16:
		*value = (bridge->iolimitupper << 16 | bridge->iobaseupper);
		break;

	case PCI_ROM_ADDRESS1:
		*value = 0;
		break;

	default:
		*value = 0xffffffff;
		return PCIBIOS_BAD_REGISTER_NUMBER;
	}

	if (size == 2)
		*value = (*value >> (8 * (where & 3))) & 0xffff;
	else if (size == 1)
		*value = (*value >> (8 * (where & 3))) & 0xff;

	return PCIBIOS_SUCCESSFUL;
}

static int mvebu_sw_pci_bridge_write(struct mvebu_pcie_port *port,
				     unsigned int where, int size, u32 value)
{
	struct mvebu_sw_pci_bridge *bridge = &port->bridge;
	u32 mask, reg;
	int err;

	if (size == 4)
		mask = 0x0;
	else if (size == 2)
		mask = ~(0xffff << ((where & 3) * 8));
	else if (size == 1)
		mask = ~(0xff << ((where & 3) * 8));
	else
		return PCIBIOS_BAD_REGISTER_NUMBER;

	err = mvebu_sw_pci_bridge_read(port, where & ~3, 4, &reg);
	if (err)
		return err;

	value = (reg & mask) | value << ((where & 3) * 8);

	switch (where & ~3) {
	case PCI_COMMAND:
		if (!mvebu_has_ioport(port))
			value &= ~PCI_COMMAND_IO;

		bridge->command = value & 0xffff;
		break;

	case PCI_BASE_ADDRESS_0 ... PCI_BASE_ADDRESS_1:
		bridge->bar[((where & ~3) - PCI_BASE_ADDRESS_0) / 4] = value;
		break;

	case PCI_IO_BASE:
		 
		bridge->iobase = (value & 0xff) | PCI_IO_RANGE_TYPE_32;
		bridge->iolimit = ((value >> 8) & 0xff) | PCI_IO_RANGE_TYPE_32;
		bridge->secondary_status = value >> 16;
		mvebu_pcie_handle_iobase_change(port);
		break;

	case PCI_MEMORY_BASE:
		bridge->membase = value & 0xffff;
		bridge->memlimit = value >> 16;
		mvebu_pcie_handle_membase_change(port);
		break;

	case PCI_IO_BASE_UPPER16:
		bridge->iobaseupper = value & 0xffff;
		bridge->iolimitupper = value >> 16;
		mvebu_pcie_handle_iobase_change(port);
		break;

	case PCI_PRIMARY_BUS:
		bridge->primary_bus             = value & 0xff;
		bridge->secondary_bus           = (value >> 8) & 0xff;
		bridge->subordinate_bus         = (value >> 16) & 0xff;
		bridge->secondary_latency_timer = (value >> 24) & 0xff;
		mvebu_pcie_set_local_bus_nr(port, bridge->secondary_bus);
		break;

	default:
		break;
	}

	return PCIBIOS_SUCCESSFUL;
}

static inline struct mvebu_pcie *sys_to_pcie(struct pci_sys_data *sys)
{
	return sys->private_data;
}

static struct mvebu_pcie_port *
mvebu_pcie_find_port(struct mvebu_pcie *pcie, struct pci_bus *bus,
		     int devfn)
{
	int i;

	for (i = 0; i < pcie->nports; i++) {
		struct mvebu_pcie_port *port = &pcie->ports[i];
		if (bus->number == 0 && port->devfn == devfn)
			return port;
		if (bus->number != 0 &&
		    bus->number >= port->bridge.secondary_bus &&
		    bus->number <= port->bridge.subordinate_bus)
			return port;
	}

	return NULL;
}

static int mvebu_pcie_wr_conf(struct pci_bus *bus, u32 devfn,
			      int where, int size, u32 val)
{
	struct mvebu_pcie *pcie = sys_to_pcie(bus->sysdata);
	struct mvebu_pcie_port *port;
	unsigned long flags;
	int ret;

	port = mvebu_pcie_find_port(pcie, bus, devfn);
	if (!port)
		return PCIBIOS_DEVICE_NOT_FOUND;

	if (bus->number == 0)
		return mvebu_sw_pci_bridge_write(port, where, size, val);

	if (!port->haslink)
		return PCIBIOS_DEVICE_NOT_FOUND;

	if (bus->number == port->bridge.secondary_bus &&
	    PCI_SLOT(devfn) != 0)
		return PCIBIOS_DEVICE_NOT_FOUND;

	spin_lock_irqsave(&port->conf_lock, flags);
	ret = mvebu_pcie_hw_wr_conf(port, bus, devfn,
				    where, size, val);
	spin_unlock_irqrestore(&port->conf_lock, flags);

	return ret;
}

static int mvebu_pcie_rd_conf(struct pci_bus *bus, u32 devfn, int where,
			      int size, u32 *val)
{
	struct mvebu_pcie *pcie = sys_to_pcie(bus->sysdata);
	struct mvebu_pcie_port *port;
	unsigned long flags;
	int ret;

	port = mvebu_pcie_find_port(pcie, bus, devfn);
	if (!port) {
		*val = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	if (bus->number == 0)
		return mvebu_sw_pci_bridge_read(port, where, size, val);

	if (!port->haslink) {
		*val = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	if (bus->number == port->bridge.secondary_bus &&
	    PCI_SLOT(devfn) != 0) {
		*val = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	spin_lock_irqsave(&port->conf_lock, flags);
	ret = mvebu_pcie_hw_rd_conf(port, bus, devfn,
				    where, size, val);
	spin_unlock_irqrestore(&port->conf_lock, flags);

	return ret;
}

static struct pci_ops mvebu_pcie_ops = {
	.read = mvebu_pcie_rd_conf,
	.write = mvebu_pcie_wr_conf,
};

static int __init mvebu_pcie_setup(int nr, struct pci_sys_data *sys)
{
	struct mvebu_pcie *pcie = sys_to_pcie(sys);
	int i;
	int domain = 0;

#ifdef CONFIG_PCI_DOMAINS
	domain = sys->domain;
#endif

	snprintf(pcie->mem_name, sizeof(pcie->mem_name), "PCI MEM %04x",
		 domain);
	pcie->mem.name = pcie->mem_name;

	snprintf(pcie->io_name, sizeof(pcie->io_name), "PCI I/O %04x", domain);
	pcie->realio.name = pcie->io_name;

	if (request_resource(&iomem_resource, &pcie->mem))
		return 0;

	if (resource_size(&pcie->realio) != 0) {
		if (request_resource(&ioport_resource, &pcie->realio)) {
			release_resource(&pcie->mem);
			return 0;
		}
		pci_add_resource_offset(&sys->resources, &pcie->realio,
					sys->io_offset);
	}
	pci_add_resource_offset(&sys->resources, &pcie->mem, sys->mem_offset);
	pci_add_resource(&sys->resources, &pcie->busn);

	for (i = 0; i < pcie->nports; i++) {
		struct mvebu_pcie_port *port = &pcie->ports[i];
		if (!port->base)
			continue;
		mvebu_pcie_setup_hw(port);
	}

	return 1;
}

static int __init mvebu_pcie_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	struct of_phandle_args oirq;
	int ret;

	ret = of_irq_parse_pci(dev, &oirq);
	if (ret)
		return ret;

	return irq_create_of_mapping(&oirq);
}

static struct pci_bus *mvebu_pcie_scan_bus(int nr, struct pci_sys_data *sys)
{
	struct mvebu_pcie *pcie = sys_to_pcie(sys);
	struct pci_bus *bus;

	bus = pci_create_root_bus(&pcie->pdev->dev, sys->busnr,
				  &mvebu_pcie_ops, sys, &sys->resources);
	if (!bus)
		return NULL;

	pci_scan_child_bus(bus);

	return bus;
}

void mvebu_pcie_add_bus(struct pci_bus *bus)
{
	struct mvebu_pcie *pcie = sys_to_pcie(bus->sysdata);
	bus->msi = pcie->msi;
}

resource_size_t mvebu_pcie_align_resource(struct pci_dev *dev,
					  const struct resource *res,
					  resource_size_t start,
					  resource_size_t size,
					  resource_size_t align)
{
	if (dev->bus->number != 0)
		return start;

	if (res->flags & IORESOURCE_IO)
		return round_up(start, max_t(resource_size_t, SZ_64K,
					     rounddown_pow_of_two(size)));
	else if (res->flags & IORESOURCE_MEM)
		return round_up(start, max_t(resource_size_t, SZ_1M,
					     rounddown_pow_of_two(size)));
	else
		return start;
}

static void __init mvebu_pcie_enable(struct mvebu_pcie *pcie)
{
	struct hw_pci hw;

	memset(&hw, 0, sizeof(hw));

	hw.nr_controllers = 1;
	hw.private_data   = (void **)&pcie;
	hw.setup          = mvebu_pcie_setup;
	hw.scan           = mvebu_pcie_scan_bus;
	hw.map_irq        = mvebu_pcie_map_irq;
	hw.ops            = &mvebu_pcie_ops;
	hw.align_resource = mvebu_pcie_align_resource;
	hw.add_bus        = mvebu_pcie_add_bus;

	pci_common_init(&hw);
}

static void __iomem * __init
mvebu_pcie_map_registers(struct platform_device *pdev,
			 struct device_node *np,
			 struct mvebu_pcie_port *port)
{
	struct resource regs;
	int ret = 0;

	ret = of_address_to_resource(np, 0, &regs);
	if (ret)
		return ERR_PTR(ret);

	return devm_ioremap_resource(&pdev->dev, &regs);
}

static void __init mvebu_pcie_msi_enable(struct mvebu_pcie *pcie)
{
	struct device_node *msi_node;

	msi_node = of_parse_phandle(pcie->pdev->dev.of_node,
				    "msi-parent", 0);
	if (!msi_node)
		return;

	pcie->msi = of_pci_find_msi_chip_by_node(msi_node);

	if (pcie->msi)
		pcie->msi->dev = &pcie->pdev->dev;
}

#define DT_FLAGS_TO_TYPE(flags)       (((flags) >> 24) & 0x03)
#define    DT_TYPE_IO                 0x1
#define    DT_TYPE_MEM32              0x2
#define DT_CPUADDR_TO_TARGET(cpuaddr) (((cpuaddr) >> 56) & 0xFF)
#define DT_CPUADDR_TO_ATTR(cpuaddr)   (((cpuaddr) >> 48) & 0xFF)

static int mvebu_get_tgt_attr(struct device_node *np, int devfn,
			      unsigned long type,
			      unsigned int *tgt,
			      unsigned int *attr)
{
	const int na = 3, ns = 2;
	const __be32 *range;
	int rlen, nranges, rangesz, pna, i;

	*tgt = -1;
	*attr = -1;

	range = of_get_property(np, "ranges", &rlen);
	if (!range)
		return -EINVAL;

	pna = of_n_addr_cells(np);
	rangesz = pna + na + ns;
	nranges = rlen / sizeof(__be32) / rangesz;

	for (i = 0; i < nranges; i++) {
		u32 flags = of_read_number(range, 1);
		u32 slot = of_read_number(range, 2);
		u64 cpuaddr = of_read_number(range + na, pna);
#if defined(MY_ABC_HERE)
		unsigned long rtype = 0;
#else  
		unsigned long rtype;
#endif  

		if (DT_FLAGS_TO_TYPE(flags) == DT_TYPE_IO)
			rtype = IORESOURCE_IO;
		else if (DT_FLAGS_TO_TYPE(flags) == DT_TYPE_MEM32)
			rtype = IORESOURCE_MEM;

		if (slot == PCI_SLOT(devfn) && type == rtype) {
			*tgt = DT_CPUADDR_TO_TARGET(cpuaddr);
			*attr = DT_CPUADDR_TO_ATTR(cpuaddr);
			return 0;
		}

		range += rangesz;
	}

	return -ENOENT;
}

static int mvebu_pcie_suspend(struct platform_device *pdev, pm_message_t message)
{
	int i;

	for (i = 0; i < nports; i++)
		mbus_pcie_save[i] = (readl(port_bak[i]->base + PCIE_STAT_OFF));

	return 0;
}

int mvebu_pcie_resume(void)
{
	int i;

	for (i = 0; i < nports; i++) {
		writel_relaxed(mbus_pcie_save[i], port_bak[i]->base + PCIE_STAT_OFF);
		mvebu_pcie_setup_hw(port_bak[i]);
	}

	return 0;
}

#if defined(MY_ABC_HERE)
static const struct dev_pm_ops mvebu_pcie_pm_ops = {
	.suspend_noirq        = mvebu_pcie_suspend,
	.resume_noirq         = mvebu_pcie_resume,
};
#endif  

static int __init mvebu_pcie_probe(struct platform_device *pdev)
{
	struct mvebu_pcie *pcie;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *child;
	int i, ret;

	pcie = devm_kzalloc(&pdev->dev, sizeof(struct mvebu_pcie),
			    GFP_KERNEL);
	if (!pcie)
		return -ENOMEM;

	pcie->pdev = pdev;

	mvebu_mbus_get_pcie_mem_aperture(&pcie->mem);
	if (resource_size(&pcie->mem) == 0) {
		dev_err(&pdev->dev, "invalid memory aperture size\n");
		return -EINVAL;
	}

	mvebu_mbus_get_pcie_io_aperture(&pcie->io);

	if (resource_size(&pcie->io) != 0) {
		pcie->realio.flags = pcie->io.flags;
		pcie->realio.start = PCIBIOS_MIN_IO;
		pcie->realio.end = min_t(resource_size_t,
					 IO_SPACE_LIMIT,
					 resource_size(&pcie->io));
	} else
		pcie->realio = pcie->io;

	ret = of_pci_parse_bus_range(np, &pcie->busn);
	if (ret) {
		dev_err(&pdev->dev, "failed to parse bus-range property: %d\n",
			ret);
		return ret;
	}

	i = 0;
	for_each_child_of_node(pdev->dev.of_node, child) {
		if (!of_device_is_available(child))
			continue;
		i++;
	}

	pcie->ports = devm_kzalloc(&pdev->dev, i *
				   sizeof(struct mvebu_pcie_port),
				   GFP_KERNEL);
	if (!pcie->ports)
		return -ENOMEM;

	i = 0;
	for_each_child_of_node(pdev->dev.of_node, child) {
		struct mvebu_pcie_port *port = &pcie->ports[i];

		if (!of_device_is_available(child))
			continue;

		port->pcie = pcie;

		if (of_property_read_u32(child, "marvell,pcie-port",
					 &port->port)) {
			dev_warn(&pdev->dev,
				 "ignoring PCIe DT node, missing pcie-port property\n");
			continue;
		}

		if (of_property_read_u32(child, "marvell,pcie-lane",
					 &port->lane))
			port->lane = 0;

		port->name = kasprintf(GFP_KERNEL, "pcie%d.%d",
				       port->port, port->lane);

		port->devfn = of_pci_get_devfn(child);
		if (port->devfn < 0)
			continue;

		ret = mvebu_get_tgt_attr(np, port->devfn, IORESOURCE_MEM,
					 &port->mem_target, &port->mem_attr);
		if (ret < 0) {
			dev_err(&pdev->dev, "PCIe%d.%d: cannot get tgt/attr for mem window\n",
				port->port, port->lane);
			continue;
		}

		if (resource_size(&pcie->io) != 0)
			mvebu_get_tgt_attr(np, port->devfn, IORESOURCE_IO,
					   &port->io_target, &port->io_attr);
		else {
			port->io_target = -1;
			port->io_attr = -1;
		}

		port->clk = of_clk_get_by_name(child, NULL);
		if (IS_ERR(port->clk)) {
			dev_err(&pdev->dev, "PCIe%d.%d: cannot get clock\n",
			       port->port, port->lane);
			continue;
		}

		ret = clk_prepare_enable(port->clk);
		if (ret)
			continue;

		port->base = mvebu_pcie_map_registers(pdev, child, port);
		if (IS_ERR(port->base)) {
			dev_err(&pdev->dev, "PCIe%d.%d: cannot map registers\n",
				port->port, port->lane);
			port->base = NULL;
			clk_disable_unprepare(port->clk);
			continue;
		}

		mvebu_pcie_set_local_dev_nr(port, 1);

		if (mvebu_pcie_link_up(port)) {
			port->haslink = 1;
			dev_info(&pdev->dev, "PCIe%d.%d: link up\n",
				 port->port, port->lane);
		} else {
			port->haslink = 0;
			dev_info(&pdev->dev, "PCIe%d.%d: link down\n",
				 port->port, port->lane);
		}

		port->dn = child;
		spin_lock_init(&port->conf_lock);
		mvebu_sw_pci_bridge_init(port);
		port_bak[i] = port;
		i++;
	}

	nports = i;
	pcie->nports = i;
	mvebu_pcie_msi_enable(pcie);
	mvebu_pcie_enable(pcie);

	return 0;
}

static const struct of_device_id mvebu_pcie_of_match_table[] = {
	{ .compatible = "marvell,armada-xp-pcie", },
	{ .compatible = "marvell,armada-370-pcie", },
	{ .compatible = "marvell,armada-375-pcie", },
	{ .compatible = "marvell,armada-38x-pcie", },
	{},
};
MODULE_DEVICE_TABLE(of, mvebu_pcie_of_match_table);

static struct platform_driver mvebu_pcie_driver = {
#if defined(MY_ABC_HERE)
	 
#else  
#ifdef CONFIG_PM
	.suspend        = mvebu_pcie_suspend,
	 
#endif
#endif  
	.driver = {
		.owner = THIS_MODULE,
		.name = "mvebu-pcie",
#if defined(MY_ABC_HERE)
#ifdef CONFIG_PM
		.pm = &mvebu_pcie_pm_ops,
#endif
#endif  
		.of_match_table =
		   of_match_ptr(mvebu_pcie_of_match_table),
	},
};

static int __init mvebu_pcie_init(void)
{
	return platform_driver_probe(&mvebu_pcie_driver,
				     mvebu_pcie_probe);
}

subsys_initcall(mvebu_pcie_init);

MODULE_AUTHOR("Thomas Petazzoni <thomas.petazzoni@free-electrons.com>");
MODULE_DESCRIPTION("Marvell EBU PCIe driver");
MODULE_LICENSE("GPLv2");
