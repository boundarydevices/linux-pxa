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
 * File: davincifb_osd.c	
 */

//#include "davinci_vpbe.h"
#include <video/davincifb.h>

extern vpbe_dm_info_t *dm;
extern struct vpbe_fb_videomode *modelist[];

/*
 * vpbe_enable_window 
 */
/* Enables the window */
int vpbe_enable_window(vpbe_dm_win_info_t * win)
{
	int result = 0;

	set_win_enable(win->info.fix.id, VPBE_ENABLE);

	win->window_enable = VPBE_ENABLE;
	return result;
}

/*
 * vpbe_disable_window	
 */
/* The Function	disables the window */
int vpbe_disable_window(vpbe_dm_win_info_t * win)
{
	int result = 0;

	set_win_enable(win->info.fix.id, VPBE_DISABLE);
	win->window_enable = VPBE_DISABLE;
	return result;
}

/*
 * vpbe_bitmap_set_blend_factor 
 */
/* This	is used	for setting the	blend factor for the bitmap window.*/
int
vpbe_bitmap_set_blend_factor(u8 * id, vpbe_bitmap_blend_params_t * blend_para)
{
	int result = 0;

	/* If function call is not for OSD window, return error */
	if (!is_win(id, OSD0) && !is_win(id, OSD1))
		return -VPBE_WRONG_WINDOW_ID;

	/* Error checkingfor parameter passed */
	if ((blend_para->enable_colorkeying > VPBE_ENABLE)
	    || (blend_para->bf > 7) || (blend_para->colorkey > 0xFFFF))
		return -VPBE_WRONG_WINDOW_ID;

	if (is_win(id, OSD0)) {
		dispc_reg_merge(OSD_OSDWIN0MD,
				(blend_para->
				 enable_colorkeying) <<
				OSD_OSDWIN0MD_TE0_SHIFT, OSD_OSDWIN0MD_TE0);
		dispc_reg_merge(OSD_OSDWIN0MD,
				(blend_para->
				 bf) << OSD_OSDWIN0MD_BLND0_SHIFT,
				OSD_OSDWIN0MD_BLND0);
		dm->osd0->conf_params.bitmap_params.blend_info = *blend_para;
	} else {
		dispc_reg_merge(OSD_OSDWIN1MD,
				(blend_para->
				 enable_colorkeying) <<
				OSD_OSDWIN1MD_TE1_SHIFT, OSD_OSDWIN1MD_TE1);
		dispc_reg_merge(OSD_OSDWIN1MD,
				(blend_para->
				 bf) << OSD_OSDWIN1MD_BLND1_SHIFT,
				OSD_OSDWIN1MD_BLND1);
		dm->osd1->conf_params.bitmap_params.blend_info = *blend_para;
	}
	dispc_reg_merge(OSD_TRANSPVA,
			(blend_para->
			 colorkey) << OSD_TRANSPVA_RGBTRANS_SHIFT,
			OSD_TRANSPVA_RGBTRANS);

	return result;
}

/*
 * vpbe_bitmap_set_ram_clut 
 */
/* This	function allows	user to	setup the color lookup table (CLUT)in RAM.*/
int vpbe_bitmap_set_ram_clut()
{
	int result = 0;
	int i;

	for (i = 0; i < 256; i++) {
		/* Wait for the MISCCTL.CPBSY bit to be zero */
		while ((dispc_reg_in(OSD_MISCCT) & OSD_MISCCT_CPBSY)) ;
		/* Set CLUTRAMCR.CADDR to current loop count */
		dispc_reg_merge(OSD_CLUTRAMC, i, OSD_CLUTRAM_CADDR);
		/* Write Cr value in CLUTRAMCR.CR field */
		dispc_reg_merge(OSD_CLUTRAMC,
				(dm->
				 ram_clut[i][2]) << OSD_CLUTRAM_CR_SHIFT,
				OSD_CLUTRAM_CR);

		/* Write Y value in CLUTRAMYCB.Y field */
		dispc_reg_merge(OSD_CLUTRAMYC,
				(dm->
				 ram_clut[i][0]) << OSD_CLUTRAMY_Y_SHIFT,
				OSD_CLUTRAMY_Y);
		/* Write Cb value in CLUTRAMYCB.CB field */
		dispc_reg_merge(OSD_CLUTRAMYC,
				(dm->
				 ram_clut[i][1]) << OSD_CLUTRAMY_CB_SHIFT,
				OSD_CLUTRAMY_CB);
	}
	return result;
}

