/*
 * ALSA SoC TLV320AIC23 codec driver
 *
 * Author:      Arun KS, <arunks@mistralsolutions.com>
 * Copyright:   (C) 2008 Mistral Solutions Pvt Ltd.,
 *
 * Based on sound/soc/codecs/wm8731.c by Richard Purdie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Notes:
 *  The AIC23 is a driver for a low power stereo audio
 *  codec tlv320aic23
 *
 *  The machine layer should disable unsupported inputs/outputs by
 *  snd_soc_dapm_disable_pin(codec, "LHPOUT"), etc.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include "tlv320aic23.h"

#define AIC23_VERSION "0.1"

/* AIC23 driver private data */
struct aic23 {
	struct snd_soc_codec codec;
	int mclk;
	int master;
	unsigned short datfm;
	unsigned short digital_audio_format;
	unsigned short sample_rate_control;
	unsigned short left_output_volume;
	unsigned short right_output_volume;
	int requested_adc;
	int requested_dac;
	struct work_struct deferred_trigger_work;
	unsigned char right_first;
#define ACTIVE_PLAYBACK 1
#define ACTIVE_CAPTURE 2
	unsigned char active_mask;
	unsigned char volume_mute;
	u16 reg_cache[AIC23_NUM_CACHE_REGS];	/* shadow registers */
};


/* ---------------------------------------------------------------------
 * Register access routines
 */
static unsigned int tlv320aic23_read_cache(struct snd_soc_codec *codec,
					 unsigned int reg)
{
	if (reg >= AIC23_NUM_CACHE_REGS) {
		printk(KERN_ERR "%s: Invalid register %i\n", __func__, reg);
		return -EINVAL;
	}
	return ((u16*)codec->reg_cache)[reg];
}

/* This is a write-only codec */
/*
static unsigned int tlv320aic23_read(struct snd_soc_codec *codec,
				   unsigned int reg)
{
	return tlv320aic23_read_cache(codec,reg);
}
*/

static int tlv320aic23_write1(struct snd_soc_codec *codec, unsigned int reg,
			   unsigned int value)
{
	u8 data[2];
	printk(KERN_DEBUG "\n%s: reg=%i val=0x%x\n", __func__, reg, value);
	/* update cache */
	if (reg < AIC23_NUM_CACHE_REGS)
		((u16*)codec->reg_cache)[reg] = value;
	/* data is
	 *   D15..D9 WM8731 register offset
	 *   D8...D0 register data
	 */
	value &= 0x1ff;
	value |= (reg<<9);
	/* store high order byte 1st, (big endian mode) */
	data[0] = (u8)(value>>8);
	data[1] = (u8)(value);

	if (codec->hw_write(codec->control_data, data, 2) == 2)
		return 0;
	else
		return -EIO;
}

static int tlv320aic23_write(struct snd_soc_codec *codec, unsigned int reg,
			   unsigned int value)
{
	struct aic23 *aic23 = container_of(codec, struct aic23, codec);

	if (reg >= AIC23_NUM_REGS) {
		printk(KERN_ERR "%s: Invalid register %i\n", __func__, reg);
		return -EINVAL;
	}
	if (reg==TLV320AIC23_LCHNVOL) {
		aic23->volume_mute = 0;
		aic23->left_output_volume = value;
	} else if (reg==TLV320AIC23_RCHNVOL) {
		aic23->volume_mute = 0;
		aic23->right_output_volume = value;
	}
	return tlv320aic23_write1(codec, reg, value);
}

static int tlv320aic23_set(struct snd_soc_codec *codec, unsigned int reg, unsigned int new)
{
	unsigned int prev = tlv320aic23_read_cache(codec, reg);
	if (new != prev) {
		tlv320aic23_write1(codec, reg, new);
		return 1;
	}
	return 0;
}

static int tlv320aic23_modify(struct snd_soc_codec *codec, unsigned int reg,
		unsigned int clear, unsigned int set)
{
	unsigned int prev = tlv320aic23_read_cache(codec, reg);
	unsigned int new = (prev & ~clear) | set; 
	if (new != prev) {
		tlv320aic23_write1(codec, reg, new);
		return 1;
	}
	return 0;
}

/* ---------------------------------------------------------------------
 * ALSA controls
 */
