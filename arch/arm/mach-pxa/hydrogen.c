/*
 *  linux/arch/arm/mach-pxa/hydrogen.c
 *
 *  Support for the Boundary Devices Hydrogen board, a
 *  PXA-270 based single board computer with on-board
 *  relays and support for an Okaya QVGA display
 *
 *  Author:	Eric Nelson
 *  Created:	June 27, 2008
 *  Copyright:	Boundary Devices
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/sysdev.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/bitops.h>
#include <linux/fb.h>
#include <linux/ioport.h>
#include <linux/bootmem.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/backlight.h>
#include <linux/dma-mapping.h>
#include <net/ax88796.h>
#include <sound/ac97_codec.h>

#include <asm/types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/sizes.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>
#include <asm/mach/flash.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa2xx-regs.h>
#include <asm/arch/pxa2xx-gpio.h>
#include <asm/arch/audio.h>
#include <asm/arch/mmc.h>
#include <asm/arch/gpio.h>
#include <linux/mmc/host.h>
#include <asm/arch/irda.h>
#include <asm/arch/ohci.h>
#include <asm/arch/i2c.h>

#include "generic.h"
#include "devices.h"
#include "read_regs.h"

#define MMC_CARD_DETECT_GP 36
/* UCB1400 registers */
#define AC97_IO_DATA_REG          0x005A
#define AC97_IO_DIRECTION_REG     0x005C
#define BOUNDARY_AC97_MUTE        (0+(1<<4))
#define BOUNDARY_AC97_UNMUTE      ((1<<8)+(1<<4))
#define BOUNDARY_AC97_OUTPUTS 0x0101

static void __init hydrogen_init_irq(void)
{
	int gpdr = GPDR(0);	//0-31
	pxa27x_init_irq();
	set_irq_type(IRQ_GPIO(22), IRQT_FALLING);	//pcmcia irq
	if ((gpdr & (1 << 4)) == 0)
		set_irq_type(IRQ_GPIO(4), IRQT_RISING);	/* UCB1400 Interrupt, neon board  */
	if ((gpdr & (1 << 5)) == 0)
		set_irq_type(IRQ_GPIO(5), IRQT_RISING);	/* SM501 Interrupt, neon,neon-b board  */
	if ((gpdr & (1 << 23)) == 0)
		set_irq_type(IRQ_GPIO(23), IRQT_RISING);	/* UCB1400 Interrupt, neon-b board  */
	if ((gpdr & (1 << 24)) == 0)
		set_irq_type(IRQ_GPIO(24), IRQT_RISING);	/* 91c111 Interrupt, sm501 board  */
        GPDR(0) = gpdr & ~(1<<12);
        set_irq_type(IRQ_GPIO(12), IRQ_TYPE_EDGE_FALLING);	/* Asix */
	set_irq_type(IRQ_GPIO(MMC_CARD_DETECT_GP), IRQT_FALLING);	//MMC card detect
        set_irq_type(IRQ_GPIO(3), IRQ_TYPE_EDGE_FALLING);	/* i2c touch */
}

static void __init
fixup_hydrogen(struct machine_desc *desc, struct tag *t,
	       char **cmdline, struct meminfo *mi)
{
	if (t->hdr.tag != ATAG_CORE) {
		unsigned long size = mi->bank[0].size;
		if ((size & 0xfffff) || (size == 0)) {
			size = 32 << 20;
			printk(KERN_ERR "!!!!! Invalid memory size\r\n");
		}
		SET_BANK(0, 0xa0000000, size);
		mi->nr_banks = 1;
	}
}

#ifdef CONFIG_SND_AC97_CODEC
extern struct snd_ac97 *pxa2xx_ac97_ac97;

static int audio_startup(struct snd_pcm_substream *substream, void *priv)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
		snd_ac97_write(pxa2xx_ac97_ac97, AC97_IO_DIRECTION_REG, BOUNDARY_AC97_OUTPUTS );
		snd_ac97_write(pxa2xx_ac97_ac97, AC97_IO_DATA_REG, BOUNDARY_AC97_UNMUTE );
	}
	return 0;
}

static void audio_shutdown(struct snd_pcm_substream *substream, void *priv)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
		snd_ac97_write(pxa2xx_ac97_ac97, AC97_IO_DATA_REG, BOUNDARY_AC97_MUTE );
	}
}

static void audio_suspend(void *priv)
{
	snd_ac97_write(pxa2xx_ac97_ac97, AC97_IO_DATA_REG, BOUNDARY_AC97_MUTE );
}

static void audio_resume(void *priv)
{
	snd_ac97_write(pxa2xx_ac97_ac97, AC97_IO_DATA_REG, BOUNDARY_AC97_UNMUTE );
}

static pxa2xx_audio_ops_t audio_ops = {
	.startup	= audio_startup,
	.shutdown	= audio_shutdown,
	.suspend	= audio_suspend,
	.resume		= audio_resume,
};

static struct platform_device hydrogen_audio_device = {
	.name		= "pxa2xx-ac97",
	.id		= -1,
	.dev		= { .platform_data = &audio_ops },
};
#endif

