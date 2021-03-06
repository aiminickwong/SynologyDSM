#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/module.h>
#include <sound/soc.h>

#include "s3c24xx_simtec.h"

static const struct snd_soc_dapm_widget dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_LINE("Line In", NULL),
	SND_SOC_DAPM_LINE("Line Out", NULL),
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
};

static const struct snd_soc_dapm_route base_map[] = {
	{ "Headphone Jack", NULL, "LHPOUT"},
	{ "Headphone Jack", NULL, "RHPOUT"},

	{ "Line Out", NULL, "LOUT" },
	{ "Line Out", NULL, "ROUT" },

	{ "LLINEIN", NULL, "Line In"},
	{ "RLINEIN", NULL, "Line In"},

	{ "MICIN", NULL, "Mic Jack"},
};

static int simtec_tlv320aic23_init(struct snd_soc_pcm_runtime *rtd)
{
#if defined(MY_ABC_HERE)
	 
#else  
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	snd_soc_dapm_enable_pin(dapm, "Headphone Jack");
	snd_soc_dapm_enable_pin(dapm, "Line In");
	snd_soc_dapm_enable_pin(dapm, "Line Out");
	snd_soc_dapm_enable_pin(dapm, "Mic Jack");
#endif  

	simtec_audio_init(rtd);

	return 0;
}

static struct snd_soc_dai_link simtec_dai_aic23 = {
	.name		= "tlv320aic23",
	.stream_name	= "TLV320AIC23",
	.codec_name	= "tlv320aic3x-codec.0-001a",
	.cpu_dai_name	= "s3c24xx-iis",
	.codec_dai_name = "tlv320aic3x-hifi",
	.platform_name	= "s3c24xx-iis",
	.init		= simtec_tlv320aic23_init,
};

static struct snd_soc_card snd_soc_machine_simtec_aic23 = {
	.name		= "Simtec",
	.owner		= THIS_MODULE,
	.dai_link	= &simtec_dai_aic23,
	.num_links	= 1,

	.dapm_widgets	= dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(dapm_widgets),
	.dapm_routes	= base_map,
	.num_dapm_routes = ARRAY_SIZE(base_map),
};

static int simtec_audio_tlv320aic23_probe(struct platform_device *pd)
{
	return simtec_audio_core_probe(pd, &snd_soc_machine_simtec_aic23);
}

static struct platform_driver simtec_audio_tlv320aic23_driver = {
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "s3c24xx-simtec-tlv320aic23",
		.pm	= simtec_audio_pm,
	},
	.probe	= simtec_audio_tlv320aic23_probe,
	.remove	= simtec_audio_remove,
};

module_platform_driver(simtec_audio_tlv320aic23_driver);

MODULE_ALIAS("platform:s3c24xx-simtec-tlv320aic23");
MODULE_AUTHOR("Ben Dooks <ben@simtec.co.uk>");
MODULE_DESCRIPTION("ALSA SoC Simtec Audio support");
MODULE_LICENSE("GPL");
