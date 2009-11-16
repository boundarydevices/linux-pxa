/*
 * linux/drivers/mtd/nand/davinci_nand.c
 *
 * NAND Flash Driver
 *
 * Copyright (C) 2006 Texas Instruments.
 *
 * ported to 2.6.23 (C) 2008 by
 * Sander Huijsen <Shuijsen@optelecom-nkf.com>
 * Troy Kisky <troy.kisky@boundarydevices.com>
 * Dirk Behme <Dirk.Behme@gmail.com>
 *
 * --------------------------------------------------------------------------
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
 * --------------------------------------------------------------------------
 *
 *  Overview:
 *   This is a device driver for the NAND flash device found on the
 *   DaVinci board which utilizes the Samsung k9k2g08 part.
 *
 *  Modifications:
 *  ver. 1.0: Feb 2005, Vinod/Sudhakar
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/dma-mapping.h>

#include <mach/hardware.h>
#include <mach/nand.h>
#include <mach/mux.h>
#include <mach/edma.h>
#include <mach/gpio.h>

#define USE_DMA
#define USE_DMA_FOR_WRITE

/* FIXME: this should be passed in using platform_data */
#define DAVINCI_ASYNC_EMIF_CNTRL_BASE           (0x01E00000)


#define DRIVER_NAME "davinci_nand"

struct d_nand_chip {
	struct nand_chip c;
	unsigned int mem_start;
	unsigned long mem_size;
	struct completion cmd_complete;
	struct device *parent;
	unsigned char chip_num;
	unsigned char irq_enabled;
	signed char irq;
#ifdef USE_DMA
	/* dma variables */
	unsigned char start_dma_needed;
	unsigned char dma_active;
	dma_addr_t bounce_phys;
	unsigned char *bounce_base;
	unsigned char *bounce_oob;
	unsigned int *bounce_ecc;
	unsigned int *bounce_fcr;
	int bounce_size;
	int edma;
	int edma_oob_link;
	int edma_ecc;
	int edma_fcr;
	int edma_ecc_link;
	int edma_fcr_link;
	struct edmacc_param parm_edma;
	struct edmacc_param parm_ecc;
#ifdef USE_DMA_FOR_WRITE
	/* transfer part of oob before ecc, rounded down to multiple of 16 */
	int oob_bytes_before_ecc;
	int edma_oob_write;
	struct edmacc_param parm_edma_write;
#endif
	struct completion dma_complete;
#endif
	void __iomem *base;
};

struct nand_davinci_info {
	struct mtd_info mtd;
	struct d_nand_chip chip;
	struct clk *clk;
};
static inline unsigned int davinci_nand_readl(struct d_nand_chip *chip, int offset)
{
	return __raw_readl(chip->base + offset);
}

static inline void davinci_nand_writel(struct d_nand_chip *chip, unsigned long value, int offset)
{
	__raw_writel(value, chip->base + offset);
}

#define NAND_BUSY_FLAG           0x01

static inline int nand_is_ready_emwait(struct d_nand_chip *chip)
{
	return (davinci_nand_readl(chip, NANDFSR_OFFSET) & NAND_BUSY_FLAG);
}

static inline int nand_is_ready_gpirq(int irq)
{
	return gpio_get_value(IRQ_TO_GPIO(irq));
}

static inline int nand_is_ready(struct d_nand_chip *chip)
{
	int irq = chip->irq;
	if ((irq == IRQ_EMIF_EMWAIT_RISE) || (irq < 0))
		return nand_is_ready_emwait(chip);
	return nand_is_ready_gpirq(irq);
}


#ifdef CONFIG_MTD_PARTITIONS
const char *part_probes[] = { "cmdlinepart", NULL };
#endif

static uint8_t scan_ff_pattern[] = { 0xff, 0xff };

/* BB marker is byte 5 in OOB of page 0 */
static struct nand_bbt_descr davinci_memorybased_small = {
	.options = NAND_BBT_SCAN2NDPAGE,
	.offs = 5,
	.len = 1,
	.pattern = scan_ff_pattern
};

/* BB marker is bytes 0-1 in OOB of page 0 */
static struct nand_bbt_descr davinci_memorybased_large = {
	.options = 0,
	.offs = 0,
	.len = 2,
	.pattern = scan_ff_pattern
};

/*
 * Hardware specific access to control-lines
 */
static void nand_davinci_hwcontrol(struct mtd_info *mtd, int cmd,
				   unsigned int ctrl)
{
	struct d_nand_chip *chip = mtd->priv;
	u32 IO_ADDR_W = (u32)chip->c.IO_ADDR_W;

	/* Did the control lines change? */
	if (ctrl & NAND_CTRL_CHANGE) {
		IO_ADDR_W &= ~(MASK_ALE|MASK_CLE);

		if ((ctrl & NAND_CTRL_CLE) == NAND_CTRL_CLE)
			IO_ADDR_W |= MASK_CLE;
		else if ((ctrl & NAND_CTRL_ALE) == NAND_CTRL_ALE)
			IO_ADDR_W |= MASK_ALE;

		chip->c.IO_ADDR_W = (void __iomem *)IO_ADDR_W;
	}

	if (cmd != NAND_CMD_NONE)
		writeb(cmd, chip->c.IO_ADDR_W);
}

static void nand_davinci_select_chip(struct mtd_info *mtd, int chip)
{
	/* do nothing */
}

#define read_fast(pDst, pRegs, cnt) asm volatile ( \
	"	cmp	%2,#16\n" \
	"1:	ldrhs	r0,[%1]\n" \
	"	ldrhs	r1,[%1]\n" \
	"	ldrhs	r2,[%1]\n" \
	"	ldrhs	r3,[%1]\n" \
	"	stmhsia	%0!,{r0,r1,r2,r3}\n" \
	"	beq	3f\n" \
	"	subhs	%2,%2,#16\n" \
	"	cmp	%2,#16\n" \
	"	bhs	1b\n" \
	"	tst	%2,#0x0c\n" \
	"2:	ldrne	r0,[%1]\n" \
	"	strne	r0,[%0],#4\n" \
	"	subne	%2,%2,#4\n" \
	"	tst	%2,#0x0c\n" \
	"	bne	2b\n" \
	"	tst	%2,#2\n" \
	"	ldrneh	r0,[%1]\n" \
	"	strneh	r0,[%0],#2\n" \
	"	tst	%2,#1\n" \
	"	ldrneb	r0,[%1]\n" \
	"	strneb	r0,[%0],#1\n" \
	"3:\n" \
	 : "+r"(pDst) : "r"(pRegs), "r"(cnt) \
	 : "r0", "r1", "r2", "r3")

