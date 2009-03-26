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
 * File: davincifb_dlcd.c	
 */
#include <video/davincifb.h>
extern vpbe_dm_info_t *dm;

void vpbe_enable_lcd(int on)
{
	if (on) {
		/* Enable LCD out */
                dispc_reg_out(VENC_LCDOUT, 0x1);
	} else {
		/* Disable LCD out */
                dispc_reg_out(VENC_LCDOUT, 0x0);
	}
	
}

/*
 * vpbe_davincifb_480p_prgb_config 
 */
void vpbe_davincifb_480p_prgb_config(void)
{
	volatile unsigned int venc, lcdout;
	venc = dispc_reg_in(VENC_VMOD) & VENC_VMOD_VENC;
	lcdout = dispc_reg_in(VENC_LCDOUT);
	vpbe_set_display_default();
	/* Clear component and composite mode registers 
	   (applicable to Analog DACS) */
	dispc_reg_out(VENC_CVBS, 0);
	dispc_reg_out(VENC_CMPNT, 0);
	/* Set Base Pixel X and Base Pixel Y */
	dispc_reg_out(OSD_BASEPX, dm->videomode.basepx);
	dispc_reg_out(OSD_BASEPY, dm->videomode.basepy);
	/* Set VIDCTL to select VCLKE = 1, 
	   VCLKZ =1, SYDIR = 0 (set o/p), DOMD = 0 */
	dispc_reg_merge(VENC_VIDCTL, 1 << VENC_VIDCTL_VCLKE_SHIFT,
			VENC_VIDCTL_VCLKE);
	dispc_reg_merge(VENC_VIDCTL, 0 << VENC_VIDCTL_VCLKZ_SHIFT,
			VENC_VIDCTL_VCLKZ);
	dispc_reg_merge(VENC_VIDCTL, 0 << VENC_VIDCTL_SYDIR_SHIFT,
			VENC_VIDCTL_SYDIR);
	dispc_reg_merge(VENC_VIDCTL, 0 << VENC_VIDCTL_YCDIR_SHIFT,
			VENC_VIDCTL_YCDIR);

	dispc_reg_merge(VENC_DCLKCTL,
			1 << VENC_DCLKCTL_DCKEC_SHIFT,
			VENC_DCLKCTL_DCKEC);

	dispc_reg_out(VENC_DCLKPTN0, 0x1);

	vpbe_set_display_timing(&dm->videomode);
	dispc_reg_out(VENC_SYNCCTL,
		      (VENC_SYNCCTL_SYEV |
		       VENC_SYNCCTL_SYEH | VENC_SYNCCTL_HPL
		       | VENC_SYNCCTL_VPL));
	/* Set OSD clock and OSD Sync Adavance registers */
	dispc_reg_out(VENC_OSDCLK0, 0);
	dispc_reg_out(VENC_OSDCLK1, 1);
	/* set VPSS clock */
	dispc_reg_out(VPSS_CLKCTL, 0x18);

	/* Configure VMOD. No change in VENC bit */
	dispc_reg_out(VENC_VMOD, 0x2010 | venc);
	dispc_reg_out(VENC_LCDOUT, lcdout);

	/* Change window parameters for progressive display */
	change_win_param(FB_VMODE_NONINTERLACED);
}

/*
 * vpbe_davincifb_400p_prgb_config 
 */
