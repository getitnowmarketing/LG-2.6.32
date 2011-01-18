/* arch/arm/mach-msm/board-alohav-input.c
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
#include <linux/gpio_event.h>
#include <linux/keyreset.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <mach/board.h>
#include <mach/board_lge.h>
#include <mach/rpc_server_handset.h>

#include "board-alohav.h"

/* head set device */
static struct msm_handset_platform_data hs_platform_data = {
	.hs_name = "7k_handset",
	.pwr_key_delay_ms = 500, /* 0 will disable end key */
};

static struct platform_device hs_device = {
	.name   = "msm-handset",
	.id     = -1,
	.dev    = {
		.platform_data = &hs_platform_data,
	},
};

/* pp2106 qwerty keypad device */
static unsigned short pp2106_keycode[PP2106_KEYPAD_ROW][PP2106_KEYPAD_COL] =
{
	/*0*/	/*1*/	/*2*/	/*3*/			/*4*/			/*5*/				/*6*/			/*7*/
	{KEY_1,	KEY_2,	KEY_3,	KEY_4,			KEY_5,			KEY_FOLDER_HOME,	KEY_FOLDER_MENU,		KEY_BACK},
	{KEY_7,	KEY_8,	KEY_9,	KEY_0,			KEY_BACKSPACE,	KEY_HOME,			KEY_MENU,		KEY_6},
	{KEY_Q,	KEY_W,	KEY_E,	KEY_R,			KEY_T,			KEY_SEND,			KEY_RESERVED,	KEY_SEARCH},
	{KEY_U,	KEY_I,	KEY_O,	KEY_P,			KEY_RESERVED,	KEY_CAMERAFOCUS,	KEY_CAMERA,		KEY_Y},
	{KEY_A,	KEY_S,	KEY_D,	KEY_F,			KEY_G,			KEY_RESERVED,		KEY_RESERVED,	KEY_LEFTALT},
	{KEY_J,	KEY_K,	KEY_L,	KEY_ENTER,		KEY_RESERVED,	KEY_RIGHT,			KEY_DOWN,		KEY_H},
	{KEY_Z,	KEY_X,	KEY_C,	KEY_V,			KEY_RESERVED, 	KEY_VOLUMEUP,		KEY_VOLUMEDOWN,	KEY_LEFTSHIFT},
	{KEY_B,	KEY_N,	KEY_M,	KEY_DOT,	KEY_LEFT,		KEY_UP,				KEY_REPLY,			KEY_SPACE},
};

static struct pp2106_platform_data pp2106_pdata = {
	.keypad_row = PP2106_KEYPAD_ROW,
	.keypad_col = PP2106_KEYPAD_COL,
	.keycode = (unsigned char *)pp2106_keycode,
	.reset_pin = GPIO_PP2106_RESET,
	.irq_pin = GPIO_PP2106_IRQ,
	.sda_pin = GPIO_PP2106_SDA,
	.scl_pin = GPIO_PP2106_SCL,
};

static struct platform_device qwerty_device = {
	.name = "kbd_pp2106",
	.id = -1,
	.dev = {
		.platform_data = &pp2106_pdata,
	},
};

/* hall ic device */
static struct gpio_event_direct_entry alohav_slide_switch_map[] = {
	    { GPIO_HALLIC_IRQ,          SW_LID          },
};

static int alohav_gpio_slide_input_func(struct gpio_event_input_devs *input_dev,
		struct gpio_event_info *info, void **data, int func)
{
	if (func == GPIO_EVENT_FUNC_INIT) {
		gpio_tlmm_config(GPIO_CFG(GPIO_HALLIC_IRQ, 0, GPIO_INPUT, GPIO_PULL_UP,
					GPIO_2MA), GPIO_ENABLE);
	}
	return gpio_event_input_func(input_dev, info, data, func);
}
static int alohav_gpio_slide_power(
		const struct gpio_event_platform_data *pdata, bool on)
{
	return 0;
}

