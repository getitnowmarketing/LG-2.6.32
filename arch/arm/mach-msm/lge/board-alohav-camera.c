/* arch/arm/mach-msm/board-alohav-camera.c
 * Copyright (C) 2009 LGE, Inc.
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
#include <linux/types.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <asm/setup.h>
#include <mach/gpio.h>
#include <mach/board.h>
#include <mach/camera.h>
#include <mach/msm_iomap.h>
#include <mach/vreg.h>
#include <mach/board_lge.h>

#include "board-alohav.h"

/* LGE_CHANGES_S [cis@lge.com] 2010-02-17, [VS740] for sleep current */
#if defined (CONFIG_MACH_MSM7X27_ALOHAV)
#include <mach/vreg.h>

#define MSM_VREG_OP(name, op, level) \
	do { \
		vreg = vreg_get(0, name); \
		vreg_set_level(vreg, level); \
		if (vreg_##op(vreg)) \
			printk(KERN_ERR "%s: %s vreg operation failed \n", \
				(vreg_##op == vreg_enable) ? "vreg_enable" \
					: "vreg_disable", name); \
	} while (0)
#endif
/* LGE_CHANGES_E [cis@lge.com] 2010-02-17, [VS740] for sleep current */

struct i2c_board_info i2c_devices[1] = {
#if defined (CONFIG_MT9T111)
	{
		I2C_BOARD_INFO("mt9t111", CAM_I2C_SLAVE_ADDR >> 1),
	},
#endif
};

#if defined (CONFIG_MSM_CAMERA)
static uint32_t camera_off_gpio_table[] = {
	/* CAMERA reset, pwndown */
	GPIO_CFG(0,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT0 */
	GPIO_CFG(1,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT1 */
	/* parallel CAMERA interfaces */
	GPIO_CFG(4,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT0 */
	GPIO_CFG(5,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT1 */
	GPIO_CFG(6,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT2 */
	GPIO_CFG(7,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT3 */
	GPIO_CFG(8,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT4 */
	GPIO_CFG(9,  0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT5 */
	GPIO_CFG(10, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT6 */
	GPIO_CFG(11, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT7 */
	GPIO_CFG(12, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* PCLK */
	GPIO_CFG(13, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* HSYNC_IN */
	GPIO_CFG(14, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* VSYNC_IN */
	GPIO_CFG(15, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* MCLK */
	GPIO_CFG(MOVIE_MODE_EN, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* MOVIE_MODE_EN */
	GPIO_CFG(FLASH_INHIBIT, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* FLASH_INHIBIT */
	GPIO_CFG(FLASH_EN, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* FLASH_EN */
	/* CAMERA hw i2c */
	GPIO_CFG(60, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* I2C_SCL */
	GPIO_CFG(61, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* I2C_SDA */
};

static uint32_t camera_on_gpio_table[] = {
	/* parallel CAMERA interfaces */
	GPIO_CFG(4,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT0 */
	GPIO_CFG(5,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT1 */
	GPIO_CFG(6,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT2 */
	GPIO_CFG(7,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT3 */
	GPIO_CFG(8,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT4 */
	GPIO_CFG(9,  1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT5 */
	GPIO_CFG(10, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT6 */
	GPIO_CFG(11, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT7 */
	GPIO_CFG(12, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_16MA), /* PCLK */
	GPIO_CFG(13, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* HSYNC_IN */
	GPIO_CFG(14, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* VSYNC_IN */
	GPIO_CFG(15, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* MCLK */
	GPIO_CFG(MOVIE_MODE_EN, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA), /* MOVIE_MODE_EN */
	GPIO_CFG(FLASH_INHIBIT, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA), /* FLASH_INHIBIT */
	GPIO_CFG(FLASH_EN, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA), /* FLASH_EN */
};

static void config_gpio_table(uint32_t *table, int len)
{
	int n, rc;
	for (n = 0; n < len; n++) {
		rc = gpio_tlmm_config(table[n], GPIO_ENABLE);
		if (rc) {
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, table[n], rc);
			break;
		}
	}
}

void config_camera_on_gpios(void)
{
	config_gpio_table(camera_on_gpio_table,
		ARRAY_SIZE(camera_on_gpio_table));
}

void config_camera_off_gpios(void)
{
	config_gpio_table(camera_off_gpio_table,
		ARRAY_SIZE(camera_off_gpio_table));
}

int camera_power_on (void)
{
	struct vreg *vreg_wlan;
	struct vreg *vreg_rftx;
	struct device *dev = alohav_bd6084_dev();

	if(bd6084gu_ldo_set_level(dev, LDO_NO_CAM_DVDD, 1800))
		printk(KERN_ERR "%s: ldo %d control error\n", __func__, LDO_NO_CAM_DVDD);
	if(bd6084gu_ldo_enable(dev, LDO_NO_CAM_DVDD, 1))
		printk(KERN_ERR "%s: ldo %d control error\n", __func__, LDO_NO_CAM_DVDD);

	vreg_rftx = vreg_get(0, "rftx");
	vreg_set_level(vreg_rftx, 2800);
	vreg_enable(vreg_rftx);

	vreg_wlan = vreg_get(0, "wlan");
	vreg_set_level(vreg_wlan, 2600);
	vreg_enable(vreg_wlan);

	if(bd6084gu_ldo_set_level(dev, LDO_NO_CAM_LVDD, 2800))
		printk(KERN_ERR "%s: ldo %d control error\n", __func__, LDO_NO_CAM_LVDD);
	if(bd6084gu_ldo_enable(dev, LDO_NO_CAM_LVDD, 1))
		printk(KERN_ERR "%s: ldo %d control error\n", __func__, LDO_NO_CAM_LVDD);

	mdelay(50);

	if (bd6084gu_ldo_set_level(dev, LDO_NO_CAM_CLKVDD, 2600))
		printk(KERN_ERR "%s: ldo %d control error\n", __func__, LDO_NO_CAM_CLKVDD);
	if (bd6084gu_ldo_enable(dev, LDO_NO_CAM_CLKVDD, 1))
		printk(KERN_ERR "%s: ldo %d control error\n", __func__, LDO_NO_CAM_CLKVDD);
	
	mdelay(5);

	return 0;
}

int camera_power_off (void)
{
	struct vreg *vreg_wlan;
	struct vreg *vreg_rftx;
	struct device *dev = alohav_bd6084_dev();

	if (bd6084gu_ldo_set_level(dev, LDO_NO_CAM_CLKVDD, 2600))
		printk(KERN_ERR "%s: ldo %d control error\n", __func__, LDO_NO_CAM_CLKVDD);
	if (bd6084gu_ldo_enable(dev, LDO_NO_CAM_CLKVDD, 0))
		printk(KERN_ERR "%s: ldo %d control error\n", __func__, LDO_NO_CAM_CLKVDD);

	mdelay(50);

	if(bd6084gu_ldo_set_level(dev, LDO_NO_CAM_LVDD, 2800))
		printk(KERN_ERR "%s: ldo %d control error\n", __func__, LDO_NO_CAM_LVDD);
	if(bd6084gu_ldo_enable(dev, LDO_NO_CAM_LVDD, 0))
		printk(KERN_ERR "%s: ldo %d control error\n", __func__, LDO_NO_CAM_LVDD);

	if(bd6084gu_ldo_set_level(dev, LDO_NO_CAM_DVDD, 1800))
		printk(KERN_ERR "%s: ldo %d control error\n", __func__, LDO_NO_CAM_LVDD);
	if(bd6084gu_ldo_enable(dev, LDO_NO_CAM_DVDD, 0))
		printk(KERN_ERR "%s: ldo %d control error\n", __func__, LDO_NO_CAM_LVDD);

	vreg_rftx = vreg_get(0, "rftx");
	vreg_set_level(vreg_rftx, 0);
	vreg_disable(vreg_rftx);

	vreg_wlan = vreg_get(0, "wlan");
	vreg_set_level(vreg_wlan, 0);
	vreg_disable(vreg_wlan);

	return 0;
}

static struct msm_camera_device_platform_data msm_camera_device_data = {
	.camera_gpio_on  = config_camera_on_gpios,
	.camera_gpio_off = config_camera_off_gpios,
	.ioext.mdcphy = MSM_MDC_PHYS,
	.ioext.mdcsz  = MSM_MDC_SIZE,
	.ioext.appphy = MSM_CLK_CTL_PHYS,
	.ioext.appsz  = MSM_CLK_CTL_SIZE,
	.camera_power_on = camera_power_on,
	.camera_power_off = camera_power_off,
};

#if defined (CONFIG_MT9T111)
static struct msm_camera_sensor_flash_data flash_none = {
	.flash_type = MSM_CAMERA_FLASH_LED,
};

static struct msm_camera_sensor_info msm_camera_sensor_mt9t111_data = {
	.sensor_name    = "mt9t111",
	.sensor_reset   = 0,
	.sensor_pwd     = 1,
	.vcm_pwd        = 0,
	.vcm_enable		= 0,
	.pdata          = &msm_camera_device_data,
	.flash_data		= &flash_none,
};

static struct platform_device msm_camera_sensor_mt9t111 = {
	.name      = "msm_camera_mt9t111",
	.dev       = {
		.platform_data = &msm_camera_sensor_mt9t111_data,
	},
};
#endif

#if defined (CONFIG_AAT1270_FLASH)
struct aat1270_flash_platform_data aat1270_flash_pdata = {
	.gpio_flen		= FLASH_EN,
	.gpio_en_set	= MOVIE_MODE_EN,
	.gpio_inh		= FLASH_INHIBIT,
};

static struct platform_device aat1270_flash_device = {
	.name				= "aat1270_flash",
	.id					= -1,
	.dev.platform_data	= &aat1270_flash_pdata,
};
#endif

#endif

static struct platform_device *alohav_camera_devices[] __initdata = {
#if defined (CONFIG_MT9T111)
	&msm_camera_sensor_mt9t111,
#endif
#if defined (CONFIG_AAT1270_FLASH)
	&aat1270_flash_device,
#endif

};

void __init lge_add_camera_devices(void)
{
/* LGE_CHANGES_S [cis@lge.com] 2010-02-17, [VS740] for sleep current */
#if defined (CONFIG_MACH_MSM7X27_ALOHAV)
	{
		struct vreg *vreg;
		MSM_VREG_OP("wlan", enable, 2600);  // cam_iovdd, rolled back to the original
		mdelay(10);
		MSM_VREG_OP("wlan", disable, 2600);  // cam_iovdd, rolled back to the original
	}
#endif
/* LGE_CHANGES_E [cis@lge.com] 2010-02-17, [VS740] for sleep current */

    platform_add_devices(alohav_camera_devices, ARRAY_SIZE(alohav_camera_devices));
}
