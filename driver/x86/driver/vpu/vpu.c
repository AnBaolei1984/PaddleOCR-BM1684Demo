//=====================================================================
//  This file is linux device driver for VPU.
//---------------------------------------------------------------------
//  This confidential and proprietary software may be used only
//  as authorized by a licensing agreement from BITMAIN Inc.
//  In the event of publication, the following notice is applicable:
//
//  (C) COPYRIGHT 2015 - 2025  BITMAIN INC. ALL RIGHTS RESERVED
//
//  The entire notice above must be reproduced on all authorized copies.
//
//=====================================================================

#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/signal.h>
#else
#include <linux/sched.h>
#endif
#include "vmm.h"
#include "vpu.h"
#include "bm_irq.h"
#include "bm_common.h"
#include "bm_memcpy.h"
#include "bm_gmem.h"
#include "../bm1684/bm1684_reg.h"

#ifdef ENABLE_DEBUG_MSG
#define DPRINTK(args...)	pr_info(args)
#else
#define DPRINTK(args...)
#endif

#define VPU_SUPPORT_ISR
#ifdef VPU_SUPPORT_ISR
//#define VPU_IRQ_CONTROL
#endif

#define VPU_SUPPORT_RESERVED_VIDEO_MEMORY
#define VPU_SUPPORT_CLOSE_COMMAND

#define BIT_BASE                    (0x0000)
#define BIT_INT_CLEAR               (BIT_BASE + 0x00C)
#define BIT_INT_STS                 (BIT_BASE + 0x010)
#define BIT_BUSY_FLAG               (BIT_BASE + 0x160)
#define BIT_RUN_COMMAND             (BIT_BASE + 0x164)
#define BIT_RUN_INDEX               (BIT_BASE + 0x168)
#define BIT_RUN_COD_STD             (BIT_BASE + 0x16C)
#define BIT_INT_REASON              (BIT_BASE + 0x174)


/* WAVE5 registers */
#define W5_REG_BASE                 (0x0000)
#define W5_VPU_INT_REASON_CLEAR     (W5_REG_BASE + 0x0034)
#define W5_VPU_HOST_INT_REQ         (W5_REG_BASE + 0x0038)
#define W5_VPU_VINT_CLEAR           (W5_REG_BASE + 0x003C)
#define W5_VPU_VPU_INT_STS          (W5_REG_BASE + 0x0044)
#define W5_VPU_INT_REASON           (W5_REG_BASE + 0x004c)
#define W5_VPU_BUSY_STATUS          (W5_REG_BASE + 0x0070)
#define W5_VPU_HOST_INT_REQ         (W5_REG_BASE + 0x0038)
#define W5_RET_SUCCESS              (W5_REG_BASE + 0x0108)
#define W5_RET_FAIL_REASON          (W5_REG_BASE + 0x010C)

#ifdef VPU_SUPPORT_CLOSE_COMMAND
#define W5_RET_STAGE0_INSTANCE_INFO     (W5_REG_BASE + 0x01EC)
// for debug the current status
#define W5_COMMAND                      (W5_REG_BASE + 0x0100)
#define W5_RET_QUEUE_STATUS             (W5_REG_BASE + 0x01E0)
#define W5_RET_STAGE1_INSTANCE_INFO     (W5_REG_BASE + 0x01F0)
#define W5_RET_STAGE2_INSTANCE_INFO     (W5_REG_BASE + 0x01F4)
#define W5_CMD_INSTANCE_INFO            (W5_REG_BASE + 0x0110)
#endif

#define W5_RET_BS_EMPTY_INST        (W5_REG_BASE + 0x01E4)
#define W5_RET_QUEUE_CMD_DONE_INST  (W5_REG_BASE + 0x01E8)
#define W5_RET_DONE_INSTANCE_INFO   (W5_REG_BASE + 0x01FC)
#define VPU_PRODUCT_CODE_REGISTER   (BIT_BASE    + 0x1044)

#ifndef VM_RESERVED /*for kernel up to 3.7.0 version*/
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define VPU_REMAP_REG 0x50010064
#define VPU_REG_SIZE  0x10000
#define SW_RESET_REG1 0x50010c04
#define SW_RESET_REG2 0x50010c08

#define TOP_CLK_EN_REG1 0x50000804
#define CLK_EN_AXI_VD0_WAVE1_GEN_REG1 25
#define CLK_EN_APB_VD0_WAVE1_GEN_REG1 24
#define CLK_EN_AXI_VD0_WAVE0_GEN_REG1 23
#define CLK_EN_APB_VD0_WAVE0_GEN_REG1 22
#define CLK_EN_AXI_VDE_AXI_BRIDGE_GEN_REG1 21
#define CLK_EN_APB_VDE_WAVE_GEN_REG1 20
#define CLK_EN_AXI_VDE_WAVE_GEN_REG1 19
#define TOP_CLK_EN_REG2 0x50000808
#define CLK_EN_AXI_VD1_WAVE1_GEN_REG2 3
#define CLK_EN_APB_VD1_WAVE1_GEN_REG2 2
#define CLK_EN_AXI_VD1_WAVE0_GEN_REG2 1
#define CLK_EN_APB_VD1_WAVE0_GEN_REG2 0

#define vpu_read_register(core, addr) \
	bm_read32(bmdi, bmdi->vpudrvctx.s_vpu_reg_phy_base[core] + \
			bmdi->vpudrvctx.s_bit_firmware_info[core].reg_base_offset + addr)

#define vpu_write_register(core, addr, val) \
	bm_write32(bmdi, bmdi->vpudrvctx.s_vpu_reg_phy_base[core] + \
			bmdi->vpudrvctx.s_bit_firmware_info[core].reg_base_offset + addr, val)

#define BM1682_VPU_IRQ_NUM_0 (1000)
#define BM1682_VPU_IRQ_NUM_1 (1001)
#define BM1682_VPU_IRQ_NUM_2 (1002)

#define BM1684_VPU_IRQ_NUM_0 (31)
#define BM1684_VPU_IRQ_NUM_1 (32)
#define BM1684_VPU_IRQ_NUM_2 (64 + 51)
#define BM1684_VPU_IRQ_NUM_3 (64 + 52)
#define BM1684_VPU_IRQ_NUM_4 (64 + 0)

#define TOP_PLL_CTRL(fbdiv, p1, p2, refdiv) \
	(((fbdiv & 0xfff) << 16) | ((p2 & 0x7) << 12) | ((p1 & 0x7) << 8) | (refdiv & 0x1f))
#define PLL_STAT_LOCK_OFFSET    0x8

struct bm_pll_ctrl {
	unsigned int fbdiv;
	unsigned int postdiv1;
	unsigned int postdiv2;
	unsigned int refdiv;
};

static u32 s_vpu_irq_1682[] = {
	BM1682_VPU_IRQ_NUM_0,
	BM1682_VPU_IRQ_NUM_1,
	BM1682_VPU_IRQ_NUM_2,
};

static u32 s_vpu_irq_1684[] = {
	BM1684_VPU_IRQ_NUM_0,
	BM1684_VPU_IRQ_NUM_1,
	BM1684_VPU_IRQ_NUM_2,
	BM1684_VPU_IRQ_NUM_3,
	BM1684_VPU_IRQ_NUM_4
};

static u32 s_vpu_reg_phy_base_1682[] = {
	0x50440000,
	0x50450000,
	0x50460000,
};

static u32 s_vpu_reg_phy_base_1684[] = {
	0x50050000,
	0x50060000,
	0x500D0000,
	0x500E0000,
	0x50126000,
};

struct vpu_reset_info {
	u32 reg;
	u32 bit_n;
};

static struct vpu_reset_info bm_vpu_rst[MAX_NUM_VPU_CORE] = {
	{SW_RESET_REG1, 18},
	{SW_RESET_REG1, 19},
	{SW_RESET_REG1, 28},
	{SW_RESET_REG1, 29},
	{SW_RESET_REG2, 10},
};

static struct vpu_reset_info bm_vpu_en_axi_clk[MAX_NUM_VPU_CORE] = {
	{TOP_CLK_EN_REG1, CLK_EN_AXI_VD0_WAVE0_GEN_REG1},
	{TOP_CLK_EN_REG1, CLK_EN_AXI_VD0_WAVE1_GEN_REG1},
	{TOP_CLK_EN_REG2, CLK_EN_AXI_VD1_WAVE0_GEN_REG2},
	{TOP_CLK_EN_REG2, CLK_EN_AXI_VD1_WAVE1_GEN_REG2},
	{TOP_CLK_EN_REG1, CLK_EN_AXI_VDE_WAVE_GEN_REG1},
};

static struct vpu_reset_info bm_vpu_en_apb_clk[MAX_NUM_VPU_CORE] = {
	{TOP_CLK_EN_REG1, CLK_EN_APB_VD0_WAVE0_GEN_REG1},
	{TOP_CLK_EN_REG1, CLK_EN_APB_VD0_WAVE1_GEN_REG1},
	{TOP_CLK_EN_REG2, CLK_EN_APB_VD1_WAVE0_GEN_REG2},
	{TOP_CLK_EN_REG2, CLK_EN_APB_VD1_WAVE1_GEN_REG2},
	{TOP_CLK_EN_REG1, CLK_EN_APB_VDE_WAVE_GEN_REG1},
};

enum init_op {
	FREE,
	INIT,
};

typedef enum {
	INT_WAVE5_INIT_VPU          = 0,
	INT_WAVE5_WAKEUP_VPU        = 1,
	INT_WAVE5_SLEEP_VPU         = 2,

	INT_WAVE5_CREATE_INSTANCE   = 3,
	INT_WAVE5_FLUSH_INSTANCE    = 4,
	INT_WAVE5_DESTORY_INSTANCE  = 5,

	INT_WAVE5_INIT_SEQ          = 6,
	INT_WAVE5_SET_FRAMEBUF      = 7,
	INT_WAVE5_DEC_PIC           = 8,
	INT_WAVE5_ENC_PIC           = 8,
	INT_WAVE5_ENC_SET_PARAM     = 9,
	INT_WAVE5_ENC_LOW_LATENCY   = 13,
	INT_WAVE5_DEC_QUERY         = 14,
	INT_WAVE5_BSBUF_EMPTY       = 15,
	INT_WAVE5_BSBUF_FULL        = 15,
} Wave5InterruptBit;

struct bmdi_list {
	struct bm_device_info *bmdi;
	struct list_head list;
};

static LIST_HEAD(bmdi_list_head);
static int vpu_polling_create;
static int vpu_open_inst_count;
static struct task_struct *irq_task;

static int bm_vpu_reset_core(struct bm_device_info *bmdi, int core_idx, int reset);
static int bm_vpu_hw_reset(struct bm_device_info *bmdi, int core_idx);

static int bm_vpu_clk_unprepare(struct bm_device_info *bmdi, int core_idx)
{
	u32 value = 0;

        if (bmdi->cinfo.chip_id == 0x1684) {
                value = bm_read32(bmdi, bm_vpu_en_apb_clk[core_idx].reg);
                value &= ~(0x1 << bm_vpu_en_apb_clk[core_idx].bit_n);
                bm_write32(bmdi, bm_vpu_en_apb_clk[core_idx].reg, value);
                value = bm_read32(bmdi, bm_vpu_en_apb_clk[core_idx].reg);

                value = bm_read32(bmdi, bm_vpu_en_axi_clk[core_idx].reg);
                value &= ~(0x1 << bm_vpu_en_axi_clk[core_idx].bit_n);
                bm_write32(bmdi, bm_vpu_en_axi_clk[core_idx].reg, value);
                value = bm_read32(bmdi, bm_vpu_en_axi_clk[core_idx].reg);
        }

        return value;
}

static int bm_vpu_clk_prepare(struct bm_device_info *bmdi, int core_idx)
{
	u32 value = 0;

        if (bmdi->cinfo.chip_id == 0x1684) {
                value = bm_read32(bmdi, bm_vpu_en_axi_clk[core_idx].reg);
                value |= (0x1 << bm_vpu_en_axi_clk[core_idx].bit_n);
                bm_write32(bmdi, bm_vpu_en_axi_clk[core_idx].reg, value);
                value = bm_read32(bmdi, bm_vpu_en_axi_clk[core_idx].reg);

                value = bm_read32(bmdi, bm_vpu_en_apb_clk[core_idx].reg);
                value |= (0x1 << bm_vpu_en_apb_clk[core_idx].bit_n);
                bm_write32(bmdi, bm_vpu_en_apb_clk[core_idx].reg, value);
                value = bm_read32(bmdi, bm_vpu_en_apb_clk[core_idx].reg);
        }

        return value;
}

#ifdef VPU_SUPPORT_CLOCK_CONTROL
static void bm_vpu_clk_enable(int op)
{
}
#endif

#define PTHREAD_MUTEX_T_HANDLE_SIZE 4
typedef enum {
	CORE_LOCK,
	CORE_DISPLAY_LOCK,
} LOCK_INDEX;
static int get_lock(struct bm_device_info *bmdi, int core_idx, LOCK_INDEX lock_index)
{
    vpudrv_buffer_t* p = &(bmdi->vpudrvctx.instance_pool[core_idx]);
    int *addr = (int*)(p->base + p->size - PTHREAD_MUTEX_T_HANDLE_SIZE*4);
    int val = 0;
    const int val1 = current->tgid; //in soc, getpid() get the tgid.
    const int val2 = current->pid;
    int ret = 1;
    int count = 0;
	addr += lock_index;

    while((val = __sync_val_compare_and_swap(addr, 0, val1)) != 0){
        if(val == val1 || val==val2) {
            ret = 1;
            break;
        } else {
            val = 0;
        }

        if(count >= 5000) {
            pr_info("can't get lock, org: %d, ker: %d", *addr, val1);
            ret = 0;
            break;
        }
        msleep(2);
        count += 1;
    }

    return ret;
}
static void release_lock(struct bm_device_info *bmdi, int core_idx, LOCK_INDEX lock_index)
{
    vpudrv_buffer_t* p = &(bmdi->vpudrvctx.instance_pool[core_idx]);
    int *addr = (int *)(p->base + p->size - PTHREAD_MUTEX_T_HANDLE_SIZE*4);
	addr += lock_index;
    __sync_lock_release(addr);
}

static int get_vpu_create_inst_flag(struct bm_device_info *bmdi, int core_idx)
{
    vpudrv_buffer_t* p = &(bmdi->vpudrvctx.instance_pool[core_idx]);
    int *addr = (int *)(p->base + p->size - PTHREAD_MUTEX_T_HANDLE_SIZE*4);
    return __atomic_load_n((int *)(addr + 2), __ATOMIC_SEQ_CST);
}
static void release_vpu_create_inst_flag(struct bm_device_info *bmdi, int core_idx, int inst_idx)
{
    vpudrv_buffer_t* p = &(bmdi->vpudrvctx.instance_pool[core_idx]);
    int *addr = (int *)(p->base + p->size - PTHREAD_MUTEX_T_HANDLE_SIZE*4);
    int vpu_create_inst_flag = get_vpu_create_inst_flag(bmdi, core_idx);
    __atomic_store_n((int *)(addr + 2), (vpu_create_inst_flag & (~(1 << inst_idx))), __ATOMIC_SEQ_CST);
}

