/*
 * include/video/davincifb.h
 *
 * Framebuffer driver for Texas Instruments DM644x display controller.
 *
 * Copyright (C) 2006 Texas Instruments, Inc.
 * Rishi Bhattacharya <support@ti.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */
#ifndef _DAVINCIFB_H_
#define _DAVINCIFB_H_

/* define the custom FBIO_WAITFORVSYNC ioctl */
#define FBIO_WAITFORVSYNC	_IOW('F', 0x20, u_int32_t)
#define FBIO_SETATTRIBUTE       _IOW('F', 0x21, struct fb_fillrect)
#define FBIO_SETPOSX		_IOW('F', 0x22, u_int32_t)
#define FBIO_SETPOSY		_IOW('F', 0x23, u_int32_t)

typedef struct zoom_params
{
	u_int32_t window_id;
	u_int32_t zoom_h;
	u_int32_t zoom_v;
} zoom_params_t;
#define FBIO_SETZOOM		_IOW('F', 0x24, struct zoom_params)
#define FBIO_GETSTD		_IOR('F', 0x25, u_int32_t)

struct vpfe_resizer_params
{
	u_int32_t rsz_cnt;	//busy-lock
	u_int32_t out_size;	//busy-lock
	u_int32_t in_start;	//busy-lock
	u_int32_t in_size;	//busy-lock
	u_int32_t sdr_inadd;	//shadowed
	u_int32_t sdr_inoff;	//shadowed
	u_int32_t sdr_outadd;	//shadowed
	u_int32_t sdr_outoff;	//shadowed
	u_int32_t hfilt[16];	//busy-lock
	u_int32_t vfilt[16];	//busy-lock
	u_int32_t yenh;		//busy-lock
};

typedef struct fb_set_start {
	int		offset;		/* offset from smem_start */
	unsigned long	physical;	/* absolute physical address when offset < 0 */

	u_int64_t	sync;		/* input:  target sync counter for change or 0 for no sync at all,
					   output: sync counter of actual change or 0 if still pending */
} fb_set_start_t;



#define FBIO_RESIZER		_IOW('F', 0x26, struct vpfe_resizer_params)
#define FBIO_SYNC		_IOW('F', 0x27, u_int32_t)
#define FBIO_FILLRECT		_IOW('F', 0x28, struct fb_fillrect)

#define FBIO_ENABLE_DISABLE_WIN		_IOW('F', 0x30, unsigned char)

/*  Structure for transparency and the blending factor for the bitmap window  */
typedef struct vpbe_bitmap_blend_params {
	unsigned int colorkey;			/* color key to be blend */
	unsigned int enable_colorkeying;	/* enable color keying */
	unsigned int bf;			/* valid range from 0 to 7 only. */
} vpbe_bitmap_blend_params_t;

#define FBIO_SET_BITMAP_BLEND_FACTOR	_IOW('F', 0x31, vpbe_bitmap_blend_params_t)

#define RAM_CLUT_SIZE	256*3
#define FBIO_SET_BITMAP_WIN_RAM_CLUT	_IOW('F', 0x32, unsigned char[RAM_CLUT_SIZE])
#define FBIO_ENABLE_DISABLE_ATTRIBUTE_WIN _IOW('F', 0x33, unsigned int)

/*  Structure for OSD window blinking options */
/*  Enable/Disable enum */
typedef enum {
	VPBE_DISABLE = 0,
	VPBE_ENABLE = 1
} ATTENUATION, TRANSPARENCY, EXPANSION, BLINKING;

typedef struct vpbe_blink_option {
	BLINKING blinking;	/* 1: Enable blinking 0: Disable */
	unsigned int interval;	/* Valid only if blinking is 1 */
} vpbe_blink_option_t;

#define FBIO_GET_BLINK_INTERVAL		_IOR('F', 0x34, vpbe_blink_option_t)
#define FBIO_SET_BLINK_INTERVAL		_IOW('F', 0x35, vpbe_blink_option_t)

/*  Structure for Video window configurable parameters  */
/*  Enum for Boolean variables  */
typedef enum {
	SET_0 = 0,
	SET_1 = 1
} CB_CR_ORDER, ATTRIBUTE, ROM_RAM_CLUT;

/*  Structure for window expansion  */
typedef struct vpbe_win_expansion {
	EXPANSION horizontal;
	EXPANSION vertical;		/* 1: Enable 0:disable */
} vpbe_win_expansion_t;

typedef struct vpbe_video_config_params {
	CB_CR_ORDER cb_cr_order;	/*Cb/Cr order in input data for a pixel. */
					/*    0: cb cr  1:  cr cb */
	vpbe_win_expansion_t exp_info;  /* HZ/VT Expansion enable disable */
} vpbe_video_config_params_t;

#define FBIO_GET_VIDEO_CONFIG_PARAMS	_IOR('F', 0x36, vpbe_video_config_params_t)
#define FBIO_SET_VIDEO_CONFIG_PARAMS	_IOW('F', 0x37, vpbe_video_config_params_t)

