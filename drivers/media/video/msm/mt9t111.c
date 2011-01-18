/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include "mt9t111.h"

//-------------------------------------------
// [START] camera sensor tuning test code insup.choi@lge.com
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/fcntl.h> 
static u_int16_t start_flash_control = 0;
static u_int8_t valid_gain_control = 0;
//-------------------------------------------

static int sensor_mode_before = -1;
static int scene_before_status = -1;

/* Micron MT9T111 Registers and their values */
/* Sensor Core Registers */

struct mt9t111_work {
	struct work_struct work;
};

static struct  mt9t111_work *mt9t111_sensorw;
static struct  i2c_client *mt9t111_client;

struct mt9t111_ctrl {
	const struct msm_camera_sensor_info *sensordata;
};


static struct mt9t111_ctrl *mt9t111_ctrl;

static DECLARE_WAIT_QUEUE_HEAD(mt9t111_wait_queue);
DECLARE_MUTEX(mt9t111_sem);

int msm_camera_mclk_value[]=
{
	36000000,
	36000000,
	24000000,	// 2 
	24576000,	// 3
	25600000,	// 4
	28000000,	// 5
	29900000,	// 6
	31000000,	// 7
	32000000,	// 8
	48000000,	// 9
};

int msm_camera_pclk_value[]=
{
	72000000,	// 0
	48000000,	// 1
};

/*=============================================================
	EXTERNAL DECLARATIONS
==============================================================*/
extern struct mt9t111_reg mt9t111_regs;

#define LG_CAMERA_HIDDEN_MENU

#ifdef LG_CAMERA_HIDDEN_MENU
extern bool sensorAlwaysOnTest;
#endif

/*=============================================================*/

/* START add recording mode setting */
static struct platform_device *mt9t111_dev;
static int recording_mode = 0;
/* END add recording mode setting */

static int mt9t111_reset(const struct msm_camera_sensor_info *dev)
{
	int rc = 0;

//	sensor_mode_before = -1;

	rc = gpio_request(dev->sensor_reset, "mt9t111");

	if (!rc) {
		rc = gpio_direction_output(dev->sensor_reset, 0);
		msleep(20);
		rc = gpio_direction_output(dev->sensor_reset, 1);
		msleep(20);
	}

	gpio_free(dev->sensor_reset);
	return rc;
}

static int mt9t111_reset_init(const struct msm_camera_sensor_info *dev)
{
	int rc = 0;

	rc = gpio_request(dev->sensor_reset, "mt9t111");

	if (!rc) {
		rc = gpio_direction_output(dev->sensor_reset, 0);
		msleep(20);
		rc = gpio_direction_output(dev->sensor_reset, 1);
		msleep(20);
		rc = gpio_direction_output(dev->sensor_reset, 0);
		msleep(20);
	}

	gpio_free(dev->sensor_reset);
	return rc;
}

static int32_t mt9t111_i2c_txdata(unsigned short saddr,
	unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		},
	};

	if (i2c_transfer(mt9t111_client->adapter, msg, 1) < 0) {
		CDBG("mt9t111_i2c_txdata failed\n");
		return -EIO;
	}

	return 0;
}

static int32_t mt9t111_i2c_write(unsigned short saddr,
	unsigned short waddr, unsigned short wdata, enum mt9t111_width width)
{
	int32_t rc = -EIO;
	unsigned char buf[4];

	memset(buf, 0, sizeof(buf));
	
	CDBG("LG_FW_mt9t111_i2c_write, addr = 0x%x, val = 0x%x!\n", waddr, wdata);
	switch (width) {
	case WORD_LEN: {
		buf[0] = (waddr & 0xFF00)>>8;
		buf[1] = (waddr & 0x00FF);
		buf[2] = (wdata & 0xFF00)>>8;
		buf[3] = (wdata & 0x00FF);

		rc = mt9t111_i2c_txdata(saddr, buf, 4);
	}
		break;

	case BYTE_LEN: {
		buf[0] = waddr;
		buf[1] = wdata;
		rc = mt9t111_i2c_txdata(saddr, buf, 2);
	}
		break;

	default:
		break;
	}

	if (rc < 0)
		CDBG(
		"i2c_write failed, addr = 0x%x, val = 0x%x!\n",
		waddr, wdata);

	return rc;
}

static int mt9t111_i2c_rxdata(unsigned short saddr,
	unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
	{
		.addr   = saddr,
		.flags = 0,
		.len   = 2,
		.buf   = rxdata,
	},
	{
		.addr   = saddr,
		.flags = I2C_M_RD,
		.len   = length,
		.buf   = rxdata,
	},
	};

	if (i2c_transfer(mt9t111_client->adapter, msgs, 2) < 0) {
		CDBG("mt9t111_i2c_rxdata failed!\n");
		return -EIO;
	}

	return 0;
}

static int32_t mt9t111_i2c_read(unsigned short   saddr,
	unsigned short raddr, unsigned short *rdata, enum mt9t111_width width)
{
	int32_t rc = 0;
	unsigned char buf[4];

	if (!rdata)
		return -EIO;

	memset(buf, 0, sizeof(buf));

	switch (width) {
	case WORD_LEN: {
		buf[0] = (raddr & 0xFF00)>>8;
		buf[1] = (raddr & 0x00FF);

		rc = mt9t111_i2c_rxdata(saddr, buf, 2);
		if (rc < 0)
			return rc;

		*rdata = buf[0] << 8 | buf[1];
	}
		break;

	default:
		break;
	}

	if (rc < 0)
		CDBG("mt9t111_i2c_read failed!\n");

	return rc;
}

static int mt9t111_poll_wait(u_int16_t mcu_addr, u_int16_t readAddr , u_int16_t vToBe, u_int16_t total_wait_time, u_int16_t polling_period)
{
	 u_int16_t cnt=0;
	 u_int16_t max_cnt=0;
	 u_int16_t vCur=0;

	 max_cnt = total_wait_time/polling_period;
	
	for(cnt = 0; cnt < max_cnt; cnt++)
	{
		msleep(polling_period);

		if(mcu_addr != 0)
		{
			if(mt9t111_i2c_write(mt9t111_client->addr, 0x098E, mcu_addr, WORD_LEN))
				CDBG("LG_FW_CAM: Camsensor I2C Fail. (%x, %x) \n", 0x098E, mcu_addr); 
		}

		if(!mt9t111_i2c_read(mt9t111_client->addr, readAddr, &vCur, WORD_LEN))
		{
			if(vCur == vToBe)//polling value ?? ???ٸ??? ???̸? break 
			{
				return 0; //break;
			}
		}
		else
			CDBG("LG_FW_CAM: Camsensor I2C read Fail. (%x :  %x) \n", readAddr, vCur); 
	}
	return -1;
}

//-------------------------------------------
// [START] camera sensor tuning test code insup.choi@lge.com
#ifdef CONFIG_MSM_CAMERA_TUNING
#define TO_BE_READ_SIZE 4000*8	// 8pages (4096x8)
#define IS_NUM(c) ((0x30<=c)&&(c<=0x39))
#define IS_CHAR(c) ((0x41<=c)&&(c<=0x46))
#define IS_VALID(c) (IS_NUM(c)||IS_CHAR(c))	// NUM or CHAR
#define TO_BE_NUM_OFFSET(c)  (IS_NUM(c) ? 0x30:0x37)	
char *file_buf_alloc_pages=NULL;
char *file_buf_alloc_pages_free=NULL;

#endif
//-------------------------------------------

static long mt9t111_reg_init(void)
{
	int16_t i=0;

//-------------------------------------------
// [START] camera sensor tuning test code insup.choi@lge.com
#ifdef CONFIG_MSM_CAMERA_TUNING
	{
		uint16_t value=0, read_idx=0, j=0;
		struct file *phMscd_Filp = NULL;
		mm_segment_t old_fs=get_fs();

		phMscd_Filp = filp_open("/data/mt9t111.txt", O_RDWR |O_LARGEFILE, 0);
			
		if (IS_ERR(phMscd_Filp) || !phMscd_Filp)
			goto use_compiled_value;

		file_buf_alloc_pages = kmalloc(TO_BE_READ_SIZE, GFP_KERNEL);
		file_buf_alloc_pages_free = file_buf_alloc_pages;
		
		if(!file_buf_alloc_pages)
			goto use_compiled_value;

		set_fs(get_ds());
		phMscd_Filp->f_op->read(phMscd_Filp, file_buf_alloc_pages, TO_BE_READ_SIZE-1, &phMscd_Filp->f_pos);
		CDBG("LG_FW_%s  :  %d sensor setting was read from sd card, first sensor setting is %s \n", __func__, (int)&phMscd_Filp->f_pos, file_buf_alloc_pages); 
		set_fs(old_fs);

		do
		{
			if(IS_VALID(file_buf_alloc_pages[read_idx]))
			{
				value = ((file_buf_alloc_pages[read_idx]-TO_BE_NUM_OFFSET(file_buf_alloc_pages[read_idx]))*0x1000 \
						+ (file_buf_alloc_pages[read_idx+1]-TO_BE_NUM_OFFSET(file_buf_alloc_pages[read_idx+1]))*0x100 \
						+ (file_buf_alloc_pages[read_idx+2]-TO_BE_NUM_OFFSET(file_buf_alloc_pages[read_idx+2]))*0x10 \
						+ (file_buf_alloc_pages[read_idx+3]-TO_BE_NUM_OFFSET(file_buf_alloc_pages[read_idx+3])));

				if(i<=j)
					mt9t111_regs.prev_init[i++].register_address = value;
				else
					mt9t111_regs.prev_init[j++].register_value = value;

				read_idx+=4;
			}
			else
				++read_idx;
		}while(i==0 || ((mt9t111_regs.prev_init[i-1].register_address!=0xfffe)||(mt9t111_regs.prev_init[i-1].register_value!=0xfffe)));


// 	free_pages((unsigned long)file_buf_alloc_pages_free, 3);
		kfree(file_buf_alloc_pages_free);
		filp_close(phMscd_Filp,NULL);
	}
	
	use_compiled_value:
#endif
//-------------------------------------------

	for(i=0; i<TO_BE_READ_SIZE/10;i++)
	{
		if( mt9t111_regs.prev_init[i].register_address== 0xFFFE && mt9t111_regs.prev_init[i].register_value == 0xFFFE)
			break;
		if( mt9t111_regs.prev_init[i].register_address== 0xFFFF)
		{
			if(mt9t111_regs.prev_init[i].register_value == 0xFFFF)
			{
				i++;
				if(mt9t111_regs.prev_init[i].register_address == 0x098E)
				{
					mt9t111_poll_wait(mt9t111_regs.prev_init[i].register_value, mt9t111_regs.prev_init[i+1].register_address, mt9t111_regs.prev_init[i+1].register_value, 500, 5);
				i++;
				}
				else
				{				 
					mt9t111_poll_wait(0, mt9t111_regs.prev_init[i].register_address, mt9t111_regs.prev_init[i].register_value, 500, 5);
				}
			}
			else
			{
				msleep(mt9t111_regs.prev_init[i].register_value);
			}
		}
		else	
		{
			if(mt9t111_i2c_write(mt9t111_client->addr,mt9t111_regs.prev_init[i].register_address, mt9t111_regs.prev_init[i].register_value, WORD_LEN))
			{					 
				CDBG("LG_FW_CAM: Camsensor I2C Failed. (%x, %x), i=%d \n", mt9t111_regs.prev_init[i].register_address, mt9t111_regs.prev_init[i].register_value, i); 
				return -1;
			}
		}
	}
	
	CDBG("LG_FW_CAM: Camsensor preview I2C success. \n"); 

	return 0;
}



