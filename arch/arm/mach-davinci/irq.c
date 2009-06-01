/*
 * Interrupt handler for DaVinci boards.
 *
 * Copyright (C) 2006 Texas Instruments.
 * 2007 Boundary Devices. Troy Kisky <troy.kisky@boundarydevices.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <mach/cputype.h>
#include <mach/common.h>
#include <asm/mach/irq.h>
#include <mach/gpio.h>

#define FIQ_REG0_OFFSET		0x0000
#define FIQ_REG1_OFFSET		0x0004
#define IRQ_REG0_OFFSET		0x0008
#define IRQ_REG1_OFFSET		0x000C
#define IRQ_ENT_REG0_OFFSET	0x0018
#define IRQ_ENT_REG1_OFFSET	0x001C
#define IRQ_INCTL_REG_OFFSET	0x0020
#define IRQ_EABASE_REG_OFFSET	0x0024
#define IRQ_INTPRI0_REG_OFFSET	0x0030
#define IRQ_INTPRI7_REG_OFFSET	0x004C

const u8 *davinci_def_priorities;
#ifdef DEBUG
#define dbg(format, arg...) printk(KERN_DEBUG format "\n" , ## arg)
#else
#define dbg(format, arg...) do {} while (0)
#endif

#define INTC_BASE IO_ADDRESS(DAVINCI_ARM_INTC_BASE)

static inline unsigned int davinci_irq_readl(int offset)
{
	return __raw_readl(davinci_intc_base + offset);
}

static inline void davinci_irq_writel(unsigned long value, int offset)
{
	__raw_writel(value, davinci_intc_base + offset);
}

/* Disable interrupt */
static void davinci_mask_irq0(unsigned int irq)
{
	u32 l = davinci_irq_readl(IRQ_ENT_REG0_OFFSET);
	l &= ~(1 << irq);
	davinci_irq_writel(l, IRQ_ENT_REG0_OFFSET);
}
/* Disable interrupt */
static void davinci_mask_irq1(unsigned int irq)
{
	u32 l = davinci_irq_readl(IRQ_ENT_REG1_OFFSET);
	l &= ~(1 << (irq&0x1f));
	davinci_irq_writel(l, IRQ_ENT_REG1_OFFSET);
}

/* Enable interrupt */
static void davinci_unmask_irq0(unsigned int irq)
{
	u32 l = davinci_irq_readl(IRQ_ENT_REG0_OFFSET);
	l |= (1 << irq);
	davinci_irq_writel(l, IRQ_ENT_REG0_OFFSET);
}
/* Enable interrupt */
static void davinci_unmask_irq1(unsigned int irq)
{
	u32 l = davinci_irq_readl(IRQ_ENT_REG1_OFFSET);
	l |= (1 << (irq&0x1f));
	davinci_irq_writel(l, IRQ_ENT_REG1_OFFSET);
}

/* EOI interrupt */
static void davinci_ack_irq0(unsigned int irq)
{
	davinci_irq_writel((1 << irq), IRQ_REG0_OFFSET);
}
/* EOI interrupt */
static void davinci_ack_irq1(unsigned int irq)
{
	davinci_irq_writel((1 << (irq&0x1f)), IRQ_REG1_OFFSET);
}

static struct irq_chip davinci_irq_chip_0 = {
	.name	= "AINTC0",
	.ack	= davinci_ack_irq0,
	.mask	= davinci_mask_irq0,
	.unmask = davinci_unmask_irq0,
};
static struct irq_chip davinci_irq_chip_1 = {
	.name	= "AINTC1",
	.ack	= davinci_ack_irq1,
	.mask	= davinci_mask_irq1,
	.unmask = davinci_unmask_irq1,
};

/* ---------------------------------------------  */
static long GPIO_IRQ_rising_edge[3];
static long GPIO_IRQ_falling_edge[3];
static long GPIO_IRQ_mask[3];