/*
 * vpbe_bitmap_set_as_attribute 
 */
/* Configure OSD1 as bitmap window. */
int vpbe_enable_disable_attribute_window(u32 flag)
{
	int result = 0;
	if ((flag != 0) && (flag != 1)){
		return -VPBE_INVALID_PARA_VALUE;
	}
/* 'flag' must be more then 8 bit because we are going to shift it by 16 */
	if (flag) {
		dispc_reg_merge(OSD_OSDWIN1MD,
				VPBE_ENABLE << OSD_OSDWIN1MD_OASW_SHIFT,
				OSD_OSDWIN1MD_OASW);
		dispc_reg_merge(OSD_OSDWIN1MD,
				2 << OSD_OSDWIN1MD_BMW1_SHIFT,
				OSD_OSDWIN1MD_BMW1);
		dm->osd1->conf_params.bitmap_params.enable_attribute =
		    VPBE_ENABLE;
		dm->osd1->conf_params.bitmap_params.stored_bits_per_pixel =
		    dm->osd1->info.var.bits_per_pixel;
		dm->osd1->info.var.bits_per_pixel = VPBE_BITMAP_BIT_4;
	} else {
		dispc_reg_merge(OSD_OSDWIN1MD,
				VPBE_DISABLE << OSD_OSDWIN1MD_OASW_SHIFT,
				OSD_OSDWIN1MD_OASW);
		dm->osd1->conf_params.bitmap_params.enable_attribute =
		    VPBE_DISABLE;
		if (dm->osd1->conf_params.bitmap_params.stored_bits_per_pixel) {

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

			dm->osd1->info.var.bits_per_pixel =
			    dm->osd1->conf_params.bitmap_params.
			    stored_bits_per_pixel;
		}
	}
	return result;
}

/*
 *  vpbe_set_blinking 
 */
/* Set the OSD window 0 blinking and  blink interval. */
int vpbe_set_blinking(u8 * id, vpbe_blink_option_t * required)
{
	int result = 0;

	if (!is_win(id, OSD1)) {
		return -VPBE_WRONG_WINDOW_ID;
	}
	if ((required->blinking > VPBE_ENABLE) || (required->interval > 3))
		return -VPBE_INVALID_PARA_VALUE;

	dispc_reg_merge(OSD_OSDATRMD, required->blinking, OSD_OSDATRMD_BLNK);
	dispc_reg_merge(OSD_OSDATRMD,
			required->interval << OSD_OSDATRMD_BLNKINT_SHIFT,
			OSD_OSDATRMD_BLNKINT);

	dm->osd1->conf_params.bitmap_params.blink_info = *required;

	return result;
}

/*
 * vpbe_get_blinking 
 */
/* This	function is used to get	the OSD	window 0 blinking params.*/
int vpbe_get_blinking(u8 * id, vpbe_blink_option_t * current_conf)
{
	int result = 0;

	if (!is_win(id, OSD1)) {
		return -VPBE_WRONG_WINDOW_ID;
	}
	*current_conf = dm->osd1->conf_params.bitmap_params.blink_info;

	return result;
}

/*
 * vpbe_set_vid_params 
 */