#ifdef VPU_SUPPORT_CLOSE_COMMAND
int CodaCloseInstanceCommand(struct bm_device_info *bmdi, int core, u32 instanceIndex)
{
	int ret = 0;
	unsigned long timeout = jiffies + HZ; /* vpu wait timeout to 1sec */

	vpu_write_register(core, BIT_BUSY_FLAG, 1);
	vpu_write_register(core, BIT_RUN_INDEX, instanceIndex);
#define DEC_SEQ_END 2
	vpu_write_register(core, BIT_RUN_COMMAND, DEC_SEQ_END);

	while (vpu_read_register(core, BIT_BUSY_FLAG)) {
		if (time_after(jiffies, timeout)) {
			DPRINTK("CodaCloseInstanceCommand after BUSY timeout");
			ret = 1;
			goto DONE_CMD_CODA;
		}
	}
DONE_CMD_CODA:
    DPRINTK("sent CodaCloseInstanceCommand 55 ret=%d", ret);
    return ret;
}
static int WaitBusyTimeout(struct bm_device_info *bmdi, u32 core, u32 addr)
{
    unsigned long timeout = jiffies + HZ; /* vpu wait timeout to 1sec */
    while (vpu_read_register(core, addr)) {
        if (time_after(jiffies, timeout)) {
            return 1;
        }
    }
    return 0;
}
#define W5_CMD_DEC_SET_DISP_IDC             (W5_REG_BASE + 0x0118)
#define W5_CMD_DEC_CLR_DISP_IDC             (W5_REG_BASE + 0x011C)
#define W5_QUERY_OPTION             (W5_REG_BASE + 0x0104)
static void Wave5BitIssueCommandInst(struct bm_device_info *bmdi, u32 core, u32 instanceIndex, u32 cmd)
{
    vpu_write_register(core, W5_CMD_INSTANCE_INFO,  instanceIndex&0xffff);
    vpu_write_register(core, W5_VPU_BUSY_STATUS, 1);
    vpu_write_register(core, W5_COMMAND, cmd);
    vpu_write_register(core, W5_VPU_HOST_INT_REQ, 1);
}

static int SendQuery(struct bm_device_info *bmdi, u32 core, u32 instanceIndex, u32 queryOpt)
{
    // Send QUERY cmd
    vpu_write_register(core, W5_QUERY_OPTION, queryOpt);
    vpu_write_register(core, W5_VPU_BUSY_STATUS, 1);
#define W5_QUERY           0x4000
    Wave5BitIssueCommandInst(bmdi, core, instanceIndex, W5_QUERY);

    if(WaitBusyTimeout(bmdi, core, W5_VPU_BUSY_STATUS)) {
        pr_err("SendQuery BUSY timeout");
        return 2;
    }

    if (vpu_read_register(core, W5_RET_SUCCESS) == 0)
        return 1;

    return 0;
}

static int Wave5DecClrDispFlag(struct bm_device_info *bmdi, u32 core, u32 instanceIndex, u32 index)
{
    int ret = 0;

    vpu_write_register(core, W5_CMD_DEC_CLR_DISP_IDC, (1<<index));
    vpu_write_register(core, W5_CMD_DEC_SET_DISP_IDC, 0);
#define UPDATE_DISP_FLAG     3
    ret = SendQuery(bmdi, core, instanceIndex, UPDATE_DISP_FLAG);

    if (ret != 0) {
        pr_err("Wave5DecClrDispFlag QUERY FAILURE\n");
        return 1;
    }

    return 0;
}
#define W5_BS_OPTION                            (W5_REG_BASE + 0x0120)
static int Wave5VpuDecSetBitstreamFlag(struct bm_device_info *bmdi, u32 core, u32 instanceIndex)
{
    vpu_write_register(core, W5_BS_OPTION, 2);
#define W5_UPDATE_BS       0x8000
    Wave5BitIssueCommandInst(bmdi, core, instanceIndex, W5_UPDATE_BS);
    if(WaitBusyTimeout(bmdi, core, W5_VPU_BUSY_STATUS)) {
        pr_err("Wave5VpuDecSetBitstreamFlag BUSY timeout");
        return 2;
    }

    if (vpu_read_register(core, W5_RET_SUCCESS) == 0) {
        pr_err("Wave5VpuDecSetBitstreamFlag failed.\n");
        return 1;
    }

    return 0;
}
#define W5_CMD_DEC_ADDR_REPORT_BASE         (W5_REG_BASE + 0x0114)
#define W5_CMD_DEC_REPORT_SIZE              (W5_REG_BASE + 0x0118)
#define W5_CMD_DEC_REPORT_PARAM             (W5_REG_BASE + 0x011C)
#define W5_CMD_DEC_GET_RESULT_OPTION        (W5_REG_BASE + 0x0128)

static int FlushDecResult(struct bm_device_info *bmdi, u32 core, u32 instanceIndex)
{
    int     ret = 0;
    u32     regVal;

    vpu_write_register(core, W5_CMD_DEC_ADDR_REPORT_BASE, 0);
    vpu_write_register(core, W5_CMD_DEC_REPORT_SIZE,      0);
    vpu_write_register(core, W5_CMD_DEC_REPORT_PARAM,     31);
    //vpu_write_register(core, W5_CMD_DEC_GET_RESULT_OPTION, 0x1);

#define GET_RESULT 2
    // Send QUERY cmd
    ret = SendQuery(bmdi, core, instanceIndex, GET_RESULT);
    if (ret != 0) {
        regVal = vpu_read_register(core, W5_RET_FAIL_REASON);
        DPRINTK("flush result reason: 0x%x", regVal);
        return 1;
    }

    return 0;
}
int Wave5CloseInstanceCommand(struct bm_device_info *bmdi, int core, u32 instanceIndex)
{
	int ret = 0;
	unsigned long timeout = jiffies + HZ;	/* vpu wait timeout to 1sec */

#define W5_DESTROY_INSTANCE 0x0020
	vpu_write_register(core, W5_CMD_INSTANCE_INFO,  (instanceIndex&0xffff));
	vpu_write_register(core, W5_VPU_BUSY_STATUS, 1);
	vpu_write_register(core, W5_COMMAND, W5_DESTROY_INSTANCE);

	vpu_write_register(core, W5_VPU_HOST_INT_REQ, 1);

	while (vpu_read_register(core, W5_VPU_BUSY_STATUS)) {
		if (time_after(jiffies, timeout)) {
			pr_info("Wave5CloseInstanceCommand after BUSY timeout\n");
			ret = 1;
			goto DONE_CMD;
		}
	}

	if (vpu_read_register(core, W5_RET_SUCCESS) == 0) {
		// pr_info("Wave5CloseInstanceCommand failed REASON=[0x%x]\n", vpu_read_register(core, W5_RET_FAIL_REASON));
#define WAVE5_VPU_STILL_RUNNING 0x00001000
		if (vpu_read_register(core, W5_RET_FAIL_REASON) == WAVE5_VPU_STILL_RUNNING)
			ret = 2;
		else
			ret = 1;
		goto DONE_CMD;
	}
	ret = 0;
DONE_CMD:
	DPRINTK("sent Wave5CloseInstanceCommandt 55 ret=%d", ret);
	return ret;
}

int CloseInstanceCommand(struct bm_device_info *bmdi, int core, u32 instanceIndex)
{
	int product_code;
	product_code = vpu_read_register(core, VPU_PRODUCT_CODE_REGISTER);
	if (PRODUCT_CODE_W_SERIES(product_code)) {
		if(WAVE521C_CODE != product_code) {
			u32 i =0;
			u32 interrupt_flag_in_q = 0;
			Wave5VpuDecSetBitstreamFlag(bmdi, core, instanceIndex);
			interrupt_flag_in_q = kfifo_out_spinlocked(&bmdi->vpudrvctx.interrupt_pending_q[core][instanceIndex],
							&i, sizeof(u32), &bmdi->vpudrvctx.s_kfifo_lock[core][instanceIndex]);
			if (interrupt_flag_in_q > 0) {
				//FlushDecResult(bmdi, core, instanceIndex);
				DPRINTK("interrupt flag : %d\n", interrupt_flag_in_q);
			}
			FlushDecResult(bmdi, core, instanceIndex);
			for(i=0; i<62; i++) {
			int ret = Wave5DecClrDispFlag(bmdi, core, instanceIndex, i);
				if(ret != 0)
					break;
			}
		}
		return Wave5CloseInstanceCommand(bmdi, core, instanceIndex);
	}
	else {
		return CodaCloseInstanceCommand(bmdi, core, instanceIndex);
	}
}
#endif

static int bm_vpu_alloc_dma_buffer(struct bm_device_info *bmdi, vpudrv_buffer_t *vb)
{
	if (!vb)
		return -1;

	if (bmdi->cinfo.chip_id == 0x1684) {
		vb->phys_addr = (unsigned long)vmem_alloc(&bmdi->vpudrvctx.s_vmem, vb->size, 0);
		if (vb->phys_addr  == (unsigned long)-1) {
			DPRINTK(KERN_ERR "[VPUDRV] Physical memory allocation error size=%d\n", vb->size);
			return -1;
		}
	} else if (bmdi->cinfo.chip_id == 0x1682) {
		if (vb->core_idx < 2) {
			vb->phys_addr = vmem_alloc(&bmdi->vpudrvctx.s_vmemboda, vb->size, 0);
		} else{
			vb->phys_addr = vmem_alloc(&bmdi->vpudrvctx.s_vmemwave, vb->size, 0);
		}
		if (vb->phys_addr  == (unsigned long)-1) {
			DPRINTK(KERN_ERR "[VPUDRV] Physical memory allocation error size=%d\n", vb->size);
			return -1;
		}
	}

	vb->base = (unsigned long)(bmdi->vpudrvctx.s_video_memory.base + (vb->phys_addr - bmdi->vpudrvctx.s_video_memory.phys_addr));
	return 0;
}

static void bm_vpu_free_dma_buffer(struct vpudrv_buffer_t *vb, video_mm_t *s_vmem)
{
	if (!vb)
		return;
	if (vb->base)
		vmem_free(s_vmem, vb->phys_addr, 0);
}

static int bm_vpu_free_instances(struct file *filp)
{
	struct bm_device_info *bmdi = (struct bm_device_info *)filp->private_data;
	vpudrv_instanace_list_t *vil, *n;
	vpudrv_instance_pool_t *vip;
	void *vip_base;
	int ret = 0;

	DPRINTK("[VPUDRV] bm_vpu_free_instances\n");

	list_for_each_entry_safe(vil, n, &bmdi->vpudrvctx.s_inst_list_head, list) {
		if (vil->filp == filp) {
			vip_base = (void *)(bmdi->vpudrvctx.instance_pool[vil->core_idx].base);
			DPRINTK("[VPUDRV] %s detect instance crash instIdx=%d,\
					coreIdx=%d, vip_base=%p, instance_pool_size_per_core=%d\n",\
					__func__, (int)vil->inst_idx, (int)vil->core_idx, vip_base,
					bmdi->vpudrvctx.instance_pool[vil->core_idx].size);
			vip = (vpudrv_instance_pool_t *)vip_base;
			if (vip) {
				if (vip->pendingInstIdxPlus1 - 1 == vil->inst_idx)
					vip->pendingInstIdxPlus1 = 0;
				/* only first 4 byte is key point(inUse of CodecInst in vpuapi) to free the corresponding instance. */
				memset(&vip->codecInstPool[vil->inst_idx], 0x00, 4);
			}
			if (ret == 0) {
				ret = vil->core_idx + 1;
			} else if (ret != vil->core_idx + 1) {
				pr_err("please check the release core index: %ld ....\n", vil->core_idx);
			}
			vip->vpu_instance_num -= 1;
			bmdi->vpudrvctx.s_vpu_open_ref_count[vil->core_idx]--;
			list_del(&vil->list);
			kfree(vil);
			if (bmdi->cinfo.chip_id == 0x1682)
				if (vpu_open_inst_count > 0)
					vpu_open_inst_count--;
		}
	}

	return ret;
}

static int bm_vpu_free_buffers(struct file *filp)
{
	vpudrv_buffer_pool_t *pool, *n;
	vpudrv_buffer_t vb;
	struct bm_device_info *bmdi = NULL;
	vpu_drv_context_t *dev = NULL;

	DPRINTK("[VPUDRV] bm_vpu_free_buffers\n");
	bmdi = (struct bm_device_info *)filp->private_data;
	dev = &bmdi->vpudrvctx;
	list_for_each_entry_safe(pool, n, &bmdi->vpudrvctx.s_vbp_head, list) {
		if (pool->filp == filp) {
			vb = pool->vb;
			if (vb.base) {
				if (bmdi->cinfo.chip_id == 0x1684) {
					bm_vpu_free_dma_buffer(&vb, &dev->s_vmem);
				} else {
					if (vb.phys_addr >= dev->s_vmemwave.base_addr) {
						bm_vpu_free_dma_buffer(&vb, &dev->s_vmemwave);
					} else {
						bm_vpu_free_dma_buffer(&vb, &dev->s_vmemboda);
					}

				}
				list_del(&pool->list);
				kfree(pool);
			}
		}
	}
	return 0;
}

int bm_vpu_open(struct inode *inode, struct file *filp)
{
	struct bm_device_info *bmdi = (struct bm_device_info *)filp->private_data;
	int ret;
	DPRINTK("[VPUDRV][+] %s\n", __func__);

	//if (!bmdi->vpudrvctx.open_count)
	// bm_vpu_deassert(&bmdi->vpudrvctx.vpu_rst_ctrl);
	if ((ret =  mutex_lock_interruptible(&bmdi->vpudrvctx.s_vpu_lock)) == 0) {
#if 0
		if (bmdi->vpudrvctx.open_count == 0) {
			int i;
			for (i = 0; i < MAX_NUM_VPU_CORE; i++)
				bm_vpu_hw_reset(bmdi, i);
		}
#endif
		bmdi->vpudrvctx.open_count++;
		mutex_unlock(&bmdi->vpudrvctx.s_vpu_lock);
	}

	DPRINTK("[VPUDRV][-] %s\n", __func__);

	return 0;
}

static inline int get_core_idx(struct file *filp)
{
	int core_idx = -1;
	vpu_core_idx_context *pool, *n;
	struct bm_device_info *bmdi = NULL;
	vpu_drv_context_t *dev = NULL;

	bmdi = (struct bm_device_info *)filp->private_data;
	dev = &bmdi->vpudrvctx;
	list_for_each_entry_safe(pool, n, &bmdi->vpudrvctx.s_core_idx_head, list) {
		if (pool->filp == filp) {
			if (core_idx != -1)
				pr_err("maybe error in get core index, please check.\n");
			core_idx = pool->core_idx;

			list_del(&pool->list);
			vfree(pool);
			//break;
		}
	}
	return core_idx;
}

