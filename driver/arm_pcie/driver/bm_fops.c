#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include "bm_common.h"
#include "bm_uapi.h"
#include "bm_thread.h"
#include "bm_fw.h"
#include "bm_ctl.h"
#include "bm_drv.h"
#include "bm1684_clkrst.h"
#include "bm1684_base64.h"
#include "bm_timer.h"
#ifndef SOC_MODE
#include "spi.h"
#include "i2c.h"
#include "bm1684_vpp.h"
#include "bm1684_jpu.h"
#include "bm1684_irq.h"
#include "vpu/vpu.h"
#endif

extern dev_t bm_devno_base;
extern dev_t bm_ctl_devno_base;

static int bmdev_open(struct inode *inode, struct file *file)
{
	struct bm_device_info *bmdi;
	pid_t open_pid;
	struct bm_handle_info *h_info;
	struct bm_thread_info *thd_info = NULL;

	PR_TRACE("bmdev_open\n");
	bmdi = container_of(inode->i_cdev, struct bm_device_info, cdev);

	open_pid = current->pid;

	h_info = kmalloc(sizeof(struct bm_handle_info), GFP_KERNEL);
	if (!h_info)
		return -ENOMEM;
	hash_init(h_info->api_htable);
	h_info->file = file;
	h_info->open_pid = open_pid;
	h_info->gmem_used = 0ULL;
	h_info->h_send_api_seq = 0ULL;
	h_info->h_cpl_api_seq = 0ULL;
	init_completion(&h_info->h_msg_done);
	mutex_init(&h_info->h_api_seq_mutex);

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	thd_info = bmdrv_create_thread_info(h_info, open_pid);
	if (!thd_info) {
		kfree(h_info);
		mutex_unlock(&bmdi->gmem_info.gmem_mutex);
		return -ENOMEM;
	}
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	list_add(&h_info->list, &bmdi->handle_list);
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);

	file->private_data = bmdi;
#ifndef SOC_MODE
	if (bmdrv_get_gmem_mode(bmdi) != GMEM_TPU_ONLY) {
		bm_vpu_open(inode, file);
		bm_jpu_open(inode, file);
	}
#endif

#ifdef USE_RUNTIME_PM
	pm_runtime_get_sync(bmdi->cinfo.device);
#endif
	return 0;
}

static ssize_t bmdev_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
#ifndef SOC_MODE
	return bm_vpu_read(filp, buf, len, ppos);
#else
	return -1;
#endif
}

static ssize_t bmdev_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
#ifndef SOC_MODE
	return bm_vpu_write(filp, buf, len, ppos);
#else
	return -1;
#endif
}

static int bmdev_fasync(int fd, struct file *filp, int mode)
{
#ifndef SOC_MODE
	return bm_vpu_fasync(fd, filp, mode);
#else
	return -1;
#endif
}

static int bmdev_close(struct inode *inode, struct file *file)
{
	struct bm_device_info *bmdi = file->private_data;
	struct bm_handle_info *h_info;

	if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
		pr_err("bmdrv: file list is not found!\n");
		return -EINVAL;
	}

#ifndef SOC_MODE
	if (bmdrv_get_gmem_mode(bmdi) != GMEM_TPU_ONLY) {
		bm_vpu_release(inode, file);
		bm_jpu_release(inode, file);
	}
#endif
	/* invalidate pending APIs in msgfifo */
	bmdev_invalidate_pending_apis(bmdi, h_info);

	mutex_lock(&bmdi->gmem_info.gmem_mutex);
	bmdrv_delete_thread_info(h_info);
	list_del(&h_info->list);
	kfree(h_info);
	mutex_unlock(&bmdi->gmem_info.gmem_mutex);

	file->private_data = NULL;

#ifdef USE_RUNTIME_PM
	pm_runtime_put_sync(bmdi->cinfo.device);
#endif
	PR_TRACE("bmdev_close\n");
	return 0;
}

