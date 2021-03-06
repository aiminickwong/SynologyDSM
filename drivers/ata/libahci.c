#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#if defined(MY_DEF_HERE)
#include <linux/gpio.h>
#endif  
#ifdef MY_DEF_HERE
#include <scsi/scsi_device.h>
#endif  
#include <linux/libata.h>
#ifdef MY_DEF_HERE
#include <linux/pci.h>
#include <linux/leds.h>
#if defined(MY_DEF_HERE)
#include <linux/pci_ids.h>
#endif  
#endif  
#include "ahci.h"
#include "libata.h"
#ifdef MY_DEF_HERE
extern void syno_ledtrig_active_set(int iLedNum);
extern int *gpGreenLedMap;
#if defined(MY_DEF_HERE)
extern int SYNO_CTRL_HDD_ACT_NOTIFY(int index);
#endif  
#endif  
#ifdef MY_ABC_HERE
#include <linux/synolib.h>
#endif  

#if defined(CONFIG_SYNO_LSP_HI3536)
#ifdef CONFIG_ARCH_HI3536
#include "hi_ahci_sys_hi3536_defconfig.h"
#endif

#ifdef CONFIG_ARCH_HI3521A
#include "hi_ahci_sys_hi3521a_defconfig.h"
#endif

#ifdef CONFIG_ARCH_HI3531A
#include "hi_ahci_sys_hi3531a_defconfig.h"
#endif
#endif  

static int ahci_skip_host_reset;
int ahci_ignore_sss;
EXPORT_SYMBOL_GPL(ahci_ignore_sss);
#if defined(CONFIG_SYNO_LSP_HI3536)
static int fbs_en = CONFIG_HI_SATA_FBS;
#endif  

module_param_named(skip_host_reset, ahci_skip_host_reset, int, 0444);
MODULE_PARM_DESC(skip_host_reset, "skip global host reset (0=don't skip, 1=skip)");

module_param_named(ignore_sss, ahci_ignore_sss, int, 0444);
MODULE_PARM_DESC(ignore_sss, "Ignore staggered spinup flag (0=don't ignore, 1=ignore)");

#if defined(CONFIG_SYNO_LSP_HI3536)
module_param(fbs_en, uint, 0600);
MODULE_PARM_DESC(fbs_en, "ahci fbs flags (default:1)");

#if (defined(CONFIG_ARCH_HI3536) \
	|| defined(CONFIG_ARCH_HI3521A) \
	|| defined(CONFIG_ARCH_HI3531A)) \
	&& !defined(CONFIG_SATA_AHCI)
#define AHCI_TIMEOUT_COUNT	10
#define AHCI_POLL_TIMER		(20 * HZ)

struct ata_fbs_ctrl {
	unsigned int fbs_enable_ctrl;  
	unsigned int fbs_mode_ctrl;    
	unsigned int fbs_enable_flag;
	unsigned int fbs_disable_flag;
	unsigned int fbs_cmd_issue_flag;
	struct timer_list poll_timer;
};
static struct ata_fbs_ctrl fbs_ctrl[4];
#endif
#endif  
static int ahci_set_lpm(struct ata_link *link, enum ata_lpm_policy policy,
			unsigned hints);
static ssize_t ahci_led_show(struct ata_port *ap, char *buf);
static ssize_t ahci_led_store(struct ata_port *ap, const char *buf,
			      size_t size);
static ssize_t ahci_transmit_led_message(struct ata_port *ap, u32 state,
					ssize_t size);

static int ahci_scr_read(struct ata_link *link, unsigned int sc_reg, u32 *val);
static int ahci_scr_write(struct ata_link *link, unsigned int sc_reg, u32 val);
static unsigned int ahci_qc_issue(struct ata_queued_cmd *qc);
static bool ahci_qc_fill_rtf(struct ata_queued_cmd *qc);
static int ahci_port_start(struct ata_port *ap);
static void ahci_port_stop(struct ata_port *ap);
static void ahci_qc_prep(struct ata_queued_cmd *qc);
static int ahci_pmp_qc_defer(struct ata_queued_cmd *qc);
static void ahci_freeze(struct ata_port *ap);
static void ahci_thaw(struct ata_port *ap);
static void ahci_set_aggressive_devslp(struct ata_port *ap, bool sleep);
static void ahci_enable_fbs(struct ata_port *ap);
static void ahci_disable_fbs(struct ata_port *ap);
static void ahci_pmp_attach(struct ata_port *ap);
static void ahci_pmp_detach(struct ata_port *ap);
static int ahci_softreset(struct ata_link *link, unsigned int *class,
			  unsigned long deadline);
static int ahci_pmp_retry_softreset(struct ata_link *link, unsigned int *class,
			  unsigned long deadline);
static int ahci_hardreset(struct ata_link *link, unsigned int *class,
			  unsigned long deadline);
static void ahci_postreset(struct ata_link *link, unsigned int *class);
static void ahci_error_handler(struct ata_port *ap);
static void ahci_post_internal_cmd(struct ata_queued_cmd *qc);
static void ahci_dev_config(struct ata_device *dev);
#ifdef CONFIG_PM
static int ahci_port_suspend(struct ata_port *ap, pm_message_t mesg);
#endif
static ssize_t ahci_activity_show(struct ata_device *dev, char *buf);
static ssize_t ahci_activity_store(struct ata_device *dev,
				   enum sw_activity val);
static void ahci_init_sw_activity(struct ata_link *link);

static ssize_t ahci_show_host_caps(struct device *dev,
				   struct device_attribute *attr, char *buf);
static ssize_t ahci_show_host_cap2(struct device *dev,
				   struct device_attribute *attr, char *buf);
static ssize_t ahci_show_host_version(struct device *dev,
				      struct device_attribute *attr, char *buf);
static ssize_t ahci_show_port_cmd(struct device *dev,
				  struct device_attribute *attr, char *buf);
static ssize_t ahci_read_em_buffer(struct device *dev,
				   struct device_attribute *attr, char *buf);
static ssize_t ahci_store_em_buffer(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size);
static ssize_t ahci_show_em_supported(struct device *dev,
				      struct device_attribute *attr, char *buf);

static DEVICE_ATTR(ahci_host_caps, S_IRUGO, ahci_show_host_caps, NULL);
static DEVICE_ATTR(ahci_host_cap2, S_IRUGO, ahci_show_host_cap2, NULL);
static DEVICE_ATTR(ahci_host_version, S_IRUGO, ahci_show_host_version, NULL);
static DEVICE_ATTR(ahci_port_cmd, S_IRUGO, ahci_show_port_cmd, NULL);

#ifdef MY_DEF_HERE
static ssize_t
ata_ahci_locate_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct ata_port *ap = ata_shost_to_port(sdev->host);
	struct ata_device *atadev = ata_scsi_find_dev(ap, sdev);
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_em_priv *emp = &pp->em_priv[atadev->link->pmp];

	return sprintf(buf, "%ld\n", emp->locate);
}

static void ahci_sw_locate_set(struct ata_link *link, u8 blEnable);

static ssize_t
ata_ahci_locate_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct ata_port *ap = ata_shost_to_port(sdev->host);
	struct ata_device *atadev = ata_scsi_find_dev(ap, sdev);
	int val;

	if (ap->flags & ATA_FLAG_SW_LOCATE) {
		val = simple_strtoul(buf, NULL, 0);
		switch (val) {
		case 0: 
		case 1:
			ahci_sw_locate_set(atadev->link, val);
			return count;
		default:
			return -EIO;
		}
	}
	return -EINVAL;
}
DEVICE_ATTR(sw_locate, S_IWUSR | S_IRUGO, ata_ahci_locate_show, ata_ahci_locate_store);

static ssize_t
ata_ahci_fault_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct ata_port *ap = ata_shost_to_port(sdev->host);
	struct ata_device *atadev = ata_scsi_find_dev(ap, sdev);
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_em_priv *emp = &pp->em_priv[atadev->link->pmp];

	return sprintf(buf, "%ld\n", emp->fault);
}

static void ahci_sw_fault_set(struct ata_link *link, u8 blEnable);

void sata_syno_ahci_diskled_set(int iHostNum, int iPresent, int iFault)
{
	struct ata_port *pAp = NULL;
	struct ata_device *pAtaDev = NULL;
	struct Scsi_Host *pScsiHost = NULL;
	struct ata_link *pAtaLink = NULL;

	if (NULL == (pScsiHost = scsi_host_lookup(iHostNum))) {
			goto END;
	}

	pAp = ata_shost_to_port(pScsiHost);
	if (!pAp) {
		goto RELEASESRC;
	}

	if (pAp->nr_pmp_links) {
		goto RELEASESRC;
	}

#ifdef MY_DEF_HERE
	iPresent &= giSynoHddLedEnabled;
	iFault &= giSynoHddLedEnabled;
#endif  
	 
	ata_for_each_link(pAtaLink, pAp, EDGE) {
		ata_for_each_dev(pAtaDev, pAtaLink, ALL) {
			 
			ahci_sw_locate_set(pAtaDev->link, iPresent);
			ahci_sw_fault_set(pAtaDev->link, iFault);
		}
	}

RELEASESRC:
	scsi_host_put(pScsiHost);
END:
	return;
}
EXPORT_SYMBOL(sata_syno_ahci_diskled_set);

static ssize_t
ata_ahci_fault_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct ata_port *ap = ata_shost_to_port(sdev->host);
	struct ata_device *atadev = ata_scsi_find_dev(ap, sdev);
	int val;

	if (ap->flags & ATA_FLAG_SW_FAULT) {
		val = simple_strtoul(buf, NULL, 0);
		switch (val) {
		case 0: 
		case 1:
			ahci_sw_fault_set(atadev->link, val);
			return count;
		default:
			return -EIO;
		}
	}
	return -EINVAL;
}
DEVICE_ATTR(sw_fault, S_IWUSR | S_IRUGO, ata_ahci_fault_show, ata_ahci_fault_store);
#endif  

static DEVICE_ATTR(em_buffer, S_IWUSR | S_IRUGO,
		   ahci_read_em_buffer, ahci_store_em_buffer);
static DEVICE_ATTR(em_message_supported, S_IRUGO, ahci_show_em_supported, NULL);

struct device_attribute *ahci_shost_attrs[] = {
	&dev_attr_link_power_management_policy,
	&dev_attr_em_message_type,
	&dev_attr_em_message,
	&dev_attr_ahci_host_caps,
	&dev_attr_ahci_host_cap2,
	&dev_attr_ahci_host_version,
	&dev_attr_ahci_port_cmd,
	&dev_attr_em_buffer,
	&dev_attr_em_message_supported,
#ifdef MY_ABC_HERE
	&dev_attr_syno_manutil_power_disable,
	&dev_attr_syno_pm_gpio,
	&dev_attr_syno_pm_info,
#endif  
#ifdef MY_ABC_HERE
	&dev_attr_syno_diskname_trans,
#endif  
#ifdef MY_ABC_HERE
	&dev_attr_syno_sata_disk_led_ctrl,
#endif  
	NULL
};
EXPORT_SYMBOL_GPL(ahci_shost_attrs);

struct device_attribute *ahci_sdev_attrs[] = {
	&dev_attr_sw_activity,
	&dev_attr_unload_heads,
#ifdef MY_ABC_HERE
	&dev_attr_syno_wcache,
#endif  
#ifdef MY_DEF_HERE
	&dev_attr_sw_locate,
	&dev_attr_sw_fault,
#endif  
	NULL
};
EXPORT_SYMBOL_GPL(ahci_sdev_attrs);

struct ata_port_operations ahci_ops = {
	.inherits		= &sata_pmp_port_ops,

	.qc_defer		= ahci_pmp_qc_defer,
	.qc_prep		= ahci_qc_prep,
	.qc_issue		= ahci_qc_issue,
	.qc_fill_rtf		= ahci_qc_fill_rtf,

	.freeze			= ahci_freeze,
	.thaw			= ahci_thaw,
	.softreset		= ahci_softreset,
	.hardreset		= ahci_hardreset,
	.postreset		= ahci_postreset,
	.pmp_softreset		= ahci_softreset,
	.error_handler		= ahci_error_handler,
	.post_internal_cmd	= ahci_post_internal_cmd,
	.dev_config		= ahci_dev_config,

	.scr_read		= ahci_scr_read,
	.scr_write		= ahci_scr_write,
	.pmp_attach		= ahci_pmp_attach,
	.pmp_detach		= ahci_pmp_detach,

	.set_lpm		= ahci_set_lpm,
	.em_show		= ahci_led_show,
	.em_store		= ahci_led_store,
	.sw_activity_show	= ahci_activity_show,
	.sw_activity_store	= ahci_activity_store,
#if defined(MY_DEF_HERE)
	.transmit_led_message	= ahci_transmit_led_message,
#endif  
#ifdef CONFIG_PM
	.port_suspend		= ahci_port_suspend,
	.port_resume		= ahci_port_resume,
#endif
	.port_start		= ahci_port_start,
	.port_stop		= ahci_port_stop,
};
EXPORT_SYMBOL_GPL(ahci_ops);

struct ata_port_operations ahci_pmp_retry_srst_ops = {
	.inherits		= &ahci_ops,
	.softreset		= ahci_pmp_retry_softreset,
};
EXPORT_SYMBOL_GPL(ahci_pmp_retry_srst_ops);

int ahci_em_messages = 1;
EXPORT_SYMBOL_GPL(ahci_em_messages);
module_param(ahci_em_messages, int, 0444);
 
MODULE_PARM_DESC(ahci_em_messages,
	"AHCI Enclosure Management Message control (0 = off, 1 = on)");

int devslp_idle_timeout = 1000;	 
module_param(devslp_idle_timeout, int, 0644);
MODULE_PARM_DESC(devslp_idle_timeout, "device sleep idle timeout");