#define write_fast(pSrc, pRegs, cnt) asm volatile ( \
	"	cmp	%2,#16\n" \
	"1:	ldmhsia	%0!,{r0,r1,r2,r3}\n" \
	"	strhs	r0,[%1]\n" \
	"	strhs	r1,[%1]\n" \
	"	strhs	r2,[%1]\n" \
	"	strhs	r3,[%1]\n" \
	"	beq	3f\n" \
	"	subhs	%2,%2,#16\n" \
	"	cmp	%2,#16\n" \
	"	bhs	1b\n" \
	"	tst	%2,#0x0c\n" \
	"2:	ldrne	r0,[%0],#4\n" \
	"	strne	r0,[%1]\n" \
	"	subne	%2,%2,#4\n" \
	"	tst	%2,#0x0c\n" \
	"	bne	2b\n" \
	"	tst	%2,#2\n" \
	"	ldrneh	r0,[%0],#2\n" \
	"	strneh	r0,[%1]\n" \
	"	tst	%2,#1\n" \
	"	ldrneb	r0,[%0],#1\n" \
	"	strneb	r0,[%1]\n" \
	"3:\n" \
	 : "+r"(pSrc) : "r"(pRegs), "r"(cnt) \
	 : "r0", "r1", "r2", "r3")

/*
 * Read from memory register: we can read 4 bytes at a time.
 * The hardware takes care of actually reading the NAND flash.
 */
static void nand_davinci_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct nand_chip *chip = mtd->priv;
	read_fast(buf, chip->IO_ADDR_R, len);
}

static void nand_davinci_write_buf(struct mtd_info *mtd, const uint8_t *buf,
	int len)
{
	struct nand_chip *chip = mtd->priv;
	write_fast(buf, chip->IO_ADDR_W, len);
}


static void nand_davinci_enable_hwecc(struct mtd_info *mtd, int mode)
{
	struct d_nand_chip *chip = mtd->priv;
	u32 retval;
	/* 0 - cs2, 1 - cs3, 2 - cs4, 3 - cs5 */
	u_int32_t   chip_num = chip->chip_num;

	/* Reset ECC hardware */
	davinci_nand_readl(chip, NANDF1ECC_OFFSET + (chip_num<<2));

	/* Restart ECC hardware */
	retval = davinci_nand_readl(chip, NANDFCR_OFFSET);
	retval |= 0x100<<chip_num;
#ifdef USE_DMA
	if (chip->bounce_fcr)
		*(chip->bounce_fcr) = retval;
#endif
	davinci_nand_writel(chip, retval, NANDFCR_OFFSET);
}

/*
 * Read DaVinci ECC register
 */
static u32 nand_davinci_readecc(struct mtd_info *mtd)
{
	struct d_nand_chip *chip = mtd->priv;
	/* Read register ECC and clear it */
	return davinci_nand_readl(chip, NANDF1ECC_OFFSET + (chip->chip_num<<2));
}

/*
 * Read DaVinci ECC registers and rework into MTD format
 */
static int nand_davinci_calculate_ecc(struct mtd_info *mtd,
				      const u_char *dat, u_char *ecc_code)
{
	unsigned int ecc_val = nand_davinci_readecc(mtd);
	/* squeeze 0 middle bits out so that it fits in 3 bytes */
	unsigned int tmp = (ecc_val&0x0fff)|((ecc_val&0x0fff0000)>>4);
	/* invert so that erased block ecc is correct */
	tmp = ~tmp;
	ecc_code[0] = (u_char)(tmp);
	ecc_code[1] = (u_char)(tmp >> 8);
	ecc_code[2] = (u_char)(tmp >> 16);

	return 0;
}

static int nand_davinci_correct_data(struct mtd_info *mtd, u_char *dat,
				     u_char *read_ecc, u_char *calc_ecc)
{
	struct d_nand_chip *chip = mtd->priv;
	u_int32_t ecc_nand = read_ecc[0] | (read_ecc[1] << 8) |
					  (read_ecc[2] << 16);
	u_int32_t ecc_calc = calc_ecc[0] | (calc_ecc[1] << 8) |
					  (calc_ecc[2] << 16);
	u_int32_t diff = ecc_calc ^ ecc_nand;
	if (!diff)
		return 0;

	if ((((diff>>12)^diff) & 0xfff) == 0xfff) {
		/* Correctable error */
		if ((diff>>(12+3)) < chip->c.ecc.size) {
			dat[diff>>(12+3)] ^= (1 << ((diff>>12)&7));
			return 1;
		}
	} else if (!(diff & (diff-1))) {
		/* Single bit ECC error in the ECC itself,
		   nothing to fix */
		return 1;
	}
	/* Uncorrectable error */
	dev_warn(chip->parent, "uncorrectable ecc error, "
		"read=%x, calc=%x, diff=%x\n", ecc_nand, ecc_calc, diff);
	return -1;

}

#ifdef USE_DMA
static inline int calculate_ecc_dma(struct d_nand_chip *chip,
		u_char *ecc_code)
{
	int i = chip->c.ecc.steps;
	unsigned int *p = chip->bounce_ecc;
	while (i) {
		unsigned int ecc_val = *p++;
		/* squeeze 0 middle bits out so that it fits in 3 bytes */
		unsigned int tmp = (ecc_val&0x0fff)|((ecc_val&0x0fff0000)>>4);
		/* invert so that erased block ecc is correct */
		tmp = ~tmp;
		*ecc_code++ = (u_char)(tmp);
		*ecc_code++ = (u_char)(tmp >> 8);
		*ecc_code++ = (u_char)(tmp >> 16);
		i--;
	}
	return 0;
}
static inline int correct_data_dma(struct mtd_info *mtd,
		struct d_nand_chip *chip,
		u_char *dat, u_char *read_ecc, u_char *calc_ecc)
{
	int i = chip->c.ecc.steps;
	int cnt = 0;
	int failed = 0;
	int err_mask = 0;
	int mask = 1;
	while (i) {
		int ret;
		ret = nand_davinci_correct_data(mtd, dat, read_ecc, calc_ecc);
		if (ret < 0) {
			err_mask |= mask;
			failed++;
		} else
			cnt += ret;
		mask <<= 1;
		dat += chip->c.ecc.size;
		read_ecc += 3;
		calc_ecc += 3;
		i--;
	}
	if (failed)
		dev_warn(chip->parent, "segment error mask=0x%x, "
				"corrected=%i\n", err_mask, cnt);
	mtd->ecc_stats.failed += failed;
	mtd->ecc_stats.corrected += cnt;
	return (failed)? -1 : cnt;
}

