#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/msi.h>
#include <linux/smp.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/slab.h>

#include "pci.h"

static int pci_msi_enable = 1;

#define msix_table_size(flags)	((flags & PCI_MSIX_FLAGS_QSIZE) + 1)

#if defined (MY_DEF_HERE)
#if defined(CONFIG_GENERIC_HARDIRQS)
int __weak arch_setup_msi_irq(struct pci_dev *dev, struct msi_desc *desc)
{
	struct msi_chip *chip = dev->bus->msi;
	int err;

	if (!chip || !chip->setup_irq)
		return -EINVAL;

	err = chip->setup_irq(chip, dev, desc);
	if (err < 0)
		return err;

	irq_set_chip_data(desc->irq, chip);

	return 0;
}

void __weak arch_teardown_msi_irq(unsigned int irq)
{
	struct msi_chip *chip = irq_get_chip_data(irq);

	if (!chip || !chip->teardown_irq)
		return;

	chip->teardown_irq(chip, irq);
}

int __weak arch_msi_check_device(struct pci_dev *dev, int nvec, int type)
{
	struct msi_chip *chip = dev->bus->msi;

	if (!chip || !chip->check_device)
		return 0;

	return chip->check_device(chip, dev, nvec, type);
}
#else
int __weak arch_setup_msi_irq(struct pci_dev *dev, struct msi_desc *desc)
{
	return -ENOSYS;
}

void __weak arch_teardown_msi_irq(unsigned int irq)
{
}

int __weak arch_msi_check_device(struct pci_dev *dev, int nvec, int type)
{
	return 0;
}
#endif  
#elif defined(MY_ABC_HERE)
int __weak arch_setup_msi_irq(struct pci_dev *dev, struct msi_desc *desc)
{
	struct msi_chip *chip = dev->bus->msi;
	int err;

	if (!chip || !chip->setup_irq)
		return -EINVAL;

	err = chip->setup_irq(chip, dev, desc);
	if (err < 0)
		return err;

	irq_set_chip_data(desc->irq, chip);

   return 0;
}
#else  
#ifndef arch_msi_check_device
int arch_msi_check_device(struct pci_dev *dev, int nvec, int type)
{
	return 0;
}
#endif

#ifndef arch_setup_msi_irqs
# define arch_setup_msi_irqs default_setup_msi_irqs
# define HAVE_DEFAULT_MSI_SETUP_IRQS
#endif
#endif  

#if defined(MY_ABC_HERE)
void __weak arch_teardown_msi_irq(unsigned int irq)
{
	struct msi_chip *chip = irq_get_chip_data(irq);
	if (!chip || !chip->teardown_irq)
		return;

	chip->teardown_irq(chip, irq);
}

int __weak arch_msi_check_device(struct pci_dev *dev, int nvec, int type)
{
	struct msi_chip *chip = dev->bus->msi;

	if (!chip || !chip->check_device)
		return 0;

	return chip->check_device(chip, dev, nvec, type);
}
#endif  

#if defined (MY_DEF_HERE) || defined(MY_ABC_HERE)
int __weak arch_setup_msi_irqs(struct pci_dev *dev, int nvec, int type)
{
	struct msi_desc *entry;
	int ret;

	if (type == PCI_CAP_ID_MSI && nvec > 1)
		return 1;

	list_for_each_entry(entry, &dev->msi_list, list) {
		ret = arch_setup_msi_irq(dev, entry);
		if (ret < 0)
			return ret;
		if (ret > 0)
			return -ENOSPC;
	}

	return 0;
}
#else  
#ifdef HAVE_DEFAULT_MSI_SETUP_IRQS
int default_setup_msi_irqs(struct pci_dev *dev, int nvec, int type)
{
	struct msi_desc *entry;
	int ret;

	if (type == PCI_CAP_ID_MSI && nvec > 1)
		return 1;

	list_for_each_entry(entry, &dev->msi_list, list) {
		ret = arch_setup_msi_irq(dev, entry);
		if (ret < 0)
			return ret;
		if (ret > 0)
			return -ENOSPC;
	}

	return 0;
}
#endif
#endif  

