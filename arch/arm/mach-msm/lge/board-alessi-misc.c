/* arch/arm/mach-msm/lge/board-alessi-misc.c
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
#include <linux/interrupt.h>
#include <asm/setup.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <mach/pmic.h>
#include <mach/msm_battery.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <asm/io.h>
#include <mach/rpc_server_handset.h>
#include <mach/board_lge.h>
#include "board-alessi.h"

static u32 thunderg_battery_capacity(u32 current_soc)
{
	if(current_soc > 100)
		current_soc = 100;

	return current_soc;
}

static struct msm_psy_batt_pdata msm_psy_batt_data = {
	.voltage_min_design     = 3200,
	.voltage_max_design     = 4200,
	.avail_chg_sources      = AC_CHG | USB_CHG ,
	.batt_technology        = POWER_SUPPLY_TECHNOLOGY_LION,
	.calculate_capacity		= thunderg_battery_capacity,
};

static struct platform_device msm_batt_device = {
	.name           = "msm-battery",
	.id         = -1,
	.dev.platform_data  = &msm_psy_batt_data,
};

/* Alessi Board Vibrator Functions for Android Vibrator Driver */
#define GPIO_LIN_MOTOR_PWM		28
#define GPIO_LIN_MOTOR_EN       76

#define GP_MN_CLK_MDIV			0x004C
#define GP_MN_CLK_NDIV			0x0050
#define GP_MN_CLK_DUTY			0x0054

/* about 22.93 kHz, should be checked */
#define GPMN_M_DEFAULT			21
#define GPMN_N_DEFAULT			4500
#define GPMN_D_DEFAULT             	(GPMN_N_DEFAULT >> 1)
#define PWM_MULTIPLIER			((GPMN_N_DEFAULT >> 1) - 60) //(GPMN_N_DEFAULT >> 1)
/* default duty cycle = disable motor ic */

#define GPMN_M_MASK				0x01FF
#define GPMN_N_MASK				0x1FFF
#define GPMN_D_MASK				0x1FFF

#define REG_WRITEL(value, reg)	writel(value, (MSM_WEB_BASE+reg))

/*LED has 15 steps (10mA per step). LED's  max power capacity is 150mA. (0~255 level)*/
#define MAX_BACKLIGHT_LEVEL	16	// 150mA
#define TUNED_MAX_BACKLIGHT_LEVEL	40	// 60mA


static void button_bl_leds_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	int ret;

	ret = pmic_set_led_intensity(LED_KEYPAD, value / TUNED_MAX_BACKLIGHT_LEVEL);

	if (ret)
		dev_err(led_cdev->dev, "can't set keypad backlight\n");

}

struct led_classdev thunderg_custom_leds[] = {
	{
		.name = "button-backlight",
		.brightness_set = button_bl_leds_set,
		.brightness = LED_OFF,
	},
};

static int register_leds(struct platform_device *pdev)
{
	int rc;
	rc = led_classdev_register(&pdev->dev, &thunderg_custom_leds);
	if (rc) {
		dev_err(&pdev->dev, "unable to register led class driver\n");
		return rc;
	}
	button_bl_leds_set(&thunderg_custom_leds, LED_OFF);
	return rc;
}

static int unregister_leds(struct platform_device *pdev)
{
	//led_classdev_unregister(&thunderg_custom_leds);
	button_bl_leds_set(&thunderg_custom_leds, LED_OFF);
	return 0;
}

static int suspend_leds(struct platform_device *dev,
		pm_message_t state)
{
	//led_classdev_suspend(&thunderg_custom_leds);

	return 0;
}

static int resume_leds(struct platform_device *dev)
{
	led_classdev_resume(&thunderg_custom_leds);

	return 0;
}

static struct msm_pmic_leds_pdata leds_pdata = {
	.custom_leds		= thunderg_custom_leds,
	.register_custom_leds	= register_leds,
	.unregister_custom_leds	= unregister_leds,
	.suspend_custom_leds	= suspend_leds,
	.resume_custom_leds	= resume_leds,
	.msm_keypad_led_set	= button_bl_leds_set,
};

