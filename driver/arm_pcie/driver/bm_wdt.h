#ifndef _BM_WDT_H_
#define _BM_WDT_H_

#ifndef SOC_MODE
struct bm_device_info;
int bmdrv_wdt_start(struct bm_device_info *bmdi);
#endif

#endif
