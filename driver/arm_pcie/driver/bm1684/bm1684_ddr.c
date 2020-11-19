#include <linux/delay.h>
#include "bm_common.h"
#include "bm_io.h"
#include "bm1684_pcie.h"
#include "bm_pcie.h"
#include "bm_memcpy.h"
#include "bm1684_ddr.h"
#include "bm1684_card.h"

void bm1684_disable_ddr_interleave(struct bm_device_info *bmdi)
{
	int reg_val = 0;

	/* disable interleave */
	reg_val = top_reg_read(bmdi, 0x8);
	reg_val &= ~(1 << 3);
	top_reg_write(bmdi, 0x8, reg_val);
}

void bm1684_enable_ddr_interleave(struct bm_device_info *bmdi)
{
	int reg_val = 0;

	/* enable interleave */
	reg_val = top_reg_read(bmdi, 0x8);
	reg_val |= (1 << 3);
	top_reg_write(bmdi, 0x8, reg_val);
}

void enable_ddr_refresh_sync_d0a_d0b(struct bm_device_info *bmdi)
{
	int reg_val = 0xf2;

	top_reg_write(bmdi, 0x68, reg_val);
}

void enable_ddr_refresh_sync_d0a_d0b_d1_d2(struct bm_device_info *bmdi)
{
	int reg_val = 0x2;

	top_reg_write(bmdi, 0x68, reg_val);
}

#ifndef SOC_MODE
#define DDR0A_IRQ_ID 56
#define DDR0B_IRQ_ID 57
#define DDR1_IRQ_ID 56
#define DDR2_IRQ_ID 57

void bm1684_ddr0a_ecc_irq_handler(struct bm_device_info *bmdi)
{
	u32 status = ddr_reg_read_enh(bmdi, 0x0, 0x78);

	pr_info("ddr0a ecc status = 0x%x", status);
	bmdi->cinfo.ddr_ecc_correctN += 1;
	status = ddr_reg_read_enh(bmdi, 0x0, 0x7c);
	ddr_reg_write_enh(bmdi, 0x0, 0x7c, (status | 0x1f));
}

void bm1684_ddr0b_ecc_irq_handler(struct bm_device_info *bmdi)
{
	u32 status = ddr_reg_read_enh(bmdi, 0x1, 0x78);

	pr_info("ddr0b ecc status = 0x%x", status);
	bmdi->cinfo.ddr_ecc_correctN += 1;
	status = ddr_reg_read_enh(bmdi, 0x1, 0x7c);
	ddr_reg_write_enh(bmdi, 0x1, 0x7c, (status | 0x1f));
}

void bm1684_ddr1_ecc_irq_handler(struct bm_device_info *bmdi)
{
	u32 status = ddr_reg_read_enh(bmdi, 0x2, 0x78);

	pr_info("ddr1 ecc status = 0x%x", status);
	bmdi->cinfo.ddr_ecc_correctN += 1;
	status = ddr_reg_read_enh(bmdi, 0x2, 0x7c);
	ddr_reg_write_enh(bmdi, 0x2, 0x7c, (status | 0x1f));
}

void bm1684_ddr2_ecc_irq_handler(struct bm_device_info *bmdi)
{
	u32 status = ddr_reg_read_enh(bmdi, 0x3, 0x78);

	pr_info("ddr2 ecc status = 0x%x", status);
	bmdi->cinfo.ddr_ecc_correctN += 1;
	status = ddr_reg_read_enh(bmdi, 0x3, 0x7c);
	ddr_reg_write_enh(bmdi, 0x3, 0x7c, (status | 0x1f));
}

void bm1684_ddr_ecc_request_irq(struct bm_device_info *bmdi)
{
	bmdrv_submodule_request_irq(bmdi, DDR0A_IRQ_ID, bm1684_ddr0a_ecc_irq_handler);
	bmdrv_submodule_request_irq(bmdi, DDR0B_IRQ_ID, bm1684_ddr0b_ecc_irq_handler);
	bmdrv_submodule_request_irq(bmdi, DDR1_IRQ_ID, bm1684_ddr1_ecc_irq_handler);
	bmdrv_submodule_request_irq(bmdi, DDR2_IRQ_ID, bm1684_ddr2_ecc_irq_handler);
}

