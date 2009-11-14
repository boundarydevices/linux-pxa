/* 
 * Copyright (C) 2009 Boundary Devices
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
 */

#ifndef __DAVINVIFB_CONFIG_H
#define __DAVINVIFB_CONFIG_H
typedef struct
{
	unsigned pixclock;
	unsigned short xres,hsync_len,left_margin,right_margin;
	unsigned short yres,vsync_len,upper_margin,lower_margin;
	unsigned char vsyn_acth,hsyn_acth,pclk_redg,oepol_actl,dPol;
	unsigned char enable,unscramble,rotation,active,vSyncHz,crt;
	unsigned char enc_mult, enc_div;
} DISPLAYCFG;
#endif
