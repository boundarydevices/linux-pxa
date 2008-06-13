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
 * File: davincifb.h	
 */

#ifndef __DAVINVIFB_H
#define __DAVINVIFB_H

/* include Linux files */
#include <linux/fb.h>



/* 
 * Defines and Constants
 */
#ifdef __KERNEL__
#define DAVINCIFB_DEVICE "davincifb"
#define DAVINCIFB_DRIVER "davincifb"

#define MULTIPLE_BUFFERING      1

#ifdef MULTIPLE_BUFFERING
#define DOUBLE_BUF      2
#define TRIPLE_BUF      3
#else
#define DOUBLE_BUF      1
#define TRIPLE_BUF      1
#endif

/* usage:	if (is_win(info->fix.id, OSD0)) ... */
#define is_win(name, x) ((strcmp(name, x ## _FBNAME) == 0) ? 1 : 0)

/*
 * display controller register I/O routines
 */
u32 dispc_reg_in(u32 offset);
u32 dispc_reg_out(u32 offset, u32 val);
u32 dispc_reg_merge(u32 offset, u32 val, u32 mask);

#endif				/*__KERNEL__*/

/*  Error return codes  */
#define VPBE_INVALID_PARA_VALUE         700
#define VPBE_WRONG_WINDOW_ID            701
#define VPBE_CURRENTLY_IN_REQUIRED_MODE 702
#define VPBE_INSUFFICIENT_CLUT_VALUES   703
#define VPBE_CLUT_WRITE_TIMEOUT         704
#define VPBE_VID0_BUF_ADR_NULL          705
#define VPBE_WINDOW_NOT_DISABLED        706
#define VPBE_WINDOW_NOT_ENABLED         707

#ifndef __KERNEL__
/*  Window ID definations */
#define OSD0      0
#define VID0      1
#define OSD1      2
#define VID1      3
#endif

/* There are 4 framebuffers, each represented by an fb_info and
 * a dm_win_info structure */
#define OSD0_FBNAME "dm_osd0_fb"
#define OSD1_FBNAME "dm_osd1_fb"
#define VID0_FBNAME "dm_vid0_fb"
#define VID1_FBNAME "dm_vid1_fb"

/*  FIXME: Digital LCD RGB matrix coefficients */
#define DLCD_DGY_VAL    0
#define DLCD_DRV_VAL    0
#define DLCD_DGU_VAL    0
#define DLCD_DBU_VAL		0

/* Defines for bitmap format */
#define VPBE_BITMAP_BIT_1	1
#define VPBE_BITMAP_BIT_2	2
#define VPBE_BITMAP_BIT_4	4
#define VPBE_BITMAP_BIT_8	8
#define VPBE_BITMAP_RGB565	16
#define VPBE_VIDEO_YUV422 	16
#define VPBE_VIDEO_RGB888 	24

/* Defines foe cursor parameter validation*/
#define MAX_CURSOR_WIDTH	0x3FF
#define MAX_CURSOR_HEIGHT	0x1FF
#define MAX_CURSOR_LINEWIDTH    7

#define BASEX		0x80
#define BASEY		0x12
#define BASEX_DLCD		0x59
#define BASEY_DLCD		0x22

/*
 * Enumerations 
 */
/*  Enum for blending factor  */
typedef enum vpbe_blend_factor {
	OSD_CONTRIBUTION_ZERO = 0,
	OSD_CONTRIBUTION_1_BY_8 = 1,
	OSD_CONTRIBUTION_2_BY_8 = 2,
	OSD_CONTRIBUTION_3_BY_8 = 3,
	OSD_CONTRIBUTION_4_BY_8 = 4,
	OSD_CONTRIBUTION_5_BY_8 = 5,
	OSD_CONTRIBUTION_6_BY_8 = 6,
	OSD_CONTRIBUTION_ONE = 7
} vpbe_blend_factor_t;

/*  Defines for Display Interface */
#define  PRGB		0
#define  COMPOSITE      1
#define  SVIDEO    	2
#define  COMPONENT 	3
#define  RGB       	4
#define  YCC16     	5
#define  YCC8      	6
#define  SRGB      	7
#define  EPSON     	8
#define  CASIO1G   	9
#define  UDISP     	10
#define  STN       	11
#define VPBE_MAX_INTERFACES	12

/*  Defines for Display Mode */
#define  LCD    0
#define  NTSC	1
#define  PAL    2
#define  P525   3
#define  P625   4

