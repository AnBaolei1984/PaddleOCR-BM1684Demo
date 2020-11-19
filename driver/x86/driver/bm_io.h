#ifndef _BM_IO_H_
#define _BM_IO_H_

#define REG_WRITE8(base, off, val)	iowrite8((val), (u8 *)(base + off))
#define REG_WRITE16(base, off, val) iowrite16((val), (u16 *)(base + off))
//#define REG_WRITE32(base, off, val) iowrite32((val), (u32 *)(base + off))
#define REG_WRITE32(base, off, val) \
{\
	iowrite32((val), (u32 *)(base + off)); \
	ioread32((u32 *)(base + off)); \
}
#include "bm_uapi.h"
#define REG_READ8(base, off)	ioread8((u8 *)(base + off))
#define REG_READ16(base, off)	ioread16((u16 *)(base + off))
#define REG_READ32(base, off)	ioread32((u32 *)(base + off))

/* General purpose register bit definition */
#define GP_REG_MESSAGE_WP          0
#define GP_REG_MESSAGE_RP          1
#define GP_REG_FW_STATUS             9
#define GP_REG_ARM9FW_LOG_RP             11
#define GP_REG_ARM9FW_LOG_WP             12
// bm1682 GP registers
#define GP_REG_MESSAGE_IRQSTATUS     2
#define GP_REG_CDMA_IRQSTATUS        3
#define GP_REG_MSGIRQ_NUM_LO     4
#define GP_REG_MSGIRQ_NUM_HI     5
#define GP_REG_API_PROCESS_TIME  6
// bm1684 GP registers
#define GP_REG_MSI_DATA         3
#define GP_REG_ARM9_READ_REG_ADDR     8

#define GP_REG_MESSAGE_WP_CHANNEL_XPU          GP_REG_MESSAGE_WP
#define GP_REG_MESSAGE_RP_CHANNEL_XPU          GP_REG_MESSAGE_RP
#define GP_REG_MESSAGE_WP_CHANNEL_CPU          6
#define GP_REG_MESSAGE_RP_CHANNEL_CPU          7

struct bm_io_bar_vaddr {
	void __iomem *dev_info_bar_vaddr;
	void __iomem *shmem_bar_vaddr;
	void __iomem *top_bar_vaddr;
	void __iomem *gp_bar_vaddr;
	void __iomem *i2c_bar_vaddr;
	void __iomem *cdma_bar_vaddr;
	void __iomem *pwm_bar_vaddr;
	void __iomem *ddr_bar_vaddr;
	void __iomem *bdc_bar_vaddr;
	void __iomem *smmu_bar_vaddr;
	void __iomem *intc_bar_vaddr;
	void __iomem *cfg_bar_vaddr;
	void __iomem *nv_timer_bar_vaddr;
	void __iomem *spi_bar_vaddr;
	void __iomem *gpio_bar_vaddr;
	void __iomem *vpp0_bar_vaddr;
	void __iomem *vpp1_bar_vaddr;
	void __iomem *uart_bar_vaddr;
	void __iomem *wdt_bar_vaddr;
	void __iomem *tpu_bar_vaddr;
	void __iomem *gdma_bar_vaddr;
};

struct bm_card_reg {
	u32 dev_info_base_addr;
	u32 shmem_base_addr;
	u32 ddr_base_addr;
	u32 top_base_addr;
	u32 gp_base_addr;
	u32 i2c_base_addr;
	u32 intc_base_addr;
	u32 cfg_base_addr;
	u32 pwm_base_addr;
	u32 bdc_base_addr;
	u32 cdma_base_addr;
	u32 smmu_base_addr;
	u32 nv_timer_base_addr;
	u32 spi_base_addr;
	u32 gpio_base_addr;
	u32 vpp0_base_addr;
	u32 vpp1_base_addr;
	u32 uart_base_addr;
	u32 wdt_base_addr;
	u32 tpu_base_addr;
	u32 gdma_base_addr;
};

