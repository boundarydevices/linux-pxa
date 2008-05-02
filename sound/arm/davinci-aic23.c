/*
 * linux/sound/davinci-aic23.c -- AIC23 support for Davinci chips.
 *
 * Author:	Eric Nelson
 * Created:	Feb 19, 2008
 * Copyright:	Boundary Devices
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/asound.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <linux/soundcard.h>
#include <linux/clk.h>
#include <sound/control.h>

#include <asm/irq.h>
#include <linux/mutex.h>
#include <mach/hardware.h>
#include <mach/mcbsp.h>
#include <mach/edma.h>
#include <mach/gpio.h>

#define MCBSP_DXR   0x01E02004
#define MCBSP_DRR   0x01E02000

#define MUTE_GPIO 7
#define MUTED 0
#define NOTMUTED 1

#define NUM_DMA_CHANNELS 4

#define IRAM_RESERVED 32
#define IRAM_SIZE 0x4000	//16k in ram0/1 combination
#define IRAM_LEFT (IRAM_SIZE-IRAM_RESERVED)
#define IRAM_MULTIPLE 8
#define BUFFER_BYTES (IRAM_MULTIPLE*IRAM_LEFT)
#define PERIOD_BYTES (IRAM_LEFT/NUM_DMA_CHANNELS)

struct dma_playback_info {
   int master_chan ;
   int channels[NUM_DMA_CHANNELS];
   int ram_to_iram_channel ;
   int ram_to_iram_tcc ;
   int cur_dma ;
};

static const struct snd_pcm_hardware davinci_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.period_bytes_min	= 32,
	.period_bytes_max	= PERIOD_BYTES,
	.periods_min		= NUM_DMA_CHANNELS,
	.periods_max		= NUM_DMA_CHANNELS*IRAM_MULTIPLE,
	.buffer_bytes_max	= BUFFER_BYTES,
	.fifo_size		= 32,
	.rates			= (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
				   SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_32000 |\
				   SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 ),
	.rate_min =         8000,
	.rate_max =         48000,
        .channels_min = 2,
        .channels_max = 2,
};

// Codec TLV320AIC23
#define LEFT_LINE_VOLUME_ADDR		0x00
#define RIGHT_LINE_VOLUME_ADDR		0x01
#define LEFT_CHANNEL_VOLUME_ADDR	0x02
#define RIGHT_CHANNEL_VOLUME_ADDR	0x03
#define ANALOG_AUDIO_CONTROL_ADDR	0x04
#define DIGITAL_AUDIO_CONTROL_ADDR	0x05
#define POWER_DOWN_CONTROL_ADDR		0x06
#define DIGITAL_AUDIO_FORMAT_ADDR	0x07
#define SAMPLE_RATE_CONTROL_ADDR	0x08
#define DIGITAL_INTERFACE_ACT_ADDR	0x09
#define RESET_CONTROL_ADDR		0x0F
#define NUM_AIC23_REGS			(RESET_CONTROL_ADDR+1)

// Left (right) line input volume control register
#define LRS_ENABLED			0x0100
#define LIM_MUTED			0x0080
#define LIV_DEFAULT			0x0017
#define LIV_MAX				0x001f
#define LIV_MIN				0x0000

// Left (right) channel headphone volume control register
#define LZC_ON				0x0080
#define LHV_DEFAULT			0x0079
#define LHV_MAX				0x007f
#define LHV_MIN				0x0000

// Analog audio path control register
#define STE_ENABLED			0x0020
#define DAC_SELECTED			0x0010
#define BYPASS_ON			0x0008
#define INSEL_MIC			0x0004
#define MICM_MUTED			0x0002
#define MICB_20DB			0x0001

// Digital audio path control register
#define DACM_MUTE			0x0008
#define DEEMP_32K			0x0002
#define DEEMP_44K			0x0004
#define DEEMP_48K			0x0006
#define ADCHP_ON			0x0001

// Power control down register
#define DEVICE_POWER_OFF	  	0x0080
#define CLK_OFF				0x0040
#define OSC_OFF				0x0020
#define OUT_OFF				0x0010
#define DAC_OFF				0x0008
#define ADC_OFF				0x0004
#define MIC_OFF				0x0002
#define LINE_OFF			0x0001
#define NOTHING_POWERED_ON		0x004F

// Digital audio interface register
#define MS_MASTER			0x0040
#define LRSWAP_ON			0x0020
#define LRP_ON				0x0010
#define IWL_16				0x0000
#define IWL_20				0x0004
#define IWL_24				0x0008
#define IWL_32				0x000C
#define FOR_I2S				0x0002
#define FOR_DSP				0x0003

// Sample rate control register
#define CLKOUT_HALF			0x0080
#define CLKIN_HALF			0x0040
#define BOSR_384fs			0x0002 // BOSR_272fs when in USB mode
#define USB_CLK_ON			0x0001
#define SR_MASK                         0xf
#define CLKOUT_SHIFT                    7
#define CLKIN_SHIFT                     6
#define SR_SHIFT                        2
#define BOSR_SHIFT                      1

// Digital interface register
#define ACT_ON				0x0001

#define TLV320AIC23ID1                  (0x1a)	// cs low
#define TLV320AIC23ID2                  (0x1b)	// cs high

#define DEFAULT_BITPERSAMPLE		16
#define AUDIO_RATE_DEFAULT		44100
#define REC_MASK 			(SOUND_MASK_LINE | SOUND_MASK_MIC)
#define DEV_MASK 			(REC_MASK | SOUND_MASK_VOLUME)

#define MONO				1
#define STEREO				2

#define SET_VOLUME 			1
#define SET_LINE			2
#define SET_MIC				3
#define SET_RECSRC			4
#define SET_IGAIN			5
#define SET_OGAIN			6
#define SET_BASS			7
#define SET_TREBLE			8
#define SET_MICBIAS			9

#define DEFAULT_OUTPUT_VOLUME	80
#define DEFAULT_INPUT_VOLUME	20	/* 0 ==> mute line in */
#define DEFAULT_INPUT_IGAIN		20
#define DEFAULT_INPUT_OGAIN		100

