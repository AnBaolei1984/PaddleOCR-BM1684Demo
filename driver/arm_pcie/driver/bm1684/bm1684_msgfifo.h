#ifndef _BM1684_MSGFIFO_H_
#define _BM1684_MSGFIFO_H_
struct bm_device_info;
u32 bm1684_pending_msgirq_cnt(struct bm_device_info *bmdi);
int bm1684_clear_msgirq(struct bm_device_info *bmdi);
#endif