static long mt9t111_reg_write_group(struct register_address_value_pair *group_reg,
        uint16_t group_reg_size)
{
	int16_t i=0;

	for(i=0; i<group_reg_size;i++)
	{
		if( group_reg[i].register_address == 0xFFFE &&
                        mt9t111_regs.prev_init[i].register_value == 0xFFFE) {
			CDBG("LG_FW_CAM: EOF");
				break;
		}
		if( group_reg[i].register_address == 0xFFFF)
		{
			if(group_reg[i].register_value == 0xFFFF)
			{
				i++;
				if(group_reg[i].register_address == 0x098E)
				{
					mt9t111_poll_wait(
                                                group_reg[i].register_value,
                                                group_reg[i+1].register_address,
                                                group_reg[i+1].register_value, 500, 5);
				i++;
				}
				else
				{				 
					mt9t111_poll_wait(0,
                                                group_reg[i+1].register_address,
                                                group_reg[i+1].register_value, 500, 5);
				}
			}
			else
			{
				msleep(group_reg[i+1].register_value);
			}
		}
		else	
		{
			if(mt9t111_i2c_write(mt9t111_client->addr,
                                group_reg[i].register_address, group_reg[i].register_value, WORD_LEN))
			{					 
				CDBG("LG_FW_CAM: Camsensor I2C Failed. (%x, %x), i=%d \n",
                                        group_reg[i].register_address, group_reg[i].register_value, i); 
				return -1;
			}
		}
	}
	
	CDBG("LG_FW_CAM: Camsensor preview I2C success. \n"); 

	return 0;
}

static int retry = 0;

static long mt9t111_set_sensor_mode(int mode, int usedelay)
{
	long rc = 0;
	/* START add recording mode setting */
	if ((recording_mode==1) && (mode == SENSOR_PREVIEW_MODE)) 
		mode = SENSOR_VIDEO_RECORDING_MODE;
	else if ((recording_mode==2) && (mode == SENSOR_PREVIEW_MODE))
		mode = SENSOR_VIDEO_MMS_RECORDING_MODE;
	/* END add recording mode setting */

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		CDBG("preview\n");
		if((sensor_mode_before==SENSOR_VIDEO_RECORDING_MODE)||(sensor_mode_before==SENSOR_VIDEO_MMS_RECORDING_MODE))
		{
			CDBG("%s  mode %d  sensor_mode_before %d \n",__func__, mode, sensor_mode_before);

			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x481D, WORD_LEN);				//Base Frame Lines (A)
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0363, WORD_LEN);				//      = 833
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x4825, WORD_LEN);				//Line Length (A)
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0CB1, WORD_LEN);				//      = 3383
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC8A5, WORD_LEN);				//search_f1_50
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0020, WORD_LEN);				//      = 32
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC8A6, WORD_LEN);				//search_f2_50
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0023, WORD_LEN);				//      = 35
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC8A7, WORD_LEN);				//search_f1_60
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0026, WORD_LEN);				//      = 38
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC8A8, WORD_LEN);				//search_f2_60
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0029, WORD_LEN);				//      = 41
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC844, WORD_LEN);				//period_50Hz (A)
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x00A1, WORD_LEN);				//      = 160
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC92F, WORD_LEN);				//period_50Hz (A MSB)
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);				//      = 0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC845, WORD_LEN);				//period_60Hz (A)
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0086, WORD_LEN);				//      = 133
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC92D, WORD_LEN);				//period_60Hz (A MSB)
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);				//      = 0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6815, WORD_LEN);				// MCU_ADDRESS	[PRI_A_CONFIG_FD_MAX_FDZONE_50HZ]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000D, WORD_LEN);				// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6817, WORD_LEN);				// MCU_ADDRESS	[PRI_A_CONFIG_FD_MAX_FDZONE_60HZ]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000D, WORD_LEN);
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xA80E, WORD_LEN); 		// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_60HZ]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0008, WORD_LEN); 				// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE81F, WORD_LEN);				// MCU_ADDRESS	[PRI_A_CONFIG_AE_RULE_BASE_TARGET]
//			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0047, WORD_LEN);			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0049, WORD_LEN);			// MCU_DATA_0

			//[Refresh]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN);				// MCU_ADDRESS [SEQ_CMD]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN);				// MCU_DATA_0
			if(mt9t111_poll_wait(0x8400,0x0990, 0x0000, 500, 10))
				CDBG("LG_FW_CAM: Camsensor preview mode fail. \n"); 				
		}

		if(sensor_mode_before==SENSOR_SNAPSHOT_MODE)
		{
	//=================================================================================
// awb range return
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x2c05, WORD_LEN);// MCU_ADDRESS [AF_STATUS] 		
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0002, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac38, WORD_LEN);// MCU_ADDRESS [AF_STATUS] 		
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x003c, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac39, WORD_LEN);// MCU_ADDRESS [AF_STATUS] 		
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0085, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3a, WORD_LEN);// MCU_ADDRESS [AF_STATUS] 		
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0034, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3b, WORD_LEN);// MCU_ADDRESS [AF_STATUS] 		
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0085, WORD_LEN);// MCU_DATA_0
//=================================================================================
		}
		
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC05, WORD_LEN);		// MCU_ADDRESS [PRI_B_NUM_OF_FRAMES_RUN]
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0005, WORD_LEN);	// MCU_DATA_0
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN);	// MCU_ADDRESS [SEQ_CMD]
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0001, WORD_LEN);	// MCU_DATA_0
		if(rc)
		{
			CDBG("LG_FW_CAM: Camsensor preview write fail. \n"); 
			return rc;
		}
		
//		if(!mt9t111_poll_wait(0x8400,0x0990, 0x0000, 500, 5))
		if(!mt9t111_poll_wait(0x8401,0x0990, 0x0003, 500, 5))
		{
			sensor_mode_before = mode;
			return 0;
		}
		else
		{
			CDBG("LG_FW_CAM: Camsensor preview status fail. \n"); 
			return -1;
		}
			
		break;

	case SENSOR_SNAPSHOT_MODE:
		CDBG("LG_FW_SNAPSHOT_MODE\n");
		retry = 0;

		while (retry < 3) 
		{
			printk("retry num = %d\n", retry);
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC05, WORD_LEN);		// MCU_ADDRESS [PRI_B_NUM_OF_FRAMES_RUN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN);	// MCU_ADDRESS [SEQ_CMD]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0002, WORD_LEN);	// MCU_DATA_0
			if(rc)
			{
				CDBG("LG_FW_CAM: Camsensor capture write fail. \n"); 
				return rc;
			}
		if(!mt9t111_poll_wait(0x8401, 0x0990, 0x0007, 500, 30))
		{
			sensor_mode_before = mode;
			return 0;
		}
			else
			{				
				++retry;
				CDBG("LG_FW_CAM: Camsensor capture status fail. Retry %d!!!\n", retry);
			}
		}
		return -1;
			
		break;

	case SENSOR_VIDEO_RECORDING_MODE:
		CDBG("recording\n");
//		[Video Mode - Fixed 22fps]
		rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x481D, WORD_LEN); 		// MCU_ADDRESS //Base Frame Lines (A)
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x03C9, WORD_LEN);			// 	  = 969
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x4825, WORD_LEN);			//Line Length (A)
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x07AC, WORD_LEN);			// 	  = 1964
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC8A5, WORD_LEN);			//search_f1_50
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0024, WORD_LEN);			// 	  = 36
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC8A6, WORD_LEN);			//search_f2_50
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0026, WORD_LEN);			// 	  = 38
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC8A7, WORD_LEN);			//search_f1_60
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x002B, WORD_LEN);			// 	  = 43
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC8A8, WORD_LEN);			//search_f2_60
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x002D, WORD_LEN);			// 	  = 45
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC844, WORD_LEN);			//period_50Hz (A)
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000B, WORD_LEN);			// 	  = 11
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC92F, WORD_LEN);			//period_50Hz (A MSB)
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0001, WORD_LEN);			// 	  = 1
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC845, WORD_LEN);			//period_60Hz (A)
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x00DE, WORD_LEN);			// 	  = 222

		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC92D, WORD_LEN);			//period_60Hz (A MSB)
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);			// 	  = 0
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6815, WORD_LEN);			//Max FD Zone 50 Hz
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0004, WORD_LEN);			// 	  = 4
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6817, WORD_LEN);			//Max FD Zone 60 Hz
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0004, WORD_LEN);			// 	  = 4

		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xA80E, WORD_LEN);			// MCU_ADDRESS [AE_TRACK_GATE]
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0008, WORD_LEN);			// MCU_DATA_0

		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE81F, WORD_LEN);			// MCU_ADDRESS [PRI_A_CONFIG_AE_RULE_BASE_TARGET]
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0049, WORD_LEN);			// MCU_DATA_0
		
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN);			// MCU_ADDRESS [SEQ_CMD]
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN);			// MCU_DATA_0

		if(rc)
		{
			CDBG("LG_FW_CAM: Camsensor capture write fail. \n"); 
			return rc;
		}
		if(!mt9t111_poll_wait(0x8400,0x0990, 0x0000, 500, 10))
		{
			sensor_mode_before = mode;
			return 0;
		}
		else
		{
			CDBG("LG_FW_CAM: Camsensor preview status fail. \n"); 
			return -1;
		}
		break;

	case SENSOR_VIDEO_MMS_RECORDING_MODE:
		CDBG("mms recording\n");

