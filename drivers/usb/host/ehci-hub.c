#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/usb/otg.h>

#define	PORT_WAKE_BITS	(PORT_WKOC_E|PORT_WKDISC_E|PORT_WKCONN_E)

#ifdef	CONFIG_PM

static int ehci_hub_control(
	struct usb_hcd	*hcd,
	u16		typeReq,
	u16		wValue,
	u16		wIndex,
	char		*buf,
	u16		wLength
);

static void ehci_handover_companion_ports(struct ehci_hcd *ehci)
{
	u32 __iomem	*reg;
	u32		status;
	int		port;
	__le32		buf;
	struct usb_hcd	*hcd = ehci_to_hcd(ehci);

	if (!ehci->owned_ports)
		return;

	port = HCS_N_PORTS(ehci->hcs_params);
	while (port--) {
		if (test_bit(port, &ehci->owned_ports)) {
			reg = &ehci->regs->port_status[port];
			status = ehci_readl(ehci, reg) & ~PORT_RWC_BITS;
			if (!(status & PORT_POWER)) {
				status |= PORT_POWER;
				ehci_writel(ehci, status, reg);
			}
		}
	}

#ifdef MY_DEF_HERE
	port = HCS_N_PORTS(ehci->hcs_params);
	while (port--) {
		if (test_bit(port, &ehci->owned_ports)) {
			reg = &ehci->regs->port_status[port];
			status = handshake(ehci, reg, PORT_CONNECT,
					   PORT_CONNECT, 150000);
			if (status != 0)
				ehci_dbg(ehci, "port %d: disconnected\n",
					 port + 1);
		}
	}
#else  
	msleep(20);
#endif  

	spin_lock_irq(&ehci->lock);
	port = HCS_N_PORTS(ehci->hcs_params);
	while (port--) {
		if (test_bit(port, &ehci->owned_ports)) {
			reg = &ehci->regs->port_status[port];
			status = ehci_readl(ehci, reg) & ~PORT_RWC_BITS;

			if (status & PORT_OWNER)
				clear_bit(port, &ehci->owned_ports);
			else if (test_bit(port, &ehci->companion_ports))
				ehci_writel(ehci, status & ~PORT_PE, reg);
			else {
				spin_unlock_irq(&ehci->lock);
				ehci_hub_control(hcd, SetPortFeature,
						USB_PORT_FEAT_RESET, port + 1,
						NULL, 0);
				spin_lock_irq(&ehci->lock);
			}
		}
	}
	spin_unlock_irq(&ehci->lock);

	if (!ehci->owned_ports)
		return;
	msleep(90);		 

	spin_lock_irq(&ehci->lock);
	port = HCS_N_PORTS(ehci->hcs_params);
	while (port--) {
		if (test_bit(port, &ehci->owned_ports)) {
			spin_unlock_irq(&ehci->lock);
			ehci_hub_control(hcd, GetPortStatus,
					0, port + 1,
					(char *) &buf, sizeof(buf));
			spin_lock_irq(&ehci->lock);

			reg = &ehci->regs->port_status[port];
			status = ehci_readl(ehci, reg) & ~PORT_RWC_BITS;
			if (status & PORT_OWNER)
				ehci_writel(ehci, status | PORT_CSC, reg);
			else {
				ehci_dbg(ehci, "failed handover port %d: %x\n",
						port + 1, status);
				ehci_writel(ehci, status & ~PORT_PE, reg);
			}
		}
	}

	ehci->owned_ports = 0;
	spin_unlock_irq(&ehci->lock);
}

static int ehci_port_change(struct ehci_hcd *ehci)
{
	int i = HCS_N_PORTS(ehci->hcs_params);

	if (ehci_readl(ehci, &ehci->regs->status) & STS_PCD)
		return 1;

	while (i--)
		if (ehci_readl(ehci, &ehci->regs->port_status[i]) & PORT_CSC)
			return 1;

	return 0;
}

