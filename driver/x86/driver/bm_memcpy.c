#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include "bm_common.h"
#include "bm_memcpy.h"
#include "bm_cdma.h"
#include "vpu/vpu.h"
#include "bm_gmem.h"
#include "bm1684/bm1684_jpu.h"

int bmdrv_memcpy_init(struct bm_device_info *bmdi)
{
	int ret = 0;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;

	init_completion(&memcpy_info->cdma_done);
	mutex_init(&memcpy_info->cdma_mutex);

	bmdrv_stagemem_init(bmdi, &memcpy_info->stagemem_s2d);
	bmdrv_stagemem_init(bmdi, &memcpy_info->stagemem_d2s);

	switch (bmdi->cinfo.chip_id) {
	case 0x1682:
		memcpy_info->bm_cdma_transfer = bm1682_cdma_transfer;
		memcpy_info->bm_disable_smmu_transfer = bm1682_disable_smmu_transfer;
		memcpy_info->bm_enable_smmu_transfer = bm1682_enable_smmu_transfer;
		break;
	case 0x1684:
		memcpy_info->bm_cdma_transfer = bm1684_cdma_transfer;
		memcpy_info->bm_disable_smmu_transfer = bm1684_disable_smmu_transfer;
		memcpy_info->bm_enable_smmu_transfer = bm1684_enable_smmu_transfer;
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

void bmdrv_memcpy_deinit(struct bm_device_info *bmdi)
{
	bmdrv_stagemem_release(bmdi, &bmdi->memcpy_info.stagemem_s2d);
	bmdrv_stagemem_release(bmdi, &bmdi->memcpy_info.stagemem_d2s);
}

int bmdrv_stagemem_init(struct bm_device_info *bmdi, struct bm_stagemem *stagemem)
{
	int ret = 0;

	mutex_init(&stagemem->stage_mutex);
	ret = bmdrv_stagemem_alloc(bmdi, CONFIG_HOST_REALMEM_SIZE, &stagemem->p_addr,
			&stagemem->v_addr);
	if (ret)
		return ret;
	stagemem->size = CONFIG_HOST_REALMEM_SIZE;
	return ret;
}

int bmdrv_stagemem_release(struct bm_device_info *bmdi, struct bm_stagemem *stagemem)
{
	return bmdrv_stagemem_free(bmdi, stagemem->p_addr, stagemem->v_addr, stagemem->size);
}

int bmdrv_stagemem_free(struct bm_device_info *bmdi, u64 paddr, void *vaddr, u64 size)
{
#ifdef USE_DMA_COHERENT
	dma_free_coherent(bmdi->cinfo.device, size, vaddr, paddr);
#else
	set_memory_wb((unsigned long)vaddr, size / PAGE_SIZE);
	free_pages((unsigned long)vaddr, get_order(size));
#endif
	return 0;
}

int bmdrv_stagemem_alloc(struct bm_device_info *bmdi, u64 size, dma_addr_t *ppaddr,
		void **pvaddr)
{
	u32 arg32l;
	void *vaddr;
	dma_addr_t paddr;

	arg32l = (u32)size;
#ifdef USE_DMA_COHERENT
	vaddr = dma_alloc_coherent(bmdi->cinfo.device, arg32l, &paddr, GFP_USER);
#else
	/*
	 * use GFP_USER only would likely get paddr larger than 4GB,
	 * GFP_DMA can ensure paddr within 32bit. dma_alloc_coherent
	 * would likely do the same thing, check intel_alloc_coherent.
	 */
	vaddr = (void *)__get_free_pages(GFP_DMA | GFP_USER, get_order(size));
	memset(vaddr, 0, size);
	paddr = virt_to_phys(vaddr);
	/*
	 * use uncached-minus type of memory explicitly.
	 */
	set_memory_uc((unsigned long)vaddr, size / PAGE_SIZE);
#endif
	if (!vaddr) {
		dev_err(bmdi->dev, "buffer allocation failed\n");
		return -ENOMEM;
	}
	*ppaddr = paddr;
	if (pvaddr != NULL)
		*pvaddr = vaddr;
	return 0;
}

int bmdev_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct bm_device_info *bmdi = file->private_data;

#ifndef SOC_MODE
	u64 vpu_vmem_addr = 0, vpu_vmem_size = 0;

	vpu_vmem_addr = bmdi->vpudrvctx.s_video_memory.base;
	vpu_vmem_size = bmdi->vpudrvctx.s_video_memory.size;
#endif

	if (vma->vm_pgoff != 0) {
#ifdef SOC_MODE
		return bmdrv_gmem_map(bmdi, vma->vm_pgoff<<PAGE_SHIFT, vma->vm_end-vma->vm_start, vma);
#else
		if (((vma->vm_pgoff >= (vpu_vmem_addr >> PAGE_SHIFT)) &&
		     (vma->vm_pgoff <= ((vpu_vmem_addr + vpu_vmem_size)) >> PAGE_SHIFT))
		    || (vma->vm_pgoff == (bmdi->vpudrvctx.instance_pool[0].phys_addr >> PAGE_SHIFT))
		    || (vma->vm_pgoff == (bmdi->vpudrvctx.instance_pool[1].phys_addr >> PAGE_SHIFT))
		    || (vma->vm_pgoff == (bmdi->vpudrvctx.instance_pool[2].phys_addr >> PAGE_SHIFT))
		    || (vma->vm_pgoff == (bmdi->vpudrvctx.instance_pool[3].phys_addr >> PAGE_SHIFT))
		    || (vma->vm_pgoff == (bmdi->vpudrvctx.instance_pool[4].phys_addr >> PAGE_SHIFT))
		    || (vma->vm_pgoff == (bmdi->vpudrvctx.s_vpu_register[0].phys_addr >> PAGE_SHIFT))
		    || (vma->vm_pgoff == (bmdi->vpudrvctx.s_vpu_register[1].phys_addr >> PAGE_SHIFT))
		    || (vma->vm_pgoff == (bmdi->vpudrvctx.s_vpu_register[2].phys_addr >> PAGE_SHIFT))
		    || (vma->vm_pgoff == (bmdi->vpudrvctx.s_vpu_register[3].phys_addr >> PAGE_SHIFT))
		    || (vma->vm_pgoff == (bmdi->vpudrvctx.s_vpu_register[4].phys_addr >> PAGE_SHIFT))) {

			return bm_vpu_mmap(file, vma);

		} else if (!bm_jpu_addr_judge(vma->vm_pgoff, bmdi)) {
			return bm_jpu_mmap(file, vma);
		} else {
			dev_err(bmdi->dev, "pcie mode unsupport mmap, mmap failed!\n");
			return -EINVAL;
		}
#endif
	} else {
		dev_err(bmdi->dev, "vm_pgoff is 0, mmap failed!\n");
		return -EINVAL;
	}
}

void bmdev_construct_cdma_arg(pbm_cdma_arg parg,
		u64 src,
		u64 dst,
		u64 size,
		MEMCPY_DIR dir,
		bool intr,
		bool use_iommu)
{
	parg->src = src;
	parg->dst = dst;
	parg->size = size;
	parg->dir = dir;
	parg->intr = intr;
	parg->use_iommu = use_iommu;
}

void bmdev_construct_smmu_arg(struct iommu_region *iommu_rgn,
		u64 user_start,
		u64 user_size,
		u32 is_dst,
		u32 dir)
{
	iommu_rgn->user_start = user_start;
	iommu_rgn->user_size = user_size;
	iommu_rgn->is_dst = is_dst;
	iommu_rgn->dir = dir;
}

int bmdev_memcpy_s2d(struct bm_device_info *bmdi, struct file *file, uint64_t dst, void __user *src, u32 size,
		bool intr, bm_cdma_iommu_mode cdma_iommu_mode)
{
	u32 pass_idx = 0;
	u32 cur_addr_inc = 0;
	unsigned long size_step;
	u32 realmem_size = CONFIG_HOST_REALMEM_SIZE;
	void __user *src_cpy;
	bm_cdma_arg cdma_arg;
	int ret = 0;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;

	if (cdma_iommu_mode == KERNEL_USER_SETUP_IOMMU) {
		struct bm_buffer_object *bo_src = NULL;

		while (cur_addr_inc < size) {
			struct iommu_region iommu_rgn_src, iommu_rgn_dst;

			mutex_lock(&memcpy_info->cdma_mutex);
			memset(&iommu_rgn_src, 0, sizeof(struct iommu_region));
			memset(&iommu_rgn_dst, 0, sizeof(struct iommu_region));

			bmdev_construct_smmu_arg(&iommu_rgn_src, (u64)src + cur_addr_inc, size - cur_addr_inc, 0, DMA_H2D);
			bmdev_construct_smmu_arg(&iommu_rgn_dst, (u64)dst + cur_addr_inc, size - cur_addr_inc, 1, DMA_H2D);
			if ((memcpy_info->bm_enable_smmu_transfer(memcpy_info, &iommu_rgn_src, &iommu_rgn_dst, &bo_src) != 0)) {
				dev_err(bmdi->dev, "s2d enable smmu_trnasfer error\n");
				ret = -ENOMEM;
				mutex_unlock(&memcpy_info->cdma_mutex);
				break;
			}
			bmdev_construct_cdma_arg(&cdma_arg,
				(iommu_rgn_src.user_start - iommu_rgn_src.start_aligned) + iommu_rgn_src.entry_start * PAGE_SIZE,
				(iommu_rgn_dst.user_start - iommu_rgn_dst.start_aligned) + iommu_rgn_dst.entry_start * PAGE_SIZE,
				iommu_rgn_src.real_size,
				HOST2CHIP, intr, true);
			memcpy_info->bm_cdma_transfer(bmdi, file, &cdma_arg, false);
			cur_addr_inc += iommu_rgn_src.real_size;
			if ((memcpy_info->bm_disable_smmu_transfer(memcpy_info, &iommu_rgn_src, &iommu_rgn_dst, &bo_src) != 0)) {
				dev_err(bmdi->dev, "s2d disable smmu_trnasfer error\n");
				mutex_unlock(&memcpy_info->cdma_mutex);
				ret = -ENOMEM;
				break;
			}
			mutex_unlock(&memcpy_info->cdma_mutex);
		};
	} else { // treat prealloc mode as not use iommu
		for (pass_idx = 0, cur_addr_inc = 0; pass_idx < (size + realmem_size - 1) / realmem_size; pass_idx++) {
			if ((pass_idx + 1) * realmem_size < size)
				size_step = realmem_size;
			else
				size_step = size - pass_idx * realmem_size;
			src_cpy = (u8 __user *)src + cur_addr_inc;
			mutex_lock(&memcpy_info->stagemem_s2d.stage_mutex);
			if (copy_from_user(memcpy_info->stagemem_s2d.v_addr, src_cpy, size_step)) {
				mutex_unlock(&memcpy_info->stagemem_s2d.stage_mutex);
				pr_err("bmdev_memcpy_s2d copy_from_user fail\n");
				return -EFAULT;
			}
			bmdev_construct_cdma_arg(&cdma_arg, memcpy_info->stagemem_s2d.p_addr & 0xffffffffff,
				dst + cur_addr_inc, size_step, HOST2CHIP, intr, false);
			if (memcpy_info->bm_cdma_transfer(bmdi, file, &cdma_arg, true)) {
				mutex_unlock(&memcpy_info->stagemem_s2d.stage_mutex);
				return -EBUSY;
			}
			mutex_unlock(&memcpy_info->stagemem_s2d.stage_mutex);
			cur_addr_inc += size_step;
		}
	}
	return ret;
}

//for now, ignore mmu mode in memcpy_s2d_internal
int bmdev_memcpy_s2d_internal(struct bm_device_info *bmdi, u64 dst, const void *src, u32 size)
{
	u32 pass_idx = 0;
	u32 cur_addr_inc = 0;
	unsigned long size_step;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
	u32 realmem_size = memcpy_info->stagemem_s2d.size;
	void *src_cpy;
	bm_cdma_arg cdma_arg;

	for (pass_idx = 0, cur_addr_inc = 0; pass_idx < (size + realmem_size - 1) / realmem_size; pass_idx++) {
		if ((pass_idx + 1) * realmem_size < size)
			size_step = realmem_size;
		else
			size_step = size - pass_idx * realmem_size;
		src_cpy = (u8 *)src + cur_addr_inc;
		mutex_lock(&memcpy_info->stagemem_s2d.stage_mutex);
		memcpy(memcpy_info->stagemem_s2d.v_addr, src_cpy, size_step);
		bmdev_construct_cdma_arg(&cdma_arg, memcpy_info->stagemem_s2d.p_addr & 0xffffffffff,
				dst + cur_addr_inc, size_step, HOST2CHIP, false, false);
		if (memcpy_info->bm_cdma_transfer(bmdi, NULL, &cdma_arg, true)) {
			mutex_unlock(&memcpy_info->stagemem_s2d.stage_mutex);
			return -EBUSY;
		}
		mutex_unlock(&memcpy_info->stagemem_s2d.stage_mutex);
		cur_addr_inc += size_step;
	}
	return 0;
}

int bmdev_memcpy_d2s(struct bm_device_info *bmdi, struct file *file, void __user *dst, u64 src, u32 size,
		bool intr, bm_cdma_iommu_mode cdma_iommu_mode)
{
	u32 pass_idx = 0;
	u32 cur_addr_inc = 0;
	unsigned long size_step;
	u32 realmem_size = CONFIG_HOST_REALMEM_SIZE;
	void __user *dst_cpy;
	bm_cdma_arg cdma_arg;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
	int ret = 0;

	if (cdma_iommu_mode == KERNEL_USER_SETUP_IOMMU) {
		struct bm_buffer_object *bo_dst = NULL;

		while (cur_addr_inc < size) {
			struct iommu_region iommu_rgn_src, iommu_rgn_dst;

			mutex_lock(&memcpy_info->cdma_mutex);
			memset(&iommu_rgn_src, 0, sizeof(struct iommu_region));
			memset(&iommu_rgn_src, 0, sizeof(struct iommu_region));
			memset(&iommu_rgn_dst, 0, sizeof(struct iommu_region));

			bmdev_construct_smmu_arg(&iommu_rgn_src, (u64)src + cur_addr_inc, size - cur_addr_inc, 0, DMA_D2H);
			bmdev_construct_smmu_arg(&iommu_rgn_dst, (u64)dst + cur_addr_inc, size - cur_addr_inc, 1, DMA_D2H);
			if ((memcpy_info->bm_enable_smmu_transfer(memcpy_info, &iommu_rgn_src, &iommu_rgn_dst, &bo_dst) != 0)) {
				dev_err(bmdi->dev, "d2s enable smmu_trnasfer error\n");
				mutex_unlock(&memcpy_info->cdma_mutex);
				ret = -ENOMEM;
				break;
			}
			bmdev_construct_cdma_arg(&cdma_arg,
				(iommu_rgn_src.user_start - iommu_rgn_src.start_aligned) + iommu_rgn_src.entry_start * PAGE_SIZE,
				(iommu_rgn_dst.user_start - iommu_rgn_dst.start_aligned) + iommu_rgn_dst.entry_start * PAGE_SIZE,
				iommu_rgn_src.real_size, CHIP2HOST, intr, true);
			memcpy_info->bm_cdma_transfer(bmdi, file, &cdma_arg, false);
			cur_addr_inc += iommu_rgn_src.real_size;

			if ((memcpy_info->bm_disable_smmu_transfer(memcpy_info, &iommu_rgn_src, &iommu_rgn_dst, &bo_dst) != 0)) {
				dev_err(bmdi->dev, "d2s disable smmu_trnasfer error\n");
				ret = -ENOMEM;
				mutex_unlock(&memcpy_info->cdma_mutex);
				break;
			}
			mutex_unlock(&memcpy_info->cdma_mutex);
		};
	} else {
		for (pass_idx = 0, cur_addr_inc = 0; pass_idx < (size + realmem_size - 1) / realmem_size; pass_idx++) {
			if ((pass_idx + 1) * realmem_size < size)
				size_step = realmem_size;
			else
				size_step = size - pass_idx * realmem_size;
			dst_cpy = (u8 __user *)dst + cur_addr_inc;
			bmdev_construct_cdma_arg(&cdma_arg, src + cur_addr_inc,
				memcpy_info->stagemem_d2s.p_addr & 0xffffffffff,
				size_step, CHIP2HOST, intr, false);
			mutex_lock(&memcpy_info->stagemem_d2s.stage_mutex);
			if (memcpy_info->bm_cdma_transfer(bmdi, file, &cdma_arg, true)) {
				mutex_unlock(&memcpy_info->stagemem_d2s.stage_mutex);
				return -EBUSY;
			}
			if (copy_to_user(dst_cpy, memcpy_info->stagemem_d2s.v_addr, size_step)) {
				mutex_unlock(&memcpy_info->stagemem_d2s.stage_mutex);
				pr_err("bmdev_memcpy_d2s copy_to_user fail\n");
				return -EFAULT;
			}
			mutex_unlock(&memcpy_info->stagemem_d2s.stage_mutex);
			cur_addr_inc += size_step;
		}
	}
	return ret;
}

int bmdev_memcpy_c2c(struct bm_device_info *bmdi, struct file *file, u64 src, u64 dst, u32 size,
		bool intr, bm_cdma_iommu_mode cdma_iommu_mode)
{
	int ret = 0;
	bm_cdma_arg cdma_arg;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;

	bmdev_construct_cdma_arg(&cdma_arg, src,
			dst, size, CHIP2CHIP, intr, false);
	if (memcpy_info->bm_cdma_transfer(bmdi, file, &cdma_arg, true))
		return -EBUSY;
	return ret;
}

int bmdev_memcpy(struct bm_device_info *bmdi, struct file *file, unsigned long arg)
{
	int ret = 0;
	struct bm_memcpy_param memcpy_param;

	ret = copy_from_user(&memcpy_param, (const struct bm_memcpy_param __user *)arg,
			sizeof(memcpy_param));
	if (ret) {
		pr_err("bm-sophon%d copy_from_user fail\n", bmdi->dev_index);
		return ret;
	}
	if (memcpy_param.dir == HOST2CHIP)
		ret = bmdev_memcpy_s2d(bmdi, file, memcpy_param.device_addr, memcpy_param.host_addr,
				memcpy_param.size, memcpy_param.intr, memcpy_param.cdma_iommu_mode);
	else if (memcpy_param.dir == CHIP2HOST)
		ret = bmdev_memcpy_d2s(bmdi, file, memcpy_param.host_addr, memcpy_param.device_addr,
				memcpy_param.size, memcpy_param.intr, memcpy_param.cdma_iommu_mode);
	else if (memcpy_param.dir == CHIP2CHIP)
		ret = bmdev_memcpy_c2c(bmdi, file, memcpy_param.src_device_addr, memcpy_param.device_addr,
				memcpy_param.size, memcpy_param.intr, memcpy_param.cdma_iommu_mode);
	else {
		pr_err("bm-sophon%d memcpy_param dir err\n", bmdi->dev_index);
		ret = -EINVAL;
	}
	return ret;
}
