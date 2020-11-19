#include <linux/device.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include <linux/version.h>
#include "bm_common.h"
#include "bm1682_reg.h"
#include "bm_thread.h"
#include "bm_timer.h"
#include "bm_irq.h"
#ifndef SOC_MODE
#include "bm1682_pcie.h"
#endif

/* CDMA bit definition */
#define BM1682_CDMA_RESYNC_ID_BIT                  2
#define BM1682_CDMA_ENABLE_BIT                     0
#define BM1682_CDMA_INT_ENABLE_BIT                 3

#define BM1682_CDMA_EOD_BIT                        2
#define BM1682_CDMA_PLAIN_DATA_BIT                 6
#define BM1682_CDMA_OP_CODE_BIT                    7

u32 bm1682_cdma_transfer(struct bm_device_info *bmdi, struct file *file, pbm_cdma_arg parg, bool lock_cdma)
{
	u64 ret_wait = 0;
	u8  src_addr_hi = 0;
	u32 src_addr_lo = 0;
	u8  dst_addr_hi = 0;
	u32 dst_addr_lo = 0;
	u32 max_payload = 0;
	u32 cdma_max_payload = 0;
	u32 interleave_reg_val = 0;
	u64 src = parg->src;
	u64 dst = parg->dst;
	u32 timeout_ms = bmdi->cinfo.delay_ms;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
	struct timespec64 cdma_start_time, cdma_cur_time;
#else
	struct timeval cdma_start_time, cdma_cur_time;
#endif

#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		timeout_ms *= PALLADIUM_CLK_RATIO;
#endif
	if (lock_cdma)
		mutex_lock(&memcpy_info->cdma_mutex);

	if (parg->intr) {
		u32 int_mask_val = cdma_reg_read(bmdi, CDMA_INT_MASK);

		int_mask_val &= 0xfffffffe;
		cdma_reg_write(bmdi, CDMA_INT_MASK, int_mask_val);
	}

	//reset sync id
	cdma_reg_write(bmdi, CDMA_MAIN_CTRL, 1 << BM1682_CDMA_RESYNC_ID_BIT);

#ifndef SOC_MODE
	if (parg->dir == HOST2CHIP)
		src |= (u64)1<<39;
	else
		dst |= (u64)1<<39;
#endif

	src_addr_hi = src >> 32;
	src_addr_lo = src & 0xffffffff;
	dst_addr_hi = dst >> 32;
	dst_addr_lo = dst & 0xffffffff;

	cdma_reg_write(bmdi, CDMA_CMD_ACCP3, dst_addr_lo);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP4, src_addr_lo);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP5, parg->size/4);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP7, parg->size/4);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP2, 0);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP1, 1);

	max_payload = 1;
	cdma_max_payload = cdma_reg_read(bmdi, CDMA_CMD_9F8);
	cdma_max_payload = (cdma_max_payload & 0xfffffff7) | max_payload;
	cdma_reg_write(bmdi, CDMA_CMD_9F8, cdma_max_payload);
	interleave_reg_val = cdma_reg_read(bmdi, CDMA_CMD_9FC);
	interleave_reg_val = (interleave_reg_val & (0xffff00fe)) | ((2 << 8) + 1);
	cdma_reg_write(bmdi, CDMA_CMD_9FC, interleave_reg_val);

    /* Using interrupt(10s timeout) or polling for detect cmda done */
	if (parg->intr) {
		cdma_reg_write(bmdi, CDMA_CMD_ACCP0, (src_addr_hi<<24)
				| (dst_addr_hi<<16)
				| (1 << BM1682_CDMA_OP_CODE_BIT)
				| (1 << BM1682_CDMA_EOD_BIT)
				| (1 << BM1682_CDMA_ENABLE_BIT)
				| (1 << BM1682_CDMA_PLAIN_DATA_BIT)
				| (1 << BM1682_CDMA_INT_ENABLE_BIT));
		cdma_reg_write(bmdi, CDMA_MAIN_CTRL, (1 << BM1682_CDMA_ENABLE_BIT)
				| (1 << BM1682_CDMA_INT_ENABLE_BIT));

		ret_wait = wait_for_completion_timeout(&memcpy_info->cdma_done,
			msecs_to_jiffies(timeout_ms));
		if (ret_wait)
			PR_TRACE("End : wait cdma done\n");
		else {
			PR_TRACE("End : wait cdma timeout!\n");
			if (lock_cdma)
				mutex_unlock(&memcpy_info->cdma_mutex);
			return -EBUSY;
		}
	} else {
		cdma_reg_write(bmdi, CDMA_CMD_ACCP0, (src_addr_hi<<24)
				| (dst_addr_hi<<16)
				| (1 << BM1682_CDMA_OP_CODE_BIT)
				| (1 << BM1682_CDMA_EOD_BIT)
				| (1 << BM1682_CDMA_ENABLE_BIT)
				| (1 << BM1682_CDMA_PLAIN_DATA_BIT));
		cdma_reg_write(bmdi, CDMA_MAIN_CTRL, 1 << BM1682_CDMA_ENABLE_BIT);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
		ktime_get_real_ts64(&cdma_start_time);
#else
		do_gettimeofday(&cdma_start_time);
#endif
		while ((cdma_reg_read(bmdi, CDMA_SYNC_STAT) & 0xffff0000) != 0x10000) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
			ktime_get_real_ts64(&cdma_cur_time);
#else
			do_gettimeofday(&cdma_cur_time);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
			if (cdma_cur_time.tv_sec * 1000 + cdma_cur_time.tv_nsec/1000000
					- cdma_start_time.tv_sec*1000 -
					cdma_start_time.tv_nsec/1000000 > timeout_ms){
#else
			if (cdma_cur_time.tv_sec * 1000 + cdma_cur_time.tv_usec/1000 -
				cdma_start_time.tv_sec*1000 - cdma_start_time.tv_usec/1000 > timeout_ms){
#endif
				pr_err("cdma trans over time src %x:%x dst %x:%x\n",
						src_addr_hi, src_addr_lo, dst_addr_hi, dst_addr_lo);
				break;
			}
		}
		PR_TRACE("cdma transfer using pool mode end\n");
	}
	if (lock_cdma)
		mutex_unlock(&memcpy_info->cdma_mutex);
	return 0;
}

#ifdef SOC_MODE
void bm1682_clear_cdmairq(struct bm_device_info *bmdi)
{
	cdma_reg_write(bmdi, CDMA_INT_STATUS, 0x1);
}
#else
void bm1682_clear_cdmairq(struct bm_device_info *bmdi)
{
	gp_reg_write_enh(bmdi, GP_REG_CDMA_IRQSTATUS, 0);
}
#endif
