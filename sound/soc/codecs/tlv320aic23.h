/*
 * sound/soc/codecs/tlv320aic23.h
 *
 * Hardware definitions for TI TLV320AIC23 audio codec
 *
 * Author: Troy Kisky
 * Copyright (C) 2008 Boundary Devices
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __SOUND_CODECS_TLV320AIC23_H
#define __SOUND_CODECS_TLV320AIC23_H

#define AIC23_NUM_REGS 16
#define AIC23_NUM_CACHE_REGS (AIC23_DIGITAL_INTERFACE_ACT+1)

#define AIC23_LEFT_INPUT_VOLUME		0x00
#define AIC23_RIGHT_INPUT_VOLUME	0x01
#define AIC23_LEFT_OUTPUT_VOLUME	0x02
#define AIC23_RIGHT_OUTPUT_VOLUME	0x03
#define AIC23_ANALOG_AUDIO_CONTROL	0x04
#define AIC23_DIGITAL_AUDIO_CONTROL	0x05
#define AIC23_POWER_DOWN_CONTROL	0x06
#define AIC23_DIGITAL_AUDIO_FORMAT	0x07
#define AIC23_SAMPLE_RATE_CONTROL	0x08
#define AIC23_DIGITAL_INTERFACE_ACT	0x09
#define AIC23_RESET			0x0F

/* Left (right) line input volume control register */
#define LRIV_MIN			0x000
#define LRIV_DEFAULT			0x017
#define LRIV_MAX			0x01f
#define LRIV_MUTE_BIT			7
#define LRIV_MUTE			0x080
#define LRIV_SIMULT_UPDATE		0x100

/* Left (right) channel headphone volume control register */
#define LROV_MIN			0x000
#define LROV_DEFAULT			0x079
#define LROV_MAX			0x07f
#define LROV_ZERO_CROSS_BIT		7
#define LROV_ZERO_CROSS			0x080
#define LROV_SIMULT_UPDATE		0x100

/* Analog audio path control register */
#define AAC_MIC_BOOST_BIT		0
#define AAC_MIC_BOOST_20DB		0x001
#define AAC_MIC_MUTE_BIT		1
#define AAC_MIC_MUTE			0x002
#define AAC_INSEL_BIT			2
#define AAC_INSEL_MIC			(1<<AAC_INSEL_BIT)
#define AAC_BYPASS_BIT			3
#define AAC_BYPASS_ON			0x008
#define AAC_DAC_BIT			4
#define AAC_DAC_ON			0x010
#define AAC_SIDE_TONE_ENABLE_BIT	5
#define AAC_SIDE_TONE_ENABLE		0x020
#define AAC_SIDE_TONE_VOL_BIT		6
#define AAC_SIDE_TONE(x)		((x)<<6)
#define AAC_SIDE_TONE_MASK		0x7

/* Digital audio path control register */
#define DAC_HIGH_PASS_FILTER_BIT	0
#define DAC_HIGH_PASS_FILTER		0x0001
#define DAC_DEEMP_BIT 			1
#define DAC_DEEMP_NONE			0x0000
#define DAC_DEEMP_32K			0x0002
#define DAC_DEEMP_44K			0x0004
#define DAC_DEEMP_48K			0x0006
#define DAC_SOFT_MUTE_BIT		3
#define DAC_SOFT_MUTE			0x0008

/* Power control down register */
#define PDC_LINE_BIT			0
#define PDC_MIC_BIT			1
#define PDC_ADC_BIT			2
#define PDC_DAC_BIT			3
#define PDC_OUT_BIT			4
#define PDC_OSC_BIT			5
#define PDC_CLK_BIT			6
#define PDC_DEVICE_POWER_BIT	  	7

/* Digital audio interface register */
#define DAF_FOR_RIGHT_ALIGN		0x0000	/* bits 1-0 */
#define DAF_FOR_LEFT_ALIGN		0x0001
#define DAF_FOR_I2S			0x0002
#define DAF_FOR_DSP			0x0003

#define DAF_IWL_16			0x0000	/* bits 3-2 */
#define DAF_IWL_20			0x0004
#define DAF_IWL_24			0x0008
#define DAF_IWL_32			0x000C

#define DAF_MSB_ON_2ND_BCLK		0x0010
#define DAF_LRSWAP_ON			0x0020
#define DAF_MASTER(x)			((x)<<6)
#define DAF_MASTER_MODE			DAF_MASTER(1)

/* Sample rate control register */
#define SRC_USB_MODE			0x0001
#define SRC_BOSR_USB272			0x0002
#define SRC_BOSR_NORMAL384		0x0002
#define SRC_SR(x)			((x)<<2)
#define SRC_SR_MASK                     0xf
#define SRC_CLKIN(x)			((x)<<6)
#define SRC_CLKIN_HALF			SRC_CLKIN(1)
#define SRC_CLKOUT(x)			((x)<<7)
#define SRC_CLKOUT_HALF			SRC_CLKOUT(1)

/* Digital interface register */
#define DIA_ACTIVATE			0x0001

struct tlv320aic23_setup_data {
	hw_write_t hw_write;
};
extern struct snd_soc_dai tlv320aic23_dai;
extern struct snd_soc_codec_device tlv320aic23_soc_codec_dev;

#endif /* __SOUND_CODECS_TLV320AIC23_H */