//		[Video Mode - Fixed 15fps]
		rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x481D, WORD_LEN);			// MCU_ADDRESS	//Base Frame Lines (A)
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x06F1, WORD_LEN); 		// MCU_DATA_0	// 	= 865
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x4825, WORD_LEN);			// MCU_ADDRESS	//Line Length (A)
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x07AC, WORD_LEN);			// MCU_DATA_0	//		= 3984
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC8A5, WORD_LEN);			// MCU_ADDRESS	//search_f1_50
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0024, WORD_LEN);			// MCU_DATA_0	//		= 32
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC8A6, WORD_LEN);			// MCU_ADDRESS	//search_f2_50
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0026, WORD_LEN);			// MCU_DATA_0	//		= 35
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC8A7, WORD_LEN);			// MCU_ADDRESS	//search_f1_60
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x002B, WORD_LEN);			// MCU_DATA_0	//		= 38
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC8A8, WORD_LEN);			// MCU_ADDRESS	//search_f2_60
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x002D, WORD_LEN);			// MCU_DATA_0	//		= 41
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC844, WORD_LEN);			// MCU_ADDRESS	//period_50Hz (A)
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000B, WORD_LEN);			// MCU_DATA_0	//		= 161
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC92F, WORD_LEN);			// MCU_ADDRESS	//period_60Hz (A)
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0001, WORD_LEN); 		// MCU_DATA_0	// 	= 134
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC845, WORD_LEN);			// MCU_ADDRESS	//period_60Hz (A)
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x00DE, WORD_LEN); 		// MCU_DATA_0	// 	= 134
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC92D, WORD_LEN); 		// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_50HZ]
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN); 		// MCU_DATA_0
 		
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6815, WORD_LEN); 		// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_50HZ]
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); 		// MCU_DATA_0
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6817, WORD_LEN); 		// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_60HZ]
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); 		// MCU_DATA_0
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xA80E, WORD_LEN); 		// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_50HZ]
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0018, WORD_LEN); 		// MCU_DATA_0                                   
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE81F, WORD_LEN); 		// MCU_ADDRESS [PRI_A_CONFIG_AE_RULE_BASE_TARGET]
//		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x001E, WORD_LEN); 		// MCU_DATA_0
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0024, WORD_LEN); 		// MCU_DATA_0

		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN);			// MCU_ADDRESS	[SEQ_CMD]
		rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN);			// MCU_DATA_0

		if(rc)
		{
			CDBG("LG_FW_CAM: Camsensor capture write fail. \n"); 
			return rc;
		}
		if(!mt9t111_poll_wait(0x8400,0x0990, 0x0000, 500, 10))
		{
			sensor_mode_before = mode;
			return 0;
		}
		else
		{
			CDBG("LG_FW_CAM: Camsensor preview status fail. \n"); 
			return -1;
		}
			
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static long mt9t111_set_effect(int effect, int usedelay)
{
	long rc = 0;
	static long before_status = -1;
	CDBG("%s  effect %d \n",__func__, effect);

	switch (before_status) 
	{
		case CAMERA_EFFECT_WHITEBOARD:
		case CAMERA_EFFECT_BLACKBOARD:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xc948, WORD_LEN);// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x3210, 0x01B8, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x326A, 0x1208, WORD_LEN);// MCU_DATA_0
			break;
		case CAMERA_EFFECT_POSTERIZE: 
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE883, WORD_LEN);// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC83, WORD_LEN);// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x3210, 0x01B8, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE884, WORD_LEN); 	// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN); 	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC84, WORD_LEN); 	// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN); 	// MCU_DATA_0
			break;
	}
	
	switch (effect) 
	{
		case CAMERA_EFFECT_OFF: 
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE883, WORD_LEN);// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC83, WORD_LEN);// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x3210, 0x01B8, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE884, WORD_LEN); 	// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN); 	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC84, WORD_LEN); 	// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN); 	// MCU_DATA_0
			break;
		
		case CAMERA_EFFECT_MONO: 
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE883, WORD_LEN); 	// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0001, WORD_LEN); 	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC83, WORD_LEN); 	// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0001, WORD_LEN); 	// MCU_DATA_0
			break;		

		case CAMERA_EFFECT_NEGATIVE:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE883, WORD_LEN); 	// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0003, WORD_LEN); 	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC83, WORD_LEN); 	// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0003, WORD_LEN); 	// MCU_DATA_0
			break;
		
		case CAMERA_EFFECT_SOLARIZE:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE883, WORD_LEN); 	// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0004, WORD_LEN); 	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC83, WORD_LEN); 	// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0004, WORD_LEN); 	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE884, WORD_LEN); 	// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN); 	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC84, WORD_LEN); 	// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN); 	// MCU_DATA_0
			break;		
			
		case CAMERA_EFFECT_SEPIA:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE883, WORD_LEN); 	// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0002, WORD_LEN); 	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC83, WORD_LEN); 	// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0002, WORD_LEN); 	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE885, WORD_LEN); 	// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SEPIA_CR]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000A, WORD_LEN); 	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC85, WORD_LEN); 	// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SEPIA_CR]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000A, WORD_LEN); 	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE886, WORD_LEN); 	// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SEPIA_CB]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x00E4, WORD_LEN); 	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC86, WORD_LEN); 	// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SEPIA_CB]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x00E4, WORD_LEN); 	// MCU_DATA_0
			break;
		
		case CAMERA_EFFECT_AQUA:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE883, WORD_LEN); 	// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0002, WORD_LEN); 	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC83, WORD_LEN); 	// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0002, WORD_LEN); 	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE885, WORD_LEN); 	// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SEPIA_CR]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0081, WORD_LEN); 	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC85, WORD_LEN); 	// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SEPIA_CR]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0081, WORD_LEN); 	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE886, WORD_LEN); 	// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SEPIA_CB]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x002F, WORD_LEN); 	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC86, WORD_LEN); 	// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SEPIA_CB]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x002F, WORD_LEN); 	// MCU_DATA_0
			break;

		case CAMERA_EFFECT_POSTERIZE:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE883, WORD_LEN);		  // MCU_ADDRESS [CAM1_LL_NR_START_0]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0004, WORD_LEN);			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC83, WORD_LEN);		  // MCU_ADDRESS [CAM1_LL_NR_START_1]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0004, WORD_LEN);			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE884, WORD_LEN);		  // MCU_ADDRESS [CAM1_LL_NR_START_2]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x00FF, WORD_LEN);			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC84, WORD_LEN);		  // MCU_ADDRESS [CAM1_LL_NR_START_3]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x00FF, WORD_LEN);			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x326A, 0x2400, WORD_LEN);		  // MCU_ADDRESS [CAM1_LL_LL_STOP_2]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x3210, 0x05B8, WORD_LEN);			// MCU_DATA_0
			break;

		case CAMERA_EFFECT_WHITEBOARD:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE883, WORD_LEN);		  // MCU_ADDRESS [CAM1_LL_NR_START_0]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0003, WORD_LEN);			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC83, WORD_LEN);		  // MCU_ADDRESS [CAM1_LL_NR_START_1]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0003, WORD_LEN);			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC948, WORD_LEN);		  // MCU_ADDRESS [CAM1_LL_NR_START_2]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x3210, 0x05B8, WORD_LEN);		  // MCU_ADDRESS [CAM1_LL_NR_START_3]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x326A, 0x2F03, WORD_LEN);			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN);		  // MCU_ADDRESS [CAM1_LL_LL_STOP_2]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x9900, 0x0006, WORD_LEN);			// MCU_DATA_0
			break;

		case CAMERA_EFFECT_BLACKBOARD:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE883, WORD_LEN);		  // MCU_ADDRESS [CAM1_LL_NR_START_0]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0001, WORD_LEN);			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC83, WORD_LEN);		  // MCU_ADDRESS [CAM1_LL_NR_START_1]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0001, WORD_LEN);			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC948, WORD_LEN);		  // MCU_ADDRESS [CAM1_LL_NR_START_2]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x3210, 0x05B8, WORD_LEN);		  // MCU_ADDRESS [CAM1_LL_NR_START_3]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x326A, 0x2F03, WORD_LEN);			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN);		  // MCU_ADDRESS [CAM1_LL_LL_STOP_2]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x9900, 0x0006, WORD_LEN);			// MCU_DATA_0
			break;
	}

	if (usedelay) {
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN);  	// MCU_ADDRESS [SEQ_CMD]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN);  	// MCU_DATA_0
	
			rc |= mt9t111_poll_wait(0x8400, 0x0990, 0x0000, 1000, 10);
	}
	
	if(rc)
		CDBG("%s   effect %d \n",__func__, effect);
	else
		before_status = effect;
		
	return rc;
}

static long mt9t111_set_wb(int wb, int usedelay)
{
	long rc = 0;
	CDBG("%s  wb %d \n",__func__, wb);

	switch (wb) 
	{
		default:
		case CAMERA_YUV_WB_AUTO:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xAC02, WORD_LEN);		// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000A, WORD_LEN);		// AWB_MODE	by LGIT
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x683F, WORD_LEN);		// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x01D9, WORD_LEN);		// AWB_R_RATIO_PRE_AWB
			break;
		
		case CAMERA_YUV_WB_INCANDESCENT:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xAC02, WORD_LEN);		// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);		// AWB_MODE
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x2C03, WORD_LEN);		// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);		// AWB_R_RATIO_PRE_AWB
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xAC3C, WORD_LEN);		// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0059, WORD_LEN);		// AWB_R_RATIO_PRE_AWB
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xAC3D, WORD_LEN);		// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x003C, WORD_LEN);		// AWB_R_RATIO_PRE_AWB
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x683F, WORD_LEN);		// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);		// AWB_B_RATIO_PRE_AWB
			break;		

		case CAMERA_YUV_WB_DAYLIGHT:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xAC02, WORD_LEN); 	// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN); 	// AWB_MODE
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x2C03, WORD_LEN);		// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);		// AWB_R_RATIO_PRE_AWB
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xAC3C, WORD_LEN); 	// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x003C, WORD_LEN); 	// AWB_R_RATIO_PRE_AWB
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xAC3D, WORD_LEN); 	// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0065, WORD_LEN); 	// AWB_R_RATIO_PRE_AWB
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x683F, WORD_LEN); 	// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN); 	// AWB_B_RATIO_PRE_AWB
			break;
		
		case CAMERA_YUV_WB_FLUORESCENT:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xAC02, WORD_LEN);		// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);		// AWB_MODE
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x2C03, WORD_LEN);		// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);		// AWB_R_RATIO_PRE_AWB
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xAC3C, WORD_LEN);		// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0055, WORD_LEN);		// AWB_R_RATIO_PRE_AWB
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xAC3D, WORD_LEN);		// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x004C, WORD_LEN);		// AWB_B_RATIO_PRE_AWB
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x683F, WORD_LEN);		// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);		// PRI_A_CONFIG_AWB_ALGO_RUN
			break;		
			
		case CAMERA_YUV_WB_CLOUDY_DAYLIGHT:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xAC02, WORD_LEN); 	// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN); 	// AWB_MODE
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x2C03, WORD_LEN);		// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);		// AWB_R_RATIO_PRE_AWB
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xAC3C, WORD_LEN); 	// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0032, WORD_LEN); 	// AWB_R_RATIO_PRE_AWB
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xAC3D, WORD_LEN); 	// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0074, WORD_LEN); 	// AWB_B_RATIO_PRE_AWB
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x683F, WORD_LEN); 	// MCU_ADDRESS
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN); 	// PRI_A_CONFIG_AWB_ALGO_RUN
			break;
	}

	if (usedelay) {
			CDBG("%s  white balance refresh \n",__func__);

			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN);  	// MCU_ADDRESS [SEQ_CMD]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN);  	// MCU_DATA_0
	
			rc |= mt9t111_poll_wait(0x8400, 0x0990, 0x0000, 1000, 10);
	}

		if(rc)
			CDBG("%s  white balance failed %d \n",__func__, wb);
		
	return rc;
}

static long mt9t111_set_brightness(int brightness)
{
	long rc = 0;

	switch (brightness) 
	{
		case 0:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0xB000, WORD_LEN);		// Y_RGB_OFFSET
			break;
		case 1:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0xC000, WORD_LEN);		// Y_RGB_OFFSET
			break;
		case 2:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0xD000, WORD_LEN);		// Y_RGB_OFFSET
			break;
		case 3:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0xE000, WORD_LEN);		// Y_RGB_OFFSET
			break;
		case 4:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0xF000, WORD_LEN);		// Y_RGB_OFFSET
			break;
		case 5:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0xF700, WORD_LEN);		// Y_RGB_OFFSET  // Default
			break;
		default:
		case 6:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0x0000, WORD_LEN);		// Y_RGB_OFFSET
			break;
		case 7:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0x0F00, WORD_LEN);		// Y_RGB_OFFSET
			break;
		case 8:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0x1F00, WORD_LEN);		// Y_RGB_OFFSET
			break;
		case 9:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0x2F00, WORD_LEN);		// Y_RGB_OFFSET
			break;
		case 10:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0x4F00, WORD_LEN);		// Y_RGB_OFFSET
			break;
		case 11:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0x5F00, WORD_LEN);		// Y_RGB_OFFSET		
			break;
	}

		if(rc)
			CDBG("%s  brightness %d \n",__func__, brightness);
		
	return rc;
}

