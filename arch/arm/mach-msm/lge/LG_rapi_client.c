/* LGE_CHANGES LGE_RAPI_COMMANDS  */
/* Created by khlee@lge.com  
 * arch/arm/mach-msm/lge/LG_rapi_client.c
 *
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
 *
 */
#include <linux/kernel.h>
#include <linux/err.h>
#include <mach/oem_rapi_client.h>
#include <mach/lg_diag_testmode.h>
#if defined(CONFIG_MACH_MSM7X27_ALOHAV)
#include <mach/msm_battery_alohav.h>
#elif defined(CONFIG_MACH_MSM7X27_THUNDERC)
/* ADD THUNERC feature to use VS740 BATT DRIVER
 * 2010--5-13, taehung.kim@lge.com
 */
#include <mach/msm_battery_thunderc.h>
#else
#include <mach/msm_battery.h>
#endif

#define GET_INT32(buf)       (int32_t)be32_to_cpu(*((uint32_t*)(buf)))
#define PUT_INT32(buf, v)        (*((uint32_t*)buf) = (int32_t)be32_to_cpu((uint32_t)(v)))
#define GET_U_INT32(buf)         ((uint32_t)GET_INT32(buf))
#define PUT_U_INT32(buf, v)      PUT_INT32(buf, (int32_t)(v))

#define GET_LONG(buf)            ((long)GET_INT32(buf))
#define PUT_LONG(buf, v) \
	(*((u_long*)buf) = (long)be32_to_cpu((u_long)(v)))

#define GET_U_LONG(buf)	      ((u_long)GET_LONG(buf))
#define PUT_U_LONG(buf, v)	      PUT_LONG(buf, (long)(v))


#define GET_BOOL(buf)            ((bool_t)GET_LONG(buf))
#define GET_ENUM(buf, t)         ((t)GET_LONG(buf))
#define GET_SHORT(buf)           ((short)GET_LONG(buf))
#define GET_U_SHORT(buf)         ((u_short)GET_LONG(buf))

#define PUT_ENUM(buf, v)         PUT_LONG(buf, (long)(v))
#define PUT_SHORT(buf, v)        PUT_LONG(buf, (long)(v))
#define PUT_U_SHORT(buf, v)      PUT_LONG(buf, (long)(v))

#define LG_RAPI_CLIENT_MAX_OUT_BUFF_SIZE 128
#define LG_RAPI_CLIENT_MAX_IN_BUFF_SIZE 128


static uint32_t open_count;
struct msm_rpc_client *client;

/*LGE_CHANGES yongman.kwon 2010-09-07[MS690] : firstboot check */
extern boot_info;

int LG_rapi_init(void)
{
	client = oem_rapi_client_init();
	if (IS_ERR(client)) {
		pr_err("%s: couldn't open oem rapi client\n", __func__);
		printk("myeonggyu - %s: couldn't open oem rapi client\n", __func__);
		return PTR_ERR(client);
	}
	open_count++;

	return 0;
}

void Open_check(void)
{
	/* to double check re-open; */
	if(open_count > 0) return;
	LG_rapi_init();
}

int msm_chg_LG_cable_type(void)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	char output[LG_RAPI_CLIENT_MAX_OUT_BUFF_SIZE];

	Open_check();

/* LGE_CHANGES_S [younsuk.song@lge.com] 2010-09-06, Add error control code. Repeat 3 times if error occurs*/
	
	int rc= -1;
	int errCount= 0;

	do 
	{
		arg.event = LG_FW_RAPI_CLIENT_EVENT_GET_LINE_TYPE;
		arg.cb_func = NULL;
		arg.handle = (void*) 0;
		arg.in_len = 0;
		arg.input = NULL;
		arg.out_len_valid = 1;
		arg.output_valid = 1;
		arg.output_size = 4;

		ret.output = NULL;
		ret.out_len = NULL;

		rc= oem_rapi_client_streaming_function(client, &arg, &ret);
	
		if (rc < 0)
			pr_err("get LG_cable_type error \r\n");
		else
			pr_info("msm_chg_LG_cable_type: %d \r\n", GET_INT32(ret.output));

	} while (rc < 0 && errCount++ < 3);

