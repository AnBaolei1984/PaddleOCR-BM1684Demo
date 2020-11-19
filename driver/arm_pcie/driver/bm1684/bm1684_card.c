#include <linux/delay.h>
#include "bm_common.h"
#include "bm_pcie.h"
#include "bm_card.h"
#include "l2_sram_table.h"
#include "bm1684_reg.h"
#include "bm1684_irq.h"

#ifdef SOC_MODE
#include <linux/reset.h>
#include <linux/clk.h>
#endif

#ifndef SOC_MODE
#include "i2c.h"

int bm1684_card_get_chip_num(struct bm_device_info *bmdi)
{
	int mode = 0x0;
	int num = 0x0;

	if (bmdi->bmcd != NULL)
		return bmdi->bmcd->chip_num;

	mode = bmdrv_pcie_get_mode(bmdi) & 0x7;

	switch (mode) {
	case 0x0:
	case 0x6:
	case 0x1:
		num = 0x3;
		break;
	case 0x4:
	case 0x3:
		num = 0x1;
		break;
	default:
		num = 0x1;
		break;
	}
	pr_info("chip num = %d\n", num);
	return num;
}

int bm1684_get_board_version_from_mcu(struct bm_device_info *bmdi)
{
	int board_version = 0;
	u8 mcu_sw_version, board_type, hw_version = 0;

	if (bmdi->cinfo.platform == PALLADIUM) {
		bmdi->cinfo.board_version = 0;
		return 0;
	}
	if (bm_mcu_read_reg(bmdi, 0, &board_type) != 0)
		return -1;
	if (bm_mcu_read_reg(bmdi, 2, &hw_version) != 0)
		return -1;
	if (bm_mcu_read_reg(bmdi, 1, &mcu_sw_version) != 0)
		return -1;

	if (board_type == 0x0 && hw_version == 0x4) {
		if (bm_get_board_type(bmdi, &board_type) != 0) {
			return -1;
		}
	}

	board_version = (int)board_type;
	board_version = board_version << 8 | (int)hw_version;
	board_version = board_version | (int)mcu_sw_version << 16;
	bmdi->cinfo.board_version = board_version;
	return 0;
}

int bm1684_get_board_type_by_id(struct bm_device_info *bmdi, char *s_board_type, int id)
{
	int ret = 0;

	switch (id) {
	case BOARD_TYPE_EVB:
		strncpy(s_board_type, "EVB", 10);
		break;
	case BOARD_TYPE_SC5:
		strncpy(s_board_type, "SC5", 10);
		break;
	case BOARD_TYPE_SA5:
		strncpy(s_board_type, "SA5", 10);
		break;
	case BOARD_TYPE_SE5:
		strncpy(s_board_type, "SE5", 10);
		break;
	case BOARD_TYPE_SM5_P:
		strncpy(s_board_type, "SM5_P", 10);
		break;
	case BOARD_TYPE_SM5_S:
		strncpy(s_board_type, "SM5_S", 10);
		break;
	case BOARD_TYPE_SA6:
		strncpy(s_board_type, "SA6", 10);
		break;
	case BOARD_TYPE_SC5_PLUS:
		strncpy(s_board_type, "SC5+", 10);
		break;
	case BOARD_TYPE_SC5_H:
		strncpy(s_board_type, "SC5H", 10);
		break;
	default:
		strncpy(s_board_type, "Error", 10);
		pr_info("Ivalid board type %d\n", id);
		ret = -1;
		break;
	}

	return ret;
}

int bm1684_get_board_version_by_id(struct bm_device_info *bmdi, char *s_board_version,
		int board_id, int pcb_version, int bom_version)
{
	int ret = 0;
	int board_version = (bom_version & 0xf) | ((pcb_version & 0xf) << 4);

	switch (board_id) {
	case BOARD_TYPE_EVB:
	case BOARD_TYPE_SC5:
		if (board_version == 0x4)
			strncpy(s_board_version, "V1_2", 10);
		else if (pcb_version == 0x0)
			strncpy(s_board_version, "V1_1", 10);
		else
			strncpy(s_board_version, "invalid", 10);
		break;
	case BOARD_TYPE_SA5:
		strncpy(s_board_version, "V1_0", 10);
		break;
	case BOARD_TYPE_SE5:
		strncpy(s_board_version, "V1_0", 10);
		break;
	case BOARD_TYPE_SM5_P:
	case BOARD_TYPE_SM5_S:
		if (pcb_version == 0x1)
			if (bom_version == 0x0)
				strncpy(s_board_version, "V1_0", 10);
			else if (bom_version == 0x1)
				strncpy(s_board_version, "V1_1", 10);
			else
				strncpy(s_board_version, "invalid", 10);
		else
			strncpy(s_board_version, "invalid", 10);
		break;
	case BOARD_TYPE_SA6:
		strncpy(s_board_version, "V1_0", 10);
		break;
	case BOARD_TYPE_SC5_H:
		strncpy(s_board_version, "V1_0", 10);
		break;
	case BOARD_TYPE_SC5_PLUS:
		if (board_version == 0x0)
			strncpy(s_board_version, "V1_0", 10);
		else if (board_version == 0x1)
			strncpy(s_board_version, "V1_1", 10);
		else
			strncpy(s_board_version, "invalid", 10);
		break;
	default:
		strncpy(s_board_version, "invalid", 10);
		pr_info("Ivalid bord type %d\n", board_id);
		ret = -1;
		break;
	}

	return ret;
}
#endif