#define OUTPUT_VOLUME_MIN		LHV_MIN
#define OUTPUT_VOLUME_MAX		LHV_MAX
#define OUTPUT_VOLUME_RANGE		(OUTPUT_VOLUME_MAX - OUTPUT_VOLUME_MIN+1)
#define OUTPUT_VOLUME_MASK		OUTPUT_VOLUME_MAX

#define INPUT_VOLUME_MIN 		LIV_MIN
#define INPUT_VOLUME_MAX 		LIV_MAX
#define INPUT_VOLUME_RANGE 		(INPUT_VOLUME_MAX - INPUT_VOLUME_MIN)
#define INPUT_VOLUME_MASK 		INPUT_VOLUME_MAX

#define INPUT_GAIN_MIN			LIG_MIN
#define INPUT_GAIN_MAX			LIG_MAX
#define INPUT_GAIN_RANGE		(INPUT_GAIN_MAX - INPUT_GAIN_MIN)

#define OUTPUT_GAIN_MIN			LOG_MIN
#define OUTPUT_GAIN_MAX			LOG_MAX
#define OUTPUT_GAIN_RANGE		(INPUT_GAIN_MAX - INPUT_GAIN_MIN)

#define NUMBER_SAMPLE_RATES_SUPPORTED 10

static struct aic23_local_info {
	u8	volume;
	u16 volume_reg;
	u8	line;
	u8	mic;
	int recsrc;
	u8 nochan;
	u16 igain;
	u16 ogain;
	u8 micbias;
	u8 bass;
	u8 treble;
	u16 input_volume_reg;
	int mod_cnt;
} aic23_local = { 0 };

static struct proc_dir_entry *procentry = 0 ;
static char const procentryname[] = {
   "aic23"
};

extern struct clk *davinci_mcbsp_get_clock(void);

static long audio_samplerate = AUDIO_RATE_DEFAULT;

static struct davinci_mcbsp_reg_cfg initial_config = {
	.spcr2 = FREE | XINTM(3),
	.spcr1 = RINTM(3),
	.rcr2  = RWDLEN2(DAVINCI_MCBSP_WORD_16) | RFIG | RDATDLY(0) | RFRLEN2(0) | RPHASE,
	.rcr1  = RWDLEN1(DAVINCI_MCBSP_WORD_16) | RFRLEN1(0),
	.xcr2  = XWDLEN2(DAVINCI_MCBSP_WORD_16) | XFIG | XDATDLY(0) | XFRLEN2(0) | XPHASE,
	.xcr1  = XWDLEN1(DAVINCI_MCBSP_WORD_16) | XFRLEN1(0),
	.srgr1 = FWID(DEFAULT_BITPERSAMPLE - 1),
	.srgr2 = FSGM | FPER(DEFAULT_BITPERSAMPLE * 2 - 1),
	/* configure McBSP to be the I2S slave */
	.pcr0 = CLKXP | CLKRP,
};

