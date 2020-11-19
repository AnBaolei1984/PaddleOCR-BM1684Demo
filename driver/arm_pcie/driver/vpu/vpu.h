//=========================================================================
//  This file is linux device driver for VPU.
//-------------------------------------------------------------------------
//
//       This confidential and proprietary software may be used only
//     as authorized by a licensing agreement from Chips&Media Inc.
//     In the event of publication, the following notice is applicable:
//
//            (C) COPYRIGHT 2006 - 2015  CHIPS&MEDIA INC.
//                      ALL RIGHTS RESERVED
//
//       The entire notice above must be reproduced on all authorized
//       copies.
//
//=========================================================================

#ifndef __VPU_DRV_H__
#define __VPU_DRV_H__

#include <linux/types.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include "vpuconfig.h"
#include "vmm_type.h"
#include "bm_cdma.h"
#include "bm_io.h"

#define VDI_IOCTL_MAGIC  'V'
#define VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY        _IO(VDI_IOCTL_MAGIC, 0)
#define VDI_IOCTL_FREE_PHYSICALMEMORY             _IO(VDI_IOCTL_MAGIC, 1)
#define VDI_IOCTL_WAIT_INTERRUPT                  _IO(VDI_IOCTL_MAGIC, 2)
#define VDI_IOCTL_SET_CLOCK_GATE                  _IO(VDI_IOCTL_MAGIC, 3)
#define VDI_IOCTL_RESET                           _IO(VDI_IOCTL_MAGIC, 4)
#define VDI_IOCTL_GET_INSTANCE_POOL               _IO(VDI_IOCTL_MAGIC, 5)
#define VDI_IOCTL_GET_COMMON_MEMORY               _IO(VDI_IOCTL_MAGIC, 6)
#define VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO  _IO(VDI_IOCTL_MAGIC, 8)
#define VDI_IOCTL_OPEN_INSTANCE                   _IO(VDI_IOCTL_MAGIC, 9)
#define VDI_IOCTL_CLOSE_INSTANCE                  _IO(VDI_IOCTL_MAGIC, 10)
#define VDI_IOCTL_GET_INSTANCE_NUM                _IO(VDI_IOCTL_MAGIC, 11)
#define VDI_IOCTL_GET_REGISTER_INFO               _IO(VDI_IOCTL_MAGIC, 12)
#define VDI_IOCTL_GET_FREE_MEM_SIZE               _IO(VDI_IOCTL_MAGIC, 13)
#define VDI_IOCTL_GET_FIRMWARE_STATUS             _IO(VDI_IOCTL_MAGIC, 14)

#ifndef SOC_MODE
#define VDI_IOCTL_WRITE_VMEM                     _IO(VDI_IOCTL_MAGIC, 20)
#define VDI_IOCTL_READ_VMEM                      _IO(VDI_IOCTL_MAGIC, 21)
#endif

#define VDI_IOCTL_SYSCXT_SET_STATUS              _IO(VDI_IOCTL_MAGIC, 26)
#define VDI_IOCTL_SYSCXT_GET_STATUS              _IO(VDI_IOCTL_MAGIC, 27)
#define VDI_IOCTL_SYSCXT_CHK_STATUS              _IO(VDI_IOCTL_MAGIC, 28)
#define VDI_IOCTL_SYSCXT_SET_EN                  _IO(VDI_IOCTL_MAGIC, 29)

typedef struct vpudrv_syscxt_info_s {
	unsigned int core_idx;
	unsigned int inst_idx;
	unsigned int core_status;
	unsigned int pos;
	unsigned int fun_en;

	unsigned int is_all_block;
	int is_sleep;
} vpudrv_syscxt_info_t;


typedef struct vpudrv_buffer_t {
	unsigned int  size;
	unsigned long phys_addr;
	unsigned long base;         /* kernel logical address in use kernel */
	unsigned long virt_addr;    /* virtual user space address */

	unsigned int  core_idx;
	unsigned int  reserved;
} vpudrv_buffer_t;

typedef struct vpu_bit_firmware_info_t {
	unsigned int size;          /* size of this structure*/
	unsigned int core_idx;
	unsigned long reg_base_offset;
	unsigned short bit_code[512];
} vpu_bit_firmware_info_t;

typedef struct vpudrv_inst_info_t {
	unsigned int core_idx;
	unsigned int inst_idx;
	int inst_open_count;       /* for output only*/
} vpudrv_inst_info_t;

typedef struct vpudrv_intr_info_t {
	unsigned int timeout;
	int            intr_reason;
#ifdef SUPPORT_MULTI_INST_INTR
	int            intr_inst_index;
#endif
	int         core_idx;
} vpudrv_intr_info_t;

/* To track the allocated memory buffer */
typedef struct vpudrv_buffer_pool_t {
	struct list_head list;
	struct vpudrv_buffer_t vb;
	struct file *filp;
} vpudrv_buffer_pool_t;

/* To track the instance index and buffer in instance pool */
typedef struct vpudrv_instanace_list_t {
	struct list_head list;
	unsigned long inst_idx;
	unsigned long core_idx;
	struct file *filp;
} vpudrv_instanace_list_t;

typedef struct vpudrv_instance_pool_t {
	unsigned char   codecInstPool[MAX_NUM_INSTANCE_VPU][MAX_INST_HANDLE_SIZE_VPU];
	vpudrv_buffer_t vpu_common_buffer;
	int             vpu_instance_num;
	int             instance_pool_inited;
	void            *pendingInst;
	int             pendingInstIdxPlus1;
} vpudrv_instance_pool_t;

