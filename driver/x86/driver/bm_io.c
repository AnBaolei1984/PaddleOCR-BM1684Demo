#include "bm_common.h"
#include "bm_io.h"

#ifdef SOC_MODE
void bm_get_bar_offset(struct bm_bar_info *pbar_info, u32 address,
		void __iomem **bar_vaddr, u32 *offset)
{
	/* Choose bar the address belongs to, and compute the offset on bar */
	if (address >= pbar_info->bar4_dev_start &&
			address < pbar_info->bar4_dev_start + pbar_info->bar4_len) {
		*bar_vaddr = pbar_info->bar4_vaddr;
		*offset = address - pbar_info->bar4_dev_start;
	} else if (address >= pbar_info->bar2_dev_start &&
			address < pbar_info->bar2_dev_start + pbar_info->bar2_len) {
		*bar_vaddr = pbar_info->bar2_vaddr;
		*offset = address - pbar_info->bar2_dev_start;
	} else if (address >= pbar_info->bar1_dev_start &&
			address < pbar_info->bar1_dev_start + pbar_info->bar1_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
		*offset = address - pbar_info->bar1_dev_start;
	} else if (address >= pbar_info->bar0_dev_start &&
			address < pbar_info->bar0_dev_start + pbar_info->bar0_len) {
		*bar_vaddr = pbar_info->bar0_vaddr;
		*offset = address - pbar_info->bar0_dev_start;
	} else {
		pr_err("%s invalid address = 0x%x\n", __func__, address);
	}
}
#else
void bm_get_bar_offset(struct bm_bar_info *pbar_info, u32 address,
		void __iomem **bar_vaddr, u32 *offset)
{
	/* Choose bar the address belongs to, and compute the offset on bar */
	if (address >= pbar_info->bar4_dev_start &&
			address < pbar_info->bar4_dev_start + pbar_info->bar4_len) {
		*bar_vaddr = pbar_info->bar4_vaddr;
		*offset = address - pbar_info->bar4_dev_start;
	} else if (address >= pbar_info->bar2_dev_start &&
			address < pbar_info->bar2_dev_start + pbar_info->bar2_len) {
		*bar_vaddr = pbar_info->bar2_vaddr;
		*offset = address - pbar_info->bar2_dev_start;
	} else if (address >= pbar_info->bar0_dev_start &&
			address < pbar_info->bar0_dev_start + pbar_info->bar0_len) {
		*bar_vaddr = pbar_info->bar0_vaddr;
		*offset = address - pbar_info->bar0_dev_start;
	} else if (address >= pbar_info->bar1_dev_start &&
			address < pbar_info->bar1_dev_start + pbar_info->bar1_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
		*offset = address - pbar_info->bar1_dev_start;
	} else if (address >= pbar_info->bar1_part7_dev_start &&
			address < pbar_info->bar1_part7_dev_start + pbar_info->bar1_part7_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
		*offset = address - pbar_info->bar1_part7_dev_start + pbar_info->bar1_part7_offset;
	} else if (address >= pbar_info->bar1_part6_dev_start &&
			address < pbar_info->bar1_part6_dev_start + pbar_info->bar1_part6_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
		*offset = address - pbar_info->bar1_part6_dev_start + pbar_info->bar1_part6_offset;
	} else if (address >= pbar_info->bar1_part5_dev_start &&
			address < pbar_info->bar1_part5_dev_start + pbar_info->bar1_part5_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
		*offset = address - pbar_info->bar1_part5_dev_start + pbar_info->bar1_part5_offset;
	} else if (address >= pbar_info->bar1_part4_dev_start &&
			address < pbar_info->bar1_part4_dev_start + pbar_info->bar1_part4_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
		*offset = address - pbar_info->bar1_part4_dev_start + pbar_info->bar1_part4_offset;
	} else if (address >= pbar_info->bar1_part3_dev_start &&
			address < pbar_info->bar1_part3_dev_start + pbar_info->bar1_part3_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
		*offset = address - pbar_info->bar1_part3_dev_start + pbar_info->bar1_part3_offset;
	} else if (address >= pbar_info->bar1_part0_dev_start &&
			address < pbar_info->bar1_part0_dev_start + pbar_info->bar1_part0_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
		*offset = address - pbar_info->bar1_part0_dev_start + pbar_info->bar1_part0_offset;
	} else if (address >= pbar_info->bar1_part1_dev_start &&
			address < pbar_info->bar1_part1_dev_start + pbar_info->bar1_part1_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
		*offset = address - pbar_info->bar1_part1_dev_start + pbar_info->bar1_part1_offset;
	} else if (address >= pbar_info->bar1_part2_dev_start &&
			address < pbar_info->bar1_part2_dev_start + pbar_info->bar1_part2_len) {
		*bar_vaddr = pbar_info->bar1_vaddr;
		*offset = address - pbar_info->bar1_part2_dev_start + pbar_info->bar1_part2_offset;
	} else {
		pr_err("%s invalid address address = 0x%x\n", __func__, address);
	}
	//	pr_info("bar addr = 0x%p, offset = 0x%x, address 0x%x\n", *bar_vaddr, *offset, address);
}