#if defined (MY_DEF_HERE) || defined(MY_ABC_HERE)
 
#else  
#ifndef arch_teardown_msi_irqs
# define arch_teardown_msi_irqs default_teardown_msi_irqs
# define HAVE_DEFAULT_MSI_TEARDOWN_IRQS
#endif

#endif  

#if defined (HAVE_DEFAULT_MSI_TEARDOWN_IRQS) || defined(MY_DEF_HERE) || defined(MY_ABC_HERE)
void default_teardown_msi_irqs(struct pci_dev *dev)
{
	struct msi_desc *entry;

	list_for_each_entry(entry, &dev->msi_list, list) {
		int i, nvec;
		if (entry->irq == 0)
			continue;
		nvec = 1 << entry->msi_attrib.multiple;
		for (i = 0; i < nvec; i++)
			arch_teardown_msi_irq(entry->irq + i);
	}
}
#endif

#if defined (MY_DEF_HERE) || defined(MY_ABC_HERE)
void __weak arch_teardown_msi_irqs(struct pci_dev *dev)
{
	return default_teardown_msi_irqs(dev);
}
#else  
#ifndef arch_restore_msi_irqs
# define arch_restore_msi_irqs default_restore_msi_irqs
# define HAVE_DEFAULT_MSI_RESTORE_IRQS
#endif
#endif  

#if defined(HAVE_DEFAULT_MSI_RESTORE_IRQS) || defined(MY_DEF_HERE) || defined(MY_ABC_HERE)
void default_restore_msi_irqs(struct pci_dev *dev, int irq)
{
	struct msi_desc *entry;

	entry = NULL;
	if (dev->msix_enabled) {
		list_for_each_entry(entry, &dev->msi_list, list) {
			if (irq == entry->irq)
				break;
		}
	} else if (dev->msi_enabled)  {
		entry = irq_get_msi_desc(irq);
	}

	if (entry)
		write_msi_msg(irq, &entry->msg);
}
#endif
#if defined (MY_DEF_HERE) || defined(MY_ABC_HERE)

void __weak arch_restore_msi_irqs(struct pci_dev *dev, int irq)
{
	return default_restore_msi_irqs(dev, irq);
}
#endif  

static void msi_set_enable(struct pci_dev *dev, int enable)
{
	u16 control;

	pci_read_config_word(dev, dev->msi_cap + PCI_MSI_FLAGS, &control);
	control &= ~PCI_MSI_FLAGS_ENABLE;
	if (enable)
		control |= PCI_MSI_FLAGS_ENABLE;
	pci_write_config_word(dev, dev->msi_cap + PCI_MSI_FLAGS, control);
}

static void msix_set_enable(struct pci_dev *dev, int enable)
{
	u16 control;

	pci_read_config_word(dev, dev->msix_cap + PCI_MSIX_FLAGS, &control);
	control &= ~PCI_MSIX_FLAGS_ENABLE;
	if (enable)
		control |= PCI_MSIX_FLAGS_ENABLE;
	pci_write_config_word(dev, dev->msix_cap + PCI_MSIX_FLAGS, control);
}

static inline __attribute_const__ u32 msi_mask(unsigned x)
{
	 
	if (x >= 5)
		return 0xffffffff;
	return (1 << (1 << x)) - 1;
}

static inline __attribute_const__ u32 msi_capable_mask(u16 control)
{
	return msi_mask((control >> 1) & 7);
}

static inline __attribute_const__ u32 msi_enabled_mask(u16 control)
{
	return msi_mask((control >> 4) & 7);
}

static u32 __msi_mask_irq(struct msi_desc *desc, u32 mask, u32 flag)
{
	u32 mask_bits = desc->masked;

	if (!desc->msi_attrib.maskbit)
		return 0;

	mask_bits &= ~mask;
	mask_bits |= flag;
	pci_write_config_dword(desc->dev, desc->mask_pos, mask_bits);

	return mask_bits;
}

static void msi_mask_irq(struct msi_desc *desc, u32 mask, u32 flag)
{
	desc->masked = __msi_mask_irq(desc, mask, flag);
}