/* Asix AX88796 10/100 ethernet controller */

static struct ax_plat_data asix_platform_data = {
	.flags		= AXFLG_HAS_EEPROM,
	.wordlength	= 2,
	.dcr_val	= 0x49,
	.rcr_val	= 0x1F,
};

#define ASIX_IRQ (IRQ_GPIO(12))

static struct resource asix_resources[] = {
	[0] = {
		.start = PXA_CS1_PHYS,
		.end   = PXA_CS1_PHYS + (0x20 * 0x20) -1,
		.flags = IORESOURCE_MEM
	},
	[1] = {
		.start = ASIX_IRQ,
		.end   = ASIX_IRQ,
		.flags = IORESOURCE_IRQ
	}
};

static struct platform_device asix_device = {
	.name		= "ax88796",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(asix_resources),
	.resource	= asix_resources,
	.dev		= {
		.platform_data = &asix_platform_data,
	}
};

static u64 pxafb_yuv_dma_mask = DMA_BIT_MASK(32);
static struct platform_device pxafb_yuv_device = {
	.name		= "pxafb_yuv",
	.id		= 3,
	.num_resources	= 0,
	.resource	= 0,
	.dev		= {
		.dma_mask = &pxafb_yuv_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

static struct platform_device *platform_devices[] __initdata = {
#ifdef CONFIG_SND_AC97_CODEC
        &hydrogen_audio_device,
#endif
        &asix_device,
        &pxafb_yuv_device
};

static struct pxafb_mode_info fb_modes __initdata = {
	.pixclock = 7400000,	//(3-1)
	.xres = 320,
	.yres = 240,
	.bpp = 16,
	.hsync_len = 30,
	.left_margin = 20,
	.right_margin = 28,
	.vsync_len = 3,
	.upper_margin = 5,
	.lower_margin = 15,
	.sync = 0, // !FB_SYNC_HOR_HIGH_ACT !FB_SYNC_VERT_HIGH_ACT,
};

static struct pxafb_mach_info fb_hw = {
	.num_modes = 1,
	.lccr0 = LCCR0_Act,
	.lccr3 = LCCR3_PCP,
};

#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
static int hydrogen_backlight_update_status(struct backlight_device *bl)
{
	int brightness = bl->props.brightness;
	int duty;
	if ((bl->props.power != FB_BLANK_UNBLANK) ||
	    (bl->props.fb_blank != FB_BLANK_UNBLANK))
		brightness = 0;

	duty = brightness ^ 0x3ff;	/* on this panel the duty cycle need inverted */
	if (duty && (duty != bl->props.max_brightness)) {
		gpio_set_value(GPIO17_PWM1, 1);
		pxa_gpio_mode(GPIO16_PWM0_MD);
		pxa_gpio_mode(GPIO17_PWM1 | GPIO_OUT);
		pxa_set_cken(CKEN_PWM0, 1);
		PWM_CTRL0 = 0;
		PWM_PWDUTY0 = duty;
		PWM_PERVAL0 = bl->props.max_brightness;
	} else {
//value is either high or low, PWM not needed
		PWM_CTRL0 = 0;
		PWM_PWDUTY0 = duty;
		PWM_PERVAL0 = bl->props.max_brightness;
		gpio_set_value(GPIO16_PWM0, (duty)? 1 : 0);
		gpio_set_value(GPIO17_PWM1, (brightness)? 1 : 0);
		pxa_gpio_mode(GPIO16_PWM0 | GPIO_OUT);
		pxa_gpio_mode(GPIO17_PWM1 | GPIO_OUT);
		pxa_set_cken(CKEN_PWM0, 0);
	}
	return 0;
}

static int hydrogen_backlight_get_brightness(struct backlight_device *bl)
{
	return PWM_PWDUTY0 ^ 0x3ff;
}

static /*const*/ struct backlight_ops hydrogen_backlight_ops = {
	.update_status	= hydrogen_backlight_update_status,
	.get_brightness	= hydrogen_backlight_get_brightness,
};

static void __init hydrogen_backlight_register(void)
{
	struct backlight_device *bl;

	bl = backlight_device_register("hydrogen-bl", &pxa_device_fb.dev,
				       NULL, &hydrogen_backlight_ops);
	if (IS_ERR(bl)) {
		printk(KERN_ERR "hydrogen: unable to register backlight: %ld\n",
		       PTR_ERR(bl));
		return;
	}
	bl->props.max_brightness = 1023;
	bl->props.brightness = 1023;
	backlight_update_status(bl);
}
#else
#define hydrogen_backlight_register()	do { } while (0)
#endif

static int hydrogen_mci_init(struct device *dev, irq_handler_t intHandler,
			     void *data)
{
	int err;

	/*
	 * setup GPIO for PXA27x MMC controller
	 */
	pxa_gpio_mode(GPIO32_MMCCLK_MD);
	pxa_gpio_mode(GPIO112_MMCCMD_MD);
	pxa_gpio_mode(GPIO92_MMCDAT0_MD);
	pxa_gpio_mode(GPIO109_MMCDAT1_MD);
	pxa_gpio_mode(GPIO110_MMCDAT2_MD);
	pxa_gpio_mode(GPIO111_MMCDAT3_MD);

	err =
	    request_irq(IRQ_GPIO(MMC_CARD_DETECT_GP), intHandler, IRQF_DISABLED,
			"MMC card detect", data);
	if (err) {
		printk(KERN_ERR
		       "hydrogen_mci_init: MMC/SD: can't request MMC card detect IRQ\n");
		return -1;
	}

	return 0;
}

static void hydrogen_mci_setpower(struct device *dev, unsigned int vdd)
{
}

static void hydrogen_mci_exit(struct device *dev, void *data)
{
	free_irq(IRQ_GPIO(MMC_CARD_DETECT_GP), data);
}

static struct pxamci_platform_data hydrogen_mci_platform_data = {
	.ocr_mask = MMC_VDD_32_33 | MMC_VDD_33_34,
	.init = hydrogen_mci_init,
	.setpower = hydrogen_mci_setpower,
	.exit = hydrogen_mci_exit,
};

static int hydrogen_ohci_init(struct device *dev)
{
	unsigned uhchr = UHCHR;
	unsigned gpdr = GPDR(88);
	/* Set the Power Control Polarity Low and Power Sense
	   Polarity Low to active low. */
	uhchr = (uhchr | UHCHR_PCPL | UHCHR_PSPL);
	uhchr &= ~(UHCHR_SSEP1 | UHCHR_SSEP2 | UHCHR_SSEP3 | UHCHR_SSE);
	if (gpdr & (1 << (88 & 0x1f))) {
		/* Output, Power sense not used, make active high */
		uhchr &= ~(UHCHR_PCPL | UHCHR_PSPL);
	}
	UHCHR = uhchr;
	return 0;
}

static struct pxaohci_platform_data hydrogen_ohci_platform_data = {
	.port_mode = PMM_PERPORT_MODE,
	.init = hydrogen_ohci_init,
};

static void __init hydrogen_init(void)
{
	/* system bus arbiter setting
	 * - Core_Park
	 * - LCD_wt:DMA_wt:CORE_Wt = 2:3:4
	 */
	ARB_CNTRL = ARB_CORE_PARK | 0x234;

	pxa_gpio_mode(GPIO45_SYSCLK_AC97_MD);

	printk( KERN_ERR "%s: %u devices\n", __func__, ARRAY_SIZE(platform_devices));
	fb_hw.modes = &fb_modes;
	set_pxa_fb_info(&fb_hw);
	platform_add_devices(platform_devices, ARRAY_SIZE(platform_devices));
	hydrogen_backlight_register();

	pxa_set_mci_info(&hydrogen_mci_platform_data);
	pxa_set_ohci_info(&hydrogen_ohci_platform_data);
	pxa_set_i2c_info(NULL);

	pxa_mode_from_registers(&pxa_device_fb);
}

#define DEBUG_SIZE (PAGE_SIZE*4)
static struct map_desc hydrogen_io_desc[] __initdata = {
 /* virtual      	      pfn    	    length      domain       r  w  c  b */
 { 0xfff00000, __phys_to_pfn(0x00000000), DEBUG_SIZE, MT_HIGH_VECTORS },	//DOMAIN_USER,   1, 0, 1, 1for debugging variables, DOMAIN_USER because of errata on exiting SDS
};
static void __init hydrogen_map_io(void)
{
	void* init_maps;
	void* src=(void *)0xff000000;
	pxa_map_io();

	/* initialize sleep mode regs (wake-up sources, etc) */
	PGSR0 = 0x00008800;
	PGSR1 = 0x00000002;
	PGSR2 = 0x0001FC00;
	PGSR3 = 0x00001F81;
	PWER = 0xC0000002;
	PRER = 0x00000002;
	PFER = 0x00000002;
	/*
	 * Create a mapping for 1st pages of flash and DEBUG variables
	 * This is copied to ram to allow debugging while flash is being written.
	 * It also allows caching, and faster access on cache miss.
	 * Also, user mode need read access because of errata in exiting SDS
	 */
	init_maps = alloc_bootmem_low_pages(DEBUG_SIZE);
	memcpy(init_maps,src,DEBUG_SIZE);
	cpu_dcache_clean_area(init_maps,DEBUG_SIZE);

	hydrogen_io_desc[0].pfn = __phys_to_pfn(virt_to_phys(init_maps));
	iotable_init(hydrogen_io_desc,ARRAY_SIZE(hydrogen_io_desc));
}

MACHINE_START(SCANPASS, "Boundary Devices Hydrogen Board")
	/* Maintainer: Boundary Devices */
	.phys_io = 0x40000000,
	.io_pg_offst = (io_p2v(0x40000000) >> 18) & 0xfffc,
	.boot_params = 0xa0000100,	/* BLOB boot parameter setting */
	.map_io = hydrogen_map_io,
	.init_irq = hydrogen_init_irq,
	.fixup = fixup_hydrogen,
	.timer = &pxa_timer,
	.init_machine = hydrogen_init, 
MACHINE_END
