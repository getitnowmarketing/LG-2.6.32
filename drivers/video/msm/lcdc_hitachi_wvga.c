/* Copyright (c) 2010, LGE. All rights reserved.
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
 * Author : Munyoung Hwang <munyoung.hwang@lge.com>
 *
 */

#include <linux/delay.h>
#include <mach/gpio.h>
#include "msm_fb.h"

#include <linux/timer.h>
#include <mach/board_lge.h>

#define MODULE_NAME             "lcdc_hitachi_wvga"

#define dprintk(fmt, args...)	printk(KERN_INFO "%s:%s: " fmt, MODULE_NAME, __func__, ## args)

static int spi_cs;
static int spi_sclk;
static int spi_sdi;
static int spi_sdo;

struct hitachi_state_type{
	boolean disp_initialized;
	boolean display_on;
	boolean disp_powered_up;
};

struct hitachi_tuning_data {
	u8 threw;
	u8 ulmtw;
	u8 llmtw;
};

static struct hitachi_state_type hitachi_state = { 0 };
static struct msm_panel_hitachi_pdata *lcdc_hitachi_pdata;
static struct hitachi_tuning_data hitachi_tuning;

static unsigned int h_back_porch = 30;

static void lcdc_spi_pin_assign(void)
{
	spi_sclk = *(lcdc_hitachi_pdata->gpio_num);
	spi_cs   = *(lcdc_hitachi_pdata->gpio_num + 1);
	spi_sdi  = *(lcdc_hitachi_pdata->gpio_num + 2);
	spi_sdo  = *(lcdc_hitachi_pdata->gpio_num + 3);
}

static void hitachi_spi_init(void)
{
	/* Set the output so that we don't disturb the slave device */
	gpio_set_value(spi_sclk, 1);
	gpio_set_value(spi_sdi, 0);
	gpio_set_value(spi_sdo, 0);

	/* Set the Chip Select deasserted (active low) */
	gpio_set_value(spi_cs, 1);
}

static void hitachi_spi_write_cmd(u8 val)
{
	int i;

	/* set DCX to 0 */
	gpio_set_value(spi_sclk, 0);
	gpio_set_value(spi_sdi, 0);
	udelay(1);
	gpio_set_value(spi_sclk, 1);
	udelay(1);

	/* Clock should be Low before entering */
	for (i = 0; i < 8; i++) {
		/* #1: Drive the Clk Low */
		gpio_set_value(spi_sclk, 0);

		/* #2: Drive the Data (High or Low) */
		if (val & 0x80)
			gpio_set_value(spi_sdi, 1);
		else
			gpio_set_value(spi_sdi, 0);

		udelay(1);
		/* #3: Drive the Clk High */
		gpio_set_value(spi_sclk, 1);
		udelay(1);

		/* #4: Next bit */
		val <<= 1;
	}
}

static void hitachi_spi_write_data(u8 val)
{
	int i;

	/* set DCX to 1 */
	gpio_set_value(spi_sclk, 0);
	gpio_set_value(spi_sdi, 1);
	udelay(1);
	gpio_set_value(spi_sclk, 1);
	udelay(1);

	/* Clock should be Low before entering */
	for (i = 0; i < 8; i++) {
		/* #1: Drive the Clk Low */
		gpio_set_value(spi_sclk, 0);

		/* #2: Drive the Data (High or Low) */
		if (val & 0x80)
			gpio_set_value(spi_sdi, 1);
		else
			gpio_set_value(spi_sdi, 0);

		udelay(1);
		/* #3: Drive the Clk High */
		gpio_set_value(spi_sclk, 1);
		udelay(1);

		/* #4: Next bit */
		val <<= 1;
	}
}

static void serigo(u8 reg, u8 data)
{
	/* Transmit command first, then data */
	if(reg != 0xFF) {
		gpio_set_value(spi_cs, 0);
		udelay(1);
		hitachi_spi_write_cmd(reg);

	}
	/* Transmit data */
	hitachi_spi_write_data(data);

	gpio_set_value(spi_cs, 1);
}