#define DEFAULT_MODE 0
#define  P480   0
#define  P400   1
#define  P350   2
#define NON_EXISTING_MODE 255

typedef enum clk_source {
	CLK_SOURCE_CLK27 = 0,
	CLK_SOURCE_CLK54 = 1,
	CLK_SOURCE_VPBECLK = 2
} CLK_SOURCE;

/*
 * Structures and Union Definitions
 */

/*  Structure for display format  */
typedef struct vpbe_display_format {
	unsigned char interface;	/* Output interface type */
	unsigned char mode;	/* output mode */
} vpbe_display_format_t;

/*Union of structures giving the CLUT index for the 1, 2, 4 bit bitmap values.*/

/*  Unioun for video/OSD configuration parameters  */
typedef union vpbe_conf_params {

	struct vpbe_video_params {
		CB_CR_ORDER cb_cr_order;
		/* HZ/VT Expansion enable disable */
		vpbe_win_expansion_t exp_info;
	} video_params;

	struct vpbe_bitmap_params {
		/* Attenuation value for YUV o/p */
		ATTENUATION attenuation_enable;
		/* 0: ROM DM270, 1: ROM DM320, 2:RAM CLUT */
		unsigned char clut_select;
		/* Only for bitmap width = 1,2,4 bits */
		vpbe_clut_idx_t clut_idx;
		/* 0: OSD window is bitmap window */
		/* 1: OSD window is attribute window */
		ATTRIBUTE enable_attribute;
		/* To hold bps value. 
		   Used to switch back from attribute to bitmap. */
		unsigned int stored_bits_per_pixel;
		/* Blending information */
		vpbe_bitmap_blend_params_t blend_info;
		/* OSD Blinking information */
		vpbe_blink_option_t blink_info;
	} bitmap_params;

} vpbe_conf_params_t;

typedef struct vpbe_video_params vpbe_video_params_t;
typedef struct vpbe_bitmap_params vpbe_bitmap_params_t;

/* Structure to hold window position */
typedef struct vpbe_window_position {
	unsigned int xpos;	/* X position of the window */
	unsigned int ypos;	/* Y position of the window */
} vpbe_window_position_t;

#ifdef __KERNEL__
/*  Structure for each window */
typedef struct vpbe_dm_win_info {
	struct fb_info info;
	vpbe_window_position_t win_pos;	/* X,Y position of window */
	/* Size of window is already there in var_info structure. */

	dma_addr_t fb_base_phys;	/*framebuffer area */
	unsigned int fb_base;	/*window memory pointer */
	unsigned int fb_size;	/*memory size */
	unsigned int pseudo_palette[17];
	int alloc_fb_mem;
	/*flag to identify if framebuffer area is fixed or not */
	unsigned long sdram_address;
	struct vpbe_dm_info *dm;
	unsigned char window_enable;	/*Additions for all windows */
	zoom_params_t zoom;	/*Zooming parameters */
	unsigned char field_frame_select;	/*To select Field or frame */
	unsigned char numbufs;	/*Number of buffers valid 2 or 3 */
	vpbe_conf_params_t conf_params;
	/*window configuration parameter union pointer */
} vpbe_dm_win_info_t;
#endif				/*__KERNEL__*/

#ifdef __KERNEL__
/* 
 * Structure for the driver holding information of windows, 
 *  memory base addresses etc.
 */
typedef struct vpbe_dm_info {
	vpbe_dm_win_info_t *osd0;
	vpbe_dm_win_info_t *osd1;
	vpbe_dm_win_info_t *vid0;
	vpbe_dm_win_info_t *vid1;

/* to map the registers */
	dma_addr_t mmio_base_phys;
	unsigned int mmio_base;
	unsigned int mmio_size;

	wait_queue_head_t vsync_wait;
	unsigned int vsync_cnt;
	int timeout;

	/* this is the function that configures the output device (NTSC/PAL/LCD)
	 * for the required output format (composite/s-video/component/rgb)
	 */
	void (*output_device_config) (void);

	struct device *dev;

	vpbe_backg_color_t backg;	/* background color */
	vpbe_dclk_t dclk;	/*DCLK parameters */
	vpbe_display_format_t display;	/*Display interface and mode */
	vpbe_fb_videomode_t videomode;	/*Cuurent videomode */
	char ram_clut[256][3];	/*RAM CLUT array */
	struct fb_cursor cursor;	/* cursor config params from fb.h */
/*Flag that indicates whether any of the display is enabled or not*/
	int display_enable;
	wait_queue_head_t resizer_wait;
	unsigned long resizer_cnt;
		
} vpbe_dm_info_t;

