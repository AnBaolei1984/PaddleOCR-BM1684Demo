#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/irqreturn.h>
#if KERNEL_VERSION(4, 11, 0) <= LINUX_VERSION_CODE
#include <linux/sched/signal.h>
#else
#include <linux/sched.h>
#endif

#include "bm_common.h"
#include "bm_memcpy.h"
#include "bm_irq.h"
#include "bm_gmem.h"
#include "bm1684_jpu.h"

static int dbg_enable;

#define DPRINTK(args...) \
	do { \
		if (dbg_enable) \
			pr_info(args); \
	} while (0)

#ifndef VM_RESERVED /* for kernel up to 3.7.0 version */
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

static int jpu_reg_phy_base[MAX_NUM_JPU_CORE] = {
	0x50030000,
	0x50040000,
	0x500b0000,
	0x500c0000,
};

static int jpu_irq[MAX_NUM_JPU_CORE] = {
	JPU_IRQ_NUM_0,
	JPU_IRQ_NUM_1,
	JPU_IRQ_NUM_2,
	JPU_IRQ_NUM_3,
};

static int jpu_core_reset_bit[MAX_NUM_JPU_CORE] = {
	JPU_CORE0_RST_BIT,
	JPU_CORE1_RST_BIT,
	JPU_CORE2_RST_BIT,
	JPU_CORE3_RST_BIT,
};

static int jpu_reset_core(struct bm_device_info *bmdi, int core)
{
	int val = 0;

	val = bm_read32(bmdi, JPU_RST_REG);
	val &= ~(1 << jpu_core_reset_bit[core]);
	bm_write32(bmdi, JPU_RST_REG, val);
	udelay(10);
	val |= (1 << jpu_core_reset_bit[core]);
	bm_write32(bmdi, JPU_RST_REG, val);

	return 0;
}

static int jpu_reset_all_cores(struct bm_device_info *bmdi)
{
	int i;

	mutex_lock(&bmdi->jpudrvctx.jpu_core_lock);
	for (i = 0; i < MAX_NUM_JPU_CORE; i++)
		jpu_reset_core(bmdi, i);
	mutex_unlock(&bmdi->jpudrvctx.jpu_core_lock);

	return 0;
}

static int jpu_set_remap(struct bm_device_info *bmdi,
		unsigned int core_idx,
		unsigned long read_addr,
		unsigned long write_addr)
{
	int val, read_high, write_high, shift_w, shift_r;

	read_high = read_addr >> 32;
	write_high = write_addr >> 32;

	if (core_idx >= MAX_NUM_JPU_CORE || core_idx < 0) {
		pr_err("[JPUDRV]:jpu_set_remap core_idx :%d error\n", core_idx);
		return -1;
	}

	shift_w = core_idx << 3;
	shift_r = (core_idx << 3) + 4;

	val = bm_read32(bmdi, JPU_CONTROL_REG);
	val &= ~(7 << shift_w);
	val &= ~(7 << shift_r);
	val |= read_high << shift_r;
	val |= write_high << shift_w;
	bm_write32(bmdi, JPU_CONTROL_REG, val);

	return 0;
}

int bm_jpu_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int jpu_free_instances(struct file *filp)
{
	jpudrv_instance_list_t *jil, *n;
	struct bm_device_info *bmdi;

	bmdi = (struct bm_device_info *)filp->private_data;

	mutex_lock(&bmdi->jpudrvctx.jpu_core_lock);
	list_for_each_entry_safe(jil, n, &bmdi->jpudrvctx.inst_head, list) {
		if (jil->filp == filp) {
			if (jil->inuse == 1) {
				jil->inuse = 0;
				mdelay(100);
				jpu_reset_core(bmdi, jil->inst_idx);
				up(&bmdi->jpudrvctx.jpu_sem);
			}
			jil->filp = NULL;
		}
	}
	mutex_unlock(&bmdi->jpudrvctx.jpu_core_lock);

	return 0;
}


