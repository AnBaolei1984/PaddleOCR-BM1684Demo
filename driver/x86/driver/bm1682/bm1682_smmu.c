#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <asm/page.h>
#include <linux/io.h>
#include "bm_common.h"
#include "bm_cdma.h"
#include "bm_memcpy.h"
#include "bm1682_smmu.h"
#include "bm1682_cdma.h"
#include "bm1682_reg.h"

#define IOMMU_IRQ_READ_ID  2
#define IOMMU_IRQ_WRITE_ID 3

/* SMMU bit definition */

/* Device view, which must be modified with the above one at the same time */
#ifndef SOC_MODE
#define SMMU_VIEW_OF_PAGE_TABLE       (0x100000000L + SMMU_RESERVED_START)
#else
#define SMMU_VIEW_OF_PAGE_TABLE       (0x200000000L + SMMU_RESERVED_START)
#endif

/* 1 entry for 4KB VA, 1MB page table for 1GB VA, 4GB too big,
 * driver can hardly allocate struct pages* from slab
 */

#define SMMU_FSR_WRITE_FAULT_TYPE (1 << 3) // 1: invalid page entry; 0: cross boundary (input VA too large)
#define SMMU_FSR_WRITE_FAULT      (1 << 2)
#define SMMU_FSR_READ_FAULT_TYPE  (1 << 1) // 1: invalid page entry; 0: cross boundary (input VA too large)
#define SMMU_FSR_READ_FAULT       (1 << 0)

#define SMMU_SCR_DISABLE           (0) // 1: enable address translation; 0: disable address translation, PA == VA
#define SMMU_SCR_ENABLE           (1 << 0) // 1: enable address translation; 0: disable address translation, PA == VA
#define SMMU_SCR_INTERRUPT_MASK   (1 << 1) // write 1 to mask interrupts, default value 1

#define IOMMU_TASK_ALIGNMENT (16) // every DMA task is aligned to 16 page entries(Hardware prefetch 16 entry), must be power of 2

/*
 * usage:
 * 1. acquire the big lock (optional)
 * 2. ioctl: bm_demand_iommu_entries
 * 3. ioctl: bm_bo_create -> list_add -> bm_setup_iommu_pages
 * 3. send host DMA API
 * 4. send host DMA descriptor
 * 5. wait host DMA API done
 * 6. ioctl: bm_free_iommu_pages -> list_del -> bm_bo_release
 * 7. ioctl: bm_release_iommu_entries
 * 8. release the big lock (optional)
 */

/* struct iommu_region
 *
 *      ----    <-- start_aligned / entry_start
 *     |    |
 *     |//0/|   ----  <--user_start
 *     |////|  |    |
 *      ----   |    |
 *             |    |
 *      ----   |    |
 *     |////|  |    |
 *     |//1/|  |    |
 *     |////|  |    |
 *      ----   |    |
 *             |    |
 *      ----   |    |
 *     |////|  |    |
 *     |//2/|  |    |
 *     |////|  |    |
 *      ----   |    | <-- real_size / real_num (as we may can't get enough IOMMU entries)
 *             |    |
 *      ----   |    |
 *     |////|  |    |
 *     |//3/|   ----  <-- user_size
 *     |    |
 *      ----    <-- end_aligned / entry_end (no actual variables)
 *
 * NOTE: here we assume data of pages in the middle (page 1 and 2 in above layout) must have
 * an offet=0. so we can not use bounce buffer: when we use dma_map_single on page 1 and
 * another dma_map_single on page 2, if bounce buffers are used, data will be copied to lower
 * page and the offset in the new page may not be 0, this will cause wrong calculation in DMA VA.
 * so we must guarantee that our PCIe device's DMA mask can cover the whole DRAM and no bounce
 * buffer is needed.
 */

/* every thread create a bm_buffer_object,
 * Data structure to store user address, Pages, sg table mapping
 */
int bm1682_disable_iommu(struct iommu_ctrl *ctrl);
static int bm_bo_create(struct bm_buffer_object **bo, struct iommu_region *iommu)
{
    struct bm_buffer_object *bbo = NULL;

    if ((iommu->user_start == 0) || (iommu->user_size == 0)) {
        pr_err("invalid parameters %lld %d.\n", iommu->user_start, iommu->user_size);
        return -EINVAL;
    }

    bbo = kzalloc(sizeof(struct bm_buffer_object), GFP_KERNEL);
    if (!bbo) {
        pr_err("alloc mem for bm buffer object failed.\n");
        return -ENOMEM;
    }

    bbo->iommu = *iommu;
    bbo->nr_pages = bbo->iommu.real_num;
    bbo->pages = kcalloc(bbo->nr_pages, sizeof(struct page *), GFP_KERNEL);
    if (!bbo->pages) {
        pr_err("alloc mem for struct page pointer failed.\n");
        kfree(bbo);
        return -ENOMEM;
    }

    INIT_LIST_HEAD(&bbo->entry);
    *bo = bbo;
    return 0;
}