static inline void read_buf_dma(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct d_nand_chip *chip = mtd->priv;
	int edma = chip->edma;
	int bounce = 0;
	dma_addr_t phys;
	/* startup dma */
	INIT_COMPLETION(chip->dma_complete);
	phys = dma_map_single(chip->parent, buf, len, DMA_FROM_DEVICE);
	if (dma_mapping_error(chip->parent, phys)) {
		phys = chip->bounce_phys;
		bounce = 1;
		dev_warn(chip->parent, "bouncing\n");
	}
	chip->parm_edma.dst = phys;
	edma_write_slot(edma, &chip->parm_edma);

	if (nand_is_ready(chip)) {
		chip->dma_active = 1;
		edma_start(edma);
	} else if (chip->irq < 0) {
		nand_wait_ready(mtd);
		chip->dma_active = 1;
		edma_start(edma);
	} else {
		chip->start_dma_needed = 1;
		if (!chip->irq_enabled) {
			enable_irq(chip->irq);
			chip->irq_enabled = 1;
		}
		if (nand_is_ready(chip)) {
			rmb();
			if (chip->start_dma_needed) {
				chip->start_dma_needed = 0;
				chip->dma_active = 1;
				edma_start(edma);
			}
		}
	}
	wait_for_completion(&chip->dma_complete);
	if (bounce)
		memcpy(buf, chip->bounce_base, len);
	else
		dma_unmap_single(chip->parent, phys, len, DMA_FROM_DEVICE);
}
#if 0
void dump(uint8_t *buf, int len)
{
	int i = 0;
	unsigned int *p = (unsigned int *)buf;
	while (len >= 16) {
		printk(KERN_ERR "%08x %08x %08x %08x  %04x\n",
				p[3], p[2], p[1], p[0], i);
		len -= 16;
		i += 16;
		p += 4;
	}
}
#endif
static int read_page_dma(struct mtd_info *mtd, struct nand_chip *_chip,
	uint8_t *buf, int page)
{
	struct d_nand_chip *chip = (struct d_nand_chip *)_chip;
	int i;
	uint8_t *ecc_calc = chip->c.buffers->ecccalc;
	uint8_t *ecc_code = chip->c.buffers->ecccode;
	uint32_t *eccpos = chip->c.ecc.layout.eccpos;

	nand_davinci_enable_hwecc(mtd, NAND_ECC_READ);
	read_buf_dma(mtd, buf, mtd->writesize);
	calculate_ecc_dma(chip, ecc_calc);

	i = chip->c.ecc.total;
	while (i--)
		*ecc_code++ = chip->bounce_oob[*eccpos++];

	correct_data_dma(mtd, chip, buf, chip->c.buffers->ecccode, ecc_calc);
#if 0
	{
		struct edmacc_param parm;
		edma_read_slot(chip->edma_ecc, &parm);
		printk(KERN_ERR "%x %x, %x %x, %x %x, %x %x\n",
			chip->parm_ecc.dst, parm.dst,
			chip->parm_ecc.src_dst_cidx, parm.src_dst_cidx,
			chip->parm_ecc.a_b_cnt, parm.a_b_cnt,
			chip->parm_ecc.ccnt, parm.ccnt);
		dump(buf, mtd->writesize);
		dump(chip->bounce_oob, mtd->oobsize);
	}
#endif
	return 0;
}
#ifdef USE_DMA_FOR_WRITE
int get_oob_bytes_before_ecc(struct d_nand_chip *chip)
{
	uint32_t *eccpos = chip->c.ecc.layout.eccpos;
	int oob_before = 63;
	int i;
	for (i = 0; i < chip->c.ecc.total; i++)
		if (oob_before > eccpos[i])
			oob_before = eccpos[i];
	return oob_before & ~0xf;	/* keep multiple of 16 */
}

static inline void write_buf_dma(struct mtd_info *mtd, const uint8_t *buf,
		int len)
{
	struct d_nand_chip *chip = mtd->priv;
	int edma = chip->edma;
	int bounce = 0;
	dma_addr_t phys;
	/* startup dma */
	INIT_COMPLETION(chip->dma_complete);
	phys = dma_map_single(chip->parent, (uint8_t *)buf, len, DMA_TO_DEVICE);
	if (dma_mapping_error(chip->parent, phys)) {
		memcpy(chip->bounce_base, buf, len);
		phys = chip->bounce_phys;
		bounce = 1;
		dev_warn(chip->parent, "bouncing write\n");
	}
	chip->parm_edma_write.src = phys;
	edma_write_slot(edma, &chip->parm_edma_write);

	chip->dma_active = 1;
	edma_start(edma);
	wait_for_completion(&chip->dma_complete);
	if (bounce == 0)
		dma_unmap_single(chip->parent, phys, len, DMA_TO_DEVICE);
}
/**
 * write_page_dma - hardware ecc based page write function
 */
static void write_page_dma(struct mtd_info *mtd, struct nand_chip *_chip,
	const uint8_t *buf)
{
	struct d_nand_chip *chip = (struct d_nand_chip *)_chip;
	int i;
	int before_ecc = chip->oob_bytes_before_ecc;
	uint8_t *ecc_calc = chip->c.buffers->ecccalc;
	uint32_t *eccpos = chip->c.ecc.layout.eccpos;

	nand_davinci_enable_hwecc(mtd, NAND_ECC_WRITE);
	write_buf_dma(mtd, buf, mtd->writesize);
	calculate_ecc_dma(chip, ecc_calc);

	for (i = 0; i < chip->c.ecc.total; i++)
		chip->c.oob_poi[eccpos[i]] = ecc_calc[i];

	memcpy(chip->bounce_oob, chip->c.oob_poi, before_ecc);
	nand_davinci_write_buf(mtd, chip->c.oob_poi + before_ecc,
			mtd->oobsize - before_ecc);
}
#endif

