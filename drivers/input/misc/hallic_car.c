/* drivers/misc/hall_bu52004.c
 *
 * (C) 2008 LGE, Inc.
 * (C) 2007 Google, Inc.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/timer.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>  
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/earlysuspend.h>
#include <linux/switch.h>
#include <asm/io.h>
#include <mach/gpio.h>
#include <mach/msm_iomap.h>
#include <mach/system.h> 
#include <linux/mutex.h>

/* Miscellaneous device */
#define MISC_DEV_NAME		"hall-ic-dock" 

#define REPORT_ANDROID 	1 /* definition in input_report from KERNEL to ANDROID */
#define USE_IRQ			1
#define GPIO_CARKIT_IRQ 37
#define GPIO_MULTIKIT_IRQ 39
#define JIFFIES_TO_MS(t) ((t) * 1000 / HZ)
#define MS_TO_JIFFIES(j) ((j * HZ) / 1000)

#define DECK_DEBUG 1
#if DECK_DEBUG
#define SDBG(fmt, args...) printk(fmt, ##args)
#else
#define SDBG(fmt, args...) do {} while (0)
#endif /* SLIDE_DEBUG */


static void hall_ic_dock_work_func(struct work_struct *work);

#define SLIDEUP_TIMEOUT_MS 400 /* 400: slide delay-timeout  */	
#define SLIDEUP_FAST_TIMEOUT_MS 100 /* 400: slide delay-timeout  */	
static int first_slide_timeout_ms =SLIDEUP_FAST_TIMEOUT_MS;

static atomic_t current_state = ATOMIC_INIT(0);
//static atomic_t last_state = ATOMIC_INIT(0);
//static atomic_t report_state = ATOMIC_INIT(0);
static struct workqueue_struct *hallic_dock_wq;
static int s_hall_ic_carkit_gpio;
static int s_hall_ic_multikit_gpio;
static int s_hallic_state ;
//static int hall_ic_on =-1; 
static int check_count =0;
static int suspend_flag = 0;
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend hall_ic_early_suspend;
#endif
static int enable_irq_on =-1;
struct mutex dock_dev_mutex;

struct hall_ic_dock_device {
	struct switch_dev sdev;
	struct input_dev *input_dev;
	struct work_struct work;
	struct timer_list timer;
	struct input_dev *kpdev;
	spinlock_t	lock;
	int use_irq;
	int enabled;
	int sample_rate;
};

static struct hall_ic_dock_device *dock_dev = NULL;

enum {
	GPIO_CARKIT_DETECT=0,
	GPIO_MULTIKIT_DETECT=0,
	GPIO_UNDOCKED = 1,
};

enum {
	HALL_IC_EARLY_SUSPEND 	= 2,
	HALL_IC_EARLY_RESUME 	= 0,
	HALL_IC_SUSPEND 		= 1,
	HALL_IC_RESUME 			= 0,
};

enum {
	DOCK_STATE_UNDOCKED	= 0,
	DOCK_STATE_DESK = 1, /* multikit */
	DOCK_STATE_CAR = 2,  /* carkit */
	DOCK_STATE_UNKNOWN,
};

static int reported_dock_state = DOCK_STATE_UNDOCKED;
static int prev_dock_state = DOCK_STATE_UNDOCKED;

#if 0
static void hall_ic_dock_disable(void)
{
	s_hallic_state = 0;
	SDBG("\nandroid-hall-ic: hall_ic_disble_set\n");
}

static void hall_ic_dock_enable(void)
{
	s_hallic_state = 1;
	SDBG("\nandroid-hall-ic: hall_ic_enable_set\n");
}

static void hall_ic_dock_set(int amp)
{

	SDBG("\nandroid-hall-ic: hall-ic_set\n");

	if(amp == 0) {
		hall_ic_dock_disable();
	}else {
		hall_ic_dock_enable();
	}
}
#endif

