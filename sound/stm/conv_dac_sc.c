#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <sound/core.h>
#include <sound/info.h>
#include "common.h"
#include <linux/reset.h>

static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);

#define FORMAT (SND_STM_FORMAT__I2S | SND_STM_FORMAT__SUBFRAME_32_BITS)
#define OVERSAMPLING 256

static struct of_device_id conv_dac_sc_match[];

enum {
	CONVDACSC_NOT_STANDBY,
	CONVDACSC_STANDBY,
	CONVDACSC_SOFTMUTE,
	CONVDACSC_MUTE_L,
	CONVDACSC_MUTE_R,
	CONVDACSC_MODE,
	CONVDACSC_ANALOG_NOT_PWR_DW,
	CONVDACSC_ANALOG_STANDBY,
	CONVDACSC_ANALOG_PWR_DW,
	CONVDACSC_ANALOG_NOT_PWR_DW_BG,
	 
	MAX_REGFIELDS
};

struct conv_dac_sc_sysconf_regfields {
	bool avail;
	struct reg_field regfield;
};

struct conv_dac_sc {
	struct device *dev;

	struct snd_stm_conv_converter *converter;
	const char *bus_id;
	struct device_node *source_bus_id;
	const char *source_bus_id_description;

	int channel_to;
	int channel_from;

	unsigned int format;
	int oversampling;
	struct reset_control *nrst;

	struct regmap *sysconf_regmap;
	 
	const struct conv_dac_sc_sysconf_regfields *sysconf_regfields;
	 
	struct regmap_field *mode;
	struct regmap_field *nsb;
	struct regmap_field *sb;
	struct regmap_field *softmute;
	struct regmap_field *mute_l;
	struct regmap_field *mute_r;
	struct regmap_field *sbana;
	struct regmap_field *npdana;
	struct regmap_field *pdana;
	struct regmap_field *pndbg;

	snd_stm_magic_field;
};

static const struct conv_dac_sc_sysconf_regfields
	conv_dac_sc_sysconf_regfields_stih416[MAX_REGFIELDS] = {
	[CONVDACSC_NOT_STANDBY] = {true, REG_FIELD(STIH416_AUDIO_DAC_CTRL, 3, 3)},
	[CONVDACSC_SOFTMUTE] = {true, REG_FIELD(STIH416_AUDIO_DAC_CTRL, 4, 4)},
	[CONVDACSC_MODE] = {true, REG_FIELD(STIH416_AUDIO_DAC_CTRL, 1, 2)},
	[CONVDACSC_ANALOG_PWR_DW] = {true, REG_FIELD(STIH416_AUDIO_DAC_CTRL, 5, 5)},
	[CONVDACSC_ANALOG_NOT_PWR_DW_BG] = {true, REG_FIELD(STIH416_AUDIO_DAC_CTRL, 6, 6)},
};

static const struct conv_dac_sc_sysconf_regfields
	conv_dac_sc_sysconf_regfields_stih407[MAX_REGFIELDS] = {
	[CONVDACSC_STANDBY] = {true, REG_FIELD(STIH407_AUDIO_DAC_CTRL, 2, 2)},
	[CONVDACSC_SOFTMUTE] = {true, REG_FIELD(STIH407_AUDIO_DAC_CTRL, 0, 0)},
	[CONVDACSC_ANALOG_STANDBY] = {true, REG_FIELD(STIH407_AUDIO_DAC_CTRL, 1, 1)},
};

static unsigned int conv_dac_sc_get_format(void *priv)
{
	struct conv_dac_sc *conv = priv;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	dev_dbg(conv->dev, "%s(priv=%p)", __func__, priv);

	return conv->format;
}

static int conv_dac_sc_get_oversampling(void *priv)
{
	struct conv_dac_sc *conv = priv;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	dev_dbg(conv->dev, "%s(priv=%p)", __func__, priv);

	return conv->oversampling;
}

static int conv_dac_sc_set_enabled(int enabled, void *priv)
{
	struct conv_dac_sc *conv = priv;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	dev_dbg(conv->dev, "%s(enabled=%d, priv=%p)", __func__, enabled, priv);

	dev_dbg(conv->dev, "%sabling DAC for %s",
			enabled ? "En" : "Dis", conv->bus_id);

	if (enabled) {
#ifdef MY_DEF_HERE
		 
		if (conv->sbana)
			regmap_field_write(conv->sbana, 0);
#endif  
		 
		if (conv->nsb)
			regmap_field_write(conv->nsb, 1);
		if (conv->sb)
			regmap_field_write(conv->sb, 0);

		if (conv->nrst)
			reset_control_deassert(conv->nrst);
	} else {
		 
		if (conv->nrst)
			reset_control_assert(conv->nrst);

		if (conv->nsb)
			regmap_field_write(conv->nsb, 0);
		if (conv->sb)
			regmap_field_write(conv->sb, 1);
#ifdef MY_DEF_HERE
		 
		if (conv->sbana)
			regmap_field_write(conv->sbana, 1);
#endif  
	}

	return 0;
}