static int bm_bo_release(struct bm_buffer_object *bo)
{
    kfree(bo->pages);
    kfree(bo);
    return 0;
}

static int printed = 0;

/* I/O copy host page table to device */
static int
iommu_validate_entries_to_device(struct iommu_ctrl *ctrl, int start, int number)
{
    struct bm_memcpy_info *memcpy_info= container_of(ctrl, struct bm_memcpy_info, iommuctl);
    struct bm_device_info *bmdi = container_of(memcpy_info, struct bm_device_info, memcpy_info);
    bm_cdma_arg cdma_arg;
    bmdev_construct_cdma_arg(&cdma_arg, ctrl->host_entry_base_paddr + start * sizeof(struct bm1682_iommu_entry),
                             bmdi->gmem_info.resmem_info.smmu_addr + start * sizeof(struct bm1682_iommu_entry),
                             number * sizeof(struct bm1682_iommu_entry), HOST2CHIP, false, false);

    if (bm1682_cdma_transfer(bmdi, NULL, &cdma_arg, false)) {
        dma_free_coherent(bmdi->cinfo.device, number * sizeof(struct bm1682_iommu_entry),
                               ctrl->host_entry_base_vaddr, ctrl->host_entry_base_paddr);
        pr_err("update device page table fail\n");
		return -EBUSY;
	}

    /* enable translation */
    //bm_enable_iommu(ctrl);

    smmu_reg_write(bmdi, SMMU_IR, 1);
    while ((smmu_reg_read(bmdi, SMMU_IS) & 0x1) != 0x1) {
	if(!printed){
        dev_info(ctrl->device, "flushing SMMU start 0x%x number 0x%x\n", start, number);
	printed ++;
	}
        mdelay(1);
    }
    return 0;
}

static int
iommu_invalidate_entries_to_device(struct iommu_ctrl *ctrl, int start, int number)
{
    struct bm_memcpy_info *memcpy_info= container_of(ctrl, struct bm_memcpy_info, iommuctl);
    struct bm_device_info *bmdi = container_of(memcpy_info, struct bm_device_info, memcpy_info);
	bm_cdma_arg cdma_arg;
    memset(ctrl->host_entry_base_vaddr + start * sizeof(struct bm1682_iommu_entry), 0, number * sizeof(struct bm1682_iommu_entry));

    bmdev_construct_cdma_arg(&cdma_arg, ctrl->host_entry_base_paddr + start * sizeof(struct bm1682_iommu_entry),
                             bmdi->gmem_info.resmem_info.smmu_addr + start * sizeof(struct bm1682_iommu_entry),
                             number * sizeof(struct bm1682_iommu_entry), HOST2CHIP, false, false);

    if (bm1682_cdma_transfer(bmdi, NULL, &cdma_arg, true)) {
        dma_free_coherent(bmdi->cinfo.device, number * sizeof(struct bm1682_iommu_entry),
                               ctrl->host_entry_base_vaddr, ctrl->host_entry_base_paddr);
		return -EBUSY;
	}

    return 0;
}

/* Availabe entry(b_size + t_szie) in ring buffer, two layout
(1) alloc_ptr ahead free_ptr

               free          alloc
 * +---- ~ -----+----- ~ -----+---- ~ -----+
 * |            |/////////////|            |
 * +---- ~ -----+----- ~ -----+---- ~ -----+
 * <---t_size---><-allocated--><--b_size--->

(2) free_ptr ahead alloc_ptr (b_size = 0)

               alloc         free
 * +---- ~ -----+----- ~ -----+---- ~ -----+
 * |////////////|             |////////////|
 * +---- ~ -----+----- ~ -----+---- ~ -----+
 * <-allocated--><---t_size---><-allocated->

 */
