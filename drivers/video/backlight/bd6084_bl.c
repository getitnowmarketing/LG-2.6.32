/* drivers/video/backlight/bd6084_bl.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/board_lge.h>

#define MODULE_NAME         "bd6084gu"
#define CONFIG_BACKLIGHT_LEDS_CLASS

#ifdef CONFIG_BACKLIGHT_LEDS_CLASS
#include <linux/leds.h>
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define CONFIG_BD6084GU_ALC_TUNING

/********************************************
 * Definition
 ********************************************/
#define LCD_LED_MAX			0x40		/* 13mA */
#define LCD_LED_NOR			0X31		/* 10mA */
#define LCD_LED_MIN			0x0			/* 2mA */
#define DEFAULT_BRIGHTNESS	LCD_LED_MAX
#define BD6084GU_LDO_NUM    4

#define BD6084GU_ALC_LOG_CURVE     0
#define BD6084GU_ALC_LINEAR_CURVE  1
#define BD6084GU_ALC_CURVE_DEFAULT BD6084GU_ALC_LOG_CURVE
#define BD6084GU_ALC_MIN_DEFAULT   0x1B /* 5.6mA */
#define BD6084GU_ALC_MAX_DEFAULT   0x38 /* 11.4mA */
#define BD6084GU_ALC_STEPUP_DEFAULT 0x06 /* 1.4mA */

#ifdef CONFIG_BACKLIGHT_LEDS_CLASS
#define LEDS_BACKLIGHT_NAME "lcd-backlight"
#endif

enum {
	ALC_MODE,
	NORMAL_MODE,
} BD6094GU_MODE;

enum {
	UNINIT_STATE=-1,
	POWERON_STATE,
	NORMAL_STATE,
	SLEEP_STATE,
	POWEROFF_STATE
} BD6094GU_STATE;

