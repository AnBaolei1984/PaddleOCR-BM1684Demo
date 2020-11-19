#ifndef __BM1682_IRQ_H
#define __BM1682_IRQ_H

#ifndef SOC_MODE
void bm1682_get_irq_status(struct bm_device_info *bmdi, u32 *status);
#endif

#endif