static long mt9t111_set_iso(int iso, int usedelay)
{
	long rc = 0;

	CDBG("%s  iso %d \n",__func__, iso);

	switch (iso) 
	{
		default:
		case CAMERA_YUV_ISO_AUTO:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0x0000, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_FDZONE]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6815, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_FDZONE]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000D, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6817, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000D, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C15, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MIN_VIRT_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000D, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C17, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000D, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x682D, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_DGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x682F, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MIN_VIRT_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0180, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6837, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0040, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6839, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_DGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0180, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6835, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_DGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0080, WORD_LEN);						// MCU_DATA_0
			break;
		
		case CAMERA_YUV_ISO_800:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0x0500, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_FDZONE]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6815, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_FDZONE]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0011, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6817, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0011, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C15, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MIN_VIRT_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0011, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C17, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0011, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x682D, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_DGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0011, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x682F, WORD_LEN);					// MCU_ADDRESS [AE_TRACK_FDZONE]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0170, WORD_LEN);					// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6837, WORD_LEN);					// MCU_ADDRESS [AE_TRACK_VIRT_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0170, WORD_LEN);					// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6839, WORD_LEN);					// MCU_ADDRESS [AE_TRACK_VIRT_DGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0170, WORD_LEN);					// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6835, WORD_LEN);					// MCU_ADDRESS [AE_TRACK_VIRT_DGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0090, WORD_LEN);					// MCU_DATA_0
			break;
		
		case CAMERA_YUV_ISO_400:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0x1000, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_FDZONE]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6815, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_FDZONE]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0013, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6817, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0013, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C15, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MIN_VIRT_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0013, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C17, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0013, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x682D, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_DGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0012, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x682F, WORD_LEN);					// MCU_ADDRESS [AE_TRACK_FDZONE]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0980, 0x0190, WORD_LEN);					// MCU_ADDRESS [AE_TRACK_FDZONE]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x099E, 0x6837, WORD_LEN);					// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0980, 0x0190, WORD_LEN);					// MCU_ADDRESS [AE_TRACK_VIRT_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x099E, 0x6839, WORD_LEN);					// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0980, 0x0190, WORD_LEN);					// MCU_ADDRESS [AE_TRACK_VIRT_DGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x099E, 0x6835, WORD_LEN);					// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0980, 0x00A0, WORD_LEN);					// MCU_ADDRESS [AE_TRACK_VIRT_DGAIN]
			break;		
		
		case CAMERA_YUV_ISO_200:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0x1500, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_FDZONE]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6815, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_FDZONE]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0017, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6817, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0017, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C15, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MIN_VIRT_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0017, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C17, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0017, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x682D, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_DGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0017, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x682F, WORD_LEN);					// MCU_ADDRESS [AE_TRACK_FDZONE]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0980, 0x01B0, WORD_LEN);					// MCU_ADDRESS [AE_TRACK_FDZONE]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x099E, 0x6837, WORD_LEN);					// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0980, 0x01B0, WORD_LEN);					// MCU_ADDRESS [AE_TRACK_VIRT_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x099E, 0x6839, WORD_LEN);					// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0980, 0x01B0, WORD_LEN);					// MCU_ADDRESS [AE_TRACK_VIRT_DGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x099E, 0x6835, WORD_LEN);					// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0980, 0x00B0, WORD_LEN);					// MCU_ADDRESS [AE_TRACK_VIRT_DGAIN]
			break;
		
		case CAMERA_YUV_ISO_100:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0x2000, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_FDZONE]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6815, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_FDZONE]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0017, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6817, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0017, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C15, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MIN_VIRT_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0017, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C17, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0017, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x682D, WORD_LEN);						// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_DGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0017, WORD_LEN);						// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x682F, WORD_LEN);					// MCU_ADDRESS [AE_TRACK_FDZONE]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0980, 0x01E0, WORD_LEN);					// MCU_DATA_0                                           
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x099E, 0x6837, WORD_LEN);					// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MIN_VIRT_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0980, 0x01E0, WORD_LEN);					// MCU_DATA_0                                           
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x099E, 0x6839, WORD_LEN);					// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0980, 0x01E0, WORD_LEN);					// MCU_DATA_0                                           
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x099E, 0x6835, WORD_LEN);					// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0980, 0x00C0, WORD_LEN);					// MCU_ADDRESS [AE_TRACK_VIRT_DGAIN]
			break;
	}

	if(usedelay) {
		
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN);  	// MCU_ADDRESS [SEQ_CMD]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN);  	// MCU_DATA_0
	
			rc |= mt9t111_poll_wait(0x8400, 0x0990, 0x0000, 1000, 10);
	}

		if(rc)
			CDBG("%s  iso %d \n",__func__, iso);
		
	return rc;
}

static long mt9t111_set_scenemode(int scenemode)
{
	long rc = 0;

	CDBG("%s  scenemode %d  scene_before_status %d\n",__func__, scenemode, scene_before_status);

// recovery before status
	switch (scene_before_status) 
	{
		case CAMERA_SCENEMODES_PORTRAIT:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0x0000, WORD_LEN); 			// MCU_ADDRESS [CAM1_SYS_BRIGHT_COLORKILL]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE86F, WORD_LEN); 			// MCU_ADDRESS [CAM1_SYS_BRIGHT_COLORKILL]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0060, WORD_LEN); 			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC948, WORD_LEN); 			// MCU_ADDRESS [CAM1_SYS_DARK_COLOR_KILL]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); 			// MCU_DATA_0

			//refresh
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN); // MCU_ADDRESS [SEQ_CMD]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); // MCU_DATA_0
			break;

		case CAMERA_SCENEMODES_LANDSCAPE:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC917, WORD_LEN); 	// MCU_ADDRESS [AF_ALGO]                         
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0007, WORD_LEN); 	// MCU_DATA_0                                    
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC91A, WORD_LEN); 	// MCU_ADDRESS [AF_BEST_POSITION]                
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0007, WORD_LEN); 	// MCU_DATA_0                                    
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x326C, 0x1603, WORD_LEN); 			// MCU_ADDRESS [AF_ALGO]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6815, WORD_LEN); 			// MCU_ADDRESS [CAM1_SYS_UV_COLOR_BOOST]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000D, WORD_LEN); 			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6817, WORD_LEN); 			// MCU_ADDRESS [AF_BEST_POSITION]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000D, WORD_LEN); 			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C15, WORD_LEN); 			// MCU_ADDRESS [CAM1_SYS_UV_COLOR_BOOST]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000D, WORD_LEN); 			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C17, WORD_LEN); 			// MCU_ADDRESS [AF_BEST_POSITION]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000D, WORD_LEN); 			// MCU_DATA_0

			//refresh
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN); // MCU_ADDRESS [SEQ_CMD]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); // MCU_DATA_0
			break;

		case CAMERA_SCENEMODES_SPORTS:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6815, WORD_LEN); 			// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_50HZ]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000D, WORD_LEN); 			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6817, WORD_LEN); 			// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_60HZ]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000D, WORD_LEN); 			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C15, WORD_LEN); 			// MCU_ADDRESS [PRI_B_CONFIG_FD_MAX_FDZONE_50HZ]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000D, WORD_LEN); 			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C17, WORD_LEN); 			// MCU_ADDRESS [PRI_B_CONFIG_FD_MAX_FDZONE_60HZ]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000D, WORD_LEN); 			// MCU_DATA_0

			//refresh
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN); // MCU_ADDRESS [SEQ_CMD]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); // MCU_DATA_0
			break;

		case CAMERA_SCENEMODES_NIGHT:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6815, WORD_LEN); 			// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_50HZ]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000D, WORD_LEN); 				// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6817, WORD_LEN); 				// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_60HZ]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000D, WORD_LEN); 				// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x682D, WORD_LEN); 				// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_FDZONE] 
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); 				// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C15, WORD_LEN); 				// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_60HZ]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000D, WORD_LEN); 				// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C17, WORD_LEN); 				// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_60HZ]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000D, WORD_LEN); 				// MCU_DATA_0

			//refresh
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN); // MCU_ADDRESS [SEQ_CMD]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); // MCU_DATA_0
			break;

		case CAMERA_SCENEMODES_SUNSET:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE883, WORD_LEN);// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC83, WORD_LEN);// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x3210, 0x01B8, WORD_LEN);// MCU_DATA_0
			
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN); // MCU_ADDRESS [SEQ_CMD]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); // MCU_DATA_0
			break;
		
		default:
		case CAMERA_SCENEMODE_AUTO:
			break;
	}


	switch (scenemode) 
	{
		case CAMERA_SCENEMODE_AUTO:
			/*
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x3003, WORD_LEN); 			// MCU_ADDRESS [AF_ALGO]
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0002, WORD_LEN); 			// MCU_DATA_0
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC948, WORD_LEN); 			// MCU_ADDRESS [CAM1_SYS_UV_COLOR_BOOST]
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); 			// MCU_DATA_0
			
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC94A, WORD_LEN); 			// MCU_ADDRESS [CAM1_SYS_BRIGHT_COLORKILL]
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0065, WORD_LEN); 			// MCU_DATA_0
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC949, WORD_LEN); 			// MCU_ADDRESS [CAM1_SYS_DARK_COLOR_KILL]
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0012, WORD_LEN); 			// MCU_DATA_0
			
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6815, WORD_LEN); 			// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_50HZ]
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000C, WORD_LEN); 			// MCU_DATA_0
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6817, WORD_LEN); 			// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_60HZ]
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000C, WORD_LEN); 			// MCU_DATA_0
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C15, WORD_LEN); 			// MCU_ADDRESS [PRI_B_CONFIG_FD_MAX_FDZONE_50HZ]
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000C, WORD_LEN); 			// MCU_DATA_0
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C17, WORD_LEN); 			// MCU_ADDRESS [PRI_B_CONFIG_FD_MAX_FDZONE_60HZ]
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000C, WORD_LEN); 			// MCU_DATA_0
			
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x682D, WORD_LEN); 				// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_FDZONE]																  
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0005, WORD_LEN); 				// MCU_DATA_0
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x682F, WORD_LEN); 				// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_AGAIN]
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0120, WORD_LEN); 				// MCU_DATA_0
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6839, WORD_LEN); 				// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_AGAIN]
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0120, WORD_LEN); 			// MCU_DATA_0
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6835, WORD_LEN); 				// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_DGAIN]
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x00C0, WORD_LEN); 				// MCU_DATA_0	
			
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xAC02, WORD_LEN); 			// MCU_ADDRESS
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000A, WORD_LEN); 			// AWB_MODE by LGIT
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x2C03, WORD_LEN); 			// MCU_ADDRESS
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN); 			// AWB_R_RATIO_PRE_AWB
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xAC3C, WORD_LEN); 			// MCU_ADDRESS
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x003C, WORD_LEN); 			// AWB_R_RATIO_PRE_AWB	 
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xAC3D, WORD_LEN); 			// MCU_ADDRESS
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0050, WORD_LEN); 			// AWB_B_RATIO_PRE_AWB	 
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x683F, WORD_LEN); 			// MCU_ADDRESS
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x01D9, WORD_LEN); 			// PRI_A_CONFIG_AWB_ALGO_RUN

			//refresh
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN); // MCU_ADDRESS [SEQ_CMD]
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); // MCU_DATA_0
			*/
			break;

		case CAMERA_SCENEMODES_PORTRAIT:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x337E, 0x1000, WORD_LEN); 			// MCU_ADDRESS [CAM1_SYS_BRIGHT_COLORKILL]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE86F, WORD_LEN); 			// MCU_ADDRESS [CAM1_SYS_BRIGHT_COLORKILL]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0030, WORD_LEN); 			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC948, WORD_LEN); 			// MCU_ADDRESS [CAM1_SYS_DARK_COLOR_KILL]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0004, WORD_LEN); 			// MCU_DATA_0

			//refresh
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN); // MCU_ADDRESS [SEQ_CMD]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); // MCU_DATA_0
			break;

		case CAMERA_SCENEMODES_LANDSCAPE:

			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC917, WORD_LEN); 	// MCU_ADDRESS [AF_ALGO]                         
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN); 	// MCU_DATA_0                                    
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xC91A, WORD_LEN); 	// MCU_ADDRESS [AF_BEST_POSITION]                
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN); 	// MCU_DATA_0                                    
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x326C, 0x0000, WORD_LEN); 	// MCU_ADDRESS [CAM1_LL_LL_START_1]            
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6815, WORD_LEN); 			// MCU_ADDRESS [AF_BEST_POSITION]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000A, WORD_LEN); 			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6817, WORD_LEN); 			// MCU_ADDRESS [CAM1_SYS_UV_COLOR_BOOST]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000A, WORD_LEN); 			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C15, WORD_LEN); 			// MCU_ADDRESS [AF_BEST_POSITION]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000A, WORD_LEN); 			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C17, WORD_LEN); 	// MCU_ADDRESS [PRI_B_CONFIG_FD_MAX_FDZONE_60HZ] 
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000A, WORD_LEN); 			// MCU_DATA_0

			//refresh
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN); // MCU_ADDRESS [SEQ_CMD]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); // MCU_DATA_0
			break;

		case CAMERA_SCENEMODES_SPORTS:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6815, WORD_LEN); 			// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_50HZ]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); 			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6817, WORD_LEN); 			// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_60HZ]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); 			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C15, WORD_LEN); 			// MCU_ADDRESS [PRI_B_CONFIG_FD_MAX_FDZONE_50HZ]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); 			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C17, WORD_LEN); 			// MCU_ADDRESS [PRI_B_CONFIG_FD_MAX_FDZONE_60HZ]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); 			// MCU_DATA_0

			//refresh
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN); // MCU_ADDRESS [SEQ_CMD]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); // MCU_DATA_0
			break;

		case CAMERA_SCENEMODES_NIGHT:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6815, WORD_LEN); 			// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_50HZ]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0012, WORD_LEN); 			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6817, WORD_LEN); 			// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_60HZ]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0012, WORD_LEN); 			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x682D, WORD_LEN); 			// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_FDZONE]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0012, WORD_LEN); 			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C15, WORD_LEN); 			// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0012, WORD_LEN); 			// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6C17, WORD_LEN);			// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_AGAIN]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0012, WORD_LEN); 			// MCU_DATA_0

			//refresh
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN); // MCU_ADDRESS [SEQ_CMD]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); // MCU_DATA_0
			break;

		case CAMERA_SCENEMODES_SUNSET:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE883, WORD_LEN); 	// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0002, WORD_LEN);	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC83, WORD_LEN);	// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SELECT_FX]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0002, WORD_LEN);	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE885, WORD_LEN);	// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SEPIA_CR]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0014, WORD_LEN);	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC85, WORD_LEN);	// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SEPIA_CR]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0014, WORD_LEN);	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xE886, WORD_LEN);	// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SEPIA_CB]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);	// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xEC86, WORD_LEN);	// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SEPIA_CB]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);	// MCU_DATA_0

			//refresh
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x8400, WORD_LEN); // MCU_ADDRESS [SEQ_CMD]
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0006, WORD_LEN); // MCU_DATA_0
			break;
		
		default:
			break;
	}

		if(rc)
			CDBG("%s  scenemode %d failed \n",__func__, scenemode);
		else
		{
			scene_before_status = scenemode;
		}
		
	return rc;
}