static void dma_cb(unsigned lch, u16 ch_status, void *data)
{
	struct d_nand_chip *chip = (struct d_nand_chip *)data;
	if (DMA_COMPLETE != ch_status) {
		dev_err(chip->parent, "[DMA FAILED]\n");
	} else if (chip->dma_active) {
		chip->dma_active = 0;
		complete(&chip->dma_complete);
		return;
	} else {
		dev_err(chip->parent, "unexpected dma callback\n");
	}
	edma_stop(chip->edma);
	edma_clean_channel(chip->edma);
	edma_stop(chip->edma_ecc);
	edma_clean_channel(chip->edma_ecc);
	edma_write_slot(chip->edma_ecc, &chip->parm_ecc);
	chip->dma_active = 0;
	complete(&chip->dma_complete);
}

static int get_dma_channels(struct mtd_info *mtd, struct d_nand_chip *chip)
{
	int edma;
	int edma_oob_link;
	int edma_ecc;
	int edma_fcr;
	int edma_ecc_link;
	int edma_fcr_link;
	int r;
	struct edmacc_param parm;
	enum dma_event_q queue_no = EVENTQ_1;
	int eccsize = chip->c.ecc.size;

#ifdef USE_DMA_FOR_WRITE
	int edma_oob_write;
	int before_ecc = get_oob_bytes_before_ecc(chip);
	chip->oob_bytes_before_ecc = before_ecc;
#endif

	chip->bounce_size = mtd->writesize + mtd->oobsize +
		((chip->c.ecc.steps+2)<<2);
	chip->bounce_base = (unsigned char *)dma_alloc_coherent(chip->parent,
		chip->bounce_size, &chip->bounce_phys, GFP_KERNEL | GFP_DMA);
	if (!chip->bounce_base) {
		dev_err(chip->parent, "%s : dma_alloc_coherent fail.\n",
			__FUNCTION__);
		return -ENOMEM;
	}
	printk(KERN_INFO "**** dma_alloc v=0x%08x p=0x%08x size=0x%x\n",
		(u32)chip->bounce_base, chip->bounce_phys, chip->bounce_size);


	/* Acquire master DMA channel */
	r = edma = edma_alloc_channel(EDMA_CHANNEL_ANY, dma_cb, chip, queue_no);
	if (r < 0)
		goto out1;

	r = edma_oob_link = edma_alloc_slot(EDMA_CTLR(edma), EDMA_SLOT_ANY);
	if (r < 0)
		goto out2;

	r = edma_ecc = edma_alloc_channel(EDMA_CHANNEL_ANY, NULL, NULL, queue_no);
	if (r < 0)
		goto out3;

	r = edma_fcr = edma_alloc_channel(EDMA_CHANNEL_ANY, NULL, NULL, queue_no);
	if (r < 0)
		goto out4;

	r = edma_ecc_link = edma_alloc_slot(EDMA_CTLR(edma_ecc), EDMA_SLOT_ANY);
	if (r < 0)
		goto out5;

	r = edma_fcr_link = edma_alloc_slot(EDMA_CTLR(edma_fcr), EDMA_SLOT_ANY);
	if (r < 0)
		goto out6;

#ifdef USE_DMA_FOR_WRITE
	r = edma_oob_write = edma_alloc_slot(EDMA_CTLR(edma), EDMA_SLOT_ANY);
	if (r < 0)
		goto out7;
	chip->edma_oob_write = edma_oob_write;
#endif

	chip->edma = edma;
	chip->edma_oob_link = edma_oob_link;
	chip->edma_ecc = edma_ecc;
	chip->edma_fcr = edma_fcr;
	chip->edma_ecc_link = edma_ecc_link;
	chip->edma_fcr_link = edma_fcr_link;
	chip->bounce_oob = chip->bounce_base + mtd->writesize;
	chip->bounce_ecc = (unsigned int *)(chip->bounce_oob + mtd->oobsize);
	chip->bounce_fcr = chip->bounce_ecc + chip->c.ecc.steps;
/* Intialize main page transfer,
 * EMIF does not support constant addressing mode (don't use FIFO)
 */
	edma_set_src(edma, chip->mem_start, INCR, W32BIT);
	edma_set_src_index(edma, 0, 0);
	edma_set_dest(edma, chip->bounce_phys, INCR, W32BIT);
	edma_set_dest_index(edma, 8, eccsize);
	edma_set_transfer_params(edma, 8, eccsize>>3,
		chip->c.ecc.steps, eccsize>>3, ABSYNC);
	edma_link(edma, edma_oob_link);

	edma_read_slot(edma, &chip->parm_edma);
	chip->parm_edma.opt &= ~(TCCMODE | TCINTEN | ITCINTEN | EDMA_TCC(0x3f));
	chip->parm_edma.opt |= (TCCHEN | ITCCHEN) | ((edma_ecc & 0x3f)<<12);
	edma_write_slot(edma, &chip->parm_edma);

/* Intialize oob transfer */
	edma_set_src(edma_oob_link, chip->mem_start, INCR, W32BIT);
	edma_set_src_index(edma_oob_link, 0, 0);
	edma_set_dest(edma_oob_link, chip->bounce_phys + mtd->writesize, INCR,
			W32BIT);
	edma_set_dest_index(edma_oob_link, 8, 0);
	edma_set_transfer_params(edma_oob_link, 8, mtd->oobsize>>3,
		1, mtd->oobsize>>3, ABSYNC);
	edma_read_slot(edma_oob_link, &parm);
	parm.opt &= ~(TCCMODE | EDMA_TCC(0x3f));
	parm.opt |= TCINTEN | (edma<<12);
	edma_write_slot(edma_oob_link, &parm);

/* Intialize ecc transfer */
	edma_set_src(edma_ecc, DAVINCI_ASYNC_EMIF_CNTRL_BASE +
			NANDF1ECC_OFFSET + (chip->chip_num<<2), INCR, W32BIT);
	edma_set_src_index(edma_ecc, 0, 0);
	edma_set_dest(edma_ecc, chip->bounce_phys +
		mtd->writesize + mtd->oobsize, INCR, W32BIT);
	edma_set_dest_index(edma_ecc, 4, -4 * (chip->c.ecc.steps - 1));
	edma_set_transfer_params(edma_ecc, 4, chip->c.ecc.steps, 0xffff,
			chip->c.ecc.steps, ASYNC);
	edma_link(edma_ecc, edma_ecc_link);	/* link to self */
	edma_read_slot(edma_ecc, &chip->parm_ecc);
	chip->parm_ecc.opt &= ~(TCCMODE | TCINTEN | ITCINTEN | EDMA_TCC(0x3f));
	chip->parm_ecc.opt |= (TCCHEN | ITCCHEN) | ((edma_fcr & 0x3f)<<12);
	edma_write_slot(edma_ecc, &chip->parm_ecc);
	edma_write_slot(edma_ecc_link, &chip->parm_ecc);

/* Intialize fcr transfer */
	edma_set_src(edma_fcr, chip->bounce_phys +
			chip->bounce_size - 8, INCR, W32BIT);
	edma_set_src_index(edma_fcr, 0, 0);
	edma_set_dest(edma_fcr, DAVINCI_ASYNC_EMIF_CNTRL_BASE +
			NANDFCR_OFFSET, INCR, W32BIT);
	edma_set_dest_index(edma_fcr, 0, 0);
	edma_set_transfer_params(edma_fcr, 4, 0xffff, 0xffff, 0xffff,
			ASYNC);
	edma_link(edma_fcr, edma_fcr_link);	/* link to self */
	edma_read_slot(edma_fcr, &parm);
	parm.opt &= ~(TCCMODE | TCINTEN | ITCINTEN | EDMA_TCC(0x3f));
	parm.opt |= (TCCHEN | ITCCHEN) | ((edma & 0x3f)<<12);
	edma_write_slot(edma_fcr, &parm);
	edma_write_slot(edma_fcr_link, &parm);

#ifdef USE_DMA_FOR_WRITE
/* Intialize main page write transfer,
 * use same opt as read
 */
	chip->parm_edma_write.opt	= chip->parm_edma.opt;
	chip->parm_edma_write.src	= chip->bounce_phys;
	chip->parm_edma_write.a_b_cnt	= ((eccsize>>3)<<16) | 8;
	chip->parm_edma_write.dst	= chip->mem_start;
	chip->parm_edma_write.src_dst_bidx = (0<<16) | 8;
	chip->parm_edma_write.link_bcntrld = ((eccsize>>3)<<16);
	chip->parm_edma_write.src_dst_cidx = (0<<16) | eccsize;
	chip->parm_edma_write.ccnt	= chip->c.ecc.steps;

	edma_write_slot(edma, &chip->parm_edma_write);
	edma_link(edma, edma_oob_write);
	edma_read_slot(edma, &chip->parm_edma_write);

/* Intialize oob write transfer */
	edma_set_src(edma_oob_write,
			chip->bounce_phys + mtd->writesize, INCR, W32BIT);
	edma_set_src_index(edma_oob_write, 8, 0);
	edma_set_dest(edma_oob_write,
			chip->mem_start, INCR, W32BIT);
	edma_set_dest_index(edma_oob_write, 0, 0);
	edma_set_transfer_params(edma_oob_write, 8, before_ecc>>3,
		1, (mtd->oobsize - before_ecc)>>3, ABSYNC);
	edma_read_slot(edma_oob_write, &parm);
	parm.opt &= ~(TCCMODE | EDMA_TCC(0x3f));
	parm.opt |= TCINTEN | (edma<<12);
	edma_write_slot(edma_oob_write, &parm);
#endif
	return 0;
#ifdef USE_DMA_FOR_WRITE
out7:
	edma_free_slot(edma_fcr_link);
#endif
out6:
	edma_free_slot(edma_ecc_link);
out5:
	edma_free_channel(edma_fcr);
out4:
	edma_free_channel(edma_ecc);
out3:
	edma_free_slot(edma_oob_link);
out2:
	edma_free_channel(edma);
out1:
	dev_err(chip->parent, "davinci_request_dma() failed with %d\n", r);

	if (chip->bounce_base)
		dma_free_coherent(NULL, chip->bounce_size,
				(void *)chip->bounce_base, chip->bounce_phys);
	chip->bounce_base = NULL;
	return r;
}
#endif


