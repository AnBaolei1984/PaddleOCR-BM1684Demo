#include <linux/delay.h>
#include "bm_common.h"
#include "bm1682_reg.h"

void bm1682_stop_arm9(struct bm_device_info *bmdi)
{
	u32 ctrl_word;
	struct chip_info *cinfo = &bmdi->cinfo;
	/*send fiq to arm9, let it get ready to die*/
	bm_write32(bmdi, cinfo->bm_reg->intc_base_addr + 0xc8, 1);

	udelay(1000);

	/* reset arm9 */
	ctrl_word = top_reg_read(bmdi, TOP_ARM_RESET);
	ctrl_word &= ~(1 << 2);
	top_reg_write(bmdi, TOP_ARM_RESET, ctrl_word);

	/* switch the itcm to pcie */
	ctrl_word = top_reg_read(bmdi, TOP_ITCM_SWITCH);
	ctrl_word |= (1 << 10);
	top_reg_write(bmdi, TOP_ITCM_SWITCH, ctrl_word);
}

void bm1682_start_arm9(struct bm_device_info *bmdi)
{
	u32 ctrl_word;
	/* switch itcm to arm9 */
	ctrl_word = top_reg_read(bmdi, TOP_ITCM_SWITCH);
	ctrl_word &= ~(1 << 10);
	top_reg_write(bmdi, TOP_ITCM_SWITCH, ctrl_word);

	/* dereset arm9 */
	ctrl_word = top_reg_read(bmdi, TOP_ARM_RESET);
	ctrl_word |= (1 << 2);
	top_reg_write(bmdi, TOP_ARM_RESET, ctrl_word);
}

void bm1682_top_init(struct bm_device_info *bmdi)
{
// set the start address of arm9 memory to 0x2 0000 0000, we need to set the default top register 0x1c0
// ddr_addr_mode bit 3:0 to 0x4, the default value is 0x2
#ifdef SOC_MODE
	u32 addr_mode;

	addr_mode = top_reg_read(bmdi, TOP_ARM_ADDRMODE);
	addr_mode &= 0xFFFFFFF0;
	addr_mode |= 0x00000004;
	top_reg_write(bmdi, TOP_ARM_ADDRMODE, addr_mode);
#endif
	top_reg_write(bmdi, TOP_I2C1_CLK_DIV, 0x102);
	top_reg_write(bmdi, TOP_I2C2_CLK_DIV, 0x101);
}
