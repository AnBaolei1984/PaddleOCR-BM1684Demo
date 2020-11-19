#ifndef _BM1684_PCIE_H_
#define _BM1684_PCIE_H_
#include "bm_io.h"

// BAR0
#define REG_OFFSET_PCIE_CFG		0x0
#define REG_OFFSET_PCIE_iATU	0x300000
#define REG_OFFSET_PCIE_APB	0x200000
#define REG_OFFSET_PCIE_TOP	(REG_OFFSET_PCIE_iATU + 0x80000)

// BAR1
#define BAR1_PART0_OFFSET	0x0
#define BAR1_PART1_OFFSET	0x80000
#define BAR1_PART2_OFFSET	0x380000
#define BAR1_PART3_OFFSET	0x390000
#define BAR1_PART4_OFFSET	0x391000
#define BAR1_PART5_OFFSET	0x392000
#define BAR1_PART6_OFFSET	0x393000
#define BAR1_PART7_OFFSET	0x394000

void bm1684_map_bar(struct bm_device_info *bmdi, struct pci_dev *pdev);
void bm1684_unmap_bar(struct bm_bar_info *bari);
int bm1684_setup_bar_dev_layout(struct bm_bar_info *, const struct bm_bar_info *);
void bm1684_pcie_calculate_cdma_max_payload(struct bm_device_info *bmdi);
void pci_slider_bar4_config_device_addr(struct bm_bar_info *bari, u32 addr);
int bmdrv_pcie_get_mode(struct bm_device_info *bmdi);
int bm1684_set_chip_index(struct bm_device_info *bmdi);

#endif
