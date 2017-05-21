#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/mdio.h>

#include <linux/atomic.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#ifdef MY_DEF_HERE
#include <linux/synobios.h>
#endif

void phy_print_status(struct phy_device *phydev)
{
	if (phydev->link)
		pr_info("%s - Link is Up - %d/%s\n",
			dev_name(&phydev->dev),
			phydev->speed,
			DUPLEX_FULL == phydev->duplex ? "Full" : "Half");
	else
		pr_info("%s - Link is Down\n", dev_name(&phydev->dev));
}
EXPORT_SYMBOL(phy_print_status);

static int phy_clear_interrupt(struct phy_device *phydev)
{
	int err = 0;

	if (phydev->drv->ack_interrupt)
		err = phydev->drv->ack_interrupt(phydev);

	return err;
}

static int phy_config_interrupt(struct phy_device *phydev, u32 interrupts)
{
	int err = 0;

	phydev->interrupts = interrupts;
	if (phydev->drv->config_intr)
		err = phydev->drv->config_intr(phydev);

	return err;
}

static inline int phy_aneg_done(struct phy_device *phydev)
{
	int retval;

	retval = phy_read(phydev, MII_BMSR);

	return (retval < 0) ? retval : (retval & BMSR_ANEGCOMPLETE);
}

struct phy_setting {
	int speed;
	int duplex;
	u32 setting;
};

static const struct phy_setting settings[] = {
	{
		.speed = 10000,
		.duplex = DUPLEX_FULL,
		.setting = SUPPORTED_10000baseT_Full,
	},
	{
		.speed = SPEED_1000,
		.duplex = DUPLEX_FULL,
		.setting = SUPPORTED_1000baseT_Full,
	},
	{
		.speed = SPEED_1000,
		.duplex = DUPLEX_HALF,
		.setting = SUPPORTED_1000baseT_Half,
	},
	{
		.speed = SPEED_100,
		.duplex = DUPLEX_FULL,
		.setting = SUPPORTED_100baseT_Full,
	},
	{
		.speed = SPEED_100,
		.duplex = DUPLEX_HALF,
		.setting = SUPPORTED_100baseT_Half,
	},
	{
		.speed = SPEED_10,
		.duplex = DUPLEX_FULL,
		.setting = SUPPORTED_10baseT_Full,
	},
	{
		.speed = SPEED_10,
		.duplex = DUPLEX_HALF,
		.setting = SUPPORTED_10baseT_Half,
	},
};

#define MAX_NUM_SETTINGS ARRAY_SIZE(settings)

static inline int phy_find_setting(int speed, int duplex)
{
	int idx = 0;

	while (idx < ARRAY_SIZE(settings) &&
			(settings[idx].speed != speed ||
			settings[idx].duplex != duplex))
		idx++;

	return idx < MAX_NUM_SETTINGS ? idx : MAX_NUM_SETTINGS - 1;
}

static inline int phy_find_valid(int idx, u32 features)
{
	while (idx < MAX_NUM_SETTINGS && !(settings[idx].setting & features))
		idx++;

	return idx < MAX_NUM_SETTINGS ? idx : MAX_NUM_SETTINGS - 1;
}

static inline bool phy_check_valid(int speed, int duplex, u32 features)
{
	unsigned int idx;

	idx = phy_find_valid(phy_find_setting(speed, duplex), features);

	return settings[idx].speed == speed && settings[idx].duplex == duplex &&
		(settings[idx].setting & features);
}

static void phy_sanitize_settings(struct phy_device *phydev)
{
	u32 features = phydev->supported;
	int idx;

	if ((features & SUPPORTED_Autoneg) == 0)
		phydev->autoneg = AUTONEG_DISABLE;

	idx = phy_find_valid(phy_find_setting(phydev->speed, phydev->duplex),
			features);

	phydev->speed = settings[idx].speed;
	phydev->duplex = settings[idx].duplex;
}