static int conv_dac_sc_set_muted(int muted, void *priv)
{
	struct conv_dac_sc *conv = priv;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	dev_dbg(conv->dev, "%s(muted=%d, priv=%p)", __func__, muted, priv);

	dev_dbg(conv->dev, "%suting DAC for %s", muted ? "M" : "Unm",
			conv->bus_id);

	if (conv->softmute)
		regmap_field_write(conv->softmute, muted ? 1 : 0);
	if (conv->mute_l)
		regmap_field_write(conv->mute_l, muted ? 1 : 0);
	if (conv->mute_r)
		regmap_field_write(conv->mute_r, muted ? 1 : 0);

	return 0;
}

static struct snd_stm_conv_ops conv_dac_sc_ops = {
	.get_format	  = conv_dac_sc_get_format,
	.get_oversampling = conv_dac_sc_get_oversampling,
	.set_enabled	  = conv_dac_sc_set_enabled,
	.set_muted	  = conv_dac_sc_set_muted,
};

#ifdef MY_DEF_HERE
static int conv_dac_sc_set_idle(struct conv_dac_sc *conv)
{
	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	if (conv->nrst)
		reset_control_assert(conv->nrst);

	if (conv->nsb)
		regmap_field_write(conv->nsb, 0);
	if (conv->sb)
		regmap_field_write(conv->sb, 1);

	if (conv->softmute)
		regmap_field_write(conv->softmute, 1);
	if (conv->mute_l)
		regmap_field_write(conv->mute_l, 1);
	if (conv->mute_r)
		regmap_field_write(conv->mute_r, 1);

	if (conv->mode)
		regmap_field_write(conv->mode, 0);
	if (conv->npdana)
		regmap_field_write(conv->npdana, 0);
	if (conv->sbana)
		regmap_field_write(conv->sbana, 1);
	if (conv->pdana)
		regmap_field_write(conv->pdana, 1);
	if (conv->pndbg)
		regmap_field_write(conv->pndbg, 1);

	return 0;
}
#endif  

static int conv_dac_sc_register(struct snd_device *snd_device)
{
	struct conv_dac_sc *conv = snd_device->device_data;

#ifdef MY_DEF_HERE
#else  
	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));
#endif  

	dev_dbg(conv->dev, "%s(snd_device=%p)", __func__, snd_device);

#ifdef MY_DEF_HERE
	return conv_dac_sc_set_idle(conv);
#else  
	 
	if (conv->nrst)
		reset_control_assert(conv->nrst);

	if (conv->nsb)
		regmap_field_write(conv->nsb, 0);
	if (conv->sb)
		regmap_field_write(conv->sb, 1);

	if (conv->softmute)
		regmap_field_write(conv->softmute, 1);
	if (conv->mute_l)
		regmap_field_write(conv->mute_l, 1);
	if (conv->mute_r)
		regmap_field_write(conv->mute_r, 1);

	if (conv->mode)
		regmap_field_write(conv->mode, 0);
	if (conv->npdana)
		regmap_field_write(conv->npdana, 0);
	if (conv->sbana)
		regmap_field_write(conv->sbana, 0);
	if (conv->pdana)
		regmap_field_write(conv->pdana, 1);
	if (conv->pndbg)
		regmap_field_write(conv->pndbg, 1);

	return 0;
#endif  
}

static int conv_dac_sc_disconnect(struct snd_device *snd_device)
{
	struct conv_dac_sc *conv = snd_device->device_data;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	dev_dbg(conv->dev, "%s(snd_device=%p)", __func__, snd_device);

	if (conv->nrst)
		reset_control_assert(conv->nrst);

	if (conv->nsb)
		regmap_field_write(conv->nsb, 0);
	if (conv->sb)
		regmap_field_write(conv->sb, 1);

	if (conv->softmute)
		regmap_field_write(conv->softmute, 1);
	if (conv->mute_l)
		regmap_field_write(conv->mute_l, 1);
	if (conv->mute_r)
		regmap_field_write(conv->mute_r, 1);

	if (conv->mode)
		regmap_field_write(conv->mode, 0);
	if (conv->npdana)
		regmap_field_write(conv->npdana, 1);
	if (conv->sbana)
		regmap_field_write(conv->sbana, 1);
	if (conv->pdana)
		regmap_field_write(conv->pdana, 0);
	if (conv->pndbg)
		regmap_field_write(conv->pndbg, 0);

	return 0;
}