static const char *rec_src_text[] = { "Line In", "Mic" };
static const char *deemph_text[] = {"None", "32Khz", "44.1Khz", "48Khz"};
static const char *sidetone_text[] = {"-6db","-9db","-12db","-18db","0db"};

static const struct soc_enum tlv320aic23_enum[] = {
	SOC_ENUM_SINGLE(TLV320AIC23_ANLG, AAC_INSEL_BIT, \
			2, rec_src_text),
	SOC_ENUM_SINGLE(TLV320AIC23_ANLG, AAC_SIDE_TONE_VOL_BIT, \
			5, sidetone_text),
	SOC_ENUM_SINGLE(TLV320AIC23_DIGT, DAC_DEEMP_BIT, \
			4, deemph_text),
};

static const struct snd_kcontrol_new tlv320aic23_snd_controls[] = {
	SOC_DOUBLE_R("Capture Volume", TLV320AIC23_LINVOL, \
		TLV320AIC23_RINVOL, 0, 0x1f, 0),
	SOC_DOUBLE_R("Line Capture Switch", TLV320AIC23_LINVOL, \
		TLV320AIC23_RINVOL, LRIV_MUTE_BIT, 1, 1),
	SOC_DOUBLE_R("Master Playback Volume", TLV320AIC23_LCHNVOL, \
		TLV320AIC23_RCHNVOL, 0, 0x7f, 0),
	SOC_DOUBLE_R("Master Playback ZC Switch", TLV320AIC23_LCHNVOL, \
		TLV320AIC23_RINVOL, LROV_ZERO_CROSS_BIT, 1, 0),
	SOC_SINGLE("Mic Boost (+20dB)", TLV320AIC23_ANLG,
		AAC_MIC_BOOST_BIT, 1, 0),
	SOC_SINGLE("Capture Mic Switch", TLV320AIC23_ANLG, \
		AAC_MIC_MUTE_BIT, 1, 1),
	SOC_ENUM("Sidetone Playback Volume", tlv320aic23_enum[1]),

	SOC_SINGLE("Digital Playback Switch", TLV320AIC23_DIGT, DAC_SOFT_MUTE_BIT, 1, 1),
	SOC_SINGLE("ADC High Pass Filter Switch", TLV320AIC23_DIGT, \
		DAC_HIGH_PASS_FILTER_BIT, 1, 1),
	SOC_ENUM("Playback De-emphasis", tlv320aic23_enum[2]),
};

/* add non dapm controls */
static int tlv320aic23_add_controls(struct snd_soc_codec *codec)
{
	int err, i;

	for (i = 0; i < ARRAY_SIZE(tlv320aic23_snd_controls); i++) {
		err = snd_ctl_add(codec->card,
				  snd_soc_cnew(&tlv320aic23_snd_controls[i],
						codec, NULL));
		if (err < 0)
			return err;
	}
	return 0;
}

#define FAKE_DAC_ON_BIT 9
/* Output Mixer */
static const struct snd_kcontrol_new tlv320aic23_output_mixer_controls[] = {
SOC_DAPM_SINGLE("HiFi Playback Switch", TLV320AIC23_ANLG, \
	FAKE_DAC_ON_BIT, 1, 0), /* use a fake bit(9), and control in driver */
SOC_DAPM_SINGLE("Line Bypass Switch", TLV320AIC23_ANLG, \
	AAC_BYPASS_BIT, 1, 0),
SOC_DAPM_SINGLE("Mic Sidetone Switch", TLV320AIC23_ANLG, \
	AAC_SIDE_TONE_ENABLE_BIT, 1, 0),
};

/* Input mux */
static const struct snd_kcontrol_new tlv320aic23_input_mux_controls =
SOC_DAPM_ENUM("Input Select", tlv320aic23_enum[0]);