static int davinci_gpio_irq_set_type(unsigned int irq, unsigned int type)
{
	int gp = IRQ_TO_GPIO(irq);
	int idx = gp >> 5;
	int mask = 1<<(gp&0x1f);
	struct gpio_controller  *__iomem g = __gpio_to_controller(gp);

	if (type == IRQ_TYPE_PROBE) {
		unsigned int dir;
	    /* Don't mess with enabled GPIOs using preconfigured edges or
	       GPIOs set to output during probe */
		dir = __raw_readl(&g->dir);	/* 1 means input */
		if ((GPIO_IRQ_rising_edge[idx] | GPIO_IRQ_falling_edge[idx] |
				~dir) & mask)
			return 0;
		type = IRQ_TYPE_EDGE_BOTH;
	}

	{
		unsigned int dir;
		unsigned long flags;
		local_irq_save(flags);
		/* 1 means input, make sure gp is an input */
		dir = __raw_readl(&g->dir);
		__raw_writel(dir|mask, &g->dir);
		local_irq_restore(flags);
	}
	if (type & IRQ_TYPE_EDGE_RISING) {
		__set_bit(gp, GPIO_IRQ_rising_edge);
	} else {
		__clear_bit(gp, GPIO_IRQ_rising_edge);
	}

	if (type & IRQ_TYPE_EDGE_FALLING) {
		__set_bit(gp, GPIO_IRQ_falling_edge);
	} else {
		__clear_bit(gp, GPIO_IRQ_falling_edge);
	}

	if (GPIO_IRQ_mask[idx] & mask) {
		__raw_writel(mask, (GPIO_IRQ_rising_edge[idx]&mask)?
				&g->set_rising : &g->clr_rising);
		__raw_writel(mask, (GPIO_IRQ_falling_edge[idx]&mask)?
				&g->set_falling : &g->clr_falling);
	}
	return 0;
}

/*
 * GPIO IRQs must be acknowledged.  This is for GPIO 0-7.
 */
static void davinci_ack_low_gpio(unsigned int irq)
{
	int gp = irq - IRQ_GPIO0;
	struct gpio_controller  *__iomem g = __gpio_to_controller(0);
	__raw_writel((1 << gp), &g->intstat);
	davinci_irq_writel((1 << (irq&0x1f)), IRQ_REG1_OFFSET);
	dbg("irq davinci_ack_low_gpio");
}

static struct irq_chip davinci_low_gpio_chip = {
	.name		= "low gpio",
	.ack		= davinci_ack_low_gpio,
	.mask		= davinci_mask_irq1,
	.unmask		= davinci_unmask_irq1,
	.set_type	= davinci_gpio_irq_set_type,
};
/* ---------------------------------------------  */
/*
 * Demux handler for GPIO>=8 edge detect interrupts
 */
static void davinci_gpio_demux_handler(unsigned int irq, struct irq_desc *desc)
{
	int index = irq-IRQ_GPIOBNK0;
	/* 16 bits/bank */
	struct gpio_controller  *__iomem g = __gpio_to_controller(index<<4);
	unsigned int mask;
	do {
		int irqBase;
		unsigned cur_level;
		davinci_irq_writel((1 << (irq&0x1f)), IRQ_REG1_OFFSET);
		mask = __raw_readl(&g->intstat);
		/* remove status bits not relevent to this interrrupt */
		mask &= (index&1)? 0xffff0000 : ((index)? 0xffff : 0xff00);
		dbg("irq davinci_gpio_demux_handler, irq=%i, mask=0x%08x",
			irq, mask);

		if (!mask)
			break;
		/* acknowledge changes */
		__raw_writel(mask, &g->intstat);
		cur_level = g->in_data;
		irqBase = IRQ_GPIO(8)-8 + ((index&~0x1)<<4);
		do {
			/* subtract 1 because ffs will
			 * return 1-32 instead of 0-31
			 */
			int i = ffs(mask) - 1;
			unsigned m = (1<<i);
			mask &= ~m;
			if (cur_level & m) {
				if (!(GPIO_IRQ_rising_edge[(index >> 1)] & m))
					continue;
			} else {
				if (!(GPIO_IRQ_falling_edge[(index >> 1)] & m))
					continue;
			}
			desc_handle_irq(irqBase+i, irq_desc+irqBase+i);
		} while (mask);
	} while (1);
}

static void davinci_ack_muxed_gpio(unsigned int irq)
{
	int gp = irq - (IRQ_GPIO(8)-8);
	int mask = 1<<(gp&0x1f);
	struct gpio_controller  *__iomem g = __gpio_to_controller(gp);
	__raw_writel(mask, &g->intstat);
	dbg("irq=%i gp=%i mask=%x davinci_ack_muxed_gpio", irq, gp, mask);
}

