/*
 * Copyright (C) 2010 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/ata.h>
#include <linux/clk.h>
#include <linux/delay.h>
#if defined(CONFIG_DUMB_BATTERY) || defined (CONFIG_DUMB_BATTERY_MODULE)
#include <linux/dumb_battery.h>
#endif
#include <linux/fec.h>
#include <linux/fsl_devices.h>
#include <linux/gpio.h>
#ifdef CONFIG_KEYBOARD_GPIO
#include <linux/gpio_keys.h>
#endif
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/ipu.h>
#include <linux/irq.h>
#include <linux/ldb.h>
#include <linux/mfd/sc16is7xx-reg.h>
#if defined(CONFIG_MTD)
#include <linux/mtd/map.h>
#endif
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mxcfb.h>
#include <linux/nodemask.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pmic_external.h>
#include <linux/pmic_status.h>
#include <linux/powerkey.h>
#include <linux/pwm_backlight.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>
#include <linux/sched.h>
#include <linux/spi/ltc1960.h>
#include <linux/spi/spi.h>
#include <linux/types.h>
#include <linux/usb/android_composite.h>
#include <linux/wl12xx.h>
#include <linux/android_pmem.h>

#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/i2c.h>
#include <mach/iomux-mx53.h>
#include <mach/memory.h>
#include <mach/mmc.h>
#include <mach/mxc_dvfs.h>
#include <mach/mxc_iim.h>

#include <asm/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/flash.h>
#include <asm/mach/keypad.h>
#include <asm/mach/time.h>
#include <asm/mach-types.h>
#include <asm/setup.h>

#include "crm_regs.h"
#include "devices.h"
#include "usb.h"

//#define REV0		//this board should no longer exist

/*
 * board changes needed for esai1 pins
 * Pad		gpio		nitrogen53_v1/2	nitrogen53/nitrogen53_a
 * EIM_A22	gpio2[20]			Display enable for chimei 7" panel (high active)
 * EIM_A17	gpio2[21]			PMIC_IRQ
 * GPIO_16	gpio7[11]	PMIC_IRQ	i2c3-sda
 * GPIO_6	gpio1[6]	i2c3-sda	SCKT of esai1
 */

extern int __init mx53_nitrogen_init_da9052(unsigned irq);

#define MAKE_GP(port, bit) ((port - 1) * 32 + bit)

/* newer boards use GP7:11 for I2C3 SDA pin, older use GP1:6 */
#define N53_I2C_2_SDA_CURRENT MAKE_GP(7, 11)
#define N53_I2C_2_SDA_PREVIOUS MAKE_GP(1, 6)
#define N53_WL1271_INT			MAKE_GP(2, 24)
#define N53_WL1271_WL_EN		MAKE_GP(3, 0)
#define N53_WL1271_BT_EN		MAKE_GP(3, 1)

struct gpio nitrogen53_gpios[] __initdata = {
#ifdef REV0
	{.label = "touch_int_gp1_0",	.gpio = MAKE_GP(1, 0),		.flags = GPIOF_DIR_IN},
#endif
#define N53_I2C_2_SCL				MAKE_GP(1, 3)
	{.label = "i2c-2-scl",		.gpio = MAKE_GP(1, 3),		.flags = GPIOF_DIR_IN},
//	{.label = "???Menu key",	.gpio = MAKE_GP(1, 4),		.flags = GPIOF_DIR_IN},

#define N53_I2C_1_SCL				MAKE_GP(2, 30)
	{.label = "i2c-1-scl",		.gpio = MAKE_GP(2, 30),		.flags = GPIOF_DIR_IN},
#define N53_SD3_CD				MAKE_GP(3, 11)
	{.label = "sdhc3-cd",		.gpio = MAKE_GP(3, 11),		.flags = GPIOF_DIR_IN},
#define N53_SD3_WP				MAKE_GP(3, 12)
	{.label = "sdhc3-wp",		.gpio = MAKE_GP(3, 12),		.flags = GPIOF_DIR_IN},
#define N53_SD1_CD				MAKE_GP(3, 13)
	{.label = "sdhc1-cd",		.gpio = MAKE_GP(3, 13),		.flags = GPIOF_DIR_IN},
#define N53_SD1_WP				MAKE_GP(3, 14)
	{.label = "sdhc1-wp",		.gpio = MAKE_GP(3, 14),		.flags = GPIOF_DIR_IN},
#define N53_SC16IS7XX_INT			MAKE_GP(3, 20)
	{.label = "sc16is7xx-int",	.gpio = MAKE_GP(3, 20),		.flags = GPIOF_DIR_IN},		/* EIM_D20 */
#define N53_I2C_0_SCL				MAKE_GP(3, 21)
	{.label = "i2c-0-scl",		.gpio = MAKE_GP(3, 21),		.flags = GPIOF_DIR_IN},
#define N53_I2C_0_SDA				MAKE_GP(3, 28)
	{.label = "i2c-0-sda",		.gpio = MAKE_GP(3, 28),		.flags = GPIOF_DIR_IN},
//The gpio_keys.c file will request these, they are here for documentation only
//	{.label = "Menu key",		.gpio = MAKE_GP(3, 25),		.flags = GPIOF_DIR_IN},
//	{.label = "Back key",		.gpio = MAKE_GP(3, 26),		.flags = GPIOF_DIR_IN},
//	{.label = "Search key",		.gpio = MAKE_GP(3, 27),		.flags = GPIOF_DIR_IN},
//	{.label = "Home key",		.gpio = MAKE_GP(3, 29),		.flags = GPIOF_DIR_IN},
	{.label = "On/Off key",		.gpio = MAKE_GP(3, 30),		.flags = GPIOF_DIR_IN},
#define N53_I2C_1_SDA				MAKE_GP(4, 13)
	{.label = "i2c-1-sda",		.gpio = MAKE_GP(4, 13),		.flags = GPIOF_DIR_IN},
#define N53_TFP410_INT				MAKE_GP(4, 15)
	{.label = "tfp410int",		.gpio = MAKE_GP(4, 15),		.flags = GPIOF_DIR_IN},		/* KEY_ROW4 */
#define N53_I2C_CONNECTOR_INT			MAKE_GP(7, 12)
	{.label = "i2c_int",		.gpio = MAKE_GP(7, 12),		.flags = GPIOF_DIR_IN},

/* Outputs */
#define CAMERA_STROBE				MAKE_GP(1, 7)
	{.label = "Camera strobe",	.gpio = MAKE_GP(1, 7),		.flags = 0},
	// make sure gp2[29] is high, i2c_sel for tfp410
#define N53_TFP410_I2CMODE			MAKE_GP(2, 29)
	{.label = "tfp410_i2cmode",	.gpio = MAKE_GP(2, 29),		.flags = GPIOF_INIT_HIGH},	/* EIM_EB1 */
#define N53_I2C_CONNECTOR_BUFFER_ENABLE		MAKE_GP(3, 10)
	{.label = "I2C conn. buf en",	.gpio = MAKE_GP(3, 10),		.flags = 0},			/* EIM_DA10 */
#define N53_SS1					MAKE_GP(3, 19)
	{.label = "ecspi_ss1",		.gpio = MAKE_GP(3, 19),		.flags = GPIOF_INIT_HIGH},	/* low active */
//	{.label = "Shutdown output",	.gpio = MAKE_GP(3, 31),		.flags = 0},
	{.label = "cam-reset",		.gpio = MAKE_GP(4, 0),		.flags = GPIOF_INIT_HIGH},
#define N53_AMP_ENABLE				MAKE_GP(4, 7)	/* KEY_ROW0 */
	{.label = "speaker_amp",	.gpio = MAKE_GP(4, 7),		.flags = 0},
#define CAMERA_RESET				MAKE_GP(4, 14)
	{.label = "Camera reset",	.gpio = MAKE_GP(4, 14),		.flags = 0},
#define N53_USB_HUB_RESET			MAKE_GP(5, 0)
	{.label = "USB HUB reset",	.gpio = MAKE_GP(5, 0),		.flags = 0},
	{.label = "eMMC reset",		.gpio = MAKE_GP(5, 2),		.flags = GPIOF_INIT_HIGH},	/* EIM_A25 */
#define N53_CAMERA_STANDBY			MAKE_GP(5, 20)
	{.label = "Camera standby",	.gpio = MAKE_GP(5, 20),		.flags = 0},
#define N53_OTG_VBUS				MAKE_GP(6, 6)
	{.label = "otg-vbus",		.gpio = MAKE_GP(6, 6),		.flags = 0},	/* disable VBUS */
#define EVK_USBH1_VBUS				MAKE_GP(7, 8)
	{.label = "usbh1-vbus",		.gpio = MAKE_GP(7, 8),		.flags = 0},
#define N53_PHY_RESET				MAKE_GP(7, 13)
	{.label = "ICS1893 reset",	.gpio = MAKE_GP(7, 13),		.flags = 0},	/* ICS1893 Ethernet PHY reset */
#if defined(CONFIG_SERIAL_IMX_RS485)
	{.label = "RS485 transmit enable",.gpio = CONFIG_SERIAL_IMX_RS485_GPIO,	.flags = 0},
#endif
};

#define BUTTON_PAD_CTRL	(PAD_CTL_HYS | PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_22K_UP | PAD_CTL_DSE_HIGH)

/*!
 * @file mach-mx53/mx53_evk.c
 *
 * @brief This file contains the board specific initialization routines.
 *
 * @ingroup MSL_MX53
 */
extern struct cpu_wp *(*get_cpu_wp)(int *wp);
extern void (*set_num_cpu_wp)(int num);
static int num_cpu_wp = 3;

