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
 * File: davincifb_venc.c	
 */

#include <video/davincifb.h>

extern vpbe_dm_info_t *dm;

void vpbe_enable_venc(int on)
{
	dispc_reg_merge(VENC_VMOD, on, VENC_VMOD_VENC);
}

void vpbe_enable_dacs(int on)
{
	if (on) {
		dispc_reg_out(VENC_DACTST, 0);
	} else {
		dispc_reg_out(VENC_DACTST, 0xF000);
	}
}

/*
 * Function to change window parameters as per display.
 */
void change_win_param(int flag)
{
	dm->vid0->win_pos.xpos = 0;
	dm->vid0->win_pos.ypos = 0;
	dm->vid0->info.var.xres = dm->videomode.xres;
	dm->vid0->info.var.yres = dm->videomode.yres;
	dm->vid0->info.var.vmode = dm->videomode.vmode;

	dm->vid1->win_pos.xpos = 0;
	dm->vid1->win_pos.ypos = 0;
	dm->vid1->info.var.xres = dm->videomode.xres;
	dm->vid1->info.var.yres = dm->videomode.yres;
	dm->vid1->info.var.vmode = dm->videomode.vmode;

	dm->osd0->win_pos.xpos = 0;
	dm->osd0->win_pos.ypos = 0;
	dm->osd0->info.var.xres = dm->videomode.xres;
	dm->osd0->info.var.yres = dm->videomode.yres;
	dm->osd0->info.var.vmode = dm->videomode.vmode;

	dm->osd1->win_pos.xpos = 0;
	dm->osd1->win_pos.ypos = 0;
	dm->osd1->info.var.xres = dm->videomode.xres;
	dm->osd1->info.var.yres = dm->videomode.yres;
	dm->osd1->info.var.vmode = dm->videomode.vmode;

	set_interlaced(dm->vid0->info.fix.id, dm->videomode.vmode);
	set_interlaced(dm->vid1->info.fix.id, dm->videomode.vmode);
	set_interlaced(dm->osd0->info.fix.id, dm->videomode.vmode);
	set_interlaced(dm->osd1->info.fix.id, dm->videomode.vmode);

	if (flag == FB_VMODE_INTERLACED) {
		set_win_position(dm->vid0->info.fix.id,
				 dm->vid0->win_pos.xpos,
				 dm->vid0->win_pos.ypos / 2,
				 dm->vid0->info.var.xres,
				 dm->vid0->info.var.yres / 2);
		set_win_position(dm->vid1->info.fix.id,
				 dm->vid1->win_pos.xpos,
				 dm->vid1->win_pos.ypos / 2,
				 dm->vid1->info.var.xres,
				 dm->vid1->info.var.yres / 2);
		set_win_position(dm->osd0->info.fix.id,
				 dm->osd0->win_pos.xpos,
				 dm->osd0->win_pos.ypos / 2,
				 dm->osd0->info.var.xres,
				 dm->osd0->info.var.yres / 2);
		set_win_position(dm->osd1->info.fix.id,
				 dm->osd1->win_pos.xpos,
				 dm->osd1->win_pos.ypos / 2,
				 dm->osd1->info.var.xres,
				 dm->osd1->info.var.yres / 2);
	} else {
		set_win_position(dm->vid0->info.fix.id,
				 dm->vid0->win_pos.xpos,
				 dm->vid0->win_pos.ypos,
				 dm->vid0->info.var.xres,
				 dm->vid0->info.var.yres);
		set_win_position(dm->vid1->info.fix.id,
				 dm->vid1->win_pos.xpos,
				 dm->vid1->win_pos.ypos,
				 dm->vid1->info.var.xres,
				 dm->vid1->info.var.yres);
		set_win_position(dm->osd0->info.fix.id,
				 dm->osd0->win_pos.xpos,
				 dm->osd0->win_pos.ypos,
				 dm->osd0->info.var.xres,
				 dm->osd0->info.var.yres);
		set_win_position(dm->osd1->info.fix.id,
				 dm->osd1->win_pos.xpos,
				 dm->osd1->win_pos.ypos,
				 dm->osd1->info.var.xres,
				 dm->osd1->info.var.yres);
	}
}