static int iommu_get_free_entries(struct iommu_ctrl *ctrl, int *t_half, int *b_half)
{
    int t_size, b_size;

    if (ctrl->full) {
        t_size = 0;
        b_size = 0;
        goto out;
    }

    if (ctrl->alloc_ptr >= ctrl->free_ptr) {
        t_size = ctrl->free_ptr;
        b_size = ctrl->entry_num - ctrl->alloc_ptr;
    } else {
        t_size = ctrl->free_ptr - ctrl->alloc_ptr;
        b_size = 0;
    }

out:
    if (t_half)
        *t_half = t_size;
    if (b_half)
        *b_half = b_size;
    return t_size + b_size;
}

/* user alloc memory, and ask for page entry.
 * host try to allocate page entry from avaiable space in ring buffer.
 * if not able to satisfy all user request, just complete partial number.
 * and tell user the real completed number in iommu_user ioctl response.
 * user will decide when to ask for remain part.
 */
static int iommu_alloc_entries(struct iommu_ctrl *ctrl, int number, int *start)
{
    int total, t_size, b_size, available;
    /* hardware limitation, prefetch 16 entries every time */
    int aligned_number = round_up(number, IOMMU_TASK_ALIGNMENT);

    total = iommu_get_free_entries(ctrl, &t_size, &b_size);
    if (total == 0) {
        available = 0;
        ctrl->full = 1;
        *start = -1;
        goto out;
    }
    /* Hardware DMA don't support download both b_size and t_size buffer simultaneously,
     * every time a user request only could get page entry in b_size or t_size.
     */
    if (b_size == 0){ // alloc_ptr < free_ptr, only top half is available
        if (t_size >= aligned_number) {
            *start = ctrl->alloc_ptr;
            ctrl->alloc_ptr += aligned_number;
            if(ctrl->alloc_ptr >= ctrl->free_ptr) ctrl->full = 1;
            available = aligned_number;
        } else {
            *start = ctrl->alloc_ptr;
            ctrl->alloc_ptr = ctrl->free_ptr;
            ctrl->full = 1;
            available = t_size;
        }
    } else {
        if (b_size >= aligned_number) {
            *start = ctrl->alloc_ptr;
            ctrl->alloc_ptr += aligned_number;
            if (ctrl->alloc_ptr >= ctrl->entry_num) {
                ctrl->alloc_ptr = 0;
                if (t_size == 0)
                    ctrl->full = 1;
            }
            available = aligned_number;
        } else { // use bottom half as best result
            *start = ctrl->alloc_ptr;
            ctrl->alloc_ptr = 0;
            available = b_size;
            if (t_size == 0)
                ctrl->full = 1;
        }
    }
out:
    return available;
}

static int iommu_free_entries(struct iommu_ctrl *ctrl, int number)
{
    int aligned_number = round_up(number, IOMMU_TASK_ALIGNMENT);

    if (aligned_number > ctrl->entry_num)
        return -EINVAL;

    /* Do not support free b_size and t_size simultaneouslly */
    ctrl->free_ptr += aligned_number;
    if (ctrl->free_ptr >= ctrl->entry_num)
        ctrl->free_ptr = 0;

    ctrl->full = 0;
    return 0;
}

/* user ioctl demand iommu entries
 * Input: iommu_region, VA of user memory region
 * 1. Make VA start PAGE_SIZE aligned
 * 2. Alloc page entries
 * 3. update iommu_region data structure to user
 */
