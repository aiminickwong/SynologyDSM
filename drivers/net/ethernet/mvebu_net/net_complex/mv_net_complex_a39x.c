#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/platform_device.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <linux/inetdevice.h>
#include <linux/interrupt.h>
#include <linux/mbus.h>
#include <asm/setup.h>
#include <linux/list.h>
#include <linux/firmware.h>
#include <linux/io.h>

#ifdef CONFIG_ARCH_MVEBU
#include "mvNetConfig.h"
#endif  

#include "mv_net_complex_a39x.h"

static u32 mv_net_complex_vbase_addr;
static u32 mv_net_complex_misc_vbase_addr;
static u32 mv_net_complex_phy_vbase_addr;
static u32 mv_net_reg_virt_base;

static struct resource mv_net_complex_resources[] = {
	{
		.name	= "netcomplex_misc",
		.start	= INTER_REGS_PHYS_BASE | 0x18200,
		.end	= (INTER_REGS_PHYS_BASE | 0x18200) + 0x100 - 1,  
		.flags	= IORESOURCE_MEM,
	}, {
		.name	= "netcomplex_phy",
		.start	= INTER_REGS_PHYS_BASE | 0x18300,
		.end	= (INTER_REGS_PHYS_BASE | 0x18300) + 0x200 - 1,  
		.flags	= IORESOURCE_MEM,
	}, {
		.name	= "netcomplex_base",
		.start	= INTER_REGS_PHYS_BASE | 0x18a00,
		.end	= (INTER_REGS_PHYS_BASE | 0x18a00) + 0x1000 - 1,  
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device mv_net_complex_plat = {
	.name			= MV_NET_COMPLEX_NAME,
	.num_resources		= ARRAY_SIZE(mv_net_complex_resources),
	.resource		= mv_net_complex_resources,
	.dev			= {
		.coherent_dma_mask = DMA_BIT_MASK(32),
	}
};

static void mv_net_assert_load_config(void)
{
	u32 reg;

	reg = MV_REG_READ(MV_NETCOMP_SYSTEM_SOFT_RESET);
	reg &= ~NETC_NSS_SRAM_LOAD_CONF_MASK;
	reg &= ~NETC_NSS_PPC_LOAD_CONF_MASK;
	reg &= ~NETC_NSS_MACS_LOAD_CONF_MASK;
	reg &= ~NETC_NSS_QM1_LOAD_CONF_MASK;
	MV_REG_WRITE(MV_NETCOMP_SYSTEM_SOFT_RESET, reg);
}

static void mv_net_de_assert_load_config(void)
{
	u32 reg;

	reg = MV_REG_READ(MV_NETCOMP_SYSTEM_SOFT_RESET);
	reg |= NETC_NSS_SRAM_LOAD_CONF_MASK;
	reg |= NETC_NSS_PPC_LOAD_CONF_MASK;
	reg |= NETC_NSS_MACS_LOAD_CONF_MASK;
	reg |= NETC_NSS_QM1_LOAD_CONF_MASK;
	MV_REG_WRITE(MV_NETCOMP_SYSTEM_SOFT_RESET, reg);
}

static void mv_net_pm_clock_down(void)
{
	u32 reg;

	reg = MV_REG_READ(MV_NETCOMP_CLOCK_GATING);

	reg &= ~NETC_CLOCK_GATING_SRAM_X2_MASK;
	reg &= ~NETC_CLOCK_GATING_SRAM_MASK;
	reg &= ~NETC_CLOCK_GATING_PPC_CMAC_MASK;
	reg &= ~NETC_CLOCK_GATING_PPC_PP_MASK;
	reg &= ~NETC_CLOCK_GATING_PPC_NSS_MASK;
	reg &= ~NETC_CLOCK_GATING_CMAC_MASK;
	reg &= ~NETC_CLOCK_GATING_NSS_MASK;
	reg &= ~NETC_CLOCK_GATING_QM2_MASK;
	reg &= ~NETC_CLOCK_GATING_QM1_X2_MASK;
	reg &= ~NETC_CLOCK_GATING_QM1_MASK;

	MV_REG_WRITE(MV_NETCOMP_CLOCK_GATING, reg);
}

static void mv_net_pm_clock_up(void)
{
	u32 reg;

	reg = MV_REG_READ(MV_NETCOMP_CLOCK_GATING);

	reg |= NETC_CLOCK_GATING_SRAM_X2_MASK;
	reg |= NETC_CLOCK_GATING_SRAM_MASK;
	reg |= NETC_CLOCK_GATING_PPC_CMAC_MASK;
	reg |= NETC_CLOCK_GATING_PPC_PP_MASK;
	reg |= NETC_CLOCK_GATING_PPC_NSS_MASK;
	reg |= NETC_CLOCK_GATING_CMAC_MASK;
	reg |= NETC_CLOCK_GATING_NSS_MASK;
	reg |= NETC_CLOCK_GATING_QM2_MASK;
	reg |= NETC_CLOCK_GATING_QM1_X2_MASK;
	reg |= NETC_CLOCK_GATING_QM1_MASK;

	MV_REG_WRITE(MV_NETCOMP_CLOCK_GATING, reg);
}

static void mv_net_restore_regs_defaults(void)
{
	 
	mv_net_pm_clock_down();
	mv_net_assert_load_config();
	mv_net_pm_clock_up();
	mv_net_pm_clock_down();
	mv_net_de_assert_load_config();
	mv_net_pm_clock_up();
}

void mv_net_complex_nss_select(u32 val)
{
	u32 reg;

	reg = MV_REG_READ(MV_NETCOMP_FUNCTION_ENABLE_CTRL_1);
	reg &= ~NETC_PACKET_PROCESS_MASK;

	val <<= NETC_PACKET_PROCESS_OFFSET;
	val &= NETC_PACKET_PROCESS_MASK;

	reg |= val;

	MV_REG_WRITE(MV_NETCOMP_FUNCTION_ENABLE_CTRL_1, reg);
}

static void mv_net_complex_active_port(u32 port, u32 val)
{
	u32 reg;

	reg = MV_REG_READ(MV_NETCOMP_PORTS_CONTROL_1);
	reg &= ~NETC_PORTS_ACTIVE_MASK(port);

	val <<= NETC_PORTS_ACTIVE_OFFSET(port);
	val &= NETC_PORTS_ACTIVE_MASK(port);

	reg |= val;

	MV_REG_WRITE(MV_NETCOMP_PORTS_CONTROL_1, reg);
}

static void mv_net_complex_xaui_enable(u32 port, u32 val)
{
	u32 reg;

	reg = MV_REG_READ(MV_NETCOMP_CONTROL_0);
	reg &= ~NETC_CTRL_ENA_XAUI_MASK;

	val <<= NETC_CTRL_ENA_XAUI_OFFSET;
	val &= NETC_CTRL_ENA_XAUI_MASK;

	reg |= val;

	MV_REG_WRITE(MV_NETCOMP_CONTROL_0, reg);
}

static void mv_net_complex_rxaui_enable(u32 port, u32 val)
{
	u32 reg;

	reg = MV_REG_READ(MV_NETCOMP_CONTROL_0);
	reg &= ~NETC_CTRL_ENA_RXAUI_MASK;

	val <<= NETC_CTRL_ENA_RXAUI_OFFSET;
	val &= NETC_CTRL_ENA_RXAUI_MASK;

	reg |= val;

	MV_REG_WRITE(MV_NETCOMP_CONTROL_0, reg);
}

static void mv_net_complex_gop_reset(u32 val)
{
	u32 reg;

	reg = MV_REG_READ(MV_NETCOMP_SYSTEM_SOFT_RESET);
	reg &= ~NETC_GOP_SOFT_RESET_MASK;

	val <<= NETC_GOP_SOFT_RESET_OFFSET;
	val &= NETC_GOP_SOFT_RESET_MASK;

	reg |= val;

	MV_REG_WRITE(MV_NETCOMP_SYSTEM_SOFT_RESET, reg);
}

static void mv_net_complex_gop_clock_logic_set(u32 val)
{
	u32 reg;

	reg = MV_REG_READ(MV_NETCOMP_PORTS_CONTROL_0);
	reg &= ~NETC_CLK_DIV_PHASE_MASK;

	val <<= NETC_CLK_DIV_PHASE_OFFSET;
	val &= NETC_CLK_DIV_PHASE_MASK;

	reg |= val;

	MV_REG_WRITE(MV_NETCOMP_PORTS_CONTROL_0, reg);
}

static void mv_net_complex_port_rf_reset(u32 port, u32 val)
{
	u32 reg;

	reg = MV_REG_READ(MV_NETCOMP_PORTS_CONTROL_1);
	reg &= ~NETC_PORT_GIG_RF_RESET_MASK(port);

	val <<= NETC_PORT_GIG_RF_RESET_OFFSET(port);
	val &= NETC_PORT_GIG_RF_RESET_MASK(port);

	reg |= val;

	MV_REG_WRITE(MV_NETCOMP_PORTS_CONTROL_1, reg);
}

static void mv_net_complex_gbe_mode_select(u32 val)
{
	u32 reg;

	reg = MV_REG_READ(MV_NETCOMP_CONTROL_0);
	reg &= ~NETC_GBE_PORT1_MODE_MASK;

	val <<= NETC_GBE_PORT1_MODE_OFFSET;
	val &= NETC_GBE_PORT1_MODE_MASK;

	reg |= val;

	MV_REG_WRITE(MV_NETCOMP_CONTROL_0, reg);
}

static void mv_net_complex_bus_width_select(u32 val)
{
	u32 reg;

	reg = MV_REG_READ(MV_NETCOMP_PORTS_CONTROL_0);
	reg &= ~NETC_BUS_WIDTH_SELECT_MASK;

	val <<= NETC_BUS_WIDTH_SELECT_OFFSET;
	val &= NETC_BUS_WIDTH_SELECT_MASK;

	reg |= val;

	MV_REG_WRITE(MV_NETCOMP_PORTS_CONTROL_0, reg);
}

static void mv_net_complex_sample_stages_timing(u32 val)
{
	u32 reg;

	reg = MV_REG_READ(MV_NETCOMP_PORTS_CONTROL_0);
	reg &= ~NETC_GIG_RX_DATA_SAMPLE_MASK;

	val <<= NETC_GIG_RX_DATA_SAMPLE_OFFSET;
	val &= NETC_GIG_RX_DATA_SAMPLE_MASK;

	reg |= val;

	MV_REG_WRITE(MV_NETCOMP_PORTS_CONTROL_0, reg);
}

static void mv_net_complex_com_phy_selector_config(u32 netComplex)
{
	u32 selector = MV_REG_READ(COMMON_PHYS_SELECTORS_REG);

	if (netComplex & MV_NETCOMP_GE_MAC0_2_SGMII_L0) {
		selector &= ~COMMON_PHYS_SELECTOR_LANE_MASK(0);
		selector |= 0x4 << COMMON_PHYS_SELECTOR_LANE_OFFSET(0);
	}
	if (netComplex &  MV_NETCOMP_GE_MAC0_2_SGMII_L1) {
		selector &= ~COMMON_PHYS_SELECTOR_LANE_MASK(1);
		selector |= 0x8 << COMMON_PHYS_SELECTOR_LANE_OFFSET(1);
	}
	if (netComplex &  MV_NETCOMP_GE_MAC1_2_SGMII_L1) {
		selector &= ~COMMON_PHYS_SELECTOR_LANE_MASK(1);
		selector |= 0x9 << COMMON_PHYS_SELECTOR_LANE_OFFSET(1);
	}
	if (netComplex &  MV_NETCOMP_GE_MAC1_2_SGMII_L2) {
		selector &= ~COMMON_PHYS_SELECTOR_LANE_MASK(2);
		selector |= 0x5 << COMMON_PHYS_SELECTOR_LANE_OFFSET(2);
	}
	if (netComplex &  MV_NETCOMP_GE_MAC2_2_SGMII_L3) {
		selector &= ~COMMON_PHYS_SELECTOR_LANE_MASK(3);
		selector |= 0x7 << COMMON_PHYS_SELECTOR_LANE_OFFSET(3);
	}
	if (netComplex &  MV_NETCOMP_GE_MAC3_2_SGMII_L4) {
		selector &= ~COMMON_PHYS_SELECTOR_LANE_MASK(4);
		selector |= 0x8 << COMMON_PHYS_SELECTOR_LANE_OFFSET(4);
	}
	if (netComplex &  MV_NETCOMP_GE_MAC2_2_SGMII_L5) {
		selector &= ~COMMON_PHYS_SELECTOR_LANE_MASK(5);
		selector |= 0x6 << COMMON_PHYS_SELECTOR_LANE_OFFSET(5);
	}
	if (netComplex &  MV_NETCOMP_GE_MAC3_2_SGMII_L6) {
		selector &= ~COMMON_PHYS_SELECTOR_LANE_MASK(6);
		selector |= 0x2 << COMMON_PHYS_SELECTOR_LANE_OFFSET(6);
	}

	MV_REG_WRITE(COMMON_PHYS_SELECTORS_REG, selector);
}

static void mv_net_complex_qsgmii_ctrl_config(void)
{
	u32 reg;
	 
	reg = (MV_REG_READ(MV_NETCOMP_QSGMII_CTRL_1) & (~NETC_QSGMII_CTRL_RSTN_MASK));
	reg |= 0 << NETC_QSGMII_CTRL_RSTN_OFFSET;
	MV_REG_WRITE(MV_NETCOMP_QSGMII_CTRL_1, reg);
	 
	reg = (MV_REG_READ(MV_NETCOMP_QSGMII_CTRL_1) & (~NETC_QSGMII_CTRL_VERSION_MASK));
	reg |= 1 << NETC_QSGMII_CTRL_VERSION_OFFSET;
	MV_REG_WRITE(MV_NETCOMP_QSGMII_CTRL_1, reg);
	 
	reg = (MV_REG_READ(MV_NETCOMP_QSGMII_CTRL_1) & (~NETC_QSGMII_CTRL_V3ACTIVE_MASK));
	reg |= 0 << NETC_QSGMII_CTRL_V3ACTIVE_OFFSET;
	MV_REG_WRITE(MV_NETCOMP_QSGMII_CTRL_1, reg);
	 
	reg = (MV_REG_READ(MV_NETCOMP_QSGMII_CTRL_1) & (~NETC_QSGMII_CTRL_RSTN_MASK));
	reg |= 1 << NETC_QSGMII_CTRL_RSTN_OFFSET;
	MV_REG_WRITE(MV_NETCOMP_QSGMII_CTRL_1, reg);
}

static void mv_net_complex_mac_to_rgmii(u32 port, enum mvNetComplexPhase phase)
{
	switch (phase) {
	case MV_NETC_FIRST_PHASE:
		 
		mv_net_complex_bus_width_select(1);
		 
		mv_net_complex_gbe_mode_select(1);
		break;
	case MV_NETC_SECOND_PHASE:
		 
		mv_net_complex_port_rf_reset(port, 1);
		break;
	}
}

static void mv_net_complex_mac_to_qsgmii(u32 port, enum mvNetComplexPhase phase)
{
	switch (phase) {
	case MV_NETC_FIRST_PHASE:
		 
		mv_net_complex_bus_width_select(0);
		 
		mv_net_complex_gbe_mode_select(0);
		 
		mv_net_complex_sample_stages_timing(0);
		 
		mv_net_complex_qsgmii_ctrl_config();
		break;
	case MV_NETC_SECOND_PHASE:
		 
		mv_net_complex_port_rf_reset(port, 1);
		break;
	}
}

static void mv_net_complex_mac_to_sgmii(u32 port, enum mvNetComplexPhase phase, u32 netComplex)
{
	switch (phase) {
	case MV_NETC_FIRST_PHASE:
		 
		mv_net_complex_bus_width_select(1);
		 
		mv_net_complex_gbe_mode_select(0);
		 
		mv_net_complex_sample_stages_timing(0);
		 
		mv_net_complex_com_phy_selector_config(netComplex);
		break;
	case MV_NETC_SECOND_PHASE:
		 
		mv_net_complex_port_rf_reset(port, 1);
		break;
	}
}

static void mv_net_complex_mac_to_rxaui(u32 port, enum mvNetComplexPhase phase)
{
	switch (phase) {
	case MV_NETC_FIRST_PHASE:
		 
		mv_net_complex_rxaui_enable(port, 1);
		break;
	case MV_NETC_SECOND_PHASE:
		 
		mv_net_complex_port_rf_reset(port, 1);
		break;
	}
}

static void mv_net_complex_mac_to_xaui(u32 port, enum mvNetComplexPhase phase)
{
	switch (phase) {
	case MV_NETC_FIRST_PHASE:
		 
		mv_net_complex_xaui_enable(port, 1);
		break;
	case MV_NETC_SECOND_PHASE:
		 
		mv_net_complex_port_rf_reset(port, 1);
		break;
	}
}

#if defined(MY_ABC_HERE)
int mv_net_complex_dynamic_init(u32 net_comp_config)
{
	u32 i;

	for (i = 0; i < 4; i++)
		mv_net_complex_active_port(i, 1);

	if ((net_comp_config & 0x1) == 1)
		mv_net_complex_rxaui_enable(0, 1);
	else
		mv_net_complex_rxaui_enable(0, 0);

	mv_net_complex_bus_width_select(1);
	 
	mv_net_complex_gbe_mode_select(0);
	 
	mv_net_complex_sample_stages_timing(0);

	mv_net_complex_com_phy_selector_config(net_comp_config);

	for (i = 0; i < 4; i++)
		mv_net_complex_port_rf_reset(i, 1);

	mv_net_complex_gop_clock_logic_set(1);
	 
	mv_net_complex_gop_reset(1);

	return 0;
}
#endif  

int mv_net_complex_init(u32 net_comp_config, enum mvNetComplexPhase phase)
{
	u32 reg;
	u32 c = net_comp_config, i;

	if (phase == MV_NETC_FIRST_PHASE) {
		 
		reg = (MV_REG_READ(MV_NETCOMP_AMB_ACCESS_CTRL_0) & (~NETC_AMB_ACCESS_CTRL_MASK));
		reg |= (mv_net_reg_virt_base & NETC_AMB_ACCESS_CTRL_MASK);
		MV_REG_WRITE(MV_NETCOMP_AMB_ACCESS_CTRL_0, reg);

		mv_net_complex_gop_reset(0);
		 
		for (i = 0; i < 4; i++)
			mv_net_complex_active_port(i, 1);
	}

	if (c & MV_NETCOMP_GE_MAC0_2_RXAUI)
		mv_net_complex_mac_to_rxaui(0, phase);

	if (c & MV_NETCOMP_GE_MAC0_2_XAUI)
		mv_net_complex_mac_to_xaui(0, phase);

	if (c & (MV_NETCOMP_GE_MAC0_2_SGMII_L0 | MV_NETCOMP_GE_MAC0_2_SGMII_L1))
		mv_net_complex_mac_to_sgmii(0, phase, c);

	if (c & MV_NETCOMP_GE_MAC0_2_QSGMII)
		mv_net_complex_mac_to_qsgmii(0, phase);

	if (c & (MV_NETCOMP_GE_MAC1_2_SGMII_L1 | MV_NETCOMP_GE_MAC1_2_SGMII_L2 |
			MV_NETCOMP_GE_MAC1_2_SGMII_L4))
		mv_net_complex_mac_to_sgmii(1, phase, c);

	if (c & MV_NETCOMP_GE_MAC1_2_QSGMII)
		mv_net_complex_mac_to_qsgmii(1, phase);

	if (c & MV_NETCOMP_GE_MAC1_2_RGMII1)
		mv_net_complex_mac_to_rgmii(1, phase);

	if (c & (MV_NETCOMP_GE_MAC2_2_SGMII_L3 | MV_NETCOMP_GE_MAC2_2_SGMII_L5))
		mv_net_complex_mac_to_sgmii(2, phase, c);

	if (c & MV_NETCOMP_GE_MAC2_2_QSGMII)
		mv_net_complex_mac_to_qsgmii(2, phase);

	if (c & (MV_NETCOMP_GE_MAC3_2_SGMII_L4 | MV_NETCOMP_GE_MAC3_2_SGMII_L6))
		mv_net_complex_mac_to_sgmii(3, phase, c);

	if (c & MV_NETCOMP_GE_MAC3_2_QSGMII)
		mv_net_complex_mac_to_qsgmii(3, phase);

	if (phase == MV_NETC_FIRST_PHASE)
		 
		mv_net_complex_nss_select(1);

#if defined(MY_ABC_HERE)
	if (phase == MV_NETC_SECOND_PHASE) {
#else  
	else if (phase == MV_NETC_SECOND_PHASE) {
#endif  
		 
		mv_net_complex_gop_clock_logic_set(1);
		 
		mv_net_complex_gop_reset(1);

#if defined(MY_ABC_HERE)
		 
#else  
		 
		mv_net_restore_regs_defaults();
#endif  
	}

	return 0;
}

static int mv_net_complex_plat_data_get(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "netcomplex_misc");
	if (!res) {
		pr_err("Can not find SoC control registers base address, aborting\n");
		return -1;
	}

	mv_net_complex_misc_vbase_addr = (u32)devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!mv_net_complex_misc_vbase_addr) {
		pr_err("Cannot map netcomplex misc registers, aborting\n");
		return -1;
	}
	pr_info("Net complex misc registers base: PHYS = 0x%x, VIRT = 0x%0x, size = %d Bytes\n",
		res->start, mv_net_complex_misc_vbase_addr, resource_size(res));

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "netcomplex_phy");
	if (!res) {
		pr_err("Can not find PHY registers base address, aborting\n");
		return -1;
	}