void davincifb_ntsc_composite_config(void)
{
	volatile unsigned int venc, dactst;
	dactst = dispc_reg_in(VENC_DACTST);
	venc = dispc_reg_in(VENC_VMOD) & VENC_VMOD_VENC;
	vpbe_set_display_default();
	/* to set DAC & VENC CLK to 54 MHz */
	dispc_reg_out(VPSS_CLKCTL, 0x18);

	/* to set VENC CLK DIV to 1 - final clock is 54 MHz */
	dispc_reg_merge(VPBE_PCR, 0, VPBE_PCR_VENC_DIV);

	/* set OSD Divisor to 1 */
	dispc_reg_out(VENC_OSDCLK0, 1);
	dispc_reg_out(VENC_OSDCLK1, 2);

	dispc_reg_merge(VENC_VDPRO, 0, VENC_VDPRO_DAFRQ);
	dispc_reg_merge(VENC_VDPRO, 0, VENC_VDPRO_DAUPS);
	/* Set Base Pixel X and Base Pixel Y */
	dispc_reg_out(OSD_BASEPX, dm->videomode.basepx);
	dispc_reg_out(OSD_BASEPY, dm->videomode.basepy);

	/* Set REC656 Mode */
	dispc_reg_out(VENC_YCCCTL, 0x1);

	/* Configure VMOD. No change in VENC bit */
	dispc_reg_out(VENC_VMOD, 0x1002 | venc);
	dispc_reg_out(VENC_DACTST, dactst);

	/* Change window parameters for interlaced display */
	change_win_param(FB_VMODE_INTERLACED);
}

void davincifb_ntsc_svideo_config(void)
{
	volatile unsigned int venc, dactst;
	dactst = dispc_reg_in(VENC_DACTST);
	venc = dispc_reg_in(VENC_VMOD) & VENC_VMOD_VENC;
	vpbe_set_display_default();
	/* to set DAC & VENC CLK to 54 MHz */
	dispc_reg_out(VPSS_CLKCTL, 0x18);
	/* to set VENC CLK DIV to 1 - final clock is 54 MHz */
	dispc_reg_merge(VPBE_PCR, 0, VPBE_PCR_VENC_DIV);
	/*  set OSD Divisor to 1 */
	dispc_reg_out(VENC_OSDCLK0, 1);
	dispc_reg_out(VENC_OSDCLK1, 2);

	dispc_reg_merge(VENC_VDPRO, 0, VENC_VDPRO_DAFRQ);
	dispc_reg_merge(VENC_VDPRO, 0, VENC_VDPRO_DAUPS);
	/* Set Base Pixel X and Base Pixel Y */
	dispc_reg_out(OSD_BASEPX, dm->videomode.basepx);
	dispc_reg_out(OSD_BASEPY, dm->videomode.basepy);

	/* Set REC656 Mode */
	dispc_reg_out(VENC_YCCCTL, 0x1);

	/* Configure VMOD. No change in VENC bit */
	dispc_reg_out(VENC_VMOD, 0x1002 | venc);
	dispc_reg_out(VENC_DACTST, dactst);

	/* Enable S-Video Output; DAC B: S-Video Y, DAC C: S-Video C  */
	dispc_reg_out(VENC_DACSEL, 0x210);

	/* Change window parameters for interlaced display */
	change_win_param(FB_VMODE_INTERLACED);
}

