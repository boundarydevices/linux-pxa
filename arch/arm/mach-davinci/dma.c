/*
 * linux/arch/arm/mach-davinci/dma.c
 *
 * TI DaVinci DMA file
 *
 * Copyright (C) 2006 Texas Instruments.
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
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <mach/memory.h>
#include <linux/kernel.h>
#include <mach/hardware.h>
#include <mach/irqs.h>

#include <mach/edma.h>
/**************************************************************************\
* Register Overlay Structure for PARAMENTRY
\**************************************************************************/
#define PARM_OPT		0x00
#define PARM_SRC		0x04
#define PARM_A_B_CNT		0x08
#define PARM_DST		0x0c
#define PARM_SRC_DST_BIDX	0x10
#define PARM_LINK_BCNTRLD	0x14
#define PARM_SRC_DST_CIDX	0x18
#define PARM_CCNT		0x1c
#define PARM_SIZE		0x20

/**************************************************************************\
* Register Overlay Structure for SHADOW
\**************************************************************************/
#define SH_ER		0x00	/* 64 bits */
#define SH_ECR		0x08	/* 64 bits */
#define SH_ESR		0x10	/* 64 bits */
#define SH_CER		0x18	/* 64 bits */
#define SH_EER		0x20	/* 64 bits */
#define SH_EECR		0x28	/* 64 bits */
#define SH_EESR		0x30	/* 64 bits */
#define SH_SER		0x38	/* 64 bits */
#define SH_SECR		0x40	/* 64 bits */
#define SH_IER		0x50	/* 64 bits */
#define SH_IECR		0x58	/* 64 bits */
#define SH_IESR		0x60	/* 64 bits */
#define SH_IPR		0x68	/* 64 bits */
#define SH_ICR		0x70	/* 64 bits */
#define SH_IEVAL	0x78
#define SH_QER		0x80
#define SH_QEER		0x84
#define SH_QEECR	0x88
#define SH_QEESR	0x8c
#define SH_QSER		0x90
#define SH_QSECR	0x94
#define SH_SIZE		0x200

/**************************************************************************\
* Register Overlay Structure
\**************************************************************************/
#define EDMA_REV	0x0000
#define EDMA_CCCFG	0x0004
#define EDMA_QCHMAP	0x0200	/* 8 registers */
#define EDMA_DMAQNUM	0x0240	/* 8 registers */
#define EDMA_QDMAQNUM	0x0260
#define EDMA_QUETCMAP	0x0280
#define EDMA_QUEPRI	0x0284
#define EDMA_EMR	0x0300	/* 64 bits */
#define EDMA_EMCR	0x0308	/* 64 bits */
#define EDMA_QEMR	0x0310
#define EDMA_QEMCR	0x0314
#define EDMA_CCERR	0x0318
#define EDMA_CCERRCLR	0x031c
#define EDMA_EEVAL	0x0320
#define EDMA_DRAE	0x0340	/* 4 x 64 bits*/
#define EDMA_QRAE	0x0380	/* 4 registers */
#define EDMA_QUEEVTENTRY	0x0400	/* 2 x 16 registers */
#define EDMA_QSTAT	0x0600	/* 2 registers */
#define EDMA_QWMTHRA	0x0620
#define EDMA_QWMTHRB	0x0624
#define EDMA_CCSTAT	0x0640
#define EDMA_AETCTL	0x0700
#define EDMA_AETSTAT	0x0704
#define EDMA_AETCMD	0x0708
#define EDMA_M		0x1000	/* not referenced */
#define EDMA_SHADOW0	0x2000	/* 4 shadow regions */
#define EDMA_PARM	0x4000	/* 128 param entries */

#define PARM_OFFSET(param_no)	(EDMA_PARM + ((param_no) << 5))

unsigned int edmacc_regs_base;

static inline unsigned int edma_read(int offset)
{
	return (unsigned int)__raw_readl(edmacc_regs_base + offset);
}

static inline void edma_write(int offset, int val)
{
	__raw_writel(val, edmacc_regs_base + offset);
}
static inline void edma_modify(int offset, unsigned and, unsigned or)
{
	unsigned val = edma_read(offset);
	val &= and;
	val |= or;
	edma_write(offset, val);
}
static inline void edma_and(int offset, unsigned and)
{
	unsigned val = edma_read(offset);
	val &= and;
	edma_write(offset, val);
}
static inline void edma_or(int offset, unsigned or)
{
	unsigned val = edma_read(offset);
	val |= or;
	edma_write(offset, val);
}
static inline unsigned int edma_read_array(int offset, int i)
{
	return edma_read(offset + (i << 2));
}
static inline void edma_write_array(int offset, int i, unsigned val)
{
	edma_write(offset + (i << 2), val);
}
static inline void edma_modify_array(int offset, int i,
		unsigned and, unsigned or)
{
	edma_modify(offset + (i << 2), and, or);
}
static inline void edma_or_array(int offset, int i, unsigned or)
{
	edma_or(offset + (i << 2), or);
}
static inline void edma_or_array2(int offset, int i, int j, unsigned or)
{
	edma_or(offset + ((i*2 + j) << 2), or);
}
static inline void edma_write_array2(int offset, int i, int j, unsigned val)
{
	edma_write(offset + ((i*2 + j) << 2), val);
}
static inline unsigned int edma_shadow0_read(int offset)
{
	return edma_read(EDMA_SHADOW0 + offset);
}
static inline unsigned int edma_shadow0_read_array(int offset, int i)
{
	return edma_read(EDMA_SHADOW0 + offset + (i << 2));
}
static inline void edma_shadow0_write(int offset, unsigned val)
{
	edma_write(EDMA_SHADOW0 + offset, val);
}
static inline void edma_shadow0_write_array(int offset, int i, unsigned val)
{
	edma_write(EDMA_SHADOW0 + offset + (i << 2), val);
}
static inline unsigned int edma_parm_read(int offset, int param_no)
{
	return edma_read(EDMA_PARM + offset + (param_no << 5));
}
static inline void edma_parm_write(int offset, int param_no, unsigned val)
{
	edma_write(EDMA_PARM + offset + (param_no << 5), val);
}
static inline void edma_parm_modify(int offset, int param_no,
		unsigned and, unsigned or)
{
	edma_modify(EDMA_PARM + offset + (param_no << 5), and, or);
}
static inline void edma_parm_and(int offset, int param_no, unsigned and)
{
	edma_and(EDMA_PARM + offset + (param_no << 5), and);
}
static inline void edma_parm_or(int offset, int param_no, unsigned or)
{
	edma_or(EDMA_PARM + offset + (param_no << 5), or);
}
#define DAVINCI_DMA_3PCC_BASE 0x01C00000

static spinlock_t dma_chan_lock;
static struct device_driver edma_driver;
static struct platform_device edma_dev;

#define LOCK_INIT     spin_lock_init(&dma_chan_lock)
#define LOCK          spin_lock(&dma_chan_lock)
#define UNLOCK        spin_unlock(&dma_chan_lock)

typedef void (*intr_callback) (void);
static int register_dma_interrupts(intr_callback, intr_callback, intr_callback,
				   intr_callback);

static unsigned int get_edma_base(void)
{
	return (unsigned int) IO_ADDRESS(DAVINCI_DMA_3PCC_BASE);
}