static struct snd_device_ops conv_dac_sc_snd_device_ops = {
	.dev_register	= conv_dac_sc_register,
	.dev_disconnect	= conv_dac_sc_disconnect,
};

static int conv_dac_sc_parse_dt(struct platform_device *pdev,
		struct conv_dac_sc *conv)
{
	struct device_node *pnode = pdev->dev.of_node;
	int val;

	val = get_property_hdl(&pdev->dev, pnode, "source-bus-id", 0);
	if (!val) {
		dev_err(&pdev->dev, "unable to find DT source-bus-id node");
		return -EINVAL;
	}
	conv->source_bus_id = of_find_node_by_phandle(val);
	of_property_read_string(conv->source_bus_id, "description",
				&conv->source_bus_id_description);

	of_property_read_u32(pnode, "channel-to", &conv->channel_to);
	of_property_read_u32(pnode, "channel-from", &conv->channel_from);
	of_property_read_u32(pnode, "format", &conv->format);
	of_property_read_u32(pnode, "oversampling", &conv->oversampling);

	conv->sysconf_regmap =
		syscon_regmap_lookup_by_phandle(pnode, "st,syscfg");
	if (!conv->sysconf_regmap) {
		dev_err(conv->dev, "No syscfg phandle specified\n");
		return -EINVAL;
	}

	conv->sysconf_regfields =
		of_match_node(conv_dac_sc_match, pnode)->data;
	if (!conv->sysconf_regfields) {
		dev_err(conv->dev, "compatible data not provided\n");
		return -EINVAL;
	}

	return 0;
}

static void conv_dac_sc_sysconf_claim(struct conv_dac_sc *conv)
{

	if (conv->sysconf_regfields[CONVDACSC_NOT_STANDBY].avail == true)
		conv->nsb = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_NOT_STANDBY].regfield);
	if (conv->sysconf_regfields[CONVDACSC_STANDBY].avail == true)
		conv->sb = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_STANDBY].regfield);

	if (conv->sysconf_regfields[CONVDACSC_SOFTMUTE].avail == true)
		conv->softmute = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_SOFTMUTE].regfield);
	if (conv->sysconf_regfields[CONVDACSC_MUTE_L].avail == true)
		conv->mute_l = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_MUTE_L].regfield);
	if (conv->sysconf_regfields[CONVDACSC_MUTE_R].avail == true)
		conv->mute_r = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_MUTE_R].regfield);

	if (conv->sysconf_regfields[CONVDACSC_MODE].avail == true)
		conv->mode = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_MODE].regfield);

	if (conv->sysconf_regfields[CONVDACSC_ANALOG_NOT_PWR_DW].avail == true)
		conv->npdana = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_ANALOG_NOT_PWR_DW].regfield);
	if (conv->sysconf_regfields[CONVDACSC_ANALOG_STANDBY].avail == true)
		conv->sbana = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_ANALOG_STANDBY].regfield);
	if (conv->sysconf_regfields[CONVDACSC_ANALOG_PWR_DW].avail == true)
		conv->pdana = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_ANALOG_PWR_DW].regfield);
	if (conv->sysconf_regfields[CONVDACSC_ANALOG_NOT_PWR_DW_BG].avail == true)
		conv->pndbg = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_ANALOG_NOT_PWR_DW_BG].regfield);
}

