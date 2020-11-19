#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sizes.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include "bm_common.h"
#include "bm1684_clkrst.h"
#include "bm1684_reg.h"
#include "bm1684_card.h"
#include "bm1684_cdma.h"
#include "bm1684_smmu.h"

#ifdef SOC_MODE
#include <linux/reset.h>
#include <linux/clk.h>
#endif

static int bmdrv_1684_fvco_check(int fref, int fbdiv, int refdiv)
{
	int foutvco = (fref * fbdiv) / refdiv;
	if((foutvco >= 800) && (foutvco <= 3200))
		return 1;
	else
		return 0;
}

static int bmdrv_1684_target_check(int target)
{
	if((target > 16) && (target < 3200))
		return 1;
	else
		return 0;
}

static int bmdrv_1684_pfd_check(int fref, int refdiv)
{
	if(fref/refdiv < 10)
		return 0;
	else
		return 1;
}

static int bmdrv_1684_postdiv_check(int postdiv1, int postdiv2)
{
	if(postdiv1 < postdiv2)
		return 0;
	else
		return 1;
}

static int bmdrv_1684_cal_tpll_para(int target_freq, int* fbdiv, int* refdiv,
                             int* postdiv1, int* postdiv2)
{
	int refdiv_index = 0;
	int fbdiv_index = 0;
	int postdiv1_index = 0;
	int postdiv2_index = 0;
	int success = 0;
	int temp_target = 0;

	if(!bmdrv_1684_target_check(target_freq)) return 0;

	for(refdiv_index=BM1684_TPLL_REFDIV_MIN; refdiv_index<=BM1684_TPLL_REFDIV_MAX; refdiv_index++)
		for(fbdiv_index=BM1684_TPLL_FBDIV_MIN; fbdiv_index<=BM1684_TPLL_FBDIV_MAX; fbdiv_index++)
			for(postdiv1_index=BM1684_TPLL_POSTDIV1_MIN; postdiv1_index<=BM1684_TPLL_POSTDIV1_MAX;postdiv1_index++)
				for(postdiv2_index=BM1684_TPLL_POSTDIV2_MIN; postdiv2_index<=BM1684_TPLL_POSTDIV2_MAX;postdiv2_index++) {
					temp_target = BM1684_TPLL_FREF * fbdiv_index/refdiv_index/postdiv1_index/postdiv2_index;
					if(temp_target == target_freq) {
						if(bmdrv_1684_fvco_check(BM1684_TPLL_FREF, fbdiv_index, refdiv_index) &&
               bmdrv_1684_pfd_check(BM1684_TPLL_FREF, refdiv_index) &&
               bmdrv_1684_postdiv_check(postdiv1_index, postdiv2_index)) {
							*fbdiv = fbdiv_index;
							*refdiv = refdiv_index;
							*postdiv1 = postdiv1_index;
							*postdiv2 = postdiv2_index;
							success = 1;
							break;
						}
						else
							continue;
					}
				}

	return success;
}

int bmdev_clk_hwlock_lock(struct bm_device_info* bmdi)
{
	u32 timeout_count = bmdi->cinfo.delay_ms*1000;

	while (top_reg_read(bmdi, TOP_CLK_LOCK)) {
		udelay(1);
		if (--timeout_count == 0) {
			pr_err("wait clk hwlock timeout\n");
			return -1;
		}
	}

	return 0;
}

void bmdev_clk_hwlock_unlock(struct bm_device_info* bmdi)
{
	top_reg_write(bmdi, TOP_CLK_LOCK, 0);
}

