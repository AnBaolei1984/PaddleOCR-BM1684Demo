#include <linux/delay.h>
#include <linux/device.h>
#include "bm_common.h"
#include "spi.h"

/*spi register*/

#define SPI_CTRL               0x000
#define SPI_CE_CTRL            0x004
#define SPI_DLY_CTRL           0x008
#define SPI_DMMR               0x00c
#define SPI_TRANS_CSR          0x010
#define SPI_TRANS_NUM          0x014
#define SPI_FIFO_PORT          0x018
#define SPI_FIFO_PT            0x020
#define SPI_INT_STS            0x028
#define SPI_INT_EN             0x02c

static int spi_data_in_tran(struct bm_device_info *bmdi, u8 *dst_buf, u8 *cmd_buf,
	int with_cmd, int addr_bytes, int data_bytes)
{
	u32 *p_data = (u32 *)cmd_buf;
	u32 tran_csr = 0;
	int cmd_bytes = addr_bytes + ((with_cmd) ? 1 : 0);
	int i, xfer_size, off;
	int wait_cnt = 1000;

	if (data_bytes > 65535) {
		dev_err(bmdi->cinfo.device, "SPI data in overflow, should be less than 65535 bytes(%d)\n", data_bytes);
		return -1;
	}

	/* init tran_csr */
	tran_csr = spi_reg_read(bmdi, SPI_TRANS_CSR);
	tran_csr &= ~(BIT_SPI_TRAN_CSR_TRAN_MODE_MASK
			| BIT_SPI_TRAN_CSR_ADDR_BYTES_MASK
			| BIT_SPI_TRAN_CSR_FIFO_TRG_LVL_MASK
			| BIT_SPI_TRAN_CSR_WITH_CMD);
	tran_csr |= (addr_bytes << SPI_TRAN_CSR_ADDR_BYTES_SHIFT);
	tran_csr |= (with_cmd) ? BIT_SPI_TRAN_CSR_WITH_CMD : 0;
	tran_csr |= BIT_SPI_TRAN_CSR_FIFO_TRG_LVL_8_BYTE;
	tran_csr |= BIT_SPI_TRAN_CSR_TRAN_MODE_RX;

	spi_reg_write(bmdi, SPI_FIFO_PT, 0); // flush FIFO before filling fifo
	if (with_cmd) {
		for (i = 0; i < ((cmd_bytes - 1) / 4 + 1); i++)
			spi_reg_write(bmdi, SPI_FIFO_PORT, p_data[i]);
	}

	/* issue tran */
	spi_reg_write(bmdi, SPI_INT_STS, 0); // clear all int
	spi_reg_write(bmdi, SPI_TRANS_NUM, data_bytes);
	tran_csr |= BIT_SPI_TRAN_CSR_GO_BUSY;
	spi_reg_write(bmdi, SPI_TRANS_CSR, tran_csr);

	/* check rd int to make sure data out done and in data started */
	while ((spi_reg_read(bmdi, SPI_INT_STS) & BIT_SPI_INT_RD_FIFO) == 0 &&
			wait_cnt--)
		udelay(1);
	if (wait_cnt == 0) {
		dev_err(bmdi->cinfo.device, "SPI read int timeout!\n");
		return -EBUSY;
	}

	/* get data */
	p_data = (u32 *)dst_buf;
	off = 0;
	while (off < data_bytes) {
		if (data_bytes - off >= SPI_MAX_FIFO_DEPTH)
			xfer_size = SPI_MAX_FIFO_DEPTH;
		else
			xfer_size = data_bytes - off;

		wait_cnt = 3000;
		while ((spi_reg_read(bmdi, SPI_FIFO_PT) & 0xF) != xfer_size && wait_cnt--)
			udelay(1);
		if (wait_cnt == 0) {
			dev_err(bmdi->cinfo.device, "SPI wait to read FIFO timeout!\n");
			return -EBUSY;
		}
		for (i = 0; i < ((xfer_size - 1) / 4 + 1); i++)
			p_data[off / 4 + i] = spi_reg_read(bmdi, SPI_FIFO_PORT);
		off += xfer_size;
	}

	wait_cnt = 1000;
	/* wait tran done */
	while ((spi_reg_read(bmdi, SPI_INT_STS) & BIT_SPI_INT_TRAN_DONE) == 0 &&
			wait_cnt--)
		udelay(1);
	if (wait_cnt == 0) {
		dev_err(bmdi->cinfo.device, "SPI wait tran timeout!\n");
		return -EBUSY;
	}
	spi_reg_write(bmdi, SPI_FIFO_PT, 0);    //should flush FIFO after tran
	return 0;
}