static struct gpio_event_input_info alohav_slide_switch_info = {
	.info.func = alohav_gpio_slide_input_func,
	.debounce_time.tv64 = 0,
	.flags = 0,
	.type = EV_SW,
	.keymap = alohav_slide_switch_map,
	.keymap_size = ARRAY_SIZE(alohav_slide_switch_map)
};

static struct gpio_event_info *alohav_gpio_slide_info[] = {
	&alohav_slide_switch_info.info,
};

static struct gpio_event_platform_data alohav_gpio_slide_data = {
	.name = "gpio-slide-detect",
	.info = alohav_gpio_slide_info,
	.info_count = ARRAY_SIZE(alohav_gpio_slide_info),
	.power = alohav_gpio_slide_power,
};

static struct platform_device alohav_gpio_slide_device = {
	.name = GPIO_EVENT_DEV_NAME,
	.id = 0,
	.dev        = {
		.platform_data  = &alohav_gpio_slide_data,
	},
};

static struct platform_device hallic_dock_device = {
	.name   = "hall-ic-dock",
	.id = -1,
	.dev = {
		.platform_data = NULL,
	},
};

/* keyreset platform device */
static int alohav_reset_keys_up[] = {
	KEY_HOME,
	0
};

static struct keyreset_platform_data alohav_reset_keys_pdata = {
	.keys_up = alohav_reset_keys_up,
	.keys_down = {
		KEY_LEFTALT,
		KEY_LEFTSHIFT,
		KEY_MENU,
		0
	},
};

struct platform_device alohav_reset_keys_device = {
	.name = KEYRESET_NAME,
	.dev.platform_data = &alohav_reset_keys_pdata,
};

/* input platform device */
static struct platform_device *alohav_input_devices[] __initdata = {
	&hs_device,
	&qwerty_device,
	&alohav_gpio_slide_device,
	&hallic_dock_device,
#ifdef LG_BLOCK_KERNEL_MISC
	// block if the TARGET_BUILD_VARIANT is user
#else
	//&alohav_reset_keys_device,
#endif
};

/* MCS6000 Touch */
static struct gpio_i2c_pin ts_i2c_pin[] = {
	[0] = {
		.sda_pin	= TS_GPIO_I2C_SDA,
		.scl_pin	= TS_GPIO_I2C_SCL,
		.reset_pin	= 0,		
		.irq_pin	= TS_GPIO_IRQ,
	},
};

static struct i2c_gpio_platform_data ts_i2c_pdata = {
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.udelay				= 2,
};

static struct platform_device ts_i2c_device = {
	.name	= "i2c-gpio",
	.dev.platform_data = &ts_i2c_pdata,
};

static int ts_set_vreg(unsigned char onoff)
{
	struct vreg *vreg_touch;
	int rc;

	printk("[Touch] %s() onoff:%d\n",__FUNCTION__, onoff);

	vreg_touch = vreg_get(0, "ruim");

	if(IS_ERR(vreg_touch)) {
		printk("[Touch] vreg_get fail : touch");
		return -1;
	}

	if (onoff) {
		rc = vreg_set_level(vreg_touch, 3050);
		if (rc != 0) {
			printk("[Touch] vreg_set_level failed\n");
			return -1;
		}
		vreg_enable(vreg_touch);
	} else
		vreg_disable(vreg_touch);

	return 0;	
}

static struct touch_platform_data ts_pdata = {
	.ts_x_min = TS_X_MIN,
	.ts_x_max = TS_X_MAX,
	.ts_y_min = TS_Y_MIN,
	.ts_y_max = TS_Y_MAX,
	.power = ts_set_vreg,
	.irq 	  = TS_GPIO_IRQ,
	.scl      = TS_GPIO_I2C_SCL,
	.sda      = TS_GPIO_I2C_SDA,
};

static struct i2c_board_info ts_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("touch_mcs6000", TS_I2C_SLAVE_ADDR),
		.type = "touch_mcs6000",
		.platform_data = &ts_pdata,
	},
};