static const struct snd_soc_dapm_widget tlv320aic23_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("DAC", "HiFi Playback", TLV320AIC23_PWR, PDC_DAC_BIT, 1),
	SND_SOC_DAPM_ADC("ADC", "HiFi Capture", TLV320AIC23_PWR, PDC_ADC_BIT, 1),
	SND_SOC_DAPM_MUX("Capture Source", SND_SOC_NOPM, 0, 0,
			&tlv320aic23_input_mux_controls),
	SND_SOC_DAPM_MIXER("Output Mixer", TLV320AIC23_PWR, PDC_OUT_BIT, 1,
			&tlv320aic23_output_mixer_controls[0],
			ARRAY_SIZE(tlv320aic23_output_mixer_controls)),
	SND_SOC_DAPM_PGA("Line Input", TLV320AIC23_PWR, PDC_LINE_BIT, 1, NULL, 0),
	SND_SOC_DAPM_MICBIAS("Mic Bias", TLV320AIC23_PWR, PDC_MIC_BIT, 1),

	SND_SOC_DAPM_OUTPUT("LHPOUT"),
	SND_SOC_DAPM_OUTPUT("RHPOUT"),
	SND_SOC_DAPM_OUTPUT("LOUT"),
	SND_SOC_DAPM_OUTPUT("ROUT"),

	SND_SOC_DAPM_INPUT("LLINEIN"),
	SND_SOC_DAPM_INPUT("RLINEIN"),

	SND_SOC_DAPM_INPUT("MICIN"),
};

static const struct snd_soc_dapm_route intercon[] = {
	/* Output Mixer */
	{"Output Mixer", "HiFi Playback Switch", "DAC"},
	{"Output Mixer", "Line Bypass Switch", "Line Input"},
	{"Output Mixer", "Mic Sidetone Switch", "Mic Bias"},

	/* Outputs */
	{"RHPOUT", NULL, "Output Mixer"},
	{"LHPOUT", NULL, "Output Mixer"},
	{"LOUT", NULL, "Output Mixer"},
	{"ROUT", NULL, "Output Mixer"},

	/* Inputs */
	{"Line Input", NULL, "LLINEIN"},
	{"Line Input", NULL, "RLINEIN"},
	{"Mic Bias", NULL, "MICIN"},

	/* input mux */
	{"Capture Source", "Line In", "Line Input"},
	{"Capture Source", "Mic", "Mic Bias"},
	{"ADC", NULL, "Capture Source"},
};

/*
 * Common Crystals used
 * 11.2896 Mhz /128 = *88.2k  /192 = 58.8k
 * 12.0000 Mhz /125 = *96k    /136 = 88.235K
 * 12.2880 Mhz /128 = *96k    /192 = 64k
 * 16.9344 Mhz /128 = 132.3k /192 = *88.2k
 * 18.4320 Mhz /128 = 144k   /192 = *96k
 */

/*
 * Normal BOSR 0-256/2 = 128, 1-384/2 = 192
 * USB BOSR 0-250/2 = 125, 1-272/2 = 136
 */
static const int bosr_usb_divisor_table[] = {
	128, 125, 192, 136
};
#define LOWER_GROUP ((1<<0) | (1<<1) | (1<<2) | (1<<3) | (1<<6) | (1<<7))
#define UPPER_GROUP ((1<<8) | (1<<9) | (1<<10) | (1<<11)        | (1<<15))
static const unsigned short sr_valid_mask[] = {
	LOWER_GROUP|UPPER_GROUP,	/* Normal, bosr - 0*/
	LOWER_GROUP|UPPER_GROUP,	/* Normal, bosr - 1*/
	LOWER_GROUP,			/* Usb, bosr - 0*/
	UPPER_GROUP,			/* Usb, bosr - 1*/
};
/*
 * Every divisor is a factor of 11*12
 */
#define SR_MULT (11*12)
#define A(x) (x) ? (SR_MULT/x) : 0
static const unsigned char sr_adc_mult_table[] = {
	A(2), A(2), A(12), A(12),  A(0), A(0), A(3), A(1),
	A(2), A(2), A(11), A(11),  A(0), A(0), A(0), A(1)
};
static const unsigned char sr_dac_mult_table[] = {
	A(2), A(12), A(2), A(12),  A(0), A(0), A(3), A(1),
	A(2), A(11), A(2), A(11),  A(0), A(0), A(0), A(1)
};

static unsigned get_score(int adc, int adc_l, int adc_h, int need_adc,
		int dac, int dac_l, int dac_h, int need_dac)
{
	if ((adc >= adc_l) && (adc <= adc_h) &&
			(dac >= dac_l) && (dac <= dac_h)) {
		int diff_adc = need_adc - adc;
		int diff_dac = need_dac - dac;
		return abs(diff_adc) + abs(diff_dac);
	}
	return UINT_MAX;
}