static void ahci_enable_ahci(void __iomem *mmio)
{
	int i;
	u32 tmp;

	tmp = readl(mmio + HOST_CTL);
	if (tmp & HOST_AHCI_EN)
		return;

	for (i = 0; i < 5; i++) {
		tmp |= HOST_AHCI_EN;
		writel(tmp, mmio + HOST_CTL);
		tmp = readl(mmio + HOST_CTL);	 
		if (tmp & HOST_AHCI_EN)
			return;
		msleep(10);
	}

	WARN_ON(1);
}

static ssize_t ahci_show_host_caps(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ata_port *ap = ata_shost_to_port(shost);
	struct ahci_host_priv *hpriv = ap->host->private_data;

	return sprintf(buf, "%x\n", hpriv->cap);
}

static ssize_t ahci_show_host_cap2(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ata_port *ap = ata_shost_to_port(shost);
	struct ahci_host_priv *hpriv = ap->host->private_data;

	return sprintf(buf, "%x\n", hpriv->cap2);
}

static ssize_t ahci_show_host_version(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ata_port *ap = ata_shost_to_port(shost);
	struct ahci_host_priv *hpriv = ap->host->private_data;
	void __iomem *mmio = hpriv->mmio;

	return sprintf(buf, "%x\n", readl(mmio + HOST_VERSION));
}

static ssize_t ahci_show_port_cmd(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ata_port *ap = ata_shost_to_port(shost);
	void __iomem *port_mmio = ahci_port_base(ap);

	return sprintf(buf, "%x\n", readl(port_mmio + PORT_CMD));
}

static ssize_t ahci_read_em_buffer(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ata_port *ap = ata_shost_to_port(shost);
	struct ahci_host_priv *hpriv = ap->host->private_data;
	void __iomem *mmio = hpriv->mmio;
	void __iomem *em_mmio = mmio + hpriv->em_loc;
	u32 em_ctl, msg;
	unsigned long flags;
	size_t count;
	int i;

	spin_lock_irqsave(ap->lock, flags);

	em_ctl = readl(mmio + HOST_EM_CTL);
	if (!(ap->flags & ATA_FLAG_EM) || em_ctl & EM_CTL_XMT ||
	    !(hpriv->em_msg_type & EM_MSG_TYPE_SGPIO)) {
		spin_unlock_irqrestore(ap->lock, flags);
		return -EINVAL;
	}

	if (!(em_ctl & EM_CTL_MR)) {
		spin_unlock_irqrestore(ap->lock, flags);
		return -EAGAIN;
	}

	if (!(em_ctl & EM_CTL_SMB))
		em_mmio += hpriv->em_buf_sz;

	count = hpriv->em_buf_sz;

	if (count > PAGE_SIZE) {
		if (printk_ratelimit())
			ata_port_warn(ap,
				      "EM read buffer size too large: "
				      "buffer size %u, page size %lu\n",
				      hpriv->em_buf_sz, PAGE_SIZE);
		count = PAGE_SIZE;
	}

	for (i = 0; i < count; i += 4) {
		msg = readl(em_mmio + i);
		buf[i] = msg & 0xff;
		buf[i + 1] = (msg >> 8) & 0xff;
		buf[i + 2] = (msg >> 16) & 0xff;
		buf[i + 3] = (msg >> 24) & 0xff;
	}

	spin_unlock_irqrestore(ap->lock, flags);

	return i;
}

static ssize_t ahci_store_em_buffer(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ata_port *ap = ata_shost_to_port(shost);
	struct ahci_host_priv *hpriv = ap->host->private_data;
	void __iomem *mmio = hpriv->mmio;
	void __iomem *em_mmio = mmio + hpriv->em_loc;
	const unsigned char *msg_buf = buf;
	u32 em_ctl, msg;
	unsigned long flags;
	int i;

	if (!(ap->flags & ATA_FLAG_EM) ||
	    !(hpriv->em_msg_type & EM_MSG_TYPE_SGPIO) ||
	    size % 4 || size > hpriv->em_buf_sz)
		return -EINVAL;

	spin_lock_irqsave(ap->lock, flags);

	em_ctl = readl(mmio + HOST_EM_CTL);
	if (em_ctl & EM_CTL_TM) {
		spin_unlock_irqrestore(ap->lock, flags);
		return -EBUSY;
	}

	for (i = 0; i < size; i += 4) {
		msg = msg_buf[i] | msg_buf[i + 1] << 8 |
		      msg_buf[i + 2] << 16 | msg_buf[i + 3] << 24;
		writel(msg, em_mmio + i);
	}

	writel(em_ctl | EM_CTL_TM, mmio + HOST_EM_CTL);

	spin_unlock_irqrestore(ap->lock, flags);

	return size;
}

static ssize_t ahci_show_em_supported(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ata_port *ap = ata_shost_to_port(shost);
	struct ahci_host_priv *hpriv = ap->host->private_data;
	void __iomem *mmio = hpriv->mmio;
	u32 em_ctl;

	em_ctl = readl(mmio + HOST_EM_CTL);

	return sprintf(buf, "%s%s%s%s\n",
		       em_ctl & EM_CTL_LED ? "led " : "",
		       em_ctl & EM_CTL_SAFTE ? "saf-te " : "",
		       em_ctl & EM_CTL_SES ? "ses-2 " : "",
		       em_ctl & EM_CTL_SGPIO ? "sgpio " : "");
}