static iomux_v3_cfg_t mx53common_pads[] = {
	/* AUD3, J8 - PCM */
	MX53_PAD_CSI0_D4__AUD3_TXC,
	MX53_PAD_CSI0_D5__AUD3_TXD,
	MX53_PAD_CSI0_D6__AUD3_TXFS,
	MX53_PAD_CSI0_D7__AUD3_RXD,

	/* AUD4, sgtl5000 */
	MX53_PAD_SD2_CLK__GPIO_1_10,	/* temp AUD4_RXFS */
	MX53_PAD_SD2_CMD__GPIO_1_11,	/* temp AUD4_RXC */
	MX53_PAD_SD2_DATA0__AUD4_RXD,
	MX53_PAD_SD2_DATA1__AUD4_TXFS,
	MX53_PAD_SD2_DATA3__AUD4_TXC,
	MX53_PAD_SD2_DATA2__AUD4_TXD,

	/* ECSPI1 */
	MX53_PAD_EIM_D16__CSPI1_SCLK,
	MX53_PAD_EIM_D17__CSPI1_MISO,
	MX53_PAD_EIM_D18__CSPI1_MOSI,
	MX53_PAD_EIM_D19__GPIO_3_19,	/* SS1 */

#ifdef REV0
	MX53_PAD_GPIO_0__GPIO_1_0,
#else
	/* SGTL5000 clock sys_mclk */
	MX53_PAD_GPIO_0__SSI_EXT1_CLK,
#endif

	/* esdhc1 */
	MX53_PAD_SD1_CMD__SD1_CMD,
	MX53_PAD_SD1_CLK__SD1_CLK,
	MX53_PAD_SD1_DATA0__SD1_DATA0,
	MX53_PAD_SD1_DATA1__SD1_DATA1,
	MX53_PAD_SD1_DATA2__SD1_DATA2,
	MX53_PAD_SD1_DATA3__SD1_DATA3,
	MX53_PAD_EIM_DA13__GPIO_3_13,	/* SDHC1 SD_CD */
	MX53_PAD_EIM_DA14__GPIO_3_14,	/* SDHC1 SD_WP */

	/* esdhc3 */
	MX53_PAD_ATA_DATA8__SD3_DAT0,
	MX53_PAD_ATA_DATA9__SD3_DAT1,
	MX53_PAD_ATA_DATA10__SD3_DAT2,
	MX53_PAD_ATA_DATA11__SD3_DAT3,
	MX53_PAD_ATA_DATA0__SD3_DAT4,
	MX53_PAD_ATA_DATA1__SD3_DAT5,
	MX53_PAD_ATA_DATA2__SD3_DAT6,
	MX53_PAD_ATA_DATA3__SD3_DAT7,
	MX53_PAD_ATA_RESET_B__SD3_CMD,
	MX53_PAD_ATA_IORDY__SD3_CLK,
	MX53_PAD_EIM_DA11__GPIO_3_11,	/* SDHC3 SD_CD */
	MX53_PAD_EIM_DA12__GPIO_3_12,	/* SDHC3 SD_WP */

	/* FEC pins */
	MX53_PAD_GPIO_18__GPIO_7_13,	/* low active reset pin*/
	MX53_PAD_FEC_MDIO__FEC_MDIO,
	MX53_PAD_FEC_REF_CLK__FEC_REF_CLK,
	MX53_PAD_FEC_RX_ER__FEC_RX_ER,
	MX53_PAD_FEC_CRS_DV__FEC_CRS_DV,
	MX53_PAD_FEC_RXD1__FEC_RXD1,
	MX53_PAD_FEC_RXD0__FEC_RXD0,
	MX53_PAD_KEY_COL2__FEC_RXD2,	/* Nitrogen53 */
	MX53_PAD_KEY_COL0__FEC_RXD3,	/* Nitrogen53 */
	MX53_PAD_FEC_TX_EN__FEC_TX_EN,
	MX53_PAD_FEC_TXD1__FEC_TXD1,
	MX53_PAD_FEC_TXD0__FEC_TXD0,
	MX53_PAD_KEY_ROW2__FEC_TXD2,	/* Nitrogen53 */
	MX53_PAD_GPIO_19__FEC_TXD3,	/* Nitrogen53 */
	/* FEC TX_ER - unused output from mx53 */
	MX53_PAD_KEY_ROW1__FEC_COL,	/* Nitrogen53 */
	MX53_PAD_KEY_COL3__FEC_CRS,	/* Nitrogen53 */
	MX53_PAD_KEY_COL1__FEC_RX_CLK,	/* Nitrogen53 */
	MX53_PAD_FEC_MDC__FEC_MDC,

	/* GPIO1 */
	NEW_PAD_CTRL(MX53_PAD_GPIO_4__GPIO_1_4, BUTTON_PAD_CTRL) | MUX_SION_MASK,	/* ??Menu */
	NEW_PAD_CTRL(MX53_PAD_GPIO_5__GPIO_1_5, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Keyboard */
	MX53_PAD_GPIO_8__GPIO_1_8,	/* J23 - rgb gp */

	/* GPIO2 */
	NEW_PAD_CTRL(MX53_PAD_EIM_A22__GPIO_2_16, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Keyboard */
	NEW_PAD_CTRL(MX53_PAD_EIM_A21__GPIO_2_17, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Keyboard */
	NEW_PAD_CTRL(MX53_PAD_EIM_A20__GPIO_2_18, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Keyboard */
	NEW_PAD_CTRL(MX53_PAD_EIM_A19__GPIO_2_19, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Keyboard */
	NEW_PAD_CTRL(MX53_PAD_EIM_A18__GPIO_2_20, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Keyboard */
	NEW_PAD_CTRL(MX53_PAD_EIM_A17__GPIO_2_21, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Keyboard */
	NEW_PAD_CTRL(MX53_PAD_EIM_A16__GPIO_2_22, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* wl1271 btwakeup, Camera power down(nitrogenA) */
	MX53_PAD_EIM_EB1__GPIO_2_29,									/* tpf410_i2cmode */

	/* GPIO3 */
	MX53_PAD_EIM_DA0__GPIO_3_0,	/* wl1271 wl_en */
	MX53_PAD_EIM_DA1__GPIO_3_1,	/* wl1271 bt_en */
	MX53_PAD_EIM_DA6__GPIO_3_6,	/* GPIO spare on Nitrogen53A */
	MX53_PAD_EIM_DA10__GPIO_3_10,	/* I2C Connector Buffer enable */
	/* Keyboard, NitrogenA - SC16IS7XX i2c serial interrupt */
	NEW_PAD_CTRL(MX53_PAD_EIM_D20__GPIO_3_20, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Keyboard */
	NEW_PAD_CTRL(MX53_PAD_EIM_D22__GPIO_3_22, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* Keyboard */
	NEW_PAD_CTRL(MX53_PAD_EIM_D26__GPIO_3_26, BUTTON_PAD_CTRL) | MUX_SION_MASK,	/* Back key */
	NEW_PAD_CTRL(MX53_PAD_EIM_D27__GPIO_3_27, BUTTON_PAD_CTRL) | MUX_SION_MASK,	/* Search Key */
	NEW_PAD_CTRL(MX53_PAD_EIM_D29__GPIO_3_29, BUTTON_PAD_CTRL) | MUX_SION_MASK,	/* Home Key */

	/* GPIO4 */
	MX53_PAD_KEY_ROW0__GPIO_4_7,	/* N53_AMP_ENABLE, Speaker Amp Enable */
	MX53_PAD_KEY_ROW4__GPIO_4_15,	/* tfp410int */

	/* GPIO5 */
	MX53_PAD_EIM_WAIT__GPIO_5_0,	/* USB HUB reset */
	NEW_PAD_CTRL(MX53_PAD_EIM_A25__GPIO_5_2, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),	/* NitrogenA - eMMC reset */
	MX53_PAD_EIM_A24__GPIO_5_4,	/* USB OTG USB_OC */

	/* GPIO6 */
	MX53_PAD_EIM_A23__GPIO_6_6,	/* USB OTG USB_PWR */
	MX53_PAD_NANDF_CS3__GPIO_6_16,	/* NitrogenA mic mux */

	/* GPIO7 */
	MX53_PAD_ATA_DA_2__GPIO_7_8,	/* USB HOST USB_PWR */
	MX53_PAD_GPIO_17__GPIO_7_12,	/* I2C Connector interrupt */

	/* I2C1 */
	MX53_PAD_EIM_D21__I2C1_SCL,	/* GPIO3[21] */
	MX53_PAD_EIM_D28__I2C1_SDA,	/* GPIO3[28] */

	/* I2C2 */
	MX53_PAD_EIM_EB2__I2C2_SCL,	/* GPIO2[30] */
	MX53_PAD_KEY_ROW3__I2C2_SDA,	/* GPIO4[13] */

	/* I2C3 */
	MX53_PAD_GPIO_3__I2C3_SCL,	/* GPIO1[3] */
	/* sda is board dependent */


	/* IPU - Camera */
//	MX53_PAD_CSI0_D8__CSI0_D8,	/* ov5640 doesn't use D8-D11 */
//	MX53_PAD_CSI0_D9__CSI0_D9,
//	MX53_PAD_CSI0_D10__CSI0_D10,
//	MX53_PAD_CSI0_D11__CSI0_D11,
	MX53_PAD_CSI0_D12__CSI0_D12,
	MX53_PAD_CSI0_D13__CSI0_D13,
	MX53_PAD_CSI0_D14__CSI0_D14,
	MX53_PAD_CSI0_D15__CSI0_D15,
	MX53_PAD_CSI0_D16__CSI0_D16,
	MX53_PAD_CSI0_D17__CSI0_D17,
	MX53_PAD_CSI0_D18__CSI0_D18,
	MX53_PAD_CSI0_D19__CSI0_D19,
	MX53_PAD_CSI0_VSYNC__CSI0_VSYNC,
	MX53_PAD_CSI0_MCLK__CSI0_HSYNC,
	MX53_PAD_CSI0_PIXCLK__CSI0_PIXCLK,
	MX53_PAD_NANDF_CS2__CSI0_MCLK,
	MX53_PAD_GPIO_2__GPIO_1_2,	/* CAMERA_POWERDOWN (nitrogen53) */
	MX53_PAD_GPIO_7__GPIO_1_7,	/* CAMERA_STROBE */
	MX53_PAD_KEY_COL4__GPIO_4_14,	/* CAMERA_RESET */
	MX53_PAD_CSI0_DATA_EN__GPIO_5_20, /* Camera Standby */

	/* IPU - Display */
	MX53_PAD_DI0_DISP_CLK__DI0_DISP_CLK,	/* DI0 display clock */
	MX53_PAD_DI0_PIN15__DI0_PIN15,		/* DI0 data enable */
	MX53_PAD_DI0_PIN2__DI0_PIN2,		/* DI0 HSYNC */
	MX53_PAD_DI0_PIN3__DI0_PIN3,		/* DI0 VSYNC */
	MX53_PAD_DISP0_DAT0__DISP0_DAT0,
	MX53_PAD_DISP0_DAT1__DISP0_DAT1,
	MX53_PAD_DISP0_DAT2__DISP0_DAT2,
	MX53_PAD_DISP0_DAT3__DISP0_DAT3,
	MX53_PAD_DISP0_DAT4__DISP0_DAT4,
	MX53_PAD_DISP0_DAT5__DISP0_DAT5,
	MX53_PAD_DISP0_DAT6__DISP0_DAT6,
	MX53_PAD_DISP0_DAT7__DISP0_DAT7,
	MX53_PAD_DISP0_DAT8__DISP0_DAT8,
	MX53_PAD_DISP0_DAT9__DISP0_DAT9,
	MX53_PAD_DISP0_DAT10__DISP0_DAT10,
	MX53_PAD_DISP0_DAT11__DISP0_DAT11,
	MX53_PAD_DISP0_DAT12__DISP0_DAT12,
	MX53_PAD_DISP0_DAT13__DISP0_DAT13,
	MX53_PAD_DISP0_DAT14__DISP0_DAT14,
	MX53_PAD_DISP0_DAT15__DISP0_DAT15,
	MX53_PAD_DISP0_DAT16__DISP0_DAT16,
	MX53_PAD_DISP0_DAT17__DISP0_DAT17,
	MX53_PAD_DISP0_DAT18__DISP0_DAT18,
	MX53_PAD_DISP0_DAT19__DISP0_DAT19,
	MX53_PAD_DISP0_DAT20__DISP0_DAT20,
	MX53_PAD_DISP0_DAT21__DISP0_DAT21,
	MX53_PAD_DISP0_DAT22__DISP0_DAT22,
	MX53_PAD_DISP0_DAT23__DISP0_DAT23,
	MX53_PAD_EIM_RW__DI1_PIN8,	/* VGA VSync */

	/* LDB - LVDS0 */
	MX53_PAD_LVDS0_TX3_P__LVDS0_TX3,
	MX53_PAD_LVDS0_CLK_P__LVDS0_CLK,
	MX53_PAD_LVDS0_TX2_P__LVDS0_TX2,
	MX53_PAD_LVDS0_TX1_P__LVDS0_TX1,
	MX53_PAD_LVDS0_TX0_P__LVDS0_TX0,

	/* LDB - LVDS1 */
	MX53_PAD_LVDS1_TX3_P__LVDS1_TX3,
	MX53_PAD_LVDS1_CLK_P__LVDS1_CLK,
	MX53_PAD_LVDS1_TX2_P__LVDS1_TX2,
	MX53_PAD_LVDS1_TX1_P__LVDS1_TX1,
	MX53_PAD_LVDS1_TX0_P__LVDS1_TX0,

	/* PWM1 backlight */
	MX53_PAD_GPIO_9__PWMO,		/* pwm1 */

	/* PWM2 backlight */
	MX53_PAD_GPIO_1__PWMO,		/* pwm2 */

	/* UART1 */
	MX53_PAD_ATA_DIOW__UART1_TXD,
	MX53_PAD_ATA_DMACK__UART1_RXD,

	/* UART2 */
	MX53_PAD_ATA_DMARQ__UART2_TXD,
	MX53_PAD_ATA_BUFFER_EN__UART2_RXD,
	MX53_PAD_ATA_INTRQ__UART2_CTS,
	MX53_PAD_ATA_DIOR__UART2_RTS,
};

/* working point(wp): 0 - 800MHz; 1 - 166.25MHz; */
static struct cpu_wp cpu_wp_auto[] = {
	{
	 .pll_rate = 1000000000,
	 .cpu_rate = 1000000000,
	 .pdf = 0,
	 .mfi = 10,
	 .mfd = 11,
	 .mfn = 5,
	 .cpu_podf = 0,
	 .cpu_voltage = 1150000,},
	{
	 .pll_rate = 800000000,
	 .cpu_rate = 800000000,
	 .pdf = 0,
	 .mfi = 8,
	 .mfd = 2,
	 .mfn = 1,
	 .cpu_podf = 0,
	 .cpu_voltage = 1050000,},
	{
	 .pll_rate = 800000000,
	 .cpu_rate = 160000000,
	 .pdf = 4,
	 .mfi = 8,
	 .mfd = 2,
	 .mfn = 1,
	 .cpu_podf = 4,
	 .cpu_voltage = 850000,},
};

static struct fb_videomode video_modes[] = {
	{
	 /* NTSC TV output */
	 "TV-NTSC", 60, 720, 480, 74074,
	 122, 15,
	 18, 26,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_INTERLACED,
	 0,},
	{
	 /* PAL TV output */
	 "TV-PAL", 50, 720, 576, 74074,
	 132, 11,
	 22, 26,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_INTERLACED | FB_VMODE_ODD_FLD_FIRST,
	 0,},
	{
	 /* 1080i50 TV output */
	 "1080I50", 50, 1920, 1080, 13468,
	 192, 527,
	 20, 24,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_INTERLACED | FB_VMODE_ODD_FLD_FIRST,
	 0,},
	{
	 /* 1080i60 TV output */
	 "1080I60", 60, 1920, 1080, 13468,
	 192, 87,
	 20, 24,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_INTERLACED | FB_VMODE_ODD_FLD_FIRST,
	 0,},
	{
	 /* 800x480 @ 57 Hz , pixel clk @ 27MHz */
	 "CLAA-WVGA", 57, 800, 480, 37037, 40, 60, 10, 10, 20, 10,
	 FB_SYNC_CLK_LAT_FALL,
	 FB_VMODE_NONINTERLACED,
	 0,},
	{
	 "XGA", 60, 1024, 768, 15385,
	 220, 40,
	 21, 7,
	 60, 10,
	 0,
	 FB_VMODE_NONINTERLACED,
	 0,},
	{
	 /* 720p30 TV output */
	 "720P30", 30, 1280, 720, 13468,
	 260, 1759,
	 25, 4,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_NONINTERLACED,
	 0,},
	{
	 "720P60", 60, 1280, 720, 13468,
	 260, 109,
	 25, 4,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_NONINTERLACED,
	 0,},
	{
	/* VGA 1280x1024 108M pixel clk output */
	"SXGA", 60, 1280, 1024, 9259,
	48, 248,
	1, 38,
	112, 3,
	0,
	FB_VMODE_NONINTERLACED,
	0,},
	{
	/* 1600x1200 @ 60 Hz 162M pixel clk*/
	"UXGA", 60, 1600, 1200, 6172,
	304, 64,
	1, 46,
	192, 3,
	FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT,
	FB_VMODE_NONINTERLACED,
	0,},
	{
	 /* 1080p24 TV output */
	 "1080P24", 24, 1920, 1080, 13468,
	 192, 637,
	 38, 6,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_NONINTERLACED,
	 0,},
	{
	 /* 1080p25 TV output */
	 "1080P25", 25, 1920, 1080, 13468,
	 192, 527,
	 38, 6,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_NONINTERLACED,
	 0,},
	{
	 /* 1080p30 TV output */
	 "1080P30", 30, 1920, 1080, 13468,
	 192, 87,
	 38, 6,
	 1, 1,
	 FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	 FB_VMODE_NONINTERLACED,
	 0,},
	{
	 "1080P60", 60, 1920, 1080, 7692,
	 100, 40,
	 30, 3,
	 10, 2,
	 0,
	 FB_VMODE_NONINTERLACED,
	 0,},
};

struct cpu_wp *mx53_evk_get_cpu_wp(int *wp)
{
	*wp = num_cpu_wp;
	return cpu_wp_auto;
}

void mx53_evk_set_num_cpu_wp(int num)
{
	num_cpu_wp = num;
	return;
}

static struct mxc_w1_config mxc_w1_data = {
	.search_rom_accelerator = 1,
};

#if defined(CONFIG_MXC_PWM) && defined(CONFIG_BACKLIGHT_PWM)
/* GPIO_1 lcd backlight(pwm2) */
static struct platform_pwm_backlight_data mxc_backlight_data1 = {
	.pwm_id = 1,
	.max_brightness = 255,
	.dft_brightness = CONFIG_DEFAULT_PWM0_BACKLIGHT,
	.pwm_period_ns = 1000000000/32768,	/* 30517 */
};
#endif

#if defined(CONFIG_MXC_PWM) && defined(CONFIG_BACKLIGHT_PWM)
/* GPIO_9 backlight (pwm1) */
static struct platform_pwm_backlight_data mxc_backlight_data2 = {
	.pwm_id = 0,
	.max_brightness = 255,
	.dft_brightness = CONFIG_DEFAULT_PWM1_BACKLIGHT,
	.pwm_period_ns = 1000000000/32768,	/* 30517 */
};
#endif

extern void mx5_ipu_reset(void);
static struct mxc_ipu_config mxc_ipu_data = {
	.rev = 3,
	.reset = mx5_ipu_reset,
};

extern void mx5_vpu_reset(void);
static struct mxc_vpu_platform_data mxc_vpu_data = {
	.reset = mx5_vpu_reset,
};

static struct fec_platform_data fec_data = {
	.phy = PHY_INTERFACE_MODE_GMII,
};

/* workaround for ecspi chipselect pin may not keep correct level when idle */
static void mx53_evk_gpio_spi_chipselect_active(int cspi_mode, int status,
					     int chipselect)
{
	if ((cspi_mode == 1) && (chipselect == 2)) {
		gpio_set_value(N53_SS1, 0);		/* low active */
	}
}

static void mx53_evk_gpio_spi_chipselect_inactive(int cspi_mode, int status,
					       int chipselect)
{
	if ((cspi_mode == 1) && (chipselect == 2)) {
		gpio_set_value(N53_SS1, 1);		/* low active */
	}
}

static struct mxc_spi_master mxcspi1_data = {
	.maxchipselect = 4,
	.spi_version = 23,
	.chipselect_active = mx53_evk_gpio_spi_chipselect_active,
	.chipselect_inactive = mx53_evk_gpio_spi_chipselect_inactive,
};

static struct mxc_spi_master mxcspi2_data = {
	.maxchipselect = 4,
	.spi_version = 23,
};

#define PRINT_SDA
#define PD_CLK_I2C	0
#define PD_CLK_GP	1
#define PD_SDA_I2C	2
#define PD_SDA_GP	3
/* Generate a pulse on the i2c clock pin. */
static void i2c_clock_toggle(unsigned gp_clk, unsigned gp_dat, const iomux_v3_cfg_t *pd)
{
	unsigned i;
	printk(KERN_INFO "%s, gp_clk=0x%x, gp_dat=0x%x\n", __FUNCTION__, gp_clk, gp_dat);
	gpio_direction_input(gp_clk);
	mxc_iomux_v3_setup_pad(pd[PD_CLK_GP]);

#ifdef PRINT_SDA
	gpio_direction_input(gp_dat);
	mxc_iomux_v3_setup_pad(pd[PD_SDA_GP]);
	printk(KERN_INFO "%s dat = %i\n", __FUNCTION__, gpio_get_value(gp_dat));
#endif
	/* Send high and low on the SCL line */
	for (i = 0; i < 9; i++) {
		gpio_direction_output(gp_clk,0);
		udelay(20);
		gpio_direction_input(gp_clk);
#ifdef PRINT_SDA
		printk(KERN_INFO "%s dat = %i\n", __FUNCTION__, gpio_get_value(gp_dat));
#endif
		udelay(20);
	}

        mxc_iomux_v3_setup_pad(pd[PD_CLK_I2C]);
#ifdef PRINT_SDA
	mxc_iomux_v3_setup_pad(pd[PD_SDA_I2C]);
#endif
}

static void i2c_clock_toggle0(void)
{
	const iomux_v3_cfg_t pd[] = {
		MX53_PAD_EIM_D21__I2C1_SCL, MX53_PAD_EIM_D21__GPIO_3_21,
		MX53_PAD_EIM_D28__I2C1_SDA, MX53_PAD_EIM_D28__GPIO_3_28,
	};
	i2c_clock_toggle(N53_I2C_0_SCL, N53_I2C_0_SDA, pd);
}

static void i2c_clock_toggle1(void)
{
	const iomux_v3_cfg_t pd[] = {
		MX53_PAD_EIM_EB2__I2C2_SCL, NEW_PAD_CTRL(MX53_PAD_EIM_EB2__GPIO_2_30, PAD_CTRL_9) | MUX_SION_MASK,
		MX53_PAD_KEY_ROW3__I2C2_SDA, NEW_PAD_CTRL(MX53_PAD_KEY_ROW3__GPIO_4_13, PAD_CTRL_9) | MUX_SION_MASK,
	};
	i2c_clock_toggle(N53_I2C_1_SCL, N53_I2C_1_SDA, pd);
}

static void i2c_clock_toggle2_current(void)
{
	const iomux_v3_cfg_t pd[] = {
		MX53_PAD_GPIO_3__I2C3_SCL, NEW_PAD_CTRL(MX53_PAD_GPIO_3__GPIO_1_3, PAD_CTRL_9) | MUX_SION_MASK,
		MX53_PAD_GPIO_16__I2C3_SDA, NEW_PAD_CTRL(MX53_PAD_GPIO_16__GPIO_7_11, PAD_CTRL_9) | MUX_SION_MASK,
	};
	i2c_clock_toggle(N53_I2C_2_SCL, N53_I2C_2_SDA_CURRENT, pd);
}

#if defined(CONFIG_MACH_NITROGEN_V1_IMX53)||defined(CONFIG_MACH_NITROGEN_V2_IMX53)
static void i2c_clock_toggle2_previous(void)
{
	const iomux_v3_cfg_t pd[] = {
		MX53_PAD_GPIO_3__I2C3_SCL, NEW_PAD_CTRL(MX53_PAD_GPIO_3__GPIO_1_3, PAD_CTRL_9) | MUX_SION_MASK,
		MX53_PAD_GPIO_6__I2C3_SDA, NEW_PAD_CTRL(MX53_PAD_GPIO_6__GPIO_1_6, PAD_CTRL_9) | MUX_SION_MASK,
	};
	i2c_clock_toggle(N53_I2C_2_SCL, N53_I2C_2_SDA_PREVIOUS, pd);
}
static struct imxi2c_platform_data mxci2c2_previous_data = {
	.bitrate = 100000,
	.i2c_clock_toggle = i2c_clock_toggle2_previous,
};
#endif

static struct imxi2c_platform_data mxci2c0_data = {
	.bitrate = 100000,
	.i2c_clock_toggle = i2c_clock_toggle0,
};
static struct imxi2c_platform_data mxci2c1_data = {
	.bitrate = 100000,
	.i2c_clock_toggle = i2c_clock_toggle1,
};
static struct imxi2c_platform_data mxci2c2_data = {
	.bitrate = 100000,
	.i2c_clock_toggle = i2c_clock_toggle2_current,
};

static struct mxc_dvfs_platform_data dvfs_core_data = {
	.reg_id = "DA9052_BUCK_CORE",
	.clk1_id = "cpu_clk",
	.clk2_id = "gpc_dvfs_clk",
	.gpc_cntr_offset = MXC_GPC_CNTR_OFFSET,
	.gpc_vcr_offset = MXC_GPC_VCR_OFFSET,
	.ccm_cdcr_offset = MXC_CCM_CDCR_OFFSET,
	.ccm_cacrr_offset = MXC_CCM_CACRR_OFFSET,
	.ccm_cdhipr_offset = MXC_CCM_CDHIPR_OFFSET,
	.prediv_mask = 0x1F800,
	.prediv_offset = 11,
	.prediv_val = 3,
	.div3ck_mask = 0xE0000000,
	.div3ck_offset = 29,
	.div3ck_val = 2,
	.emac_val = 0x08,
	.upthr_val = 25,
	.dnthr_val = 9,
	.pncthr_val = 33,
	.upcnt_val = 10,
	.dncnt_val = 10,
	.delay_time = 30,
	.num_wp = 3,
};

static struct mxc_bus_freq_platform_data bus_freq_data = {
	.gp_reg_id = "DA9052_BUCK_CORE",
	.lp_reg_id = "DA9052_BUCK_PRO",
};

static struct tve_platform_data tve_data = {
	.dac_reg = "VVIDEO",
};

static struct ldb_platform_data ldb_data = {
	.lvds_bg_reg = "VAUDIO",
	.ext_ref = LDB_EXT_REF,
};

static void mxc_iim_enable_fuse(void)
{
	u32 reg;

	if (!ccm_base)
		return;

	/* enable fuse blown */
	reg = readl(ccm_base + 0x64);
	reg |= 0x10;
	writel(reg, ccm_base + 0x64);
}

static void mxc_iim_disable_fuse(void)
{
	u32 reg;

	if (!ccm_base)
		return;
	/* enable fuse blown */
	reg = readl(ccm_base + 0x64);
	reg &= ~0x10;
	writel(reg, ccm_base + 0x64);
}

static struct mxc_iim_data iim_data = {
	.bank_start = MXC_IIM_MX53_BANK_START_ADDR,
	.bank_end   = MXC_IIM_MX53_BANK_END_ADDR,
	.enable_fuse = mxc_iim_enable_fuse,
	.disable_fuse = mxc_iim_disable_fuse,
};


static void mx53_gpio_usbotg_driver_vbus(bool on)
{
	/* Enable OTG VBus with GPIO high */
	/* Disable OTG VBus with GPIO low */
	gpio_set_value(N53_OTG_VBUS, on ? 1 : 0);
	pr_info("%s: on=%d\n", __func__, on);
}

static void mx53_gpio_host1_driver_vbus(bool on)
{
	gpio_set_value(EVK_USBH1_VBUS, on ? 1 : 0);
	pr_info("%s: on=%d\n", __func__, on);
}

static struct resource mxcfb_resources[] = {
	[0] = {
	       .flags = IORESOURCE_MEM,
	       },
};

static struct mxc_fb_platform_data fb_data[] = {
	{
	 .interface_pix_fmt = IPU_PIX_FMT_RGB565,
	 .mode_str = "CLAA-WVGA",
	 .mode = video_modes,
	 .num_modes = ARRAY_SIZE(video_modes),
	 },
	{
	 .interface_pix_fmt = IPU_PIX_FMT_GBR24,
	 .mode_str = "1024x768M-16@60",
	 .mode = video_modes,
	 .num_modes = ARRAY_SIZE(video_modes),
	 },
};

extern int primary_di;
static int __init mxc_init_fb(void)
{
//	if (!machine_is_mx53_evk())
//		return 0;

	if (primary_di) {
		printk(KERN_INFO "DI1 is primary\n");
		/* DI1 -> DP-BG channel: */
		mxc_fb_devices[1].num_resources = ARRAY_SIZE(mxcfb_resources);
		mxc_fb_devices[1].resource = mxcfb_resources;
		mxc_register_device(&mxc_fb_devices[1], &fb_data[1]);

		/* DI0 -> DC channel: */
		mxc_register_device(&mxc_fb_devices[0], &fb_data[0]);
	} else {
		printk(KERN_INFO "DI0 is primary\n");

		/* DI0 -> DP-BG channel: */
		mxc_fb_devices[0].num_resources = ARRAY_SIZE(mxcfb_resources);
		mxc_fb_devices[0].resource = mxcfb_resources;
		mxc_register_device(&mxc_fb_devices[0], &fb_data[0]);

		/* DI1 -> DC channel: */
		mxc_register_device(&mxc_fb_devices[1], &fb_data[1]);
	}

	/*
	 * DI0/1 DP-FG channel:
	 */
	mxc_register_device(&mxc_fb_devices[2], NULL);

	return 0;
}
device_initcall(mxc_init_fb);

struct plat_i2c_generic_data {
	unsigned irq;
	unsigned gp;
};

static struct plat_i2c_generic_data i2c_generic_data = {
	.irq = gpio_to_irq(N53_I2C_CONNECTOR_INT),
	.gp = N53_I2C_CONNECTOR_INT,
};

struct plat_i2c_tfp410_data {
	int irq;
	int gp;
	int gp_i2c_sel;
};

static struct plat_i2c_tfp410_data i2c_tfp410_data = {
	.irq = gpio_to_irq(N53_TFP410_INT), .gp = N53_TFP410_INT,
	.gp_i2c_sel = N53_TFP410_I2CMODE
};

struct sc16is7xx_gpio_platform_data i2c_sc16is7xx_gpio_data = {
	.gpio_base = -1,
	.irq_base = MXC_BOARD_IRQ_START,
};

static struct sc16is7xx_platform_data i2c_sc16is7xx_data = {
	.irq = gpio_to_irq(N53_SC16IS7XX_INT),
	.gp = N53_SC16IS7XX_INT,
	.gpio_data = &i2c_sc16is7xx_gpio_data,
};

#if defined(CONFIG_MTD)
static struct mtd_partition mxc_spi_nor_partitions[] = {
	{
	 .name = "bootloader",
	 .offset = 0,
	 .size = 0x000100000,},
	{
	 .name = "kernel",
	 .offset = MTDPART_OFS_APPEND,
	 .size = MTDPART_SIZ_FULL,},
};

static struct flash_platform_data mxc_spi_flash_data = {
	.name = "m25p80",
	.parts = mxc_spi_nor_partitions,
	.nr_parts = ARRAY_SIZE(mxc_spi_nor_partitions),
	.type = "sst25vf016b",
};


static struct spi_board_info mxc_spi_nor_device[] __initdata = {
	{
	 .modalias = "m25p80",
	 .max_speed_hz = 25000000,	/* max spi clock (SCK) speed in HZ */
	 .bus_num = 1,
	 .chip_select = 1,
	 .platform_data = &mxc_spi_flash_data,
	},
};
#endif

#if defined(CONFIG_LTC1960) || defined(CONFIG_LTC1960_MODULE)
#if defined (CONFIG_BATTERY_BQ20Z75) || defined (CONFIG_BATTERY_BQ20Z75)
static struct ltc1960_battery_info_t removable_battery = {
	.name = "bq20z75"
,	.charge_uv = 12600000
,	.charge_ua = 2500000
,	.trickle_seconds = 5*60
};
#endif

static struct ltc1960_battery_info_t permanent_battery = {
	.name = "dumb"
,	.charge_uv = 12600000
,	.charge_ua = 1500000
,	.trickle_seconds = 60
};

static struct ltc1960_platform_data_t ltc1960_pdata = {
	.batteries = {
		&permanent_battery,
#if defined (CONFIG_BATTERY_BQ20Z75) || defined (CONFIG_BATTERY_BQ20Z75)
		&removable_battery,
#endif
	}
};
#endif

static struct spi_board_info spidev[] __initdata = {
	{
#if defined(CONFIG_LTC1960) || defined(CONFIG_LTC1960_MODULE)
	.modalias = "ltc1960",
	.platform_data = &ltc1960_pdata,
	 .max_speed_hz = 100000,	/* max spi clock (SCK) speed in HZ */
#else
	 .modalias = "spidev",
	 .max_speed_hz = 1000000,	/* max spi clock (SCK) speed in HZ */
#endif
	 .bus_num = 2,
	 .chip_select = 1}
};

static int sdhc_write_protect(struct device *dev)
{
	unsigned short rc = 0;

	if (to_platform_device(dev)->id == 0)
		rc = gpio_get_value(N53_SD1_WP);
	else
		rc = gpio_get_value(N53_SD3_WP);

	return rc;
}

static unsigned int sdhc_get_card_det_status(struct device *dev)
{
	int ret;
	if (to_platform_device(dev)->id == 0) {
		ret = gpio_get_value(N53_SD1_CD);
	} else {		/* config the det pin for SDHC3 */
		ret = gpio_get_value(N53_SD3_CD);
	}

	return ret;
}

static struct mxc_mmc_platform_data mmc1_data = {
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30
		| MMC_VDD_31_32,
	.caps = MMC_CAP_4_BIT_DATA,
	.min_clk = 400000,
	.max_clk = 50000000,
	.card_inserted_state = 0,
	.status = sdhc_get_card_det_status,
	.wp_status = sdhc_write_protect,
	.clock_mmc = "esdhc_clk",
	.power_mmc = NULL,
};

static struct mxc_mmc_platform_data mmc3_data = {
	.ocr_mask = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_29_30
		| MMC_VDD_31_32,
	.caps = MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA
		| MMC_CAP_DATA_DDR,
	.min_clk = 400000,
	.max_clk = 50000000,
	.card_inserted_state = 0,
	.status = sdhc_get_card_det_status,
	.wp_status = sdhc_write_protect,
	.clock_mmc = "esdhc_clk",
};

static int mxc_sgtl5000_amp_enable(int enable)
{
	gpio_set_value(N53_AMP_ENABLE, enable);
	return 0;
}

static int headphone_det_status(void)
{
	return 0;
}

static int mxc_sgtl5000_init(void);

static struct mxc_audio_platform_data sgtl5000_data = {
	.ssi_num = 1,
	.src_port = 2,
	.ext_port = 4,
	.hp_status = headphone_det_status,
	.amp_enable = mxc_sgtl5000_amp_enable,
	.sysclk = 26000000,
	.init = mxc_sgtl5000_init,
};

static int mxc_sgtl5000_init(void)
{
#ifndef REV0
	struct clk *ssi_ext1;
	int rate;

	ssi_ext1 = clk_get(NULL, "ssi_ext1_clk");
	if (IS_ERR(ssi_ext1))
		return -1;

	rate = clk_round_rate(ssi_ext1, 26000000);
	if (rate < 8000000 || rate > 27000000) {
		printk(KERN_ERR "Error: SGTL5000 mclk freq %d out of range!\n",
		       rate);
		clk_put(ssi_ext1);
		return -1;
	}

	clk_set_rate(ssi_ext1, rate);
	printk(KERN_INFO "SGTL5000 mclk freq is %d\n", rate);
	clk_enable(ssi_ext1);
	sgtl5000_data.sysclk = rate;
#endif
	return 0;
}

static struct platform_device mxc_sgtl5000_device = {
	.name = "imx-3stack-sgtl5000",
};

static struct mxc_mlb_platform_data mlb_data = {
	.reg_nvcc = "VCAM",
	.mlb_clk = "mlb_clk",
};

#ifdef CONFIG_KEYBOARD_GPIO
static struct gpio_keys_button gpio_keys[] = {
	{
		.type	= EV_KEY,
		.gpio	= MAKE_GP(3,29),
		.code	= KEY_HOME,		/* 102 (0x66) */
		.desc	= "Home Button",
		.wakeup	= 1,
		.active_low = 1,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= MAKE_GP(3,26),
		.code	= KEY_BACK,		/* 158 (0x9E) */
		.desc	= "Back Button",
		.wakeup	= 1,
		.active_low = 1,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
#ifdef CONFIG_MACH_NITROGEN_A_IMX53
		.gpio	= MAKE_GP(3,25),
#else
		.gpio	= MAKE_GP(1,4),
#endif
		.code	= KEY_MENU,		/* 139 (0x88) */
		.desc	= "Menu Button",
		.wakeup	= 1,
		.active_low = 1,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= MAKE_GP(3,27),
		.code	= KEY_SEARCH,		/* 217 (0xD9) */
		.desc	= "Search Button",
		.wakeup	= 1,
		.active_low = 1,
		.debounce_interval = 30,
	},
#ifdef CONFIG_MACH_NITROGEN_A_IMX53
	{
		.type	= EV_KEY,
		.gpio	= MAKE_GP(3,22),
		.code	= KEY_POWER,		/* 116 (0x74) */
		.desc	= "Power Button",
		.wakeup	= 1,
		.active_low = 1,
		.debounce_interval = 30,
	},
#ifdef CONFIG_GPIO_SC16IS7XX_IRQ
	{
		.type	= EV_KEY,
		.gpio	= 255,			/* J2 Pin 1 */
		.code	= KEY_CAMERA,
		.desc	= "Camera Button",
		.wakeup	= 1,
		.active_low = 0,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= 252,			/* J4 Pin 4 */
		.code	= KEY_PHONE,
		.desc	= "Phone Button",
		.wakeup	= 1,
		.active_low = 0,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= 253,			/* J2 Pin 3 */
		.code	= KEY_VOLUMEUP,
		.desc	= "Volume+ Button",
		.wakeup	= 1,
		.active_low = 0,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= 248,			/* J2 Pin 4 */
		.code	= KEY_VOLUMEDOWN,
		.desc	= "Volume- Button",
		.wakeup	= 1,
		.active_low = 0,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= 251,			/* J4 Pin 3 */
		.code	= KEY_DOCUMENTS,
		.desc	= "Documents Button",
		.wakeup	= 1,
		.active_low = 0,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= 249,			/* J4 Pin 1 */
		.code	= KEY_F1,
		.desc	= "Torch LED Button",
		.wakeup	= 1,
		.active_low = 0,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= 250,			/* J4 Pin 2 */
		.code	= KEY_F2,
		.desc	= "Splice Monitor Button",
		.wakeup	= 1,
		.active_low = 0,
		.debounce_interval = 30,
	},
	{
		.type	= EV_KEY,
		.gpio	= 254,			/* J2 Pin 2 */
		.code	= KEY_F3,
		.desc	= "Belt Survey Button",
		.wakeup	= 1,
		.active_low = 0,
		.debounce_interval = 30,
	},
#endif
#endif
};

static struct gpio_keys_platform_data gpio_keys_platform_data = {
	.buttons        = gpio_keys,
	.nbuttons       = ARRAY_SIZE(gpio_keys),
};

static struct platform_device gpio_keys_device = {
	.name   = "gpio-keys",
	.id     = -1,
	.dev    = {
		.platform_data  = &gpio_keys_platform_data,
	},
};
#endif

/* NAND Flash Partitions */
#ifdef CONFIG_MTD_PARTITIONS
static struct mtd_partition nand_flash_partitions[] = {
/* MX53 ROM require the boot FCB/DBBT support which need
 * more space to store such info on NAND boot partition.
 * 16M should cover all kind of NAND boot support on MX53.
 */
	{
	 .name = "bootloader",
	 .offset = 0,
	 .size = 16 * 1024 * 1024},
	{
	 .name = "nand.kernel",
	 .offset = MTDPART_OFS_APPEND,
	 .size = 5 * 1024 * 1024},
	{
	 .name = "nand.rootfs",
	 .offset = MTDPART_OFS_APPEND,
	 .size = 256 * 1024 * 1024},
	{
	 .name = "nand.userfs1",
	 .offset = MTDPART_OFS_APPEND,
	 .size = 256 * 1024 * 1024},
	{
	 .name = "nand.userfs2",
	 .offset = MTDPART_OFS_APPEND,
	 .size = MTDPART_SIZ_FULL},
};
#endif

static int nand_init(void)
{
	u32 i, reg;
	void __iomem *base;

	#define M4IF_GENP_WEIM_MM_MASK          0x00000001
	#define WEIM_GCR2_MUX16_BYP_GRANT_MASK  0x00001000

	base = ioremap(MX53_BASE_ADDR(M4IF_BASE_ADDR), SZ_4K);
	reg = __raw_readl(base + 0xc);
	reg &= ~M4IF_GENP_WEIM_MM_MASK;
	__raw_writel(reg, base + 0xc);

	iounmap(base);

	base = ioremap(MX53_BASE_ADDR(WEIM_BASE_ADDR), SZ_4K);
	for (i = 0x4; i < 0x94; i += 0x18) {
		reg = __raw_readl((u32)base + i);
		reg &= ~WEIM_GCR2_MUX16_BYP_GRANT_MASK;
		__raw_writel(reg, (u32)base + i);
	}

	iounmap(base);

	return 0;
}

static struct flash_platform_data mxc_nand_data = {
#ifdef CONFIG_MTD_PARTITIONS
	.parts = nand_flash_partitions,
	.nr_parts = ARRAY_SIZE(nand_flash_partitions),
#endif
	.width = 1,
	.init = nand_init,
};

static struct mxc_spdif_platform_data mxc_spdif_data = {
	.spdif_tx = 1,
	.spdif_rx = 0,
	.spdif_clk_44100 = 0,	/* Souce from CKIH1 for 44.1K */
	.spdif_clk_48000 = 7,	/* Source from CKIH2 for 48k and 32k */
	.spdif_clkid = 0,
	.spdif_clk = NULL,	/* spdif bus clk */
};


static int __initdata enable_w1 = { 0 };
static int __init w1_setup(char *__unused)
{
	enable_w1 = 1;
	return cpu_is_mx53();
}
__setup("w1", w1_setup);


static int __initdata enable_spdif = { 0 };
static int __init spdif_setup(char *__unused)
{
	enable_spdif = 1;
	return 1;
}
__setup("spdif", spdif_setup);

static struct android_pmem_platform_data android_pmem_data = {
	.name = "pmem_adsp",
	.size = SZ_32M,
};

static struct android_pmem_platform_data android_pmem_gpu_data = {
	.name = "pmem_gpu",
	.size = SZ_32M,
	.cached = 1,
};

static char *usb_functions_ums[] = {
	"usb_mass_storage",
};

static char *usb_functions_ums_adb[] = {
	"usb_mass_storage",
	"adb",
};

static char *usb_functions_rndis[] = {
	"rndis",
};

static char *usb_functions_rndis_adb[] = {
	"rndis",
	"adb",
};

static char *usb_functions_all[] = {
	"rndis",
	"usb_mass_storage",
	"adb"
};

static struct android_usb_product usb_products[] = {
	{
		.product_id	= 0x0c01,
		.num_functions	= ARRAY_SIZE(usb_functions_ums),
		.functions	= usb_functions_ums,
	},
	{
		.product_id	= 0x0c02,
		.num_functions	= ARRAY_SIZE(usb_functions_ums_adb),
		.functions	= usb_functions_ums_adb,
	},
	{
		.product_id	= 0x0c03,
		.num_functions	= ARRAY_SIZE(usb_functions_rndis),
		.functions	= usb_functions_rndis,
	},
	{
		.product_id	= 0x0c04,
		.num_functions	= ARRAY_SIZE(usb_functions_rndis_adb),
		.functions	= usb_functions_rndis_adb,
	},
};

static struct usb_mass_storage_platform_data mass_storage_data = {
	.nluns		= 3,
	.vendor		= "Freescale",
	.product	= "Android Phone",
	.release	= 0x0100,
};

static struct usb_ether_platform_data rndis_data = {
	.vendorID	= 0x0bb4,
	.vendorDescr	= "Freescale",
};

static struct android_usb_platform_data android_usb_data = {
	.vendor_id      = 0x0bb4,
	.product_id     = 0x0c01,
	.version        = 0x0100,
	.product_name   = "Android Phone",
	.manufacturer_name = "Freescale",
	.num_products = ARRAY_SIZE(usb_products),
	.products = usb_products,
	.num_functions = ARRAY_SIZE(usb_functions_all),
	.functions = usb_functions_all,
};

#if defined(CONFIG_VIDEO_BOUNDARY_CAMERA) || defined(CONFIG_VIDEO_BOUNDARY_CAMERA_MODULE)
#define MAX_CAMERA_FRAME_SIZE (((2592*1944*2+PAGE_SIZE-1)/PAGE_SIZE)*PAGE_SIZE)
#define MAX_CAMERA_FRAME_COUNT 4
#define MAX_CAMERA_MEM SZ_64M

static unsigned long camera_buf_phys = 0UL ;
unsigned long get_camera_phys(unsigned maxsize) {
	if (maxsize <= MAX_CAMERA_MEM)
		return camera_buf_phys ;
	else
		return 0UL ;
}
EXPORT_SYMBOL(get_camera_phys);

#endif

#if (defined(CONFIG_VIDEO_BOUNDARY_CAMERA) || defined(CONFIG_VIDEO_BOUNDARY_CAMERA_MODULE)) \
    && (defined(CONFIG_TOUCHSCREEN_DA9052) || defined(CONFIG_TOUCHSCREEN_DA9052)) \
    && defined(CONFIG_FIVE_WIRE)
    #error cannot simultaneously support camera and DA905x five wire touch
#endif    

/*!
 * Board specific fixup function. It is called by \b setup_arch() in
 * setup.c file very early on during kernel starts. It allows the user to
 * statically fill in the proper values for the passed-in parameters. None of
 * the parameters is used currently.
 *
 * @param  desc         pointer to \b struct \b machine_desc
 * @param  tags         pointer to \b struct \b tag
 * @param  cmdline      pointer to the command line
 * @param  mi           pointer to \b struct \b meminfo
 */
static void __init fixup_mxc_board(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
	char *str;
	struct tag *t;
	struct tag *mem_tag = 0;
	int total_mem = SZ_1G;
	int temp_mem = 0;
#if defined(CONFIG_MXC_AMD_GPU) || defined(CONFIG_MXC_AMD_GPU_MODULE)
	int gpu_mem = SZ_64M;
#endif

	mxc_set_cpu_type(MXC_CPU_MX53);
	get_cpu_wp = mx53_evk_get_cpu_wp;
	set_num_cpu_wp = mx53_evk_set_num_cpu_wp;

	for_each_tag(t, tags) {
		if (t->hdr.tag == ATAG_CMDLINE) {
			str = t->u.cmdline.cmdline;
			str = strstr(str, "mem=");
			if (str != NULL) {
				str += 4;
				temp_mem = memparse(str, &str);
			}

#if defined(CONFIG_MXC_AMD_GPU) || defined(CONFIG_MXC_AMD_GPU_MODULE)
			str = t->u.cmdline.cmdline;
			str = strstr(str, "gpu_memory=");
			if (str != NULL) {
				str += 11;
				gpu_mem = memparse(str, &str);
			}
#endif
			break;
		}
	}

	for_each_tag(mem_tag, tags) {
		if (mem_tag->hdr.tag == ATAG_MEM) {
			total_mem = mem_tag->u.mem.size;
			break;
		}
	}

	if (temp_mem > 0 && temp_mem < total_mem)
		total_mem = temp_mem;

	if (mem_tag) {
#if defined(CONFIG_MXC_AMD_GPU) || defined(CONFIG_MXC_AMD_GPU_MODULE)
		/*reserve memory for gpu*/
		gpu_device.resource[5].end = mem_tag->u.mem.start + total_mem - 1 ;
		gpu_device.resource[5].start = mem_tag->u.mem.start + total_mem - gpu_mem ;
		total_mem -= gpu_mem ;
#endif

#ifdef CONFIG_ANDROID_PMEM
		android_pmem_data.start = mem_tag->u.mem.start + total_mem - android_pmem_data.size ;
		total_mem -= android_pmem_data.size ;
		android_pmem_gpu_data.start = mem_tag->u.mem.start + total_mem - android_pmem_gpu_data.size ;
		total_mem -= android_pmem_gpu_data.size ;
#endif
#if defined(CONFIG_VIDEO_BOUNDARY_CAMERA) || defined(CONFIG_VIDEO_BOUNDARY_CAMERA_MODULE)
		camera_buf_phys = mem_tag->u.mem.start + total_mem - MAX_CAMERA_MEM ;
		total_mem -= MAX_CAMERA_MEM ;
		printk (KERN_INFO "0x%x bytes of camera mem at 0x%lx\n", MAX_CAMERA_MEM, camera_buf_phys);
#endif
		mem_tag->u.mem.size = total_mem ;
	}
#ifdef CONFIG_DEBUG_LL
	mx5_map_uart();
#endif
}

#if defined(CONFIG_VIDEO_BOUNDARY_CAMERA) || defined(CONFIG_VIDEO_BOUNDARY_CAMERA_MODULE)
static struct platform_device boundary_camera_device = {
	.name = "boundary_camera",
};

static struct platform_device boundary_camera_interfaces[] = {
#ifdef CONFIG_BOUNDARY_CAMERA_CSI0
	{ .name = "boundary_camera_csi0", },
#endif
#ifdef CONFIG_BOUNDARY_CAMERA_CSI1
	{ .name = "boundary_camera_csi1", },
#endif
};

static struct mxc_camera_platform_data camera_data = {
	.io_regulator = "VDD_IO",
	.analog_regulator = "VDD_A",
	.mclk = 26000000,
	.csi = 0,
	.power_down = MAKE_GP(1, 2),
	.reset = CAMERA_RESET,
	.i2c_bus = 1,
	.i2c_id = 0x3c,
	.sensor_name = "ov5640",
};

static void init_camera(void)
{
	struct clk *clk ;
	int i ;
	clk = clk_get(NULL,"csi_mclk1");
	if(clk){
		clk_set_rate(clk,24000000);
	} else
		printk(KERN_ERR "%s: Error getting CSI clock\n", __func__ );

	mxc_register_device(&boundary_camera_device, &camera_data);
	for (i = 0 ; i < ARRAY_SIZE(boundary_camera_interfaces); i++ ){
		mxc_register_device(&boundary_camera_interfaces[i], &camera_data);
	}
}
#endif

#if defined(CONFIG_DUMB_BATTERY) || defined (CONFIG_DUMB_BATTERY_MODULE)
static struct dumb_battery_platform_t dumb_plat = {
	.init_level = 30
,	.charge_sec = 60*60
,	.discharge_sec = 120*60
};

static struct platform_device dumb_battery_device = {
	.name   = "dumb_battery",
	.id     = -1,
	.dev    = {
		.platform_data  = &dumb_plat
	},
};
#endif

#if defined (CONFIG_DA905X_CHARDEV) || defined (CONFIG_DA905X_CHARDEV_MODULE)
static struct platform_device da905x_chardev_dev = {
	.name   = "da905x_chardev",
	.id     = -1,
	.dev    = {
		.platform_data  = 0
	},
};
#endif

#if defined(CONFIG_GPIO_OUTPUT) || defined (CONFIG_GPIO_OUTPUT_MODULE)
static struct platform_device gpio_output_pdev = {
       .name = "gpio_output",
};
#endif

static void __init mx53_evk_io_init(void)
{
	/* MX53 Nitrogen board */
	if (gpio_request_array(nitrogen53_gpios, ARRAY_SIZE(nitrogen53_gpios))) {
		printk (KERN_ERR "%s gpio_request_array failed\n", __func__ );
	}
	mxc_iomux_v3_setup_multiple_pads(mx53common_pads,
			ARRAY_SIZE(mx53common_pads));
	pr_info("MX53 Nitrogen board \n");

#if defined(CONFIG_DUMB_BATTERY) || defined (CONFIG_DUMB_BATTERY_MODULE)
	platform_device_register(&dumb_battery_device);
#endif

#if defined (CONFIG_DA905X_CHARDEV) || defined (CONFIG_DA905X_CHARDEV_MODULE)
	platform_device_register(&da905x_chardev_dev);
#endif

#if defined(CONFIG_GPIO_OUTPUT) || defined (CONFIG_GPIO_OUTPUT_MODULE)
       platform_device_register(&gpio_output_pdev);
#endif

#ifdef CONFIG_KEYBOARD_GPIO
	platform_device_register(&gpio_keys_device);
#endif
#if defined(CONFIG_FB_MXC_PMIC_LCD_MODULE) || defined(CONFIG_FB_MXC_PMIC_LCD)
	platform_device_register(&lcd_pmic_device);
#endif
	msleep(5);
	gpio_set_value(N53_USB_HUB_RESET, 1);		/* release USB Hub reset */
	gpio_set_value(N53_PHY_RESET, 1);		/* release ICS1893 Ethernet PHY reset */
#if defined(CONFIG_VIDEO_BOUNDARY_CAMERA) || defined(CONFIG_VIDEO_BOUNDARY_CAMERA_MODULE)
	init_camera();
#endif

#if defined (CONFIG_TOUCHSCREEN_I2C) || defined (CONFIG_TOUCHSCREEN_I2C_MODULE) \
 ||  defined (CONFIG_TOUCHSCREEN_EP0700M01) || defined (CONFIG_TOUCHSCREEN_EP0700M01_MODULE)
	gpio_set_value(N53_I2C_CONNECTOR_BUFFER_ENABLE,1);
#endif

#if defined(CONFIG_SERIAL_IMX_RS485)
	gpio_set_value(CONFIG_SERIAL_IMX_RS485_GPIO,CONFIG_SERIAL_IMX_RS485_GPIO_ACTIVE_HIGH^1);
#endif
}

static void nitrogen_power_off(void)
{
#if defined(CONFIG_MACH_NITROGEN_A_IMX53)
#define POWER_DOWN	MAKE_GP(3,23)
	gpio_set_value(POWER_DOWN, 0);
#endif
	while (1) {
	}
}

#ifdef CONFIG_WL12XX_PLATFORM_DATA
static struct regulator_consumer_supply nitrogen53_vmmc2_supply =
	REGULATOR_SUPPLY("vmmc", "mxsdhci.2");

/* VMMC2 for driving the WL12xx module */
static struct regulator_init_data nitrogen53_vmmc2 = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies = &nitrogen53_vmmc2_supply,
};

static struct fixed_voltage_config nitrogen53_vwlan = {
	.supply_name		= "vwl1271",
	.microvolts		= 1800000, /* 1.80V */
	.gpio			= N53_WL1271_WL_EN,
	.startup_delay		= 70000, /* 70ms */
	.enable_high		= 1,
	.enabled_at_boot	= 0,
	.init_data		= &nitrogen53_vmmc2,
};

static struct platform_device nitrogen53_wlan_regulator = {
	.name		= "reg-fixed-voltage",
	.id		= 1,
	.dev = {
		.platform_data	= &nitrogen53_vwlan,
	},
};

struct wl12xx_platform_data nitrogen53_wlan_data __initdata = {
	.irq = gpio_to_irq(N53_WL1271_INT),
	.board_ref_clock = WL12XX_REFCLOCK_38, /* 38.4 MHz */
};
#endif

/*!
 * Board specific initialization.
 */
static void __init mxc_board_init(struct i2c_board_info *bi0, int bi0_size,
	struct i2c_board_info *bi1, int bi1_size,
	struct i2c_board_info *bi2, int bi2_size,
	unsigned da9052_irq, struct imxi2c_platform_data *i2c2_data)
{

	mxc_ipu_data.di_clk[0] = clk_get(NULL, "ipu_di0_clk");
	mxc_ipu_data.di_clk[1] = clk_get(NULL, "ipu_di1_clk");
	mxc_ipu_data.csi_clk[0] = clk_get(NULL, "ssi_ext1_clk");
	mxc_spdif_data.spdif_core_clk = clk_get(NULL, "spdif_xtal_clk");
	clk_put(mxc_spdif_data.spdif_core_clk);

	/* SD card detect irqs */
	mxcsdhc3_device.resource[2].start = gpio_to_irq(N53_SD3_CD);
	mxcsdhc3_device.resource[2].end = gpio_to_irq(N53_SD3_CD);
	mxcsdhc1_device.resource[2].start = gpio_to_irq(N53_SD1_CD);
	mxcsdhc1_device.resource[2].end = gpio_to_irq(N53_SD1_CD);

	mxc_cpu_common_init();
	mx53_evk_io_init();

	mxc_register_device(&mxc_dma_device, NULL);
	mxc_register_device(&mxc_wdt_device, NULL);
	mxc_register_device(&mxcspi1_device, &mxcspi1_data);
	mxc_register_device(&mxcspi2_device, &mxcspi2_data);

	mxc_register_device(&mxci2c_devices[0], &mxci2c0_data);
	mxc_register_device(&mxci2c_devices[1], &mxci2c1_data);
	mxc_register_device(&mxci2c_devices[2], i2c2_data);
	mxc_register_device(&mxc_rtc_device, NULL);

	mx53_nitrogen_init_da9052(da9052_irq);

	mxc_register_device(&mxc_w1_master_device, &mxc_w1_data);
	mxc_register_device(&mxc_ipu_device, &mxc_ipu_data);
	mxc_register_device(&mxc_ldb_device, &ldb_data);
	mxc_register_device(&mxc_tve_device, &tve_data);
	mxc_register_device(&mxcvpu_device, &mxc_vpu_data);
	mxc_register_device(&gpu_device, &z160_revision);
	mxc_register_device(&mxcscc_device, NULL);

/*	mxc_register_device(&mx53_lpmode_device, NULL);
	mxc_register_device(&sdram_autogating_device, NULL);
*/
	mxc_register_device(&mxc_dvfs_core_device, &dvfs_core_data);
	mxc_register_device(&busfreq_device, &bus_freq_data);

	/*
	mxc_register_device(&mxc_dvfs_per_device, &dvfs_per_data);
	*/

	mxc_register_device(&mxc_iim_device, &iim_data);
	mxc_register_device(&mxc_pwm1_device, NULL);
	mxc_register_device(&mxc_pwm2_device, NULL);
#if defined(CONFIG_MXC_PWM) && defined(CONFIG_BACKLIGHT_PWM)
	mxc_register_device(&mxc_pwm1_backlight_device,	&mxc_backlight_data1);
	mxc_register_device(&mxc_pwm2_backlight_device,	&mxc_backlight_data2);
#endif

/*	mxc_register_device(&mxc_keypad_device, &keypad_plat_data); */

	mxc_register_device(&mxcsdhc1_device, &mmc1_data);
	mxc_register_device(&mxcsdhc3_device, &mmc3_data);
	mxc_register_device(&mxc_ssi1_device, NULL);
	mxc_register_device(&mxc_ssi2_device, NULL);
	mxc_register_device(&ahci_fsl_device, &sata_data);
	mxc_register_device(&mxc_alsa_spdif_device, &mxc_spdif_data);
	mxc_register_device(&mxc_fec_device, &fec_data);
	mxc_register_device(&mxc_ptp_device, NULL);

#if defined(CONFIG_MTD)
	spi_register_board_info(mxc_spi_nor_device,
				ARRAY_SIZE(mxc_spi_nor_device));
#endif
	spi_register_board_info(spidev,
				ARRAY_SIZE(spidev));
	if (bi0)
		i2c_register_board_info(0, bi0, bi0_size);
	if (bi1)
		i2c_register_board_info(1, bi1, bi1_size);
	if (bi2)
		i2c_register_board_info(2, bi2, bi2_size);

	pm_power_off = nitrogen_power_off;

	mxc_register_device(&mxc_sgtl5000_device, &sgtl5000_data);
	mxc_register_device(&mxc_mlb_device, &mlb_data);
	mx5_set_otghost_vbus_func(mx53_gpio_usbotg_driver_vbus);
	mx5_usb_dr_init();
	mx5_set_host1_vbus_func(mx53_gpio_host1_driver_vbus);
	mx5_usbh1_init();
	mxc_register_device(&mxc_nandv2_mtd_device, &mxc_nand_data);
	mxc_register_device(&mxc_v4l2_device, NULL);
	mxc_register_device(&mxc_v4l2out_device, NULL);
	mxc_register_device(&mxc_android_pmem_device, &android_pmem_data);
	mxc_register_device(&mxc_android_pmem_gpu_device, &android_pmem_gpu_data);
	mxc_register_device(&usb_mass_storage_device, &mass_storage_data);
	mxc_register_device(&usb_rndis_device, &rndis_data);
	mxc_register_device(&android_usb_device, &android_usb_data);

#ifdef CONFIG_WL12XX_PLATFORM_DATA
	/* WL12xx WLAN Init */
	if (wl12xx_set_platform_data(&nitrogen53_wlan_data))
		pr_err("error setting wl12xx data\n");
	platform_device_register(&nitrogen53_wlan_regulator);
#endif
}

static void __init mx53_evk_timer_init(void)
{
	mx53_clocks_init(32768, 24000000, 22579200, 24576000);
}

static struct sys_timer mxc_timer = {
	.init	= mx53_evk_timer_init,
};

/*****************************************************************************/
	/* Stuff common to MX53_NITROGEN and MX53_NITROGEN_A */
#if defined(CONFIG_MACH_NITROGEN_IMX53) || defined(CONFIG_MACH_NITROGEN_A_IMX53)
static struct i2c_board_info mxc_i2c1_board_info[] __initdata = {
#if defined (CONFIG_TOUCHSCREEN_I2C) || defined (CONFIG_TOUCHSCREEN_I2C_MODULE)
	{
	 .type = "Pic16F616-ts",
	 .addr = 0x22,
	 .platform_data  = &i2c_generic_data,
	},
#endif
#if defined (CONFIG_TOUCHSCREEN_EP0700M01) || defined (CONFIG_TOUCHSCREEN_EP0700M01_MODULE)
	{
	 .type = "ep0700m01-ts",
	 .addr = 0x38,
	 .platform_data  = &i2c_generic_data,
	},
#endif
	{
	 .type = "tfp410",
	 .addr = 0x38,
	 .platform_data  = &i2c_tfp410_data,
	},
#if defined(CONFIG_MACH_NITROGEN_A_IMX53)
	{
	 .type = "lsm303c",
	 .addr = 0x1e,
	 .platform_data  = &i2c_generic_data,
	},
	{
	 .type = "lsm303a",
	 .addr = 0x19,
	 .platform_data  = &i2c_generic_data,
	},
	{
	 .type = "bq20z75",
	 .addr = 0x0b,
	},
#endif
};

static struct i2c_board_info mxc_i2c2_board_info[] __initdata = {
	{
	 .type = "sgtl5000-i2c",
	 .addr = 0x0a,
	},
	{
	 .type = "sc16is7xx-uart",
	 .addr = 0x49,
	 .platform_data  = &i2c_sc16is7xx_data,
	},
};
#endif
/*****************************************************************************/


/*****************************************************************************/

#ifdef CONFIG_MACH_NITROGEN_A_IMX53
struct gpio nitrogen53_gpios_specific_a[] __initdata = {
	{.label = "pmic-int",		.gpio = MAKE_GP(2, 21),		.flags = GPIOF_DIR_IN},
	{.label = "Camera power down",	.gpio = MAKE_GP(2, 22),		.flags = GPIOF_INIT_HIGH},
	{.label = "mic_mux",		.gpio = MAKE_GP(6, 16),		.flags = 0},
	{.label = "i2c-2-sda",		.gpio = MAKE_GP(7, 11),		.flags = GPIOF_DIR_IN},
	{.label = "power_down_req",	.gpio = POWER_DOWN,		.flags = GPIOF_INIT_HIGH},
};

static iomux_v3_cfg_t nitrogen53_pads_specific_a[] __initdata = {
	/* ECSPI2, Nitrogen53A only */
	MX53_PAD_EIM_CS1_CSPI2_MOSI,	/* Nitrogen uses as WL1271_irq */
	MX53_PAD_EIM_OE_CSPI2_MISO,	/* Nitrogen uses as VGA Hsync */
	MX53_PAD_EIM_LBA_CSPI2_CS2,
	MX53_PAD_EIM_CS0_CSPI2_SCLK,

	MX53_PAD_GPIO_16__I2C3_SDA,	/* gpio7[11] */

	/* Nitrogen uses the following pin for UART3, CTS, TXD, RXD */
	NEW_PAD_CTRL(MX53_PAD_EIM_D23__GPIO_3_23, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),

	NEW_PAD_CTRL(MX53_PAD_EIM_D24__GPIO_3_24, PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP),
	NEW_PAD_CTRL(MX53_PAD_EIM_D25__GPIO_3_25, BUTTON_PAD_CTRL) | MUX_SION_MASK,	/* Menu key */

	/* UART3 */
	MX53_PAD_ATA_CS_0__UART3_TXD,
	MX53_PAD_ATA_CS_1__UART3_RXD,
	MX53_PAD_EIM_D30__UART3_CTS,
	MX53_PAD_EIM_D31__UART3_RTS,
};

static void __init mxc_board_init_nitrogen_a(void)
{
	unsigned da9052_irq = gpio_to_irq(MAKE_GP(2, 21));	/* pad EIM_A17 */
	camera_data.power_down = MAKE_GP(2, 22);
	if (gpio_request_array(nitrogen53_gpios_specific_a,
			ARRAY_SIZE(nitrogen53_gpios_specific_a))) {
		printk (KERN_ERR "%s gpio_request_array failed\n", __func__ );
	}
	mxc_iomux_v3_setup_multiple_pads(nitrogen53_pads_specific_a,
			ARRAY_SIZE(nitrogen53_pads_specific_a));
	mxc_board_init(NULL, 0,
		mxc_i2c1_board_info, ARRAY_SIZE(mxc_i2c1_board_info),
		mxc_i2c2_board_info, ARRAY_SIZE(mxc_i2c2_board_info),
		da9052_irq, &mxci2c2_data);
}

MACHINE_START(NITROGEN_A_IMX53, "Boundary Devices Nitrogen_A MX53 Board")
	/* Maintainer: Freescale Semiconductor, Inc. */
	.fixup = fixup_mxc_board,
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.timer = &mxc_timer,
	.init_machine = mxc_board_init_nitrogen_a,
MACHINE_END
#endif

/*****************************************************************************/

#ifdef CONFIG_MACH_NITROGEN_IMX53
struct gpio nitrogen53_gpios_specific[] __initdata = {
	{.label = "Camera power down",	.gpio = MAKE_GP(1, 2),		.flags = GPIOF_INIT_HIGH},
	{.label = "pmic-int",		.gpio = MAKE_GP(2, 21),		.flags = GPIOF_DIR_IN},
#if defined(CONFIG_WL12XX_SDIO)
	{.label = "wl1271_btwakeup",	.gpio = MAKE_GP(2, 22),		.flags = GPIOF_DIR_IN},		/* EIM_A16 */
	{.label = "wl1271_int",		.gpio = MAKE_GP(2, 24),		.flags = GPIOF_DIR_IN},		/* EIM_CS1 */
	{.label = "wl1271_wl_en",	.gpio = MAKE_GP(3, 0),		.flags = 0},			/* EIM_DA0, high active */
	{.label = "wl1271_bt_en",	.gpio = MAKE_GP(3, 1),		.flags = 0},			/* EIM_DA1, high active */
#endif
	{.label = "i2c-2-sda",		.gpio = MAKE_GP(7, 11),		.flags = GPIOF_DIR_IN},
};

static iomux_v3_cfg_t nitrogen53_pads_specific[] __initdata = {
	MX53_PAD_EIM_CS1__GPIO_2_24,	/* WL1271_irq, NitrogenA uses as ECSPI2 MOSI */
	MX53_PAD_EIM_OE__DI1_PIN7,	/* VGA HSync, NitrogenA uses as ECSPI2 MISO */
	MX53_PAD_EIM_CS0__GPIO_2_23,	/* Keyboard, NitrogenA uses as ECSPI2 SCLK */

	MX53_PAD_GPIO_16__I2C3_SDA,	/* gpio7[11] */
	MX53_PAD_EIM_D30__GPIO_3_30,	/* On/Off key */

	/* UART3 */
	MX53_PAD_EIM_D24__UART3_TXD,
	MX53_PAD_EIM_D25__UART3_RXD,
	MX53_PAD_EIM_D23__UART3_CTS,
	MX53_PAD_EIM_D31__UART3_RTS,
};

static void __init mxc_board_init_nitrogen(void)
{
	unsigned da9052_irq = gpio_to_irq(MAKE_GP(2, 21));	/* pad EIM_A17 */
	if (gpio_request_array(nitrogen53_gpios_specific,
			ARRAY_SIZE(nitrogen53_gpios_specific))) {
		printk (KERN_ERR "%s gpio_request_array failed\n", __func__ );
	}
	gpio_free(N53_WL1271_WL_EN);
	gpio_free(N53_WL1271_BT_EN);

	mxc_iomux_v3_setup_multiple_pads(nitrogen53_pads_specific,
			ARRAY_SIZE(nitrogen53_pads_specific));
	mxc_board_init(NULL, 0,
		mxc_i2c1_board_info, ARRAY_SIZE(mxc_i2c1_board_info),
		mxc_i2c2_board_info, ARRAY_SIZE(mxc_i2c2_board_info),
		da9052_irq, &mxci2c2_data);
}

MACHINE_START(NITROGEN_IMX53, "Boundary Devices Nitrogen MX53 Board")
	/* Maintainer: Freescale Semiconductor, Inc. */
	.fixup = fixup_mxc_board,
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.timer = &mxc_timer,
	.init_machine = mxc_board_init_nitrogen,
MACHINE_END
#endif

/*****************************************************************************/

#ifdef CONFIG_MACH_NITROGEN_V2_IMX53
static struct i2c_board_info mxc_i2c1_board_info_v2[] __initdata = {
#if defined (CONFIG_TOUCHSCREEN_I2C)
	{
	 .type = "Pic16F616-ts",
	 .addr = 0x22,
	 .platform_data  = &i2c_generic_data,
	},
#endif
	{
	 .type = "tfp410",
	 .addr = 0x38,
	 .platform_data  = &i2c_tfp410_data,
	},
};

static struct i2c_board_info mxc_i2c2_board_info_v2[] __initdata = {
	{
	 .type = "sgtl5000-i2c",
	 .addr = 0x0a,
	},
	{
	 .type = "ep0700m01-ts",
	 .addr = 0x38,
	 .platform_data  = &i2c_generic_data,
	},
};

static iomux_v3_cfg_t nitrogen53_pads_specific_v2[] __initdata = {
	MX53_PAD_GPIO_6__I2C3_SDA,	/* GPIO1[6] */
	MX53_PAD_GPIO_16__GPIO_7_11,
};

struct gpio nitrogen53_gpios_specific_v2[] __initdata = {
	{.label = "Camera power down",	.gpio = MAKE_GP(1, 2),		.flags = GPIOF_INIT_HIGH},
	{.label = "i2c-2-sda",		.gpio = MAKE_GP(1, 6),		.flags = GPIOF_DIR_IN},
	{.label = "pmic-int",		.gpio = MAKE_GP(7, 11),		.flags = GPIOF_DIR_IN},
};

static void __init mxc_board_init_nitrogen_v2(void)
{
	unsigned da9052_irq = gpio_to_irq(MAKE_GP(7, 11));	/* pad GPIO_16 */
	if (gpio_request_array(nitrogen53_gpios_specific_v2,
			ARRAY_SIZE(nitrogen53_gpios_specific_v2))) {
		printk (KERN_ERR "%s gpio_request_array failed\n", __func__ );
	}
	mxc_iomux_v3_setup_multiple_pads(nitrogen53_pads_specific_v2,
			ARRAY_SIZE(nitrogen53_pads_specific_v2));
	mxc_board_init(NULL, 0,
		mxc_i2c1_board_info, ARRAY_SIZE(mxc_i2c1_board_info_v2),
		mxc_i2c2_board_info, ARRAY_SIZE(mxc_i2c2_board_info_v2),
		da9052_irq, &mxci2c2_previous_data);
}

MACHINE_START(NITROGEN_V2_IMX53, "Boundary Devices Nitrogen MX53 rev. 2 Board (DA9053)")
	/* Maintainer: Freescale Semiconductor, Inc. */
	.fixup = fixup_mxc_board,
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.timer = &mxc_timer,
	.init_machine = mxc_board_init_nitrogen_v2,
MACHINE_END
#endif

/*****************************************************************************/

#ifdef CONFIG_MACH_NITROGEN_V1_IMX53
static struct i2c_board_info mxc_i2c0_board_info_v1[] __initdata = {
	{
	 .type = "Pic16F616-ts",
	 .addr = 0x22,
	 .platform_data  = &i2c_generic_data,
	},
	{
	 .type = "tfp410",
	 .addr = 0x38,
	 .platform_data  = &i2c_tfp410_data,
	},
};

static struct i2c_board_info mxc_i2c2_board_info_v1[] __initdata = {
	{
	 .type = "sgtl5000-i2c",
	 .addr = 0x0a,
	},
	{
	 .type = "ep0700m01-ts",
	 .addr = 0x38,
	 .platform_data  = &i2c_generic_data,
	},
};

static iomux_v3_cfg_t nitrogen53_pads_specific_v1[] __initdata = {
	MX53_PAD_GPIO_6__I2C3_SDA,	/* GPIO1[6] */
	MX53_PAD_GPIO_16__GPIO_7_11,
};

struct gpio nitrogen53_gpios_specific_v1[] __initdata = {
	{.label = "Camera power down",	.gpio = MAKE_GP(1, 2),		.flags = GPIOF_INIT_HIGH},
	{.label = "i2c-2-sda",		.gpio = MAKE_GP(1, 6),		.flags = GPIOF_DIR_IN},
	{.label = "pmic-int",		.gpio = MAKE_GP(7, 11),		.flags = GPIOF_DIR_IN},
};

static void __init mxc_board_init_nitrogen_v1(void)
{
	unsigned da9052_irq = gpio_to_irq(MAKE_GP(7, 11));	/* pad GPIO_16 */
	if (gpio_request_array(nitrogen53_gpios_specific_v1,
			ARRAY_SIZE(nitrogen53_gpios_specific_v1))) {
		printk (KERN_ERR "%s gpio_request_array failed\n", __func__ );
	}
	mxc_iomux_v3_setup_multiple_pads(nitrogen53_pads_specific_v1,
			ARRAY_SIZE(nitrogen53_pads_specific_v1));
	mxc_board_init(	mxc_i2c0_board_info_v1, ARRAY_SIZE(mxc_i2c0_board_info_v1),
		NULL, 0,
		mxc_i2c2_board_info_v1, ARRAY_SIZE(mxc_i2c2_board_info_v1),
		da9052_irq, &mxci2c2_previous_data);
}

MACHINE_START(NITROGEN_V1_IMX53, "Boundary Devices Nitrogen MX53 rev. 1 Board")
	/* Maintainer: Freescale Semiconductor, Inc. */
	.fixup = fixup_mxc_board,
	.map_io = mx5_map_io,
	.init_irq = mx5_init_irq,
	.timer = &mxc_timer,
	.init_machine = mxc_board_init_nitrogen_v1,
MACHINE_END
#endif