static int spi_data_read(struct bm_device_info *bmdi, u8 *dst_buf, int addr, int size)
{
	u8 cmd_buf[4];

	cmd_buf[0] = SPI_CMD_READ;
	cmd_buf[1] = ((addr) >> 16) & 0xFF;
	cmd_buf[2] = ((addr) >> 8) & 0xFF;
	cmd_buf[3] = (addr) & 0xFF;
	return spi_data_in_tran(bmdi, dst_buf, cmd_buf, 1, 3, size);
}

int bm_spi_data_read(struct bm_device_info *bmdi, u8 *dst_buf, int addr, int size)
{
	return spi_data_read(bmdi, dst_buf, (int)addr, size);
}

int bm_spi_flash_read_sector(struct bm_device_info *bmdi, u32 addr, u8 *buf)
{
	return spi_data_read(bmdi, buf, (int)addr, 256);
}

void bm_spi_init(struct bm_device_info *bmdi)
{
	u32 tran_csr = 0;
	// disable DMMR (direct memory mapping read)
	spi_reg_write(bmdi, SPI_DMMR, 0);
	// soft reset
	spi_reg_write(bmdi, SPI_CTRL, spi_reg_read(bmdi, SPI_CTRL)
		| BIT_SPI_CTRL_SRST | 0x3);

	tran_csr |= (0x03 << SPI_TRAN_CSR_ADDR_BYTES_SHIFT);
	tran_csr |= BIT_SPI_TRAN_CSR_FIFO_TRG_LVL_4_BYTE;
	tran_csr |= BIT_SPI_TRAN_CSR_WITH_CMD;
	spi_reg_write(bmdi, SPI_TRANS_CSR, tran_csr);
}

void bm_spi_enable_dmmr(struct bm_device_info *bmdi)
{
	spi_reg_write(bmdi, SPI_DMMR, 1);
}

/* here are APIs for SPI flash programming */
static int spi_non_data_tran(struct bm_device_info *bmdi, u8 *cmd_buf,
	u32 with_cmd, u32 addr_bytes)
{
	u32 *p_data = (u32 *)cmd_buf;
	u32 tran_csr = 0;
	int wait_cnt = 1000;

	if (addr_bytes > 3) {
		dev_err(bmdi->cinfo.device, "non-data: addr bytes should be less than 3 (%d)\n", addr_bytes);
		return -1;
	}

	/* init tran_csr */
	tran_csr = spi_reg_read(bmdi, SPI_TRANS_CSR);
	tran_csr &= ~(BIT_SPI_TRAN_CSR_TRAN_MODE_MASK
		| BIT_SPI_TRAN_CSR_ADDR_BYTES_MASK
		| BIT_SPI_TRAN_CSR_FIFO_TRG_LVL_MASK
		| BIT_SPI_TRAN_CSR_WITH_CMD);
	tran_csr |= (addr_bytes << SPI_TRAN_CSR_ADDR_BYTES_SHIFT);
	tran_csr |= BIT_SPI_TRAN_CSR_FIFO_TRG_LVL_1_BYTE;
	tran_csr |= (with_cmd ? BIT_SPI_TRAN_CSR_WITH_CMD : 0);

	spi_reg_write(bmdi, SPI_FIFO_PT, 0); //do flush FIFO before filling fifo

	spi_reg_write(bmdi, SPI_FIFO_PORT, p_data[0]);

	/* issue tran */
	spi_reg_write(bmdi, SPI_INT_STS, 0);
	tran_csr |= BIT_SPI_TRAN_CSR_GO_BUSY;
	spi_reg_write(bmdi, SPI_TRANS_CSR, tran_csr);

	/* wait tran done */
	while ((spi_reg_read(bmdi, SPI_INT_STS) & BIT_SPI_INT_TRAN_DONE) == 0 &&
			wait_cnt--)
		udelay(1);
	if (wait_cnt == 0) {
		dev_err(bmdi->cinfo.device, "SPI wait tran timeout!\n");
		return -EBUSY;
	}
	spi_reg_write(bmdi, SPI_FIFO_PT, 0); //do flush FIFO before filling fifo
	return 0;
}

