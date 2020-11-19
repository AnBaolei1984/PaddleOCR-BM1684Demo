#ifndef _BM_MEMCPY_H_
#define _BM_MEMCPY_H_

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sizes.h>
#include <asm/pgalloc.h>
#include "bm_cdma.h"

#ifdef SOC_MODE
#define CONFIG_HOST_REALMEM_SIZE 0x100000
#else
#define CONFIG_HOST_REALMEM_SIZE 0x400000
#endif

struct bm_stagemem {
	void *v_addr;
	u64 p_addr;
	u32 size;
	struct mutex stage_mutex;
};

struct mmap_info {
	void *vaddr;
	phys_addr_t paddr;
	size_t size;
};

struct bm_memcpy_info {
	struct bm_stagemem stagemem_s2d;
	struct bm_stagemem stagemem_d2s;

	struct mmap_info mminfo;

	struct completion cdma_done;
	struct mutex cdma_mutex;
	int cdma_max_payload;

	struct iommu_ctrl iommuctl;
	int (*bm_memcpy_init)(struct bm_device_info *);
	void (*bm_memcpy_deinit)(struct bm_device_info *);
	u32 (*bm_cdma_transfer)(struct bm_device_info *, struct file *, pbm_cdma_arg, bool);
	int (*bm_disable_smmu_transfer)(struct bm_memcpy_info *, struct iommu_region *, struct iommu_region *, struct bm_buffer_object **);
	int (*bm_enable_smmu_transfer)(struct bm_memcpy_info *, struct iommu_region *, struct iommu_region *, struct bm_buffer_object **);
};

struct bm_memcpy_param {
	void __user *host_addr;
	u64 src_device_addr;
	u64 device_addr;
	u32 size;
	MEMCPY_DIR dir;
	bool intr;
	bm_cdma_iommu_mode cdma_iommu_mode;
};

void bmdev_construct_cdma_arg(pbm_cdma_arg parg, u64 src, u64 dst, u64 size, MEMCPY_DIR dir,
		bool intr, bool use_iommu);
int bmdrv_memcpy_init(struct bm_device_info *bmdi);
void bmdrv_memcpy_deinit(struct bm_device_info *bmdi);
int bmdev_mmap(struct file *file, struct vm_area_struct *vma);
int bmdrv_stagemem_init(struct bm_device_info *bmdi, struct bm_stagemem *stagemem);
int bmdrv_stagemem_release(struct bm_device_info *bmdi, struct bm_stagemem *stagemem);
int bmdrv_stagemem_alloc(struct bm_device_info *bmdi, u64 size, dma_addr_t *ppaddr, void **pvaddr);
int bmdrv_stagemem_free(struct bm_device_info *bmdi, u64 paddr, void *vaddr, u64 size);
int bmdev_memcpy(struct bm_device_info *bmdi, struct file *file, unsigned long arg);
int bmdev_memcpy_s2d_internal(struct bm_device_info *bmdi, u64 dst, const void *src, u32 size);
int bmdev_memcpy_s2d(struct bm_device_info *bmdi,  struct file *file,
		uint64_t dst, void __user *src, u32 size, bool intr, bm_cdma_iommu_mode cdma_iommu_mode);

#endif
