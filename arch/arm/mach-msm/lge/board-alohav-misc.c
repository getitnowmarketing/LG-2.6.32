/* arch/arm/mach-msm/lge/board-alohav-misc.c
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
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/power_supply.h>
#include <asm/setup.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <mach/pmic.h>
#include <mach/msm_battery.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <asm/io.h>

#include <mach/board_lge.h>
#include "board-alohav.h"

/* 
 * jinkyu.choi@lge.com, in case of AlohaV Board,
 * leds driver must control the vreg power for keeping leds on state 
 * during battery charging via USB or AC
 * because the vreg power of leds and touch is same power.
 */

#define LED_R_ON (1<<0)
#define LED_G_ON (1<<1)
#define LED_B_ON (1<<2)

int leds_registered = 0;
int factory_reset_checked = 0;
int led_power_onoff = 0;
extern int factory_reset_check(void);

int led_set_vreg(unsigned char onoff)
{
	struct vreg *vreg_led;
	int rc;

	//vreg_led = vreg_get(0, "ruim");
	vreg_led = vreg_get(0,"gp14");

	if(IS_ERR(vreg_led)) {
		printk("[RGB LED] vreg_get fail\n");
		return -1;
	}

	if (onoff) {
		rc = vreg_set_level(vreg_led, 3050);
		if (rc != 0) {
			printk("[RGB LED] vreg_set_level failed\n");
			return -1;
		}
		vreg_enable(vreg_led);
	} else
		vreg_disable(vreg_led);

	printk("[RGB LED] %s() on/off: %s\n",__FUNCTION__, onoff?"ON":"OFF");
	return 0;	
}
EXPORT_SYMBOL(led_set_vreg);

static void pmic_mpp_isink_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	int mpp_number;
	int on_off;
	unsigned int rgb = 0;
	static int power_on = 0;
	static unsigned int status = 0;

	if (!strcmp(led_cdev->name ,"red")) {
		mpp_number = (int)PM_MPP_13;
		rgb = LED_R_ON;
		//printk("[RGB LED] Red on/off %d\n", value);
	} else if (!strcmp(led_cdev->name, "green")) {
		mpp_number = (int)PM_MPP_14;
		rgb = LED_G_ON;
		//printk("[RGB LED] Green on/off %d\n", value);
	} else if (!strcmp(led_cdev->name, "blue")) {
		mpp_number = (int)PM_MPP_15;
		rgb = LED_B_ON;
		//printk("[RGB LED] Blue on/off %d\n", value);
	} else
		return;

	if(value == 0) {
		status &= ~rgb;
		if (power_on && (!status)) {
			if (led_set_vreg(0) == 0)
				power_on = 0;
		}
		on_off = (int)PM_MPP__I_SINK__SWITCH_DIS;
	} else {
		status |= rgb;
		if (!power_on) {
			if (led_set_vreg(1) == 0)
				power_on = 1;
		}
		on_off = (int)PM_MPP__I_SINK__SWITCH_ENA;
	}
	//printk("[RGB LED] current status %d, power_on %d\n", status, power_on);
	
	pmic_secure_mpp_config_i_sink((enum mpp_which)mpp_number,
		PM_MPP__I_SINK__LEVEL_5mA, (enum mpp_i_sink_switch)on_off);

	led_power_onoff = power_on;

#if 1 /* FIXME: temporal removing */
	if(leds_registered == 1 && factory_reset_checked == 0)
	{
		factory_reset_check();
		factory_reset_checked = 1;
	}
#endif
}

static void button_backlight_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	if (value == 0)
		gpio_set_value(EL_EN_GPIO, 0);
	else
		gpio_set_value(EL_EN_GPIO, 1);
}

struct led_classdev alohav_custom_leds[] = {
	{
		.name = "red",
		.brightness_set = pmic_mpp_isink_set,
		.brightness = LED_OFF,
	},
	{
		.name = "green",
		.brightness_set = pmic_mpp_isink_set,
		.brightness = LED_OFF,
	},
	{
		.name = "blue",
		.brightness_set = pmic_mpp_isink_set,
		.brightness = LED_OFF,
	},
	{
		.name = "button-backlight",
		.brightness_set = button_backlight_set,
		.brightness = LED_OFF,
	},
};

