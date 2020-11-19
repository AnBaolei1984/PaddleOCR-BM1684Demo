#include <linux/cdev.h>
#include <linux/device.h>
#include "bm_common.h"
#include "bm_thread.h"

struct bm_thread_info *bmdrv_find_thread_info(struct bm_handle_info *h_info, pid_t pid)
{
	struct bm_thread_info *thd_info = NULL;
	int pid_found = 0;

	if (h_info) {
		hash_for_each_possible_rcu(h_info->api_htable, thd_info, node, pid) {
			if (thd_info->user_pid == pid) {
				pid_found = 1;
				break;
			}
		}
	}
	if (pid_found)
		return thd_info;
	else
		return NULL;
}

struct bm_thread_info *bmdrv_create_thread_info(struct bm_handle_info *h_info, pid_t pid)
{
	struct bm_thread_info *thd_info;

	thd_info = kmalloc(sizeof(struct bm_thread_info), GFP_KERNEL);
	if (!thd_info)
		return thd_info;
	thd_info->user_pid = pid;

	init_completion(&thd_info->msg_done);
	thd_info->last_api_seq = 0;
	thd_info->cpl_api_seq = 0;

	thd_info->profile.cdma_in_time = 0ULL;
	thd_info->profile.cdma_in_counter = 0ULL;
	thd_info->profile.cdma_out_time = 0ULL;
	thd_info->profile.cdma_out_counter = 0ULL;
	thd_info->profile.tpu_process_time = 0ULL;
	thd_info->profile.sent_api_counter = 0ULL;
	thd_info->profile.completed_api_counter = 0ULL;

	thd_info->trace_enable = 0;
	thd_info->trace_item_num = 0ULL;
	mutex_init(&thd_info->trace_mutex);
	INIT_LIST_HEAD(&thd_info->trace_list);

	hash_add_rcu(h_info->api_htable, &thd_info->node, pid);

	return thd_info;
}

void bmdrv_delete_thread_info(struct bm_handle_info *h_info)
{
	struct bm_thread_info *thd_info;
	int bucket;
	struct hlist_node *tmp;

	hash_for_each_safe(h_info->api_htable, bucket, tmp, thd_info, node) {
		hash_del(&thd_info->node);
		kfree(thd_info);
	}
}