int phy_ethtool_sset(struct phy_device *phydev, struct ethtool_cmd *cmd)
{
	u32 speed = ethtool_cmd_speed(cmd);

	if (cmd->phy_address != phydev->addr)
		return -EINVAL;

	cmd->advertising &= phydev->supported;

	if (cmd->autoneg != AUTONEG_ENABLE && cmd->autoneg != AUTONEG_DISABLE)
		return -EINVAL;

	if (cmd->autoneg == AUTONEG_ENABLE && cmd->advertising == 0)
		return -EINVAL;

	if (cmd->autoneg == AUTONEG_DISABLE &&
	    ((speed != SPEED_1000 &&
	      speed != SPEED_100 &&
	      speed != SPEED_10) ||
	     (cmd->duplex != DUPLEX_HALF &&
	      cmd->duplex != DUPLEX_FULL)))
		return -EINVAL;

	phydev->autoneg = cmd->autoneg;

	phydev->speed = speed;

	phydev->advertising = cmd->advertising;

	if (AUTONEG_ENABLE == cmd->autoneg)
		phydev->advertising |= ADVERTISED_Autoneg;
	else
		phydev->advertising &= ~ADVERTISED_Autoneg;

	phydev->duplex = cmd->duplex;

	phy_start_aneg(phydev);

	return 0;
}
EXPORT_SYMBOL(phy_ethtool_sset);

int phy_ethtool_gset(struct phy_device *phydev, struct ethtool_cmd *cmd)
{
	cmd->supported = phydev->supported;

	cmd->advertising = phydev->advertising;

	ethtool_cmd_speed_set(cmd, phydev->speed);
	cmd->duplex = phydev->duplex;
	cmd->port = PORT_MII;
	cmd->phy_address = phydev->addr;
	cmd->transceiver = XCVR_EXTERNAL;
	cmd->autoneg = phydev->autoneg;

	return 0;
}
EXPORT_SYMBOL(phy_ethtool_gset);

int phy_mii_ioctl(struct phy_device *phydev,
		struct ifreq *ifr, int cmd)
{
	struct mii_ioctl_data *mii_data = if_mii(ifr);
	u16 val = mii_data->val_in;

