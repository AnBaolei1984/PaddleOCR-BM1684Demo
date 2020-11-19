#ifndef _BM1682_DDR_H_
#define _BM1682_DDR_H_

#define DDR_INTERLEAVE_ENABLE
#define CONFIG_DDR_CLK 2400

struct bm_device_info;
int bm1682_ddr_top_init(struct bm_device_info *bmdi);

#endif