void hall_ic_dock_report_event(int state) 
{
	if(reported_dock_state != state)
	{
		printk(KERN_INFO"%s: report_state:%d\n",__func__,state);

		switch_set_state(&dock_dev->sdev, state);
		reported_dock_state = state;
	}

	spin_lock_irq(&dock_dev->lock);
	if(enable_irq_on ==0){
		printk(KERN_INFO"%s: enable irq\n",__func__);
		enable_irq(s_hall_ic_carkit_gpio);
		enable_irq(s_hall_ic_multikit_gpio);
		enable_irq_on = 1;
	}
  spin_unlock(&dock_dev->lock);
	return;
}
EXPORT_SYMBOL(hall_ic_dock_report_event);

static void hall_timer(unsigned long arg)
{
	queue_work(hallic_dock_wq, &dock_dev->work);
	return;
}


static void hall_ic_dock_work_func(struct work_struct *work)
{
	struct hall_ic_dock_device *dev = container_of(work, struct hall_ic_dock_device, work);
	short gpio_carkit, gpio_multikit;
	int state;

	gpio_carkit = gpio_get_value(GPIO_CARKIT_IRQ);
	gpio_multikit = gpio_get_value(GPIO_MULTIKIT_IRQ);

	if(gpio_carkit ==  GPIO_CARKIT_DETECT && gpio_multikit == GPIO_UNDOCKED)
	{
		state = DOCK_STATE_CAR;
	}
	else if(gpio_carkit ==  GPIO_UNDOCKED && gpio_multikit == GPIO_MULTIKIT_DETECT)
	{
    // hw has changed to detect only car for both case. DOCK_STATE_DESK will come from ARM9
		state = DOCK_STATE_CAR; //DOCK_STATE_DESK;
	}
	else if(gpio_carkit == GPIO_UNDOCKED && gpio_multikit == GPIO_UNDOCKED)
	{
		state = DOCK_STATE_UNDOCKED;
	}
	else
	{
		state = DOCK_STATE_UNKNOWN;
	}

#if 1
  if(check_count == 0 && prev_dock_state == state)
  {
		spin_lock_irq(&dock_dev->lock);
		if(enable_irq_on ==0){
			printk(KERN_INFO"%s: enable irq\n",__func__);
			enable_irq(s_hall_ic_carkit_gpio);
			enable_irq(s_hall_ic_multikit_gpio);
			enable_irq_on = 1;
		}
		spin_unlock(&dock_dev->lock);
		printk(KERN_INFO"%s: prev:%d, curr:%d, count:%d\n",__func__,prev_dock_state,state,check_count);
		printk(KERN_INFO"%s: stop wp and enable intr \n",__func__);
		return;    
  }
  else if (prev_dock_state == state)
	{
		check_count++;
	}
	else
	{
		check_count = 1;
	}
#else
	if(prev_dock_state == state)
	{
		check_count++;
	}
	else
	{
		check_count = 0;
	}
#endif
	printk(KERN_INFO"%s: prev:%d, curr:%d, count:%d\n",__func__,prev_dock_state,state,check_count);

	prev_dock_state = state;

	if(check_count == 5)
	{
		hall_ic_dock_report_event(state);
		check_count = 0;
	}
	else
	{
		mod_timer(&dev->timer, jiffies + (first_slide_timeout_ms * HZ / 1000));
	}
}

static int hall_ic_dock_irq_handler(int irq, void *dev_id)
{
	struct hall_ic_dock_device *dev = dev_id;
	short gpio_carkit, gpio_multikit;
	SDBG("\n\nCheck point : %s\n", __FUNCTION__);

	gpio_carkit = gpio_get_value(GPIO_CARKIT_IRQ);
	gpio_multikit = gpio_get_value(GPIO_MULTIKIT_IRQ);

	if(gpio_carkit == GPIO_CARKIT_DETECT || gpio_multikit == GPIO_MULTIKIT_DETECT)
		atomic_set(&current_state, 0);
	else
		atomic_set(&current_state, 1);

	spin_lock_irq(&dev->lock);

	if(s_hallic_state == 1){
		SDBG("\n\nCheck point : disable irq : %s\n ", __FUNCTION__);
		disable_irq_nosync(s_hall_ic_carkit_gpio);
		disable_irq_nosync(s_hall_ic_multikit_gpio);
		enable_irq_on = 0;
		queue_work(hallic_dock_wq, &dev->work);
	}
  spin_unlock(&dev->lock);
	return IRQ_HANDLED;
}

