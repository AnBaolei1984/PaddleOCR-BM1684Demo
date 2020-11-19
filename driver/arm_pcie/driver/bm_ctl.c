#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/sizes.h>
#include <linux/uaccess.h>
#include "bm_common.h"
#include "bm_pcie.h"
#include "bm1684_clkrst.h"
#include "bm_ctl.h"
#include "bm_drv.h"
#include "bm_wdt.h"
#include "bm1684_card.h"

#ifndef SOC_MODE
#include "bm_card.h"
#endif

struct bm_ctrl_info *bmci;
int dev_count;

int bmdrv_init_bmci(struct chip_info *cinfo)
{
	int rc;

	bmci = kzalloc(sizeof(*bmci), GFP_KERNEL);
	if (!bmci)
		return -ENOMEM;
	sprintf(bmci->dev_ctl_name, "%s", BMDEV_CTL_NAME);
	INIT_LIST_HEAD(&bmci->bm_dev_list);
	bmci->dev_count = 0;
	rc = bmdev_ctl_register_device(bmci);
	return rc;
}

int bmdrv_remove_bmci(void)
{
	bmdev_ctl_unregister_device(bmci);
	kfree(bmci);
	return 0;
}

int bmdrv_ctrl_add_dev(struct bm_ctrl_info *bmci, struct bm_device_info *bmdi)
{
	struct bm_dev_list *bmdev_list;

	dev_count++;
	/* record a reference in the bmdev list in bm_ctl */
	bmci->dev_count = dev_count;
	bmdev_list = kmalloc(sizeof(struct bm_dev_list), GFP_KERNEL);
	if (!bmdev_list)
		return -ENOMEM;
	bmdev_list->bmdi = bmdi;
	list_add(&bmdev_list->list, &bmci->bm_dev_list);
	return 0;
}

int bmdrv_ctrl_del_dev(struct bm_ctrl_info *bmci, struct bm_device_info *bmdi)
{
	struct bm_dev_list *bmdev_list, *tmp;

	dev_count--;
	bmci->dev_count = dev_count;
	list_for_each_entry_safe(bmdev_list, tmp, &bmci->bm_dev_list, list) {
		if (bmdev_list->bmdi == bmdi) {
			list_del(&bmdev_list->list);
			kfree(bmdev_list);
		}
	}
	return 0;
}

static int bmctl_get_bus_id(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	return bmdi->misc_info.domain_bdf;
#else
	return 0;
#endif
}

struct bm_device_info *bmctl_get_bmdi(struct bm_ctrl_info *bmci, int dev_id)
{
	struct bm_dev_list *pos, *tmp;
	struct bm_device_info *bmdi;

	if (dev_id >= bmci->dev_count) {
		pr_err("bmdrv: invalid device id %d!\n", dev_id);
		return NULL;
	}
	list_for_each_entry_safe(pos, tmp, &bmci->bm_dev_list, list) {
		bmdi = pos->bmdi;
		if (bmdi->dev_index == dev_id)
			return bmdi;
	}
	pr_err("bmdrv: bmdi not found!\n");
	return NULL;
}

struct bm_device_info *bmctl_get_card_bmdi(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	int dev_id = 0;

	if ((bmdi->misc_info.domain_bdf & 0x7) == 0x0)
		return bmdi;

	if ((bmdi->misc_info.domain_bdf & 0x7) == 0x1
		|| (bmdi->misc_info.domain_bdf & 0x7) == 0x2) {
		if ((bmdi->misc_info.domain_bdf & 0x7) == 0x1)
			dev_id = bmdi->dev_index - 1;

		if ((bmdi->misc_info.domain_bdf & 0x7) == 0x2)
			dev_id = bmdi->dev_index - 2;

		if (dev_id < 0 || dev_id >= 0xff) {
			pr_err("bmctl_get_card_bmdi fail, dev_id = 0x%x\n", dev_id);
			return NULL;
		}

		return bmctl_get_bmdi(bmci, dev_id);
	}

	return NULL;
#else
	return bmdi;
#endif
}

