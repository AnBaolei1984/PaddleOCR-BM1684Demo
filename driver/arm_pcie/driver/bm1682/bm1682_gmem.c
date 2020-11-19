#include "bm_common.h"
#include "bm_gmem.h"
#include "bm1682_reg.h"

#ifndef SOC_MODE
int bmdrv_bm1682_parse_reserved_mem_info(struct bm_device_info *bmdi)
{
	struct reserved_mem_info *resmem_info = &bmdi->gmem_info.resmem_info;

	resmem_info->armfw_addr = GLOBAL_MEM_START_ADDR + FW_DDR_IMG_START;
	resmem_info->armfw_size = FW_DDR_IMG_SIZE;
	resmem_info->eutable_addr = GLOBAL_MEM_START_ADDR + EU_CONSTANT_TABLE_START;
	resmem_info->eutable_size = EU_CONSTANT_TABLE_SIZE;
	resmem_info->armreserved_addr = GLOBAL_MEM_START_ADDR + ARM_RESERVED_START;
	resmem_info->armreserved_size = ARM_RESERVED_SIZE;
	resmem_info->smmu_addr = GLOBAL_MEM_START_ADDR + SMMU_RESERVED_START;
	resmem_info->smmu_size = SMMU_RESERVED_SIZE;
	resmem_info->warpaffine_addr = GLOBAL_MEM_START_ADDR + WARP_AFFINE_RESERVED_START;
	resmem_info->warpaffine_size = WARP_AFFINE_RESERVED_SIZE;
	resmem_info->npureserved_addr[0] = GLOBAL_MEM_START_ADDR + EFECTIVE_GMEM_START;
	resmem_info->npureserved_size[0] = EFECTIVE_GMEM_SIZE;
	resmem_info->npureserved_addr[1] = 0;
	resmem_info->npureserved_size[1] = 0;
	resmem_info->vpu_vmem_addr = VPU_VMEM_RESERVED_START;
	resmem_info->vpu_vmem_size = VPU_VMEM_RESERVED_SIZE;

	pr_info("pcie mode: armfw_addr = 0x%llx, armfw_size=0x%llx", resmem_info->armfw_addr, resmem_info->armfw_size);
	pr_info("pcie mode: eutable_addr = 0x%llx, eutable_size=0x%llx", resmem_info->eutable_addr, resmem_info->eutable_size);
	pr_info("pcie mode: armreserved_addr = 0x%llx, armreserved_size=0x%llx", resmem_info->armreserved_addr, resmem_info->armreserved_size);
	pr_info("pcie mode: smmu_addr = 0x%llx, smmu_size=0x%llx", resmem_info->smmu_addr, resmem_info->smmu_size);
	pr_info("pcie mode: warpaffine_addr = 0x%llx, warpaffine_size=0x%llx", resmem_info->warpaffine_addr, resmem_info->warpaffine_size);
	pr_info("pcie mode: npureserved_addr = 0x%llx, npureserved_size=0x%llx", resmem_info->npureserved_addr[0], resmem_info->npureserved_size[0]);
	pr_info("pcie mode: vpu_vmem_addr = 0x%llx, vpu_vmem_size=0x%llx", resmem_info->vpu_vmem_addr, resmem_info->vpu_vmem_size);
	return 0;
}
#else
#include <linux/of_address.h>
int bmdrv_bm1682_parse_reserved_mem_info(struct bm_device_info *bmdi)
{
	struct platform_device *pdev = bmdi->cinfo.pdev;
	struct reserved_mem_info *resmem_info = &bmdi->gmem_info.resmem_info;

	struct device_node *node;
	struct resource r[6];
	int ret = 0;
	int i = 0;

	for (i = 0; i < 6; i++) {
		node = of_parse_phandle(pdev->dev.of_node, "memory-region", i);
		if (!node) {
			dev_err(&pdev->dev, "no memory-region specified index %d\n", i);
			return -EINVAL;
		}

		ret = of_address_to_resource(node, 0, &r[i]);
		if (ret)
			return ret;
	}

	resmem_info->armfw_addr = r[0].start;
	resmem_info->armfw_size = resource_size(&r[0]);
	resmem_info->eutable_addr = r[1].start;
	resmem_info->eutable_size = resource_size(&r[1]);
	resmem_info->armreserved_addr = r[2].start;
	resmem_info->armreserved_size = resource_size(&r[2]);
	resmem_info->smmu_addr = r[3].start;
	resmem_info->smmu_size = resource_size(&r[3]);
	resmem_info->warpaffine_addr = r[4].start;
	resmem_info->warpaffine_size = resource_size(&r[4]);
	resmem_info->npureserved_addr[0] = r[5].start;
	resmem_info->npureserved_size[0] = resource_size(&r[5]);
	resmem_info->npureserved_addr[1] = 0;
	resmem_info->npureserved_size[1] = 0;
	pr_info("soc mode: armfw_addr = 0x%llx, armfw_size=0x%llx", resmem_info->armfw_addr, resmem_info->armfw_size);
	pr_info("soc mode: eutable_addr = 0x%llx, eutable_size=0x%llx", resmem_info->eutable_addr, resmem_info->eutable_size);
	pr_info("soc mode: armreserved_addr = 0x%llx, armreserved_size=0x%llx", resmem_info->armreserved_addr, resmem_info->armreserved_size);
	pr_info("soc mode: smmu_addr = 0x%llx, smmu_size=0x%llx", resmem_info->smmu_addr, resmem_info->smmu_size);
	pr_info("soc mode: warpaffine_addr = 0x%llx, warpaffine_size=0x%llx", resmem_info->warpaffine_addr, resmem_info->warpaffine_size);
	pr_info("soc mode: npureserved_addr = 0x%llx, npureserved_size=0x%llx", resmem_info->npureserved_addr[0], resmem_info->npureserved_size[0]);
	return 0;
}
#endif
