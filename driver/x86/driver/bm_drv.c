#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/uaccess.h>
#include <linux/mempool.h>
#include "bm_common.h"
#include "bm_msgfifo.h"
#include "bm_drv.h"
#include "bm_thread.h"
#include "bm_debug.h"
#include <linux/version.h>

/* be carefull with global variables, keep multi-card support in mind */
dev_t bm_devno_base;
dev_t bm_ctl_devno_base;

static void bmdrv_print_cardinfo(struct chip_info *cinfo)
{
#ifndef SOC_MODE
	u16 pcie_version;
#endif
	dev_info(cinfo->device, "bar0 0x%llx size 0x%llx, vaddr = 0x%p\n",
			cinfo->bar_info.bar0_start, cinfo->bar_info.bar0_len,
			cinfo->bar_info.bar0_vaddr);
	dev_info(cinfo->device, "bar1 0x%llx size 0x%llx, vaddr = 0x%p\n",
			cinfo->bar_info.bar1_start, cinfo->bar_info.bar1_len,
			cinfo->bar_info.bar1_vaddr);
	dev_info(cinfo->device, "bar2 0x%llx size 0x%llx, vaddr = 0x%p\n",
			cinfo->bar_info.bar2_start, cinfo->bar_info.bar2_len,
			cinfo->bar_info.bar2_vaddr);
#ifndef SOC_MODE
	dev_info(cinfo->device, "bar1 part0 offset 0x%llx size 0x%llx, dev_addr = 0x%llx\n",
			cinfo->bar_info.bar1_part0_offset, cinfo->bar_info.bar1_part0_len,
			cinfo->bar_info.bar1_part0_dev_start);
	dev_info(cinfo->device, "bar1 part1 offset 0x%llx size 0x%llx, dev_addr = 0x%llx\n",
			cinfo->bar_info.bar1_part1_offset, cinfo->bar_info.bar1_part1_len,
			cinfo->bar_info.bar1_part1_dev_start);
	dev_info(cinfo->device, "bar1 part2 offset 0x%llx size 0x%llx, dev_addr = 0x%llx\n",
			cinfo->bar_info.bar1_part2_offset, cinfo->bar_info.bar1_part2_len,
			cinfo->bar_info.bar1_part2_dev_start);

	pcie_version = pcie_caps_reg(cinfo->pcidev) & PCI_EXP_FLAGS_VERS;
	dev_info(cinfo->device, "PCIe version 0x%x\n", pcie_version);
	dev_info(cinfo->device, "board version 0x%x\n", cinfo->board_version);
#endif
}

void bmdrv_post_api_process(struct bm_device_info *bmdi,
		struct api_fifo_entry api_entry, u32 channel)
{
	struct bm_thread_info *ti = api_entry.thd_info;
	struct bm_trace_item *ptitem = NULL;
	u32 next_rp = 0;
	u32 api_id = 0;
	u32 api_size = 0;
	u32 api_duration = 0;
	u32 api_result = 0;

	next_rp = bmdev_msgfifo_add_pointer(bmdi, bmdi->api_info[channel].sw_rp, offsetof(bm_kapi_header_t, api_id) / sizeof(u32));
	api_id = shmem_reg_read_enh(bmdi, next_rp, channel);
	next_rp = bmdev_msgfifo_add_pointer(bmdi, bmdi->api_info[channel].sw_rp, offsetof(bm_kapi_header_t, api_size) / sizeof(u32));
	api_size = shmem_reg_read_enh(bmdi, next_rp, channel);
	next_rp = bmdev_msgfifo_add_pointer(bmdi, bmdi->api_info[channel].sw_rp, offsetof(bm_kapi_header_t, duration) / sizeof(u32));
	api_duration = shmem_reg_read_enh(bmdi, next_rp, channel);
	next_rp = bmdev_msgfifo_add_pointer(bmdi, bmdi->api_info[channel].sw_rp, offsetof(bm_kapi_header_t, result) / sizeof(u32));
	api_result = shmem_reg_read_enh(bmdi, next_rp, channel);
#ifdef PCIE_MODE_ENABLE_CPU
	if (channel == BM_MSGFIFO_CHANNEL_CPU)
		next_rp = bmdev_msgfifo_add_pointer(bmdi, bmdi->api_info[channel].sw_rp, (sizeof(bm_kapi_header_t) + sizeof(bm_kapi_opt_header_t)) / sizeof(u32) + api_size);
	else
#endif
		next_rp = bmdev_msgfifo_add_pointer(bmdi, bmdi->api_info[channel].sw_rp, sizeof(bm_kapi_header_t) / sizeof(u32) + api_size);
	bmdi->api_info[channel].sw_rp = next_rp;
	//todo, print a log for tmp;
	if (api_result != 0)
		pr_err("bm-sophon%d api process fail, error id is %d", bmdi->dev_index, api_id);