static inline u32 get_inst_idx(vpu_drv_context_t *dev, u32 reg_val)
{
	u32 inst_idx;
	int i;

	for (i = 0; i < dev->max_num_instance; i++) {
		if (((reg_val >> i) & 0x01) == 1)
			break;
	}
	inst_idx = i;
	return inst_idx;
}

static u32 bm_get_vpu_inst_idx(vpu_drv_context_t *dev, u32 *reason, u32 empty_inst, u32 done_inst, u32 other_inst)
{
	u32 inst_idx;
	u32 reg_val;
	u32 int_reason;
	int_reason = *reason;

	DPRINTK("[VPUDRV][+]%s, int_reason=0x%x, empty_inst=0x%x, done_inst=0x%x\n",\
			__func__, int_reason, empty_inst, done_inst);

	if (int_reason & (1 << INT_WAVE5_BSBUF_EMPTY)) {
#if defined(CHIP_BM1682)
		reg_val = (empty_inst & 0xffff); // at most 16 instances
#else
		reg_val = empty_inst;
#endif
		inst_idx = get_inst_idx(dev, reg_val);
		*reason = (1 << INT_WAVE5_BSBUF_EMPTY);
		DPRINTK("[VPUDRV] %s, W5_RET_BS_EMPTY_INST reg_val=0x%x, inst_idx=%d\n",\
				__func__, reg_val, inst_idx);
		goto GET_VPU_INST_IDX_HANDLED;
	}

	if (int_reason & (1 << INT_WAVE5_INIT_SEQ)) {
#if defined(CHIP_BM1682)
		reg_val = (done_inst & 0xffff); // at most 16 instances
#else
		reg_val = done_inst;
#endif
		inst_idx = get_inst_idx(dev, reg_val);
		*reason  = (1 << INT_WAVE5_INIT_SEQ);
		DPRINTK("[VPUDRV] %s, W5_RET_QUEUE_CMD_DONE_INST INIT_SEQ reg_val=0x%x, inst_idx=%d\n",\
				__func__, reg_val, inst_idx);
		goto GET_VPU_INST_IDX_HANDLED;
	}

	if (int_reason & (1 << INT_WAVE5_DEC_PIC)) {
#if defined(CHIP_BM1682)
		reg_val = (done_inst & 0xffff); // at most 16 instances
#else
		reg_val = done_inst;
#endif
		inst_idx = get_inst_idx(dev, reg_val);
		*reason  = (1 << INT_WAVE5_DEC_PIC);
		DPRINTK("[VPUDRV] %s, W5_RET_QUEUE_CMD_DONE_INST DEC_PIC reg_val=0x%x, inst_idx=%d\n",\
				__func__, reg_val, inst_idx);

#if defined(CHIP_BM1682)
		if (int_reason & (1 << INT_WAVE5_ENC_LOW_LATENCY)) {
			u32 ll_inst_idx;
			reg_val = (done_inst >> 16);
			ll_inst_idx = get_inst_idx(dev, reg_val); // ll_inst_idx < 16
			if (ll_inst_idx == inst_idx)
				*reason = ((1 << INT_WAVE5_DEC_PIC) | (1 << INT_WAVE5_ENC_LOW_LATENCY));
			DPRINTK("[VPUDRV] %s, W5_RET_QUEUE_CMD_DONE_INST DEC_PIC and ENC_LOW_LATENCY reg_val=0x%x,\
					inst_idx=%d, ll_inst_idx=%d\n", __func__, reg_val, inst_idx, ll_inst_idx);
		}
#endif
		goto GET_VPU_INST_IDX_HANDLED;
	}

	if (int_reason & (1 << INT_WAVE5_ENC_SET_PARAM)) {
#if defined(CHIP_BM1682)
		reg_val = (done_inst & 0xffff); // at most 16 instances
#else
		reg_val = done_inst;
#endif
		inst_idx = get_inst_idx(dev, reg_val);
		*reason  = (1 << INT_WAVE5_ENC_SET_PARAM);
		DPRINTK("[VPUDRV] %s, W5_RET_QUEUE_CMD_DONE_INST ENC_SET_PARAM reg_val=0x%x, inst_idx=%d\n",\
				__func__, reg_val, inst_idx);
		goto GET_VPU_INST_IDX_HANDLED;
	}

#if defined(CHIP_BM1682)
	if (int_reason & (1 << INT_WAVE5_ENC_LOW_LATENCY)) {
		reg_val = (done_inst >> 16); // 16 instances at most
		inst_idx = get_inst_idx(dev, reg_val);
		*reason  = (1 << INT_WAVE5_ENC_LOW_LATENCY);
		DPRINTK("[VPUDRV] %s, W5_RET_QUEUE_CMD_DONE_INST ENC_LOW_LATENCY reg_val=0x%x, inst_idx=%d\n",\
				__func__, reg_val, inst_idx);
		goto GET_VPU_INST_IDX_HANDLED;
	}
#endif

	//if interrupt is not for empty and dec_pic and init_seq.
	reg_val = (other_inst & 0xFF);
	inst_idx = reg_val; // at most 256 instances
	*reason  = int_reason;
	DPRINTK("[VPUDRV] %s, W5_RET_DONE_INSTANCE_INFO reg_val=0x%x, inst_idx=%d\n",\
			__func__, reg_val, inst_idx);

GET_VPU_INST_IDX_HANDLED:

	DPRINTK("[VPUDRV][-]%s, inst_idx=%d. *reason=0x%x\n", __func__, inst_idx, *reason);

	return inst_idx;
}

static irqreturn_t bm_vpu_irq_handler(struct bm_device_info *bmdi)
{
	vpu_drv_context_t *dev = &bmdi->vpudrvctx;

	/* this can be removed. it also work in VPU_WaitInterrupt of API function */
	int core;
	int product_code;
	u32 intr_reason;
	u32 intr_inst_index;

#ifdef VPU_IRQ_CONTROL
	disable_irq_nosync(bmdi->cinfo.irq_id);
#endif

	intr_inst_index = 0;
	intr_reason = 0;

#ifdef VPU_SUPPORT_ISR
	for (core = 0; core < dev->max_num_vpu_core; core++) {
		if (bmdi->vpudrvctx.s_vpu_irq[core] == bmdi->cinfo.irq_id)
			break;
	}
#endif

	DPRINTK("[VPUDRV][+]%s, core_idx:%d get irq: %d\n", __func__, core, bmdi->cinfo.irq_id);
	if (core >= dev->max_num_vpu_core) {
		DPRINTK(KERN_ERR "[VPUDRV] : can't find the core index in irq_process\n");
		return IRQ_HANDLED;
	}

	if (bmdi->vpudrvctx.s_bit_firmware_info[core].size == 0) {
		/* it means that we didn't get an information the current core from API layer. No core activated.*/
		DPRINTK(KERN_ERR "[VPUDRV] : bmdi->vpudrvctx.s_bit_firmware_info[core].size is zero\n");
		return IRQ_HANDLED;
	}

	product_code = vpu_read_register(core, VPU_PRODUCT_CODE_REGISTER);

	if (PRODUCT_CODE_W_SERIES(product_code)) {
		if (vpu_read_register(core, W5_VPU_VPU_INT_STS)) {
			u32 empty_inst;
			u32 done_inst;
			u32 other_inst;

			intr_reason = vpu_read_register(core, W5_VPU_INT_REASON);
			empty_inst = vpu_read_register(core, W5_RET_BS_EMPTY_INST);
			done_inst = vpu_read_register(core, W5_RET_QUEUE_CMD_DONE_INST);
			other_inst = vpu_read_register(core, W5_RET_DONE_INSTANCE_INFO);

			DPRINTK("[VPUDRV] vpu_irq_handler reason=0x%x, empty_inst=0x%x,done_inst=0x%x, other_inst=0x%x\n",\
					intr_reason, empty_inst, done_inst, other_inst);

			intr_inst_index = bm_get_vpu_inst_idx(dev, &intr_reason, empty_inst, done_inst, other_inst);
			if (intr_inst_index >= 0 && intr_inst_index < bmdi->vpudrvctx.max_num_instance) {
				if (intr_reason == (1 << INT_WAVE5_BSBUF_EMPTY)) {
					empty_inst = empty_inst & ~(1 << intr_inst_index);
					vpu_write_register(core, W5_RET_BS_EMPTY_INST, empty_inst);
					DPRINTK("[VPUDRV] %s, W5_RET_BS_EMPTY_INST Clear empty_inst=0x%x, intr_inst_index=%d\n",\
							__func__, empty_inst, intr_inst_index);
				}

				if ((intr_reason == (1 << INT_WAVE5_DEC_PIC)) ||
						(intr_reason == (1 << INT_WAVE5_INIT_SEQ)) ||
						(intr_reason == (1 << INT_WAVE5_ENC_SET_PARAM))) {
					done_inst = done_inst & ~(1 << intr_inst_index);
					vpu_write_register(core, W5_RET_QUEUE_CMD_DONE_INST, done_inst);
					DPRINTK("[VPUDRV] %s, W5_RET_QUEUE_CMD_DONE_INST Clear done_inst=0x%x,intr_inst_index=%d\n",\
							__func__, done_inst, intr_inst_index);
				}

				if ((intr_reason == (1 << INT_WAVE5_ENC_LOW_LATENCY))) {
					done_inst = (done_inst >> 16);
					done_inst = done_inst & ~(1 << intr_inst_index);
					done_inst = (done_inst << 16);
					vpu_write_register(core, W5_RET_QUEUE_CMD_DONE_INST, done_inst);
					DPRINTK("[VPUDRV] W5_RET_QUEUE_CMD_DONE_INST INT_WAVE5_ENC_LOW_LATENCY Clear done\n");
					DPRINTK("inst=0x%x, intr_inst_index=%d\n", done_inst, intr_inst_index);
				}

				if (!kfifo_is_full(&bmdi->vpudrvctx.interrupt_pending_q[core][intr_inst_index])) {
					if (intr_reason == ((1 << INT_WAVE5_DEC_PIC) | (1 << INT_WAVE5_ENC_LOW_LATENCY))) {
						u32 ll_intr_reason = (1 << INT_WAVE5_DEC_PIC);
						unsigned long flags = 0;
						spin_lock_irqsave(&bmdi->vpudrvctx.s_kfifo_lock[core][intr_inst_index], flags);
						kfifo_in(&bmdi->vpudrvctx.interrupt_pending_q[core][intr_inst_index], &ll_intr_reason, sizeof(u32));
						spin_unlock_irqrestore(&bmdi->vpudrvctx.s_kfifo_lock[core][intr_inst_index], flags);
					} else {
						unsigned long flags = 0;
						spin_lock_irqsave(&bmdi->vpudrvctx.s_kfifo_lock[core][intr_inst_index], flags);
						kfifo_in(&bmdi->vpudrvctx.interrupt_pending_q[core][intr_inst_index], &intr_reason, sizeof(u32));
						spin_unlock_irqrestore(&bmdi->vpudrvctx.s_kfifo_lock[core][intr_inst_index], flags);
					}
				} else {
					DPRINTK("[VPUDRV] : kfifo_is_full kfifo_count=%d \n",
							kfifo_len(&bmdi->vpudrvctx.interrupt_pending_q[core][intr_inst_index]));
				}
			} else {
				DPRINTK("[VPUDRV] : intr_inst_index is wrong intr_inst_index=%d \n", intr_inst_index);
			}

			vpu_write_register(core, W5_VPU_INT_REASON_CLEAR, intr_reason);
			vpu_write_register(core, W5_VPU_VINT_CLEAR, 0x1);
		}
	} else if (PRODUCT_CODE_NOT_W_SERIES(product_code)) {
		if (vpu_read_register(core, BIT_INT_STS)) {
			unsigned long flags = 0;
			intr_reason = vpu_read_register(core, BIT_INT_REASON);
			intr_inst_index = 0; /* in case of coda seriese. treats intr_inst_index is already 0 */
			spin_lock_irqsave(&bmdi->vpudrvctx.s_kfifo_lock[core][intr_inst_index], flags);
			kfifo_in(&bmdi->vpudrvctx.interrupt_pending_q[core][intr_inst_index], &intr_reason, sizeof(u32));
			spin_unlock_irqrestore(&bmdi->vpudrvctx.s_kfifo_lock[core][intr_inst_index], flags);
			vpu_write_register(core, BIT_INT_CLEAR, 0x1);
		}
	} else {
		DPRINTK("[VPUDRV] Unknown product id : %08x\n", product_code);
		return IRQ_HANDLED;
	}
	DPRINTK("[VPUDRV] product: 0x%08x\n", product_code);

	if (dev->async_queue)
		kill_fasync(&dev->async_queue, SIGIO, POLL_IN); /* notify the interrupt to user space */
	if (intr_inst_index >= 0 && intr_inst_index < bmdi->vpudrvctx.max_num_instance) {
		bmdi->vpudrvctx.interrupt_flag[core][intr_inst_index] = 1;
		wake_up_interruptible(&bmdi->vpudrvctx.interrupt_wait_q[core][intr_inst_index]);
	}

	DPRINTK("[VPUDRV][-]%s\n", __func__);

	return IRQ_HANDLED;
}

static int polling_interrupt_work(void *data)
{
	unsigned int regval = 0;
	int product_code, i;
	struct bm_device_info *bmdi = NULL;
	struct bmdi_list *pos, *n;

	while (!kthread_should_stop()) {
		list_for_each_entry_safe(pos, n, &bmdi_list_head, list) {
			if (pos->bmdi) {
				bmdi = pos->bmdi;
				for (i = 0; i < bmdi->vpudrvctx.max_num_vpu_core; i++) {
					product_code = vpu_read_register(i, VPU_PRODUCT_CODE_REGISTER);
					if (PRODUCT_CODE_W_SERIES(product_code))
						regval = vpu_read_register(i, W5_VPU_VPU_INT_STS);
					else
						regval = vpu_read_register(i, BIT_INT_STS);
					if (regval == (unsigned int)-1) {
						schedule_timeout(HZ);
						continue;
					}

					if (regval) {
						bmdi->cinfo.irq_id = bmdi->vpudrvctx.s_vpu_irq[i];
						bm_vpu_irq_handler(bmdi);
						regval = 0;
					}

					schedule_timeout(0);
				}
			}

		}
	}
	return 0;
}

void create_irq_poll_thread(void)
{
	irq_task = kthread_run(polling_interrupt_work, NULL, "IrqPoll");
	if (IS_ERR(irq_task))
		DPRINTK(KERN_ERR "[VPUDRV] : %s failed!\n", __func__);
	else
		DPRINTK("[VPUDRV] : %s sucess!\n", __func__);
}

void destory_irq_poll_thread(void)
{
	if (irq_task) {
		kthread_stop(irq_task);
		irq_task = NULL;
	}
}