static struct platform_device msm_device_pmic_leds = {
	.name                           = "pmic-leds",
	.id                                     = -1,
	.dev.platform_data      = &leds_pdata,
};
/* lge carkit device */
static char *dock_state_string[] = {
	"0",
	"1",
	"2",
};

enum {
	DOCK_STATE_UNDOCKED = 0,
	DOCK_STATE_DESK = 1, /* multikit */
	DOCK_STATE_CAR = 2, /* carkit */
	DOCK_STATE_UNKNOWN,
};

enum {
	KIT_DOCKED = 0,
	KIT_UNDOCKED = 1,
};

static void alessi_desk_dock_detect_callback(int state)
{
	int ret;

	if (state)
		state = DOCK_STATE_DESK;

	ret = lge_gpio_switch_pass_event("dock", state);

	if (ret)
		printk(KERN_INFO "%s: desk dock event report fail\n", __func__);

	return;
}

static void alessi_register_callback(void)
{
	rpc_server_hs_register_callback(alessi_desk_dock_detect_callback);

	return;
}
static int alessi_gpio_carkit_work_func(void)
{
	int state;
	int gpio_value;

	gpio_value = gpio_get_value(GPIO_CARKIT_DETECT);
	printk(KERN_INFO"%s: carkit detected : %s\n", __func__, 
			gpio_value?"undocked":"docked");
	if (gpio_value == KIT_DOCKED)
		state = DOCK_STATE_CAR;
	else
		state = DOCK_STATE_UNDOCKED;

	return state;
}

static char *alessi_gpio_carkit_print_state(int state)
{
	return dock_state_string[state];
}

static int alessi_gpio_carkit_sysfs_store(const char *buf, size_t size)
{
	int state;

	if (!strncmp(buf, "undock", size-1))
		state = DOCK_STATE_UNDOCKED;
	else if (!strncmp(buf, "desk", size-1))
		state = DOCK_STATE_DESK;
	else if (!strncmp(buf, "car", size-1))
		state = DOCK_STATE_CAR;
	else
		return -EINVAL;

	return state;
}

static unsigned alessi_carkit_gpios[] = {
	GPIO_CARKIT_DETECT,
};

static struct lge_gpio_switch_platform_data alessi_carkit_data = {
	.name = "dock",
	.gpios = alessi_carkit_gpios,
	.num_gpios = ARRAY_SIZE(alessi_carkit_gpios),
	.irqflags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.wakeup_flag = 1,
	.work_func = alessi_gpio_carkit_work_func,
	.print_state = alessi_gpio_carkit_print_state,
	.sysfs_store = alessi_gpio_carkit_sysfs_store,
	.additional_init = alessi_register_callback,
};

static struct platform_device alessi_carkit_device = {
	.name   = "lge-switch-gpio",
	.id = 0,
	.dev = {
		.platform_data = &alessi_carkit_data,
	},
};

int alessi_vibrator_power_set(int enable)
{
	static int is_enabled = 0;
	static struct vreg *s_vreg_vibrator;
	int rc;    
	
	if (enable) {
		if (is_enabled) {
			//printk(KERN_INFO "vibrator power was enabled, already\n");
			return 0;
		}
		s_vreg_vibrator = vreg_get(NULL, "rftx");
		rc = vreg_set_level(s_vreg_vibrator, 3000);
		if(rc != 0) {
			return -1;
		}
		vreg_enable(s_vreg_vibrator);
		is_enabled = 1;
	} else {
		if (!is_enabled) {
			//printk(KERN_INFO "vibrator power was disabled, already\n");
			return 0;
		}		
		s_vreg_vibrator = vreg_get(NULL, "rftx");
		rc = vreg_set_level(s_vreg_vibrator, 0);
		vreg_disable(s_vreg_vibrator);
		is_enabled = 0;
	}
	return 0;
}

int alessi_vibrator_pwn_set(int enable, int amp)
{
    	int gain = ((PWM_MULTIPLIER * amp) >> 7) + GPMN_D_DEFAULT;
       REG_WRITEL((gain & GPMN_D_MASK), GP_MN_CLK_DUTY);
	return 0;
}

