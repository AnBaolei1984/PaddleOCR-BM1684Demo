#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include "bm_common.h"
#include "bm_thread.h"
#include "bm_trace.h"
#include "bm_uapi.h"
#include "bm1684_perf.h"

int bmdrv_init_trace_pool(struct bm_device_info *bmdi)
{
	int ret;
	struct bm_trace_info *trace_info = &bmdi->trace_info;

	trace_info->trace_cache = NULL;
	trace_info->trace_mempool = NULL;

	trace_info->trace_cache = kmem_cache_create("bm_trace_cache",
			sizeof(struct bm_trace_item), 0, SLAB_HWCACHE_ALIGN, NULL);
	if (trace_info->trace_cache)
		trace_info->trace_mempool = mempool_create_slab_pool(1000,
				trace_info->trace_cache);
	else
		return -1;

	if (trace_info->trace_mempool) {
		ret = 0;
	} else {
		ret = -1;
		kmem_cache_destroy(trace_info->trace_cache);
	}
	return ret;
}

void bmdrv_destroy_trace_pool(struct bm_device_info *bmdi)
{
	struct bm_trace_info *trace_info = &bmdi->trace_info;

	if (trace_info->trace_mempool)
		mempool_destroy(trace_info->trace_mempool);
	trace_info->trace_mempool = NULL;
	if (trace_info->trace_cache)
		kmem_cache_destroy(trace_info->trace_cache);
	trace_info->trace_cache = NULL;
}

int bmdev_trace_enable(struct bm_device_info *bmdi, struct file *file)
{
	struct bm_thread_info *ti;
	pid_t api_pid;
	struct bm_handle_info *h_info;

	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
		pr_err("bmdrv: file list is not found!\n");
		return -EINVAL;
	}

	api_pid = current->pid;
	ti = bmdrv_find_thread_info(h_info, api_pid);

	if (!ti) {
		return -EFAULT;
	} else {
		mutex_lock(&ti->trace_mutex);
		ti->trace_enable = 1;
		mutex_unlock(&ti->trace_mutex);
		return 0;
	}
}

int bmdev_trace_disable(struct bm_device_info *bmdi, struct file *file)
{
	struct bm_thread_info *ti;
	pid_t api_pid;
	struct bm_handle_info *h_info;

	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
		pr_err("bmdrv: file list is not found!\n");
		return -EINVAL;
	}

	api_pid = current->pid;
	ti = bmdrv_find_thread_info(h_info, api_pid);

	if (!ti) {
		return -EFAULT;
	} else {
		mutex_lock(&ti->trace_mutex);
		ti->trace_enable = 0;
		mutex_unlock(&ti->trace_mutex);
		return 0;
	}
}

int bmdev_traceitem_number(struct bm_device_info *bmdi, struct file *file, unsigned long arg)
{
	struct bm_thread_info *ti;
	pid_t api_pid;
	int ret = 0;
	struct bm_handle_info *h_info;

	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
		pr_err("bmdrv: file list is not found!\n");
		return -EINVAL;
	}

	api_pid = current->pid;
	ti = bmdrv_find_thread_info(h_info, api_pid);

	if (!ti) {
		return -EFAULT;
	} else {
		mutex_lock(&ti->trace_mutex);
		ret = put_user(ti->trace_item_num, (u64 __user *)arg);
		mutex_unlock(&ti->trace_mutex);
		return ret;
	}
}

int bmdev_trace_dump_one(struct bm_device_info *bmdi, struct file *file, unsigned long arg)
{
	struct bm_thread_info *ti;
	pid_t api_pid;
	int ret = -1;
	struct list_head *oldest = NULL;
	struct bm_trace_item *ptitem = NULL;
	struct bm_handle_info *h_info;

	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
		pr_err("bmdrv: file list is not found!\n");
		return -EINVAL;
	}

	api_pid = current->pid;
	ti = bmdrv_find_thread_info(h_info, api_pid);

	if (!ti)
		return -EFAULT;

	mutex_lock(&ti->trace_mutex);
	if (!list_empty(&ti->trace_list)) {
		oldest = ti->trace_list.next;
		list_del(oldest);
		ptitem = container_of(oldest, struct bm_trace_item, node);
		ret = copy_to_user((unsigned long __user *)arg, &ptitem->payload,
				sizeof(struct bm_trace_item_data));
		mempool_free(ptitem, bmdi->trace_info.trace_mempool);
		ti->trace_item_num--;
	}
	mutex_unlock(&ti->trace_mutex);
	return ret;
}

int bmdev_trace_dump_all(struct bm_device_info *bmdi, struct file *file, unsigned long arg)
{
	struct bm_thread_info *ti;
	pid_t api_pid;
	int ret = -1;
	int i = 0;
	struct list_head *oldest = NULL;
	struct bm_trace_item *ptitem = NULL;
	struct bm_handle_info *h_info;

	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
		pr_err("bmdrv: file list is not found!\n");
		return -EINVAL;
	}

	api_pid = current->pid;
	ti = bmdrv_find_thread_info(h_info, api_pid);

	if (!ti)
		return -EFAULT;

	mutex_lock(&ti->trace_mutex);
	while (!list_empty(&ti->trace_list)) {
		oldest = ti->trace_list.next;
		list_del(oldest);
		ptitem = container_of(oldest, struct bm_trace_item, node);
		ret = copy_to_user((unsigned long __user *)(arg + i*sizeof(struct bm_trace_item_data)),
				&ptitem->payload, sizeof(struct bm_trace_item_data));
		mempool_free(ptitem, bmdi->trace_info.trace_mempool);
		i++;
	}
	mutex_unlock(&ti->trace_mutex);
	return ret;
}

int bmdev_enable_perf_monitor(struct bm_device_info *bmdi, struct bm_perf_monitor *perf_monitor)
{
	if (bmdi->cinfo.chip_id == 0x1684) {
		if (perf_monitor->monitor_id == PERF_MONITOR_TPU) {
			bm1684_enable_tpu_perf_monitor(bmdi, perf_monitor);
		} else if (perf_monitor->monitor_id == PERF_MONITOR_GDMA) {
			bm1684_enable_gdma_perf_monitor(bmdi, perf_monitor);
		} else {
			pr_info("enable perf monitor bad perf monitor id 0x%x\n",
					perf_monitor->monitor_id);
			return -1;
		}
	} else {
		pr_info("bmdev_enable_perf_monitor chip id = 0x%x not support\n",
				bmdi->cinfo.chip_id);
		return -1;
	}
	return 0;
}

int bmdev_disable_perf_monitor(struct bm_device_info *bmdi, struct bm_perf_monitor *perf_monitor)
{
	if (bmdi->cinfo.chip_id == 0x1684) {
		if (perf_monitor->monitor_id == PERF_MONITOR_TPU) {
			bm1684_disable_tpu_perf_monitor(bmdi);
		} else if (perf_monitor->monitor_id == PERF_MONITOR_GDMA) {
			bm1684_disable_gdma_perf_monitor(bmdi);
		} else {
			pr_info("disable perf monitor bad perf monitor id 0x%x\n",
					perf_monitor->monitor_id);
			return -1;
		}
	} else {
		pr_info("bmdev_disable_perf_monitor chip id = 0x%x not support\n",
				bmdi->cinfo.chip_id);
		return -1;
	}
	return 0;
}