/* This	function sets the cb-cr	order in the pixel data	for video windows. */
int vpbe_set_vid_params(u8 * id, vpbe_video_config_params_t * required)
{
	int result = 0;

	/* Validate id. Return negative in case of error. */
	if (!is_win(id, VID0) && !is_win(id, VID1))
		return -VPBE_WRONG_WINDOW_ID;

	if (is_win(id, VID0)) {
		dispc_reg_merge(OSD_MODE,
				required->cb_cr_order << OSD_MODE_CS_SHIFT,
				OSD_MODE_CS);
		vpbe_set_vid_expansion(&required->exp_info);
		dm->vid0->conf_params.video_params.cb_cr_order =
		    required->cb_cr_order;
		dm->vid0->conf_params.video_params.exp_info =
		    required->exp_info;
	} else {
		dispc_reg_merge(OSD_MODE,
				required->cb_cr_order << OSD_MODE_CS_SHIFT,
				OSD_MODE_CS);
		dm->vid1->conf_params.video_params.cb_cr_order =
		    required->cb_cr_order;
	}

	return result;
}

/*
 * vpbe_get_vid_params
 */
/* This	function gives video windows cb-cr order in the pixel data. */
int vpbe_get_vid_params(u8 * id, vpbe_video_config_params_t * current_conf)
{
	int result = 0;
	if (is_win(id, OSD0) || is_win(id, OSD1))
		return -VPBE_WRONG_WINDOW_ID;

	if (is_win(id, VID0)) {
		current_conf->cb_cr_order =
		    dm->vid0->conf_params.video_params.cb_cr_order;
		current_conf->exp_info.horizontal =
		    dm->vid0->conf_params.video_params.exp_info.horizontal;
		current_conf->exp_info.vertical =
		    dm->vid0->conf_params.video_params.exp_info.vertical;
	} else
		current_conf->cb_cr_order =
		    dm->vid1->conf_params.video_params.cb_cr_order;

	return result;
}

/*
 * vpbe_bitamp_get_params
 */
/* This	function sets the configurations of the	bitmap window. */
int vpbe_bitmap_get_params(u8 * id, struct vpbe_bitmap_config_params *required)
{
	int result = 0;

	if (is_win(id, VID0) || is_win(id, VID1))
		return -VPBE_WRONG_WINDOW_ID;

	if (is_win(id, OSD0)) {
		required->attenuation_enable =
		    dm->osd0->conf_params.bitmap_params.attenuation_enable;
		required->clut_idx =
		    dm->osd0->conf_params.bitmap_params.clut_idx;
		required->clut_select =
		    dm->osd0->conf_params.bitmap_params.clut_select;
	} else {
		required->attenuation_enable =
		    dm->osd1->conf_params.bitmap_params.attenuation_enable;
		required->clut_idx =
		    dm->osd1->conf_params.bitmap_params.clut_idx;
		required->clut_select =
		    dm->osd1->conf_params.bitmap_params.clut_select;
	}

	return result;
}

/*
 * vpbe_bitamp_set_params	
 */