int bm_jpu_release(struct inode *inode, struct file *filp)
{
	jpu_free_instances(filp);

	return 0;
}

long bm_jpu_ioctl(struct file *filp, u_int cmd, u_long arg)
{
	int ret = 0;
	struct bm_device_info *bmdi = (struct bm_device_info *)filp->private_data;

	switch (cmd) {
	case JDI_IOCTL_GET_INSTANCE_CORE_INDEX:
	{
		jpudrv_instance_list_t *jil, *n;
		jpudrv_remap_info_t  info;
		int inuse = -1, core_idx = -1;

		ret = copy_from_user(&info, (jpudrv_remap_info_t *)arg, sizeof(jpudrv_remap_info_t));
		if (ret) {
			pr_err("copy_from_user failed\n");
			ret = -EFAULT;
			break;
		}
		ret = down_interruptible(&bmdi->jpudrvctx.jpu_sem);
		if (!ret) {
			mutex_lock(&bmdi->jpudrvctx.jpu_core_lock);
			list_for_each_entry_safe(jil, n, &bmdi->jpudrvctx.inst_head, list) {
				if (!jil->inuse) {
					jil->inuse = 1;
					jil->filp = filp;
					inuse = 1;
					core_idx = jil->inst_idx;
					jpu_set_remap(bmdi, core_idx, info.read_addr, info.write_addr);
					bmdi->jpudrvctx.core[core_idx]++;
					list_del(&jil->list);
					list_add_tail(&jil->list, &bmdi->jpudrvctx.inst_head);
					DPRINTK("[JPUDRV]:inst_idx=%d, filp=%p\n", (int)jil->inst_idx, filp);
					break;
				}
				DPRINTK("[JPUDRV]:jil->inuse == 1,filp=%p\n", filp);
			}
			mutex_unlock(&bmdi->jpudrvctx.jpu_core_lock);

			if (inuse == 1) {
				info.core_idx = core_idx;
				ret = copy_to_user((void __user *)arg, &info, sizeof(jpudrv_remap_info_t));
				if (ret)
					ret = -EFAULT;
			}
		}

		if (signal_pending(current)) {
			pr_err("down_interruptible ret=%d\n", ret);
			ret = -ERESTARTSYS;
			break;
		}
		break;
	}

	case JDI_IOCTL_CLOSE_INSTANCE_CORE_INDEX:
	{
		u32 core_idx;
		jpudrv_instance_list_t *jil, *n;

		if (get_user(core_idx, (u32 __user *)arg))
			return -EFAULT;

		ret = mutex_lock_interruptible(&bmdi->jpudrvctx.jpu_core_lock);
		if (!ret) {
			list_for_each_entry_safe(jil, n, &bmdi->jpudrvctx.inst_head, list) {
				if (jil->inst_idx == core_idx && jil->filp == filp) {
					jil->inuse = 0;
					DPRINTK("[JPUDRV]:inst_idx=%d,filp=%p\n", core_idx, filp);
					break;
				}
			}
			up(&bmdi->jpudrvctx.jpu_sem);
			mutex_unlock(&bmdi->jpudrvctx.jpu_core_lock);
		}

		if (signal_pending(current)) {
			pr_err("mutex_lock interruptible ret=%d\n", ret);
			ret = -ERESTARTSYS;
			break;
		}
		break;
	}

	case JDI_IOCTL_WAIT_INTERRUPT:
	{
		jpudrv_intr_info_t  info;

		ret = copy_from_user(&info, (jpudrv_intr_info_t *)arg, sizeof(jpudrv_intr_info_t));
		if (ret != 0)
			return -EFAULT;
		if (!wait_event_interruptible_timeout(bmdi->jpudrvctx.interrupt_wait_q[info.core_idx],
					bmdi->jpudrvctx.interrupt_flag[info.core_idx] != 0,
					msecs_to_jiffies(info.timeout))) {
			pr_err("[JPUDRV]:jpu wait_event_interruptible timeout\n");
			ret = -ETIME;
			break;
		}

		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}

		bmdi->jpudrvctx.interrupt_flag[info.core_idx] = 0;
		break;
	}

	case JDI_IOCTL_RESET:
	{
		u32 core_idx;

		if (get_user(core_idx, (u32 __user *)arg))
			return -EFAULT;

		mutex_lock(&bmdi->jpudrvctx.jpu_core_lock);
		jpu_reset_core(bmdi, core_idx);
		mutex_unlock(&bmdi->jpudrvctx.jpu_core_lock);
		break;
	}

	case JDI_IOCTL_GET_REGISTER_INFO:
	{
		ret = copy_to_user((void __user *)arg, &bmdi->jpudrvctx.jpu_register,
				sizeof(jpudrv_buffer_t)*MAX_NUM_JPU_CORE);
		if (ret != 0)
			ret = -EFAULT;
		DPRINTK("[JPUDRV]:JDI_IOCTL_GET_REGISTER_INFO: phys_addr==0x%lx, virt_addr=0x%lx, size=%d\n",
				bmdi->jpudrvctx.jpu_register[0].phys_addr,
				bmdi->jpudrvctx.jpu_register[0].virt_addr,
				bmdi->jpudrvctx.jpu_register[0].size);
		break;
	}

	case JDI_IOCTL_WRITE_VMEM:
	{
		struct memcpy_args {
			unsigned long src;
			unsigned long dst;
			size_t  size;
		} memcpy_args;

		if (!bmdi)
			return -EFAULT;

		ret = copy_from_user(&memcpy_args, (struct memcpy_args *)arg, sizeof(memcpy_args));
		if (ret != 0)
			return -EFAULT;
		ret = bmdev_memcpy_s2d(bmdi, NULL, memcpy_args.dst, (void __user *)memcpy_args.src,
				memcpy_args.size, true, KERNEL_NOT_USE_IOMMU);
		if (ret) {
			pr_err("[JPUDRV]:JDI_IOCTL_WRITE_MEM failed\n");
			return -EFAULT;
		}
		break;
	}

	case JDI_IOCTL_READ_VMEM:
	{
		struct memcpy_args {
			unsigned long src;
			unsigned long dst;
			size_t  size;
		} memcpy_args;

		if (!bmdi)
			return -EFAULT;
		ret = copy_from_user(&memcpy_args, (struct memcpy_args *)arg, sizeof(memcpy_args));
		if (ret != 0)
			return -EFAULT;

		ret = bmdev_memcpy_d2s(bmdi, NULL, (void __user *)memcpy_args.dst, memcpy_args.src,
				memcpy_args.size, true, KERNEL_NOT_USE_IOMMU);
		if (ret != 0) {
			pr_err("[JPUDRV]:JDI_IOCTL_READ_MEM failed\n");
			return -EFAULT;
		}
		break;
	}

	default:
		pr_err("[JPUDRV]:No such ioctl, cmd is %d\n", cmd);
		break;
	}

	return ret;
}