void bm1684_stop_arm9(struct bm_device_info *bmdi)
{
	u32 ctrl_word;
	/*send  fiq 6 to arm9, let it get ready to die*/
	top_reg_write(bmdi, TOP_GP_REG_ARM9_IRQ_SET_OFFSET, 0x1 << 14);
#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		msleep(500);
#endif
	udelay(50);
#ifdef SOC_MODE
	reset_control_assert(bmdi->cinfo.arm9);
#else
	/* reset arm9 */
	ctrl_word = top_reg_read(bmdi, TOP_SW_RESET0);
	ctrl_word &= ~(1 << 1);
	top_reg_write(bmdi, TOP_SW_RESET0, ctrl_word);
#endif
	/* switch the itcm to pcie */
	ctrl_word = top_reg_read(bmdi, TOP_ITCM_SWITCH);
	ctrl_word |= (1 << 1);
	top_reg_write(bmdi, TOP_ITCM_SWITCH, ctrl_word);
}

void bm1684_start_arm9(struct bm_device_info *bmdi)
{
	u32 ctrl_word;
	/* switch itcm to arm9 */
	ctrl_word = top_reg_read(bmdi, TOP_ITCM_SWITCH);
	ctrl_word &= ~(1 << 1);
	top_reg_write(bmdi, TOP_ITCM_SWITCH, ctrl_word);
#ifdef SOC_MODE
	reset_control_deassert(bmdi->cinfo.arm9);
#else
	/* dereset arm9 */
	ctrl_word = top_reg_read(bmdi, TOP_SW_RESET0);
	do {
		ctrl_word |= (1 << 1);
		top_reg_write(bmdi, TOP_SW_RESET0, ctrl_word);
	} while (((ctrl_word = top_reg_read(bmdi, TOP_SW_RESET0)) & (1 << 1)) != (1 << 1));
#endif
}

int bm1684_l2_sram_init(struct bm_device_info *bmdi)
{
	bmdev_memcpy_s2d_internal(bmdi, L2_SRAM_START_ADDR + L2_SRAM_TPU_TABLE_OFFSET,
			l2_sram_table, sizeof(l2_sram_table));
	return 0;
}

#ifdef SOC_MODE
void bm1684_tpu_reset(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 tpu reset\n");
	reset_control_assert(bmdi->cinfo.tpu);
	udelay(1000);
	reset_control_deassert(bmdi->cinfo.tpu);
}

void bm1684_gdma_reset(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 gdma reset\n");
	reset_control_assert(bmdi->cinfo.gdma);
	udelay(1000);
	reset_control_deassert(bmdi->cinfo.gdma);
}

void bm1684_tpu_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 tpu clk is enable \n");
	clk_prepare_enable(bmdi->cinfo.tpu_clk);
}

void bm1684_tpu_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 tpu clk is gating \n");
	clk_disable_unprepare(bmdi->cinfo.tpu_clk);
}

void bm1684_fixed_tpu_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 tpu fixed tpu clk is enable \n");
	clk_prepare_enable(bmdi->cinfo.fixed_tpu_clk);
}

void bm1684_fixed_tpu_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 tpu fixed tpu clk is gating \n");
	clk_disable_unprepare(bmdi->cinfo.fixed_tpu_clk);
}

void bm1684_arm9_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 arm9 clk is enable \n");
	clk_prepare_enable(bmdi->cinfo.arm9_clk);
}

void bm1684_arm9_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 arm9 clk is gating \n");
	clk_disable_unprepare(bmdi->cinfo.arm9_clk);
}

void bm1684_sram_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 sram clk is enable \n");
	clk_prepare_enable(bmdi->cinfo.sram_clk);
}

void bm1684_sram_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 sram clk is gating \n");
	clk_disable_unprepare(bmdi->cinfo.sram_clk);
}

void bm1684_gdma_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 gdma clk is enable \n");
	clk_prepare_enable(bmdi->cinfo.gdma_clk);
}

void bm1684_gdma_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 gdma clk is gating \n");
	clk_disable_unprepare(bmdi->cinfo.gdma_clk);
}

void bm1684_intc_clk_enable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 intc clk is enable \n");
	clk_prepare_enable(bmdi->cinfo.intc_clk);
}

void bm1684_intc_clk_disable(struct bm_device_info *bmdi)
{
	PR_TRACE("bm1684 intc clk is gating \n");
	clk_disable_unprepare(bmdi->cinfo.intc_clk);
}
#endif