static int find_rate(int mclk, u32 need_adc, u32 need_dac)
{
	int i, j;
	int best_i = -1;
	int best_j = -1;
	int best_div = 0;
	unsigned best_score = UINT_MAX;
	int adc_l, adc_h, dac_l, dac_h;

	need_adc *= SR_MULT;
	need_dac *= SR_MULT;
	/*
	 * rates given are +/- 1/32
	 */
	adc_l = need_adc - (need_adc >> 5);
	adc_h = need_adc + (need_adc >> 5);
	dac_l = need_dac - (need_dac >> 5);
	dac_h = need_dac + (need_dac >> 5);
	for (i = 0; i < ARRAY_SIZE(bosr_usb_divisor_table); i++) {
		int base = mclk / bosr_usb_divisor_table[i];
		int mask = sr_valid_mask[i];
		for (j = 0; j < ARRAY_SIZE(sr_adc_mult_table);
				j++, mask >>= 1) {
			int adc;
			int dac;
			int score;
			if ((mask & 1) == 0)
				continue;
			adc = base * sr_adc_mult_table[j];
			dac = base * sr_dac_mult_table[j];
			score = get_score(adc, adc_l, adc_h, need_adc,
					dac, dac_l, dac_h, need_dac);
			if (best_score > score) {
				best_score = score;
				best_i = i;
				best_j = j;
				best_div = 0;
			}
			score = get_score((adc >> 1), adc_l, adc_h, need_adc,
					(dac >> 1), dac_l, dac_h, need_dac);
			/* prefer to have a /2 */
			if ((score != UINT_MAX) && (best_score >= score)) {
				best_score = score;
				best_i = i;
				best_j = j;
				best_div = 1;
			}
		}
	}
	return (best_j << 2) | best_i | (best_div << TLV320AIC23_CLKIN_SHIFT);
}

#ifdef DEBUG
static void get_current_sample_rates(int sample_rate_control, int mclk,
		u32 *sample_rate_adc, u32 *sample_rate_dac)
{
	int sr = (sample_rate_control >> 2) & 0x0f;
	int val = (mclk / bosr_usb_divisor_table[sample_rate_control & 3]);
	int adc = (val * sr_adc_mult_table[sr]) / SR_MULT;
	int dac = (val * sr_dac_mult_table[sr]) / SR_MULT;
	if (sample_rate_control & TLV320AIC23_CLKIN_HALF) {
		adc >>= 1;
		dac >>= 1;
	}
	*sample_rate_adc = adc;
	*sample_rate_dac = dac;
}
#endif

static int set_sample_rate_control(struct snd_soc_codec *codec, int mclk,
		u32 sample_rate_adc, u32 sample_rate_dac)
{
	struct aic23 *aic23 = container_of(codec, struct aic23, codec);
	/* Search for the right sample rate */
	int data = find_rate(mclk, sample_rate_adc, sample_rate_dac);
	if (data < 0) {
		printk(KERN_ERR "%s:Invalid rate %u,%u requested\n",
				__func__, sample_rate_adc,sample_rate_dac);
		return -EINVAL;
	}
	aic23->sample_rate_control = data;
	if (aic23->active_mask)
		tlv320aic23_write(codec, TLV320AIC23_SRATE, data);
#ifdef DEBUG
	{
		int adc,dac;
		get_current_sample_rates(data, mclk, &adc, &dac);
		printk(KERN_DEBUG "actual samplerate = %u,%u reg=%x\n", 
			adc, dac, data);
	}
#endif
	return 0;
}


static int tlv320aic23_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_dapm_new_controls(codec, tlv320aic23_dapm_widgets,
				  ARRAY_SIZE(tlv320aic23_dapm_widgets));

	/* set up audio path interconnects */
	snd_soc_dapm_add_routes(codec, intercon, ARRAY_SIZE(intercon));

	snd_soc_dapm_new_widgets(codec);
	return 0;
}
/* ---------------------------------------------------------------------
 * Digital Audio Interface Operations
 */
