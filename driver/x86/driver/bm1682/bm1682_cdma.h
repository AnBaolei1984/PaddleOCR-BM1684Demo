#ifndef _BM1682_CDMA_H_
#define _BM1682_CDMA_H_
#include "bm_cdma.h"
struct bm_device_info;
u32 bm1682_cdma_transfer(struct bm_device_info *bmdi, struct file *file,
		pbm_cdma_arg parg, bool lock_cdma);
void bm1682_clear_cdmairq(struct bm_device_info *bmdi);
#endif
