/* 
 * Copyright (C) 2006 Texas Instruments Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option)any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * File: davincifb.c	
 */

/*
 * Linux includes	
 */
#define DEBUG
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <linux/moduleparam.h>	/* for module_param() */
#include <asm/system.h>
#include <video/davincifb.h>
#include "davincifb_regs.h"
#include "davincifb.h"
#include <mach/edma.h>
#include <mach/hardware.h>
#include <mach/mux.h>
/*
 * Local includes	
 */
#include "davincifb_osd.c"
#include "davincifb_dlcd.c"
#include "davincifb_venc.c"

#define LCD_PANEL 0
#define CRT 1
//              xres,xsyncWidth,xbegin,xend, yres,ysyncWidth,ybegin,yend,
//			vsPol,hsPol,pPol,oePol,dPol,
//					 enable,unscramble,rotate,active,vSyncHz,type
#define hitachi_qvga_P 320, 64, 1, 16,		240, 20, 8, 3,	0,0,1,0,0,	1,0,0,1,62,LCD_PANEL
#define sharp_qvga_P   320, 20, 1, 30,		240, 4, 17, 3,	0,0,1,0,0,	1,0,0,1,62,LCD_PANEL
#define okaya_qvga_P   320, 30,37, 38,		240, 3, 16, 15,	1,1,0,0,0,	1,0,0,1,120,LCD_PANEL
#define okaya_480x272_P 480, 3,20, 38,		272, 3, 5, 15,	1,1,1,0,0,	1,0,0,1,62,LCD_PANEL
#define hitachi_hvga_P  640, 64, 34, 1,		240, 20, 8, 3,	0,0,1,0,0,	1,1,0,1,62,LCD_PANEL
#define hitachi_wvga_P  800, 64, 34, 1,		480, 20, 8, 3,	0,0,1,0,0,	1,1,0,1,62,LCD_PANEL
#define sharp_vga_P  640, 64, 34,105,		480, 20, 8,14,	0,0,1,0,0,	1,1,0,1,62,LCD_PANEL
#define qvga_portrait_P  240, 64, 34, 1,	320, 20, 8, 3,	0,0,1,0,0,	1,0,1,1,62,LCD_PANEL
#define lcd_svga_P  800, 64, 32,152,		600,  3, 1,27,	1,1,0,0,0,	1,1,0,1,62,LCD_PANEL
#define crt800x600_P  800, 64, 32,152,		600,  3, 1,27,	1,1,0,0,0,	1,1,0,1,62,CRT
#define gvision_P  800, 64, 32,16,		600,  8, 3,2,	1,1,0,0,0,	1,1,0,1,62,LCD_PANEL
#define crt1024x768_P 1024,0xe4,0x3c,0x70,	768,0x0c,0x0b,0x20, 0,0,1,0,0,	1,1,0,1,62,CRT
#define hitachi_92_P 960,15,220,1,    160,	200,148,3,	0,0,1,0,0,	1,0,0,1,62,LCD_PANEL
#define tovis_w_P	1024,104,56,160,	200,3,201,11,	1,1,0,0,0,	1,0,0,1,75,CRT
#define hitachi_wxga_P	1024,64,1,39,		768,20,8,3,	1,1,1,0,0,	1,0,0,1,75,LCD_PANEL
#define hitachi_154_P 1280,64, 24,16,	800, 20, 4, 3,	1,1,0,0,0,	1,0,0,1,62,LCD_PANEL
#define samsung1600x1050 1600,104,128,264,	1050, 4, 2, 44,	1,1,0,0,0,	1,0,0,1,62,LCD_PANEL

typedef struct
{
	unsigned short xRes,xSyncWidth,xBegin,xEnd;
	unsigned short yRes,ySyncWidth,yBegin,yEnd;
	unsigned char vsPol,hsPol,pPol,oePol,dPol;
	unsigned char enable,unscramble,rotate,active,vSyncHz,type;
	unsigned char encPerPixel;
} DISPLAYCFG;

const DISPLAYCFG stdDisplayTypes[] = {
	{hitachi_qvga_P},	//0
	{sharp_qvga_P},		//1
	{okaya_qvga_P},		//2
	{okaya_480x272_P},	//3
	{hitachi_hvga_P},	//4
	{hitachi_wvga_P},	//5
	{sharp_vga_P},		//6
	{qvga_portrait_P},	//7
	{lcd_svga_P},		//8
	{crt800x600_P},		//9
	{crt1024x768_P},	//10
	{hitachi_92_P},		//11
	{tovis_w_P},		//12
	{hitachi_wxga_P},	//13
	{hitachi_154_P},	//14
	{samsung1600x1050},     //15
	{gvision_P},		//16
};
#define DEF_INDEX 14
#define RESIZER 1

/*
 *     Module parameter definations
 */
static char *options = "";

module_param(options, charp, S_IRUGO);
static int dmach = -1 ;
static int dmatcc = TCC_ANY ;
static int noblank = 1 ;

/*
 * Globals
 */
/*     Modelist        */
/* 
 * First mode(0th mode) in case of composite, svideo and component are kept
 * 	dummy. This is for backward compatibility.	
 */