#ifdef SOC_MODE
struct bm_bar_info {
	u64 bar0_start;                     // address in host I/O memory layout
	u64 bar0_dev_start;                 // address in device memory layout
	u64 bar0_len;
	void __iomem *bar0_vaddr;
	u64 bar1_start;
	u64 bar1_dev_start;
	u64 bar1_len;
	void __iomem *bar1_vaddr;
	u64 bar2_start;
	u64 bar2_dev_start;
	u64 bar2_len;
	void __iomem *bar2_vaddr;
	u64 bar4_start;
	u64 bar4_dev_start;
	u64 bar4_len;
	void __iomem *bar4_vaddr;

	struct bm_io_bar_vaddr io_bar_vaddr;
};
#else
struct bm_bar_info {
	u64 bar0_start;
	u64 bar0_dev_start;
	u64 bar0_len;
	void __iomem *bar0_vaddr;

	u64 bar1_start;
	u64 bar1_len;		//for backforward.
	u64 bar1_dev_start;
	u64 bar1_part0_offset;
	u64 bar1_part0_dev_start;
	u64 bar1_part0_len;
	u64 bar1_part1_offset;
	u64 bar1_part1_dev_start;
	u64 bar1_part1_len;
	u64 bar1_part2_offset;
	u64 bar1_part2_dev_start;
	u64 bar1_part2_len;
	u64 bar1_part3_offset;
	u64 bar1_part3_dev_start;
	u64 bar1_part3_len;
	u64 bar1_part4_offset;
	u64 bar1_part4_dev_start;
	u64 bar1_part4_len;
	u64 bar1_part5_offset;
	u64 bar1_part5_dev_start;
	u64 bar1_part5_len;
	u64 bar1_part6_offset;
	u64 bar1_part6_dev_start;
	u64 bar1_part6_len;
	u64 bar1_part7_offset;
	u64 bar1_part7_dev_start;
	u64 bar1_part7_len;
	void __iomem *bar1_vaddr;

	u64 bar2_start;
	u64 bar2_dev_start;
	u64 bar2_len;
	u64 bar2_part0_offset;
	u64 bar2_part0_dev_start;
	u64 bar2_part0_len;
	void __iomem *bar2_vaddr;

	u64 bar4_start;
	u64 bar4_dev_start;
	u64 bar4_len;
	void __iomem *bar4_vaddr;

	struct bm_io_bar_vaddr io_bar_vaddr;
};
#endif

static const struct bm_card_reg bm_reg_1682 = {
	.shmem_base_addr  = 0x201e000,
	.ddr_base_addr  = 0x50000000,
	.top_base_addr  = 0x50008000,
	.gp_base_addr  = 0x50008240,
	.i2c_base_addr  = 0x5001e000,
	.intc_base_addr  = 0x50023000,
	.gpio_base_addr  = 0x0,
	.nv_timer_base_addr  = 0x0,
	.cfg_base_addr  = 0x0,
	.pwm_base_addr  = 0x50029000,
	.bdc_base_addr = 0x60002000,
	.cdma_base_addr  = 0x60003000,
	.smmu_base_addr  = 0x60006000,
	.spi_base_addr = 0xFFF00000,
	.vpp0_base_addr  = 0x50440000,
	.vpp1_base_addr  = 0x500F0000,
	.uart_base_addr  = 0x0,
	.wdt_base_addr  = 0x0,
	.tpu_base_addr  = 0x0,
	.gdma_base_addr  = 0x0,
};