static void ehci_adjust_port_wakeup_flags(struct ehci_hcd *ehci,
		bool suspending, bool do_wakeup)
{
	int		port;
	u32		temp;

	if (!ehci_to_hcd(ehci)->self.root_hub->do_remote_wakeup || do_wakeup)
		return;

	spin_lock_irq(&ehci->lock);

	if (ehci->has_hostpc) {
		port = HCS_N_PORTS(ehci->hcs_params);
		while (port--) {
			u32 __iomem	*hostpc_reg = &ehci->regs->hostpc[port];

			temp = ehci_readl(ehci, hostpc_reg);
			ehci_writel(ehci, temp & ~HOSTPC_PHCD, hostpc_reg);
		}
		spin_unlock_irq(&ehci->lock);
		msleep(5);
		spin_lock_irq(&ehci->lock);
	}

	port = HCS_N_PORTS(ehci->hcs_params);
	while (port--) {
		u32 __iomem	*reg = &ehci->regs->port_status[port];
		u32		t1 = ehci_readl(ehci, reg) & ~PORT_RWC_BITS;
		u32		t2 = t1 & ~PORT_WAKE_BITS;

		if (!suspending) {
			if (t1 & PORT_CONNECT)
				t2 |= PORT_WKOC_E | PORT_WKDISC_E;
			else
				t2 |= PORT_WKOC_E | PORT_WKCONN_E;
		}
		ehci_vdbg(ehci, "port %d, %08x -> %08x\n",
				port + 1, t1, t2);
		ehci_writel(ehci, t2, reg);
	}

	if (ehci->has_hostpc) {
		port = HCS_N_PORTS(ehci->hcs_params);
		while (port--) {
			u32 __iomem	*hostpc_reg = &ehci->regs->hostpc[port];

			temp = ehci_readl(ehci, hostpc_reg);
			ehci_writel(ehci, temp | HOSTPC_PHCD, hostpc_reg);
		}
	}

	if (!suspending && ehci_port_change(ehci))
		usb_hcd_resume_root_hub(ehci_to_hcd(ehci));

	spin_unlock_irq(&ehci->lock);
}

static int ehci_bus_suspend (struct usb_hcd *hcd)
{
	struct ehci_hcd		*ehci = hcd_to_ehci (hcd);
	int			port;
	int			mask;
	int			changed;

	ehci_dbg(ehci, "suspend root hub\n");

	if (time_before (jiffies, ehci->next_statechange))
		msleep(5);

	ehci_quiesce(ehci);

	spin_lock_irq (&ehci->lock);
	if (ehci->rh_state < EHCI_RH_RUNNING)
		goto done;

	if (hcd->self.root_hub->do_remote_wakeup) {
		if (ehci->resuming_ports) {
			spin_unlock_irq(&ehci->lock);
			ehci_dbg(ehci, "suspend failed because a port is resuming\n");
			return -EBUSY;
		}
	}

	ehci->bus_suspended = 0;
	ehci->owned_ports = 0;
	changed = 0;
	port = HCS_N_PORTS(ehci->hcs_params);
	while (port--) {
		u32 __iomem	*reg = &ehci->regs->port_status [port];
		u32		t1 = ehci_readl(ehci, reg) & ~PORT_RWC_BITS;
		u32		t2 = t1 & ~PORT_WAKE_BITS;

		if (t1 & PORT_OWNER)
			set_bit(port, &ehci->owned_ports);
		else if ((t1 & PORT_PE) && !(t1 & PORT_SUSPEND)) {
			t2 |= PORT_SUSPEND;
			set_bit(port, &ehci->bus_suspended);
		}

		if (hcd->self.root_hub->do_remote_wakeup) {
			 
			if (t1 & PORT_CONNECT)
				t2 |= PORT_WKOC_E | PORT_WKDISC_E;
			else
				t2 |= PORT_WKOC_E | PORT_WKCONN_E;
		}

		if (t1 != t2) {
			ehci_vdbg (ehci, "port %d, %08x -> %08x\n",
				port + 1, t1, t2);
			ehci_writel(ehci, t2, reg);
			changed = 1;
		}
	}

	if (changed && ehci->has_hostpc) {
		spin_unlock_irq(&ehci->lock);
		msleep(5);	 
		spin_lock_irq(&ehci->lock);

		port = HCS_N_PORTS(ehci->hcs_params);
		while (port--) {
			u32 __iomem	*hostpc_reg = &ehci->regs->hostpc[port];
			u32		t3;

			t3 = ehci_readl(ehci, hostpc_reg);
			ehci_writel(ehci, t3 | HOSTPC_PHCD, hostpc_reg);
			t3 = ehci_readl(ehci, hostpc_reg);
			ehci_dbg(ehci, "Port %d phy low-power mode %s\n",
					port, (t3 & HOSTPC_PHCD) ?
					"succeeded" : "failed");
		}
	}
	spin_unlock_irq(&ehci->lock);

	if (ehci->bus_suspended)
		udelay(150);

	ehci_halt (ehci);

	spin_lock_irq(&ehci->lock);
	if (ehci->enabled_hrtimer_events & BIT(EHCI_HRTIMER_POLL_DEAD))
		ehci_handle_controller_death(ehci);
	if (ehci->rh_state != EHCI_RH_RUNNING)
		goto done;
	ehci->rh_state = EHCI_RH_SUSPENDED;

	end_unlink_async(ehci);
	unlink_empty_async_suspended(ehci);
	ehci_handle_start_intr_unlinks(ehci);
	ehci_handle_intr_unlinks(ehci);
	end_free_itds(ehci);

	mask = INTR_MASK;
	if (!hcd->self.root_hub->do_remote_wakeup)
		mask &= ~STS_PCD;
	ehci_writel(ehci, mask, &ehci->regs->intr_enable);
	ehci_readl(ehci, &ehci->regs->intr_enable);

 done:
	ehci->next_statechange = jiffies + msecs_to_jiffies(10);
	ehci->enabled_hrtimer_events = 0;
	ehci->next_hrtimer_event = EHCI_HRTIMER_NO_EVENT;
	spin_unlock_irq (&ehci->lock);

	hrtimer_cancel(&ehci->hrtimer);
	return 0;
}