static int spi_data_out_tran(struct bm_device_info *bmdi, u8 *src_buf, u8 *cmd_buf,
	u32 with_cmd, u32 addr_bytes, u32 data_bytes)
{
	u32 *p_data = (u32 *)cmd_buf;
	u32 tran_csr = 0;
	u32 cmd_bytes = addr_bytes + (with_cmd ? 1 : 0);
	u32 xfer_size, off;
	int i;
	int wait_cnt = 1000;

	if (data_bytes > 65535) {
		dev_err(bmdi->cinfo.device, "data out overflow, should be less than 65535 bytes(%d)\n", data_bytes);
		return -1;
	}

	/* init tran_csr */
	tran_csr = spi_reg_read(bmdi, SPI_TRANS_CSR);
	tran_csr &= ~(BIT_SPI_TRAN_CSR_TRAN_MODE_MASK
		| BIT_SPI_TRAN_CSR_ADDR_BYTES_MASK
		| BIT_SPI_TRAN_CSR_FIFO_TRG_LVL_MASK
		| BIT_SPI_TRAN_CSR_WITH_CMD);
	tran_csr |= (addr_bytes << SPI_TRAN_CSR_ADDR_BYTES_SHIFT);
	tran_csr |= (with_cmd ? BIT_SPI_TRAN_CSR_WITH_CMD : 0);
	tran_csr |= BIT_SPI_TRAN_CSR_FIFO_TRG_LVL_8_BYTE;
	tran_csr |= BIT_SPI_TRAN_CSR_TRAN_MODE_TX;

	spi_reg_write(bmdi, SPI_FIFO_PT, 0); //do flush FIFO before filling fifo
	if (with_cmd) {
		for (i = 0; i < ((cmd_bytes - 1) / 4 + 1); i++)
			spi_reg_write(bmdi, SPI_FIFO_PORT, p_data[i]);
	}

	/* issue tran */
	spi_reg_write(bmdi, SPI_INT_STS, 0);
	spi_reg_write(bmdi, SPI_TRANS_NUM, data_bytes);
	tran_csr |= BIT_SPI_TRAN_CSR_GO_BUSY;
	spi_reg_write(bmdi, SPI_TRANS_CSR, tran_csr);
	while ((spi_reg_read(bmdi, SPI_FIFO_PT) & 0xF) != 0 &&
			wait_cnt--)
		udelay(1);
	if (wait_cnt == 0) {
		dev_err(bmdi->cinfo.device, "SPI issue tran timeout!\n");
		return -EBUSY;
	}

	/* fill data */
	p_data = (u32 *)src_buf;
	off = 0;
	while (off < data_bytes) {
		if (data_bytes - off >= SPI_MAX_FIFO_DEPTH)
			xfer_size = SPI_MAX_FIFO_DEPTH;
		else
			xfer_size = data_bytes - off;

		wait_cnt = 3000;
		while ((spi_reg_read(bmdi, SPI_FIFO_PT) & 0xF) != 0
				&& wait_cnt--) {
			udelay(10);
		}
		if (wait_cnt == 0) {
			dev_err(bmdi->cinfo.device, "wait to write FIFO timeout!\n");
			return -EBUSY;
		}

		for (i = 0; i < ((xfer_size - 1) / 4 + 1); i++)
			spi_reg_write(bmdi, SPI_FIFO_PORT, p_data[off / 4 + i]);

		off += xfer_size;
	}

	wait_cnt = 1000;
	/* wait tran done */
	while ((spi_reg_read(bmdi, SPI_INT_STS) & BIT_SPI_INT_TRAN_DONE) == 0
			&& wait_cnt--)
		udelay(1);
	if (wait_cnt == 0) {
		dev_err(bmdi->cinfo.device, "SPI wait tran timeout!\n");
		return -EBUSY;
	}
	spi_reg_write(bmdi, SPI_FIFO_PT, 0); //should flush FIFO after tran
	return 0;
}

