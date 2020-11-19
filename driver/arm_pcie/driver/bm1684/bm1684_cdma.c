#include <linux/device.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include "bm_common.h"
#include "bm1684_reg.h"
#include "bm_thread.h"
#include "bm_timer.h"
#include "bm_irq.h"

#ifdef SOC_MODE
#include <linux/reset.h>
#include <linux/clk.h>
#else
#include "bm1684_pcie.h"
#endif

/* CDMA bit definition */
#define BM1684_CDMA_RESYNC_ID_BIT                  2
#define BM1684_CDMA_ENABLE_BIT                     0
#define BM1684_CDMA_INT_ENABLE_BIT                 3

#define BM1684_CDMA_EOD_BIT                        2
#define BM1684_CDMA_PLAIN_DATA_BIT                 6
#define BM1684_CDMA_OP_CODE_BIT                    7

void bm1684_clear_cdmairq(struct bm_device_info *bmdi)
{
	int reg_val;

	reg_val = cdma_reg_read(bmdi, CDMA_INT_STATUS);
	reg_val |= (1 << 0x09);
	cdma_reg_write(bmdi, CDMA_INT_STATUS, reg_val);
}

u32 bm1684_cdma_transfer(struct bm_device_info *bmdi, struct file *file, pbm_cdma_arg parg, bool lock_cdma)
{
	struct bm_thread_info *ti = NULL;
	struct bm_handle_info *h_info = NULL;
	struct bm_trace_item *ptitem = NULL;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
	pid_t api_pid;
	u32 timeout_ms = bmdi->cinfo.delay_ms;
	u64 ret_wait = 0;
	u32 src_addr_hi = 0;
	u32 src_addr_lo = 0;
	u32 dst_addr_hi = 0;
	u32 dst_addr_lo = 0;
	u32 cdma_max_payload = 0;
	u32 reg_val = 0;
	u64 src = parg->src;
	u64 dst = parg->dst;
	u32 count = 3000;
	u64 nv_cdma_start_us = 0;
	u64 nv_cdma_end_us = 0;
	u64 nv_cdma_send_us = 0;
	u32 lock_timeout = timeout_ms * 1000;
	u32 int_mask_val;


	nv_cdma_send_us = bmdev_timer_get_time_us(bmdi);
#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		timeout_ms *= PALLADIUM_CLK_RATIO;
#endif
	if (file) {
		if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
			pr_err("bmdrv: file list is not found!\n");
			return -EINVAL;
		}
	}
	if (lock_cdma)
		mutex_lock(&memcpy_info->cdma_mutex);

	//Check the cdma is used by others(ARM9) or not.
	lock_timeout = timeout_ms * 1000;

	while (top_reg_read(bmdi, TOP_CDMA_LOCK)) {
		udelay(1);
		if (--lock_timeout == 0) {
			pr_err("cdma resource wait timeout\n");
			if (lock_cdma)
				mutex_unlock(&memcpy_info->cdma_mutex);
			return -EBUSY;
		}
	}
	PR_TRACE("CDMA lock wait %dus\n", timeout_ms * 1000 - lock_timeout);

	api_pid = current->pid;
	ti = bmdrv_find_thread_info(h_info, api_pid);

	if (ti && (parg->dir == HOST2CHIP)) {
		ti->profile.cdma_out_counter++;
		bmdi->profile.cdma_out_counter++;
	} else {
		if (ti) {
			ti->profile.cdma_in_counter++;
			bmdi->profile.cdma_in_counter++;
		}
	}

	if (ti && ti->trace_enable) {
		ptitem = (struct bm_trace_item *)mempool_alloc(bmdi->trace_info.trace_mempool, GFP_KERNEL);
		ptitem->payload.trace_type = 0;
		ptitem->payload.cdma_dir = parg->dir;
		ptitem->payload.sent_time = nv_cdma_send_us;
		ptitem->payload.start_time = 0;
	}
	/*Disable CDMA*/
	reg_val = cdma_reg_read(bmdi, CDMA_MAIN_CTRL);
	reg_val &= ~(1 << BM1684_CDMA_ENABLE_BIT);
	cdma_reg_write(bmdi, CDMA_MAIN_CTRL, reg_val);
	/*Set PIO mode*/
	reg_val = cdma_reg_read(bmdi, CDMA_MAIN_CTRL);
	reg_val &= ~(1 << 1);
	cdma_reg_write(bmdi, CDMA_MAIN_CTRL, reg_val);

	int_mask_val = cdma_reg_read(bmdi, CDMA_INT_MASK);
	int_mask_val &= ~(1 << 9);
	cdma_reg_write(bmdi, CDMA_INT_MASK, int_mask_val);

#ifdef SOC_MODE
	if (!parg->use_iommu) {
		src |= (u64)0x3f<<36;
		dst |= (u64)0x3f<<36;
	}
#else
	if (parg->dir != CHIP2CHIP) {
		if (!parg->use_iommu) {
			if (parg->dir == CHIP2HOST)
				src |= (u64)0x3f<<36;
			else
				dst |= (u64)0x3f<<36;
		}
	}
#endif
	src_addr_hi = src >> 32;
	src_addr_lo = src & 0xffffffff;
	dst_addr_hi = dst >> 32;
	dst_addr_lo = dst & 0xffffffff;
	PR_TRACE("src:0x%llx dst:0x%llx size:%lld\n", src, dst, parg->size);
	PR_TRACE("src_addr_hi 0x%x src_addr_low 0x%x\n", src_addr_hi, src_addr_lo);
	PR_TRACE("dst_addr_hi 0x%x dst_addr_low 0x%x\n", dst_addr_hi, dst_addr_lo);

	cdma_reg_write(bmdi, CDMA_CMD_ACCP3, src_addr_lo);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP4, src_addr_hi);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP5, dst_addr_lo);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP6, dst_addr_hi);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP7, parg->size);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP2, 0);
	cdma_reg_write(bmdi, CDMA_CMD_ACCP1, 1);

	/*set max payload which equal to PCIE bandwidth.*/
