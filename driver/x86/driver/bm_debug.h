#ifndef __BM_DEBUG_H__
#define __BM_DEBUG_H__
#include <linux/kthread.h>

int bmdrv_proc_init(void);
void bmdrv_proc_deinit(void);
int bmdrv_proc_file_init(struct bm_device_info *bmdi);
void bmdrv_proc_file_deinit(struct bm_device_info *bmdi);
int bm_monitor_thread_init(struct bm_device_info *bmdi);
int bm_monitor_thread_deinit(struct bm_device_info *bmdi);

struct bm_arm9fw_log_mem {
	void *host_vaddr;
	u64 host_paddr;
	u32 host_size;
	u64 device_paddr;
	u32 device_size;
};

struct bm_monitor_thread_info {
	struct bm_arm9fw_log_mem log_mem;
	struct task_struct *monitor_task;
};
#endif