static intr_callback cb[4];

/* Structure containing the dma channel parameters */
static struct davinci_dma_lch {
	int dev_id;
	int in_use;		/* 1-used 0-unused */
	int link_lch;
	int dma_running;
	int param_no;
	int tcc;
} dma_chan[DAVINCI_EDMA_NUM_PARAMENTRY];

static struct dma_interrupt_data {
	void (*callback) (int lch, unsigned short ch_status, void *data);
	void *data;
} intr_data[64];

/*
  Each bit field of the elements bellow indicate the corresponding EDMA channel
  availability  on arm side events
*/
static unsigned long edma_channels_arm[] = {
	0xffffffff,
	0xffffffff
};

/*
  Each bit field of the elements bellow indicate the corresponding QDMA channel
  availability  on arm side events
*/
static unsigned char qdma_channels_arm[] = {
	0x00
};

/*
   Each bit field of the elements bellow indicate corresponding PARAM entry
   availibility on arm side events
*/
static unsigned long param_entry_arm[] = {
	0xffffffff, 0xffffffff, 0x0000ffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
};

/*
   Each bit field of the elements bellow indicate whether a PARAM entry
   is free or in use
   1 - free
   0 - in use
*/
static unsigned long param_entry_use_status[] = {
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff
};

/*
   Each bit field of the elements bellow indicate whether a intrerrupt
   is free or in use
   1 - free
   0 - in use
*/
static unsigned long dma_intr_use_status[] = {
	0xffffffff,
	0xffffffff
};

/*
    This lists the DMA channel numbers which does not have any events
    associated with it
*/
static int dma_chan_no_event[] = {
	0, 1, 12, 13, 14, 15, 25, 30, 31, 45, 46, 47, 55, 56, 57, 58, 59, 60,
	61, 62, 63, -1
};

static int queue_tc_mapping[DAVINCI_EDMA_NUM_EVQUE + 1][2] = {
/* {event queue no, TC no} */
	{0, 0},
	{1, 1},
	{-1, -1}
};

static int queue_priority_mapping[DAVINCI_EDMA_NUM_EVQUE + 1][2] = {
	/* {event queue no, Priority} */
	{0, 3},
	{1, 7},
	{-1, -1}
};

static int qdam_to_param_mapping[8] = { 0 };


/*****************************************************************************/

static void map_dmach_queue(int ch_no, int queue_no)
{
	queue_no &= 7;
	if (ch_no < DAVINCI_EDMA_NUM_DMACH) {
		int bit = (ch_no & 0x7) * 4;
		edma_modify_array(EDMA_DMAQNUM, (ch_no >> 3),
				~(0x7 << bit), queue_no << bit);
	} else if (DAVINCI_EDMA_IS_Q(ch_no)) {
		int bit = (ch_no - DAVINCI_EDMA_QSTART) * 4;
		edma_modify(EDMA_QDMAQNUM, ~(0x7 << bit), queue_no << bit);
	}
}

/* For Davinci this Macro supports mapping only for QDMA channels and PaRam
   entry */
static void map_dmach_param(int ch_no, int param_no)
{
	if (DAVINCI_EDMA_IS_Q(ch_no)) {
		edma_modify_array(EDMA_QCHMAP, ch_no - DAVINCI_EDMA_QSTART,
				~(PAENTRY | TRWORD),
				((param_no & 0x1ff) << 5) | (QDMA_TRWORD << 2));
	}
}

static void map_queue_tc(int queue_no, int tc_no)
{
	int bit = queue_no * 4;
	edma_modify(EDMA_QUETCMAP, ~(0x7 << bit), ((tc_no & 0x7) << bit));
}

static void assign_priority_to_queue(int queue_no, int priority)
{
	int bit = queue_no * 4;
	edma_modify(EDMA_QUEPRI, ~(0x7 << bit), ((priority & 0x7) << bit));
}

/******************************************************************************
 *
 * DMA Param entry requests: Requests for the param structure entry for the dma
 *                          channel passed
 * Arguments:
 *      lch  - logical channel for which param entry is being requested.
 *
 * Return: param number on success, or negative error number on failure
 *
 *****************************************************************************/
static int request_param(int lch, int dev_id)
{
	int i = 0, j = 0, is_break = 0;
	if (lch >= 0 && lch < DAVINCI_EDMA_NUM_DMACH) {
		/*
		   In davinci there is 1:1 mapping between edma channels
		   and param sets
		 */
		LOCK;
		/* It maintains param entry availability bitmap which
		   could be updated by several thread  same channel
		   and so requires protection
		 */
		param_entry_use_status[lch / 32] &= (~(1 << (lch % 32)));
		UNLOCK;
		return lch;
	} else {
		if (dev_id >= DAVINCI_DMA_QDMA0 &&
		    dev_id <= DAVINCI_DMA_QDMA7) {
			i = 0;
		} else if (dev_id == DAVINCI_EDMA_PARAM_ANY) {
			i = DAVINCI_EDMA_NUM_DMACH;
		}

		/* This allocation alogrithm requires complete lock because
		   availabilty of param entry is checked from structure
		   param_entry_use_status and same struct is updated back also
		   once allocated
		 */

		LOCK;
		while (i < DAVINCI_EDMA_NUM_PARAMENTRY) {
			j = 0, is_break = 1;
			if ((param_entry_arm[i / 32] & (1 << (i % 32))) &&
			    (param_entry_use_status[i / 32] & (1 << (i % 32))))
			{
				if (dev_id != DAVINCI_EDMA_PARAM_ANY) {
					while (dma_chan_no_event[j] != -1) {
						if (dma_chan_no_event[j] == i) {
							is_break = 0;
						}
						j++;
					}
					if (!is_break) {
						break;
					}
				} else {
					break;
				}
				i++;
			} else {
				i++;
			}
		}
		if (i < DAVINCI_EDMA_NUM_PARAMENTRY) {
			param_entry_use_status[i / 32] &= (~(1 << (i % 32)));
			UNLOCK;
			dev_dbg(&edma_dev.dev, "param no=%d\r\n", i);
			return i;
		} else {
			UNLOCK;
			return -1;	/* no free param */
		}
	}
}

/******************************************************************************
 *
 * Free dma param entry: Freethe param entry number passed
 * Arguments:
 *      param_no - Param entry to be released or freed out
 *
 * Return: N/A
 *
 *****************************************************************************/
static void free_param(int param_no)
{
	if (param_no >= 0 && param_no < DAVINCI_EDMA_NUM_PARAMENTRY) {
		LOCK;
		/* This is global data structure and could be accessed
		   by several thread
		 */
		param_entry_use_status[param_no / 32] |= (1 << (param_no % 32));
		UNLOCK;
	}
}

/******************************************************************************
 *
 * DMA interrupt requests: Requests for the interrupt on the free channel
 *
 * Arguments:
 *      lch - logical channel number for which the interrupt is to be requested
 *            for the free channel.
 *      callback - callback function registered for the requested interrupt
 *                 channel
 *      data - channel private data.
 *
 * Return: free interrupt channel number on success, or negative error number
 *              on failure
 *
 *****************************************************************************/
