#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/err.h>
#include <linux/phy.h>
#if defined(MY_ABC_HERE)
#include <linux/phy_fixed.h>
#endif  
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_mdio.h>
#include <linux/module.h>

MODULE_AUTHOR("Grant Likely <grant.likely@secretlab.ca>");
MODULE_LICENSE("GPL");

int of_mdiobus_register(struct mii_bus *mdio, struct device_node *np)
{
	struct phy_device *phy;
	struct device_node *child;
	const __be32 *paddr;
	u32 addr;
	bool is_c45, scanphys = false;
	int rc, i, len;

	mdio->phy_mask = ~0;

	if (mdio->irq)
		for (i=0; i<PHY_MAX_ADDR; i++)
			mdio->irq[i] = PHY_POLL;

	mdio->dev.of_node = np;

	rc = mdiobus_register(mdio);
	if (rc)
		return rc;

	for_each_available_child_of_node(np, child) {
		 
		paddr = of_get_property(child, "reg", &len);
		if (!paddr || len < sizeof(*paddr)) {
			scanphys = true;
			dev_err(&mdio->dev, "%s has invalid PHY address\n",
				child->full_name);
			continue;
		}

		addr = be32_to_cpup(paddr);
		if (addr >= 32) {
			dev_err(&mdio->dev, "%s PHY address %i is too large\n",
				child->full_name, addr);
			continue;
		}

		if (mdio->irq) {
			mdio->irq[addr] = irq_of_parse_and_map(child, 0);
			if (!mdio->irq[addr])
				mdio->irq[addr] = PHY_POLL;
		}

		is_c45 = of_device_is_compatible(child,
						 "ethernet-phy-ieee802.3-c45");
		phy = get_phy_device(mdio, addr, is_c45);

		if (!phy || IS_ERR(phy)) {
			dev_err(&mdio->dev,
				"cannot get PHY at address %i\n",
				addr);
			continue;
		}

		of_node_get(child);
		phy->dev.of_node = child;

		rc = phy_device_register(phy);
		if (rc) {
			phy_device_free(phy);
			of_node_put(child);
			continue;
		}

		dev_dbg(&mdio->dev, "registered phy %s at address %i\n",
			child->name, addr);
	}

	if (!scanphys)
		return 0;

	for_each_available_child_of_node(np, child) {
		 
		paddr = of_get_property(child, "reg", &len);
		if (paddr)
			continue;

		is_c45 = of_device_is_compatible(child,
						 "ethernet-phy-ieee802.3-c45");

		for (addr = 0; addr < PHY_MAX_ADDR; addr++) {
			 
			if (mdio->phy_map[addr])
				continue;

			dev_info(&mdio->dev, "scan phy %s at address %i\n",
				 child->name, addr);

			phy = get_phy_device(mdio, addr, is_c45);
			if (!phy || IS_ERR(phy))
				continue;

			if (mdio->irq) {
				mdio->irq[addr] =
					irq_of_parse_and_map(child, 0);
				if (!mdio->irq[addr])
					mdio->irq[addr] = PHY_POLL;
			}

			of_node_get(child);
			phy->dev.of_node = child;

			rc = phy_device_register(phy);
			if (rc) {
				phy_device_free(phy);
				of_node_put(child);
				continue;
			}

			dev_info(&mdio->dev, "registered phy %s at address %i\n",
				 child->name, addr);
			break;
		}
	}

	return 0;
}
EXPORT_SYMBOL(of_mdiobus_register);

static int of_phy_match(struct device *dev, void *phy_np)
{
	return dev->of_node == phy_np;
}

struct phy_device *of_phy_find_device(struct device_node *phy_np)
{
	struct device *d;
	if (!phy_np)
		return NULL;

	d = bus_find_device(&mdio_bus_type, NULL, phy_np, of_phy_match);
	return d ? to_phy_device(d) : NULL;
}
EXPORT_SYMBOL(of_phy_find_device);

struct phy_device *of_phy_connect(struct net_device *dev,
				  struct device_node *phy_np,
				  void (*hndlr)(struct net_device *), u32 flags,
				  phy_interface_t iface)
{
	struct phy_device *phy = of_phy_find_device(phy_np);

	if (!phy)
		return NULL;

	return phy_connect_direct(dev, phy, hndlr, iface) ? NULL : phy;
}
EXPORT_SYMBOL(of_phy_connect);

#if defined(MY_ABC_HERE)
 
#else  
 
#endif  
struct phy_device *of_phy_connect_fixed_link(struct net_device *dev,
					     void (*hndlr)(struct net_device *),
					     phy_interface_t iface)
{
	struct device_node *net_np;
	char bus_id[MII_BUS_ID_SIZE + 3];
	struct phy_device *phy;
	const __be32 *phy_id;
	int sz;

	if (!dev->dev.parent)
		return NULL;

	net_np = dev->dev.parent->of_node;
	if (!net_np)
		return NULL;

	phy_id = of_get_property(net_np, "fixed-link", &sz);
	if (!phy_id || sz < sizeof(*phy_id))
		return NULL;

	sprintf(bus_id, PHY_ID_FMT, "fixed-0", be32_to_cpu(phy_id[0]));

	phy = phy_connect(dev, bus_id, hndlr, iface);
	return IS_ERR(phy) ? NULL : phy;
}
EXPORT_SYMBOL(of_phy_connect_fixed_link);

#if defined(MY_ABC_HERE) && defined(CONFIG_FIXED_PHY)
 
#define FIXED_LINK_PROPERTIES_COUNT 5
int of_phy_register_fixed_link(struct device_node *np)
{
	struct fixed_phy_status status = {};
	u32 fixed_link_props[FIXED_LINK_PROPERTIES_COUNT];
	int ret;

	ret = of_property_read_u32_array(np, "fixed-link",
					 fixed_link_props,
					 FIXED_LINK_PROPERTIES_COUNT);
	if (ret < 0)
		return ret;

	status.link       = 1;
	status.duplex     = fixed_link_props[1];
	status.speed      = fixed_link_props[2];
	status.pause      = fixed_link_props[3];
	status.asym_pause = fixed_link_props[4];

	return fixed_phy_add(PHY_POLL, fixed_link_props[0],
			     &status);
}
EXPORT_SYMBOL(of_phy_register_fixed_link);
#endif  
