/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora Forum nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * Alternatively, provided that this notice is retained in full, this software
 * may be relicensed by the recipient under the terms of the GNU General Public
 * License version 2 ("GPL") and only version 2, in which case the provisions of
 * the GPL apply INSTEAD OF those given above.  If the recipient relicenses the
 * software under the GPL, then the identification text in the MODULE_LICENSE
 * macro must be changed to reflect "GPLv2" instead of "Dual BSD/GPL".  Once a
 * recipient changes the license terms to the GPL, subsequent recipients shall
 * not relicense under alternate licensing terms, including the BSD or dual
 * BSD/GPL terms.  In addition, the following license statement immediately
 * below and between the words START and END shall also then apply when this
 * software is relicensed under the GPL:
 *
 * START
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 and only version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * END
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * Reference : lcdc_gordon.c
 * Author : M.C.Kang (knight0708@lge.com)
 *
 */

#include <linux/delay.h>
#include <mach/gpio.h>
#include "msm_fb.h"

#include <linux/timer.h>
#include <mach/board_lge.h>

#define MODULE_NAME             "lcdc_lgit_wvga"

#define dprintk(fmt, args...)	printk(KERN_INFO "%s:%s: " fmt, MODULE_NAME, __func__, ## args)
#define lgit_writew(reg, val)   serigo(reg, val)

static int spi_cs;
static int spi_sclk;
static int spi_sdi;
static int spi_sdo;

struct lgit_state_type{
	boolean disp_initialized;
	boolean display_on;
	boolean disp_powered_up;
};

static struct lgit_state_type lgit_state = { 0 };
static struct msm_panel_lgit_pdata *lcdc_lgit_pdata;

/* Tunning data */
static unsigned int h_back_porch = 0x39;

static void lcdc_spi_pin_assign(void)
{
	spi_sclk = *(lcdc_lgit_pdata->gpio_num);
	spi_cs   = *(lcdc_lgit_pdata->gpio_num + 1);
	spi_sdi  = *(lcdc_lgit_pdata->gpio_num + 2);
	spi_sdo  = *(lcdc_lgit_pdata->gpio_num + 3);
}

static void lgit_spi_init(void)
{
	/* Set the output so that we don't disturb the slave device */
	gpio_set_value(spi_sclk, 0);
	gpio_set_value(spi_sdi, 0);

	/* Set the Chip Select deasserted (active low) */
	gpio_set_value(spi_cs, 1);
}

static void lgit_spi_write_word(u16 val)
{
	int i;

	for (i = 0; i < 16; i++) {
		/* #1: Drive the Clk Low */
		gpio_set_value(spi_sclk, 0);

		/* #2: Drive the Data (High or Low) */
		if (val & 0x8000)
			gpio_set_value(spi_sdi, 1);
		else
			gpio_set_value(spi_sdi, 0);

		/* #3: Drive the Clk High */
		gpio_set_value(spi_sclk, 1);

		/* #4: Next bit */
		val <<= 1;
	}
}

static void lgit_spi_write_byte(u8 val)
{
	int i;

	/* Clock should be Low before entering */
	for (i = 0; i < 8; i++) {
		/* #1: Drive the Clk Low */
		gpio_set_value(spi_sclk, 0);

		/* #2: Drive the Data (High or Low) */
		if (val & 0x80)
			gpio_set_value(spi_sdi, 1);
		else
			gpio_set_value(spi_sdi, 0);

		/* #3: Drive the Clk High */
		gpio_set_value(spi_sclk, 1);

		/* #4: Next bit */
		val <<= 1;
	}
}

static void serigo(u16 reg, u16 data)
{
	/* Transmit register address first, then data */
	if(reg != 0xFFFF) {
		gpio_set_value(spi_cs, 0);
		udelay(1);

		/* Set index register */
		lgit_spi_write_byte(0x7C);
		lgit_spi_write_word(reg);

		gpio_set_value(spi_cs, 1);
		udelay(1);
	}
	gpio_set_value(spi_cs, 0);
	gpio_set_value(spi_sdi, 0);
	udelay(1);

	/* Transmit data */
	lgit_spi_write_byte(0x7E);
	lgit_spi_write_word(data);

	gpio_set_value(spi_sdi, 0);
	gpio_set_value(spi_cs, 1);
}