static u32 __msix_mask_irq(struct msi_desc *desc, u32 flag)
{
	u32 mask_bits = desc->masked;
	unsigned offset = desc->msi_attrib.entry_nr * PCI_MSIX_ENTRY_SIZE +
						PCI_MSIX_ENTRY_VECTOR_CTRL;
	mask_bits &= ~PCI_MSIX_ENTRY_CTRL_MASKBIT;
	if (flag)
		mask_bits |= PCI_MSIX_ENTRY_CTRL_MASKBIT;
	writel(mask_bits, desc->mask_base + offset);

	return mask_bits;
}

static void msix_mask_irq(struct msi_desc *desc, u32 flag)
{
	desc->masked = __msix_mask_irq(desc, flag);
}

#ifdef CONFIG_GENERIC_HARDIRQS

static void msi_set_mask_bit(struct irq_data *data, u32 flag)
{
	struct msi_desc *desc = irq_data_get_msi(data);

	if (desc->msi_attrib.is_msix) {
		msix_mask_irq(desc, flag);
		readl(desc->mask_base);		 
	} else {
		unsigned offset = data->irq - desc->dev->irq;
		msi_mask_irq(desc, 1 << offset, flag << offset);
	}
}

void mask_msi_irq(struct irq_data *data)
{
	msi_set_mask_bit(data, 1);
}

void unmask_msi_irq(struct irq_data *data)
{
	msi_set_mask_bit(data, 0);
}

#endif  

void __read_msi_msg(struct msi_desc *entry, struct msi_msg *msg)
{
	BUG_ON(entry->dev->current_state != PCI_D0);

	if (entry->msi_attrib.is_msix) {
		void __iomem *base = entry->mask_base +
			entry->msi_attrib.entry_nr * PCI_MSIX_ENTRY_SIZE;

		msg->address_lo = readl(base + PCI_MSIX_ENTRY_LOWER_ADDR);
		msg->address_hi = readl(base + PCI_MSIX_ENTRY_UPPER_ADDR);
		msg->data = readl(base + PCI_MSIX_ENTRY_DATA);
	} else {
		struct pci_dev *dev = entry->dev;
		int pos = dev->msi_cap;
		u16 data;

		pci_read_config_dword(dev, pos + PCI_MSI_ADDRESS_LO,
				      &msg->address_lo);
		if (entry->msi_attrib.is_64) {
			pci_read_config_dword(dev, pos + PCI_MSI_ADDRESS_HI,
					      &msg->address_hi);
			pci_read_config_word(dev, pos + PCI_MSI_DATA_64, &data);
		} else {
			msg->address_hi = 0;
			pci_read_config_word(dev, pos + PCI_MSI_DATA_32, &data);
		}
		msg->data = data;
	}
}

void read_msi_msg(unsigned int irq, struct msi_msg *msg)
{
	struct msi_desc *entry = irq_get_msi_desc(irq);

	__read_msi_msg(entry, msg);
}

void __get_cached_msi_msg(struct msi_desc *entry, struct msi_msg *msg)
{
	 
	BUG_ON(!(entry->msg.address_hi | entry->msg.address_lo |
		 entry->msg.data));

	*msg = entry->msg;
}

void get_cached_msi_msg(unsigned int irq, struct msi_msg *msg)
{
	struct msi_desc *entry = irq_get_msi_desc(irq);

	__get_cached_msi_msg(entry, msg);
}