static void dma_irq_handler(int sound_curr_lch, u16 ch_status, void *data)
{
static int first[NUM_DMA_CHANNELS] = {1,1,1,1};
	struct snd_pcm_substream *substream = data ;
        struct snd_pcm_runtime *runtime = substream->runtime ;
        struct dma_playback_info *rtdata = runtime->private_data ;

	if (ch_status == DMA_COMPLETE) {
                int err ;
		edmacc_paramentry_regs regs ;
                unsigned channel = rtdata->ram_to_iram_channel ;
                unsigned cur_dma = rtdata->cur_dma++ ;
                unsigned dest_dma = cur_dma % NUM_DMA_CHANNELS ;
                unsigned src_dma = (cur_dma+1) % runtime->periods  ;
                unsigned period_bytes = frames_to_bytes(runtime,runtime->period_size);
//		printk( KERN_ERR "%s: %d %x\n", __FUNCTION__, sound_curr_lch, ch_status );
                snd_pcm_period_elapsed(substream);

                regs.opt = 0x00100008 | rtdata->ram_to_iram_tcc ;    // OPT:  transfer complete int enable, non-static, A-synchronized
                regs.src = substream->dma_buffer.addr + src_dma*period_bytes ;
                regs.a_b_cnt = (1 << 16) | period_bytes ;
                regs.dst = DAVINCI_IRAM_BASE+IRAM_RESERVED+dest_dma*period_bytes ;
                regs.src_dst_bidx = 0 ;
                regs.link_bcntrld = 0x0000ffff ; // BCNT Reload 0, LINK invalid
                regs.src_dst_cidx = 0 ;
                regs.ccnt = 1 ;
		davinci_set_dma_params(channel, &regs);
                if( first[dest_dma] ){
printk( KERN_DEBUG "ram-to-iram dma %p->%p (%u bytes) on channel %u\n", (void *)regs.src, (void *)regs.dst, period_bytes, channel );
printk( KERN_DEBUG "%u periods\n", runtime->periods );
printk( KERN_DEBUG "%02x %02x %02x %02x\n", substream->dma_buffer.area[0],substream->dma_buffer.area[1],substream->dma_buffer.area[2],substream->dma_buffer.area[3] );
printk( KERN_DEBUG "%s: DODMA\n", __FUNCTION__ );
printk( KERN_DEBUG "opt\t\t%8x\n", regs.opt );
printk( KERN_DEBUG "src\t\t%8x\n", regs.src );
printk( KERN_DEBUG "a_b_cnt\t\t%8x\n", regs.a_b_cnt );
printk( KERN_DEBUG "dst\t\t%8x\n", regs.dst );
printk( KERN_DEBUG "src_dst_bidx\t\t%8x\n", regs.src_dst_bidx );
printk( KERN_DEBUG "link_bcntrld\t\t%8x\n", regs.link_bcntrld );
printk( KERN_DEBUG "src_dst_cidx\t\t%8x\n", regs.src_dst_cidx );
printk( KERN_DEBUG "ccnt\t\t%8x\n", regs.ccnt );
                   first[dest_dma] = 0 ;
                }
                err = davinci_start_dma(channel);
                if( err ){
printk( KERN_ERR "%s: error %d issuing DMA\n", __FUNCTION__, err );
                }
	} else {
		printk( KERN_ERR "%s: Error %x in DMA\n", __FUNCTION__, ch_status );
	}

	return;
}

static void ram_to_iram_irq_handler(int sound_curr_lch, u16 ch_status, void *data)
{
//   printk( KERN_ERR "rir\n" );
}

inline unsigned outvolume_to_regval( unsigned vol ){
    return ((vol * OUTPUT_VOLUME_RANGE) / 100) + OUTPUT_VOLUME_MIN;
}

inline unsigned regval_to_outvolume( unsigned regval ){
    return ((regval * 100)/OUTPUT_VOLUME_RANGE);
}

extern int spi_tlv320aic23_write_value(u8 reg, u16 value);
extern u16 spi_tlv320aic23_read_value(u8 reg);
extern int spi_tlv320aic23_init(void);
extern void spi_tlv320aic23_shutdown(void);

/* TLV320AIC23 is a write only device */
static __inline__ void audio_aic23_write(u8 address, u16 data)
{
	if (spi_tlv320aic23_write_value(address, data) < 0)
		printk(KERN_INFO "aic23 write failed for reg = %d\n", address);
}

static void audio_aic23_clearbits(u8 address, u16 data)
{
	int oldval = spi_tlv320aic23_read_value(address);
	audio_aic23_write(address, oldval & ~(data) );
}

static void audio_aic23_setbits(u8 address, u16 data)
{
	int oldval = spi_tlv320aic23_read_value(address);
	audio_aic23_write(address, oldval | data );
}