static u_int16_t flash_control_tbl[6][10]=
{
	{0,		0,		0,		0,		0,		0x1f0,	0x140,	0x90,	0x50,	0x50},
	{0,		0,		0,		0,		0,		0x1c0,	0x140,	0x90,	0x50,	0x50},
	{0,		0,		0,		0,		0x280,	0x1a0,	0x140,	0x90,	0x50,	0x50},
	{0,		0,		0,		0,		0x250,	0x180,	0x120,	0x80,	0x50,	0x40},
	{0,		0,		0,		0,		0x220,	0x160,	0xf0,	0x70,	0x50,	0x40},
	{0,		0,		0,		0,		0x1f0,	0x140,	0xb0,	0x60,	0x40,	0x40},
};

#ifdef CONFIG_MSM_CAMERA_TUNING
#define INT_REG_FILE_SIZE		16
static u_int16_t int_value = 0xffff;
#endif

static long mt9t111_set_flash_control(int mode)
{
	long rc = 0;
	static u_int16_t value_MB1=0, value_MB2=0, value_coarse_int_time=0, value_gain=0, r_ratio_pre_awb=0,
			b_ratio_pre_awb=0, af_position=0, af_progress = 0, af_status = 0, value_int_a = 0, value_int_b = 0;

	switch(mode)
	{
		case 0: 	// check BM1
#ifdef CONFIG_MSM_CAMERA_TUNING
		{
			struct file *phMscd_Filp = NULL;
			unsigned short r_ratio_lower=0x0040, r_ratio_upper=0x0050, b_ratio_lower=0x004a, b_ratio_upper=0x0050;

			mm_segment_t old_fs=get_fs();
	
			phMscd_Filp = filp_open("/data/flash.txt", O_RDWR |O_LARGEFILE, 0);
				
			if (IS_ERR(phMscd_Filp) || !phMscd_Filp)
				goto use_compiled_value;

			file_buf_alloc_pages = kzalloc(INT_REG_FILE_SIZE, GFP_KERNEL);
			file_buf_alloc_pages_free = file_buf_alloc_pages;
			
			if(!file_buf_alloc_pages)
				goto use_compiled_value;
	
			set_fs(get_ds());
			phMscd_Filp->f_op->read(phMscd_Filp, file_buf_alloc_pages, INT_REG_FILE_SIZE, &phMscd_Filp->f_pos);
			CDBG("%s  :  %d flash setting was read from sd card, first sensor setting is %s \n", __func__, (int)&phMscd_Filp->f_pos, file_buf_alloc_pages); 
			set_fs(old_fs);

			sscanf(file_buf_alloc_pages, "%x", &r_ratio_lower);
			file_buf_alloc_pages+=6;
			sscanf(file_buf_alloc_pages, "%x", &r_ratio_upper);
			file_buf_alloc_pages+=6;
			sscanf(file_buf_alloc_pages, "%x", &b_ratio_lower);
			file_buf_alloc_pages+=6;
			sscanf(file_buf_alloc_pages, "%x", &b_ratio_upper);
	
			kfree(file_buf_alloc_pages_free);
			filp_close(phMscd_Filp,NULL);

		use_compiled_value:
			rc = mt9t111_i2c_read(mt9t111_client->addr, 0x3012, &value_coarse_int_time, WORD_LEN); // coarse integration time
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x4827, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &value_int_a, WORD_LEN); // MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x486f, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &value_int_b, WORD_LEN); // MCU_DATA_0

			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x2821, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &value_gain, WORD_LEN); // MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3c, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &r_ratio_pre_awb, WORD_LEN); // MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3d, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &b_ratio_pre_awb, WORD_LEN); // MCU_DATA_0

			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac38, WORD_LEN);// MCU_ADDRESS [AF_STATUS] 		
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &r_ratio_lower, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac39, WORD_LEN);// MCU_ADDRESS [AF_STATUS] 		
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &r_ratio_upper, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3a, WORD_LEN);// MCU_ADDRESS [AF_STATUS] 		
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &b_ratio_lower, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3b, WORD_LEN);// MCU_ADDRESS [AF_STATUS] 		
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &b_ratio_upper, WORD_LEN);// MCU_DATA_0

			
			CDBG("LG_FW_FLASH %s [Check_BM1_Mode] value_INT_TIME is 0x%x\n" ,__func__, value_coarse_int_time);
			CDBG("LG_FW_FLASH %s [Check_BM1_Mode] value_INT_TIME_A is 0x%x\n" ,__func__, value_int_a);
			CDBG("LG_FW_FLASH %s [Check_BM1_Mode] value_INT_TIME_B is 0x%x\n" ,__func__, value_int_b);

			CDBG("LG_FW_FLASH %s [Check_BM1_Mode] value_GAIN is 0x%x\n" ,__func__, value_gain);
			CDBG("LG_FW_FLASH %s [Check_BM1_Mode] value_R_RATIO is 0x%x\n" ,__func__, r_ratio_pre_awb);
			CDBG("LG_FW_FLASH %s [Check_BM1_Mode] value_B_RATIO is 0x%x\n" ,__func__, b_ratio_pre_awb);
		}
#endif		
			start_flash_control = 1;
			value_MB1=0;
			value_MB2=0;
			af_position=0;
			
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x2c05, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);// MCU_DATA_0

			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac38, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0040, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac39, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0045, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3a, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x004A, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3b, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0050, WORD_LEN);// MCU_DATA_0

			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x3835, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &value_MB1, WORD_LEN); // MCU_DATA_0
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x3012, &value_coarse_int_time, WORD_LEN); // coarse integration time
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x2821, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &value_gain, WORD_LEN); // MCU_DATA_0
			CDBG("LG_FW_FLASH [Before_Pre_Flash] value_BM1=0x%x\n",value_MB1);
			CDBG("LG_FW_FLASH [Before_Pre_Flash] value_coarse_int_time=0x%x\n",value_coarse_int_time);
			CDBG("LG_FW_FLASH [Before_Pre_Flash] value_gain=0x%x\n",value_gain);
			CDBG("LG_FW_FLASH_CHECK 0 \n");

			break;

		case 1:	// 1 : read Brightness Metric, Integration time and Gain
