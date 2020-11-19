#ifndef _BM_THREAD_H_
#define _BM_THREAD_H_
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/hashtable.h>

#include "bm_uapi.h"

struct bm_thread_info {
	struct hlist_node node;
	pid_t user_pid;

	/* api record infomation */
	struct completion msg_done;
	u64 last_api_seq;
	u64 cpl_api_seq;

	/* profile trace information */
	bm_profile_t profile;
	struct mutex trace_mutex;
	struct list_head trace_list;
	bool trace_enable;
	u64 trace_item_num;
};

struct bm_handle_info;
struct bm_thread_info *bmdrv_find_thread_info(struct bm_handle_info *h_info, pid_t pid);
struct bm_thread_info *bmdrv_create_thread_info(struct bm_handle_info *h_info, pid_t pid);
void bmdrv_delete_thread_info(struct bm_handle_info *h_info);

#endif