static ssize_t hall_ic_dock_print_name(struct switch_dev *sdev, char *buf)
{
	switch (switch_get_state(sdev)) {
		case DOCK_STATE_UNDOCKED:
			return sprintf(buf, "UNDOCKED\n");
		case DOCK_STATE_DESK:
			return sprintf(buf, "DESK\n");
		case DOCK_STATE_CAR:
			return sprintf(buf, "CARKIT\n");
	}
	return -EINVAL;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void hall_early_suspend(struct early_suspend *h) 
{
	printk(KERN_INFO"%s: \n",__func__);
	suspend_flag = HALL_IC_EARLY_SUSPEND;

	return;
}

static void hall_late_resume(struct early_suspend *h)
{
	printk(KERN_INFO"%s: \n",__func__);	
	suspend_flag = HALL_IC_EARLY_RESUME;
	return;
}
#endif

static int hall_ic_dock_suspend(struct platform_device *pdev, pm_message_t state) 
{
	suspend_flag = HALL_IC_SUSPEND;
	return 0;
}

static int hall_ic_dock_resume(struct platform_device *pdev) 
{
	suspend_flag = HALL_IC_RESUME;
	return 0;
}

static ssize_t dock_mode_store(
		struct device *dev, struct device_attribute *attr, 
		const char *buf, size_t size)
{
	if (!strncmp(buf, "undock", size - 1)) 
		hall_ic_dock_report_event(DOCK_STATE_UNDOCKED);
	else if (!strncmp(buf, "desk", size - 1)) 
		hall_ic_dock_report_event(DOCK_STATE_DESK);
	else if (!strncmp(buf, "car", size -1))
		hall_ic_dock_report_event(DOCK_STATE_CAR);
	else
		return -EINVAL;

	return size;
}

static DEVICE_ATTR(report, S_IRUGO | S_IWUSR, NULL , dock_mode_store);

static int hall_ic_dock_probe(struct platform_device *pdev)
{
	struct hall_ic_dock_device *dev;
	int ret, err;

	dev = kzalloc(sizeof(struct hall_ic_dock_device), GFP_KERNEL);
	dock_dev = dev;
	spin_lock_init(&dev->lock);

	dev->sdev.name = "dock";
	dev->sdev.print_name = hall_ic_dock_print_name;

	ret = switch_dev_register(&dev->sdev);
	if (ret < 0)
		goto err_switch_dev_register;

	platform_set_drvdata(pdev, dev);

	INIT_WORK(&dev->work, hall_ic_dock_work_func);

	dev->use_irq = USE_IRQ;

	//s_hallic_state = 1;

	ret = gpio_request(GPIO_CARKIT_IRQ, "carkit_detect");
	if(ret < 0)
		goto err_request_carkit_detect_gpio;

	ret = gpio_request(GPIO_MULTIKIT_IRQ, "multikit_detect");
	if(ret < 0)
		goto err_request_multikit_detect_gpio;

	ret = gpio_direction_input(GPIO_CARKIT_IRQ);
	if (ret < 0)
		goto err_set_carkit_detect_gpio;

	ret = gpio_direction_input(GPIO_MULTIKIT_IRQ);
	if (ret < 0)
		goto err_set_multikit_detect_gpio;

	s_hall_ic_carkit_gpio = gpio_to_irq(GPIO_CARKIT_IRQ);

	ret = request_irq(s_hall_ic_carkit_gpio, hall_ic_dock_irq_handler,
			IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
			"hall-ic-carkit", dev); 

	if (ret) {
		printk("\nHALL IC CARKIT IRQ Check-fail\n pdev->client->irq %d\n",s_hall_ic_carkit_gpio);
		goto err_request_carkit_detect_irq;
	}

	err = set_irq_wake(s_hall_ic_carkit_gpio, 1);
	if (err) {
		pr_err("hall-ic-carkit: set_irq_wake failed for gpio %d, "
				"irq %d\n", GPIO_CARKIT_IRQ, s_hall_ic_carkit_gpio);
	}

	s_hall_ic_multikit_gpio = gpio_to_irq(GPIO_MULTIKIT_IRQ);
	ret = request_irq(s_hall_ic_multikit_gpio, hall_ic_dock_irq_handler,
			IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
			"hall-ic-multikit", dev); 

	if (ret) {
		printk("\nHALL IC MULTIKIT IRQ Check-fail\n pdev->client->irq %d\n",s_hall_ic_multikit_gpio);
		goto err_request_multikit_detect_irq;
	}

	err = set_irq_wake(s_hall_ic_multikit_gpio, 1);
	if (err) {
		pr_err("hall-ic-multikit: set_irq_wake failed for gpio %d, "
				"irq %d\n", GPIO_MULTIKIT_IRQ, s_hall_ic_multikit_gpio);
	}

	ret = device_create_file(&pdev->dev, &dev_attr_report);
	if (ret) {
		printk( "hall-ic-dock_probe: device create file Fail\n");
		device_remove_file(&pdev->dev, &dev_attr_report);
		goto err_request_irq;
	}

	enable_irq_on = 1;

	setup_timer(&dev->timer, hall_timer, (unsigned long)dev);

#ifdef CONFIG_HAS_EARLYSUSPEND
	hall_ic_early_suspend.suspend = hall_early_suspend;
	hall_ic_early_suspend.resume = hall_late_resume;
	hall_ic_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 45;
	register_early_suspend(&hall_ic_early_suspend);
#endif
	printk(KERN_ERR "hall_ic_dock: hall_ic_dock_probe: Done\n");
	s_hallic_state = 1;

	return 0;

err_request_irq:
	free_irq(s_hall_ic_multikit_gpio, 0);
err_request_multikit_detect_irq:
	free_irq(s_hall_ic_carkit_gpio, 0);
err_request_carkit_detect_irq:
err_set_multikit_detect_gpio:
err_set_carkit_detect_gpio:
	gpio_free(GPIO_MULTIKIT_IRQ);
err_request_multikit_detect_gpio:
	gpio_free(GPIO_CARKIT_IRQ);
err_request_carkit_detect_gpio:
	switch_dev_unregister(&dev->sdev);
	kfree(dev);
err_switch_dev_register:
	printk(KERN_ERR "hall_ic_dock: Failed to register driver\n");
	return ret;
}

static int hall_ic_dock_remove(struct platform_device *pdev) 
{
	struct hall_ic_dock_device *dev = dock_dev;

	del_timer_sync(&dev->timer);
	gpio_free(GPIO_CARKIT_IRQ);
	gpio_free(GPIO_MULTIKIT_IRQ);
	free_irq(s_hall_ic_carkit_gpio, 0);
	free_irq(s_hall_ic_multikit_gpio, 0);
	switch_dev_unregister(&dev->sdev);

	return 0;
}

static struct platform_driver hall_ic_dock_driver = {
	.probe		= hall_ic_dock_probe,
	.remove		= hall_ic_dock_remove,
	.suspend 	= hall_ic_dock_suspend,
	.resume		= hall_ic_dock_resume,
	.driver		= {
		.name		= "hall-ic-dock",
		.owner		= THIS_MODULE,
	},
};

static int __init hall_ic_dock_init(void)
{
	hallic_dock_wq = create_singlethread_workqueue("hallic_dock_wq");
	if (!hallic_dock_wq){
		SDBG("\n\n Check point2 : %s\n", __FUNCTION__);
		return -ENOMEM;
	}

	SDBG( "hall_ic_dock: init\n");
	return platform_driver_register(&hall_ic_dock_driver);
}

static void __exit hall_ic_dock_exit(void)
{
	SDBG( "hall_ic_dock: exit\n");
	destroy_workqueue(hallic_dock_wq);
	platform_driver_unregister(&hall_ic_dock_driver);
}

module_init(hall_ic_dock_init);
module_exit(hall_ic_dock_exit);

MODULE_LICENSE("GPL");
