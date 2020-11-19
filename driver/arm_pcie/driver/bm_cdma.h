#ifndef _BM_CDMA_H_
#define _BM_CDMA_H_

struct bm_device_info;
struct bm_memcpy_info;
typedef enum memcpy_dir
{
    HOST2CHIP,
    CHIP2HOST,
    CHIP2CHIP
} MEMCPY_DIR;

typedef struct bm_cdma_arg {
	u64 src;
	u64 dst;
	u64 size;
	MEMCPY_DIR dir;
	bool intr;
        bool use_iommu;
} bm_cdma_arg, *pbm_cdma_arg;

typedef enum {
    KERNEL_NOT_USE_IOMMU = 0,
    KERNEL_PRE_ALLOC_IOMMU,
    KERNEL_USER_SETUP_IOMMU
} bm_cdma_iommu_mode;

enum iommu_dma_dir {
    DMA_H2D = 0,
    DMA_D2H = 1,
};

/* Host Virtual Address Regsion*/
struct iommu_region {
	// pass from user to kernel
	uint64_t user_start; //  start addr
	uint32_t user_size;
	uint32_t dma_task_id;                     // not necessarily equal to API seq_no, one DMA task may have several objects
	uint32_t dir;                             // H2D or D2H
        uint32_t is_dst;  //0: src 1:dst
        uint32_t is_host; //0: device 1:host
	// pass from kernel to user
	uint64_t start_aligned;  //  start addr aligned
	uint32_t real_size;
	int32_t entry_start;
	int32_t real_num;
        int32_t occupy_num;
} __attribute__((__packed__));

struct iommu_ctrl {
    atomic_t supported;                             // low level support in chip side.
    //struct iommu_entry __iomem *device_entry_base;  // for bm1682, it is SMMU_PAGE_TABLE_BASE in BAR0

    //struct iommu_entry *host_entry_base;            // init: alloc SMMU_PAGE_ENTRY_NUM page table entry in host
    u64 host_entry_base_paddr;
    void* host_entry_base_vaddr;
    unsigned int entry_num;
    /* Manage page entry table as ring buffer,
     * (1) alloc_ptr: set when user setup iommu, wait for DMA flash
     * (2) free_ptr : set when iommu DMA flash done , run after alloc_ptr
     */
    unsigned int alloc_ptr;
    unsigned int free_ptr;
    unsigned int full;                              // Do we need it?  macro will better to judge entry full/empty
    unsigned int enabled;                           // support allocate VA address space, configure device page table
    bool enable;                                    // Where enable iommu for address translation.
    /* user try to setup iommu, but all the entry are used,
     * wait for page entry free
     */
    wait_queue_head_t entry_waitq;
    struct device *device;                           // for DMA , and debug control
};

/*  map virtual address to physical address per page */
struct bm_buffer_object {
    struct iommu_region iommu;        // VA region

    unsigned long nr_pages;
    struct page **pages;              // pages for VA, get from kernel VA/PA map (get_usr_pages)
    struct sg_table sgt;              // orgnize PA/VA in sg table for DMA.

    struct list_head entry;           // Insert to iommu_ctrl bo_list
};
#ifndef SOC_MODE
void bm_cdma_request_irq(struct bm_device_info *bmdi);
void bm_cdma_free_irq(struct bm_device_info *bmdi);
#endif
#ifdef SOC_MODE
#include <linux/irqreturn.h>
irqreturn_t bmdrv_irq_handler_cdma(int irq, void *data);
#endif
void bmdrv_cdma_irq_handler(struct bm_device_info *bmdi);
#include "bm1682_cdma.h"
#include "bm1684_cdma.h"
#include "bm1682_smmu.h"
#include "bm1684_smmu.h"
#endif
