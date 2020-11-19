#include <linux/pci.h>
#include <linux/pci_hotplug.h>
#include <linux/delay.h>
#include "bm1684_reg.h"
#include "bm_io.h"
#include "bm_pcie.h"
#include "bm_common.h"
int bm1684_set_chip_index(struct bm_device_info *bmdi)
{
	void __iomem *atu_base_addr;
	int mode = 0x0;
	int index = 0x0;

	atu_base_addr = bmdi->cinfo.bar_info.bar0_vaddr;
	mode = REG_READ32(atu_base_addr, 0x718) & 0x3;

	if (mode == 0x2)
		index = 0;
	else if (mode == 0x1)
		index = 1;
	else if (mode == 0x3) {
		if (REG_READ32((atu_base_addr + 0x100000), 0x1c) == 0x0)
			index = 0x0;
		else
			index = 0x2;
	} else
		index = -1;

	bmdi->cinfo.chip_index = index;
	pr_info("bm-sophon%d, chip_index = %d\n", bmdi->dev_index, index);
	return index;
}

void bm1684_map_bar(struct bm_device_info *bmdi, struct pci_dev *pdev)
{
	void __iomem *atu_base_addr;
	void __iomem *cfg_base_addr;
	u64 bar1_start = 0;
	u64 bar2_start = 0;
	u64 bar4_start = 0;
	int function_num = 0;
	u64 size = 0x0;

	cfg_base_addr = bmdi->cinfo.bar_info.bar0_vaddr; //0x5f800000
	if (bmdi->cinfo.chip_index > 0)
		function_num = bmdi->cinfo.chip_index;
	else
		function_num = (pdev->devfn & 0x7);

	if (function_num == 0x0) {
		bar1_start = REG_READ32(cfg_base_addr, 0x14) & ~0xf;
		bar2_start = (REG_READ32(cfg_base_addr, 0x18) & ~0xf) | ((u64)(REG_READ32(cfg_base_addr, 0x1c)) << 32);
		bar4_start = (REG_READ32(cfg_base_addr, 0x20) & ~0xf) | ((u64)(REG_READ32(cfg_base_addr, 0x24)) << 32);
#ifdef __aarch64__
		bar1_start &= (u64)0xffffffff;
		bar2_start &= (u64)0xffffffff;
		bar4_start &= (u64)0xffffffff;
#endif
		size = 0xffffff;

	} else {
		bar1_start = 0x89000000;
		bar2_start = ((u64)(0x320 + (function_num -1) *0x20) << 32);
		bar4_start = 0x8b000000;
		size = 0x4ffffffff;
	}

	atu_base_addr = bmdi->cinfo.bar_info.bar0_vaddr + REG_OFFSET_PCIE_iATU; //0x5fb00000

	//ATU0 is configured at firmware, which configure as 0x5f800000

	//ATU1
	// DTCM  512k
	REG_WRITE32(atu_base_addr, 0x300, 0);
	REG_WRITE32(atu_base_addr, 0x304, 0x80000100);		//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x308, (u32)(bar1_start & 0xffffffff) + BAR1_PART0_OFFSET);		//src addr
	REG_WRITE32(atu_base_addr, 0x30C, bar1_start >> 32);
	REG_WRITE32(atu_base_addr, 0x310, (u32)(bar1_start & 0xffffffff) + BAR1_PART0_OFFSET + 0x7ffff);	//size 512K
	REG_WRITE32(atu_base_addr, 0x314, 0x2000000);		//dst addr
	REG_WRITE32(atu_base_addr, 0x318, 0);

	//ATU2
	// Top regs  3M
	REG_WRITE32(atu_base_addr, 0x500, 0);
	REG_WRITE32(atu_base_addr, 0x504, 0x80000100); 	//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x508, (u32)(bar1_start & 0xffffffff) + BAR1_PART1_OFFSET);		//src addr
	REG_WRITE32(atu_base_addr, 0x50C, bar1_start >> 32);
	REG_WRITE32(atu_base_addr, 0x510, (u32)(bar1_start & 0xffffffff) + BAR1_PART1_OFFSET + 0x2fffff);	//size 3M
	REG_WRITE32(atu_base_addr, 0x514, 0x50000000);		//dst addr
	REG_WRITE32(atu_base_addr, 0x518, 0);

	//ATU3
	// DMA/MMU/TPU  64k
	REG_WRITE32(atu_base_addr, 0x700, 0);
	REG_WRITE32(atu_base_addr, 0x704, 0x80000100); 	//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x708, (u32)(bar1_start & 0xffffffff) + BAR1_PART2_OFFSET);		//src addr
	REG_WRITE32(atu_base_addr, 0x70C, bar1_start >> 32);
	REG_WRITE32(atu_base_addr, 0x710, (u32)(bar1_start & 0xffffffff) + BAR1_PART2_OFFSET + 0xffff);	//size 64K
	REG_WRITE32(atu_base_addr, 0x714, 0x58000000);		//dst addr
	REG_WRITE32(atu_base_addr, 0x718, 0);

	//ATU5
	// DDR2A  controller 4k
	REG_WRITE32(atu_base_addr, 0xb00, 0);
	REG_WRITE32(atu_base_addr, 0xb04, 0x80000100); 	//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0xb08, (u32)(bar1_start & 0xffffffff) + BAR1_PART3_OFFSET);		//src addr
	REG_WRITE32(atu_base_addr, 0xb0C, bar1_start >> 32);
	REG_WRITE32(atu_base_addr, 0xb10, (u32)(bar1_start & 0xffffffff) + BAR1_PART3_OFFSET + 0xfff);	//size 4K
	REG_WRITE32(atu_base_addr, 0xb14, 0x68000000);		//dst addr
	REG_WRITE32(atu_base_addr, 0xb18, 0);

	//ATU6
	// DDR0A  controller 4k
	REG_WRITE32(atu_base_addr, 0xd00, 0);
	REG_WRITE32(atu_base_addr, 0xd04, 0x80000100); 	//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0xd08, (u32)(bar1_start & 0xffffffff) + BAR1_PART4_OFFSET);		//src addr
	REG_WRITE32(atu_base_addr, 0xd0C, bar1_start >> 32);
	REG_WRITE32(atu_base_addr, 0xd10, (u32)(bar1_start & 0xffffffff) + BAR1_PART4_OFFSET + 0xfff);	//size 4K
	REG_WRITE32(atu_base_addr, 0xd14, 0x6a000000);		//dst addr
	REG_WRITE32(atu_base_addr, 0xd18, 0);

	//ATU7
	// DDR0B  controller 4k
	REG_WRITE32(atu_base_addr, 0xf00, 0);
	REG_WRITE32(atu_base_addr, 0xf04, 0x80000100); 	//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0xf08, (u32)(bar1_start & 0xffffffff) + BAR1_PART5_OFFSET);		//src addr
	REG_WRITE32(atu_base_addr, 0xf0C, bar1_start >> 32);
	REG_WRITE32(atu_base_addr, 0xf10, (u32)(bar1_start & 0xffffffff) + BAR1_PART5_OFFSET + 0xfff);	//size 4K
	REG_WRITE32(atu_base_addr, 0xf14, 0x6c000000);		//dst addr
	REG_WRITE32(atu_base_addr, 0xf18, 0);

	//ATU8
	// DDR1  controller 4k
	REG_WRITE32(atu_base_addr, 0x1100, 0);
	REG_WRITE32(atu_base_addr, 0x1104, 0x80000100); 	//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x1108, (u32)(bar1_start & 0xffffffff) + BAR1_PART6_OFFSET);		//src addr
	REG_WRITE32(atu_base_addr, 0x110C, bar1_start >> 32);
	REG_WRITE32(atu_base_addr, 0x1110, (u32)(bar1_start & 0xffffffff) + BAR1_PART6_OFFSET + 0xfff);	//size 4K
	REG_WRITE32(atu_base_addr, 0x1114, 0x6e000000);		//dst addr
	REG_WRITE32(atu_base_addr, 0x1118, 0);

	//ATU9
	// spi  controller 4k
	REG_WRITE32(atu_base_addr, 0x1300, 0);
	REG_WRITE32(atu_base_addr, 0x1304, 0x80000100);
	//address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x1308, (u32)(bar1_start & 0xffffffff)
		       	+ BAR1_PART7_OFFSET);		//src addr
	REG_WRITE32(atu_base_addr, 0x130C, bar1_start >> 32);
	REG_WRITE32(atu_base_addr, 0x1310, (u32)(bar1_start & 0xffffffff)
		       	+ BAR1_PART7_OFFSET + 0xFFF);	//size 4K
	REG_WRITE32(atu_base_addr, 0x1314, 0x6000000);		//dst addr
	REG_WRITE32(atu_base_addr, 0x1318, 0);

	/*config bar2 for chip to chip transferr*/

	REG_WRITE32(atu_base_addr, 0x2d00, 0x80000);
	REG_WRITE32(atu_base_addr, 0x2d04, 0xc0080200);
	REG_WRITE32(atu_base_addr, 0x2d08, 0);
	REG_WRITE32(atu_base_addr, 0x2d0C, 0);
	REG_WRITE32(atu_base_addr, 0x2d10, 0);
	REG_WRITE32(atu_base_addr, 0x2d14, 0x0);
	REG_WRITE32(atu_base_addr, 0x2d18, 0x0);


	/* config bar4 to DDR phy*/
	//ATU4
	REG_WRITE32(atu_base_addr, 0x900, 0);
	REG_WRITE32(atu_base_addr, 0x904, 0x80000400);
	REG_WRITE32(atu_base_addr, 0x908, (u32)(bar4_start & 0xffffffff));//src addr
	REG_WRITE32(atu_base_addr, 0x90C, bar4_start >> 32);
	REG_WRITE32(atu_base_addr, 0x910, (u32)(bar4_start & 0xffffffff)
		       	+ 0xFFFFF);		//1M size
	REG_WRITE32(atu_base_addr, 0x914, 0x6a000000); 	//dst addr
	REG_WRITE32(atu_base_addr, 0x918, 0x0);
}