static int bm_demand_iommu_entries(struct iommu_ctrl *ctrl, struct iommu_region *iommu_src, struct iommu_region *iommu_dst)
{
    int demand_src_pages, demand_dst_pages, demand_pages;
    uint64_t src_end_aligned;
    uint64_t dst_end_aligned;
    uint32_t real_size = iommu_src->user_size; // real transfer size, may cut down by ratio 0.9 repeatedly if available entry space lower than demand entry num
    int real_num;
    int ret;
    int t_half,b_half;
    if (iommu_src->user_start == 0 || iommu_src->user_size == 0 || iommu_dst->user_start == 0 || iommu_dst->user_size == 0) {
        dev_err(ctrl->device, "invalid input param from user space.");
        return -EINVAL;
    }

    iommu_src->start_aligned = round_down(iommu_src->user_start, PAGE_SIZE);
    src_end_aligned = round_up(iommu_src->user_start + iommu_src->user_size, PAGE_SIZE);
    demand_src_pages = (src_end_aligned - iommu_src->start_aligned) / PAGE_SIZE;    // demand src entry num

    iommu_dst->start_aligned = round_down(iommu_dst->user_start, PAGE_SIZE);
    dst_end_aligned = round_up(iommu_dst->user_start + iommu_dst->user_size, PAGE_SIZE);
    demand_dst_pages = (dst_end_aligned - iommu_dst->start_aligned) / PAGE_SIZE;

    demand_pages = round_up(demand_src_pages, IOMMU_TASK_ALIGNMENT) + round_up(demand_dst_pages, IOMMU_TASK_ALIGNMENT); // entry boundry between src and dst need aligned to 16
retry:

    /* best effort to fulfil user request */
    real_num = iommu_alloc_entries(ctrl, demand_pages, &iommu_src->entry_start);
    if (!real_num) {
        /* Other thread release iommu entries will wakeup current thread */
        ret = wait_event_interruptible(ctrl->entry_waitq, iommu_get_free_entries(ctrl, &t_half, &b_half));
        if (ret == -ERESTARTSYS)
            return -EINTR;
        goto retry;
    }else{
        if(real_num < 32){
            iommu_free_entries(ctrl, real_num);
            goto retry;
        }
        while(real_num < demand_pages){
            real_size = real_size / 2;  // cut down demand size by ratio 0.5 repeatedly
            src_end_aligned = round_up(iommu_src->user_start + real_size, PAGE_SIZE);
            demand_src_pages = (src_end_aligned - iommu_src->start_aligned) / PAGE_SIZE; //real page num need to store data

            dst_end_aligned = round_up(iommu_dst->user_start + real_size, PAGE_SIZE);
            demand_dst_pages = (dst_end_aligned - iommu_dst->start_aligned) / PAGE_SIZE;

	    demand_pages = round_up(demand_src_pages, IOMMU_TASK_ALIGNMENT) + round_up(demand_dst_pages, IOMMU_TASK_ALIGNMENT); // entry boundry between src and dst need aligned to 16
        }
    }
    iommu_dst->entry_start = iommu_src->entry_start + round_up(demand_src_pages, IOMMU_TASK_ALIGNMENT);
    iommu_src->real_num = demand_src_pages;
    iommu_dst->real_num = demand_dst_pages;
    iommu_src->real_size = real_size;
    iommu_dst->real_size = real_size;
    iommu_src->occupy_num = real_num;
    iommu_dst->occupy_num = real_num;
    return 0;
}

static int bm_release_iommu_entries(struct iommu_ctrl *ctrl, struct iommu_region *iommu_src, struct iommu_region *iommu_dst)
{
    if (iommu_src->entry_start < 0 || iommu_src->real_num <= 0 || iommu_dst->entry_start < 0 || iommu_dst->real_num <= 0)
        return -EINVAL;


    iommu_free_entries(ctrl, iommu_src->occupy_num);

    iommu_src->entry_start = iommu_src->real_num = -1;
    iommu_dst->entry_start = iommu_dst->real_num = -1;
    wake_up_all(&ctrl->entry_waitq);
    return 0;
}

/* 1. get physical page address for VA
 * 2. alloc sg_table for merge adjacent page to single page entry represent by scatterlist
 * 3. map page entry scatterlist to DMA address space
 * 3. assign DMA physical address for each iommu page entry
 * 4. write page entries to device
 */