/*
 * Read OOB data from flash.
 */
static int read_oob_and_check(struct mtd_info *mtd, loff_t offs, uint8_t *buf,
			      struct nand_bbt_descr *bd)
{
	int i, ret;
	int page;
	struct nand_chip *chip = mtd->priv;

	/* Calculate page address from offset */
	page = (int)(offs >> chip->page_shift);
	page &= chip->pagemask;

	/* Read OOB data from flash */
	ret = chip->ecc.read_oob(mtd, chip, page, 1);
	if (ret < 0)
		return ret;

	/* Copy read OOB data to the buffer*/
	memcpy(buf, chip->oob_poi, mtd->oobsize);

	/* Check pattern against BBM in OOB area */
	for (i = 0; i < bd->len; i++) {
		if (buf[bd->offs + i] != bd->pattern[i])
			return 1;
	}
	return 0;
}

/*
 * Fill in the memory based Bad Block Table (BBT).
 */
static int nand_davinci_memory_bbt(struct mtd_info *mtd,
				   struct nand_bbt_descr *bd)
{
	int i, numblocks;
	int startblock = 0;
	loff_t from = 0;
	struct nand_chip *chip = mtd->priv;
	int blocksize = 1 << chip->bbt_erase_shift;
	uint8_t *buf = chip->buffers->databuf;
	int len = bd->options & NAND_BBT_SCAN2NDPAGE ? 2 : 1;

	/* -numblocks- is 2 times the actual number of eraseblocks */
	numblocks = mtd->size >> (chip->bbt_erase_shift - 1);

	/* Now loop through all eraseblocks in the flash */
	for (i = startblock; i < numblocks; i += 2) {
		int j, ret;
		int offs = from;

		/* If NAND_BBT_SCAN2NDPAGE flag is set in bd->options,
		 * also each 2nd page of an eraseblock is checked
		 * for a Bad Block Marker. In that case, len equals 2.
		 */
		for (j = 0; j < len; j++) {
			/* Read OOB data and check pattern */
			ret = read_oob_and_check(mtd, from, buf, bd);
			if (ret < 0)
				return ret;

			/* Check pattern for bad block markers */
			if (ret) {
				/* Mark bad block by writing 0b11 in the
				   table */
				chip->bbt[i >> 3] |= 0x03 << (i & 0x6);

				printk(KERN_WARNING "Bad eraseblock %d at " \
						    "0x%08x\n", i >> 1,
						     (unsigned int)from);

				mtd->ecc_stats.badblocks++;
				break;
			}
			offs += mtd->writesize;
		}

		/* Make -from- point to next eraseblock */
		from += blocksize;
	}

