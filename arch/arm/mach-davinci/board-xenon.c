/*
 * linux/arch/arm/mach-davinci/board-xenon.c
 *
 * TI DaVinci XENON board
 *
 * Copyright (C) 2007 Boundary Devices.
 *
 * Based on board-evm.c from Texas Instruments
 *
 * ----------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * ----------------------------------------------------------------------------
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <linux/irq.h>

#include <linux/io.h>
#include <linux/phy.h>
#include <linux/clk.h>

#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/flash.h>

#include <mach/hardware.h>
#include <mach/common.h>
#include <mach/i2c.h>
#include <mach/psc.h>
#include <mach/nand.h>
#include <mach/gpio.h>
#include <mach/mux.h>
#include <mach/serial.h>
#include <mach/mmc.h>
#include <mach/dm644x.h>
#include <mach/usb.h>

#define DAVINCI_ASYNC_EMIF_CONTROL_BASE   0x01e00000
#define DAVINCI_ASYNC_EMIF_DATA_CE0_BASE  0x02000000
#define DAVINCI_ASYNC_EMIF_DATA_CE1_BASE  0x04000000
#define DAVINCI_ASYNC_EMIF_DATA_CE2_BASE  0x06000000
#define DAVINCI_ASYNC_EMIF_DATA_CE3_BASE  0x08000000

/* other misc. init functions */
void __init davinci_irq_init(void);
void __init davinci_map_common_io(void);


#if 0
static struct mtd_partition nand_partitions[] = {
	/* bootloader (U-Boot, etc) in first sector */
	{
		.name		= "bootloader",
		.offset		= 0,
		.size		= SZ_256K,
		.mask_flags	= MTD_WRITEABLE, /* force read-only */
	},
	/* bootloader params in the next sector */
	{
		.name		= "params",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_128K,
		.mask_flags	= MTD_WRITEABLE, /* force read-only */
	},
	/* kernel */
	{
		.name		= "kernel",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_4M,
		.mask_flags	= 0
	},
	/* file system */
	{
		.name		= "filesystem",
		.offset		= MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,
		.mask_flags	= 0
	}
};
#endif

static struct davinci_flash_platform_data nand_data = {
	.timings	= 0
	| (0 << 31)	/* selectStrobe */
	| (0 << 30)	/* extWait */
	| (1 << 26)	/* writeSetup	20 ns */
	| (3 << 20)	/* writeStrobe	40 ns */
	| (1 << 17)	/* writeHold	20 ns */
	| (0 << 13)	/* readSetup	10 ns */
	| (2 << 7)	/* readStrobe	30 ns */
	| (0 << 4)	/* readHold	10 ns */
	| (3 << 2),	/* turnAround	10 ns */
	.parts		= 0,
	.nr_parts	= 0,
	.chip_num	= 0, /* 0 - cs2, 1 - cs3, 2 - cs4, 3 - cs5 */
	.ecc_mode	= NAND_ECC_HW,
};

#define DAVINCI_ASYNC_EMIF_DATA_CE0_BASE  0x02000000
static struct resource nand_resources[] = {
	{
		.start	= DAVINCI_ASYNC_EMIF_DATA_CE0_BASE,
		.end	= DAVINCI_ASYNC_EMIF_DATA_CE0_BASE + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	}, {
		.start	= IRQ_EMIF_EMWAIT_RISE, /* IRQ_GPIO(18), IRQ_EMIF_EMWAIT_RISE */
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device nand_device = {
	.name		= "davinci_nand",
	.id		= 0,
	.dev		= {
		.platform_data	= &nand_data,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},

	.num_resources	= 2,
	.resource	= nand_resources,
};

static u64 davinci_fb_dma_mask = DMA_BIT_MASK(32);

static struct platform_device davinci_fb_device = {
	.name		= "davincifb",
	.id		= -1,
	.dev = {
		.dma_mask		= &davinci_fb_dma_mask,
		.coherent_dma_mask      = DMA_BIT_MASK(32),
	},
	.num_resources = 0,
};

static struct platform_device rtc_dev = {
	.name		= "rtc_davinci",
	.id		= -1,
};

static struct platform_device audio_dev = {
	.name		= "davinci-aic23",
	.id		= -1,
};

static struct snd_platform_data snd_data ={
	.enable_channel_combine = 1,
	.sram_size_playback	= 7 * 512,    /* Size of ping + pong buffer*/
	.sram_size_capture	= 7 * 512,    /* Size of ping + pong buffer*/
};

static struct i2c_board_info __initdata i2c_info[] =  {
	{
		I2C_BOARD_INFO("ths8200", 0x21),
		.type		= "ths8200",
	},
	{
		I2C_BOARD_INFO("Pic16F616-ts", 0x22),
		.type		= "Pic16F616-ts",
//		.platform_data	= &i2c_touch_data,
	},
};

static struct platform_device *davinci_devices[] __initdata = {
	&nand_device,
	&davinci_fb_device,
	&rtc_dev,
        &audio_dev
};


static void __init
map_io(void)
{
	dm644x_init();
}

static int mmc_get_cd(int module)
{
	return (0 == gpio_get_value(49));
}

static int mmc_get_ro(int module)
{
	return (0 != gpio_get_value(17));
}

static struct davinci_mmc_config mmc_config = {
	.get_cd		= mmc_get_cd,
	.get_ro		= mmc_get_ro,
	.wires		= 4
};

static struct davinci_uart_config uart_config __initdata = {
	.enabled_uarts = 7,
};

static __init void board_init(void)
{
	struct clk *aemif_clk;
	aemif_clk = clk_get(NULL, "aemif");
	clk_enable(aemif_clk);
	clk_put(aemif_clk);

	printk(KERN_ERR "board_init\n");
	gpio_request(50, "USB Power");
	gpio_direction_output(50, 1);	/* turn off USB power */
	davinci_init_i2c(NULL);
	i2c_register_board_info(1, i2c_info, ARRAY_SIZE(i2c_info));
#if defined(CONFIG_BLK_DEV_DAVINCI) || defined(CONFIG_BLK_DEV_DAVINCI_MODULE)
	printk(KERN_WARNING "WARNING: both IDE and NOR flash are enabled, "
	       "but share pins.\n\t Disable IDE for NOR support.\n");
#endif

	platform_add_devices(davinci_devices,
			     ARRAY_SIZE(davinci_devices));
	davinci_serial_init(&uart_config);
	dm644x_init_asp(&snd_data);
	davinci_setup_usb(500, 8);
	davinci_setup_mmc(0, &mmc_config);
}

static __init void irq_init(void)
{
	printk(KERN_ERR "irq_init\n");
	davinci_irq_init();
//	set_irq_type(IRQ_GPIO(18), IRQ_TYPE_EDGE_RISING); /* 1st board NAND ready was gp18 */
	set_irq_type(IRQ_GPIO(11), IRQ_TYPE_EDGE_FALLING);
}


MACHINE_START(XENON, "Xenon")
	/* Maintainer: Boundary Devices */
	.phys_io	= IO_PHYS,
	.io_pg_offst	= (__IO_ADDRESS(IO_PHYS) >> 18) & 0xfffc,
	.boot_params	= (DAVINCI_DDR_BASE + 0x100),
	.map_io		= map_io,
	.init_irq	= irq_init,
	.timer		= &davinci_timer,
	.init_machine	= board_init,
MACHINE_END