/*
 * spi_in_out_tran is a workaround fucntion for current 32-bit access to spic fifo:
 * AHB bus could only do 32-bit access to spic fifo, so cmd without 3-bytes addr will leave 3-byte
 * data in fifo, so set tx to mark that these 3-bytes data would be sent out.
 * So send_bytes should be 3 (wirte 1 dw into fifo) or 7(write 2 dw), get_bytes sould be the same value.
 * software would mask out unuseful data in get_bytes.
 */
static int spi_in_out_tran(struct bm_device_info *bmdi, u8 *dst_buf, u8 *src_buf,
	u32 with_cmd, u32 addr_bytes, u32 send_bytes, u32 get_bytes)
{
	u32 *p_data = (u32 *)src_buf;
	u32 total_out_bytes;
	u32 tran_csr = 0;
	int i;
	int wait_cnt = 1000;

	if (send_bytes != get_bytes) {
		dev_err(bmdi->cinfo.device, "data in&out: get_bytes should be the same as send_bytes\n");
		return -1;
	}

	if ((send_bytes > SPI_MAX_FIFO_DEPTH) || (get_bytes > SPI_MAX_FIFO_DEPTH)) {
		dev_err(bmdi->cinfo.device, "data in&out: FIFO will overflow\n");
		return -1;
	}

	/* init tran_csr */
	tran_csr = spi_reg_read(bmdi, SPI_TRANS_CSR);
	tran_csr &= ~(BIT_SPI_TRAN_CSR_TRAN_MODE_MASK
		| BIT_SPI_TRAN_CSR_ADDR_BYTES_MASK
		| BIT_SPI_TRAN_CSR_FIFO_TRG_LVL_MASK
		| BIT_SPI_TRAN_CSR_WITH_CMD);
	tran_csr |= (addr_bytes << SPI_TRAN_CSR_ADDR_BYTES_SHIFT);
	tran_csr |= BIT_SPI_TRAN_CSR_FIFO_TRG_LVL_1_BYTE;
	tran_csr |= BIT_SPI_TRAN_CSR_WITH_CMD;
	tran_csr |= BIT_SPI_TRAN_CSR_TRAN_MODE_TX;
	tran_csr |= BIT_SPI_TRAN_CSR_TRAN_MODE_RX;

	spi_reg_write(bmdi, SPI_FIFO_PT, 0); //do flush FIFO before filling fifo
	total_out_bytes = addr_bytes + send_bytes + (with_cmd ? 1 : 0);
	for (i = 0; i < ((total_out_bytes - 1) / 4 + 1); i++)
		spi_reg_write(bmdi, SPI_FIFO_PORT, p_data[i]);

	/* issue tran */
	spi_reg_write(bmdi, SPI_INT_STS, 0); //clear all int
	spi_reg_write(bmdi, SPI_TRANS_NUM, get_bytes);
	tran_csr |= BIT_SPI_TRAN_CSR_GO_BUSY;
	spi_reg_write(bmdi, SPI_TRANS_CSR, tran_csr);

	/* wait tran done and get data */
	while ((spi_reg_read(bmdi, SPI_INT_STS) & BIT_SPI_INT_TRAN_DONE) == 0
			&& wait_cnt--)
		udelay(1);
	if (wait_cnt == 0) {
		dev_err(bmdi->cinfo.device, "SPI wait tran timeout!\n");
		return -EBUSY;
	}

	p_data = (u32 *)dst_buf;
	for (i = 0; i < ((get_bytes - 1) / 4 + 1); i++)
		p_data[i] = spi_reg_read(bmdi, SPI_FIFO_PORT);

	spi_reg_write(bmdi, SPI_FIFO_PT, 0); //should flush FIFO after tran
	return 0;
}

static int spi_write_en(struct bm_device_info *bmdi)
{
	u8 cmd_buf[4];

	memset(cmd_buf, 0, sizeof(cmd_buf));
	cmd_buf[0] = SPI_CMD_WREN;
	return spi_non_data_tran(bmdi, cmd_buf, 1, 0);
}