static void davinci_mask_muxed_gpio(unsigned int irq)
{
	int gp = irq - (IRQ_GPIO(8)-8);
	int mask = 1<<(gp&0x1f);
	struct gpio_controller  *__iomem g = __gpio_to_controller(gp);
	__clear_bit(gp, GPIO_IRQ_mask);
	__raw_writel(mask, &g->clr_rising);
	__raw_writel(mask, &g->clr_falling);
	dbg("irq=%i gp=%i mask=%x davinci_mask_muxed_gpio", irq, gp, mask);
}

static void davinci_unmask_muxed_gpio(unsigned int irq)
{
	int gp = irq - (IRQ_GPIO(8)-8);
	int mask = 1<<(gp&0x1f);
	int idx = gp >> 5;
	struct gpio_controller  *__iomem g = __gpio_to_controller(gp);
	__set_bit(gp, GPIO_IRQ_mask);
	if (GPIO_IRQ_rising_edge[idx] & mask)
		__raw_writel(mask, &g->set_rising);
	if (GPIO_IRQ_falling_edge[idx] & mask)
		__raw_writel(mask, &g->set_falling);
	dbg("irq=%i gp=%i mask=%x davinci_unmask_muxed_gpio %c%c",
		irq, gp, mask, (GPIO_IRQ_rising_edge[idx] & mask)?'r':' ',
		(GPIO_IRQ_falling_edge[idx] & mask)?'f':' ');
}
static struct irq_chip davinci_muxed_gpio_chip = {
	.name		= "muxed gpio",
	.ack		= davinci_ack_muxed_gpio,
	.mask		= davinci_mask_muxed_gpio,
	.unmask		= davinci_unmask_muxed_gpio,
	.set_type	= davinci_gpio_irq_set_type,
};
/* ---------------------------------------------  */
/* EMIF Register offsets */
#define EMIF_EIMR	0x44
#define EMIF_EIMSR	0x48
#define EMIF_EIMCR	0x4c

#define DAVINCI_ASYNC_EMIF_CNTRL_BASE	(0x01E00000)

#define read_emif(offset) __raw_readl( \
	IO_ADDRESS(DAVINCI_ASYNC_EMIF_CNTRL_BASE+offset))
#define write_emif(val, offset) __raw_writel(val, \
	IO_ADDRESS(DAVINCI_ASYNC_EMIF_CNTRL_BASE+offset))
/*
 * Demux handler for emif interrupts
 */
static void davinci_emif_demux_handler(unsigned int irq, struct irq_desc *desc)
{
	unsigned int mask;

	do {
		davinci_irq_writel((1 << irq), IRQ_REG0_OFFSET);
		mask = read_emif(EMIF_EIMR) & 0x5;
		if (!mask)
			break;
		write_emif(mask, EMIF_EIMR);	/* acknowledge changes */
		if (mask & 1)
			desc_handle_irq(IRQ_EMIF_EMWAIT_TIMEOUT,
				irq_desc+IRQ_EMIF_EMWAIT_TIMEOUT);
		if (mask & 4)
			desc_handle_irq(IRQ_EMIF_EMWAIT_RISE,
				irq_desc+IRQ_EMIF_EMWAIT_RISE);
	} while (1);
}

static void davinci_ack_muxed_emif(unsigned int irq)
{
	unsigned int mask = (irq == IRQ_EMIF_EMWAIT_TIMEOUT)? 1 :
		((irq == IRQ_EMIF_EMWAIT_RISE)? 4 : 0);
	if (mask)
		write_emif(mask, EMIF_EIMR);
}
static void davinci_mask_muxed_emif(unsigned int irq)
{
	unsigned int mask = (irq == IRQ_EMIF_EMWAIT_TIMEOUT)? 1 :
		((irq == IRQ_EMIF_EMWAIT_RISE)? 4 : 0);
	if (mask)
		write_emif(mask, EMIF_EIMCR);
}
static void davinci_unmask_muxed_emif(unsigned int irq)
{
	unsigned int mask = (irq == IRQ_EMIF_EMWAIT_TIMEOUT)? 1 :
		((irq == IRQ_EMIF_EMWAIT_RISE)? 4 : 0);
	if (mask)
		write_emif(mask, EMIF_EIMSR);
}
static struct irq_chip davinci_muxed_emif_chip = {
	.name		= "muxed emif",
	.ack		= davinci_ack_muxed_emif,
	.mask		= davinci_mask_muxed_emif,
	.unmask		= davinci_unmask_muxed_emif,
};
/* ---------------------------------------------  */