struct vpbe_fb_videomode svideo[] = {
	{"DUMMY",
	 FB_VMODE_INTERLACED,
	 720,
	 480,
	 30,
	 0,
	 0,
	 0,
	 0,
	 0x80,
	 0x12,
	 0,
	 0x80,
	 0x12,
	 1},
	{"NTSC-SV",
	 FB_VMODE_INTERLACED,
	 720,
	 480,
	 30,
	 0,
	 0,
	 0,
	 0,
	 0x80,
	 0x12,
	 0,
	 0x80,
	 0x12,
	 1},
	{"PAL-SV",
	 FB_VMODE_INTERLACED,
	 720,
	 576,
	 25,
	 0,
	 0,
	 0,
	 0,
	 0x80,
	 0x12,
	 0,
	 0x80,
	 0x12,
	 1},
	{"\0", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

struct vpbe_fb_videomode component[] = {
	{"LCD",
	 FB_VMODE_NONINTERLACED,
	 720,
	 480,
	 60,
	 0,
	 0,
	 0,
	 0,
	 0x80,
	 0x12,
	 0,
	 0x80,
	 0x12,
	 1},
	{"NTSC-CO",
	 FB_VMODE_INTERLACED,
	 720,
	 480,
	 30,
	 0,
	 0,
	 0,
	 0,
	 0x80,
	 0x12,
	 0,
	 0x80,
	 0x12,
	 1},
	{"PAL-CO",
	 FB_VMODE_INTERLACED,
	 720,
	 576,
	 25,
	 0,
	 0,
	 0,
	 0,
	 0x80,
	 0x12,
	 0,
	 0x80,
	 0x12,
	 1},
	{"P525",
	 FB_VMODE_NONINTERLACED,
	 720,
	 480,
	 60,
	 0,
	 0,
	 0,
	 0,
	 0x80,
	 0x12,
	 0,
	 0x80,
	 0x22,
	 1},
	{"P625",
	 FB_VMODE_NONINTERLACED,
	 720,
	 576,
	 60,
	 0,
	 0,
	 0,
	 0,
	 0x80,
	 0x12,
	 0,
	 0x80,
	 0x22,
	 1},
	{"\0", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

struct vpbe_fb_videomode composite[] = {
	{"DUMMY_COM",
	 FB_VMODE_INTERLACED,
	 720,
	 480,
	 30,
	 0,
	 0,
	 0,
	 0,
	 0x80,
	 0x12,
	 0,
	 0x80,
	 0x12,
	 1},
	{"NTSC_COM",
	 FB_VMODE_INTERLACED,
	 720,
	 480,
	 30,
	 0,
	 0,
	 0,
	 0,
	 0x80,
	 0x12,
	 0,
	 0x80,
	 0x12,
	 1},
	{"PAL_COM",
	 FB_VMODE_INTERLACED,
	 720,
	 576,
	 25,
	 0,
	 0,
	 0,
	 0,
	 0x80,
	 0x12,
	 0,
	 0x80,
	 0x12,
	 1},
	{"\0", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};
struct vpbe_fb_videomode rgb[] = {
	{"\0", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};
struct vpbe_fb_videomode ycc16[] = {
	{"\0", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};
struct vpbe_fb_videomode ycc8[] = {
	{"\0", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

struct vpbe_fb_videomode prgb[] = {
        {"PRGB_480",
         FB_VMODE_NONINTERLACED,
         640,
         480,
         60,
         95,
         70,
         34,
         11,
         9,
         9,
         0,
         0x59,
         0x22,
         1},
        {"PRGB_400",
         FB_VMODE_NONINTERLACED,
         640,
         400,
         60,
         96,
         70,
         40,
         15,
         9,
         9,
         1,
         0x58,
         0x22,
         1},
	{"PRGB_350",
         FB_VMODE_NONINTERLACED,
         640,
         350,
         60,
         96,
         70,
         0x59,
         0x52,
         9,
         9,
         2,
         0x58,
         0x72,
         1},
        {"\0", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

struct vpbe_fb_videomode srgb[] = {
	{"\0", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};
struct vpbe_fb_videomode epson[] = {
	{"\0", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};
struct vpbe_fb_videomode casio1g[] = {
	{"\0", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};
struct vpbe_fb_videomode udisp[] = {
	{"\0", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};
struct vpbe_fb_videomode stn[] = {
	{"\0", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};
struct vpbe_fb_videomode *modelist[] =
    { prgb, composite, svideo, component, rgb, ycc16, ycc8, srgb, epson,
	casio1g, udisp, stn
};

/*
 * display controller register I/O routines
 */
u32 dispc_reg_in(u32 offset)
{
	return (inl(offset));
}

u32 dispc_reg_out(u32 offset, u32 val)
{
	outl(val, offset);
	return (val);
}

u32 dispc_reg_merge(u32 offset, u32 val, u32 mask)
{
	u32 addr = offset;
	u32 new_val = (inl(addr) & ~mask) | (val & mask);
	outl(new_val, addr);
	return (new_val);
}

struct semaphore  resizer_sem;
unsigned long fbMemBase;
vpbe_dm_info_t dm_static;
vpbe_dm_info_t *dm = &dm_static;
static struct fb_ops davincifb_ops;

/* Default resolutions		*/
#define DISP_XRES	720
#define DISP_YRES	480
/*These should be set to the max resolution supported	*/
#define	DISP_MEMX	720
#define DISP_MEMY	576

/* Random value chosen for now. Should be within the panel's supported range */
#define LCD_PANEL_CLOCK	180000

/* All window widths have to be rounded up to a multiple of 32 bytes */

/* The OSD0 window has to be always within VID0. Plus, since it is in RGB565
 * mode, it _cannot_ overlap with VID1.
 * For defaults, we are setting the OSD0 window to be displayed in the top
 * left quadrant of the screen, and the VID1 in the bottom right quadrant.
 * So the default 'xres' and 'yres' are set to  half of the screen width and
 * height respectively. Note however that the framebuffer size is allocated
 * for the full screen size so the user can change the 'xres' and 'yres' by
 * using the FBIOPUT_VSCREENINFO ioctl within the limits of the screen size.
 */
#define round16(width) ((width+0x0f)&~0xf)
#define round64(width) ((width+0x3f)&~0x3f)
#define round256(width) ((width+0xff)&~0xff)

#define OSD0_XRES	round16(DISP_XRES)	/* pixels */
#define OSD0_YRES	DISP_YRES

#define OSD0_MAX_BPP	16
/* 16 bpp, Double buffered */
static struct fb_var_screeninfo osd0_default_var = {
	.xres = OSD0_XRES,
	.yres = OSD0_YRES,
	.xres_virtual = OSD0_XRES,
	.yres_virtual = OSD0_YRES * DOUBLE_BUF,
	.xoffset = 0,
	.yoffset = 0,
	.bits_per_pixel = 16,
	.grayscale = 0,
	.red = {11, 5, 0},
	.green = {5, 6, 0},
	.blue = {0, 5, 0},
	.transp = {0, 0, 0},
	.nonstd = 0,
	.activate = FB_ACTIVATE_NOW,
	.height = -1,
	.width = -1,
	.accel_flags = 0,
	.pixclock = LCD_PANEL_CLOCK,	/* picoseconds */
	.left_margin = 40,	/* pixclocks */
	.right_margin = 4,	/* pixclocks */
	.upper_margin = 8,	/* line clocks */
	.lower_margin = 2,	/* line clocks */
	.hsync_len = 4,		/* pixclocks */
	.vsync_len = 2,		/* line clocks */
	.sync = 0,
	.vmode = FB_VMODE_INTERLACED,
};

/* Using the full screen for OSD1 by default */
#define OSD1_XRES	round64(DISP_XRES)	/* pixels */
#define OSD1_YRES	DISP_YRES

#define OSD1_MAXBPP	16
static struct fb_var_screeninfo osd1_default_var = {
	.xres = OSD1_XRES,
	.yres = OSD1_YRES,
	.xres_virtual = OSD1_XRES,
	.yres_virtual = OSD1_YRES * DOUBLE_BUF,
	.xoffset = 0,
	.yoffset = 0,
	.bits_per_pixel = 4,
	.activate = FB_ACTIVATE_NOW,
	.accel_flags = 0,
	.pixclock = LCD_PANEL_CLOCK,	/* picoseconds */
	.vmode = FB_VMODE_INTERLACED,
};

/* Using the full screen for OSD0 by default */
#define VID0_XRES	round16(DISP_XRES)	/* pixels */
#define VID0_YRES	DISP_YRES

#define VIDEO_FB_BASE	0x86000000
#define VIDEO_FB_SIZE	0x01000000

#define VID0_MAXBPP	24	/* RGB888       */
static struct fb_var_screeninfo vid0_default_var = {
	.xres = VID0_XRES,
	.yres = VID0_YRES,
	.xres_virtual = VID0_XRES,
	.yres_virtual = VID0_YRES * TRIPLE_BUF,
	.xoffset = 0,
	.yoffset = 0,
	.bits_per_pixel = 16,	/*YUV 16 bits  */
	.activate = FB_ACTIVATE_NOW,
	.accel_flags = 0,
	.pixclock = LCD_PANEL_CLOCK,	/* picoseconds  */
	.vmode = FB_VMODE_INTERLACED,
};

/* Using the bottom right quadrant of the screen screen for VID1 by default,
 * but keeping the framebuffer allocated for the full screen, so the user can
 * change the 'xres' and 'yres' later using the FBIOPUT_VSCREENINFO ioctl.
 */
#define VID1_XRES 	round16(DISP_XRES)	/* pixels */
#define VID1_YRES 	DISP_YRES
#define VID1_MAXBPP	24	/* RGB888       */

static struct fb_var_screeninfo vid1_default_var = {
	.xres = VID1_XRES,
	.yres = VID1_YRES,
	.xres_virtual = VID1_XRES,
	.yres_virtual = VID1_YRES * TRIPLE_BUF,
	.xoffset = 0,
	.yoffset = 0,
	.bits_per_pixel = 16,	/* YUV 4:2:2    */
	.activate = FB_ACTIVATE_NOW,
	.accel_flags = 0,
	.pixclock = LCD_PANEL_CLOCK,	/* picoseconds */
	.vmode = FB_VMODE_INTERLACED,
};

#define x_pos(w)    ((w)->win_pos.xpos)
#define y_pos(w)    ((w)->win_pos.ypos)
#define	zoom(w)		((w)->zoom)

struct fb_var_screeninfo * const default_vars[] = {
   &vid0_default_var
,  &vid1_default_var 
,  &osd0_default_var 
,  &osd1_default_var 
};

/*
 * ======== vpbe_set_display_default ========
 */
/* This function configures all the display registers in default state.*/
void vpbe_set_display_default()
{
	dispc_reg_out(VENC_VMOD, 0);
	dispc_reg_out(VENC_CVBS, 0);
	dispc_reg_out(VENC_CMPNT, 0x100);
	davinci_mux_peripheral(DAVINCI_MUX_RGB888,0);
	davinci_mux_peripheral(DAVINCI_MUX_LOEEN,0);
	dispc_reg_out(VENC_LCDOUT, 0);
	dispc_reg_out(VENC_VIDCTL, 0x141);
	dispc_reg_out(VENC_DCLKCTL, 0);
	dispc_reg_out(VENC_DCLKPTN0, 0);
	dispc_reg_out(VENC_SYNCCTL, 0);
	dispc_reg_out(VENC_OSDCLK0, 1);
	dispc_reg_out(VENC_OSDCLK1, 2);
	dispc_reg_out(VPSS_CLKCTL, 0);
	dispc_reg_out(VENC_HSPLS, 0);
	dispc_reg_out(VENC_HSTART, 0);
	dispc_reg_out(VENC_HVALID, 0);
	dispc_reg_out(VENC_HINT, 0);
	dispc_reg_out(VENC_VSPLS, 0);
	dispc_reg_out(VENC_VSTART, 0);
	dispc_reg_out(VENC_VVALID, 0);
	dispc_reg_out(VENC_VINT, 0);
	dispc_reg_out(VENC_YCCCTL, 0);
	dispc_reg_out(VENC_DACTST, 0xF000);
	dispc_reg_out(VENC_DACSEL, 0);
	dispc_reg_out(VPBE_PCR, 0);
	dispc_reg_out(VENC_VDPRO, 0);
}

/* Must do checks against the limits of the output device */
static int
davincifb_venc_check_mode(const vpbe_dm_win_info_t * w,
			  const struct fb_var_screeninfo *var)
{
	return 0;
}
static void set_sdram_params(char *id, u32 addr, u32 line_length);
static void set_sdram_p(vpbe_dm_win_info_t *pi)
{
	if (pi->sdram_address) {
		set_sdram_params(pi->info.fix.id, pi->sdram_address, pi->info.fix.line_length);
		pi->sdram_address = 0;
	}
}
static irqreturn_t davincifb_isr(int irq, void *arg)
{
	vpbe_dm_info_t *dm = (vpbe_dm_info_t *)arg;

        if ((dispc_reg_in(VENC_VSTAT) & 0x00000010) == 0x10) {
		set_sdram_p(dm->osd0);
		set_sdram_p(dm->osd1);
		set_sdram_p(dm->vid0);
		set_sdram_p(dm->vid1);
	} else {
		++dm->vsync_cnt;
		wake_up_interruptible(&dm->vsync_wait);
  	}
	return IRQ_HANDLED;
}

/* Wait for a vsync interrupt.  This routine sleeps so it can only be called
 * from process context.
 */
static int davincifb_wait_for_vsync(vpbe_dm_win_info_t * w)
{
	vpbe_dm_info_t *dm = w->dm;
	wait_queue_t wq;
	unsigned int cnt;
	int ret;

	init_waitqueue_entry(&wq, current);

	cnt = dm->vsync_cnt;
	ret = wait_event_interruptible_timeout(dm->vsync_wait,cnt != dm->vsync_cnt,dm->timeout);
	if (ret < 0) {
		dev_err(dm->dev, "Exited function %s, code %i \n", __FUNCTION__,ret);
		return (ret);
	}
	if ((ret == 0) && (cnt==dm->vsync_cnt)) {
		dev_err(dm->dev, "Exited function %s, timeout=%i\n", __FUNCTION__,dm->timeout);
		return (-ETIMEDOUT);
	}
	return 0;
}

#ifdef RESIZER
static irqreturn_t davincifb_resizer_isr(int irq, void *arg)
{
	vpbe_dm_info_t *dm = (vpbe_dm_info_t *)arg;
	++dm->resizer_cnt;
	wake_up_interruptible(&dm->resizer_wait);
	return IRQ_HANDLED;
}
static int resizer_wait_for_not_busy(struct vpbe_dm_win_info *w)
{
	vpbe_dm_info_t *dm = w->dm;
	wait_queue_t wq;
	unsigned long cnt;
	int ret;


	cnt = dm->resizer_cnt;
	if (dispc_reg_in(RSZ_PCR) & 2) {
		init_waitqueue_entry(&wq, current);
		ret = wait_event_interruptible_timeout(dm->resizer_wait,cnt != dm->resizer_cnt,dm->timeout);
		if (ret < 0) return (ret);
		if ((ret == 0) && (cnt == dm->resizer_cnt)) return (-ETIMEDOUT);
	}
	return (0);
}
static int davinci_resizer(struct vpfe_resizer_params* prsz,struct vpbe_dm_win_info *w)
{
	int ret = resizer_wait_for_not_busy(w);
	if (ret<0) return ret;

	{
		unsigned int reg = RSZ_RSZ_CNT;
		unsigned int regStop = RSZ_YENH;
		u_int32_t* p = &prsz->rsz_cnt;
		unsigned int start = prsz->sdr_outadd;
		if ( (start >= w->fb_base_phys)&&(start < (w->fb_base_phys+w->fb_size)) ) {
			int ret;
			down(&resizer_sem);
			while (reg<=regStop) {
				dispc_reg_out(reg,*p++);
				reg+=4;
			}
			dispc_reg_out(RSZ_PCR,1);	//enable resizer
			ret = resizer_wait_for_not_busy(w);
			up(&resizer_sem);
			return ret;
		}
	}
	return -EINVAL;
}
#endif

/* Sets a uniform attribute value over a rectangular area on the attribute
 * window. The attribute value (0 to 7) is passed through the fb_fillrect's
 * color parameter.
 */
static int davincifb_set_attr_blend(struct fb_fillrect *r)
{
	struct fb_info *info = &dm->osd1->info;
	struct fb_var_screeninfo *var = &dm->osd1->info.var;
	unsigned long start = 0;
	u8 blend;
	u32 width_bytes;

	if (r->dx + r->width > var->xres_virtual)
		return -EINVAL;
	if (r->dy + r->height > var->yres_virtual)
		return -EINVAL;
	if (r->color < 0 || r->color > 7)
		return -EINVAL;

	/* since bits_per_pixel = 4, this will truncate the width if it is 
	 * not even. Similarly r->dx will be rounded down to an even pixel.
	 * ... Do we want to return an error otherwise?
	 */
	width_bytes = r->width * var->bits_per_pixel / 8;
	start = dm->osd1->fb_base + r->dy * info->fix.line_length
	    + r->dx * var->bits_per_pixel / 8;

	blend = (((u8) r->color & 0xf) << 4) | ((u8) r->color);
	while (r->height--) {
		start += info->fix.line_length;
		memset((void *)start, blend, width_bytes);
	}

	return 0;
}

static struct completion dma_completion ;
static void davincifb_dma_callback(int lch, unsigned short ch_status, void *data)
{
	complete(&dma_completion);
}

unsigned long* gDavinciIRam;
int g_davinci_iram_phys;
/*
 * Fill an on-screen window. 
 */
static int davincifb_fill_rect(struct fb_info *info, struct fb_fillrect *r)
{
	struct fb_var_screeninfo *var = &info->var;
	edmacc_paramentry_regs regs ;
	int rval ;
	unsigned long color = 0 ;

	if (r->dx + r->width > var->xres_virtual)
		return -EINVAL;
	if (r->dy + r->height > var->yres_virtual)
		return -EINVAL;

	regs.opt = 0x0010020D | dmatcc ;    // OPT:  transfer complete int enable, static, AB-synchronized, fifo 32-bits
	regs.src = g_davinci_iram_phys;
	regs.src_dst_bidx = (info->fix.line_length<<16) | 4 ;
	regs.link_bcntrld = 0x0000ffff ; // BCNT Reload 0, LINK invalid
	regs.src_dst_cidx = 0 ;
	regs.ccnt = 1 ;

	switch( var->bits_per_pixel ){
		case 4: {
			color = (r->color&0x0f);
			color |= (color<<4);
			color |= (color<<8);
			color |= (color<<16);
			regs.a_b_cnt = ( r->height << 16 ) | (r->width >> 1);
			regs.dst = info->fix.smem_start + (r->dy*info->fix.line_length) + (r->dx >> 1);
			break;
		}
		case 16: {
			color = (r->color&0xFFFF);
			color |= (color<<16);
			regs.a_b_cnt = ( r->height << 16 ) | (r->width << 1);
			regs.dst = info->fix.smem_start + (r->dy*info->fix.line_length) + (r->dx << 1);
			break;
		}
		default:
			printk( KERN_ERR "%s: unsupported bit depth %d\n", __FUNCTION__, var->bits_per_pixel );
        }

	*gDavinciIRam = color ;

//	printk( KERN_ERR  "%s: allocated dma channel %d\n", __FUNCTION__, dmach );
	davinci_set_dma_params(dmach, &regs);
	rval = davinci_start_dma(dmach);
	wait_for_completion_interruptible(&dma_completion);

	return rval ;
}

/* These position parameters are given through fb_var_screeninfo.
 * xp = var.reserved[0], yp = var.reserved[1],
 * xl = var.xres, yl = var.yres
 */
void set_win_position(char *id, u32 xp, u32 yp, u32 xl, u32 yl)
{
	int i = 0;

	if (is_win(id, VID0)) {
		i = 0;
	} else if (is_win(id, VID1)) {
		i = 1;
	} else if (is_win(id, OSD0)) {
		i = 2;
	} else if (is_win(id, OSD1)) {
		i = 3;
	}

	dispc_reg_out(OSD_WINXP(i), xp);
	dispc_reg_out(OSD_WINYP(i), yp);
	dispc_reg_out(OSD_WINXL(i), xl);
	dispc_reg_out(OSD_WINYL(i), yl);
}

static inline void
get_win_position(vpbe_dm_win_info_t * w, u32 * xp, u32 * yp, u32 * xl, u32 * yl)
{
	struct fb_var_screeninfo *v = &w->info.var;

	*xp = x_pos(w);
	*yp = y_pos(w);
	*xl = v->xres;
	*yl = v->yres;
}

/* Returns 1 if the window parameters are within VID0, 0 otherwise */
int within_vid0_limits(u32 xp, u32 yp, u32 xl, u32 yl)
{
	u32 vid0_xp = 0, vid0_yp = 0, vid0_xl = 0, vid0_yl = 0;
	if (!dm->vid0)
		return 1;
	get_win_position(dm->vid0, &vid0_xp, &vid0_yp, &vid0_xl, &vid0_yl);

	printk("osd: %i,%i,%i,%i vid0: %i,%i,%i,%i\n",xp,yp,xl,yl, vid0_xp,vid0_yp,vid0_xl,vid0_yl);
	if ((xp >= vid0_xp) && (yp >= vid0_yp) && (xp + xl <= vid0_xp + vid0_xl)
	    && (yp + yl <= vid0_yp + vid0_yl))
		return 1;
	return 0;
}

/* VID0 must be large enough to hold all other windows */
static int check_new_vid0_size(u32 xp0, u32 yp0, u32 xl0, u32 yl0)
{
	u32 _xp = 0, _yp = 0, _xl = 0, _yl = 0;
#define WITHIN_LIMITS 				\
	((_xp >= xp0) && (_yp >= yp0) &&	\
	(_xp + _xl <= xp0 + xl0) && (_yp + _yl <= yp0 + yl0))

	if (dm->osd0) {
		get_win_position(dm->osd0, &_xp, &_yp, &_xl, &_yl);
		if (!WITHIN_LIMITS) {
			dev_err(dm->dev, "Exited function %s \n", __FUNCTION__);
			return -EINVAL;
		}
	}
	if (dm->osd1) {
		get_win_position(dm->osd1, &_xp, &_yp, &_xl, &_yl);
		if (!WITHIN_LIMITS) {
			dev_err(dm->dev, "Exited function %s \n", __FUNCTION__);
			return -EINVAL;
		}
	}
	if (dm->vid1) {
		get_win_position(dm->vid1, &_xp, &_yp, &_xl, &_yl);
		if (!WITHIN_LIMITS) {
			dev_err(dm->dev, "Exited function %s \n", __FUNCTION__);
			return -EINVAL;
		}
	}
	return 0;

#undef WITHIN_LIMITS
}

/*
 * ======== davincifb_check_var ========
 */
/**
 *      davincifb_check_var - Validates a var passed in.
 *      @var: frame buffer variable screen structure
 *      @info: frame buffer structure that represents a single frame buffer
 *
 *	Checks to see if the hardware supports the state requested by
 *	var passed in. This function does not alter the hardware state!!!
 *	This means the data stored in struct fb_info and struct xxx_par do
 *      not change. This includes the var inside of struct fb_info.
 *	Do NOT change these. This function can be called on its own if we
 *	intent to only test a mode and not actually set it.
 *	If the var passed in is slightly off by what the hardware can support
 *	then we alter the var PASSED in to what we can do.
 *
 *	Returns negative errno on error, or zero on success.
 */
static int
davincifb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	vpbe_dm_win_info_t *w = (vpbe_dm_win_info_t *) info->par;
	struct fb_var_screeninfo v;

/* Rules:
 * 1) Vid1, OSD0, OSD1 and Cursor must be fully contained inside of Vid0.
 * 2) Vid0 and Vid1 are both set to accept YUV 4:2:2 (for now).
 * 3) OSD window data is always packed into 32-bit words and left justified.
 * 4) Each horizontal line of window data must be a multiple of 32 bytes.
 *    32 bytes = 32 bytes / 2 bytes per pixel = 16 pixels.
 *    This implies that 'xres' must be a multiple of 32 bytes.
 * 5) The offset registers hold the distance between the start of one line and 
 *    the start of the next. This offset value must be a multiple of 32 bytes.
 *    This implies that 'xres_virtual' is also a multiple of 32 bytes. Note 
 *    that 'xoffset' needn't be a multiple of 32 bytes.
 * 6) OSD0 is set to accept RGB565.
 * 	dispc_reg_merge(OSD_OSDWIN0ND, OSD_OSDWIN0ND_RGB0E, OSD_OSDWIN0ND_RGB0E)
 * 7) OSD1 is set to be the attribute window.
 * 8) Vid1 startX = Vid0 startX + N * 16 pixels (32 bytes)
 * 9) Vid1 width = (16*N - 8) pixels
 * 10) both the VID window can't be RGB888
 */
	memcpy(&v, var, sizeof(v));
	/* do board-specific checks on the var */
	if (davincifb_venc_check_mode(w, &v)) {
		dev_err(dm->dev, "Exited function %s \n", __FUNCTION__);
		return -EINVAL;
	}
	v.xres_virtual = (v.bits_per_pixel>=16)? round16(v.xres) : round64(v.xres);

	if (v.xres_virtual < v.xres || v.yres_virtual < v.yres) {
		dev_err(dm->dev, "Exited function %s \n", __FUNCTION__);
		return -EINVAL;
	}
	if (v.xoffset > v.xres_virtual - v.xres) {
		dev_err(dm->dev, "Exited function %s \n", __FUNCTION__);
		return -EINVAL;
	}
	if (v.yoffset > v.yres_virtual - v.yres) {
		dev_err(dm->dev, "Exited function %s \n", __FUNCTION__);
		return -EINVAL;
	}

	if (!is_win(info->fix.id, VID0)) {
		/* Rule 1 */
		if (!within_vid0_limits(x_pos(w), y_pos(w), v.xres, v.yres)) {
			dev_err(dm->dev, "%s :within_vid0_limits fail. \n",
				__FUNCTION__);
			return -EINVAL;
		}

	}
	if (is_win(info->fix.id, VID0)) {
		if (check_new_vid0_size(x_pos(w), y_pos(w), v.xres, v.yres)) {
			dev_err(dm->dev, "%s :chack_new_vid0_size fail\n",
				__FUNCTION__);
			return -EINVAL;
		}
		/* Rule 10 */
		if ((dm->vid1->info.var.bits_per_pixel == 24)
		    && (v.bits_per_pixel == 24)) {
			dev_err(dm->dev, "%s :Rule violation: both VID can't be\
			 RGB888.\n", __FUNCTION__);
			return -EINVAL;
		}
	} else if (is_win(info->fix.id, VID1)) {
		/* Video1 may be in YUV or RGB888 format */
		if ((v.bits_per_pixel != 16) && (v.bits_per_pixel != 24)) {
			dev_err(dm->dev, "Exited function %s \n", __FUNCTION__);
			return -EINVAL;
		}
		/* Rule 10 */
		if ((dm->vid0->info.var.bits_per_pixel == 24)
		    && (v.bits_per_pixel == 24)) {
			dev_err(dm->dev,
				"%s :Rule violation: both VID can't be RGB888.\n",
				__FUNCTION__);
			return -EINVAL;
		}
	} 
        v.height = v.yres_virtual ;
        v.width = v.xres_virtual ;
	memcpy(var, &v, sizeof(v));
	info->fix.line_length = ((info->var.xres_virtual * info->var.bits_per_pixel + (32*8-1)) >> (3+5)) << 5;
	return 0;
}

/* Interlaced = Frame mode, Non-interlaced = Field mode */
void set_interlaced(char *id, unsigned int on)
{
	on = (on == 0) ? 0 : ~0;

	if (is_win(id, VID0))
		dispc_reg_merge(OSD_VIDWINMD, on, OSD_VIDWINMD_VFF0);
	else if (is_win(id, VID1))
		dispc_reg_merge(OSD_VIDWINMD, on, OSD_VIDWINMD_VFF1);
	else if (is_win(id, OSD0))
		dispc_reg_merge(OSD_OSDWIN0MD, on, OSD_OSDWIN0MD_OFF0);
	else if (is_win(id, OSD1))
		dispc_reg_merge(OSD_OSDWIN1MD, on, OSD_OSDWIN1MD_OFF1);
}

/* For zooming, we just have to set the start of framebuffer, the zoom factors 
 * and the display size. The hardware will then read only 
 * (display size / zoom factor) area of the framebuffer and  zoom and 
 * display it. In the following function, we assume that the start of 
 * framebuffer and the display size parameters are set already.
 */
static void set_zoom(int WinID, int h_factor, int v_factor)
{
	switch (WinID) {
	case VID0:
		dispc_reg_merge(OSD_VIDWINMD,
				h_factor << OSD_VIDWINMD_VHZ0_SHIFT,
				OSD_VIDWINMD_VHZ0);
		dispc_reg_merge(OSD_VIDWINMD,
				v_factor << OSD_VIDWINMD_VVZ0_SHIFT,
				OSD_VIDWINMD_VVZ0);
		break;
	case VID1:
		dispc_reg_merge(OSD_VIDWINMD,
				h_factor << OSD_VIDWINMD_VHZ1_SHIFT,
				OSD_VIDWINMD_VHZ1);
		dispc_reg_merge(OSD_VIDWINMD,
				v_factor << OSD_VIDWINMD_VVZ1_SHIFT,
				OSD_VIDWINMD_VVZ1);
		break;
	case OSD0:
		dispc_reg_merge(OSD_OSDWIN0MD,
				h_factor << OSD_OSDWIN0MD_OHZ0_SHIFT,
				OSD_OSDWIN0MD_OHZ0);
		dispc_reg_merge(OSD_OSDWIN0MD,
				v_factor << OSD_OSDWIN0MD_OVZ0_SHIFT,
				OSD_OSDWIN0MD_OVZ0);
		break;
	case OSD1:
		dispc_reg_merge(OSD_OSDWIN1MD,
				h_factor << OSD_OSDWIN1MD_OHZ1_SHIFT,
				OSD_OSDWIN1MD_OHZ1);
		dispc_reg_merge(OSD_OSDWIN1MD,
				v_factor << OSD_OSDWIN1MD_OVZ1_SHIFT,
				OSD_OSDWIN1MD_OVZ1);
		break;
	}
}

/* Chooses the ROM CLUT for now. Can be extended later. */
void set_bg_color(u8 clut, u8 color_offset)
{
	clut = 0;		/* 0 = ROM, 1 = RAM */

	dispc_reg_merge(OSD_MODE, OSD_MODE_BCLUT & clut, OSD_MODE_BCLUT);
	dispc_reg_merge(OSD_MODE, color_offset << OSD_MODE_CABG_SHIFT,
			OSD_MODE_CABG);
}

static void set_sdram_params(char *id, u32 addr, u32 line_length)
{

	/* The parameters to be written to the registers should be in 
	 * multiple of 32 bytes
	 */

	addr = addr;		/* div by 32 */
	line_length = line_length / 32;

//	printk(KERN_INFO "ofst %i\n",line_length);
	if (is_win(id, VID0)) {
		dispc_reg_out(OSD_VIDWIN0ADR, addr);
		dispc_reg_out(OSD_VIDWIN0OFST, line_length);
	} else if (is_win(id, VID1)) {
		dispc_reg_out(OSD_VIDWIN1ADR, addr);
		dispc_reg_out(OSD_VIDWIN1OFST, line_length);
	} else if (is_win(id, OSD0)) {
		dispc_reg_out(OSD_OSDWIN0ADR, addr);
		dispc_reg_out(OSD_OSDWIN0OFST, line_length);
	} else if (is_win(id, OSD1)) {
		dispc_reg_out(OSD_OSDWIN1ADR, addr);
		dispc_reg_out(OSD_OSDWIN1OFST, line_length);
	}
}

void set_win_enable(char *id, unsigned int on)
{
	on = (on == 0) ? 0 : ~0;

	if (is_win(id, VID0)) {
		if (dm->videomode.vmode == FB_VMODE_INTERLACED)
			/* Turning off VID0 use due to field inversion issue */
			dispc_reg_merge(OSD_VIDWINMD, 0, OSD_VIDWINMD_ACT0);
		else
			dispc_reg_merge(OSD_VIDWINMD, on, OSD_VIDWINMD_ACT0);
	} else if (is_win(id, VID1))
		dispc_reg_merge(OSD_VIDWINMD, on, OSD_VIDWINMD_ACT1);
	else if (is_win(id, OSD0))
		dispc_reg_merge(OSD_OSDWIN0MD, on, OSD_OSDWIN0MD_OACT0);
	else if (is_win(id, OSD1)) {
		/* The OACT1 bit is applicable only if OSD1 is not used as 
		 * the attribute window
		 */
		if (!(dispc_reg_in(OSD_OSDWIN1MD) & OSD_OSDWIN1MD_OASW))
			dispc_reg_merge(OSD_OSDWIN1MD, on, OSD_OSDWIN1MD_OACT1);
	}
}

static void set_win_mode(char *id)
{
	if (is_win(id, VID0)) {
		if (dm->vid0->info.var.bits_per_pixel == 16) {
			/* disable RGB888 format */
			dispc_reg_merge(OSD_MISCCT,
					VPBE_DISABLE <<
					OSD_MISCCT_RGBEN_SHIFT,
					OSD_MISCCT_RGBEN);
		}
		if (dm->vid0->info.var.bits_per_pixel == 24) {
			/* set RGB888 format */
			dispc_reg_merge(OSD_MISCCT,
					SET_0 << OSD_MISCCT_RGBWIN_SHIFT,
					OSD_MISCCT_RGBWIN);
			dispc_reg_merge(OSD_MISCCT,
					VPBE_ENABLE <<
					OSD_MISCCT_RGBEN_SHIFT,
					OSD_MISCCT_RGBEN);
		}
	}

	if (is_win(id, VID1)) {
		if (dm->vid1->info.var.bits_per_pixel == 16) {
			/* disable RGB888 format */
			dispc_reg_merge(OSD_MISCCT,
					VPBE_DISABLE <<
					OSD_MISCCT_RGBEN_SHIFT,
					OSD_MISCCT_RGBEN);
		}
		if (dm->vid1->info.var.bits_per_pixel == 24) {
			/* set RGB888 format */
			dispc_reg_merge(OSD_MISCCT,
					SET_1 << OSD_MISCCT_RGBWIN_SHIFT,
					OSD_MISCCT_RGBWIN);
			dispc_reg_merge(OSD_MISCCT,
					VPBE_ENABLE <<
					OSD_MISCCT_RGBEN_SHIFT,
					OSD_MISCCT_RGBEN);
		}
	}

	if ((is_win(id, OSD0))) {
		if (dm->osd0->info.var.bits_per_pixel == 16) {
			/* set RGB565 format */
			dispc_reg_merge(OSD_OSDWIN0MD,
					1 << OSD_OSDWIN0MD_RGB0E_SHIFT,
					OSD_OSDWIN0MD_RGB0E);
		} else {
			dispc_reg_merge(OSD_OSDWIN0MD,
					0 << OSD_OSDWIN0MD_RGB0E_SHIFT,
					OSD_OSDWIN0MD_RGB0E);
		}
		/* Set bits per pixel */
		if (dm->osd0->info.var.bits_per_pixel == 8) {
			dispc_reg_merge(OSD_OSDWIN0MD,
					3 << OSD_OSDWIN0MD_BMW0_SHIFT,
					OSD_OSDWIN0MD_BMW0);
		} else if (dm->osd0->info.var.bits_per_pixel == 4) {
			dispc_reg_merge(OSD_OSDWIN0MD,
					2 << OSD_OSDWIN0MD_BMW0_SHIFT,
					OSD_OSDWIN0MD_BMW0);
		} else if (dm->osd0->info.var.bits_per_pixel == 2) {
			dispc_reg_merge(OSD_OSDWIN0MD,
					1 << OSD_OSDWIN0MD_BMW0_SHIFT,
					OSD_OSDWIN0MD_BMW0);
		} else if (dm->osd0->info.var.bits_per_pixel == 1) {
			dispc_reg_merge(OSD_OSDWIN0MD,
					0 << OSD_OSDWIN0MD_BMW0_SHIFT,
					OSD_OSDWIN0MD_BMW0);
		}
		dispc_reg_merge(OSD_OSDWIN0MD,
				(dm->osd0->conf_params.bitmap_params.blend_info.bf) << OSD_OSDWIN0MD_BLND0_SHIFT,
				OSD_OSDWIN0MD_BLND0);
	}
	if ((is_win(id, OSD1))) {
		if (dm->osd1->conf_params.bitmap_params.enable_attribute) {
			dispc_reg_merge(OSD_OSDWIN1MD, OSD_OSDWIN1MD_OASW,
					OSD_OSDWIN1MD_OASW);
			dispc_reg_merge(OSD_OSDWIN1MD,
					2 << OSD_OSDWIN1MD_BMW1_SHIFT,
					OSD_OSDWIN1MD_BMW1);
			dispc_reg_merge(OSD_OSDWIN1MD, 0x00,
					OSD_OSDWIN1MD_OACT1);
		} else {
			dispc_reg_merge(OSD_OSDWIN1MD, 0x00,
					OSD_OSDWIN1MD_OASW);
			if (dm->osd1->info.var.bits_per_pixel == 16)
				/* set RGB565 format */
				dispc_reg_merge(OSD_OSDWIN1MD,
						1 <<
						OSD_OSDWIN1MD_RGB1E_SHIFT,
						OSD_OSDWIN1MD_RGB1E);
			else {
				dispc_reg_merge(OSD_OSDWIN1MD,
						0 <<
						OSD_OSDWIN1MD_RGB1E_SHIFT,
						OSD_OSDWIN1MD_RGB1E);
			}
			if (dm->osd1->info.var.bits_per_pixel == 8) {
				dispc_reg_merge(OSD_OSDWIN1MD,
						3 <<
						OSD_OSDWIN1MD_BMW1_SHIFT,
						OSD_OSDWIN1MD_BMW1);
			} else if (dm->osd1->info.var.bits_per_pixel == 4) {
				dispc_reg_merge(OSD_OSDWIN1MD,
						2 <<
						OSD_OSDWIN1MD_BMW1_SHIFT,
						OSD_OSDWIN1MD_BMW1);
			} else if (dm->osd1->info.var.bits_per_pixel == 2) {
				dispc_reg_merge(OSD_OSDWIN1MD,
						1 <<
						OSD_OSDWIN1MD_BMW1_SHIFT,
						OSD_OSDWIN1MD_BMW1);
			} else if (dm->osd1->info.var.bits_per_pixel == 1) {
				dispc_reg_merge(OSD_OSDWIN1MD,
						0 <<
						OSD_OSDWIN1MD_BMW1_SHIFT,
						OSD_OSDWIN1MD_BMW1);
			}
			dispc_reg_merge(OSD_OSDWIN1MD,
					(dm->osd1->conf_params.bitmap_params.blend_info.bf) << OSD_OSDWIN1MD_BLND1_SHIFT,
					OSD_OSDWIN1MD_BLND1);
//			dispc_reg_merge(OSD_OSDWIN1MD, ~0, OSD_OSDWIN1MD_OACT1);
		}
	}
}

static int davincifb_set_par(struct fb_info *info)
{
	struct vpbe_dm_win_info *w = (struct vpbe_dm_win_info *)info->par;
	u32 start = 0, offset = 0;
	int shift = (dm->videomode.vmode == FB_VMODE_INTERLACED) ? 1 : 0;
//ensure this is a multiple of 32 bytes
//	printk(KERN_INFO "set_par %x %x\n",info->var.xres_virtual,info->var.xres);
//	memset(info->pseudo_palette, 0x0, sizeof(int) * 17);

	/* First set everything in 'dm' */
	if (is_win(info->fix.id, VID0)) {
		dm->vid0->info = *info;
	}
	if (is_win(info->fix.id, VID1)) {
		dm->vid1->info = *info;
	}
	if (is_win(info->fix.id, OSD0)) {
		dm->osd0->info = *info;
	}
	if (is_win(info->fix.id, OSD1)) {
		dm->osd1->info = *info;
	}

	if (0) {
		//the line below is a bug that fixes the TI demo ap bug
		info->var.xres_virtual = (info->var.bits_per_pixel>=16)? round16(info->var.xres) : round64(info->var.xres);
		info->fix.line_length = ((info->var.xres_virtual * info->var.bits_per_pixel + (32*8-1)) >> (3+5)) << 5;
	}
	offset = info->var.yoffset * info->fix.line_length + ((info->var.xoffset * info->var.bits_per_pixel)>>3);
	start = (u32) w->fb_base_phys + offset;
	set_sdram_params(info->fix.id, start, info->fix.line_length);

	set_interlaced(info->fix.id, dm->videomode.vmode);
	set_win_position(info->fix.id, x_pos(w), y_pos(w) >> shift, info->var.xres, info->var.yres >> shift);
	set_win_mode(info->fix.id);
	return 0;
}

static int
davincifb_sync(struct fb_info *info)
{
	vpbe_dm_win_info_t *w = (vpbe_dm_win_info_t *) info->par ;
//	consistent_sync( (void *)w->fb_base, w->fb_size, DMA_TO_DEVICE );
	dma_cache_maint( (void *)w->fb_base, w->fb_size, DMA_TO_DEVICE );
//	printk( KERN_ERR "%s completed\n", __FUNCTION__ );
	return 0 ;
}

static int
davincifb_set_start( struct fb_set_start *set, struct fb_info *info )
{
	struct vpbe_dm_win_info *win   = (struct vpbe_dm_win_info *) info->par;
	unsigned long            start = 0;

	/* Physical mode (absolute address)? */
	if (set->offset < 0) {
		start = set->physical;

		/* FIXME: address checks */
	}
	else {
		/* Offset mode (from frame buffer device base). */
		if (set->offset + info->var.yres * info->fix.line_length >= win->fb_size)
			return -EFAULT;

		start = win->fb_base_phys + set->offset;
	}

	/* Set on explicit sync count? */
	if (set->sync > 1) {
		if (set->sync <= dm->vsync_cnt) {
			set_sdram_params( info->fix.id, start, info->fix.line_length );
			win->sdram_address = start;

			set->sync = dm->vsync_cnt;
		}
		else {
			/* FIXME: No queue yet. */
			win->sdram_address = start;

			set->sync = 0;
		}
	}
	/* Set on next sync? */
	else {
           if (set->sync)
              davincifb_wait_for_vsync(win);
           set_sdram_params( info->fix.id, start, info->fix.line_length );
           set->sync = dm->vsync_cnt;
	}

	return 0;
}


/*
 * davincifb_ioctl - handler for private ioctls.
 */
static int
davincifb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	struct vpbe_dm_win_info *w = (struct vpbe_dm_win_info *)info->par;
	void __user *argp = (void __user *)arg;
	struct fb_fillrect rect;
	struct fb_set_start set_start;
	zoom_params_t zoom;
	int retval = 0;
	long std = 0;

	char *win_id = w->info.fix.id;
	vpbe_bitmap_blend_params_t blend_para;
	vpbe_blink_option_t blink_option;
	vpbe_video_config_params_t vid_conf_params;
	vpbe_bitmap_config_params_t bitmap_conf_params;
	vpbe_dclk_t dclk;
	vpbe_backg_color_t backg_color;
	struct vpbe_mode_info mode_info;
	struct vpbe_fb_videomode vmode;
	struct vpbe_window_position win_pos;
	struct fb_cursor cursor;

	if (!w->fb_base) {
		dev_err(dm->dev, "ERROR: window is not enabled while inserting\
			 the module.\n");
		return -EINVAL;
	}
	switch (cmd) {
	case FBIO_WAITFORVSYNC:
		/* This ioctl   accepts an integer argument     to specify a
		 * display.      We     only support one display,       so we   will
		 * simply       ignore the argument.
		 */
		return davincifb_wait_for_vsync(w);
		break;
	case FBIO_SETATTRIBUTE:
		if (copy_from_user(&rect, argp, sizeof(rect)))
			return -EFAULT;
		return davincifb_set_attr_blend(&rect);
		break;
	case FBIO_SETPOS:
		if (copy_from_user(&win_pos, argp, sizeof(win_pos)))
			return -EFAULT;
		if (win_pos.xpos >= 0 && win_pos.xpos <= info->var.xres) {
			w->win_pos.xpos = win_pos.xpos;
			if (davincifb_check_var(&w->info.var, &w->info) != 0)
				return -EFAULT;
			if (davincifb_set_par(&w->info) != 0)
				return -EFAULT;
		} else
			return -EINVAL;
		if (win_pos.ypos >= 0 && win_pos.ypos <= info->var.yres) {
			w->win_pos.ypos = win_pos.ypos;
			if (davincifb_check_var(&w->info.var, &w->info) != 0)
				return -EFAULT;
			if (davincifb_set_par(&w->info) != 0)
				return -EFAULT;
		} else
			return -EINVAL;
		break;
	case FBIO_SETZOOM:
		if (copy_from_user(&zoom, argp, sizeof(zoom)))
			return -EFAULT;
		if (((zoom.zoom_h == 2) ||
		     (zoom.zoom_h == 0) ||
		     (zoom.zoom_h == 1)) &&
		    ((zoom.zoom_v == 2) || (zoom.zoom_v == 0)
		     || (zoom.zoom_v == 1))) {
			set_zoom(zoom.window_id, zoom.zoom_h, zoom.zoom_v);
			return 0;
		} else {
			return -EINVAL;
		}
		break;
	case FBIO_SET_START:
		if (copy_from_user(&set_start, argp, sizeof(set_start)))
			return -EFAULT;
		retval = davincifb_set_start( &set_start, &w->info );
		if (retval)
			return retval;
		if (copy_to_user(argp, &set_start, sizeof(set_start)))
			return -EFAULT;
		break;
	case FBIO_ENABLE_DISABLE_WIN:
		switch (arg) {
		case 1:
			retval = vpbe_enable_window(w);
			break;
		case 0:
			retval = vpbe_disable_window(w);
			break;
		default:
			retval = -VPBE_INVALID_PARA_VALUE;
			break;
		}
		break;

	case FBIO_SET_BITMAP_BLEND_FACTOR:
		if ((retval =
		     copy_from_user(&blend_para, argp, sizeof(blend_para))) < 0)
			return retval;
		if ((retval =
		     vpbe_bitmap_set_blend_factor(win_id, &blend_para)) < 0)
			return retval;
		break;

	case FBIO_SET_BITMAP_WIN_RAM_CLUT:
		if ((retval =
		     copy_from_user(dm->ram_clut, argp, RAM_CLUT_SIZE)) < 0)
			return retval;
		vpbe_bitmap_set_ram_clut();
		break;

	case FBIO_ENABLE_DISABLE_ATTRIBUTE_WIN:
		if ((retval = vpbe_enable_disable_attribute_window(arg)) < 0)
			return retval;
		break;

	case FBIO_GET_BLINK_INTERVAL:
		if ((retval = vpbe_get_blinking(win_id, &blink_option)) < 0)
			return retval;
		if ((retval =
		     copy_to_user(argp, &blink_option,
				  sizeof(blink_option))) < 0)
			return retval;
		break;

	case FBIO_SET_BLINK_INTERVAL:
		if ((retval =
		     copy_from_user(&blink_option, argp,
				    sizeof(blink_option))) < 0)
			return retval;
		if ((retval = vpbe_set_blinking(win_id, &blink_option)) < 0)
			return retval;
		break;

	case FBIO_GET_VIDEO_CONFIG_PARAMS:
		if ((retval =
		     vpbe_get_vid_params(win_id, &vid_conf_params)) < 0)
			return retval;
		if ((retval =
		     copy_to_user(argp, &vid_conf_params,
				  sizeof(vid_conf_params))) < 0)
			return retval;
		break;

	case FBIO_SET_VIDEO_CONFIG_PARAMS:
		if ((retval =
		     copy_from_user(&vid_conf_params, argp,
				    sizeof(vid_conf_params))) < 0)
			return retval;
		if ((retval =
		     vpbe_set_vid_params(win_id, &vid_conf_params)) < 0)
			return retval;
		break;

	case FBIO_GET_BITMAP_CONFIG_PARAMS:
		if ((retval =
		     vpbe_bitmap_get_params(win_id, &bitmap_conf_params)) < 0)
			return retval;
		if ((retval =
		     copy_to_user(argp, &bitmap_conf_params,
				  sizeof(bitmap_conf_params))) < 0)
			return retval;
		break;

	case FBIO_SET_BITMAP_CONFIG_PARAMS:
		if ((retval =
		     copy_from_user(&bitmap_conf_params, argp,
				    sizeof(bitmap_conf_params))) < 0)
			return retval;
		if ((retval =
		     vpbe_bitmap_set_params(win_id, &bitmap_conf_params)) < 0)
			return retval;
		break;

	case FBIO_SET_DCLK:
		if (!is_win(win_id, VID0))
			return -EINVAL;
		if ((retval = copy_from_user(&dclk, argp, sizeof(dclk))) < 0)
			return retval;
		if ((retval = vpbe_set_dclk(&dclk)) < 0)
			return retval;
		break;

	case FBIO_SET_INTERFACE:
		if ((retval = vpbe_set_interface(arg)) < 0)
			return retval;
		break;

	case FBIO_GET_INTERFACE:
		if ((retval =
		     copy_to_user(argp, &dm->display.interface,
				  sizeof(dm->display.interface))) < 0)
			return retval;
		break;

	case FBIO_QUERY_TIMING:
		if ((retval =
		     copy_from_user(&mode_info, argp, sizeof(mode_info))) < 0)
			return retval;
		if ((retval = vpbe_query_mode(&mode_info)) < 0)
			return retval;
		if ((retval =
		     copy_to_user(argp, &mode_info, sizeof(mode_info))) < 0)
			return retval;
		break;

	case FBIO_SET_TIMING:
		if ((retval = copy_from_user(&vmode, argp, sizeof(vmode))) < 0)
			return retval;
		if ((retval = vpbe_set_mode(&vmode)) < 0)
			return retval;
		break;

	case FBIO_GET_TIMING:
		if ((retval =
		     copy_to_user(argp, &dm->videomode,
				  sizeof(dm->videomode))) < 0)
			return retval;
		break;

	case FBIO_SET_VENC_CLK_SOURCE:
		if (!is_win(win_id, VID0))
			return -EINVAL;
		if ((retval = vpbe_set_venc_clk_source(arg)) < 0)
			return retval;
		break;

	case FBIO_SET_BACKG_COLOR:
		if ((retval =
		     copy_from_user(&backg_color, argp,
				    sizeof(backg_color))) < 0)
			return retval;
		if ((retval = vpbe_set_backg_color(&backg_color)) < 0)
			return retval;
		break;

	case FBIO_ENABLE_DISPLAY:
		if (arg) {
			if ((davincifb_check_var
			     (&dm->vid0->info.var, &dm->vid0->info)) < 0) {
				dev_err(dm->dev, "Exited function %s \n",
					__FUNCTION__);
				return -EINVAL;
			}
			if ((davincifb_check_var
			     (&dm->vid1->info.var, &dm->vid1->info)) < 0) {
				dev_err(dm->dev, "Exited function %s \n",
					__FUNCTION__);
				return -EINVAL;
			}
			if ((davincifb_check_var
			     (&dm->osd0->info.var, &dm->osd0->info)) < 0) {
				dev_err(dm->dev, "Exited function %s \n",
					__FUNCTION__);
				return -EINVAL;
			}
			if ((davincifb_check_var
			     (&dm->osd1->info.var, &dm->osd1->info)) < 0) {
				dev_err(dm->dev, "Exited function %s \n",
					__FUNCTION__);
				return -EINVAL;
			}
			dm->display_enable = 1;
		} else {
			dm->display_enable = 0;
		}

		/* While enabling, enable VENC before DACs
		   while disabling, disable DACS before VENC 
		   to minimize corruption */
		if (arg) 
			vpbe_enable_venc(arg);

		if (dm->display.interface == PRGB)
			vpbe_enable_lcd(arg);
		else if (dm->display.interface == SVIDEO ||
			 dm->display.interface == COMPONENT ||
			 dm->display.interface == COMPOSITE)
			vpbe_enable_dacs(arg);
		
		if (!arg)
			vpbe_enable_venc(arg);
		break;

	case FBIO_SET_CURSOR:
		if (copy_from_user(&cursor, argp, sizeof(cursor)))
			return -EFAULT;
		if (vpbe_set_cursor_params(&cursor) < 0)
			return -EFAULT;
		break;
		/*backported IOCTLS */
	case FBIO_GETSTD:
		std = ((dm->display.mode << 16) | (dm->display.interface));
		/*(NTSC <<16) | (COPOSITE); */
		if (copy_to_user(argp, &std, sizeof(u_int32_t))) return -EFAULT;
		return 0;
		break;

	case FBIO_SETPOSX:
		if (arg >= 0 && arg <= info->var.xres) {
			w->win_pos.xpos = arg;
			if (davincifb_check_var(&w->info.var, &w->info) != 0)
				return -EFAULT;
			if (davincifb_set_par(&w->info) != 0)
				return -EFAULT;
			return 0;
		} else
			return -EINVAL;
		break;

	case FBIO_SETPOSY:
		if (arg >= 0 && arg <= info->var.yres) {
			w->win_pos.ypos = arg;
			if (davincifb_check_var(&w->info.var, &w->info) != 0)
				return -EFAULT;
			if (davincifb_set_par(&w->info) != 0)
				return -EFAULT;
			return 0;
		} else
			return -EINVAL;
		break;
#ifdef RESIZER
	case FBIO_RESIZER:
		{
			struct vpfe_resizer_params rsz;
			if (copy_from_user(&rsz, argp, sizeof(rsz))) return -EFAULT;
			return davinci_resizer(&rsz,w);
		}
#endif

	case FBIO_SYNC:
		{
			return davincifb_sync(&w->info);
		}
	case FBIO_FILLRECT:
		{
			if (copy_from_user(&rect, argp, sizeof(rect)))
				return -EFAULT;
			return davincifb_fill_rect(info,&rect);
		}
	default:
		retval = -EINVAL;
		break;
	}
	return retval;
}

/**
 *  	davincifb_setcolreg - Optional function. Sets a color register.
 *      @regno: Which register in the CLUT we are programming 
 *      @red: The red value which can be up to 16 bits wide 
 *	@green: The green value which can be up to 16 bits wide 
 *	@blue:  The blue value which can be up to 16 bits wide.
 *	@transp: If supported the alpha value which can be up to 16 bits wide.
 *      @info: frame buffer info structure
 * 
 *  	Set a single color register. The values supplied have a 16 bit
 *  	magnitude which needs to be scaled in this function for the hardware.
 *	Things to take into consideration are how many color registers, if
 *	any, are supported with the current color visual. With truecolor mode
 *	no color palettes are supported. Here a psuedo palette is created 
 *	which we store the value in pseudo_palette in struct fb_info. For
 *	pseudocolor mode we have a limited color palette. To deal with this
 *	we can program what color is displayed for a particular pixel value.
 *	DirectColor is similar in that we can program each color field. If
 *	we have a static colormap we don't need to implement this function. 
 * 
 *	Returns negative errno on error, or zero on success.
 */
static int
davincifb_setcolreg(unsigned regno, unsigned red, unsigned green,
		    unsigned blue, unsigned transp, struct fb_info *info)
{
	/* only pseudo-palette (16 bpp) allowed */
	if (regno >= 16)	/* maximum number of palette entries */
		return 1;

	if (info->var.grayscale) {
		/* grayscale = 0.30*R + 0.59*G + 0.11*B */
		red = green = blue = (red * 77 + green * 151 + blue * 28) >> 8;
	}

	/* Truecolor has hardware-independent 16-entry pseudo-palette */
	if (info->fix.visual == FB_VISUAL_TRUECOLOR) {
		u32 v;

		if (regno >= 16)
			return 1;

		red >>= (16 - info->var.red.length);
		green >>= (16 - info->var.green.length);
		blue >>= (16 - info->var.blue.length);

		v = (red << info->var.red.offset) |
		    (green << info->var.green.offset) | (blue << info->var.
							 blue.offset);

		switch (info->var.bits_per_pixel) {
		case 16:
			((u32 *)info->pseudo_palette)[regno] = v;
			break;
		default:
			return 1;
		}
		return 0;
	}
	return 0;
}

/**
 *      davincifb_pan_display - NOT a required function. Pans the display.
 *      @var: frame buffer variable screen structure
 *      @info: frame buffer structure that represents a single frame buffer
 *
 *	Pan (or wrap, depending on the `vmode' field) the display using the
 *  	`xoffset' and `yoffset' fields of the `var' structure.
 *  	If the values don't fit, return -EINVAL.
 *
 *      Returns negative errno on error, or zero on success.
 */
static int
davincifb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct vpbe_dm_win_info *w = (struct vpbe_dm_win_info *)info->par;
	u32 start = 0, offset = 0;

	if (var->xoffset > var->xres_virtual - var->xres) {
		return -EINVAL;
	}
	if (var->yoffset > var->yres_virtual - var->yres) {
		return -EINVAL;
	}
	offset = var->yoffset * info->fix.line_length +
		((var->xoffset * var->bits_per_pixel)>>3);
	start = (u32) w->fb_base_phys + offset;
	if (dm->videomode.vmode == FB_VMODE_NONINTERLACED)
		set_sdram_params(info->fix.id, start, info->fix.line_length);
	else {
		if ((dispc_reg_in(VENC_VSTAT) & 0x00000010) == 0x10)
			set_sdram_params(info->fix.id, start,
					 info->fix.line_length);
		else
			w->sdram_address = start;
	}
	return 0;
}

/**
 *      davincifb_blank - NOT a required function. Blanks the display.
 *      @blank_mode: the blank mode we want. 
 *      @info: frame buffer structure that represents a single frame buffer
 *
 *      Blank the screen if blank_mode != 0, else unblank. Return 0 if
 *      blanking succeeded, != 0 if un-/blanking failed due to e.g. a 
 *      video mode which doesn't support it. Implements VESA suspend
 *      and powerdown modes on hardware that supports disabling hsync/vsync:
 *      blank_mode == 2: suspend vsync
 *      blank_mode == 3: suspend hsync
 *      blank_mode == 4: powerdown
 *
 *      Returns negative errno on error, or zero on success.
 *
 */
int davincifb_blank(int blank_mode, struct fb_info *info)
{
   if((0==blank_mode) || (0==noblank)){
   	set_win_enable(info->fix.id, (blank_mode)? 0 :1);
   }
	return 0;
}

int parse_win_params(char *wp, int *xres, int *yres, int *xpos, int *ypos)
{
	char *s;

	if ((s = strsep(&wp, "x")) == NULL)
		return -EINVAL;
	*xres = simple_strtoul(s, NULL, 0);

	if ((s = strsep(&wp, "@")) == NULL)
		return -EINVAL;
	*yres = simple_strtoul(s, NULL, 0);

	if ((s = strsep(&wp, ",")) == NULL)
		return -EINVAL;
	*xpos = simple_strtoul(s, NULL, 0);

	if ((s = strsep(&wp, ":")) == NULL)
		return -EINVAL;
	*ypos = simple_strtoul(s, NULL, 0);

	return 0;
}

struct fb_info *init_fb_info(struct vpbe_dm_win_info *w,
			     struct fb_var_screeninfo *var, 
                             char *id,
                             const DISPLAYCFG* disp)
{
	struct fb_info *info = &(w->info);
	vpbe_dm_info_t *dm = w->dm;

	/* initialize the fb_info structure */
	info->flags = FBINFO_DEFAULT;
	info->fbops = &davincifb_ops;
	info->screen_base = (char *)(w->fb_base);
	info->pseudo_palette = w->pseudo_palette;
	info->par = w;

	/* Initialize variable screeninfo.
	 * The variable screeninfo can be directly specified by the user
	 * via an ioctl.
	 */
	memcpy(&info->var, var, sizeof(info->var));
	info->var.activate = FB_ACTIVATE_NOW;

	/* Initialize fixed screeninfo.
	 * The fixed screeninfo cannot be directly specified by the user, but
	 * it may change to reflect changes to the var info.
	 */
	strlcpy(info->fix.id, id, sizeof(info->fix.id));
	info->fix.smem_start = w->fb_base_phys;
	info->fix.smem_len = w->fb_size;
	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.visual = (info->var.bits_per_pixel <= 8) ?
	    FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;
	info->fix.xpanstep = 0;
	info->fix.ypanstep = 0;
	info->fix.ywrapstep = 0;
	info->fix.type_aux = 0;
	info->fix.mmio_start = dm->mmio_base_phys;
	info->fix.mmio_len = dm->mmio_size;
	info->fix.accel = FB_ACCEL_NONE;
	w->sdram_address = 0;

	return info;
}

DISPLAYCFG cur_display_settings;
const DISPLAYCFG* build_current_settings(void)
{
	unsigned int vmod = dispc_reg_in(VENC_VMOD);
	unsigned int totalX = dispc_reg_in(VENC_DCLKHR);
	unsigned int encPerPixel = dispc_reg_in(VENC_HINT) / (totalX-1);
	unsigned int vid_ctl = dispc_reg_in(VENC_VIDCTL);
	unsigned int sync_ctl = dispc_reg_in(VENC_SYNCCTL);
	DISPLAYCFG* disp = &cur_display_settings;

	if ((vmod & (VENC_VMOD_VMD|VENC_VMOD_VENC)) !=
		(VENC_VMOD_VMD|VENC_VMOD_VENC)) {
		*disp = stdDisplayTypes[DEF_INDEX];
		encPerPixel = 1;
		if ((disp->xRes * disp->yRes) <= (640*480)) encPerPixel = 2;
		if ((disp->xRes * disp->yRes) <= (640*240)) encPerPixel = 4;
		if ((disp->xRes * disp->yRes) <= (320*240)) encPerPixel = 8;
		disp->encPerPixel = encPerPixel;
		return disp;
	}

	disp->encPerPixel = encPerPixel;
	disp->xRes = dispc_reg_in(VENC_HVALID) / encPerPixel;
	disp->xSyncWidth = dispc_reg_in(VENC_HSPLS) / encPerPixel;
	disp->xBegin = dispc_reg_in(VENC_HSTART) / encPerPixel;
	disp->xEnd = totalX - (disp->xRes + disp->xSyncWidth + disp->xBegin);

	disp->yRes = dispc_reg_in(VENC_VVALID);
	disp->ySyncWidth = dispc_reg_in(VENC_VSPLS);
	disp->yBegin = dispc_reg_in(VENC_VSTART);
	disp->yEnd = dispc_reg_in(VENC_VINT)+ 1 - (disp->yRes + disp->ySyncWidth + disp->yBegin);

	disp->vsPol = (sync_ctl>>3)&1;
	disp->hsPol = (sync_ctl>>2)&1;
	disp->pPol = ((vid_ctl>>14)&1)^1;
	disp->oePol = (dispc_reg_in(VENC_LCDOUT)>>1)&1;

	disp->enable = 1;
	return disp;
}

static void update_var
   ( struct fb_var_screeninfo *var,
     const DISPLAYCFG         *disp )
{
   var->xres_virtual = var->xres = disp->xRes ;
   var->yres_virtual = var->yres = disp->yRes ;
   var->xres = disp->xRes ;
   var->xres = disp->xRes ;
   var->xres = disp->xRes ;
}

void vpbe_davincifb_lcd_component_config(void)
{
	const DISPLAYCFG* disp = &cur_display_settings;
	if (1) {
		int encPerPixel = disp->encPerPixel;
		unsigned short val[4];
		int bit;
		int gbit;
		dispc_reg_out(OSD_BASEPX, disp->xBegin);
		dispc_reg_out(OSD_BASEPY, disp->yBegin);

		/* Reset video encoder module */
		dispc_reg_out(VENC_VMOD, 0);
		dispc_reg_out(VPSS_CLKCTL, 0x09);	//disable DAC clock
		dispc_reg_out(VPBE_PCR, 0);		//not divided by 2
//		dispc_reg_out(VPBE_PCR, 2);		//divided by 2
		dispc_reg_out(VENC_VIDCTL,((disp->pPol^1)<<14)|(1<<13));
		dispc_reg_out(VENC_SYNCCTL,(disp->vsPol<<3)|(disp->hsPol<<2)|0x03);
		dispc_reg_out(VENC_HSPLS,disp->xSyncWidth*encPerPixel);
		dispc_reg_out(VENC_VSPLS,disp->ySyncWidth);
		dispc_reg_out(VENC_HINT,(disp->xRes+disp->xSyncWidth+disp->xBegin+disp->xEnd-1)*encPerPixel);
		dispc_reg_out(VENC_HSTART,disp->xBegin*encPerPixel);
		dispc_reg_out(VENC_HVALID,disp->xRes*encPerPixel);
		dispc_reg_out(VENC_VINT,(disp->yRes+disp->ySyncWidth+disp->yBegin+disp->yEnd-1));
		dispc_reg_out(VENC_VSTART,disp->yBegin);
		dispc_reg_out(VENC_VVALID,disp->yRes);
		dispc_reg_out(VENC_HSDLY,0);
		dispc_reg_out(VENC_VSDLY,0);
		dispc_reg_out(VENC_RGBCTL,0);
		// Drive all 24 LCD pins by default
		davinci_mux_peripheral(DAVINCI_MUX_RGB666,0);
		davinci_mux_peripheral(DAVINCI_MUX_RGB888,1);
		davinci_mux_peripheral(DAVINCI_MUX_LOEEN,1);
		dispc_reg_out(VENC_LCDOUT,(disp->oePol<<1)|1);	//enable active high on gpio0
		
		val[0] = 0;
		val[1] = 0;
		val[2] = 0;
		val[3] = 0;
		gbit = (encPerPixel>>1);
		bit = (encPerPixel>>1);
		while (bit < 64) {
			val[bit>>4] |= (1<<(bit&0xf));
			bit++;
			gbit++;
			if (gbit>=encPerPixel) {
				gbit = (encPerPixel>>1);
				bit += gbit;
			}
		}
		bit -= gbit;
		dispc_reg_out(VENC_DCLKCTL,((encPerPixel==1)?(1<<11):0) | (bit-1));
		dispc_reg_out(VENC_DCLKPTN0,val[0]);
		dispc_reg_out(VENC_DCLKPTN1,val[1]);
		dispc_reg_out(VENC_DCLKPTN2,val[2]);
		dispc_reg_out(VENC_DCLKPTN3,val[3]);
		dispc_reg_out(VENC_DCLKHS,0);
		dispc_reg_out(VENC_DCLKHSA,0);
		dispc_reg_out(VENC_DCLKHR,disp->xRes+disp->xSyncWidth+disp->xBegin+disp->xEnd);
		dispc_reg_out(VENC_DCLKVS,0);
		dispc_reg_out(VENC_DCLKVR,disp->yRes+disp->ySyncWidth+disp->yBegin+disp->yEnd);

		val[0] = 0;
		bit = 0;
		while (bit < 16) {
			val[0] |= (1<<bit);
			bit+=encPerPixel;
		}
		if (bit>16) bit -= encPerPixel;
		dispc_reg_out(VENC_OSDCLK0,bit-1);
		dispc_reg_out(VENC_OSDCLK1,val[0]);
		dispc_reg_out(VENC_OSDHAD,0);
		dispc_reg_out(VENC_VMOD, (VENC_VMOD_VDMD_RGB666<<VENC_VMOD_VDMD_SHIFT)|
				VENC_VMOD_VMD|
				VENC_VMOD_VENC);
	} else {
		dispc_reg_out(VENC_VMOD, 0);
	}
}

void FinishInit(vpbe_dm_win_info_t *w)
{
	w->info.var.xres_virtual = (w->info.var.bits_per_pixel>=16)? round16(w->info.var.xres) : round64(w->info.var.xres); 
	w->info.var.yres_virtual = w->info.var.yres * w->numbufs;
        w->info.var.width = w->info.var.xres_virtual ;
        w->info.var.height = w->info.var.yres_virtual ;
	w->info.fix.line_length = ((w->info.var.xres_virtual * w->info.var.bits_per_pixel + (32*8-1)) >> (3+5)) << 5;

	/* Set default zoom in register */
	set_zoom(w->zoom.window_id, w->zoom.zoom_h,w->zoom.zoom_v);
	/* Set default buffer size */
	w->fb_size = (w->info.var.yres_virtual * w->info.var.xres_virtual * w->info.var.bits_per_pixel)>>3;
}
void FinishInitVid(vpbe_dm_win_info_t *w)
{
	vpbe_video_params_t *vid_par = &w->conf_params.video_params;
	vpbe_video_config_params_t vid_conf_params;

	FinishInit(w);

	/* Set the defaults vid params in registers */
	vid_conf_params.cb_cr_order = vid_par->cb_cr_order;
	vid_conf_params.exp_info = vid_par->exp_info;
	vpbe_set_vid_params(w->info.fix.id, &vid_conf_params);
}
void FinishInitOsd(vpbe_dm_win_info_t *w)
{
	vpbe_bitmap_params_t *bit_par = &dm->osd0->conf_params.bitmap_params;
	vpbe_bitmap_config_params_t bitmap_config_params;

	FinishInit(w);

	/* Set default bitmap params in registers */
	bitmap_config_params.attenuation_enable = bit_par->attenuation_enable;
	bitmap_config_params.clut_select = bit_par->clut_select;
	vpbe_bitmap_set_params(dm->osd0->info.fix.id, &bitmap_config_params);
}

/*
 * ========	set_vid0_default_conf	========
 */
/* The function	set defaults of VID0 window. */
void set_vid0_default_conf()
{
	vpbe_video_params_t *vid_par = &dm->vid0->conf_params.video_params;

	vid_par->cb_cr_order = SET_0;	/* default order CB-CR */
	vid_par->exp_info.horizontal = VPBE_DISABLE;	/* Expansion disable */
	vid_par->exp_info.vertical = VPBE_DISABLE;

	dm->vid0->alloc_fb_mem = 1;
	dm->vid0->window_enable = VPBE_ENABLE;
	dm->vid0->zoom.window_id = VID0;
	dm->vid0->zoom.zoom_h = 0;	/* No Zoom */
	dm->vid1->zoom.zoom_v = 0;
	dm->vid0->field_frame_select = FB_VMODE_INTERLACED;
	dm->vid0->numbufs = TRIPLE_BUF;

}

/*
 * ========	set_vid1_default_conf	========
 */
/* The function	set defaults of VID1 window. */
void set_vid1_default_conf()
{
	vpbe_video_params_t *vid_par = &dm->vid1->conf_params.video_params;

	vid_par->cb_cr_order = SET_0;	/* default order CB-CR */
	vid_par->exp_info.horizontal = VPBE_DISABLE;	/* Expansion disable */
	vid_par->exp_info.vertical = VPBE_DISABLE;

	dm->vid1->alloc_fb_mem = 1;
	dm->vid1->window_enable = VPBE_ENABLE;
	dm->vid1->zoom.window_id = VID1;
	dm->vid1->zoom.zoom_h = 0;	/* No Zoom */
	dm->vid1->zoom.zoom_v = 0;
	dm->vid1->field_frame_select = FB_VMODE_INTERLACED;
	dm->vid1->numbufs = TRIPLE_BUF;
}

/*
 * ========	set_osd0_default_conf	========
 */
/* The function	set defaults of OSD0 window. */
void set_osd0_default_conf()
{
	vpbe_bitmap_params_t *bit_par = &dm->osd0->conf_params.bitmap_params;

	bit_par->attenuation_enable = SET_0;	/* Attenuation Disabled */
	bit_par->clut_select = SET_0;	/* ROM_CLUT */
	bit_par->enable_attribute = SET_0;
	bit_par->blend_info.bf = 0x0;
	bit_par->blink_info.blinking = VPBE_DISABLE;
	bit_par->blink_info.interval = 0;

	dm->osd0->alloc_fb_mem = 1;
	dm->osd0->window_enable = VPBE_ENABLE;
	dm->osd0->zoom.window_id = OSD0;
	dm->osd0->zoom.zoom_h = 0;	/* No Zoom */
	dm->osd0->zoom.zoom_v = 0;
	dm->osd0->field_frame_select = FB_VMODE_INTERLACED;
	dm->osd0->numbufs = DOUBLE_BUF;
}

/*
 * ========	set_osd1_default_conf	========
 */
/* The function	set defaults of OSD1 window. */
void set_osd1_default_conf()
{
	vpbe_bitmap_params_t *bit_par = &dm->osd1->conf_params.bitmap_params;

	bit_par->attenuation_enable = SET_0;	/* Attenuation Disabled */
	bit_par->clut_select = 2;	/* RAM CLUT */
	bit_par->enable_attribute = SET_0;
	bit_par->blend_info.bf = 0x0;
	bit_par->blink_info.blinking = VPBE_DISABLE;
	bit_par->blink_info.interval = 0;

	dm->osd1->alloc_fb_mem = 1;
	dm->osd1->window_enable = VPBE_ENABLE;
	dm->osd1->zoom.window_id = OSD1;
	dm->osd1->zoom.zoom_h = 0;
	dm->osd1->zoom.zoom_v = 0;
	dm->osd1->field_frame_select = FB_VMODE_INTERLACED;
	dm->osd1->numbufs = DOUBLE_BUF;
	dm->osd1->conf_params.bitmap_params.stored_bits_per_pixel = SET_0;
}

/*
 * ========	set_cursor_default_conf	========
 */
/* The function	set defaults of cursor window. */
void set_cursor_default_conf()
{
	dm->cursor.enable = VPBE_DISABLE;
	dm->cursor.image.dx = 10;
	dm->cursor.image.dy = 10;
	dm->cursor.image.width = 10;
	dm->cursor.image.height = 10;
	dm->cursor.image.fg_color = 10;
	dm->cursor.image.depth = 10;

	/* Set default cursor params in registers. */
	vpbe_set_cursor_params(&dm->cursor);
}

/*
 * ========	set_dm_default_conf	========
 */
/* The function	set defaults of dm structure. */
void set_dm_default_conf()
{
	dm->backg.clut_select = SET_0;
	dm->backg.color_offset = 0x80;

	dm->dclk.dclk_pattern_width = 0;
	dm->dclk.dclk_pattern0 = 0;
	dm->dclk.dclk_pattern1 = 0;
	dm->dclk.dclk_pattern2 = 0;
	dm->dclk.dclk_pattern3 = 0;

	dm->display.mode = NTSC;
	dm->display.interface = COMPOSITE;

	/* Configure defaults in registers */

	/* Initialize the VPSS Clock Control register */
	dispc_reg_out(VPSS_CLKCTL, 0x18);

	/* Reset OSD registers to default. */
	dispc_reg_out(OSD_MODE, 0);
	dispc_reg_out(OSD_OSDWIN0MD, 0);

	/* Set blue background color */
	vpbe_set_backg_color(&dm->backg);

	/* Field Inversion Workaround  */
	dispc_reg_out(OSD_MODE, 0x280);
}


static int CheckWinParms(char* this_opt,char* pName,vpbe_dm_win_info_t *w,int* pFlag)
{
	u32 xres, yres, xpos, ypos;
	u32 sxres, syres, sxpos, sypos;
	if (!strncmp(this_opt, pName, 5)) {
		if (!strncmp(this_opt + 5, "off", 3)) {
			w->window_enable = VPBE_DISABLE;
		} else if (!parse_win_params(this_opt + 5,&xres, &yres, &xpos, &ypos)) {
			*pFlag = 1;
			sxres = w->info.var.xres;
			syres = w->info.var.yres;
			sxpos = w->win_pos.xpos;
			sypos = w->win_pos.ypos;

			w->info.var.xres = xres;
			w->info.var.yres = yres;
			w->win_pos.xpos = xpos;
			w->win_pos.ypos = ypos;
			w->window_enable = VPBE_DISABLE;
			if ((davincifb_check_var(&w->info.var, &w->info)) < 0) {
				w->info.var.xres = sxres; //restore previous val
				w->info.var.yres = syres;
				w->win_pos.xpos = sxpos;
				w->win_pos.ypos = sypos;
			}
			w->window_enable = VPBE_ENABLE;
		}
		return 1;
	}
	return 0;
}
/* 
 * Pass boot-time options by adding the following string to the boot params:
 * 	video=dm64xxfb:options
 * Valid options:
 * 	output=[ntsc|pal|525p|625p|350p|400p|480p]
 * 	format=[composite|s-video|component|prgb]
 * 	vid0=[off|MxN@X,Y]
 * 	vid1=[off|MxN@X,Y]
 * 	osd0=[off|MxN@X,Y]
 * 	osd1=[off|MxN@X,Y]
 * 		MxN specify the window resolution (displayed size)
 * 		X,Y specify the window position
 * 		M, N, X, Y are integers
 * 		M, X should be multiples of 16
 *    noblank=[0|1]
 *	For example
 *	video=dm64xxfb:output=s-video:format=ntsc:vid0=720x480@0,0
 *	:osd0=off:osd1=off	
 */

int davincifb_setup(char *options)
{
	char *this_opt;
	struct fb_info *info;
        int i ;
	int format_yres = 480;
	int flag = 0, flag_osd0 = 0, flag_osd1 = 0, flag_vid0 = 0, flag_vid1 = 0;
	const DISPLAYCFG* disp = build_current_settings();
	
	for( i = 0 ; i < 4 ; i++ ){
           update_var( default_vars[i], disp );
        }
        dm->videomode.xres = disp->xRes ;
        dm->videomode.yres = disp->yRes ;
        dm->videomode.fps  = disp->vSyncHz ;
//	dm->videomode.left_margin;
//	dm->videomode.right_margin;
//	dm->videomode.upper_margin;
//	dm->videomode.lower_margin;
        /* Default settings for var_screeninfo and fix_screeninfo */
	info = init_fb_info(dm->vid0, &vid0_default_var, VID0_FBNAME, disp);
	info = init_fb_info(dm->vid1, &vid1_default_var, VID1_FBNAME, disp);
	info = init_fb_info(dm->osd0, &osd0_default_var, OSD0_FBNAME, disp);
	info = init_fb_info(dm->osd1, &osd1_default_var, OSD1_FBNAME, disp);
	/* Configure default settings for video/bitmap/cursor config params */
	set_vid0_default_conf();
	set_vid1_default_conf();
	set_osd0_default_conf();
	set_osd1_default_conf();
	set_cursor_default_conf();

	/* Default setting for backg/dclk/display_interface/display_mode */
	set_dm_default_conf();
	switch (dm->display.interface) {
	case SVIDEO:
	case COMPONENT:
	case COMPOSITE:{
			if ((dm->display.mode == NTSC) ||
			    (dm->display.mode == P525))
				format_yres = 480;
			else
				/*P625, PAL */
				format_yres = 576;
		}
		break;
	case PRGB:{
			if (dm->display.mode == P350)
				format_yres = 350;
			else if (dm->display.mode == P400)
				format_yres = 400;
			else
				format_yres = 480;
		}
		break;
	default:
		break;
	}

	dm->osd0->info.var.yres = format_yres;
	dm->osd1->info.var.yres = format_yres;
	dm->vid0->info.var.yres = format_yres;
	dm->vid1->info.var.yres = format_yres;
	dev_dbg(dm->dev, "davincifb: Options \"%s\"\n", options);

	if (!options || !*options)
		return 0;

	while ((this_opt = strsep(&options, ":")) != NULL) {

		if (!*this_opt)
			continue;

		if (!strncmp(this_opt, "output=", 7)) {
			flag = 1;
			if (!strncmp(this_opt + 7, "lcd", 3)) {
				dm->display.mode = LCD;
				dm->display.interface = RGB;
				dm->videomode.vmode = FB_VMODE_NONINTERLACED;
				dm->osd0->info.var.xres = disp->xRes;
				dm->osd1->info.var.xres = disp->xRes;
				dm->vid0->info.var.xres = disp->xRes;
				dm->vid1->info.var.xres = disp->xRes;

				dm->osd0->info.var.yres = disp->yRes;
				dm->osd1->info.var.yres = disp->yRes;
				dm->vid0->info.var.yres = disp->yRes;
				dm->vid1->info.var.yres = disp->yRes;
			} else {
				if (!strncmp(this_opt + 7, "ntsc", 4))
					dm->display.mode = NTSC;
				else if (!strncmp(this_opt + 7, "pal", 3))
					dm->display.mode = PAL;
				else if (!strncmp(this_opt + 7, "525p", 4))
					dm->display.mode = P525;
				else if (!strncmp(this_opt + 7, "625p", 4))
					dm->display.mode = P625;
				else if (!strncmp(this_opt + 7, "480p", 4))
					dm->display.mode = P480;
				else if (!strncmp(this_opt + 7, "400p", 4))
					dm->display.mode = P400;
				else if (!strncmp(this_opt + 7, "350p", 4))
					dm->display.mode = P350;
				else
					dm->display.mode = NTSC;
				dm->osd0->info.var.xres = OSD0_XRES;
				dm->osd1->info.var.xres = OSD1_XRES;
				dm->vid0->info.var.xres = VID0_XRES;
				dm->vid1->info.var.xres = VID1_XRES;
			}
		} else if (!strncmp(this_opt, "format=", 7)) {
			flag = 1;
			if (!strncmp(this_opt + 7, "s-video", 7))
				dm->display.interface = SVIDEO;
			else if (!strncmp(this_opt + 7, "component", 9))
				dm->display.interface = COMPONENT;
			else if (!strncmp(this_opt + 7, "composite", 9))
				dm->display.interface = COMPOSITE;
			else if (!strncmp(this_opt + 7, "rgb", 3))
				dm->display.interface = RGB;
			else if (!strncmp(this_opt + 7, "ycc16", 5))
				dm->display.interface = YCC16;
			else if (!strncmp(this_opt + 7, "ycc8", 4))
				dm->display.interface = YCC8;
			else if (!strncmp(this_opt + 7, "prgb", 4))
				dm->display.interface = PRGB;
			else if (!strncmp(this_opt + 7, "srgb", 4))
				dm->display.interface = SRGB;
			else if (!strncmp(this_opt + 7, "epson", 5))
				dm->display.interface = EPSON;
			else if (!strncmp(this_opt + 7, "casio1g", 7))
				dm->display.interface = CASIO1G;
			else if (!strncmp(this_opt + 7, "udisp", 5))
				dm->display.interface = UDISP;
			else if (!strncmp(this_opt + 7, "stn", 3))
				dm->display.interface = STN;
			else
				dm->display.interface = COMPONENT;
		} else {
			if (!CheckWinParms(this_opt,"vid0=",dm->vid0,&flag_vid0))
				if (!CheckWinParms(this_opt,"vid1=",dm->vid1,&flag_vid1))
					if (!CheckWinParms(this_opt,"osd0=",dm->osd0,&flag_osd0))
						if (!CheckWinParms(this_opt,"osd1=",dm->osd1,&flag_osd1)) {
							if (!strncmp(this_opt, "fbMemBase=", 5)) {
								char* p;
								fbMemBase = simple_strtoul(this_opt+10, &p, 0);
								if (*p=='M') fbMemBase = (fbMemBase<<20)+0x80000000;
							} else if( !strncmp(this_opt, "noblank=", 8)) {
								noblank=simple_strtoul(this_opt+8,0,0);
							}
						}
		}
	}
	if (flag) {
		switch (dm->display.interface) {
		case SVIDEO:
		case COMPONENT:
		case COMPOSITE:{
				if ((dm->display.mode == NTSC) ||
				    (dm->display.mode == P525) ||
				    (dm->display.mode == LCD))
					format_yres = 480;
				else
					/*P625, PAL */
					format_yres = 576;
			}
			break;
		case PRGB:{
				if (dm->display.mode == P350)
					format_yres = 350;
				else if (dm->display.mode == P400)
					format_yres = 400;
				else
					format_yres = 480;
			}
			break;
		default:
			break;
		}
		if (dm->display.mode != LCD) {
			if (!flag_osd0) dm->osd0->info.var.yres = format_yres;
			if (!flag_osd1) dm->osd1->info.var.yres = format_yres;
			if (!flag_vid0) dm->vid0->info.var.yres = format_yres;
			if (!flag_vid1) dm->vid1->info.var.yres = format_yres;
		}
	}
	FinishInitVid(dm->vid0);
	FinishInitVid(dm->vid1);
	FinishInitOsd(dm->osd0);
	FinishInitOsd(dm->osd1);
	dev_dbg(dm->dev, "DaVinci: "
		"Output on %s%s, Enabled windows: %s %s %s %s\n",
		((dm->display.mode == P480)
		 && (dm->display.interface ==
		     PRGB)) ? "480P" : ((dm->display.mode == P400)
					&& (dm->display.interface ==
					    PRGB)) ? "400P" : ((dm->display.
								mode == P350)
							       && (dm->display.
								   interface ==
								   PRGB)) ?
		"350P" : (dm->display.mode ==
			  LCD) ? "LCD" : (dm->display.mode ==
					  NTSC) ? "NTSC" : (dm->display.mode ==
							    PAL) ? "PAL" : (dm->
									    display.
									    mode
									    ==
									    P525)
		? "525P" : (dm->display.mode ==
			    P625) ? "625P" : "unknown device!",
		(dm->display.interface ==
		 SVIDEO) ? " in SVIDEO format" : (dm->display.interface ==
						  COMPONENT) ?
		" in COMPONENT format" : (dm->display.interface ==
					  COMPOSITE) ? " in COMPOSITE format"
		: (dm->display.interface ==
		   RGB) ? " in RGB format" : (dm->display.interface == YCC16)
		? " in YCC16 format" : (dm->display.interface ==
					YCC8) ? " in YCC8 format" : (dm->
								     display.
								     interface
								     ==
								     PRGB) ?
		" in PRGB format" : (dm->display.interface ==
				     SRGB) ? " in SRGB format" : (dm->display.
								  interface ==
								  EPSON) ?
		" in EPSON format" : (dm->display.interface ==
				      CASIO1G) ? " in CASIO1G format" : (dm->
									 display.
									 interface
									 ==
									 UDISP)
		? " in UDISP format" : (dm->display.interface ==
					STN) ? " in STN format" : "",
		(dm->vid0->window_enable) ? "Video0" : "",
		(dm->vid1->window_enable) ? "Video1" : "",
		(dm->osd0->window_enable) ? "OSD0" : "",
		(dm->osd1->window_enable) ? "OSD1" : "");

	if (dm->vid0->window_enable)
		dev_dbg(dm->dev, "Setting Video0 size %dx%d, "
			"position (%d,%d)\n",
			dm->vid0->info.var.xres, dm->vid0->info.var.yres,
			dm->vid0->win_pos.xpos, dm->vid0->win_pos.ypos);
	if (dm->vid1->window_enable)
		dev_dbg(dm->dev, "Setting Video1 size %dx%d, "
			"position (%d,%d)\n",
			dm->vid1->info.var.xres, dm->vid1->info.var.yres,
			dm->vid1->win_pos.xpos, dm->vid1->win_pos.ypos);
	if (dm->osd0->window_enable)
		dev_dbg(dm->dev, "Setting OSD0 size %dx%d, "
			"position (%d,%d)\n",
			dm->osd0->info.var.xres, dm->osd0->info.var.yres,
			dm->osd0->win_pos.xpos, dm->osd0->win_pos.ypos);
	if (dm->osd1->window_enable)
		dev_dbg(dm->dev, "Setting OSD1 size %dx%d, "
			"position (%d,%d)\n",
			dm->osd1->info.var.xres, dm->osd1->info.var.yres,
			dm->osd1->win_pos.xpos, dm->osd1->win_pos.ypos);
	return 0;
}


void init_display_function(struct vpbe_display_format *d)
{
	if (d->mode == NON_EXISTING_MODE)
		dm->output_device_config = vpbe_davincifb_dlcd_nonstd_config;
	else if ((d->mode == NTSC) && (d->interface == COMPOSITE))
		dm->output_device_config = davincifb_ntsc_composite_config;
	else if ((d->mode == NTSC) && (d->interface == SVIDEO))
		dm->output_device_config = davincifb_ntsc_svideo_config;
	else if ((d->mode == NTSC) && (d->interface == COMPONENT))
		dm->output_device_config = davincifb_ntsc_component_config;
	else if ((d->mode == PAL) && (d->interface == COMPOSITE))
		dm->output_device_config = davincifb_pal_composite_config;
	else if ((d->mode == PAL) && (d->interface == SVIDEO))
		dm->output_device_config = davincifb_pal_svideo_config;
	else if ((d->mode == PAL) && (d->interface == COMPONENT))
		dm->output_device_config = davincifb_pal_component_config;
	else if ((d->mode == NTSC) && (d->interface == RGB))
		dm->output_device_config = vpbe_davincifb_ntsc_rgb_config;
	else if ((d->mode == PAL) && (d->interface == RGB))
		dm->output_device_config = vpbe_davincifb_pal_rgb_config;
	else if ((d->mode == P525) && (d->interface == COMPONENT))
		dm->output_device_config = vpbe_davincifb_525p_component_config;
	else if ((d->mode == P625) && (d->interface == COMPONENT))
		dm->output_device_config = vpbe_davincifb_625p_component_config;
	else if ((d->mode == DEFAULT_MODE) && (d->interface == YCC16))
		dm->output_device_config = vpbe_davincifb_default_ycc16_config;
	else if ((d->mode == DEFAULT_MODE) && (d->interface == YCC8))
		dm->output_device_config = vpbe_davincifb_default_ycc8_config;
	else if ((d->mode == P480) && (d->interface == PRGB))
		dm->output_device_config = vpbe_davincifb_480p_prgb_config;
	else if ((d->mode == P400) && (d->interface == PRGB))
		dm->output_device_config = vpbe_davincifb_400p_prgb_config;
	else if ((d->mode == P350) && (d->interface == PRGB))
		dm->output_device_config = vpbe_davincifb_350p_prgb_config;
	else if ((d->mode == DEFAULT_MODE) && (d->interface == SRGB))
		dm->output_device_config = vpbe_davincifb_default_srgb_config;
	else if ((d->mode == DEFAULT_MODE) && (d->interface == EPSON))
		dm->output_device_config = vpbe_davincifb_default_epson_config;
	else if ((d->mode == DEFAULT_MODE) && (d->interface == CASIO1G))
		dm->output_device_config = vpbe_davincifb_default_casio_config;
	else if ((d->mode == DEFAULT_MODE) && (d->interface == UDISP))
		dm->output_device_config = vpbe_davincifb_default_UDISP_config;
	else if ((d->mode == DEFAULT_MODE) && (d->interface == STN))
		dm->output_device_config = vpbe_davincifb_default_STN_config;
	/* Add support for other displays       here */
	else if (d->mode == LCD)
		dm->output_device_config = vpbe_davincifb_lcd_component_config;
	else {
		dev_err(dm->dev, "Unsupported output	device!\n");
		dm->output_device_config = NULL;
	}
}
/*
 * ========	vpbe_mem_alloc_max	========
 */
/* The function	allocates max memory required for the 
				frame buffers of the window. */
int vpbe_mem_alloc_max(struct vpbe_dm_win_info *w)
{
	dev_dbg(dm->dev, "Allocating buffer of size: %x\n", w->fb_size);
	if (fbMemBase) {
		unsigned int size = ((w->fb_size+0xffff)& ~0xffff) * TRIPLE_BUF ;
		w->fb_base_phys = fbMemBase;
		fbMemBase += size;
		if (!request_mem_region(w->fb_base_phys, size, w->info.fix.id)) {
			dev_err(dm->dev, "%s: cannot reserve FB region\n", w->info.fix.id);
			return -ENOMEM;
		}
		w->fb_base = (unsigned long)ioremap(w->fb_base_phys, size);
		if (!w->fb_base) {
			dev_err(dm->dev, "%s: cannot map framebuffer\n", w->info.fix.id);
			return -ENOMEM;
		}
		w->alloc_fb_mem=0;
		printk(KERN_ERR "**** Allocated buffer v=0x%08x p=0x%08x size=0x%x\n", w->fb_base, w->fb_base_phys, size);
	} else {
		w->fb_base = (unsigned long)dma_alloc_coherent
		    (dm->dev, w->fb_size, &w->fb_base_phys, GFP_KERNEL | GFP_DMA);
		if (!w->fb_base) {
			dev_err(dm->dev, "%s : dma_alloc_coherent fail.\n",
				__FUNCTION__);
			return -ENOMEM;
		}
		printk(KERN_ERR "**** dma_alloc v=0x%08x p=0x%08x size=0x%x\n", w->fb_base, w->fb_base_phys, w->fb_size);
	}

	w->info.fix.smem_start = w->fb_base_phys;
	w->info.fix.smem_len = w->fb_size;

	w->info.screen_base = (char *)(w->fb_base);

	return 0;
}

/*
 * ========	vpbe_mem_release_window_buf	========
 */
/* The function	allocates the memory required for the 
				frame buffers of the window. */
int vpbe_mem_release_window_buf(struct vpbe_dm_win_info *w)
{
	if (w->fb_base) {
		if (!w->alloc_fb_mem) {
			unsigned int size = (w->fb_size+0xffff)& ~0xffff;
			iounmap((void *)w->fb_base);
			release_mem_region(w->fb_base_phys, size);
		} else {
			dma_free_coherent(NULL, w->fb_size, (void *)w->fb_base,
				  w->fb_base_phys);
		}
		w->fb_base = 0;
		w->info.screen_base = NULL;
		w->info.fix.smem_start = 0;
		w->info.fix.smem_len = 0;
	}
	return 0;
}
/*
 * mem_release
 */
int mem_release(struct vpbe_dm_win_info *w)
{
	if (w) {
		vpbe_mem_release_window_buf(w);
		kfree(w);
	}
	return 0;
}

/*
 * ========	vpbe_mem_alloc_struct	========
 */
/* The function	allocate the memory required for the 
				frame buffers of the window. */
int vpbe_mem_alloc_struct(struct vpbe_dm_win_info **w)
{
	*w = kmalloc(sizeof(struct vpbe_dm_win_info), GFP_KERNEL);
	if (!*w) {
		return -ENOMEM;
	}
	memset(*w, 0, sizeof(struct vpbe_dm_win_info));

	return 0;
}

/*
 *  Cleanup
 */
int davincifb_remove(struct device *dev)
{
	free_irq(IRQ_VENCINT, &dm);

	/* Cleanup all framebuffers */
	if (dm->osd0) {
		unregister_framebuffer(&dm->osd0->info);
		mem_release(dm->osd0);
	}
	if (dm->osd1) {
		unregister_framebuffer(&dm->osd1->info);
		mem_release(dm->osd1);
	}
	if (dm->vid0) {
		unregister_framebuffer(&dm->vid0->info);
		mem_release(dm->vid0);
	}
	if (dm->vid1) {
		unregister_framebuffer(&dm->vid1->info);
		mem_release(dm->vid1);
	}

        if( 0 <= dmach ){
           davinci_free_dma(dmach);
           printk( KERN_ERR "%s: released dma channel %d\n", __FUNCTION__, dmach );
           dmach = -1 ;
        }
	/* Turn OFF the output device */
	dm->output_device_config();
	vpbe_enable_venc(0);
	if (dm->display.interface == PRGB)
		vpbe_enable_lcd(0);
	else if (dm->display.interface == SVIDEO ||
		 dm->display.interface == COMPONENT ||
		 dm->display.interface == COMPOSITE)
		vpbe_enable_dacs(0);

	if (dm->mmio_base)
		iounmap((void *)dm->mmio_base);
	release_mem_region(dm->mmio_base_phys, dm->mmio_size);

	return 0;
}

/*
 * ========	vpbe_davincifb_probe ========
 */
int vpbe_davincifb_probe(struct device *dev)
{
	struct platform_device *pdev;
        int rval ;

//	dma_addr_t vbase = VIDEO_FB_BASE;

	pdev = to_platform_device(dev);
	if (pdev->num_resources != 0) {
		dev_err(dev, "probed for an	unknown	device\n");
		return -ENODEV;
	}

	dm->dev = dev;
	dm->mmio_base_phys = OSD_REG_BASE;
	dm->mmio_size = OSD_REG_SIZE;

	/* request the mem regions */
	if (!request_mem_region
	    (dm->mmio_base_phys, dm->mmio_size, DAVINCIFB_DRIVER)) {
		dev_err(dev, ":	cannot reserve MMIO	region\n");
		return -ENODEV;
	}

	/* map the regions */
	dm->mmio_base =
	    (unsigned long)ioremap(dm->mmio_base_phys, dm->mmio_size);
	if (!dm->mmio_base) {
		dev_err(dev, ":	cannot map MMIO\n");
		goto release_mmio;
	}

	/* Do default settings for all the windows      */
	davincifb_setup(options);

	/* Set fb_videomode structure */
        if( LCD != dm->display.mode )
		dm->videomode = modelist[dm->display.interface][dm->display.mode];

	/* choose config function */
	init_display_function(&dm->display);

	/* initialize   the     vsync   wait queue */
	init_waitqueue_head(&dm->vsync_wait);
#ifdef RESIZER
	init_waitqueue_head(&dm->resizer_wait);
#endif
	dm->timeout = HZ / 5;

        dmatcc = TCC_ANY ;
	if( 0 != ( rval = davinci_request_dma(DAVINCI_DMA_CHANNEL_ANY, "davincifb", davincifb_dma_callback, 0, &dmach, &dmatcc, EVENTQ_1)) ){
		printk( KERN_ERR "%s: error %d requesting DMA\n", __FUNCTION__, rval );
		goto release_mmio;
	}
        printk( KERN_ERR "%s: allocated dma channel %d, tcc %d\n", __FUNCTION__, dmach, dmatcc );
        dmatcc <<= 12 ;
        init_completion(&dma_completion);

/*
 *	WINDOW SETUP:	
 *		Perform following for all the windows
 *	a)	If window is to be enabled, allocate buffers.
 *	b)	Register fb device.
 *	c)	Call davincifb_set_par to configure all OSD registers
 *	d)	Set appropriate register to enable the window, 
 *		if command line option is given to enable the window.    
 *	e)	Initialize video output by executing configuration function.
 */

	/*
	 * OSD0 Setup
	 */
	/* Allocate buffers */
	if (dm->osd0->window_enable) {
		if (vpbe_mem_alloc_max(dm->osd0) < 0)
			goto vpbe_probe_exit;
		/* memset buffer with some value for background color */
		memset((void *)dm->osd0->fb_base, 0x00, dm->osd0->fb_size);
	}
	/* Register FB device */
	if (register_framebuffer(&dm->osd0->info) < 0) {
		dev_err(dev,
			OSD0_FBNAME
			"Unable	to register	OSD0 framebuffer\n");
		mem_release(dm->osd0);
		goto vpbe_probe_exit;
	}
	/* Configure registers */
	davincifb_set_par(&dm->osd0->info);
	/* Enable the window */
	set_win_enable(dm->osd0->info.fix.id, dm->osd0->window_enable);

	/*
	 * VID0 Setup
	 */
	/* Allocate buffers */
	if (dm->vid0->window_enable) {
		if (vpbe_mem_alloc_max(dm->vid0) < 0)
			goto vpbe_probe_exit;

		memset((void *)dm->vid0->fb_base, 0x80, dm->vid0->fb_size);
	}
	if (register_framebuffer(&dm->vid0->info) < 0) {
		dev_err(dev,
			VID0_FBNAME
			"Unable	to register	VID0 framebuffer\n");
		mem_release(dm->vid0);
		goto vpbe_probe_exit;
	}
	davincifb_set_par(&dm->vid0->info);
	/*
	 * OSD1 Setup
	 */
	if (dm->osd1->window_enable) {
		if (vpbe_mem_alloc_max(dm->osd1) < 0)
			goto vpbe_probe_exit;
		memset((void *)dm->osd1->fb_base, 0x77, dm->osd1->fb_size);
	}
	if (register_framebuffer(&dm->osd1->info) < 0) {
		dev_err(dev,
			OSD1_FBNAME
			"Unable	to register	OSD1 framebuffer\n");
		mem_release(dm->osd1);
		goto vpbe_probe_exit;
	}
	davincifb_set_par(&dm->osd1->info);

	/*
	 * VID1 Setup
	 */
	if (dm->vid1->window_enable) {
		if (vpbe_mem_alloc_max(dm->vid1) < 0)
			goto vpbe_probe_exit;
		memset((void *)dm->vid1->fb_base, 0x80, dm->vid1->fb_size);
	}
	if (register_framebuffer(&dm->vid1->info) < 0) {
		dev_err(dev,
			VID1_FBNAME
			"Unable	to register	VID1 framebuffer\n");
		mem_release(dm->vid1);
		goto vpbe_probe_exit;
	}
	davincifb_set_par(&dm->vid1->info);
	if (dm->display.mode != LCD) {
		set_win_enable(dm->vid0->info.fix.id, dm->vid0->window_enable);
		set_win_enable(dm->osd1->info.fix.id, dm->osd1->window_enable);
		set_win_enable(dm->vid1->info.fix.id, dm->vid1->window_enable);
	}

	dm->output_device_config();
	vpbe_enable_venc(1);
	if (dm->display.interface == PRGB)
		vpbe_enable_lcd(1);
	else if (dm->display.interface == SVIDEO ||
		 dm->display.interface == COMPONENT ||
		 dm->display.interface == COMPOSITE)
		vpbe_enable_dacs(1);

	if (request_irq(IRQ_VENCINT, davincifb_isr, 0, DAVINCIFB_DRIVER, dm)) {
		dev_err(dev,DAVINCIFB_DRIVER ": could not install interrupt service routine\n");
		goto vpbe_probe_exit;
	}
#ifdef RESIZER
	if (request_irq(IRQ_RSZINT, davincifb_resizer_isr, 0, DAVINCIFB_DRIVER,dm)) {
		dev_err(dev, DAVINCIFB_DRIVER ": could not install interrupt service routine\n");
		goto vpbe_probe_exit;
	}
#endif

	if (!dm->osd0->window_enable) {
		memset(&dm->osd0->info.var, 0x00, sizeof(dm->osd0->info.var));
		dm->osd0->info.fix.line_length = 0;
	}
	if (!dm->vid0->window_enable) {
		memset(&dm->vid0->info.var, 0x00, sizeof(dm->vid0->info.var));
		dm->vid0->info.fix.line_length = 0;
	}
	if (!dm->osd1->window_enable) {
		memset(&dm->osd1->info.var, 0x00, sizeof(dm->osd1->info.var));
		dm->osd1->info.fix.line_length = 0;
	}
	if (!dm->vid1->window_enable) {
		memset(&dm->vid1->info.var, 0x00, sizeof(dm->vid1->info.var));
		dm->vid1->info.fix.line_length = 0;
	}

	return 0;

      vpbe_probe_exit:
	davincifb_remove(dev);
	return -ENODEV;

      release_mmio:
	release_mem_region(dm->mmio_base_phys, dm->mmio_size);
	return -ENODEV;
}
int no_soft_cursor(struct fb_info *info, struct fb_cursor *cursor)
{
	return 0;
}

/*
 *  Frame buffer operations
 */
static struct fb_ops davincifb_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = davincifb_check_var,
	.fb_set_par = davincifb_set_par,
	.fb_setcolreg = davincifb_setcolreg,
	.fb_blank = davincifb_blank,
	.fb_pan_display = davincifb_pan_display,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	/* .fb_cursor = soft_cursor, */
	.fb_cursor = no_soft_cursor,
	.fb_rotate = NULL,
	.fb_sync = NULL,
	.fb_ioctl = davincifb_ioctl,
};

static void davincifb_release_dev(struct device *dev)
{
}

static u64 davincifb_dmamask = ~(u32) 0;

static struct platform_device davincifb_device = {
	.name = DAVINCIFB_DEVICE,
	.id = 0,
	.dev = {
		.release = davincifb_release_dev,
		.dma_mask = &davincifb_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = 0,
};

static struct device_driver davincifb_driver = {
	.name = DAVINCIFB_DRIVER,
	.bus = &platform_bus_type,
	.probe = vpbe_davincifb_probe,
	.remove = davincifb_remove,
	.suspend = NULL,
	.resume = NULL,
};

/*	Register both	the	driver and the device	*/
static int __init davincifb_init(void)
{
	int r = 0;
	struct device *dev = &davincifb_device.dev;
	init_MUTEX(&resizer_sem);

	g_davinci_iram_phys = davinci_alloc_iram(4);
	if (g_davinci_iram_phys<=0) {
		r = -1;
		goto vpbe_init_exit;
	}
	gDavinciIRam = (unsigned long*)ioremap(g_davinci_iram_phys, 4);
	/* Allocate memory for struc 'vpbe_dm_win_info' for all the windows */
	if (vpbe_mem_alloc_struct(&dm->vid0) < 0) {
		r = -1;
		goto vpbe_init_exit;
	}
	if (vpbe_mem_alloc_struct(&dm->vid1) < 0) {
		r = -1;
		goto vpbe_init_exit;
	}
	if (vpbe_mem_alloc_struct(&dm->osd0) < 0) {
		r = -1;
		goto vpbe_init_exit;
	}
	if (vpbe_mem_alloc_struct(&dm->osd1) < 0) {
		r = -1;
		goto vpbe_init_exit;
	}
	dm->vid0->dm = dm;
	dm->vid1->dm = dm;
	dm->osd0->dm = dm;
	dm->osd1->dm = dm;

#ifndef MODULE
	{
		/* handle options for "dm64xxfb" for backwards compatability */
		char *names[] = { "davincifb", "dm64xxfb" };
		int i, num_names = 2, done = 0;

		for (i = 0; i < num_names && !done; i++) {
			if (fb_get_options(names[i], &options)) {
				dev_err(dev, " Disabled on command-line.\n");
				r = -ENODEV;
				goto vpbe_init_exit;
			}
		}
	}
#endif

	/* Register the device with LDM */
	if (platform_device_register(&davincifb_device)) {
		dev_err(dev, "failed to	register davincifb device\n");
		r = -ENODEV;
		goto vpbe_init_exit;
	}
	/* Register the driver with LDM */
	if (driver_register(&davincifb_driver)) {
		dev_err(dev, "failed to	register davincifb driver\n");
		platform_device_unregister(&davincifb_device);
		r = -ENODEV;
		goto vpbe_init_exit;
	}
	return 0;
      vpbe_init_exit:
	if (dm->vid0)
		kfree(dm->vid0);
	if (dm->vid1)
		kfree(dm->vid1);
	if (dm->osd0)
		kfree(dm->osd0);
	if (dm->osd1)
		kfree(dm->osd1);
	if (g_davinci_iram_phys > 0) {
		davinci_free_iram(g_davinci_iram_phys, 4);
		g_davinci_iram_phys = 0;
	}
	return r;
}

static void __exit davincifb_cleanup(void)
{
	iounmap((void*)gDavinciIRam);
	if (g_davinci_iram_phys > 0) davinci_free_iram(g_davinci_iram_phys, 4);
	driver_unregister(&davincifb_driver);
	platform_device_unregister(&davincifb_device);
}

module_init(davincifb_init);
module_exit(davincifb_cleanup);

MODULE_DESCRIPTION("Framebuffer driver for TI DaVinci");
MODULE_AUTHOR("Texas Instruments");
MODULE_LICENSE("GPL");