	mv_net_complex_phy_vbase_addr = (u32)devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!mv_net_complex_phy_vbase_addr) {
		pr_err("Cannot map netcomplex phy registers, aborting\n");
		return -1;
	}
	pr_info("PP3 netcomplex PHY registers base: PHYS = 0x%x, VIRT = 0x%0x, size = %d Bytes\n",
		res->start, mv_net_complex_phy_vbase_addr, resource_size(res));

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "netcomplex_base");
	if (!res) {
		pr_err("Can not find PHY registers base address, aborting\n");
		return -1;
	}

	mv_net_complex_vbase_addr = (u32)devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!mv_net_complex_vbase_addr) {
		pr_err("Cannot map netcomplex base registers, aborting\n");
		return -1;
	}
	pr_info("PP3 netcomplex base registers base: PHYS = 0x%x, VIRT = 0x%0x, size = %d Bytes\n",
		res->start, mv_net_complex_vbase_addr, resource_size(res));

	mv_net_reg_virt_base = (u32)ioremap(INTER_REGS_PHYS_BASE, INTER_REGS_SIZE);
	if (!mv_net_reg_virt_base) {
		pr_err("Cannot map base registers, aborting\n");
		return -1;
	}

	return 0;
}

#if defined(MY_ABC_HERE)
u32 net_complex = 0x421;
#endif  