static int tlv320aic23_hw_params(struct snd_pcm_substream *substream,
		   struct snd_pcm_hw_params *params,
		   struct snd_soc_dai *dai)
{
	int ret;
	unsigned data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	struct aic23 *aic23 = container_of(codec, struct aic23, codec);
	u32 sample_rate_adc = aic23->requested_adc;
	u32 sample_rate_dac = aic23->requested_dac;
	u32 sample_rate = params_rate(params);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		aic23->requested_dac = sample_rate_dac = sample_rate; 
		if (!sample_rate_adc)
			sample_rate_adc = sample_rate;
	} else {
		aic23->requested_adc = sample_rate_adc = sample_rate;
		if (!sample_rate_dac)
			sample_rate_dac = sample_rate;
	}
	ret = set_sample_rate_control(codec, aic23->mclk, sample_rate_adc, sample_rate_dac);
	if (ret < 0)
		return ret;
	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
		data = TLV320AIC23_IWL_16;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S20_3BE:
		data = TLV320AIC23_IWL_20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
		data = TLV320AIC23_IWL_24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S32_BE:
		data = TLV320AIC23_IWL_32;
		break;
	default:
		printk(KERN_ERR "%s: bad format\n", __func__);
		return -EINVAL;
	}
	data |= DAF_MASTER(aic23->master) | aic23->datfm;
	if (1) {
		if (aic23->right_first)
			data |= TLV320AIC23_LRSWAP_ON;
	} else {
		data |= (tlv320aic23_read_cache(codec, TLV320AIC23_DIGT_FMT) & TLV320AIC23_LRSWAP_ON);
	}
	aic23->digital_audio_format = data;
	if (aic23->active_mask)
		tlv320aic23_write(codec, TLV320AIC23_DIGT_FMT, data);
	return 0;
}

/*
 * tlv320aic23_mute - Mute control to reduce noise when changing audio format
 */
static int tlv320aic23_mute_codec(struct snd_soc_codec *codec, int mute)
{
	u16 reg = tlv320aic23_read_cache(codec, TLV320AIC23_DIGT);
	if (mute)
		reg |= TLV320AIC23_DACM_MUTE;
	else
		reg &= ~TLV320AIC23_DACM_MUTE;
	tlv320aic23_write(codec, TLV320AIC23_DIGT, reg);
	return 0;
}

#define VOLUME_MUTE 0x180

/*
 * tlv320aic23_mute_volume - Mute control to reduce noise when starting/stopping streams
 */
static int tlv320aic23_mute_volume(struct snd_soc_codec *codec, int mute)
{
	struct aic23 *aic23 = container_of(codec, struct aic23, codec);
	if (0) if (aic23->volume_mute != mute) {
		aic23->volume_mute = mute;
		if (mute) {
			tlv320aic23_write1(codec, TLV320AIC23_LCHNVOL, VOLUME_MUTE);
			tlv320aic23_write1(codec, TLV320AIC23_RCHNVOL, VOLUME_MUTE);
		} else {
			tlv320aic23_write1(codec, TLV320AIC23_LCHNVOL, aic23->left_output_volume);
			tlv320aic23_write1(codec, TLV320AIC23_RCHNVOL, aic23->right_output_volume);
		}
		return 1;
	}
	return 0;
}

static int tlv320aic23_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	struct aic23 *aic23 = container_of(codec, struct aic23, codec);
	u16 daf_prev = tlv320aic23_read_cache(codec, TLV320AIC23_DIGT_FMT);
	u16 src_prev = tlv320aic23_read_cache(codec, TLV320AIC23_SRATE);
	/* set active */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		tlv320aic23_mute_volume(codec, 1);
		tlv320aic23_mute_codec(codec, 0);
	}
	if (aic23->digital_audio_format != daf_prev)
		tlv320aic23_write(codec, TLV320AIC23_DIGT_FMT, aic23->digital_audio_format);
	if (aic23->sample_rate_control != src_prev)
		tlv320aic23_write(codec, TLV320AIC23_SRATE, aic23->sample_rate_control);
	return 0;
}