static long bm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct bm_device_info *bmdi = (struct bm_device_info *)file->private_data;
	int ret = 0;

	if (bmdi->status_over_temp || bmdi->status_pcie) {
		pr_err("the temperature is too high, causing the MCU to power off the board, causing the pcie link to be abnormal\n");
		return -1;
	}

	switch (cmd) {
#ifndef SOC_MODE
	case BMDEV_TRIGGER_VPP:
		//pr_info("begin process trigger_vpp in bm_ioctl.\n");
		ret = trigger_vpp(bmdi, arg);
		//pr_info("process trigger_vpp in bm_ioctl complete.ret=%d\n", ret);
		break;
	case BMDEV_TRIGGER_BMCPU:
		{
			unsigned char buffer[4] = {0xea, 0xea, 0xea, 0xea};
			unsigned char zeros[4] = {0};

			mutex_lock(&bmdi->c_attr.attr_mutex);
			ret = bmdev_memcpy_s2d_internal(bmdi, 0x102ffffc, (const void *)buffer, sizeof(buffer));
			msleep(arg);
			mutex_unlock(&bmdi->c_attr.attr_mutex);
			bmdev_memcpy_s2d_internal(bmdi, 0x102ffffc, (const void *)zeros, sizeof(zeros));
			break;
		}
	case BMDEV_I2C_READ_SLAVE:
		ret = bm_i2c_read_slave(bmdi, arg);
		break;

	case BMDEV_I2C_WRITE_SLAVE:
		ret = bm_i2c_write_slave(bmdi, arg);
		break;

#endif
	case BMDEV_MEMCPY:
		ret = bmdev_memcpy(bmdi, file, arg);
		break;

	case BMDEV_ALLOC_GMEM:
		ret = bmdrv_gmem_ioctl_alloc_mem(bmdi, file, arg);
		break;

	case BMDEV_ALLOC_GMEM_ION:
		ret = bmdrv_gmem_ioctl_alloc_mem_ion(bmdi, file, arg);
		break;

	case BMDEV_FREE_GMEM:
		ret = bmdrv_gmem_ioctl_free_mem(bmdi, file, arg);
		break;

	case BMDEV_TOTAL_GMEM:
		ret = put_user(bmdrv_gmem_total_size(bmdi), (u64 __user *)arg);
		break;

	case BMDEV_AVAIL_GMEM:
		ret = put_user(bmdrv_gmem_avail_size(bmdi), (u64 __user *)arg);
		break;
	if (bmdi->status_sync_api == 0) {
		case BMDEV_SEND_API:
			ret = bmdrv_send_api(bmdi, file, arg, false);
			break;

		case BMDEV_SEND_API_EXT:
			ret = bmdrv_send_api(bmdi, file, arg, true);
			break;

		case BMDEV_QUERY_API_RESULT:
			ret = bmdrv_query_api(bmdi, file, arg);
			break;

		case BMDEV_THREAD_SYNC_API:
			ret = bmdrv_thread_sync_api(bmdi, file);
			bmdi->status_sync_api = ret;
			break;

		case BMDEV_HANDLE_SYNC_API:
			ret = bmdrv_handle_sync_api(bmdi, file);
			bmdi->status_sync_api = ret;
			break;

		case BMDEV_DEVICE_SYNC_API:
			ret = bmdrv_device_sync_api(bmdi);
			bmdi->status_sync_api = ret;
			break;
	} else {
		pr_err("bm-sophon%d: tpu hang\n",bmdi->dev_index);
		ret = -EBUSY;
	}
	case BMDEV_REQUEST_ARM_RESERVED:
		ret = put_user(bmdi->gmem_info.resmem_info.armreserved_addr, (unsigned long __user *)arg);
		break;

	case BMDEV_RELEASE_ARM_RESERVED:
		break;

	case BMDEV_UPDATE_FW_A9:
		{
			bm_fw_desc fw;

			if (copy_from_user(&fw, (bm_fw_desc __user *)arg, sizeof(bm_fw_desc)))
				return -EFAULT;

#ifndef SOC_MODE
#if SYNC_API_INT_MODE == 1
			if (bmdi->cinfo.chip_id == 0x1684)
				bm1684_pcie_msi_irq_enable(bmdi->cinfo.pcidev, bmdi);
#endif
#endif
			ret = bmdrv_fw_load(bmdi, file, &fw);
			break;
		}
#ifndef SOC_MODE
	case BMDEV_PROGRAM_A53:
		{
			struct bin_buffer bin_buf;
			u8 *kernel_bin_addr;

			if (copy_from_user(&bin_buf, (struct bin_buffer __user *)arg,
						sizeof(struct bin_buffer)))
				return -EFAULT;
			kernel_bin_addr = kmalloc(bin_buf.size, GFP_KERNEL);
			if (!kernel_bin_addr)
				return -ENOMEM;
			if (copy_from_user(kernel_bin_addr, (u8 __user *)bin_buf.buf,
						bin_buf.size))
				return -EFAULT;
			bm_spi_init(bmdi);
			ret = bm_spi_flash_program(bmdi, kernel_bin_addr, bin_buf.target_addr, bin_buf.size);
			bm_spi_enable_dmmr(bmdi);
			kfree(kernel_bin_addr);
			break;
		}

	case BMDEV_PROGRAM_MCU:
		{
			struct bin_buffer bin_buf;
			u8 *kernel_bin_addr;
			u8 *read_bin_addr;

			if (copy_from_user(&bin_buf, (struct bin_buffer __user *)arg,
						sizeof(struct bin_buffer)))
				return -EFAULT;
			kernel_bin_addr = kmalloc(bin_buf.size, GFP_KERNEL);
			if (!kernel_bin_addr)
				return -ENOMEM;
			if (copy_from_user(kernel_bin_addr, (u8 __user *)bin_buf.buf,
						bin_buf.size)) {
				kfree(kernel_bin_addr);
				return -EFAULT;
			}
			ret = bm_mcu_program(bmdi, kernel_bin_addr, bin_buf.size, bin_buf.target_addr);
			if (ret) {
				kfree(kernel_bin_addr);
				break;
			}
			pr_info("mcu program offset 0x%x size 0x%x complete\n", bin_buf.target_addr,
					bin_buf.size);
			msleep(1500);

			read_bin_addr = kmalloc(bin_buf.size, GFP_KERNEL);
			if (!read_bin_addr) {
				kfree(kernel_bin_addr);
				return -ENOMEM;
			}
			ret = bm_mcu_read(bmdi, read_bin_addr, bin_buf.size, bin_buf.target_addr);
			if (!ret) {
				ret = memcmp(kernel_bin_addr, read_bin_addr, bin_buf.size);
				if (ret)
					pr_err("read after program mcu and check failed!\n");
				else
					pr_info("read after program mcu and check succeeds.\n");
			}
			kfree(kernel_bin_addr);
			kfree(read_bin_addr);
			break;
		}

	case BMDEV_CHECKSUM_MCU:
		{
			struct bin_buffer bin_buf;
			unsigned char cksum[16];

			if (copy_from_user(&bin_buf, (struct bin_buffer __user *)arg,
						sizeof(struct bin_buffer)))
				return -EFAULT;
			ret = bm_mcu_checksum(bmdi, bin_buf.target_addr, bin_buf.size, cksum);
			if (ret)
				return -EFAULT;
			if (copy_to_user((u8 __user *)bin_buf.buf, cksum, sizeof(cksum)))
				return -EFAULT;
			break;
		}

	case BMDEV_GET_BOOT_INFO:
		{
			struct bm_boot_info boot_info;

			if (bm_spi_flash_get_boot_info(bmdi, &boot_info))
				return -EFAULT;

			if (copy_to_user((struct bm_boot_info __user *)arg, &boot_info,
						sizeof(struct bm_boot_info)))
				return -EFAULT;

			break;
		}

	case BMDEV_UPDATE_BOOT_INFO:
		{
			struct bm_boot_info boot_info;

			if (copy_from_user(&boot_info, (struct bm_boot_info __user *)arg,
						sizeof(struct bm_boot_info)))
				return -EFAULT;

			if (bm_spi_flash_update_boot_info(bmdi, &boot_info))
				return -EFAULT;

			break;
		}

	case BMDEV_SET_REG:
		{
			struct bm_reg reg;

			if (copy_from_user(&reg, (struct bm_reg __user *)arg,
						sizeof(struct bm_reg)))
				return -EFAULT;

			if (bm_set_reg(bmdi, &reg))
				return -EFAULT;
			break;
		}

	case BMDEV_GET_REG:
		{
			struct bm_reg reg;

			if (copy_from_user(&reg, (struct bm_reg __user *)arg,
						sizeof(struct bm_reg)))
				return -EFAULT;

			if (bm_get_reg(bmdi, &reg))
				return -EFAULT;

			if (copy_to_user((struct bm_reg __user *)arg, &reg,
						sizeof(struct bm_reg)))
				return -EFAULT;

			break;
		}

	case BMDEV_SN:
		ret = bm_burning_info_sn(bmdi, arg);
		break;
	case BMDEV_MAC0:
		ret = bm_burning_info_mac(bmdi, 0, arg);
		break;
	case BMDEV_MAC1:
		ret = bm_burning_info_mac(bmdi, 1, arg);
		break;
	case BMDEV_BOARD_TYPE:
		ret = bm_burning_info_board_type(bmdi, arg);
		break;
#endif
	case BMDEV_ENABLE_PERF_MONITOR:
		{
			struct bm_perf_monitor perf_monitor;

			if (copy_from_user(&perf_monitor, (struct bm_perf_monitor __user *)arg,
						sizeof(struct bm_perf_monitor)))
				return -EFAULT;

			if (bmdev_enable_perf_monitor(bmdi, &perf_monitor))
				return -EFAULT;

			break;
		}

	case BMDEV_DISABLE_PERF_MONITOR:
		{
			struct bm_perf_monitor perf_monitor;

			if (copy_from_user(&perf_monitor, (struct bm_perf_monitor __user *)arg,
						sizeof(struct bm_perf_monitor)))
				return -EFAULT;

			if (bmdev_disable_perf_monitor(bmdi, &perf_monitor))
				return -EFAULT;

			break;
		}

	case BMDEV_GET_DEVICE_TIME:
		{
			unsigned long time_us = 0;

			time_us = bmdev_timer_get_time_us(bmdi);

			ret = copy_to_user((unsigned long __user *)arg,
					&time_us, sizeof(unsigned long));
			break;
		}

	case BMDEV_GET_PROFILE:
		{
			struct bm_thread_info *thd_info;
			pid_t api_pid;
			struct bm_handle_info *h_info;

			if (bmdev_gmem_get_handle_info(bmdi, file, &h_info)) {
				pr_err("bmdrv: file list is not found!\n");
				return -EINVAL;
			}

			mutex_lock(&bmdi->gmem_info.gmem_mutex);
			api_pid = current->pid;
			thd_info = bmdrv_find_thread_info(h_info, api_pid);

			if (!thd_info)
				ret = -EFAULT;
			else
				ret = copy_to_user((unsigned long __user *)arg,
						&thd_info->profile, sizeof(bm_profile_t));
			mutex_unlock(&bmdi->gmem_info.gmem_mutex);
			break;
		}

	case BMDEV_GET_DEV_STAT:
		{
			bm_dev_stat_t stat;
			struct bm_chip_attr *c_attr;

			c_attr = &bmdi->c_attr;
			stat.mem_total = (int)(bmdrv_gmem_total_size(bmdi)/1024/1024);
			stat.mem_used = stat.mem_total - (int)(bmdrv_gmem_avail_size(bmdi)/1024/1024);
			stat.tpu_util = c_attr->bm_get_npu_util(bmdi);
			bmdrv_heap_mem_used(bmdi, &stat);
			ret = copy_to_user((unsigned long __user *)arg, &stat, sizeof(bm_dev_stat_t));
			break;
		}

	case BMDEV_TRACE_ENABLE:
		ret = bmdev_trace_enable(bmdi, file);
		break;

	case BMDEV_TRACE_DISABLE:
		ret = bmdev_trace_disable(bmdi, file);
		break;

	case BMDEV_TRACEITEM_NUMBER:
		ret = bmdev_traceitem_number(bmdi, file, arg);
		break;

	case BMDEV_TRACE_DUMP:
		ret = bmdev_trace_dump_one(bmdi, file, arg);
		break;

	case BMDEV_TRACE_DUMP_ALL:
		ret = bmdev_trace_dump_all(bmdi, file, arg);
		break;

	case BMDEV_GET_MISC_INFO:
		ret = copy_to_user((unsigned long __user *)arg,	&bmdi->misc_info,
				sizeof(struct bm_misc_info));
		break;

	case BMDEV_SET_TPU_DIVIDER:
		if (bmdi->misc_info.pcie_soc_mode == BM_DRV_SOC_MODE)
			ret = -EPERM;
		else {
			mutex_lock(&bmdi->clk_reset_mutex);
			ret = bmdev_clk_ioctl_set_tpu_divider(bmdi, arg);
			mutex_unlock(&bmdi->clk_reset_mutex);
		}
		break;

	case BMDEV_SET_TPU_FREQ:
		mutex_lock(&bmdi->clk_reset_mutex);
		ret = bmdev_clk_ioctl_set_tpu_freq(bmdi, arg);
		mutex_unlock(&bmdi->clk_reset_mutex);
		break;

	case BMDEV_GET_TPU_FREQ:
		mutex_lock(&bmdi->clk_reset_mutex);
		ret = bmdev_clk_ioctl_get_tpu_freq(bmdi, arg);
		mutex_unlock(&bmdi->clk_reset_mutex);
		break;

	case BMDEV_SET_MODULE_RESET:
		if (bmdi->misc_info.pcie_soc_mode == BM_DRV_SOC_MODE)
			ret = -EPERM;
		else {
			mutex_lock(&bmdi->clk_reset_mutex);
			ret = bmdev_clk_ioctl_set_module_reset(bmdi, arg);
			mutex_unlock(&bmdi->clk_reset_mutex);
		}
		break;

#ifndef SOC_MODE
	case BMDEV_BASE64_PREPARE:
		{
			struct ce_base  test_base;

			switch (bmdi->cinfo.chip_id) {
			case 0x1682:
				pr_info("bm1682 not supported!\n");
				break;
			case 0x1684:
				ret = copy_from_user(&test_base, (struct ce_base *)arg,
						sizeof(struct ce_base));
				if (ret) {
					pr_err("s2d failed\n");
					return -EFAULT;
				}
				base64_prepare(bmdi, test_base);
				break;
			}
			break;
		}

	case BMDEV_BASE64_START:
		{
			switch (bmdi->cinfo.chip_id) {
			case 0x1682:
				pr_info("bm1682 not supported!\n");
				break;
			case 0x1684:
				ret = 0;
				base64_start(bmdi);
				if (ret)
					return -EFAULT;
				pr_info("setting ready\n");
				break;
			}
			break;
		}

	case BMDEV_BASE64_CODEC:
		{
			struct ce_base test_base;

			switch (bmdi->cinfo.chip_id) {
			case 0x1682:
				pr_info("bm1682 not supported!\n");
				break;
			case 0x1684:
				ret = copy_from_user(&test_base, (struct ce_base *)arg,
						sizeof(struct ce_base));
				if (ret) {
					pr_err("s2d failed\n");
					return -EFAULT;
				}
				mutex_lock(&bmdi->spacc_mutex);
				base64_prepare(bmdi, test_base);
				base64_start(bmdi);
				mutex_unlock(&bmdi->spacc_mutex);
				if (ret)
					return -EFAULT;
				break;
			}
			break;
		}

#endif

#ifdef SOC_MODE
	case BMDEV_INVALIDATE_GMEM:
		{
			u64 arg64;
			u32 arg32l, arg32h;

			if (get_user(arg64, (u64 __user *)arg)) {
				dev_dbg(bmdi->dev, "cmd 0x%x get user failed\n", cmd);
				return -EINVAL;
			}
			arg32l = (u32)arg64;
			arg32h = (u32)((arg64 >> 32) & 0xffffffff);
			bmdrv_gmem_invalidate(bmdi, ((unsigned long)arg32h)<<6, arg32l);
			break;
		}

	case BMDEV_FLUSH_GMEM:
		{
			u64 arg64;
			u32 arg32l, arg32h;

			if (get_user(arg64, (u64 __user *)arg)) {
				dev_dbg(bmdi->dev, "cmd 0x%x get user failed\n", cmd);
				return -EINVAL;
			}

			arg32l = (u32)arg64;
			arg32h = (u32)((arg64 >> 32) & 0xffffffff);
			bmdrv_gmem_flush(bmdi, ((unsigned long)arg32h)<<6, arg32l);
			break;
		}
#endif

	default:
		dev_err(bmdi->dev, "*************Invalid ioctl parameter************\n");
		return -EINVAL;
	}
	return ret;
}