typedef union vpbe_clut_idx {
	struct _for_4bit_bimap {
		unsigned char bitmap_val_0;
		unsigned char bitmap_val_1;
		unsigned char bitmap_val_2;
		unsigned char bitmap_val_3;
		unsigned char bitmap_val_4;
		unsigned char bitmap_val_5;
		unsigned char bitmap_val_6;
		unsigned char bitmap_val_7;
		unsigned char bitmap_val_8;
		unsigned char bitmap_val_9;
		unsigned char bitmap_val_10;
		unsigned char bitmap_val_11;
		unsigned char bitmap_val_12;
		unsigned char bitmap_val_13;
		unsigned char bitmap_val_14;
		unsigned char bitmap_val_15;
	} for_4bit_bimap;
	struct _for_2bit_bimap {
		unsigned char bitmap_val_0;
		unsigned char dummy0[4];
		unsigned char bitmap_val_1;
		unsigned char dummy1[4];
		unsigned char bitmap_val_2;
		unsigned char dummy2[4];
		unsigned char bitmap_val_3;
	} for_2bit_bimap;
	struct _for_1bit_bimap {
		unsigned char bitmap_val_0;
		unsigned char dummy0[14];
		unsigned char bitmap_val_1;
	} for_1bit_bimap;
} vpbe_clut_idx_t;

/*  Structure for bitmap window configurable parameters */
typedef struct vpbe_bitmap_config_params {
	/* Only for bitmap width = 1,2,4 bits */
	vpbe_clut_idx_t clut_idx;
	/* Attenuation value for YUV o/p for bitmap window */
	unsigned char attenuation_enable;
	/* 0: ROM DM270, 1:ROM DM320, 2:RAM CLUT */
	unsigned char clut_select;
} vpbe_bitmap_config_params_t;

#define FBIO_GET_BITMAP_CONFIG_PARAMS	_IOR('F', 0x38, vpbe_bitmap_config_params_t)
#define FBIO_SET_BITMAP_CONFIG_PARAMS	_IOW('F', 0x39, vpbe_bitmap_config_params_t)

/*  Structure for DCLK parameters */
typedef struct vpbe_dclk {
	unsigned char dclk_pattern_width;
	unsigned int dclk_pattern0;
	unsigned int dclk_pattern1;
	unsigned int dclk_pattern2;
	unsigned int dclk_pattern3;
} vpbe_dclk_t;

#define FBIO_SET_DCLK			_IOW('F', 0x40, vpbe_dclk_t)
#define FBIO_SET_INTERFACE		_IOW('F', 0x41, unsigned char)
#define FBIO_GET_INTERFACE		_IOR('F', 0x42, unsigned char)

/*
 *  *  Videmode structure for display interface and mode settings
 *   */
typedef struct vpbe_fb_videomode {
	unsigned char name[10]; /* Mode name ( NTSC , PAL) */
	unsigned int vmode;     /* FB_MODE_INTERLACED or FB_MODE_NON_INTERLACED */
	unsigned int xres;      /* X Resolution of the display */
	unsigned int yres;      /* Y Resolution of the display */
	unsigned int fps;       /* frames per second */
	/* Timing Parameters applicable for std = 0 only */
	unsigned int left_margin;
	unsigned int right_margin;
	unsigned int upper_margin;
	unsigned int lower_margin;
	unsigned int hsync_len;
	unsigned int vsync_len;
	unsigned int sync;	/* 0: hsync -ve/vsync -ve */
				/*1: hsync -ve/vsync +ve */
				/*2: hsync +ve/vsync -ve */
				/*3: hsync +ve/vsync +ve */
	unsigned int basepx;    /* Display x,y start position */
	unsigned int basepy;
	/*  1= Mode s available in modelist 0=Mode is not available in modelist */
	unsigned int std;
} vpbe_fb_videomode_t;

/* Structure to interface videomode to application*/
typedef struct vpbe_mode_info {
	vpbe_fb_videomode_t vid_mode;
	unsigned char interface;
	unsigned char mode_idx;
} vpbe_mode_info_t;

#define FBIO_QUERY_TIMING		_IOWR('F', 0x43, struct vpbe_mode_info)
#define FBIO_SET_TIMING			_IOW('F', 0x44, struct vpbe_fb_videomode)
#define FBIO_GET_TIMING			_IOR('F', 0x45, struct vpbe_fb_videomode)
#define FBIO_SET_VENC_CLK_SOURCE	_IOW('F', 0x46, unsigned char)

/*  Structure for background color  */
typedef struct vpbe_backg_color {
	unsigned char clut_select;	/* 2: RAM CLUT 1:ROM1 CLUT 0:ROM0 CLUT */
	unsigned char color_offset;	/* index of color */
} vpbe_backg_color_t;

#define FBIO_SET_BACKG_COLOR		_IOW('F', 0x47, vpbe_backg_color_t)
#define FBIO_ENABLE_DISPLAY		_IOW('F', 0x48, unsigned char)
#define FBIO_SETPOS			_IOW('F', 0x49, u_int32_t)
#define FBIO_SET_CURSOR			_IOW('F', 0x50, struct fb_cursor)
#define FBIO_SET_START			_IOW('F', 0x66, struct fb_set_start)

#endif /* _DAVINCIFB_H_ */