static void codec_trigger_deferred(struct work_struct *work)
{
	struct aic23 *aic23 = container_of(work, struct aic23,
			deferred_trigger_work);
	struct snd_soc_codec *codec = &aic23->codec;
	int playback = aic23->active_mask & ACTIVE_PLAYBACK;
	u16 dia = (aic23->active_mask) ? 1 : 0;
	if (playback) {
		tlv320aic23_set(codec, TLV320AIC23_ACTIVE, 1);
		tlv320aic23_modify(codec, TLV320AIC23_ANLG, 0, TLV320AIC23_DAC_SELECTED);
		tlv320aic23_mute_volume(codec, 0);
	} else {
		tlv320aic23_mute_volume(codec, 1);
		if (dia)
			tlv320aic23_set(codec, TLV320AIC23_ACTIVE, dia);
		tlv320aic23_modify(codec, TLV320AIC23_ANLG, TLV320AIC23_DAC_SELECTED, 0);
		if (!dia)
			tlv320aic23_set(codec, TLV320AIC23_ACTIVE, dia);
	}
}

static int tlv320aic23_trigger(struct snd_pcm_substream *substream, int cmd,
		   struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	struct aic23 *aic23 = container_of(codec, struct aic23, codec);
	int ret = 0;
	int playback = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK);
	unsigned char mask = (playback)? ACTIVE_PLAYBACK : ACTIVE_CAPTURE;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		aic23->active_mask |= mask;
		schedule_work(&aic23->deferred_trigger_work);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		aic23->active_mask &= ~mask;
		schedule_work(&aic23->deferred_trigger_work);
		/* don't stop driving data lines
		 * until digital_mute done
		 */
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static void tlv320aic23_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	struct aic23 *aic23 = container_of(codec, struct aic23, codec);

	/* deactivate */
	if (!codec->active) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			aic23->active_mask &= ~ACTIVE_PLAYBACK;
		else
			aic23->active_mask &= ~ACTIVE_CAPTURE;
	}
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		aic23->requested_dac = 0;
	else
		aic23->requested_adc = 0;
	schedule_work(&aic23->deferred_trigger_work);
}

/*
 * tlv320aic23_mute - Mute control to reduce noise when changing audio format
 */
static int tlv320aic23_mute(struct snd_soc_dai *dai, int mute)
{
	return tlv320aic23_mute_codec(dai->codec, mute);
}
static int tlv320aic23_inform_channel_order(struct snd_soc_dai *codec_dai, int right_first)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct aic23 *aic23 = container_of(codec, struct aic23, codec);
	aic23->right_first = right_first;
	return 0;
}

static int tlv320aic23_set_sysclk(struct snd_soc_dai *codec_dai,
			    int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct aic23 *aic23 = container_of(codec, struct aic23, codec);
	aic23->mclk = freq;
	return 0;
}

static int tlv320aic23_set_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct aic23 *aic23 = container_of(codec, struct aic23, codec);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		aic23->master = 1;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		aic23->master = 0;
		break;
	default:
		printk(KERN_ERR "%s:bad master\n", __func__);
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		aic23->datfm = TLV320AIC23_FOR_I2S;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		aic23->datfm = TLV320AIC23_FOR_DSP;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		aic23->datfm = TLV320AIC23_FOR_DSP | TLV320AIC23_LRP_ON;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		aic23->datfm = TLV320AIC23_FOR_RJUST;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		aic23->datfm = TLV320AIC23_FOR_LJUST;
		break;
	default:
		printk(KERN_ERR "%s:bad format\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int tlv320aic23_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	u16 reg = tlv320aic23_read_cache(codec, TLV320AIC23_PWR) & 0x7f;

	switch (level) {
	case SND_SOC_BIAS_ON:
		/* vref/mid, osc on, dac unmute */
		tlv320aic23_write(codec, TLV320AIC23_PWR, reg);
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		/* power down clock output */
		tlv320aic23_write(codec, TLV320AIC23_PWR,
				reg | TLV320AIC23_CLK_OFF);
		break;
	case SND_SOC_BIAS_OFF:
		/* everything off, dac mute, inactive */
		tlv320aic23_mute_codec(codec, 1);
		tlv320aic23_write(codec, TLV320AIC23_ACTIVE, 0x0);
		tlv320aic23_write(codec, TLV320AIC23_PWR, 0xffff);
		break;
	}
	codec->bias_level = level;
	return 0;
}