static int bmctl_get_smi_attr(struct bm_ctrl_info *bmci, struct bm_smi_attr *pattr)
{
	struct bm_device_info *bmdi;
	struct chip_info *cinfo;
	struct bm_chip_attr *c_attr;

	bmdi = bmctl_get_bmdi(bmci, pattr->dev_id);
	if (!bmdi)
		return -1;

	cinfo = &bmdi->cinfo;
	c_attr = &bmdi->c_attr;

	pattr->chip_mode = bmdi->misc_info.pcie_soc_mode;
	if (pattr->chip_mode == 0)
		pattr->domain_bdf = bmctl_get_bus_id(bmdi);
	else
		pattr->domain_bdf = ATTR_NOTSUPPORTED_VALUE;
	pattr->chip_id = bmdi->cinfo.chip_id;
	pattr->status = bmdi->status;

	pattr->mem_total = (int)(bmdrv_gmem_total_size(bmdi)/1024/1024);
	pattr->mem_used = pattr->mem_total - (int)(bmdrv_gmem_avail_size(bmdi)/1024/1024);

	pattr->tpu_util = c_attr->bm_get_npu_util(bmdi);

	/* use attribute lock */
	mutex_lock(&c_attr->attr_mutex);
	if (c_attr->bm_get_chip_temp != NULL)
		c_attr->bm_get_chip_temp(bmdi, &pattr->chip_temp);
	else
		pattr->chip_temp = ATTR_NOTSUPPORTED_VALUE;
	if (c_attr->bm_get_board_temp != NULL)
		c_attr->bm_get_board_temp(bmdi, &pattr->board_temp);
	else
		pattr->board_temp = ATTR_NOTSUPPORTED_VALUE;

	if (c_attr->bm_get_tpu_power != NULL) {
#ifndef SOC_MODE
		bm_read_vdd_tpu_voltage(bmdi, &pattr->vdd_tpu_volt);
		if ((pattr->vdd_tpu_volt > 0) && (pattr->vdd_tpu_volt < 0xffff))
			c_attr->last_valid_tpu_volt = pattr->vdd_tpu_volt;
		else
			pattr->vdd_tpu_volt = c_attr->last_valid_tpu_volt;

		bm_read_vdd_tpu_current(bmdi, &pattr->vdd_tpu_curr);
		if ((pattr->vdd_tpu_curr > 0) && (pattr->vdd_tpu_curr < 0xffff))
			c_attr->last_valid_tpu_curr = pattr->vdd_tpu_curr;
		else
			pattr->vdd_tpu_curr = c_attr->last_valid_tpu_curr;
#endif
		c_attr->bm_get_tpu_power(bmdi, &pattr->tpu_power);
		if ((pattr->tpu_power > 0) && (pattr->tpu_power < 0xffff))
			c_attr->last_valid_tpu_power = pattr->tpu_power;
		else
			pattr->tpu_power = c_attr->last_valid_tpu_power;
	} else {
		pattr->tpu_power = ATTR_NOTSUPPORTED_VALUE;
		pattr->vdd_tpu_volt = ATTR_NOTSUPPORTED_VALUE;
		pattr->vdd_tpu_curr = ATTR_NOTSUPPORTED_VALUE;
	}
	if (c_attr->bm_get_board_power != NULL) {
		c_attr->bm_get_board_power(bmdi, &pattr->board_power);
#ifndef SOC_MODE
		bm_read_mcu_current(bmdi, 0x28, &pattr->atx12v_curr);
#endif
	} else {
		pattr->board_power = ATTR_NOTSUPPORTED_VALUE;
		pattr->atx12v_curr = ATTR_NOTSUPPORTED_VALUE;
	}
#ifndef SOC_MODE
	memcpy(pattr->sn, cinfo->sn, 17);
#else
	memcpy(pattr->sn, "N/A", 3);
#endif

#ifndef SOC_MODE
	if (bmdi->cinfo.chip_id != 0x1682) {
		bm1684_get_board_type_by_id(bmdi, pattr->board_type,
			BM1684_BOARD_TYPE(bmdi));
	}
#else
	strncpy(pattr->board_type, "SOC", 3);
#endif
#ifndef SOC_MODE
	if (bmdi->cinfo.chip_id != 0x1682) {
		if (bmdi->bmcd != NULL) {
			pattr->card_index = bmdi->bmcd->card_index;
			pattr->chip_index_of_card = bmdi->dev_index - bmdi->bmcd->dev_start_index;
			if (pattr->chip_index_of_card < 0 || pattr->chip_index_of_card >  bmdi->bmcd->chip_num)
				pattr->chip_index_of_card = 0;
		}
	}
#else
	pattr->card_index = 0x0;
	pattr->chip_index_of_card = 0x0;
#endif
	mutex_unlock(&c_attr->attr_mutex);

	if (c_attr->fan_control)
		pattr->fan_speed = c_attr->bm_get_fan_speed(bmdi);
	else
		pattr->fan_speed = ATTR_NOTSUPPORTED_VALUE;

	switch (pattr->chip_id) {
	case 0x1682:
		pattr->tpu_min_clock = 687;
		pattr->tpu_max_clock = 687;
		pattr->tpu_current_clock = 687;
		pattr->board_max_power = 75;
		break;
	case 0x1684:
		if (pattr->chip_mode == 0) {
			pattr->tpu_min_clock = bmdi->boot_info.tpu_min_clk;
			pattr->tpu_max_clock = bmdi->boot_info.tpu_max_clk;
		} else {
			pattr->tpu_min_clock = 75;
			pattr->tpu_max_clock = 550;
		}
		mutex_lock(&bmdi->clk_reset_mutex);
		pattr->tpu_current_clock = bmdrv_1684_clk_get_tpu_freq(bmdi);
		if (pattr->tpu_current_clock < pattr->tpu_min_clock
                           || (pattr->tpu_current_clock > pattr->tpu_max_clock)) {
			pattr->tpu_current_clock = (int)0xFFFFFC00;
		}
		mutex_unlock(&bmdi->clk_reset_mutex);
		break;
	default:
		break;
	}
	if (pattr->board_power != ATTR_NOTSUPPORTED_VALUE)
		pattr->board_max_power = bmdi->boot_info.max_board_power;
	else
		pattr->board_max_power = ATTR_NOTSUPPORTED_VALUE;

#ifdef SOC_MODE
		pattr->ecc_enable = ATTR_NOTSUPPORTED_VALUE;
		pattr->ecc_correct_num = ATTR_NOTSUPPORTED_VALUE;
#else
	pattr->ecc_enable = bmdi->misc_info.ddr_ecc_enable;
	if (pattr->ecc_enable == 0)
		pattr->ecc_correct_num = ATTR_NOTSUPPORTED_VALUE;
	else
		pattr->ecc_correct_num = bmdi->cinfo.ddr_ecc_correctN;
#endif
	return 0;
}