void bm1684_ddr_ecc_free_irq(struct bm_device_info *bmdi)
{
	bmdrv_submodule_free_irq(bmdi, DDR0A_IRQ_ID);
	bmdrv_submodule_free_irq(bmdi, DDR0B_IRQ_ID);
	bmdrv_submodule_free_irq(bmdi, DDR1_IRQ_ID);
	bmdrv_submodule_free_irq(bmdi, DDR2_IRQ_ID);
}
#endif

void ddr_phy_reg_write16(struct bm_device_info *bmdi, u32 addr, u32 value)
{
#ifndef SOC_MODE
	void __iomem *bar4_addr;
	u32 temp_offset = 0;

	pci_slider_bar4_config_device_addr(&bmdi->cinfo.bar_info, addr);
	bar4_addr = bmdi->cinfo.bar_info.bar4_vaddr;

	temp_offset = addr & 0xfffff;
	REG_WRITE16(bar4_addr, temp_offset, value);
#else
	bm_write16(bmdi, addr, value);
#endif
}

u16 ddr_phy_reg_read16(struct bm_device_info *bmdi, u32 addr)
{
#ifndef SOC_MODE
	void __iomem *bar4_addr;
	u32 temp_offset = 0;
	u16 data = 0;

	pci_slider_bar4_config_device_addr(&bmdi->cinfo.bar_info, addr);
	bar4_addr = bmdi->cinfo.bar_info.bar4_vaddr;

	temp_offset = addr & 0xfffff;
	data = REG_READ16(bar4_addr, temp_offset);
	return data;
#else
	return bm_read16(bmdi, addr);
#endif
}

int bm1684_ddr_format_by_cdma(struct bm_device_info *bmdi)
{
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
	u32 realmem_size = memcpy_info->stagemem_s2d.size;
	u64 dst = 0x100000000;
//	u64 ddr0_total_size = 0x200000000*7/8; //ONLY 7/8 NEED FORMAT
	u64 ddr0_total_size = 1024*1024*100; //ONLY 7/8 NEED FORMAT
	u64 cur_addr_inc = 0x0;
	u32 size_step = realmem_size;
	bm_cdma_arg cdma_arg;

	memset(memcpy_info->stagemem_s2d.v_addr, 0xfe, realmem_size);
	while (cur_addr_inc < ddr0_total_size) {
		mutex_lock(&memcpy_info->stagemem_s2d.stage_mutex);
		bmdev_construct_cdma_arg(&cdma_arg, memcpy_info->stagemem_s2d.p_addr & 0xffffffffff,
				dst + cur_addr_inc, size_step, HOST2CHIP, false, false);
		if (memcpy_info->bm_cdma_transfer(bmdi, NULL, &cdma_arg, true)) {
			mutex_unlock(&memcpy_info->stagemem_s2d.stage_mutex);
			return -EBUSY;
		}
		cur_addr_inc += realmem_size;
		if (cur_addr_inc > ddr0_total_size) {
			bmdev_construct_cdma_arg(&cdma_arg, memcpy_info->stagemem_s2d.p_addr & 0xffffffffff,
					dst + cur_addr_inc - realmem_size, ddr0_total_size%size_step, HOST2CHIP, false, false);
			if (memcpy_info->bm_cdma_transfer(bmdi, NULL, &cdma_arg, true)) {
				mutex_unlock(&memcpy_info->stagemem_s2d.stage_mutex);
				return -EBUSY;
			}
		}
		mutex_unlock(&memcpy_info->stagemem_s2d.stage_mutex);
	}

	return 0;
}

int bm1684_ddr_top_init(struct bm_device_info *bmdi)
{
#ifdef FW_SIMPLE
	return 0;
#else
	u8 board_type = 0;

	board_type = (u8)((bmdi->misc_info.board_version >> 8) & 0xff);
	if (board_type == BOARD_TYPE_SM5_S || board_type == BOARD_TYPE_SA5) {
		pr_info("soc mode, ddr not init by pcie\n");
		return 0;
	}
	if (bmdi->cinfo.platform == PALLADIUM) {
		bm1684_pld_ddr_top_init(bmdi);
		return 0;
	}
	if (bm1684_lpddr4_init(bmdi) < 0)
		return -1;
	return 0;
#endif
}