static int request_dma_interrupt(int lch,
				 void (*callback) (int lch,
						   unsigned short ch_status,
						   void *data),
				 void *data, int param_no, int requested_tcc)
{
	signed int free_intr_no = -1;
	int i = 0, j = 0, is_break = 0;
	/* edma channels */
	if (lch >= 0 && lch < DAVINCI_EDMA_NUM_DMACH) {
		/* Bitmap dma_intr_use_status is used to identify availabe tcc
		   for interrupt purpose. This could be modified by several
		   thread and same structure is checked availabilty as well as
		   updated once it's found that resource is avialable */
		LOCK;
		if (dma_intr_use_status[lch / 32] & (1 << (lch % 32))) {
			/* in use */
			dma_intr_use_status[lch / 32] &= (~(1 << (lch % 32)));
			UNLOCK;
			free_intr_no = lch;
			dev_dbg(&edma_dev.dev, "interrupt no=%d\r\n", free_intr_no);
		} else {
			UNLOCK;
			dev_dbg(&edma_dev.dev, "EDMA:Error\r\n");
			return -1;
		}
	}

	/* qdma channels */
	else if (DAVINCI_EDMA_IS_Q(lch)) {
		if (requested_tcc != TCC_ANY) {
			/* Complete allocation algo requires lock and as it's
			   shared resources could be invoked by several thread.
			   Structure dma_intr_use_status is used to check
			   whether resource is availabe or not and latter marked
			   as not available in the same structure */
			LOCK;
			if (dma_intr_use_status[requested_tcc / 32] &
			    (1 << (requested_tcc % 32))) {
				j = 0;
				is_break = 1;
				while (dma_chan_no_event[j] != -1) {
					if (dma_chan_no_event[j] ==
					    requested_tcc) {
						is_break = 0;
						break;
					}
					j++;
				}
				if (!is_break) {
					dma_intr_use_status[requested_tcc / 32]
					    &= (~(1 << (requested_tcc % 32)));
					free_intr_no = requested_tcc;
					dev_dbg(&edma_dev.dev,
						"interrupt no=%d\r\n",
						free_intr_no);
				} else {
					UNLOCK;
					dev_dbg(&edma_dev.dev,
						"Error - wrong tcc passed\r\n");
					return -1;
				}
				UNLOCK;
			} else {
				UNLOCK;
				dev_dbg(&edma_dev.dev,
					"Error - wrong tcc passed\r\n");
				return -1;
			}
		} else {
			i = 0;
			LOCK;
			while (i < DAVINCI_EDMA_NUM_DMACH) {
				j = 0;
				is_break = 1;
				if (dma_intr_use_status[i / 32] &
				    (1 << (i % 32))) {
					while (dma_chan_no_event[j] != -1) {
						if (dma_chan_no_event[j] == i) {
							is_break = 0;
							break;
						}
						j++;
					}
					if (!is_break) {
						dma_intr_use_status[i / 32] &=
						    (~(1 << (i % 32)));
						free_intr_no = i;

						dev_dbg(&edma_dev.dev,
							"interrupt no=%d\r\n",
							free_intr_no);
						break;
					}
					i++;
				} else {
					i++;
				}
			}
			UNLOCK;
		}
	} else {
		dev_dbg(&edma_dev.dev, "ERROR lch = %d\r\n", lch);
	}
	if (is_break) {
		dev_dbg(&edma_dev.dev, "While allocating EDMA channel for QDMA");
	}
	if (DAVINCI_EDMA_IS_Q(lch)) {
		edma_or_array2(EDMA_DRAE, 0, free_intr_no >> 5,
				(1 << (free_intr_no & 0x1f)));
	}
	if (free_intr_no >= 0 && free_intr_no < 64) {
		intr_data[free_intr_no].callback = callback;
		intr_data[free_intr_no].data = data;
		edma_shadow0_write_array(SH_IESR, free_intr_no >> 5,
				(1 << (free_intr_no & 0x1f)));
	}
	return free_intr_no;
}

/******************************************************************************
 *
 * Free the dma interrupt: Releases the dma interrupt on the channel
 *
 * Arguments:
 *      intr_no - interrupt number on the channel to be released or freed out
 *
 * Return: N/A
 *
 *****************************************************************************/
static void free_dma_interrupt(int intr_no)
{
	if (intr_no >= 0 && intr_no < 64) {
		edma_shadow0_write_array(SH_ICR, intr_no >> 5,
				(1 << (intr_no & 0x1f)));
		LOCK;
		/* Global structure and could be modified by several task */
		dma_intr_use_status[intr_no >> 5] |= (1 << (intr_no & 0x1f));
		UNLOCK;
		intr_data[intr_no].callback = NULL;
		intr_data[intr_no].data = NULL;

	}
}

/**
 * davinci_dma_getposition - returns the current transfer points
 * @lch: logical channel number
 * @src: source port position
 * @dst: destination port position
 *
 * Returns current source and destination address of a paticular
 * DMA channel
 **/
void davinci_dma_getposition(int lch, dma_addr_t *src, dma_addr_t *dst)
{
	edmacc_paramentry_regs temp;

	davinci_get_dma_params(lch, &temp);
	if (src != NULL)
		*src = temp.src;
	if (dst != NULL)
		*dst = temp.dst;
}
EXPORT_SYMBOL(davinci_dma_getposition);
/******************************************************************************
 *
 * DMA interrupt handler
 *
 *****************************************************************************/
static void dma_irq_handler(void)
{
	int i;
	unsigned int cnt;
	cnt = 0;
	if ((edma_shadow0_read_array(SH_IPR, 0) == 0)
	    && (edma_shadow0_read_array(SH_IPR, 1) == 0))
		return;
	while (1) {
		int j;
		if (edma_shadow0_read_array(SH_IPR, 0))
			j = 0;
		else if (edma_shadow0_read_array(SH_IPR, 1))
			j = 1;
		else
			break;
		dev_dbg(&edma_dev.dev, "IPR%d =%x\r\n", j,
				edma_shadow0_read_array(SH_IPR, j));
		for (i = 0; i < 32; i++) {
			int k = (j << 5) + i;
			if (edma_shadow0_read_array(SH_IPR, j) & (1 << i)) {
				/* Clear the corresponding IPR bits */
				edma_shadow0_write_array(SH_ICR, j, (1 << i));
				if (intr_data[k].callback) {
					intr_data[k].callback(k, DMA_COMPLETE,
						intr_data[k].data);
				}
			}
		}
		cnt++;
		if (cnt > 10) {
			break;
		}
	}
	edma_shadow0_write(SH_IEVAL, 1);
}

/******************************************************************************
 *
 * DMA error interrupt handler
 *
 *****************************************************************************/
