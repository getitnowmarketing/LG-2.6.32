/*
 * arch/arm/mach-msm/lge/lge_mtd_direct_access.c
 *
 * Copyright (C) 2010 LGE, Inc
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

#include <asm/div64.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/err.h>
#include <linux/mtd/mtd.h>
#include <linux/sched.h>
// LGE_CHANGE [dojip.kim@lge.com] 2010-08-23, do something after cold boot
#include "lg_fw_diag_communication.h"

#if defined(CONFIG_MACH_MSM7X27_THUNDERC)

//20100728 myeonggyu.son@lge.com [MS690] for FOTA STO partition [START]
//#define MISC_PART_NUM 6
#define MISC_PART_NUM 7
// LGE_CHANGE_S [hoseok.kim@lge.com] 2010.09.20 // LG_FW_USERDATA_BACKUP, ask hoseok.kim@lge.com to apply to other model .
//#define MISC_PART_NUM 8
// LGE_CHANGE_E [hoseok.kim@lge.com] 2010.09.20 // LG_FW_USERDATA_BACKUP, ask hoseok.kim@lge.com to apply to other model .

//20100728 myeonggyu.son@lge.com [MS690] for FOTA STO partition [END]

#define MISC_PART_SIZE 4

//20100728 myeonggyu.son@lge.com [MS690] for FOTA STO partition [START]
//#define PERSIST_PART_NUM 7
#define PERSIST_PART_NUM 8
// LGE_CHANGE_S [hoseok.kim@lge.com] 2010.09.20 // LG_FW_USERDATA_BACKUP, ask hoseok.kim@lge.com to apply to other model .
//#define PERSIST_PART_NUM 9
// LGE_CHANGE_E [hoseok.kim@lge.com] 2010.09.20 // LG_FW_USERDATA_BACKUP, ask hoseok.kim@lge.com to apply to other model .
//20100728 myeonggyu.son@lge.com [MS690] for FOTA STO partition [END]

#define PERSIST_PART_SIZE 12
#define PAGE_NUM_PER_BLK 64
#define PAGE_SIZE_BYTE 2048
#else
#define MISC_PART_NUM	4
#endif

static struct mtd_info *mtd;

// kthread_lg_diag: page allocation failure. order:5, mode:0xd0, lge_init_mtd_access: error: cannot allocate memory
//static unsigned char *global_buf;
static unsigned char global_buf[PAGE_NUM_PER_BLK*PAGE_SIZE_BYTE];
static unsigned char *bbt;

static int pgsize;
static int bufsize;
static int ebcnt;
static int pgcnt;

int lge_erase_block(int ebnum);
int lge_write_block(int ebnum, unsigned char *buf, size_t size);
int lge_read_block(int ebnum, unsigned char *buf);

static int scan_for_bad_eraseblocks(void);
static int is_block_bad(int ebnum);

int init_mtd_access(int partition, int block);

static int dev;
static int target_block;
static int dummy_arg = 0;
/* LGE_CHANGE [sm.shim@lge.com] 2010-08-22, merge First Boot Complete Test from VS660 */
int boot_info = 0;

module_param(dev, int, S_IRUGO);
module_param(target_block, int, S_IRUGO);
/*LGE_CHANGES yongman.kwon 2010-09-07[MS690] : firstboot check [START]*/
//module_param(boot_info, int, S_IWUSR | S_IRUGO);
extern void lg_set_boot_info(void);
static void set_boot_info(const char *val, struct kernel_param *kp)
{
	//change ASCII -> int
	//atoi(*val); 48 means '0' as ASCII.
	boot_info = *val - 48;

	printk("[INFORPC]boot info = %d\n", boot_info);

	lg_set_boot_info();	
}
module_param_call(boot_info, set_boot_info, param_get_int, &dummy_arg, S_IWUSR | S_IRUGO);
/*LGE_CHANGES yongman.kwon 2010-09-07[MS690] : firstboot check [START]*/

static int test_init(void)
{
	int partition = PERSIST_PART_NUM;
	int block = 0;

	return init_mtd_access(partition, block);
}
module_param_call(init, test_init, param_get_bool, &dummy_arg, S_IWUSR | S_IRUGO);

static int test_erase_block(void)
{
	int i;
	int err;

	/* Erase eraseblock */
	printk(KERN_INFO"%s: erasing block\n", __func__);

	for (i = 0; i < ebcnt; i++) {
		if (bbt[i])
			continue;
		else
			break;
	}

	err = lge_erase_block(i);
	if (err) {
		printk(KERN_INFO"%s: erased %u block fail\n", __func__, i);
		return err;
	}

	printk(KERN_INFO"%s: erased %u block\n", __func__, i);

	return 0;
}
module_param_call(erase_block, test_erase_block, param_get_bool, &dummy_arg, S_IWUSR | S_IRUGO);