void davincifb_ntsc_component_config(void)
{
	volatile unsigned int venc, dactst;
	dactst = dispc_reg_in(VENC_DACTST);
	venc = dispc_reg_in(VENC_VMOD) & VENC_VMOD_VENC;
	vpbe_set_display_default();
	/* Set Base Pixel X and Base Pixel Y */
	dispc_reg_out(OSD_BASEPX, dm->videomode.basepx);
	dispc_reg_out(OSD_BASEPY, dm->videomode.basepy);

	/* to set DAC & VENC CLK to 54 MHz */
	dispc_reg_out(VPSS_CLKCTL, 0x18);

	/* To set VENC CLK DIV to 1 - final clock is 54 MHz */
	dispc_reg_merge(VPBE_PCR, 0, VPBE_PCR_VENC_DIV);

	/* set OSD Divisor to 1 */
	dispc_reg_out(VENC_OSDCLK0, 1);
	dispc_reg_out(VENC_OSDCLK1, 2);

	dispc_reg_merge(VENC_VDPRO, 0, VENC_VDPRO_DAFRQ);
	dispc_reg_merge(VENC_VDPRO, 0, VENC_VDPRO_DAUPS);

	/* Set REC656 Mode */
	dispc_reg_out(VENC_YCCCTL, 0x1);

	/* Configure VMOD. No change in VENC bit */
	dispc_reg_out(VENC_VMOD, 0x1002 | venc);
	dispc_reg_out(VENC_DACTST, dactst);

	/* Enable Component output; DAC A: Y, DAC B: Pb, DAC C: Pr  */
	dispc_reg_out(VENC_DACSEL, 0x543);

	/* Change window parameters for interlaced display */
	change_win_param(FB_VMODE_INTERLACED);
}

void davincifb_pal_composite_config(void)
{
	volatile unsigned int venc, dactst;
	dactst = dispc_reg_in(VENC_DACTST);
	venc = dispc_reg_in(VENC_VMOD) & VENC_VMOD_VENC;
	vpbe_set_display_default();
	/* to set DAC & VENC CLK to 54 MHz */
	dispc_reg_out(VPSS_CLKCTL, 0x18);

	/* to set VENC CLK DIV to 1 - final clock is 54 MHz */
	dispc_reg_merge(VPBE_PCR, 0, VPBE_PCR_VENC_DIV);

	/* set OSD Divisor to 1 */
	dispc_reg_out(VENC_OSDCLK0, 1);
	dispc_reg_out(VENC_OSDCLK1, 2);

	dispc_reg_merge(VENC_VDPRO, 0, VENC_VDPRO_DAFRQ);
	dispc_reg_merge(VENC_VDPRO, 0, VENC_VDPRO_DAUPS);

	dispc_reg_out(OSD_BASEPX, dm->videomode.basepx);
	dispc_reg_out(OSD_BASEPY, dm->videomode.basepy);

	/* Set REC656 Mode */
	dispc_reg_out(VENC_YCCCTL, 0x1);

	/* Configure VMOD. No change in VENC bit */
	dispc_reg_out(VENC_VMOD, 0x1042 | venc);
	dispc_reg_out(VENC_DACTST, dactst);

	/* Change window parameters for interlaced display */
	change_win_param(FB_VMODE_INTERLACED);
}

void davincifb_pal_svideo_config(void)
{
	volatile unsigned int venc, dactst;
	dactst = dispc_reg_in(VENC_DACTST);
	venc = dispc_reg_in(VENC_VMOD) & VENC_VMOD_VENC;
	vpbe_set_display_default();
	/* to set DAC & VENC CLK to 54 MHz */
	dispc_reg_out(VPSS_CLKCTL, 0x18);

	dispc_reg_merge(VPBE_PCR, 0, VPBE_PCR_VENC_DIV);

	dispc_reg_out(VENC_OSDCLK0, 1);
	dispc_reg_out(VENC_OSDCLK1, 2);

	dispc_reg_merge(VENC_VDPRO, 0, VENC_VDPRO_DAFRQ);
	dispc_reg_merge(VENC_VDPRO, 0, VENC_VDPRO_DAUPS);

	dispc_reg_out(OSD_BASEPX, dm->videomode.basepx);
	dispc_reg_out(OSD_BASEPY, dm->videomode.basepy);

	/* Set REC656 Mode */
	dispc_reg_out(VENC_YCCCTL, 0x1);

	/* Configure VMOD. No change in VENC bit */
	dispc_reg_out(VENC_VMOD, 0x1042 | venc);
	dispc_reg_out(VENC_DACTST, dactst);

	/* Enable S-Video Output; DAC B: S-Video Y, DAC C: S-Video C  */
	dispc_reg_out(VENC_DACSEL, 0x210);

	/* Change window parameters for interlaced display */
	change_win_param(FB_VMODE_INTERLACED);
}

