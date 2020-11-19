#ifndef _BM_CLKRST_H_
#define _BM_CLKRST_H_

typedef enum {
	MODULE_CDMA = 0,
	MODULE_GDMA = 1,
	MODULE_TPU = 2,
	MODULE_SMMU = 3,
	MODULE_SRAM = 4,
	MODULE_END = 5
} MODULE_ID;

#define BM1684_TPLL_REFDIV_MIN 1
#define BM1684_TPLL_REFDIV_MAX 63

#define BM1684_TPLL_FBDIV_MIN 16
#define BM1684_TPLL_FBDIV_MAX 320

#define BM1684_TPLL_POSTDIV1_MIN 1
#define BM1684_TPLL_POSTDIV1_MAX 7
#define BM1684_TPLL_POSTDIV2_MIN 1
#define BM1684_TPLL_POSTDIV2_MAX 7

#define BM1684_TPLL_FREF 25

#define BIT_CLK_TPU_ENABLE	13
#define BIT_CLK_TPU_ASSERT	0

#define BIT_AUTO_CLK_GATE_TPU_SUBSYSTEM_AXI_SRAM	2
#define BIT_AUTO_CLK_GATE_TPU_SUBSYSTEM_FABRIC		4
#define BIT_AUTO_CLK_GATE_PCIE_SUBSYSTEM_FABRIC		5

#define BIT_SW_RESET_TPU	17
#define BIT_SW_RESET_AXI_SRAM	16
#define BIT_SW_RESET_GDMA	15
#define BIT_SW_RESET_SMMU	12
#define BIT_SW_RESET_CDMA	11

void bmdrv_clk_set_tpu_divider(struct bm_device_info *bmdi, int devider_factor);
int bmdrv_clk_get_tpu_divider(struct bm_device_info *bmdi);
int bmdev_clk_ioctl_set_tpu_divider(struct bm_device_info* bmdi, unsigned long arg);
int bmdev_clk_ioctl_set_tpu_freq(struct bm_device_info* bmdi, unsigned long arg);
int bmdrv_1684_clk_get_tpu_freq(struct bm_device_info *bmdi);
int bmdrv_clk_set_tpu_target_freq(struct bm_device_info *bmdi, int target);
int bmdev_clk_ioctl_get_tpu_freq(struct bm_device_info* bmdi, unsigned long arg);
int bmdev_clk_ioctl_set_module_reset(struct bm_device_info* bmdi, unsigned long arg);

void bmdrv_clk_enable_tpu_subsystem_axi_sram_auto_clk_gate(struct bm_device_info *bmdi);
void bmdrv_clk_disable_tpu_subsystem_axi_sram_auto_clk_gate(struct bm_device_info *bmdi);
void bmdrv_clk_enable_tpu_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi);
void bmdrv_clk_disable_tpu_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi);
void bmdrv_clk_enable_pcie_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi);
void bmdrv_clk_disable_pcie_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi);

int bmdev_clk_hwlock_lock(struct bm_device_info* bmdi);
void bmdev_clk_hwlock_unlock(struct bm_device_info* bmdi);

void bmdrv_sw_reset_tpu(struct bm_device_info *bmdi);
void bmdrv_sw_reset_axi_sram(struct bm_device_info *bmdi);
void bmdrv_sw_reset_gdma(struct bm_device_info *bmdi);
void bmdrv_sw_reset_smmu(struct bm_device_info *bmdi);
void bmdrv_sw_reset_cdma(struct bm_device_info *bmdi);
#ifdef SOC_MODE
void bm1684_modules_reset(struct bm_device_info* bmdi);
int bm1684_modules_reset_init(struct bm_device_info* bmdi);
int bm1684_modules_clk_init(struct bm_device_info* bmdi);
void bm1684_modules_clk_deinit(struct bm_device_info* bmdi);
void bm1684_modules_clk_enable(struct bm_device_info* bmdi);
void bm1684_modules_clk_disable(struct bm_device_info* bmdi);
#endif

#endif