//========================================================================================
// awb range set
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x3835, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &value_MB2, WORD_LEN); // MCU_DATA_0
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x3012, &value_coarse_int_time, WORD_LEN); // coarse integration time
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x2821, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &value_gain, WORD_LEN); // MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3c, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &r_ratio_pre_awb, WORD_LEN); // MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3d, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &b_ratio_pre_awb, WORD_LEN); // MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xb024, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &af_position, WORD_LEN); // MCU_DATA_0
//========================================================================================

			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB019, WORD_LEN);// MCU_ADDRESS [AF_PROGRESS]
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &af_progress, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x3000, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
			rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &af_status, WORD_LEN);// MCU_DATA_0
			
			CDBG("LG_FW_FLASH %s [After_AF_Done] value_BM2 is 0x%x\n" ,__func__, value_MB2);
			CDBG("LG_FW_FLASH %s [After_AF_Done] value_INT_TIME is 0x%x\n" ,__func__, value_coarse_int_time);
			CDBG("LG_FW_FLASH %s [After_AF_Done] value_GAIN is 0x%x\n" ,__func__, value_gain);
			CDBG("LG_FW_FLASH %s [After_AF_Done] value_R_RATIO is 0x%x\n" ,__func__, r_ratio_pre_awb);
			CDBG("LG_FW_FLASH %s [After_AF_Done] value_B_RATIO is 0x%x\n" ,__func__, b_ratio_pre_awb);
			CDBG("LG_FW_FLASH %s [After_AF_Done] value_AF_POSITION is 0x%x\n" ,__func__, af_position);
			CDBG("LG_FW_FLASH %s [After_AF_Done] value_AF_PROGRESS is 0x%x\n" ,__func__, af_progress);
			CDBG("LG_FW_FLASH %s [After_AF_Done] value_AF_STATUS is 0x%x\n" ,__func__, af_status);			

			valid_gain_control = 1;
			CDBG("LG_FW_FLASH_CHECK 1 \n");

			break;

		case 2:	// 2 : before getting raw image
			if(valid_gain_control)
			{
				valid_gain_control = 0;
				u_int16_t flash_control_tbl_x=0, flash_control_tbl_y=0, flash_control_tbl_idx_x=0, flash_control_tbl_idx_y=0;
				unsigned short r_ratio_lower=0, r_ratio_upper=0, b_ratio_lower=0, b_ratio_upper=0;

				flash_control_tbl_y = value_MB1-value_MB2;
				flash_control_tbl_x = af_position;

				rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac38, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
				rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &r_ratio_lower, WORD_LEN);// MCU_DATA_0
				rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac39, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
				rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &r_ratio_upper, WORD_LEN);// MCU_DATA_0
				rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3a, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
				rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &b_ratio_lower, WORD_LEN);// MCU_DATA_0
				rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3b, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
				rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &b_ratio_upper, WORD_LEN);// MCU_DATA_0

				CDBG("LG_FW_FLASH %s [After_Change_to_Capture_Mode] r_ratio_lower is 0x%x\n" ,__func__, r_ratio_lower);
				CDBG("LG_FW_FLASH %s [After_Change_to_Capture_Mode] r_ratio_upper is 0x%x\n" ,__func__, r_ratio_upper);
				CDBG("LG_FW_FLASH %s [After_Change_to_Capture_Mode] b_ratio_lower is 0x%x\n" ,__func__, b_ratio_lower);
				CDBG("LG_FW_FLASH %s [After_Change_to_Capture_Mode] b_ratio_upper is 0x%x\n" ,__func__, b_ratio_upper);

				CDBG("LG_FW_FLASH %s [Calculated Value] value_BM1 is 0x%x\n " ,__func__, value_MB1);
				CDBG("LG_FW_FLASH %s [Calculated Value] value_BM2 is 0x%x\n " ,__func__, value_MB2);
				CDBG("LG_FW_FLASH %s [Calculated Value] value_BM1-value_BM2 is 0x%x\n " ,__func__, flash_control_tbl_y);
				CDBG("LG_FW_FLASH %s [Calculated Value] af_position is 0x%x\n " ,__func__, af_position);

				rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3c, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
				rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &r_ratio_pre_awb, WORD_LEN); // MCU_DATA_0
				rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3d, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
				rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &b_ratio_pre_awb, WORD_LEN); // MCU_DATA_0
				CDBG("LG_FW_FLASH %s [After_AF_Done] value_R_RATIO is 0x%x\n" ,__func__, r_ratio_pre_awb);
				CDBG("LG_FW_FLASH %s [After_AF_Done] value_B_RATIO is 0x%x\n" ,__func__, b_ratio_pre_awb);

				if(flash_control_tbl_y>=0 && flash_control_tbl_y<=0x300)
					flash_control_tbl_idx_y=0;
				else if(flash_control_tbl_y<=0x400)
					flash_control_tbl_idx_y=1;
				else if(flash_control_tbl_y<=0x700)
					flash_control_tbl_idx_y=2;
				else if(flash_control_tbl_y<=0x800)
					flash_control_tbl_idx_y=3;
				else if(flash_control_tbl_y<=0xf00)
					flash_control_tbl_idx_y=4;
				else
					flash_control_tbl_idx_y=5;
				
				if(flash_control_tbl_x==0)
					flash_control_tbl_idx_x=0;
				else if(flash_control_tbl_x<=0x1a)
					flash_control_tbl_idx_x=1;
				else if(flash_control_tbl_x<=0x33)
					flash_control_tbl_idx_x=2;
				else if(flash_control_tbl_x<=0x40)
					flash_control_tbl_idx_x=3;
				else if(flash_control_tbl_x<=0x50)
					flash_control_tbl_idx_x=4;
				else if(flash_control_tbl_x<=0x5a)
					flash_control_tbl_idx_x=5;
				else if(flash_control_tbl_x<=0x68)
					flash_control_tbl_idx_x=6;
				else if(flash_control_tbl_x<=0x77)
					flash_control_tbl_idx_x=7;
				else if(flash_control_tbl_x<=0x9d)
					flash_control_tbl_idx_x=8;
				else
					flash_control_tbl_idx_x=9;

				if(flash_control_tbl[flash_control_tbl_idx_y][flash_control_tbl_idx_x] == 0)
				{
					unsigned short coarse_int_time_read=0;
					rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x3012, &coarse_int_time_read, WORD_LEN); // coarse integration time
					rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x486F, WORD_LEN);
					rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, coarse_int_time_read, WORD_LEN);
					CDBG("LG_FW_READ_INT_VALUE: 0x%x\n", coarse_int_time_read);
				}
				else//(flash_control_tbl[flash_control_tbl_idx_y][flash_control_tbl_idx_x] != 0)
				{
					rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x486F, WORD_LEN);
					rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, flash_control_tbl[flash_control_tbl_idx_y][flash_control_tbl_idx_x], WORD_LEN);
					CDBG("LG_FW_WRITE_INT_VALUE: 0x%x\n", flash_control_tbl[flash_control_tbl_idx_y][flash_control_tbl_idx_x]);
				}
#if defined (CONFIG_MSM_CAMERA_TUNING)
				if(int_value != 0xffff)  {
					CDBG("LG_FW_WRITE_INT_VALUE: 0x%x\n", int_value);
					rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x486F, WORD_LEN);
					rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, int_value, WORD_LEN);
				}				
#endif
				rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB019, WORD_LEN);// MCU_ADDRESS [AF_PROGRESS]
				rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &af_progress, WORD_LEN);// MCU_DATA_0
				rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x3000, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
				rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &af_status, WORD_LEN);// MCU_DATA_0
				
				CDBG("LG_FW_FLASH %s [After_Change_to_Capture_Mode] value_AF_PROGRESS is 0x%x\n" ,__func__, af_progress);
				CDBG("LG_FW_FLASH %s [After_Change_to_Capture_Mode] value_AF_STATUS is 0x%x\n" ,__func__, af_status);
			}
			else
			{
				unsigned short r_ratio_lower=0, r_ratio_upper=0, b_ratio_lower=0, b_ratio_upper=0;

				rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac38, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
				rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &r_ratio_lower, WORD_LEN);// MCU_DATA_0
				rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac39, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
				rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &r_ratio_upper, WORD_LEN);// MCU_DATA_0
				rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3a, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
				rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &b_ratio_lower, WORD_LEN);// MCU_DATA_0
				rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3b, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
				rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &b_ratio_upper, WORD_LEN);// MCU_DATA_0

				CDBG("LG_FW_FLASH %s [After_Change_to_Capture_Mode_without_flash] r_ratio_lower is 0x%x\n" ,__func__, r_ratio_lower);
				CDBG("LG_FW_FLASH %s [After_Change_to_Capture_Mode_without_flash] r_ratio_upper is 0x%x\n" ,__func__, r_ratio_upper);
				CDBG("LG_FW_FLASH %s [After_Change_to_Capture_Mode_without_flash] b_ratio_lower is 0x%x\n" ,__func__, b_ratio_lower);
				CDBG("LG_FW_FLASH %s [After_Change_to_Capture_Mode_without_flash] b_ratio_upper is 0x%x\n" ,__func__, b_ratio_upper);
				CDBG("LG_FW_FLASH %s [Calculated Value] af_position is 0x%x\n " ,__func__, af_position);

				rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3c, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
				rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &r_ratio_pre_awb, WORD_LEN); // MCU_DATA_0
				rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3d, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
				rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &b_ratio_pre_awb, WORD_LEN); // MCU_DATA_0
				CDBG("LG_FW_FLASH %s [After_AF_Done] value_R_RATIO is 0x%x\n" ,__func__, r_ratio_pre_awb);
				CDBG("LG_FW_FLASH %s [After_AF_Done] value_B_RATIO is 0x%x\n" ,__func__, b_ratio_pre_awb);

			}
			CDBG("LG_FW_FLASH_CHECK 2 \n");

			break;

		default:	// 0xff : cancel flash control
//=================================================================================
// awb range return
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x2c05, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0002, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac38, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x003c, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac39, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0085, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3a, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0034, WORD_LEN);// MCU_DATA_0
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3b, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0085, WORD_LEN);// MCU_DATA_0
//=================================================================================
			start_flash_control = 0;
			
			CDBG("LG_FW_FLASH_CHECK DEFAULT \n");
			break;
	}
		
	return rc;
}
static int mt9t111_set_af_param_start(int mode)
{
	int rc = 0;
	
	CDBG("%s..\n",__func__);

	switch (mode) {
		case MSM_CAMERA_AF_NORMAL:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x3017, WORD_LEN);
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x2020, WORD_LEN);		
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB01D, WORD_LEN);		//AF_step number: 8	                          
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0008, WORD_LEN);		                                               
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB01F, WORD_LEN);		//AF_step size	                                
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000F, WORD_LEN);			                                              
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x440B, WORD_LEN);		//AF_actuator min physical value	              
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);			                                              
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x440D, WORD_LEN);		//AF_actuator max physical value 	            
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x03C0, WORD_LEN);		                                               
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB01C, WORD_LEN);		//AF_initial position	                        
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);		
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB025, WORD_LEN);		//AF_step number: 0	                          
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x001A, WORD_LEN);			                                              
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB026, WORD_LEN);		//AF_step number: 1	                          
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0033, WORD_LEN);			                                              
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB027, WORD_LEN);		//AF_step number: 2	                          
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0040, WORD_LEN);			                                              
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB028, WORD_LEN);		//AF_step number: 3                            
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0050, WORD_LEN);			                                              
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB029, WORD_LEN);		//AF_step number: 4                            
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x005A, WORD_LEN);			                                              
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB02A, WORD_LEN);		//AF_step number: 5                            
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0068, WORD_LEN);		                                            
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB02B, WORD_LEN);		//AF_step number: 6                            
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0077, WORD_LEN);			                                              
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB02C, WORD_LEN);		//AF_step number: 7                            
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x00FF, WORD_LEN);
			CDBG("%s  mt9t111 afmode MSM_CAMERA_AF_NORMAL..\n",__func__);
			break;

		case MSM_CAMERA_AF_MACRO:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB01D, WORD_LEN);		//AF_step number: 7	                          
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0007, WORD_LEN);		                                               
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB01F, WORD_LEN);		//AF_step size	                                
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x000F, WORD_LEN);			                                              
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x440B, WORD_LEN);		//AF_actuator min physical value	              
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);			                                              
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x440D, WORD_LEN);		//AF_actuator max physical value 	            
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x03C0, WORD_LEN);		                                               
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB01C, WORD_LEN);		//AF_initial position	                        
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);		
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB025, WORD_LEN);		//AF_step number: 0	                          
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0077, WORD_LEN);			                                              
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB026, WORD_LEN);	//AF_step number: 1	                          
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0087, WORD_LEN);			                                              
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB027, WORD_LEN);		//AF_step number: 2	                          
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0097, WORD_LEN);			                                              
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB028, WORD_LEN);		//AF_step number: 3                            
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x00A7, WORD_LEN);			                                              
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB029, WORD_LEN);		//AF_step number: 4                            
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x00B7, WORD_LEN);			                                              
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB02A, WORD_LEN);		//AF_step number: 5                            
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x00C7, WORD_LEN);		                                            
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB02B, WORD_LEN);		//AF_step number: 6                            
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x00D7, WORD_LEN);
			CDBG("%s  mt9t111 afmode MSM_CAMERA_AF_MACRO..\n",__func__);
			break;

		case MSM_CAMERA_AF_AUTO:
			rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x3017, WORD_LEN);
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x2020, WORD_LEN);
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB01D, WORD_LEN);		//AF_step number: 11 								  
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0009, WORD_LEN);
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB01F, WORD_LEN);		//AF_step size 										  
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0005, WORD_LEN);																		 
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x440B, WORD_LEN);		//AF_actuator min physical value 				  
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);																		 
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x440D, WORD_LEN);		//AF_actuator max physical value 					
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x03C0, WORD_LEN);																	  
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB01C, WORD_LEN);		//AF_initial position									
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);		
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB025, WORD_LEN);		//AF_step number: 0									  
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);																		 
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB026, WORD_LEN);		//AF_step number: 1									  
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x001A, WORD_LEN);																		 
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB027, WORD_LEN);		//AF_step number: 2									  
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0033, WORD_LEN);																		 
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB028, WORD_LEN);		//AF_step number: 3									  
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0040, WORD_LEN);																		 
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB029, WORD_LEN);		//AF_step number: 4									  
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0050, WORD_LEN);																		 
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB02A, WORD_LEN);		//AF_step number: 5									  
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x005A, WORD_LEN);																	 
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB02B, WORD_LEN);		//AF_step number: 6									  
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0068, WORD_LEN);																		 
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB02C, WORD_LEN);		//AF_step number: 7									  
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0077, WORD_LEN);																		
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB02D, WORD_LEN);		//AF_step number: 8										
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x009D, WORD_LEN);
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB02E, WORD_LEN);		//AF_step number: 9										
			rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);

			CDBG("%s  mt9t111 afmode MSM_CAMERA_AF_AUTO..\n",__func__);
			break;
	}

	return rc;
}