static void serigo2(u8 reg)
{
	/* Transmit command  */
	if(reg != 0xFF) {
		gpio_set_value(spi_cs, 0);
		udelay(1);
		hitachi_spi_write_cmd(reg);
	}
	gpio_set_value(spi_cs, 1);
}


static void hitachi_seq_power_on(void)
{
	/* Power on set by panel reset routine */
	if(lcdc_hitachi_pdata && lcdc_hitachi_pdata->gpio) {
		gpio_set_value(lcdc_hitachi_pdata->gpio, 0);
		mdelay(5);
		gpio_set_value(lcdc_hitachi_pdata->gpio, 1);
		mdelay(10);
	}
}

static void hitachi_seq_power_off(void)
{
	/* Power off set by panel power save routine */
	if(lcdc_hitachi_pdata && lcdc_hitachi_pdata->gpio) {
		gpio_set_value(lcdc_hitachi_pdata->gpio, 0);
		mdelay(5);
	}
}

static void hitachi_seq_exit_sleep(void)
{
	serigo(0x3A, 0x60);	 /* set pixel format 24bit:0x70, 18bit:0x60 */
	serigo2(0x11);		 /* exit sleep mode */
	mdelay(117);		 /* > 7 frame */
}

static void hitachi_seq_enter_sleep(void)
{
	serigo(0xDA, 0x01);			/* Manual sequencer control */
	serigo2(0x10);				/* enter sleep mode */
	mdelay(67);					/* > 4 frame */
	serigo(0xB0, 0x03);		 /* Manufacturer command access protect */
	serigo2(0x01);			 /* Soft reset */
	mdelay(2);
}

static void hitachi_seq_set_display_on(void)
{
	serigo(0x36, 0x00);			/* set address mode */
	serigo2(0x29);				/* set display on */
	mdelay(1);
}

static void hitachi_seq_set_display_off(void)
{
	serigo2(0x28);				/* set display off */
	mdelay(5);
}

static void hitachi_seq_enter_deep_stanby(void)
{
	serigo(0xB0, 0x04);		  /* Manufacturer command access packet */
	serigo(0xB1, 0x01);		  /* Low power mode control */
	mdelay(5);
}

static void hitachi_seq_exit_deep_stanby(void)
{
}

static void hitachi_seq_blc_on(void)
{
	/* Manufacturer Command Access Packet */
	serigo(0xB0, 0x04);
	/* Backlight Control(1) */
	serigo(0xB8, 0x01);
	serigo(0xFF, hitachi_tuning.threw);
	serigo(0xFF, hitachi_tuning.threw);
	serigo(0xFF, hitachi_tuning.ulmtw);
	serigo(0xFF, hitachi_tuning.ulmtw);
	serigo(0xFF, hitachi_tuning.llmtw);
	serigo(0xFF, hitachi_tuning.llmtw);
	serigo(0xFF, 0x04);
	serigo(0xFF, 0x1F);
	serigo(0xFF, 0x90);
	serigo(0xFF, 0x90);
	serigo(0xFF, 0x1F);
	serigo(0xFF, 0x3D);
	serigo(0xFF, 0x6B);
	serigo(0xFF, 0xAA);
	serigo(0xFF, 0x00);
	/* Backlight Control(2) */
	serigo(0xB9, 0x00);
	serigo(0xFF, 0xFF);
	serigo(0xFF, 0x02);
	serigo(0xFF, 0x08);

	/* Manufacturer Command Access Packet */
	serigo(0xB0, 0x03);
}

static void hitachi_seq_blc_off(void)
{
	/* Manufacturer Command Access Packet */
	serigo(0xB0, 0x04);
	/* Backlight Control(1) */
	serigo(0xB8, 0x00);
	serigo(0xFF, hitachi_tuning.threw);
	serigo(0xFF, hitachi_tuning.threw);
	serigo(0xFF, hitachi_tuning.ulmtw);
	serigo(0xFF, hitachi_tuning.ulmtw);
	serigo(0xFF, hitachi_tuning.llmtw);
	serigo(0xFF, hitachi_tuning.llmtw);
	serigo(0xFF, 0x04);
	serigo(0xFF, 0x1F);
	serigo(0xFF, 0x90);
	serigo(0xFF, 0x90);
	serigo(0xFF, 0x1F);
	serigo(0xFF, 0x3D);
	serigo(0xFF, 0x6B);
	serigo(0xFF, 0xAA);
	serigo(0xFF, 0x00);
	/* Backlight Control(2) */
	serigo(0xB9, 0x00);
	serigo(0xFF, 0xFF);
	serigo(0xFF, 0x02);
	serigo(0xFF, 0x08);

	/* Manufacturer Command Access Packet */
	serigo(0xB0, 0x03);
}