void bm_get_bar_base(struct bm_bar_info *pbar_info, u32 address, u64 *base)
{
	/* Choose bar the address belongs to, and compute the offset on bar */
	if (address >= pbar_info->bar2_dev_start &&
			address < pbar_info->bar2_dev_start + pbar_info->bar2_len) {
		*base = pbar_info->bar2_start;
	} else if (address >= pbar_info->bar0_dev_start &&
			address < pbar_info->bar0_dev_start + pbar_info->bar0_len) {
		*base = pbar_info->bar0_start;
	} else if (address >= pbar_info->bar1_dev_start &&
			address < pbar_info->bar1_dev_start + pbar_info->bar1_len) {
		*base = pbar_info->bar1_start;
	} else if (address >= pbar_info->bar1_part6_dev_start &&
			address < pbar_info->bar1_part6_dev_start + pbar_info->bar1_part6_len) {
		*base = pbar_info->bar1_start;
	} else if (address >= pbar_info->bar1_part5_dev_start &&
			address < pbar_info->bar1_part5_dev_start + pbar_info->bar1_part5_len) {
		*base = pbar_info->bar1_start;
	} else if (address >= pbar_info->bar1_part4_dev_start &&
			address < pbar_info->bar1_part4_dev_start + pbar_info->bar1_part4_len) {
		*base = pbar_info->bar1_start;
	} else if (address >= pbar_info->bar1_part3_dev_start &&
			address < pbar_info->bar1_part3_dev_start + pbar_info->bar1_part3_len) {
		*base = pbar_info->bar1_start;
	} else if (address >= pbar_info->bar1_part0_dev_start &&
			address < pbar_info->bar1_part0_dev_start + pbar_info->bar1_part0_len) {
		*base = pbar_info->bar1_start;
	} else if (address >= pbar_info->bar1_part1_dev_start &&
			address < pbar_info->bar1_part1_dev_start + pbar_info->bar1_part1_len) {
		*base = pbar_info->bar1_start;
	} else if (address >= pbar_info->bar1_part2_dev_start &&
			address < pbar_info->bar1_part2_dev_start + pbar_info->bar1_part2_len) {
		*base = pbar_info->bar1_start;
	} else {
		pr_err("%s pcie mode invalid address\n", __func__);
	}
}
#endif

static void __iomem *bm_get_devmem_vaddr(struct bm_device_info *bmdi, u32 address)
{
	u32 offset = 0;
	void __iomem *bar_vaddr = NULL;
	struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

	bm_get_bar_offset(pbar_info, address, &bar_vaddr, &offset);
	return bar_vaddr + offset;
}

u32 bm_read32(struct bm_device_info *bmdi, u32 address)
{
	u32 offset = 0;
	void __iomem *bar_vaddr = NULL;
	struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

	bm_get_bar_offset(pbar_info, address, &bar_vaddr, &offset);
	return ioread32(bar_vaddr + offset);
}