static int spi_read_status(struct bm_device_info *bmdi)
{
	u8 cmd_buf[4];
	u8 data_buf[4];

	memset(cmd_buf, 0, sizeof(cmd_buf));
	memset(data_buf, 0, sizeof(data_buf));

	cmd_buf[0] = SPI_CMD_RDSR;
	spi_in_out_tran(bmdi, data_buf, cmd_buf, 1, 0, 3, 3);

	return data_buf[0];
}

u32 bm_spi_read_id(struct bm_device_info *bmdi)
{
	u8 cmd_buf[4];
	u8 data_buf[4];
	u32 read_id = 0;

	memset(cmd_buf, 0, sizeof(cmd_buf));
	memset(data_buf, 0, sizeof(data_buf));

	cmd_buf[0] = SPI_CMD_RDID;
	spi_in_out_tran(bmdi, data_buf, cmd_buf, 1, 0, 3, 3);
	read_id = (data_buf[2] << 16) | (data_buf[1] << 8) | (data_buf[0]);
	return read_id;
}

static int spi_page_program(struct bm_device_info *bmdi, u8 *src_buf, u32 addr, u32 size)
{
	u8 cmd_buf[4];

	cmd_buf[0] = SPI_CMD_PP;
	cmd_buf[1] = (addr >> 16) & 0xFF;
	cmd_buf[2] = (addr >> 8) & 0xFF;
	cmd_buf[3] = addr & 0xFF;

	spi_data_out_tran(bmdi, src_buf, cmd_buf, 1, 3, size);
	return 0;
}

static void spi_sector_erase(struct bm_device_info *bmdi, u32 addr)
{
	u8 cmd_buf[4];

	cmd_buf[0] = SPI_CMD_SE;
	cmd_buf[1] = (addr >> 16) & 0xFF;
	cmd_buf[2] = (addr >> 8) & 0xFF;
	cmd_buf[3] = addr & 0xFF;

	spi_non_data_tran(bmdi, cmd_buf, 1, 3);
}

int bm_spi_flash_erase_sector(struct bm_device_info *bmdi, u32 addr)
{
	u32 spi_status;
	int wait_cnt = 0;

	spi_write_en(bmdi);
	spi_status = spi_read_status(bmdi);
	if ((spi_status & SPI_STATUS_WEL) == 0) {
		dev_err(bmdi->cinfo.device, "write en failed, get status: 0x%x\n", spi_status);
		return -1;
	}

	spi_sector_erase(bmdi, addr);

	while (1) {
		mdelay(100);
		spi_status = spi_read_status(bmdi);
		if (((spi_status & SPI_STATUS_WIP) == 0) || (wait_cnt > 30)) { // 3s, spec 0.15~1s
			pr_info("sector erase done, get status: 0x%x, wait: %d\n", spi_status, wait_cnt);
			break;
		}
		wait_cnt++;
	}
	return 0;
}

int bm_spi_flash_program_sector(struct bm_device_info *bmdi, u32 addr)
{
	u8 cmd_buf[256], spi_status;
	u32 wait_cnt = 0;

	memset(cmd_buf, 0x5A, sizeof(cmd_buf));
	spi_write_en(bmdi);

	spi_status = spi_read_status(bmdi);
	if (spi_status != 0x02) {
		dev_err(bmdi->cinfo.device, "spi status check failed, get status: 0x%x\n", spi_status);
		return -1;
	}

	spi_page_program(bmdi, cmd_buf, addr, sizeof(cmd_buf));

	while (1) {
		udelay(100);
		spi_status = spi_read_status(bmdi);
		if (((spi_status & SPI_STATUS_WIP) == 0) || (wait_cnt > 600)) { // 60ms, spec 120~2800us
			PR_TRACE("sector prog done, get status: 0x%x\n", spi_status);
			break;
		}
		wait_cnt++;
	}
	return 0;
}