/* LGE_CHANGES_E [younsuk.song@lge.com] */
	
	memcpy(output,ret.output,*ret.out_len);

	kfree(ret.output);
	kfree(ret.out_len);

	return (GET_INT32(output));  
}


void send_to_arm9(void*	pReq, void* pRsp)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();

	arg.event = LG_FW_TESTMODE_EVENT_FROM_ARM11;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(DIAG_TEST_MODE_F_req_type);
	arg.input = (char*)pReq;
	arg.out_len_valid = 1;
	arg.output_valid = 1;

	if( ((DIAG_TEST_MODE_F_req_type*)pReq)->sub_cmd_code == TEST_MODE_FACTORY_RESET_CHECK_TEST)
		arg.output_size = sizeof(DIAG_TEST_MODE_F_rsp_type) - sizeof(test_mode_rsp_type);
	else
		arg.output_size = sizeof(DIAG_TEST_MODE_F_rsp_type);

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);
	memcpy(pRsp,ret.output,*ret.out_len);

	return;
}

void set_operation_mode(boolean info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();

	arg.event = LG_FW_SET_OPERATIN_MODE;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(boolean);
	arg.input = (char*) &info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;

	ret.output = (char*) NULL;
	ret.out_len = 0;

	oem_rapi_client_streaming_function(client,&arg, &ret);
}
EXPORT_SYMBOL(set_operation_mode);


#ifdef CONFIG_MACH_MSM7X27_THUNDERC
void battery_info_get(struct batt_info* resp_buf)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	uint32_t out_len;
	int ret_val;
	struct batt_info rsp_buf;

	Open_check();

	arg.event = LG_FW_A2M_BATT_INFO_GET;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = sizeof(rsp_buf);

	ret.output = (char*)&rsp_buf;
	ret.out_len = &out_len;

	ret_val = oem_rapi_client_streaming_function(client, &arg, &ret);
	if(ret_val == 0) {
		resp_buf->valid_batt_id = GET_U_INT32(&rsp_buf.valid_batt_id);
		resp_buf->batt_therm = GET_U_INT32(&rsp_buf.batt_therm);
		resp_buf->batt_temp = GET_INT32(&rsp_buf.batt_temp);
		/* LGE_CHANGE_S [dojip.kim@lge.com] 2010-05-17, [LS670],
		 * add extra batt info
		 */
#if defined(CONFIG_MACH_MSM7X27_THUNDERC)
		resp_buf->chg_current = GET_U_INT32(&rsp_buf.chg_current);
		resp_buf->batt_thrm_state = GET_U_INT32(&rsp_buf.batt_thrm_state);
#endif
		/* LGE_CHANGE_E [dojip.kim@lge.com] 2010-05-17 */ 
	} else { /* In case error */
		resp_buf->valid_batt_id = 1; /* authenticated battery id */
		resp_buf->batt_therm = 100;  /* 100 battery therm adc */
		resp_buf->batt_temp = 30;    /* 30 degree celcius */
		/* LGE_CHANGE_S [dojip.kim@lge.com] 2010-05-17, [LS670],
		 * add extra batt info
		 */
#if defined(CONFIG_MACH_MSM7X27_THUNDERC)
		resp_buf->chg_current = 0;
		resp_buf->batt_thrm_state = 0;
#endif
		/* LGE_CHANGE_E [dojip.kim@lge.com] 2010-05-17 */ 
	}
	return;
}
EXPORT_SYMBOL(battery_info_get);

void pseudo_batt_info_set(struct pseudo_batt_info_type* info)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();

	arg.event = LG_FW_A2M_PSEUDO_BATT_INFO_SET;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(struct pseudo_batt_info_type);
	arg.input = (char*)info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;  /* alloc memory for response */

	ret.output = (char*)NULL;
	ret.out_len = 0;

	oem_rapi_client_streaming_function(client, &arg, &ret);
	return;
}
EXPORT_SYMBOL(pseudo_batt_info_set);
void block_charging_set(int bypass)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();
	arg.event = LG_FW_A2M_BLOCK_CHARGING_SET;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(int);
	arg.input = (char*) &bypass;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;

	ret.output = (char*)NULL;
	ret.out_len = 0;

	oem_rapi_client_streaming_function(client,&arg,&ret);
	return;
}
EXPORT_SYMBOL(block_charging_set);
#endif	/* CONFIG_MACH_MSM7X27_THUNDERC */

