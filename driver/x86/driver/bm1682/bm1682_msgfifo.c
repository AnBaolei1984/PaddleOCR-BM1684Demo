#include "bm_common.h"
#include "bm_io.h"
#include "bm1682_reg.h"

#ifndef SOC_MODE
int bm1682_clear_msgirq(struct bm_device_info *bmdi)
{
	gp_reg_write_enh(bmdi, GP_REG_MESSAGE_IRQSTATUS, 0);
	return 0;
}
#else
// use GDMA IRQ to emulate MSG IRQ
int bm1682_clear_msgirq(struct bm_device_info *bmdi)
{
	u32 irq_status = bm_read32(bmdi, GDMA_ENIGNE_BASE_ADDR + GDMA_ENIGNE_INTERRUPT_STATUS);

	if (irq_status & GDMA_SIMU_PCIE_INTSTATUS_CLEAR) {
		bm_write32(bmdi, GDMA_ENIGNE_BASE_ADDR + GDMA_ENIGNE_INTERRUPT_STATUS,
				GDMA_SIMU_PCIE_INTSTATUS_CLEAR | (irq_status &
				GDMA_SIMU_PCIE_INTSTATUS_MASK));
		gp_reg_write_enh(bmdi, GP_REG_MESSAGE_IRQSTATUS, 0);
	} else {
		return -1;
	}
	return 0;
}
#endif

u32 bm1682_pending_msgirq_cnt(struct bm_device_info *bmdi)
{
	u64 new_msgirq_num;
	u32 pending_msgirq_cnt;
	u32 lo, hi;
	u64 first_num, second_num;
	struct bm_api_info *api_info;

#ifdef PCIE_MODE_ENABLE_CPU
	if (bmdi->cinfo.irq_id == MSG_IRQ_ID_CHANNEL_XPU)
		api_info = &bmdi->api_info[BM_MSGFIFO_CHANNEL_XPU];
	else
		api_info = &bmdi->api_info[BM_MSGFIFO_CHANNEL_CPU];
#else
	api_info = &bmdi->api_info[BM_MSGFIFO_CHANNEL_XPU];
#endif
	lo = gp_reg_read_enh(bmdi, GP_REG_MSGIRQ_NUM_LO);
	hi = gp_reg_read_enh(bmdi, GP_REG_MSGIRQ_NUM_HI);
	first_num = ((u64)hi << 32) | lo;

	lo = gp_reg_read_enh(bmdi, GP_REG_MSGIRQ_NUM_LO);
	hi = gp_reg_read_enh(bmdi, GP_REG_MSGIRQ_NUM_HI);
	second_num = ((u64)hi << 32) | lo;

	while (first_num != second_num) {
		first_num = second_num;
		lo = gp_reg_read_enh(bmdi, GP_REG_MSGIRQ_NUM_LO);
		hi = gp_reg_read_enh(bmdi, GP_REG_MSGIRQ_NUM_HI);
		second_num = ((u64)hi << 32) | lo;
	}
	new_msgirq_num = first_num;
	pending_msgirq_cnt = new_msgirq_num - api_info->msgirq_num;
	api_info->msgirq_num = new_msgirq_num;
	return pending_msgirq_cnt;
}
