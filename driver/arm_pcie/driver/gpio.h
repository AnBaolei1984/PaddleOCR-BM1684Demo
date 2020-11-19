#ifndef _BM_GPIO_H_
#define _BM_GPIO_H_

/*pinmux for gpio0~gpio31*/
#define PINMUXGPIO0             0x50010488
#define PINMUXGPIO1_GPIO2         0x5001048c
#define PINMUXGPIO3_GPIO4         0x50010490
#define PINMUXGPIO5_GPIO6         0x50010494
#define PINMUXGPIO7_GPIO8         0x50010498
#define PINMUXGPIO9_GPIO10         0x5001049c
#define PINMUXGPIO11_GPIO12         0x500104a0
#define PINMUXGPIO13_GPIO14         0x500104a4
#define PINMUXGPIO15_GPIO16         0x500104a8
#define PINMUXGPIO17_GPIO18         0x500104ac
#define PINMUXGPIO19_GPIO20         0x500104b0
#define PINMUXGPIO21_GPIO22         0x500104b4
#define PINMUXGPIO23_GPIO24         0x500104b8
#define PINMUXGPIO25_GPIO26         0x500104bc
#define PINMUXGPIO27_GPIO28         0x500104c0
#define PINMUXGPIO29_GPIO30         0x500104c4
#define PINMUXGPIO31                 0x500104c8

struct bm_device_info;
int bmdrv_gpio_init(struct bm_device_info *bmdi);
int bmdrv_pinmux_init(struct bm_device_info *bmdi);
void bm_gpio_request_irq(struct bm_device_info *bmdi);
void bm_gpio_free_irq(struct bm_device_info *bmdi);
#endif