static int ehci_bus_resume (struct usb_hcd *hcd)
{
	struct ehci_hcd		*ehci = hcd_to_ehci (hcd);
	u32			temp;
	u32			power_okay;
	int			i;
	unsigned long		resume_needed = 0;

	if (time_before (jiffies, ehci->next_statechange))
		msleep(5);
	spin_lock_irq (&ehci->lock);
	if (!HCD_HW_ACCESSIBLE(hcd) || ehci->shutdown)
		goto shutdown;

	if (unlikely(ehci->debug)) {
		if (!dbgp_reset_prep(hcd))
			ehci->debug = NULL;
		else
			dbgp_external_startup(hcd);
	}

	power_okay = ehci_readl(ehci, &ehci->regs->intr_enable);
	ehci_dbg(ehci, "resume root hub%s\n",
			power_okay ? "" : " after power loss");

	ehci_writel(ehci, 0, &ehci->regs->intr_enable);

	ehci_writel(ehci, 0, &ehci->regs->segment);
	ehci_writel(ehci, ehci->periodic_dma, &ehci->regs->frame_list);
	ehci_writel(ehci, (u32) ehci->async->qh_dma, &ehci->regs->async_next);

	ehci->command |= CMD_RUN;
	ehci_writel(ehci, ehci->command, &ehci->regs->command);
	ehci->rh_state = EHCI_RH_RUNNING;

	i = HCS_N_PORTS(ehci->hcs_params);
	while (i--) {
		temp = ehci_readl(ehci, &ehci->regs->port_status[i]);
		if ((temp & PORT_PE) &&
				!(temp & (PORT_SUSPEND | PORT_RESUME))) {
			ehci_dbg(ehci, "Port status(0x%x) is wrong\n", temp);
			spin_unlock_irq(&ehci->lock);
			msleep(8);
			spin_lock_irq(&ehci->lock);
			break;
		}
	}

	if (ehci->shutdown)
		goto shutdown;

	if (ehci->bus_suspended && ehci->has_hostpc) {
		i = HCS_N_PORTS(ehci->hcs_params);
		while (i--) {
			if (test_bit(i, &ehci->bus_suspended)) {
				u32 __iomem	*hostpc_reg =
							&ehci->regs->hostpc[i];

				temp = ehci_readl(ehci, hostpc_reg);
				ehci_writel(ehci, temp & ~HOSTPC_PHCD,
						hostpc_reg);
			}
		}
		spin_unlock_irq(&ehci->lock);
		msleep(5);
		spin_lock_irq(&ehci->lock);
		if (ehci->shutdown)
			goto shutdown;
	}

	i = HCS_N_PORTS (ehci->hcs_params);
	while (i--) {
		temp = ehci_readl(ehci, &ehci->regs->port_status [i]);
		temp &= ~(PORT_RWC_BITS | PORT_WAKE_BITS);
		if (test_bit(i, &ehci->bus_suspended) &&
				(temp & PORT_SUSPEND)) {
			temp |= PORT_RESUME;
			set_bit(i, &resume_needed);
		}
		ehci_writel(ehci, temp, &ehci->regs->port_status [i]);
	}

	if (resume_needed) {
		spin_unlock_irq(&ehci->lock);
		msleep(20);
		spin_lock_irq(&ehci->lock);
		if (ehci->shutdown)
			goto shutdown;
	}

	i = HCS_N_PORTS (ehci->hcs_params);
	while (i--) {
		temp = ehci_readl(ehci, &ehci->regs->port_status [i]);
		if (test_bit(i, &resume_needed)) {
			temp &= ~(PORT_RWC_BITS | PORT_SUSPEND | PORT_RESUME);
			ehci_writel(ehci, temp, &ehci->regs->port_status [i]);
			ehci_vdbg (ehci, "resumed port %d\n", i + 1);
		}
	}

	ehci->next_statechange = jiffies + msecs_to_jiffies(5);
	spin_unlock_irq(&ehci->lock);

	ehci_handover_companion_ports(ehci);

	spin_lock_irq(&ehci->lock);
	if (ehci->shutdown)
		goto shutdown;
	ehci_writel(ehci, INTR_MASK, &ehci->regs->intr_enable);
	(void) ehci_readl(ehci, &ehci->regs->intr_enable);
	spin_unlock_irq(&ehci->lock);

	return 0;

 shutdown:
	spin_unlock_irq(&ehci->lock);
	return -ESHUTDOWN;
}