void bm1684_unmap_bar(struct bm_bar_info *bari) {
	void __iomem *atu_base_addr;
	int i = 0;
	atu_base_addr = bari->bar0_vaddr + REG_OFFSET_PCIE_iATU;
	for (i = 0; i < 30; i++) {
		REG_WRITE32(atu_base_addr, 0x300 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x304 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x308 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x30C + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x310 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x314 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x318 + i*0x200, 0);
	}

}

static const struct bm_bar_info bm1684_bar_layout[] = {
	{
		.bar0_len = 0x400000,
		.bar0_dev_start = 0x5f800000,

		.bar1_len = 0x400000,
		.bar1_dev_start = 0x00000000,
		.bar1_part0_offset = 0,
		.bar1_part0_len = 0x80000,
		.bar1_part0_dev_start = 0x2000000,

		.bar1_part1_offset = 0x80000,
		.bar1_part1_len = 0x300000,
		.bar1_part1_dev_start = 0x50000000,

		.bar1_part2_offset = 0x380000,
		.bar1_part2_len = 0x10000,
		.bar1_part2_dev_start = 0x58000000,

		.bar1_part3_offset = 0x390000,
		.bar1_part3_len = 0x1000,
		.bar1_part3_dev_start = 0x68000000,

		.bar1_part4_offset = 0x391000,
		.bar1_part4_len = 0x1000,
		.bar1_part4_dev_start = 0x6a000000,

		.bar1_part5_offset = 0x392000,
		.bar1_part5_len = 0x1000,
		.bar1_part5_dev_start = 0x6c000000,

		.bar1_part6_offset = 0x393000,
		.bar1_part6_len = 0x1000,
		.bar1_part6_dev_start = 0x6e000000,

		.bar1_part7_offset = 0x394000,
		.bar1_part7_len = 0x1000,
		.bar1_part7_dev_start = 0x6000000,

		.bar2_len = 0x100000,
		.bar2_dev_start = 0x0,
		.bar2_part0_offset = 0x0,
		.bar2_part0_len = 0x100000,
		.bar2_part0_dev_start = 0x200000000,

		.bar4_len = 0x100000,
		.bar4_dev_start = 0x0,
	},
};

/* Setup the right bar layout based on bar len,
 * return 0 if find, else not find.
 */
int bm1684_setup_bar_dev_layout(struct bm_bar_info *bar_info, const struct bm_bar_info *bar_layout)
{
	if (NULL == bar_layout) {
		bar_layout = bm1684_bar_layout;
	}
	if (bar_layout->bar1_len == bar_info->bar1_len &&
			bar_layout->bar2_len == bar_info->bar2_len) {
		bar_info->bar0_dev_start = bar_layout->bar0_dev_start;
		bar_info->bar1_dev_start = bar_layout->bar1_dev_start;
		bar_info->bar1_part0_dev_start = bar_layout->bar1_part0_dev_start;
		bar_info->bar1_part1_dev_start = bar_layout->bar1_part1_dev_start;
		bar_info->bar1_part2_dev_start = bar_layout->bar1_part2_dev_start;
		bar_info->bar1_part3_dev_start = bar_layout->bar1_part3_dev_start;
		bar_info->bar1_part4_dev_start = bar_layout->bar1_part4_dev_start;
		bar_info->bar1_part5_dev_start = bar_layout->bar1_part5_dev_start;
		bar_info->bar1_part6_dev_start = bar_layout->bar1_part6_dev_start;
		bar_info->bar1_part7_dev_start = bar_layout->bar1_part7_dev_start;
		bar_info->bar1_part0_offset = bar_layout->bar1_part0_offset;
		bar_info->bar1_part1_offset = bar_layout->bar1_part1_offset;
		bar_info->bar1_part2_offset = bar_layout->bar1_part2_offset;
		bar_info->bar1_part3_offset = bar_layout->bar1_part3_offset;
		bar_info->bar1_part4_offset = bar_layout->bar1_part4_offset;
		bar_info->bar1_part5_offset = bar_layout->bar1_part5_offset;
		bar_info->bar1_part6_offset = bar_layout->bar1_part6_offset;
		bar_info->bar1_part7_offset = bar_layout->bar1_part7_offset;
		bar_info->bar1_part0_len = bar_layout->bar1_part0_len;
		bar_info->bar1_part1_len = bar_layout->bar1_part1_len;
		bar_info->bar1_part2_len = bar_layout->bar1_part2_len;
		bar_info->bar1_part3_len = bar_layout->bar1_part3_len;
		bar_info->bar1_part4_len = bar_layout->bar1_part4_len;
		bar_info->bar1_part5_len = bar_layout->bar1_part5_len;
		bar_info->bar1_part6_len = bar_layout->bar1_part6_len;
		bar_info->bar1_part7_len = bar_layout->bar1_part7_len;

		bar_info->bar2_dev_start = bar_layout->bar2_dev_start;
		bar_info->bar2_part0_dev_start = bar_layout->bar2_part0_dev_start;
		bar_info->bar2_part0_offset = bar_layout->bar2_part0_offset;
		bar_info->bar2_part0_len = bar_layout->bar2_part0_len;

		bar_info->bar4_dev_start = bar_layout->bar4_dev_start;
		bar_info->bar4_len = bar_layout->bar4_len;
		return 0;
	}
	/* Not find */
	return -1;
}

