#ifndef _BM_COMMON_H_
#define _BM_COMMON_H_
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kobject.h>
#include <linux/spinlock.h>
#include <linux/mempool.h>
#include <linux/proc_fs.h>
#ifndef SOC_MODE
#include <linux/pci.h>
#include "bm_irq.h"
#include "vpu/vpu.h"
#include "bm1684/bm1684_jpu.h"
#include "bm1684/bm1684_vpp.h"
#else
#include <linux/platform_device.h>
#endif
#include "bm_api.h"
#include "bm_io.h"
#include "bm_trace.h"
#include "bm_memcpy.h"
#include "bm_gmem.h"
#include "bm_attr.h"
#include "bm_uapi.h"
#include "bm_msgfifo.h"
#include "bm_debug.h"

#ifndef __maybe_unused
#define __maybe_unused      __attribute__((unused))
#endif

//#define PR_DEBUG

#define BM_CHIP_VERSION 	2
#define BM_MAJOR_VERSION	2
#define BM_MINOR_VERSION	0
#define BM_DRIVER_VERSION	((BM_CHIP_VERSION<<16) | (BM_MAJOR_VERSION<<8) | BM_MINOR_VERSION)

#ifdef SOC_MODE
#define MAX_CARD_NUMBER		1
#else
#define MAX_CARD_NUMBER		64
#endif

#ifdef SOC_MODE
#define BM_CLASS_NAME   "bm-tpu"
#define BM_CDEV_NAME	"bm-tpu"
#else
#define BM_CLASS_NAME   "bm-sophon"
#define BM_CDEV_NAME	"bm-sophon"
#endif

#define BMDEV_CTL_NAME		"bmdev-ctl"

/*
 * memory policy
 * define it to use dma_xxx series APIs, which provide write-back
 * memory type, otherwise use kmalloc+set_memory_uc to get uncached-
 * minus memory type.
 */
#define USE_DMA_COHERENT

/* specify if platform is palladium */
#define PALLADIUM_CLK_RATIO 4000
#define DELAY_MS 20000
#define POLLING_MS 1


typedef enum {
	DEVICE,
	PALLADIUM,
	FPGA
} PLATFORM;

struct smbus_devinfo {
	u8 chip_temp_reg;
	u8 board_temp_reg;
	u8 board_power_reg;
	u8 fan_speed_reg;
	u8 vendor_id_reg;
	u8 hw_version_reg;
	u8 fw_version_reg;
	u8 board_name_reg;
	u8 sub_vendor_id_reg;
};
struct chip_info {
	const struct bm_card_reg *bm_reg;
	struct smbus_devinfo dev_info;
	struct device *device;
	const char *chip_type;
	char dev_name[10];
	struct bm_bar_info bar_info;
	int share_mem_size;
	PLATFORM platform;
	u32 delay_ms;
	u32 polling_ms;
	unsigned int chip_id;
	int chip_index;
#ifdef SOC_MODE
	u32 irq_id_cdma;
	u32 irq_id_msg;
	struct platform_device *pdev;
	struct reset_control *arm9;
	struct reset_control *tpu;
	struct reset_control *gdma;
	struct reset_control *smmu;
	struct reset_control *cdma;
	struct clk *tpu_clk;
	struct clk *gdma_clk;
	struct clk *pcie_clk;
	struct clk *smmu_clk;
	struct clk *cdma_clk;
	struct clk *fixed_tpu_clk;
	struct clk *intc_clk;
	struct clk *sram_clk;
	struct clk *arm9_clk;
#else
	int a53_enable;
	unsigned long long heap2_size;
	int irq_id;
	char drv_irq_name[16];
	char boot_loader_version[2][64];
	char sn[18];
	u32 ddr_failmap;
	u32 ddr_ecc_correctN;
	struct pci_dev *pcidev;
	bool has_msi;
	int board_version; /*bit [8:15] board type, bit [0:7] hardware version*/
	bmdrv_submodule_irq_handler bmdrv_module_irq_handler[128];
	void (*bmdrv_map_bar)(struct bm_device_info *, struct pci_dev *);
	void (*bmdrv_unmap_bar)(struct bm_bar_info *);
	void (*bmdrv_pcie_calculate_cdma_max_payload)(struct bm_device_info *);
	void (*bmdrv_enable_irq)(struct bm_device_info *bmdi,
			int irq_num, bool irq_enable);
	void (*bmdrv_get_irq_status)(struct bm_device_info *bmdi,
			unsigned int *status);
	void (*bmdrv_unmaskall_intc_irq)(struct bm_device_info *bmdi);
#endif
	int (*bmdrv_setup_bar_dev_layout)(struct bm_bar_info *,
			const struct bm_bar_info *);

	void (*bmdrv_start_arm9)(struct bm_device_info *);
	void (*bmdrv_stop_arm9)(struct bm_device_info *);

	void (*bmdrv_clear_cdmairq)(struct bm_device_info *bmdi);
	int (*bmdrv_clear_msgirq)(struct bm_device_info *bmdi);
	int (*bmdrv_clear_cpu_msgirq)(struct bm_device_info *bmdi);

	u32 (*bmdrv_pending_msgirq_cnt)(struct bm_device_info *bmdi);
};

struct bm_device_info {
	int dev_index;
	u64 bm_send_api_seq;
	struct cdev cdev;
	struct device *dev;
	struct device *parent;
	dev_t devno;
	void *priv;
	struct kobject kobj;

	struct mutex device_mutex;
	struct chip_info cinfo;
	struct bm_chip_attr c_attr;
	struct bm_card *bmcd;
	int enable_dyn_freq;
	int dump_reg_type;
	int fixed_fan_speed;
	int status; /* active or fault */
	int status_pcie;
	int status_over_temp;
	int status_sync_api;
	struct bm_api_info api_info[BM_MSGFIFO_CHANNEL_NUM];

	struct bm_gmem_info gmem_info;

	struct list_head handle_list;

	struct bm_memcpy_info memcpy_info;

	struct mutex clk_reset_mutex;

	struct bm_trace_info trace_info;

	struct bm_misc_info misc_info;

	struct bm_boot_info boot_info;

	struct bm_profile profile;

	struct bm_monitor_thread_info monitor_thread_info;

	struct proc_dir_entry *proc_dir;

#ifndef SOC_MODE
	vpp_drv_context_t vppdrvctx;
	vpu_drv_context_t vpudrvctx;
	jpu_drv_context_t jpudrvctx;
	struct mutex spacc_mutex;
#endif
};

struct bin_buffer {
	unsigned char *buf;
	unsigned int size;
	unsigned int target_addr;
};

#ifdef PR_DEBUG
#define PR_TRACE(fmt, ...) pr_info(fmt, ##__VA_ARGS__) // to minimize print
#else
#define PR_TRACE(fmt, ...)
#endif

#endif /* _BM_COMMON_H_ */