int bmctl_ioctl_get_attr(struct bm_ctrl_info *bmci, unsigned long arg)
{
	int ret = 0;
	struct bm_smi_attr smi_attr;

	ret = copy_from_user(&smi_attr, (struct bm_smi_attr __user *)arg,
			sizeof(struct bm_smi_attr));
	if (ret) {
		pr_err("bmdev_ctl_ioctl: copy_from_user fail\n");
		return ret;
	}

	ret = bmctl_get_smi_attr(bmci, &smi_attr);
	if (ret)
		return ret;

	ret = copy_to_user((struct bm_smi_attr __user *)arg, &smi_attr,
			sizeof(struct bm_smi_attr));
	if (ret) {
		pr_err("BMCTL_GET_SMI_ATTR: copy_to_user fail\n");
		return ret;
	}
	return 0;
}

static int bmctl_get_smi_proc_gmem(struct bm_ctrl_info *bmci,
		struct bm_smi_proc_gmem *smi_proc_gmem)
{
	struct bm_device_info *bmdi;
	struct bm_handle_info *h_info;
	int proc_cnt = 0;

	bmdi = bmctl_get_bmdi(bmci, smi_proc_gmem->dev_id);
	if (!bmdi)
		return -1;

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	list_for_each_entry(h_info, &bmdi->handle_list, list) {
		smi_proc_gmem->pid[proc_cnt] = h_info->open_pid;
		smi_proc_gmem->gmem_used[proc_cnt] = h_info->gmem_used / 1024 / 1024;
		proc_cnt++;
		if (proc_cnt == 128)
			break;
	}
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);
	smi_proc_gmem->proc_cnt = proc_cnt;
	return 0;
}

int bmctl_ioctl_get_proc_gmem(struct bm_ctrl_info *bmci, unsigned long arg)
{
	int ret = 0;
	struct bm_smi_proc_gmem *smi_proc_gmem = kmalloc(sizeof(struct bm_smi_proc_gmem), GFP_KERNEL);

	if (!smi_proc_gmem)
		return -ENOMEM;

	ret = copy_from_user(smi_proc_gmem, (struct bm_smi_proc_gmem __user *)arg,
			sizeof(struct bm_smi_proc_gmem));
	if (ret) {
		pr_err("bmdev_ctl_ioctl: copy_from_user fail\n");
		kfree(smi_proc_gmem);
		return ret;
	}

	ret = bmctl_get_smi_proc_gmem(bmci, smi_proc_gmem);
	if (ret) {
		kfree(smi_proc_gmem);
		return ret;
	}

	ret = copy_to_user((struct bm_smi_proc_gmem__user *)arg, smi_proc_gmem,
			sizeof(struct bm_smi_proc_gmem));
	if (ret) {
		pr_err("BMCTL_GET_SMI_PROC_GMEM: copy_to_user fail\n");
		kfree(smi_proc_gmem);
		return ret;
	}
	kfree(smi_proc_gmem);
	return 0;
}