static int bm_setup_iommu_pages(struct iommu_ctrl *ctrl, struct bm_buffer_object *bo)
{
    int ret = 0, i, entry_idx = 0;
    long page_done;
    struct scatterlist *sg;
    struct bm1682_iommu_entry *last_entry = NULL;

    if (bo->pages == NULL || bo->nr_pages == 0 ||
        bo->iommu.user_start == 0 || bo->iommu.user_size == 0 ||
        bo->iommu.real_size == 0)
        ret = -EINVAL;
    if (bo->iommu.start_aligned == 0 || bo->iommu.start_aligned & (PAGE_SIZE - 1))
        ret = -EINVAL;
    if (ret) {
        dev_err(ctrl->device, "invalid IOMMU config: [0x%llx / 0x%llx] [%d / %d] (%d) [%d / %d] [%p %ld]\n",
            bo->iommu.user_start, bo->iommu.start_aligned,
            bo->iommu.user_size, bo->iommu.real_size,
            bo->iommu.dma_task_id,
            bo->iommu.entry_start, bo->iommu.real_num,
            bo->pages, bo->nr_pages);
        return ret;
    }


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
    page_done = get_user_pages(bo->iommu.start_aligned, bo->nr_pages,
                    bo->iommu.is_dst == 1 ? 1 : 0, // dst need write, src only need read
                    bo->pages, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
    page_done = get_user_pages(bo->iommu.start_aligned, bo->nr_pages,
                    bo->iommu.is_dst == 1 ? 1 : 0, // dst need write, src only need read
                    0, bo->pages, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
    page_done = get_user_pages(current, current->mm, bo->iommu.start_aligned, bo->nr_pages,
                    bo->iommu.is_dst == 1 ? 1 : 0, // dst need write, src only need read
                    0, bo->pages, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 168)
    page_done = get_user_pages(current, current->mm, bo->iommu.start_aligned, bo->nr_pages,
                    bo->iommu.is_dst == 1 ? 1 : 0, // dst need write, src only need read
                    bo->pages, NULL);
#else
    page_done = get_user_pages(current, current->mm, bo->iommu.start_aligned, bo->nr_pages,
                    bo->iommu.is_dst == 1 ? 1 : 0, // dst need write, src only need read
                    0, bo->pages, NULL);
#endif

    if (page_done != bo->nr_pages) {
        dev_err(ctrl->device, "get_user_pages returned %ld against %ld\n", page_done, bo->nr_pages);
        if (page_done > 0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
            release_pages(bo->pages, page_done);
#else
            release_pages(bo->pages, page_done, 0);
#endif
        ret = -EFAULT;
        goto fail;
    }

    ret = sg_alloc_table_from_pages(&bo->sgt, bo->pages, bo->nr_pages,
                        bo->iommu.user_start & ~PAGE_MASK,
                        bo->iommu.real_size,
                        GFP_KERNEL);
    if (ret) {
        dev_err(ctrl->device, "sg_alloc_table_from_pages failed %d\n", ret);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
        release_pages(bo->pages, bo->nr_pages);
#else
        release_pages(bo->pages, bo->nr_pages, 0);
#endif
        ret = -EFAULT;
        goto fail;
    }

    ret = dma_map_sg(ctrl->device, bo->sgt.sgl, bo->sgt.nents,
                bo->iommu.dir == HOST2CHIP ? DMA_TO_DEVICE : DMA_FROM_DEVICE);

    if (ret == 0) {
        dev_err(ctrl->device, "dma_map_sg failed\n");
        sg_free_table(&bo->sgt);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
        release_pages(bo->pages, bo->nr_pages);
#else
        release_pages(bo->pages, bo->nr_pages, 0);
#endif
        ret = -EFAULT;
        goto fail;
    } else {
        bo->sgt.nents = ret;
    }

    for_each_sg(bo->sgt.sgl, sg, bo->sgt.nents, i) {
        dma_addr_t start_aligned = round_down(sg_dma_address(sg), PAGE_SIZE);
        dma_addr_t end_aligned = round_up(sg_dma_address(sg) + sg_dma_len(sg), PAGE_SIZE);
        int j;
        for (j = 0; j < (end_aligned - start_aligned) / PAGE_SIZE; j++) {
            struct bm1682_iommu_entry *entry = ctrl->host_entry_base_vaddr + sizeof(struct bm1682_iommu_entry) * (bo->iommu.entry_start + entry_idx);
            dma_addr_t paddr = (start_aligned + j * PAGE_SIZE);
            entry->paddr = paddr >> PAGE_SHIFT;
            entry->valid = 1;
            last_entry = entry;

            entry_idx++;
	 }
    }

    /* DMA stop transfer when hit this flag, used to separate per thread bmVA sapce */
    last_entry->dma_task_end = 1;

    iommu_validate_entries_to_device(ctrl, bo->iommu.entry_start, bo->iommu.real_num);

fail:
    return ret;
}

static int bm_setup_raw_iommu_pages(struct iommu_ctrl *ctrl,  struct iommu_region *iommu)
{
    struct bm1682_iommu_entry *entry = NULL;
    dma_addr_t paddr;
    int i = 0;

    for(i = 0; i < iommu->real_num; i++){
        entry = ctrl->host_entry_base_vaddr + sizeof(struct bm1682_iommu_entry) * (iommu->entry_start + i);
        paddr = iommu->start_aligned + i * PAGE_SIZE;
        entry->paddr = paddr >> PAGE_SHIFT;
        entry->valid = 1;
    }
    entry->dma_task_end = 1;

    iommu_validate_entries_to_device(ctrl, iommu->entry_start, iommu->real_num);

    return 0;
}

static int bm_free_iommu_pages(struct  iommu_ctrl *ctrl, struct bm_buffer_object *bo)
{
    int ret = 0, i;
    if (bo->iommu.entry_start < 0 || bo->iommu.real_num <= 0)
        return -EINVAL;

    for (i = 0; i < bo->iommu.real_num; i++) {
        struct bm1682_iommu_entry *entry = ctrl->host_entry_base_vaddr + sizeof(struct bm1682_iommu_entry) * (bo->iommu.entry_start + i);

        entry->valid = 0;
    }
    dma_unmap_sg(ctrl->device, bo->sgt.sgl, bo->sgt.nents,
            bo->iommu.dir == HOST2CHIP ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
    sg_free_table(&bo->sgt);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
    release_pages(bo->pages, bo->nr_pages);
#else
    release_pages(bo->pages, bo->nr_pages, 0);
#endif

    iommu_invalidate_entries_to_device(ctrl, bo->iommu.entry_start, bo->iommu.real_num);

    return ret;
}

static int bm_free_raw_iommu_pages(struct iommu_ctrl *ctrl, struct iommu_region *iommu)
{
    int ret = 0;
    int i;
    struct bm1682_iommu_entry *entry;

    for (i = 0; i < iommu->real_num; i++) {
        entry = ctrl->host_entry_base_vaddr + sizeof(struct bm1682_iommu_entry) * (iommu->entry_start + i);
        entry->valid = 0;
    }

    iommu_invalidate_entries_to_device(ctrl, iommu->entry_start, iommu->real_num);

    return ret;
}
/* Default supported for 1682, better to read capability from reigster */
static int bm_check_device_iommu_cap(struct iommu_ctrl *ctrl)
{
    atomic_set(&ctrl->supported, 1);
    return 0;
}

/* Initilize iommu
 * 1. Init iommu_ctrl data structure
 * 2. Allocate host page entry ring buffer
 */
int bm1682_init_iommu(struct iommu_ctrl *ctrl, struct device *dev)
{
    int entry_num = SMMU_PAGE_ENTRY_NUM;
    struct bm_memcpy_info *memcpy_info= container_of(ctrl, struct bm_memcpy_info, iommuctl);
    struct bm_device_info *bmdi = container_of(memcpy_info, struct bm_device_info, memcpy_info);

    if (bm_check_device_iommu_cap(ctrl) != 0)
        return 0;
    /* entry num must be aligned to 16 */
    if ((entry_num == 0) || (entry_num & (IOMMU_TASK_ALIGNMENT - 1))) {
        dev_err(ctrl->device, "entry num must be aligned to 16, but actual num:%d", entry_num);
        return -EINVAL;
    }

    ctrl->host_entry_base_vaddr = dma_alloc_coherent(bmdi->cinfo.device, entry_num * sizeof(struct bm1682_iommu_entry),
                                                     &ctrl->host_entry_base_paddr, GFP_KERNEL);
    if (!ctrl->host_entry_base_vaddr) {
        dev_err(ctrl->device, "malloc mem for iommu entry failed\n");
        return -ENOMEM;
    }

    memset(ctrl->host_entry_base_vaddr, 0, entry_num * sizeof(struct bm1682_iommu_entry));

    ctrl->entry_num = entry_num;
    ctrl->free_ptr = ctrl->alloc_ptr = ctrl->full = 0;
    ctrl->enable = false;
    ctrl->device = dev;
    init_waitqueue_head(&ctrl->entry_waitq);
    iommu_invalidate_entries_to_device(ctrl, 0, ctrl->entry_num);
    return 0;
}

int bm1682_deinit_iommu(struct iommu_ctrl *ctrl)
{
    struct bm_memcpy_info *memcpy_info= container_of(ctrl, struct bm_memcpy_info, iommuctl);
    struct bm_device_info *bmdi = container_of(memcpy_info, struct bm_device_info, memcpy_info);

    if (atomic_read(&ctrl->supported) == 0)
        return 0;

    if (ctrl->enable) {
        bm1682_disable_iommu(ctrl);
        iommu_invalidate_entries_to_device(ctrl, 0, ctrl->entry_num);
    }

    dma_free_coherent(bmdi->cinfo.device, SMMU_PAGE_ENTRY_NUM * sizeof(struct bm1682_iommu_entry),
                      ctrl->host_entry_base_vaddr, ctrl->host_entry_base_paddr);
    ctrl->free_ptr = ctrl->alloc_ptr = ctrl->full = 0;
    wake_up_all(&ctrl->entry_waitq); // wakeup pending callers
    return 0;
}

/* Enable iommu by write related registers. */
static int bmdev_enable_iommu(struct iommu_ctrl *ctrl)
{
    struct bm_memcpy_info *memcpy_info= container_of(ctrl, struct bm_memcpy_info, iommuctl);
    struct bm_device_info *bmdi = container_of(memcpy_info, struct bm_device_info, memcpy_info);

    if (ctrl->enable == true)
        return 0;

    if (atomic_read(&ctrl->supported) == 0) {
        return -EPERM;
    }

    dev_info(ctrl->device, "Enable SMMU.\n");
    /*
     * Workaround for bm1682, read the register once again after writing,
     * which aims to increase the delay between 2 write operations
     */
    smmu_reg_write(bmdi, SMMU_ICR, 1);      //clear interrupt
    smmu_reg_read(bmdi, SMMU_ICR);

    smmu_reg_write(bmdi, SMMU_IR, 1);       //clear prefetched page table in cache
    smmu_reg_read(bmdi, SMMU_IR);


    //check cache status for page table
    while ((smmu_reg_read(bmdi, SMMU_IS) & 0x1) != 0x1) {
	if(!printed){
	printed ++;
        dev_info(ctrl->device, "flushing SMMU\n");
	}
        mdelay(1);
    }

    /* 64bit TTBR register */
    smmu_reg_write(bmdi, SMMU_TTBR, SMMU_VIEW_OF_PAGE_TABLE & 0xFFFFFFFF);
    smmu_reg_read(bmdi, SMMU_TTBR);

    smmu_reg_write(bmdi, SMMU_TTBR + 4, SMMU_VIEW_OF_PAGE_TABLE >> 32);
    smmu_reg_read(bmdi, SMMU_TTBR + 4);

    /* 64bit TTER register */
    smmu_reg_write(bmdi, SMMU_TTER, (SMMU_PAGE_ENTRY_NUM * PAGE_SIZE - 1) & 0xFFFFFFFF);
    smmu_reg_read(bmdi, SMMU_TTER);

    smmu_reg_write(bmdi, SMMU_TTER + 4, (SMMU_PAGE_ENTRY_NUM * PAGE_SIZE - 1) >> 32);
    smmu_reg_read(bmdi, SMMU_TTER + 4);

    /* enable translation */
    smmu_reg_write(bmdi, SMMU_SCR, SMMU_SCR_ENABLE);
    smmu_reg_read(bmdi, SMMU_SCR);

    ctrl->enable = true;

    return 0;
}

/*  Disable iommu by write registers
 */
int bm1682_disable_iommu(struct iommu_ctrl *ctrl)
{
    struct bm_memcpy_info *memcpy_info= container_of(ctrl, struct bm_memcpy_info, iommuctl);
    struct bm_device_info *bmdi = container_of(memcpy_info, struct bm_device_info, memcpy_info);

    if (ctrl->enable == false)
        return 0;

    if (atomic_read(&ctrl->supported) == 0) {
        return -EPERM;
    }

    dev_info(ctrl->device, "Disable SMMU.\n");

    smmu_reg_write(bmdi, SMMU_ICR, 1);
    smmu_reg_read(bmdi, SMMU_ICR);

    smmu_reg_write(bmdi, SMMU_IR, 1);
    smmu_reg_read(bmdi, SMMU_IR);

    while ((smmu_reg_read(bmdi, SMMU_IS) & 0x1) != 0x1) {
        dev_info(ctrl->device, "flushing SMMU\n");
        mdelay(1);
    }

    smmu_reg_write(bmdi, SMMU_SCR, SMMU_SCR_INTERRUPT_MASK);
    smmu_reg_read(bmdi, SMMU_SCR);

    smmu_reg_write(bmdi, SMMU_SCR, SMMU_SCR_DISABLE);
    smmu_reg_read(bmdi, SMMU_SCR);

    ctrl->enable = false;
    return 0;
}

/* Device Page Fault Interrupt Handler:
 * Now, user configure iommu with ioctl while alloc memory, so page fault
 * is regard as a hardware error, driver just ignore
 * 1. device iommu stop work, upload interrupt
 * 2. kernel driver process Interrupt:
 * 3. kernel driver write SMMU_ICR
 * 4. device iommu continue work(should be reset in current implementation)
 */

#ifndef SOC_MODE
static void bm1682_iommu_irq_handler(struct bm_device_info *bmdi)
{
	smmu_reg_write(bmdi, SMMU_ICR, 1);
}
#endif
void bm1682_smmu_get_irq_status(struct bm_device_info *bmdi, u32 *status)
{
	u32 irq_status;
	struct iommu_ctrl *ctrl = &bmdi->memcpy_info.iommuctl;
	irq_status = smmu_reg_read(bmdi, SMMU_FSR);
	if (irq_status & SMMU_FSR_WRITE_FAULT) {
		status[0] |= 1<<2;
		dev_err(ctrl->device, "SMMU write fault: 0x%x, addr 0x%llx\n",
				irq_status, bm_read64(bmdi, bmdi->cinfo.bm_reg->smmu_base_addr + SMMU_FARW));
	}

	if (irq_status & SMMU_FSR_READ_FAULT) {
		status[0] |= 1<<3;
		dev_err(ctrl->device, "SMMU read fault: 0x%x, addr 0x%llx\n",
				irq_status, bm_read64(bmdi, bmdi->cinfo.bm_reg->smmu_base_addr + SMMU_FARR));
	}
}

void bm1682_iommu_request_irq(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	bmdrv_submodule_request_irq(bmdi, IOMMU_IRQ_READ_ID, bm1682_iommu_irq_handler);
	bmdrv_submodule_request_irq(bmdi, IOMMU_IRQ_WRITE_ID, bm1682_iommu_irq_handler);
#endif
}

void bm1682_iommu_free_irq(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	bmdrv_submodule_free_irq(bmdi, IOMMU_IRQ_READ_ID);
	bmdrv_submodule_free_irq(bmdi, IOMMU_IRQ_WRITE_ID);
#endif
}

int bm1682_enable_smmu_transfer(struct bm_memcpy_info *memcpy_info, struct iommu_region *iommu_rgn_src,
		struct iommu_region *iommu_rgn_dst, struct bm_buffer_object **bo_buffer)
{
	int ret = 0;
	struct iommu_region *raw_rgn = iommu_rgn_dst;
	struct iommu_region *bo_rgn = iommu_rgn_src;

	if (iommu_rgn_src->dir == DMA_D2H) {
		raw_rgn = iommu_rgn_src;
		bo_rgn = iommu_rgn_dst;
	}

	if((ret = bm_demand_iommu_entries(&memcpy_info->iommuctl, iommu_rgn_src, iommu_rgn_dst)) != 0) {
		// how to recover???
		dev_err(memcpy_info->iommuctl.device, "alloc iommu failed! %d\n", ret);
		ret = -ENOMEM;
		return ret;
	}
	if((ret = bm_bo_create(bo_buffer, bo_rgn)) != 0) {
		dev_err(memcpy_info->iommuctl.device, "bm_bo_create src failed %d\n", ret);
		ret = -ENOMEM;
		return ret;
	}
	if((ret = bm_setup_iommu_pages(&memcpy_info->iommuctl, *bo_buffer)) < 0) {
		list_del(&(*bo_buffer)->entry);
		bm_bo_release(*bo_buffer);
		dev_err(memcpy_info->iommuctl.device, "bm_setup_iommu_pages src failed %d\n", ret);
		ret = -EIO;
		return ret;
	}
	bm_setup_raw_iommu_pages(&memcpy_info->iommuctl, raw_rgn);
	bmdev_enable_iommu(&memcpy_info->iommuctl);
	return ret;
}

int bm1682_disable_smmu_transfer(struct bm_memcpy_info *memcpy_info, struct iommu_region *iommu_rgn_src, struct iommu_region *iommu_rgn_dst, struct bm_buffer_object **bo_buffer)
{
	int ret = 0;
	struct iommu_region *raw_rgn = iommu_rgn_dst;

	if(iommu_rgn_src->dir == DMA_D2H)
		raw_rgn = iommu_rgn_src;

	bm1682_disable_iommu(&memcpy_info->iommuctl);
	ret = bm_free_iommu_pages(&memcpy_info->iommuctl, *bo_buffer);
	if(ret) {
		dev_err(memcpy_info->iommuctl.device, "bm_free_src_iommu_pages failed %d", ret);
		ret = -EFAULT;
	}
	ret = bm_free_raw_iommu_pages(&memcpy_info->iommuctl, raw_rgn);
	if(ret) {
		dev_err(memcpy_info->iommuctl.device, "bm_free_dst_iommu_pages failed %d", ret);
		ret = -EFAULT;
	}
	ret = bm_bo_release(*bo_buffer);
	if(ret) {
		dev_err(memcpy_info->iommuctl.device, "bm_bo_src_release failed %d", ret);
		ret = -EFAULT;
	}
	ret = bm_release_iommu_entries(&memcpy_info->iommuctl, iommu_rgn_src, iommu_rgn_dst);
	if(ret) {
		dev_err(memcpy_info->iommuctl.device, "bm_release_iommu_entries failed %d\n", ret);
		return -EFAULT;
	}

	return  ret;
}
