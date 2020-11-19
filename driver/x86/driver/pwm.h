#ifndef _BM_PWM_H_
#define _BM_PWM_H_
struct bm_device_info;
int set_pwm_level(struct bm_device_info *, u32, u16, u32);
int set_pwm_high(struct bm_device_info *, u32);
int set_pwm_low(struct bm_device_info *, u32);
#endif

