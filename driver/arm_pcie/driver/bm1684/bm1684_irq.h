#ifndef __BM1684_IRQ_H
#define __BM1684_IRQ_H

#define INTC0_BASE_ADDR_OFFSET  0x0
#define INTC1_BASE_ADDR_OFFSET  0x8000
#define IRQ_INTEN_L_OFFSET      0x0
#define IRQ_INTEN_H_OFFSET      0x4
#define IRQ_MASK_L_OFFSET       0x8
#define IRQ_MASK_H_OFFSET       0xc
#define IRQ_STATUS_L_OFFSET     0x20
#define IRQ_STATUS_H_OFFSET     0x24

#define GPIO_IRQ_ID     20
#define VPP0_IRQ_ID     30
#define VPP1_IRQ_ID     114

#define READ_ARM9_ID    51

#define TOP_GP_REG_ARM9_IRQ_STATUS_OFFSET   0xb8
#define TOP_GP_REG_ARM9_IRQ_SET_OFFSET      0x190
#define TOP_GP_REG_ARM9_IRQ_CLEAR_OFFSET    0x194
#define TOP_GP_REG_A53_IRQ_STATUS_OFFSET    0xbc
#define TOP_GP_REG_A53_IRQ_SET_OFFSET       0x198
#define TOP_GP_REG_A53_IRQ_CLEAR_OFFSET     0x19c
#define MSG_DONE_IRQ_MASK                   0x1
#define CPU_MSG_DONE_IRQ_MASK               0x2

#ifndef SOC_MODE
struct bm_device_info;
void bm1684_unmaskall_intc_irq(struct bm_device_info *bmdi);
void bm1684_pcie_msi_irq_enable(struct pci_dev *pdev, struct bm_device_info *bmdi);
void bm1684_pcie_msi_irq_disable(struct bm_device_info *bmdi);
void bm1684_enable_intc_irq(struct bm_device_info *bmdi, int irq_num, bool irq_enable);
void bm1684_get_irq_status(struct bm_device_info *bmdi, unsigned int *status);
#endif

#endif