//---------------------------------------------------------
//	20091118 cis setting resolution
static int mt9t111_set_af_start(int mode)
{
	long rc = 0;
	CDBG("%s..\n",__func__);

	mt9t111_set_af_param_start(mode);

	rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x3003, WORD_LEN); // MCU_ADDRESS [AF_ALGO]  <-	Full Scan Setting
	rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0002, WORD_LEN); // MCU_DATA_0
	rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB019, WORD_LEN); // MCU_ADDRESS [AF_PROGRESS] <- AF Trigger Start
	rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0001, WORD_LEN); // MCU_DATA_0
	return rc;
}
//---------------------------------------------------------

//---------------------------------------------------------
//	20091118 cis setting resolution
static int mt9t111_set_af_cancel(void)
{
	long rc = 0;
	CDBG("%s..\n",__func__);

	CDBG("LG_FW_AF_CANCEL\n");
	rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x3003, WORD_LEN); // MCU_ADDRESS [AF_ALGO]  <-	Full Scan Setting
	rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN); // MCU_DATA_0

	valid_gain_control = 0;

//=================================================================================
// awb range return
		rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x2c05, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
		rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0002, WORD_LEN);// MCU_DATA_0
		rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac38, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
		rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x003c, WORD_LEN);// MCU_DATA_0
		rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac39, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
		rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0085, WORD_LEN);// MCU_DATA_0
		rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3a, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
		rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0034, WORD_LEN);// MCU_DATA_0
		rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3b, WORD_LEN);// MCU_ADDRESS [AF_STATUS]			
		rc = mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0085, WORD_LEN);// MCU_DATA_0
//=================================================================================
		start_flash_control = 0;

	return rc;
}
//---------------------------------------------------------

static int flash_brightness = 0;
static int mt9t111_get_flash_brightness(int func)
{
	unsigned short value_gain_max=0xffff, value_gain_current=0xffff, value_MB1_brightness=0xffff, rc=-1;
	int value_MB1_brightness_compare[]={0x0600, 0x0400, 0x0500};	// camera, norman camcorder, mms camcorder
	
	flash_brightness = 0;

	rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x6839, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
	rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &value_gain_max, WORD_LEN); // MCU_DATA_0
	rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x2821, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
	rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &value_gain_current, WORD_LEN); // MCU_DATA_0
	rc |= mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x3835, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
	rc |= mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &value_MB1_brightness, WORD_LEN); // MCU_DATA_0


	switch(func)
	{
		case 0 : //check whether af fail or not
			if((value_gain_max==value_gain_current)&&(value_MB1_brightness>=0x1000))
				flash_brightness = 1;	// af fail
			break;

		case 1 : //check whether need flash or not
//			if((value_gain_max==value_gain_current)&&(value_MB1_brightness>=0x0600))
			if((value_gain_max==value_gain_current)&&(value_MB1_brightness>=value_MB1_brightness_compare[recording_mode]))
				flash_brightness = 2;	// need flash
			break;
	}

	
	CDBG("%s..	value_gain_max %d value_gain_current %d value_MB1_brightness %d func %d value_MB1_brightness_compare 0x%x\n ",__func__, value_gain_max, value_gain_current,value_MB1_brightness, func, value_MB1_brightness_compare[recording_mode]);
	return flash_brightness;
}


static long mt9t111_get_af_status(void)
{
	u_int16_t vCur=YUV_AF_MODE_UNLOCKED;	// 1: af locked, 2:af unlocked, 3:af init, -1:process failed
	u_int16_t value_coarse_int_time;
	u_int16_t value_gain, value_int_a, value_int_b;
	int rc;

	if(mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB019, WORD_LEN))
	{
		CDBG("af status write failed..\n");
		return -1;
	}
	
	if(mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &vCur, WORD_LEN))
	{
		CDBG("af status read failed..\n");
		return -1;
	}
	CDBG("%s..	vCur : %d	 step1 \n",__func__, vCur);


	if(vCur==0)
	{
		
		CDBG("%s..	af done \n",__func__);

		msleep(10);

		if(mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x3000, WORD_LEN))
		{
			CDBG("af status write failed..\n");
			return -1;
		}
		
		if(mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &vCur, WORD_LEN))
		{
			CDBG("af status read failed..\n");
			return -1;
		}
		
		rc = mt9t111_i2c_read(mt9t111_client->addr, 0x3012, &value_coarse_int_time, WORD_LEN); // coarse integration time
		rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x4827, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
		rc = mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &value_int_a, WORD_LEN); // MCU_DATA_0
		rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x486f, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
		rc = mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &value_int_b, WORD_LEN); // MCU_DATA_0

		rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x2821, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
		rc = mt9t111_i2c_read(mt9t111_client->addr, 0x0990, &value_gain, WORD_LEN); // MCU_DATA_0
		rc = mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xac3c, WORD_LEN); // MCU_ADDRESS [PRI_A_CONFIG_AWB_ALGO_RUN]
			
		CDBG("LG_FW_FLASH %s [POST_AF_DONE]value_INT_TIME is 0x%x\n" ,__func__, value_coarse_int_time);
		CDBG("LG_FW_FLASH %s [POST_AF_DONE]value_INT_TIME_A is 0x%x\n" ,__func__, value_int_a);
		CDBG("LG_FW_FLASH %s [POST_AF_DONE]value_INT_TIME_B is 0x%x\n" ,__func__, value_int_b);

		CDBG("LG_FW_FLASH %s [POST_AF_DONE]value_GAIN is 0x%x\n" ,__func__, value_gain);
	
		
		if((vCur&0x1) == 0)
		{
			
			if(start_flash_control)
			{
				mt9t111_set_flash_control(1);
				start_flash_control = 0;
			}
			
			CDBG("%s..	vCur : %d    af done & success \n",__func__, vCur);
		}
		else if((vCur&0x1)==1)
		{
			if(start_flash_control)
			{
				mt9t111_set_flash_control(1);
				start_flash_control = 0;
			}
			
			/* move focusing to infinity */
			mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0x3003, WORD_LEN);
			mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0001, WORD_LEN);
			mt9t111_i2c_write(mt9t111_client->addr, 0x098E, 0xB024, WORD_LEN);
			mt9t111_i2c_write(mt9t111_client->addr, 0x0990, 0x0000, WORD_LEN);
			
			CDBG("%s..	vCur : %d	 af done & af locking fail \n",__func__, vCur);
			vCur = 0xff;
		}
	}

	CDBG("%s..	vCur : %d	 step2 \n",__func__, vCur);

	
	if(vCur==0)
	{
		CDBG("AF_LOCKED\n");
		msleep(10);
		vCur=YUV_AF_MODE_LOCKED;
	}
	else if(vCur==0xff)
	{
		vCur=YUV_AF_MODE_LOCKED_FAILED;
		CDBG("AF_LOCKED_FAILED\n");
	}
	else {
		vCur=YUV_AF_MODE_UNLOCKED;
		CDBG("AF_UNLOCKED\n");
	}
	
	return vCur;
}


//-----------------------------------------------------------------------
//LG_FW : 2009.09.14 cis - skip this for software i2c registeration
static int mt9t111_sensor_init_probe(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

	CDBG("init entry \n");
	rc = mt9t111_reset(data);
	if (rc < 0) {
		CDBG("reset failed!\n");
		goto init_probe_fail;
	}

	mdelay(5);

	rc = mt9t111_reg_init();
	if (rc < 0)
		goto init_probe_fail;

	return rc;

init_probe_fail:
	return rc;
}

int mt9t111_sensor_init(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

	mt9t111_ctrl = kzalloc(sizeof(struct mt9t111_ctrl), GFP_KERNEL);
	if (!mt9t111_ctrl) {
		CDBG("mt9t111_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}

	if (data)
		mt9t111_ctrl->sensordata = data;
	
	{
		int idx=0;
		idx = msm_camera_mclk_idx_value();
		msm_camio_clk_rate_set(msm_camera_mclk_value[idx]);
		CDBG("%s: sensor clock setting%d\n", __func__, msm_camera_mclk_value[idx]);
	}

	gpio_request(data->sensor_reset, "mt9t111");
	gpio_request(data->sensor_pwd, "mt9t111");

	if(gpio_direction_output(data->sensor_reset, 0))
		printk(KERN_ERR "%s : gpio conftol fail : %d\n",
				 __func__, data->sensor_reset);
	if(gpio_direction_output(data->sensor_pwd, 0))
		printk(KERN_ERR "%s : gpio conftol fail : %d\n",
				 __func__, data->sensor_pwd);

	gpio_free(data->sensor_pwd);
	gpio_free(data->sensor_reset);	

	data->pdata->camera_power_on();

	msm_camio_camif_pad_reg_reset();

	rc = mt9t111_sensor_init_probe(data);
	if (rc < 0) {
		CDBG("mt9t111_sensor_init failed!\n");
		goto init_fail;
	}

init_done:
	return rc;

init_fail:
	kfree(mt9t111_ctrl);
	return rc;
}

static int mt9t111_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&mt9t111_wait_queue);
	return 0;
}