static void dma_ccerr_handler(void)
{
	int i;
	unsigned int cnt;
	cnt = 0;
	if ((edma_read_array(EDMA_EMR, 0) == 0) &&
	    (edma_read_array(EDMA_EMR, 1) == 0) &&
	    (edma_read(EDMA_QEMR) == 0) && (edma_read(EDMA_CCERR) == 0))
		return;
	while (1) {
		int j = -1;
		if (edma_read_array(EDMA_EMR, 0))
			j = 0;
		else if (edma_read_array(EDMA_EMR, 1))
			j = 1;
		if (j >= 0) {
			dev_dbg(&edma_dev.dev, "EMR%d =%x\r\n", j,
					edma_read_array(EDMA_EMR, j));
			for (i = 0; i < 32; i++) {
				int k = (j << 5) + i;
				if (edma_read_array(EDMA_EMR, j) & (1 << i)) {
					/* Clear the corresponding EMR bits */
					edma_write_array(EDMA_EMCR, j, 1 << i);
					/* Clear any SER */
					edma_shadow0_write_array(SH_SECR, j,
							(1 << i));
					if (intr_data[k].callback) {
						intr_data[k].callback(k,
								      DMA_CC_ERROR,
								      intr_data
								      [k].data);
					}
				}
			}
		} else if (edma_read(EDMA_QEMR)) {
			dev_dbg(&edma_dev.dev, "QEMR =%x\r\n",
				edma_read(EDMA_QEMR));
			for (i = 0; i < 8; i++) {
				if (edma_read(EDMA_QEMR) & (1 << i)) {
					/* Clear the corresponding IPR bits */
					edma_write(EDMA_QEMCR, 1 << i);
					edma_shadow0_write(SH_QSECR, (1 << i));
				}
			}
		} else if (edma_read(EDMA_CCERR)) {
			dev_dbg(&edma_dev.dev, "CCERR =%x\r\n",
				edma_read(EDMA_CCERR));
			for (i = 0; i < 8; i++) {
				if (edma_read(EDMA_CCERR) & (1 << i)) {
					/* Clear the corresponding IPR bits */
					edma_write(EDMA_CCERRCLR, 1 << i);
				}
			}
		}
		if ((edma_read_array(EDMA_EMR, 0) == 0)
		    && (edma_read_array(EDMA_EMR, 1) == 0)
		    && (edma_read(EDMA_QEMR) == 0)
		    && (edma_read(EDMA_CCERR) == 0)) {
			break;
		}
		cnt++;
		if (cnt > 10) {
			break;
		}
	}
	edma_write(EDMA_EEVAL, 1);
}

/******************************************************************************
 *
 * DMA error interrupt handler
 *
 *****************************************************************************/
static void dma_tc1err_handler(void)
{

}

/******************************************************************************
 *
 * DMA error interrupt handler
 *
 *****************************************************************************/
static void dma_tc2err_handler(void)
{

}

/******************************************************************************
 *
 * DMA initialisation on davinci
 *
 *****************************************************************************/
int __init arch_dma_init(void)
{
	int i;
	edma_driver.name = "edma";
	edma_dev.name = "dma";
	edma_dev.id = -1;
	edma_dev.dev.driver = &edma_driver;

	edmacc_regs_base = get_edma_base();
	dev_dbg(&edma_dev.dev, "DMA REG BASE ADDR=%x\n", edmacc_regs_base);
	memset(dma_chan, 0x00, sizeof(dma_chan));
	for (i = 0; i < DAVINCI_EDMA_NUM_PARAMENTRY * PARM_SIZE; i += 4)
		edma_write(EDMA_PARM + i, 0);

	/* Everything lives on transfer controller 1 until otherwise specified.
	 * This way, long transfers on the low priority queue
	 * started by the codec engine will not cause audio defects.
	 */
	for (i = 0; i < DAVINCI_EDMA_QEND; i++)
		map_dmach_queue(i, 1);

	i = 0;
	/* Event queue to TC mapping */
	while (queue_tc_mapping[i][0] != -1) {
		map_queue_tc(queue_tc_mapping[i][0], queue_tc_mapping[i][1]);
		i++;
	}
	i = 0;
	/* Event queue priority mapping */
	while (queue_priority_mapping[i][0] != -1) {
		assign_priority_to_queue(queue_priority_mapping[i][0],
					 queue_priority_mapping[i][1]);
		i++;
	}
	for (i = 0; i < DAVINCI_EDMA_NUM_REGIONS; i++) {
		edma_write_array2(EDMA_DRAE, i, 0, 0x0);
		edma_write_array2(EDMA_DRAE, i, 1, 0x0);
		edma_write_array(EDMA_QRAE, i, 0x0);
	}
	LOCK_INIT;
	return 0;
}

/******************************************************************************
 *
 * DMA channel requests: Requests for the dma device passed if it is free
 *
 * Arguments:
 *      dev_id     - request for the param entry device id
 *      dev_name   - device name
 *      callback   - pointer to the channel callback.
 *      Arguments:
 *          lch  - channel no, which is the IPR bit position,
 *		   indicating from which channel the interrupt arised.
 *          data - channel private data, which is received as one of the
 *		   arguments in davinci_request_dma.
 *      data - private data for the channel to be requested, which is used to
 *                   pass as a parameter in the callback function
 *		     in irq handler.
 *      lch - contains the device id allocated
 *  tcc        - Transfer Completion Code, used to set the IPR register bit
 *                   after transfer completion on that channel.
 *  eventq_no  - Event Queue no to which the channel will be associated with
 *               (valied only if you are requesting for a DMA MasterChannel)
 *               Values : 0 to 7
 *                       -1 for Default queue
 * INPUT:   dev_id
 * OUTPUT:  *dma_ch_out
 *
 * Return: zero on success, or corresponding error no on failure
 *
 *****************************************************************************/