#else

#define ehci_bus_suspend	NULL
#define ehci_bus_resume		NULL

#endif	 

static void set_owner(struct ehci_hcd *ehci, int portnum, int new_owner)
{
	u32 __iomem		*status_reg;
	u32			port_status;
	int 			try;

	status_reg = &ehci->regs->port_status[portnum];

	for (try = 4; try > 0; --try) {
		spin_lock_irq(&ehci->lock);
		port_status = ehci_readl(ehci, status_reg);
		if ((port_status & PORT_OWNER) == new_owner
				|| (port_status & (PORT_OWNER | PORT_CONNECT))
					== 0)
			try = 0;
		else {
			port_status ^= PORT_OWNER;
			port_status &= ~(PORT_PE | PORT_RWC_BITS);
			ehci_writel(ehci, port_status, status_reg);
		}
		spin_unlock_irq(&ehci->lock);
		if (try > 1)
			msleep(5);
	}
}

static int check_reset_complete (
	struct ehci_hcd	*ehci,
	int		index,
	u32 __iomem	*status_reg,
	int		port_status
) {
	if (!(port_status & PORT_CONNECT))
		return port_status;

	if (!(port_status & PORT_PE)) {

		if (ehci_is_TDI(ehci)) {
			ehci_dbg (ehci,
				"Failed to enable port %d on root hub TT\n",
				index+1);
			return port_status;
		}

		ehci_dbg (ehci, "port %d full speed --> companion\n",
			index + 1);

		port_status |= PORT_OWNER;
		port_status &= ~PORT_RWC_BITS;
		ehci_writel(ehci, port_status, status_reg);

		if (ehci->has_amcc_usb23)
			set_ohci_hcfs(ehci, 1);
	} else {
		ehci_dbg(ehci, "port %d reset complete, port enabled\n",
			index + 1);
		 
		if (ehci->has_amcc_usb23)
			set_ohci_hcfs(ehci, 0);
	}

	return port_status;
}