	printk(KERN_NOTICE "Bad block scan: %d out of %d blocks are bad.\n",
			    mtd->ecc_stats.badblocks, numblocks>>1);

	return 0;
}

/*
 * This function creates a memory based bad block table (BBT).
 * It is largely based on the standard BBT function, but all
 * unnecessary junk is thrown out to speed up.
 */
static int nand_davinci_scan_bbt(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct nand_bbt_descr *bd;
	int len, ret = 0;

	chip->bbt_td = NULL;
	chip->bbt_md = NULL;

	/* pagesize determines location of BBM */
	if (mtd->writesize > 512)
		bd = &davinci_memorybased_large;
	else
		bd = &davinci_memorybased_small;

	chip->badblock_pattern = bd;

	/* Use 2 bits per page meaning 4 page markers per byte */
	len = mtd->size >> (chip->bbt_erase_shift + 2);

	/* Allocate memory (2bit per block) and clear the memory bad block
	   table */
	chip->bbt = kzalloc(len, GFP_KERNEL);
	if (!chip->bbt) {
		printk(KERN_ERR "nand_davinci_scan_bbt: Out of memory\n");
		return -ENOMEM;
	}

	/* Now try to fill in the BBT */
	ret = nand_davinci_memory_bbt(mtd, bd);
	if (ret) {
		printk(KERN_ERR "nand_davinci_scan_bbt: "
		       "Can't scan flash and build the RAM-based BBT\n");

		kfree(chip->bbt);
		chip->bbt = NULL;
	}

	return ret;
}

static irqreturn_t nand_isr_emwait(int irq, void *mtd_id)
{
	struct mtd_info *mtd = (struct mtd_info *)mtd_id;
	struct d_nand_chip *chip = mtd->priv;
	if (nand_is_ready_emwait(chip)) {
#ifdef USE_DMA
		if (chip->start_dma_needed) {
			chip->start_dma_needed = 0;
			chip->dma_active = 1;
			edma_start(chip->edma);
		}
#endif
		complete(&chip->cmd_complete);
	}
	return IRQ_HANDLED;
}

static irqreturn_t nand_isr_gpirq(int irq, void *mtd_id)
{
	struct mtd_info *mtd = (struct mtd_info *)mtd_id;
	struct d_nand_chip *chip = mtd->priv;
	if (nand_is_ready_gpirq(chip->irq)) {
#ifdef USE_DMA
		if (chip->start_dma_needed) {
			chip->start_dma_needed = 0;
			chip->dma_active = 1;
			edma_start(chip->edma);
		}
#endif
		complete(&chip->cmd_complete);
	}
	return IRQ_HANDLED;
}

/*
 * nand_davinci_waitfunc_with_irq
 * Wait for command done. (erase/program only)
 * allow 400ms for erase
 * allow  20ms for program
 */
static int waitfunc_emwaitirq(struct mtd_info *mtd, struct nand_chip *_chip)
{
	struct d_nand_chip *chip = (struct d_nand_chip *)_chip;
	unsigned long timeout = (chip->c.state == FL_ERASING)?
			((HZ * 400) / 1000) : ((HZ * 20) / 1000);
	__raw_writeb(NAND_CMD_STATUS, (chip->c.IO_ADDR_R+MASK_CLE));
	INIT_COMPLETION(chip->cmd_complete);
	if (!chip->irq_enabled) {
		enable_irq(chip->irq);
		chip->irq_enabled = 1;
	}
	if (!nand_is_ready_emwait(chip)) {
		if (wait_for_completion_timeout(&chip->cmd_complete,
				timeout) == 0)
			printk(KERN_ERR "waitfunc timeout\n");
	}
	return __raw_readb(chip->c.IO_ADDR_R);
}
static int waitfunc_gpirq(struct mtd_info *mtd, struct nand_chip *_chip)
{
	struct d_nand_chip *chip = (struct d_nand_chip *)_chip;
	unsigned long timeout = (chip->c.state == FL_ERASING)?
			((HZ * 400) / 1000) : ((HZ * 20) / 1000);
	__raw_writeb(NAND_CMD_STATUS, (chip->c.IO_ADDR_R+MASK_CLE));
	INIT_COMPLETION(chip->cmd_complete);
	if (!chip->irq_enabled) {
		enable_irq(chip->irq);
		chip->irq_enabled = 1;
	}
	if (!nand_is_ready_gpirq(chip->irq)) {
		if (wait_for_completion_timeout(&chip->cmd_complete,
				timeout) == 0)
			printk(KERN_ERR "waitfunc timeout\n");
	}
	return __raw_readb(chip->c.IO_ADDR_R);
}

/*
 * Check hardware register for wait status. Returns 1 if device is ready,
 * 0 if it is still busy.
 */
static int dev_ready_emwait(struct mtd_info *mtd)
{
	struct d_nand_chip *chip = (struct d_nand_chip *)(mtd->priv);
	return nand_is_ready_emwait(chip);
}
#if 0
static int dev_ready_emwaitirq(struct mtd_info *mtd)
{
	struct d_nand_chip *chip = (struct d_nand_chip *)(mtd->priv);
	return nand_is_ready_emwait(chip);
}
static int dev_ready_gpirq(struct mtd_info *mtd)
{
	struct d_nand_chip *chip = (struct d_nand_chip *)(mtd->priv);
	return nand_is_ready_gpirq(chip->irq);
}
#else
static int dev_ready_emwaitirq(struct mtd_info *mtd)
{
	struct d_nand_chip *chip = (struct d_nand_chip *)(mtd->priv);
	if (chip->irq_enabled) {
		disable_irq(chip->irq);
		chip->irq_enabled = 0;
	}
	return nand_is_ready_emwait(chip);
}
static int dev_ready_gpirq(struct mtd_info *mtd)
{
	struct d_nand_chip *chip = (struct d_nand_chip *)(mtd->priv);
	if (chip->irq_enabled) {
		disable_irq(chip->irq);
		chip->irq_enabled = 0;
	}
	return nand_is_ready_gpirq(chip->irq);
}
#endif


