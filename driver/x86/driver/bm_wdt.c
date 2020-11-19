#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>

#include "bm_common.h"
#include "bm_io.h"
#include "bm1684_reg.h"

#define WDOG_CONTROL_REG_OFFSET		    0x00
#define WDOG_CONTROL_REG_WDT_EN_MASK	    0x01
#define WDOG_CONTROL_REG_WDT_EN_INTRT	    0x02
#define WDOG_CONTROL_REG_WDT_RT_16T_PERIOD	0x0C
#define WDOG_CONTROL_REG_WDT_RT_32T_PERIOD	0x10

#define WDOG_TIMEOUT_RANGE_REG_OFFSET	    0x04
#define WDOG_TIMEOUT_RANGE_TOPINIT_SHIFT    4

#define WDOG_CURRENT_COUNT_REG_OFFSET	    0x08

#define WDOG_COUNTER_RESTART_REG_OFFSET     0x0c
#define WDOG_COUNTER_RESTART_KICK_VALUE	    0x76

#define WDOG_COMP_VERSION_REG_OFFSET	    0xf8

/* The maximum TOP (timeout period) value that can be set in the watchdog. */
#define DW_WDT_MAX_TOP		15

#define DW_WDT_DEFAULT_SECONDS	2

static inline int bmdrv_wdt_is_enabled(struct bm_device_info *bmdi)
{
	return wdt_reg_read(bmdi, WDOG_CONTROL_REG_OFFSET) &
		WDOG_CONTROL_REG_WDT_EN_MASK;
}

static inline int bmdrv_wdt_top_in_seconds(struct bm_device_info *bmdi, unsigned top)
{
	/*
	 * There are 16 possible timeout values in 0..15 where the number of
	 * cycles is 2 ^ (16 + i) and the watchdog counts down.
	 */
	return (1U << (16 + top)) / (100 * 1000 * 1000);
}

static int bmdrv_wdt_set_timeout(struct bm_device_info *bmdi, unsigned int top_s)
{
	int i, top_val = DW_WDT_MAX_TOP;

	/*
	 * Iterate over the timeout values until we find the closest match. We
	 * always look for >=.
	 */
	for (i = 0; i <= DW_WDT_MAX_TOP; ++i)
		if (bmdrv_wdt_top_in_seconds(bmdi, i) >= top_s) {
			top_val = i;
			break;
		}

	/*
	 * Set the new value in the watchdog.  Some versions of bmdrv_wdt
	 * have have TOPINIT in the TIMEOUT_RANGE register (as per
	 * CP_WDT_DUAL_TOP in WDT_COMP_PARAMS_1).  On those we
	 * effectively get a pat of the watchdog right here.
	 */

	wdt_reg_write(bmdi, WDOG_TIMEOUT_RANGE_REG_OFFSET, top_val |
			top_val << WDOG_TIMEOUT_RANGE_TOPINIT_SHIFT);
	return 0;
}

int bmdrv_wdt_start(struct bm_device_info *bmdi)
{
	unsigned int val = 0;
	/* enable watch dog to reset chip on top register */
	val = top_reg_read(bmdi, TOP_ITCM_SWITCH);
	top_reg_write(bmdi, TOP_ITCM_SWITCH, val | 0x4);

	bmdrv_wdt_set_timeout(bmdi, DW_WDT_DEFAULT_SECONDS);

	/*before start count, should restart the counter*/
	wdt_reg_write(bmdi, WDOG_COUNTER_RESTART_REG_OFFSET,
			WDOG_COUNTER_RESTART_KICK_VALUE);

	wdt_reg_write(bmdi, WDOG_CONTROL_REG_OFFSET, WDOG_CONTROL_REG_WDT_EN_MASK |
			WDOG_CONTROL_REG_WDT_RT_16T_PERIOD);

	return 0;
}