static int test_write_block(const char *val, struct kernel_param *kp)
{
	int i;
	int err;
/* LGE_CHANGE_S [sm.shim@lge.com] 2010-08-22, merge First Boot Complete Test from VS660 */
	unsigned char *test_string;
	unsigned long flag=0;
	// LGE_CHANGE [dojip.kim@lge.com] 2010-08-23, do something after cold boot
	struct diagcmd_dev *diagpdev;

	flag = simple_strtoul(val,NULL,10);
	if(flag==5)
		test_string="FACT_RESET_5";
	else if(flag==6)
		test_string="FACT_RESET_6";
	// LGE_CHANGE [dojip.kim@lge.com] 2010-09-04, for RTN and Factory reset
	else if (flag==3)
		test_string="FACT_RESET_3";
	else
		return -1;

	test_init();
	test_erase_block();
	printk(KERN_INFO"%s: writing block: flag = %lu\n", __func__, flag);
/* LGE_CHANGE_E [sm.shim@lge.com] 2010-08-22, merge First Boot Complete Test from VS660 */

	for (i = 0; i < ebcnt; i++) {
		if (bbt[i])
			continue;
		else
			break;
	}

	err = lge_write_block(i, test_string, strlen(test_string));
	if (err) {
		printk(KERN_INFO"%s: write %u block fail\n", __func__, i);
		return err;
	}

	printk(KERN_INFO"%s: write %u block\n", __func__, i);

	// LGE_CHANGE [dojip.kim@lge.com] 2010-08-23, do something after cold boot
	// LGE_CHANGE [hoseok.kim@lge.com] 2010-10-07, do something after cold boot
	if (flag == 5 ||flag == 6 ) {
		diagpdev = diagcmd_get_dev();	
		if (diagpdev != NULL) {
			update_diagcmd_state(diagpdev, "ADBSET", 0);
		}
	}

	return 0;
}
module_param_call(write_block, test_write_block, param_get_bool, &dummy_arg, S_IWUSR | S_IRUGO);

/* LGE_CHANGE_S [sm.shim@lge.com] 2010-08-22, merge First Boot Complete Test from VS660 */
#define FACTORY_RESET_STR_SIZE 11
#define FACTORY_RESET_STR "FACT_RESET_"
static int test_read_block(char *buf, struct kernel_param *kp)
{
	int i;
	int err;
	unsigned char status=0;
	char* temp = "1";

	printk(KERN_INFO"%s: read block\n", __func__);
	test_init();

	for (i = 0; i < ebcnt; i++) {
		if (bbt[i])
			continue;
		else
			break;
	}

	err = lge_read_block(i, global_buf);
	if (err) {
		printk(KERN_INFO"%s: read %u block fail\n", __func__, i);
		goto error;
	}

	printk(KERN_INFO"%s: read %u block\n", __func__, i);
	printk(KERN_INFO"%s: %s\n", __func__, global_buf);
	
	if(memcmp(global_buf, FACTORY_RESET_STR, FACTORY_RESET_STR_SIZE)==0){
		status = global_buf[FACTORY_RESET_STR_SIZE];
		err = sprintf(buf,"%s",global_buf+FACTORY_RESET_STR_SIZE);
		return err;
	}
	else{
error:
		err = sprintf(buf,"%s",temp);
		return err;
	}
	
}
module_param_call(read_block, param_get_bool, test_read_block, &dummy_arg, (S_IWUSR | S_IRUGO));
/* LGE_CHANGE_E [sm.shim@lge.com] 2010-08-22, merge First Boot Complete Test from VS660 */

int lge_erase_block(int ebnum)
{
	int err;
	struct erase_info ei;
	loff_t addr = ebnum * mtd->erasesize;

	memset(&ei, 0, sizeof(struct erase_info));
	ei.mtd  = mtd;
	ei.addr = addr;
	ei.len  = mtd->erasesize;

	err = mtd->erase(mtd, &ei);
	if (err) {
		printk(KERN_INFO"%s: error %d while erasing EB %d\n", __func__,  err, ebnum);
		return err;
	}

	if (ei.state == MTD_ERASE_FAILED) {
		printk(KERN_INFO"%s: some erase error occurred at EB %d\n", __func__,
		       ebnum);
		return -EIO;
	}

	return 0;
}
EXPORT_SYMBOL(lge_erase_block);

int lge_write_block(int ebnum, unsigned char *buf, size_t size)
{
	int err = 0;
	size_t written = 0;
	loff_t addr = ebnum * mtd->erasesize;

	memset(global_buf, 0, mtd->erasesize);
	memcpy(global_buf, buf, size);
	err = mtd->write(mtd, addr, pgsize, &written, global_buf);
	if (err || written != pgsize)
		printk(KERN_INFO"%s: error: write failed at %#llx\n",
		      __func__, (long long)addr);

	return err;
}
EXPORT_SYMBOL(lge_write_block);