/* This	function sets the configurations of the	bitmap window. */
int vpbe_bitmap_set_params(u8 * id, struct vpbe_bitmap_config_params *required)
{
	int result = 0;

	if (is_win(id, VID0) || is_win(id, VID1))
		return -VPBE_WRONG_WINDOW_ID;

	if (required->clut_select > 2)
		return -VPBE_INVALID_PARA_VALUE;

	if (is_win(id, OSD0)) {
		if (((dm->osd1->conf_params.bitmap_params.clut_select == SET_1)
		     && (required->clut_select == SET_0))
		    ||
		    ((dm->osd1->conf_params.bitmap_params.clut_select == SET_0)
		     && (required->clut_select == SET_1)))
			return -VPBE_INVALID_PARA_VALUE;
	} else {
		if (((dm->osd0->conf_params.bitmap_params.clut_select == SET_1)
		     && (required->clut_select == SET_0))
		    ||
		    ((dm->osd0->conf_params.bitmap_params.clut_select == SET_0)
		     && (required->clut_select == SET_1)))
			return -VPBE_INVALID_PARA_VALUE;
	}
	if (is_win(id, OSD0)) {
		if (required->clut_select == 2) {	/* RAM CLUT */
			dispc_reg_merge(OSD_OSDWIN0MD,
					SET_1 << OSD_OSDWIN0MD_CLUTS0_SHIFT,
					OSD_OSDWIN0MD_CLUTS0);
		} else if (required->clut_select == 1) {	/* ROM1 CLUT */
			dispc_reg_merge(OSD_OSDWIN0MD,
					SET_0 << OSD_OSDWIN0MD_CLUTS0_SHIFT,
					OSD_OSDWIN0MD_CLUTS0);
			dispc_reg_merge(OSD_MISCCT,
					SET_1 << OSD_MISCCT_RSEL_SHIFT,
					OSD_MISCCT_RSEL);
		} else {	/* ROM0 CLUT */
			dispc_reg_merge(OSD_OSDWIN0MD,
					SET_0 << OSD_OSDWIN0MD_CLUTS0_SHIFT,
					OSD_OSDWIN1MD_CLUTS0);
			dispc_reg_merge(OSD_MISCCT,
					SET_0 << OSD_MISCCT_RSEL_SHIFT,
					OSD_MISCCT_RSEL);
		}
		dispc_reg_merge(OSD_OSDWIN0MD,
				required->
				attenuation_enable << OSD_OSDWIN0MD_ATN0E_SHIFT,
				OSD_OSDWIN0MD_ATN0E);
		if ((dm->osd0->info.var.bits_per_pixel == VPBE_BITMAP_BIT_1)
		    || (dm->osd0->info.var.bits_per_pixel == VPBE_BITMAP_BIT_2)
		    || (dm->osd0->info.var.bits_per_pixel == VPBE_BITMAP_BIT_4)) {
			dispc_reg_merge(OSD_W0BMP01,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_0 <<
					OSD_W0BMP01_PAL00_SHIFT,
					OSD_W0BMP01_PAL00);
			dispc_reg_merge(OSD_W0BMP01,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_1 <<
					OSD_W0BMP01_PAL01_SHIFT,
					OSD_W0BMP01_PAL01);
			dispc_reg_merge(OSD_W0BMP23,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_2 <<
					OSD_W0BMP23_PAL02_SHIFT,
					OSD_W0BMP23_PAL02);
			dispc_reg_merge(OSD_W0BMP23,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_3 <<
					OSD_W0BMP23_PAL03_SHIFT,
					OSD_W0BMP23_PAL03);
			dispc_reg_merge(OSD_W0BMP45,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_4 <<
					OSD_W0BMP45_PAL04_SHIFT,
					OSD_W0BMP45_PAL04);
			dispc_reg_merge(OSD_W0BMP45,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_5 <<
					OSD_W0BMP45_PAL05_SHIFT,
					OSD_W0BMP45_PAL05);
			dispc_reg_merge(OSD_W0BMP67,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_6 <<
					OSD_W0BMP67_PAL06_SHIFT,
					OSD_W0BMP67_PAL06);
			dispc_reg_merge(OSD_W0BMP67,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_7 <<
					OSD_W0BMP67_PAL07_SHIFT,
					OSD_W0BMP67_PAL07);
			dispc_reg_merge(OSD_W0BMP89,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_8 <<
					OSD_W0BMP89_PAL08_SHIFT,
					OSD_W0BMP89_PAL08);
			dispc_reg_merge(OSD_W0BMP89,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_9 <<
					OSD_W0BMP89_PAL09_SHIFT,
					OSD_W0BMP89_PAL09);
			dispc_reg_merge(OSD_W0BMPAB,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_10 <<
					OSD_W0BMPAB_PAL10_SHIFT,
					OSD_W0BMPAB_PAL10);
			dispc_reg_merge(OSD_W0BMPAB,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_11 <<
					OSD_W0BMPAB_PAL11_SHIFT,
					OSD_W0BMPAB_PAL11);
			dispc_reg_merge(OSD_W0BMPCD,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_12 <<
					OSD_W0BMPCD_PAL12_SHIFT,
					OSD_W0BMPCD_PAL12);
			dispc_reg_merge(OSD_W0BMPCD,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_13 <<
					OSD_W0BMPCD_PAL13_SHIFT,
					OSD_W0BMPCD_PAL13);
			dispc_reg_merge(OSD_W0BMPEF,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_14 <<
					OSD_W0BMPEF_PAL14_SHIFT,
					OSD_W0BMPEF_PAL14);
			dispc_reg_merge(OSD_W0BMPEF,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_15 <<
					OSD_W0BMPEF_PAL15_SHIFT,
					OSD_W0BMPEF_PAL15);
		}
		dm->osd0->conf_params.bitmap_params.clut_idx =
		    required->clut_idx;
		dm->osd0->conf_params.bitmap_params.attenuation_enable =
		    required->attenuation_enable;
		dm->osd0->conf_params.bitmap_params.clut_select =
		    required->clut_select;
	} else if (is_win(id, OSD1)) {
		if (required->clut_select == 2) {	/* RAM CLUT */
			dispc_reg_merge(OSD_OSDWIN1MD,
					SET_1 << OSD_OSDWIN1MD_CLUTS1_SHIFT,
					OSD_OSDWIN1MD_CLUTS1);
		} else if (required->clut_select == 1) {	/* ROM1 CLUT */
			dispc_reg_merge(OSD_OSDWIN1MD,
					SET_0 << OSD_OSDWIN1MD_CLUTS1_SHIFT,
					OSD_OSDWIN1MD_CLUTS1);
			dispc_reg_merge(OSD_MISCCT,
					SET_1 << OSD_MISCCT_RSEL_SHIFT,
					OSD_MISCCT_RSEL);
		} else {	/* ROM0 CLUT */
			dispc_reg_merge(OSD_OSDWIN1MD,
					SET_0 << OSD_OSDWIN1MD_CLUTS1_SHIFT,
					OSD_OSDWIN1MD_CLUTS1);
			dispc_reg_merge(OSD_MISCCT,
					SET_0 << OSD_MISCCT_RSEL_SHIFT,
					OSD_MISCCT_RSEL);
		}
		dispc_reg_merge(OSD_OSDWIN1MD,
				required->
				attenuation_enable <<
				OSD_OSDWIN1MD_ATN1E_SHIFT, OSD_OSDWIN1MD_ATN1E);
		if ((dm->osd1->info.var.bits_per_pixel == VPBE_BITMAP_BIT_1)
		    || (dm->osd1->info.var.bits_per_pixel == VPBE_BITMAP_BIT_2)
		    || (dm->osd1->info.var.bits_per_pixel == VPBE_BITMAP_BIT_4)) {
			dispc_reg_merge(OSD_W1BMP0,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_0 << OSD_W1BMP0_PAL00_SHIFT,
					OSD_W1BMP0_PAL00);
			dispc_reg_merge(OSD_W1BMP0,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_1 << OSD_W1BMP0_PAL01_SHIFT,
					OSD_W1BMP0_PAL01);
			dispc_reg_merge(OSD_W1BMP2,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_2 << OSD_W1BMP2_PAL02_SHIFT,
					OSD_W1BMP2_PAL02);
			dispc_reg_merge(OSD_W1BMP2,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_3 << OSD_W1BMP2_PAL03_SHIFT,
					OSD_W1BMP2_PAL03);
			dispc_reg_merge(OSD_W1BMP4,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_4 << OSD_W1BMP4_PAL04_SHIFT,
					OSD_W1BMP4_PAL04);
			dispc_reg_merge(OSD_W1BMP4,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_5 << OSD_W1BMP4_PAL05_SHIFT,
					OSD_W1BMP4_PAL05);
			dispc_reg_merge(OSD_W1BMP6,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_6 << OSD_W1BMP6_PAL06_SHIFT,
					OSD_W1BMP6_PAL06);
			dispc_reg_merge(OSD_W1BMP6,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_7 << OSD_W1BMP6_PAL07_SHIFT,
					OSD_W1BMP6_PAL07);
			dispc_reg_merge(OSD_W1BMP8,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_8 << OSD_W1BMP8_PAL08_SHIFT,
					OSD_W1BMP8_PAL08);
			dispc_reg_merge(OSD_W1BMP8,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_9 << OSD_W1BMP8_PAL09_SHIFT,
					OSD_W1BMP8_PAL09);
			dispc_reg_merge(OSD_W1BMPA,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_10 << OSD_W1BMPA_PAL10_SHIFT,
					OSD_W1BMPA_PAL10);
			dispc_reg_merge(OSD_W1BMPA,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_11 << OSD_W1BMPA_PAL11_SHIFT,
					OSD_W1BMPA_PAL11);
			dispc_reg_merge(OSD_W1BMPC,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_12 << OSD_W1BMPC_PAL12_SHIFT,
					OSD_W1BMPC_PAL12);
			dispc_reg_merge(OSD_W1BMPC,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_13 << OSD_W1BMPC_PAL13_SHIFT,
					OSD_W1BMPC_PAL13);
			dispc_reg_merge(OSD_W1BMPE,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_14 << OSD_W1BMPE_PAL14_SHIFT,
					OSD_W1BMPE_PAL14);
			dispc_reg_merge(OSD_W1BMPE,
					required->clut_idx.for_4bit_bimap.
					bitmap_val_15 << OSD_W1BMPE_PAL15_SHIFT,
					OSD_W1BMPE_PAL15);
		}
		dm->osd1->conf_params.bitmap_params.clut_idx =
		    required->clut_idx;
		dm->osd1->conf_params.bitmap_params.attenuation_enable =
		    required->attenuation_enable;
		dm->osd1->conf_params.bitmap_params.clut_select =
		    required->clut_select;
	}
	return result;
}