void bm1684_pcie_calculate_cdma_max_payload(struct bm_device_info *bmdi)
{
	void __iomem *atu_base_addr;
	int max_payload = 0x0;
	int max_rd_req = 0x0;
	int mode = 0x0;
	int total_func_num = 0;
	int i = 0;
	int temp_value = 2;
	int temp_low = 2;
	atu_base_addr = bmdi->cinfo.bar_info.bar0_vaddr + REG_OFFSET_PCIE_iATU + 0x80000; //0x5fb80000
	mode =  bmdrv_pcie_get_mode(bmdi) & 0x7;
	if (mode == 0x3)
		total_func_num = 0x1;
	else
		total_func_num = (mode & 0x3) + 0x1;

	max_payload = REG_READ32(atu_base_addr, 0x44);
	max_rd_req = REG_READ32(atu_base_addr, 0x40);
	for (i = 0; i < total_func_num; i++) {
		temp_value = max_rd_req >> (16+i*0x3);
		temp_value &= 0x3;
		if (temp_value < temp_low)
			temp_low = temp_value;
		temp_value = max_payload >> i*0x3;
		temp_value &= 0x3;
		if (temp_value < temp_low)
			temp_low = temp_value;
	}
	bmdi->memcpy_info.cdma_max_payload = temp_low;
	pr_info("max_payload = 0x%x, max_rd_req = 0x%x, mode = 0x%x, total_func_num = 0x%x, max_paload = 0x%x \n",
		max_payload, max_rd_req, mode, total_func_num, bmdi->memcpy_info.cdma_max_payload);
}

static struct bm_bar_info bm_mode_chose_layout[] = {
	{
		.bar0_len = 0x400000,
		.bar0_dev_start = 0x5f800000,

		.bar1_len = 0x400000,
		.bar1_dev_start = 0x00000000,
		.bar1_part0_offset = 0,
		.bar1_part0_len = 0x80000,
		.bar1_part0_dev_start = 0x2000000,

		.bar1_part1_offset = 0x80000,
		.bar1_part1_len = 0x300000,
		.bar1_part1_dev_start = 0x50000000,

		.bar1_part2_offset = 0x380000,
		.bar1_part2_len = 0x10000,
		.bar1_part2_dev_start = 0x58000000,

		.bar1_part3_offset = 0x390000,
		.bar1_part3_len = 0x1000,
		.bar1_part3_dev_start = 0x68000000,

		.bar1_part4_offset = 0x391000,
		.bar1_part4_len = 0x1000,
		.bar1_part4_dev_start = 0x6a000000,

		.bar1_part5_offset = 0x392000,
		.bar1_part5_len = 0x1000,
		.bar1_part5_dev_start = 0x6c000000,

		.bar1_part6_offset = 0x393000,
		.bar1_part6_len = 0x1000,
		.bar1_part6_dev_start = 0x6e000000,

		.bar1_part7_offset = 0x394000,
		.bar1_part7_len = 0x1000,
		.bar1_part7_dev_start = 0x6000000,

		.bar2_len = 0x100000,
		.bar2_dev_start = 0x0,
		.bar2_part0_offset = 0x0,
		.bar2_part0_len = 0x100000,
		.bar2_part0_dev_start = 0x5ff00000,

		.bar4_len = 0x100000,
		.bar4_dev_start = 0x60100000,
	},
};

int bmdrv_get_chip_num(struct bm_device_info *bmdi)
{
	void __iomem *abp_addr;
	int chip_num = 0x0;

	abp_addr = bmdi->cinfo.bar_info.bar0_vaddr + REG_OFFSET_PCIE_iATU + 0x80000; //0x5fb00000
	chip_num = (REG_READ32(abp_addr,0) >> 28) & 0x7;
	PR_TRACE("driver chip num = 0x%x\n", chip_num);
	return chip_num;
}

void bmdrv_init_for_mode_chose(struct bm_device_info *bmdi, struct pci_dev *pdev, struct bm_bar_info *bari)
{
	void __iomem *atu_base_addr;
	void __iomem *cfg_base_addr;
	int function_num = 0x0;
	u64 bar1_start = 0;
	u64 bar2_start = 0;
	u64 bar4_start = 0;
	int i = 0;

	cfg_base_addr = bmdi->cinfo.bar_info.bar0_vaddr; //0x5f800000
	if (bmdi->cinfo.chip_index > 0)
		function_num = bmdi->cinfo.chip_index;
	else
		function_num = (pdev->devfn & 0x7);

	bmdi->cinfo.bmdrv_setup_bar_dev_layout(bari, bm_mode_chose_layout);
	if (function_num == 0x0) {
		bar1_start = REG_READ32(cfg_base_addr, 0x14) & ~0xf;
		bar2_start = (REG_READ32(cfg_base_addr, 0x18) & ~0xf) | ((u64)(REG_READ32(cfg_base_addr, 0x1c)) << 32);
		bar4_start = (REG_READ32(cfg_base_addr, 0x20) & ~0xf) | ((u64)(REG_READ32(cfg_base_addr, 0x24)) << 32);
#ifdef __aarch64__
		bar1_start &= (u64)0xffffffff;
		bar2_start &= (u64)0xffffffff;
		bar4_start &= (u64)0xffffffff;
#endif
	} else {
		bar1_start = 0x89000000;
		bar2_start = ((u64)(0x320 + (function_num -1) *0x20) << 32) ;
		bar4_start = 0x8b000000;
	}

	atu_base_addr = bari->bar0_vaddr + REG_OFFSET_PCIE_iATU; //0x5fb00000

	for (i = 0; i < 30; i++) {
		REG_WRITE32(atu_base_addr, 0x300 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x304 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x308 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x30C + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x310 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x314 + i*0x200, 0);
		REG_WRITE32(atu_base_addr, 0x318 + i*0x200, 0);
	}

	//ATU1
	// DTCM  512k
	REG_WRITE32(atu_base_addr, 0x300, 0);
	REG_WRITE32(atu_base_addr, 0x304, 0x80000100);      //address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x308, (u32)(bar1_start & 0xffffffff) + BAR1_PART0_OFFSET);        //src addr
	REG_WRITE32(atu_base_addr, 0x30C, bar1_start >> 32);
	REG_WRITE32(atu_base_addr, 0x310, (u32)(bar1_start & 0xffffffff) + BAR1_PART0_OFFSET + 0x7ffff);  //size 512K
	REG_WRITE32(atu_base_addr, 0x314, 0x2000000);       //dst addr
	REG_WRITE32(atu_base_addr, 0x318, 0);

	//ATU2
	// Top regs  3M
	REG_WRITE32(atu_base_addr, 0x500, 0);
	REG_WRITE32(atu_base_addr, 0x504, 0x80000100);  //address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x508, (u32)(bar1_start & 0xffffffff) + BAR1_PART1_OFFSET);        //src addr
	REG_WRITE32(atu_base_addr, 0x50C, bar1_start >> 32);
	REG_WRITE32(atu_base_addr, 0x510, (u32)(bar1_start & 0xffffffff) + BAR1_PART1_OFFSET + 0x2fffff); //size 3M
	REG_WRITE32(atu_base_addr, 0x514, 0x50000000);      //dst addr
	REG_WRITE32(atu_base_addr, 0x518, 0);

	//ATU3
	// DMA/MMU/TPU  64k
	REG_WRITE32(atu_base_addr, 0x700, 0);
	REG_WRITE32(atu_base_addr, 0x704, 0x80000100);  //address match mode for bar1, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0x708, (u32)(bar1_start & 0xffffffff) + BAR1_PART2_OFFSET);        //src addr
	REG_WRITE32(atu_base_addr, 0x70C, bar1_start >> 32);
	REG_WRITE32(atu_base_addr, 0x710, (u32)(bar1_start & 0xffffffff) + BAR1_PART2_OFFSET + 0xffff);   //size 64K
	REG_WRITE32(atu_base_addr, 0x714, 0x58000000);      //dst addr
	REG_WRITE32(atu_base_addr, 0x718, 0);

	//ATU5
	// pcie rc  4M
	REG_WRITE32(atu_base_addr, 0xb00, 0);
	REG_WRITE32(atu_base_addr, 0xb04, 0x80000000);  //address match mode for bar2, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0xb08, (u32)(bar2_start & 0xffffffff));//src addr
	REG_WRITE32(atu_base_addr, 0xb0C, bar2_start >> 32);
	REG_WRITE32(atu_base_addr, 0xb10, (u32)(bar2_start & 0xffffffff) + 0xfffff); //size 1M
	REG_WRITE32(atu_base_addr, 0xb14, 0x5ff00000);      //dst addr
	REG_WRITE32(atu_base_addr, 0xb18, 0);
	REG_WRITE32(atu_base_addr, 0xb20, bar2_start >> 32);

	//ATU6
	// for down cfg
	REG_WRITE32(atu_base_addr, 0xd00, 0x0);
	REG_WRITE32(atu_base_addr, 0xd04, 0x80000000);  //address match mode for bar4, BIT[10:8] are not checked in address match mode
	REG_WRITE32(atu_base_addr, 0xd08, (u32)(bar4_start & 0xffffffff));        //src addr
	REG_WRITE32(atu_base_addr, 0xd0C, bar4_start >> 32);
	REG_WRITE32(atu_base_addr, 0xd10, (u32)(bar4_start & 0xffffffff) + 0xfffff);   //size 1M
	REG_WRITE32(atu_base_addr, 0xd14, 0x80100000);      //dst addr
	REG_WRITE32(atu_base_addr, 0xd18, 0);
	REG_WRITE32(atu_base_addr, 0xd20, bar4_start >> 32);
}