static int tpll_ctl_reg = 1 | 2<< 8 | 1 << 12 | 44<< 16;
int bmdrv_clk_set_tpu_target_freq(struct bm_device_info *bmdi, int target)
{
	int fbdiv = 0;
	int refdiv = 0;
	int postdiv1 = 0;
	int postdiv2 = 0;
	int val = 0;

	if (bmdi->misc_info.pcie_soc_mode == 0) {
		if(target<bmdi->boot_info.tpu_min_clk ||
			target>bmdi->boot_info.tpu_max_clk) {
			pr_err("%s: freq %d is too small or large\n", __func__, target);
			return -1;
		}
	}

	if(!bmdrv_1684_cal_tpll_para(target, &fbdiv, &refdiv, &postdiv1, &postdiv2)) {
		pr_err("%s: not support freq %d\n", __func__, target);
		return -1;
	}

	if (bmdev_clk_hwlock_lock(bmdi) != 0)
	{
		pr_err("%s: get clk hwlock fail\n", __func__);
		return -1;
	}

	/* gate */
	val = top_reg_read(bmdi, TOP_PLL_ENABLE);
	val &= ~(1 << 1);
	top_reg_write(bmdi, TOP_PLL_ENABLE, val);
	udelay(1);

	/* Modify TPLL Control Register */
	val = top_reg_read(bmdi, TOP_TPLL_CTL);
	if (bmdi->cinfo.platform == PALLADIUM) {
		val = tpll_ctl_reg;
	}
	val &= 0xf << 28;
	val |= refdiv & 0x1f;
	val |= (postdiv1 & 0x7) << 8;
	val |= (postdiv2 & 0x7) << 12;
	val |= (fbdiv & 0xfff) << 16;
	top_reg_write(bmdi, TOP_TPLL_CTL, val);
	if (bmdi->cinfo.platform == PALLADIUM) {
		tpll_ctl_reg = val;
	}
	udelay(1);

	/* poll status until bit9=1 adn bit1=0 */
	if (bmdi->cinfo.platform == DEVICE) {
		val = top_reg_read(bmdi, TOP_PLL_STATUS);
		while((val&0x202) != 0x200) {
			udelay(1);
			val = top_reg_read(bmdi, TOP_PLL_STATUS);
		}
	}

	/* ungate */
	val = top_reg_read(bmdi, TOP_PLL_ENABLE);
	val |= 1 << 1;
	top_reg_write(bmdi, TOP_PLL_ENABLE, val);

	bmdev_clk_hwlock_unlock(bmdi);

	return 0;
}

int bmdrv_1684_clk_get_tpu_freq(struct bm_device_info *bmdi)
{
	int fbdiv = 0;
	int refdiv = 0;
	int postdiv1 = 0;
	int postdiv2 = 0;
	int val = 0;
	int fout = 0;

	/* Modify TPLL Control Register */
	val = top_reg_read(bmdi, TOP_TPLL_CTL);
	if (bmdi->cinfo.platform == PALLADIUM) {
		val = tpll_ctl_reg;
	}
	refdiv = val & 0x1f;
	postdiv1 = (val >> 8) & 0x7;
	postdiv2 = (val >> 12) & 0x7;
	fbdiv = (val >> 16) & 0xfff;

	fout = 1000*BM1684_TPLL_FREF*fbdiv/refdiv/postdiv1/postdiv2;
	return fout/1000;
}

void bmdrv_clk_set_tpu_divider(struct bm_device_info *bmdi, int divider_factor)
{
	int val = 0;

	if (bmdev_clk_hwlock_lock(bmdi) != 0)
	{
		pr_err("%s: get clk hwlock fail\n", __func__);
		return;
	}

	/* gate */
	val = top_reg_read(bmdi, TOP_CLK_ENABLE1);
	val &= ~(1 << BIT_CLK_TPU_ENABLE);
	top_reg_write(bmdi, TOP_CLK_ENABLE1, val);
	udelay(1);

	/* assert */
	val = top_reg_read(bmdi, TOP_CLK_DIV0);
	val &= ~(1 << BIT_CLK_TPU_ASSERT);
	top_reg_write(bmdi, TOP_CLK_DIV0, val);
	udelay(1);

	/* change divider */
	val &= ~(0x1f << 16);
	val |= (divider_factor & 0x1f) << 16;
	val |= 1 << 3;
	top_reg_write(bmdi, TOP_CLK_DIV0, val);
	udelay(1);

	/* deassert */
	val |= 1;
	top_reg_write(bmdi, TOP_CLK_DIV0, val);
	udelay(1);

	/* ungate */
	val = top_reg_read(bmdi, TOP_CLK_ENABLE1);
	val |= 1 << BIT_CLK_TPU_ENABLE;
	top_reg_write(bmdi, TOP_CLK_ENABLE1, val);

	bmdev_clk_hwlock_unlock(bmdi);
}

int bmdrv_clk_get_tpu_divider(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_CLK_DIV0);
	return (val >> 16) & 0x1f;
}