	switch (cmd) {
	case SIOCGMIIPHY:
		mii_data->phy_id = phydev->addr;
		 
	case SIOCGMIIREG:
		mii_data->val_out = mdiobus_read(phydev->bus, mii_data->phy_id,
						 mii_data->reg_num);
		break;

	case SIOCSMIIREG:
		if (mii_data->phy_id == phydev->addr) {
			switch(mii_data->reg_num) {
			case MII_BMCR:
				if ((val & (BMCR_RESET|BMCR_ANENABLE)) == 0)
					phydev->autoneg = AUTONEG_DISABLE;
				else
					phydev->autoneg = AUTONEG_ENABLE;
				if ((!phydev->autoneg) && (val & BMCR_FULLDPLX))
					phydev->duplex = DUPLEX_FULL;
				else
					phydev->duplex = DUPLEX_HALF;
				if ((!phydev->autoneg) &&
						(val & BMCR_SPEED1000))
					phydev->speed = SPEED_1000;
				else if ((!phydev->autoneg) &&
						(val & BMCR_SPEED100))
					phydev->speed = SPEED_100;
				break;
			case MII_ADVERTISE:
				phydev->advertising = val;
				break;
			default:
				 
				break;
			}
		}

		mdiobus_write(phydev->bus, mii_data->phy_id,
			      mii_data->reg_num, val);

		if (mii_data->reg_num == MII_BMCR &&
		    val & BMCR_RESET &&
		    phydev->drv->config_init) {
			phy_scan_fixups(phydev);
			phydev->drv->config_init(phydev);
		}
		break;

	case SIOCSHWTSTAMP:
		if (phydev->drv->hwtstamp)
			return phydev->drv->hwtstamp(phydev, ifr);
		 
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}
EXPORT_SYMBOL(phy_mii_ioctl);

int phy_start_aneg(struct phy_device *phydev)
{
	int err;

	mutex_lock(&phydev->lock);

	if (AUTONEG_DISABLE == phydev->autoneg)
		phy_sanitize_settings(phydev);

	err = phydev->drv->config_aneg(phydev);

	if (err < 0)
		goto out_unlock;

	if (phydev->state != PHY_HALTED) {
		if (AUTONEG_ENABLE == phydev->autoneg) {
			phydev->state = PHY_AN;
			phydev->link_timeout = PHY_AN_TIMEOUT;
		} else {
			phydev->state = PHY_FORCING;
			phydev->link_timeout = PHY_FORCE_TIMEOUT;
		}
	}

out_unlock:
	mutex_unlock(&phydev->lock);
	return err;
}
EXPORT_SYMBOL(phy_start_aneg);

static void phy_change(struct work_struct *work);

void phy_start_machine(struct phy_device *phydev,
		void (*handler)(struct net_device *))
{
	phydev->adjust_state = handler;

	schedule_delayed_work(&phydev->state_queue, HZ);
}

void phy_stop_machine(struct phy_device *phydev)
{
	cancel_delayed_work_sync(&phydev->state_queue);

	mutex_lock(&phydev->lock);
	if (phydev->state > PHY_UP)
		phydev->state = PHY_UP;
	mutex_unlock(&phydev->lock);

	phydev->adjust_state = NULL;
}

static void phy_error(struct phy_device *phydev)
{
	mutex_lock(&phydev->lock);
	phydev->state = PHY_HALTED;
	mutex_unlock(&phydev->lock);
}

static irqreturn_t phy_interrupt(int irq, void *phy_dat)
{
	struct phy_device *phydev = phy_dat;

	if (PHY_HALTED == phydev->state)
		return IRQ_NONE;		 

	disable_irq_nosync(irq);
	atomic_inc(&phydev->irq_disable);

	schedule_work(&phydev->phy_queue);

	return IRQ_HANDLED;
}

static int phy_enable_interrupts(struct phy_device *phydev)
{
	int err;

	err = phy_clear_interrupt(phydev);

	if (err < 0)
		return err;

	err = phy_config_interrupt(phydev, PHY_INTERRUPT_ENABLED);

	return err;
}

static int phy_disable_interrupts(struct phy_device *phydev)
{
	int err;

	err = phy_config_interrupt(phydev, PHY_INTERRUPT_DISABLED);

	if (err)
		goto phy_err;

	err = phy_clear_interrupt(phydev);

	if (err)
		goto phy_err;

	return 0;

phy_err:
	phy_error(phydev);

	return err;
}

int phy_start_interrupts(struct phy_device *phydev)
{
	int err = 0;

	INIT_WORK(&phydev->phy_queue, phy_change);

	atomic_set(&phydev->irq_disable, 0);
	if (request_irq(phydev->irq, phy_interrupt,
				IRQF_SHARED,
				"phy_interrupt",
				phydev) < 0) {
		pr_warn("%s: Can't get IRQ %d (PHY)\n",
			phydev->bus->name, phydev->irq);
		phydev->irq = PHY_POLL;
		return 0;
	}

	err = phy_enable_interrupts(phydev);

	return err;
}
EXPORT_SYMBOL(phy_start_interrupts);

int phy_stop_interrupts(struct phy_device *phydev)
{
	int err;

	err = phy_disable_interrupts(phydev);

	if (err)
		phy_error(phydev);

	free_irq(phydev->irq, phydev);

	cancel_work_sync(&phydev->phy_queue);
	 
	while (atomic_dec_return(&phydev->irq_disable) >= 0)
		enable_irq(phydev->irq);

	return err;
}
EXPORT_SYMBOL(phy_stop_interrupts);

static void phy_change(struct work_struct *work)
{
	int err;
	struct phy_device *phydev =
		container_of(work, struct phy_device, phy_queue);

	if (phydev->drv->did_interrupt &&
	    !phydev->drv->did_interrupt(phydev))
		goto ignore;

	err = phy_disable_interrupts(phydev);

	if (err)
		goto phy_err;

	mutex_lock(&phydev->lock);
	if ((PHY_RUNNING == phydev->state) || (PHY_NOLINK == phydev->state))
		phydev->state = PHY_CHANGELINK;
	mutex_unlock(&phydev->lock);

	atomic_dec(&phydev->irq_disable);
	enable_irq(phydev->irq);

	if (PHY_HALTED != phydev->state)
		err = phy_config_interrupt(phydev, PHY_INTERRUPT_ENABLED);

	if (err)
		goto irq_enable_err;

	cancel_delayed_work_sync(&phydev->state_queue);
	schedule_delayed_work(&phydev->state_queue, 0);

	return;

ignore:
	atomic_dec(&phydev->irq_disable);
	enable_irq(phydev->irq);
	return;

irq_enable_err:
	disable_irq(phydev->irq);
	atomic_inc(&phydev->irq_disable);
phy_err:
	phy_error(phydev);
}

void phy_stop(struct phy_device *phydev)
{
	mutex_lock(&phydev->lock);

	if (PHY_HALTED == phydev->state)
		goto out_unlock;

	if (phydev->irq != PHY_POLL) {
		 
		phy_config_interrupt(phydev, PHY_INTERRUPT_DISABLED);

		phy_clear_interrupt(phydev);
	}

	phydev->state = PHY_HALTED;

out_unlock:
	mutex_unlock(&phydev->lock);

}

void phy_start(struct phy_device *phydev)
{
	mutex_lock(&phydev->lock);

	switch (phydev->state) {
		case PHY_STARTING:
			phydev->state = PHY_PENDING;
			break;
		case PHY_READY:
			phydev->state = PHY_UP;
			break;
		case PHY_HALTED:
			phydev->state = PHY_RESUMING;
		default:
			break;
	}
	mutex_unlock(&phydev->lock);
}
EXPORT_SYMBOL(phy_stop);
EXPORT_SYMBOL(phy_start);

void phy_state_machine(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct phy_device *phydev =
			container_of(dwork, struct phy_device, state_queue);
	int needs_aneg = 0;
	int err = 0;
#ifdef MY_DEF_HERE
	struct rtnl_link_stats64 temp;
	const struct rtnl_link_stats64 *stats = NULL;
	int reg_val = 0;
	unsigned long pkt_error = 0, pkt_frame = 0;
#endif

	mutex_lock(&phydev->lock);

	if (phydev->adjust_state)
		phydev->adjust_state(phydev->attached_dev);

	switch(phydev->state) {
		case PHY_DOWN:
		case PHY_STARTING:
		case PHY_READY:
		case PHY_PENDING:
			break;
		case PHY_UP:
			needs_aneg = 1;

			phydev->link_timeout = PHY_AN_TIMEOUT;

			break;
		case PHY_AN:
			err = phy_read_status(phydev);

			if (err < 0)
				break;

			if (!phydev->link) {
				phydev->state = PHY_NOLINK;
				netif_carrier_off(phydev->attached_dev);
				phydev->adjust_link(phydev->attached_dev);
				break;
			}

			err = phy_aneg_done(phydev);
			if (err < 0)
				break;

			if (err > 0) {
				phydev->state = PHY_RUNNING;
				netif_carrier_on(phydev->attached_dev);
				phydev->adjust_link(phydev->attached_dev);

			} else if (0 == phydev->link_timeout--) {
				needs_aneg = 1;
				 
				if (phydev->drv->flags & PHY_HAS_MAGICANEG)
					break;
			}
			break;
		case PHY_NOLINK:
			err = phy_read_status(phydev);

			if (err)
				break;

			if (phydev->link) {
				phydev->state = PHY_RUNNING;
				netif_carrier_on(phydev->attached_dev);
				phydev->adjust_link(phydev->attached_dev);
			}
			break;
		case PHY_FORCING:
			err = genphy_update_link(phydev);

			if (err)
				break;

			if (phydev->link) {
				phydev->state = PHY_RUNNING;
				netif_carrier_on(phydev->attached_dev);
			} else {
				if (0 == phydev->link_timeout--)
					needs_aneg = 1;
			}

			phydev->adjust_link(phydev->attached_dev);
			break;
		case PHY_RUNNING:
			 
			if (PHY_POLL == phydev->irq)
				phydev->state = PHY_CHANGELINK;

#ifdef MY_DEF_HERE
			if (!syno_is_hw_version(HW_DS2015xs)) {
				if (0 == phydev->is_phyerr_reset) {
					stats = dev_get_stats(phydev->attached_dev, &temp);
					pkt_error = stats->rx_errors;
					pkt_frame = stats->rx_length_errors + stats->rx_over_errors +
						stats->rx_crc_errors + stats->rx_frame_errors;
					if (pkt_error > 100 || pkt_frame > 100) {
						printk(KERN_ERR "Phy error! Try to reset phy for next successful connection\n");
						phy_write(phydev, 31, 0x0);
						reg_val = phy_read(phydev, MII_BMCR);
						reg_val |= BMCR_RESET;
						phy_write(phydev, MII_BMCR, reg_val);
						mdelay(20);
						phydev->is_phyerr_reset = 1;
					}
				}
			}
#endif
			break;
		case PHY_CHANGELINK:
			err = phy_read_status(phydev);

			if (err)
				break;

			if (phydev->link) {
				phydev->state = PHY_RUNNING;
				netif_carrier_on(phydev->attached_dev);
			} else {
				phydev->state = PHY_NOLINK;
				netif_carrier_off(phydev->attached_dev);
			}

			phydev->adjust_link(phydev->attached_dev);

			if (PHY_POLL != phydev->irq)
				err = phy_config_interrupt(phydev,
						PHY_INTERRUPT_ENABLED);
			break;
		case PHY_HALTED:
			if (phydev->link) {
				phydev->link = 0;
				netif_carrier_off(phydev->attached_dev);
				phydev->adjust_link(phydev->attached_dev);
			}
			break;
		case PHY_RESUMING:

			err = phy_clear_interrupt(phydev);

			if (err)
				break;

			err = phy_config_interrupt(phydev,
					PHY_INTERRUPT_ENABLED);

			if (err)
				break;

			if (AUTONEG_ENABLE == phydev->autoneg) {
				err = phy_aneg_done(phydev);
				if (err < 0)
					break;

				if (err > 0) {
					err = phy_read_status(phydev);
					if (err)
						break;

					if (phydev->link) {
						phydev->state = PHY_RUNNING;
						netif_carrier_on(phydev->attached_dev);
					} else
						phydev->state = PHY_NOLINK;
					phydev->adjust_link(phydev->attached_dev);
				} else {
					phydev->state = PHY_AN;
					phydev->link_timeout = PHY_AN_TIMEOUT;
				}
			} else {
				err = phy_read_status(phydev);
				if (err)
					break;

				if (phydev->link) {
					phydev->state = PHY_RUNNING;
					netif_carrier_on(phydev->attached_dev);
				} else
					phydev->state = PHY_NOLINK;
				phydev->adjust_link(phydev->attached_dev);
			}
			break;
	}

	mutex_unlock(&phydev->lock);

	if (needs_aneg)
		err = phy_start_aneg(phydev);

	if (err < 0)
		phy_error(phydev);

	schedule_delayed_work(&phydev->state_queue, PHY_STATE_TIME * HZ);
}

static inline void mmd_phy_indirect(struct mii_bus *bus, int prtad, int devad,
				    int addr)
{
	 
	bus->write(bus, addr, MII_MMD_CTRL, devad);

	bus->write(bus, addr, MII_MMD_DATA, prtad);

	bus->write(bus, addr, MII_MMD_CTRL, (devad | MII_MMD_CTRL_NOINCR));
}

static int phy_read_mmd_indirect(struct mii_bus *bus, int prtad, int devad,
				 int addr)
{
	u32 ret;

	mmd_phy_indirect(bus, prtad, devad, addr);

	ret = bus->read(bus, addr, MII_MMD_DATA);

	return ret;
}

static void phy_write_mmd_indirect(struct mii_bus *bus, int prtad, int devad,
				   int addr, u32 data)
{
	mmd_phy_indirect(bus, prtad, devad, addr);

	bus->write(bus, addr, MII_MMD_DATA, data);
}

int phy_init_eee(struct phy_device *phydev, bool clk_stop_enable)
{
	int ret = -EPROTONOSUPPORT;

	if ((phydev->duplex == DUPLEX_FULL) &&
	    ((phydev->interface == PHY_INTERFACE_MODE_MII) ||
	    (phydev->interface == PHY_INTERFACE_MODE_GMII) ||
	     (phydev->interface >= PHY_INTERFACE_MODE_RGMII &&
	      phydev->interface <= PHY_INTERFACE_MODE_RGMII_TXID))) {
		int eee_lp, eee_cap, eee_adv;
		u32 lp, cap, adv;
		int status;

		status = phy_read_status(phydev);
		if (status)
			return status;

		eee_cap = phy_read_mmd_indirect(phydev->bus, MDIO_PCS_EEE_ABLE,
						MDIO_MMD_PCS, phydev->addr);
		if (eee_cap < 0)
			return eee_cap;

		cap = mmd_eee_cap_to_ethtool_sup_t(eee_cap);
		if (!cap)
			goto eee_exit;

		eee_lp = phy_read_mmd_indirect(phydev->bus, MDIO_AN_EEE_LPABLE,
					       MDIO_MMD_AN, phydev->addr);
		if (eee_lp < 0)
			return eee_lp;

		eee_adv = phy_read_mmd_indirect(phydev->bus, MDIO_AN_EEE_ADV,
						MDIO_MMD_AN, phydev->addr);
		if (eee_adv < 0)
			return eee_adv;

		adv = mmd_eee_adv_to_ethtool_adv_t(eee_adv);
		lp = mmd_eee_adv_to_ethtool_adv_t(eee_lp);
		if (!phy_check_valid(phydev->speed, phydev->duplex, lp & adv))
			goto eee_exit;

		if (clk_stop_enable) {
			 
			int val = phy_read_mmd_indirect(phydev->bus, MDIO_CTRL1,
							MDIO_MMD_PCS,
							phydev->addr);
			if (val < 0)
				return val;

			val |= MDIO_PCS_CTRL1_CLKSTOP_EN;
			phy_write_mmd_indirect(phydev->bus, MDIO_CTRL1,
					       MDIO_MMD_PCS, phydev->addr, val);
		}

		ret = 0;  
	}

eee_exit:
	return ret;
}
EXPORT_SYMBOL(phy_init_eee);

int phy_get_eee_err(struct phy_device *phydev)
{
	return phy_read_mmd_indirect(phydev->bus, MDIO_PCS_EEE_WK_ERR,
				     MDIO_MMD_PCS, phydev->addr);

}
EXPORT_SYMBOL(phy_get_eee_err);

int phy_ethtool_get_eee(struct phy_device *phydev, struct ethtool_eee *data)
{
	int val;

	val = phy_read_mmd_indirect(phydev->bus, MDIO_PCS_EEE_ABLE,
				    MDIO_MMD_PCS, phydev->addr);
	if (val < 0)
		return val;
	data->supported = mmd_eee_cap_to_ethtool_sup_t(val);

	val = phy_read_mmd_indirect(phydev->bus, MDIO_AN_EEE_ADV,
				    MDIO_MMD_AN, phydev->addr);
	if (val < 0)
		return val;
	data->advertised = mmd_eee_adv_to_ethtool_adv_t(val);

	val = phy_read_mmd_indirect(phydev->bus, MDIO_AN_EEE_LPABLE,
				    MDIO_MMD_AN, phydev->addr);
	if (val < 0)
		return val;
	data->lp_advertised = mmd_eee_adv_to_ethtool_adv_t(val);

	return 0;
}
EXPORT_SYMBOL(phy_ethtool_get_eee);

int phy_ethtool_set_eee(struct phy_device *phydev, struct ethtool_eee *data)
{
	int val;

	val = ethtool_adv_to_mmd_eee_adv_t(data->advertised);
	phy_write_mmd_indirect(phydev->bus, MDIO_AN_EEE_ADV, MDIO_MMD_AN,
			       phydev->addr, val);

	return 0;
}
EXPORT_SYMBOL(phy_ethtool_set_eee);

int phy_ethtool_set_wol(struct phy_device *phydev, struct ethtool_wolinfo *wol)
{
	if (phydev->drv->set_wol)
		return phydev->drv->set_wol(phydev, wol);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(phy_ethtool_set_wol);

void phy_ethtool_get_wol(struct phy_device *phydev, struct ethtool_wolinfo *wol)
{
	if (phydev->drv->get_wol)
		phydev->drv->get_wol(phydev, wol);
}
EXPORT_SYMBOL(phy_ethtool_get_wol);
