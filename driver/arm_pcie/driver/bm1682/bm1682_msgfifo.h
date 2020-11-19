#ifndef _BM1682_MSGFIFO_H_
#define _BM1682_MSGFIFO_H_
struct bm_device_info;
int bm1682_clear_msgirq(struct bm_device_info *bmdi);
u32 bm1682_pending_msgirq_cnt(struct bm_device_info *bmdi);
#endif