static int
ehci_hub_status_data (struct usb_hcd *hcd, char *buf)
{
	struct ehci_hcd	*ehci = hcd_to_ehci (hcd);
	u32		temp, status;
	u32		mask;
	int		ports, i, retval = 1;
	unsigned long	flags;
	u32		ppcd = ~0;

	buf [0] = 0;
	ports = HCS_N_PORTS (ehci->hcs_params);
	if (ports > 7) {
		buf [1] = 0;
		retval++;
	}

	status = ehci->resuming_ports;

	if (!ignore_oc)
		mask = PORT_CSC | PORT_PEC | PORT_OCC;
	else
		mask = PORT_CSC | PORT_PEC;
	 
	spin_lock_irqsave (&ehci->lock, flags);

	if (ehci->has_ppcd)
		ppcd = ehci_readl(ehci, &ehci->regs->status) >> 16;

	for (i = 0; i < ports; i++) {
		 
		if (ppcd & (1 << i))
			temp = ehci_readl(ehci, &ehci->regs->port_status[i]);
		else
			temp = 0;

		if ((temp & mask) != 0 || test_bit(i, &ehci->port_c_suspend)
				|| (ehci->reset_done[i] && time_after_eq(
					jiffies, ehci->reset_done[i]))) {
			if (i < 7)
			    buf [0] |= 1 << (i + 1);
			else
			    buf [1] |= 1 << (i - 7);
			status = STS_PCD;
		}
	}

	if (ehci->resuming_ports)
		mod_timer(&hcd->rh_timer, jiffies + msecs_to_jiffies(25));

	spin_unlock_irqrestore (&ehci->lock, flags);
	return status ? retval : 0;
}

static void
ehci_hub_descriptor (
	struct ehci_hcd			*ehci,
	struct usb_hub_descriptor	*desc
) {
	int		ports = HCS_N_PORTS (ehci->hcs_params);
	u16		temp;

	desc->bDescriptorType = 0x29;
	desc->bPwrOn2PwrGood = 10;	 
	desc->bHubContrCurrent = 0;

	desc->bNbrPorts = ports;
	temp = 1 + (ports / 8);
	desc->bDescLength = 7 + 2 * temp;

	memset(&desc->u.hs.DeviceRemovable[0], 0, temp);
	memset(&desc->u.hs.DeviceRemovable[temp], 0xff, temp);

	temp = 0x0008;			 
	if (HCS_PPC (ehci->hcs_params))
		temp |= 0x0001;		 
	else
		temp |= 0x0002;		 
#if 0
 
	if (HCS_INDICATOR (ehci->hcs_params))
		temp |= 0x0080;		 
#endif
	desc->wHubCharacteristics = cpu_to_le16(temp);
}

