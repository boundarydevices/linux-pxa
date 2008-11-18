/*
 * ALSA SoC TLV320AIC23 codec driver
 *
 * Author:      Arun KS, <arunks@mistralsolutions.com>
 * Copyright:   (C) 2008 Mistral Solutions Pvt Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _TLV320AIC23_H
#define _TLV320AIC23_H

#define AIC23_NUM_REGS 16
#define AIC23_NUM_CACHE_REGS (TLV320AIC23_ACTIVE+1)

/* Codec TLV320AIC23 */
#define TLV320AIC23_LINVOL		0x00
#define TLV320AIC23_RINVOL		0x01
#define TLV320AIC23_LCHNVOL		0x02
#define TLV320AIC23_RCHNVOL		0x03
#define TLV320AIC23_ANLG		0x04
#define TLV320AIC23_DIGT		0x05
#define TLV320AIC23_PWR			0x06
#define TLV320AIC23_DIGT_FMT		0x07
#define TLV320AIC23_SRATE		0x08
#define TLV320AIC23_ACTIVE		0x09
#define TLV320AIC23_RESET		0x0F

/* Left (right) line input volume control register */
#define LRIV_MUTE_BIT			7

#define TLV320AIC23_LRS_ENABLED		0x0100
#define TLV320AIC23_LIM_MUTED		0x0080
#define TLV320AIC23_LIV_DEFAULT		0x0017
#define TLV320AIC23_LIV_MAX		0x001f
#define TLV320AIC23_LIV_MIN		0x0000

/* Left (right) channel headphone volume control register */
#define LROV_ZERO_CROSS_BIT		7

#define TLV320AIC23_LZC_ON		0x0080
#define TLV320AIC23_LHV_DEFAULT		0x0079
#define TLV320AIC23_LHV_MAX		0x007f
#define TLV320AIC23_LHV_MIN		0x0000

/* Analog audio path control register */
#define AAC_SIDE_TONE_VOL_BIT		6
#define AAC_SIDE_TONE_ENABLE_BIT	5
#define AAC_DAC_BIT			4
#define AAC_BYPASS_BIT			3
#define AAC_INSEL_BIT			2
#define AAC_MIC_MUTE_BIT		1
#define AAC_MIC_BOOST_BIT		0

#define TLV320AIC23_STA_REG(x)		((x)<<6)
#define TLV320AIC23_STE_ENABLED		0x0020
#define TLV320AIC23_DAC_SELECTED	0x0010
#define TLV320AIC23_BYPASS_ON		0x0008
#define TLV320AIC23_INSEL_MIC		0x0004
#define TLV320AIC23_MICM_MUTED		0x0002
#define TLV320AIC23_MICB_20DB		0x0001

/* Digital audio path control register */
#define DAC_SOFT_MUTE_BIT		3
#define DAC_DEEMP_BIT 			1
#define DAC_HIGH_PASS_FILTER_BIT	0

#define TLV320AIC23_DACM_MUTE		0x0008
#define TLV320AIC23_DEEMP_NONE		0x0000
#define TLV320AIC23_DEEMP_32K		0x0002
#define TLV320AIC23_DEEMP_44K		0x0004
#define TLV320AIC23_DEEMP_48K		0x0006
#define TLV320AIC23_ADCHP_ON		0x0001

/* Power control down register */
#define PDC_DEVICE_POWER_BIT	  	7
#define PDC_CLK_BIT			6
#define PDC_OSC_BIT			5
#define PDC_OUT_BIT			4
#define PDC_DAC_BIT			3
#define PDC_ADC_BIT			2
#define PDC_MIC_BIT			1
#define PDC_LINE_BIT			0

#define TLV320AIC23_DEVICE_PWR_OFF 	0x0080
#define TLV320AIC23_CLK_OFF		0x0040
#define TLV320AIC23_OSC_OFF		0x0020
#define TLV320AIC23_OUT_OFF		0x0010
#define TLV320AIC23_DAC_OFF		0x0008
#define TLV320AIC23_ADC_OFF		0x0004
#define TLV320AIC23_MIC_OFF		0x0002
#define TLV320AIC23_LINE_OFF		0x0001

/* Digital audio interface register */
#define DAF_MASTER(x)			((x)<<6)
#define TLV320AIC23_MS_MASTER		0x0040
#define TLV320AIC23_LRSWAP_ON		0x0020
#define TLV320AIC23_LRP_ON		0x0010
#define TLV320AIC23_IWL_16		0x0000
#define TLV320AIC23_IWL_20		0x0004
#define TLV320AIC23_IWL_24		0x0008
#define TLV320AIC23_IWL_32		0x000C
#define TLV320AIC23_FOR_I2S		0x0002
#define TLV320AIC23_FOR_DSP		0x0003
#define TLV320AIC23_FOR_LJUST		0x0001
#define TLV320AIC23_FOR_RJUST		0x0000


/* Sample rate control register */
#define TLV320AIC23_CLKOUT_HALF		0x0080
#define TLV320AIC23_CLKIN_HALF		0x0040
#define TLV320AIC23_BOSR_384fs		0x0002	/* BOSR_272fs in USB mode */
#define TLV320AIC23_USB_CLK_ON		0x0001
#define TLV320AIC23_SR_MASK             0xf
#define TLV320AIC23_CLKOUT_SHIFT        7
#define TLV320AIC23_CLKIN_SHIFT         6
#define TLV320AIC23_SR_SHIFT            2
#define TLV320AIC23_BOSR_SHIFT          1

/* Digital interface register */
#define TLV320AIC23_ACT_ON		0x0001

/*
 * AUDIO related MACROS
 */

#define TLV320AIC23_DEFAULT_OUT_VOL	0x70
#define TLV320AIC23_DEFAULT_IN_VOLUME	0x10

#define TLV320AIC23_OUT_VOL_MIN		TLV320AIC23_LHV_MIN
#define TLV320AIC23_OUT_VOL_MAX		TLV320AIC23_LHV_MAX
#define TLV320AIC23_OUT_VO_RANGE	(TLV320AIC23_OUT_VOL_MAX - \
					TLV320AIC23_OUT_VOL_MIN)
#define TLV320AIC23_OUT_VOL_MASK	TLV320AIC23_OUT_VOL_MAX

#define TLV320AIC23_IN_VOL_MIN		TLV320AIC23_LIV_MIN
#define TLV320AIC23_IN_VOL_MAX		TLV320AIC23_LIV_MAX
#define TLV320AIC23_IN_VOL_RANGE	(TLV320AIC23_IN_VOL_MAX - \
					TLV320AIC23_IN_VOL_MIN)
#define TLV320AIC23_IN_VOL_MASK		TLV320AIC23_IN_VOL_MAX

#define TLV320AIC23_SIDETONE_MASK	0x1c0
#define TLV320AIC23_SIDETONE_0		0x100
#define TLV320AIC23_SIDETONE_6		0x000
#define TLV320AIC23_SIDETONE_9		0x040
#define TLV320AIC23_SIDETONE_12		0x080
#define TLV320AIC23_SIDETONE_18		0x0c0

struct tlv320aic23_setup_data {
	hw_write_t hw_write;
};
extern struct snd_soc_dai tlv320aic23_dai;
extern struct snd_soc_codec_device tlv320aic23_soc_codec_dev;

#endif /* _TLV320AIC23_H */