int bmdrv_pcie_get_mode(struct bm_device_info *bmdi)
{
	int mode = 0x0;

	//get gpio9,10,11
	mode = gpio_reg_read(bmdi, 0x50);
	mode >>= 0x9;
	mode &= 0x7;
	pr_info("pcie get mode is 0x%x \n", mode);
	return mode;
}

void bmdrv_pcie_set_function1_iatu_config(struct pci_dev *pdev, struct bm_device_info *bmdi)
{
	void __iomem *atu_base_addr;
	int value = 0x0;
	int function_num = 0x0;
	struct bm_bar_info *bari = &bmdi->cinfo.bar_info;

	if (bmdi->cinfo.chip_index > 0)
		function_num = bmdi->cinfo.chip_index;
	else
		function_num = (pdev->devfn & 0x7);

	atu_base_addr = bmdi->cinfo.bar_info.bar0_vaddr + REG_OFFSET_PCIE_iATU; //0x5fb00000

	REG_WRITE32(atu_base_addr, 0x1508, 0x0);
	REG_WRITE32(atu_base_addr, 0x150c, 0x0);
	REG_WRITE32(atu_base_addr, 0x1510, 0x0);  //size 4M
	REG_WRITE32(atu_base_addr, 0x1514, 0x0);
	REG_WRITE32(atu_base_addr, 0x1518, 0x6);         //inbound tagrget address 0x6_0000_0000
	REG_WRITE32(atu_base_addr, 0x1500, 0x100000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x1504, 0xc0080000); //bar match, match bar0

	REG_WRITE32(atu_base_addr, 0x1708, 0x0);
	REG_WRITE32(atu_base_addr, 0x170c, 0x0);
	REG_WRITE32(atu_base_addr, 0x1710, 0x0);  //size 4M
	REG_WRITE32(atu_base_addr, 0x1714, 0x0);
	REG_WRITE32(atu_base_addr, 0x1718, 0x7);         //inbound tagrget address 0x7_0000_0000
	REG_WRITE32(atu_base_addr, 0x1700, 0x100000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x1704, 0xc0080100); //bar match, match bar1

	REG_WRITE32(atu_base_addr, 0x1908, 0x0);
	REG_WRITE32(atu_base_addr, 0x190c, 0x0);
	REG_WRITE32(atu_base_addr, 0x1910, 0x0);  //size 1M
	REG_WRITE32(atu_base_addr, 0x1914, 0x0);
	REG_WRITE32(atu_base_addr, 0x1918, 0x320 + function_num*0x20 );         //inbound tagrget address 0x8_0000_0000
	REG_WRITE32(atu_base_addr, 0x1900, 0x100000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x1904, 0xc0080200); //bar match, match bar2

	REG_WRITE32(atu_base_addr, 0x1b08, 0x0);
	REG_WRITE32(atu_base_addr, 0x1b0c, 0x0);
	REG_WRITE32(atu_base_addr, 0x1b10, 0x0);  //size 1M
	REG_WRITE32(atu_base_addr, 0x1b14, 0x0);
	REG_WRITE32(atu_base_addr, 0x1b18, 0x9);       //inbound target address 0x9_0000_0000
	REG_WRITE32(atu_base_addr, 0x1b00, 0x100000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x1b04, 0xc0080400); //bar match, match bar4

	atu_base_addr = bmdi->cinfo.bar_info.bar2_vaddr; //0x5ff00000
	REG_WRITE32(atu_base_addr, 0x008, 0x0);
	REG_WRITE32(atu_base_addr, 0x00c, 0x6);       //outbound base address 6_0000_0000
	REG_WRITE32(atu_base_addr, 0x010, 0x400000 -1);  //size 4M
	REG_WRITE32(atu_base_addr, 0x020, 0x6);  //size 4M
	REG_WRITE32(atu_base_addr, 0x014, 0x88000000);
	REG_WRITE32(atu_base_addr, 0x018, 0x0);         //outbound target address 8800_0000
	REG_WRITE32(atu_base_addr, 0x000, 0x2000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x004, 0x80000000); //address match

	REG_WRITE32(atu_base_addr, 0x208, 0x0);
	REG_WRITE32(atu_base_addr, 0x20c, 0x7);       //outbound base address 7_0000_0000
	REG_WRITE32(atu_base_addr, 0x210, 0x400000 -1 );  //size 4M
	REG_WRITE32(atu_base_addr, 0x220, 0x7);  //size 4M
	REG_WRITE32(atu_base_addr, 0x214, 0x89000000);
	REG_WRITE32(atu_base_addr, 0x218, 0x0);         //outbound target address 8900_0000
	REG_WRITE32(atu_base_addr, 0x200, 0x2000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x204, 0x80000000); //address match

	REG_WRITE32(atu_base_addr, 0x408, 0x0);
	REG_WRITE32(atu_base_addr, 0x40c, 0x320 + function_num*0x20);       //outbound base address 8_0000_0000
	REG_WRITE32(atu_base_addr, 0x410, 0xffffffff);
	REG_WRITE32(atu_base_addr, 0x420, 0x324 + function_num*0x20);  //size 1M
	REG_WRITE32(atu_base_addr, 0x414, 0x0);
	REG_WRITE32(atu_base_addr, 0x418, 0x320 + function_num*0x20);         //outbound target address 8a00_0000
	REG_WRITE32(atu_base_addr, 0x400, 0x2000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x404, 0x80000000); //address match

	REG_WRITE32(atu_base_addr, 0x608, 0x0);
	REG_WRITE32(atu_base_addr, 0x60c, 0x9);       //outbound base address 9_0000_0000
	REG_WRITE32(atu_base_addr, 0x610, 0x100000 -1);  //size 1M
	REG_WRITE32(atu_base_addr, 0x620, 0x9);  //size 1M
	REG_WRITE32(atu_base_addr, 0x614, 0x8b000000); //outbound target address 0x8b00_0000
	REG_WRITE32(atu_base_addr, 0x618, 0x0);
	REG_WRITE32(atu_base_addr, 0x600, 0x2000);         //type 4'b0000, MRd/Mwr type
	REG_WRITE32(atu_base_addr, 0x604, 0x80000000); //address match

	REG_WRITE32(atu_base_addr, 0x808, 0x80000000); //out cfg
	REG_WRITE32(atu_base_addr, 0x80c, 0x0);
	REG_WRITE32(atu_base_addr, 0x810, 0x80300000 -1);
	REG_WRITE32(atu_base_addr, 0x814, 0x0);
	REG_WRITE32(atu_base_addr, 0x818, 0x0);
	REG_WRITE32(atu_base_addr, 0x800, 0x4);
	REG_WRITE32(atu_base_addr, 0x804, 0x90000000);

//enable rc msi
	REG_WRITE32(bari->bar0_vaddr + REG_OFFSET_PCIE_iATU, 0xb14, 0x5fc00000);      //dst addr
	REG_READ32(bari->bar0_vaddr + REG_OFFSET_PCIE_iATU, 0xb14);      //dst addr
	value = REG_READ32(bari->bar2_vaddr, 0x4);
	value |= 0x7;
	REG_WRITE32(bari->bar2_vaddr, 0x4, value);
	value = REG_READ32(bari->bar2_vaddr, 0x50);
	value |= (0x1 << 16);
	REG_WRITE32(bari->bar2_vaddr, 0x50, value);
	REG_READ32(bari->bar2_vaddr, 0x50);
	REG_WRITE32(bari->bar0_vaddr + REG_OFFSET_PCIE_iATU, 0xb14, 0x5ff00000);      //dst addr
	REG_READ32(bari->bar0_vaddr + REG_OFFSET_PCIE_iATU, 0xb14);      //dst addr

}