/* chip_num: 0 - cs2, 1 - cs3, 2 - cs4, 3 - cs5 */
static void __devinit nand_davinci_flash_init(struct d_nand_chip *chip,
		unsigned int chip_num, int width16, int platform_timings)
{
	u32 regval, tmp;

	/* We don't care what the polarity of EMWAIT is because
	 * we don't use extended wait
	 * Let a driver that cares control it.
	regval = davinci_nand_readl(chip, AWCCR_OFFSET);
	regval |= 0x10000000;
	davinci_nand_writel(chip, regval, AWCCR_OFFSET);
	 */

	/*------------------------------------------------------------------*
	 *  NAND FLASH CHIP TIMEOUT @ 459 MHz                               *
	 *                                                                  *
	 *  AEMIF.CLK freq   = PLL1/6 = 459/6 = 76.5 MHz                    *
	 *  AEMIF.CLK period = 1/76.5 MHz = 13.1 ns                         *
	 *                                                                  *
	 *------------------------------------------------------------------*/
	platform_timings = (platform_timings & ~1) | width16;
	tmp = davinci_nand_readl(chip, A1CR_OFFSET);
	if (tmp != platform_timings) {
		if (chip_num == 0) {
			printk(KERN_WARNING "Warning: NAND config: Set A1CR " \
			    "reg to 0x%08x, was 0x%08x, should be done by " \
			    "bootloader.\n", platform_timings, tmp);
		}
		davinci_nand_writel(chip, platform_timings,
				A1CR_OFFSET + (chip_num<<2));
	}

	regval = davinci_nand_readl(chip, NANDFCR_OFFSET);
	regval |= 1<<chip_num;
	davinci_nand_writel(chip, regval, NANDFCR_OFFSET);
}

/*
 * Main initialization routine
 */
int __devinit nand_davinci_probe(struct platform_device *pdev)
{
	struct davinci_flash_platform_data *pdata = pdev->dev.platform_data;
	struct resource		  *res = pdev->resource;
	struct d_nand_chip     	  *chip;
	struct device        	  *dev = NULL;
	u32                  	  nand_rev_code;
	struct nand_davinci_info *info;
#ifdef CONFIG_MTD_CMDLINE_PARTS
	const char             	  *master_name;
	int 		     	  mtd_parts_nb = 0;
	struct mtd_partition 	  *mtd_parts = 0;
#endif

	unsigned int chip_num;
	int width16;
	int irq = -1;
	unsigned int mem_start = 0;
	unsigned long mem_size = 0;
	int i;
	int err;
	nand_ecc_modes_t                ecc_mode;

	/* 0 - cs2, 1 - cs3, 2 - cs4, 3 - cs5 */
	chip_num = pdata->chip_num;
	if (chip_num > 3)
		return -ENXIO;

	for (i = 0; i < pdev->num_resources; i++) {
		if (res->flags == IORESOURCE_MEM) {
			mem_start = res->start;
			mem_size = res->end - mem_start + 1;
		} else if (res->flags == IORESOURCE_IRQ) {
			irq = res->start;
		}
		res++;
	}

	/* Allocate memory for MTD device structure and private data */
	info = kzalloc(sizeof(struct nand_davinci_info), GFP_KERNEL);
	if (!info) {
		printk(KERN_ERR "Unable to allocate davinci NAND MTD device " \
		       "structure.\n");
		return -ENOMEM;
	}

	/* Get pointer to private data */
	chip = &info->chip;
	/* Link the private data with the MTD structure */
	info->mtd.priv = chip;
	if (!request_mem_region(mem_start, mem_size, "nand")) {
		printk(KERN_ERR "request_mem_region failed 0x%x-0x%lx\n",
			mem_start, mem_start+mem_size-1);
		err = -EBUSY;
		goto out_free_info;
	}
	chip->c.IO_ADDR_R = ioremap(mem_start, mem_size);
	if (chip->c.IO_ADDR_R == NULL) {
		printk(KERN_ERR "DaVinci NAND: ioremap failed.\n");
		err = -ENOMEM;
		goto out_release_mem_region;
	}
	init_completion(&chip->cmd_complete);
	chip->mem_start = mem_start;
	chip->mem_size = mem_size;
	chip->chip_num = chip_num;
	chip->irq = -1;
	chip->c.options = pdata->options;
	chip->parent = &pdev->dev;

	info->clk = clk_get(dev, "aemif");
	if (IS_ERR(info->clk)) {
		printk(KERN_ERR "Error %ld getting aemif clock?\n",
		       PTR_ERR(info->clk));
		err = -ENXIO;
		goto out_iounmap;
	}

	clk_enable(info->clk);

	
	chip->base = ioremap(DAVINCI_ASYNC_EMIF_CNTRL_BASE, 4096);
	nand_rev_code = davinci_nand_readl(chip, NRCSR_OFFSET);

	printk("DaVinci NAND Controller rev. %d.%d\n",
	       (nand_rev_code >> 8) & 0xff, nand_rev_code & 0xff);


	chip->c.IO_ADDR_W   = chip->c.IO_ADDR_R;
	chip->c.chip_delay  = 0;
	chip->c.select_chip = nand_davinci_select_chip;

	if (pdata)
		ecc_mode = pdata->ecc_mode;
	else
		ecc_mode = NAND_ECC_HW;

	chip->c.ecc.mode  = ecc_mode;

	/* Set address of hardware control function */
	chip->c.cmd_ctrl  = nand_davinci_hwcontrol;
	chip->c.dev_ready = dev_ready_emwait;

	/* Set ECC size and bytes */
	chip->c.ecc.size = 256;
	chip->c.ecc.bytes = 3;
	if (ecc_mode == NAND_ECC_HW) {
		chip->c.ecc.size = 512;
		chip->c.ecc.calculate = nand_davinci_calculate_ecc;
		chip->c.ecc.correct   = nand_davinci_correct_data;
		chip->c.ecc.hwctl     = nand_davinci_enable_hwecc;
	}

	/* Speed up the read buffer */
	chip->c.read_buf      = nand_davinci_read_buf;
	chip->c.write_buf     = nand_davinci_write_buf;

	/* Speed up the creation of the bad block table */
	chip->c.scan_bbt      = nand_davinci_scan_bbt;

	if (irq >= 0) {
		if (request_irq(irq, (irq == IRQ_EMIF_EMWAIT_RISE)?
				&nand_isr_emwait : &nand_isr_gpirq,
				0, "davinci_nand", &info->mtd)) {
			printk(KERN_ERR "nand: request_irq failed, irq:%i\n",
				irq);
		} else {
			if (irq == IRQ_EMIF_EMWAIT_RISE) {
				chip->c.dev_ready = dev_ready_emwaitirq;
				chip->c.waitfunc = waitfunc_emwaitirq;
			} else {
				chip->c.dev_ready = dev_ready_gpirq;
				chip->c.waitfunc = waitfunc_gpirq;
			}
			chip->irq_enabled = 1;
			chip->irq = irq;
		}
	}

	info->mtd.owner = THIS_MODULE;

	width16 = (chip->c.options & NAND_BUSWIDTH_16) ? 1 : 0;
	nand_davinci_flash_init(chip, chip_num, width16, pdata->timings);

	/* Scan to find existence of the device */
	if (nand_scan_finish(&info->mtd, 1)) {
		width16 ^= 1;
		nand_davinci_flash_init(chip, chip_num, width16, pdata->timings);
		if (nand_scan_finish(&info->mtd, 1)) {
			printk(KERN_ERR "Chip Select is not set for NAND\n");
			err = -ENXIO;
			goto out_clk_disable;
		} else {
			printk(KERN_ERR "!!!platform option data does not "
					"match chip width\n");
		}
	}
#define BOOTCFG_OFFSET 0x0014
	if (chip_num == 0) {
		void __iomem *base = IO_ADDRESS(DAVINCI_SYSTEM_MODULE_BASE);
		/* grab default width from EM_WIDTH */
		int w = (__raw_readl(base + BOOTCFG_OFFSET)>>5) & 1;
		if (width16 != w) {
			printk(KERN_ERR "!!EM_WIDTH pin does not match chip "
			  "width, this will prevent booting from nand\n");
		}
	}

#ifdef USE_DMA
	chip->edma = -1;
	chip->edma_oob_link = -1;
	chip->edma_ecc = -1;
	chip->edma_fcr = -1;
	chip->edma_ecc_link = -1;
	chip->edma_fcr_link = -1;
#ifdef USE_DMA_FOR_WRITE
	chip->edma_oob_write = -1;
#endif
	init_completion(&chip->dma_complete);
	if (get_dma_channels(&info->mtd, chip) == 0) {
		chip->c.ecc.read_page = read_page_dma;
#ifdef USE_DMA_FOR_WRITE
		chip->c.ecc.write_page = write_page_dma;
#endif
		printk(KERN_INFO "nand read DMA active\n");
	}
#endif
	/* Register the partitions */
	add_mtd_partitions(&info->mtd, pdata->parts, pdata->nr_parts);

#ifdef CONFIG_MTD_CMDLINE_PARTS
	/* Set info->mtd.name = 0 temporarily */
	master_name = info->mtd.name;
	info->mtd.name = NULL;

	/* info->mtd.name == 0, means: don't bother checking
	   <mtd-id> */
	mtd_parts_nb = parse_mtd_partitions(&info->mtd, part_probes,
					    &mtd_parts, 0);

	/* Restore info->mtd.name */
	info->mtd.name = master_name;

	add_mtd_partitions(&info->mtd, mtd_parts, mtd_parts_nb);
#endif
	dev_set_drvdata(&pdev->dev, info);
	printk(KERN_INFO "nand options=%x\n", chip->c.options);
	return 0;

out_clk_disable:
	clk_disable(info->clk);
out_iounmap:
	iounmap(chip->c.IO_ADDR_R);
out_release_mem_region:
	release_mem_region(mem_start, mem_size);
out_free_info:
	kfree(info);
	return err;

}