void ahci_save_initial_config(struct device *dev,
			      struct ahci_host_priv *hpriv,
			      unsigned int force_port_map,
			      unsigned int mask_port_map)
{
	void __iomem *mmio = hpriv->mmio;
	u32 cap, cap2, vers, port_map;
	int i;

	ahci_enable_ahci(mmio);

	hpriv->saved_cap = cap = readl(mmio + HOST_CAP);
	hpriv->saved_port_map = port_map = readl(mmio + HOST_PORTS_IMPL);

	vers = readl(mmio + HOST_VERSION);
	if ((vers >> 16) > 1 ||
	   ((vers >> 16) == 1 && (vers & 0xFFFF) >= 0x200))
		hpriv->saved_cap2 = cap2 = readl(mmio + HOST_CAP2);
	else
		hpriv->saved_cap2 = cap2 = 0;

	if ((cap & HOST_CAP_64) && (hpriv->flags & AHCI_HFLAG_32BIT_ONLY)) {
		dev_info(dev, "controller can't do 64bit DMA, forcing 32bit\n");
		cap &= ~HOST_CAP_64;
	}

	if ((cap & HOST_CAP_NCQ) && (hpriv->flags & AHCI_HFLAG_NO_NCQ)) {
		dev_info(dev, "controller can't do NCQ, turning off CAP_NCQ\n");
		cap &= ~HOST_CAP_NCQ;
	}

	if (!(cap & HOST_CAP_NCQ) && (hpriv->flags & AHCI_HFLAG_YES_NCQ)) {
		dev_info(dev, "controller can do NCQ, turning on CAP_NCQ\n");
		cap |= HOST_CAP_NCQ;
	}

	if ((cap & HOST_CAP_PMP) && (hpriv->flags & AHCI_HFLAG_NO_PMP)) {
		dev_info(dev, "controller can't do PMP, turning off CAP_PMP\n");
		cap &= ~HOST_CAP_PMP;
	}

	if ((cap & HOST_CAP_SNTF) && (hpriv->flags & AHCI_HFLAG_NO_SNTF)) {
		dev_info(dev,
			 "controller can't do SNTF, turning off CAP_SNTF\n");
		cap &= ~HOST_CAP_SNTF;
	}

	if (!(cap & HOST_CAP_FBS) && (hpriv->flags & AHCI_HFLAG_YES_FBS)) {
		dev_info(dev, "controller can do FBS, turning on CAP_FBS\n");
		cap |= HOST_CAP_FBS;
	}

	if (force_port_map && port_map != force_port_map) {
		dev_info(dev, "forcing port_map 0x%x -> 0x%x\n",
			 port_map, force_port_map);
		port_map = force_port_map;
	}

	if (mask_port_map) {
		dev_warn(dev, "masking port_map 0x%x -> 0x%x\n",
			port_map,
			port_map & mask_port_map);
		port_map &= mask_port_map;
	}

	if (port_map) {
		int map_ports = 0;

		for (i = 0; i < AHCI_MAX_PORTS; i++)
			if (port_map & (1 << i))
				map_ports++;

		if (map_ports > ahci_nr_ports(cap)) {
			dev_warn(dev,
				 "implemented port map (0x%x) contains more ports than nr_ports (%u), using nr_ports\n",
				 port_map, ahci_nr_ports(cap));
			port_map = 0;
		}
	}

#ifdef MY_ABC_HERE
	if (!port_map) {
#else  
	if (!port_map && vers < 0x10300) {
#endif  
		port_map = (1 << ahci_nr_ports(cap)) - 1;
		dev_warn(dev, "forcing PORTS_IMPL to 0x%x\n", port_map);

		hpriv->saved_port_map = port_map;
	}

	hpriv->cap = cap;
	hpriv->cap2 = cap2;
	hpriv->port_map = port_map;
}
EXPORT_SYMBOL_GPL(ahci_save_initial_config);

static void ahci_restore_initial_config(struct ata_host *host)
{
	struct ahci_host_priv *hpriv = host->private_data;
	void __iomem *mmio = hpriv->mmio;

	writel(hpriv->saved_cap, mmio + HOST_CAP);
	if (hpriv->saved_cap2)
		writel(hpriv->saved_cap2, mmio + HOST_CAP2);
	writel(hpriv->saved_port_map, mmio + HOST_PORTS_IMPL);
	(void) readl(mmio + HOST_PORTS_IMPL);	 
}

static unsigned ahci_scr_offset(struct ata_port *ap, unsigned int sc_reg)
{
	static const int offset[] = {
		[SCR_STATUS]		= PORT_SCR_STAT,
		[SCR_CONTROL]		= PORT_SCR_CTL,
		[SCR_ERROR]		= PORT_SCR_ERR,
		[SCR_ACTIVE]		= PORT_SCR_ACT,
		[SCR_NOTIFICATION]	= PORT_SCR_NTF,
	};
	struct ahci_host_priv *hpriv = ap->host->private_data;

	if (sc_reg < ARRAY_SIZE(offset) &&
	    (sc_reg != SCR_NOTIFICATION || (hpriv->cap & HOST_CAP_SNTF)))
		return offset[sc_reg];
	return 0;
}

static int ahci_scr_read(struct ata_link *link, unsigned int sc_reg, u32 *val)
{
	void __iomem *port_mmio = ahci_port_base(link->ap);
	int offset = ahci_scr_offset(link->ap, sc_reg);

	if (offset) {
		*val = readl(port_mmio + offset);
		return 0;
	}
	return -EINVAL;
}

static int ahci_scr_write(struct ata_link *link, unsigned int sc_reg, u32 val)
{
	void __iomem *port_mmio = ahci_port_base(link->ap);
	int offset = ahci_scr_offset(link->ap, sc_reg);

	if (offset) {
		writel(val, port_mmio + offset);
		return 0;
	}
	return -EINVAL;
}

void ahci_start_engine(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 tmp;

	tmp = readl(port_mmio + PORT_CMD);
	tmp |= PORT_CMD_START;
	writel(tmp, port_mmio + PORT_CMD);
	readl(port_mmio + PORT_CMD);  
}
EXPORT_SYMBOL_GPL(ahci_start_engine);

int ahci_stop_engine(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 tmp;

	tmp = readl(port_mmio + PORT_CMD);

	if ((tmp & (PORT_CMD_START | PORT_CMD_LIST_ON)) == 0)
		return 0;

	tmp &= ~PORT_CMD_START;
	writel(tmp, port_mmio + PORT_CMD);

	tmp = ata_wait_register(ap, port_mmio + PORT_CMD,
				PORT_CMD_LIST_ON, PORT_CMD_LIST_ON, 1, 500);
	if (tmp & PORT_CMD_LIST_ON)
		return -EIO;

	return 0;
}
EXPORT_SYMBOL_GPL(ahci_stop_engine);

static void ahci_start_fis_rx(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	struct ahci_host_priv *hpriv = ap->host->private_data;
	struct ahci_port_priv *pp = ap->private_data;
	u32 tmp;

	if (hpriv->cap & HOST_CAP_64)
		writel((pp->cmd_slot_dma >> 16) >> 16,
		       port_mmio + PORT_LST_ADDR_HI);
	writel(pp->cmd_slot_dma & 0xffffffff, port_mmio + PORT_LST_ADDR);

	if (hpriv->cap & HOST_CAP_64)
		writel((pp->rx_fis_dma >> 16) >> 16,
		       port_mmio + PORT_FIS_ADDR_HI);
	writel(pp->rx_fis_dma & 0xffffffff, port_mmio + PORT_FIS_ADDR);

	tmp = readl(port_mmio + PORT_CMD);
	tmp |= PORT_CMD_FIS_RX;
	writel(tmp, port_mmio + PORT_CMD);

	readl(port_mmio + PORT_CMD);
}

static int ahci_stop_fis_rx(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 tmp;

	tmp = readl(port_mmio + PORT_CMD);
	tmp &= ~PORT_CMD_FIS_RX;
	writel(tmp, port_mmio + PORT_CMD);

	tmp = ata_wait_register(ap, port_mmio + PORT_CMD, PORT_CMD_FIS_ON,
				PORT_CMD_FIS_ON, 10, 1000);
	if (tmp & PORT_CMD_FIS_ON)
		return -EBUSY;

	return 0;
}

static void ahci_power_up(struct ata_port *ap)
{
	struct ahci_host_priv *hpriv = ap->host->private_data;
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 cmd;

	cmd = readl(port_mmio + PORT_CMD) & ~PORT_CMD_ICC_MASK;

	if (hpriv->cap & HOST_CAP_SSS) {
		cmd |= PORT_CMD_SPIN_UP;
		writel(cmd, port_mmio + PORT_CMD);
	}

	writel(cmd | PORT_CMD_ICC_ACTIVE, port_mmio + PORT_CMD);
}

static int ahci_set_lpm(struct ata_link *link, enum ata_lpm_policy policy,
			unsigned int hints)
{
	struct ata_port *ap = link->ap;
	struct ahci_host_priv *hpriv = ap->host->private_data;
	struct ahci_port_priv *pp = ap->private_data;
	void __iomem *port_mmio = ahci_port_base(ap);

	if (policy != ATA_LPM_MAX_POWER) {
		 
		pp->intr_mask &= ~PORT_IRQ_PHYRDY;
		writel(pp->intr_mask, port_mmio + PORT_IRQ_MASK);

		sata_link_scr_lpm(link, policy, false);
	}

	if (hpriv->cap & HOST_CAP_ALPM) {
		u32 cmd = readl(port_mmio + PORT_CMD);

		if (policy == ATA_LPM_MAX_POWER || !(hints & ATA_LPM_HIPM)) {
			cmd &= ~(PORT_CMD_ASP | PORT_CMD_ALPE);
			cmd |= PORT_CMD_ICC_ACTIVE;

			writel(cmd, port_mmio + PORT_CMD);
			readl(port_mmio + PORT_CMD);

			ata_msleep(ap, 10);
		} else {
			cmd |= PORT_CMD_ALPE;
			if (policy == ATA_LPM_MIN_POWER)
				cmd |= PORT_CMD_ASP;

			writel(cmd, port_mmio + PORT_CMD);
		}
	}

	if ((hpriv->cap2 & HOST_CAP2_SDS) &&
	    (hpriv->cap2 & HOST_CAP2_SADM) &&
	    (link->device->flags & ATA_DFLAG_DEVSLP)) {
		if (policy == ATA_LPM_MIN_POWER)
			ahci_set_aggressive_devslp(ap, true);
		else
			ahci_set_aggressive_devslp(ap, false);
	}

	if (policy == ATA_LPM_MAX_POWER) {
		sata_link_scr_lpm(link, policy, false);

		pp->intr_mask |= PORT_IRQ_PHYRDY;
		writel(pp->intr_mask, port_mmio + PORT_IRQ_MASK);
	}

	return 0;
}

#ifdef CONFIG_PM
static void ahci_power_down(struct ata_port *ap)
{
	struct ahci_host_priv *hpriv = ap->host->private_data;
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 cmd, scontrol;

	if (!(hpriv->cap & HOST_CAP_SSS))
		return;

	scontrol = readl(port_mmio + PORT_SCR_CTL);
	scontrol &= ~0xf;
	writel(scontrol, port_mmio + PORT_SCR_CTL);

	cmd = readl(port_mmio + PORT_CMD) & ~PORT_CMD_ICC_MASK;
	cmd &= ~PORT_CMD_SPIN_UP;
	writel(cmd, port_mmio + PORT_CMD);
}
#endif

#ifdef MY_DEF_HERE
static int syno_need_ahci_software_activity(struct ata_port *ap)
{
	struct pci_dev *pdev = NULL;
	int ret = 0;

	if (syno_is_hw_version(HW_DS2415p) || syno_is_hw_version(HW_RS2416p) || syno_is_hw_version(HW_RS2416rpp)) {
		goto END;
	}

	if (ap != NULL) {
		pdev = to_pci_dev(ap->dev);
		if (pdev != NULL && pdev->vendor == 0x8086) {
			switch (pdev->device) {
				 
				case 0x1f22:
				case 0x1f32:
					ret = 1;
					break;
				default:
					break;
			}
		}
#if defined(MY_DEF_HERE)
		if (pdev != NULL && pdev->vendor == PCI_VENDOR_ID_ANNAPURNA_LABS) {
			switch (pdev->device) {
				case 0x0031:
					ret = 1;
					break;
				default:
					break;
			}
		}
#endif  
	}

END:
	return ret;
}

static void syno_sw_activity(struct ata_port *ap)
{
#if defined(MY_DEF_HERE)
	SYNO_CTRL_HDD_ACT_NOTIFY(ap->syno_disk_index);
#else  
	if(NULL == gpGreenLedMap){
		return;
	}
	syno_ledtrig_active_set(gpGreenLedMap[ap->syno_disk_index]);		
#endif  
}

int syno_ahci_disk_led_enable(const unsigned short hostnum, const int iValue)
{
	struct Scsi_Host *shost = scsi_host_lookup(hostnum);
	struct ata_port *ap = NULL;
	int ret = -EINVAL;
	struct ahci_port_priv *pp = NULL;
	struct ahci_em_priv *emp = NULL;
	struct ata_link *link = NULL;
	unsigned long flags;

	if (NULL == shost) {
		goto END;
	}

	if (NULL == (ap = ata_shost_to_port(shost))) {
		goto END;
	}

	pp = ap->private_data;
	spin_lock_irqsave(ap->lock, flags);
	ata_for_each_link(link, ap, EDGE) {
		emp = &pp->em_priv[link->pmp];
		emp->saved_activity = emp->activity = 0;
		del_timer(&emp->timer);
	}

	if (iValue) {
		ap->flags |= ATA_FLAG_SW_ACTIVITY;
		ata_for_each_link(link, ap, EDGE) {
			ahci_init_sw_activity(link);
		}
	} else {
		ap->flags &= ~ATA_FLAG_SW_ACTIVITY;
		ata_for_each_link(link, ap, EDGE) {
			link->flags &= ~ATA_LFLAG_SW_ACTIVITY;
		}
	}
	spin_unlock_irqrestore(ap->lock, flags);
	ret = 0;

END:
	if (shost) {
		scsi_host_put(shost);
	}
	return ret;
}
EXPORT_SYMBOL(syno_ahci_disk_led_enable);

#ifdef MY_ABC_HERE
 
int syno_ahci_disk_led_enable_by_port(const unsigned short diskPort, const int iValue)
{
	int i = 0;
	unsigned short scsiHostNum = 0;
	 
	for (i = 0; i < SATA_REMAP_MAX; i++) {
		if ((unsigned short)syno_get_remap_idx(i) == (diskPort - 1)) {
			scsiHostNum = (unsigned short) i;
			break;
		}
	}

	return syno_ahci_disk_led_enable(scsiHostNum, iValue);
}
EXPORT_SYMBOL(syno_ahci_disk_led_enable_by_port);
#endif  
#endif  

static void ahci_start_port(struct ata_port *ap)
{
	struct ahci_host_priv *hpriv = ap->host->private_data;
	struct ahci_port_priv *pp = ap->private_data;
	struct ata_link *link;
	struct ahci_em_priv *emp;
	ssize_t rc;
	int i;

	ahci_start_fis_rx(ap);

	if (!(hpriv->flags & AHCI_HFLAG_DELAY_ENGINE))
		ahci_start_engine(ap);

	if (ap->flags & ATA_FLAG_EM) {
		ata_for_each_link(link, ap, EDGE) {
			emp = &pp->em_priv[link->pmp];

			for (i = 0; i < EM_MAX_RETRY; i++) {
#if defined(MY_DEF_HERE)
				rc = ap->ops->transmit_led_message(ap,
#else  
				rc = ahci_transmit_led_message(ap,
#endif  
							       emp->led_state,
							       4);
				if (rc == -EBUSY)
					ata_msleep(ap, 1);
				else
					break;
			}
		}
	}

#ifdef MY_DEF_HERE
	if (syno_need_ahci_software_activity(ap)) {
		ap->flags |= ATA_FLAG_SW_ACTIVITY;
	}
	 
#if defined(MY_DEF_HERE)
	ap->flags |= ATA_FLAG_SW_ACTIVITY;
#endif  
#endif  

	if (ap->flags & ATA_FLAG_SW_ACTIVITY)
		ata_for_each_link(link, ap, EDGE)
			ahci_init_sw_activity(link);

}

static int ahci_deinit_port(struct ata_port *ap, const char **emsg)
{
	int rc;

	rc = ahci_stop_engine(ap);
	if (rc) {
		*emsg = "failed to stop engine";
		return rc;
	}

	rc = ahci_stop_fis_rx(ap);
	if (rc) {
		*emsg = "failed stop FIS RX";
		return rc;
	}

	return 0;
}

int ahci_reset_controller(struct ata_host *host)
{
	struct ahci_host_priv *hpriv = host->private_data;
	void __iomem *mmio = hpriv->mmio;
	u32 tmp;

	ahci_enable_ahci(mmio);

	if (!ahci_skip_host_reset) {
		tmp = readl(mmio + HOST_CTL);
		if ((tmp & HOST_RESET) == 0) {
			writel(tmp | HOST_RESET, mmio + HOST_CTL);
			readl(mmio + HOST_CTL);  
		}

		tmp = ata_wait_register(NULL, mmio + HOST_CTL, HOST_RESET,
					HOST_RESET, 10, 1000);

		if (tmp & HOST_RESET) {
			dev_err(host->dev, "controller reset failed (0x%x)\n",
				tmp);
			return -EIO;
		}

		ahci_enable_ahci(mmio);

		ahci_restore_initial_config(host);
	} else
		dev_info(host->dev, "skipping global host reset\n");

	return 0;
}
EXPORT_SYMBOL_GPL(ahci_reset_controller);

static void ahci_sw_activity(struct ata_link *link)
{
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_em_priv *emp = &pp->em_priv[link->pmp];

#ifdef MY_DEF_HERE
        if (!giSynoHddLedEnabled) {
                return;
        }
#endif  
	if (!(link->flags & ATA_LFLAG_SW_ACTIVITY))
		return;

	emp->activity++;
	if (!timer_pending(&emp->timer))
		mod_timer(&emp->timer, jiffies + msecs_to_jiffies(10));
}

#ifdef MY_DEF_HERE
static void ahci_sw_locate_set(struct ata_link *link, u8 blEnable)
{
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_em_priv *emp = &pp->em_priv[link->pmp];
	unsigned long led_message = emp->led_state;
	unsigned long flags;

	led_message &= EM_MSG_LED_VALUE;

	led_message |= ap->port_no | (link->pmp << 8);

	emp->locate= blEnable;

	spin_lock_irqsave(ap->lock, flags);
	if (emp->saved_locate == emp->locate) {
		spin_unlock_irqrestore(ap->lock, flags);
		goto END;
	}
	emp->saved_locate = emp->locate;
	led_message &= ~(EM_MSG_LOCATE_LED_MASK);
	led_message |= (emp->locate << 19);
	spin_unlock_irqrestore(ap->lock, flags);
	ahci_transmit_led_message(ap, led_message, 4);
	mdelay(10);
END:
	return;
}

static void ahci_sw_fault_set(struct ata_link *link, u8 blEnable)
{
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_em_priv *emp = &pp->em_priv[link->pmp];
	unsigned long led_message = emp->led_state;
	unsigned long flags;

	led_message &= EM_MSG_LED_VALUE;

	led_message |= ap->port_no | (link->pmp << 8);

	emp->fault= blEnable;

	spin_lock_irqsave(ap->lock, flags);
	if (emp->saved_fault == emp->fault) {
		spin_unlock_irqrestore(ap->lock, flags);
		goto END;
	}
	emp->saved_fault = emp->fault;
	led_message &= ~(EM_MSG_FAULT_LED_MASK);
	led_message |= (emp->fault << 22);
	spin_unlock_irqrestore(ap->lock, flags);
	ahci_transmit_led_message(ap, led_message, 4);
	mdelay(10);
END:
	return;
}
#endif  

static void ahci_sw_activity_blink(unsigned long arg)
{
	struct ata_link *link = (struct ata_link *)arg;
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_em_priv *emp = &pp->em_priv[link->pmp];
	unsigned long led_message = emp->led_state;
#ifdef MY_DEF_HERE
#else  
	u32 activity_led_state;
#endif  
	unsigned long flags;

	led_message &= EM_MSG_LED_VALUE;
	led_message |= ap->port_no | (link->pmp << 8);

#ifdef MY_DEF_HERE
	spin_lock_irqsave(ap->lock, flags);
	if (emp->saved_activity != emp->activity) {
		emp->saved_activity = emp->activity;

		syno_sw_activity(ap);

		mod_timer(&emp->timer, jiffies + msecs_to_jiffies(100));
	}
	spin_unlock_irqrestore(ap->lock, flags);
#else  
	spin_lock_irqsave(ap->lock, flags);
	if (emp->saved_activity != emp->activity) {
		emp->saved_activity = emp->activity;
		 
		activity_led_state = led_message & EM_MSG_LED_VALUE_ON;

		if (activity_led_state)
			activity_led_state = 0;
		else
			activity_led_state = 1;

		led_message &= ~EM_MSG_LED_VALUE_ACTIVITY;

		led_message |= (activity_led_state << 16);
		mod_timer(&emp->timer, jiffies + msecs_to_jiffies(100));
	} else {
		 
		led_message &= ~EM_MSG_LED_VALUE_ACTIVITY;
#if defined(MY_DEF_HERE)
		if ((ata_phys_link_online(link)) || (emp->blink_policy == BLINK_OFF))
			led_message |= (1 << 16);
		mod_timer(&emp->timer, jiffies + msecs_to_jiffies(500));
#else  
		if (emp->blink_policy == BLINK_OFF)
			led_message |= (1 << 16);
#endif  
	}
	spin_unlock_irqrestore(ap->lock, flags);
#if defined(MY_DEF_HERE)
	ap->ops->transmit_led_message(ap, led_message, 4);
#else  
	ahci_transmit_led_message(ap, led_message, 4);
#endif  
#endif  
}

static void ahci_init_sw_activity(struct ata_link *link)
{
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_em_priv *emp = &pp->em_priv[link->pmp];

	emp->saved_activity = emp->activity = 0;
#if defined(MY_DEF_HERE)
	emp->blink_policy = BLINK_ON;
#endif  
	setup_timer(&emp->timer, ahci_sw_activity_blink, (unsigned long)link);

#ifdef MY_DEF_HERE
#else  
	if (emp->blink_policy)
#endif  
		link->flags |= ATA_LFLAG_SW_ACTIVITY;
}

int ahci_reset_em(struct ata_host *host)
{
	struct ahci_host_priv *hpriv = host->private_data;
	void __iomem *mmio = hpriv->mmio;
	u32 em_ctl;

	em_ctl = readl(mmio + HOST_EM_CTL);
	if ((em_ctl & EM_CTL_TM) || (em_ctl & EM_CTL_RST))
		return -EINVAL;

	writel(em_ctl | EM_CTL_RST, mmio + HOST_EM_CTL);
	return 0;
}
EXPORT_SYMBOL_GPL(ahci_reset_em);

#if defined(MY_DEF_HERE)
ssize_t al_ahci_transmit_led_message(struct ata_port *ap, u32 state,
					    ssize_t size)
{
	struct ahci_host_priv *hpriv = ap->host->private_data;
	struct ahci_port_priv *pp = ap->private_data;
	unsigned long flags;
	int led_val = 0;
	int pmp;
	struct ahci_em_priv *emp;

	pmp = (state & EM_MSG_LED_PMP_SLOT) >> 8;
	if (pmp < EM_MAX_SLOTS)
		emp = &pp->em_priv[pmp];
	else
		return -EINVAL;

	if (hpriv->led_gpio[ap->port_no] == -1)
		return -EINVAL;

	spin_lock_irqsave(&ap->host->lock, flags);

	if(state & EM_MSG_LED_VALUE_ON)
		led_val = 1;

	gpio_set_value(hpriv->led_gpio[ap->port_no], led_val);

	emp->led_state = state;

	spin_unlock_irqrestore(&ap->host->lock, flags);
	return size;
}
#endif  

static ssize_t ahci_transmit_led_message(struct ata_port *ap, u32 state,
					ssize_t size)
{
	struct ahci_host_priv *hpriv = ap->host->private_data;
	struct ahci_port_priv *pp = ap->private_data;
	void __iomem *mmio = hpriv->mmio;
	u32 em_ctl;
	u32 message[] = {0, 0};
	unsigned long flags;
	int pmp;
	struct ahci_em_priv *emp;

	pmp = (state & EM_MSG_LED_PMP_SLOT) >> 8;
	if (pmp < EM_MAX_SLOTS)
		emp = &pp->em_priv[pmp];
	else
		return -EINVAL;

	spin_lock_irqsave(ap->lock, flags);

	em_ctl = readl(mmio + HOST_EM_CTL);
	if (em_ctl & EM_CTL_TM) {
		spin_unlock_irqrestore(ap->lock, flags);
		return -EBUSY;
	}

	if (hpriv->em_msg_type & EM_MSG_TYPE_LED) {
		 
		message[0] |= (4 << 8);

		message[1] = ((state & ~EM_MSG_LED_HBA_PORT) | ap->port_no);

		writel(message[0], mmio + hpriv->em_loc);
		writel(message[1], mmio + hpriv->em_loc+4);

		writel(em_ctl | EM_CTL_TM, mmio + HOST_EM_CTL);
	}

	emp->led_state = state;

	spin_unlock_irqrestore(ap->lock, flags);
	return size;
}

static ssize_t ahci_led_show(struct ata_port *ap, char *buf)
{
	struct ahci_port_priv *pp = ap->private_data;
	struct ata_link *link;
	struct ahci_em_priv *emp;
	int rc = 0;

	ata_for_each_link(link, ap, EDGE) {
		emp = &pp->em_priv[link->pmp];
		rc += sprintf(buf, "%lx\n", emp->led_state);
	}
	return rc;
}

static ssize_t ahci_led_store(struct ata_port *ap, const char *buf,
				size_t size)
{
	int state;
	int pmp;
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_em_priv *emp;

	state = simple_strtoul(buf, NULL, 0);

	pmp = (state & EM_MSG_LED_PMP_SLOT) >> 8;
	if (pmp < EM_MAX_SLOTS)
		emp = &pp->em_priv[pmp];
	else
		return -EINVAL;

	if (emp->blink_policy)
		state &= ~EM_MSG_LED_VALUE_ACTIVITY;

#if defined(MY_DEF_HERE)
	return ap->ops->transmit_led_message(ap, state, size);
#else  
	return ahci_transmit_led_message(ap, state, size);
#endif  
}

static ssize_t ahci_activity_store(struct ata_device *dev, enum sw_activity val)
{
	struct ata_link *link = dev->link;
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_em_priv *emp = &pp->em_priv[link->pmp];
	u32 port_led_state = emp->led_state;

	if (val == OFF) {
		 
		link->flags &= ~(ATA_LFLAG_SW_ACTIVITY);

		port_led_state &= EM_MSG_LED_VALUE_OFF;
		port_led_state |= (ap->port_no | (link->pmp << 8));
#if defined(MY_DEF_HERE)
		ap->ops->transmit_led_message(ap, port_led_state, 4);
#else  
		ahci_transmit_led_message(ap, port_led_state, 4);
#endif  
	} else {
		link->flags |= ATA_LFLAG_SW_ACTIVITY;
		if (val == BLINK_OFF) {
			 
			port_led_state &= EM_MSG_LED_VALUE_OFF;
			port_led_state |= (ap->port_no | (link->pmp << 8));
			port_led_state |= EM_MSG_LED_VALUE_ON;  
#if defined(MY_DEF_HERE)
			ap->ops->transmit_led_message(ap, port_led_state, 4);
#else  
			ahci_transmit_led_message(ap, port_led_state, 4);
#endif  
		}
	}
	emp->blink_policy = val;
	return 0;
}

static ssize_t ahci_activity_show(struct ata_device *dev, char *buf)
{
	struct ata_link *link = dev->link;
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_em_priv *emp = &pp->em_priv[link->pmp];

	return sprintf(buf, "%d\n", emp->blink_policy);
}

static void ahci_port_init(struct device *dev, struct ata_port *ap,
			   int port_no, void __iomem *mmio,
			   void __iomem *port_mmio)
{
	const char *emsg = NULL;
	int rc;
	u32 tmp;

	rc = ahci_deinit_port(ap, &emsg);
	if (rc)
		dev_warn(dev, "%s (%d)\n", emsg, rc);

	tmp = readl(port_mmio + PORT_SCR_ERR);
	VPRINTK("PORT_SCR_ERR 0x%x\n", tmp);
	writel(tmp, port_mmio + PORT_SCR_ERR);

	tmp = readl(port_mmio + PORT_IRQ_STAT);
	VPRINTK("PORT_IRQ_STAT 0x%x\n", tmp);
	if (tmp)
		writel(tmp, port_mmio + PORT_IRQ_STAT);

#if defined(CONFIG_SYNO_LSP_HI3536) && defined(CONFIG_ARCH_HI3531A)
	writel(1 << ap->port_no, mmio + HOST_IRQ_STAT);
#else  
	writel(1 << port_no, mmio + HOST_IRQ_STAT);
#endif  
}

void ahci_init_controller(struct ata_host *host)
{
	struct ahci_host_priv *hpriv = host->private_data;
	void __iomem *mmio = hpriv->mmio;
	int i;
	void __iomem *port_mmio;
	u32 tmp;

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];

		port_mmio = ahci_port_base(ap);
		if (ata_port_is_dummy(ap))
			continue;

		ahci_port_init(host->dev, ap, i, mmio, port_mmio);
	}

	tmp = readl(mmio + HOST_CTL);
	VPRINTK("HOST_CTL 0x%x\n", tmp);
	writel(tmp | HOST_IRQ_EN, mmio + HOST_CTL);
	tmp = readl(mmio + HOST_CTL);
	VPRINTK("HOST_CTL 0x%x\n", tmp);
}
EXPORT_SYMBOL_GPL(ahci_init_controller);