static int aic23_update(int flag, int val)
{
	u16 volume;

	switch (flag) {
	case SET_VOLUME:
		/* Ignore separate left/right channel for now,
			even the codec does support it. */
		val &= 0xff;

		printk( KERN_DEBUG "Set Volume %d\n",val );
		if (val < 0 || val > 100) {
			printk(KERN_ERR "Trying a bad volume value(%d)!\n",val);
                        val = 100 ;
//			return -EPERM;
		}
		// Convert 0 -> 100 volume to 0x00 (LHV_MIN) -> 0x7f (LHV_MAX) 
		// volume range
		volume = outvolume_to_regval(val);

		// R/LHV[6:0] 1111111 (+6dB) to 0000000 (-73dB) in 1db steps,
		// default 1111001 (0dB)
		aic23_local.volume_reg &= ~OUTPUT_VOLUME_MASK;
		aic23_local.volume_reg |= volume;
                aic23_local.volume = val ;
		audio_aic23_write(LEFT_CHANNEL_VOLUME_ADDR, aic23_local.volume_reg);
		audio_aic23_write(RIGHT_CHANNEL_VOLUME_ADDR, aic23_local.volume_reg);
//		audio_aic23_write(LEFT_CHANNEL_VOLUME_ADDR, 0);
//		audio_aic23_write(RIGHT_CHANNEL_VOLUME_ADDR, 0);
		printk( KERN_DEBUG "Set Volume %d/%d, reg=%x\n",val,volume,aic23_local.volume_reg&OUTPUT_VOLUME_MASK);
		break;

	case SET_LINE:
	case SET_MIC:
		/* Ignore separate left/right channel for now,
			even the codec does support it. */
		val &= 0xff;

		if (val < 0 || val > 100) {
			printk(KERN_ERR "Trying a bad volume value(%d)!\n",val);
			return -EPERM;
		}
		// Convert 0 -> 100 volume to 0x0 (LIV_MIN) -> 0x1f (LIV_MAX) 
		// volume range
		volume = ((val * INPUT_VOLUME_RANGE) / 100) + INPUT_VOLUME_MIN;

		// R/LIV[4:0] 11111 (+12dB) to 00000 (-34.5dB) in 1.5dB steps,
		// default 10111 (0dB)
		aic23_local.input_volume_reg &= ~INPUT_VOLUME_MASK;
		aic23_local.input_volume_reg |= volume;
		audio_aic23_write(LEFT_LINE_VOLUME_ADDR, aic23_local.input_volume_reg);
		audio_aic23_write(RIGHT_LINE_VOLUME_ADDR, aic23_local.input_volume_reg);
		printk( KERN_DEBUG "Set input Volume %d, reg=%x\n",val,aic23_local.input_volume_reg);
		break;
	case SET_RECSRC:
		/* Ignore separate left/right channel for now,
	   	   even the codec does support it. */
		val &= 0xff;

		if (hweight32(val) > 1)
			val &= ~aic23_local.recsrc;

		if (val == SOUND_MASK_MIC) {
			/* enable the mic input*/
			printk( KERN_DEBUG "Enabling mic\n");
			audio_aic23_write(ANALOG_AUDIO_CONTROL_ADDR, DAC_SELECTED | INSEL_MIC);
		}
		else if (val == SOUND_MASK_LINE) {
			/* enable ADC's, enable line iput */
			printk( KERN_DEBUG " Enabling line in\n");
			audio_aic23_write(ANALOG_AUDIO_CONTROL_ADDR, DAC_SELECTED);
		}
		else {
			/* do nothing */
		}
		aic23_local.recsrc = val;
		break;


	case SET_MICBIAS:
		break;

	case SET_BASS:
		break;

	case SET_TREBLE:
		break;
	}
	return 0;
}

struct sample_rate_reg_info {
	u32 sample_rate;
	u8	control;		/* SR3, SR2, SR1, SR0 and BOSR */
	u8	divider;		/* if 0 CLKIN = MCLK, if 1 CLKIN = MCLK/2 */
};

static const struct sample_rate_reg_info
reg_info[NUMBER_SAMPLE_RATES_SUPPORTED] = {
	{96000, 0x0E, 0},
	{88200, 0x1F, 0},
	{48000, 0x00, 0},
	{44100, 0x11, 0},
	{32000, 0x0C, 0},
	{24000, 0x00, 1},
	{22050, 0x11, 1},
	{16000, 0x0C, 1},
	{ 8000, 0x06, 0},
	{ 4000, 0x06, 1},
};

static int davinci_set_samplerate(long sample_rate)
{
	u8 count = 0;
	u16 data = 0;
	/* wait for any frame to complete */
	udelay(125);

	/* Search for the right sample rate */
	while ((reg_info[count].sample_rate != sample_rate) &&
		(count < NUMBER_SAMPLE_RATES_SUPPORTED)) {
		count++;
	}
	if (count == NUMBER_SAMPLE_RATES_SUPPORTED) {
		printk(KERN_ERR "Invalid Sample Rate %d requested\n",
			(int)sample_rate);
		return -EPERM;
	}

	data = (reg_info[count].divider << CLKIN_SHIFT) | 
		(reg_info[count].control << BOSR_SHIFT) | USB_CLK_ON;

	audio_aic23_write(SAMPLE_RATE_CONTROL_ADDR, data);
	printk( KERN_DEBUG "samplerate = %ld reg=%x\n", sample_rate,data);

	audio_samplerate = sample_rate;

	return 0;
}

static int volume_info
   ( struct snd_kcontrol *kcontrol,
     struct snd_ctl_elem_info *uinfo)
{
      uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
      uinfo->count = 1;
      uinfo->value.integer.min = 0 ;
      uinfo->value.integer.max = 100 ;
      printk( KERN_DEBUG "%s: get volume\n", __FUNCTION__ );
      return 0;
}

static int volume_get
   ( struct snd_kcontrol *kcontrol,
     struct snd_ctl_elem_value *ucontrol)
{
	printk( KERN_DEBUG "%s: get volume\n", __FUNCTION__ );
	ucontrol->value.integer.value[0] = regval_to_outvolume( aic23_local.volume_reg & OUTPUT_VOLUME_MASK );
	return 0;
}

static int volume_put(struct snd_kcontrol *kcontrol,
                        struct snd_ctl_elem_value *ucontrol)
{
   unsigned value = ucontrol->value.integer.value[0];
   printk( KERN_DEBUG "%s: set volume to %u\n", __FUNCTION__, value );
   aic23_update(SET_VOLUME, value);
   return 1 ;
}