static int _vpu_syscxt_chkinst(vpu_crst_context_t *p_crst_cxt)
{
	int i = 0;

	for (; i < MAX_NUM_INSTANCE_VPU; i++) {
		int pos = p_crst_cxt->count % 2;
		pos = (p_crst_cxt->reset ? pos : 1 - pos) * 8;
		if (((p_crst_cxt->instcall[i] >> pos) & 3) != 0)
		    return -1;
	}

	return 1;
}

static int _vpu_syscxt_printinst(int core_idx, vpu_crst_context_t *p_crst_cxt)
{
	int i = 0;

	DPRINTK("+*********************+\n");
	for (; i < MAX_NUM_INSTANCE_VPU; i++) {
		DPRINTK("core %d, inst %d, map:0x%x.\n",
				core_idx, i, p_crst_cxt->instcall[i]);
	}
	DPRINTK("-*********************-\n\n");
	return 1;
}


static int _vpu_syscxt_chkready(int core_idx, vpu_crst_context_t *p_crst_cxt)
{
	int num = 0, i = 0;
	for(; i < MAX_NUM_INSTANCE_VPU; i++) {
		int pos = (p_crst_cxt->count % 2) * 8;
		if (((p_crst_cxt->instcall[i] >> pos) & 3) != 0)
			num += 1;
	}

	return num;
}