static void ahci_dev_config(struct ata_device *dev)
{
	struct ahci_host_priv *hpriv = dev->link->ap->host->private_data;

	if (hpriv->flags & AHCI_HFLAG_SECT255) {
		dev->max_sectors = 255;
		ata_dev_info(dev,
			     "SB600 AHCI: limiting to 255 sectors per cmd\n");
	}
}

unsigned int ahci_dev_classify(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	struct ata_taskfile tf;
	u32 tmp;

	tmp = readl(port_mmio + PORT_SIG);
	tf.lbah		= (tmp >> 24)	& 0xff;
	tf.lbam		= (tmp >> 16)	& 0xff;
	tf.lbal		= (tmp >> 8)	& 0xff;
	tf.nsect	= (tmp)		& 0xff;

	return ata_dev_classify(&tf);
}
EXPORT_SYMBOL_GPL(ahci_dev_classify);

void ahci_fill_cmd_slot(struct ahci_port_priv *pp, unsigned int tag,
			u32 opts)
{
	dma_addr_t cmd_tbl_dma;

#if defined(CONFIG_SYNO_LSP_HI3536) && defined(CONFIG_HI_SATA_RAM)
	 
	cmd_tbl_dma = pp->cmd_tbl_dma + tag * 0x800;
#else  
	cmd_tbl_dma = pp->cmd_tbl_dma + tag * AHCI_CMD_TBL_SZ;
#endif  

	pp->cmd_slot[tag].opts = cpu_to_le32(opts);
	pp->cmd_slot[tag].status = 0;
	pp->cmd_slot[tag].tbl_addr = cpu_to_le32(cmd_tbl_dma & 0xffffffff);
	pp->cmd_slot[tag].tbl_addr_hi = cpu_to_le32((cmd_tbl_dma >> 16) >> 16);
}
EXPORT_SYMBOL_GPL(ahci_fill_cmd_slot);

int ahci_kick_engine(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	struct ahci_host_priv *hpriv = ap->host->private_data;
	u8 status = readl(port_mmio + PORT_TFDATA) & 0xFF;
	u32 tmp;
	int busy, rc;

	rc = ahci_stop_engine(ap);
	if (rc)
		goto out_restart;

	busy = status & (ATA_BUSY | ATA_DRQ);
	if (!busy && !sata_pmp_attached(ap)) {
		rc = 0;
		goto out_restart;
	}

	if (!(hpriv->cap & HOST_CAP_CLO)) {
		rc = -EOPNOTSUPP;
		goto out_restart;
	}

	tmp = readl(port_mmio + PORT_CMD);
	tmp |= PORT_CMD_CLO;
	writel(tmp, port_mmio + PORT_CMD);

	rc = 0;
	tmp = ata_wait_register(ap, port_mmio + PORT_CMD,
				PORT_CMD_CLO, PORT_CMD_CLO, 1, 500);
	if (tmp & PORT_CMD_CLO)
		rc = -EIO;

 out_restart:
	ahci_start_engine(ap);
	return rc;
}
EXPORT_SYMBOL_GPL(ahci_kick_engine);

static int ahci_exec_polled_cmd(struct ata_port *ap, int pmp,
				struct ata_taskfile *tf, int is_cmd, u16 flags,
				unsigned long timeout_msec)
{
	const u32 cmd_fis_len = 5;  
	struct ahci_port_priv *pp = ap->private_data;
	void __iomem *port_mmio = ahci_port_base(ap);
	u8 *fis = pp->cmd_tbl;
	u32 tmp;

	ata_tf_to_fis(tf, pmp, is_cmd, fis);
	ahci_fill_cmd_slot(pp, 0, cmd_fis_len | flags | (pmp << 12));

	if (pp->fbs_enabled && pp->fbs_last_dev != pmp) {
		tmp = readl(port_mmio + PORT_FBS);
		tmp &= ~(PORT_FBS_DEV_MASK | PORT_FBS_DEC);
		tmp |= pmp << PORT_FBS_DEV_OFFSET;
		writel(tmp, port_mmio + PORT_FBS);
		pp->fbs_last_dev = pmp;
	}

	writel(1, port_mmio + PORT_CMD_ISSUE);

	if (timeout_msec) {
		tmp = ata_wait_register(ap, port_mmio + PORT_CMD_ISSUE,
					0x1, 0x1, 1, timeout_msec);
		if (tmp & 0x1) {
			ahci_kick_engine(ap);
			return -EBUSY;
		}
	} else
		readl(port_mmio + PORT_CMD_ISSUE);	 

	return 0;
}

int ahci_do_softreset(struct ata_link *link, unsigned int *class,
		      int pmp, unsigned long deadline,
		      int (*check_ready)(struct ata_link *link))
{
	struct ata_port *ap = link->ap;
	struct ahci_host_priv *hpriv = ap->host->private_data;
	struct ahci_port_priv *pp = ap->private_data;
	const char *reason = NULL;
	unsigned long now, msecs;
	struct ata_taskfile tf;
	bool fbs_disabled = false;
	int rc;

#if defined(CONFIG_SYNO_LSP_HI3536)
#if (defined(CONFIG_ARCH_HI3536) \
	|| defined(CONFIG_ARCH_HI3521A) \
	|| defined(CONFIG_ARCH_HI3531A)) \
	&& !defined(CONFIG_SATA_AHCI)
	unsigned int port_num = ap->port_no;
#endif
#endif  

	DPRINTK("ENTER\n");

