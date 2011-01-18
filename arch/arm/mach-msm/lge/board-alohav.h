/* arch/arm/mach-msm/include/mach/board_alohav.h
 * Copyright (C) 2009 LGE, Inc.
 * Author: SungEun Kim <cleaneye@lge.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __ARCH_MSM_BOARD_ALOHAV_H
#define __ARCH_MSM_BOARD_ALOHAV_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <asm/setup.h>
#include "pm.h"

/* sdcard related macros */
#ifdef CONFIG_MMC_MSM_CARD_HW_DETECTION
#define GPIO_SD_DETECT_N    94
#define VREG_SD_LEVEL       3000

#define GPIO_SD_DATA_3      51
#define GPIO_SD_DATA_2      52
#define GPIO_SD_DATA_1      53
#define GPIO_SD_DATA_0      54
#define GPIO_SD_CMD         55
#define GPIO_SD_CLK         56
#endif

/* touch-screen macros */
#define TS_X_MIN			0
#define TS_X_MAX			480
#define TS_Y_MIN			0
#define TS_Y_MAX			800
#define TS_GPIO_I2C_SDA		23
#define TS_GPIO_I2C_SCL		16
#define TS_GPIO_IRQ			17
#define TS_I2C_SLAVE_ADDR	0x20

/* camera */
#define LDO_NO_CAM_CLKVDD	2
#define LDO_NO_CAM_LVDD		3
#define LDO_NO_CAM_DVDD		4
#define CAM_I2C_SLAVE_ADDR	0x78

/* hall ic macros */
#define GPIO_HALLIC_IRQ		86
#define PROHIBIT_TIME		1000	/* default 1 sec */

/* proximity sensor */
#define PROXI_GPIO_I2C_SCL	77
#define PROXI_GPIO_I2C_SDA 	78
#define PROXI_GPIO_DOUT		79
#define PROXI_I2C_ADDRESS	0x44 /*slave address 7bit*/
#define PROXI_LDO_NO_VCC	1

/* accelerometer */
#define ACCEL_GPIO_I2C_SCL  	47
#define ACCEL_GPIO_I2C_SDA  	48
#define ACCEL_GPIO_CPAS_TRG 	49
#define ACCEL_GPIO_CPAS_BUSY 	87
#define ACCEL_GPIO_CPAS_RST 	50
#define ACCEL_I2C_ADDRESS		0x18 /*slave address 7bit*/

/*Ecompass*/
#define ECOM_I2C_ADDRESS                0x1c /* slave address 7bit */


/* gyro(ami602) sensor*/
#define GYRO_I2C_ADDRESS        0x30

/* msm pmic leds */
#define EL_EN_GPIO			96

/* aat1270 flash */
#define FLASH_EN		109
#define MOVIE_MODE_EN	110
#define FLASH_INHIBIT	38

/* bluetooth gpio pin */
/* LGE_CHANGE_S [kiwone.seo@lge.com] 2010-02-03, LG_FW_AUDIO_BROADCOM_BT */
#if defined (CONFIG_MACH_MSM7X27_ALOHAV_REVA)
enum {
	BT_WAKE         = 109,
	BT_RFR          = 43,
	BT_CTS          = 44,
	BT_RX           = 45,
	BT_TX           = 46,
	BT_PCM_DOUT     = 68,
	BT_PCM_DIN      = 69,
	BT_PCM_SYNC     = 70,
	BT_PCM_CLK      = 71,
	BT_HOST_WAKE    = 27,
	BT_RESET_N			= 82,
};
#else
enum {
	BT_WAKE         = 81,
	BT_RFR          = 19,
	BT_CTS          = 20,
	BT_RX           = 21,
	BT_TX           = 108,
	BT_PCM_DOUT     = 68,
	BT_PCM_DIN      = 69,
	BT_PCM_SYNC     = 70,
	BT_PCM_CLK      = 71,
	BT_HOST_WAKE    = 27,
	BT_RESET_N			= 82,
};
#endif
/* LGE_CHANGE_E [kiwone.seo@lge.com] 2010-02-03, LG_FW_AUDIO_BROADCOM_BT */

/* pp2106 qwerty keypad macros */
#define KEY_SPEAKERMODE 241 // KEY_VIDEO_NEXT is not used in GED
#define KEY_CAMERAFOCUS 242 // KEY_VIDEO_PREV is not used in GED
#define KEY_FOLDER_HOME 243
#define KEY_FOLDER_MENU 244

#define PP2106_KEYPAD_ROW	8
#define PP2106_KEYPAD_COL	8

#define GPIO_PP2106_RESET	33
#define GPIO_PP2106_IRQ		36
#define GPIO_PP2106_SDA		34
#define GPIO_PP2106_SCL		35

/* interface variable */
extern struct platform_device msm_device_snd;
extern struct platform_device msm_device_adspdec;
extern struct i2c_board_info i2c_devices[1];

/* interface functions */
void config_camera_on_gpios(void);
void config_camera_off_gpios(void);
struct device* alohav_bd6084_dev(void);
#endif
