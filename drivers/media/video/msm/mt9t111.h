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

#ifndef MT9T111_H
#define MT9T111_H

#include <linux/types.h>
#include <mach/camera.h>
#define TO_BE_READ_SIZE 4000*8	// 8pages (4096x8)

extern struct mt9t111_reg mt9t111_regs;

enum mt9t111_width {
	WORD_LEN,
	BYTE_LEN
};

struct mt9t111_i2c_reg_conf {
	unsigned short waddr;
	unsigned short wdata;
	enum mt9t111_width width;
	unsigned short mdelay_time;
};

struct mt9t111_reg {
	const struct register_address_value_pair *prev_snap_reg_settings;
	uint16_t prev_snap_reg_settings_size;
	const struct register_address_value_pair *noise_reduction_reg_settings;
	uint16_t noise_reduction_reg_settings_size;
	const struct mt9t111_i2c_reg_conf *plltbl;
	uint16_t plltbl_size;
	const struct mt9t111_i2c_reg_conf *stbl;
	uint16_t stbl_size;
	const struct mt9t111_i2c_reg_conf *rftbl;
	uint16_t rftbl_size;
	struct register_address_value_pair *prev_init;
	uint16_t prev_init_size;
	struct register_address_value_pair *prev_group_1;
	uint16_t prev_group_1_size;
	struct register_address_value_pair *prev_group_2;
	uint16_t prev_group_2_size;
	struct register_address_value_pair *prev_group_3;
	uint16_t prev_group_3_size;
	struct register_address_value_pair *prev_group_4;
	uint16_t prev_group_4_size;
	struct register_address_value_pair *prev_group_5;
	uint16_t prev_group_5_size;
	struct register_address_value_pair *prev_group_6;
	uint16_t prev_group_6_size;
	struct register_address_value_pair *prev_group_7;
	uint16_t prev_group_7_size;
	struct register_address_value_pair *prev_group_8;
	uint16_t prev_group_8_size;
	struct register_address_value_pair *prev_group_9;
	uint16_t prev_group_9_size;
	struct register_address_value_pair *prev_group_10;
	uint16_t prev_group_10_size;
	struct register_address_value_pair *prev_group_11;
	uint16_t prev_group_11_size;
	struct register_address_value_pair *prev_group_12;
	uint16_t prev_group_12_size;
	struct register_address_value_pair *prev_group_13;
	uint16_t prev_group_13_size;
	struct register_address_value_pair *prev_group_14;
	uint16_t prev_group_14_size;
	struct register_address_value_pair *prev_group_15;
	uint16_t prev_group_15_size;

};

#endif /* MT9T111_H */