static struct snd_kcontrol_new volume_control __devinitdata = {
       .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
       .name = "PCM Playback Volume",
       .index = 0,
       .access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
       .private_value = 0xffff,
       .info = volume_info,
       .get = volume_get,
       .put = volume_put
};

/*******************************************************************************
 *
 * DMA channel requests
 *
 ******************************************************************************/
static int
davinci_request_sound_dma
   ( int device_id, 
     const char *device_name, 
     void *data,
     int *master_ch, 
     int *channels,
     int *ram_to_iram_channel,
     int *ram_to_iram_tcc )
{
	int i, err = 0;
	int tcc = TCC_ANY ;
	edmacc_paramentry_regs regs ;

	/* request for the Master channel and setup the params */
	err = davinci_request_dma( device_id, device_name, dma_irq_handler, data, master_ch, &tcc, EVENTQ_0);
	/* Handle Failure condition here */
	if (err < 0) {
		printk( KERN_ERR "Error in requesting Master channel %d = 0x%x\n", device_id, err );
		return err;
	}
	printk( KERN_DEBUG "%s: master channel %d, tcc %d\n", __FUNCTION__, *master_ch, tcc );
        memset(&regs,0,sizeof(regs) );
	davinci_set_dma_params(*master_ch, &regs);

	for (i = 0; i < NUM_DMA_CHANNELS; i++) {
//                tcc = TCC_ANY ;
		err = davinci_request_dma( DAVINCI_EDMA_PARAM_ANY, device_name, dma_irq_handler, data, channels+i, &tcc, EVENTQ_0);
		/* Handle Failure condition here */
		if (err < 0) {
			int j;
			for (j = 0; j < i; j++)
				davinci_free_dma(channels[j]);
			printk( KERN_ERR "Error in requesting channel %d=0x%x\n", i, err);
			return err;
		}
		printk( KERN_DEBUG "%s: channel[%d] == %d, tcc %d\n", __FUNCTION__, i, channels[i], tcc );
	}

        *ram_to_iram_tcc = TCC_ANY ;
        err = davinci_request_dma( DAVINCI_DMA_CHANNEL_ANY, device_name, ram_to_iram_irq_handler, data, ram_to_iram_channel, ram_to_iram_tcc, EVENTQ_1);
        if( err ){
           int j;
           for (j = 0; j < i; j++)
                 davinci_free_dma(channels[j]);
           davinci_free_dma(*master_ch);
           printk( KERN_ERR "%s: error allocating ram-to-iram dma channel\n", __FUNCTION__ );
           return err ;
        }
        printk( KERN_DEBUG "%s: ram-to-iram channel %u, tcc %d\n", __FUNCTION__, *ram_to_iram_channel, *ram_to_iram_tcc );

	/* Chain the channels together */
	for( i = 0; i < NUM_DMA_CHANNELS; i++ ){
		int cur_chan = channels[i];
		int nex_chan = ((NUM_DMA_CHANNELS - 1 == i) ? channels[0] : channels[i + 1]);
		davinci_dma_link_lch(cur_chan, nex_chan);
	}

	return 0;
}

/******************************************************************************
 *
 * DMA channel requests Freeing
 *
 ******************************************************************************/
static int davinci_free_sound_dma(int master_ch, int *channels, int iram_channel)
{
	int i;

        printk( KERN_DEBUG "%s\n", __FUNCTION__ );
	/* release the Master channel */
	davinci_free_dma(master_ch);

	for (i = 0; i < NUM_DMA_CHANNELS; i++) {
		int cur_chan = channels[i];
		int nex_chan = ((NUM_DMA_CHANNELS - 1 == i) ? channels[0] : channels[i + 1]);

		davinci_dma_unlink_lch(cur_chan, nex_chan);
		davinci_free_dma(cur_chan);
	}
        davinci_free_dma(iram_channel);
	return 0;
}

static void mute(void)
{
	if( 0 == (spi_tlv320aic23_read_value(POWER_DOWN_CONTROL_ADDR)&DAC_OFF) ){
//		audio_aic23_setbits(DIGITAL_AUDIO_CONTROL_ADDR, DACM_MUTE);
		audio_aic23_setbits(POWER_DOWN_CONTROL_ADDR, DAC_OFF);
		gpio_direction_output(MUTE_GPIO,MUTED);
	}
}

static void unmute(void)
{
	gpio_direction_output(MUTE_GPIO,NOTMUTED);
	audio_aic23_clearbits(POWER_DOWN_CONTROL_ADDR, DAC_OFF);
	audio_aic23_clearbits(DIGITAL_AUDIO_CONTROL_ADDR, DACM_MUTE);
}