void bmdrv_pcie_set_function2_iatu_config(struct pci_dev *pdev, struct bm_device_info *bmdi)
{
	void __iomem *atu_base_addr;
	int function_num = 0x0;

	if (bmdi->cinfo.chip_index > 0)
		function_num = bmdi->cinfo.chip_index;
	else
		function_num = (pdev->devfn & 0x7);

	atu_base_addr = bmdi->cinfo.bar_info.bar0_vaddr + REG_OFFSET_PCIE_iATU; //0x5fb00000

	REG_WRITE32(atu_base_addr, 0x1d08, 0x0);
	REG_WRITE32(atu_base_addr, 0x1d0c, 0x0);
	REG_WRITE32(atu_base_addr, 0x1d10, 0x0);  //size 4M
	REG_WRITE32(atu_base_addr, 0x1d14, 0x80000000);
	REG_WRITE32(atu_base_addr, 0x1d18, 0x6);         //inbound tagrget address 0x6_8000_0000
	REG_WRITE32(atu_base_addr, 0x1d00, 0x200000);         //set function num as 2,type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x1d04, 0xc0080000); //bar match, match bar0

	REG_WRITE32(atu_base_addr, 0x1f08, 0x0);
	REG_WRITE32(atu_base_addr, 0x1f0c, 0x0);
	REG_WRITE32(atu_base_addr, 0x1f10, 0x0);  //size 4M
	REG_WRITE32(atu_base_addr, 0x1f14, 0x80000000);
	REG_WRITE32(atu_base_addr, 0x1f18, 0x7);         //inbound tagrget address 0x7_8000_0000
	REG_WRITE32(atu_base_addr, 0x1f00, 0x200000);         //set function num as 3,type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x1f04, 0xc0080100); //bar match, match bar1

	REG_WRITE32(atu_base_addr, 0x2108, 0x0);
	REG_WRITE32(atu_base_addr, 0x210c, 0x0);
	REG_WRITE32(atu_base_addr, 0x2110, 0x0);  //size 4M
	REG_WRITE32(atu_base_addr, 0x2114, 0x0);
	REG_WRITE32(atu_base_addr, 0x2118, 0x340 + function_num*0x20);         //inbound tagrget address 0x8_8000_0000
	REG_WRITE32(atu_base_addr, 0x2100, 0x200000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x2104, 0xc0080200); //bar match, match bar2

	REG_WRITE32(atu_base_addr, 0x2308, 0x0);
	REG_WRITE32(atu_base_addr, 0x230c, 0x0);
	REG_WRITE32(atu_base_addr, 0x2310, 0x0);  //size 1M
	REG_WRITE32(atu_base_addr, 0x2314, 0x80000000);
	REG_WRITE32(atu_base_addr, 0x2318, 0x9);       //inbound target address 0x9_8000_0000
	REG_WRITE32(atu_base_addr, 0x2300, 0x200000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x2304, 0xc0080400); //bar match, match bar4

	atu_base_addr = bmdi->cinfo.bar_info.bar2_vaddr; //0x5ff00000
	REG_WRITE32(atu_base_addr, 0xa08, 0x80000000);
	REG_WRITE32(atu_base_addr, 0xa0c, 0x6);       //outbound base address 6_8000_0000
	REG_WRITE32(atu_base_addr, 0xa10, 0x400000 + 0x80000000 - 0x1);  //size 4M
	REG_WRITE32(atu_base_addr, 0xa20, 0x6);  //
	REG_WRITE32(atu_base_addr, 0xa14, 0x8c000000);
	REG_WRITE32(atu_base_addr, 0xa18, 0x0);         //outbound tagrget address 8c00_0000
	REG_WRITE32(atu_base_addr, 0xa00, 0x0);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0xa04, 0x80000000); //address match

	REG_WRITE32(atu_base_addr, 0xc08, 0x80000000);
	REG_WRITE32(atu_base_addr, 0xc0c, 0x7);       //outbound base address 7_8000_0000
	REG_WRITE32(atu_base_addr, 0xc10, 0x400000 + 0x80000000 - 0x1);  //size 4M
	REG_WRITE32(atu_base_addr, 0xc20, 0x7);  //size 4M
	REG_WRITE32(atu_base_addr, 0xc14, 0x8d000000);
	REG_WRITE32(atu_base_addr, 0xc18, 0x0);         //outbound tagrget address 8d00_0000
	REG_WRITE32(atu_base_addr, 0xc00, 0x0);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0xc04, 0x80000000); //address match

	REG_WRITE32(atu_base_addr, 0xe08, 0x0);
	REG_WRITE32(atu_base_addr, 0xe0c, 0x340 + function_num*0x20);       //outbound base address 8_8000_0000
	REG_WRITE32(atu_base_addr, 0xe10, 0xffffffff);  //size 4M
	REG_WRITE32(atu_base_addr, 0xe20, 0x344 + function_num*0x20);  //size 4M
	REG_WRITE32(atu_base_addr, 0xe14, 0x0);
	REG_WRITE32(atu_base_addr, 0xe18, 0x340 + function_num*0x20);         //outbound tagrget address 8e00_0000
	REG_WRITE32(atu_base_addr, 0xe00, 0x0);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0xe04, 0x80000000); //address match

	REG_WRITE32(atu_base_addr, 0x1008, 0x80000000);
	REG_WRITE32(atu_base_addr, 0x100c, 0x9);       //outbound base address 8_8000_0000
	REG_WRITE32(atu_base_addr, 0x1010, 0x100000 + 0x80000000 - 0x1);  //size 1M
	REG_WRITE32(atu_base_addr, 0x1020, 0x08);  //size 1M
	REG_WRITE32(atu_base_addr, 0x1014, 0x8f000000);
	REG_WRITE32(atu_base_addr, 0x1018, 0x0);
	REG_WRITE32(atu_base_addr, 0x1000, 0x0);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x1004, 0x80000000); //address match

}