int davinci_request_dma(int dev_id, const char *name,
			void (*callback) (int lch, unsigned short ch_status,
					  void *data),
			void *data, int *lch,
			int *tcc, enum dma_event_q eventq_no)
{

	int ret_val = 0, i = 0;
	static int req_flag = 0;
	/* checking the ARM side events */
	if (dev_id >= 0 && (dev_id < DAVINCI_EDMA_NUM_DMACH)) {
		if (!(edma_channels_arm[dev_id / 32] & (0x1 << (dev_id % 32)))) {
			dev_dbg(&edma_dev.dev,
				"dev_id = %d not supported on ARM side\r\n",
				dev_id);
			return -EINVAL;
		}
	} else if (DAVINCI_EDMA_IS_Q(dev_id)) {
		if (!(qdma_channels_arm[0] &
		      (0x1 << (dev_id - DAVINCI_EDMA_QSTART)))) {

			dev_dbg(&edma_dev.dev,
				"dev_id = %d not supported on ARM side\r\n",
				dev_id);
			return -EINVAL;
		}
	}

	if ((dev_id != DAVINCI_DMA_CHANNEL_ANY) &&
	    (dev_id != DAVINCI_EDMA_PARAM_ANY)) {
		if (DAVINCI_EDMA_IS_Q(dev_id)) {
			edma_or_array(EDMA_QRAE, 0,
					1 << (dev_id - DAVINCI_EDMA_QSTART));
		} else {
			edma_or_array2(EDMA_DRAE, 0, dev_id >> 5,
					1 << (dev_id & 0x1f));
		}
	}

	if (!req_flag) {
		if (register_dma_interrupts
		    (dma_irq_handler, dma_ccerr_handler,
		     dma_tc1err_handler, dma_tc2err_handler)) {
			dev_dbg(&edma_dev.dev,
				"register_dma_interrupts failed\r\n");
			return -EINVAL;
		} else
			req_flag = 1;
	}

	if (dev_id >= 0 && dev_id < (DAVINCI_EDMA_NUM_DMACH)) {
		/* The 64 Channels are mapped to the first 64 PARAM entries */
		if (!dma_chan[dev_id].in_use) {
			*lch = dev_id;
			dma_chan[*lch].param_no = request_param(*lch, dev_id);
			if (dma_chan[*lch].param_no == -1) {
				return -EINVAL;
			} else
				dev_dbg(&edma_dev.dev, "param_no=%d\r\n",
					dma_chan[*lch].param_no);
			if (callback) {
				dma_chan[*lch].tcc =
				    request_dma_interrupt(*lch, callback, data,
							  dma_chan[*lch].
							  param_no, *tcc);
				if (dma_chan[*lch].tcc == -1) {
					return -EINVAL;
				} else {
					*tcc = dma_chan[*lch].tcc;
					dev_dbg(&edma_dev.dev, "tcc_no=%d\r\n",
						dma_chan[*lch].tcc);
				}
			} else
				dma_chan[*lch].tcc = -1;

			map_dmach_queue(dev_id, eventq_no);
			ret_val = 0;
			/* ensure no events are pending */
			davinci_stop_dma(dev_id);
		} else
			ret_val = -EINVAL;
	} else if (DAVINCI_EDMA_IS_Q(dev_id)) {
		int temp_ch;
		temp_ch = qdam_to_param_mapping[dev_id - DAVINCI_EDMA_QSTART];
		if ((temp_ch != -1) && (dma_chan[temp_ch].in_use)) {
			ret_val = -EINVAL;
		} else {
			*lch = dev_id;
			dma_chan[*lch].param_no = request_param(*lch, dev_id);
			if (dma_chan[*lch].param_no == -1) {
				dev_dbg(&edma_dev.dev, "request_param failed\r\n");
				return -EINVAL;
			} else {
				dev_dbg(&edma_dev.dev, "param_no=%d\r\n",
					dma_chan[*lch].param_no);
				map_dmach_param(*lch, dma_chan[*lch].param_no);
			}
			if (callback) {
				dma_chan[*lch].tcc =
				    request_dma_interrupt(*lch, callback, data,
							  dma_chan[*lch].
							  param_no, *tcc);
				if (dma_chan[*lch].tcc == -1) {
					return -EINVAL;
				} else {
					*tcc = dma_chan[*lch].tcc;
					dev_dbg(&edma_dev.dev, "tcc_no=%d\r\n",
						dma_chan[*lch].tcc);
				}
			} else
				dma_chan[*lch].tcc = -1;
			map_dmach_queue(dev_id, eventq_no);
			ret_val = 0;
		}
	} else if (dev_id == DAVINCI_DMA_CHANNEL_ANY) {
		i = 0;
		ret_val = 0;
		while (dma_chan_no_event[i] != -1) {
			if (!dma_chan[dma_chan_no_event[i]].in_use) {
				int j;
				*lch = dma_chan_no_event[i];
				j = dma_chan[*lch].param_no =
						request_param(*lch, dev_id);
				if (j == -1)
					return -EINVAL;
				dev_dbg(&edma_dev.dev, "param_no=%d\r\n", j);
				if (DAVINCI_EDMA_IS_Q(j)) {
					edma_or_array(EDMA_QRAE, 0, 1 << (j -
							DAVINCI_EDMA_QSTART));
				} else {
					edma_or_array2(EDMA_DRAE, 0, j >> 5,
							1 << (j & 0x1f));
				}
				if (callback) {
					dma_chan[*lch].tcc =
						request_dma_interrupt(*lch,
								callback, data,
								j, *tcc);
					if (dma_chan[*lch].tcc == -1) {
						return -EINVAL;
					} else {
						*tcc = dma_chan[*lch].tcc;
					}
				} else {
					dma_chan[*lch].tcc = -1;
				}
				map_dmach_queue(*lch, eventq_no);
				ret_val = 0;
				break;
			}
			i++;
		}
	} else if (dev_id == DAVINCI_EDMA_PARAM_ANY) {
		ret_val = 0;
		for (i = DAVINCI_EDMA_QEND;
		     i < DAVINCI_EDMA_NUM_PARAMENTRY; i++) {
			if (!dma_chan[i].in_use) {
				dev_dbg(&edma_dev.dev, "any link = %d\r\n", i);
				*lch = i;
				dma_chan[*lch].param_no =
				    request_param(*lch, dev_id);
				if (dma_chan[*lch].param_no == -1) {
					dev_dbg(&edma_dev.dev,
						"request_param failed\r\n");
					return -EINVAL;
				} else {
					dev_dbg(&edma_dev.dev, "param_no=%d\r\n",
						dma_chan[*lch].param_no);
				}
				if (*tcc != -1)
					dma_chan[*lch].tcc = *tcc;
				else
					dma_chan[*lch].tcc = -1;
				ret_val = 0;
				break;
			}
		}
	} else {
		ret_val = -EINVAL;
	}
	if (!ret_val) {
		if (DAVINCI_EDMA_IS_Q(dev_id)) {
			/* Master Channel */
			unsigned int opt;
			int temp_ch = dma_chan[*lch].param_no;
			qdam_to_param_mapping[dev_id - DAVINCI_EDMA_QSTART] =
				temp_ch;
			LOCK;
			/* It's used global data structure and used to find out
			   whether channel is available or not */
			dma_chan[temp_ch].in_use = 1;
			UNLOCK;
			dma_chan[temp_ch].dev_id = *lch;
			dma_chan[temp_ch].tcc = dma_chan[*lch].tcc;
			dma_chan[temp_ch].param_no = temp_ch;
			opt = edma_parm_read(PARM_OPT, temp_ch);
			if (dma_chan[*lch].tcc != -1) {
				opt &= ~TCC;
				opt |= ((0x3f & dma_chan[*lch].tcc) << 12);
				opt |= TCINTEN;
			} else {
				opt &= ~TCINTEN;
			}
			edma_parm_write(PARM_OPT, temp_ch, opt);
			/* assign the link field to no link. i.e 0xffff */
			edma_parm_or(PARM_LINK_BCNTRLD, temp_ch, 0xffff);
		} else {
			int j;
			/* Slave Channel */
			LOCK;
			/* Global structure to identify whether resoures is
			   available or not */
			dma_chan[*lch].in_use = 1;
			UNLOCK;
			dma_chan[*lch].dev_id = *lch;
			j = dma_chan[*lch].param_no;
			if (dma_chan[*lch].tcc != -1) {
				edma_parm_modify(PARM_OPT, j, ~TCC,
					((0x3f & dma_chan[*lch].tcc) << 12)
					| TCINTEN);
			} else {
				edma_parm_and(PARM_OPT, j, ~TCINTEN);
			}
			/* assign the link field to no link. i.e 0xffff */
			edma_parm_or(PARM_LINK_BCNTRLD, j, 0xffff);
		}
	}
	return ret_val;
}

/******************************************************************************
 *
 * DMA channel free: Free dma channle
 * Arguments:
 *      dev_id     - request for the param entry device id
 *
 * Return: zero on success, or corresponding error no on failure
 *
 *****************************************************************************/
void davinci_free_dma(int lch)
{
	if (DAVINCI_EDMA_IS_Q(lch))
		lch = qdam_to_param_mapping[lch - DAVINCI_EDMA_QSTART];
	LOCK;
	dma_chan[lch].in_use = 0;
	UNLOCK;
	free_param(dma_chan[lch].param_no);

	if ((lch >= 0) && (lch < DAVINCI_EDMA_QEND))
		free_dma_interrupt(dma_chan[lch].tcc);
}