static const struct bm_card_reg bm_reg_1684 = {
	.dev_info_base_addr  = 0x201bf80,
	.shmem_base_addr  = 0x201c000,
	.ddr_base_addr  = 0x68000000,
	.top_base_addr  = 0x50010000,
	.gp_base_addr  = 0x50010080,
	.i2c_base_addr  = 0x5001c000, // i2c1
	.intc_base_addr  = 0x50023000,
	.gpio_base_addr  = 0x50027000, // GPIO0
	.nv_timer_base_addr  = 0x50010180,
	.cfg_base_addr  = 0x60000000,
	.pwm_base_addr  = 0x50029000, // PWM0
	.bdc_base_addr = 0x58001000,
	.cdma_base_addr  = 0x58003000,
	.smmu_base_addr  = 0x58002000,
	.spi_base_addr = 0x06000000,
	.vpp0_base_addr  = 0x50070000,
	.vpp1_base_addr  = 0x500F0000,
	.uart_base_addr  = 0x50118000, // UART0
	.wdt_base_addr  = 0x50026000,
	.tpu_base_addr  = 0x58001000,
	.gdma_base_addr  = 0x58000000,
};

struct bm_device_info;

u32 bm_read32(struct bm_device_info *bmdi, u32 address);
u32 bm_write32(struct bm_device_info *bmdi, u32 address, u32 data);
u8 bm_read8(struct bm_device_info *bmdi, u32 address);
u32 bm_write8(struct bm_device_info *bmdi, u32 address, u8 data);
u16 bm_read16(struct bm_device_info *bmdi, u32 address);
u32 bm_write16(struct bm_device_info *bmdi, u32 address, u16 data);
u64 bm_read64(struct bm_device_info *bmdi, u32 address);
u64 bm_write64(struct bm_device_info *bmdi, u32 address, u64 data);

void dev_info_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val, int len);
u8 dev_info_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void shmem_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val, u32 channel);
u32 shmem_reg_read(struct bm_device_info *bmdi, u32 reg_offset, u32 channel);
void top_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 top_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void gp_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 gp_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void i2c_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 i2c_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void smbus_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 smbus_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void pwm_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 pwm_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void cdma_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 cdma_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void ddr_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 ddr_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void bdc_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 bdc_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void smmu_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 smmu_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void intc_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 intc_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void cfg_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 cfg_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void nv_timer_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 nv_timer_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void spi_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 spi_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void gpio_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 gpio_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void vpp0_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 vpp0_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void vpp1_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 vpp1_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void uart_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 uart_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void wdt_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 wdt_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void tpu_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 tpu_reg_read(struct bm_device_info *bmdi, u32 reg_offset);
void gdma_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val);
u32 gdma_reg_read(struct bm_device_info *bmdi, u32 reg_offset);

static inline void gp_reg_write_enh(struct bm_device_info *bmdi, u32 idx, u32 data)
{
	gp_reg_write(bmdi, idx * 4, data);
}

static inline u32 gp_reg_read_enh(struct bm_device_info *bmdi, u32 idx)
{
	return gp_reg_read(bmdi, idx * 4);
}

static inline void shmem_reg_write_enh(struct bm_device_info *bmdi, u32 idx, u32 data, u32 channel)
{
	shmem_reg_write(bmdi, idx * 4, data, channel);
}

static inline u32 shmem_reg_read_enh(struct bm_device_info *bmdi, u32 idx, u32 channel)
{
	return shmem_reg_read(bmdi, idx * 4, channel);
}

/* DDR read/write enhance: operate on specified ddr controller region */
#define DDR_REGION_SIZE 0x1000

static inline void ddr_reg_write_enh(struct bm_device_info *bmdi, u32 ddr_ctrl, u32 reg_offset, u32 val)
{
	ddr_reg_write(bmdi, DDR_REGION_SIZE * ddr_ctrl + reg_offset, val);
}

static inline u32 ddr_reg_read_enh(struct bm_device_info *bmdi, u32 ddr_ctrl, u32 reg_offset)
{
	return ddr_reg_read(bmdi, DDR_REGION_SIZE * ddr_ctrl + reg_offset);
}

void io_init(struct bm_device_info *bmdi);

int bm_set_reg(struct bm_device_info *bmdi, struct bm_reg *reg);
int bm_get_reg(struct bm_device_info *bmdi, struct bm_reg *reg);
#endif