static int jpu_map_to_register(struct file *filp, struct vm_area_struct *vm, int core_idx)
{
	unsigned long pfn;
	struct bm_device_info *bmdi = (struct bm_device_info *)filp->private_data;

	vm->vm_flags |= VM_IO | VM_RESERVED;
	vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);
	pfn = bmdi->jpudrvctx.jpu_register[core_idx].phys_addr >> PAGE_SHIFT;

	return remap_pfn_range(vm, vm->vm_start, pfn, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
}

int bm_jpu_mmap(struct file *filp, struct vm_area_struct *vm)
{
	int i = 0;
	struct bm_device_info *bmdi = (struct bm_device_info *)filp->private_data;

	for (i = 0; i < MAX_NUM_JPU_CORE; i++) {
		if (vm->vm_pgoff == (bmdi->jpudrvctx.jpu_register[i].phys_addr>>PAGE_SHIFT)) {
			DPRINTK("jpu core %d,vm->vm_pgoff = 0x%lx\n", i, bmdi->jpudrvctx.jpu_register[i].phys_addr);
			return jpu_map_to_register(filp, vm, i);
		}
	}

	return -1;
}

static irqreturn_t bm_jpu_irq_handler(struct bm_device_info *bmdi)
{
	int core = 0;
	int irq =  bmdi->cinfo.irq_id;

	DPRINTK("[JPUDRV]:irq handler card :%d irq :%d\n", bmdi->dev_index, irq);
	for (core = 0; core < MAX_NUM_JPU_CORE; core++) {
		if (bmdi->jpudrvctx.jpu_irq[core] == irq)
			break;
	}

	bmdi->jpudrvctx.interrupt_flag[core] = 1;
	wake_up_interruptible(&bmdi->jpudrvctx.interrupt_wait_q[core]);

	return IRQ_HANDLED;
}