static long bmdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct bm_device_info *bmdi = (struct bm_device_info *)file->private_data;
	int ret = 0;

	if ((_IOC_TYPE(cmd)) == BMDEV_IOCTL_MAGIC)
		ret = bm_ioctl(file, cmd, arg);
#ifndef SOC_MODE
	else if ((_IOC_TYPE(cmd)) == VDI_IOCTL_MAGIC)
		ret = bm_vpu_ioctl(file, cmd, arg);
	else if ((_IOC_TYPE(cmd)) == JDI_IOCTL_MAGIC)
		ret = bm_jpu_ioctl(file, cmd, arg);
#endif
	else {
		dev_dbg(bmdi->dev, "Unknown cmd 0x%x\n", cmd);
		return -EINVAL;
	}
	return ret;
}

static int bmdev_ctl_open(struct inode *inode, struct file *file)
{
	struct bm_ctrl_info *bmci;

	bmci = container_of(inode->i_cdev, struct bm_ctrl_info, cdev);
	file->private_data = bmci;
	return 0;
}

static int bmdev_ctl_close(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long bmdev_ctl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct bm_ctrl_info *bmci = file->private_data;
	int ret = 0;

	switch (cmd) {
	case BMCTL_GET_DEV_CNT:
		ret = put_user(bmci->dev_count, (int __user *)arg);
		break;

	case BMCTL_GET_SMI_ATTR:
		ret = bmctl_ioctl_get_attr(bmci, arg);
		break;

	case BMCTL_GET_PROC_GMEM:
		ret = bmctl_ioctl_get_proc_gmem(bmci, arg);
		break;

	case BMCTL_SET_LED:
#ifndef SOC_MODE
		ret = bmctl_ioctl_set_led(bmci, arg);
#endif
		break;

	case BMCTL_SET_ECC:
#ifndef SOC_MODE
		ret = bmctl_ioctl_set_ecc(bmci, arg);
#endif
		break;

		/* test i2c1 slave */
	case BMCTL_TEST_I2C1:
		//	ret = bmctl_test_i2c1(bmci, arg);
		break;

	case BMCTL_DEV_RECOVERY:
		ret = -EPERM;
#ifndef SOC_MODE
		ret = bmctl_ioctl_recovery(bmci, arg);
#endif
		break;

	case BMCTL_GET_DRIVER_VERSION:
		ret = put_user(BM_DRIVER_VERSION, (int __user *)arg);
		break;

	default:
		pr_err("*************Invalid ioctl parameter************\n");
		return -EINVAL;
	}
	return ret;
}