u32 bm_write32(struct bm_device_info *bmdi, u32 address, u32 data)
{
	u32 offset = 0;
	void __iomem *bar_vaddr = NULL;
	struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

	bm_get_bar_offset(pbar_info, address, &bar_vaddr, &offset);
	iowrite32(data, bar_vaddr + offset);
	return 0;
}

u8 bm_read8(struct bm_device_info *bmdi, u32 address)
{
	return ioread8(bm_get_devmem_vaddr(bmdi, address));
}

u32 bm_write8(struct bm_device_info *bmdi, u32 address, u8 data)
{
	iowrite8(data, bm_get_devmem_vaddr(bmdi, address));
	return 0;
}

u16 bm_read16(struct bm_device_info *bmdi, u32 address)
{
	return ioread16(bm_get_devmem_vaddr(bmdi, address));
}

u32 bm_write16(struct bm_device_info *bmdi, u32 address, u16 data)
{
	iowrite16(data, bm_get_devmem_vaddr(bmdi, address));
	return 0;
}

u64 bm_read64(struct bm_device_info *bmdi, u32 address)
{
	u64 temp = 0;

	temp = bm_read32(bmdi, address + 4);
	temp = (u64)bm_read32(bmdi, address) | (temp << 32);
	return temp;
}

u64 bm_write64(struct bm_device_info *bmdi, u32 address, u64 data)
{
	bm_write32(bmdi, address, data & 0xFFFFFFFF);
	bm_write32(bmdi, address + 4, data >> 32);
	return 0;
}

void bm_reg_init_vaddr(struct bm_device_info *bmdi, u32 address, void __iomem **reg_base_vaddr)
{
	u32 offset = 0;
	struct bm_bar_info *pbar_info = &bmdi->cinfo.bar_info;

	if (address == 0) {
		*reg_base_vaddr = NULL;
		return;
	}
	bm_get_bar_offset(pbar_info, address, reg_base_vaddr, &offset);
	*reg_base_vaddr += offset;
	PR_TRACE("device address = 0x%x, vaddr = 0x%llx\n", address, *reg_base_vaddr);
}

/* Define register operation as a generic marco. it provide
 * xxx_reg_read/write wrapper, currently support read/write u32,
 * using bm_read/write APIs directly for access u16/u8.
 */
void dev_info_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val, int len)
{
	int i, j;
	char *p = (u8 *)&val;

	for (i = len - 1, j = 0; i >= 0; i--)
		iowrite8(*(p + i), bmdi->cinfo.bar_info.io_bar_vaddr.dev_info_bar_vaddr + reg_offset + j++);
}

u8 dev_info_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread8(bmdi->cinfo.bar_info.io_bar_vaddr.dev_info_bar_vaddr + reg_offset);
}

void shmem_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val, u32 channel)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.shmem_bar_vaddr + channel *
			(bmdi->cinfo.share_mem_size / BM_MSGFIFO_CHANNEL_NUM) * 4  + reg_offset);
}

u32 shmem_reg_read(struct bm_device_info *bmdi, u32 reg_offset, u32 channel)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.shmem_bar_vaddr + channel *
			(bmdi->cinfo.share_mem_size / BM_MSGFIFO_CHANNEL_NUM) * 4 + reg_offset);
}

void top_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.top_bar_vaddr + reg_offset);
}

u32 top_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.top_bar_vaddr + reg_offset);
}

void gp_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.gp_bar_vaddr + reg_offset);
}

u32 gp_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.gp_bar_vaddr + reg_offset);
}

void i2c_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.i2c_bar_vaddr + reg_offset);
}

u32 i2c_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.i2c_bar_vaddr + reg_offset);
}

/* smbus is i2c0 master in bm1684 */
void smbus_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.i2c_bar_vaddr - 0x2000 + reg_offset);
}

u32 smbus_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.i2c_bar_vaddr - 0x2000 + reg_offset);
}

