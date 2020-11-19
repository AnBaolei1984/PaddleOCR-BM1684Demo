#include <linux/types.h>
#include "bm_io.h"
#include "bm_common.h"
#include "bm1682_reg.h"

extern void bm1682_smmu_get_irq_status(struct bm_device_info *bmdi, u32 *status);

void bm1682_get_irq_status(struct bm_device_info *bmdi, u32 *status)
{
	u32 irq_status;

	irq_status = gp_reg_read_enh(bmdi, GP_REG_CDMA_IRQSTATUS);
	if (irq_status == IRQ_STATUS_CDMA_INT) {
		status[1] |= 1 << 14;
		PR_TRACE("The cdma irq received\n");
	}

	irq_status = gp_reg_read_enh(bmdi, GP_REG_MESSAGE_IRQSTATUS);
	if (irq_status == IRQ_STATUS_MSG_DONE_INT) {
		status[1] |= 1 << 16;
		PR_TRACE("The msg done irq received\n");
	}

	bm1682_smmu_get_irq_status(bmdi, status);
}