static int conv_dac_sc_probe(struct platform_device *pdev)
{
	int result = 0;
	struct conv_dac_sc *conv;
	struct snd_card *card = snd_stm_card_get(SND_STM_CARD_TYPE_AUDIO);

	BUG_ON(!card);

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	conv = devm_kzalloc(&pdev->dev, sizeof(*conv), GFP_KERNEL);
	if (!conv) {
		dev_err(&pdev->dev, "Failed to allocate driver structure");
		return -ENOMEM;
	}

	snd_stm_magic_set(conv);
	conv->dev = &pdev->dev;
	conv->bus_id = dev_name(&pdev->dev);

	if (!pdev->dev.of_node) {
		dev_err(conv->dev, "device not in dt");
		return -EINVAL;
	}

	conv->nrst = devm_reset_control_get(&pdev->dev, "dac_nrst");
	if (IS_ERR(conv->nrst)) {
		dev_info(conv->dev, "audio dac soft reset control not defined\n");
		conv->nrst = NULL;
	}

	if (conv_dac_sc_parse_dt(pdev, conv)) {
		dev_err(conv->dev, "invalid dt");
		return -EINVAL;
	}

	if (!conv->source_bus_id) {
		dev_err(conv->dev, "Invalid source bus id!");
		return -EINVAL;
	}

	if (conv->format == 0)
		conv->format = FORMAT;

	if (conv->oversampling == 0)
		conv->oversampling = OVERSAMPLING;

	conv_dac_sc_sysconf_claim(conv);

	if (!conv->nsb && !conv->sb) {
		dev_err(conv->dev, "Failed to claim any standby sysconf!");
		return -EINVAL;
	}

	if (!conv->softmute && !conv->mute_l && !conv->mute_r) {
		dev_err(conv->dev, "Failed to claim any mute sysconf!");
		return -EINVAL;
	}

	dev_dbg(&pdev->dev, "Attached to %s", conv->source_bus_id_description);

	conv->converter = snd_stm_conv_register_converter(
			"DAC SC", &conv_dac_sc_ops, conv,
			&platform_bus_type, conv->source_bus_id,
			conv->channel_from, conv->channel_to, NULL);
	if (!conv->converter) {
		dev_err(conv->dev, "Can't attach to player!");
		result = -ENODEV;
		goto error_attach;
	}

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, conv,
			&conv_dac_sc_snd_device_ops);
	if (result < 0) {
		dev_err(conv->dev, "ALSA low-level device creation failed!");
		goto error_device;
	}

	platform_set_drvdata(pdev, conv);

	return 0;

error_device:
	snd_stm_conv_unregister_converter(conv->converter);
error_attach:
	snd_stm_magic_clear(conv);
	return result;
}

static int conv_dac_sc_remove(struct platform_device *pdev)
{
	struct conv_dac_sc *conv = platform_get_drvdata(pdev);

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	snd_stm_conv_unregister_converter(conv->converter);

	if (conv->nsb)
		regmap_field_free(conv->nsb);
	if (conv->sb)
		regmap_field_free(conv->sb);
	if (conv->softmute)
		regmap_field_free(conv->softmute);
	if (conv->mute_l)
		regmap_field_free(conv->mute_l);
	if (conv->mute_r)
		regmap_field_free(conv->mute_r);
	if (conv->mode)
		regmap_field_free(conv->mode);
	if (conv->sbana)
		regmap_field_free(conv->sbana);
	if (conv->npdana)
		regmap_field_free(conv->npdana);
	if (conv->pdana)
		regmap_field_free(conv->pdana);
	if (conv->pndbg)
		regmap_field_free(conv->pndbg);

	snd_stm_magic_clear(conv);

	return 0;
}

#ifdef MY_DEF_HERE
#ifdef CONFIG_PM_SLEEP
static int conv_dac_sc_resume(struct device *dev)
{
	struct conv_dac_sc *conv = dev_get_drvdata(dev);

	dev_dbg(dev, "%s(dev=%p)", __func__, dev);

	return conv_dac_sc_set_idle(conv);
}

SIMPLE_DEV_PM_OPS(conv_dac_sc_pm_ops, NULL, conv_dac_sc_resume);

#define CONV_DAC_SC_PM_OPS	(&conv_dac_sc_pm_ops)
#else
#define CONV_DAC_SC_PM_OPS	NULL
#endif
#endif  

static struct of_device_id conv_dac_sc_match[] = {
	{
		.compatible = "st,snd_conv_dac_sc_stih416",
		.data = conv_dac_sc_sysconf_regfields_stih416
	},
	{
		.compatible = "st,snd_conv_dac_sc_stih407",
		.data = conv_dac_sc_sysconf_regfields_stih407
	},
	{},
};

MODULE_DEVICE_TABLE(of, conv_dac_sc_match);

static struct platform_driver conv_dac_sc_platform_driver = {
	.driver.name	= "snd_conv_dac_sc",
	.driver.of_match_table = conv_dac_sc_match,
#ifdef MY_DEF_HERE
	.driver.pm	= CONV_DAC_SC_PM_OPS,
#endif  
	.probe		= conv_dac_sc_probe,
	.remove		= conv_dac_sc_remove,
};

module_platform_driver(conv_dac_sc_platform_driver);

MODULE_AUTHOR("John Boddie <john.boddie@st.com>");
MODULE_DESCRIPTION("STMicroelectronics sysconf-based DAC converter driver");
MODULE_LICENSE("GPL");