static void hitachi_seq_refresh_on_blc_off(void)
{
	/* Manufacturer Command Access Packet */
	serigo(0xB0, 0x04);
	/* set pixel format 24bit:0x70, 18bit:0x60 */
	serigo(0x3A, 0x60);
	/* Backlight Control(1) */
	serigo(0xB8, 0x00);
	serigo(0xFF, hitachi_tuning.threw);
	serigo(0xFF, hitachi_tuning.threw);
	serigo(0xFF, hitachi_tuning.ulmtw);
	serigo(0xFF, hitachi_tuning.ulmtw);
	serigo(0xFF, hitachi_tuning.llmtw);
	serigo(0xFF, hitachi_tuning.llmtw);
	serigo(0xFF, 0x04);
	serigo(0xFF, 0x1F);
	serigo(0xFF, 0x90);
	serigo(0xFF, 0x90);
	serigo(0xFF, 0x1F);
	serigo(0xFF, 0x3D);
	serigo(0xFF, 0x6B);
	serigo(0xFF, 0xAA);
	serigo(0xFF, 0x00);
	/* Backlight Control(2) */
	serigo(0xB9, 0x00);
	serigo(0xFF, 0xFF);
	serigo(0xFF, 0x02);
	serigo(0xFF, 0x08);
	/* Manual Sequencer Control */
	serigo(0xDA, 0x00);
	/* Test mode20 */
	serigo(0xFD, 0x00);
	serigo(0xFF, 0x00);
	serigo(0xFF, 0x08);
	mdelay(5);
	/* NVM Read control */
	serigo(0xE2, 0x62);
	/* NVM Access control */
	serigo(0xE0, 0x00);
	serigo(0xFF, 0x20);
	mdelay(2);
	/* set address mode */
	serigo(0x36, 0x00);
	/* set display on */
	serigo2(0x29);
	mdelay(5);
}

static void hitachi_init(void)
{
	hitachi_seq_power_on();
	hitachi_seq_exit_sleep();
	hitachi_seq_blc_off();
	hitachi_seq_set_display_on();
	hitachi_seq_refresh_on_blc_off();
}

static void hitachi_init_tuning_data(void)
{
	hitachi_tuning.threw = 0x02;
	hitachi_tuning.ulmtw = 0xFF;
	hitachi_tuning.llmtw = 0xEB;
}

static void hitachi_sleep(void)
{
	hitachi_seq_set_display_off();
	hitachi_seq_enter_sleep();
	hitachi_seq_power_off();
}

static void hitachi_disp_powerup(void)
{
	if (!hitachi_state.disp_powered_up && !hitachi_state.display_on) {
	      hitachi_state.disp_powered_up = TRUE;
	}
}

static void hitachi_disp_on(void)
{
	dprintk("hitachi disp on\n");
	if (hitachi_state.disp_powered_up && !hitachi_state.display_on) {
		dprintk("hitachi disp on initialize data go out\n");
		hitachi_init();
		hitachi_state.display_on = TRUE;
	}
}

static int lcdc_hitachi_panel_on(struct platform_device *pdev)
{
	dprintk("hitachi panel on\n");
	if (!hitachi_state.disp_initialized) {
		if(lcdc_hitachi_pdata->panel_config_gpio)
			lcdc_hitachi_pdata->panel_config_gpio(1);
		hitachi_spi_init();
		hitachi_disp_powerup();
		if(system_state == 	SYSTEM_BOOTING &&
		   lcdc_hitachi_pdata->initialized) {
			hitachi_state.display_on = TRUE;
		} else {
			hitachi_disp_on();
		}
		hitachi_state.disp_initialized = TRUE;
	}
	return 0;
}