int alessi_vibrator_ic_enable_set(int enable)
{
	if(enable)	{
		REG_WRITEL((GPMN_M_DEFAULT & GPMN_M_MASK), GP_MN_CLK_MDIV);
		REG_WRITEL((~(GPMN_N_DEFAULT - GPMN_M_DEFAULT) & GPMN_N_MASK), GP_MN_CLK_NDIV);
		gpio_direction_output(GPIO_LIN_MOTOR_EN, 1);
	} else {
		gpio_direction_output(GPIO_LIN_MOTOR_EN, 0);
	}
	return 0;
}

static struct android_vibrator_platform_data alessi_vibrator_data = {
	.enable_status = 0,	
	.power_set = alessi_vibrator_power_set,
	.pwm_set = alessi_vibrator_pwn_set,
	.ic_enable_set = alessi_vibrator_ic_enable_set,
	.amp_value = 125,
};

static struct platform_device android_vibrator_device = {
	.name   = "android-vibrator",
	.id = -1,
	.dev = {
		.platform_data = &alessi_vibrator_data,
	},
};

/* ear sense driver */
static char *ear_state_string[] = {
	"0",
	"1",
};

enum {
	EAR_STATE_EJECT = 0,
	EAR_STATE_INJECT = 1, 
};

enum {
	EAR_EJECT = 0,
	EAR_INJECT = 1,
};

static int thunderg_gpio_earsense_work_func(void)
{
	int state;
	int gpio_value;
	
	gpio_value = gpio_get_value(GPIO_EAR_SENSE);
	printk(KERN_INFO"%s: ear sense detected : %s\n", __func__, 
			gpio_value?"injected":"ejected");
	if (gpio_value == EAR_EJECT) {
		state = EAR_STATE_EJECT;
		gpio_set_value(GPIO_HS_MIC_BIAS_EN, 0);
	} else {
		state = EAR_STATE_INJECT;
		gpio_set_value(GPIO_HS_MIC_BIAS_EN, 1);
	}

	return state;
}

static char *thunderg_gpio_earsense_print_state(int state)
{
	return ear_state_string[state];
}

static int thunderg_gpio_earsense_sysfs_store(const char *buf, size_t size)
{
	int state;

	if (!strncmp(buf, "eject", size - 1))
		state = EAR_STATE_EJECT;
	else if (!strncmp(buf, "inject", size - 1))
		state = EAR_STATE_INJECT;
	else
		return -EINVAL;

	return state;
}

static unsigned thunderg_earsense_gpios[] = {
	GPIO_EAR_SENSE,
};

static struct lge_gpio_switch_platform_data thunderg_earsense_data = {
	.name = "h2w",
	.gpios = thunderg_earsense_gpios,
	.num_gpios = ARRAY_SIZE(thunderg_earsense_gpios),
	.irqflags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.wakeup_flag = 1,
	.work_func = thunderg_gpio_earsense_work_func,
	.print_state = thunderg_gpio_earsense_print_state,
	.sysfs_store = thunderg_gpio_earsense_sysfs_store,
};

static struct platform_device thunderg_earsense_device = {
	.name   = "lge-switch-gpio",
	.id = 1,
	.dev = {
		.platform_data = &thunderg_earsense_data,
	},
};

/* misc platform devices */
static struct platform_device *thunderg_misc_devices[] __initdata = {
	&msm_device_pmic_leds,
	&msm_batt_device,
	&android_vibrator_device,
	&alessi_carkit_device,
	&thunderg_earsense_device,
};

// 20100831 hyuncheol0.kim <Disable the carkit device for Alessi. [START]
static struct platform_device *alessi_misc_devices[] __initdata = {
	&msm_device_pmic_leds,
	&msm_batt_device,
	&android_vibrator_device,
	&thunderg_earsense_device,
};
// 20100831 hyuncheol0.kim <Disable the carkit device for Alessi. [END]

/* main interface */
void __init lge_add_misc_devices(void)
{
// 20100831 hyuncheol0.kim <Disable the carkit device for Alessi. [START]
	//platform_add_devices(thunderg_misc_devices, ARRAY_SIZE(thunderg_misc_devices));
	platform_add_devices(alessi_misc_devices, ARRAY_SIZE(alessi_misc_devices));
// 20100831 hyuncheol0.kim <Disable the carkit device for Alessi. [END]
}