#ifdef SOC_MODE
	cdma_max_payload = 0x1;
#else
	cdma_max_payload = memcpy_info->cdma_max_payload;
#endif
	cdma_max_payload &= 0x7;

	cdma_reg_write(bmdi, CDMA_CMD_9F8, cdma_max_payload);

	/* Using interrupt(10s timeout) or polling for detect cmda done */
	if (parg->intr) {

		cdma_reg_read(bmdi, CDMA_MAIN_CTRL);
		reg_val |= ((1 << BM1684_CDMA_ENABLE_BIT) | (1 << BM1684_CDMA_INT_ENABLE_BIT));
		cdma_reg_write(bmdi, CDMA_MAIN_CTRL, reg_val);
		nv_cdma_start_us = bmdev_timer_get_time_us(bmdi);
		cdma_reg_write(bmdi, CDMA_CMD_ACCP0, (1 << BM1684_CDMA_ENABLE_BIT)
				| (1 << BM1684_CDMA_INT_ENABLE_BIT));
		PR_TRACE("wait cdma\n");
		ret_wait = wait_for_completion_timeout(&memcpy_info->cdma_done,
				msecs_to_jiffies(timeout_ms));
		nv_cdma_end_us = bmdev_timer_get_time_us(bmdi);
		PR_TRACE("src:0x%llx dst:0x%llx size:%llx\n", src, dst, parg->size);
		PR_TRACE("time = %lld\n", nv_cdma_end_us - nv_cdma_start_us);
		PR_TRACE("time = %lld, function_num = %d, start = %lld, end = %lld, size = %llx, max_payload = %d\n",
			nv_cdma_end_us - nv_cdma_start_us, bmdi->cinfo.chip_index&0x3,
			nv_cdma_start_us, nv_cdma_end_us, parg->size, cdma_max_payload);

		if (ret_wait) {
			PR_TRACE("End : wait cdma done\n");
		} else {
			pr_info("End : wait cdma timeout!\n");
			top_reg_write(bmdi, TOP_CDMA_LOCK, 0);
			if (lock_cdma)
				mutex_unlock(&memcpy_info->cdma_mutex);
			return -EBUSY;
		}
	} else {
		cdma_reg_read(bmdi, CDMA_MAIN_CTRL);
		reg_val |= (1 << BM1684_CDMA_ENABLE_BIT);
		reg_val &= ~(1 << BM1684_CDMA_INT_ENABLE_BIT);
		cdma_reg_write(bmdi, CDMA_MAIN_CTRL, reg_val);
		nv_cdma_start_us = bmdev_timer_get_time_us(bmdi);
		cdma_reg_write(bmdi, CDMA_CMD_ACCP0, (1 << BM1684_CDMA_ENABLE_BIT)
				| (1 << BM1684_CDMA_INT_ENABLE_BIT));
		while (((cdma_reg_read(bmdi, CDMA_INT_STATUS) >> 0x9) & 0x1) != 0x1) {
			udelay(1);
			if (--count == 0) {
				pr_err("cdma polling wait timeout\n");
				top_reg_write(bmdi, TOP_CDMA_LOCK, 0);
				if (lock_cdma)
					mutex_unlock(&memcpy_info->cdma_mutex);
				return -EBUSY;
			}
		}
		nv_cdma_end_us = bmdev_timer_get_time_us(bmdi);
		reg_val = cdma_reg_read(bmdi, CDMA_INT_STATUS);
		reg_val |= (1 << 0x09);
		cdma_reg_write(bmdi, CDMA_INT_STATUS, reg_val);

		PR_TRACE("cdma transfer using polling mode end\n");
	}

	if (ti && (parg->dir == HOST2CHIP)) {
		ti->profile.cdma_out_time += nv_cdma_end_us - nv_cdma_start_us;
		bmdi->profile.cdma_out_time += nv_cdma_end_us - nv_cdma_start_us;
	} else {
		if (ti) {
			ti->profile.cdma_in_time += nv_cdma_end_us - nv_cdma_start_us;
			bmdi->profile.cdma_in_time += nv_cdma_end_us - nv_cdma_start_us;
		}
	}

	if (ti && ti->trace_enable) {
		ptitem->payload.end_time = nv_cdma_end_us;
		ptitem->payload.start_time = nv_cdma_start_us;
		INIT_LIST_HEAD(&ptitem->node);
		mutex_lock(&ti->trace_mutex);
		list_add_tail(&ptitem->node, &ti->trace_list);
		ti->trace_item_num++;
		mutex_unlock(&ti->trace_mutex);
	}

	top_reg_write(bmdi, TOP_CDMA_LOCK, 0);

	if (lock_cdma)
		mutex_unlock(&memcpy_info->cdma_mutex);
	return 0;
}
#ifdef SOC_MODE
void bm1684_cdma_reset(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 cdma reset\n");
	reset_control_assert(bmdi->cinfo.cdma);
	udelay(1000);
	reset_control_deassert(bmdi->cinfo.cdma);
}

void bm1684_cdma_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 cdma clk is enable\n");
	clk_prepare_enable(bmdi->cinfo.cdma_clk);
}

void bm1684_cdma_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 cdma clk is gating\n");
	clk_disable_unprepare(bmdi->cinfo.cdma_clk);
}
#endif