int bmctl_ioctl_set_led(struct bm_ctrl_info *bmci, unsigned long arg)
{
	int dev_id = arg & 0xff;
	int led_op = (arg >> 8) & 0xff;
	struct bm_device_info *bmdi = bmctl_get_bmdi(bmci, dev_id);
	struct bm_chip_attr *c_attr = &bmdi->c_attr;

	if (!bmdi)
		return -1;
	if (bmdi->cinfo.chip_id == 0x1684 &&
			bmdi->cinfo.platform == DEVICE &&
			c_attr->bm_set_led_status) {
		mutex_lock(&c_attr->attr_mutex);
		switch (led_op) {
		case 0:
			c_attr->bm_set_led_status(bmdi, LED_ON);
			c_attr->led_status = LED_ON;
			break;
		case 1:
			c_attr->bm_set_led_status(bmdi, LED_OFF);
			c_attr->led_status = LED_OFF;
			break;
		case 2:
			c_attr->bm_set_led_status(bmdi, LED_BLINK_ONE_TIMES_PER_2S);
			c_attr->led_status = LED_BLINK_ONE_TIMES_PER_2S;
			break;
		default:
			break;
		}
		mutex_unlock(&c_attr->attr_mutex);
	}
	return 0;
}

int bmctl_ioctl_set_ecc(struct bm_ctrl_info *bmci, unsigned long arg)
{
	int dev_id = arg & 0xff;
	int ecc_op = (arg >> 8) & 0xff;
	struct bm_device_info *bmdi = bmctl_get_bmdi(bmci, dev_id);

	if (!bmdi)
		return -1;

	if (bmdi->misc_info.chipid == 0x1684)
		set_ecc(bmdi, ecc_op);

	return 0;
}

#ifndef SOC_MODE
int bmctl_ioctl_recovery(struct bm_ctrl_info *bmci, unsigned long arg)
{
	int dev_id = arg & 0xff;
	int func_num = 0x0;
	struct pci_bus *root_bus;
	struct bm_device_info *bmdi = bmctl_get_bmdi(bmci, dev_id);
	struct bm_device_info *bmdi_1 =  NULL;
	struct bm_device_info *bmdi_2 =  NULL;

	if (!bmdi)
		return -ENODEV;

	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS) {
		func_num = bmdi->misc_info.domain_bdf&0x7;
		if (func_num == 0) {
			bmdi = bmctl_get_bmdi(bmci, dev_id);
			bmdi_1 =  bmctl_get_bmdi(bmci, dev_id + 1);
			bmdi_2 =  bmctl_get_bmdi(bmci, dev_id + 2);

			if (bmdi_2 == NULL || bmdi_1 == NULL || bmdi == NULL)
				return -ENODEV;
		}

		if (func_num == 1) {
			bmdi = bmctl_get_bmdi(bmci, dev_id - 1);
			bmdi_1 =  bmctl_get_bmdi(bmci, dev_id);
			bmdi_2 =  bmctl_get_bmdi(bmci, dev_id + 1);

			if (bmdi_2 == NULL || bmdi_1 == NULL || bmdi == NULL)
				return -ENODEV;
		}

		if (func_num == 2) {
			bmdi = bmctl_get_bmdi(bmci, dev_id - 2);
			bmdi_1 =  bmctl_get_bmdi(bmci, dev_id - 1);
			bmdi_2 =  bmctl_get_bmdi(bmci, dev_id);
			if (bmdi_2 == NULL || bmdi_1 == NULL || bmdi == NULL)
				return -ENODEV;
		}

		if (BM1684_HW_VERSION(bmdi) == 0x0) {
			pr_info("not support for sc5+ v1_0\n");
			return  -EPERM;
		}

		if (bmdi->misc_info.chipid == 0x1684) {
			pr_info("to reboot 1684, devid is %d\n", dev_id);
			bmdrv_wdt_start(bmdi);
		}

		pci_stop_and_remove_bus_device_locked(to_pci_dev(bmdi_2->cinfo.device));
		pci_stop_and_remove_bus_device_locked(to_pci_dev(bmdi_1->cinfo.device));
		pci_stop_and_remove_bus_device_locked(to_pci_dev(bmdi->cinfo.device));

		msleep(6500);
	} else {
		if (bmdi->misc_info.chipid == 0x1684) {
			pr_info("to reboot 1684, devid is %d\n", dev_id);
			bmdrv_wdt_start(bmdi);
		}
		pci_stop_and_remove_bus_device_locked(to_pci_dev(bmdi->cinfo.device));

		msleep(6500);
	}

	pci_lock_rescan_remove();
	list_for_each_entry(root_bus, &pci_root_buses, node) {
		pci_rescan_bus(root_bus);
	}
	pci_unlock_rescan_remove();

	return 0;
}
#endif