void __write_msi_msg(struct msi_desc *entry, struct msi_msg *msg)
{
	if (entry->dev->current_state != PCI_D0) {
		 
	} else if (entry->msi_attrib.is_msix) {
		void __iomem *base;
		base = entry->mask_base +
			entry->msi_attrib.entry_nr * PCI_MSIX_ENTRY_SIZE;

		writel(msg->address_lo, base + PCI_MSIX_ENTRY_LOWER_ADDR);
		writel(msg->address_hi, base + PCI_MSIX_ENTRY_UPPER_ADDR);
		writel(msg->data, base + PCI_MSIX_ENTRY_DATA);
	} else {
		struct pci_dev *dev = entry->dev;
		int pos = dev->msi_cap;
		u16 msgctl;

		pci_read_config_word(dev, pos + PCI_MSI_FLAGS, &msgctl);
		msgctl &= ~PCI_MSI_FLAGS_QSIZE;
		msgctl |= entry->msi_attrib.multiple << 4;
		pci_write_config_word(dev, pos + PCI_MSI_FLAGS, msgctl);

		pci_write_config_dword(dev, pos + PCI_MSI_ADDRESS_LO,
				       msg->address_lo);
		if (entry->msi_attrib.is_64) {
			pci_write_config_dword(dev, pos + PCI_MSI_ADDRESS_HI,
					       msg->address_hi);
			pci_write_config_word(dev, pos + PCI_MSI_DATA_64,
					      msg->data);
		} else {
			pci_write_config_word(dev, pos + PCI_MSI_DATA_32,
					      msg->data);
		}
	}
	entry->msg = *msg;
}

void write_msi_msg(unsigned int irq, struct msi_msg *msg)
{
	struct msi_desc *entry = irq_get_msi_desc(irq);

	__write_msi_msg(entry, msg);
}

static void free_msi_irqs(struct pci_dev *dev)
{
	struct msi_desc *entry, *tmp;

	list_for_each_entry(entry, &dev->msi_list, list) {
		int i, nvec;
		if (!entry->irq)
			continue;
		nvec = 1 << entry->msi_attrib.multiple;
#ifdef CONFIG_GENERIC_HARDIRQS
		for (i = 0; i < nvec; i++)
			BUG_ON(irq_has_action(entry->irq + i));
#endif
	}

	arch_teardown_msi_irqs(dev);

	list_for_each_entry_safe(entry, tmp, &dev->msi_list, list) {
		if (entry->msi_attrib.is_msix) {
			if (list_is_last(&entry->list, &dev->msi_list))
				iounmap(entry->mask_base);
		}

		if (entry->kobj.parent) {
			kobject_del(&entry->kobj);
			kobject_put(&entry->kobj);
		}

		list_del(&entry->list);
		kfree(entry);
	}
}

static struct msi_desc *alloc_msi_entry(struct pci_dev *dev)
{
	struct msi_desc *desc = kzalloc(sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return NULL;

	INIT_LIST_HEAD(&desc->list);
	desc->dev = dev;

	return desc;
}

static void pci_intx_for_msi(struct pci_dev *dev, int enable)
{
	if (!(dev->dev_flags & PCI_DEV_FLAGS_MSI_INTX_DISABLE_BUG))
		pci_intx(dev, enable);
}

static void __pci_restore_msi_state(struct pci_dev *dev)
{
	u16 control;
	struct msi_desc *entry;

	if (!dev->msi_enabled)
		return;

	entry = irq_get_msi_desc(dev->irq);

	pci_intx_for_msi(dev, 0);
	msi_set_enable(dev, 0);
	arch_restore_msi_irqs(dev, dev->irq);

	pci_read_config_word(dev, dev->msi_cap + PCI_MSI_FLAGS, &control);
	msi_mask_irq(entry, msi_capable_mask(control), entry->masked);
	control &= ~PCI_MSI_FLAGS_QSIZE;
	control |= (entry->msi_attrib.multiple << 4) | PCI_MSI_FLAGS_ENABLE;
	pci_write_config_word(dev, dev->msi_cap + PCI_MSI_FLAGS, control);
}

static void __pci_restore_msix_state(struct pci_dev *dev)
{
	struct msi_desc *entry;
	u16 control;

	if (!dev->msix_enabled)
		return;
	BUG_ON(list_empty(&dev->msi_list));
	entry = list_first_entry(&dev->msi_list, struct msi_desc, list);
	pci_read_config_word(dev, dev->msix_cap + PCI_MSIX_FLAGS, &control);

	pci_intx_for_msi(dev, 0);
	control |= PCI_MSIX_FLAGS_ENABLE | PCI_MSIX_FLAGS_MASKALL;
	pci_write_config_word(dev, dev->msix_cap + PCI_MSIX_FLAGS, control);

	list_for_each_entry(entry, &dev->msi_list, list) {
		arch_restore_msi_irqs(dev, entry->irq);
		msix_mask_irq(entry, entry->masked);
	}

	control &= ~PCI_MSIX_FLAGS_MASKALL;
	pci_write_config_word(dev, dev->msix_cap + PCI_MSIX_FLAGS, control);
}

void pci_restore_msi_state(struct pci_dev *dev)
{
	__pci_restore_msi_state(dev);
	__pci_restore_msix_state(dev);
}
EXPORT_SYMBOL_GPL(pci_restore_msi_state);

#define to_msi_attr(obj) container_of(obj, struct msi_attribute, attr)
#define to_msi_desc(obj) container_of(obj, struct msi_desc, kobj)

struct msi_attribute {
	struct attribute        attr;
	ssize_t (*show)(struct msi_desc *entry, struct msi_attribute *attr,
			char *buf);
	ssize_t (*store)(struct msi_desc *entry, struct msi_attribute *attr,
			 const char *buf, size_t count);
};

static ssize_t show_msi_mode(struct msi_desc *entry, struct msi_attribute *atr,
			     char *buf)
{
	return sprintf(buf, "%s\n", entry->msi_attrib.is_msix ? "msix" : "msi");
}

static ssize_t msi_irq_attr_show(struct kobject *kobj,
				 struct attribute *attr, char *buf)
{
	struct msi_attribute *attribute = to_msi_attr(attr);
	struct msi_desc *entry = to_msi_desc(kobj);

