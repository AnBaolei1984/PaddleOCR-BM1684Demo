#include "bm_common.h"
#include "bm1684_perf.h"
#include "bm_io.h"

void bm1684_enable_tpu_perf_monitor(struct bm_device_info *bmdi, struct bm_perf_monitor *perf_monitor)
{
	int size = perf_monitor->buffer_size;
	u64 start_addr = perf_monitor->buffer_start_addr;
	u64 end_addr = start_addr + size;
	u32 value = 0;

	PR_TRACE("bm1684 enable  tpu perf monitor size = 0x%x, start addr = 0x%llx, end addr = 0x%llx\n",
			size, start_addr, end_addr);
	//set start address low 15bit, 0x170 bit [17:31]
	value = tpu_reg_read(bmdi, 0x170);
	value &= ~(0x7fff << 17);
	value |= (start_addr & 0x7fff) << 17;
	tpu_reg_write(bmdi, 0x170, value);

	//set start address high 20bit, 0x174 bit [0:19]
	value = tpu_reg_read(bmdi, 0x174);
	value &= ~(0xfffff << 0);
	value |= (start_addr >> 15 & 0xfffff) << 0;
	tpu_reg_write(bmdi, 0x174, value);

	//set end address low 12bit, 0x174 bit [20:31]
	value = tpu_reg_read(bmdi, 0x174);
	value &= ~(0xfff << 20);
	value |= (end_addr & 0xfff) << 20;
	tpu_reg_write(bmdi, 0x174, value);

	//set end address high 23bit, 0x178 bit [0:22]
	value = tpu_reg_read(bmdi, 0x178);
	value &= ~(0x7fffff << 0);
	value |= (end_addr >> 12 & 0x7fffff) << 0;
	tpu_reg_write(bmdi, 0x178, value);

	//set config_cmpt_en 0x178 bit[23] = 1, set_config_cmt_val as 1 0x178 bit[24:31] = 0x1
	//bit[23:31] = 0x3;
	value = tpu_reg_read(bmdi, 0x178);
	value &= ~(0x1ff << 23);
	value |= 0x3 << 23;
	tpu_reg_write(bmdi, 0x178, value);

	//set_conifg_cmtval as 1 0x17c bit[0:7] = 0, //set cfg_rd_instr_en =1 0x17c bit[8] = 1;
	//set cfg_rd_instr_stall_en =1 0x17c bit[9] = 1;
	//set cfg_wr_instr_en =1 0x17c bit[10] = 1;
	//bit [0:10] = 0x7 << 8
	value = tpu_reg_read(bmdi, 0x17c);
	value &= ~(0x7ff << 0);
	value |= 0x7 << 8;
	tpu_reg_write(bmdi, 0x17c, value);

	//enable tpu monitor 0x170 bit[16] = 1;
	value = tpu_reg_read(bmdi, 0x170);
	value &= ~(0x1 << 16);
	value |= 0x1 << 16;
	tpu_reg_write(bmdi, 0x170, value);
}

void bm1684_disable_tpu_perf_monitor(struct bm_device_info *bmdi)
{
	u32 value = 0;

	value = tpu_reg_read(bmdi, 0x170);
	value &= 0xffff;
	tpu_reg_write(bmdi, 0x170, value);

	value = tpu_reg_read(bmdi, 0x174);
	value &= 0x0;
	tpu_reg_write(bmdi, 0x174, value);

	value = tpu_reg_read(bmdi, 0x178);
	value &= 0x0;
	tpu_reg_write(bmdi, 0x178, value);

	value = tpu_reg_read(bmdi, 0x17c);
	value &= 0xfffff800;
	tpu_reg_write(bmdi, 0x17c, value);
}

void bm1684_enable_gdma_perf_monitor(struct bm_device_info *bmdi, struct bm_perf_monitor *perf_monitor)
{
	int size = perf_monitor->buffer_size;
	u64 start_addr = perf_monitor->buffer_start_addr;
	u64 end_addr = start_addr + size;

	PR_TRACE("bm1684 enable  gdma perf monitor size = 0x%x, start addr = 0x%llx, end addr = 0x%llx\n",
			size, start_addr, end_addr);
	bm_write32(bmdi, (0x5800006c - 0x4), bm_read32(bmdi, (0x5800006c - 0x4)) | 0x1);//work around for gdma issue.
	gdma_reg_write(bmdi, 0x70, (start_addr >> 32) & 0xffffffff);
	gdma_reg_write(bmdi, 0x74, (start_addr) & 0xffffffff);
	gdma_reg_write(bmdi, 0x78, (end_addr >> 32) & 0xffffffff);
	gdma_reg_write(bmdi, 0x7c, (end_addr) & 0xffffffff);
	gdma_reg_write(bmdi, 0x6c, 0x1);
}

void bm1684_disable_gdma_perf_monitor(struct bm_device_info *bmdi)
{
	bm_write32(bmdi, (0x5800006c - 0x4), bm_read32(bmdi, (0x5800006c - 0x4)) & ~0x1);//work around for gdma issue.
	gdma_reg_write(bmdi, 0x6c, 0x0);
	gdma_reg_write(bmdi, 0x70, 0x0);
	gdma_reg_write(bmdi, 0x74, 0x0);
	gdma_reg_write(bmdi, 0x78, 0x0);
	gdma_reg_write(bmdi, 0x7c, 0x0);
}

