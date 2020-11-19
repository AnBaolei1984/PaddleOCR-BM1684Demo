#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include "bm_common.h"
#include "bm_cdma.h"
#include "bm_fw.h"
#include "bm_memcpy.h"
#include "bm1682_eu_cmd.h"
#ifdef SOC_MODE
#include "bm1682_soc_firmware_ddr.h"
#include "bm1682_soc_firmware_tcm.h"
#else
#include "bm1682_firmware_ddr.h"
#include "bm1682_firmware_tcm.h"
#endif
#include "bm1684_firmware_ddr.h"
#include "bm1684_firmware_tcm.h"

static int bmdrv_load_firmware(struct bm_device_info *bmdi, struct file *file, const unsigned int *firmware,
		int word_num, u64 dst)
{
	bm_cdma_arg cdma_arg;
	int i = 0;
	struct bm_stagemem *stagemem_d2s = &bmdi->memcpy_info.stagemem_d2s;
	unsigned int *p = stagemem_d2s->v_addr;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;

	if (bmdev_memcpy_s2d_internal(bmdi, dst, firmware, word_num * sizeof(u32))) {
		pr_err("bmdrv: memcpy s2d firmware failed!\n");
		return -EFAULT;
	}

	mutex_lock(&stagemem_d2s->stage_mutex);
	memset(stagemem_d2s->v_addr, 0, word_num * sizeof(u32));

	for (i = 0; i < word_num; i++) {
		if (p[i] != 0)
			pr_info("after clean index = %d, value = 0x%x\n", i, p[i]);
	}

	bmdev_construct_cdma_arg(&cdma_arg, dst,
			stagemem_d2s->p_addr,
			word_num * sizeof(u32),
			CHIP2HOST,
			false,
			false);

	if (memcpy_info->bm_cdma_transfer(bmdi, file, &cdma_arg, true)) {
		mutex_unlock(&stagemem_d2s->stage_mutex);
		return -EBUSY;
	}

	for (i = 0; i < word_num; i++) {
		if (p[i] != firmware[i]) {
			pr_info("compare fw fail, host = 0x%x, chip = 0x%x, index = %d\n", p[i], firmware[i], i);
			mutex_unlock(&stagemem_d2s->stage_mutex);
			return -EFAULT;
		}
	}
	mutex_unlock(&stagemem_d2s->stage_mutex);
	return 0;
}

static int bmdrv_wait_fwinit_done(struct bm_device_info *bmdi)
{
	u32 cnt = 50;
	int polling_ms = bmdi->cinfo.polling_ms;
#ifndef SOC_MODE
	if (bmdi->cinfo.platform == PALLADIUM)
		polling_ms *= PALLADIUM_CLK_RATIO;
#endif
	while ((gp_reg_read_enh(bmdi, GP_REG_FW_STATUS) != LAST_INI_REG_VAL)) {
		mdelay(polling_ms);
		if (--cnt == 0)
			break;
	}
	if (cnt) {
		pr_info("bmdrv: firmware init done!\n");
		return 0;
	}
	pr_err("bmdrv: firmware init timeout!\n");
	return -EBUSY;
}

static int bmdrv_fw_download_kernel(struct bm_device_info *bmdi, struct file *file)
{
	int ret;
	const unsigned int *fw_ddr_array, *fw_tcm_array;
	int fw_ddr_size, fw_tcm_size;

	switch (bmdi->cinfo.chip_id) {
	case 0x1682:
		fw_ddr_array = bm1682_firmware_ddr_array;
		fw_tcm_array = bm1682_firmware_tcm_array;
		fw_ddr_size = sizeof(bm1682_firmware_ddr_array);
		fw_tcm_size = sizeof(bm1682_firmware_tcm_array);
		break;
	case 0x1684:
		fw_ddr_array = bm1684_firmware_ddr_array;
		fw_tcm_array = bm1684_firmware_tcm_array;
		fw_ddr_size = sizeof(bm1684_firmware_ddr_array);
		fw_tcm_size = sizeof(bm1684_firmware_tcm_array);
		break;
	default:
		return -EINVAL;
	}
	if (fw_ddr_size != 0) {
		ret = bmdrv_load_firmware(bmdi, file, fw_ddr_array,
			fw_ddr_size / sizeof(u32),
			bmdi->gmem_info.resmem_info.armfw_addr);

		if (ret)
			return ret;
		pr_info("bmdrv: firmware loaded to ddr\n");
	}
	ret = bmdrv_load_firmware(bmdi, file, fw_tcm_array,
			fw_tcm_size / sizeof(u32),
			0);
	if (ret)
		return ret;
	pr_info("bmdrv: firmware loaded to tcm\n");
	return ret;
}