	if (!attribute->show)
		return -EIO;

	return attribute->show(entry, attribute, buf);
}

static const struct sysfs_ops msi_irq_sysfs_ops = {
	.show = msi_irq_attr_show,
};

static struct msi_attribute mode_attribute =
	__ATTR(mode, S_IRUGO, show_msi_mode, NULL);

static struct attribute *msi_irq_default_attrs[] = {
	&mode_attribute.attr,
	NULL
};

static void msi_kobj_release(struct kobject *kobj)
{
	struct msi_desc *entry = to_msi_desc(kobj);

	pci_dev_put(entry->dev);
}

static struct kobj_type msi_irq_ktype = {
	.release = msi_kobj_release,
	.sysfs_ops = &msi_irq_sysfs_ops,
	.default_attrs = msi_irq_default_attrs,
};

static int populate_msi_sysfs(struct pci_dev *pdev)
{
	struct msi_desc *entry;
	struct kobject *kobj;
	int ret;
	int count = 0;

	pdev->msi_kset = kset_create_and_add("msi_irqs", NULL, &pdev->dev.kobj);
	if (!pdev->msi_kset)
		return -ENOMEM;

	list_for_each_entry(entry, &pdev->msi_list, list) {
		kobj = &entry->kobj;
		kobj->kset = pdev->msi_kset;
		pci_dev_get(pdev);
		ret = kobject_init_and_add(kobj, &msi_irq_ktype, NULL,
				     "%u", entry->irq);
		if (ret)
			goto out_unroll;

		count++;
	}

	return 0;

out_unroll:
	list_for_each_entry(entry, &pdev->msi_list, list) {
		if (!count)
			break;
		kobject_del(&entry->kobj);
		kobject_put(&entry->kobj);
		count--;
	}
	return ret;
}

static int msi_verify_entries(struct pci_dev *dev)
{
	struct msi_desc *entry;

	list_for_each_entry(entry, &dev->msi_list, list) {
		if (!dev->no_64bit_msi || !entry->msg.address_hi)
			continue;
		dev_err(&dev->dev, "Device has broken 64-bit MSI but arch"
			" tried to assign one above 4G\n");
		return -EIO;
	}
	return 0;
}

static int msi_capability_init(struct pci_dev *dev, int nvec)
{
	struct msi_desc *entry;
	int ret;
	u16 control;
	unsigned mask;

	msi_set_enable(dev, 0);	 

	pci_read_config_word(dev, dev->msi_cap + PCI_MSI_FLAGS, &control);
	 
	entry = alloc_msi_entry(dev);
	if (!entry)
		return -ENOMEM;

	entry->msi_attrib.is_msix	= 0;
	entry->msi_attrib.is_64		= !!(control & PCI_MSI_FLAGS_64BIT);
	entry->msi_attrib.entry_nr	= 0;
	entry->msi_attrib.maskbit	= !!(control & PCI_MSI_FLAGS_MASKBIT);
	entry->msi_attrib.default_irq	= dev->irq;	 
	entry->msi_attrib.pos		= dev->msi_cap;

	if (control & PCI_MSI_FLAGS_64BIT)
		entry->mask_pos = dev->msi_cap + PCI_MSI_MASK_64;
	else
		entry->mask_pos = dev->msi_cap + PCI_MSI_MASK_32;
	 
	if (entry->msi_attrib.maskbit)
		pci_read_config_dword(dev, entry->mask_pos, &entry->masked);
	mask = msi_capable_mask(control);
	msi_mask_irq(entry, mask, mask);

	list_add_tail(&entry->list, &dev->msi_list);

	ret = arch_setup_msi_irqs(dev, nvec, PCI_CAP_ID_MSI);
	if (ret) {
		msi_mask_irq(entry, mask, ~mask);
		free_msi_irqs(dev);
		return ret;
	}

	ret = msi_verify_entries(dev);
	if (ret) {
		msi_mask_irq(entry, mask, ~mask);
		free_msi_irqs(dev);
		return ret;
	}

	ret = populate_msi_sysfs(dev);
	if (ret) {
		msi_mask_irq(entry, mask, ~mask);
		free_msi_irqs(dev);
		return ret;
	}

	pci_intx_for_msi(dev, 0);
	msi_set_enable(dev, 1);
	dev->msi_enabled = 1;

	dev->irq = entry->irq;
	return 0;
}

static void __iomem *msix_map_region(struct pci_dev *dev, unsigned nr_entries)
{
	resource_size_t phys_addr;
	u32 table_offset;
	u8 bir;

	pci_read_config_dword(dev, dev->msix_cap + PCI_MSIX_TABLE,
			      &table_offset);
	bir = (u8)(table_offset & PCI_MSIX_TABLE_BIR);
	table_offset &= PCI_MSIX_TABLE_OFFSET;
	phys_addr = pci_resource_start(dev, bir) + table_offset;

	return ioremap_nocache(phys_addr, nr_entries * PCI_MSIX_ENTRY_SIZE);
}

static int msix_setup_entries(struct pci_dev *dev, void __iomem *base,
			      struct msix_entry *entries, int nvec)
{
	struct msi_desc *entry;
	int i;