/******************************************************************************
 *
 * DMA source parameters setup
 * ARGUMENTS:
 *      lch         - channel for which the source parameters to be configured
 *      src_port    - Source port address
 *      addressMode - indicates wether addressing mode is fifo.
 *
 *****************************************************************************/
void davinci_set_dma_src_params(int lch, unsigned long src_port,
				enum address_mode mode, enum fifo_width width)
{
	if (DAVINCI_EDMA_IS_Q(lch))
		lch = qdam_to_param_mapping[lch - DAVINCI_EDMA_QSTART];
	if (lch >= 0 && lch < DAVINCI_EDMA_NUM_PARAMENTRY) {
		int j = dma_chan[lch].param_no;
		unsigned int i = edma_parm_read(PARM_OPT, j);
		if (mode) {
			/* set SAM and program FWID */
			i = (i & ~(EDMA_FWID)) | (SAM | ((width & 0x7) << 8));
		} else {
			/* clear SAM */
			i &= ~SAM;
		}
		edma_parm_write(PARM_OPT, j, i);

		/* set the source port address
		   in source register of param structure */
		edma_parm_write(PARM_SRC, j, src_port);
	}
}

/******************************************************************************
 *
 * DMA destination parameters setup
 * ARGUMENTS:
 *    lch - channel or param device for destination parameters to be configured
 *    dest_port    - Destination port address
 *    addressMode  - indicates wether addressing mode is fifo.
 *
 *****************************************************************************/
void davinci_set_dma_dest_params(int lch, unsigned long dest_port,
				 enum address_mode mode, enum fifo_width width)
{
	if (DAVINCI_EDMA_IS_Q(lch))
		lch = qdam_to_param_mapping[lch - DAVINCI_EDMA_QSTART];
	if (lch >= 0 && lch < DAVINCI_EDMA_NUM_PARAMENTRY) {
		int j = dma_chan[lch].param_no;
		unsigned int i = edma_parm_read(PARM_OPT, j);
		if (mode) {
			/* set DAM and program FWID */
			i = (i & ~(EDMA_FWID)) | (DAM | ((width & 0x7) << 8));
		} else {
			/* clear DAM */
			i &= ~DAM;
		}
		edma_parm_write(PARM_OPT, j, i);
		/* set the destination port address
		   in dest register of param structure */
		edma_parm_write(PARM_DST, j, dest_port);
	}
}

/******************************************************************************
 *
 * DMA source index setup
 * ARGUMENTS:
 *      lch     - channel or param device for configuration of source index
 *      srcbidx - source B-register index
 *      srccidx - source C-register index
 *
 *****************************************************************************/
void davinci_set_dma_src_index(int lch, short src_bidx, short src_cidx)
{
	if (DAVINCI_EDMA_IS_Q(lch))
		lch = qdam_to_param_mapping[lch - DAVINCI_EDMA_QSTART];
	if (lch >= 0 && lch < DAVINCI_EDMA_NUM_PARAMENTRY) {
		edma_parm_modify(PARM_SRC_DST_BIDX, dma_chan[lch].param_no,
				0xffff0000, src_bidx);
		edma_parm_modify(PARM_SRC_DST_CIDX, dma_chan[lch].param_no,
				0xffff0000, src_cidx);
	}
}

/******************************************************************************
 *
 * DMA destination index setup
 * ARGUMENTS:
 *      lch    - channel or param device for configuration of destination index
 *      srcbidx - dest B-register index
 *      srccidx - dest C-register index
 *
 *****************************************************************************/
void davinci_set_dma_dest_index(int lch, short dest_bidx, short dest_cidx)
{
	if (DAVINCI_EDMA_IS_Q(lch))
		lch = qdam_to_param_mapping[lch - DAVINCI_EDMA_QSTART];
	if (lch >= 0 && lch < DAVINCI_EDMA_NUM_PARAMENTRY) {
		edma_parm_modify(PARM_SRC_DST_BIDX, dma_chan[lch].param_no,
				0x0000ffff, dest_bidx << 16);
		edma_parm_modify(PARM_SRC_DST_CIDX, dma_chan[lch].param_no,
				0x0000ffff, dest_cidx << 16);
	}
}

/******************************************************************************
 *
 * DMA transfer parameters setup
 * ARGUMENTS:
 *      lch  - channel or param device for configuration of aCount, bCount and
 *         cCount regs.
 *      acnt - acnt register value to be configured
 *      bcnt - bcnt register value to be configured
 *      ccnt - ccnt register value to be configured
 *
 *****************************************************************************/
void davinci_set_dma_transfer_params(int lch, unsigned short acnt,
				     unsigned short bcnt, unsigned short ccnt,
				     unsigned short bcntrld,
				     enum sync_dimension sync_mode)
{
	if (DAVINCI_EDMA_IS_Q(lch))
		lch = qdam_to_param_mapping[lch - DAVINCI_EDMA_QSTART];
	if (lch >= 0 && lch < DAVINCI_EDMA_NUM_PARAMENTRY) {
		int j = dma_chan[lch].param_no;
		edma_parm_modify(PARM_LINK_BCNTRLD, j,
				0x0000ffff, bcntrld << 16);
		if (sync_mode == ASYNC)
			edma_parm_and(PARM_OPT, j, ~SYNCDIM);
		else
			edma_parm_or(PARM_OPT, j, SYNCDIM);
		/* Set the acount, bcount, ccount registers */
		edma_parm_write(PARM_A_B_CNT, j, (bcnt << 16) | acnt);
		edma_parm_write(PARM_CCNT, j, ccnt);
	}
}

/******************************************************************************
 *
 * davinci_set_dma_params -
 * ARGUMENTS:
 *      lch - logical channel number
 *
 *****************************************************************************/
void davinci_set_dma_params(int lch, edmacc_paramentry_regs * temp)
{
	if (DAVINCI_EDMA_IS_Q(lch))
		lch = qdam_to_param_mapping[lch - DAVINCI_EDMA_QSTART];
	if (lch >= 0 && lch < DAVINCI_EDMA_NUM_PARAMENTRY) {
		int j = dma_chan[lch].param_no;
		edma_parm_write(PARM_OPT, j, temp->opt);
		edma_parm_write(PARM_SRC, j, temp->src);
		edma_parm_write(PARM_A_B_CNT, j, temp->a_b_cnt);
		edma_parm_write(PARM_DST, j, temp->dst);
		edma_parm_write(PARM_SRC_DST_BIDX, j, temp->src_dst_bidx);
		edma_parm_write(PARM_LINK_BCNTRLD, j, temp->link_bcntrld);
		edma_parm_write(PARM_SRC_DST_CIDX, j, temp->src_dst_cidx);
	}
}

/******************************************************************************
 *
 * davinci_get_dma_params -
 * ARGUMENTS:
 *      lch - logical channel number
 *
 *****************************************************************************/