static const struct file_operations bmdev_fops = {
	.open = bmdev_open,
	.read = bmdev_read,
	.write = bmdev_write,
	.fasync = bmdev_fasync,
	.release = bmdev_close,
	.unlocked_ioctl = bmdev_ioctl,
	.mmap = bmdev_mmap,
	.owner = THIS_MODULE,
};

static const struct file_operations bmdev_ctl_fops = {
	.open = bmdev_ctl_open,
	.release = bmdev_ctl_close,
	.unlocked_ioctl = bmdev_ctl_ioctl,
	.owner = THIS_MODULE,
};

int bmdev_register_device(struct bm_device_info *bmdi)
{
	bmdi->devno = MKDEV(MAJOR(bm_devno_base), MINOR(bm_devno_base) + bmdi->dev_index);
	bmdi->dev = device_create(bmdrv_class_get(), bmdi->parent, bmdi->devno, NULL,
			"%s%d", BM_CDEV_NAME, bmdi->dev_index);

	cdev_init(&bmdi->cdev, &bmdev_fops);

	bmdi->cdev.owner = THIS_MODULE;
	cdev_add(&bmdi->cdev, bmdi->devno, 1);

	dev_set_drvdata(bmdi->dev, bmdi);
	dev_dbg(bmdi->dev, "%s\n", __func__);
	return 0;
}

int bmdev_unregister_device(struct bm_device_info *bmdi)
{
	dev_dbg(bmdi->dev, "%s\n", __func__);
	cdev_del(&bmdi->cdev);
	device_destroy(bmdrv_class_get(), bmdi->devno);
	return 0;
}

int bmdev_ctl_register_device(struct bm_ctrl_info *bmci)
{
	bmci->devno = MKDEV(MAJOR(bm_ctl_devno_base), MINOR(bm_ctl_devno_base));
	bmci->dev = device_create(bmdrv_class_get(), NULL, bmci->devno, NULL,
			"%s", BMDEV_CTL_NAME);
	cdev_init(&bmci->cdev, &bmdev_ctl_fops);
	bmci->cdev.owner = THIS_MODULE;
	cdev_add(&bmci->cdev, bmci->devno, 1);

	dev_set_drvdata(bmci->dev, bmci);

	return 0;
}

int bmdev_ctl_unregister_device(struct bm_ctrl_info *bmci)
{
	cdev_del(&bmci->cdev);
	device_destroy(bmdrv_class_get(), bmci->devno);
	return 0;
}
