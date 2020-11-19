#ifndef _BM_ATTR_H_
#define _BM_ATTR_H_
#include "bm_common.h"

#define FAN_PWM_PERIOD 4000
#define LED_PWM_PERIOD 100000000UL  // p_clk 100MHz
#define BM_THERMAL_WINDOW_WIDTH 5

#define LED_OFF 0
#define LED_ON  1
#define LED_BLINK_FAST 2
#define LED_BLINK_ONE_TIMES_PER_S   3
#define LED_BLINK_ONE_TIMES_PER_2S   4
#define LED_BLINK_THREE_TIMES_PER_S   5

struct bm_thermal_info {
	int elapsed_temp[BM_THERMAL_WINDOW_WIDTH];
	int idx;
	int max_clk_tmp;
	int half_clk_tmp;
	int min_clk_tmp;
};

struct bm_chip_attr {
	u16 fan_speed;
	u16 fan_rev_read;
	atomic_t npu_utilization;
	int npu_cnt;
	int npu_busy_cnt;
	int npu_timer_interval;
	u64 npu_busy_time_sum_ms;
	u64 npu_start_probe_time;
#define NPU_STAT_WINDOW_WIDTH 100
	int npu_status[NPU_STAT_WINDOW_WIDTH];
	int npu_status_idx;
	struct mutex attr_mutex;
	struct work_struct attr_work;
	struct timer_list attr_timer_temp;
	struct timer_list attr_timer_npu;
	atomic_t timer_on;
	bool fan_control;
	int led_status;
	struct bm_thermal_info thermal_info;

	int (*bm_card_attr_init)(struct bm_device_info *);
	void (*bm_card_attr_deinit)(struct bm_device_info *);
	int (*bm_get_chip_temp)(struct bm_device_info *, int *);
	int (*bm_get_board_temp)(struct bm_device_info *, int *);
	int (*bm_get_tpu_power)(struct bm_device_info *, u32 *);
	int (*bm_get_board_power)(struct bm_device_info *, u32 *);
	int (*bm_get_fan_speed)(struct bm_device_info *);
	int (*bm_get_npu_util)(struct bm_device_info *);

	int (*bm_set_led_status)(struct bm_device_info *, int);
	int last_valid_tpu_power;
	int last_valid_tpu_volt;
	int last_valid_tpu_curr;
};

int bmdrv_card_attr_init(struct bm_device_info *bmdi);
void bmdrv_card_attr_deinit(struct bm_device_info *bmdi);
int bmdrv_enable_attr(struct bm_device_info *bmdi);
int bmdrv_disable_attr(struct bm_device_info *bmdi);
#ifndef SOC_MODE
int bm_read_tmp451_local_temp(struct bm_device_info *bmdi, int *temp);
int bm_read_tmp451_remote_temp(struct bm_device_info *bmdi, int *temp);
int bm_read_vdd_tpu_power(struct bm_device_info *, u32 *);
int bm_read_vdd_tpu_mem_power(struct bm_device_info *, u32 *);
int bm_read_vddc_power(struct bm_device_info *, u32 *);
int bm_read_sc5_power(struct bm_device_info *, u32 *);
int bm_read_sc5_plus_power(struct bm_device_info *, u32 *);
int bm_read_mcu_current(struct bm_device_info *, u8, u32 *);
int bm_read_mcu_voltage(struct bm_device_info *, u8, u32 *);
int bm_read_mcu_chip_temp(struct bm_device_info *, int *);
int bm_read_mcu_board_temp(struct bm_device_info *, int *);
int bm_read_vdd_tpu_current(struct bm_device_info *, u32 *);
int bm_read_vdd_tpu_voltage(struct bm_device_info *, u32 *);
int bm_read_vdd_pmu_tpu_temp(struct bm_device_info *, u32 *);
int bm_read_vdd_tpu_mem_current(struct bm_device_info *, u32 *);
int bm_read_vdd_tpu_mem_voltage(struct bm_device_info *, u32 *);
int bm_read_vdd_pmu_tpu_mem_temp(struct bm_device_info *, u32 *);
int bm_read_vddc_current(struct bm_device_info *, u32 *);
int bm_read_vddc_voltage(struct bm_device_info *, u32 *);
int bm_read_vddc_pmu_vddc_temp(struct bm_device_info *, u32 *);
int bm_burning_info_sn(struct bm_device_info *, unsigned long);
int bm_burning_info_mac(struct bm_device_info *, int, unsigned long);
int bm_burning_info_board_type(struct bm_device_info *, unsigned long);
int bm_get_sn(struct bm_device_info *, char *);
int bm_get_board_type(struct bm_device_info *, char *);
#endif
int bm_read_npu_util(struct bm_device_info *bmdi);
void bmdrv_adjust_fan_speed(struct bm_device_info *bmdi, u32 temp);
int bm_get_fan_speed(struct bm_device_info *bmdi);
int set_fan_speed(struct bm_device_info *bmdi, u16 spd);
int reset_fan_speed(struct bm_device_info *bmdi);
int set_led_status(struct bm_device_info *bmdi, int);
int set_ecc(struct bm_device_info *bmdi, int ecc_able);
#endif
