#ifndef _BM1684_CDMA_H_
#define _BM1684_CDMA_H_
#include "bm_cdma.h"
struct bm_device_info;
u32 bm1684_cdma_transfer(struct bm_device_info *bmdi, struct file *file,
		pbm_cdma_arg parg, bool lock_cdma);
void bm1684_clear_cdmairq(struct bm_device_info *bmdi);
#ifdef SOC_MODE
void bm1684_cdma_reset(struct bm_device_info *bmdi);
void bm1684_cdma_clk_enable(struct bm_device_info *bmdi);
void bm1684_cdma_clk_disable(struct bm_device_info *bmdi);
#endif
#endif