static void lgit_init(void)
{
	lgit_writew(0x0020, 0x0001);	//RGB Interface Select
	lgit_writew(0x003A, 0x0060);	//Interface Pixel Format		60 --> 18bit 70--> 24bit

	lgit_writew(0x00B1, 0x0016);	//RGB Interface Setting
	lgit_writew(0xFFFF, h_back_porch);	//H back Porch
	lgit_writew(0xFFFF, 0x0008);	//V back Porch

	lgit_writew(0x00B2, 0x0000);	//Panel Characteristics Setting
	lgit_writew(0xFFFF, 0x00C8);

	lgit_writew(0x00B3, 0x0011);	//Entry Mode Setting
	lgit_writew(0xFFFF, 0x00FF);

	lgit_writew(0x00B4, 0x0010);	//Display Mode Control	//0.2 : 0 --> 0.4 : 0x10

	lgit_writew(0x00B6, h_back_porch-1);	//Display Control 2, SDT
	lgit_writew(0xFFFF, 0x0018);
	lgit_writew(0xFFFF, 0x003A);
	lgit_writew(0xFFFF, 0x0040);
	lgit_writew(0xFFFF, 0x0010);

	lgit_writew(0x00B7, 0x0000);	//Display Control 3
	lgit_writew(0xFFFF, 0x0010);
	lgit_writew(0xFFFF, 0x0001);

	lgit_writew(0x00C3, 0x0001);	//Power Control 3	//0x0005
	lgit_writew(0xFFFF, 0x0005);    //Step up freq
	lgit_writew(0xFFFF, 0x0000);
	lgit_writew(0xFFFF, 0x0007);
	lgit_writew(0xFFFF, 0x0001);

	lgit_writew(0x00C4, 0x0033);	//Power Control 4
	lgit_writew(0xFFFF, 0x0003);
	lgit_writew(0xFFFF, 0x0000);
	lgit_writew(0xFFFF, 0x001B);
	lgit_writew(0xFFFF, 0x001B);
	lgit_writew(0xFFFF, 0x0000);
	lgit_writew(0xFFFF, 0x0000);
	lgit_writew(0xFFFF, 0x0004);

	lgit_writew(0x00C5, 0x006B);	//Power Control 5
	lgit_writew(0xFFFF, 0x0007);

	lgit_writew(0x00C6, 0x0023);	//Power Control 6 //0x0023
	lgit_writew(0xFFFF, 0x0000);
	lgit_writew(0xFFFF, 0x0000);

	lgit_writew(0x00C8, 0x0001);	//Backlight Control
	lgit_writew(0xFFFF, 0x0001);
	lgit_writew(0xFFFF, 0x0003);
	lgit_writew(0xFFFF, 0x00FF);
	msleep(70);

	lgit_writew(0x00D0, 0x0000);	//Set Positive Gamma R.G
	lgit_writew(0xFFFF, 0x0025);
	lgit_writew(0xFFFF, 0x0072);
	lgit_writew(0xFFFF, 0x0005);
	lgit_writew(0xFFFF, 0x0008);
	lgit_writew(0xFFFF, 0x0007);
	lgit_writew(0xFFFF, 0x0063);
	lgit_writew(0xFFFF, 0x0024);
	lgit_writew(0xFFFF, 0x0003);

	lgit_writew(0x00D1, 0x0040);	//Set Negative Gamma R.G
	lgit_writew(0xFFFF, 0x0015);
	lgit_writew(0xFFFF, 0x0074);
	lgit_writew(0xFFFF, 0x0027);
	lgit_writew(0xFFFF, 0x001C);
	lgit_writew(0xFFFF, 0x0000);
	lgit_writew(0xFFFF, 0x0055);
	lgit_writew(0xFFFF, 0x0043);
	lgit_writew(0xFFFF, 0x0004);

	lgit_writew(0x00D2, 0x0000);	//Set Positive Gamma Blue
	lgit_writew(0xFFFF, 0x0025);
	lgit_writew(0xFFFF, 0x0072);
	lgit_writew(0xFFFF, 0x0005);
	lgit_writew(0xFFFF, 0x0008);
	lgit_writew(0xFFFF, 0x0007);
	lgit_writew(0xFFFF, 0x0063);
	lgit_writew(0xFFFF, 0x0024);
	lgit_writew(0xFFFF, 0x0003);

	lgit_writew(0x00D3, 0x0040);	//Set Positive Gamma Blue
	lgit_writew(0xFFFF, 0x0015);
	lgit_writew(0xFFFF, 0x0074);
	lgit_writew(0xFFFF, 0x0027);
	lgit_writew(0xFFFF, 0x001C);
	lgit_writew(0xFFFF, 0x0000);
	lgit_writew(0xFFFF, 0x0055);
	lgit_writew(0xFFFF, 0x0043);
	lgit_writew(0xFFFF, 0x0004);

	lgit_writew(0x00C2, 0x0008);	//Power Control 2
	msleep(20);
	lgit_writew(0x00C2, 0x0018);	//Power Control 2
	msleep(20);
	lgit_writew(0x00C2, 0x00B8);	//Power Control 2
	msleep(20);
	lgit_writew(0x00B5, 0x0001);	//Display Control 1
	msleep(20);

	lgit_writew(0x0029, 0x0001);	//Display ON
}

static void lgit_sleep(void)
{
	lgit_writew(0x0028, 0x0000);	//Display off
	msleep(20);
	lgit_writew(0x00B5, 0x0000);	//Display control 1
	msleep(20);
	lgit_writew(0x00C2, 0x0018);	//Power Control2
	msleep(20);
	lgit_writew(0x00C2, 0x0000);	//Power Control2
	msleep(20);

	lgit_writew(0x00C4, 0x0000);	//Power Control4
	lgit_writew(0xffff, 0x0000);
	lgit_writew(0xffff, 0x0000);
	lgit_writew(0xffff, 0x001B);
	lgit_writew(0xffff, 0x001B);
	lgit_writew(0xffff, 0x0000);
	lgit_writew(0xffff, 0x0000);
	lgit_writew(0xffff, 0x0004);
	msleep(20);
}

