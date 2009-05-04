/*
 * ASoC driver for Xenon platform
 *
 * Author:      Troy Kisky, <troy.kisky@boundarydevices.com>
 * Copyright:   (C) 2008 Boundary Devices
 * based on davinici-evm.c by Vladimir Barinov
 * Copyright:   (C) 2007 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/dma.h>
#include <mach/hardware.h>
#include <mach/edma.h>
#include <mach/gpio.h>

#include "../codecs/tlv320aic23.h"
#include "davinci-pcm.h"
#include "davinci-i2s.h"

#define XENON_CODEC_CLOCK 12000000
#define MUTE_GPIO 7
#define MUTED 0
#define NOTMUTED 1

#if 1
#define AUDIO_FORMAT (SND_SOC_DAIFMT_DSP_A | \
	SND_SOC_DAIFMT_CBM_CFM | SND_SOC_DAIFMT_NB_NF)
#else
#define AUDIO_FORMAT (SND_SOC_DAIFMT_DSP_B | \
	SND_SOC_DAIFMT_CBM_CFM | SND_SOC_DAIFMT_NB_NF)
#endif

static int xenon_startup(struct snd_pcm_substream *substream)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		gpio_direction_output(MUTE_GPIO,NOTMUTED);
	}
	return 0;
}

static void xenon_shutdown(struct snd_pcm_substream *substream)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		gpio_direction_output(MUTE_GPIO,MUTED);
	}
}

static int xenon_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	int ret = 0;

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, AUDIO_FORMAT);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, AUDIO_FORMAT);
	if (ret < 0)
		return ret;

	/* set the codec system clock */
	ret = snd_soc_dai_set_sysclk(codec_dai, 0, XENON_CODEC_CLOCK,
					    SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;
	return 0;
}

static struct snd_soc_ops xenon_ops = {
	.startup = xenon_startup,
	.shutdown = xenon_shutdown,
	.hw_params = xenon_hw_params,
};

/* davinci-xenon machine dapm widgets */
static const struct snd_soc_dapm_widget aic23_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
};

/* davinci-xenon machine audio_mapnections to the codec pins */
static const struct snd_soc_dapm_route audio_map[] = {
	/* headphone connected to LHPOUT1, RHPOUT1 */
	{"Headphone Jack", NULL, "LHPOUT"},
	{"Headphone Jack", NULL, "RHPOUT"},

	/* speaker connected to LOUT, ROUT */
	{"Ext Spk", NULL, "ROUT"},
	{"Ext Spk", NULL, "LOUT"},

	/* mic is connected to MICIN (via right channel of headphone jack) */
	{"MICIN", NULL, "Mic Jack"},
};

/* Logic for a tlv320aic23 as connected on a davinci-xenon */
static int xenon_tlv320aic23_init(struct snd_soc_codec *codec)
{
//	snd_soc_dapm_disable_pin(codec, "LLINEIN");
//	snd_soc_dapm_disable_pin(codec, "RLINEIN");

	/* Add davinci-xenon specific widgets */
	snd_soc_dapm_new_controls(codec, aic23_dapm_widgets,
				  ARRAY_SIZE(aic23_dapm_widgets));

	/* Set up davinci-xenon specific audio path audio_map */
	snd_soc_dapm_add_routes(codec, audio_map, ARRAY_SIZE(audio_map));

	/* always connected */
	snd_soc_dapm_enable_pin(codec, "Headphone Jack");
	snd_soc_dapm_enable_pin(codec, "Mic Jack");
	snd_soc_dapm_enable_pin(codec, "Ext Spk");
	snd_soc_dapm_sync(codec);
	return 0;
}

/* davinci-xenon digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link xenon_dai = {
	.name = "TLV320AIC23",
	.stream_name = "AIC23",
	.cpu_dai = &davinci_i2s_dai,
	.codec_dai = &tlv320aic23_dai,
	.init = xenon_tlv320aic23_init,
	.ops = &xenon_ops,
};

/* davinci-xenon audio machine driver */
static struct snd_soc_machine snd_soc_machine_xenon = {
	.name = "DaVinci Xenon",
	.dai_link = &xenon_dai,
	.num_links = 1,
};

void davinci_spi_shutdown(void);
int davinci_spi_init(void);
int davinci_spi_hw_write(void *control_data,const char* data,int len);

/* xenon audio private data */
static struct tlv320aic23_setup_data xenon_tlv320aic23_setup = {
	.hw_write = davinci_spi_hw_write,
};

/* xenon audio subsystem */
static struct snd_soc_device xenon_snd_devdata = {
	.machine = &snd_soc_machine_xenon,
	.platform = &davinci_soc_platform,
	.codec_dev = &tlv320aic23_soc_codec_dev,
	.codec_data = &xenon_tlv320aic23_setup,
};

#define DAVINCI_MCBSP_BASE	(0x01E02000)
static struct resource xenon_snd_resources[] = {
	{
		.start = DAVINCI_MCBSP_BASE,
		.end = DAVINCI_MCBSP_BASE + SZ_8K - 1,
		.flags = IORESOURCE_MEM,
	},
};

static struct evm_snd_platform_data xenon_snd_data = {
	.tx_dma_ch	= DAVINCI_DMA_MCBSP_TX,
	.rx_dma_ch	= DAVINCI_DMA_MCBSP_RX,
};

static struct platform_device *xenon_snd_device;


static int __init xenon_init(void)
{
	int ret;

	ret = davinci_spi_init();
	if (ret < 0)
		return ret;
	xenon_snd_device = platform_device_alloc("soc-audio", 0);
	if (!xenon_snd_device)
		return -ENOMEM;

	platform_set_drvdata(xenon_snd_device, &xenon_snd_devdata);
	xenon_snd_devdata.dev = &xenon_snd_device->dev;
	xenon_snd_device->dev.platform_data = &xenon_snd_data;

	ret = platform_device_add_resources(xenon_snd_device, xenon_snd_resources,
					    ARRAY_SIZE(xenon_snd_resources));
	if (ret) {
		platform_device_put(xenon_snd_device);
		return ret;
	}

	ret = platform_device_add(xenon_snd_device);
	if (ret)
		platform_device_put(xenon_snd_device);

	return ret;
}

static void __exit xenon_exit(void)
{
	platform_device_unregister(xenon_snd_device);
	davinci_spi_shutdown();
}

module_init(xenon_init);
module_exit(xenon_exit);

MODULE_AUTHOR("Troy Kisky");
MODULE_DESCRIPTION("Xenon ASoC driver");
MODULE_LICENSE("GPL");
