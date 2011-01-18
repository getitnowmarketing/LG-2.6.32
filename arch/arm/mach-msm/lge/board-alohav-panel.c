/* arch/arm/mach-msm/board-alohav-panel.c
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

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <mach/msm_rpcrouter.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <mach/board.h>
#include <mach/board_lge.h>
#include "devices.h"
#include "board-alohav.h"

#define LCDC_CONFIG_PROC          21
#define LCDC_UN_CONFIG_PROC       22
#define LCDC_API_PROG             0x30000066
#define LCDC_API_VERS             0x00010001

#define GPIO_OUT_132    132
#define GPIO_OUT_131    131
#define GPIO_OUT_103    103
#define GPIO_OUT_102    102
#define GPIO_OUT_88     88

#define GPIO_OUT_84		84
#define GPIO_IN_85		85
#define GPIO_IN_102		102

static struct msm_rpc_endpoint *lcdc_ep;

static int msm_fb_lcdc_config(int on)
{
	int rc = 0;
	struct rpc_request_hdr hdr;

	if (on)
		pr_info("lcdc config\n");
	else
		pr_info("lcdc un-config\n");

	lcdc_ep = msm_rpc_connect_compatible(LCDC_API_PROG, LCDC_API_VERS, 0);
	if (IS_ERR(lcdc_ep)) {
		printk(KERN_ERR "%s: msm_rpc_connect failed! rc = %ld\n",
			__func__, PTR_ERR(lcdc_ep));
		return -EINVAL;
	}

	rc = msm_rpc_call(lcdc_ep,
				(on) ? LCDC_CONFIG_PROC : LCDC_UN_CONFIG_PROC,
				&hdr, sizeof(hdr),
				5 * HZ);
	if (rc)
		printk(KERN_ERR
			"%s: msm_rpc_call failed! rc = %d\n", __func__, rc);

	msm_rpc_close(lcdc_ep);
	return rc;
}

static int gpio_array_num[] = {
				GPIO_OUT_132, /* spi_clk */
				GPIO_OUT_131, /* spi_cs  */
				GPIO_OUT_103, /* spi_sdi */
				GPIO_OUT_102, /* spi_sdoi */
				GPIO_OUT_84
				};

static void __init lcdc_lgit_gpio_init(void)
{
	if (gpio_request(GPIO_OUT_132, "spi_clk"))
		pr_err("failed to request gpio spi_clk\n");
	if (gpio_request(GPIO_OUT_131, "spi_cs"))
		pr_err("failed to request gpio spi_cs\n");
	if (gpio_request(GPIO_OUT_103, "spi_sdi"))
		pr_err("failed to request gpio spi_sdi\n");
	if (gpio_request(GPIO_OUT_102, "spi_sdoi"))
		pr_err("failed to request gpio spi_sdoi\n");
	if (gpio_request(GPIO_OUT_84, "gpio_dac"))
		pr_err("failed to request gpio_dac\n");
	if(gpio_request(GPIO_IN_85,"lcd_maker_id"))
    	pr_err("failed to request lcd_maker_id\n");
}