#if defined(CONFIG_SYNO_LSP_HI3536)
#if (defined(CONFIG_ARCH_HI3536) \
	|| defined(CONFIG_ARCH_HI3521A) \
	|| defined(CONFIG_ARCH_HI3531A)) \
	&& !defined(CONFIG_SATA_AHCI)
	if (fbs_ctrl[port_num].fbs_enable_ctrl &&
			(link->pmp == SATA_PMP_CTRL_PORT)) {
		struct ahci_port_priv *pp = ap->private_data;

		if (pp->fbs_enabled == false)
			ahci_enable_fbs(ap);

		fbs_ctrl[port_num].fbs_enable_flag = 0;
		fbs_ctrl[port_num].fbs_disable_flag = 0;
		fbs_ctrl[port_num].fbs_cmd_issue_flag = 0;

	}
#endif
#endif  

	rc = ahci_kick_engine(ap);
	if (rc && rc != -EOPNOTSUPP)
		ata_link_warn(link, "failed to reset engine (errno=%d)\n", rc);

	if (!ata_is_host_link(link) && pp->fbs_enabled) {
		ahci_disable_fbs(ap);
		fbs_disabled = true;
	}

	ata_tf_init(link->device, &tf);

	msecs = 0;
	now = jiffies;
	if (time_after(deadline, now))
		msecs = jiffies_to_msecs(deadline - now);

	tf.ctl |= ATA_SRST;
	if (ahci_exec_polled_cmd(ap, pmp, &tf, 0,
				 AHCI_CMD_RESET | AHCI_CMD_CLR_BUSY, msecs)) {
		rc = -EIO;
		reason = "1st FIS failed";
		goto fail;
	}

	ata_msleep(ap, 1);

	tf.ctl &= ~ATA_SRST;
#ifdef MY_ABC_HERE
	if ((hpriv->flags & AHCI_HFLAG_YES_MV9235_FIX)) {
		 
		msecs = 0;
		now = jiffies;
		if (time_after(deadline, now))
			msecs = jiffies_to_msecs(deadline - now);
		if(ahci_exec_polled_cmd(ap, pmp, &tf, 0, 0, msecs)) {
			rc = -EIO;
			reason = "2nd FIS failed";
			goto fail;
		}
	} else {
#endif  
	ahci_exec_polled_cmd(ap, pmp, &tf, 0, 0, 0);
#ifdef MY_ABC_HERE
	}
#endif  

	rc = ata_wait_after_reset(link, deadline, check_ready);
	if (rc == -EBUSY && hpriv->flags & AHCI_HFLAG_SRST_TOUT_IS_OFFLINE) {
		 
		ata_link_info(link, "device not ready, treating as offline\n");
		*class = ATA_DEV_NONE;
	} else if (rc) {
		 
		reason = "device not ready";
		goto fail;
	} else
		*class = ahci_dev_classify(ap);

	if (fbs_disabled)
		ahci_enable_fbs(ap);

	DPRINTK("EXIT, class=%u\n", *class);
	return 0;

 fail:
	ata_link_err(link, "softreset failed (%s)\n", reason);
#ifdef MY_ABC_HERE
	 
	if (fbs_disabled)
		ahci_enable_fbs(ap);
#endif  
	return rc;
}

int ahci_check_ready(struct ata_link *link)
{
	void __iomem *port_mmio = ahci_port_base(link->ap);
	u8 status = readl(port_mmio + PORT_TFDATA) & 0xFF;

	return ata_check_ready(status);
}
EXPORT_SYMBOL_GPL(ahci_check_ready);

static int ahci_softreset(struct ata_link *link, unsigned int *class,
			  unsigned long deadline)
{
	int pmp = sata_srst_pmp(link);

	DPRINTK("ENTER\n");

	return ahci_do_softreset(link, class, pmp, deadline, ahci_check_ready);
}
EXPORT_SYMBOL_GPL(ahci_do_softreset);

static int ahci_bad_pmp_check_ready(struct ata_link *link)
{
	void __iomem *port_mmio = ahci_port_base(link->ap);
	u8 status = readl(port_mmio + PORT_TFDATA) & 0xFF;
	u32 irq_status = readl(port_mmio + PORT_IRQ_STAT);

	if (irq_status & PORT_IRQ_BAD_PMP)
		return -EIO;

	return ata_check_ready(status);
}

int ahci_pmp_retry_softreset(struct ata_link *link, unsigned int *class,
				unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	void __iomem *port_mmio = ahci_port_base(ap);
	int pmp = sata_srst_pmp(link);
	int rc;
	u32 irq_sts;

	DPRINTK("ENTER\n");

	rc = ahci_do_softreset(link, class, pmp, deadline,
			       ahci_bad_pmp_check_ready);

	if (rc == -EIO) {
		irq_sts = readl(port_mmio + PORT_IRQ_STAT);
		if (irq_sts & PORT_IRQ_BAD_PMP) {
			ata_link_warn(link,
					"applying PMP SRST workaround "
					"and retrying\n");
			rc = ahci_do_softreset(link, class, 0, deadline,
					       ahci_check_ready);
		}
	}

	return rc;
}

static int ahci_hardreset(struct ata_link *link, unsigned int *class,
			  unsigned long deadline)
{
	const unsigned long *timing = sata_ehc_deb_timing(&link->eh_context);
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	u8 *d2h_fis = pp->rx_fis + RX_FIS_D2H_REG;
	struct ata_taskfile tf;
	bool online;
	int rc;

	DPRINTK("ENTER\n");

	ahci_stop_engine(ap);

	ata_tf_init(link->device, &tf);
	tf.command = 0x80;
	ata_tf_to_fis(&tf, 0, 0, d2h_fis);

	rc = sata_link_hardreset(link, timing, deadline, &online,
				 ahci_check_ready);

	ahci_start_engine(ap);

	if (online)
		*class = ahci_dev_classify(ap);

	DPRINTK("EXIT, rc=%d, class=%u\n", rc, *class);
	return rc;
}

static void ahci_postreset(struct ata_link *link, unsigned int *class)
{
	struct ata_port *ap = link->ap;
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 new_tmp, tmp;

#if defined(CONFIG_SYNO_LSP_HI3536)
#if (defined(CONFIG_ARCH_HI3536) || defined(CONFIG_ARCH_HI3531A)) \
	&& !defined(CONFIG_SATA_AHCI)
	u32 sstatus;
#endif
#endif  
	ata_std_postreset(link, class);

#if defined(CONFIG_SYNO_LSP_HI3536)
#if defined(CONFIG_ARCH_HI3536) && !defined(CONFIG_SATA_AHCI)
	if ((sata_scr_read(link, SCR_STATUS, &sstatus) == 0) &&
				((sstatus & 0xf) != 0x3)) {
		switch (ap->port_no) {
		case 0:
			writel(0x419, (void *)IO_ADDRESS(0x12120090));
			writel(0x439, (void *)IO_ADDRESS(0x12120090));
			writel(0x419, (void *)IO_ADDRESS(0x12120090));
			writel(0x0, (void *)IO_ADDRESS(0x12120090));
			break;
		case 1:
			writel(0x419, (void *)IO_ADDRESS(0x12120094));
			writel(0x439, (void *)IO_ADDRESS(0x12120094));
			writel(0x419, (void *)IO_ADDRESS(0x12120094));
			writel(0x0, (void *)IO_ADDRESS(0x12120094));
			break;
		case 2:
			writel(0x439, (void *)IO_ADDRESS(0x12120098));
			writel(0x479, (void *)IO_ADDRESS(0x12120098));
			writel(0x439, (void *)IO_ADDRESS(0x12120098));
			writel(0x0, (void *)IO_ADDRESS(0x12120098));
			break;
		case 3:
			writel(0x419, (void *)IO_ADDRESS(0x12120098));
			writel(0x459, (void *)IO_ADDRESS(0x12120098));
			writel(0x419, (void *)IO_ADDRESS(0x12120098));
			writel(0x0, (void *)IO_ADDRESS(0x12120098));
			break;
		default:
			break;
		}
	}
#endif

#if defined(CONFIG_ARCH_HI3531A) && !defined(CONFIG_SATA_AHCI)
	if ((sata_scr_read(link, SCR_STATUS, &sstatus) == 0) &&
				((sstatus & 0xf) != 0x3)) {
		switch (ap->port_no) {
		case 0:
			writel(0x439, (void *)IO_ADDRESS(0x12120138));
			writel(0x479, (void *)IO_ADDRESS(0x12120138));
			writel(0x439, (void *)IO_ADDRESS(0x12120138));
			writel(0x0, (void *)IO_ADDRESS(0x12120138));
			break;
		case 1:
			writel(0x419, (void *)IO_ADDRESS(0x12120138));
			writel(0x459, (void *)IO_ADDRESS(0x12120138));
			writel(0x419, (void *)IO_ADDRESS(0x12120138));
			writel(0x0, (void *)IO_ADDRESS(0x12120138));
			break;
		case 2:
			writel(0x439, (void *)IO_ADDRESS(0x12120134));
			writel(0x479, (void *)IO_ADDRESS(0x12120134));
			writel(0x439, (void *)IO_ADDRESS(0x12120134));
			writel(0x0, (void *)IO_ADDRESS(0x12120134));
			break;
		case 3:
			writel(0x419, (void *)IO_ADDRESS(0x12120134));
			writel(0x459, (void *)IO_ADDRESS(0x12120134));
			writel(0x419, (void *)IO_ADDRESS(0x12120134));
			writel(0x0, (void *)IO_ADDRESS(0x12120134));
			break;
		default:
			break;
		}
	}
#endif
#endif  

	new_tmp = tmp = readl(port_mmio + PORT_CMD);
	if (*class == ATA_DEV_ATAPI)
		new_tmp |= PORT_CMD_ATAPI;
	else
		new_tmp &= ~PORT_CMD_ATAPI;
	if (new_tmp != tmp) {
		writel(new_tmp, port_mmio + PORT_CMD);
		readl(port_mmio + PORT_CMD);  
	}
}

static unsigned int ahci_fill_sg(struct ata_queued_cmd *qc, void *cmd_tbl)
{
	struct scatterlist *sg;
	struct ahci_sg *ahci_sg = cmd_tbl + AHCI_CMD_TBL_HDR_SZ;
	unsigned int si;

	VPRINTK("ENTER\n");

	for_each_sg(qc->sg, sg, qc->n_elem, si) {
		dma_addr_t addr = sg_dma_address(sg);
		u32 sg_len = sg_dma_len(sg);

		ahci_sg[si].addr = cpu_to_le32(addr & 0xffffffff);
		ahci_sg[si].addr_hi = cpu_to_le32((addr >> 16) >> 16);
		ahci_sg[si].flags_size = cpu_to_le32(sg_len - 1);
	}

	return si;
}

#ifdef MY_ABC_HERE
int sata_syno_ahci_defer_cmd(struct ata_queued_cmd *qc)
{
	struct ata_link *link = qc->dev->link;
	struct ata_port *ap = link->ap;
	u8 prot = qc->tf.protocol;

	int is_excl = (ata_is_atapi(prot) ||
		       (qc->flags & ATA_QCFLAG_RESULT_TF));

	if (unlikely(ap->excl_link)) {
		if (link == ap->excl_link) {
			if (ap->nr_active_links)
				return ATA_DEFER_PORT;
			qc->flags |= ATA_QCFLAG_CLEAR_EXCL;
		} else {
			if (!ap->nr_active_links) {
				 
				if (is_excl) {
					ap->excl_link = link;
					qc->flags |= ATA_QCFLAG_CLEAR_EXCL;
				} else {
					 
					ap->excl_link = NULL;
				}
			} else {
				return ATA_DEFER_PORT;
			}
		}
	} else if (unlikely(is_excl)) {
		ap->excl_link = link;
		if (ap->nr_active_links)
			return ATA_DEFER_PORT;
		qc->flags |= ATA_QCFLAG_CLEAR_EXCL;
	}

	return ata_std_qc_defer(qc);
}
EXPORT_SYMBOL_GPL(sata_syno_ahci_defer_cmd);
#endif  

#ifdef MY_ABC_HERE
int ahci_syno_pmp_3x26_qc_defer(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	if (sata_pmp_attached(ap) && (ap->uiStsFlags & SYNO_STATUS_IS_SIL3x26)) {
		return sata_syno_ahci_defer_cmd(qc);
	}
	else
		return ata_std_qc_defer(qc);
}
EXPORT_SYMBOL_GPL(ahci_syno_pmp_3x26_qc_defer);
#endif