static void __init alohav_init_i2c_touch(int bus_num)
{
	ts_i2c_device.id = bus_num;

	init_gpio_i2c_pin(&ts_i2c_pdata, ts_i2c_pin[0],	&ts_i2c_bdinfo[0]);
	i2c_register_board_info(bus_num, &ts_i2c_bdinfo[0], 1);
	platform_device_register(&ts_i2c_device);
}

/* gp2ap proximity sensor */
static struct gpio_i2c_pin proxi_i2c_pin[] = {
	[0] = {
		.sda_pin	= PROXI_GPIO_I2C_SDA,
		.scl_pin	= PROXI_GPIO_I2C_SCL,
		.reset_pin	= 0,		
		.irq_pin	= PROXI_GPIO_DOUT,
	},
};

static struct i2c_gpio_platform_data proxi_i2c_pdata = {
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.udelay = 2,
};

static struct platform_device proxi_i2c_device = {
	.name = "i2c-gpio",
	.dev.platform_data = &proxi_i2c_pdata,
};

static int prox_power_set(unsigned char onoff)
{
	int ret = 0;
	struct device *dev = alohav_bd6084_dev();

	if (onoff) {
		if (bd6084gu_ldo_set_level(dev, PROXI_LDO_NO_VCC, 2800))
			printk(KERN_ERR " ldo %d control error\n", PROXI_LDO_NO_VCC);

		if (bd6084gu_ldo_enable(dev, PROXI_LDO_NO_VCC, 1))
			printk(KERN_ERR "ldo %d control error\n", PROXI_LDO_NO_VCC);	
	} else {
		if (bd6084gu_ldo_set_level(dev, PROXI_LDO_NO_VCC, 0))
			printk(KERN_ERR "ldo %d control error\n", PROXI_LDO_NO_VCC);

		if (bd6084gu_ldo_enable(dev, PROXI_LDO_NO_VCC, 0))
			printk(KERN_ERR "ldo %d control error\n", PROXI_LDO_NO_VCC);
	}

	return ret;
}

static struct proximity_platform_data proxi_pdata = {
	.irq_num	= PROXI_GPIO_DOUT,
	.power		= prox_power_set,
	.methods		= 0,
	.operation_mode		= 0,
	.debounce	 = 0,
	.cycle = 2,
};

static struct i2c_board_info proxi_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("proximity_gp2ap", PROXI_I2C_ADDRESS),
		.type = "proximity_gp2ap",
		.platform_data = &proxi_pdata,
	},
};

static void __init alohav_init_i2c_proximity(int bus_num)
{
	proxi_i2c_device.id = bus_num;

	init_gpio_i2c_pin(&proxi_i2c_pdata, proxi_i2c_pin[0], &proxi_i2c_bdinfo[0]);

	i2c_register_board_info(bus_num, &proxi_i2c_bdinfo[0], 1);
	platform_device_register(&proxi_i2c_device);
}

static int accel_power_set(unsigned char onoff)
{
	int ret = 0;
	return ret;
}

static struct acceleration_platform_data accel_pdata = {
	.power		= accel_power_set,
};

