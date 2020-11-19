#include "bm_common.h"
#include "bm_io.h"
#include "bm1684_irq.h"

#ifndef SOC_MODE
int bm1684_clear_msgirq(struct bm_device_info *bmdi)
{
	int irq_status = 0;

	if (bmdi->cinfo.irq_id == MSG_IRQ_ID_CHANNEL_XPU) {
		irq_status = top_reg_read(bmdi, TOP_GP_REG_ARM9_IRQ_STATUS_OFFSET);

		if ((irq_status & (MSG_DONE_IRQ_MASK)) == 0x1) {
			top_reg_write(bmdi, TOP_GP_REG_ARM9_IRQ_CLEAR_OFFSET, MSG_DONE_IRQ_MASK);
			return 0;
		} else {
			return -1;
		}
	} else {
		irq_status = top_reg_read(bmdi, TOP_GP_REG_ARM9_IRQ_STATUS_OFFSET);

		if ((irq_status & (CPU_MSG_DONE_IRQ_MASK)) == CPU_MSG_DONE_IRQ_MASK) {
			top_reg_write(bmdi, TOP_GP_REG_ARM9_IRQ_CLEAR_OFFSET, CPU_MSG_DONE_IRQ_MASK);
			return 0;
		} else {
			return -1;
		}
	}
}
#else
int bm1684_clear_msgirq(struct bm_device_info *bmdi)
{
	int irq_status = top_reg_read(bmdi, TOP_GP_REG_A53_IRQ_STATUS_OFFSET);

	if ((irq_status & (MSG_DONE_IRQ_MASK)) == 0x1) {
		top_reg_write(bmdi, TOP_GP_REG_A53_IRQ_CLEAR_OFFSET, MSG_DONE_IRQ_MASK);
		return 0;
	} else {
		return -1;
	}
}
#endif

u32 bm1684_pending_msgirq_cnt(struct bm_device_info *bmdi)
{
	return 1;
}