static int do_page_program(struct bm_device_info *bmdi, u8 *src_buf, u32 addr, u32 size)
{
	u8 spi_status;
	u32 wait_cnt = 0;

	if (size > SPI_FLASH_BLOCK_SIZE) {
		dev_err(bmdi->cinfo.device, "size larger than a page\n");
		return -1;
	}
	if ((addr % SPI_FLASH_BLOCK_SIZE) != 0) {
		dev_err(bmdi->cinfo.device, "addr not alignned to page\n");
		return -1;
	}

	spi_write_en(bmdi);

	spi_status = spi_read_status(bmdi);
	if (spi_status != 0x02) {
		dev_err(bmdi->cinfo.device, "spi status check failed, get status: 0x%x\n", spi_status);
		return -1;
	}

	spi_page_program(bmdi, src_buf, addr, size);

	while (1) {
		udelay(100);
		spi_status = spi_read_status(bmdi);
		if (((spi_status & SPI_STATUS_WIP) == 0) || (wait_cnt > 600)) { // 60ms, spec 120~2800us
			pr_info("page prog done, get status: 0x%x\n", spi_status);
			break;
		}
		wait_cnt++;
	}
	return 0;
}

int bm_spi_flash_program(struct bm_device_info *bmdi, u8 *src_buf, u32 base, u32 size)
{
	u32 xfer_size, off, cmp_ret;
	u8 cmp_buf[SPI_FLASH_BLOCK_SIZE], erased_sectors, i;
	u32 id, sector_size;

	id = bm_spi_read_id(bmdi);
	if (id == SPI_ID_M25P128)
		sector_size = 256 * 1024;
	else if (id == SPI_ID_N25Q128 || id == SPI_ID_GD25LQ128)
		sector_size = 64 * 1024;
	else {
		dev_err(bmdi->cinfo.device, "unrecognized flash ID 0x%x\n", id);
		return -EINVAL;
	}

	if ((base % sector_size) != 0) {
		dev_err(bmdi->cinfo.device, "<flash offset addr> is not aligned erase sector size (0x%x)!\n", sector_size);
		return -EINVAL;
	}

	erased_sectors = (size + sector_size) / sector_size;
	pr_info("Start erasing %d sectors, each %d bytes...\n", erased_sectors, sector_size);

	for (i = 0; i < erased_sectors; i++)
		bm_spi_flash_erase_sector(bmdi, base + i * sector_size);

	pr_info("--program boot fw, page size %d\n", SPI_FLASH_BLOCK_SIZE);

	off = 0;
	i = 0;
	while (off < size) {
		if ((size - off) >= SPI_FLASH_BLOCK_SIZE)
			xfer_size = SPI_FLASH_BLOCK_SIZE;
		else
			xfer_size = size - off;

		if (do_page_program(bmdi, src_buf + off, base + off, xfer_size) != 0) {
			dev_err(bmdi->cinfo.device, "page prog failed @ 0x%x\n", base + off);
			return -1;
		}

		spi_data_read(bmdi, cmp_buf, base + off, xfer_size);
		cmp_ret = memcmp(src_buf + off, cmp_buf, xfer_size);
		if (cmp_ret != 0) {
			dev_err(bmdi->cinfo.device, "memcmp failed\n");
			return cmp_ret;
		}
		off += xfer_size;

		pr_info(".");
		if (++i % 32 == 0) {
			pr_info("\n");
			i = 0;
		}
	}
	pr_info("--program boot fw success\n");
	return 0;
}

#define BOOT_INFO_SPI_ADDR  (1024*1024*2)

int bm_spi_flash_get_boot_info(struct bm_device_info *bmdi, struct bm_boot_info *boot_info)
{
	int ret = 0;
	u32 size = sizeof(struct bm_boot_info);
	u32 boot_info_spi_addr = BOOT_INFO_SPI_ADDR;

	memset(boot_info, 0x0, size);

	bm_spi_init(bmdi);
	ret = spi_data_read(bmdi, (u8 *)boot_info, boot_info_spi_addr, size);
	bm_spi_enable_dmmr(bmdi);
	return ret;
}

int bm_spi_flash_update_boot_info(struct bm_device_info *bmdi, struct bm_boot_info *boot_info)
{
	int ret = 0;
	u32 size = sizeof(struct bm_boot_info);
	u32 boot_info_spi_addr = BOOT_INFO_SPI_ADDR;

	bm_spi_init(bmdi);
	ret = bm_spi_flash_program(bmdi, (u8 *)boot_info, boot_info_spi_addr, size);
	bm_spi_enable_dmmr(bmdi);
	return ret;
}