inline void aic23_configure(void)
{
	/* Reset codec */
	audio_aic23_write(RESET_CONTROL_ADDR, 0);

	/* Initialize the AIC23 internal state */

	/* Left/Right line input volume control */
	aic23_local.line = DEFAULT_INPUT_VOLUME;
	aic23_local.mic = DEFAULT_INPUT_VOLUME;
	aic23_update(SET_LINE, DEFAULT_INPUT_VOLUME);

	/* Left/Right headphone channel volume control */
	/* Zero-cross detect on */
	aic23_local.volume_reg = LZC_ON;
	aic23_update(SET_VOLUME, aic23_local.volume);
        printk( KERN_DEBUG "%s: volume %d\n", __FUNCTION__, aic23_local.volume );

	/* Analog audio path control, DAC selected, delete INSEL_MIC for line in */
	audio_aic23_write(ANALOG_AUDIO_CONTROL_ADDR, DAC_SELECTED | INSEL_MIC);	// | MICB_20DB | BYPASS_ON

	/* Digital audio path control, de-emphasis control 44.1kHz */
	audio_aic23_write(DIGITAL_AUDIO_CONTROL_ADDR, DEEMP_44K|DACM_MUTE);

	/* Power control, nothing is on */
	audio_aic23_write(POWER_DOWN_CONTROL_ADDR, NOTHING_POWERED_ON);

	/* Digital audio interface, master/slave mode, I2S, 16 bit */
	audio_aic23_write(DIGITAL_AUDIO_FORMAT_ADDR, MS_MASTER | IWL_16 | FOR_DSP);

	/* Enable digital interface */
	audio_aic23_write(DIGITAL_INTERFACE_ACT_ADDR, ACT_ON);

	/* clock configuration */
	davinci_set_samplerate(audio_samplerate);
}

#define davinci_aic23suspend	NULL
#define davinci_aic23resume	NULL

static void private_free(struct snd_pcm_runtime *runtime)
{
   printk( KERN_DEBUG "%s:%p\n", __FUNCTION__,runtime->private_data );
   if(runtime->private_data){
      struct dma_playback_info *data = runtime->private_data ;
      davinci_stop_dma(data->master_chan);
      davinci_free_sound_dma(data->master_chan, data->channels, data->ram_to_iram_channel);
      kfree(runtime->private_data);
      runtime->private_data = 0; 
   }
}

static int davinci_pcm_preallocate_dma_buffer(struct snd_pcm_substream *substream)
{
	struct snd_dma_buffer *buf = &substream->dma_buffer ;
	size_t size ;

        buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = substream->pcm->card->dev;
	size = BUFFER_BYTES ;
	buf->private_data = NULL;
        substream->private_data = NULL ;

   {
	struct snd_dma_buffer *iram = kzalloc(sizeof(*iram), GFP_KERNEL);
        BUG_ON(0==iram);
        iram->area = (char *)ioremap(DAVINCI_IRAM_BASE, IRAM_SIZE) + IRAM_RESERVED ; /* 16k */ ;
        iram->addr = DAVINCI_IRAM_BASE + IRAM_RESERVED ;
        memset(iram->area,0,IRAM_LEFT);
	buf->area = dma_alloc_writecombine(buf->dev.dev, size, &buf->addr, GFP_KERNEL);
        printk( KERN_DEBUG "allocated big buffer at %p (0x%x, %u bytes)\n", buf->area, buf->addr, size );
        substream->private_data = iram ;
   }

        if (!buf->area){
		kfree(substream->private_data);
		substream->private_data = NULL ;
		return -ENOMEM;
        }
	buf->bytes = size;
        memset(buf->area,0xff,size);
        printk( KERN_DEBUG "dma bufs at %p (addr 0x%x), size %u\n", buf->area, buf->addr, buf->bytes );
	return 0;
}

static int davinci_pcm_open(struct snd_pcm_substream *substream)
{
   struct snd_pcm_runtime *runtime = substream->runtime;
   struct dma_playback_info *data = 0 ;
   int err ;

   printk( KERN_DEBUG "%s: %lu fragments\n", __FUNCTION__, runtime ? runtime->avail_max : -1 );

   err = davinci_pcm_preallocate_dma_buffer(substream);
   if (err)
      goto errout;

   data = kzalloc(sizeof(*data), GFP_KERNEL);
   if( !data ){
      return -ENOMEM ;
   }

   err = davinci_request_sound_dma( DAVINCI_DMA_MCBSP_TX, substream->name, substream, &data->master_chan, data->channels, &data->ram_to_iram_channel, &data->ram_to_iram_tcc );
   if( err ){
      printk( KERN_ERR "%s: err %d\n", __FUNCTION__, err );
      goto errout ;
   }
   data->ram_to_iram_tcc <<= 12 ;

   runtime->private_free = private_free ;
   runtime->private_data = data ;
   runtime->hw = davinci_pcm_hardware ;

   return 0 ;

errout:
   if(data)
      kfree(data);

   return err ;
}

static int davinci_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
#if 0
	printk( KERN_DEBUG "%s: format 0x%x, rate %u, %u channels, dma %p, %lu frags\n"
                , __FUNCTION__ 
                , runtime->format
                , runtime->rate
                , runtime->channels
                , (void *)runtime->dma_addr
                , runtime->avail_max
                );
	printk( KERN_DEBUG "%s: %u periods of %lu bytes. bufsize == %lu\n"
                , __FUNCTION__
                , runtime->periods
                , runtime->period_size
                , runtime->buffer_size
                );