static int kr3dh_config_gpio(int config)
{
	if (config) {	// for wake state
		gpio_tlmm_config(GPIO_CFG(ACCEL_GPIO_I2C_SCL, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
		gpio_tlmm_config(GPIO_CFG(ACCEL_GPIO_I2C_SDA, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
	} else {		// for sleep state
		gpio_tlmm_config(GPIO_CFG(ACCEL_GPIO_I2C_SCL, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
		gpio_tlmm_config(GPIO_CFG(ACCEL_GPIO_I2C_SDA, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
	}

	return 0;
}
static int kr_init(void){return 0;}
static void kr_exit(void){}
static int power_on(void){return 0;}
static int power_off(void){return 0;}

struct kr3dh_platform_data kr3dh_data = {
	.poll_interval = 100,
	.min_interval = 0,
	.g_range = 0x00,
	.axis_map_x = 0,
	.axis_map_y = 1,
	.axis_map_z = 2,

	.negate_x = 0,
	.negate_y = 0,
	.negate_z = 0,

	.power_on = power_on,
	.power_off = power_off,
	.kr_init = kr_init,
	.kr_exit = kr_exit,
	.gpio_config = kr3dh_config_gpio,
};

static struct i2c_board_info accel_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("KR3DH", ACCEL_I2C_ADDRESS),
		.type = "KR3DH",
		.platform_data = &kr3dh_data,
	}
};

/* ecompass */
static struct gpio_i2c_pin ecom_i2c_pin[] = {
	[0] = {
		.sda_pin        = ACCEL_GPIO_I2C_SDA,
		.scl_pin        = ACCEL_GPIO_I2C_SCL,
		.reset_pin      = ACCEL_GPIO_CPAS_RST,
		.irq_pin        = ACCEL_GPIO_CPAS_BUSY,
	},
};

static struct i2c_gpio_platform_data ecom_i2c_pdata = {
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.udelay = 2,
};

static struct platform_device ecom_i2c_device = {
	.name = "i2c-gpio",
	.dev.platform_data = &ecom_i2c_pdata,
};

static int ecom_power_set(unsigned char onoff)
{
	struct vreg *vreg_compass;
	int err;

	vreg_compass = vreg_get(0, "synt");

	if (onoff) {
		vreg_enable(vreg_compass);

		err = vreg_set_level(vreg_compass, 3000);
		if (err != 0) {
			printk("vreg_compass failed.\n");
			return -1;
		}
	} 
	else {
		vreg_disable(vreg_compass);
	}

	return 0;
}

static s16 m_hlayout[2][9] ={
	{1, 0, 0, 0, 1, 0, 0, 0, 1},
	{0, 1, 0, -1, 0, 0, 0, 0, 1}
};

static s16 m_alayout[2][9] = {
	{1, 0, 0, 0, 1, 0, 0, 0, 1},
	{0, -1, 0, 1, 0, 0, 0, 0, 1}
};

static struct ecom_platform_data ecom_pdata = {
        .pin_int        = ACCEL_GPIO_CPAS_BUSY,
        .pin_rst        = ACCEL_GPIO_CPAS_RST,
        .power          = ecom_power_set,
        .accelerator_name = "KR3DH",
        .fdata_sign_x = -1,
        .fdata_sign_y = 1,
        .fdata_sign_z = -1,
        .fdata_order0 = 0,
        .fdata_order1 = 1,
        .fdata_order2 = 2,
        .sensitivity1g = 1024,
        .h_layout = m_hlayout,
        .a_layout = m_alayout,
};

static struct i2c_board_info ecom_i2c_bdinfo[] = {
	[0] = {
		I2C_BOARD_INFO("akm8973", ECOM_I2C_ADDRESS),
		.type = "akm8973",
		.platform_data = &ecom_pdata,
	}
};

static void __init alohav_init_i2c_ecompass(int bus_num)
{
	ecom_i2c_device.id = bus_num;

	init_gpio_i2c_pin(&ecom_i2c_pdata, ecom_i2c_pin[0], &ecom_i2c_bdinfo[0]);

	i2c_register_board_info(bus_num, &ecom_i2c_bdinfo[0], 1);
	i2c_register_board_info(bus_num, &accel_i2c_bdinfo[0], 1);
	platform_device_register(&ecom_i2c_device);
}

/* common function */
void __init lge_add_input_devices(void)
{
	platform_add_devices(alohav_input_devices, ARRAY_SIZE(alohav_input_devices));

	lge_add_gpio_i2c_device(alohav_init_i2c_touch);
	lge_add_gpio_i2c_device(alohav_init_i2c_proximity);
	lge_add_gpio_i2c_device(alohav_init_i2c_ecompass);
}