static int register_leds(struct platform_device *pdev)
{
	int rc;
	int i;

    gpio_tlmm_config(GPIO_CFG(EL_EN_GPIO, 0, GPIO_OUTPUT, GPIO_PULL_UP,
				                GPIO_2MA), GPIO_ENABLE);

	for(i = 0 ; i < ARRAY_SIZE(alohav_custom_leds) ; i++) {
		rc = led_classdev_register(&pdev->dev, &alohav_custom_leds[i]);
		if (rc) {
    		dev_err(&pdev->dev, "unable to register led class driver : alohav_custom_leds\n");
    		return rc;
	    }
	    pmic_mpp_isink_set(&alohav_custom_leds[i], LED_OFF);
	}

	leds_registered = 1;
	return rc;
}

static void unregister_leds (void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(alohav_custom_leds); ++i)
		led_classdev_unregister(&alohav_custom_leds[i]);
}

static void suspend_leds (void)
{
	/* 
	 * jinkyu.choi@lge.com, in case of VS740
	 * do nothing for keeping on the LEDs ON status
	 * though the device goes to the deep sleep state
	 */
#if 0
	int i;
	for (i = 0; i < ARRAY_SIZE(alohav_custom_leds); ++i)
		led_classdev_suspend(&alohav_custom_leds[i]);
#endif
}

static void resume_leds (void)
{
	/* 
	 * jinkyu.choi@lge.com, in case of VS740
	 * do nothing for keeping on the LEDs ON status
	 * though the device goes to the deep sleep state
	 */
#if 0
	int i;
	for (i = 0; i < ARRAY_SIZE(alohav_custom_leds); ++i)
		led_classdev_resume(&alohav_custom_leds[i]);
#endif
}

int keypad_led_set(unsigned char value)
{
	int ret;
	
	ret = pmic_set_led_intensity(LED_KEYPAD, value);

	return ret;
}

static struct msm_pmic_leds_pdata leds_pdata = {
	.custom_leds			= alohav_custom_leds,
	.register_custom_leds	= register_leds,
	.unregister_custom_leds	= unregister_leds,
	.suspend_custom_leds	= suspend_leds,
	.resume_custom_leds		= resume_leds,
	.msm_keypad_led_set		= keypad_led_set,
};

static struct platform_device msm_device_pmic_leds = {
	.name				= "pmic-leds",
	.id					= -1,
	.dev.platform_data	= &leds_pdata,
};

static struct msm_psy_batt_pdata msm_psy_batt_data = {
	.voltage_min_design     = 3200,
	.voltage_max_design     = 4200,
	.avail_chg_sources      = AC_CHG | USB_CHG ,
	.batt_technology        = POWER_SUPPLY_TECHNOLOGY_LION,
};

static struct platform_device msm_batt_device = {
	.name           = "msm-battery",
	.id         = -1,
	.dev.platform_data  = &msm_psy_batt_data,
};

/* Vibrator Functions for Android Vibrator Driver */
#define VIBE_IC_VOLTAGE			3050	/* Change from 3000 to 3050 */
#define GPIO_LIN_MOTOR_PWM		28
#define GPIO_LIN_MOTOR_EN		76

#define GP_MN_CLK_MDIV_REG		0x004C
#define GP_MN_CLK_NDIV_REG		0x0050
#define GP_MN_CLK_DUTY_REG		0x0054

/* about 22.93 kHz, should be checked */
#define GPMN_M_DEFAULT			21
#define GPMN_N_DEFAULT			4500
/* default duty cycle = disable motor ic */
#define GPMN_D_DEFAULT			(GPMN_N_DEFAULT >> 1) 
#define PWM_MAX_HALF_DUTY		((GPMN_N_DEFAULT >> 1) - 80) /* minimum operating spec. should be checked */

#define GPMN_M_MASK				0x01FF
#define GPMN_N_MASK				0x1FFF
#define GPMN_D_MASK				0x1FFF