/*
 * vpbe_set_cursor_params	
 */
/* Function to set cursor window parameters	*/
int vpbe_set_cursor_params(struct fb_cursor *required)
{
	/* Validation for cursor configuration parameters */
	/* Rule cursor must be fully contained inside vid0 window */
	if ((!within_vid0_limits
	     (required->image.dx, required->image.dy,
	      required->image.width, required->image.height))
	    || (required->image.fg_color > 0xff)
	    || (required->image.width > MAX_CURSOR_WIDTH)
	    || (required->image.height > MAX_CURSOR_HEIGHT)
	    || (required->image.depth > MAX_CURSOR_LINEWIDTH))
		return -VPBE_INVALID_PARA_VALUE;

	dm->cursor = *required;

	dispc_reg_merge(OSD_RECTCUR, dm->cursor.enable, OSD_RECTCUR_RCACT);
	dispc_reg_out(OSD_CURXP, dm->cursor.image.dx);
	dispc_reg_out(OSD_CURXL, dm->cursor.image.width);

	/* Field/Frame setting for Cursor are kept as same as VID0 window. */
	if (dm->videomode.vmode == FB_VMODE_INTERLACED) {
		dispc_reg_out(OSD_CURYP, dm->cursor.image.dy / 2);
		dispc_reg_out(OSD_CURYL, dm->cursor.image.height / 2);
	} else {
		dispc_reg_out(OSD_CURYP, dm->cursor.image.dy);
		dispc_reg_out(OSD_CURYL, dm->cursor.image.height);
	}

	dispc_reg_merge(OSD_RECTCUR,
			dm->cursor.image.depth << OSD_RECTCUR_RCHW_SHIFT,
			OSD_RECTCUR_RCHW);
	dispc_reg_merge(OSD_RECTCUR,
			dm->cursor.image.depth << OSD_RECTCUR_RCVW_SHIFT,
			OSD_RECTCUR_RCVW);

	dispc_reg_merge(OSD_RECTCUR, 0 << OSD_RECTCUR_CLUTSR_SHIFT,
			OSD_RECTCUR_CLUTSR);
	dispc_reg_merge(OSD_RECTCUR,
			dm->cursor.image.
			fg_color << OSD_RECTCUR_RCAD_SHIFT, OSD_RECTCUR_RCAD);
	return 0;
}