static int ehci_hub_control (
	struct usb_hcd	*hcd,
	u16		typeReq,
	u16		wValue,
	u16		wIndex,
	char		*buf,
	u16		wLength
) {
	struct ehci_hcd	*ehci = hcd_to_ehci (hcd);
	int		ports = HCS_N_PORTS (ehci->hcs_params);
	u32 __iomem	*status_reg = &ehci->regs->port_status[
				(wIndex & 0xff) - 1];
	u32 __iomem	*hostpc_reg = &ehci->regs->hostpc[(wIndex & 0xff) - 1];
	u32		temp, temp1, status;
	unsigned long	flags;
	int		retval = 0;
	unsigned	selector;

	spin_lock_irqsave (&ehci->lock, flags);
	switch (typeReq) {
	case ClearHubFeature:
		switch (wValue) {
		case C_HUB_LOCAL_POWER:
		case C_HUB_OVER_CURRENT:
			 
			break;
		default:
			goto error;
		}
		break;
	case ClearPortFeature:
		if (!wIndex || wIndex > ports)
			goto error;
		wIndex--;
		temp = ehci_readl(ehci, status_reg);
		temp &= ~PORT_RWC_BITS;

		switch (wValue) {
		case USB_PORT_FEAT_ENABLE:
			ehci_writel(ehci, temp & ~PORT_PE, status_reg);
			break;
		case USB_PORT_FEAT_C_ENABLE:
			ehci_writel(ehci, temp | PORT_PEC, status_reg);
			break;
		case USB_PORT_FEAT_SUSPEND:
			if (temp & PORT_RESET)
				goto error;
			if (ehci->no_selective_suspend)
				break;
#ifdef CONFIG_USB_OTG
			if ((hcd->self.otg_port == (wIndex + 1))
			    && hcd->self.b_hnp_enable) {
				otg_start_hnp(hcd->phy->otg);
				break;
			}
#endif
			if (!(temp & PORT_SUSPEND))
				break;
			if ((temp & PORT_PE) == 0)
				goto error;

			if (ehci->has_hostpc) {
				temp1 = ehci_readl(ehci, hostpc_reg);
				ehci_writel(ehci, temp1 & ~HOSTPC_PHCD,
						hostpc_reg);
				spin_unlock_irqrestore(&ehci->lock, flags);
				msleep(5); 
				spin_lock_irqsave(&ehci->lock, flags);
			}
			 
			temp &= ~PORT_WAKE_BITS;
			ehci_writel(ehci, temp | PORT_RESUME, status_reg);
			ehci->reset_done[wIndex] = jiffies
					+ msecs_to_jiffies(20);
			break;
		case USB_PORT_FEAT_C_SUSPEND:
			clear_bit(wIndex, &ehci->port_c_suspend);
			break;
		case USB_PORT_FEAT_POWER:
			if (HCS_PPC (ehci->hcs_params))
				ehci_writel(ehci, temp & ~PORT_POWER,
						status_reg);
			break;
		case USB_PORT_FEAT_C_CONNECTION:
			ehci_writel(ehci, temp | PORT_CSC, status_reg);
			break;
		case USB_PORT_FEAT_C_OVER_CURRENT:
			ehci_writel(ehci, temp | PORT_OCC, status_reg);
			break;
		case USB_PORT_FEAT_C_RESET:
			 
			break;
		default:
			goto error;
		}
		ehci_readl(ehci, &ehci->regs->command);	 
		break;
	case GetHubDescriptor:
		ehci_hub_descriptor (ehci, (struct usb_hub_descriptor *)
			buf);
		break;
	case GetHubStatus:
		 
		memset (buf, 0, 4);
		 
		break;
	case GetPortStatus:
		if (!wIndex || wIndex > ports)
			goto error;
		wIndex--;
		status = 0;
		temp = ehci_readl(ehci, status_reg);

		if (temp & PORT_CSC)
			status |= USB_PORT_STAT_C_CONNECTION << 16;
		if (temp & PORT_PEC)
			status |= USB_PORT_STAT_C_ENABLE << 16;

		if ((temp & PORT_OCC) && !ignore_oc){
			status |= USB_PORT_STAT_C_OVERCURRENT << 16;

			if (((temp & PORT_OC) || (ehci->need_oc_pp_cycle))
					&& HCS_PPC(ehci->hcs_params)) {
				ehci_writel(ehci,
					temp & ~(PORT_RWC_BITS | PORT_POWER),
					status_reg);
				temp = ehci_readl(ehci, status_reg);
			}
		}

		if (temp & PORT_RESUME) {

			if (!ehci->reset_done[wIndex]) {
				 
				ehci->reset_done[wIndex] = jiffies
						+ msecs_to_jiffies(20);
				usb_hcd_start_port_resume(&hcd->self, wIndex);
				set_bit(wIndex, &ehci->resuming_ports);
				 
				mod_timer(&ehci_to_hcd(ehci)->rh_timer,
						ehci->reset_done[wIndex]);
			}

			else if (time_after_eq(jiffies,
					ehci->reset_done[wIndex])) {
				clear_bit(wIndex, &ehci->suspended_ports);
				set_bit(wIndex, &ehci->port_c_suspend);
				ehci->reset_done[wIndex] = 0;
				usb_hcd_end_port_resume(&hcd->self, wIndex);

				temp &= ~(PORT_RWC_BITS |
						PORT_SUSPEND | PORT_RESUME);
				ehci_writel(ehci, temp, status_reg);
				clear_bit(wIndex, &ehci->resuming_ports);
				retval = handshake(ehci, status_reg,
					   PORT_RESUME, 0, 2000  );
				if (retval != 0) {
					ehci_err(ehci,
						"port %d resume error %d\n",
						wIndex + 1, retval);
					goto error;
				}
				temp = ehci_readl(ehci, status_reg);
			}
		}

		if ((temp & PORT_RESET)
				&& time_after_eq(jiffies,
					ehci->reset_done[wIndex])) {
			status |= USB_PORT_STAT_C_RESET << 16;
			ehci->reset_done [wIndex] = 0;
			clear_bit(wIndex, &ehci->resuming_ports);

			ehci_writel(ehci, temp & ~(PORT_RWC_BITS | PORT_RESET),
					status_reg);
			 
			retval = handshake(ehci, status_reg,
					PORT_RESET, 0, 1000);
			if (retval != 0) {
				ehci_err (ehci, "port %d reset error %d\n",
					wIndex + 1, retval);
				goto error;
			}

			temp = check_reset_complete (ehci, wIndex, status_reg,
					ehci_readl(ehci, status_reg));
		}

		if (!(temp & (PORT_RESUME|PORT_RESET))) {
			ehci->reset_done[wIndex] = 0;
			clear_bit(wIndex, &ehci->resuming_ports);
		}

		if ((temp & PORT_CONNECT) &&
				test_bit(wIndex, &ehci->companion_ports)) {
			temp &= ~PORT_RWC_BITS;
			temp |= PORT_OWNER;
			ehci_writel(ehci, temp, status_reg);
			ehci_dbg(ehci, "port %d --> companion\n", wIndex + 1);
			temp = ehci_readl(ehci, status_reg);
		}

		if (temp & PORT_CONNECT) {
			status |= USB_PORT_STAT_CONNECTION;
			 
			if (ehci->has_hostpc) {
				temp1 = ehci_readl(ehci, hostpc_reg);
				status |= ehci_port_speed(ehci, temp1);
			} else
				status |= ehci_port_speed(ehci, temp);
		}
		if (temp & PORT_PE)
			status |= USB_PORT_STAT_ENABLE;

		if (temp & (PORT_SUSPEND|PORT_RESUME)) {
			status |= USB_PORT_STAT_SUSPEND;
		} else if (test_bit(wIndex, &ehci->suspended_ports)) {
			clear_bit(wIndex, &ehci->suspended_ports);
			clear_bit(wIndex, &ehci->resuming_ports);
			ehci->reset_done[wIndex] = 0;
			if (temp & PORT_PE)
				set_bit(wIndex, &ehci->port_c_suspend);
			usb_hcd_end_port_resume(&hcd->self, wIndex);
		}

		if (temp & PORT_OC)
			status |= USB_PORT_STAT_OVERCURRENT;
		if (temp & PORT_RESET)
			status |= USB_PORT_STAT_RESET;
		if (temp & PORT_POWER)
			status |= USB_PORT_STAT_POWER;
		if (test_bit(wIndex, &ehci->port_c_suspend))
			status |= USB_PORT_STAT_C_SUSPEND << 16;

#ifndef	VERBOSE_DEBUG
	if (status & ~0xffff)	 
#endif
		dbg_port (ehci, "GetStatus", wIndex + 1, temp);
		put_unaligned_le32(status, buf);
		break;
	case SetHubFeature:
		switch (wValue) {
		case C_HUB_LOCAL_POWER:
		case C_HUB_OVER_CURRENT:
			 
			break;
		default:
			goto error;
		}
		break;
	case SetPortFeature:
		selector = wIndex >> 8;
		wIndex &= 0xff;
		if (unlikely(ehci->debug)) {
			 
			if (wIndex == HCS_DEBUG_PORT(ehci->hcs_params) &&
			    (readl(&ehci->debug->control) & DBGP_ENABLED)) {
				retval = -ENODEV;
				goto error_exit;
			}
		}
		if (!wIndex || wIndex > ports)
			goto error;
		wIndex--;
		temp = ehci_readl(ehci, status_reg);
		if (temp & PORT_OWNER)
			break;

		temp &= ~PORT_RWC_BITS;
		switch (wValue) {
		case USB_PORT_FEAT_SUSPEND:
			if (ehci->no_selective_suspend)
				break;
			if ((temp & PORT_PE) == 0
					|| (temp & PORT_RESET) != 0)
				goto error;

			temp &= ~PORT_WKCONN_E;
			temp |= PORT_WKDISC_E | PORT_WKOC_E;
			ehci_writel(ehci, temp | PORT_SUSPEND, status_reg);
			if (ehci->has_hostpc) {
				spin_unlock_irqrestore(&ehci->lock, flags);
				msleep(5); 
				spin_lock_irqsave(&ehci->lock, flags);
				temp1 = ehci_readl(ehci, hostpc_reg);
				ehci_writel(ehci, temp1 | HOSTPC_PHCD,
					hostpc_reg);
				temp1 = ehci_readl(ehci, hostpc_reg);
				ehci_dbg(ehci, "Port%d phy low pwr mode %s\n",
					wIndex, (temp1 & HOSTPC_PHCD) ?
					"succeeded" : "failed");
			}
			set_bit(wIndex, &ehci->suspended_ports);
			break;
		case USB_PORT_FEAT_POWER:
#ifdef MY_DEF_HERE
			if (!(temp & PORT_OC) && HCS_PPC(ehci->hcs_params))
#else  
			if (HCS_PPC (ehci->hcs_params))
#endif  
				ehci_writel(ehci, temp | PORT_POWER,
						status_reg);
			break;
		case USB_PORT_FEAT_RESET:
			if (temp & PORT_RESUME)
				goto error;
			 
			if ((temp & (PORT_PE|PORT_CONNECT)) == PORT_CONNECT
					&& !ehci_is_TDI(ehci)
					&& PORT_USB11 (temp)) {
				ehci_dbg (ehci,
					"port %d low speed --> companion\n",
					wIndex + 1);
				temp |= PORT_OWNER;
			} else {
				ehci_vdbg (ehci, "port %d reset\n", wIndex + 1);
				temp |= PORT_RESET;
				temp &= ~PORT_PE;

				ehci->reset_done [wIndex] = jiffies
						+ msecs_to_jiffies (50);
			}
			ehci_writel(ehci, temp, status_reg);
			break;

		case USB_PORT_FEAT_TEST:
			if (!selector || selector > 5)
				goto error;
			spin_unlock_irqrestore(&ehci->lock, flags);
			ehci_quiesce(ehci);
			spin_lock_irqsave(&ehci->lock, flags);

			while (ports--) {
				u32 __iomem *sreg =
						&ehci->regs->port_status[ports];

				temp = ehci_readl(ehci, sreg) & ~PORT_RWC_BITS;
				if (temp & PORT_PE)
					ehci_writel(ehci, temp | PORT_SUSPEND,
							sreg);
			}

			spin_unlock_irqrestore(&ehci->lock, flags);
			ehci_halt(ehci);
			spin_lock_irqsave(&ehci->lock, flags);

			temp = ehci_readl(ehci, status_reg);
			temp |= selector << 16;
			ehci_writel(ehci, temp, status_reg);
			break;

		default:
			goto error;
		}
		ehci_readl(ehci, &ehci->regs->command);	 
		break;

	default:
error:
		 
		retval = -EPIPE;
	}
error_exit:
	spin_unlock_irqrestore (&ehci->lock, flags);
	return retval;
}

static void ehci_relinquish_port(struct usb_hcd *hcd, int portnum)
{
	struct ehci_hcd		*ehci = hcd_to_ehci(hcd);

	if (ehci_is_TDI(ehci))
		return;
	set_owner(ehci, --portnum, PORT_OWNER);
}

static int ehci_port_handed_over(struct usb_hcd *hcd, int portnum)
{
	struct ehci_hcd		*ehci = hcd_to_ehci(hcd);
	u32 __iomem		*reg;

	if (ehci_is_TDI(ehci))
		return 0;
	reg = &ehci->regs->port_status[portnum - 1];
	return ehci_readl(ehci, reg) & PORT_OWNER;
}