int bmdev_clk_ioctl_set_tpu_divider(struct bm_device_info* bmdi, unsigned long arg)
{
	int divider = 0;

	if(get_user(divider, (u64 __user *)arg)) {
		pr_err("bmdrv: bmdev_clk_ioctl_set_tpu_divider get user failed!\n");
		return -1;
	} else {
		printk("bmdrv_clk_set_tpu_divider: divider=%d\n", divider);
		bmdrv_clk_set_tpu_divider(bmdi, divider);
		return 0;
	}
}

int bmdev_clk_ioctl_set_tpu_freq(struct bm_device_info* bmdi, unsigned long arg)
{
	int target = 0;

	if(get_user(target, (u64 __user *)arg)) {
		pr_err("bmdrv: bmdev_clk_ioctl_set_tpu_freq get user failed!\n");
		return -1;
	} else {
		return bmdrv_clk_set_tpu_target_freq(bmdi, target);
	}
}

int bmdev_clk_ioctl_get_tpu_freq(struct bm_device_info* bmdi, unsigned long arg)
{
	int freq = 0;
	int ret = 0;

	freq = bmdrv_1684_clk_get_tpu_freq(bmdi);
	ret = put_user(freq, (int __user *)arg);
	return ret;
}

void bmdrv_clk_enable_tpu_subsystem_axi_sram_auto_clk_gate(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val |= 1 << BIT_AUTO_CLK_GATE_TPU_SUBSYSTEM_AXI_SRAM;
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
}

void bmdrv_clk_disable_tpu_subsystem_axi_sram_auto_clk_gate(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val &= ~(1 << BIT_AUTO_CLK_GATE_TPU_SUBSYSTEM_AXI_SRAM);
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
}

void bmdrv_clk_enable_tpu_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val |= 1 << BIT_AUTO_CLK_GATE_TPU_SUBSYSTEM_FABRIC;
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
}

void bmdrv_clk_disable_tpu_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val &= ~(1 << BIT_AUTO_CLK_GATE_TPU_SUBSYSTEM_FABRIC);
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
}

void bmdrv_clk_enable_pcie_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val |= 1 << BIT_AUTO_CLK_GATE_PCIE_SUBSYSTEM_FABRIC;
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
}

void bmdrv_clk_disable_pcie_subsystem_fabric_auto_clk_gate(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_AUTO_CLK_GATE_EN0);
	val &= ~(1 << BIT_AUTO_CLK_GATE_PCIE_SUBSYSTEM_FABRIC);
	top_reg_write(bmdi, TOP_AUTO_CLK_GATE_EN0, val);
}

void bmdrv_sw_reset_tpu(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val &= ~(1 << BIT_SW_RESET_TPU);
	top_reg_write(bmdi, TOP_SW_RESET0, val);
	udelay(10);

	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val |= 1 << BIT_SW_RESET_TPU;
	top_reg_write(bmdi, TOP_SW_RESET0, val);
}

void bmdrv_sw_reset_axi_sram(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val &= ~(1 << BIT_SW_RESET_AXI_SRAM);
	top_reg_write(bmdi, TOP_SW_RESET0, val);
	udelay(10);

	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val |= 1 << BIT_SW_RESET_AXI_SRAM;
	top_reg_write(bmdi, TOP_SW_RESET0, val);
}

void bmdrv_sw_reset_gdma(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val &= ~(1 << BIT_SW_RESET_GDMA);
	top_reg_write(bmdi, TOP_SW_RESET0, val);
	udelay(10);

	val = top_reg_read(bmdi, TOP_SW_RESET0);
	val |= 1 << BIT_SW_RESET_GDMA;
	top_reg_write(bmdi, TOP_SW_RESET0, val);
}

void bmdrv_sw_reset_smmu(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_SW_RESET1);
	val &= ~(1 << BIT_SW_RESET_SMMU);
	top_reg_write(bmdi, TOP_SW_RESET1, val);
	udelay(10);

	val = top_reg_read(bmdi, TOP_SW_RESET1);
	val |= 1 << BIT_SW_RESET_SMMU;
	top_reg_write(bmdi, TOP_SW_RESET1, val);
}

void bmdrv_sw_reset_cdma(struct bm_device_info *bmdi)
{
	int val = 0;
	val = top_reg_read(bmdi, TOP_SW_RESET1);
	val &= ~(1 << BIT_SW_RESET_CDMA);
	top_reg_write(bmdi, TOP_SW_RESET1, val);
	udelay(10);

	val = top_reg_read(bmdi, TOP_SW_RESET1);
	val |= 1 << BIT_SW_RESET_CDMA;
	top_reg_write(bmdi, TOP_SW_RESET1, val);
}