#define dprintk(fmt, args...) \
	do { \
		if(debug) \
			printk(KERN_INFO "%s:%s: " fmt, MODULE_NAME, __func__, ## args); \
	}while(0);

#define eprintk(fmt, args...)   printk(KERN_ERR "%s:%s: " fmt, MODULE_NAME, __func__, ## args)

struct ldo_vout_struct {
	unsigned char reg;
	unsigned vol;
};

struct bd6084gu_ctrl_tbl {
	unsigned char reg;
	unsigned char val;
};

struct bd6084gu_alc {
	int min;
	int max;
	int stepup;
	int curve;
};

struct bd6084gu_driver_data {
	struct i2c_client *client;
	struct backlight_device *bd;
	struct led_classdev *led;
	struct bd6084gu_alc alc;
	int gpio;
	int intensity;
	int max_intensity;
	int mode;
	int state;
	int init_on_boot;
	int ldo_ref[BD6084GU_LDO_NUM];
	unsigned char reg_ldo_enable;
	unsigned char reg_ldo_vout[2];
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

/********************************************
 * Global variable
 ********************************************/
static unsigned int debug = 0;
module_param(debug, uint, 0644);

//N-1) Power ON change to Normal mode
static struct bd6084gu_ctrl_tbl N_1[] = {
	{0x00, 0x01},	// RESET(All register Initializing)
	{0x01, 0x1E},	// WLED5,6,7 = MainLED, MainLED is normal mode, WPWMIN=OFF
//	{0x03, 0x63},	// (initial) LED current =20mA at normal mode
	{0x03, LCD_LED_MAX},	// (initial) LED current at normal mode
	{0x09, 0x00},	// TLH=THL=minmum setting
	{0x02, 0x01},	// MainLED ON at Normal mode
	{0xff, 0xfe}	// end of command
};

//N-7) Normal mode change to ALC mode
static struct bd6084gu_ctrl_tbl N_7[] = {
	{0x09, 0x87},	// TLH=32.77ms, THL=65.54ms
	{0x0A, 0x01},	// (initial) ADCYC=0.52s, Auto Gain Control,SBIASON=High, MDCIR=low
	{0x0B, 0x03},	// (initial) CRV=log curve, Step=1.1mA
	{0x0D, 0x13},	// (initial) ILED(min)=4mA
	{0x0E, 0x63},	// (initial) ILED(max)=20mA
	{0x01, 0x1F},	// WLED5,6,7 = MainLED, MainLED is ALC mode, WPWMIN=OFF
	{0x02, 0x41},	// Main LED=ON, ALCEN=ON
	{0xff, 0xfe}	// end of command
};

//N-7-R)ALC mode cahnge to Normal mode
static struct bd6084gu_ctrl_tbl N_7_R[] = {
	{0x01, 0x1E},	// WLED5,6,7 = MainLED, MainLED is normal mode, WPWMIN=OFF
	{0x09, 0x00},	// TLH=THL=minmum setting
	{0x02, 0x01},	// ALC=OFF (Main LED=ON)
	{0xff, 0xfe}	// end of command
};

#if 0
//N-2)Normal mode change to Dimming mode
static struct bd6084gu_ctrl_tbl N_2[] = {
	{0x03, 0x04},	// Main LED current = 1mA
	{0xff, 0xfe}	// end of command
};

//N-4)Dimming mode change to Sleep mode
static struct bd6084gu_ctrl_tbl N_4[] = {
	{0x02, 0x00},	// Main LED =OFF, ALC=OFF
	{0xff, 0xfe}	// end of command
};
#endif

//N-6)Sleep mode change to Normal mode
static struct bd6084gu_ctrl_tbl N_6[] = {
//	{0x03, 0x63},	// Main LED current = 20mA
	{0x02, 0x01},	// MainLED ON at Normal mode
	{0xff, 0xfe}	// end of command
};

#if 0
//N-3)Dimming mode change to Normal mode
static struct bd6084gu_ctrl_tbl N_3[] = {
	{0x03, 0x63},	// Main LED current = 20mA
	{0xff, 0xfe}	// end of command
};
#endif

//N-5-2) Nornmal mode change to sleep with slope down
static struct bd6084gu_ctrl_tbl N_5_2[] = {
	{0x09, 0x55},	// TLH=THL=8.192ms
	{0x03, 0x00},	// Main LED current = minimum
	{0xff, 0x0a},	// 10 x 100 delay
	{0x02, 0x00},	// Main LED =OFF
	{0xff, 0xfe}	// end of command
};

//Software reset
//w	76	00	01	// Software reset

#if 0
//A-1) Power ON at ALC mode
//************** initial settings *************************************
static struct bd6084gu_ctrl_tbl A_1[] = {
	{0x00, 0x01},	// RESET(All register Initializing)
	{0x01, 0x3F},	// WLED5,6,7,8 = MainLED, MainLED is ALC mode, WPWMIN=OFF
	{0x09, 0x87},	// TLH=32.77ms, THL=65.54ms
	{0x0A, 0x01},	// (initial) ADCYC=0.52s, Auto Gain Control,SBIASON=High, MDCIR=low
	{0x0B, 0x01},	// (initial) CRV=log curve, Step=1.1mA
	{0x0D, 0x13},	// (initial) ILED(min)=4mA
	{0x0E, 0x63},	// (initial) ILED(max)=20mA
//**********************************************************************
	{0x02, 0x41},	// MainLED ON at ALC mode (non-PWM)
	{0xff, 0xfe}	// end of command
};

//A-4) Dimming mode change to Sleep
//A-5-1) ALC mode change to Sleep
static struct bd6084gu_ctrl_tbl A_4[] = {
	{0X02, 0X00},	// Main LED =OFF, ALC=OFF
	{0xff, 0xfe}	// end of command
};

//A-2) ALC mode change to Dimming mode
static struct bd6084gu_ctrl_tbl A_2[] = {
	{0x03, 0x04},      // Main LED currrent =1mA(ch)
	{0x09, 0x00},	// TLH=THL=minmum setting
	{0x01, 0x3E},	// WLED5,6,7,8 = MainLED, MainLED is normal mode, WPWMIN=OFF
	{0xff, 0xfe}	// end of command
};

//A-3) Dimming mode change to ALC mode
//w	76	0A	01	// (initial) ADCYC=0.52s, Auto Gain Control,SBIASON=High, MDCIR=low
static struct bd6084gu_ctrl_tbl A_3[] = {
	{0x09, 0x87},	// TLH=32.77ms, THL=65.54ms
	{0x01, 0x3F},	// WLED5,6,7,8 = MainLED, MainLED is ALC mode, WPWMIN=OFF
	{0x02, 0x41},	// MainLED ON at ALC mode (non-PWM)
	{0xff, 0xfe}	// end of command
};

//A-7-R) Normal mode change to ALC mode
static struct bd6084gu_ctrl_tbl A_7_R[] = {
	{0x09, 0x87},	// TLH=32.77ms, THL=65.54ms
	{0x01, 0x3F},	// WLED5,6,7,8 = MainLED, MainLED is ALC mode, WPWMIN=OFF
	{0xff, 0xfe}	// end of command
};

#endif

#if 0
//A-7) ALC mode change  to Normal mode
static struct bd6084gu_ctrl_tbl A_7[] = {
	{0x01, 0x1E},	// WLED5,6,7 = MainLED, MainLED is normal mode, WPWMIN=OFF
	{0x03, LCD_LED_MAX /*0x63*/},	// (initial) LED current =20mA at normal mode
	{0x09, 0x00},	// TLH=THL=minmum setting
	{0xff, 0xfe}	// end of command
};
#endif


//A-6) Sleep mode change to ALC mode 
//w	76	0A	01	// (initial) ADCYC=0.52s, Auto Gain Control,SBIASON=High, MDCIR=low
static struct bd6084gu_ctrl_tbl A_6[] = {
	{0x0A, 0x01},   // (initial) ADCYC=0.52s, Auto Gain Control,SBIASON=High, MDCIR=low
	{0x09, 0x87},	// TLH=32.77ms, THL=65.54ms
	{0x01, 0x1F},	// WLED5,6,7 = MainLED, MainLED is ALC mode, WPWMIN=OFF
	{0x02, 0x41},	// MainLED ON at ALC mode (non-PWM)
	{0xff, 0xfe}	// end of command
};

//A-5-2) ALC mode change to sleep with slope down
static struct bd6084gu_ctrl_tbl A_5_2[] = {
	{0x03, 0x00},      // Main LED currrent =minimum
	{0x01, 0x1E},	// WLED5,6,7 = MainLED, MainLED is normal mode, WPWMIN=OFF
	{0x09, 0x55},	// TLH=THL=8.192ms
	{0xff, 0x0a},	// 10 x 100 delay
	{0x02, 0x00},	// Main LED =OFF, ALC=OFF
	{0xff, 0xfe}	// end of command
};

static struct ldo_vout_struct ldo_vout_table[] = {
        {/* 0000 */ 0x00, 1200},
        {/* 0001 */ 0x01, 1300},
        {/* 0010 */ 0x02, 1500},
        {/* 0011 */ 0x03, 1600},
        {/* 0100 */ 0x04, 1800},
        {/* 0101 */ 0x05, 2200},
        {/* 0110 */ 0x06, 2400},
        {/* 0111 */ 0x07, 2500},
        {/* 1000 */ 0x08, 2600},
        {/* 1001 */ 0x09, 2700},
        {/* 1010 */ 0x0A, 2800},
        {/* 1011 */ 0x0B, 2900},
        {/* 1100 */ 0x0C, 3000},
        {/* 1101 */ 0x0D, 3100},
        {/* 1110 */ 0x0E, 3200},
        {/* 1111 */ 0x0F, 3300},
        {/* Invalid */ 0xFF, 0},
};

/********************************************
 * Functions
 ********************************************/
static int bd6084gu_read(struct i2c_client *client, u8 reg, u8 *pval)
{
	int ret;
	int status;

	if (client == NULL) { 	/* No global client pointer? */
		eprintk("client is null\n");
		return -1;
	}

	if ((ret = i2c_smbus_read_byte_data(client, reg)) >= 0) {
		*pval = ret;
		status = 0;
	} else {
		status = -EIO;
		eprintk("fail to read(reg=0x%x,val=0x%x)\n", reg,*pval);
	}

	return status;
}

static int bd6084gu_write(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;
	int status;

	if (client == NULL) {		/* No global client pointer? */
		eprintk("client is null\n");
		return -1;
	}

	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret == 0) {
		status = 0;
	} else {
		status = -EIO;
		eprintk("fail to write(reg=0x%x,val=0x%x)\n", reg, val);
	}

	return status;
}

static int bd6084gu_set_ldos(struct i2c_client *i2c_dev, unsigned num, int enable)
{
	struct bd6084gu_driver_data *drvdata = i2c_get_clientdata(i2c_dev);

	if(drvdata) {
		if(enable) drvdata->reg_ldo_enable |= 1 << (num-1);
		else drvdata->reg_ldo_enable &= ~(1 << (num-1));
		
		dprintk("enable ldos, reg:0x13 value:0x%x\n", drvdata->reg_ldo_enable);
		
		return bd6084gu_write(i2c_dev, 0x13, drvdata->reg_ldo_enable);
	}
	return -EIO;
}

static unsigned char bd6084gu_ldo_get_vout_val(unsigned vol)
{
	int i = 0;
	do {
		if (ldo_vout_table[i].vol == vol)
			return ldo_vout_table[i].reg;
		else
			i++;
	} while (ldo_vout_table[i].vol != 0);

	return ldo_vout_table[i].reg;
}

static int bd6084gu_ldo_set_vout(struct i2c_client *i2c_dev, unsigned num, unsigned char val)
{
	struct bd6084gu_driver_data *drvdata = i2c_get_clientdata(i2c_dev);
	unsigned char *next_val;
	unsigned char reg;

	if(drvdata) {
		if(num <= 2) {
			reg = 0x14;
			next_val = &drvdata->reg_ldo_vout[0];
		} else {
			reg = 0x15;
			next_val = &drvdata->reg_ldo_vout[1];
		}
		if(num % 2) {
			*next_val &= 0xF0;
		}
		else {
			*next_val &= 0x0F;
			val = val << 4;
		}
		*next_val |= val;
		dprintk("target register[0x%x], value[0x%x]\n",	reg, *next_val);
		return bd6084gu_write(i2c_dev, reg, *next_val);
	}
	return -EIO;
}

/*******************************************************
 * Function: bd6084gu_ldo_set_level
 * Description: Set LDO vout level
 * Parameter
 *         num: ldo number and it is 1-based value
 *         level: voltage level
 *******************************************************/
int bd6084gu_ldo_enable(struct device *dev, unsigned num, unsigned enable)
{
	struct i2c_adapter *adap;
	struct i2c_client *client;
	struct bd6084gu_driver_data *drvdata;
	int err = 0;

	dprintk("ldo_no[%d], on/off[%d]\n",num, enable);

	if(num > 0 && num <= BD6084GU_LDO_NUM) {
		if((adap=dev_get_drvdata(dev)) && (client=i2c_get_adapdata(adap))) {
			drvdata = i2c_get_clientdata(client);
			if(enable) {
				if(drvdata->ldo_ref[num-1]++ == 0) {
					dprintk("ref count = 0, call bd6084gu_set_ldos\n");
					err = bd6084gu_set_ldos(client, num, enable);
				}
			}
			else {
				if(--drvdata->ldo_ref[num-1] == 0) {
					dprintk("ref count = 0, call bd6084gu_set_ldos\n");
					err = bd6084gu_set_ldos(client, num, enable);
				}
			}
			return err;
		}
	}
	return -ENODEV;
}
EXPORT_SYMBOL(bd6084gu_ldo_enable);

/*******************************************************
 * Function: bd6084gucp_ldo_set_level
 * Description: Set LDO vout level
 * Parameter
 *         num: ldo number and it is 1-based value
 *         level: voltage level
 *******************************************************/
int bd6084gu_ldo_set_level(struct device *dev, unsigned num, unsigned vol)
{
	struct i2c_adapter *adap;
	struct i2c_client *client;
	unsigned char val;

	dprintk("ldo_no[%d], level[%d]\n", num, vol);
	if(vol == 0) {
		dprintk("Do nothing on setting level 0.\n");
		return 0;
	}

	if(num > 0 && num <= BD6084GU_LDO_NUM) {
		if((adap=dev_get_drvdata(dev)) && (client=i2c_get_adapdata(adap))) {
			val = bd6084gu_ldo_get_vout_val(vol);
			if(val == 0xFF) {
				printk(KERN_INFO "%s: Invalid ldo level(%d).\n", __func__, vol);
				return -ENODEV;
			}
			dprintk("vout register value 0x%x for level %d\n", val, vol);
			return bd6084gu_ldo_set_vout(client, num, val);
		}
	}
	return -ENODEV;
}
EXPORT_SYMBOL(bd6084gu_ldo_set_level);

static int bd6084gu_set_table(struct bd6084gu_driver_data *drvdata, struct bd6084gu_ctrl_tbl *ptbl)
{
	unsigned int i = 0;

	if (ptbl == NULL) {
		eprintk("input ptr is null\n");
		return -EIO;
	}

	for( ;;) {
		if (ptbl->reg == 0xFF) {
			if (ptbl->val != 0xfe)
				udelay(ptbl->val);
			else
				break;
		}	
		else {
			if(bd6084gu_write(drvdata->client, ptbl->reg, ptbl->val) != 0)
				dprintk("i2c failed addr:%d, value:%d\n", ptbl->reg, ptbl->val);
		}
		ptbl++;
		i++;
	}
	return 0;
}

static void bd6084gu_hw_reset(struct bd6084gu_driver_data *drvdata)
{
	if(drvdata->client && gpio_is_valid(drvdata->gpio)) {
		gpio_configure(drvdata->gpio, GPIOF_DRIVE_OUTPUT);
		gpio_set_value(drvdata->gpio, 1);
		udelay(5);
		gpio_set_value(drvdata->gpio, 0);
		udelay(5);
		gpio_set_value(drvdata->gpio, 1);
		udelay(5);
	}
}

static void bd6084gu_go_alc_mode(struct bd6084gu_driver_data *drvdata)
{
	bd6084gu_write(drvdata->client, 0x09, 0x87);
	bd6084gu_write(drvdata->client, 0x0A, 0x01);
	bd6084gu_write(drvdata->client, 0x0B, (drvdata->alc.curve << 16) | drvdata->alc.stepup);
	bd6084gu_write(drvdata->client, 0x0D, drvdata->alc.min);
	bd6084gu_write(drvdata->client, 0x0E, drvdata->alc.max);
	bd6084gu_write(drvdata->client, 0x01, 0x1F);
	bd6084gu_write(drvdata->client, 0x02, 0x41);
}

/* This function is reset software of a BD6091GU chip */
/*
static void bd6084gu_sw_rst(void)
{
	dprintk("SW Reset\n");

	mutex_lock(&control_lock);

	bd6084gu_write(0x00, 0x01);
	bd6084gu_write(0x01, 0x0E);
	bd6084gu_write(0x02, 0x01);
	bd6084gu_write(0x03, 0xFF);
	mutex_unlock(&control_lock);
}
*/

static void bd6084gu_go_opmode(struct bd6084gu_driver_data *drvdata)
{
	dprintk("operation mode is %s\n", (drvdata->mode == NORMAL_MODE) ? "normal_mode" : "alc_mode");
	
	switch (drvdata->mode) {
		case NORMAL_MODE:
			bd6084gu_set_table(drvdata, &N_1[0]);
			drvdata->state = NORMAL_STATE;
			break;
		case ALC_MODE:
			bd6084gu_go_alc_mode(drvdata);
			drvdata->state = NORMAL_STATE;
			break;
		default:
			eprintk("Invalid Mode\n");
			break;
	}
}

static void bd6084gu_init(struct bd6084gu_driver_data *drvdata)
{
	if(drvdata->init_on_boot || system_state != SYSTEM_BOOTING) {
		bd6084gu_hw_reset(drvdata);
		bd6084gu_go_opmode(drvdata);
	}
	drvdata->state = NORMAL_STATE;
}


/* This function provide BD6091GU sleep enter routine for power management. */
static void bd6084gu_sleep(struct bd6084gu_driver_data *drvdata)
{
	if (!drvdata || drvdata->state == SLEEP_STATE)
		return;

	dprintk("operation mode is %s\n", (drvdata->mode == NORMAL_MODE) ? "normal_mode" : "alc_mode");
	
	switch (drvdata->mode) {
		case NORMAL_MODE:
			drvdata->state = SLEEP_STATE;
			bd6084gu_set_table(drvdata, &N_5_2[0]);
			break;

		case ALC_MODE:
			drvdata->state = SLEEP_STATE;
			bd6084gu_set_table(drvdata, &A_5_2[0]);
			udelay(500);
			break;

		default:
			eprintk("Invalid Mode\n");
			break;
	}
}

static void bd6084gu_wakeup(struct bd6084gu_driver_data *drvdata)
{
	if (!drvdata || drvdata->state == NORMAL_STATE)
		return;

	dprintk("operation mode is %s\n", (drvdata->mode == NORMAL_MODE) ? "normal_mode" : "alc_mode");

	if (drvdata->state == POWEROFF_STATE) {
		bd6084gu_go_opmode(drvdata);
	}
	else if(drvdata->state == SLEEP_STATE) {
		bd6084gu_write(drvdata->client, 0x03, drvdata->intensity);
		if(drvdata->mode == NORMAL_MODE) {
			bd6084gu_set_table(drvdata, &N_6[0]);
			drvdata->state = NORMAL_STATE;
		}
		else if(drvdata->mode == ALC_MODE) {
			bd6084gu_set_table(drvdata, &A_6[0]);
			drvdata->state = NORMAL_STATE;
		}
	}

}

static void bd6084gu_poweron(struct bd6084gu_driver_data *drvdata)
{
	if (!drvdata || drvdata->state != POWEROFF_STATE)
		return;
	
	dprintk("POWER ON \n");

	bd6084gu_init(drvdata);
	
	if (drvdata->mode == NORMAL_MODE)
		bd6084gu_write(drvdata->client, 0x03, drvdata->intensity);
}

static void bd6084gu_poweroff(struct bd6084gu_driver_data *drvdata)
{
	if (!drvdata || drvdata->state == POWEROFF_STATE)
		return;

	dprintk("POWER OFF \n");

	if (drvdata->state == SLEEP_STATE) {
		gpio_direction_output(drvdata->gpio, 0);
		msleep(6);
		return;
	}
		
	bd6084gu_write(drvdata->client, 0x00, 0x01);	// sofrware reset
	gpio_tlmm_config(GPIO_CFG(drvdata->gpio, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),GPIO_ENABLE);
	gpio_direction_output(drvdata->gpio, 0);
	mdelay(6);

	drvdata->state = POWEROFF_STATE;
}

static int bd6084gu_send_intensity(struct bd6084gu_driver_data *drvdata, int next)
{
	if(drvdata->mode == NORMAL_MODE) {
		if(next > drvdata->max_intensity)
			next = drvdata->max_intensity;
		if(next < LCD_LED_MIN)
			next = LCD_LED_MIN;
		dprintk("next current is %d\n", next);

		if(drvdata->state == NORMAL_STATE && drvdata->intensity != next)
			bd6084gu_write(drvdata->client, 0x03, next);
		
		drvdata->intensity = next;
	}
	else {
		dprintk("A manual setting for intensity is only permitted in normal mode\n");
	}

	return 0;
}

static int bd6084gu_get_intensity(struct bd6084gu_driver_data *drvdata)
{
	return drvdata->intensity;
}

static int bd6084gu_get_alc_level(struct bd6084gu_driver_data *drvdata)
{
	u8 level = 0;

	if(drvdata->mode == ALC_MODE) {
		if(bd6084gu_read(drvdata->client, 0x0C, &level))
			eprintk("Error while read register(0x13).\n");
		dprintk("alc level: %d\n", level);
	}
	else {
		eprintk("Current mode is not ALC mode\n");
	}

	return level;
}

#ifdef CONFIG_PM
#ifdef CONFIG_HAS_EARLYSUSPEND
static void bd6084gu_early_suspend(struct early_suspend * h)
{	
	struct bd6084gu_driver_data *drvdata = container_of(h, struct bd6084gu_driver_data,
						    early_suspend);

	dprintk("start\n");
	bd6084gu_sleep(drvdata);

	return;
}

static void bd6084gu_late_resume(struct early_suspend * h)
{	
	struct bd6084gu_driver_data *drvdata = container_of(h, struct bd6084gu_driver_data,
						    early_suspend);

	dprintk("start\n");
	bd6084gu_wakeup(drvdata);

	return;
}
#else
static int bd6084gu_suspend(struct i2c_client *i2c_dev, pm_message_t state)
{
	struct bd6084gu_driver_data *drvdata = i2c_get_clientdata(i2c_dev);
	bd6084gu_sleep(drvdata);
	return 0;
}

static int bd6084gu_resume(struct i2c_client *i2c_dev)
{
	struct bd6084gu_driver_data *drvdata = i2c_get_clientdata(i2c_dev);
	bd6084gu_wakeup(drvdata);
	return 0;
}
#endif	/* CONFIG_HAS_EARLYSUSPEND */
#else
#define bd6084gu_suspend	NULL
#define bd6084gu_resume	NULL
#endif	/* CONFIG_PM */

void bd6084gu_switch_mode(struct device *dev, int next_mode)
{
	struct bd6084gu_driver_data *drvdata = dev_get_drvdata(dev);

	if(!drvdata || drvdata->mode == next_mode)
		return;

	if(next_mode == ALC_MODE)
		bd6084gu_go_alc_mode(drvdata);
	else if(next_mode == NORMAL_MODE) {
		bd6084gu_set_table(drvdata, &N_7_R[0]);
		bd6084gu_write(drvdata->client, 0x03, drvdata->intensity);
	}
	else {
		printk(KERN_ERR "%s: invalid mode(%d)!!!\n", __func__, next_mode);
		return;
	}

	drvdata->mode = next_mode;
	return;
}

ssize_t bd6084gu_show_alc(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bd6084gu_driver_data *drvdata = dev_get_drvdata(dev->parent);
	int r;

	if(!drvdata) return 0;

	r = snprintf(buf, PAGE_SIZE, "%s\n", (drvdata->mode == ALC_MODE) ? "1":"0");
	
	return r;
}

ssize_t bd6084gu_store_alc(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int alc;
	int next_mode;

	if (!count)
		return -EINVAL;

	sscanf(buf, "%d", &alc);

	if(alc)
		next_mode = ALC_MODE;
	else
		next_mode = NORMAL_MODE;

	bd6084gu_switch_mode(dev->parent, next_mode);

	return count;
}

ssize_t bd6084gu_show_reg(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bd6084gu_driver_data *drvdata = dev_get_drvdata(dev);
	int len = 0;
	unsigned char val;

	len += snprintf(buf,       PAGE_SIZE,       "\nBD6091GU Registers is following..\n");
	bd6084gu_read(drvdata->client, 0x01, &val);
	len += snprintf(buf + len, PAGE_SIZE - len, "[0x01] = 0x%x\n", val);
	bd6084gu_read(drvdata->client, 0x03, &val);
	len += snprintf(buf + len, PAGE_SIZE - len, "[0x03] = 0x%x\n", val);	
	bd6084gu_read(drvdata->client, 0x0B, &val);
	len += snprintf(buf + len, PAGE_SIZE - len, "[0x0B] = 0x%x\n", val);		
	bd6084gu_read(drvdata->client, 0x0D, &val);
	len += snprintf(buf + len, PAGE_SIZE - len, "[0x0D] = 0x%x\n", val);

	return len;
}

ssize_t bd6084gu_show_drvstat(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bd6084gu_driver_data *drvdata = dev_get_drvdata(dev->parent);
	int len = 0;

	len += snprintf(buf,       PAGE_SIZE,       "\nBD6091GU Backlight Driver Status is following..\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "mode                   = %3d\n", drvdata->mode);
	len += snprintf(buf + len, PAGE_SIZE - len, "state                  = %3d\n", drvdata->state);
	len += snprintf(buf + len, PAGE_SIZE - len, "current intensity      = %3d\n", drvdata->intensity);

	return len;
}

ssize_t bd6084gu_show_alc_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bd6084gu_driver_data *drvdata = dev_get_drvdata(dev->parent);
	int alc_level;
	int r;

	if(!drvdata) return 0;

	alc_level = bd6084gu_get_alc_level(drvdata);
	r = snprintf(buf, 40, "%d\n", alc_level);

	return r;
}

ssize_t bd6084gu_show_max_intensity(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct bd6084gu_driver_data *drvdata = dev_get_drvdata(dev->parent);
	int r;

	if(!drvdata) return 0;

	r = snprintf(buf, PAGE_SIZE, "%d\n", drvdata->max_intensity);
	
	return r;
}

ssize_t bd6084gu_store_max_intensity(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int maxval;
	struct bd6084gu_driver_data *drvdata = dev_get_drvdata(dev->parent);

	if (!count)
		return -EINVAL;

	sscanf(buf, "%d", &maxval);
	drvdata->max_intensity = maxval;

	bd6084gu_send_intensity(drvdata, drvdata->max_intensity);

	return count;
}

#ifdef CONFIG_BD6084GU_ALC_TUNING
ssize_t bd6084gu_store_alc_min(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct bd6084gu_driver_data *drvdata = dev_get_drvdata(dev->parent);
	int min;

	sscanf(buf, "%d", &min);
	drvdata->alc.min = min;
	if(drvdata->mode == ALC_MODE)
		bd6084gu_go_alc_mode(drvdata);

	return count;
}

ssize_t bd6084gu_store_alc_max(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct bd6084gu_driver_data *drvdata = dev_get_drvdata(dev->parent);
	int max;

	sscanf(buf, "%d", &max);
	drvdata->alc.max = max;
	if(drvdata->mode == ALC_MODE)
		bd6084gu_go_alc_mode(drvdata);

	return count;
}

ssize_t bd6084gu_store_alc_stepup(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct bd6084gu_driver_data *drvdata = dev_get_drvdata(dev->parent);
	int stepup;

	sscanf(buf, "%d", &stepup);
	drvdata->alc.stepup = stepup;
	if(drvdata->mode == ALC_MODE)
		bd6084gu_go_alc_mode(drvdata);

	return count;
}

ssize_t bd6084gu_store_alc_curve(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct bd6084gu_driver_data *drvdata = dev_get_drvdata(dev->parent);
	int curve;

	sscanf(buf, "%d", &curve);
	drvdata->alc.curve = curve;
	if(drvdata->mode == ALC_MODE)
		bd6084gu_go_alc_mode(drvdata);

	return count;
}
#endif

DEVICE_ATTR(alc,    0664, bd6084gu_show_alc,    bd6084gu_store_alc);
DEVICE_ATTR(reg,     0444, bd6084gu_show_reg,     NULL);
DEVICE_ATTR(drvstat, 0444, bd6084gu_show_drvstat, NULL);
DEVICE_ATTR(alc_level, 0444, bd6084gu_show_alc_level, NULL);
DEVICE_ATTR(max_intensity, 0664, bd6084gu_show_max_intensity, bd6084gu_store_max_intensity);

#ifdef CONFIG_BD6084GU_ALC_TUNING
DEVICE_ATTR(alc_min, 0664, NULL, bd6084gu_store_alc_min);
DEVICE_ATTR(alc_max, 0664, NULL, bd6084gu_store_alc_max);
DEVICE_ATTR(alc_stepup, 0664, NULL, bd6084gu_store_alc_stepup);
DEVICE_ATTR(alc_curve, 0664, NULL, bd6084gu_store_alc_curve);
#endif

static int bd6084gu_set_brightness(struct backlight_device *bd)
{
	struct bd6084gu_driver_data *drvdata = dev_get_drvdata(bd->dev.parent);
	return bd6084gu_send_intensity(drvdata, bd->props.brightness);
}

static int bd6084gu_get_brightness(struct backlight_device *bd)
{
	struct bd6084gu_driver_data *drvdata = dev_get_drvdata(bd->dev.parent);
	return bd6084gu_get_intensity(drvdata);
}

static struct backlight_ops bd6084gu_ops = {
	.get_brightness = bd6084gu_get_brightness,
	.update_status  = bd6084gu_set_brightness,
};


#ifdef CONFIG_BACKLIGHT_LEDS_CLASS
static void leds_brightness_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	struct bd6084gu_driver_data *drvdata = dev_get_drvdata(led_cdev->dev->parent);
	int brightness;
	int next;

	if (!drvdata) {
		eprintk("Error getting drvier data\n");
		return;
	}

	brightness = bd6084gu_get_intensity(drvdata);

	next = value * drvdata->max_intensity / LED_FULL;
	dprintk("input brightness value=%d]\n", next);

	if (brightness != next) {
		dprintk("brightness[current=%d, next=%d]\n", brightness, next);
		bd6084gu_send_intensity(drvdata, next);
	}
}