void davincifb_pal_component_config(void)
{
	volatile unsigned int venc, dactst;
	dactst = dispc_reg_in(VENC_DACTST);
	venc = dispc_reg_in(VENC_VMOD) & VENC_VMOD_VENC;
	vpbe_set_display_default();

	dispc_reg_out(VPSS_CLKCTL, 0x18);

	dispc_reg_merge(VPBE_PCR, 0, VPBE_PCR_VENC_DIV);

	dispc_reg_out(VENC_OSDCLK0, 1);
	dispc_reg_out(VENC_OSDCLK1, 2);

	dispc_reg_merge(VENC_VDPRO, 0, VENC_VDPRO_DAFRQ);
	dispc_reg_merge(VENC_VDPRO, 0, VENC_VDPRO_DAUPS);

	dispc_reg_out(OSD_BASEPX, dm->videomode.basepx);
	dispc_reg_out(OSD_BASEPY, dm->videomode.basepy);

	/* Set REC656 Mode */
	dispc_reg_out(VENC_YCCCTL, 0x1);

	/* Configure VMOD. No change in VENC bit */
	dispc_reg_out(VENC_VMOD, 0x1042 | venc);
	dispc_reg_out(VENC_DACTST, dactst);

	/* Enable Component output; DAC A: Y, DAC B: Pb, DAC C: Pr  */
	dispc_reg_out(VENC_DACSEL, 0x543);

	/* Change window parameters for interlaced display */
	change_win_param(FB_VMODE_INTERLACED);
}

/*
 * vpbe_davincifb_ntsc_rgb_config 
 */
/* This function configures the video encoder to NTSC RGB setting.*/
void vpbe_davincifb_ntsc_rgb_config(void)
{
	vpbe_set_display_default();

	dispc_reg_out(OSD_BASEPX, dm->videomode.basepx);
	dispc_reg_out(OSD_BASEPY, dm->videomode.basepy);

	dispc_reg_merge(VENC_CMPNT, 1 << VENC_CMPNT_MRGB_SHIFT,
			VENC_CMPNT_MRGB);
	/* Set VMOD to select VENC =ENABLE, SDTV_NTSC, HDMD = SDTV */
	/* Enable Composite output and start video encoder */

	dispc_reg_merge(VENC_VMOD,
			SDTV_NTSC << VENC_VMOD_TVTYP_SHIFT,
			VENC_VMOD_TVTYP);
	/* Enable Component output; DAC A: Y, DAC B: Pb, DAC C: Pr  */
	dispc_reg_out(VENC_DACSEL, 0x543);
}

/*
 * vpbe_davincifb_pal_rgb_config 
 */
/* This function configures the video encoder to PAL RGB setting.*/
void vpbe_davincifb_pal_rgb_config(void)
{
	vpbe_set_display_default();

	dispc_reg_out(OSD_BASEPX, dm->videomode.basepx);
	dispc_reg_out(OSD_BASEPY, dm->videomode.basepy);

	dispc_reg_merge(VENC_CMPNT, 1 << VENC_CMPNT_MRGB_SHIFT,
			VENC_CMPNT_MRGB);
	/* Set VMOD to select SDTV_NTSC, HDMD = SDTV */
	dispc_reg_merge(VENC_VMOD,
			SDTV_PAL << VENC_VMOD_TVTYP_SHIFT,
			VENC_VMOD_TVTYP);
	/* Enable Component output; DAC A: Y, DAC B: Pb, DAC C: Pr  */
	dispc_reg_out(VENC_DACSEL, 0x543);
}

/*
 * vpbe_davincifb_525p_component_config 
 */