void bmdrv_clk_set_module_reset(struct bm_device_info* bmdi, MODULE_ID module)
{
	switch (module) {
	case MODULE_CDMA:
		bmdrv_sw_reset_cdma(bmdi);
		break;
	case MODULE_GDMA:
		bmdrv_sw_reset_gdma(bmdi);
		break;
	case MODULE_TPU:
		bmdrv_sw_reset_tpu(bmdi);
		break;
	case MODULE_SMMU:
		bmdrv_sw_reset_smmu(bmdi);
		break;
	case MODULE_SRAM:
		bmdrv_sw_reset_axi_sram(bmdi);
		break;
	default:
		break;
	}
}

int bmdev_clk_ioctl_set_module_reset(struct bm_device_info* bmdi, unsigned long arg)
{
	MODULE_ID module = (MODULE_ID)0;

	if(get_user(module, (u64 __user *)arg)) {
		pr_err("bmdrv: bmdev_clk_ioctl_set_module get user failed!\n");
		return -1;
	} else {
		printk("bmdrv_clk_set_module_reset: module=%d\n", module);
		bmdrv_clk_set_module_reset(bmdi, module);
		return 0;
	}
}

#ifdef SOC_MODE
int bm1684_modules_reset_init(struct bm_device_info* bmdi)
{

	struct device *dev = &bmdi->cinfo.pdev->dev;
	struct chip_info *cinfo =  &bmdi->cinfo;
	int ret = 0;

	cinfo->arm9 = devm_reset_control_get(dev, "arm9");
	if (IS_ERR(cinfo->arm9)) {
		ret = PTR_ERR(cinfo->arm9);
		dev_err(dev, "failed to retrieve arm9 reset");
		return ret;
	}
	cinfo->tpu = devm_reset_control_get(dev, "tpu");
	if (IS_ERR(cinfo->tpu)) {
		ret = PTR_ERR(cinfo->tpu);
		dev_err(dev, "failed to retrieve tpu reset");
		return ret;
	}
	cinfo->gdma = devm_reset_control_get(dev, "gdma");
	if (IS_ERR(cinfo->gdma)) {
		ret = PTR_ERR(cinfo->gdma);
		dev_err(dev, "failed to retrieve gdma reset");
		return ret;
	}
	cinfo->cdma = devm_reset_control_get(dev, "cdma");
	if (IS_ERR(cinfo->cdma)) {
		ret = PTR_ERR(cinfo->cdma);
		dev_err(dev, "failed to retrieve cdma reset");
		return ret;
	}
	cinfo->smmu = devm_reset_control_get(dev, "smmu");
	if (IS_ERR(cinfo->smmu)) {
		ret = PTR_ERR(cinfo->smmu);
		dev_err(dev, "failed to retrieve smmu reset");
		return ret;
	}

	return ret;
}

void bm1684_modules_reset(struct bm_device_info* bmdi)
{
	bm1684_smmu_reset(bmdi);
	bm1684_cdma_reset(bmdi);
	bm1684_tpu_reset(bmdi);
	bm1684_gdma_reset(bmdi);
}

