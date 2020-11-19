#include "bm_common.h"

/*pwm register*/

#define PWM_HLPERIOD0          0x000
#define PWM_PERIOD0            0x004
#define PWM_HLPERIOD1          0x008
#define PWM_PERIOD1            0x00c
#define PWM_HLPERIOD2          0x010
#define PWM_PERIOD2            0x014
#define PWM_HLPERIOD3          0x018
#define PWM_PERIOD3            0x01c
#define FREQ0NUM	       0x020
#define FREQ1NUM               0x028
int set_pwm_level(struct bm_device_info *bmdi, u32 period, u16 hi_perc, u32 index)
{
	u32 hi_period;

	if (index > 0x3)
		return 0;

	if (hi_perc > 100) {
		pr_err("bmdrv: input pwm high percentage incorrect.\n");
		return -1;
	}

	hi_period = (u64)hi_perc * period / 100;
	pwm_reg_write(bmdi, PWM_HLPERIOD0 + index * 8, hi_period);
	pwm_reg_write(bmdi, PWM_PERIOD0 + index * 8, period);
	pwm_reg_write(bmdi, FREQ0NUM + index * 8, 0x5F5E100);  //The time window width 1000ms and p_clk=100MHZ
	return 0;
}

int set_pwm_high(struct bm_device_info *bmdi, u32 index)
{
	if (index > 0x3)
		return 0;

	pwm_reg_write(bmdi, PWM_HLPERIOD0 + index * 8, 0);
	pwm_reg_write(bmdi, PWM_PERIOD0 + index * 8, 0);
	pwm_reg_write(bmdi, FREQ0NUM + index * 8, 0x0);
	return 0;
}

int set_pwm_low(struct bm_device_info *bmdi, u32 index)
{
	if (index > 0x3)
		return 0;

	pwm_reg_write(bmdi, PWM_HLPERIOD0 + index * 8, 1);
	pwm_reg_write(bmdi, PWM_PERIOD0 + index * 8, 0);
	pwm_reg_write(bmdi, FREQ0NUM + index * 8, 0x0);
	return 0;
}
