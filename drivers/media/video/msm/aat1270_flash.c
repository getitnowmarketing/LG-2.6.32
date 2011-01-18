/*
 * drivers/media/video/msm/aat1270_flash.c
 *
 * Flash(AAT1270) driver
 *
 * Copyright (C) 2010 LGE, Inc.
 *
 * Author: Jun-Yeong Han <junyeong.han@lge.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <../../../drivers/staging/android/timed_output.h>
#include <linux/leds.h>
#include <linux/gpio.h>

#include <mach/camera.h>

#include <mach/board_lge.h>

#define FLASH_OFF		0
#define FLASH_ON		1

static struct delayed_work work_flash_on;
static struct work_struct work_flash_off;
static struct hrtimer flash_timer;
static struct aat1270_flash_platform_data *aat1270_pdata;

static void flash_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	led_cdev->brightness = value;
}

static struct led_classdev flash_led = {
	.name			= "spotlight",
	.brightness_set	= flash_led_set,
	.brightness		= LED_OFF,
};

/*
 * S2Cwire Data			Movie Mode Current (%)
 * Percentage of Maximum
 * 	1						100
 * 	2						89
 * 	3						79
 *	4						71
 *	5						63
 *	6						56
 *	7						50
 *	8						44.7
 *	9						39.8
 *	10						35.5
 *	11						31.6
 *	12						28.2
 *	13						25.1
 *	14						22.4
 *	15						20
 *	16						0
 */

int aat1270_flash_led_set_current(int on)
{	
	int s2c_count;

	printk("aat1270_flash_led_set_current\n");
	if (on == FLASH_ON) {
		if (flash_led.brightness == LED_FULL) {
			gpio_direction_output(aat1270_pdata->gpio_flen, 1);
		} else if (flash_led.brightness == LED_HALF) {
			for(s2c_count = 0; s2c_count < 7; ++s2c_count) {
				gpio_direction_output(aat1270_pdata->gpio_en_set, 0);
				udelay(1);
				gpio_direction_output(aat1270_pdata->gpio_en_set, 1);
				udelay(1);
			}
		}
	} else if (on == FLASH_OFF) {
		gpio_direction_output(aat1270_pdata->gpio_flen, 0);
		gpio_direction_output(aat1270_pdata->gpio_en_set, 0);
	}
	
	return 0;
}
EXPORT_SYMBOL(aat1270_flash_led_set_current);

static void flash_on(struct work_struct *work)
{
	printk("LG_FW_SNAPSHOT FLASH START\n");
	aat1270_flash_led_set_current(FLASH_ON);
}

static void flash_off(struct work_struct *work)
{
	aat1270_flash_led_set_current(FLASH_OFF);
}

static void timed_flash_on(struct timed_output_dev *sdev)
{
	schedule_delayed_work(&work_flash_on, msecs_to_jiffies(500));
}

static void timed_flash_off(struct timed_output_dev *sdev)
{
	schedule_work(&work_flash_off);
}

static void flash_enable(struct timed_output_dev *dev, int value)
{
	
	if(value == 0x1234)
	{
		printk("LG_FW_AUTO_FLASH START\n");
		aat1270_flash_led_set_current(FLASH_ON);
		return;			
	}
	else if(value==0x4321)
	{
		printk("LG_FW_AUTO_FLASH END\n");
		aat1270_flash_led_set_current(FLASH_OFF);
		return;
	}
	
	hrtimer_cancel(&flash_timer);

	if (value == 0)
		timed_flash_off(dev);
	else {
		value += 500;
		value = (value > 15000 ? 15000 : value);

		timed_flash_on(dev);

		hrtimer_start(&flash_timer,
			ktime_set(value / 1000, (value % 1000) * 1000000),
			HRTIMER_MODE_REL);
	}
}

static int flash_get_time(struct timed_output_dev *dev)
{
	if (hrtimer_active(&flash_timer)) {
		ktime_t r = hrtimer_get_remaining(&flash_timer);
		return r.tv.sec * 1000 + r.tv.nsec / 1000000;
	} else
		return 0;
}

static enum hrtimer_restart flash_timer_func(struct hrtimer *timer)
{
	timed_flash_off(0);
	return HRTIMER_NORESTART;
}

static struct timed_output_dev to_flash = {
	.name		= "flash",
	.enable		= flash_enable,
	.get_time	= flash_get_time,
};

static int __devinit aat1270_probe(struct platform_device *pdev)
{
	aat1270_pdata = pdev->dev.platform_data;

	gpio_tlmm_config(GPIO_CFG(aat1270_pdata->gpio_flen, 0, GPIO_OUTPUT,
		GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
	gpio_tlmm_config(GPIO_CFG(aat1270_pdata->gpio_en_set, 0, GPIO_OUTPUT,
		GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
	gpio_tlmm_config(GPIO_CFG(aat1270_pdata->gpio_inh, 0, GPIO_OUTPUT,
		GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);

	gpio_direction_output(aat1270_pdata->gpio_flen, 0);
	gpio_direction_output(aat1270_pdata->gpio_en_set, 0);
	gpio_direction_output(aat1270_pdata->gpio_inh, 0);

	INIT_DELAYED_WORK(&work_flash_on, flash_on);
	INIT_WORK(&work_flash_off, flash_off);

	hrtimer_init(&flash_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	flash_timer.function = flash_timer_func;

	led_classdev_register(&pdev->dev, &flash_led);

	timed_output_dev_register(&to_flash);
	
	return 0;
}
 
static int __devexit aat1270_remove(struct platform_device *pdev)
{
	led_classdev_unregister(&flash_led);
	return 0;
}
 
static struct platform_driver aat1270_driver = {
	.probe = aat1270_probe,
	.remove = __devexit_p(aat1270_remove),
	.driver = {
		.name = "aat1270_flash",
		.owner = THIS_MODULE,
	},
};

static int __init aat1270_init(void)
{
	return platform_driver_register(&aat1270_driver);
}
module_init(aat1270_init);

static void __exit aat1270_exit(void)
{
	platform_driver_unregister(&aat1270_driver);
}
module_exit(aat1270_exit);

MODULE_DESCRIPTION();
MODULE_AUTHOR("Jun-Yeong HAN <junyeong.han@lge.com>");
MODULE_LICENSE("GPL");