long bm_vpu_ioctl(struct file *filp, u_int cmd, u_long arg)
{
	int ret = 0;
	struct bm_device_info *bmdi = (struct bm_device_info *)filp->private_data;
	vpu_drv_context_t *dev = &bmdi->vpudrvctx;

	switch (cmd) {
		case VDI_IOCTL_SYSCXT_SET_EN:
			{
				vpudrv_syscxt_info_t syscxt_info;
				int i;
				vpu_crst_context_t *p_crst_cxt;

				ret = copy_from_user(&syscxt_info, (vpudrv_syscxt_info_t *)arg, sizeof(vpudrv_syscxt_info_t));
				if (ret) {
					return -EFAULT;
				}

				mutex_lock(&dev->s_vpu_lock);
				for (i = 0; i < MAX_NUM_VPU_CORE; i++) {
					p_crst_cxt = &dev->crst_cxt[i];
					p_crst_cxt->disable = !syscxt_info.fun_en;
				}
				mutex_unlock(&dev->s_vpu_lock);
				pr_info("[VPUDRV] crst set enable:%d.\n", syscxt_info.fun_en);
			}

		case VDI_IOCTL_SYSCXT_SET_STATUS:
			{
				vpudrv_syscxt_info_t syscxt_info;
				int core_idx;
				vpu_crst_context_t *p_crst_cxt;

				ret = copy_from_user(&syscxt_info, (vpudrv_syscxt_info_t *)arg, sizeof(vpudrv_syscxt_info_t));
				if (ret) {
					return -EFAULT;
				}

				mutex_lock(&dev->s_vpu_lock);
				core_idx = syscxt_info.core_idx;
				p_crst_cxt = &dev->crst_cxt[core_idx];
				if ((_vpu_syscxt_chkready(core_idx, p_crst_cxt) != dev->s_vpu_open_ref_count[core_idx]) && (syscxt_info.core_status)) {
				    pr_info("[VPUDRV] core %d not ready! status: %d\n", core_idx, p_crst_cxt->status);
				    mutex_unlock(&dev->s_vpu_lock);
				    break;
				}
				p_crst_cxt->status = p_crst_cxt->disable ? SYSCXT_STATUS_WORKDING : syscxt_info.core_status;
				mutex_unlock(&dev->s_vpu_lock);

				if (p_crst_cxt->status) {
					pr_info("[VPUDRV] core %d set status:%d. |********************|\n",
							core_idx, p_crst_cxt->status);

				} else {
					pr_info("[VPUDRV] core %d set status:%d.\n",
							core_idx, p_crst_cxt->status);
				}

				break;
			}
		case VDI_IOCTL_SYSCXT_GET_STATUS:
			{
				vpudrv_syscxt_info_t syscxt_info;
				int core_idx;
				vpu_crst_context_t *p_crst_cxt;

				ret = copy_from_user(&syscxt_info, (vpudrv_syscxt_info_t *)arg, sizeof(vpudrv_syscxt_info_t));
				if (ret) {
					return -EFAULT;
				}

				mutex_lock(&dev->s_vpu_lock);
				core_idx = syscxt_info.core_idx;
				p_crst_cxt = &dev->crst_cxt[core_idx];
				syscxt_info.core_status = p_crst_cxt->status;
				mutex_unlock(&dev->s_vpu_lock);

				ret = copy_to_user((void __user *)arg, &syscxt_info, sizeof(vpudrv_syscxt_info_t));
				if (ret != 0) {
					return -EFAULT;
				}
				break;
			}
		case VDI_IOCTL_SYSCXT_CHK_STATUS:
			{
				vpudrv_syscxt_info_t syscxt_info;
				int core_idx;
				int instid, shift, mask;
				vpu_crst_context_t *p_crst_cxt;

				ret = copy_from_user(&syscxt_info, (vpudrv_syscxt_info_t *)arg, sizeof(vpudrv_syscxt_info_t));
				if (ret) {
					return -EFAULT;
				}

				mutex_lock(&dev->s_vpu_lock);
				core_idx = syscxt_info.core_idx;
				instid = syscxt_info.inst_idx;
				p_crst_cxt = &dev->crst_cxt[core_idx];
				shift = (p_crst_cxt->count % 2) * 8;
				mask = (1u << syscxt_info.pos) << shift;
				if ((SYSCXT_STATUS_WORKDING == p_crst_cxt->status) && (syscxt_info.is_sleep < 0)) {
					int __num = _vpu_syscxt_chkready(core_idx, p_crst_cxt);
					if ((p_crst_cxt->instcall[instid] & mask) == 0) {
						pr_info("[VPUDRV] core %d instant %d mask:0x%x. pos %d, reset: %d, inst num: %d, chk num:%d\n",
							core_idx, instid, mask, p_crst_cxt->count%2, p_crst_cxt->reset, dev->s_vpu_open_ref_count[core_idx], __num);
					}

					p_crst_cxt->instcall[instid] |= mask;
					syscxt_info.is_all_block = 0;
					syscxt_info.is_sleep = 0;
					p_crst_cxt->reset = __num == dev->s_vpu_open_ref_count[core_idx] ? 1 : p_crst_cxt->reset;
				} else {
					p_crst_cxt->instcall[instid] &= ~mask;
					syscxt_info.is_sleep = 1;
					syscxt_info.is_all_block = 0;
					if (_vpu_syscxt_chkinst(p_crst_cxt) == 1) {
						syscxt_info.is_all_block = 1;

						if (p_crst_cxt->reset == 1) {
							bm_vpu_hw_reset(bmdi, core_idx);
							p_crst_cxt->reset = 0;
							p_crst_cxt->count += 1;
						}
						pr_info("[VPUDRV] core %d instant %d will re-start. pos %d\n", core_idx, instid, 1-(p_crst_cxt->count%2));
						_vpu_syscxt_printinst(core_idx, p_crst_cxt);
					}

					DPRINTK("[VPUDRV] core: %d, inst: %d, pos:%d, instcall %d, status: %d, sleep:%d, wakeup:%d.\n",
							core_idx, instid, syscxt_info.pos, p_crst_cxt->instcall[instid],
							p_crst_cxt->status, syscxt_info.is_sleep, syscxt_info.is_all_block);
				}
				mutex_unlock(&dev->s_vpu_lock);

				ret = copy_to_user((void __user *)arg, &syscxt_info, sizeof(vpudrv_syscxt_info_t));
				if (ret != 0) {
					return -EFAULT;
				}
				break;
			}
		case VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY:
			{
				vpudrv_buffer_pool_t *vbp;

				DPRINTK("[VPUDRV][+]VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY\n");
				if ((ret = down_killable(&dev->s_vpu_sem)) == 0) {
					vbp = kzalloc(sizeof(*vbp), GFP_KERNEL);
					if (!vbp) {
						up(&dev->s_vpu_sem);
						return -ENOMEM;
					}

					ret = copy_from_user(&(vbp->vb), (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
					if (ret) {
						kfree(vbp);
						up(&dev->s_vpu_sem);
						return -EFAULT;
					}

					ret = bm_vpu_alloc_dma_buffer(bmdi, &(vbp->vb));
					if (ret == -1) {
						ret = -ENOMEM;
						kfree(vbp);
						up(&dev->s_vpu_sem);
						break;
					}

					ret = copy_to_user((void __user *)arg, &(vbp->vb), sizeof(vpudrv_buffer_t));
					if (ret) {
						kfree(vbp);
						ret = -EFAULT;
						up(&dev->s_vpu_sem);
						break;
					}

					vbp->filp = filp;
					list_add(&vbp->list, &bmdi->vpudrvctx.s_vbp_head);

					up(&dev->s_vpu_sem);
				}

				DPRINTK("[VPUDRV][-]VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY\n");
			}
			break;

		case VDI_IOCTL_FREE_PHYSICALMEMORY:
			{
				vpudrv_buffer_pool_t *vbp, *n;
				vpudrv_buffer_t vb;

				DPRINTK("[VPUDRV][+]VDI_IOCTL_FREE_PHYSICALMEMORY\n");
				if ((ret = down_interruptible(&dev->s_vpu_sem)) == 0) {
					ret = copy_from_user(&vb, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
					if (ret) {
						up(&dev->s_vpu_sem);
						return -EACCES;
					}

					if (vb.base) {
						if (bmdi->cinfo.chip_id == 0x1684) {
							bm_vpu_free_dma_buffer(&vb, &bmdi->vpudrvctx.s_vmem);
						} else {
							if (vb.base >= dev->s_vmemwave.base_addr) {
								bm_vpu_free_dma_buffer(&vb, &dev->s_vmemwave);
							} else{
								bm_vpu_free_dma_buffer(&vb, &dev->s_vmemboda);
							}

						}
					}
					list_for_each_entry_safe(vbp, n, &bmdi->vpudrvctx.s_vbp_head, list) {
						if (vbp->vb.base == vb.base) {
							list_del(&vbp->list);
							kfree(vbp);
							break;
						}
					}
					up(&dev->s_vpu_sem);
				} else {
					return -ERESTARTSYS;
				}

				DPRINTK("[VPUDRV][-]VDI_IOCTL_FREE_PHYSICALMEMORY\n");
			}
			break;

		case VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO:
			{
				DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO\n");
				if (bmdi->vpudrvctx.s_video_memory.base != 0) {
					ret = copy_to_user((void __user *)arg, &bmdi->vpudrvctx.s_video_memory, sizeof(vpudrv_buffer_t));
					if (ret != 0)
						ret = -EFAULT;
				} else {
					ret = -EFAULT;
				}
				DPRINTK("[VPUDRV][-]VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO\n");
			}
			break;

		case VDI_IOCTL_GET_FREE_MEM_SIZE:
			{
				vmem_info_t vinfo = {0};
				unsigned long size;

				DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_FREE_MEM_SIZE\n");
				vmem_get_info(&bmdi->vpudrvctx.s_vmem, &vinfo);
				size = vinfo.free_pages * vinfo.page_size;
				ret = copy_to_user((void __user *)arg, &size, sizeof(unsigned long));
				if (ret != 0)
					ret = -EFAULT;

				DPRINTK("[VPUDRV][-]VDI_IOCTL_GET_FREE_MEM_SIZE\n");
			}
			break;

		case VDI_IOCTL_WAIT_INTERRUPT:
			{
#ifdef SUPPORT_TIMEOUT_RESOLUTION
				ktime_t kt;
#endif
				vpudrv_intr_info_t info;
				u32 intr_inst_index;
				u32 intr_reason_in_q;
				u32 interrupt_flag_in_q;
				u32 core_idx;

				DPRINTK("[VPUDRV][+]VDI_IOCTL_WAIT_INTERRUPT\n");
				ret = copy_from_user(&info, (vpudrv_intr_info_t *)arg, sizeof(vpudrv_intr_info_t));
				if (ret != 0)
					return -EFAULT;

				intr_inst_index = info.intr_inst_index;
				core_idx = info.core_idx;

				if (bmdi->cinfo.chip_id == 0x1682)
					core_idx %= bmdi->vpudrvctx.max_num_vpu_core;

				DPRINTK("[VPUDRV]--core index: %d\n", core_idx);
				intr_reason_in_q = 0;
				interrupt_flag_in_q = kfifo_out_spinlocked(&bmdi->vpudrvctx.interrupt_pending_q[core_idx][intr_inst_index],
							&intr_reason_in_q, sizeof(u32), &bmdi->vpudrvctx.s_kfifo_lock[core_idx][intr_inst_index]);
				if (interrupt_flag_in_q > 0) {
					dev->interrupt_reason[core_idx][intr_inst_index] = intr_reason_in_q;
					DPRINTK("[VPUDRV] Interrupt Remain : intr_inst_index=%d, intr_reason_in_q=0x%x, interrupt_flag_in_q=%d\n",
							intr_inst_index, intr_reason_in_q, interrupt_flag_in_q);
					goto INTERRUPT_REMAIN_IN_QUEUE;
				}

#ifdef SUPPORT_TIMEOUT_RESOLUTION
				kt =  ktime_set(0, info.timeout * 1000 * 1000);
				ret = wait_event_interruptible_hrtimeout(bmdi->vpudrvctx.interrupt_wait_q[core_idx][intr_inst_index],
						bmdi->vpudrvctx.interrupt_flag[core_idx][intr_inst_index] != 0, kt);
				if (ret == -ETIME) {
					DPRINTK("[VPUDRV][-]VDI_IOCTL_WAIT_INTERRUPT timeout = %d\n", info.timeout);
					break;
				}

#else
				ret = wait_event_interruptible_timeout(bmdi->vpudrvctx.interrupt_wait_q[core_idx][intr_inst_index],
						bmdi->vpudrvctx.interrupt_flag[core_idx][intr_inst_index] != 0, msecs_to_jiffies(info.timeout));
				if (!ret) {
					ret = -ETIME;
					DPRINTK(KERN_ERR "[VPUDRV][-]VDI_IOCTL_WAIT_INTERRUPT timeout = %d\n", info.timeout);
					break;
				}

#endif
				if (signal_pending(current)) {
					ret = -ERESTARTSYS;
					break;
				}

				intr_reason_in_q = 0;
				interrupt_flag_in_q = kfifo_out_spinlocked(&bmdi->vpudrvctx.interrupt_pending_q[core_idx][intr_inst_index],
							&intr_reason_in_q, sizeof(u32), &bmdi->vpudrvctx.s_kfifo_lock[core_idx][intr_inst_index]);
				if (interrupt_flag_in_q > 0) {
					dev->interrupt_reason[core_idx][intr_inst_index] = intr_reason_in_q;
				} else {
					dev->interrupt_reason[core_idx][intr_inst_index] = 0;
				}

				DPRINTK("[VPUDRV] inst_index(%d), bmdi->vpudrvctx.interrupt_flag(%d), reason(0x%08lx)\n",
						intr_inst_index, bmdi->vpudrvctx.interrupt_flag[core_idx][intr_inst_index],
						dev->interrupt_reason[core_idx][intr_inst_index]);

INTERRUPT_REMAIN_IN_QUEUE:

				info.intr_reason = dev->interrupt_reason[core_idx][intr_inst_index];
				bmdi->vpudrvctx.interrupt_flag[core_idx][intr_inst_index] = 0;
				dev->interrupt_reason[core_idx][intr_inst_index] = 0;

#ifdef VPU_IRQ_CONTROL
				enable_irq(bmdi->vpudrvctx.s_vpu_irq[core_idx]);
#endif
				ret = copy_to_user((void __user *)arg, &info, sizeof(vpudrv_intr_info_t));
				DPRINTK("[VPUDRV][-]VDI_IOCTL_WAIT_INTERRUPT\n");
				if (ret != 0)
					return -EFAULT;
			}
			break;

		case VDI_IOCTL_SET_CLOCK_GATE:
			{
				u32 clkgate;

				//DPRINTK("[VPUDRV][+]VDI_IOCTL_SET_CLOCK_GATE\n");
				if (get_user(clkgate, (u32 __user *)arg))
					return -EFAULT;
#ifdef VPU_SUPPORT_CLOCK_CONTROL
				bm_vpu_clk_enable(clkgate);
#endif
				//DPRINTK("[VPUDRV][-]VDI_IOCTL_SET_CLOCK_GATE\n");

			}
			break;

	case VDI_IOCTL_GET_INSTANCE_POOL:
		{
			vpudrv_buffer_t vdb = {0};
			int core_idx = -1;
			int size = 0;

			DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_INSTANCE_POOL\n");

			ret = copy_from_user(&vdb, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
			if (ret != 0) {
				ret = -EFAULT;
				pr_err("[%s,%d] copy_from_user failed!\n", __func__, __LINE__);
				break;
			}

			if ((ret =  mutex_lock_interruptible(&dev->s_vpu_lock)) == 0) {
				vpu_core_idx_context *pool, *n;
				int check_flag = 0;

				core_idx = vdb.core_idx;
				size     = vdb.size;
				if (core_idx >= MAX_NUM_VPU_CORE) {
					pr_err("wrong core_idx: %d\n", core_idx);
					mutex_unlock(&dev->s_vpu_lock);
					return -EFAULT;
				}

				list_for_each_entry_safe(pool, n, &bmdi->vpudrvctx.s_core_idx_head, list) {
					if (pool->filp == filp && pool->core_idx == core_idx) {
						check_flag = 1;
						break;
					}
				}
				if (check_flag == 0) {
					vpu_core_idx_context *p_core_idx_context = vzalloc(sizeof(vpu_core_idx_context));
					if (p_core_idx_context == NULL) {
						pr_err("no mem in vzalloc.\n");
					}
					else {
						p_core_idx_context->core_idx = core_idx;
						p_core_idx_context->filp = filp;
						list_add(&p_core_idx_context->list, &bmdi->vpudrvctx.s_core_idx_head);
					}
				}

				if (bmdi->vpudrvctx.instance_pool[core_idx].base == 0) {
					unsigned long base = (unsigned long)vmalloc(size);
					if (base == 0) {
						ret = -ENOMEM;
						pr_err("[%s,%d] vmalloc(%d) failed!\n", __func__, __LINE__, size);
						mutex_unlock(&dev->s_vpu_lock);
						break;
					}

					/* clearing memory */
					memset((void *)base, 0x0, size);

					bmdi->vpudrvctx.instance_pool[core_idx].size      = size;
					bmdi->vpudrvctx.instance_pool[core_idx].base      = base;
					bmdi->vpudrvctx.instance_pool[core_idx].phys_addr = ((unsigned long)vmalloc_to_pfn((void *)base))<<PAGE_SHIFT;
					bmdi->vpudrvctx.instance_pool[core_idx].virt_addr = 0;
					bmdi->vpudrvctx.instance_pool[core_idx].core_idx = core_idx;
					bm_vpu_clk_prepare(bmdi, core_idx);
					bm_vpu_reset_core(bmdi, core_idx, 1);
				}

				vdb = bmdi->vpudrvctx.instance_pool[core_idx];
				mutex_unlock(&dev->s_vpu_lock);
			}
			else {
				return -ERESTARTSYS;
			}

			ret = copy_to_user((void __user *)arg, &vdb, sizeof(vpudrv_buffer_t));
			if (ret != 0) {
				ret = -EFAULT;
				pr_err("[%s,%d] copy_to_user failed!\n", __func__, __LINE__);
				break;
			}

			/* success to get memory for instance pool */
			DPRINTK("[VPUDRV][-] VDI_IOCTL_GET_INSTANCE_POOL core: %d, size: %d\n", core_idx, size);
		}
		break;

		case VDI_IOCTL_GET_COMMON_MEMORY:
			{
				vpudrv_buffer_t vdb = {0};

				DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_COMMON_MEMORY\n");
				ret = copy_from_user(&vdb, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
				if (ret != 0) {
					ret = -EFAULT;
					pr_err("[%s,%d] copy_from_user failed!\n", __func__, __LINE__);
					break;
				}
				if (vdb.core_idx >= MAX_NUM_VPU_CORE) {
					ret = -EFAULT;
					pr_err("[%s,%d] core_idx incorrect!\n", __func__, __LINE__);
					break;
				}
				if ((ret = down_interruptible(&dev->s_vpu_sem)) == 0) {
					if (bmdi->vpudrvctx.s_common_memory[vdb.core_idx].base != 0) {
						if (vdb.size != bmdi->vpudrvctx.s_common_memory[vdb.core_idx].size) {
							pr_err("maybe wrong in the common buffer size: %d and %d\n", vdb.size, bmdi->vpudrvctx.s_common_memory[vdb.core_idx].size);
							ret = -EFAULT;
						}
						else {
							ret = copy_to_user((void __user *)arg, &bmdi->vpudrvctx.s_common_memory[vdb.core_idx], sizeof(vpudrv_buffer_t));
							if (ret != 0)
								ret = -EFAULT;
						}
					} else {
						memcpy(&bmdi->vpudrvctx.s_common_memory[vdb.core_idx], &vdb, sizeof(vpudrv_buffer_t));
						if (bm_vpu_alloc_dma_buffer(bmdi, &bmdi->vpudrvctx.s_common_memory[vdb.core_idx]) != -1) {
							ret = copy_to_user((void __user *)arg, &bmdi->vpudrvctx.s_common_memory[vdb.core_idx], sizeof(vpudrv_buffer_t));
						}
						else {
							pr_err("[%s,%d] can not allocate the common buffer.\n", __func__, __LINE__);
							ret = -EFAULT;
						}
					}
					up(&dev->s_vpu_sem);
				}

				DPRINTK("[VPUDRV][-]VDI_IOCTL_GET_COMMON_MEMORY\n");
			}
			break;

		case VDI_IOCTL_OPEN_INSTANCE:
			{
				vpudrv_inst_info_t inst_info;
				vpudrv_instanace_list_t *vil;

				DPRINTK("[VPUDRV][+]VDI_IOCTL_OPEN_INSTANCE\n");

				vil = kzalloc(sizeof(*vil), GFP_KERNEL);
				if (!vil)
					return -ENOMEM;

				if (copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg, sizeof(vpudrv_inst_info_t)))
					return -EFAULT;

				vil->inst_idx = inst_info.inst_idx;
				vil->core_idx = inst_info.core_idx;
				vil->filp = filp;
				if ((ret =  mutex_lock_interruptible(&dev->s_vpu_lock)) == 0) {
					list_add(&vil->list, &bmdi->vpudrvctx.s_inst_list_head);
					bmdi->vpudrvctx.s_vpu_open_ref_count[vil->core_idx]++; /* flag just for that vpu is in opened or closed */
					inst_info.inst_open_count = bmdi->vpudrvctx.s_vpu_open_ref_count[vil->core_idx];
					kfifo_reset(&bmdi->vpudrvctx.interrupt_pending_q[inst_info.core_idx][inst_info.inst_idx]);
					bmdi->vpudrvctx.interrupt_flag[inst_info.core_idx][inst_info.inst_idx] = 0;

					if (bmdi->cinfo.chip_id == 0x1682) {
						vpu_open_inst_count++;
						if (vpu_polling_create == 0) {
							create_irq_poll_thread();
							vpu_polling_create = 1;
						}
					}
					mutex_unlock(&dev->s_vpu_lock);
				} else {
					kfree(vil);
				}

				if (copy_to_user((void __user *)arg, &inst_info, sizeof(vpudrv_inst_info_t))) {
					kfree(vil);
					return -EFAULT;
				}

				DPRINTK("[VPUDRV] VDI_IOCTL_OPEN_INSTANCE core_idx=%d, inst_idx=%d,bmdi->vpudrvctx.s_vpu_open_ref_count=%d, inst_open_count=%d\n",\
						(int)inst_info.core_idx, (int)inst_info.inst_idx,\
						bmdi->vpudrvctx.s_vpu_open_ref_count[inst_info.core_idx], inst_info.inst_open_count);
				DPRINTK("[VPUDRV][-]VDI_IOCTL_OPEN_INSTANCE\n");
			}
			break;

		case VDI_IOCTL_CLOSE_INSTANCE:
			{
				vpudrv_inst_info_t inst_info;
				vpudrv_instanace_list_t *vil, *n;

				DPRINTK("[VPUDRV][+]VDI_IOCTL_CLOSE_INSTANCE\n");

				if (copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg, sizeof(vpudrv_inst_info_t)))
					return -EFAULT;

				if ((ret =  mutex_lock_interruptible(&dev->s_vpu_lock)) == 0) {
					list_for_each_entry_safe(vil, n, &bmdi->vpudrvctx.s_inst_list_head, list) {
						if (vil->inst_idx == inst_info.inst_idx && vil->core_idx == inst_info.core_idx) {
							list_del(&vil->list);
							vpu_open_inst_count--;
							bmdi->vpudrvctx.s_vpu_open_ref_count[vil->core_idx]--;
							inst_info.inst_open_count = bmdi->vpudrvctx.s_vpu_open_ref_count[vil->core_idx];
							kfree(vil);
							break;
						}
					}
					bmdi->vpudrvctx.crst_cxt[inst_info.core_idx].instcall[inst_info.inst_idx] = 0;
					kfifo_reset(&bmdi->vpudrvctx.interrupt_pending_q[inst_info.core_idx][inst_info.inst_idx]);
					bmdi->vpudrvctx.interrupt_flag[inst_info.core_idx][inst_info.inst_idx] = 0;

					mutex_unlock(&dev->s_vpu_lock);
				}
				if (copy_to_user((void __user *)arg, &inst_info, sizeof(vpudrv_inst_info_t)))
					return -EFAULT;

				DPRINTK("[VPUDRV][-] VDI_IOCTL_CLOSE_INSTANCE core_idx=%d, inst_idx=%d,bmdi->vpudrvctx.s_vpu_open_ref_count=%d, inst_open_count=%d\n",\
						(int)inst_info.core_idx,\
						(int)inst_info.inst_idx,\
						bmdi->vpudrvctx.s_vpu_open_ref_count[inst_info.core_idx],\
						inst_info.inst_open_count);

				DPRINTK("[VPUDRV][-]VDI_IOCTL_CLOSE_INSTANCE\n");
			}
			break;

		case VDI_IOCTL_GET_INSTANCE_NUM:
			{
				vpudrv_inst_info_t inst_info;
				vpudrv_instanace_list_t *vil, *n;

				DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_INSTANCE_NUM\n");

				ret = copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg, sizeof(vpudrv_inst_info_t));
				if (ret != 0)
					break;

				if ((ret =  mutex_lock_interruptible(&dev->s_vpu_lock)) == 0) {
					inst_info.inst_open_count = 0;
					list_for_each_entry_safe(vil, n, &bmdi->vpudrvctx.s_inst_list_head, list) {
						if (vil->core_idx == inst_info.core_idx)
							inst_info.inst_open_count++;
					}
					mutex_unlock(&dev->s_vpu_lock);
				}

				ret = copy_to_user((void __user *)arg, &inst_info, sizeof(vpudrv_inst_info_t));

				DPRINTK("[VPUDRV] VDI_IOCTL_GET_INSTANCE_NUM core_idx=%d, inst_idx=%d, open_count=%d\n",\
						(int)inst_info.core_idx, (int)inst_info.inst_idx, inst_info.inst_open_count);
			}
			DPRINTK("[VPUDRV][-]VDI_IOCTL_GET_INSTANCE_NUM\n");
			break;

		case VDI_IOCTL_RESET:
			{
				int core_idx;

				DPRINTK("[VPUDRV][+]VDI_IOCTL_RESET\n");
				ret = copy_from_user(&core_idx, (int *)arg, sizeof(int));
				if (ret != 0)
					break;
				DPRINTK("Reset VPU core %d ...\n", core_idx);
				bm_vpu_hw_reset(bmdi, core_idx);
				DPRINTK("[VPUDRV][-]VDI_IOCTL_RESET\n");
			}
			break;

		case VDI_IOCTL_GET_REGISTER_INFO:
			{
				u32 core_idx;
				vpudrv_buffer_t info;

				DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_REGISTER_INFO\n");
				ret = copy_from_user(&info, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
				if (ret != 0)
					return -EFAULT;
				core_idx = info.size;
				DPRINTK("[VPUDRV]--core index: %d\n", core_idx);
				ret = copy_to_user((void __user *)arg, &bmdi->vpudrvctx.s_vpu_register[core_idx], sizeof(vpudrv_buffer_t));
				if (ret != 0)
					ret = -EFAULT;

				DPRINTK("[VPUDRV][-]VDI_IOCTL_GET_REGISTER_INFO bmdi->vpudrvctx.s_vpu_register.phys_addr==0x%lx,\
						bmdi->vpudrvctx.s_vpu_register.virt_addr=0x%lx, bmdi->vpudrvctx.s_vpu_register.size=%d\n",\
						bmdi->vpudrvctx.s_vpu_register[core_idx].phys_addr,\
						bmdi->vpudrvctx.s_vpu_register[core_idx].virt_addr,\
						bmdi->vpudrvctx.s_vpu_register[core_idx].size);

				DPRINTK("[VPUDRV][-]VDI_IOCTL_GET_REGISTER_INFO\n");
			}
			break;

		case VDI_IOCTL_GET_FIRMWARE_STATUS:
			{
				u32 core_idx;

				DPRINTK("[VPUDRV][+]VDI_IOCTL_GET_FIRMWARE_STATUS\n");
				if (get_user(core_idx, (u32 __user *) arg))
					return -EFAULT;

				if (core_idx >= dev->max_num_vpu_core)
					return -EFAULT;
				if (bmdi->vpudrvctx.s_bit_firmware_info[core_idx].size == 0)
					ret = 100;

				DPRINTK("[VPUDRV][-]VDI_IOCTL_GET_FIRMWARE_STATUS\n");
			}
			break;

		case VDI_IOCTL_WRITE_VMEM:
			{
				vpu_video_mem_op_t mem_op_write;

				DPRINTK("[VPUDRV][+]VDI_IOCTL_WRITE_VMEM\n");
				ret = copy_from_user(&mem_op_write, (vpu_video_mem_op_t *)arg, sizeof(vpu_video_mem_op_t));
				if (ret != 0) {
					return -EFAULT;
				}
				// only for bm1682
				if (bmdi->cinfo.chip_id == 0x1682)
					mem_op_write.size = (mem_op_write.size + 3) / 4 * 4;

				ret = bmdev_memcpy_s2d(bmdi, filp, mem_op_write.dst, (void __user *)mem_op_write.src,
						mem_op_write.size, true, KERNEL_NOT_USE_IOMMU);
				if (ret) {
					return -EFAULT;
				}
				DPRINTK("[VPUDRV][-]VDI_IOCTL_WRITE_VMEM\n");
			}
			break;

		case VDI_IOCTL_READ_VMEM:
			{
				vpu_video_mem_op_t mem_op_read;

				DPRINTK("[VPUDRV][+]VDI_IOCTL_READ_VMEM\n");
				ret = copy_from_user(&mem_op_read, (vpu_video_mem_op_t *)arg, sizeof(vpu_video_mem_op_t));
				if (ret != 0) {
					return -EFAULT;
				}

				ret = bmdev_memcpy_d2s(bmdi, filp, (void __user *)mem_op_read.dst, mem_op_read.src,
						mem_op_read.size, true, KERNEL_NOT_USE_IOMMU);
				if (ret != 0) {
					return -EFAULT;
				}

				DPRINTK("[VPUDRV][-]VDI_IOCTL_READ_VMEM\n");
			}
			break;

		default: DPRINTK(KERN_ERR "[VPUDRV] No such IOCTL, cmd is %d\n", cmd);
			break;
	}

	return ret;
}

ssize_t bm_vpu_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
	struct bm_device_info *bmdi = (struct bm_device_info *)filp->private_data;

	if (!buf) {
		DPRINTK(KERN_ERR "[VPUDRV] vpu_read buf = NULL error\n");
		return -EFAULT;
	}

	if (len == sizeof(vpudrv_regrw_info_t)) {
		vpudrv_regrw_info_t *vpu_reg_info;

		vpu_reg_info = kmalloc(sizeof(vpudrv_regrw_info_t), GFP_KERNEL);
		if (!vpu_reg_info) {
			DPRINTK(KERN_ERR "[VPUDRV] vpu_read  vpudrv_regrw_info allocation error\n");
			return -EFAULT;
		}

		if (copy_from_user(vpu_reg_info, buf, len)) {
			DPRINTK(KERN_ERR "[VPUDRV] vpu_read copy_from_user error for vpudrv_regrw_info\n");
			kfree(vpu_reg_info);
			return -EFAULT;
		}

		if (vpu_reg_info->size > 0 && vpu_reg_info->size <= 16 && !(vpu_reg_info->size & 0x3)) {
			int i;

			for (i = 0; i < vpu_reg_info->size; i += sizeof(int)) {
				u32 value;

				value = bm_read32(bmdi, vpu_reg_info->reg_base + i);
				vpu_reg_info->value[i] = value & vpu_reg_info->mask[i];
			}

			if (copy_to_user(buf, vpu_reg_info, len)) {
				DPRINTK(KERN_ERR "[VPUDRV] vpu_read copy_to_user error for vpudrv_regrw_info\n");
				kfree(vpu_reg_info);
				return -EFAULT;
			}

			kfree(vpu_reg_info);
			return len;
		} else {
			DPRINTK(KERN_ERR "[VPUDRV] vpu_read invalid size %d\n", vpu_reg_info->size);
		}

		kfree(vpu_reg_info);
	}

	return -1;
}

ssize_t bm_vpu_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
	struct bm_device_info *bmdi = (struct bm_device_info *)filp->private_data;
	int ret;

	DPRINTK("[VPUDRV] vpu_write len=%d\n", (int)len);
	if (!buf) {
		DPRINTK(KERN_ERR "[VPUDRV] vpu_write buf = NULL error\n");
		return -EFAULT;
	}

	if ((ret =  mutex_lock_interruptible(&bmdi->vpudrvctx.s_vpu_lock)) == 0) {
		if (len == sizeof(vpu_bit_firmware_info_t)) {
			vpu_bit_firmware_info_t *bit_firmware_info;

			bit_firmware_info = kmalloc(sizeof(vpu_bit_firmware_info_t), GFP_KERNEL);
			if (!bit_firmware_info) {
				mutex_unlock(&bmdi->vpudrvctx.s_vpu_lock);
				DPRINTK(KERN_ERR "[VPUDRV] vpu_write  bit_firmware_info allocation error\n");
				return -EFAULT;
			}

			if (copy_from_user(bit_firmware_info, buf, len)) {
				mutex_unlock(&bmdi->vpudrvctx.s_vpu_lock);
				DPRINTK(KERN_ERR "[VPUDRV] vpu_write copy_from_user error for bit_firmware_info\n");
				return -EFAULT;
			}

			if (bit_firmware_info->size == sizeof(vpu_bit_firmware_info_t)) {
				DPRINTK("[VPUDRV] vpu_write set bit_firmware_info coreIdx=0x%x, reg_base_offset=0x%x size=0x%x, bit_code[0]=0x%x\n",
						bit_firmware_info->core_idx,\
						(int)bit_firmware_info->reg_base_offset,\
						bit_firmware_info->size,\
						bit_firmware_info->bit_code[0]);

				if (bit_firmware_info->core_idx > bmdi->vpudrvctx.max_num_vpu_core) {
					mutex_unlock(&bmdi->vpudrvctx.s_vpu_lock);
					DPRINTK(KERN_ERR "[VPUDRV] vpu_write coreIdx[%d] is exceeded than bmdi->vpudrvctx.max_num_vpu_core[%d]\n",\
							bit_firmware_info->core_idx, bmdi->vpudrvctx.max_num_vpu_core);
					return -ENODEV;
				}

				memcpy((void *)&bmdi->vpudrvctx.s_bit_firmware_info[bit_firmware_info->core_idx],
						bit_firmware_info, sizeof(vpu_bit_firmware_info_t));
				kfree(bit_firmware_info);
				mutex_unlock(&bmdi->vpudrvctx.s_vpu_lock);
				return len;
			}

			kfree(bit_firmware_info);
		} else if (len == sizeof(vpudrv_regrw_info_t)) {
			vpudrv_regrw_info_t *vpu_reg_info;

			vpu_reg_info = kmalloc(sizeof(vpudrv_regrw_info_t), GFP_KERNEL);
			if (!vpu_reg_info) {
				mutex_unlock(&bmdi->vpudrvctx.s_vpu_lock);
				DPRINTK(KERN_ERR "[VPUDRV] vpu_write  vpudrv_regrw_info allocation error\n");
				return -EFAULT;
			}

			if (copy_from_user(vpu_reg_info, buf, len)) {
				mutex_unlock(&bmdi->vpudrvctx.s_vpu_lock);
				DPRINTK(KERN_ERR "[VPUDRV] vpu_write copy_from_user error for vpudrv_regrw_info\n");
				kfree(vpu_reg_info);
				return -EFAULT;
			}

			if (vpu_reg_info->size > 0 && vpu_reg_info->size <= 16 && !(vpu_reg_info->size & 0x3)) {
				int i;

				for (i = 0; i < vpu_reg_info->size; i += sizeof(int)) {
					u32 value;

					value = bm_read32(bmdi, vpu_reg_info->reg_base + i);
					value = value & ~vpu_reg_info->mask[i];
					value = value | (vpu_reg_info->value[i] & vpu_reg_info->mask[i]);
					bm_write32(bmdi, vpu_reg_info->reg_base + i, value);
				}

				kfree(vpu_reg_info);

				mutex_unlock(&bmdi->vpudrvctx.s_vpu_lock);

				return len;
			} else {
				DPRINTK(KERN_ERR "[VPUDRV] vpu_write invalid size %d\n", vpu_reg_info->size);
			}

			kfree(vpu_reg_info);
		}
		mutex_unlock(&bmdi->vpudrvctx.s_vpu_lock);
	}

	return -1;
}

static long get_exception_instance_info(struct file *filp)
{
	struct bm_device_info *bmdi = (struct bm_device_info *)filp->private_data;
	vpudrv_instanace_list_t *vil, *n;
	long core_idx = -1;
	long ret = 0;

	list_for_each_entry_safe(vil, n, &bmdi->vpudrvctx.s_inst_list_head, list)
	{
		if (vil->filp == filp) {
			if (core_idx == -1) {
				core_idx = vil->core_idx;
			} else if(core_idx != vil->core_idx) {
				pr_err("please check the release core index: %ld ....\n", vil->core_idx);
			}
			if (vil->inst_idx > 31)
				pr_err("please check the release core index too big. inst_idx : %ld\n", vil->inst_idx);
			ret |= (1UL<<vil->inst_idx);
		}
	}

	if (core_idx != -1) {
		ret |= (core_idx << 32);
	}

	return ret;
}

static int close_vpu_instance(long flags, struct file *filp)
{
	int core_idx, i;
	int vpu_create_inst_flag = 0;
	struct bm_device_info *bmdi = (struct bm_device_info *)filp->private_data;
	int release_lock_flag = 0;
	if (flags == 0)
		return release_lock_flag;

	core_idx = (flags >> 32) & 0xff;
	if (core_idx >= MAX_NUM_VPU_CORE) {
		pr_err("[VPUDRV] please check the core_idx in close wave.(%d)\n", core_idx);
		return release_lock_flag;
	}

	if (bmdi->vpudrvctx.crst_cxt[core_idx].status != SYSCXT_STATUS_WORKDING) {
		pr_err("[VPUDRV] core %d is exception\n", core_idx);
		return release_lock_flag;
	}

	for (i = 0; i < MAX_NUM_INSTANCE_VPU; i++) {
		if ((flags & (1UL<<i)) != 0) {
			//close the vpu instance
			int count = 0;

			while (1) {
				int ret;

				get_lock(bmdi, core_idx, CORE_LOCK);
				release_lock_flag = 1;
				//whether the instance has been created
				vpu_create_inst_flag = get_vpu_create_inst_flag(bmdi, core_idx);

				if ((vpu_create_inst_flag & (1 << i)) != 0) {
					ret = CloseInstanceCommand(bmdi, core_idx, i);
					release_lock(bmdi, core_idx, CORE_LOCK);
				} else {
					release_lock(bmdi, core_idx, CORE_LOCK);
					break;
				}
				if (ret != 0) {
					msleep(1);	// delay more to give idle time to OS;
					count += 1;
					if (count > 5) {
						pr_err("can not stop instances core %d inst %d\n", (int)core_idx, i);
						release_vpu_create_inst_flag(bmdi, core_idx, i);
						return release_lock_flag;
					}
					continue; // means there is command which should be flush.
				} else {
					release_vpu_create_inst_flag(bmdi, core_idx, i);
					DPRINTK("stop instances core %d inst %d success\n", (int)core_idx, i);
				}
				break;
			}
		}
	}
	return release_lock_flag;
}

int bm_vpu_release(struct inode *inode, struct file *filp)
{
	int ret =0, i, j;
	u32 open_count;
	long except_info = 0;
	struct bm_device_info *bmdi = (struct bm_device_info *)filp->private_data;
	vpu_drv_context_t *dev = &bmdi->vpudrvctx;
	int core_idx, release_lock_flag;

	DPRINTK("[VPUDRV] vpu_release\n");

	mutex_lock(&dev->s_vpu_lock);
	core_idx = get_core_idx(filp);
	//pr_info("core_idx : %d, filp: %p\n", core_idx, filp);
	except_info = get_exception_instance_info(filp);
	mutex_unlock(&dev->s_vpu_lock);
	release_lock_flag = close_vpu_instance(except_info, filp);

	if (core_idx >= 0 && core_idx < MAX_NUM_VPU_CORE) {
		get_lock(bmdi, core_idx, CORE_LOCK);
	}

	/* found and free the not handled buffer by user applications */
	down(&dev->s_vpu_sem);
	bm_vpu_free_buffers(filp);
	up(&dev->s_vpu_sem);

	mutex_lock(&dev->s_vpu_lock);
	/* found and free the not closed instance by user applications */
	ret = bm_vpu_free_instances(filp);
	mutex_unlock(&dev->s_vpu_lock);
	if (core_idx >= 0 && core_idx < MAX_NUM_VPU_CORE) {
		release_lock(bmdi, core_idx, CORE_LOCK);
		if (get_lock(bmdi, core_idx, CORE_DISPLAY_LOCK) == 1)
			release_lock(bmdi, core_idx, CORE_DISPLAY_LOCK);
	}

	if (vpu_polling_create == 1 && vpu_open_inst_count == 0) {
		destory_irq_poll_thread();
		vpu_polling_create = 0;
	}

	mutex_lock(&dev->s_vpu_lock);
	bmdi->vpudrvctx.open_count--;
	open_count = bmdi->vpudrvctx.open_count;

	if (open_count == 0) {
		for (j = 0; j < bmdi->vpudrvctx.max_num_vpu_core; j++) {
			for (i = 0; i < bmdi->vpudrvctx.max_num_instance; i++) {
				kfifo_reset(&bmdi->vpudrvctx.interrupt_pending_q[j][i]);
				bmdi->vpudrvctx.interrupt_flag[j][i] = 0;
			}
		}

		for (i = 0; i < bmdi->vpudrvctx.max_num_vpu_core; i++) {
			if (bmdi->vpudrvctx.instance_pool[i].base) {
				DPRINTK("[VPUDRV] free instance pool\n");
				vfree((const void *)bmdi->vpudrvctx.instance_pool[i].base);
				bmdi->vpudrvctx.instance_pool[i].base = 0;
				bmdi->vpudrvctx.instance_pool[i].phys_addr = 0;
				bm_vpu_reset_core(bmdi, i, 0);
				bm_vpu_clk_unprepare(bmdi, i);
			}
			/*
			if (bmdi->vpudrvctx.s_common_memory[i].base) {
				DPRINTK("[VPUDRV] free common memory\n");
				if (bmdi->cinfo.chip_id == 0x1684) {
					bm_vpu_free_dma_buffer(&bmdi->vpudrvctx.s_common_memory[i], &bmdi->vpudrvctx.s_vmem);
				} else {
					if (bmdi->vpudrvctx.s_common_memory[i].core_idx < 2)
						bm_vpu_free_dma_buffer(&bmdi->vpudrvctx.s_common_memory[i], &bmdi->vpudrvctx.s_vmemboda);
					else
						bm_vpu_free_dma_buffer(&bmdi->vpudrvctx.s_common_memory[i], &bmdi->vpudrvctx.s_vmemboda);
				}
				bmdi->vpudrvctx.s_common_memory[i].base = 0;
			}
			*/
		}

	}
	memset(&dev->crst_cxt[0], 0, sizeof(vpu_crst_context_t) * MAX_NUM_VPU_CORE);

	if (bmdi->cinfo.chip_id == 0x1684) {
		if (ret > 0 && bmdi->vpudrvctx.s_vpu_open_ref_count[ret-1] == 0) {
			DPRINTK(KERN_INFO "exception will reset the vpu core: %d\n", ret - 1);
			bmdi->vpudrvctx.s_bit_firmware_info[ret-1].size = 0;
		}
	}
	mutex_unlock(&dev->s_vpu_lock);

	return 0;
}

int bm_vpu_fasync(int fd, struct file *filp, int mode)
{
	struct bm_device_info *bmdi = (struct bm_device_info *)filp->private_data;
	vpu_drv_context_t *dev = &bmdi->vpudrvctx;

	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static int bm_vpu_map_to_register(struct file *filp, struct vm_area_struct *vm, int core_idx)
{
	unsigned long pfn;
	struct bm_device_info *bmdi = (struct bm_device_info *)filp->private_data;

	vm->vm_flags |= VM_IO | VM_RESERVED;
	vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);
	pfn = bmdi->vpudrvctx.s_vpu_register[core_idx].phys_addr >> PAGE_SHIFT;

	return remap_pfn_range(vm, vm->vm_start, pfn, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
}

static int bm_vpu_map_to_physical_memory(struct file *filp, struct vm_area_struct *vm)
{
	vm->vm_flags |= VM_IO | VM_RESERVED;
	vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);

	return remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
}

static int bm_vpu_map_to_instance_pool_memory(struct file *fp, struct vm_area_struct *vm, int core_idx)
{
	int ret;
	long length = vm->vm_end - vm->vm_start;
	unsigned long start = vm->vm_start;
	struct bm_device_info *bmdi = (struct bm_device_info *)fp->private_data;
	char *vmalloc_area_ptr = (char *)bmdi->vpudrvctx.instance_pool[core_idx].base;
	unsigned long pfn;

	vm->vm_flags |= VM_RESERVED;

	/* loop over all pages, map it page individually */
	while (length > 0) {
		pfn = vmalloc_to_pfn(vmalloc_area_ptr);
		if ((ret = remap_pfn_range(vm, start, pfn, PAGE_SIZE, PAGE_SHARED)) < 0) {
			return ret;
		}
		start += PAGE_SIZE;
		vmalloc_area_ptr += PAGE_SIZE;
		length -= PAGE_SIZE;
	}

	return 0;
}

static int vpu_map_vmalloc(struct file *fp, struct vm_area_struct *vm, char *vmalloc_area_ptr)
{
	int ret;
	long length = vm->vm_end - vm->vm_start;
	unsigned long start = vm->vm_start;
	unsigned long pfn;

	vm->vm_flags |= VM_RESERVED;
	/* loop over all pages, map it page individually */
	while (length > 0) {
		pfn = vmalloc_to_pfn(vmalloc_area_ptr);
		if ((ret = remap_pfn_range(vm, start, pfn, PAGE_SIZE, PAGE_SHARED)) < 0) {
			return ret;
		}
		start += PAGE_SIZE;
		vmalloc_area_ptr += PAGE_SIZE;
		length -= PAGE_SIZE;
	}
	return 0;
}

/*!
 * @brief memory map interface for vpu file operation
 * @return  0 on success or negative error code on error
 */
int bm_vpu_mmap(struct file *fp, struct vm_area_struct *vm)
{
	int i = 0;
	struct bm_device_info *bmdi = (struct bm_device_info *)fp->private_data;

	if (vm->vm_pgoff == 1)
		return vpu_map_vmalloc(fp, vm, (char *)bmdi->vpudrvctx.s_vpu_dump_flag);


	for (i = 0; i < bmdi->vpudrvctx.max_num_vpu_core; i++) {
		if (vm->vm_pgoff == (bmdi->vpudrvctx.s_vpu_register[i].phys_addr>>PAGE_SHIFT))
			return bm_vpu_map_to_register(fp, vm, i);
		if (vm->vm_pgoff == (bmdi->vpudrvctx.instance_pool[i].phys_addr>>PAGE_SHIFT))
			return bm_vpu_map_to_instance_pool_memory(fp, vm, i);
	}

	return bm_vpu_map_to_physical_memory(fp, vm);
}

static int bm_vpu_address_init(struct bm_device_info *bmdi, int op)
{
	u32 offset = 0, i;
	u64 bar_paddr = 0;
	void __iomem *bar_vaddr = NULL;
	struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

	for (i = 0; i < bmdi->vpudrvctx.max_num_vpu_core; i++) {
		if (op) {
			bm_get_bar_offset(pbar_info, bmdi->vpudrvctx.s_vpu_reg_phy_base[i], &bar_vaddr, &offset);
			bm_get_bar_base(pbar_info, bmdi->vpudrvctx.s_vpu_reg_phy_base[i], &bar_paddr);

			bmdi->vpudrvctx.s_vpu_register[i].phys_addr = (unsigned long)(bar_paddr + offset);
			bmdi->vpudrvctx.s_vpu_register[i].virt_addr = (unsigned long)(bar_vaddr + offset);
			bmdi->vpudrvctx.s_vpu_register[i].size = VPU_REG_SIZE;
			DPRINTK("[VPUDRV] : vpu get from physical base addr==0x%lx, virtual base=0x%lx, core idx = 0x%x\n",\
					bmdi->vpudrvctx.s_vpu_register[i].phys_addr, bmdi->vpudrvctx.s_vpu_register[i].virt_addr, i);

		} else {
			bmdi->vpudrvctx.s_vpu_register[i].virt_addr = 0;
			bmdi->vpudrvctx.s_vpu_register[i].phys_addr = 0;
			bmdi->vpudrvctx.s_vpu_register[i].size = 0;
		}
	}

	return 0;
}

static int bm_vpu_reserved_mem_init(struct bm_device_info *bmdi, int op)
{
	struct bm_gmem_info *gmem_info = &bmdi->gmem_info;

	if (!gmem_info) {
		DPRINTK("KERN_ERR [VPUDRV]: get gmem_info failed\n");
		return -1;
	}

	if (op) {
		bmdi->vpudrvctx.s_video_memory.size = gmem_info->resmem_info.vpu_vmem_size;
		bmdi->vpudrvctx.s_video_memory.phys_addr = gmem_info->resmem_info.vpu_vmem_addr;
		bmdi->vpudrvctx.s_video_memory.base = bmdi->vpudrvctx.s_video_memory.phys_addr;

		if (bmdi->cinfo.chip_id == 0x1684) {
			if (vmem_init(&bmdi->vpudrvctx.s_vmem, bmdi->vpudrvctx.s_video_memory.phys_addr, bmdi->vpudrvctx.s_video_memory.size) < 0) {
				DPRINTK(KERN_ERR "[VPUDRV] :  fail to init vmem system\n");
				return -1;
			}

		} else if (bmdi->cinfo.chip_id == 0x1682) {
			if (!bmdi->vpudrvctx.s_video_memory.base) {
				DPRINTK(KERN_ERR "[VPUDRV] :  fail to remap video memory physical phys_addr==0x%lx, base==0x%lx, size=%d\n",
						bmdi->vpudrvctx.s_video_memory.phys_addr, bmdi->vpudrvctx.s_video_memory.base, (int)bmdi->vpudrvctx.s_video_memory.size);
				return -1;
			}

			if (vmem_init(&bmdi->vpudrvctx.s_vmemboda, bmdi->vpudrvctx.s_video_memory.phys_addr, bmdi->vpudrvctx.s_video_memory.size / 2) < 0) {
				DPRINTK(KERN_ERR "[VPUDRV] :  fail to init vmem boda system\n");
				return -1;
			}

			if (vmem_init(&bmdi->vpudrvctx.s_vmemwave, bmdi->vpudrvctx.s_video_memory.phys_addr + bmdi->vpudrvctx.s_video_memory.size / 2,
						bmdi->vpudrvctx.s_video_memory.size / 2) < 0) {
				DPRINTK(KERN_ERR "[VPUDRV] :  fail to init vmem wave system\n");
				return -1;
			}

			DPRINTK("[VPUDRV] base of vmemboda is %lx base of vmemwave is %lx\n",\
					bmdi->vpudrvctx.s_vmemboda.base_addr, bmdi->vpudrvctx.s_vmemwave.base_addr);
			DPRINTK("[VPUDRV] success to probe vpu device with reserved video memory phys_addr==0x%lx, base = =0x%lx\n",
					bmdi->vpudrvctx.s_video_memory.phys_addr, bmdi->vpudrvctx.s_video_memory.base);

		} else {
			DPRINTK("[VPUDRV] unknown chip_id :0x%x\n", bmdi->cinfo.chip_id);
			return -1;
		}

		DPRINTK("[VPUDRV] vpu device with reserved video memory phys_addr==0x%lx, base = =0x%lx\n",\
				bmdi->vpudrvctx.s_video_memory.phys_addr, bmdi->vpudrvctx.s_video_memory.base);
	} else {

		if (bmdi->vpudrvctx.s_video_memory.base) {
			bmdi->vpudrvctx.s_video_memory.base = 0;
			if (bmdi->cinfo.chip_id == 0x1684) {
				vmem_exit(&bmdi->vpudrvctx.s_vmem);
			} else {
				vmem_exit(&bmdi->vpudrvctx.s_vmemboda);
				vmem_exit(&bmdi->vpudrvctx.s_vmemwave);
			}
		}
	}

	return 0;
}

static void bm_vpu_inst_pool_init(struct bm_device_info *bmdi, int op)
{
	int i;

	for (i = 0; i < bmdi->vpudrvctx.max_num_vpu_core; i++) {
		if (op) {
			bmdi->vpudrvctx.instance_pool[i].base = 0;
		} else {
			if (bmdi->vpudrvctx.instance_pool[i].base) {
				vfree((const void *)bmdi->vpudrvctx.instance_pool[i].base);
				bmdi->vpudrvctx.instance_pool[i].base = 0;
			}
		}
	}
}

static void bm_vpu_int_wq_init(struct bm_device_info *bmdi)
{
	int i, j;

	for (j = 0; j < bmdi->vpudrvctx.max_num_vpu_core; j++)
		for (i = 0; i < bmdi->vpudrvctx.max_num_instance; i++)
			init_waitqueue_head(&bmdi->vpudrvctx.interrupt_wait_q[j][i]);
}

static int bm_vpu_int_fifo_init(struct bm_device_info *bmdi, int op)
{
	int i, j, ret = 0;
	int max_interrupt_queue = 16 * bmdi->vpudrvctx.max_num_instance;

	for (j = 0; j < bmdi->vpudrvctx.max_num_vpu_core; j++) {
		for (i = 0; i < bmdi->vpudrvctx.max_num_instance; i++) {
			if (op) {
				ret = kfifo_alloc(&bmdi->vpudrvctx.interrupt_pending_q[j][i], max_interrupt_queue * sizeof(u32), GFP_KERNEL);
				if (ret) {
					DPRINTK(KERN_ERR "[VPUDRV] kfifo_alloc failed 0x%x\n", ret);
					return ret;
				}
				spin_lock_init(&bmdi->vpudrvctx.s_kfifo_lock[j][i]);
			} else {
				kfifo_free(&bmdi->vpudrvctx.interrupt_pending_q[j][i]);
				bmdi->vpudrvctx.interrupt_flag[j][i] = 0;
			}
		}
	}

	return 0;
}

static void bm_vpu_topaddr_set(struct bm_device_info *bmdi)
{
	u32 remap_val;
	u64 vmem_addr;

	if (bmdi->cinfo.chip_id == 0x1684) {
		vpudrv_buffer_t s_vpu_remap_register = {0};
		s_vpu_remap_register.phys_addr = VPU_REMAP_REG;
		remap_val = bm_read32(bmdi, s_vpu_remap_register.phys_addr);
		remap_val = (remap_val & 0xF000FF00) | 0x4040004;
		bm_write32(bmdi, s_vpu_remap_register.phys_addr, remap_val);
	}

	if (bmdi->cinfo.chip_id == 0x1682) {
		vmem_addr = 0x270000000;
		vmem_addr = vmem_addr - vmem_addr % 0x200000000;
		remap_val = bm_read32(bmdi, 0x50008008);
		remap_val &= 0xFF00FFFF;
		remap_val |= ((vmem_addr >> 32) & 0xff) << 16;
		bm_write32(bmdi, 0x50008008, remap_val);
		remap_val = bm_read32(bmdi, 0x50008008);
	}
}

static int bm_vpu_reset_core(struct bm_device_info *bmdi, int core_idx, int reset)
{
	u32 value = 0;

	if (bmdi->cinfo.chip_id == 0x1684) {
		value = bm_read32(bmdi, bm_vpu_rst[core_idx].reg);
		if (reset == 0)
			value &= ~(0x1 << bm_vpu_rst[core_idx].bit_n);
		else if (reset == 1)
			value |= (0x1 << bm_vpu_rst[core_idx].bit_n);
		else
			DPRINTK(KERN_ERR "VPUDRV :vpu reset unsupported operation\n");
		bm_write32(bmdi, bm_vpu_rst[core_idx].reg, value);
		value = bm_read32(bmdi, bm_vpu_rst[core_idx].reg);
	}

	return value;
}

static void bm_vpu_free_common_mem(struct bm_device_info *bmdi)
{
	if (bmdi->cinfo.chip_id == 0x1684) {
		int i;
		for (i = 0; i < bmdi->vpudrvctx.max_num_vpu_core; i++) {
			if (bmdi->vpudrvctx.s_common_memory[i].base) {
				bm_vpu_free_dma_buffer(&bmdi->vpudrvctx.s_common_memory[i], &bmdi->vpudrvctx.s_vmem);
				bmdi->vpudrvctx.s_common_memory[i].base = 0;
			}
		}
	}
}

static int bm_vpu_res_init(struct bm_device_info *bmdi)
{
	INIT_LIST_HEAD(&bmdi->vpudrvctx.s_vbp_head);
	INIT_LIST_HEAD(&bmdi->vpudrvctx.s_inst_list_head);
	INIT_LIST_HEAD(&bmdi->vpudrvctx.s_core_idx_head);
	mutex_init(&bmdi->vpudrvctx.s_vpu_lock);
	sema_init(&bmdi->vpudrvctx.s_vpu_sem, 1);

	return 0;
}

static void get_pll_ctl_setting(struct bm_pll_ctrl *best, unsigned long req_rate)
{
	// for vpu 640MHZ
	best->fbdiv = 128;
	best->refdiv = 1;
	best->postdiv1 = 5;
	best->postdiv2 = 1;
}

static void bm_vpu_pll_set_freq(struct bm_device_info *bmdi, unsigned long rate)
{
	unsigned int enable, status;
	unsigned int div, status_id, pll_id;
	struct bm_pll_ctrl pll_ctrl = { 0 };
	// for vpu
	status_id = 2;
	pll_id = 3;

	get_pll_ctl_setting(&pll_ctrl, rate);

	div = TOP_PLL_CTRL(pll_ctrl.fbdiv, pll_ctrl.postdiv1, pll_ctrl.postdiv2,
			pll_ctrl.refdiv);

	enable = top_reg_read(bmdi, TOP_PLL_ENABLE);
	top_reg_write(bmdi, TOP_PLL_ENABLE, enable & ~(0x1 << status_id));
	top_reg_write(bmdi, TOP_VPLL_CTL, div);

	status = top_reg_read(bmdi, TOP_PLL_STATUS);
	while (!((status >> (PLL_STAT_LOCK_OFFSET + status_id)) & 0x1))
		status = top_reg_read(bmdi, TOP_PLL_STATUS);

	status = top_reg_read(bmdi, TOP_PLL_STATUS);
	while ((status >> status_id) & 0x1)
		status = top_reg_read(bmdi, TOP_PLL_STATUS);

	top_reg_write(bmdi, TOP_PLL_ENABLE, enable | (0x1 << pll_id));
}

int bm_vpu_init(struct bm_device_info *bmdi)
{
	int ret = 0;
	struct bmdi_list *blist;
	int i = 0;

	DPRINTK("[VPUDRV] begin vpu_init\n");
	if (bmdi->cinfo.chip_id == 0x1684) {
		bmdi->vpudrvctx.s_vpu_irq = s_vpu_irq_1684;
		bmdi->vpudrvctx.s_vpu_reg_phy_base = s_vpu_reg_phy_base_1684;
		bmdi->vpudrvctx.max_num_vpu_core = 5;
		bmdi->vpudrvctx.max_num_instance = 32;
	}

	if (bmdi->cinfo.chip_id == 0x1682) {
		bmdi->vpudrvctx.s_vpu_irq = s_vpu_irq_1682;
		bmdi->vpudrvctx.s_vpu_reg_phy_base = s_vpu_reg_phy_base_1682;
		bmdi->vpudrvctx.max_num_vpu_core = 3;
		bmdi->vpudrvctx.max_num_instance = 8;
	}

#ifdef VPU_SUPPORT_CLOCK_CONTROL
	bm_vpu_clk_enable(INIT);
#endif
	if (bmdi->cinfo.chip_id == 0x1684) {
		bm_vpu_pll_set_freq(bmdi, 640);
	}
	bm_vpu_res_init(bmdi);

	if (bmdi->cinfo.chip_id == 0x1682) {
		blist = kzalloc(sizeof(struct bmdi_list), GFP_KERNEL);
		if (blist == NULL)
			return -ENOMEM;
		blist->bmdi = bmdi;
		list_add(&blist->list, &bmdi_list_head);
	}

	ret = bm_vpu_reserved_mem_init(bmdi, INIT);
	if (ret) {
		DPRINTK(KERN_ERR "[VPUDRV] reserved mem init failed!\n");
		return ret;
	}
	ret = bm_vpu_int_fifo_init(bmdi, INIT);
	if (ret) {
		DPRINTK(KERN_ERR "[VPUDRV] fifo init failed!\n");
		return ret;
	}

	bm_vpu_int_wq_init(bmdi);
	bm_vpu_inst_pool_init(bmdi, INIT);
	bm_vpu_address_init(bmdi, INIT);
	bm_vpu_topaddr_set(bmdi);

	for (i = 0; i < bmdi->vpudrvctx.max_num_vpu_core; i++) {
		bmdi->vpudrvctx.s_common_memory[i].size = SIZE_COMMON;
		if (bm_vpu_alloc_dma_buffer(bmdi, &bmdi->vpudrvctx.s_common_memory[i]) == -1) {
			pr_err("[%s,%d] can not allocate the common buffer.\n", __func__, __LINE__);
			ret = -EFAULT;
			break;
		}
	}

	DPRINTK("[VPUDRV] end vpu_init result=0x%x\n", ret);
	return 0;
}

void bm_vpu_exit(struct bm_device_info *bmdi)
{

	struct  bmdi_list *pos, *n;
#ifdef VPU_SUPPORT_CLOCK_CONTROL
	bm_vpu_clk_enable(FREE);
#endif
	bm_vpu_inst_pool_init(bmdi, FREE);
	bm_vpu_free_common_mem(bmdi);
	bm_vpu_reserved_mem_init(bmdi, FREE);
	bm_vpu_int_fifo_init(bmdi, FREE);
	bm_vpu_address_init(bmdi, FREE);

	list_for_each_entry_safe(pos, n, &bmdi_list_head, list) {
		list_del(&pos->list);
		kfree(pos);
	}
}

static void bmdrv_vpu_irq_handler(struct bm_device_info *bmdi)
{
	bm_vpu_irq_handler(bmdi);
}

void bm_vpu_request_irq(struct bm_device_info *bmdi)
{
	int i = 0;

	if (bmdi->cinfo.chip_id == 0x1684) {
		for (i = 0; i < bmdi->vpudrvctx.max_num_vpu_core; i++)
			bmdrv_submodule_request_irq(bmdi, bmdi->vpudrvctx.s_vpu_irq[i], bmdrv_vpu_irq_handler);
	}
}

void bm_vpu_free_irq(struct bm_device_info *bmdi)
{
	int i = 0;

	if (bmdi->cinfo.chip_id == 0x1684) {
		for (i = 0; i < bmdi->vpudrvctx.max_num_vpu_core; i++)
			bmdrv_submodule_free_irq(bmdi, bmdi->vpudrvctx.s_vpu_irq[i]);
	}
}

typedef struct vpu_inst_info {
	unsigned long flag;
	unsigned long time;
	unsigned long status;
	unsigned char *url;
} vpu_inst_info_t;

static vpu_inst_info_t s_vpu_inst_info[MAX_NUM_VPU_CORE * MAX_NUM_INSTANCE_VPU] = {0};
static DEFINE_MUTEX(s_vpu_proc_lock);

static unsigned long get_ms_time(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
	struct timespec64 tv;
#else
	struct timeval tv;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
	ktime_get_real_ts64(&tv);
#else
	do_gettimeofday(&tv);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
	return tv.tv_sec * 1000 + tv.tv_nsec / 1000000;
#else
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

static void set_stop_inst_info(unsigned long flag)
{
	int i;

	mutex_lock(&s_vpu_proc_lock);
	if (flag == 0) { //set all to stop
		for (i = 0; i < MAX_NUM_VPU_CORE * MAX_NUM_INSTANCE_VPU; i++) {
			if (s_vpu_inst_info[i].flag != 0 && s_vpu_inst_info[i].status == 0) {
				s_vpu_inst_info[i].time = get_ms_time();
				s_vpu_inst_info[i].status = 1;
			}
		}
	} else {
		for (i = 0; i < MAX_NUM_VPU_CORE * MAX_NUM_INSTANCE_VPU; i++) {
			if (s_vpu_inst_info[i].flag == flag && s_vpu_inst_info[i].status == 0) {
				s_vpu_inst_info[i].time = get_ms_time();
				s_vpu_inst_info[i].status = 1;
				break;
			}
		}
	}
	mutex_unlock(&s_vpu_proc_lock);
}

static void clean_timeout_inst_info(unsigned long timeout)
{
	unsigned long cur_time = get_ms_time();
	int i;

	mutex_lock(&s_vpu_proc_lock);
	for (i = 0; i < MAX_NUM_VPU_CORE * MAX_NUM_INSTANCE_VPU; i++) {
		if (s_vpu_inst_info[i].flag != 0 && s_vpu_inst_info[i].status == 1 && cur_time - s_vpu_inst_info[i].time > timeout) {
			s_vpu_inst_info[i].flag = 0;
			s_vpu_inst_info[i].status = 0;
			s_vpu_inst_info[i].time = 0;
			vfree(s_vpu_inst_info[i].url);
			s_vpu_inst_info[i].url = NULL;
		}
	}
	mutex_unlock(&s_vpu_proc_lock);
}

static int get_inst_info_idx(unsigned long flag)
{
	int i = -1;

	for (i = 0; i < MAX_NUM_VPU_CORE * MAX_NUM_INSTANCE_VPU; i++) {
		if (s_vpu_inst_info[i].flag == flag)
			return i;
	}

	for (i = 0; i < MAX_NUM_VPU_CORE * MAX_NUM_INSTANCE_VPU; i++) {
		if (s_vpu_inst_info[i].flag == 0)
			break;
	}

	return i;
}

static void printf_inst_info(void *mem, int size)
{
	int total_len = 0;
	int total_len_first = 0;
	int i;

	sprintf(mem, "\"link\":[\n");
	total_len = strlen(mem);
	total_len_first = total_len;

	for (i = 0; i < MAX_NUM_VPU_CORE * MAX_NUM_INSTANCE_VPU; i++) {
		if (s_vpu_inst_info[i].flag != 0) {
			sprintf(mem + total_len, "{\"id\" : %lu, \"time\" : %lu, \"status\" : %lu, \"url\" : \"%s\"},\n",
					s_vpu_inst_info[i].flag, s_vpu_inst_info[i].time, s_vpu_inst_info[i].status, s_vpu_inst_info[i].url);
			total_len = strlen(mem);
			if (total_len >= size) {
				pr_err("maybe overflow the memory, please check..\n");
				break;
			}
		}
	}

	if (total_len > total_len_first)
		sprintf(mem + total_len - 2, "]}\n");
	else
		sprintf(mem + total_len, "]}\n");
}

#define MAX_DATA_SIZE (256 * 5 * 8)
static ssize_t info_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	struct bm_device_info *bmdi;
	vpu_drv_context_t *dev;
	char *dat = vmalloc(MAX_DATA_SIZE);
	int len = 0;
	int err = 0;
	int i = 0;
#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	vmem_info_t vinfo = {0};
#endif

	bmdi = PDE_DATA(file_inode(file));
	dev = &bmdi->vpudrvctx;
#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	vmem_get_info(&dev->s_vmem, &vinfo);
	size = vinfo.free_pages * (unsigned long)vinfo.page_size;
	sprintf(dat, "{\"total_mem_size\" : %lu, \"used_mem_size\" : %lu, \"free_mem_size\" : %lu,\n",
			vinfo.total_pages * (unsigned long)vinfo.page_size, vinfo.alloc_pages * (unsigned long)vinfo.page_size, size);
#else
	sprintf(dat, "{");
#endif
	len = strlen(dat);
	sprintf(dat + len, "\"core\" : [\n");
	for (i = 0; i < dev->max_num_vpu_core - 1; i++) {
		len = strlen(dat);
		sprintf(dat + len, "{\"id\":%d, \"link_num\":%d}, ", i, dev->s_vpu_open_ref_count[i]);
	}
	len = strlen(dat);
	sprintf(dat + len, "{\"id\":%d, \"link_num\":%d}],\n", i, dev->s_vpu_open_ref_count[i]);

	len = strlen(dat);
	printf_inst_info(dat+len, MAX_DATA_SIZE-len);
	len = strlen(dat) + 1;
	if (size < len) {
		DPRINTK(KERN_ERR "read buf too small\n");
		err = -EIO;
		goto info_read_error;
	}

	if (*ppos >= len) {
		err = -1;
		goto info_read_error;
	}

	err = copy_to_user(buf, dat, len);
info_read_error:
	if (err) {
		vfree(dat);
		return 0;
	}

	*ppos = len;
	vfree(dat);
	return len;
}

static ssize_t info_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	//for debug the vpu.
	char data[256] = {0};
	int err = 0;
	int core_idx = -1;
	int inst_idx = -1;
	int in_num = -1;
	int out_num = -1;
	int i, j;
	unsigned long flag;

	if (size > 256)
		return 0;
	err = copy_from_user(data, buf, size);
	if (err)
		return 0;

	sscanf(data, "%ld ", &flag);

	if (flag > MAX_NUM_VPU_CORE) {
		unsigned long __time;
		unsigned long __status;
		unsigned char __url[256];

		sscanf(data, "%ld %ld %ld, %s", &flag, &__time, &__status, __url);

		if (__status == 1) {
			set_stop_inst_info(flag);
		} else {
			clean_timeout_inst_info(60*1000);
			mutex_lock(&s_vpu_proc_lock);
			i = get_inst_info_idx(flag);
			if (i < MAX_NUM_VPU_CORE * MAX_NUM_INSTANCE_VPU) {
				if (s_vpu_inst_info[i].url != NULL)
					vfree(s_vpu_inst_info[i].url);
				j = strlen(__url);
				s_vpu_inst_info[i].url = vmalloc(j+1);
				memcpy(s_vpu_inst_info[i].url, __url, j);
				s_vpu_inst_info[i].url[j] = 0;
				s_vpu_inst_info[i].time = __time;
				s_vpu_inst_info[i].status = __status;
				s_vpu_inst_info[i].flag = flag;
			}
			mutex_unlock(&s_vpu_proc_lock);
		}
		return size;
	}

	sscanf(data, "%d %d %d %d", &core_idx, &inst_idx, &in_num, &out_num);
	pr_info("%s", data);
	pr_info("core_idx: %d, inst: %d, in_num: %d, out_num: %d\n", core_idx, inst_idx, in_num, out_num);
#if 0
	if (s_vpu_dump_flag) {
		if (core_idx >= 0 && core_idx < MAX_NUM_VPU_CORE && inst_idx >= 0 && inst_idx < MAX_NUM_INSTANCE_VPU) {
			if (in_num >= 0)
				s_vpu_dump_flag[core_idx * MAX_NUM_INSTANCE_VPU * 2 + inst_idx * 2] = in_num;
			if (out_num >= 0)
				s_vpu_dump_flag[core_idx * MAX_NUM_INSTANCE_VPU * 2 + inst_idx * 2 + 1] = out_num;
		} else if(core_idx == -1) {
			for (i = 0; i < MAX_NUM_VPU_CORE; i++)
			{
				for (j = 0; j < 2*MAX_NUM_INSTANCE_VPU; j = j+2)
				{
					pr_info("[%d, %d]; ", s_vpu_dump_flag[i*MAX_NUM_INSTANCE_VPU*2+j], s_vpu_dump_flag[i*MAX_NUM_INSTANCE_VPU*2+j+1]);
				}
				pr_info("\n");
			}
		} else if(core_idx == -2) {
			memset(s_vpu_dump_flag, 0, sizeof(u32)*MAX_NUM_VPU_CORE*MAX_NUM_INSTANCE_VPU*2);
		}
	}
#endif
	return size;
}

const struct file_operations bmdrv_vpu_file_ops = {
	.read   = info_read,
	.write  = info_write,
};

static int bm_vpu_hw_reset(struct bm_device_info *bmdi, int core_idx)
{
	DPRINTK("[VPUDRV] request vpu reset from application.\n");

	bm_vpu_reset_core(bmdi, core_idx, 0);
	mdelay(10);
	bm_vpu_reset_core(bmdi, core_idx, 1);

	return 0;
}