/*
 * Functions Definitions for 'davincifb' module
 */
int vpbe_mem_alloc_window_buf(vpbe_dm_win_info_t *);
int vpbe_mem_release_window_buf(vpbe_dm_win_info_t *);
void init_display_function(vpbe_display_format_t *);
int vpbe_mem_alloc_struct(vpbe_dm_win_info_t **);
void set_vid0_default_conf(void);
void set_vid1_default_conf(void);
void set_osd0_default_conf(void);
void set_osd1_default_conf(void);
void set_cursor_default_conf(void);
void set_dm_default_conf(void);
void set_win_enable(char *, unsigned int);
int within_vid0_limits(u32, u32, u32, u32);
void vpbe_set_display_default(void);
#ifdef __KERNEL__
void set_win_position(char *, u32, u32, u32, u32);
void change_win_param(int);
void set_interlaced(char *, unsigned int);
#endif /* __KERNEL__ */

/*
 *	Function definations for 'osd' module
 */

int vpbe_enable_window(vpbe_dm_win_info_t *);
int vpbe_disable_window(vpbe_dm_win_info_t *);
int vpbe_vid_osd_select_field_frame(u8 *, u8);
int vpbe_bitmap_set_blend_factor(u8 *, vpbe_bitmap_blend_params_t *);
int vpbe_bitmap_set_ram_clut(void);
int vpbe_enable_disable_attribute_window(u32);
int vpbe_get_blinking(u8 *, vpbe_blink_option_t *);
int vpbe_set_blinking(u8 *, vpbe_blink_option_t *);
int vpbe_set_vid_params(u8 *, vpbe_video_config_params_t *);
int vpbe_get_vid_params(u8 *, vpbe_video_config_params_t *);
int vpbe_bitmap_get_params(u8 *, vpbe_bitmap_config_params_t *);
int vpbe_bitmap_set_params(u8 *, vpbe_bitmap_config_params_t *);
int vpbe_set_cursor_params(struct fb_cursor *);
int vpbe_set_vid_expansion(vpbe_win_expansion_t *);
int vpbe_set_dclk(vpbe_dclk_t *);
int vpbe_set_display_format(vpbe_display_format_t *);
int vpbe_set_backg_color(vpbe_backg_color_t *);
int vpbe_set_interface(u8);
int vpbe_query_mode(vpbe_mode_info_t *);
int vpbe_set_mode(struct vpbe_fb_videomode *);
int vpbe_set_venc_clk_source(u8);
void set_vid0_default_conf(void);
void set_osd0_default_conf(void);
void set_vid1_default_conf(void);
void set_osd1_default_conf(void);
void set_cursor_default_conf(void);
void set_dm_default_conf(void);
/*
 * Function definations for 'venc' module
 */

void davincifb_ntsc_composite_config(void);
void davincifb_ntsc_svideo_config(void);
void davincifb_ntsc_component_config(void);
void davincifb_pal_composite_config(void);
void davincifb_pal_svideo_config(void);
void davincifb_pal_component_config(void);

void vpbe_davincifb_ntsc_rgb_config(void);
void vpbe_davincifb_pal_rgb_config(void);
void vpbe_davincifb_525p_component_config(void);
void vpbe_davincifb_625p_component_config(void);

void vpbe_enable_venc(int);
void vpbe_enable_dacs(int);
/*
 * Function definations for 'dlcd' module
 */
void vpbe_davincifb_480p_prgb_config(void);
void vpbe_davincifb_400p_prgb_config(void);
void vpbe_davincifb_350p_prgb_config(void);
void vpbe_set_display_timing(struct vpbe_fb_videomode *);

void vpbe_enable_lcd(int);
/*
 * Following functions are not implemented
 */
void vpbe_davincifb_default_ycc16_config(void);
void vpbe_davincifb_default_ycc8_config(void);
void vpbe_davincifb_default_srgb_config(void);
void vpbe_davincifb_default_epson_config(void);
void vpbe_davincifb_default_casio_config(void);
void vpbe_davincifb_default_UDISP_config(void);
void vpbe_davincifb_default_STN_config(void);
#endif				/*__KERNEL__*/

#endif				/* End of #ifndef DAVINCIFB_H */