void vpbe_davincifb_400p_prgb_config(void)
{
	volatile unsigned int venc, lcdout;
	venc = dispc_reg_in(VENC_VMOD) & VENC_VMOD_VENC;
	lcdout = dispc_reg_in(VENC_LCDOUT);
	vpbe_set_display_default();
	/* Clear component and composite mode registers 
	   (applicable to Analog DACS) */
	dispc_reg_out(VENC_CVBS, 0);
	dispc_reg_out(VENC_CMPNT, 0);
	/* Set Base Pixel X and Base Pixel Y */
	dispc_reg_out(OSD_BASEPX, dm->videomode.basepx);
	dispc_reg_out(OSD_BASEPY, dm->videomode.basepy);
	/* Set VIDCTL to select VCLKE = 1, VCLKZ =1, 
	   SYDIR = 0 (set o/p), DOMD = 0 */
	dispc_reg_merge(VENC_VIDCTL, 1 << VENC_VIDCTL_VCLKE_SHIFT,
			VENC_VIDCTL_VCLKE);
	dispc_reg_merge(VENC_VIDCTL, 0 << VENC_VIDCTL_VCLKZ_SHIFT,
			VENC_VIDCTL_VCLKZ);
	dispc_reg_merge(VENC_VIDCTL, 0 << VENC_VIDCTL_SYDIR_SHIFT,
			VENC_VIDCTL_SYDIR);
	dispc_reg_merge(VENC_VIDCTL, 0 << VENC_VIDCTL_YCDIR_SHIFT,
			VENC_VIDCTL_YCDIR);

	dispc_reg_merge(VENC_DCLKCTL,
			1 << VENC_DCLKCTL_DCKEC_SHIFT,
			VENC_DCLKCTL_DCKEC);

	dispc_reg_out(VENC_DCLKPTN0, 0x1);

	vpbe_set_display_timing(&dm->videomode);
	dispc_reg_out(VENC_SYNCCTL,
		      (VENC_SYNCCTL_SYEV |
		       VENC_SYNCCTL_SYEH | VENC_SYNCCTL_HPL));
	/* Set OSD clock and OSD Sync Adavance registers */
	dispc_reg_out(VENC_OSDCLK0, 0);
	dispc_reg_out(VENC_OSDCLK1, 1);
	/* set VPSS clock */
	dispc_reg_out(VPSS_CLKCTL, 0x18);
	/* Configure VMOD. No change in VENC bit */
	dispc_reg_out(VENC_VMOD, 0x2010 | venc);
	dispc_reg_out(VENC_LCDOUT, lcdout);

	/* Change window parameters for progressive display */
	change_win_param(FB_VMODE_NONINTERLACED);
}

/*
 * vpbe_davincifb_350p_prgb_config 
 */
void vpbe_davincifb_350p_prgb_config(void)
{
	volatile unsigned int venc, lcdout;
	venc = dispc_reg_in(VENC_VMOD) & VENC_VMOD_VENC;
	lcdout = dispc_reg_in(VENC_LCDOUT);
	vpbe_set_display_default();
	/* Clear component and composite mode registers 
	   (applicable to Analog DACS) */
	dispc_reg_out(VENC_CVBS, 0);
	dispc_reg_out(VENC_CMPNT, 0);
	/* Set Base Pixel X and Base Pixel Y */
	dispc_reg_out(OSD_BASEPX, dm->videomode.basepx);
	dispc_reg_out(OSD_BASEPY, dm->videomode.basepy);
	/* Set VIDCTL to select VCLKE = 1, VCLKZ =1, 
	   SYDIR = 0 (set o/p), DOMD = 0 */
	dispc_reg_merge(VENC_VIDCTL, 1 << VENC_VIDCTL_VCLKE_SHIFT,
			VENC_VIDCTL_VCLKE);
	dispc_reg_merge(VENC_VIDCTL, 0 << VENC_VIDCTL_VCLKZ_SHIFT,
			VENC_VIDCTL_VCLKZ);
	dispc_reg_merge(VENC_VIDCTL, 0 << VENC_VIDCTL_SYDIR_SHIFT,
			VENC_VIDCTL_SYDIR);
	dispc_reg_merge(VENC_VIDCTL, 0 << VENC_VIDCTL_YCDIR_SHIFT,
			VENC_VIDCTL_YCDIR);

	dispc_reg_merge(VENC_DCLKCTL,
			1 << VENC_DCLKCTL_DCKEC_SHIFT,
			VENC_DCLKCTL_DCKEC);

	dispc_reg_out(VENC_DCLKPTN0, 0x1);

	vpbe_set_display_timing(&dm->videomode);
	dispc_reg_out(VENC_SYNCCTL,
		      (VENC_SYNCCTL_SYEV | VENC_SYNCCTL_SYEH));
	/* Set OSD clock and OSD Sync Adavance registers */
	dispc_reg_out(VENC_OSDCLK0, 0);
	dispc_reg_out(VENC_OSDCLK1, 1);
	/* set VPSS clock */
	dispc_reg_out(VPSS_CLKCTL, 0x18);
	/* Configure VMOD. No change in VENC bit */
	dispc_reg_out(VENC_VMOD, 0x2010 | venc);
	dispc_reg_out(VENC_LCDOUT, lcdout);

	/* Change window parameters for progressive display */
	change_win_param(FB_VMODE_NONINTERLACED);
}

/*
 * vpbe_set_display_timing 
 */