int mt9t111_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	long   rc = 0;

	if (copy_from_user(&cfg_data,
			(void *)argp,
			sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	/* down(&mt9t111_sem); */

	CDBG("mt9t111_ioctl, cfgtype = %d, mode = %d, effect  %d, white balance %d \n",
		cfg_data.cfgtype, cfg_data.mode, cfg_data.cfg.effect, cfg_data.cfg.wb);

	switch (cfg_data.cfgtype) {
		case CFG_SET_MODE:
			rc = mt9t111_set_sensor_mode(cfg_data.mode, 1);
			break;

		case CFG_SET_EFFECT:
			if (cfg_data.cfg.effect == 0) {
				rc = mt9t111_set_effect(cfg_data.cfg.effect, 1);
			} else {
				rc = mt9t111_set_effect(cfg_data.cfg.effect, 1);
			}
			break;

		case CFG_SET_WB:
			rc = mt9t111_set_wb(cfg_data.cfg.wb, 1);
			break;

		case CFG_SET_AF_PARAM_INIT:
			rc = mt9t111_set_af_param_start(cfg_data.mode);
			break;

		case CFG_SET_AF_START:
			rc = mt9t111_set_af_start(cfg_data.mode);
			break;

		case CFG_SET_AF_CANCEL:
			rc = mt9t111_set_af_cancel();
			break;

		case CFG_SET_BRIGHTNESS:
			rc = mt9t111_set_brightness(cfg_data.cfg.brightness);
			break;

		case CFG_SET_ISO:
			rc = mt9t111_set_iso(cfg_data.cfg.iso, 1);
			break;

		case CFG_SET_SCENEMODE:
			rc = mt9t111_set_scenemode(cfg_data.cfg.scenemode);
			break;

		case CFG_SET_PRE_FLASH:
			CDBG("%s CFG_SET_PRE_FLASH\n", __func__);
			rc = mt9t111_set_flash_control(cfg_data.mode);
			break;

#ifdef CONFIG_MSM_CAMERA_PROGRESSIVE_INIT
		case CFG_SET_REG_GROUP_ONE:
			CDBG(" CFG_SET_REG_GROUP_ONE: E\n");
			rc = mt9t111_reg_write_group(mt9t111_regs.prev_group_1,
                                mt9t111_regs.prev_group_1_size);
			CDBG(" CFG_SET_REG_GROUP_ONE:rc=%ld X\n", rc);
			break;

		case CFG_SET_REG_GROUP_TWO:
			CDBG(" CFG_SET_REG_GROUP_TWO: E\n");
			rc = mt9t111_reg_write_group(mt9t111_regs.prev_group_2,
                                mt9t111_regs.prev_group_2_size);
			CDBG(" CFG_SET_REG_GROUP_TWO:rc=%ld X\n", rc);
			break;

		case CFG_SET_REG_GROUP_THREE:
			CDBG(" CFG_SET_REG_GROUP_THREE: E\n");
			rc = mt9t111_reg_write_group(mt9t111_regs.prev_group_3,
                                mt9t111_regs.prev_group_3_size);
			CDBG(" CFG_SET_REG_GROUP_THREE:rc=%ld X\n", rc);
			break;

		case CFG_SET_REG_GROUP_FOUR:
			CDBG(" CFG_SET_REG_GROUP_FOUR: E\n");
			rc = mt9t111_reg_write_group(mt9t111_regs.prev_group_4,
                                mt9t111_regs.prev_group_4_size);
			CDBG(" CFG_SET_REG_GROUP_FOUR:rc=%ld X\n", rc);
			break;

		case CFG_SET_REG_GROUP_FIVE:
			CDBG(" CFG_SET_REG_GROUP_FIVE: E\n");
			rc = mt9t111_reg_write_group(mt9t111_regs.prev_group_5,
                                mt9t111_regs.prev_group_5_size);
			CDBG(" CFG_SET_REG_GROUP_FIVE:rc=%ld X\n", rc);
			break;

		case CFG_SET_REG_GROUP_SIX:
			CDBG(" CFG_SET_REG_GROUP_six: E\n");
			rc = mt9t111_reg_write_group(mt9t111_regs.prev_group_6,
                                mt9t111_regs.prev_group_6_size);
			CDBG(" CFG_SET_REG_GROUP_SIX:rc=%ld X\n", rc);
			break;

		case CFG_SET_REG_GROUP_SEVEN:
			CDBG(" CFG_SET_REG_GROUP_SEVEN: E\n");
			rc = mt9t111_reg_write_group(mt9t111_regs.prev_group_7,
                                mt9t111_regs.prev_group_7_size);
			CDBG(" CFG_SET_REG_GROUP_SEVEN:rc=%ld X\n", rc);
			break;

		case CFG_SET_REG_GROUP_EIGHT:
			CDBG(" CFG_SET_REG_GROUP_EIGHT: E\n");
			rc = mt9t111_reg_write_group(mt9t111_regs.prev_group_8,
                                mt9t111_regs.prev_group_8_size);
			CDBG(" CFG_SET_REG_GROUP_EIGHT:rc=%ld X\n", rc);
			break;

		case CFG_SET_REG_GROUP_NINE:
			CDBG(" CFG_SET_REG_GROUP_NINE: E\n");
			rc = mt9t111_reg_write_group(mt9t111_regs.prev_group_9,
                                mt9t111_regs.prev_group_9_size);
			CDBG(" CFG_SET_REG_GROUP_NINE:rc=%ld X\n", rc);
			break;

		case CFG_SET_REG_GROUP_TEN:
			CDBG(" CFG_SET_REG_GROUP_TEN: E\n");
			rc = mt9t111_reg_write_group(mt9t111_regs.prev_group_10,
                                mt9t111_regs.prev_group_10_size);
			CDBG(" CFG_SET_REG_GROUP_TEN:rc=%ld X\n", rc);
			break;

		case CFG_SET_REG_GROUP_ELEVEN:
			CDBG(" CFG_SET_REG_GROUP_ELEVEN: E\n");
			rc = mt9t111_reg_write_group(mt9t111_regs.prev_group_11,
                                mt9t111_regs.prev_group_11_size);
			CDBG(" CFG_SET_REG_GROUP_ELEVEN:rc=%ld X\n", rc);
			break;

		case CFG_SET_REG_GROUP_TWELVE:
			CDBG(" CFG_SET_REG_GROUP_TWELVE: E\n");
			rc = mt9t111_reg_write_group(mt9t111_regs.prev_group_12,
                                mt9t111_regs.prev_group_12_size);
			CDBG(" CFG_SET_REG_GROUP_TWELVE:rc=%ld X\n", rc);
			break;

		case CFG_SET_REG_GROUP_THIRDTEEN:
			CDBG(" CFG_SET_REG_GROUP_THIRDTEEN: E\n");
			rc = mt9t111_reg_write_group(mt9t111_regs.prev_group_13,
                                mt9t111_regs.prev_group_13_size);
			CDBG(" CFG_SET_REG_GROUP_THIRDTEEN:rc=%ld X\n", rc);
			break;

		case CFG_SET_REG_GROUP_FOURTEEN:
			CDBG(" CFG_SET_REG_GROUP_FOURTEEN: E\n");
			rc = mt9t111_reg_write_group(mt9t111_regs.prev_group_14,
                                mt9t111_regs.prev_group_14_size);
			CDBG(" CFG_SET_REG_GROUP_FOURTEEN:rc=%ld X\n", rc);
			break;

		case CFG_SET_REG_GROUP_FIFTEEN:
			CDBG(" CFG_SET_REG_GROUP_FIFTEEN: E\n");
			rc = mt9t111_reg_write_group(mt9t111_regs.prev_group_15,
                                mt9t111_regs.prev_group_15_size);
			CDBG(" CFG_SET_REG_GROUP_FIFTEEN:rc=%ld X\n", rc);
			break;
#endif

		default:
			rc = -EINVAL;
			break;
	}

	return rc;
}

/* 1: af locked, 2:af unlocked, 3:process failed */
int mt9t111_sensor_sf_cmd(int sf_mod)
{
	int ret_value=1;

	switch (sf_mod) 
	{
		case CFG_GET_AF_STATUS:
			ret_value = mt9t111_get_af_status();
			CDBG("%s, CFG_GET_AF_STATUS : %d\n",__func__, ret_value);
			break;
		case CFG_GET_FLASH_BRIGHTNESS:
			ret_value = mt9t111_get_flash_brightness(1);
			CDBG("%s, CFG_GET_FLASH_BRIGHTNESS : %d\n",__func__, ret_value);
			break;
		default:
			break;
	}

	return ret_value;
}

int mt9t111_sensor_release(void)
{
	int rc = 0;

#ifdef LG_CAMERA_HIDDEN_MENU
	if(sensorAlwaysOnTest ==true)
	{
		printk("mt9t111_sensor_not_release\n");
		return 0;
	}
#endif

	mt9t111_ctrl->sensordata->pdata->camera_power_off();
//	sensor_mode_before = -1;

	kfree(mt9t111_ctrl);

	return rc;
}

static int mt9t111_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	mt9t111_sensorw =
		kzalloc(sizeof(struct mt9t111_work), GFP_KERNEL);

	if (!mt9t111_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, mt9t111_sensorw);
	mt9t111_init_client(client);
	mt9t111_client = client;

	CDBG("mt9t111_probe succeeded!\n");

	return 0;

probe_failure:
	kfree(mt9t111_sensorw);
	mt9t111_sensorw = NULL;
	CDBG("mt9t111_probe failed!\n");
	return rc;
}

static const struct i2c_device_id mt9t111_i2c_id[] = {
	{ "mt9t111", 0},
	{ },
};

static struct i2c_driver mt9t111_i2c_driver = {
	.id_table = mt9t111_i2c_id,
	.probe  = mt9t111_i2c_probe,
	.remove = __exit_p(mt9t111_i2c_remove),
	.driver = {
		.name = "mt9t111",
	},
};

/* START add recording mode setting */
static ssize_t recording_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	sprintf(buf, "%d", recording_mode);
	CDBG("recording_mode = %d\n", recording_mode);
	return 1;
}

static ssize_t recording_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	sscanf(buf, "%d", &recording_mode);
	CDBG("recording_mode = %d\n", recording_mode);
	return size;
}

static DEVICE_ATTR(recording, S_IRUGO | S_IWUGO, recording_show, recording_store);
/* END add recording mode setting */

static int mt9t111_sensor_probe(const struct msm_camera_sensor_info *info,
				struct msm_sensor_ctrl *s)
{
	int rc = i2c_add_driver(&mt9t111_i2c_driver);
	if (rc < 0 || mt9t111_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_done;
	}

	{
		int midx=0, pidx=0;
		midx = msm_camera_mclk_idx_value();
		pidx = msm_camera_pclk_idx_value();
		msm_camio_clk_rate_set(msm_camera_mclk_value[midx]);
		CDBG("%s: sensor clock setting midx %d  m %d\n", __func__, midx, msm_camera_mclk_value[midx]);
		CDBG("%s: sensor clock setting pidx %d  p %d\n", __func__, pidx, msm_camera_mclk_value[pidx]);
	}

	mdelay(5);

	rc = mt9t111_reset_init(info);

	s->s_init = mt9t111_sensor_init;
	s->s_release = mt9t111_sensor_release;
	s->s_config = mt9t111_sensor_config;
	s->s_sf_cmd= mt9t111_sensor_sf_cmd;

/* START add recording mode setting */
	rc = device_create_file(&mt9t111_dev->dev, &dev_attr_recording);
/* END add recording mode setting */

probe_done:
	CDBG("%s %s:%d\n", __FILE__, __func__, __LINE__);
	return rc;
}

static int __mt9t111_probe(struct platform_device *pdev)
{
/* START add recording mode setting */
	mt9t111_dev = pdev;
/* END add recording mode setting */

	return msm_camera_drv_start(pdev, mt9t111_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __mt9t111_probe,
	.driver = {
		.name = "msm_camera_mt9t111",
		.owner = THIS_MODULE,
	},
};

static int __init mt9t111_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(mt9t111_init);