void pwm_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.pwm_bar_vaddr + reg_offset);
}

u32 pwm_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.pwm_bar_vaddr + reg_offset);
}

void cdma_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.cdma_bar_vaddr + reg_offset);
}

u32 cdma_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.cdma_bar_vaddr + reg_offset);
}

void ddr_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.ddr_bar_vaddr + reg_offset);
}

u32 ddr_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.ddr_bar_vaddr + reg_offset);
}

void bdc_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.bdc_bar_vaddr + reg_offset);
}

u32 bdc_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.bdc_bar_vaddr + reg_offset);
}

void smmu_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.smmu_bar_vaddr + reg_offset);
}

u32 smmu_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.smmu_bar_vaddr + reg_offset);
}

void intc_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.intc_bar_vaddr + reg_offset);
}

u32 intc_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.intc_bar_vaddr + reg_offset);
}

void cfg_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.cfg_bar_vaddr + reg_offset);
}

u32 cfg_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.cfg_bar_vaddr + reg_offset);
}
void nv_timer_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.nv_timer_bar_vaddr + reg_offset);
}

u32 nv_timer_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.nv_timer_bar_vaddr + reg_offset);
}

void spi_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.spi_bar_vaddr + reg_offset);
}

u32 spi_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.spi_bar_vaddr + reg_offset);
}

void gpio_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.gpio_bar_vaddr + reg_offset);
}

u32 gpio_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.gpio_bar_vaddr + reg_offset);
}

void vpp0_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.vpp0_bar_vaddr + reg_offset);
}

u32 vpp0_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.vpp0_bar_vaddr + reg_offset);
}

void vpp1_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.vpp1_bar_vaddr + reg_offset);
}

u32 vpp1_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.vpp1_bar_vaddr + reg_offset);
}

void uart_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.uart_bar_vaddr + reg_offset);
}

u32 uart_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.uart_bar_vaddr + reg_offset);
}

void wdt_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.wdt_bar_vaddr + reg_offset);
}

u32 wdt_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.wdt_bar_vaddr + reg_offset);
}

void tpu_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.tpu_bar_vaddr + reg_offset);
}

u32 tpu_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.tpu_bar_vaddr + reg_offset);
}

void gdma_reg_write(struct bm_device_info *bmdi, u32 reg_offset, u32 val)
{
	iowrite32(val, bmdi->cinfo.bar_info.io_bar_vaddr.gdma_bar_vaddr + reg_offset);
}

u32 gdma_reg_read(struct bm_device_info *bmdi, u32 reg_offset)
{
	return ioread32(bmdi->cinfo.bar_info.io_bar_vaddr.gdma_bar_vaddr + reg_offset);
}

void io_init(struct bm_device_info *bmdi)
{
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->shmem_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.shmem_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->top_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.top_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->gp_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.gp_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->pwm_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.pwm_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->cdma_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.cdma_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->bdc_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.bdc_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->smmu_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.smmu_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->intc_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.intc_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->nv_timer_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.nv_timer_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->gpio_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.gpio_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->vpp0_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.vpp0_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->vpp1_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.vpp1_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->uart_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.uart_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->wdt_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.wdt_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->tpu_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.tpu_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->gdma_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.gdma_bar_vaddr);
#ifndef SOC_MODE
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->dev_info_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.dev_info_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->i2c_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.i2c_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->ddr_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.ddr_bar_vaddr);
	bm_reg_init_vaddr(bmdi, bmdi->cinfo.bm_reg->spi_base_addr, &bmdi->cinfo.bar_info.io_bar_vaddr.spi_bar_vaddr);
#endif
}

int bm_get_reg(struct bm_device_info *bmdi, struct bm_reg *reg)
{
	reg->reg_value = bm_read32(bmdi, reg->reg_addr);
	return 0;
}

int bm_set_reg(struct bm_device_info *bmdi, struct bm_reg *reg)
{
#ifdef PR_DEBUG
	bm_write32(bmdi, reg->reg_addr, reg->reg_value);
#endif
	return 0;
}