#endif
        if( audio_samplerate != runtime->rate ){
           davinci_set_samplerate(runtime->rate);
           printk( KERN_DEBUG "samplerate = %d\n", runtime->rate );
        }

	return 0 ;
}

static int davinci_pcm_close(struct snd_pcm_substream *substream)
{
   struct snd_dma_buffer *buf = &substream->dma_buffer ;
   private_free(substream->runtime);
   if( buf->area ){
	mute();
	dma_free_writecombine(buf->dev.dev, buf->bytes, buf->area, buf->addr);
	if( substream->private_data ){
		struct snd_dma_buffer *iram = substream->private_data ;
                iounmap(iram->area);
        	kfree(iram);
                substream->private_data = NULL ;
      }
      memset(buf,0,sizeof(*buf));
   }
   return 0 ;
}

static void audio_set_dma_params_play(int channel, dma_addr_t dma_ptr, u_int dma_size)
{
	davinci_set_dma_src_params(channel, (unsigned long)(dma_ptr), 0, 0);
	davinci_set_dma_dest_params(channel, (unsigned long)MCBSP_DXR, 0, 0);
	davinci_set_dma_src_index(channel, 2, 0);
	davinci_set_dma_dest_index(channel, 0, 0);
	davinci_set_dma_transfer_params(channel, 2, dma_size / 2, 1, 0, ASYNC);
}

static int davinci_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
   struct snd_dma_buffer *buf = &substream->dma_buffer ;
   struct snd_pcm_runtime *runtime = substream->runtime;
   struct dma_playback_info *data = runtime->private_data ;
   unsigned const periodBytes = params_period_bytes(params);
   dma_addr_t addr ;
   int i ;

   BUG_ON(0==buf);
   BUG_ON(0==runtime);
   BUG_ON(0==data);
   BUG_ON(0==substream->private_data);
{
   struct snd_dma_buffer *iram = substream->private_data ;
   addr = iram->addr ;
}
   for( i = 0 ; i < NUM_DMA_CHANNELS ; i++ ){
      audio_set_dma_params_play(data->channels[i], addr, periodBytes );
      addr += periodBytes ;
   }
   snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
   davinci_mcbsp_config(DAVINCI_MCBSP1, &initial_config);
   aic23_configure();
   unmute();
   return 0 ;
}

static int davinci_pcm_hw_free(struct snd_pcm_substream *substream)
{
   snd_pcm_set_runtime_buffer(substream, 0);
   return 0 ;
}

static int davinci_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
        struct dma_playback_info *data = (runtime) ? runtime->private_data : 0 ;
   
	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START: {
			// do something to start the PCM engine
			if(data){
				edmacc_paramentry_regs regs ;
                                gpio_direction_output(MUTE_GPIO,NOTMUTED);
                                data->cur_dma = 0 ;
				davinci_get_dma_params(data->channels[0], &regs);
				davinci_set_dma_params(data->master_chan, &regs);
				davinci_start_dma(data->master_chan);
                                davinci_mcbsp_start(DAVINCI_MCBSP1);
                        }
			break;
   		}
		case SNDRV_PCM_TRIGGER_STOP: {
			// do something to stop the PCM engine
			if(data){
                                gpio_direction_output(MUTE_GPIO,MUTED);
				davinci_mcbsp_stop(DAVINCI_MCBSP1);
				davinci_stop_dma(data->master_chan);
                        }
			break;
                }
   		default:
			return -EINVAL;
	}
	return 0 ;
}

static snd_pcm_uframes_t maxframes = 0 ;
static snd_pcm_uframes_t davinci_pcm_pointer(struct snd_pcm_substream *substream)
{
   struct snd_pcm_runtime *runtime = substream->runtime;
   struct dma_playback_info *data = (runtime) ? runtime->private_data : 0 ;
   struct snd_dma_buffer *iram = substream->private_data ;
   unsigned frames = 0 ;
   
   if(data && iram){
      unsigned long flags;
      unsigned nxt ;
      unsigned period ;
      edmacc_paramentry_regs temp;
      local_irq_save(flags);
      davinci_get_dma_params(data->master_chan, &temp);
      local_irq_restore(flags);
      nxt = (temp.src - iram->addr) % frames_to_bytes(runtime,runtime->period_size);
      period = (data->cur_dma%runtime->periods);
      nxt += period*frames_to_bytes(runtime, runtime->period_size);
      if (nxt >= frames_to_bytes(runtime,runtime->buffer_size)){
         printk( KERN_ERR "runtime overflow: %u of %u bytes (%u/period), %p/%p, period %u/%u/%u\n"
                 , nxt, frames_to_bytes(runtime, runtime->buffer_size), frames_to_bytes(runtime, runtime->period_size)
                 , (void *)temp.src, (void *)iram->addr, data->cur_dma, period, runtime->periods );
         nxt = 0;
      }
      frames = bytes_to_frames(runtime,nxt);
      if( frames > maxframes ){
         maxframes = frames ;
      }
   }
   return frames ;
}