#define REG_WRITEL(value, reg)	writel(value, (MSM_WEB_BASE+reg))

int alohav_vibrator_power_set(int enable)
{
	struct vreg *vibe_vreg;
	static int is_enabled = 0;
		
	vibe_vreg = vreg_get(NULL, "gp3");

	if (IS_ERR(vibe_vreg)) {
		printk(KERN_ERR "%s: vreg_get failed\n", __FUNCTION__);
		return PTR_ERR(vibe_vreg);
	}

	if (enable) {
		if (is_enabled) {
			//printk(KERN_INFO "vibrator power was enabled, already\n");
			return 0;
		}
		
		/* 3000 mV for Motor IC */
		if (vreg_set_level(vibe_vreg, VIBE_IC_VOLTAGE) <0) {		
			printk(KERN_ERR "%s: vreg_set_level failed\n", __FUNCTION__);
			return -EIO;
		}
		
		if (vreg_enable(vibe_vreg) < 0 ) {
			printk(KERN_ERR "%s: vreg_enable failed\n", __FUNCTION__);
			return -EIO;
		}
		is_enabled = 1;
	} else {
		if (!is_enabled) {
			//printk(KERN_INFO "vibrator power was disabled, already\n");
			return 0;
		}
		
		if (vreg_set_level(vibe_vreg, 0) <0) {		
			printk(KERN_ERR "%s: vreg_set_level failed\n", __FUNCTION__);
			return -EIO;
		}
		
		if (vreg_disable(vibe_vreg) < 0) {
			printk(KERN_ERR "%s: vreg_disable failed\n", __FUNCTION__);
			return -EIO;
		}
		is_enabled = 0;
	}
	return 0;
}

int alohav_vibrator_pwm_set(int enable, int amp)
{
	int gain = ((PWM_MAX_HALF_DUTY*amp) >> 7)+ GPMN_D_DEFAULT;

	REG_WRITEL((GPMN_M_DEFAULT & GPMN_M_MASK), GP_MN_CLK_MDIV_REG);
	REG_WRITEL((~( GPMN_N_DEFAULT - GPMN_M_DEFAULT )&GPMN_N_MASK), GP_MN_CLK_NDIV_REG);
		
	if (enable) {
		REG_WRITEL((gain & GPMN_D_MASK), GP_MN_CLK_DUTY_REG);
		gpio_direction_output(GPIO_LIN_MOTOR_PWM, 1);
	} else {
		REG_WRITEL(GPMN_D_DEFAULT, GP_MN_CLK_DUTY_REG);
		gpio_direction_output(GPIO_LIN_MOTOR_PWM, 0);
	}
	
	return 0;
}

int alohav_vibrator_ic_enable_set(int enable)
{
	if (enable) {
		gpio_direction_output(GPIO_LIN_MOTOR_EN, 1);
	} else {
		gpio_direction_output(GPIO_LIN_MOTOR_EN, 0);
	}
	return 0;
}

static struct android_vibrator_platform_data alohav_vibrator_data = {
	.enable_status = 0,
	.power_set = alohav_vibrator_power_set,
	.pwm_set = alohav_vibrator_pwm_set,
	.ic_enable_set = alohav_vibrator_ic_enable_set,
	.amp_value = 100, /* default AMP value, should be fixed */
};

static struct platform_device android_vibrator_device = {
	.name   = "android-vibrator",
	.id = -1,
	.dev = {
		.platform_data = &alohav_vibrator_data,
	},
};

static struct gpio_h2w_platform_data alohav_h2w_data = {
	.gpio_detect = 18,
	.gpio_button_detect = 29,
};

static struct platform_device alohav_h2w_device = {
	.name = "gpio-h2w",
	.id = -1,
	.dev = {
		.platform_data = &alohav_h2w_data,
	},
};

static struct platform_device *alohav_misc_devices[] __initdata = {
	&msm_device_pmic_leds,
	&msm_batt_device,
	&android_vibrator_device,
	&alohav_h2w_device,
};

void __init lge_add_misc_devices(void)
{
	platform_add_devices(alohav_misc_devices, ARRAY_SIZE(alohav_misc_devices));
}