	for (i = 0; i < nvec; i++) {
		entry = alloc_msi_entry(dev);
		if (!entry) {
			if (!i)
				iounmap(base);
			else
				free_msi_irqs(dev);
			 
			return -ENOMEM;
		}

		entry->msi_attrib.is_msix	= 1;
		entry->msi_attrib.is_64		= 1;
		entry->msi_attrib.entry_nr	= entries[i].entry;
		entry->msi_attrib.default_irq	= dev->irq;
		entry->msi_attrib.pos		= dev->msix_cap;
		entry->mask_base		= base;

		list_add_tail(&entry->list, &dev->msi_list);
	}

	return 0;
}

static void msix_program_entries(struct pci_dev *dev,
				 struct msix_entry *entries)
{
	struct msi_desc *entry;
	int i = 0;

	list_for_each_entry(entry, &dev->msi_list, list) {
		int offset = entries[i].entry * PCI_MSIX_ENTRY_SIZE +
						PCI_MSIX_ENTRY_VECTOR_CTRL;

		entries[i].vector = entry->irq;
		irq_set_msi_desc(entry->irq, entry);
		entry->masked = readl(entry->mask_base + offset);
		msix_mask_irq(entry, 1);
		i++;
	}
}

static int msix_capability_init(struct pci_dev *dev,
				struct msix_entry *entries, int nvec)
{
	int ret;
	u16 control;
	void __iomem *base;

