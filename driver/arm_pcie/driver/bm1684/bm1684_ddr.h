
#ifndef _BM1684_DDR_H_
#define _BM1684_DDR_H_

struct bm_device_info;
int bm1684_ddr_top_init(struct bm_device_info *bmdi);
void ddr_phy_reg_write16(struct bm_device_info *bmdi, u32 addr, u32 value);
u16 ddr_phy_reg_read16(struct bm_device_info *bmdi, u32 addr);
void bm1684_disable_ddr_interleave(struct bm_device_info *bmdi);
void bm1684_enable_ddr_interleave(struct bm_device_info *bmdi);
void enable_ddr_refresh_sync_d0a_d0b_d1_d2(struct bm_device_info *bmdi);
void enable_ddr_refresh_sync_d0a_d0b(struct bm_device_info *bmdi);
void bm1684_ddr0a_ecc_irq_handler(struct bm_device_info *bmdi);
void bm1684_ddr0b_ecc_irq_handler(struct bm_device_info *bmdi);
void bm1684_ddr_ecc_request_irq(struct bm_device_info *bmdi);
void bm1684_ddr_ecc_free_irq(struct bm_device_info *bmdi);
int bm1684_pld_ddr_top_init(struct bm_device_info *bmdi);
int bm1684_lpddr4_init(struct bm_device_info *bmdi);
int bm1684_ddr_format_by_cdma(struct bm_device_info *bmdi);
#endif
/*******************************************************/