void davinci_get_dma_params(int lch, edmacc_paramentry_regs * temp)
{
	if (DAVINCI_EDMA_IS_Q(lch))
		lch = qdam_to_param_mapping[lch - DAVINCI_EDMA_QSTART];
	if (lch >= 0 && lch < DAVINCI_EDMA_NUM_PARAMENTRY) {
		int j = dma_chan[lch].param_no;
		temp->opt = edma_parm_read(PARM_OPT, j);
		temp->src = edma_parm_read(PARM_SRC, j);
		temp->a_b_cnt = edma_parm_read(PARM_A_B_CNT, j);
		temp->dst = edma_parm_read(PARM_DST, j);
		temp->src_dst_bidx = edma_parm_read(PARM_SRC_DST_BIDX, j);
		temp->link_bcntrld = edma_parm_read(PARM_LINK_BCNTRLD, j);
		temp->src_dst_cidx = edma_parm_read(PARM_SRC_DST_CIDX, j);
	}
}

/*
 * DMA pause - pauses the dma on the channel passed
 */
void davinci_pause_dma(int lch)
{
	if ((lch >= 0) && (lch < DAVINCI_EDMA_NUM_DMACH)) {
		unsigned int mask = (1 << (lch & 0x1f));
		edma_shadow0_write_array(SH_EECR, lch >> 5, mask);
	}
}
EXPORT_SYMBOL(davinci_pause_dma);
/*
 * DMA resume - resumes the dma on the channel passed
 */
void davinci_resume_dma(int lch)
{
	if ((lch >= 0) && (lch < DAVINCI_EDMA_NUM_DMACH)) {
		unsigned int mask = (1 << (lch & 0x1f));
		edma_shadow0_write_array(SH_EESR, lch >> 5, mask);
	}
}
EXPORT_SYMBOL(davinci_resume_dma);
/******************************************************************************
 *
 * DMA Start - Starts the dma on the channel passed
 * ARGUMENTS:
 *      lch - logical channel number
 *
 *****************************************************************************/
int davinci_start_dma(int lch)
{
	int ret_val = 0;
	if ((lch >= 0) && (lch < DAVINCI_EDMA_NUM_DMACH)) {
		int i = 0;
		int j = lch >> 5;
		unsigned int mask = (1 << (lch & 0x1f));
		/* If the dma start request is for the unused events */
		while (dma_chan_no_event[i] != -1) {
			if (dma_chan_no_event[i] == lch) {
				/* EDMA channels without event association */
				dev_dbg(&edma_dev.dev, "ESR%d=%x\r\n", j,
					edma_shadow0_read_array(SH_ESR, j));
				edma_shadow0_write_array(SH_ESR, j, mask);
				return ret_val;
			}
			i++;
		}
		/* EDMA channel with event association */
		dev_dbg(&edma_dev.dev, "ER%d=%x\r\n", j,
			edma_shadow0_read_array(SH_ER, j));
		/* Clear any pending error */
		edma_write_array(EDMA_EMCR, j, mask);
		/* Clear any SER */
		edma_shadow0_write_array(SH_SECR, j, mask);
		edma_shadow0_write_array(SH_EESR, j, mask);
		dev_dbg(&edma_dev.dev, "EER%d=%x\r\n", j,
			edma_shadow0_read_array(SH_EER, j));
	} else if (DAVINCI_EDMA_IS_Q(lch)) {
		edma_shadow0_write(SH_QEESR, 1 << (lch - DAVINCI_EDMA_QSTART));
	} else {		/* for slaveChannels */
		ret_val = -EINVAL;
	}
	return ret_val;
}

/******************************************************************************
 *
 * DMA Stop - Stops the dma on the channel passed
 * ARGUMENTS:
 *      lch - logical channel number
 *
 *****************************************************************************/
void davinci_stop_dma(int lch)
{
	if ((lch >= 0) && (lch < DAVINCI_EDMA_NUM_DMACH)) {
		int j = lch >> 5;
		unsigned int mask = (1 << (lch & 0x1f));
		edma_shadow0_write_array(SH_EECR, j, mask);
		if (edma_shadow0_read_array(SH_ER, j) & mask) {
			dev_dbg(&edma_dev.dev, "ER%d=%x\n", j,
				edma_shadow0_read_array(SH_ER, j));
			edma_shadow0_write_array(SH_ECR, j, mask);
		}
		if (edma_shadow0_read_array(SH_SER, j) & mask) {
			dev_dbg(&edma_dev.dev, "SER%d=%x\n", j,
				edma_shadow0_read_array(SH_SER, j));
			edma_shadow0_write_array(SH_SECR, j, mask);
		}
		if (edma_read_array(EDMA_EMR, j) & mask) {
			dev_dbg(&edma_dev.dev, "EMR%d=%x\n", j,
				edma_read_array(EDMA_EMR, j));
			edma_write_array(EDMA_EMCR, j, mask);
		}
		dev_dbg(&edma_dev.dev, "EER%d=%x\r\n", j,
				edma_shadow0_read_array(SH_EER, j));
		/*
		 * if the requested channel is one of the event channels
		 * then just set the link field of the corresponding
		 * param entry to 0xffff
		 */
		edma_parm_or(PARM_LINK_BCNTRLD, lch, 0xffff);
	} else if (DAVINCI_EDMA_IS_Q(lch)) {
		/* for QDMA channels */
		edma_shadow0_write(SH_QEECR, 1 << (lch - DAVINCI_EDMA_QSTART));
		dev_dbg(&edma_dev.dev, "QER=%x\r\n", edma_shadow0_read(SH_QER));
		dev_dbg(&edma_dev.dev, "QEER=%x\r\n",
				edma_shadow0_read(SH_QEER));
	} else if ((lch >= DAVINCI_EDMA_QEND) &&
		   (lch < DAVINCI_EDMA_NUM_PARAMENTRY)) {
		/* for slaveChannels */
		edma_parm_or(PARM_LINK_BCNTRLD, lch, 0xffff);
	}
}

/******************************************************************************
 *
 * DMA channel link - link the two logical channels passed through by linking
 *		the link field of head to the param pointed by the lch_que.
 * ARGUMENTS:
 * lch  - logical channel number, in which the link field is linked
 *                  to the param pointed to by lch_que
 * lch_que - logical channel number or the param entry number, which is to be
 *                  linked to the lch
 *
 *****************************************************************************/
void davinci_dma_link_lch(int lch, int lch_que)
{
	if (DAVINCI_EDMA_IS_Q(lch))
		lch = qdam_to_param_mapping[lch - DAVINCI_EDMA_QSTART];
	if (DAVINCI_EDMA_IS_Q(lch_que))
		lch_que = qdam_to_param_mapping[lch_que - DAVINCI_EDMA_QSTART];
	if ((lch >= 0) && (lch < DAVINCI_EDMA_NUM_PARAMENTRY) &&
	    (lch_que >= 0) && (lch_que < DAVINCI_EDMA_NUM_PARAMENTRY)) {
		/* program LINK */
		edma_parm_modify(PARM_LINK_BCNTRLD, dma_chan[lch].param_no,
				0xffff0000,
				PARM_OFFSET(dma_chan[lch_que].param_no));
		dma_chan[lch].link_lch = lch_que;
	}
}

/******************************************************************************
 *
 * DMA channel unlink - unlink the two logical channels passed through by
 *                   setting the link field of head to 0xffff.
 * ARGUMENTS:
 * lch - logical channel number, from which the link field is to be removed
 * lch_que - logical channel number or the param entry number, which is to be
 *             unlinked from lch
 *
 *****************************************************************************/
