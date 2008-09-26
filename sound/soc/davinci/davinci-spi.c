/*
 * linux/sound/soc/davinci/davinci-spi.c
 *
 * spi driver for davinci audio
 *
 * Copyright (C) 2007 Boundary Devices
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <mach/hardware.h>
#include <mach/mux.h>
#include <mach/hardware.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <mach/irqs.h>
#include <asm/io.h>
#include <asm/types.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
//#define DEBUG

#ifdef DEBUG
#define DPRINTK(ARGS...)	do { printk("<%s>: ",__FUNCTION__);printk(ARGS); } while (0)
#else
#define DPRINTK( x... )
#endif

#define DAVINCI_SPI_TIMEOUT     (1*HZ)

#define DAVINCI_SPI_BASE	(0x01C66800)
#define    SPI_REG_BASE           IO_ADDRESS(DAVINCI_SPI_BASE)
struct spiRegs {			/* 0x01c66800*/
	volatile u32 gcr0;		/* 0x00 */
	volatile u32 gcr1;		/* 0x04 */
	volatile u32 intReg;		/* 0x08 */
	volatile u32 intLevel;		/* 0x0c */
	volatile u32 flagReg;		/* 0x10 */
	volatile u32 pinCtrl;		/* 0x14 */
	volatile u32 reserved1;		/* 0x18 */
	volatile u32 pinCtrl2;		/* 0x1c */
	volatile u32 reserved2;		/* 0x20 */
	volatile u32 reserved3;		/* 0x24 */
	volatile u32 reserved4;		/* 0x28 */
	volatile u32 reserved5;		/* 0x2c */
	volatile u32 reserved6;		/* 0x30 */
	volatile u32 reserved7;		/* 0x34 */
	volatile u32 reserved8;		/* 0x38 */
	volatile u32 dat1;		/* 0x3c */
	volatile u32 buf;		/* 0x40 */
	volatile u32 emuReg;		/* 0x44 */
	volatile u32 delay;		/* 0x48 */
	volatile u32 defChipSel;	/* 0x4c */
	volatile u32 fmt0;		/* 0x50 */
	volatile u32 fmt1;		/* 0x54 */
	volatile u32 fmt2;		/* 0x58 */
	volatile u32 fmt3;		/* 0x5c */
	volatile u32 intVect0;		/* 0x60 */
	volatile u32 intVect1;		/* 0x64 */
};

struct spi_davinci_device {
        int cmd_complete;
        wait_queue_head_t cmd_wait;
        struct semaphore write_lock;
        struct spiRegs* pSpi;
};
static struct spi_davinci_device spiDev;
static int initted = 0 ;

int davinci_spi_hw_write(void *control_data,const char* data,int len)
{
	struct spi_davinci_device* pDev = &spiDev;
	struct spiRegs* pSpi = pDev->pSpi;
	u16 value;
	value = (2<<16)|(data[0]<<8)|data[1];
	/* wait for the transaction to complete */
	down(&pDev->write_lock);
	wait_event_timeout (pDev->cmd_wait, pDev->cmd_complete, DAVINCI_SPI_TIMEOUT);
	if (!pDev->cmd_complete) {
		printk(KERN_ERR "spi: cmd complete failed\n");
	}
	DPRINTK("spi reg:%x = %x\n",reg,value);
	pDev->cmd_complete = 0;
	pSpi->dat1 = value;
	up(&pDev->write_lock);
	return 2;
}

#define SPI_CLK_DIVISOR 32

