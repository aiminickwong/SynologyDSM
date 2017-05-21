#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/mfd/syscon.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/reset.h>

#include "core.h"
#include "io.h"

#define USB2_CLKRST_CTRL 0x00
#define aux_clk_en(n) ((n)<<0)
#define sw_pipew_reset_n(n) ((n)<<4)
#define ext_cfg_reset_n(n) ((n)<<8)
#define xhci_revision(n) ((n)<<12)

#define USB2_VBUS_MNGMNT_SEL1 0x2C
 
#define SEL_OVERRIDE_VBUSVALID(n) ((n)<<0)
#define SEL_OVERRIDE_POWERPRESENT(n) ((n)<<4)
#define SEL_OVERRIDE_BVALID(n) ((n)<<8)

#define USB2_VBUS_MNGMNT_VAL1 0x30
#define OVERRIDE_VBUSVALID_VAL (1 << 0)
#define OVERRIDE_POWERPRESENT_VAL (1 << 4)
#define OVERRIDE_BVALID_VAL (1 << 8)

#define USB_HOST_DEFAULT_MASK	0xffe
#define USB_SET_PORT_DEVICE	0x1

struct st_dwc3 {
	struct device *dev;	 
	void __iomem *glue_base;	 
	struct regmap *regmap;	 
	int syscfg_reg_off;	 
	bool drd_device_conf;	 
	struct reset_control *rstc_p;	 
	struct reset_control *rstc_s;	 
};

static inline u32 st_dwc3_readl(void __iomem *base, u32 offset)
{
	return readl_relaxed(base + offset);
}

static inline void st_dwc3_writel(void __iomem *base, u32 offset, u32 value)
{
	writel_relaxed(value, base + offset);
}

static int st_dwc3_drd_init(struct st_dwc3 *dwc3_data)
{
	u32 val;

	regmap_read(dwc3_data->regmap, dwc3_data->syscfg_reg_off, &val);

	if (dwc3_data->drd_device_conf)
		val |= USB_SET_PORT_DEVICE;
	else
		val &= USB_HOST_DEFAULT_MASK;

	return regmap_write(dwc3_data->regmap, dwc3_data->syscfg_reg_off, val);
}

static void st_dwc3_init(struct st_dwc3 *dwc3_data)
{
	u32 reg = st_dwc3_readl(dwc3_data->glue_base, USB2_CLKRST_CTRL);

	reg |= aux_clk_en(1) | ext_cfg_reset_n(1) | xhci_revision(1);
	reg &= ~sw_pipew_reset_n(1);
	st_dwc3_writel(dwc3_data->glue_base, USB2_CLKRST_CTRL, reg);

	reg = st_dwc3_readl(dwc3_data->glue_base, USB2_VBUS_MNGMNT_SEL1);
	reg |= SEL_OVERRIDE_VBUSVALID(1) | SEL_OVERRIDE_POWERPRESENT(1) |
	    SEL_OVERRIDE_BVALID(1);
	st_dwc3_writel(dwc3_data->glue_base, USB2_VBUS_MNGMNT_SEL1, reg);
	udelay(100);

	reg = st_dwc3_readl(dwc3_data->glue_base, USB2_CLKRST_CTRL);
	reg |= sw_pipew_reset_n(1);
	st_dwc3_writel(dwc3_data->glue_base, USB2_CLKRST_CTRL, reg);
}

static void st_dwc3_dt_get_pdata(struct platform_device *pdev,
				 struct st_dwc3 *dwc3_data)
{
	struct device_node *np = pdev->dev.of_node;

	dwc3_data->drd_device_conf =
	    of_property_read_bool(np, "st,dwc3-drd-device");
}

#ifdef MY_DEF_HERE
static u32 st_dwc3_gsbuscfg_setup(struct device *dev, int index)
{
	struct device_node *np = dev->of_node;
	u32 gsbuscfg;
	int ret;

	ret = of_property_read_u32_index(np, "st,gsbuscfg", index, &gsbuscfg);

	if (!ret) {
		return gsbuscfg;
	} else {
		WARN_ON(ret != 0);
		return -EINVAL;
	}
}
#endif  
 
