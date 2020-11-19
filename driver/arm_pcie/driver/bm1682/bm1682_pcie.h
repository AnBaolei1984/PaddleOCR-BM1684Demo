#ifndef _BM1682_PCIE_H_
#define _BM1682_PCIE_H_
#include "bm_io.h"

void bm1682_map_bar(struct bm_device_info *bmdi, struct pci_dev *pdev);
void bm1682_unmap_bar(struct bm_bar_info *bari);
void bm1682_pcie_calculate_cdma_max_payload(struct bm_device_info *bmdi);
int bm1682_setup_bar_dev_layout(struct bm_bar_info *, const struct bm_bar_info *);
#endif