/*
 * vpbe_set_vid_expansion
 */
/* Function to set pixel clock parameters */
/* Note: we are setting expantion for video window only */
int vpbe_set_vid_expansion(vpbe_win_expansion_t * required)
{
	int result = 0;

	/*Set fields OSD_MODE.VHRSZ and OSD_MODE.VVRSZ accordingly */
	dispc_reg_merge(OSD_MODE,
			required->horizontal << OSD_MODE_VHRSZ_SHIFT,
			OSD_MODE_VHRSZ);
	dispc_reg_merge(OSD_MODE,
			required->vertical << OSD_MODE_VVRSZ_SHIFT,
			OSD_MODE_VVRSZ);

	dm->vid0->conf_params.video_params.exp_info = *required;

	return result;
}

/*
 * vpbe_set_dclk
 */
/* Function	to set pixel clock parameters	*/
int vpbe_set_dclk(vpbe_dclk_t * dclk)
{
	int result = 0;
# if 0
	if ((dclk->dclk_pattern_width > 0x3F) ||
	    (dclk->dclk_pattern0 > 0xFFFF) ||
	    (dclk->dclk_pattern1 > 0xFFFF) || (dclk->dclk_pattern2 > 0xFFFF)
	    || (dclk->dclk_pattern3 > 0xFFFF))
		return -VPBE_INVALID_PARA_VALUE;

	dispc_reg_merge(VENC_DCLKCTL, dclk->dclk_pattern_width,
			VENC_DCLKCTL_DCKPW);
	dispc_reg_out(VENC_DCLKPTN0, dclk->dclk_pattern0);
	dispc_reg_out(VENC_DCLKPTN1, dclk->dclk_pattern1);
	dispc_reg_out(VENC_DCLKPTN2, dclk->dclk_pattern2);
	dispc_reg_out(VENC_DCLKPTN3, dclk->dclk_pattern3);
#endif
	dm->dclk = *dclk;

	return result;
}