void davinci_dma_unlink_lch(int lch, int lch_que)
{
	if (DAVINCI_EDMA_IS_Q(lch))
		lch = qdam_to_param_mapping[lch - DAVINCI_EDMA_QSTART];
	if (DAVINCI_EDMA_IS_Q(lch_que))
		lch_que = qdam_to_param_mapping[lch_que - DAVINCI_EDMA_QSTART];
	if ((lch >= 0) && (lch < DAVINCI_EDMA_NUM_PARAMENTRY) &&
	    (lch_que >= 0) && (lch_que < DAVINCI_EDMA_NUM_PARAMENTRY)) {
		edma_parm_or(PARM_LINK_BCNTRLD, dma_chan[lch].param_no,
				0xffff);
		dma_chan[lch].link_lch = -1;
	}
}

/******************************************************************************
 *
 * DMA channel chain - chains the two logical channels passed through by
 * ARGUMENTS:
 * lch - logical channel number, where the tcc field is to be set
 * lch_que - logical channel number or the param entry number, which is to be
 *             chained to lch
 *
 *****************************************************************************/
void davinci_dma_chain_lch(int lch, int lch_que)
{
	if (DAVINCI_EDMA_IS_Q(lch))
		lch = qdam_to_param_mapping[lch - DAVINCI_EDMA_QSTART];
	if (DAVINCI_EDMA_IS_Q(lch_que))
		lch_que = qdam_to_param_mapping[lch_que - DAVINCI_EDMA_QSTART];
	if ((lch >= 0) && (lch < DAVINCI_EDMA_QEND) &&
	    (lch_que >= 0) && (lch_que < DAVINCI_EDMA_QEND)) {
		/* program tcc */
		edma_parm_modify(PARM_OPT, lch, ~TCC,
				((lch_que & 0x3f) << 12) | TCCHEN);
	}
}

/******************************************************************************
 *
 * DMA channel unchain - unchain the two logical channels passed through by
 * ARGUMENTS:
 * lch - logical channel number, from which the tcc field is to be removed
 * lch_que - logical channel number or the param entry number, which is to be
 *             unchained from lch
 *
 *****************************************************************************/
void davinci_dma_unchain_lch(int lch, int lch_que)
{
	if (DAVINCI_EDMA_IS_Q(lch))
		lch = qdam_to_param_mapping[lch - DAVINCI_EDMA_QSTART];
	if (DAVINCI_EDMA_IS_Q(lch_que))
		lch_que = qdam_to_param_mapping[lch_que - DAVINCI_EDMA_QSTART];
	/* reset TCCHEN */
	if ((lch >= 0) && (lch < DAVINCI_EDMA_QEND) &&
	    (lch_que >= 0) && (lch_que < DAVINCI_EDMA_QEND)) {
		edma_parm_and(PARM_OPT, lch, ~TCCHEN);
	}
}

/******************************************************************************
 *
 * It cleans ParamEntry qand bring back EDMA to initial state if media has
 * been removed before EDMA has finished.It is usedful for removable media.
 * Arguments:
 *      ch_no     - channel no
 *
 * Return: zero on success, or corresponding error no on failure
 *
 *****************************************************************************/

void davinci_clean_channel(int ch_no)
{
	if ((ch_no >= 0) && (ch_no < DAVINCI_EDMA_NUM_DMACH)) {
		int j = (ch_no >> 5);
		unsigned int mask = 1 << (ch_no & 0x1f);
		dev_dbg(&edma_dev.dev, "EMR%d =%x\r\n", j,
				edma_read_array(EDMA_EMR, j));
		edma_shadow0_write_array(SH_ECR, j, mask);
		/* Clear the corresponding EMR bits */
		edma_write_array(EDMA_EMCR, j, mask);
		/* Clear any SER */
		edma_shadow0_write_array(SH_SECR, j, mask);
		edma_write(EDMA_CCERRCLR, (1 << 16) | 0x3);
	}
}

/******************************************************************************
 *
 * DMA interrupt handlers
 *
 *****************************************************************************/
static int dma_irq_handler_l(int sound_curr_lch, void *ch_status)
{
	dev_dbg(&edma_dev.dev, "dma_irq_handler\n");
	(*cb[0]) ();
	return IRQ_HANDLED;
}

static int dma_ccerr_handler_l(int sound_curr_lch, void *ch_status)
{
	dev_dbg(&edma_dev.dev, "dma_ccerr_handler\n");
	(*cb[1]) ();
	return IRQ_HANDLED;
}

static int dma_tc1err_handler_l (int sound_curr_lch, void *ch_status)
{
	dev_dbg(&edma_dev.dev, "dma_tc1err_handler\n");
	(*cb[2]) ();
	return IRQ_HANDLED;
}

static int dma_tc2err_handler_l(int sound_curr_lch, void *ch_status)
{
	dev_dbg(&edma_dev.dev, "dma_tc2err_handler\n");
	(*cb[3]) ();
	return IRQ_HANDLED;
}

int register_dma_interrupts(intr_callback cb1, intr_callback cb2,
			intr_callback cb3, intr_callback cb4)
{
	cb[0] = cb1;
	cb[1] = cb2;
	cb[2] = cb3;
	cb[3] = cb4;
	if (!cb1 || !cb2 || !cb3 || !cb4) {
		dev_dbg(&edma_dev.dev, "NULL callback\n");
		return -1;
	}

	if (request_irq(IRQ_CCINT0, dma_irq_handler_l, 0, "EDMA", NULL)) {
		dev_dbg(&edma_dev.dev, "request_irq failed\n");
		return -1;
	}
	if (request_irq
	    (IRQ_CCERRINT, dma_ccerr_handler_l, 0, "EDMA CC Err", NULL)) {
		dev_dbg(&edma_dev.dev, "request_irq failed\n");
		return -1;
	}
	if (request_irq
	    (IRQ_TCERRINT0, dma_tc1err_handler_l, 0, "EDMA TC1 Err", NULL)) {
		dev_dbg(&edma_dev.dev, "request_irq failed\n");
		return -1;
	}
	if (request_irq
	    (IRQ_TCERRINT, dma_tc2err_handler_l, 0, "EDMA TC2 Err", NULL)) {
		dev_dbg(&edma_dev.dev, "request_irq failed\n");
		return -1;
	}
	return 0;
}

arch_initcall(arch_dma_init);
EXPORT_SYMBOL(davinci_start_dma);
EXPORT_SYMBOL(davinci_dma_link_lch);
EXPORT_SYMBOL(davinci_set_dma_params);
EXPORT_SYMBOL(davinci_get_dma_params);
EXPORT_SYMBOL(davinci_set_dma_transfer_params);
EXPORT_SYMBOL(davinci_set_dma_dest_index);
EXPORT_SYMBOL(davinci_set_dma_src_index);
EXPORT_SYMBOL(davinci_set_dma_dest_params);
EXPORT_SYMBOL(davinci_set_dma_src_params);
EXPORT_SYMBOL(davinci_request_dma);
EXPORT_SYMBOL(davinci_stop_dma);
EXPORT_SYMBOL(davinci_clean_channel);
EXPORT_SYMBOL(davinci_free_dma);
EXPORT_SYMBOL(davinci_dma_chain_lch);
EXPORT_SYMBOL(davinci_dma_unchain_lch);
EXPORT_SYMBOL(davinci_dma_unlink_lch);