static int ahci_pmp_qc_defer(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct ahci_port_priv *pp = ap->private_data;

#if defined(CONFIG_SYNO_LSP_HI3536)
#if (defined(CONFIG_ARCH_HI3536) \
	|| defined(CONFIG_ARCH_HI3521A) \
	|| defined(CONFIG_ARCH_HI3531A)) \
	&& !defined(CONFIG_SATA_AHCI)
	int is_atapi = ata_is_atapi(qc->tf.protocol);
	void __iomem *port_mmio = ahci_port_base(ap);
	unsigned int port_num = ap->port_no;
	unsigned int cmd_timeout_count;

	if (fbs_ctrl[port_num].fbs_enable_ctrl &&
			(ap->link.pmp == SATA_PMP_CTRL_PORT)) {
		if (is_atapi || fbs_ctrl[ap->port_no].fbs_cmd_issue_flag) {
			mod_timer(&fbs_ctrl[port_num].poll_timer,
					jiffies + AHCI_POLL_TIMER);

			if (!fbs_ctrl[port_num].fbs_disable_flag) {
				cmd_timeout_count = 0;
				while (readl(port_mmio + PORT_SCR_ACT)
						|| readl(port_mmio
							+ PORT_CMD_ISSUE)
						|| readl(port_mmio
							+ PORT_IRQ_STAT)) {
					cmd_timeout_count++;
					if (cmd_timeout_count >=
							AHCI_TIMEOUT_COUNT) {
						fbs_ctrl[ap->port_no].
							fbs_cmd_issue_flag = 1;
						cmd_timeout_count = 0;
						return ATA_DEFER_LINK;
					}
				}

				if (pp->fbs_enabled == true)
					ahci_disable_fbs(ap);

				ap->excl_link = NULL;
				ap->nr_active_links = 0;
				fbs_ctrl[port_num].fbs_disable_flag = 1;
				fbs_ctrl[port_num].fbs_enable_flag = 0;
				fbs_ctrl[ap->port_no].fbs_cmd_issue_flag = 0;
			}
		} else {
			if (fbs_ctrl[port_num].fbs_enable_flag) {
				cmd_timeout_count = 0;
				while (readl(port_mmio + PORT_SCR_ACT)
						|| readl(port_mmio
							+ PORT_CMD_ISSUE)
						|| readl(port_mmio
							+ PORT_IRQ_STAT)) {
					cmd_timeout_count++;
					if (cmd_timeout_count >=
							AHCI_TIMEOUT_COUNT) {
						cmd_timeout_count = 0;
						return ATA_DEFER_LINK;
					}
				}

				if (pp->fbs_enabled == false)
					ahci_enable_fbs(ap);
				fbs_ctrl[port_num].fbs_enable_flag = 0;
				fbs_ctrl[port_num].fbs_disable_flag = 0;
			}
		}
	}
#endif
#endif  
	if (!sata_pmp_attached(ap) || pp->fbs_enabled)
		return ata_std_qc_defer(qc);
	else
		return sata_pmp_qc_defer_cmd_switch(qc);
}

static void ahci_qc_prep(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct ahci_port_priv *pp = ap->private_data;
	int is_atapi = ata_is_atapi(qc->tf.protocol);
	void *cmd_tbl;
	u32 opts;
	const u32 cmd_fis_len = 5;  
	unsigned int n_elem;

#if defined(CONFIG_SYNO_LSP_HI3536) && defined(CONFIG_HI_SATA_RAM)
	 
	cmd_tbl = pp->cmd_tbl + qc->tag * 0x800;
#else  
	cmd_tbl = pp->cmd_tbl + qc->tag * AHCI_CMD_TBL_SZ;
#endif  

	ata_tf_to_fis(&qc->tf, qc->dev->link->pmp, 1, cmd_tbl);
	if (is_atapi) {
		memset(cmd_tbl + AHCI_CMD_TBL_CDB, 0, 32);
		memcpy(cmd_tbl + AHCI_CMD_TBL_CDB, qc->cdb, qc->dev->cdb_len);
	}

	n_elem = 0;
	if (qc->flags & ATA_QCFLAG_DMAMAP)
		n_elem = ahci_fill_sg(qc, cmd_tbl);

	opts = cmd_fis_len | n_elem << 16 | (qc->dev->link->pmp << 12);
	if (qc->tf.flags & ATA_TFLAG_WRITE)
		opts |= AHCI_CMD_WRITE;
	if (is_atapi)
		opts |= AHCI_CMD_ATAPI | AHCI_CMD_PREFETCH;

	ahci_fill_cmd_slot(pp, qc->tag, opts);
}

static void ahci_fbs_dec_intr(struct ata_port *ap)
{
	struct ahci_port_priv *pp = ap->private_data;
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 fbs = readl(port_mmio + PORT_FBS);
	int retries = 3;

	DPRINTK("ENTER\n");
	BUG_ON(!pp->fbs_enabled);

	writel(fbs | PORT_FBS_DEC, port_mmio + PORT_FBS);
	fbs = readl(port_mmio + PORT_FBS);
	while ((fbs & PORT_FBS_DEC) && retries--) {
		udelay(1);
		fbs = readl(port_mmio + PORT_FBS);
	}

	if (fbs & PORT_FBS_DEC)
		dev_err(ap->host->dev, "failed to clear device error\n");
}

static void ahci_error_intr(struct ata_port *ap, u32 irq_stat)
{
	struct ahci_host_priv *hpriv = ap->host->private_data;
	struct ahci_port_priv *pp = ap->private_data;
	struct ata_eh_info *host_ehi = &ap->link.eh_info;
	struct ata_link *link = NULL;
	struct ata_queued_cmd *active_qc;
	struct ata_eh_info *active_ehi;
	bool fbs_need_dec = false;
	u32 serror;

	if (pp->fbs_enabled) {
		void __iomem *port_mmio = ahci_port_base(ap);
		u32 fbs = readl(port_mmio + PORT_FBS);
		int pmp = fbs >> PORT_FBS_DWE_OFFSET;

#if defined(MY_ABC_HERE) || defined(CONFIG_SYNO_LSP_HI3536)
		if ((fbs & PORT_FBS_SDE) && (pmp < ap->nr_pmp_links)) {
#else  
		if ((fbs & PORT_FBS_SDE) && (pmp < ap->nr_pmp_links) &&
		    ata_link_online(&ap->pmp_link[pmp])) {
#endif  
			link = &ap->pmp_link[pmp];
			fbs_need_dec = true;
		}

	} else
		ata_for_each_link(link, ap, EDGE)
			if (ata_link_active(link))
				break;

	if (!link)
		link = &ap->link;

	active_qc = ata_qc_from_tag(ap, link->active_tag);
	active_ehi = &link->eh_info;

	ata_ehi_clear_desc(host_ehi);
	ata_ehi_push_desc(host_ehi, "irq_stat 0x%08x", irq_stat);

	ahci_scr_read(&ap->link, SCR_ERROR, &serror);
	ahci_scr_write(&ap->link, SCR_ERROR, serror);
	host_ehi->serror |= serror;

	if (hpriv->flags & AHCI_HFLAG_IGN_IRQ_IF_ERR)
		irq_stat &= ~PORT_IRQ_IF_ERR;

	if (irq_stat & PORT_IRQ_TF_ERR) {
		 
		if (active_qc)
			active_qc->err_mask |= AC_ERR_DEV;
		else
			active_ehi->err_mask |= AC_ERR_DEV;

		if (hpriv->flags & AHCI_HFLAG_IGN_SERR_INTERNAL)
			host_ehi->serror &= ~SERR_INTERNAL;
	}

	if (irq_stat & PORT_IRQ_UNK_FIS) {
		u32 *unk = (u32 *)(pp->rx_fis + RX_FIS_UNK);

		active_ehi->err_mask |= AC_ERR_HSM;
		active_ehi->action |= ATA_EH_RESET;
		ata_ehi_push_desc(active_ehi,
				  "unknown FIS %08x %08x %08x %08x" ,
				  unk[0], unk[1], unk[2], unk[3]);
	}

	if (sata_pmp_attached(ap) && (irq_stat & PORT_IRQ_BAD_PMP)) {
		active_ehi->err_mask |= AC_ERR_HSM;
		active_ehi->action |= ATA_EH_RESET;
		ata_ehi_push_desc(active_ehi, "incorrect PMP");
	}

	if (irq_stat & (PORT_IRQ_HBUS_ERR | PORT_IRQ_HBUS_DATA_ERR)) {
		host_ehi->err_mask |= AC_ERR_HOST_BUS;
		host_ehi->action |= ATA_EH_RESET;
		ata_ehi_push_desc(host_ehi, "host bus error");
	}

	if (irq_stat & PORT_IRQ_IF_ERR) {
		if (fbs_need_dec)
			active_ehi->err_mask |= AC_ERR_DEV;
		else {
			host_ehi->err_mask |= AC_ERR_ATA_BUS;
			host_ehi->action |= ATA_EH_RESET;
		}

		ata_ehi_push_desc(host_ehi, "interface fatal error");
	}

	if (irq_stat & (PORT_IRQ_CONNECT | PORT_IRQ_PHYRDY)) {
#ifdef MY_ABC_HERE
		syno_ata_info_print(ap);
#endif  
#ifdef MY_ABC_HERE
		if (irq_stat & PORT_IRQ_CONNECT) {
			ap->pflags |= ATA_PFLAG_SYNO_BOOT_PROBE;
		}
#endif  
		ata_ehi_hotplugged(host_ehi);
		ata_ehi_push_desc(host_ehi, "%s",
			irq_stat & PORT_IRQ_CONNECT ?
			"connection status changed" : "PHY RDY changed");
	}

#if defined(CONFIG_SYNO_LSP_HI3536)
	if (irq_stat & PORT_IRQ_FREEZE) {
		if ((irq_stat & PORT_IRQ_IF_ERR) && fbs_need_dec) {
			ata_link_abort(link);
			ahci_fbs_dec_intr(ap);
		} else
			ata_port_freeze(ap);
	} else if (fbs_need_dec) {
#else  
	if (irq_stat & PORT_IRQ_FREEZE)
		ata_port_freeze(ap);
	else if (fbs_need_dec) {
#endif  
		ata_link_abort(link);
		ahci_fbs_dec_intr(ap);
	} else
		ata_port_abort(ap);
}

static void ahci_handle_port_interrupt(struct ata_port *ap,
				       void __iomem *port_mmio, u32 status)
{
	struct ata_eh_info *ehi = &ap->link.eh_info;
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_host_priv *hpriv = ap->host->private_data;
	int resetting = !!(ap->pflags & ATA_PFLAG_RESETTING);
	u32 qc_active = 0;
	int rc;

	if (unlikely(resetting))
		status &= ~PORT_IRQ_BAD_PMP;

	if (sata_lpm_ignore_phy_events(&ap->link)) {
		status &= ~PORT_IRQ_PHYRDY;
		ahci_scr_write(&ap->link, SCR_ERROR, SERR_PHYRDY_CHG);
	}

	if (unlikely(status & PORT_IRQ_ERROR)) {
		ahci_error_intr(ap, status);
#if defined(CONFIG_SYNO_LSP_HI3536)
		if (!(status & ~(PORT_IRQ_ERROR)))
			return;
#else  
		return;
#endif  
	}

	if (status & PORT_IRQ_SDB_FIS) {
			 
		if (hpriv->cap & HOST_CAP_SNTF)
			sata_async_notification(ap);
		else {
			 
			if (pp->fbs_enabled)
				WARN_ON_ONCE(1);
			else {
				const __le32 *f = pp->rx_fis + RX_FIS_SDB;
				u32 f0 = le32_to_cpu(f[0]);
				if (f0 & (1 << 15))
					sata_async_notification(ap);
			}
		}
	}

	if (pp->fbs_enabled) {
		if (ap->qc_active) {
			qc_active = readl(port_mmio + PORT_SCR_ACT);
			qc_active |= readl(port_mmio + PORT_CMD_ISSUE);
		}
	} else {
		 
		if (ap->qc_active && pp->active_link->sactive)
			qc_active = readl(port_mmio + PORT_SCR_ACT);
		else
			qc_active = readl(port_mmio + PORT_CMD_ISSUE);
	}

	rc = ata_qc_complete_multiple(ap, qc_active);

	if (unlikely(rc < 0 && !resetting)) {
		ehi->err_mask |= AC_ERR_HSM;
		ehi->action |= ATA_EH_RESET;
		ata_port_freeze(ap);
	}
}

void ahci_port_intr(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 status;

	status = readl(port_mmio + PORT_IRQ_STAT);
	writel(status, port_mmio + PORT_IRQ_STAT);

#if defined(CONFIG_SYNO_LSP_HI3536)
#if defined(CONFIG_ARCH_HI3536) && !defined(CONFIG_SATA_AHCI)
	if (status & PORT_IRQ_CONNECT) {
		switch (ap->port_no) {
		case 0:
			writel(0x19, (void *)IO_ADDRESS(0x12120090));
			writel(0x39, (void *)IO_ADDRESS(0x12120090));
			writel(0x19, (void *)IO_ADDRESS(0x12120090));
			writel(0x0, (void *)IO_ADDRESS(0x12120090));
			break;
		case 1:
			writel(0x19, (void *)IO_ADDRESS(0x12120094));
			writel(0x39, (void *)IO_ADDRESS(0x12120094));
			writel(0x19, (void *)IO_ADDRESS(0x12120094));
			writel(0x0, (void *)IO_ADDRESS(0x12120094));
			break;
		case 2:
			writel(0x39, (void *)IO_ADDRESS(0x12120098));
			writel(0x79, (void *)IO_ADDRESS(0x12120098));
			writel(0x39, (void *)IO_ADDRESS(0x12120098));
			writel(0x0, (void *)IO_ADDRESS(0x12120098));
			break;
		case 3:
			writel(0x19, (void *)IO_ADDRESS(0x12120098));
			writel(0x59, (void *)IO_ADDRESS(0x12120098));
			writel(0x19, (void *)IO_ADDRESS(0x12120098));
			writel(0x0, (void *)IO_ADDRESS(0x12120098));
			break;
		default:
			break;
		}
	}
#endif
#if defined(CONFIG_ARCH_HI3531A) && !defined(CONFIG_SATA_AHCI)
	if (status & PORT_IRQ_CONNECT) {
		switch (ap->port_no) {
		case 0:
			writel(0x39, (void *)IO_ADDRESS(0x12120138));
			writel(0x79, (void *)IO_ADDRESS(0x12120138));
			writel(0x39, (void *)IO_ADDRESS(0x12120138));
			writel(0x0, (void *)IO_ADDRESS(0x12120138));
			break;
		case 1:
			writel(0x19, (void *)IO_ADDRESS(0x12120138));
			writel(0x59, (void *)IO_ADDRESS(0x12120138));
			writel(0x19, (void *)IO_ADDRESS(0x12120138));
			writel(0x0, (void *)IO_ADDRESS(0x12120138));
			break;
		case 2:
			writel(0x39, (void *)IO_ADDRESS(0x12120134));
			writel(0x79, (void *)IO_ADDRESS(0x12120134));
			writel(0x39, (void *)IO_ADDRESS(0x12120134));
			writel(0x0, (void *)IO_ADDRESS(0x12120134));
			break;
		case 3:
			writel(0x19, (void *)IO_ADDRESS(0x12120134));
			writel(0x59, (void *)IO_ADDRESS(0x12120134));
			writel(0x19, (void *)IO_ADDRESS(0x12120134));
			writel(0x0, (void *)IO_ADDRESS(0x12120134));
			break;
		default:
			break;
		}
	}
#endif
#endif  
	ahci_handle_port_interrupt(ap, port_mmio, status);
}

irqreturn_t ahci_thread_fn(int irq, void *dev_instance)
{
	struct ata_port *ap = dev_instance;
	struct ahci_port_priv *pp = ap->private_data;
	void __iomem *port_mmio = ahci_port_base(ap);
	unsigned long flags;
	u32 status;

	spin_lock_irqsave(&ap->host->lock, flags);
	status = pp->intr_status;
	if (status)
		pp->intr_status = 0;
	spin_unlock_irqrestore(&ap->host->lock, flags);

	spin_lock_bh(ap->lock);
	ahci_handle_port_interrupt(ap, port_mmio, status);
	spin_unlock_bh(ap->lock);

	return IRQ_HANDLED;
}
EXPORT_SYMBOL_GPL(ahci_thread_fn);

void ahci_hw_port_interrupt(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	struct ahci_port_priv *pp = ap->private_data;
	u32 status;

	status = readl(port_mmio + PORT_IRQ_STAT);
	writel(status, port_mmio + PORT_IRQ_STAT);

	pp->intr_status |= status;
}

#if defined(MY_DEF_HERE)
EXPORT_SYMBOL_GPL(ahci_hw_port_interrupt);
#endif  

irqreturn_t ahci_hw_interrupt(int irq, void *dev_instance)
{
	struct ata_port *ap_this = dev_instance;
	struct ahci_port_priv *pp = ap_this->private_data;
	struct ata_host *host = ap_this->host;
	struct ahci_host_priv *hpriv = host->private_data;
	void __iomem *mmio = hpriv->mmio;
	unsigned int i;
	u32 irq_stat, irq_masked;

	VPRINTK("ENTER\n");

	spin_lock(&host->lock);

	irq_stat = readl(mmio + HOST_IRQ_STAT);

	if (!irq_stat) {
		u32 status = pp->intr_status;

		spin_unlock(&host->lock);

		VPRINTK("EXIT\n");

		return status ? IRQ_WAKE_THREAD : IRQ_NONE;
	}

	irq_masked = irq_stat & hpriv->port_map;

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap;

#if defined(CONFIG_SYNO_LSP_HI3536) && defined(CONFIG_ARCH_HI3531A)
		ap = host->ports[i];
		if (!(irq_masked & (1 << ap->port_no)))
			continue;
#else  
		if (!(irq_masked & (1 << i)))
			continue;

		ap = host->ports[i];
#endif  
		if (ap) {
			ahci_hw_port_interrupt(ap);
			VPRINTK("port %u\n", i);
		} else {
			VPRINTK("port %u (no irq)\n", i);
			if (ata_ratelimit())
				dev_warn(host->dev,
					 "interrupt on disabled port %u\n", i);
		}
	}

	writel(irq_stat, mmio + HOST_IRQ_STAT);

	spin_unlock(&host->lock);

	VPRINTK("EXIT\n");

	return IRQ_WAKE_THREAD;
}
EXPORT_SYMBOL_GPL(ahci_hw_interrupt);

irqreturn_t ahci_interrupt(int irq, void *dev_instance)
{
	struct ata_host *host = dev_instance;
	struct ahci_host_priv *hpriv;
	unsigned int i, handled = 0;
	void __iomem *mmio;
	u32 irq_stat, irq_masked;

	VPRINTK("ENTER\n");

	hpriv = host->private_data;
	mmio = hpriv->mmio;

	irq_stat = readl(mmio + HOST_IRQ_STAT);
	if (!irq_stat)
		return IRQ_NONE;

	irq_masked = irq_stat & hpriv->port_map;

	spin_lock(&host->lock);

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap;

#if defined(CONFIG_SYNO_LSP_HI3536) && defined(CONFIG_ARCH_HI3531A)
		ap = host->ports[i];
		if (!(irq_masked & (1 << ap->port_no)))
			continue;
#else  
		if (!(irq_masked & (1 << i)))
			continue;

		ap = host->ports[i];
#endif  
		if (ap) {
			ahci_port_intr(ap);
			VPRINTK("port %u\n", i);
		} else {
			VPRINTK("port %u (no irq)\n", i);
			if (ata_ratelimit())
				dev_warn(host->dev,
					 "interrupt on disabled port %u\n", i);
		}

		handled = 1;
	}

	writel(irq_stat, mmio + HOST_IRQ_STAT);

	spin_unlock(&host->lock);

	VPRINTK("EXIT\n");

	return IRQ_RETVAL(handled);
}
EXPORT_SYMBOL_GPL(ahci_interrupt);