int bm1684_modules_clk_init(struct bm_device_info* bmdi)
{
	struct device *dev = &bmdi->cinfo.pdev->dev;
	struct chip_info *cinfo =  &bmdi->cinfo;
	int ret = 0;

	PR_TRACE("clk init 0x50010804 = 0x%x\n", top_reg_read(bmdi, 0x804));
	cinfo->tpu_clk = devm_clk_get(dev, "tpu");
	if (IS_ERR(cinfo->tpu_clk)) {
		ret = PTR_ERR(cinfo->tpu_clk);
		dev_err(dev, "failed to retrieve tpu clk");
		return ret;
	}
	cinfo->gdma_clk = devm_clk_get(dev, "gdma");
	if (IS_ERR(cinfo->gdma_clk)) {
		ret = PTR_ERR(cinfo->gdma_clk);
		dev_err(dev, "failed to retrieve gdma clk");
		return ret;
	}
	cinfo->cdma_clk = devm_clk_get(dev, "axi8_cdma");
	if (IS_ERR(cinfo->cdma_clk)) {
		ret = PTR_ERR(cinfo->cdma_clk);
		dev_err(dev, "failed to retrieve cdma clk");
		return ret;
	}
	cinfo->smmu_clk = devm_clk_get(dev, "axi8_mmu");
	if (IS_ERR(cinfo->smmu_clk)) {
		ret = PTR_ERR(cinfo->smmu_clk);
		dev_err(dev, "failed to retrieve smmu clk");
		return ret;
	}
	cinfo->pcie_clk = devm_clk_get(dev, "axi8_pcie");
	if (IS_ERR(cinfo->pcie_clk)) {
		ret = PTR_ERR(cinfo->pcie_clk);
		dev_err(dev, "failed to retrieve pcie clk");
		return ret;
	}
	cinfo->fixed_tpu_clk = devm_clk_get(dev, "fixed_tpu_clk");
	if (IS_ERR(cinfo->fixed_tpu_clk)) {
		ret = PTR_ERR(cinfo->fixed_tpu_clk);
		dev_err(dev, "failed to retrieve fixed tpu clk");
		return ret;
	}
	cinfo->sram_clk = devm_clk_get(dev, "axi_sram");
	if (IS_ERR(cinfo->sram_clk)) {
		ret = PTR_ERR(cinfo->sram_clk);
		dev_err(dev, "failed to retrieve sram clk");
		return ret;
	}
	cinfo->arm9_clk = devm_clk_get(dev, "arm9");
	if (IS_ERR(cinfo->arm9_clk)) {
		ret = PTR_ERR(cinfo->arm9_clk);
		dev_err(dev, "failed to retrieve arm9 clk");
		return ret;
	}
	cinfo->intc_clk = devm_clk_get(dev, "apb_intc");
	if (IS_ERR(cinfo->intc_clk)) {
		ret = PTR_ERR(cinfo->intc_clk);
		dev_err(dev, "failed to retrieve intc clk");
		return ret;
	}
	return 0;
}

void bm1684_modules_clk_deinit(struct bm_device_info* bmdi)
{
	struct device *dev = &bmdi->cinfo.pdev->dev;
	struct chip_info *cinfo =  &bmdi->cinfo;

	PR_TRACE("clk deinit 0x50010804 = 0x%x\n", top_reg_read(bmdi, 0x804));
	devm_clk_put(dev, cinfo->tpu_clk);
	devm_clk_put(dev, cinfo->gdma_clk);
	devm_clk_put(dev, cinfo->cdma_clk);
	devm_clk_put(dev, cinfo->smmu_clk);
	devm_clk_put(dev, cinfo->pcie_clk);
	devm_clk_put(dev, cinfo->fixed_tpu_clk);
	devm_clk_put(dev, cinfo->sram_clk);
	devm_clk_put(dev, cinfo->arm9_clk);
	devm_clk_put(dev, cinfo->intc_clk);
}

void bm1684_modules_clk_enable(struct bm_device_info* bmdi)
{
	bm1684_tpu_clk_enable(bmdi);
	bm1684_gdma_clk_enable(bmdi);
	bm1684_pcie_clk_enable(bmdi);
	bm1684_cdma_clk_enable(bmdi);
	bm1684_smmu_clk_enable(bmdi);
	bm1684_fixed_tpu_clk_enable(bmdi);
	bm1684_sram_clk_enable(bmdi);
	bm1684_arm9_clk_enable(bmdi);
	bm1684_intc_clk_enable(bmdi);
	PR_TRACE("clk enable 0x50010804 = 0x%x\n", top_reg_read(bmdi, 0x804));
}

void bm1684_modules_clk_disable(struct bm_device_info* bmdi)
{
	bm1684_tpu_clk_disable(bmdi);
	bm1684_gdma_clk_disable(bmdi);
	bm1684_pcie_clk_disable(bmdi);
	bm1684_cdma_clk_disable(bmdi);
	bm1684_smmu_clk_disable(bmdi);
	bm1684_fixed_tpu_clk_disable(bmdi);
	bm1684_sram_clk_disable(bmdi);
	bm1684_arm9_clk_disable(bmdi);
	bm1684_intc_clk_disable(bmdi);
	PR_TRACE("clk disable 0x50010804 = 0x%x\n", top_reg_read(bmdi, 0x804));
}
#endif
