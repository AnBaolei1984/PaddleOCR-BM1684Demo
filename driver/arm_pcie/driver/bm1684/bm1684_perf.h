#ifndef _BM1684_PERF_H_
#define _BM1684_PERF_H_
struct bm_device_info;
struct bm_perf_monitor;
void bm1684_enable_tpu_perf_monitor(struct bm_device_info *bmdi, struct bm_perf_monitor *perf_monitor);
void bm1684_disable_tpu_perf_monitor(struct bm_device_info *bmdi);
void bm1684_enable_gdma_perf_monitor(struct bm_device_info *bmdi, struct bm_perf_monitor *perf_monitor);
void bm1684_disable_gdma_perf_monitor(struct bm_device_info *bmdi);
#endif