	pci_read_config_word(dev, dev->msix_cap + PCI_MSIX_FLAGS, &control);

	control &= ~PCI_MSIX_FLAGS_ENABLE;
	pci_write_config_word(dev, dev->msix_cap + PCI_MSIX_FLAGS, control);

	base = msix_map_region(dev, msix_table_size(control));
	if (!base)
		return -ENOMEM;

	ret = msix_setup_entries(dev, base, entries, nvec);
	if (ret)
		return ret;

	ret = arch_setup_msi_irqs(dev, nvec, PCI_CAP_ID_MSIX);
	if (ret)
		goto error;

	ret = msi_verify_entries(dev);
	if (ret)
		goto error;

	control |= PCI_MSIX_FLAGS_MASKALL | PCI_MSIX_FLAGS_ENABLE;
	pci_write_config_word(dev, dev->msix_cap + PCI_MSIX_FLAGS, control);

	msix_program_entries(dev, entries);

	ret = populate_msi_sysfs(dev);
	if (ret) {
		ret = 0;
		goto error;
	}

	pci_intx_for_msi(dev, 0);
	dev->msix_enabled = 1;

	control &= ~PCI_MSIX_FLAGS_MASKALL;
	pci_write_config_word(dev, dev->msix_cap + PCI_MSIX_FLAGS, control);

	return 0;

error:
	if (ret < 0) {
		 
		struct msi_desc *entry;
		int avail = 0;

		list_for_each_entry(entry, &dev->msi_list, list) {
			if (entry->irq != 0)
				avail++;
		}
		if (avail != 0)
			ret = avail;
	}

	free_msi_irqs(dev);