static int davinci_pcm_ioctl(struct snd_pcm_substream *substream, unsigned int cmd, void *arg)
{
   static int prevCmd = 0 ;
   if( cmd != prevCmd ){
      prevCmd = cmd ;
   }
   return snd_pcm_lib_ioctl(substream, cmd, arg);
}


static struct snd_pcm_ops davinci_pcm_ops = {
	.open		= davinci_pcm_open,
	.close		= davinci_pcm_close,
	.ioctl		= davinci_pcm_ioctl,
	.hw_params	= davinci_pcm_hw_params,
	.hw_free	= davinci_pcm_hw_free,
	.prepare	= davinci_pcm_prepare,
	.trigger	= davinci_pcm_trigger,
	.pointer	= davinci_pcm_pointer,
};

static int aic23_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int i ;
	char *firstOut = page ;
	for( i = 0 ; (i < NUM_AIC23_REGS) && (count > 1); i++ ){
		int n = snprintf( page, count-1, "%02x\t%04x\n", i, spi_tlv320aic23_read_value(i));
		page += n ;
		count -= n ;
	}
        *eof = 1;
	return page-firstOut ;
}

static int 
aic23_proc_write( struct file *file, 
		  const char __user *buffer,
                  unsigned long count, 
		  void *data)
{
	char inbuf[80];
	if( count >= sizeof(inbuf))
		count = sizeof(inbuf)-1 ;
	if(0==copy_from_user(inbuf,buffer,count)){
		unsigned reg, value ;
		inbuf[sizeof(inbuf)-1] = '\0' ;
		if( (2 == sscanf(inbuf,"%u %u\n", &reg, &value ))
		    &&
		    (NUM_AIC23_REGS > reg) ) {
                        spi_tlv320aic23_write_value(reg,value);
			return count ;
		}
		else
			return -EINVAL ;
	}
	else
		return -EFAULT ;
}

static int __devinit davinci_aic23probe(struct platform_device *dev)
{
	struct snd_card *card;
        struct snd_kcontrol *kctl ;
	int ret;
	struct snd_pcm *pcm=0;
	int play = 1 ;
	int capt = 0 ;

	aic23_local.volume = DEFAULT_OUTPUT_VOLUME;
	aic23_local.recsrc = SOUND_MASK_MIC;	/* either of SOUND_MASK_LINE/SOUND_MASK_MIC */
	aic23_local.micbias = 1;
	aic23_local.mod_cnt = 0;
	
	ret = -ENOMEM;
	card = snd_card_new(SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1, THIS_MODULE, 0);
	if (!card)
		goto err;

	card->dev = &dev->dev;
	strncpy(card->driver, dev->dev.driver->name, sizeof(card->driver));

	snprintf(card->shortname, sizeof(card->shortname),
		 "%s", "davaic23" );
	snprintf(card->longname, sizeof(card->longname),
		 "%s (%s)", "davaic23", "davaic23" );

	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = 0xffffffff;
	
        ret = snd_pcm_new(card, "davaic23-PCM", 0, play, capt, &pcm);
	if (ret)
		goto err ;

	pcm->private_data = 0 ;
	pcm->private_free = 0 ;

	if (play) {
		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &davinci_pcm_ops);
	}
	if (capt) {
		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &davinci_pcm_ops);
	}

        spi_tlv320aic23_init();

        strcpy(card->mixername, "Mixer davaic23");

        kctl = snd_ctl_new1(&volume_control, card);
        kctl->id.device = 0 ;
	ret = snd_ctl_add(card,kctl);
        if( ret ){
           printk( KERN_ERR "%s: error %d adding volume control\n", __FUNCTION__, ret );
           goto err ;
        }

	ret = snd_card_register(card);
	if (ret == 0) {
		procentry = create_proc_entry(procentryname, 0, NULL);
		if( procentry ){
			procentry->read_proc = aic23_proc_read ;
			procentry->write_proc = aic23_proc_write ;
		}

		mute();
		platform_set_drvdata(dev, card);
		return 0;
	}

 err:
	spi_tlv320aic23_shutdown();
	if (card)
		snd_card_free(card);
	return ret;
}

static int __devexit davinci_aic23remove(struct platform_device *dev)
{
	struct snd_card *card = platform_get_drvdata(dev);

	if (card) {
                spi_tlv320aic23_shutdown();
                platform_set_drvdata(dev, NULL);
		snd_card_free(card);
	}

	return 0;
}

static struct platform_driver davinci_aic23driver = {
	.probe		= davinci_aic23probe,
	.remove		= __devexit_p(davinci_aic23remove),
	.suspend	= davinci_aic23suspend,
	.resume		= davinci_aic23resume,
	.driver		= {
		.name	= "davinci-aic23",
	},
};

static int __init davinci_aic23init(void)
{
	return platform_driver_register(&davinci_aic23driver);
}

static void __exit davinci_aic23exit(void)
{
	platform_driver_unregister(&davinci_aic23driver);
}

module_init(davinci_aic23init);
module_exit(davinci_aic23exit);

MODULE_AUTHOR("Eric Nelson");
MODULE_DESCRIPTION("AIC23 driver for Davinci chips");
MODULE_LICENSE("GPL");