static void lgit_disp_powerup(void)
{
	if (!lgit_state.disp_powered_up && !lgit_state.display_on) {
	      lgit_state.disp_powered_up = TRUE;
	}
}

static void lgit_disp_on(void)
{
	dprintk("lgit disp on\n");
	if (lgit_state.disp_powered_up && !lgit_state.display_on) {
		dprintk("lgit disp on initialize data go out\n");
		lgit_init();
		lgit_state.display_on = TRUE;
	}
}

static void lcdc_lgit_panel_reset(void)
{
	if(lcdc_lgit_pdata && lcdc_lgit_pdata->gpio) {
		gpio_set_value(lcdc_lgit_pdata->gpio, 0);
		msleep(10);
		gpio_set_value(lcdc_lgit_pdata->gpio, 1);
		msleep(10);
	}
}

static int lcdc_lgit_panel_on(struct platform_device *pdev)
{
	dprintk("lgit panel on\n");
	if (!lgit_state.disp_initialized) {
		if(lcdc_lgit_pdata->panel_config_gpio)
			lcdc_lgit_pdata->panel_config_gpio(1);
		lgit_spi_init();
		lgit_disp_powerup();
		if(system_state == 	SYSTEM_BOOTING &&
		   lcdc_lgit_pdata->initialized) {
			lgit_state.display_on = TRUE;
		} else {
			lcdc_lgit_panel_reset();
			lgit_disp_on();
		}
		lgit_state.disp_initialized = TRUE;
	}
	return 0;
}

static int lcdc_lgit_panel_off(struct platform_device *pdev)
{
	dprintk("lgit disp off\n");
	if (lgit_state.disp_powered_up && lgit_state.display_on) {
		lgit_sleep();
		if(lcdc_lgit_pdata->panel_config_gpio)
			lcdc_lgit_pdata->panel_config_gpio(0);
		lgit_state.display_on = FALSE;
		lgit_state.disp_initialized = FALSE;
	}
	return 0;
}

static ssize_t enable_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int enable;

	sscanf(buf, "%d", &enable);
	if(enable)
		lcdc_lgit_panel_on(dev->platform_data);
	else
		lcdc_lgit_panel_off(dev->platform_data);

	return count;
}

static ssize_t enable_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", lgit_state.display_on);		
}
static DEVICE_ATTR(enable,  0666, enable_show,  enable_store);  

static int __init lgit_probe(struct platform_device *pdev)
{
	int status=0;
	
	if (pdev->id == 0) {
		lcdc_lgit_pdata = pdev->dev.platform_data;
		lcdc_spi_pin_assign();
		dprintk("lgit probe (id == 0)\n");

		return 0;
	}
	msm_fb_add_device(pdev);
	dprintk("lgit probe (id not 0)\n");

	/* debugging point */
	status = device_create_file(&(pdev->dev), &dev_attr_enable);

	return status;
}

static struct platform_driver this_driver = {
	.probe  = lgit_probe,
	.driver = {
		.name   = MODULE_NAME,
	},
};

static struct msm_fb_panel_data lgit_panel_data = {
	.on = lcdc_lgit_panel_on,
	.off = lcdc_lgit_panel_off,
};

static struct platform_device this_device = {
	.name   = MODULE_NAME,
	.id	= 1,
	.dev	= {
		.platform_data = &lgit_panel_data,
	}
};

static int __init lcdc_lgit_panel_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	dprintk("LCDC lgit init\n");
	
#ifdef CONFIG_FB_MSM_LCDC_LGIT_HITACHI_WVGA
	if (msm_fb_detect_client(MODULE_NAME))
		return 0;
#endif
	ret = platform_driver_register(&this_driver);
	if (ret)
		return ret;

	pinfo = &lgit_panel_data.panel_info;
	pinfo->xres = 480;
	pinfo->yres = 800;
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 18;
	pinfo->fb_num = 2;
	pinfo->clk_rate = 24576000;

	pinfo->lcdc.h_back_porch = h_back_porch;
	pinfo->lcdc.h_front_porch = 0x17;
	pinfo->lcdc.h_pulse_width = 0x01;
	pinfo->lcdc.v_back_porch = 0x08;
	pinfo->lcdc.v_front_porch = 0x07;
	pinfo->lcdc.v_pulse_width = 0x02;
	pinfo->lcdc.border_clr = 0;     /* blk */
	pinfo->lcdc.underflow_clr = 0xff;       /* blue */
	pinfo->lcdc.hsync_skew = 0;
	
	ret = platform_device_register(&this_device);
	if (ret)
		platform_driver_unregister(&this_driver);

	return ret;
}

module_init(lcdc_lgit_panel_init);