static int mv_net_complex_probe(struct platform_device *pdev)
{
	int ret;
#if defined(MY_ABC_HERE)
	 
#else  
	u32 net_complex;
#endif  

	ret = mv_net_complex_plat_data_get(pdev);
	if (ret) {
		pr_err("net complex data get fail\n");
		return -1;
	}

#if defined(MY_ABC_HERE)
	pr_info("\nRun with net_complex = 0x%x\n", net_complex);
#else  
	net_complex = 0x421;
#endif  
	mv_net_complex_init(net_complex, 0);
	mv_net_complex_init(net_complex, 1);

#if defined(MY_ABC_HERE)
	 
	mv_net_restore_regs_defaults();
#endif  

	return 0;
}

#if defined(MY_ABC_HERE)
static int mv_nss_cmdline_config(char *s)
{
	int f;

	if (s) {
		f = sscanf(s, "0x%x", &net_complex);
		pr_info("\nnetcomplex 0x%x %s\n", net_complex, s);
	}

	return 1;
}
__setup("netcomplex=", mv_nss_cmdline_config);
#endif  

static int mv_net_complex_remove(struct platform_device *pdev)
{
	 
	return 0;
}

static struct platform_driver mv_net_complex_driver = {
	.probe		= mv_net_complex_probe,
	.remove		= mv_net_complex_remove,
	.driver = {
		.name	= MV_NET_COMPLEX_NAME,
		.owner	= THIS_MODULE,
	},
};

static int mv_net_complex_device_register(void)
{
	 
	platform_device_register(&mv_net_complex_plat);

	return 0;
}

static int __init mv_net_complex_init_module(void)
{
	int rc;

	rc = mv_net_complex_device_register();
	if (rc < 0) {
		pr_err("%s Net device register fail. rc=%d\n", __func__, rc);
		return rc;
	}

	rc = platform_driver_register(&mv_net_complex_driver);
	if (rc) {
		pr_err("%s: Can't register %s driver. rc=%d\n",
			__func__, mv_net_complex_driver.driver.name, rc);
		return rc;
	}
	pr_info("%s platform driver registered\n\n", mv_net_complex_driver.driver.name);

	return rc;
}
module_init(mv_net_complex_init_module);

static void __exit mv_net_complex_cleanup_module(void)
{
	platform_driver_unregister(&mv_net_complex_driver);
}
module_exit(mv_net_complex_cleanup_module);