void bmdrv_pcie_set_function3_iatu_config(struct pci_dev *pdev, struct bm_device_info *bmdi)
{
	void __iomem *atu_base_addr;
	int function_num = 0x0;

	if (bmdi->cinfo.chip_index > 0)
		function_num = bmdi->cinfo.chip_index;
	else
		function_num = (pdev->devfn & 0x7);

	atu_base_addr = bmdi->cinfo.bar_info.bar0_vaddr + REG_OFFSET_PCIE_iATU; //0x5fb00000

	REG_WRITE32(atu_base_addr, 0x2508, 0x0);
	REG_WRITE32(atu_base_addr, 0x250c, 0x0);
	REG_WRITE32(atu_base_addr, 0x2510, 0x0);  //size 4M
	REG_WRITE32(atu_base_addr, 0x2514, 0x0);
	REG_WRITE32(atu_base_addr, 0x2518, 0xa);         //inbound tagrget address 0xa_0000_0000
	REG_WRITE32(atu_base_addr, 0x2500, 0x300000);         //set function num as 1,type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x2504, 0xc0080000); //bar match, match bar0

	REG_WRITE32(atu_base_addr, 0x2708, 0x0);
	REG_WRITE32(atu_base_addr, 0x270c, 0x0);
	REG_WRITE32(atu_base_addr, 0x2710, 0x0);  //size 4M
	REG_WRITE32(atu_base_addr, 0x2714, 0x0);
	REG_WRITE32(atu_base_addr, 0x2718, 0xb);         //inbound tagrget address 0xb_0000_0000
	REG_WRITE32(atu_base_addr, 0x2700, 0x300000);         //set function num as 1,type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x2704, 0xc0080100); //bar match, match bar1

	REG_WRITE32(atu_base_addr, 0x2908, 0x0);
	REG_WRITE32(atu_base_addr, 0x290c, 0x0);
	REG_WRITE32(atu_base_addr, 0x2910, 0x0);  //size 4M
	REG_WRITE32(atu_base_addr, 0x2914, 0x0);
	REG_WRITE32(atu_base_addr, 0x2918, 0x360 + function_num*0x20);         //inbound tagrget address 0xc_0000_0000
	REG_WRITE32(atu_base_addr, 0x2900, 0x300000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x2904, 0xc0080200); //bar match, match bar2

	REG_WRITE32(atu_base_addr, 0x2b08, 0x0);
	REG_WRITE32(atu_base_addr, 0x2b0c, 0x0);
	REG_WRITE32(atu_base_addr, 0x2b10, 0x0);  //size 1M
	REG_WRITE32(atu_base_addr, 0x2b14, 0x0);
	REG_WRITE32(atu_base_addr, 0x2b18, 0xd);       //inbound target address 0xd_0000_0000
	REG_WRITE32(atu_base_addr, 0x2b00, 0x300000);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x2b04, 0xc0080400); //bar match, match bar4

	atu_base_addr = bmdi->cinfo.bar_info.bar2_vaddr; //0x5ff00000
	REG_WRITE32(atu_base_addr, 0x1208, 0x0);
	REG_WRITE32(atu_base_addr, 0x120c, 0xa);       //outbound base address a_0000_0000
	REG_WRITE32(atu_base_addr, 0x1210, 0x400000 - 0x1);  //size 4M
	REG_WRITE32(atu_base_addr, 0x1220, 0xa);  //size 4M
	REG_WRITE32(atu_base_addr, 0x1214, 0x90000000);
	REG_WRITE32(atu_base_addr, 0x1218, 0x0);         //outbound tagrget address 9000_0000
	REG_WRITE32(atu_base_addr, 0x1200, 0x0);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x1204, 0x80000000); //address match

	REG_WRITE32(atu_base_addr, 0x1408, 0x0);
	REG_WRITE32(atu_base_addr, 0x140c, 0xb);       //outbound base address b_0000_0000
	REG_WRITE32(atu_base_addr, 0x1410, 0x400000 - 0x1);  //size 4M
	REG_WRITE32(atu_base_addr, 0x1420, 0xb);  //size 4M
	REG_WRITE32(atu_base_addr, 0x1414, 0x91000000);
	REG_WRITE32(atu_base_addr, 0x1418, 0x0);         //outbound tagrget address 9100_0000
	REG_WRITE32(atu_base_addr, 0x1400, 0x0);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x1404, 0x80000000); //address match

	REG_WRITE32(atu_base_addr, 0x1608, 0x0);
	REG_WRITE32(atu_base_addr, 0x160c, 0x360 + function_num*0x20);       //outbound base address c_0000_0000
	REG_WRITE32(atu_base_addr, 0x1610, 0xffffffff);
	REG_WRITE32(atu_base_addr, 0x1620, 0x364 + function_num*0x20);
	REG_WRITE32(atu_base_addr, 0x1614, 0x0);
	REG_WRITE32(atu_base_addr, 0x1618, 0x360 + function_num*0x20);         //outbound tagrget address 9200_0000
	REG_WRITE32(atu_base_addr, 0x1600, 0x0);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x1604, 0x80000000); //address match

	REG_WRITE32(atu_base_addr, 0x1808, 0x0);
	REG_WRITE32(atu_base_addr, 0x180c, 0xd);       //outbound base address d_0000_0000
	REG_WRITE32(atu_base_addr, 0x1810, 0x100000 - 0x1);  //size 1M
	REG_WRITE32(atu_base_addr, 0x1820, 0xd);            //size 1M
	REG_WRITE32(atu_base_addr, 0x1814, 0x93000000);
	REG_WRITE32(atu_base_addr, 0x1818, 0x0);
	REG_WRITE32(atu_base_addr, 0x1800, 0x0);         //type 4'b0000, MRd/MWr type
	REG_WRITE32(atu_base_addr, 0x1804, 0x80000000); //address match
}

int bmdrv_calculate_chip_num(int mode, int function_num)
{
	int chip_seqnum = 0;

	switch (mode) {
	case 0x7:
	case 0x6:
	case 0x5:
		chip_seqnum = 0x1;
		break;
	case 0x3:
		chip_seqnum = 0x1;
		break;
	case 0x2:
		chip_seqnum = 0x2;
		break;
	case 0x1:
		if (function_num == 0x2)
			chip_seqnum = 0x3;
		else if (function_num == 0x1)
			chip_seqnum = 0x2;
		break;
	case 0x0:
		if (function_num == 0x3)
			chip_seqnum = 0x4;
		else if (function_num == 0x2)
			chip_seqnum = 0x3;
		else if (function_num == 0x1)
			chip_seqnum = 0x2;
		break;
	default:
		chip_seqnum = 0x0;
		break;
	}
	return chip_seqnum;
}

void bmdrv_set_chip_num(int chip_seqnum, struct bm_device_info *bmdi)
{
	void __iomem *abp_addr;
	int temp_value = 0x0;

	abp_addr = bmdi->cinfo.bar_info.bar0_vaddr + REG_OFFSET_PCIE_iATU + 0x80000; //0x5fb00000
	temp_value = REG_READ32(abp_addr, 0);
	temp_value &= ~(0x7 << 28);
	temp_value |= (chip_seqnum << 28);
	REG_WRITE32(abp_addr, 0, temp_value);
}

void bmdrv_pcie_perst(struct bm_device_info *bmdi)
{
	int value = 0;

	value = gpio_reg_read(bmdi, 0x4);
	value |= (0x1 << 16);
	gpio_reg_write(bmdi, 0x4, value);

	value = gpio_reg_read(bmdi,0x0);
	pr_info("gpio 16 value = 0x%x\n", value);
	value |= (0x1 << 16);
	gpio_reg_write(bmdi, 0x0, value);
	mdelay(300);
}

int bmdrv_pcie_polling_rc_perst(struct pci_dev *pdev, struct bm_bar_info *bari)
{
	int loop = 200;
	int ret = 0;
	void __iomem *atu_base_addr;

	atu_base_addr = bari->bar0_vaddr + REG_OFFSET_PCIE_iATU + 0x80000; //0x5fb00000

	// Wait for preset by other
	while (!((REG_READ32(atu_base_addr, 0x4c) & (1 << 23)))) {
		if (loop-- > 0) {
			msleep(1000);
			pr_info("polling rc perst \n");
		}
		else {
			ret = -1;
			pr_info("polling rc perst time ount \n");
			break;
		}
	}

	return ret;
}

