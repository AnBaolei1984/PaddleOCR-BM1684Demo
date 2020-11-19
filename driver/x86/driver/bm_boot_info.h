#ifndef __BM_BOOT_INFO_H__
#define __BM_BOOT_INFO_H__
#include"bm_common.h"

int bmdrv_get_ddr_ecc_enable(struct bm_device_info *bmdi, unsigned int chip_id);
int bmdrv_misc_info_init(struct pci_dev *pdev, struct bm_device_info *bmdi);
void bmdrv_dump_bootinfo(struct bm_device_info *bmdi);
int bmdrv_set_default_boot_info(struct bm_device_info *bmdi);
int bmdrv_check_bootinfo(struct bm_device_info *bmdi);
int bmdrv_set_pld_boot_info(struct bm_device_info *bmdi);
int bmdrv_boot_info_init(struct bm_device_info *bmdi);
#endif