static void bmdrv_jpu_irq_handler(struct bm_device_info *bmdi)
{
	bm_jpu_irq_handler(bmdi);
}

void bm_jpu_request_irq(struct bm_device_info *bmdi)
{
	int i = 0;

	for (i = 0; i < MAX_NUM_JPU_CORE; i++)
		bmdrv_submodule_request_irq(bmdi, bmdi->jpudrvctx.jpu_irq[i], bmdrv_jpu_irq_handler);
}

void bm_jpu_free_irq(struct bm_device_info *bmdi)
{
	int i = 0;

	for (i = 0; i < MAX_NUM_JPU_CORE; i++)
		bmdrv_submodule_free_irq(bmdi, bmdi->jpudrvctx.jpu_irq[i]);
}

int bm_jpu_addr_judge(unsigned long addr, struct bm_device_info *bmdi)
{
	int i = 0;

	for (i = 0; i < MAX_NUM_JPU_CORE; i++) {
		if ((bmdi->jpudrvctx.jpu_register[i].phys_addr >> PAGE_SHIFT) == addr)
			return 0;
	}

	return -1;
}


static int jpu_mem_base_init(struct bm_device_info *bmdi)
{
	int i = 0, ret = 0;
	u32 offset = 0;
	u64 bar_paddr = 0;
	void __iomem *bar_vaddr = NULL;
	struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

	for (i = 0; i < MAX_NUM_JPU_CORE; i++) {
		bm_get_bar_offset(pbar_info, jpu_reg_phy_base[i], &bar_vaddr, &offset);
		bm_get_bar_base(pbar_info, jpu_reg_phy_base[i], &bar_paddr);

		bmdi->jpudrvctx.jpu_register[i].phys_addr = (unsigned long)(bar_paddr + offset);
		bmdi->jpudrvctx.jpu_register[i].virt_addr = (unsigned long)(bar_vaddr + offset);
		bmdi->jpudrvctx.jpu_register[i].size = JPU_REG_SIZE;
		DPRINTK("[JPUDRV]:jpu reg base addr=0x%lx, virtu base addr=0x%lx, size=%u\n",
				bmdi->jpudrvctx.jpu_register[i].phys_addr,
				bmdi->jpudrvctx.jpu_register[i].virt_addr,
				bmdi->jpudrvctx.jpu_register[i].size);
	}

	bm_get_bar_offset(pbar_info, JPU_CONTROL_REG, &bar_vaddr, &offset);
	bm_get_bar_base(pbar_info, JPU_CONTROL_REG, &bar_paddr);

	bmdi->jpudrvctx.jpu_control_register.phys_addr = (unsigned long)(bar_paddr + offset);
	bmdi->jpudrvctx.jpu_control_register.virt_addr = (unsigned long)(bar_vaddr + offset);
	DPRINTK("[JPUDRV]:jpu control reg base addr=0x%lx,virtualbase=0x%lx\n",
			bmdi->jpudrvctx.jpu_control_register.phys_addr,
			bmdi->jpudrvctx.jpu_control_register.virt_addr);

	return ret;
}