/* This function sets the display timing from the fb_info structure*/
void vpbe_set_display_timing(struct vpbe_fb_videomode *videomode)
{
	dispc_reg_out(VENC_HSPLS, videomode->hsync_len);
	dispc_reg_out(VENC_HSTART, videomode->left_margin);
	dispc_reg_out(VENC_HVALID, (videomode->xres));
	dispc_reg_out(VENC_HINT,
		      videomode->xres + videomode->left_margin +
		      videomode->right_margin - 1);

	dispc_reg_out(VENC_VSPLS, videomode->vsync_len);
	dispc_reg_out(VENC_VSTART, videomode->upper_margin);
	dispc_reg_out(VENC_VVALID, (videomode->yres));
	dispc_reg_out(VENC_VINT,
		      videomode->yres + videomode->upper_margin +
		      videomode->lower_margin);

	dispc_reg_out(VENC_VVALID, (videomode->yres));
};

void vpbe_davincifb_default_ycc16_config(void)
{
}

void vpbe_davincifb_default_ycc8_config(void)
{
}

void vpbe_davincifb_default_srgb_config(void)
{
}

void vpbe_davincifb_default_epson_config(void)
{
}

void vpbe_davincifb_default_casio_config(void)
{
}

void vpbe_davincifb_default_UDISP_config(void)
{
}

void vpbe_davincifb_default_STN_config(void)
{
}

/*
 * vpbe_davincifb_dlcd_nonstd_config 
 */
/* This function configures the digital lcd controller to 
 *	PRGB666 output setting.
 * Implementation is not tested 
 */
void vpbe_davincifb_dlcd_nonstd_config(void)
{
	volatile unsigned int venc, lcdout;
	venc = dispc_reg_in(VENC_VMOD) & VENC_VMOD_VENC;
	lcdout = dispc_reg_in(VENC_LCDOUT);
	vpbe_set_display_default();

	dispc_reg_out(OSD_BASEPX, dm->videomode.basepx);
	dispc_reg_out(OSD_BASEPY, dm->videomode.basepy);
	/* Set VIDCTL to select VCLKE = 1, VCLKZ =1, 
	   SYDIR = 0 (set o/p), DOMD = 0 */
	dispc_reg_merge(VENC_VIDCTL, 1 << VENC_VIDCTL_VCLKE_SHIFT,
			VENC_VIDCTL_VCLKE);
	dispc_reg_merge(VENC_VIDCTL, 0 << VENC_VIDCTL_VCLKZ_SHIFT,
			VENC_VIDCTL_VCLKZ);
	dispc_reg_merge(VENC_VIDCTL, 0 << VENC_VIDCTL_SYDIR_SHIFT,
			VENC_VIDCTL_SYDIR);
	dispc_reg_merge(VENC_VIDCTL, 0 << VENC_VIDCTL_YCDIR_SHIFT,
			VENC_VIDCTL_YCDIR);

	dispc_reg_merge(VENC_DCLKCTL,
			1 << VENC_DCLKCTL_DCKEC_SHIFT,
			VENC_DCLKCTL_DCKEC);
	dispc_reg_out(VENC_DCLKPTN0, 0x1);

	vpbe_set_display_timing(&dm->videomode);
	/* turning on horizontal and vertical syncs */
	if (dm->videomode.yres == 350) {
		dispc_reg_out(VENC_SYNCCTL,
			      (VENC_SYNCCTL_SYEV |
			       VENC_SYNCCTL_SYEH | VENC_SYNCCTL_VPL));
	} else if (dm->videomode.yres == 400) {
		dispc_reg_out(VENC_SYNCCTL,
			      (VENC_SYNCCTL_SYEV |
			       VENC_SYNCCTL_SYEH | VENC_SYNCCTL_HPL));
	} else {	/*dm->videomode.yres ==480 */

		dispc_reg_out(VENC_SYNCCTL,
			      (VENC_SYNCCTL_SYEV |
			       VENC_SYNCCTL_SYEH | VENC_SYNCCTL_HPL
			       | VENC_SYNCCTL_VPL));
	}
	/* Set OSD clock and OSD Sync Adavance registers */
	dispc_reg_out(VENC_OSDCLK0, 0);
	dispc_reg_out(VENC_OSDCLK1, 1);
	/* set VPSS clock */
	dispc_reg_out(VPSS_CLKCTL, 0x18);
	/* Configure VMOD. No change in VENC bit */
	dispc_reg_out(VENC_VMOD, 0x2010 | venc);
	dispc_reg_out(VENC_LCDOUT, lcdout);
}