static int lcdc_hitachi_panel_off(struct platform_device *pdev)
{
	dprintk("hitachi disp off\n");
	if (hitachi_state.disp_powered_up && hitachi_state.display_on) {
		hitachi_sleep();
		if(lcdc_hitachi_pdata->panel_config_gpio)
			lcdc_hitachi_pdata->panel_config_gpio(0);
		hitachi_state.display_on = FALSE;
		hitachi_state.disp_initialized = FALSE;
	}
	return 0;
}

static ssize_t tuning_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int threw, ulmtw, llmtw;

	sscanf(buf, "%d %d %d", &threw, &ulmtw, &llmtw);
	printk(KERN_INFO "%s: threw=0x%x, ulmtw=0x%x, llmtw=0x%x\n",
		   __func__, threw, ulmtw, llmtw);
	hitachi_tuning.threw = threw;
	hitachi_tuning.ulmtw = ulmtw;
	hitachi_tuning.llmtw = llmtw;

	return count;
}

static ssize_t enable_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int enable;

	sscanf(buf, "%d", &enable);
	if(enable)
		lcdc_hitachi_panel_on(dev->platform_data);
	else
		lcdc_hitachi_panel_off(dev->platform_data);

	return count;
}

static ssize_t enable_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", hitachi_state.display_on);		
}

static DEVICE_ATTR(enable,  0666, enable_show,  enable_store);  
static DEVICE_ATTR(tuning,  0666, NULL,  tuning_store);

static int __init hitachi_probe(struct platform_device *pdev)
{
	int status=0;
	
	if (pdev->id == 0) {
		lcdc_hitachi_pdata = pdev->dev.platform_data;
		lcdc_spi_pin_assign();
		hitachi_init_tuning_data();
		dprintk("hitachi probe (id == 0)\n");

		return 0;
	}
	msm_fb_add_device(pdev);
	dprintk("hitachi probe (id not 0)\n");

	/* debugging point */
	status = device_create_file(&(pdev->dev), &dev_attr_enable);
	status = device_create_file(&(pdev->dev), &dev_attr_tuning);

	return status;
}

static struct platform_driver this_driver = {
	.probe  = hitachi_probe,
	.driver = {
		.name   = MODULE_NAME,
	},
};

static struct msm_fb_panel_data hitachi_panel_data = {
	.on = lcdc_hitachi_panel_on,
	.off = lcdc_hitachi_panel_off,
};

static struct platform_device this_device = {
	.name   = MODULE_NAME,
	.id	= 1,
	.dev	= {
		.platform_data = &hitachi_panel_data,
	}
};

static int __init lcdc_hitachi_panel_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	dprintk("LCDC hitachi init\n");
	
#ifdef CONFIG_FB_MSM_LCDC_LGIT_HITACHI_WVGA
	if (msm_fb_detect_client(MODULE_NAME))
		return 0;
#endif
	ret = platform_driver_register(&this_driver);
	if (ret)
		return ret;

	pinfo = &hitachi_panel_data.panel_info;
	pinfo->xres = 480;
	pinfo->yres = 800;
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 18;
	pinfo->fb_num = 2;
	pinfo->clk_rate = 24576000;

	pinfo->lcdc.h_back_porch = h_back_porch;
	pinfo->lcdc.h_front_porch = 30;
	pinfo->lcdc.h_pulse_width = 0x01;
	pinfo->lcdc.v_back_porch = 0x08;
	pinfo->lcdc.v_front_porch = 0x08;
	pinfo->lcdc.v_pulse_width = 0x02;
	pinfo->lcdc.border_clr = 0;     /* blk */
	pinfo->lcdc.underflow_clr = 0xff;       /* blue */
	pinfo->lcdc.hsync_skew = 0;
	
	ret = platform_device_register(&this_device);
	if (ret)
		platform_driver_unregister(&this_driver);

	return ret;
}

module_init(lcdc_hitachi_panel_init);