/*
 * vpbe_set_backg_color 
 */
/* Function to configure registers for background color	selection */
int vpbe_set_backg_color(vpbe_backg_color_t * bg)
{
	int result = 0;

	switch (bg->clut_select) {
	case 0:		/* ROM 0 CLUT */
		dispc_reg_merge(OSD_MODE,
				bg->color_offset << OSD_MODE_CABG_SHIFT,
				OSD_MODE_CABG);
		dispc_reg_merge(OSD_MISCCT, SET_0 << OSD_MISCCT_RSEL_SHIFT,
				OSD_MISCCT_RSEL);
		dispc_reg_merge(OSD_MODE, SET_0 << OSD_MODE_BCLUT_SHIFT,
				OSD_MODE_BCLUT);
		break;
	case 1:		/* ROM 1 CLUT */
		dispc_reg_merge(OSD_MODE,
				bg->color_offset << OSD_MODE_CABG_SHIFT,
				OSD_MODE_CABG);
		dispc_reg_merge(OSD_MISCCT, SET_1 << OSD_MISCCT_RSEL_SHIFT,
				OSD_MISCCT_RSEL);
		dispc_reg_merge(OSD_MODE, SET_0 << OSD_MODE_BCLUT_SHIFT,
				OSD_MODE_BCLUT);
		break;
	case 2:		/* RAM CLUT */
		dispc_reg_merge(OSD_MODE,
				bg->color_offset << OSD_MODE_CABG_SHIFT,
				OSD_MODE_CABG);
		dispc_reg_merge(OSD_MODE, SET_1 << OSD_MODE_BCLUT_SHIFT,
				OSD_MODE_BCLUT);
		break;
	default:
		return -VPBE_INVALID_PARA_VALUE;
	}

	dm->backg = *bg;

	return result;
}