int bmdrv_jpu_init(struct bm_device_info *bmdi)
{
	int i;
	jpudrv_instance_list_t *jil;

	memset(&bmdi->jpudrvctx, 0, sizeof(jpu_drv_context_t));
	memcpy(&bmdi->jpudrvctx.jpu_irq, &jpu_irq, sizeof(jpu_irq));
	INIT_LIST_HEAD(&bmdi->jpudrvctx.jbp_head);
	INIT_LIST_HEAD(&bmdi->jpudrvctx.inst_head);
	mutex_init(&bmdi->jpudrvctx.jpu_core_lock);
	sema_init(&bmdi->jpudrvctx.jpu_sem, MAX_NUM_JPU_CORE);

	for (i = 0; i < MAX_NUM_JPU_CORE; i++)
		init_waitqueue_head(&bmdi->jpudrvctx.interrupt_wait_q[i]);

	for (i = 0; i < MAX_NUM_JPU_CORE; i++) {
		jil = kzalloc(sizeof(*jil), GFP_KERNEL);
		if (!jil)
			return -ENOMEM;

		jil->inst_idx = i;
		jil->inuse = 0;
		jil->filp = NULL;

		mutex_lock(&bmdi->jpudrvctx.jpu_core_lock);
		list_add_tail(&jil->list, &bmdi->jpudrvctx.inst_head);
		mutex_unlock(&bmdi->jpudrvctx.jpu_core_lock);
	}

	jpu_reset_all_cores(bmdi);
	jpu_mem_base_init(bmdi);

	return 0;
}

int bmdrv_jpu_exit(struct bm_device_info *bmdi)
{

	if (bmdi->jpudrvctx.jpu_register[0].virt_addr)
		bmdi->jpudrvctx.jpu_register[0].virt_addr = 0;

	if (bmdi->jpudrvctx.jpu_control_register.virt_addr)
		bmdi->jpudrvctx.jpu_control_register.virt_addr = 0;

	jpu_reset_all_cores(bmdi);

	return 0;
}

static ssize_t info_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int len = 0, err = 0, i = 0;
	char data[512] = { 0 };
	struct bm_device_info *bmdi;

	bmdi = PDE_DATA(file_inode(file));
	len = strlen(data);
	sprintf(data + len, "\njpu ctl reg base addr:0x%x\n", JPU_CONTROL_REG);
	len = strlen(data);
	for (i = 0; i < MAX_NUM_JPU_CORE; i++) {
		sprintf(data + len, "\njpu core[%d] base addr:0x%x, size: 0x%x\n",
				i, jpu_reg_phy_base[i], JPU_REG_SIZE);
		len = strlen(data);
	}
	len = strlen(data);
	if (*ppos >= len)
		return 0;
	err = copy_to_user(buf, data, len);
	if (err)
		return 0;
	*ppos = len;

	return len;
}

static ssize_t info_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	int err = 0, i = 0;
	char data[256] = { 0 };
	unsigned long val = 0, addr = 0;
	struct bm_device_info *bmdi;

	bmdi = PDE_DATA(file_inode(file));

	err = copy_from_user(data, buf, size);
	if (err)
		return -EFAULT;

	err = kstrtoul(data, 16, &val);
	if (err < 0)
		return -EFAULT;
	if (val == 1)
		dbg_enable = 1;
	else if (val == 0)
		dbg_enable = 0;
	else
		addr = val;

	if (val == 0 || val == 1)
		return size;

	if (addr == JPU_CONTROL_REG)
		goto valid_address;

	for (i = 0; i < MAX_NUM_JPU_CORE; i++)
		if (addr >= jpu_reg_phy_base[i] &&
				(addr <= jpu_reg_phy_base[i] + JPU_REG_SIZE))
			goto valid_address;

	pr_err("jpu proc get addres: 0x%lx invalid\n", addr);
	return -EFAULT;

valid_address:
	val = bm_read32(bmdi, addr);
	pr_info("jpu get address :0x%lx value: 0x%lx\n", addr, val);
	return size;
}

const struct file_operations bmdrv_jpu_file_ops = {
	.read = info_read,
	.write = info_write,
};
