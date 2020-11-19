#ifndef BM1682_SMMU_H
#define BM1682_SMMU_H

#include "bm_cdma.h"

struct bm1682_iommu_entry {
    u32 paddr : 30; // right shifted 12bits from original physical address
    u32 dma_task_end : 1;
    u32 valid : 1;
} __packed;

#ifndef SOC_MODE
void bm1682_iommu_request_irq(struct bm_device_info *bmdi);
void bm1682_iommu_free_irq(struct bm_device_info *bmdi);
#endif

int bm1682_enable_smmu_transfer(struct bm_memcpy_info *memcpy_info, struct iommu_region *iommu_rgn_src, struct iommu_region *iommu_rgn_dst, struct bm_buffer_object **bo_buffer);
int bm1682_disable_smmu_transfer(struct bm_memcpy_info *memcpy_info, struct iommu_region *iommu_rgn_src, struct iommu_region *iommu_rgn_dst, struct bm_buffer_object **bo_buffer);
int bm1682_init_iommu(struct iommu_ctrl *ctrl, struct device *dev);
int bm1682_deinit_iommu(struct iommu_ctrl *ctrl);

#endif /* _BM_IOMMU_H_ */
