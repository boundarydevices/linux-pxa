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

#include <asm/setup.h>
#include <asm/io.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/flash.h>

#include <mach/hardware.h>
#include <mach/common.h>
#include <mach/board.h>
#include <mach/psc.h>
#include <mach/nand.h>
#include <mach/gpio.h>
#include <mach/mux.h>

/* other misc. init functions */
void __init davinci_psc_init(void);
void __init davinci_irq_init(void);
void __init davinci_map_common_io(void);
void __init davinci_init_common_hw(void);


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
};

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
		.coherent_dma_mask	= DMA_32BIT_MASK,
	},

	.num_resources	= 2,
	.resource	= nand_resources,
};

static struct platform_device rtc_dev = {
	.name		= "rtc_davinci",
	.id		= -1,
};

static struct platform_device audio_dev = {
	.name		= "davinci-aic23",
	.id		= -1,
};

#if 0
static struct i2c_board_info __initdata i2c_info[] =  {
	{
		I2C_BOARD_INFO("i2c-touch", 0x22),
		.type		= "i2c-touch",
		.platform_data	= &i2c_touch_data,
	},
};
#endif

static struct platform_device *davinci_devices[] __initdata = {
	&nand_device,
	&rtc_dev,
        &audio_dev
};


static void __init
map_io(void)
{
	printk(KERN_ERR "map_io\n");
	davinci_map_common_io();
}

static struct davinci_uart_config davinci_xenon_uart_config __initdata = {
	.enabled_uarts = 7,
};

static struct davinci_board_config_kernel davinci_xenon_config[] __initdata = {
	{ DAVINCI_TAG_UART,     &davinci_xenon_uart_config },
};

static __init void board_init(void)
{
	printk(KERN_ERR "board_init\n");
	davinci_board_config = davinci_xenon_config;
	davinci_board_config_size = ARRAY_SIZE(davinci_xenon_config);
	davinci_psc_init();
	gpio_direction_output(50, 1);	/* turn off USB power */
#if defined(CONFIG_BLK_DEV_DAVINCI) || defined(CONFIG_BLK_DEV_DAVINCI_MODULE)
	printk(KERN_WARNING "WARNING: both IDE and NOR flash are enabled, "
	       "but share pins.\n\t Disable IDE for NOR support.\n");
#endif
	davinci_mux_peripheral(DAVINCI_MUX_UART1, 1);
	davinci_mux_peripheral(DAVINCI_MUX_UART2, 1);

	platform_add_devices(davinci_devices,
			     ARRAY_SIZE(davinci_devices));
	davinci_serial_init();
	setup_usb(500, 8);
}

static __init void irq_init(void)
{
	printk(KERN_ERR "irq_init\n");
	davinci_init_common_hw();
	davinci_irq_init();
	set_irq_type(IRQ_GPIO(18), IRQ_TYPE_EDGE_RISING);
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