/* ---------------------------------------------------------------------
 * Digital Audio Interface Definition
 */
#define AIC23_RATES	SNDRV_PCM_RATE_8000_96000 |\
	SNDRV_PCM_RATE_5512 | SNDRV_PCM_RATE_KNOT | SNDRV_PCM_RATE_CONTINUOUS

#define AIC23_FORMATS_LE (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S20_3LE |\
			 SNDRV_PCM_FMTBIT_S24_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

#define AIC23_FORMATS_BE (SNDRV_PCM_FMTBIT_S16_BE |\
			 SNDRV_PCM_FMTBIT_S20_3BE |\
			 SNDRV_PCM_FMTBIT_S24_BE |\
			 SNDRV_PCM_FMTBIT_S32_BE)

#define AIC23_FORMATS AIC23_FORMATS_LE | AIC23_FORMATS_BE

static struct snd_soc_dai_ops tlv320aic23_dai_ops = {
	.prepare	= tlv320aic23_prepare,
	.trigger	= tlv320aic23_trigger,
	.hw_params	= tlv320aic23_hw_params,
	.shutdown	= tlv320aic23_shutdown,
	.digital_mute	= tlv320aic23_mute,
	.set_fmt	= tlv320aic23_set_fmt,
	.set_sysclk	= tlv320aic23_set_sysclk,
	.inform_channel_order = tlv320aic23_inform_channel_order,
};

struct snd_soc_dai tlv320aic23_dai = {
	.name = "tlv320aic23",
	.playback = {
		.stream_name = "HiFi Playback",
		/* fixme, codecs shouldn't need to lie */
		.channels_min = 1, /* lie, I2S dma will convert to stereo*/
		.channels_max = 2,
		.rates = AIC23_RATES,
		.formats = AIC23_FORMATS,
	},
	.capture = {
		.stream_name = "HiFi Capture",
		/* fixme, codecs shouldn't need to lie */
		.channels_min = 1, /* lie, I2S dma will convert from stereo */
		.channels_max = 2,
		.rates = AIC23_RATES,
		.formats = AIC23_FORMATS,
	},
	.ops = &tlv320aic23_dai_ops,
};
EXPORT_SYMBOL_GPL(tlv320aic23_dai);

/* ---------------------------------------------------------------------
 */

static int tlv320aic23_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	tlv320aic23_mute_codec(codec, 1);
	tlv320aic23_write(codec, TLV320AIC23_ACTIVE, 0x0);
	tlv320aic23_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int tlv320aic23_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
	u16 reg;

	/* Sync reg_cache with the hardware */
	for (reg = 0; reg < AIC23_NUM_CACHE_REGS; reg++) {
		u16 val = tlv320aic23_read_cache(codec, reg);
		tlv320aic23_write(codec, reg, val);
	}

	tlv320aic23_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	tlv320aic23_set_bias_level(codec, codec->suspend_bias_level);

	return 0;
}
/* ************************************ */

/*
 * initialise the AIC23 driver
 * register the mixer and dsp interfaces with the kernel
 */
static int tlv320aic23_init(struct snd_soc_codec *codec, struct aic23 *aic23)
{
	tlv320aic23_write(codec, TLV320AIC23_RESET, 0);
	/* all off
	 * This addresses the problem of the digital filters
	 * not being initialized when MCLK is removed
	 * slea037.pdf section 2 
	 * "Noise Fixed by Toggling Bit D7 of Power-Down Control"
	 */
	tlv320aic23_write(codec, TLV320AIC23_PWR, 0x01ff);

	/* power on device, only oscillator is on */
	tlv320aic23_write(codec, TLV320AIC23_PWR, 0x00df);
	tlv320aic23_write(codec, TLV320AIC23_PWR, 0x005f);
	tlv320aic23_write(codec, TLV320AIC23_ACTIVE, 0x0000);

	/* set the update bits */
	tlv320aic23_write(codec, TLV320AIC23_LINVOL, TLV320AIC23_LIV_DEFAULT | TLV320AIC23_LIM_MUTED);
	tlv320aic23_write(codec, TLV320AIC23_RINVOL, TLV320AIC23_LIV_DEFAULT | TLV320AIC23_LIM_MUTED);
	tlv320aic23_write(codec, TLV320AIC23_LCHNVOL,
			TLV320AIC23_LHV_DEFAULT | TLV320AIC23_LZC_ON);
	tlv320aic23_write(codec, TLV320AIC23_RCHNVOL,
			TLV320AIC23_LHV_DEFAULT | TLV320AIC23_LZC_ON);
	tlv320aic23_write(codec, TLV320AIC23_ANLG,
			(1<<FAKE_DAC_ON_BIT) | TLV320AIC23_MICB_20DB | TLV320AIC23_INSEL_MIC);
	tlv320aic23_write(codec, TLV320AIC23_DIGT,
			TLV320AIC23_DACM_MUTE | TLV320AIC23_DEEMP_NONE);
	tlv320aic23_write(codec, TLV320AIC23_DIGT_FMT,
			TLV320AIC23_IWL_16 | TLV320AIC23_FOR_DSP | TLV320AIC23_MS_MASTER);
	set_sample_rate_control(codec, aic23->mclk, 44100, 44100);
	return 0;
}