static uint32_t lcdc_lgit_gpio_table[] = {
	GPIO_CFG(GPIO_OUT_132, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(GPIO_OUT_131, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(GPIO_OUT_103, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(GPIO_OUT_102, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),
	GPIO_CFG(GPIO_OUT_84,  0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
	GPIO_CFG(GPIO_IN_85, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA),
};

static void config_lcdc_gpio_table(uint32_t *table, int len, unsigned enable)
{
	int n, rc;
	for (n = 0; n < len; n++) {
		rc = gpio_tlmm_config(table[n],
			enable ? GPIO_ENABLE : GPIO_DISABLE);
		if (rc) {
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, table[n], rc);
			break;
		}
	}
}

static void lcdc_config_gpios(int enable)
{
	config_lcdc_gpio_table(lcdc_lgit_gpio_table,
		ARRAY_SIZE(lcdc_lgit_gpio_table), enable);
}

static char *msm_fb_lcdc_vreg[] = {
	"gp2",
	"gp5"
};

#define MSM_FB_LCDC_VREG_OP(name, op, level) \
do { \
	vreg = vreg_get(0, name); \
	vreg_set_level(vreg, level); \
	if (vreg_##op(vreg)) \
		printk(KERN_ERR "%s: %s vreg operation failed \n", \
			(vreg_##op == vreg_enable) ? "vreg_enable" \
				: "vreg_disable", name); \
} while (0)

static void msm_fb_lcdc_power_save(int on)
{
	struct vreg *vreg;
	int i;

	if (on) {
		MSM_FB_LCDC_VREG_OP(msm_fb_lcdc_vreg[0], enable, 1800);
		MSM_FB_LCDC_VREG_OP(msm_fb_lcdc_vreg[1], enable, 2800);
	} else{
		MSM_FB_LCDC_VREG_OP(msm_fb_lcdc_vreg[0], disable, 0);
		MSM_FB_LCDC_VREG_OP(msm_fb_lcdc_vreg[1], disable, 0);
	}
}

static struct lcdc_platform_data lcdc_pdata = {
	.lcdc_gpio_config = msm_fb_lcdc_config,
	.lcdc_power_save   = msm_fb_lcdc_power_save,
};

static struct msm_panel_lgit_pdata lcdc_lgit_panel_data = {
	.panel_config_gpio = lcdc_config_gpios,
	.gpio_num          = gpio_array_num,
	.gpio		       = GPIO_OUT_84,
	.initialized       = 1,
};

static struct platform_device lcdc_lgit_panel_device = {
	.name   = "lcdc_lgit_wvga",
	.id     = 0,
	.dev    = {
		.platform_data = &lcdc_lgit_panel_data,
	}
};

static struct msm_panel_hitachi_pdata lcdc_hitachi_panel_data = {
	.panel_config_gpio = lcdc_config_gpios,
	.gpio_num          = gpio_array_num,
	.gpio		       = GPIO_OUT_84,
};

static struct platform_device lcdc_hitachi_panel_device = {
	.name   = "lcdc_hitachi_wvga",
	.id     = 0,
	.dev    = {
		.platform_data = &lcdc_hitachi_panel_data,
	}
};

/* backlight device */
static struct gpio_i2c_pin bl_i2c_pin[] = {
	[0] = {
		.sda_pin	= 73,
		.scl_pin	= 74,
		.reset_pin	= 26,
		.irq_pin	= 0,
	},
};

static struct i2c_gpio_platform_data bl_i2c_pdata = {
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.udelay				= 2,
};

static struct platform_device bl_i2c_device = {
	.name	= "i2c-gpio",
	.dev.platform_data = &bl_i2c_pdata,
};

static struct backlight_platform_data bd6084gu_data = {
	.gpio = 26,
	.max_current = 0x51,		/* 16.4mA */
	.init_on_boot = 0,
};

static struct i2c_board_info bl_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("bd6084gu", 0xEC >> 1),
		.type = "bd6084gu",
		.platform_data = &bd6084gu_data,
	},
};

struct device* alohav_bd6084_dev(void)
{
	return &bl_i2c_device.dev;
}

void __init alohav_init_i2c_backlight(int bus_num)
{
	bl_i2c_device.id = bus_num;
	
	init_gpio_i2c_pin(&bl_i2c_pdata, bl_i2c_pin[0],	&bl_i2c_bdinfo[0]);
	i2c_register_board_info(bus_num, &bl_i2c_bdinfo[0], 1);
	platform_device_register(&bl_i2c_device);
}

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = 97,
};

static void __init msm_fb_add_devices(void)
{
	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("pmdh", 0);
	msm_fb_register_device("lcdc", &lcdc_pdata);
}

int lge_detect_panel(const char *name)
{
	int maker_id;

	maker_id = gpio_get_value(GPIO_IN_85);
	printk(KERN_INFO"%s: marker id = %d.\n", __func__, maker_id);

	if(!strcmp(name, "lcdc_lgit_wvga") && maker_id)
		return 0;
	else if(!strcmp(name, "lcdc_hitachi_wvga") && !maker_id)
		return 0;

	return -ENODEV;
}

void __init lge_add_lcd_devices(void)
{
	int maker_id;
	maker_id = gpio_get_value(GPIO_IN_85);

	if(maker_id)
		platform_device_register(&lcdc_lgit_panel_device);
	else
		platform_device_register(&lcdc_hitachi_panel_device);

	lcdc_lgit_gpio_init();
	msm_fb_add_devices();

	lge_add_gpio_i2c_device(alohav_init_i2c_backlight);
}
