#ifndef _BM_TIMER_H_
#define _BM_TIMER_H_
#define BM_TIMER_PERIOD_NS (40)
struct bm_device_info;
void bmdev_timer_init(struct bm_device_info *bmdi);
unsigned long bmdev_timer_get_cycle(struct bm_device_info *bmdi);
unsigned long bmdev_timer_get_time_ms(struct bm_device_info *bmdi);
unsigned long bmdev_timer_get_time_us(struct bm_device_info *bmdi);
#endif
