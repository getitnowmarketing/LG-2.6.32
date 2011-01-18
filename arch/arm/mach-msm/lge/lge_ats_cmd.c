/* arch/arm/mach-msm/lge/lge_ats_atcmd.c
 *
 * Copyright (C) 2010 LGE, Inc.
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <mach/msm_rpcrouter.h>
#include <mach/board_lge.h>
#include "lge_ats.h"
#include "lge_ats_cmd.h"

#include <linux/syscalls.h>
#include <linux/fcntl.h>

void wirte_flight_mode(int mode)
{
	char buf[10];

	int fd = sys_open("/sys/devices/platform/autoall/flight", O_WRONLY, 0);
	
	if (fd == -1) {
        	return -1;
    	}

	sprintf(buf, "%d", mode);

	sys_write(fd, buf, strlen(buf));

	sys_close(fd);

}

int lge_ats_handle_atcmd(struct msm_rpc_server *server,
						 struct rpc_request_hdr *req, unsigned len,
						 void (*update_atcmd_state)(char *cmd, int state) )
{
	struct rpc_ats_atcmd_args *args = (struct rpc_ats_atcmd_args *)(req + 1);
	int result = HANDLE_OK;
	char ret_string[MAX_STRING_RET];
	uint32_t ret_value1 =0;
	uint32_t ret_value2 = 0;
	uint32_t at_cmd;
	uint32_t at_act;
	uint32_t at_param;

	at_cmd = be32_to_cpu(args->at_cmd);
	at_act = be32_to_cpu(args->at_act);
	at_param = be32_to_cpu(args->at_param);
	printk(KERN_INFO "%s: at_cmd = %d, at_act=%d, at_param=%d:\n",
		   __func__, args->at_cmd, args->at_act,args->at_param);

	memset(server->retvalue.ret_string, 0, sizeof(server->retvalue.ret_string));

	memset (ret_string, 0, sizeof(ret_string));

	switch (at_cmd)
	{
	case ATCMD_ACS:	//31
		if(at_act != ATCMD_ACTION)
			result = HANDLE_FAIL;
		update_atcmd_state("acs", at_param); //state is up? down?
		break;
	case ATCMD_VLC:	//36
		if(at_act != ATCMD_ACTION)
			result = HANDLE_FAIL;
		update_atcmd_state("vlc", at_param); //state is up? down?
		break;
	case ATCMD_SPM:	//40
		if(at_act != ATCMD_ACTION)
			result = HANDLE_FAIL;
		update_atcmd_state("spm", at_param); //state is up? down?
		break;
	case ATCMD_MPT:	//43
		if(at_act != ATCMD_ACTION)
			result = HANDLE_FAIL;
		update_atcmd_state("mpt", at_param); //state is up? down?
		break;
	case ATCMD_FMR: //42
		if(at_act != ATCMD_ACTION)
			result = HANDLE_FAIL;
		update_atcmd_state("fmr", at_param); //state is up? down?
		break;
		/*[LGE_CHANGE_S][Camera][kwangsoo.park@lge.com 2010-05-26 : AT Command AT_CAM AT_AVR */
	case ATCMD_AVR:	//45
		if(at_act != ATCMD_ACTION)
			result = HANDLE_FAIL;
		update_atcmd_state("avr", at_param); //state is up? down?
		break;
	case ATCMD_CAM: //70
		if(at_act != ATCMD_ACTION)
			result = HANDLE_FAIL;
		update_atcmd_state("cam", at_param); //state is up? down?
		break;
		/*[LGE_CHANGE_E[Camera][kwangsoo.park@lge.com 2010-05-26 : AT Command AT_CAM AT_AVR */

	case ATCMD_EMT:  // 46
		ret_value1 = external_memory_test();
		break;

	case ATCMD_FC:  // 59
		if(at_act != ATCMD_ACTION)
			result = HANDLE_FAIL;
		ret_value1 = 0;
		break;

	case ATCMD_FO:  // 60
		if(at_act != ATCMD_ACTION)
			result = HANDLE_FAIL;
		ret_value1 = 0;
		break;

	//LGE_UPDATE_S ins.lee@lge.com 2010-06-21, add AT%FLIHGT
	case ATCMD_FLIGHT:  // 82
		if(at_act != ATCMD_ACTION)
			result = HANDLE_FAIL;

		wirte_flight_mode(at_param);	
		update_atcmd_state("flight", at_param); //state is up? down?
		break;
    //LGE_UPDATE_E ins.lee@lge.com 2010-06-21, add AT%FLIHGT

	case ATCMD_MMCFORMAT:  // 129
		if(at_act != ATCMD_ACTION)
			result = HANDLE_FAIL;
		update_atcmd_state("mmcformat", 0);
		update_atcmd_state("mmcformat", 1);
		update_atcmd_state("mmcformat", 9);
		break;

	default :
		result = HANDLE_ERROR;
		break;
	}

	/* give to RPC server result */
	strncpy(server->retvalue.ret_string, ret_string, MAX_STRING_RET);
	server->retvalue.ret_string[MAX_STRING_RET-1] = 0;
	server->retvalue.ret_value1 = ret_value1;
	server->retvalue.ret_value2 = ret_value2;
	/////////////////////////////////////////////////////////////////
	
	if(result == HANDLE_OK)

		result = RPC_RETURN_RESULT_OK;
	else
		result = RPC_RETURN_RESULT_ERROR;

	return result;
}