/* This function configures the video encoder to HDTV(525p) component setting.*/
void vpbe_davincifb_525p_component_config(void)
{
	vpbe_set_display_default();

	dispc_reg_out(OSD_BASEPX, dm->videomode.basepx);
	dispc_reg_out(OSD_BASEPY, dm->videomode.basepy);

	dispc_reg_out(VPSS_CLKCTL, 0x19);

	dispc_reg_merge(VPBE_PCR, VPBE_PCR_VENC_DIV, VPBE_PCR_VENC_DIV);

	dispc_reg_out(VENC_OSDCLK0, 0);
	dispc_reg_out(VENC_OSDCLK1, 1);

	dispc_reg_merge(VENC_VDPRO, VENC_VDPRO_DAFRQ, VENC_VDPRO_DAFRQ);
	dispc_reg_merge(VENC_VDPRO, VENC_VDPRO_DAUPS, VENC_VDPRO_DAUPS);

	/* Enable Composite output and start video encoder */

	/* Set VMOD to select  
	   TVTYP = HDTV 525, HDMD = HDTV, ITLC = Non_interlace */
	dispc_reg_merge(VENC_VMOD, 1<<VENC_VMOD_VIE_SHIFT, VENC_VMOD_VIE);
	dispc_reg_merge(VENC_VMOD,
			VENC_VMOD_VDMD_YCBCR8 <<
			VENC_VMOD_VDMD_SHIFT, VENC_VMOD_VDMD);
	dispc_reg_merge(VENC_VMOD, VENC_VMOD_HDMD, VENC_VMOD_HDMD);

	dispc_reg_merge(VENC_VMOD,
			HDTV_525P << VENC_VMOD_TVTYP_SHIFT,
			VENC_VMOD_TVTYP);

	/* Enable Component output; DAC A: Y, DAC B: Pb, DAC C: Pr  */
	dispc_reg_out(VENC_DACSEL, 0x543);

	/* Set REC656 Mode */
	dispc_reg_out(VENC_YCCCTL, 0x1);

	/* Change window parameters for progressive display */
	change_win_param(FB_VMODE_NONINTERLACED);
}

/*
 * vpbe_davincifb_625p_component_config 
 */
/* This function configures the video encoder to HDTV(625p) component setting.*/
void vpbe_davincifb_625p_component_config(void)
{
	vpbe_set_display_default();

	dispc_reg_out(OSD_BASEPX, dm->videomode.basepx);
	dispc_reg_out(OSD_BASEPY, dm->videomode.basepy);

	dispc_reg_out(VPSS_CLKCTL, 0x19);

	dispc_reg_merge(VPBE_PCR, VPBE_PCR_VENC_DIV, VPBE_PCR_VENC_DIV);

	dispc_reg_out(VENC_OSDCLK0, 0);
	dispc_reg_out(VENC_OSDCLK1, 1);

	dispc_reg_merge(VENC_VDPRO, VENC_VDPRO_DAFRQ, VENC_VDPRO_DAFRQ);
	dispc_reg_merge(VENC_VDPRO, VENC_VDPRO_DAUPS, VENC_VDPRO_DAUPS);

	/* Enable Composite output and start video encoder */

	/* Set VMOD to select  
	   TVTYP = HDTV 525, HDMD = HDTV, ITLC = Non_interlace */
	dispc_reg_merge(VENC_VMOD, 1<<VENC_VMOD_VIE_SHIFT, VENC_VMOD_VIE);
	dispc_reg_merge(VENC_VMOD,
			VENC_VMOD_VDMD_YCBCR8 <<
			VENC_VMOD_VDMD_SHIFT, VENC_VMOD_VDMD);
	dispc_reg_merge(VENC_VMOD, VENC_VMOD_HDMD, VENC_VMOD_HDMD);

	dispc_reg_merge(VENC_VMOD,
			HDTV_625P << VENC_VMOD_TVTYP_SHIFT,
			VENC_VMOD_TVTYP);

	/* Enable Component output; DAC A: Y, DAC B: Pb, DAC C: Pr  */
	dispc_reg_out(VENC_DACSEL, 0x543);

	/* Set REC656 Mode */
	dispc_reg_out(VENC_YCCCTL, 0x1);

	/* Change window parameters for progressive display */
	change_win_param(FB_VMODE_NONINTERLACED);
}