/* ARM Interrupt Controller Initialization */
void __init davinci_irq_init(void)
{
	int irq;
	unsigned i;
	const u8 *davinci_def_priorities = davinci_soc_info.intc_irq_prios;

	/* Disable all interrupts */
	davinci_irq_writel(0x0, IRQ_ENT_REG0_OFFSET);
	davinci_irq_writel(0x0, IRQ_ENT_REG1_OFFSET);

	/* Interrupts disabled immediately, IRQ entry reflects all */
	davinci_irq_writel(0x0, IRQ_INCTL_REG_OFFSET);

	/* we don't use the hardware vector table, just its entry addresses */
	davinci_irq_writel(0, IRQ_EABASE_REG_OFFSET);

	/* Clear all interrupt requests */
	davinci_irq_writel(~0x0, FIQ_REG0_OFFSET);
	davinci_irq_writel(~0x0, FIQ_REG1_OFFSET);
	davinci_irq_writel(~0x0, IRQ_REG0_OFFSET);
	davinci_irq_writel(~0x0, IRQ_REG1_OFFSET);

	for (i = IRQ_INTPRI0_REG_OFFSET;
			i <= IRQ_INTPRI7_REG_OFFSET; i += 4) {
		unsigned	j;
		u32		pri;

		for (j = 0, pri = 0; j < 32; j += 4, davinci_def_priorities++)
			pri |= (*davinci_def_priorities & 0x07) << j;
		davinci_irq_writel(pri, i);
	}

	/* set up genirq dispatch for ARM INTC */
	for (irq = 0; irq < 32; irq++) {
		set_irq_chip(irq, &davinci_irq_chip_0);
		set_irq_handler(irq, handle_edge_irq);
		if (irq == IRQ_AEMIFINT)
			set_irq_chained_handler(irq,
				davinci_emif_demux_handler);
		set_irq_flags(irq, IRQF_VALID);
	}

	for (irq = 32; irq < IRQ_GPIO0; irq++) {
		set_irq_chip(irq, &davinci_irq_chip_1);
		set_irq_handler(irq, (irq == IRQ_TINT1_TINT34)?
				handle_level_irq : handle_edge_irq);
		set_irq_flags(irq, IRQF_VALID);
	}
	for (irq = IRQ_GPIO(0); irq <= IRQ_GPIO(7); irq++) {
		set_irq_chip(irq, &davinci_low_gpio_chip);
		set_irq_handler(irq, handle_edge_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}
	/* Install handler for GPIO>=8 edge detect interrupts */
	for (irq = IRQ_GPIOBNK0; irq <= IRQ_GPIOBNK4; irq++)  {
		set_irq_chip(irq, &davinci_irq_chip_1);
		set_irq_handler(irq, handle_edge_irq);
		set_irq_chained_handler(irq, davinci_gpio_demux_handler);
		set_irq_flags(irq, IRQF_VALID);
	}
	for (irq = IRQ_GPIOBNK4+1; irq < 64; irq++) {
		set_irq_chip(irq, &davinci_irq_chip_1);
		set_irq_handler(irq, handle_edge_irq);
		set_irq_flags(irq, IRQF_VALID);
	}
	for (irq = IRQ_EMIF_EMWAIT_TIMEOUT;
			irq <= IRQ_EMIF_EMWAIT_RISE; irq++) {
		set_irq_chip(irq, &davinci_muxed_emif_chip);
		set_irq_handler(irq, handle_edge_irq);
		set_irq_flags(irq, IRQF_VALID);
	}
	for (irq = IRQ_GPIO(8); irq <= IRQ_GPIO(DAVINCI_N_GPIO-1); irq++) {
		set_irq_chip(irq, &davinci_muxed_gpio_chip);
		set_irq_handler(irq, handle_edge_irq);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}
	/* enable interrupts for all banks */
	__raw_writel(0x1f, IO_ADDRESS(DAVINCI_GPIO_BASE + 0x08));
}