/*
 * Clean up routine
 */
static int nand_davinci_remove(struct platform_device *pdev)
{
	struct nand_davinci_info *info = dev_get_drvdata(&pdev->dev);
	dev_set_drvdata(&pdev->dev, NULL);
	if (info) {
		/* Release resources, unregister device */
		clk_disable(info->clk);
		if (info->chip.irq >= 0)
			free_irq(info->chip.irq, &info->mtd);
#ifdef USE_DMA
		if (info->chip.edma >= 0)
			edma_free_channel(info->chip.edma);
		if (info->chip.edma_oob_link >= 0)
			edma_free_slot(info->chip.edma_oob_link);
		if (info->chip.edma_ecc >= 0)
			edma_free_channel(info->chip.edma_ecc);
		if (info->chip.edma_fcr >= 0)
			edma_free_channel(info->chip.edma_fcr);
		if (info->chip.edma_ecc_link >= 0)
			edma_free_slot(info->chip.edma_ecc_link);
		if (info->chip.edma_fcr_link >= 0)
			edma_free_slot(info->chip.edma_fcr_link);
#ifdef USE_DMA_FOR_WRITE
		if (info->chip.edma_oob_write >= 0)
			edma_free_channel(info->chip.edma_oob_write);
#endif
		if (info->chip.bounce_base)
			dma_free_coherent(NULL, info->chip.bounce_size,
					(void *)info->chip.bounce_base,
					info->chip.bounce_phys);
#endif
		if (info->chip.c.IO_ADDR_R) {
			iounmap(info->chip.c.IO_ADDR_R);
			release_mem_region(info->chip.mem_start,
					info->chip.mem_size);
		}
		nand_release(&info->mtd);
		/* Free the MTD device structure */
		kfree(info);
	} else {
		printk(KERN_ERR "Info is NULL\n");
	}
	return 0;
}


static struct platform_driver nand_davinci_driver = {
	.probe		= nand_davinci_probe,
	.remove		= nand_davinci_remove,
	.driver		= {
		.name	= DRIVER_NAME,
	},
};

static int __init nand_davinci_init(void)
{
	return platform_driver_register(&nand_davinci_driver);
}
module_init(nand_davinci_init);

#ifdef MODULE
static void __exit nand_davinci_exit(void)
{
	platform_driver_unregister(&nand_davinci_driver);
}
module_exit(nand_davinci_exit);
#endif

MODULE_ALIAS(DRIVER_NAME);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Texas Instruments");
MODULE_DESCRIPTION("Board-specific glue layer for NAND flash on davinci" \
		   "board");
