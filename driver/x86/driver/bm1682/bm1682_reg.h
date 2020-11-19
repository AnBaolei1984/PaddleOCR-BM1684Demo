#ifndef _BM1682_REG_H_
#define _BM1682_REG_H_
#ifdef _BM1684_REG_H_
#error "bm1684/bm1682 reg headers cannot be included together"
#endif

/* this header file contains all bm1682 register definitions
 * and global base address definitions
 */

#define DTCM_MEM_START_ADDR            0x02000000
#define DTCM_MEM_SIZE                  0x0001E000           // 120KB
#define SHARE_MEM_START_ADDR           (DTCM_MEM_START_ADDR + DTCM_MEM_SIZE)
#define SHARE_MEM_SIZE                 0x00002000           // 8KB

#define SOC_GLOBAL_MEM_START_ADDR     0x200000000
#define PCIE_GLOBAL_MEM_START_ADDR     0x100000000

#ifdef SOC_MODE
  #define GLOBAL_MEM_START_ADDR_CMD      0x0
  #define GLOBAL_MEM_START_ADDR         SOC_GLOBAL_MEM_START_ADDR
  #define GLOBAL_MEM_TOTAL_SIZE         0x100000000
  #define ECC_REGION                    0x0
#else
  #define GLOBAL_MEM_START_ADDR_CMD      0x0
  #define GLOBAL_MEM_START_ADDR         PCIE_GLOBAL_MEM_START_ADDR
  #define GLOBAL_MEM_TOTAL_SIZE         0x200000000
  #define ECC_REGION                    0x0
#endif

/* bm1682 device ddr layout
 *
 * ddr_start                                                    alloc_start
 * +---- ~ -----+----- ~ -------+---- ~ --------+---- ~ ------------- ~ -------- ~ ----------------------
 * | fw image   | eu const table|  arm reserved | smmu page table | ///////////////////|vpu reserved mem|
 * +---- ~ -----+----- ~ -------+---- ~ --------+---- ~ ------------- ~ -------- ~ ----------------------
 * <----16MB---><----16KB------><------64MB-----><-------1MB----->			<-------1.28GB----->
 */
#define FW_DDR_IMG_START	0x0
#define FW_DDR_IMG_SIZE		0x1000000

#define EU_CONSTANT_TABLE_START	(FW_DDR_IMG_START+FW_DDR_IMG_SIZE)
#define EU_CONSTANT_TABLE_SIZE	0x4000

#define ARM_RESERVED_START	(EU_CONSTANT_TABLE_START+EU_CONSTANT_TABLE_SIZE)
#define ARM_RESERVED_SIZE	0x4000000

#define SMMU_PAGE_ENTRY_NUM           (0x40000)  //256K (* 4B)
#define SMMU_RESERVED_START     (ARM_RESERVED_START + ARM_RESERVED_SIZE)
#define SMMU_RESERVED_SIZE	(SMMU_PAGE_ENTRY_NUM * 4)

#define WARP_AFFINE_RESERVED_START (SMMU_RESERVED_START + SMMU_RESERVED_SIZE)
#define WARP_AFFINE_RESERVED_SIZE  (0x80000)

#ifndef SOC_MODE
#define VPU_VMEM_RESERVED_OFFSET 0x170000000
#define VPU_VMEM_RESERVED_SIZE   (0x80000000)
#define VPU_TAIL_RESERVED_SIZE   (0x10000000)
#define EFECTIVE_GMEM_START (WARP_AFFINE_RESERVED_START + WARP_AFFINE_RESERVED_SIZE)
#define VPU_VMEM_RESERVED_START (GLOBAL_MEM_START_ADDR + VPU_VMEM_RESERVED_OFFSET)
#ifdef WITH_PCIE_VPU
#define EFECTIVE_GMEM_SIZE	(GLOBAL_MEM_TOTAL_SIZE - EFECTIVE_GMEM_START - VPU_VMEM_RESERVED_SIZE - VPU_TAIL_RESERVED_SIZE)
#else
#define EFECTIVE_GMEM_SIZE	(GLOBAL_MEM_TOTAL_SIZE - EFECTIVE_GMEM_START)
#endif
#else
#define EFECTIVE_GMEM_START (WARP_AFFINE_RESERVED_START + WARP_AFFINE_RESERVED_SIZE)
#define EFECTIVE_GMEM_SIZE	(GLOBAL_MEM_TOTAL_SIZE - EFECTIVE_GMEM_START)
#endif

/* Read GP register to get current irq status */
#define IRQ_STATUS_CDMA_INT             0x1111
#define IRQ_STATUS_MSG_DONE_INT         0x2222

/*top register*/

#define TOP_ITCM_SWITCH        0x008
#define TOP_ARM_RESET          0x014
#define TOP_POSITION           0x01c
#define TOP_ARM_ADDRMODE       0x1c0
#define TOP_I2C1_CLK_DIV       0x080
#define TOP_I2C2_CLK_DIV       0x084

/*cdma register*/

#define CDMA_MAIN_CTRL         0x800
#define CDMA_INT_MASK          0x808
#define CDMA_INT_STATUS        0x80c
#define CDMA_SYNC_STAT         0x814
#define CDMA_CMD_ACCP0         0x878
#define CDMA_CMD_ACCP1         0x87c
#define CDMA_CMD_ACCP2         0x880
#define CDMA_CMD_ACCP3         0x884
#define CDMA_CMD_ACCP4         0x888
#define CDMA_CMD_ACCP5         0x88c
#define CDMA_CMD_ACCP6         0x890
#define CDMA_CMD_ACCP7         0x894
#define CDMA_CMD_ACCP8         0x898
#define CDMA_CMD_ACCP9         0x89c
#define CDMA_MAIL_BOX0         0x8A4
#define CDMA_CMD_9F8           0x9f8
#define CDMA_CMD_9FC           0x9fc

/*smmu register*/

#define SMMU_FARR              0x000
#define SMMU_FARW              0x008
#define SMMU_FSR               0x010
#define SMMU_ICR               0x018
#define SMMU_IR                0x020
#define SMMU_IS                0x028
#define SMMU_TTBR              0x030
#define SMMU_TTER              0x038
#define SMMU_SCR               0x040

/*smmu register*/
#define EFUSE_MODE             0x000
#define EFUSE_ADDR             0x004
#define EFUSE_RD_DATA          0x00c

#ifdef SOC_MODE
#define GDMA_ENIGNE_BASE_ADDR		0x60000000
#define GDMA_ENIGNE_INTERRUPT_STATUS	0x8
#define GDMA_SIMU_PCIE_INT_MASK		0xFFFFFF01
#define GDMA_SIMU_PCIE_INTSTATUS_MASK  0xFFFF01FF
#define GDMA_SIMU_PCIE_INTSTATUS_CLEAR  0x00000100
#endif
#endif