/*
 * vpbe_set_interface 
 */
/* To set display interface given by index. */
int vpbe_set_interface(u8 interface)
{
	int result = 0;

	if (interface > VPBE_MAX_INTERFACES)
		return -VPBE_INVALID_PARA_VALUE;

	/* Set Display for first mode for given interface */
	dm->display.interface = interface;
	dm->display.mode = 0;
	dm->videomode = modelist[dm->display.interface][dm->display.mode];
	/* Find appropriate display function */
	init_display_function(&dm->display);

	dm->output_device_config();

	return result;
}

/*
 * vpbe_query_mode 
 */
/*  To query whether a perticular mode given by interface 
 *  and mode index is available or not.
 *  If mode is available, set video mode structure accordingly.
 *  Else set pointer to video mode structure to NULL.	
 */
int vpbe_query_mode(vpbe_mode_info_t * mode_info)
{
	int mode;
	int supported = 1;	/* flag indicated whether mode is found ot not */

	if (mode_info->interface > VPBE_MAX_INTERFACES)
		supported = 0;
	else {
		for (mode = 0; mode <= mode_info->mode_idx; mode++) {
			if (!strcmp
			    (modelist[mode_info->interface][mode].name, "\0")) {
				supported = 0;
				break;
			}
		}
	}

	if (!supported)
		/*If mode not found...set pointer to vpbe_fb_videomode with NULL */
	{
		strcpy(mode_info->vid_mode.name, "\0");
		return 0;
	} else {
		mode_info->vid_mode =
		    modelist[mode_info->interface][mode_info->mode_idx];
	}
	return 0;
}

/*
 * vpbe_set_mode 
 */
/* To configure video mode structure(If mode is available).*/
int vpbe_set_mode(struct vpbe_fb_videomode *vm)
{
	int interface, mode;
	int result = 0, mode_found = 0, check_new_interface = 0;
	if (vm->std) {
		/* Find suitable match */
		for (interface = 0; interface < VPBE_MAX_INTERFACES;
		     interface++) {
			for (mode = 0;; mode++) {
				if (!strcmp
				    (modelist[interface][mode].name, "\0")) {
					check_new_interface = 1;
					break;
				} else
				    if ((modelist[interface][mode].vmode ==
					 vm->vmode)
					&& (modelist[interface][mode].
					    xres == vm->xres)
					&& (modelist[interface][mode].
					    yres == vm->yres)
					&& (modelist[interface][mode].
					    fps == vm->fps)
					&&
					(!strcmp
					 (modelist[interface][mode].name,
					  vm->name))) {
					/* Mode found */
					dm->display.interface = interface;
					dm->display.mode = mode;
					mode_found = 1;
					break;
				}
			}
			if (mode_found)
				break;
			if (check_new_interface)
				continue;
		}
		if (!mode_found)	/* Required mode not found. Return -1 */
			return -1;
		else
			dm->videomode =
			    modelist[dm->display.interface][dm->display.mode];
	} else {
		dm->videomode = *vm;
		/* For Non existing mode of curruntly selected interface. */
		dm->display.mode = 255;
	}
	/* Find appropriate display function */
	init_display_function(&dm->display);

	dm->output_device_config();

	return result;
}

/*
 * vpbe_set_venc_clk_source 
 */
/* To select VENC module click source.	*/
int vpbe_set_venc_clk_source(u8 muxsel)
{
	int result = 0;

/*
 *  For CLK_SOURCE_CLK27, VPSS_MUXSEL should be 0   
 *  For CLK_SOURCE_CLK54, VPSS_MUXSEL should be 1 
 *  For CLK_SOURCE_VPBECLK, VPSS_MUXSEL should be 2
 */
	if (muxsel > CLK_SOURCE_VPBECLK)
		return -VPBE_INVALID_PARA_VALUE;

	return result;
}
