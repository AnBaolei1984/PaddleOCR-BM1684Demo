#ifndef _BM_API_H_
#define _BM_API_H_
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/hashtable.h>
#include <linux/kfifo.h>
#include <linux/list.h>
#include "bm_uapi.h"

#define DEVICE_SYNC_MARKER 0xffff

#define API_FLAG_FINISH  4
#define API_FLAG_WAIT    2
#define API_FLAG_DONE    1

struct bm_device_info;
struct bm_thread_info;

struct bm_api_info {
	struct completion msg_done;
	u64 msgirq_num;
/* sw_rp is used to manage share mameory */
	u32 sw_rp;

	u32 device_sync_last;
	u32 device_sync_cpl;
	struct completion dev_sync_done;

	struct mutex api_mutex;
	struct kfifo api_fifo;
	struct list_head api_list;
	struct mutex api_fifo_mutex;

	int (*bm_api_init)(struct bm_device_info *, u32 channel);
	void (*bm_api_deinit)(struct bm_device_info *, u32 channel);
};

struct api_fifo_entry {
	struct bm_thread_info *thd_info;
	struct bm_handle_info *h_info;
	u32 thd_api_seq;
	u32 dev_api_seq;
	bm_api_id_t api_id;
	u64 sent_time_us;
	u64 global_api_seq;
	u32 api_done_flag;
	struct completion api_done;
	u64 api_data;
};

struct api_list_entry {
	struct list_head api_list_node;
	struct api_fifo_entry api_entry;
};

typedef struct bm_kapi_header {
	bm_api_id_t api_id;
	u32 api_size;  /* size of payload, not including header */
	u64 api_handle;
	u32 api_seq;
	u32 duration;
	u32 result;
}__attribute__((packed)) bm_kapi_header_t ;

typedef struct bm_kapi_opt_header {
	u64 global_api_seq;
	u64 api_data;
} bm_kapi_opt_header_t;

#define API_ENTRY_SIZE sizeof(struct api_fifo_entry)

int bmdrv_api_init(struct bm_device_info *bmdi, u32 channel);
void bmdrv_api_deinit(struct bm_device_info *bmdi, u32 channel);
int bmdrv_send_api(struct bm_device_info *bmdi, struct file *file, unsigned long arg, bool flag);
int bmdrv_query_api(struct bm_device_info *bmdi, struct file *file, unsigned long arg);
int bmdrv_thread_sync_api(struct bm_device_info *bmdi, struct file *file);
int bmdrv_handle_sync_api(struct bm_device_info *bmdi, struct file *file);
int bmdrv_device_sync_api(struct bm_device_info *bmdi);
#endif