	return ret;
}

static int pci_msi_check_device(struct pci_dev *dev, int nvec, int type)
{
	struct pci_bus *bus;
	int ret;

	if (!pci_msi_enable || !dev || dev->no_msi)
		return -EINVAL;

	if (nvec < 1)
		return -ERANGE;

	for (bus = dev->bus; bus; bus = bus->parent)
		if (bus->bus_flags & PCI_BUS_FLAGS_NO_MSI)
			return -EINVAL;

	ret = arch_msi_check_device(dev, nvec, type);
	if (ret)
		return ret;

	return 0;
}

int pci_enable_msi_block(struct pci_dev *dev, unsigned int nvec)
{
	int status, maxvec;
	u16 msgctl;

	if (!dev->msi_cap)
		return -EINVAL;

	pci_read_config_word(dev, dev->msi_cap + PCI_MSI_FLAGS, &msgctl);
	maxvec = 1 << ((msgctl & PCI_MSI_FLAGS_QMASK) >> 1);
	if (nvec > maxvec)
		return maxvec;

	status = pci_msi_check_device(dev, nvec, PCI_CAP_ID_MSI);
	if (status)
		return status;

	WARN_ON(!!dev->msi_enabled);

	if (dev->msix_enabled) {
		dev_info(&dev->dev, "can't enable MSI "
			 "(MSI-X already enabled)\n");
		return -EINVAL;
	}

	status = msi_capability_init(dev, nvec);
	return status;
}
EXPORT_SYMBOL(pci_enable_msi_block);

int pci_enable_msi_block_auto(struct pci_dev *dev, unsigned int *maxvec)
{
	int ret, nvec;
	u16 msgctl;

	if (!dev->msi_cap)
		return -EINVAL;

	pci_read_config_word(dev, dev->msi_cap + PCI_MSI_FLAGS, &msgctl);
	ret = 1 << ((msgctl & PCI_MSI_FLAGS_QMASK) >> 1);

	if (maxvec)
		*maxvec = ret;

	do {
		nvec = ret;
		ret = pci_enable_msi_block(dev, nvec);
	} while (ret > 0);

	if (ret < 0)
		return ret;
	return nvec;
}
EXPORT_SYMBOL(pci_enable_msi_block_auto);

void pci_msi_shutdown(struct pci_dev *dev)
{
	struct msi_desc *desc;
	u32 mask;
	u16 ctrl;

	if (!pci_msi_enable || !dev || !dev->msi_enabled)
		return;

	BUG_ON(list_empty(&dev->msi_list));
	desc = list_first_entry(&dev->msi_list, struct msi_desc, list);

	msi_set_enable(dev, 0);
	pci_intx_for_msi(dev, 1);
	dev->msi_enabled = 0;

	pci_read_config_word(dev, dev->msi_cap + PCI_MSI_FLAGS, &ctrl);
	mask = msi_capable_mask(ctrl);
	 
	__msi_mask_irq(desc, mask, ~mask);

	dev->irq = desc->msi_attrib.default_irq;
}

void pci_disable_msi(struct pci_dev *dev)
{
	if (!pci_msi_enable || !dev || !dev->msi_enabled)
		return;

	pci_msi_shutdown(dev);
	free_msi_irqs(dev);
	kset_unregister(dev->msi_kset);
	dev->msi_kset = NULL;
}
EXPORT_SYMBOL(pci_disable_msi);

int pci_msix_table_size(struct pci_dev *dev)
{
	u16 control;

	if (!dev->msix_cap)
		return 0;

	pci_read_config_word(dev, dev->msix_cap + PCI_MSIX_FLAGS, &control);
	return msix_table_size(control);
}

int pci_enable_msix(struct pci_dev *dev, struct msix_entry *entries, int nvec)
{
	int status, nr_entries;
	int i, j;

	if (!entries || !dev->msix_cap)
		return -EINVAL;

	status = pci_msi_check_device(dev, nvec, PCI_CAP_ID_MSIX);
	if (status)
		return status;

	nr_entries = pci_msix_table_size(dev);
	if (nvec > nr_entries)
		return nr_entries;

	for (i = 0; i < nvec; i++) {
		if (entries[i].entry >= nr_entries)
			return -EINVAL;		 
		for (j = i + 1; j < nvec; j++) {
			if (entries[i].entry == entries[j].entry)
				return -EINVAL;	 
		}
	}
	WARN_ON(!!dev->msix_enabled);

	if (dev->msi_enabled) {
		dev_info(&dev->dev, "can't enable MSI-X "
		       "(MSI IRQ already assigned)\n");
		return -EINVAL;
	}
	status = msix_capability_init(dev, entries, nvec);
	return status;
}
EXPORT_SYMBOL(pci_enable_msix);

void pci_msix_shutdown(struct pci_dev *dev)
{
	struct msi_desc *entry;

	if (!pci_msi_enable || !dev || !dev->msix_enabled)
		return;

	list_for_each_entry(entry, &dev->msi_list, list) {
		 
		__msix_mask_irq(entry, 1);
	}

	msix_set_enable(dev, 0);
	pci_intx_for_msi(dev, 1);
	dev->msix_enabled = 0;
}

void pci_disable_msix(struct pci_dev *dev)
{
	if (!pci_msi_enable || !dev || !dev->msix_enabled)
		return;

	pci_msix_shutdown(dev);
	free_msi_irqs(dev);
	kset_unregister(dev->msi_kset);
	dev->msi_kset = NULL;
}
EXPORT_SYMBOL(pci_disable_msix);

void msi_remove_pci_irq_vectors(struct pci_dev *dev)
{
	if (!pci_msi_enable || !dev)
		return;

	if (dev->msi_enabled || dev->msix_enabled)
		free_msi_irqs(dev);
}

void pci_no_msi(void)
{
	pci_msi_enable = 0;
}

int pci_msi_enabled(void)
{
	return pci_msi_enable;
}
EXPORT_SYMBOL(pci_msi_enabled);

void pci_msi_init_pci_dev(struct pci_dev *dev)
{
	INIT_LIST_HEAD(&dev->msi_list);

	dev->msi_cap = pci_find_capability(dev, PCI_CAP_ID_MSI);
	if (dev->msi_cap)
		msi_set_enable(dev, 0);

	dev->msix_cap = pci_find_capability(dev, PCI_CAP_ID_MSIX);
	if (dev->msix_cap)
		msix_set_enable(dev, 0);
}