void msm_get_MEID_type(char* sMeid)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();

	arg.event = LG_FW_MEID_GET;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 15;

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);

	memcpy(sMeid,ret.output,14); 

	kfree(ret.output);
	kfree(ret.out_len);

	return;  
}

//20100712 myeonggyu.son@lge.com [MS690] hw revision [START]
/* LGE_CHANGE [dojip.kim@lge.com] 2010-05-29, pcb version from LS680*/
#ifdef  CONFIG_LGE_PCB_VERSION
int lg_get_hw_version(void)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	byte pcb_ver = 0xFF;
	unsigned int out_len = 0xFFFFFFFF;

	Open_check();

	arg.event = LG_FW_GET_PCB_VERSION;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 1;

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);

	pcb_ver = *ret.output;
	out_len = *ret.out_len;

	return pcb_ver;
}
EXPORT_SYMBOL(lg_get_hw_version);
#endif /* CONFIG_LGE_PCB_VERSION */
//20100712 myeonggyu.son@lge.com [MS690] hw revision [END]

//20100914 yongman.kwon@lge.com [MS690] : firstboot check [START]*/
void lg_set_boot_info(void)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;

	Open_check();
	arg.event = LG_FW_SET_BOOT_INFO;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = sizeof(int);
	arg.input = (char*) &boot_info;
	arg.out_len_valid = 0;
	arg.output_valid = 0;
	arg.output_size = 0;

	ret.output = (char*)NULL;
	ret.out_len = 0;

	oem_rapi_client_streaming_function(client,&arg,&ret);
	return;

}
EXPORT_SYMBOL(lg_set_boot_info);
//20100914 yongman.kwon@lge.com [MS690] : firstboot check [START]*/

//20100914 yongman.kwon@lge.com [MS690] : power check mode [START]
int lg_get_power_check_mode(void)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	byte power_mode = 0xFF;
	unsigned int out_len = 0xFFFFFFFF;

	Open_check();

	arg.event = LG_FW_GET_POWER_MODE;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 1;

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);

	power_mode = *ret.output;
	out_len = *ret.out_len;

	return power_mode;
}
EXPORT_SYMBOL(lg_get_power_check_mode);

int lg_get_flight_mode(void)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	byte flight_mode = 0xFF;
	unsigned int out_len = 0xFFFFFFFF;

	Open_check();

	arg.event = LG_FW_GET_FLIGHT_MODE;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 1;

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);

	flight_mode = *ret.output;
	out_len = *ret.out_len;

	return flight_mode;
}
EXPORT_SYMBOL(lg_get_flight_mode);
//20100914 yongman.kwon@lge.com [MS690] power check mode [END]


//20100929 yongman.kwon@lge.com [MS690] for check prl version for wifi on/off [START]
//LG_FW_CHECK_PRL_VERSION
word lg_get_prl_version(void)
{
	struct oem_rapi_client_streaming_func_arg arg;
	struct oem_rapi_client_streaming_func_ret ret;
	word prl_version = 0xFFFF;
	unsigned int out_len = 0xFFFFFFFF;

	Open_check();

	arg.event = LG_FW_GET_PRL_VERSION;
	arg.cb_func = NULL;
	arg.handle = (void*) 0;
	arg.in_len = 0;
	arg.input = NULL;
	arg.out_len_valid = 1;
	arg.output_valid = 1;
	arg.output_size = 2;

	ret.output = NULL;
	ret.out_len = NULL;

	oem_rapi_client_streaming_function(client, &arg, &ret);

	printk("output : %d\n", *ret.output);
	
	memcpy(&prl_version, ret.output, 2);

	printk("prl_version : %d\n", prl_version);
	
	out_len = *ret.out_len;

	return prl_version;
}
EXPORT_SYMBOL(lg_get_prl_version);
//20100929 yongman.kwon@lge.com [MS690] for check prl version for wifi on/off [END]