int bmdrv_pcie_polling_rc_core_rst(struct pci_dev *pdev, struct bm_bar_info *bari)
{
	int loop = 200;
	int ret = 0;
	void __iomem *atu_base_addr;
	atu_base_addr = bari->bar0_vaddr + REG_OFFSET_PCIE_iATU + 0x80000; //0x5fb00000

	// Wait for preset by other
	while (!(((REG_READ32(atu_base_addr, 0x48) & (1 << 2)) && (REG_READ32(atu_base_addr, 0x48) & (1 << 7))))) {
		if (loop-- > 0) {
			msleep(1000);
			pr_info("polling rc core rst \n");
		}
		else {
			ret = -1;
			pr_info("polling rc core time out\n");
			break;
		}
	}
	return ret;
}

void bmdrv_pcie_set_rc_link_speed_gen_x(struct bm_bar_info *bari, int gen_speed)
{
	int value = 0;

	REG_WRITE32(bari->bar0_vaddr + REG_OFFSET_PCIE_iATU, 0xb14, 0x5fc00000);      //dst addr
	REG_READ32(bari->bar0_vaddr + REG_OFFSET_PCIE_iATU, 0xb14);      //dst addr

	REG_WRITE32(bari->bar2_vaddr, 0x8bc, REG_READ32(bari->bar2_vaddr, 0x8bc) | 0x1); // enable DBI_RO_WR_EN

	REG_WRITE32(bari->bar2_vaddr, 0x80c, REG_READ32(bari->bar2_vaddr, 0x80c) | (0x1 << 17)); // enable pcie link change speed

	value = REG_READ32(bari->bar2_vaddr ,0xa0);
	value &= ~0xf;
	value |= (0x3 & gen_speed);
	REG_WRITE32(bari->bar2_vaddr, 0xa0, value); // cap_target_link_speed Gen3

	value = REG_READ32(bari->bar2_vaddr ,0x7c);
	value &= ~0xf;
	value |= (0x3 & gen_speed);
	REG_WRITE32(bari->bar2_vaddr , 0x7c, value); // cap_max_link_speed Gen3

	REG_WRITE32(bari->bar2_vaddr, 0x8bc, REG_READ32(bari->bar2_vaddr, 0x8bc) & (~0x1)); // disable DBI_RO_WR_EN

	REG_WRITE32(bari->bar0_vaddr + REG_OFFSET_PCIE_iATU, 0xb14, 0x5ff00000);      //dst addr
	REG_READ32(bari->bar0_vaddr + REG_OFFSET_PCIE_iATU, 0xb14);      //dst addr
}

void bmdrv_pcie_set_rc_max_payload_setting(struct bm_bar_info *bari)
{
	int value = 0;

	REG_WRITE32(bari->bar0_vaddr + REG_OFFSET_PCIE_iATU, 0xb14, 0x5fc00000);      //dst addr
	REG_READ32(bari->bar0_vaddr + REG_OFFSET_PCIE_iATU, 0xb14);      //dst addr
	REG_WRITE32(bari->bar2_vaddr, 0x8bc, REG_READ32(bari->bar2_vaddr, 0x8bc) | 0x1); // enable DBI_RO_WR_EN
	value = REG_READ32(bari->bar2_vaddr ,0x78);
	value &= ~(0x7<<5);
	value |= (0x1 << 5);
	REG_WRITE32(bari->bar2_vaddr, 0x78, value);
	REG_WRITE32(bari->bar2_vaddr, 0x8bc, REG_READ32(bari->bar2_vaddr, 0x8bc) & (~0x1)); // disable DBI_RO_WR_EN
	REG_WRITE32(bari->bar0_vaddr + REG_OFFSET_PCIE_iATU, 0xb14, 0x5ff00000);      //dst addr
	REG_READ32(bari->bar0_vaddr + REG_OFFSET_PCIE_iATU, 0xb14);      //dst addr
}

void bmdrv_pcie_enable_rc(struct bm_bar_info *bari)
{
	REG_WRITE32(bari->bar0_vaddr + REG_OFFSET_PCIE_APB, 0x258,  REG_READ32(bari->bar0_vaddr + REG_OFFSET_PCIE_APB, 0x258) | 0x1); //enable rc LTSSM
}

int bmdrv_pcie_polling_rc_link_state(struct bm_bar_info *bari)
{
	int value = 0x0;
	int count = 0x20;
	int ret = 0;

	value = REG_READ32(bari->bar0_vaddr + REG_OFFSET_PCIE_APB ,0x2b4);
	value = value >> 6;
	value &= 0x3;

	while(value != 0x3) {
		value = REG_READ32(bari->bar0_vaddr + REG_OFFSET_PCIE_APB ,0x2b4);
		value = value >> 6;
		value &= 0x3;

		if (count-- > 0) {
			msleep(2000);
			pr_info("wait link state count = %d\n", count);
		} else {
			ret = -1;
			pr_info("polling rc link time out\n");
			break;
		}
	}

	return ret;
}

int try_to_link_as_gen1_speed(struct pci_dev *pdev, struct bm_device_info *bmdi, struct bm_bar_info *bari)
{
	int ret = 0;

	bmdrv_pcie_perst(bmdi);
	bmdrv_pcie_polling_rc_perst(pdev, bari);
	bmdrv_pcie_polling_rc_core_rst(pdev, bari);
	bmdrv_pcie_set_rc_link_speed_gen_x(bari, 0x1);
	bmdrv_pcie_enable_rc(bari);

	if (bmdrv_pcie_polling_rc_link_state(bari) < 0) {
		ret = -1;
		pr_info("try_to_link_as_gen1_speed still fail \n");
	}

	return ret;
}

int bmdrv_pcie_rc_init(struct pci_dev *pdev, struct bm_device_info *bmdi, struct bm_bar_info *bari)
{
	int count = 0x5;
	int ret = 0;

retry:
	bmdrv_pcie_perst(bmdi);
	bmdrv_pcie_polling_rc_perst(pdev, bari);
	bmdrv_pcie_polling_rc_core_rst(pdev, bari);
	bmdrv_pcie_set_rc_link_speed_gen_x(bari, 0x3);
	bmdrv_pcie_enable_rc(bari);

	if (bmdrv_pcie_polling_rc_link_state(bari) < 0) {
		if (count-- > 0) {
			pr_info("rc link fail, retry %d\n", count);
			goto retry;
		} else {
			pr_info("rc link fail, retry still fail\n");
			ret = -1;
		}
	}

	if (ret < 0) {
		pr_info("try to link as gen1 speed \n");
		ret = try_to_link_as_gen1_speed(pdev, bmdi, bari);
	}

	bmdrv_pcie_set_rc_max_payload_setting(bari);

	return ret;
}