	if (ti) {
		ti->profile.tpu_process_time += api_duration;
		ti->profile.completed_api_counter++;
		bmdi->profile.tpu_process_time += api_duration;
		bmdi->profile.completed_api_counter++;

		mutex_lock(&ti->trace_mutex);
		if (ti->trace_enable) {
			ptitem = (struct bm_trace_item *)mempool_alloc(bmdi->trace_info.trace_mempool, GFP_KERNEL);
			ptitem->payload.trace_type = 1;
			ptitem->payload.api_id = api_entry.api_id;
			ptitem->payload.sent_time = api_entry.sent_time_us;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0)
			ptitem->payload.end_time = ktime_get_boottime_ns() / 1000;
#else
			ptitem->payload.end_time = ktime_get_boot_ns() / 1000;
#endif
			ptitem->payload.start_time = ptitem->payload.end_time - api_duration * 4166;
			INIT_LIST_HEAD(&ptitem->node);
			list_add_tail(&ptitem->node, &ti->trace_list);
			ti->trace_item_num++;
		}
		mutex_unlock(&ti->trace_mutex);
	}
}

static char *bmdrv_class_devnode(struct device *dev, umode_t *mode)
{
	if (!mode || !dev)
		return NULL;
	*mode = 0777;
	return NULL;
}

static void bmdrv_sw_register_init(struct bm_device_info *bmdi)
{
	int channel = 0;

	for (channel = 0; channel < BM_MSGFIFO_CHANNEL_NUM; channel++) {
		bmdi->api_info[channel].bm_api_init = bmdrv_api_init;
		bmdi->api_info[channel].bm_api_deinit = bmdrv_api_deinit;
	}
	bmdi->c_attr.bm_card_attr_init = bmdrv_card_attr_init;
	bmdi->memcpy_info.bm_memcpy_init = bmdrv_memcpy_init;
	bmdi->memcpy_info.bm_memcpy_deinit = bmdrv_memcpy_deinit;
	bmdi->trace_info.bm_trace_init = bmdrv_init_trace_pool;
	bmdi->trace_info.bm_trace_deinit = bmdrv_destroy_trace_pool;
	bmdi->gmem_info.bm_gmem_init = bmdrv_gmem_init;
	bmdi->gmem_info.bm_gmem_deinit = bmdrv_gmem_deinit;
}

int bmdrv_software_init(struct bm_device_info *bmdi)
{
	int ret = 0;
	u32 channel = 0;
	struct chip_info *cinfo = &bmdi->cinfo;

	INIT_LIST_HEAD(&bmdi->handle_list);
	bmdrv_sw_register_init(bmdi);
	mutex_init(&bmdi->clk_reset_mutex);
	mutex_init(&bmdi->device_mutex);
#ifndef SOC_MODE
	mutex_init(&bmdi->spacc_mutex);
#endif

	if (bmdi->gmem_info.bm_gmem_init &&
		bmdi->gmem_info.bm_gmem_init(bmdi))
		return -EFAULT;

	for (channel = 0; channel < BM_MSGFIFO_CHANNEL_NUM; channel++) {
		if (bmdi->api_info[channel].bm_api_init &&
			bmdi->api_info[channel].bm_api_init(bmdi, channel))
			return -EFAULT;
	}

	if (bmdi->c_attr.bm_card_attr_init &&
		bmdi->c_attr.bm_card_attr_init(bmdi))
		return -EFAULT;

	if (bmdi->memcpy_info.bm_memcpy_init &&
		bmdi->memcpy_info.bm_memcpy_init(bmdi))
		return -EFAULT;

	if (bmdi->trace_info.bm_trace_init &&
		bmdi->trace_info.bm_trace_init(bmdi))
		return -EFAULT;

	bmdi->parent = cinfo->device;

	bmdrv_print_cardinfo(cinfo);

	bmdi->enable_dyn_freq = 1;

	return ret;
}

void bmdrv_software_deinit(struct bm_device_info *bmdi)
{
	u32 channel = 0;

	for (channel = 0; channel < BM_MSGFIFO_CHANNEL_NUM; channel++) {
		if (bmdi->api_info[channel].bm_api_deinit)
			bmdi->api_info[channel].bm_api_deinit(bmdi, channel);
	}
	if (bmdi->memcpy_info.bm_memcpy_deinit)
		bmdi->memcpy_info.bm_memcpy_deinit(bmdi);

	if (bmdi->gmem_info.bm_gmem_deinit)
		bmdi->gmem_info.bm_gmem_deinit(bmdi);

	if (bmdi->trace_info.bm_trace_deinit)
		bmdi->trace_info.bm_trace_deinit(bmdi);
}

struct class bmdev_class = {
	.name		= BM_CLASS_NAME,
	.owner		= THIS_MODULE,
	.devnode = bmdrv_class_devnode,
};

int bmdrv_class_create(void)
{
	int ret;

	ret = class_register(&bmdev_class);
	if (ret < 0) {
		pr_err("bmdrv: create class error\n");
		return ret;
	}

	ret = alloc_chrdev_region(&bm_devno_base, 0, MAX_CARD_NUMBER, BM_CDEV_NAME);
	if (ret < 0) {
		pr_err("bmdrv: register chrdev error\n");
		return ret;
	}
	ret = alloc_chrdev_region(&bm_ctl_devno_base, 0, 1, BMDEV_CTL_NAME);
	if (ret < 0) {
		pr_err("bmdrv: register ctl chrdev error\n");
		return ret;
	}
	return 0;
}

struct class *bmdrv_class_get(void)
{
	return &bmdev_class;
}

int bmdrv_class_destroy(void)
{
	unregister_chrdev_region(bm_devno_base, MAX_CARD_NUMBER);
	unregister_chrdev_region(bm_ctl_devno_base, 1);
	class_unregister(&bmdev_class);
	return 0;
}