static unsigned int ahci_qc_issue(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	void __iomem *port_mmio = ahci_port_base(ap);
	struct ahci_port_priv *pp = ap->private_data;

	pp->active_link = qc->dev->link;

	if (qc->tf.protocol == ATA_PROT_NCQ)
		writel(1 << qc->tag, port_mmio + PORT_SCR_ACT);

	if (pp->fbs_enabled && pp->fbs_last_dev != qc->dev->link->pmp) {
		u32 fbs = readl(port_mmio + PORT_FBS);
		fbs &= ~(PORT_FBS_DEV_MASK | PORT_FBS_DEC);
		fbs |= qc->dev->link->pmp << PORT_FBS_DEV_OFFSET;
		writel(fbs, port_mmio + PORT_FBS);
		pp->fbs_last_dev = qc->dev->link->pmp;
	}

	writel(1 << qc->tag, port_mmio + PORT_CMD_ISSUE);

	ahci_sw_activity(qc->dev->link);

	return 0;
}

static bool ahci_qc_fill_rtf(struct ata_queued_cmd *qc)
{
	struct ahci_port_priv *pp = qc->ap->private_data;
	u8 *rx_fis = pp->rx_fis;

	if (pp->fbs_enabled)
		rx_fis += qc->dev->link->pmp * AHCI_RX_FIS_SZ;

	if (qc->tf.protocol == ATA_PROT_PIO && qc->dma_dir == DMA_FROM_DEVICE &&
	    !(qc->flags & ATA_QCFLAG_FAILED)) {
		ata_tf_from_fis(rx_fis + RX_FIS_PIO_SETUP, &qc->result_tf);
		qc->result_tf.command = (rx_fis + RX_FIS_PIO_SETUP)[15];
	} else
		ata_tf_from_fis(rx_fis + RX_FIS_D2H_REG, &qc->result_tf);

	return true;
}

static void ahci_freeze(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);

	writel(0, port_mmio + PORT_IRQ_MASK);
}

static void ahci_thaw(struct ata_port *ap)
{
	struct ahci_host_priv *hpriv = ap->host->private_data;
	void __iomem *mmio = hpriv->mmio;
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 tmp;
	struct ahci_port_priv *pp = ap->private_data;

	tmp = readl(port_mmio + PORT_IRQ_STAT);
	writel(tmp, port_mmio + PORT_IRQ_STAT);
	writel(1 << ap->port_no, mmio + HOST_IRQ_STAT);

	writel(pp->intr_mask, port_mmio + PORT_IRQ_MASK);
}

static void ahci_error_handler(struct ata_port *ap)
{
	if (!(ap->pflags & ATA_PFLAG_FROZEN)) {
		 
		ahci_stop_engine(ap);
		ahci_start_engine(ap);
	}

	sata_pmp_error_handler(ap);

	if (!ata_dev_enabled(ap->link.device))
		ahci_stop_engine(ap);
}

static void ahci_post_internal_cmd(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;

	if (qc->flags & ATA_QCFLAG_FAILED)
		ahci_kick_engine(ap);
}

static void ahci_set_aggressive_devslp(struct ata_port *ap, bool sleep)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	struct ata_device *dev = ap->link.device;
	u32 devslp, dm, dito, mdat, deto;
	int rc;
	unsigned int err_mask;

	devslp = readl(port_mmio + PORT_DEVSLP);
	if (!(devslp & PORT_DEVSLP_DSP)) {
		dev_err(ap->host->dev, "port does not support device sleep\n");
		return;
	}

	if (!sleep) {
		if (devslp & PORT_DEVSLP_ADSE) {
			writel(devslp & ~PORT_DEVSLP_ADSE,
			       port_mmio + PORT_DEVSLP);
			err_mask = ata_dev_set_feature(dev,
						       SETFEATURES_SATA_DISABLE,
						       SATA_DEVSLP);
			if (err_mask && err_mask != AC_ERR_DEV)
				ata_dev_warn(dev, "failed to disable DEVSLP\n");
		}
		return;
	}

	if (devslp & PORT_DEVSLP_ADSE)
		return;

	rc = ahci_stop_engine(ap);
	if (rc)
		return;

	dm = (devslp & PORT_DEVSLP_DM_MASK) >> PORT_DEVSLP_DM_OFFSET;
	dito = devslp_idle_timeout / (dm + 1);
	if (dito > 0x3ff)
		dito = 0x3ff;

	if (dev->devslp_timing[ATA_LOG_DEVSLP_VALID] &
	    ATA_LOG_DEVSLP_VALID_MASK) {
		mdat = dev->devslp_timing[ATA_LOG_DEVSLP_MDAT] &
		       ATA_LOG_DEVSLP_MDAT_MASK;
		if (!mdat)
			mdat = 10;
		deto = dev->devslp_timing[ATA_LOG_DEVSLP_DETO];
		if (!deto)
			deto = 20;
	} else {
		mdat = 10;
		deto = 20;
	}

	devslp |= ((dito << PORT_DEVSLP_DITO_OFFSET) |
		   (mdat << PORT_DEVSLP_MDAT_OFFSET) |
		   (deto << PORT_DEVSLP_DETO_OFFSET) |
		   PORT_DEVSLP_ADSE);
	writel(devslp, port_mmio + PORT_DEVSLP);

	ahci_start_engine(ap);

	err_mask = ata_dev_set_feature(dev,
				       SETFEATURES_SATA_ENABLE,
				       SATA_DEVSLP);
	if (err_mask && err_mask != AC_ERR_DEV)
		ata_dev_warn(dev, "failed to enable DEVSLP\n");
}

static void ahci_enable_fbs(struct ata_port *ap)
{
	struct ahci_port_priv *pp = ap->private_data;
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 fbs;
	int rc;

	if (!pp->fbs_supported)
		return;

	fbs = readl(port_mmio + PORT_FBS);
	if (fbs & PORT_FBS_EN) {
		pp->fbs_enabled = true;
		pp->fbs_last_dev = -1;  
		return;
	}

	rc = ahci_stop_engine(ap);
	if (rc)
		return;

	writel(fbs | PORT_FBS_EN, port_mmio + PORT_FBS);
	fbs = readl(port_mmio + PORT_FBS);
	if (fbs & PORT_FBS_EN) {
#if defined(CONFIG_SYNO_LSP_HI3536)
#if !defined(CONFIG_ARCH_HI3536) \
	&& !defined(CONFIG_ARCH_HI3521A) \
	&& !defined(CONFIG_ARCH_HI3531A)
		dev_info(ap->host->dev, "FBS is enabled\n");
#endif
#else  
		dev_info(ap->host->dev, "FBS is enabled\n");
#endif  
		pp->fbs_enabled = true;
		pp->fbs_last_dev = -1;  
	} else
		dev_err(ap->host->dev, "Failed to enable FBS\n");

	ahci_start_engine(ap);
}

static void ahci_disable_fbs(struct ata_port *ap)
{
	struct ahci_port_priv *pp = ap->private_data;
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 fbs;
	int rc;

	if (!pp->fbs_supported)
		return;

	fbs = readl(port_mmio + PORT_FBS);
	if ((fbs & PORT_FBS_EN) == 0) {
		pp->fbs_enabled = false;
		return;
	}

	rc = ahci_stop_engine(ap);
	if (rc)
		return;

	writel(fbs & ~PORT_FBS_EN, port_mmio + PORT_FBS);
	fbs = readl(port_mmio + PORT_FBS);
	if (fbs & PORT_FBS_EN)
		dev_err(ap->host->dev, "Failed to disable FBS\n");
	else {
#if defined(CONFIG_SYNO_LSP_HI3536)
#if (defined(CONFIG_ARCH_HI3536) \
	|| defined(CONFIG_ARCH_HI3521A) \
	|| defined(CONFIG_ARCH_HI3531A)) \
	&& !defined(CONFIG_SATA_AHCI)
		dev_info(ap->host->dev, "FBS is disabled\n");
#endif
#else  
		dev_info(ap->host->dev, "FBS is disabled\n");
#endif  
		pp->fbs_enabled = false;
	}

	ahci_start_engine(ap);

#if defined(CONFIG_SYNO_LSP_HI3536)
#if (defined(CONFIG_ARCH_HI3536) \
	|| defined(CONFIG_ARCH_HI3521A) \
	|| defined(CONFIG_ARCH_HI3531A)) \
	&& !defined(CONFIG_SATA_AHCI)
	writel(HI_SATA_FIFOTH_VALUE, (port_mmio + HI_SATA_PORT_FIFOTH));
#endif
#endif  
}