static struct led_classdev bd6084gu_led_dev = {
	.name = LEDS_BACKLIGHT_NAME,
	.brightness_set = leds_brightness_set,
};
#endif

static int __init bd6084gu_probe(struct i2c_client *i2c_dev, const struct i2c_device_id *i2c_dev_id)
{
	struct backlight_platform_data *pdata;
	struct bd6084gu_driver_data *drvdata;
	struct backlight_device *bd;
	int err;

	dprintk("start, client addr=0x%x\n", i2c_dev->addr);

	drvdata = kzalloc(sizeof(struct bd6084gu_driver_data), GFP_KERNEL);
	if (!drvdata) {
		dev_err(&i2c_dev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	pdata = i2c_dev->dev.platform_data;

	if(pdata && pdata->platform_init)
		pdata->platform_init();

	drvdata->client = i2c_dev;
	drvdata->gpio = pdata->gpio;
	drvdata->init_on_boot = pdata->init_on_boot;
	drvdata->max_intensity = LCD_LED_MAX;
	if(pdata->max_current > 0)
		drvdata->max_intensity = pdata->max_current;
	drvdata->intensity = LCD_LED_MIN;
	drvdata->mode = NORMAL_MODE;
	drvdata->state = UNINIT_STATE;
	drvdata->alc.min = BD6084GU_ALC_MIN_DEFAULT;
	drvdata->alc.max = BD6084GU_ALC_MAX_DEFAULT;
	drvdata->alc.curve = BD6084GU_ALC_CURVE_DEFAULT;
	drvdata->alc.stepup = BD6084GU_ALC_STEPUP_DEFAULT;

	if(drvdata->gpio && gpio_request(drvdata->gpio, "bd6084 reset") != 0) {
		eprintk("Error while requesting gpio %d\n", drvdata->gpio);
		kfree(drvdata);
		return -ENODEV;
	}

	bd = backlight_device_register("bd6084gu-bl", &i2c_dev->dev, NULL, &bd6084gu_ops);
	if (bd == NULL) {
		eprintk("entering bd6084gu_bl probe function error \n");
		if(gpio_is_valid(drvdata->gpio))
			gpio_free(drvdata->gpio);
		kfree(drvdata);
		return -1;
	}
	bd->props.power = FB_BLANK_UNBLANK;
	bd->props.brightness = drvdata->intensity;
	bd->props.max_brightness = drvdata->max_intensity;
	drvdata->bd = bd;

#ifdef CONFIG_BACKLIGHT_LEDS_CLASS
	if(led_classdev_register(&i2c_dev->dev, &bd6084gu_led_dev) == 0) {
		eprintk("Registering led class dev successfully.\n");
		drvdata->led = &bd6084gu_led_dev;
		err = device_create_file(drvdata->led->dev, &dev_attr_alc);
		err = device_create_file(drvdata->led->dev, &dev_attr_reg);
		err = device_create_file(drvdata->led->dev, &dev_attr_drvstat);
		err = device_create_file(drvdata->led->dev, &dev_attr_alc_level);
		err = device_create_file(drvdata->led->dev, &dev_attr_max_intensity);
#ifdef CONFIG_BD6084GU_ALC_TUNING
		err = device_create_file(drvdata->led->dev, &dev_attr_alc_min);
		err = device_create_file(drvdata->led->dev, &dev_attr_alc_max);
		err = device_create_file(drvdata->led->dev, &dev_attr_alc_curve);
		err = device_create_file(drvdata->led->dev, &dev_attr_alc_stepup);
#endif
	}
#endif

	i2c_set_clientdata(i2c_dev, drvdata);
	i2c_set_adapdata(i2c_dev->adapter, i2c_dev);

	bd6084gu_init(drvdata);
	bd6084gu_send_intensity(drvdata, LCD_LED_MAX);

#ifdef CONFIG_HAS_EARLYSUSPEND
	drvdata->early_suspend.suspend = bd6084gu_early_suspend;
	drvdata->early_suspend.resume = bd6084gu_late_resume;
	drvdata->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 40;
	register_early_suspend(&drvdata->early_suspend);
#endif

	eprintk("done\n");
	return 0;
}

static int __devexit bd6084gu_remove(struct i2c_client *i2c_dev)
{
	struct bd6084gu_driver_data *drvdata = i2c_get_clientdata(i2c_dev);

	bd6084gu_send_intensity(drvdata, 0);

	backlight_device_unregister(drvdata->bd);
	led_classdev_unregister(drvdata->led);
	i2c_set_clientdata(i2c_dev, NULL);
	if (gpio_is_valid(drvdata->gpio))
		gpio_free(drvdata->gpio);
	kfree(drvdata);

	return 0;
}

static struct i2c_device_id bd6084gu_idtable[] = {
        { MODULE_NAME, 0 },
};

MODULE_DEVICE_TABLE(i2c, bd6084gu_idtable);

static struct i2c_driver bd6084gu_driver = {
	.probe 		= bd6084gu_probe,
	.remove 	= bd6084gu_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend 	= bd6084gu_suspend,
	.resume 	= bd6084gu_resume,
#endif
	.id_table 	= bd6084gu_idtable,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init bd6084gu_drv_init(void)
{
	printk("BD6084GU init start\n");
	return i2c_add_driver(&bd6084gu_driver);
}
module_init(bd6084gu_drv_init);

static void __exit bd6084gu_drv_exit(void)
{
	i2c_del_driver(&bd6084gu_driver);
}
module_exit(bd6084gu_drv_exit);

MODULE_DESCRIPTION("Backlight driver and charge pump for BD6084GU");
MODULE_LICENSE("GPL");