int config_iatu_for_function_x(struct pci_dev *pdev, struct bm_device_info *bmdi, struct bm_bar_info *bari)
{
	int mode = 0x0;
	int debug_value =0x0;
	int function_num = 0x0;
	int chip_seqnum = 0x0;
	int max_function_num = 0x0;
	int ret = 0x0;

	bm1684_set_chip_index(bmdi);
	if (bmdi->cinfo.chip_index > 0)
		function_num = bmdi->cinfo.chip_index;
	else
		function_num = (pdev->devfn & 0x7);
	bmdrv_init_for_mode_chose(bmdi,pdev, bari);
	io_init(bmdi);

	mode = bmdrv_pcie_get_mode(bmdi);
	pr_info("mode  = 0x%x\n", mode);
	debug_value = top_reg_read(bmdi,0);
	pr_info("top value = 0x%x\n",debug_value);
	if (mode == 0x4) {
		pr_info("single ep  mode \n");
		return 0;
	}

	if (mode == 0x3) {
		chip_seqnum = 0x1;
		bmdrv_set_chip_num(chip_seqnum, bmdi);
		return 0;
	}

	if (mode != 0) {
		bmdrv_pcie_rc_init(pdev, bmdi, bari);
	}

	pr_info("function_num = 0x%d \n", function_num);
	chip_seqnum = bmdrv_calculate_chip_num(mode, function_num);
	if ((chip_seqnum < 0x1) || (chip_seqnum > 0x4)) {
		pr_info("chip_seqnum is 0x%x illegal", chip_seqnum);
		return -1;
	}

	bmdrv_set_chip_num(chip_seqnum, bmdi);

	max_function_num = (mode & 0x3);

	switch (max_function_num) {
	case 0x3:
		bmdrv_pcie_set_function3_iatu_config(pdev, bmdi);
	case 0x2:
		bmdrv_pcie_set_function2_iatu_config(pdev, bmdi);
	case 0x1:
		bmdrv_pcie_set_function1_iatu_config(pdev, bmdi);
		ret = bmdrv_pci_bus_scan(pdev, bmdi, max_function_num);
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

u32 bmdrv_read_config(struct bm_device_info *bmdi, int cfg_base_addr, int offset)
{
	return bm_read32(bmdi, cfg_base_addr + offset);
}

void bmdrv_write_config(struct bm_device_info *bmdi, int cfg_base_addr, int offset, u32 value, u32 mask)
{
	u32 val = 0;
	val = bmdrv_read_config(bmdi, cfg_base_addr, offset);
	val = val | (value & mask);
	bm_write32(bmdi, cfg_base_addr + offset, val);
}

void bmdrv_pci_busmaster_memory_enable(struct bm_device_info *bmdi, int cfg_base_addr, int offset)
{
	bmdrv_write_config(bmdi, cfg_base_addr, 0x4,0x7,0x7);
}

void bmdrv_pci_msi_enable(struct bm_device_info *bmdi, int cfg_base_addr)
{
	bmdrv_write_config(bmdi, cfg_base_addr, 0x50,0x1 << 16,0x1<<16);
}
void bmdrv_pci_max_payload_setting(struct bm_device_info *bmdi, int cfg_base_addr)
{
	bmdrv_write_config(bmdi, cfg_base_addr, 0x78, 0x1 << 5,(0x7 << 5));
}

/*
*Cfg addrss translate
*		       Addr[27:20] = bus number
*                     Addr[19:15] = device number
*                     Addr[14:12] = function number
*                     Addr[11:0] = register offset
*/

int bmdrv_pci_bus_scan(struct pci_dev *pdev, struct bm_device_info *bmdi, int max_fun_num)
{
	int bus_num = 1;
	int cfg_base_addr = 0;
	int device_num = 0;
	int function_num = 0;
	int vendor_id   = 0;
	u32 count = 0;
	int root_function_num = 0x0;

	if (bmdi->cinfo.chip_index > 0)
		root_function_num = bmdi->cinfo.chip_index;
	else
		root_function_num = (pdev->devfn & 0x7);

	for (function_num = 0; function_num < max_fun_num; function_num++) {
		count = 600;
		cfg_base_addr = (bus_num << 20) | (device_num << 15) | (function_num << 12);
		cfg_base_addr += 0x60000000;
		while (vendor_id != 0x1e30) {
			vendor_id = bmdrv_read_config(bmdi, cfg_base_addr, 0) & 0xffff;
			msleep(10);
			if (--count == 0) {
				pr_err("bus num = %d, device = %d, function = %d,   not find, vendor_id = 0x%x \n", bus_num, device_num, function_num, vendor_id);
				break;
			}
		}

		pr_info("bus num = %d, device = %d, function = %d, vendor_id = 0x%x \n", bus_num, device_num, function_num, vendor_id);
		msleep(2000);
		if((function_num == 0x0) && (vendor_id == 0x1e30)) {
			bmdrv_write_config(bmdi, cfg_base_addr, 0x10, 0x88000000,0xfffffff0);
			bmdrv_write_config(bmdi, cfg_base_addr, 0x14, 0x89000000,0xfffffff0);
			bmdrv_write_config(bmdi, cfg_base_addr, 0x18, 0x0,0xfffffff0);
			bmdrv_write_config(bmdi, cfg_base_addr, 0x1c, 0x320 + root_function_num * 0x20,0xfffffff0);
			bmdrv_write_config(bmdi, cfg_base_addr, 0x20, 0x8b000000,0xfffffff0);
			bmdrv_pci_busmaster_memory_enable(bmdi, cfg_base_addr, 0x4);
			bmdrv_pci_msi_enable(bmdi, cfg_base_addr);
			bmdrv_pci_max_payload_setting(bmdi, cfg_base_addr);
			pr_info("find 1 bus num = %d,device = %d, function = %d,   find, vendor_id = 0x%x \n", bus_num, device_num, function_num, vendor_id);
			if (max_fun_num == 0x1)
				return 0;
		}
			if((function_num == 0x1) && (vendor_id == 0x1e30)) {
				bmdrv_write_config(bmdi, cfg_base_addr, 0x10, 0x8c000000,0xfffffff0);
				bmdrv_write_config(bmdi, cfg_base_addr, 0x14, 0x8d000000,0xfffffff0);
				bmdrv_write_config(bmdi, cfg_base_addr, 0x18, 0x0,0xfffffff0);
				bmdrv_write_config(bmdi, cfg_base_addr, 0x1c, 0x340 + root_function_num*0x20,0xfffffff0);
				bmdrv_write_config(bmdi, cfg_base_addr, 0x20, 0x8f000000,0xfffffff0);
				bmdrv_pci_busmaster_memory_enable(bmdi, cfg_base_addr, 0x4);
				bmdrv_pci_msi_enable(bmdi, cfg_base_addr);
				bmdrv_pci_max_payload_setting(bmdi, cfg_base_addr);
				pr_info("find 2 bus num = %d,device = %d, function = %d,   find, vendor_id = 0x%x \n", bus_num, device_num, function_num, vendor_id);
				if (max_fun_num == 0x2)
					return 0;
			}
			if((function_num == 0x2) && (vendor_id == 0x1e30)) {
				bmdrv_write_config(bmdi, cfg_base_addr, 0x10, 0x90000000,0xfffffff0);
				bmdrv_write_config(bmdi, cfg_base_addr, 0x14, 0x91000000,0xfffffff0);
				bmdrv_write_config(bmdi, cfg_base_addr, 0x18, 0x0,0xfffffff0);
				bmdrv_write_config(bmdi, cfg_base_addr, 0x1c, 0x360 + root_function_num*0x20,0xfffffff0);
				bmdrv_write_config(bmdi, cfg_base_addr, 0x20, 0x93000000,0xfffffff0);
				bmdrv_pci_busmaster_memory_enable(bmdi, cfg_base_addr, 0x4);
				bmdrv_pci_msi_enable(bmdi, cfg_base_addr);
				bmdrv_pci_max_payload_setting(bmdi, cfg_base_addr);
				pr_info("find 2 bus num = %d,device = %d, function = %d,   find, vendor_id = 0x%x \n", bus_num, device_num, function_num, vendor_id);
				if (max_fun_num == 0x3)
					return 0;
			}
		}
	pr_err("bus num = %d,device = %d, function = %d,  not find, vendor_id = 0x%x \n", bus_num, device_num, function_num, vendor_id);
	return -1;
}

void pci_slider_bar4_config_device_addr(struct bm_bar_info *bari, u32 addr)
{
	void __iomem *atu_base_addr;
	u32 dst_addr = 0;
	u32 temp_addr = 0;
	atu_base_addr = bari->bar0_vaddr + REG_OFFSET_PCIE_iATU; //0x5fb00000
	dst_addr = REG_READ32(atu_base_addr, 0x914); //dst addr

	if ((addr  > (dst_addr + 0xFFFFF)) || (addr < dst_addr) ) {
		temp_addr = addr & (~0xfffff);
		REG_WRITE32(atu_base_addr, 0x914, temp_addr); //dst addr
		temp_addr = REG_READ32(atu_base_addr, 0x914); //dst addr
	}
}