static int tlv320aic23_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct tlv320aic23_setup_data *setup_data = socdev->codec_data;
	struct snd_soc_codec *codec;
	struct aic23 *aic23;
	int ret = 0;

	printk(KERN_INFO "AIC23 Audio Codec %s\n", AIC23_VERSION);

	aic23 = kzalloc(sizeof(struct aic23), GFP_KERNEL);
	if (!aic23) {
		ret = -ENOMEM;
		goto exit1;
	}
	codec = &aic23->codec;
	INIT_WORK(&aic23->deferred_trigger_work, codec_trigger_deferred);
	aic23->master = 1;
	aic23->datfm = TLV320AIC23_FOR_DSP;
	aic23->mclk = 12000000;
	aic23->volume_mute = 1;
	codec->name = "tlv320aic23";
	codec->owner = THIS_MODULE;
	codec->private_data = aic23;
	codec->reg_cache_size = AIC23_NUM_CACHE_REGS;
	codec->reg_cache = aic23->reg_cache;
	codec->read = tlv320aic23_read_cache;
	codec->write = tlv320aic23_write;
	codec->set_bias_level = tlv320aic23_set_bias_level;
	codec->dai = &tlv320aic23_dai;
	codec->num_dai = 1;
	codec->hw_write = setup_data->hw_write;

	socdev->card->codec = codec;
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	ret = tlv320aic23_init(codec, aic23);
	if (ret < 0)
		goto exit3;
	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		printk(KERN_ERR "tlv320aic23: failed to create pcms\n");
		goto exit3;
	}
	tlv320aic23_add_controls(codec);
	tlv320aic23_add_widgets(codec);
	ret = snd_soc_init_card(socdev);
	if (ret < 0) {
		printk(KERN_ERR "tlv320aic23: failed to init card\n");
		goto exit4;
	}
	return ret;
exit4:
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
exit3:
	socdev->card->codec = NULL;
	kfree(aic23);
exit1:
	return ret;
}

/* power down chip */
static int tlv320aic23_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
	struct aic23 *aic23 = container_of(codec, struct aic23, codec);

	tlv320aic23_set_bias_level(codec, SND_SOC_BIAS_OFF);
	cancel_work_sync(&aic23->deferred_trigger_work);
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
	kfree(aic23);
	return 0;
}

struct snd_soc_codec_device tlv320aic23_soc_codec_dev = {
	.probe = 	tlv320aic23_probe,
	.remove = 	tlv320aic23_remove,
	.suspend = 	tlv320aic23_suspend,
	.resume =	tlv320aic23_resume,
};
EXPORT_SYMBOL_GPL(tlv320aic23_soc_codec_dev);

static int __devinit tlv320aic23_modinit(void)
{
	return snd_soc_register_dai(&tlv320aic23_dai);
}
module_init(tlv320aic23_modinit);

static void __exit tlv320aic23_exit(void)
{
	snd_soc_unregister_dai(&tlv320aic23_dai);
}
module_exit(tlv320aic23_exit);
MODULE_DESCRIPTION("ASoC TLV320AIC23 codec driver");
MODULE_AUTHOR("Arun KS <arunks@mistralsolutions.com>");
MODULE_LICENSE("GPL");