static int bmdrv_fw_download_user(struct bm_device_info *bmdi, struct file *file, pbm_fw_desc fw)
{
	int ret = 0;

	pr_info("bmdrv: firmware ddrfw_size is 0x%x, itcmfw_size is 0x%x\n",
			fw->ddrfw_size, fw->itcmfw_size);
	if (fw->ddrfw_size != 0) {
		ret = bmdev_memcpy_s2d(bmdi, file, bmdi->gmem_info.resmem_info.armfw_addr,
			(int __user *)fw->ddr_fw, fw->ddrfw_size, false, 0);
		if (ret)
			return ret;

		pr_info("bmdrv: firmware loaded to ddr\n");
	}

	ret = bmdev_memcpy_s2d(bmdi, file, 0, (int __user *)fw->itcm_fw,
			fw->itcmfw_size, false, 0);
	if (ret)
		return ret;

	pr_info("bmdrv: firmware loaded to itcm\n");
	return ret;
}

static int bmdrv_fw_download(struct bm_device_info *bmdi, struct file *file, pbm_fw_desc fw)
{
	if (fw)
		return bmdrv_fw_download_user(bmdi, file, fw);
	else
		return bmdrv_fw_download_kernel(bmdi, file);
}

void bmdrv_fw_unload(struct bm_device_info *bmdi)
{
	bmdi->cinfo.bmdrv_stop_arm9(bmdi);
}

static int bmdrv_eu_table_load(struct bm_device_info *bmdi)
{
	int i, cnt;
	u32 address_shift;
	u32 *eu_cmd_warp = kmalloc_array(EU_CMD_LEN, sizeof(u32), GFP_KERNEL);

	if (!eu_cmd_warp)
		return -ENOMEM;
	for (i = 0; i < EU_CMD_LEN / 4; i++) {
		eu_cmd_warp[i * 4 + 0] = eu_cmd[i * 4 + 3];
		eu_cmd_warp[i * 4 + 1] = eu_cmd[i * 4 + 2];
		eu_cmd_warp[i * 4 + 2] = eu_cmd[i * 4 + 1];
		eu_cmd_warp[i * 4 + 3] = eu_cmd[i * 4 + 0];
	}

	if (bmdev_memcpy_s2d_internal(bmdi, bmdi->gmem_info.resmem_info.eutable_addr,
			      eu_cmd_warp, EU_CMD_LEN * sizeof(u32))) {
		pr_err("bmdrv: load eu table failed!\n");
		kfree(eu_cmd_warp);
		return -EFAULT;
	}

	kfree(eu_cmd_warp);

	address_shift = bmdi->gmem_info.resmem_info.eutable_addr >> 8;

	bdc_reg_write(bmdi, 0x18, address_shift);

	cnt = 1000000;
	while (((bdc_reg_read(bmdi, 0x4) & 0x1) != 0) &&
			--cnt != 0)
		;
	if (cnt) {
		pr_info("bmdrv: load eu table done!\n");
		return 0;
	}
	pr_err("bmdrv: load eu table timeout!\n");
	return -EBUSY;
}

#ifndef SOC_MODE
extern void bmdrv_modules_request_irq(struct bm_device_info *bmdi);
extern void bm1684_pcie_msi_irq_enable(struct pci_dev *pdev,
		struct bm_device_info *bmdi);
#endif
int bmdrv_fw_load(struct bm_device_info *bmdi, struct file *file, pbm_fw_desc fw)
{
	int ret = 0;

	bmdi->cinfo.bmdrv_stop_arm9(bmdi);
	gp_reg_write_enh(bmdi, GP_REG_FW_STATUS, FW_START);

	ret = bmdrv_fw_download(bmdi, file, fw);
	if (ret) {
		pr_err("bmdrv: firmware download failed!\n");
		return ret;
	}
	bmdi->cinfo.bmdrv_start_arm9(bmdi);

	ret = bmdrv_wait_fwinit_done(bmdi);
	if (ret) {
		pr_err("bmdrv: firmware load timeout!\n");
		return ret;
	}
	if (fw) {
		bmdi->api_info[BM_MSGFIFO_CHANNEL_XPU].msgirq_num = 0UL;
		bmdi->api_info[BM_MSGFIFO_CHANNEL_XPU].sw_rp = 0;
#ifndef SOC_MODE
		/* arm9 reset may cause irq related registers reset*/
#if SYNC_API_INT_MODE == 1
	bmdrv_modules_request_irq(bmdi);
#endif
#endif
		pr_info("bmdrv: firmware load success!\n");
	} else {
		/* load eu table for 1682 during probe */
		if (bmdi->cinfo.chip_id == 0x1682)
			ret = bmdrv_eu_table_load(bmdi);
	}
	return ret;
}