static void ahci_pmp_attach(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	struct ahci_port_priv *pp = ap->private_data;
	u32 cmd;

#if defined(CONFIG_SYNO_LSP_HI3536)
#if (defined(CONFIG_ARCH_HI3536) \
	|| defined(CONFIG_ARCH_HI3521A) \
	|| defined(CONFIG_ARCH_HI3531A)) \
	&& !defined(CONFIG_SATA_AHCI)
	unsigned int port_num = ap->port_no;
#endif
#endif  

	cmd = readl(port_mmio + PORT_CMD);
	cmd |= PORT_CMD_PMP;
	writel(cmd, port_mmio + PORT_CMD);

#if defined(CONFIG_SYNO_LSP_HI3536)
#if (defined(CONFIG_ARCH_HI3536) \
	|| defined(CONFIG_ARCH_HI3521A) \
	|| defined(CONFIG_ARCH_HI3531A)) \
	&& !defined(CONFIG_SATA_AHCI)
	if (fbs_ctrl[port_num].fbs_enable_ctrl)
		ahci_enable_fbs(ap);
	else
		ahci_disable_fbs(ap);
#endif
#else  
	ahci_enable_fbs(ap);
#endif  

	pp->intr_mask |= PORT_IRQ_BAD_PMP;

	if (!(ap->pflags & ATA_PFLAG_FROZEN))
		writel(pp->intr_mask, port_mmio + PORT_IRQ_MASK);
}

static void ahci_pmp_detach(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	struct ahci_port_priv *pp = ap->private_data;
	u32 cmd;

	ahci_disable_fbs(ap);

	cmd = readl(port_mmio + PORT_CMD);
	cmd &= ~PORT_CMD_PMP;
	writel(cmd, port_mmio + PORT_CMD);

	pp->intr_mask &= ~PORT_IRQ_BAD_PMP;

	if (!(ap->pflags & ATA_PFLAG_FROZEN))
		writel(pp->intr_mask, port_mmio + PORT_IRQ_MASK);
}

int ahci_port_resume(struct ata_port *ap)
{
	ahci_power_up(ap);
	ahci_start_port(ap);

	if (sata_pmp_attached(ap))
		ahci_pmp_attach(ap);
	else
		ahci_pmp_detach(ap);

	return 0;
}
EXPORT_SYMBOL_GPL(ahci_port_resume);

#ifdef CONFIG_PM
static int ahci_port_suspend(struct ata_port *ap, pm_message_t mesg)
{
	const char *emsg = NULL;
	int rc;

	rc = ahci_deinit_port(ap, &emsg);
	if (rc == 0)
		ahci_power_down(ap);
	else {
		ata_port_err(ap, "%s (%d)\n", emsg, rc);
		ata_port_freeze(ap);
	}

	return rc;
}
#endif

#if defined(CONFIG_SYNO_LSP_HI3536)
#if (defined(CONFIG_ARCH_HI3536) \
	|| defined(CONFIG_ARCH_HI3521A) \
	|| defined(CONFIG_ARCH_HI3531A)) \
	&& !defined(CONFIG_SATA_AHCI)
static void ahci_poll_func(unsigned long arg)
{
	struct ata_port *ap = (struct ata_port *)arg;
	unsigned int port_num = ap->port_no;

	if (ap->link.pmp == SATA_PMP_CTRL_PORT) {
		fbs_ctrl[port_num].fbs_enable_flag = 1;
		fbs_ctrl[port_num].fbs_disable_flag = 0;
	}
}
#endif

#ifdef CONFIG_HI_SATA_RAM
extern unsigned int sg_num;
#endif
#endif  

static int ahci_port_start(struct ata_port *ap)
{
	struct ahci_host_priv *hpriv = ap->host->private_data;
	struct device *dev = ap->host->dev;
	struct ahci_port_priv *pp;
	void *mem;
	dma_addr_t mem_dma;
	size_t dma_sz, rx_fis_sz;
#if defined(CONFIG_SYNO_LSP_HI3536)
#ifdef CONFIG_HI_SATA_RAM
	unsigned int ram_addr_phy1, ram_addr_phy2, cmd_slot_ram, cmd_tbl_ram;
#endif
#endif  

	pp = devm_kzalloc(dev, sizeof(*pp), GFP_KERNEL);
	if (!pp)
		return -ENOMEM;

	if ((hpriv->cap & HOST_CAP_FBS) && sata_pmp_supported(ap)) {
		void __iomem *port_mmio = ahci_port_base(ap);
		u32 cmd = readl(port_mmio + PORT_CMD);
		if (cmd & PORT_CMD_FBSCP)
			pp->fbs_supported = true;
		else if (hpriv->flags & AHCI_HFLAG_YES_FBS) {
			dev_info(dev, "port %d can do FBS, forcing FBSCP\n",
				 ap->port_no);
			pp->fbs_supported = true;
		} else
			dev_warn(dev, "port %d is not capable of FBS\n",
				 ap->port_no);
	}

	if (pp->fbs_supported) {
#if defined(CONFIG_SYNO_LSP_HI3536) && defined(CONFIG_HI_SATA_RAM)
		dma_sz = AHCI_PORT_PRIV_FBS_DMA_SZ
				+ sg_num * 16 * AHCI_MAX_CMDS;
#else  
		dma_sz = AHCI_PORT_PRIV_FBS_DMA_SZ;
#endif  
		rx_fis_sz = AHCI_RX_FIS_SZ * 16;
	} else {
#if defined(CONFIG_SYNO_LSP_HI3536) && defined(CONFIG_HI_SATA_RAM)
		dma_sz = AHCI_PORT_PRIV_DMA_SZ
				+ sg_num * 16 * AHCI_MAX_CMDS;
#else  
		dma_sz = AHCI_PORT_PRIV_DMA_SZ;
#endif  
		rx_fis_sz = AHCI_RX_FIS_SZ;
	}

	mem = dmam_alloc_coherent(dev, dma_sz, &mem_dma, GFP_KERNEL);
	if (!mem)
		return -ENOMEM;
	memset(mem, 0, dma_sz);

#if defined(CONFIG_SYNO_LSP_HI3536) && defined(CONFIG_HI_SATA_RAM)
	ram_addr_phy1 = CONFIG_HI_SATA_RAM_CMD_BASE
			+ (ap->port_no * CONFIG_HI_SATA_RAM_CMD_IO_SIZE);
	cmd_slot_ram = (unsigned int)ioremap(ram_addr_phy1,
					CONFIG_HI_SATA_RAM_CMD_IO_SIZE);

	memset((void *)cmd_slot_ram, 0, CONFIG_HI_SATA_RAM_CMD_IO_SIZE);

	ram_addr_phy2 = CONFIG_HI_SATA_RAM_PRDT_BASE
			+ (ap->port_no * CONFIG_HI_SATA_RAM_PRDT_IO_SIZE);
	cmd_tbl_ram = (unsigned int)ioremap(ram_addr_phy2,
					CONFIG_HI_SATA_RAM_PRDT_IO_SIZE);

	memset((void *)cmd_tbl_ram, 0, CONFIG_HI_SATA_RAM_PRDT_IO_SIZE);

	pp->cmd_slot = (struct ahci_cmd_hdr *)cmd_slot_ram;
	pp->cmd_slot_dma = ram_addr_phy1;

	pp->rx_fis = mem;
	pp->rx_fis_dma = mem_dma;

	pp->cmd_tbl = (void *)cmd_tbl_ram;
	pp->cmd_tbl_dma = ram_addr_phy2;
#else  
	 
	pp->cmd_slot = mem;
	pp->cmd_slot_dma = mem_dma;

	mem += AHCI_CMD_SLOT_SZ;
	mem_dma += AHCI_CMD_SLOT_SZ;

	pp->rx_fis = mem;
	pp->rx_fis_dma = mem_dma;

	mem += rx_fis_sz;
	mem_dma += rx_fis_sz;

	pp->cmd_tbl = mem;
	pp->cmd_tbl_dma = mem_dma;
#endif  

	pp->intr_mask = DEF_PORT_IRQ;

	if ((hpriv->flags & AHCI_HFLAG_MULTI_MSI)) {
		spin_lock_init(&pp->lock);
		ap->lock = &pp->lock;
	}

	ap->private_data = pp;

#if defined(CONFIG_SYNO_LSP_HI3536)
#if (defined(CONFIG_ARCH_HI3536) \
	|| defined(CONFIG_ARCH_HI3521A) \
	|| defined(CONFIG_ARCH_HI3531A)) \
	&& !defined(CONFIG_SATA_AHCI)
	fbs_ctrl[ap->port_no].fbs_enable_ctrl = fbs_en;
	fbs_ctrl[ap->port_no].fbs_enable_flag = 0;
	fbs_ctrl[ap->port_no].fbs_disable_flag = 0;
	fbs_ctrl[ap->port_no].fbs_cmd_issue_flag = 0;

	init_timer(&fbs_ctrl[ap->port_no].poll_timer);
	fbs_ctrl[ap->port_no].poll_timer.function = ahci_poll_func;
	fbs_ctrl[ap->port_no].poll_timer.data = (unsigned long)ap;
	fbs_ctrl[ap->port_no].poll_timer.expires = jiffies + AHCI_POLL_TIMER;
#endif
#endif  

	return ahci_port_resume(ap);
}

static void ahci_port_stop(struct ata_port *ap)
{
	const char *emsg = NULL;
	int rc;

	rc = ahci_deinit_port(ap, &emsg);
	if (rc)
		ata_port_warn(ap, "%s (%d)\n", emsg, rc);
}

void ahci_print_info(struct ata_host *host, const char *scc_s)
{
	struct ahci_host_priv *hpriv = host->private_data;
	void __iomem *mmio = hpriv->mmio;
	u32 vers, cap, cap2, impl, speed;
	const char *speed_s;

	vers = readl(mmio + HOST_VERSION);
	cap = hpriv->cap;
	cap2 = hpriv->cap2;
	impl = hpriv->port_map;

	speed = (cap >> 20) & 0xf;
	if (speed == 1)
		speed_s = "1.5";
	else if (speed == 2)
		speed_s = "3";
	else if (speed == 3)
		speed_s = "6";
	else
		speed_s = "?";

	dev_info(host->dev,
		"AHCI %02x%02x.%02x%02x "
		"%u slots %u ports %s Gbps 0x%x impl %s mode\n"
		,

		(vers >> 24) & 0xff,
		(vers >> 16) & 0xff,
		(vers >> 8) & 0xff,
		vers & 0xff,

		((cap >> 8) & 0x1f) + 1,
		(cap & 0x1f) + 1,
		speed_s,
		impl,
		scc_s);

	dev_info(host->dev,
		"flags: "
		"%s%s%s%s%s%s%s"
		"%s%s%s%s%s%s%s"
		"%s%s%s%s%s%s%s"
		"%s%s\n"
		,

		cap & HOST_CAP_64 ? "64bit " : "",
		cap & HOST_CAP_NCQ ? "ncq " : "",
		cap & HOST_CAP_SNTF ? "sntf " : "",
		cap & HOST_CAP_MPS ? "ilck " : "",
		cap & HOST_CAP_SSS ? "stag " : "",
		cap & HOST_CAP_ALPM ? "pm " : "",
		cap & HOST_CAP_LED ? "led " : "",
		cap & HOST_CAP_CLO ? "clo " : "",
		cap & HOST_CAP_ONLY ? "only " : "",
		cap & HOST_CAP_PMP ? "pmp " : "",
		cap & HOST_CAP_FBS ? "fbs " : "",
		cap & HOST_CAP_PIO_MULTI ? "pio " : "",
		cap & HOST_CAP_SSC ? "slum " : "",
		cap & HOST_CAP_PART ? "part " : "",
		cap & HOST_CAP_CCC ? "ccc " : "",
		cap & HOST_CAP_EMS ? "ems " : "",
		cap & HOST_CAP_SXS ? "sxs " : "",
		cap2 & HOST_CAP2_DESO ? "deso " : "",
		cap2 & HOST_CAP2_SADM ? "sadm " : "",
		cap2 & HOST_CAP2_SDS ? "sds " : "",
		cap2 & HOST_CAP2_APST ? "apst " : "",
		cap2 & HOST_CAP2_NVMHCI ? "nvmp " : "",
		cap2 & HOST_CAP2_BOH ? "boh " : ""
		);
}
EXPORT_SYMBOL_GPL(ahci_print_info);

void ahci_set_em_messages(struct ahci_host_priv *hpriv,
			  struct ata_port_info *pi)
{
	u8 messages;
	void __iomem *mmio = hpriv->mmio;
	u32 em_loc = readl(mmio + HOST_EM_LOC);
	u32 em_ctl = readl(mmio + HOST_EM_CTL);

	if (!ahci_em_messages || !(hpriv->cap & HOST_CAP_EMS))
		return;

	messages = (em_ctl & EM_CTRL_MSG_TYPE) >> 16;

	if (messages) {
		 
		hpriv->em_loc = ((em_loc >> 16) * 4);
		hpriv->em_buf_sz = ((em_loc & 0xff) * 4);
		hpriv->em_msg_type = messages;
		pi->flags |= ATA_FLAG_EM;
		if (!(em_ctl & EM_CTL_ALHD))
			pi->flags |= ATA_FLAG_SW_ACTIVITY;
#ifdef MY_DEF_HERE
		if (em_ctl & EM_CTL_LED) {
			pi->flags |= ATA_FLAG_SW_LOCATE;
			pi->flags |= ATA_FLAG_SW_FAULT;
		}
#endif  
	}
}
EXPORT_SYMBOL_GPL(ahci_set_em_messages);

MODULE_AUTHOR("Jeff Garzik");
MODULE_DESCRIPTION("Common AHCI SATA low-level routines");
MODULE_LICENSE("GPL");