void spi_davinci_reset(struct spi_davinci_device* pDev)
{
	struct spiRegs* pSpi = pDev->pSpi;
	pSpi->gcr0 = 0;
	udelay(2);
	pSpi->gcr0 = 1;
	udelay(2);
	pSpi->gcr1 = 3;		/*set clkmod & master bits */
	pSpi->pinCtrl = (1<<11)|(1<<10)|(1<<9)|(1<<1)|(1<<0); /*Set DIFUN,DOFUN,CLKFUN,EN1FUN,EN0FUN */
	pSpi->fmt0 = (0<<20)|(1<<17)|(0<<16)|((SPI_CLK_DIVISOR-1)<<8)|16;	/* msb first,polarity high inactive,phase 0, 40ns min clock, 16 bit data*/
	pSpi->fmt1 = (0<<20)|(1<<17)|(0<<16)|((SPI_CLK_DIVISOR-1)<<8)|16;	/* msb first,polarity high inactive,phase 0, 40ns min clock, 16 bit data*/
	pSpi->fmt2 = (0<<20)|(1<<17)|(0<<16)|((SPI_CLK_DIVISOR-1)<<8)|16;	/* msb first,polarity high inactive,phase 0, 40ns min clock, 16 bit data*/
	pSpi->fmt3 = (0<<20)|(1<<17)|(0<<16)|((SPI_CLK_DIVISOR-1)<<8)|16;	/* msb first,polarity high inactive,phase 0, 40ns min clock, 16 bit data*/
	pSpi->dat1 = (0<<24)|(3<<16);
	pSpi->delay = 0;
	pSpi->defChipSel = 3;
	pSpi->intReg = (1<<8);		/* Enable receive interrupt (transmit finished interrupt)*/
	pSpi->intLevel = 0;		/* map all to IRQ_SPINT0 */
	pSpi->gcr1 = 3|(1<<24);		/*set clkmod & master bits, & spiena */
}

/*
 * Interrupt service routine. This gets called whenever an SPI interrupt
 * occurs.
 */
static irqreturn_t spi_davinci_isr(int this_irq, void *dev_id)
{
	struct spi_davinci_device* pDev = dev_id;
	struct spiRegs* pSpi = pDev->pSpi;
	unsigned int val;
	val = pSpi->buf;
	DPRINTK("entry\n");

	pDev->cmd_complete = 1;
	wake_up(&pDev->cmd_wait);
	return IRQ_HANDLED;
}

int davinci_spi_init(void)
{
	int status;
	struct device 	*dev = NULL;
	struct clk * spi_clock;
	unsigned long spi_rate;
	davinci_mux_peripheral(DAVINCI_MUX_ASP,1);
	davinci_mux_peripheral(DAVINCI_MUX_SPI,1);

	spi_clock = clk_get (dev, "SPICLK");

	if (IS_ERR(spi_clock))
        	return -1;

	clk_enable (spi_clock);
	spi_rate = clk_get_rate (spi_clock);

	DPRINTK("spi_rate= %ld\n", spi_rate);

	memset(&spiDev, 0, sizeof(struct spi_davinci_device));
	init_waitqueue_head(&spiDev.cmd_wait);
	init_MUTEX(&spiDev.write_lock);
	spiDev.cmd_complete = 1;
	spiDev.pSpi = (struct spiRegs*)SPI_REG_BASE;

	status = (int)request_region((int)spiDev.pSpi, 0x80, "davinci-spi");
	if (!status) {
		DPRINTK("SPI is already in use\n");
		return -ENODEV;
	}
	status = request_irq(IRQ_SPINT0, spi_davinci_isr, 0, "spi",&spiDev);
	if (status) {
		DPRINTK("failed to obtain SPI IRQ");
		release_region((int)spiDev.pSpi, 0x80);
		return status;
	}
	spi_davinci_reset(&spiDev);
        initted = 1 ;
	return 0;
}

extern void davinci_spi_shutdown(void)
{
	if(initted){
	   	struct clk * spi_clock;
	   	spi_clock = clk_get (0, "SPICLK");
		if (!IS_ERR(spi_clock))
			clk_disable (spi_clock);

		release_region((int)spiDev.pSpi, 0x80);
                free_irq(IRQ_SPINT0,&spiDev);
	}
}