/* end customer definition */

typedef struct vpudrv_regrw_info_t {
	unsigned int size;
	unsigned long reg_base;
	unsigned int value[4];
	unsigned int mask[4];
} vpudrv_regrw_info_t;

#ifndef SOC_MODE
typedef struct vpu_video_mem_op_t {
	unsigned int size; /* size of this structure */
	unsigned long src;
	unsigned long dst;
} vpu_video_mem_op_t;

typedef struct _vpu_reset_ctrl_t {
	struct reset_control *axi2_rst;
	struct reset_control *apb_video_rst;
	struct reset_control *video_axi_rst;
} vpu_reset_ctrl;

enum {
	SYSCXT_STATUS_WORKDING      = 0,
	SYSCXT_STATUS_EXCEPTION,
};

typedef struct vpu_crst_context_s {
	unsigned int instcall[MAX_NUM_INSTANCE_VPU];
	unsigned int status;
	unsigned int disable;

	unsigned int reset;
	unsigned int count;
} vpu_crst_context_t;

typedef struct vpu_core_idx_context_t {
	struct list_head list;
	int core_idx;
	struct file *filp;
} vpu_core_idx_context;

typedef struct vpu_drv_context {
	struct fasync_struct *async_queue;
	struct mutex s_vpu_lock;
	struct semaphore s_vpu_sem;
	struct list_head s_vbp_head;
	struct list_head s_inst_list_head;
	struct proc_dir_entry *entry[64];
	u32 open_count;                     /*!<< device reference count. Not instance count */
	u32 max_num_vpu_core;
	u32 max_num_instance;
	u32 *s_vpu_dump_flag;
	u32 *s_vpu_irq;
	u32 *s_vpu_reg_phy_base;

	/* move some control variables here to support multi board */
	video_mm_t s_vmemboda;
	video_mm_t s_vmemwave;
	video_mm_t s_vmem;
	vpudrv_buffer_t s_video_memory;

	/* end customer definition */
	vpudrv_buffer_t instance_pool[MAX_NUM_VPU_CORE];

	vpudrv_buffer_t s_common_memory[MAX_NUM_VPU_CORE];
	vpudrv_buffer_t s_vpu_register[MAX_NUM_VPU_CORE];
	vpu_reset_ctrl vpu_rst_ctrl;
	int s_vpu_open_ref_count[MAX_NUM_VPU_CORE];
	vpu_bit_firmware_info_t s_bit_firmware_info[MAX_NUM_VPU_CORE];

	// must allocate them
	u32 s_init_flag[MAX_NUM_VPU_CORE];
	u32 s_vpu_reg_store[MAX_NUM_VPU_CORE][64];

	unsigned long     interrupt_reason[MAX_NUM_VPU_CORE][MAX_NUM_INSTANCE_VPU];
	int               interrupt_flag[MAX_NUM_VPU_CORE][MAX_NUM_INSTANCE_VPU];
	wait_queue_head_t interrupt_wait_q[MAX_NUM_VPU_CORE][MAX_NUM_INSTANCE_VPU];
	struct kfifo      interrupt_pending_q[MAX_NUM_VPU_CORE][MAX_NUM_INSTANCE_VPU];
	spinlock_t        s_kfifo_lock[MAX_NUM_VPU_CORE][MAX_NUM_INSTANCE_VPU];

	vpu_crst_context_t crst_cxt[MAX_NUM_VPU_CORE];
	struct list_head s_core_idx_head;
} vpu_drv_context_t;

//#define DUMP_FLAG_MEM_SIZE_VPU (MAX_NUM_VPU_CORE*MAX_NUM_INSTANCE_VPU*sizeof(unsigned int)*5 + MAX_NUM_VPU_CORE*2*sizeof(unsigned int)_)

struct bm_device_info;
int bm_vpu_init(struct bm_device_info *bmdi);
void bm_vpu_exit(struct bm_device_info *bmdi);

void bm_vpu_request_irq(struct bm_device_info *bmdi);
void bm_vpu_free_irq(struct bm_device_info *bmdi);

ssize_t bm_vpu_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos);
ssize_t bm_vpu_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos);
long bm_vpu_ioctl(struct file *filp, u_int cmd, u_long arg);
int bm_vpu_mmap(struct file *fp, struct vm_area_struct *vm);
int bm_vpu_fasync(int fd, struct file *filp, int mode);
int bm_vpu_release(struct inode *inode, struct file *filp);
int bm_vpu_open(struct inode *inode, struct file *filp);
extern const struct file_operations bmdrv_vpu_file_ops;
#endif

extern int bmdev_memcpy_d2s(struct bm_device_info *bmdi, struct file *file,
		void __user *dst, u64 src, u32 size,
		bool intr, bm_cdma_iommu_mode cdma_iommu_mode);
extern int bmdev_memcpy_s2d(struct bm_device_info *bmdi, struct file *file,
		uint64_t dst, void __user *src, u32 size,
		bool intr, bm_cdma_iommu_mode cdma_iommu_mode);

extern void bm_get_bar_base(struct bm_bar_info *pbar_info, u32 address, u64 *base);

extern void bm_get_bar_offset(struct bm_bar_info *pbar_info, u32 address,
		void __iomem **bar_vaddr, u32 *offset);
#endif