static int st_dwc3_probe(struct platform_device *pdev)
{
	struct st_dwc3 *dwc3_data;
#ifdef MY_DEF_HERE
	struct dwc3_cfg_ops cfg_ops;
#endif  
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct regmap *regmap;
	int ret = 0;

	dwc3_data = devm_kzalloc(dev, sizeof(*dwc3_data), GFP_KERNEL);
	if (!dwc3_data)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "reg-glue");
	if (!res)
		return -ENXIO;

	dwc3_data->glue_base = devm_request_and_ioremap(dev, res);
	if (!dwc3_data->glue_base)
		return -EADDRNOTAVAIL;

	regmap = syscon_regmap_lookup_by_phandle(node, "st,syscfg");
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	dwc3_data->dev = &pdev->dev;
	dwc3_data->regmap = regmap;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "syscfg-reg");
	if (!res)
		return -ENXIO;
	dwc3_data->syscfg_reg_off = res->start;

	dev_info(&pdev->dev, "glue-logic addr 0x%p, syscfg-reg offset 0x%x\n",
		 dwc3_data->glue_base, dwc3_data->syscfg_reg_off);

	dwc3_data->rstc_p = devm_reset_control_get(dwc3_data->dev, "power");
	if (IS_ERR(dwc3_data->rstc_p))
		return PTR_ERR(dwc3_data->rstc_p);

	reset_control_deassert(dwc3_data->rstc_p);

	dwc3_data->rstc_s = devm_reset_control_get(dwc3_data->dev, "soft");
	if (IS_ERR(dwc3_data->rstc_s))
		return PTR_ERR(dwc3_data->rstc_s);

	reset_control_deassert(dwc3_data->rstc_s);
#ifdef MY_DEF_HERE
	memset(&cfg_ops, 0, sizeof(cfg_ops));

	if (of_find_property(node, "st,gsbuscfg", NULL)) {
		cfg_ops.dwc3_gsbuscfg = st_dwc3_gsbuscfg_setup;
	}

	ret = platform_device_add_data(pdev, &cfg_ops, sizeof(cfg_ops));

	if (ret) {
		dev_err(dwc3_data->dev, "failed to add platform data to dwc3 core\n");
		return ret;
	}
#endif  

	if (node) {
		st_dwc3_dt_get_pdata(pdev, dwc3_data);
		 
		ret = of_platform_populate(node, NULL, NULL, dev);
		if (ret) {
			dev_err(dev, "failed to add dwc3 core\n");
			return ret;
		}
	} else {
		dev_err(dev, "no device node, failed to add dwc3 core\n");
		return -ENODEV;
	}

	ret = st_dwc3_drd_init(dwc3_data);
	if (ret)
		return ret;

	dev_info(&pdev->dev, "configured as %s DRD\n",
		 dwc3_data->drd_device_conf ? "device" : "host");

	st_dwc3_init(dwc3_data);

	platform_set_drvdata(pdev, dwc3_data);

	dev_info(&pdev->dev, "probe exits fine...\n");

	return ret;
}

static int st_dwc3_remove_child(struct device *dev, void *c)
{
	struct platform_device *pdev = to_platform_device(dev);

	of_device_unregister(pdev);

	return 0;
}

static int st_dwc3_remove(struct platform_device *pdev)
{
	struct st_dwc3 *dwc3_data = platform_get_drvdata(pdev);

	device_for_each_child(&pdev->dev, NULL, st_dwc3_remove_child);
	reset_control_assert(dwc3_data->rstc_s);
	reset_control_assert(dwc3_data->rstc_p);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int st_dwc3_suspend(struct device *dev)
{
	struct st_dwc3 *dwc3_data = dev_get_drvdata(dev);

	reset_control_assert(dwc3_data->rstc_p);

	pinctrl_pm_select_sleep_state(dev);

	return 0;
}

static int st_dwc3_resume(struct device *dev)
{
	struct st_dwc3 *dwc3_data = dev_get_drvdata(dev);

	pinctrl_pm_select_default_state(dev);

	reset_control_deassert(dwc3_data->rstc_p);

	return 0;
}

static const struct dev_pm_ops st_dwc3_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(st_dwc3_suspend, st_dwc3_resume)
};

#define DEV_PM_OPS	(&st_dwc3_dev_pm_ops)
#else
#define DEV_PM_OPS	NULL
#endif  

static struct of_device_id st_dwc3_match[] = {
	{.compatible = "st,stih407-dwc3"},
	{.compatible = "st,sti8416-dwc3"},
	{},
};

MODULE_DEVICE_TABLE(of, st_dwc3_match);

static struct platform_driver st_dwc3_driver = {
	.probe = st_dwc3_probe,
	.remove = st_dwc3_remove,
	.driver = {
		.name = "usb-st-dwc3",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(st_dwc3_match),
		.pm	= DEV_PM_OPS,
	},
};

module_platform_driver(st_dwc3_driver);

MODULE_AUTHOR("Giuseppe Cavallaro <peppe.cavallaro@st.com>");
MODULE_DESCRIPTION("DesignWare USB3 STi Glue Layer");
MODULE_LICENSE("GPL v2");