int lge_read_block(int ebnum, unsigned char *buf)
{
	int err = 0;
	size_t read = 0;
	loff_t addr = ebnum * mtd->erasesize;

	//memset(buf, 0, mtd->erasesize); 
	memset(buf, 0, pgsize);
	err = mtd->read(mtd, addr, pgsize, &read, buf);
	if (err || read != pgsize)
		printk(KERN_INFO"%s: error: read failed at %#llx\n",
		      __func__, (long long)addr);

	return err;
}
EXPORT_SYMBOL(lge_read_block);

static int is_block_bad(int ebnum)
{
	loff_t addr = ebnum * mtd->erasesize;
	int ret;

	ret = mtd->block_isbad(mtd, addr);
	if (ret)
		printk(KERN_INFO"%s: block %d is bad\n", __func__, ebnum);
	return ret;
}

static int scan_for_bad_eraseblocks(void)
{
	int i, bad = 0;

	bbt = kmalloc(ebcnt, GFP_KERNEL);
	if (!bbt) {
		printk(KERN_INFO"%s: error: cannot allocate memory\n", __func__);
		return -ENOMEM;
	}
	memset(bbt, 0 , ebcnt);

	printk(KERN_INFO"%s: scanning for bad eraseblocks\n", __func__);
	for (i = 0; i < ebcnt; ++i) {
		bbt[i] = is_block_bad(i) ? 1 : 0;
		if (bbt[i])
			bad += 1;
		cond_resched();
	}
	printk(KERN_INFO"%s: scanned %d eraseblocks, %d are bad\n", __func__, i, bad);
	return 0;
}

int init_mtd_access(int partition, int block)
{
	int err = 0;
	uint64_t tmp;

	dev = partition;
	target_block = block;
	
	mtd = get_mtd_device(NULL, dev);
	if (IS_ERR(mtd)) {
		err = PTR_ERR(mtd);
		printk(KERN_INFO"%s: cannot get MTD device\n", __func__);
		return err;
	}

	if (mtd->type != MTD_NANDFLASH) {
		printk(KERN_INFO"%s: this test requires NAND flash\n", __func__);
		goto out;
	}

	if (mtd->writesize == 1) {
		printk(KERN_INFO"%s: not NAND flash, assume page size is 512 "
		       "bytes.\n", __func__);
		pgsize = 512;
	} else
		pgsize = mtd->writesize;

	tmp = mtd->size;
	do_div(tmp, mtd->erasesize);
	ebcnt = tmp;
	pgcnt = mtd->erasesize / mtd->writesize;

	printk(KERN_INFO"%s: MTD device size %llu, eraseblock size %u, "
	       "page size %u, count of eraseblocks %u, pages per "
	       "eraseblock %u, OOB size %u\n", __func__,
	       (unsigned long long)mtd->size, mtd->erasesize,
	       pgsize, ebcnt, pgcnt, mtd->oobsize);

	err = -ENOMEM;
	bufsize = pgsize * 2;
	
	#if 0 
	global_buf = kmalloc(mtd->erasesize, GFP_KERNEL);
	if (!global_buf) {
		printk(KERN_INFO"%s: error: cannot allocate memory\n", __func__);
		goto out;
	}
	#endif
	err = scan_for_bad_eraseblocks();
	if (err)
		goto out;

	return 0;

out:
	return err;
}

//FACTORY_RESET
unsigned int lge_get_mtd_part_info(void)
{
	unsigned int result = 0;

	result = (PERSIST_PART_NUM<<16) | (PERSIST_PART_SIZE&0x0000FFFF);
	printk(KERN_INFO"%s: part_num : %d, count : %d\n", __func__, PERSIST_PART_NUM, PERSIST_PART_SIZE);
	return result;
}

int lge_get_mtd_factory_mode_blk(int target_blk)
{
	int i = 0;
	int factory_mode_blk = 0;

	for (i = 0; i < ebcnt; i++) {
		bbt[i] = is_block_bad(i) ? 1 : 0;
		if(bbt[i])
			continue;
		else
		{
			factory_mode_blk += 1;
			if(factory_mode_blk == target_blk)
				return i;
		}
	}
	return -1;
}

static int __init lge_mtd_direct_access_init(void)
{
	printk(KERN_INFO"%s: finished\n", __func__);

	return 0;
}

static void __exit lge_mtd_direct_access_exit(void)
{
	return;
}

module_init(lge_mtd_direct_access_init);
module_exit(lge_mtd_direct_access_exit);

MODULE_DESCRIPTION("LGE mtd direct access apis");
MODULE_AUTHOR("SungEun Kim <cleaneye.kim@lge.com>");
MODULE_LICENSE("GPL");
