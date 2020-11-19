#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include "bm_common.h"
#include "i2c.h"
#include "spi.h"
#include "pwm.h"
#include "bm_attr.h"
#include "bm_ctl.h"
#include "bm_msgfifo.h"
#include "bm1684/bm1684_clkrst.h"
#include "bm1684/bm1684_card.h"
#ifndef SOC_MODE
#include "bm_pcie.h"
#endif

#define FREQ0DATA              0x024
#define FREQ1DATA              0x02c
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
static void attr_timer_npu_handler(struct timer_list *t);
static void attr_timer_register(struct timer_list *tmr, struct bm_chip_attr *c_attr,
		void (*timer_handler)(struct timer_list *), u32 ms);
#else
static void attr_timer_npu_handler(unsigned long param);
static void attr_timer_register(struct timer_list *tmr, struct bm_chip_attr *c_attr,
		void (*timer_handler)(unsigned long), u32 ms);
#endif

/*
 * Cat value of the npu usage
 * Test: $cat /sys/class/bm-sophon/bm-sophon0/device/npu_usage
 */
static ssize_t npu_usage_show(struct device *d, struct device_attribute *attr, char *buf)
{
#ifdef SOC_MODE
	struct platform_device *pdev  = container_of(d, struct platform_device, dev);
	struct bm_device_info *bmdi = (struct bm_device_info *)platform_get_drvdata(pdev);
#else
	struct pci_dev *pdev = container_of(d, struct pci_dev, dev);
	struct bm_device_info *bmdi = pci_get_drvdata(pdev);
#endif
	struct bm_chip_attr *cattr = NULL;
	int usage = 0;
	int usage_all = 0;

	cattr = &bmdi->c_attr;

	if (atomic_read(&cattr->timer_on) == 0)
		return sprintf(buf, "Please, set [Usage enable] to 1\n");

	usage = (int)atomic_read(&cattr->npu_utilization);
	usage_all = cattr->npu_busy_time_sum_ms * 100/cattr->npu_start_probe_time;

	return sprintf(buf, "usage:%d avusage:%d\n", usage, usage_all);
}
static DEVICE_ATTR_RO(npu_usage);

/*
 * Check the validity of the parameters(Only for method of store***)
 */
static int check_interval_store(const char *buf)
{
	int ret = -1;

	int tmp = simple_strtoul(buf, NULL, 0);

	if ((tmp >= 200) && (tmp <= 2000))
		ret = 0;

	return ret;
}

/*
 * Cat value of the npu usage interval
 * Test: $cat /sys/class/bm-sophon/bm-sophon0/device/npu_usage_interval
 */
static ssize_t show_usage_interval(struct device *d, struct device_attribute *attr, char *buf)
{
#ifdef SOC_MODE
	struct platform_device *pdev  = container_of(d, struct platform_device, dev);
	struct bm_device_info *bmdi = (struct bm_device_info *)platform_get_drvdata(pdev);
#else
	struct pci_dev *pdev = container_of(d, struct pci_dev, dev);
	struct bm_device_info *bmdi = pci_get_drvdata(pdev);
#endif
	struct bm_chip_attr *cattr = NULL;

	cattr = &bmdi->c_attr;

	return sprintf(buf, "\"interval\": %d\n", cattr->npu_timer_interval);
}

/*
 * Echo value of the usage interval
 * Test: $sudo bash -c "echo 2000 > /sys/class/bm-sophon/bm-sophon0/device/npu_usage_interval"
 */
static ssize_t store_usage_interval(struct device *d,
		struct device_attribute *attr, const char *buf, size_t count)
{
#ifdef SOC_MODE
	struct platform_device *pdev  = container_of(d, struct platform_device, dev);
	struct bm_device_info *bmdi = (struct bm_device_info *)platform_get_drvdata(pdev);
#else
	struct pci_dev *pdev = container_of(d, struct pci_dev, dev);
	struct bm_device_info *bmdi = pci_get_drvdata(pdev);
#endif
	struct bm_chip_attr *cattr = NULL;

	cattr = &bmdi->c_attr;

	/* Check the validity of the parameters */
	if (-1 == check_interval_store(buf)) {
		pr_info("Parameter error! Parameter: 200 ~ 2000\n");
		return -EINVAL;
	}

	sscanf(buf, "%d", &cattr->npu_timer_interval);
	pr_info("usage interval: %d\n", cattr->npu_timer_interval);

	return strnlen(buf, count);
}

static DEVICE_ATTR(npu_usage_interval, 0664, show_usage_interval, store_usage_interval);

static ssize_t show_usage_enable(struct device *d, struct device_attribute *attr, char *buf)
{
#ifdef SOC_MODE
	struct platform_device *pdev  = container_of(d, struct platform_device, dev);
	struct bm_device_info *bmdi = (struct bm_device_info *)platform_get_drvdata(pdev);
#else
	struct pci_dev *pdev = container_of(d, struct pci_dev, dev);
	struct bm_device_info *bmdi = pci_get_drvdata(pdev);
#endif

	return sprintf(buf, "\"enable\": %d\n", atomic_read(&bmdi->c_attr.timer_on));
}

static int check_enable_store(const char *buf)
{
	int ret = -1;

	int tmp = simple_strtoul(buf, NULL, 0);

	if ((0 == tmp) || (1 == tmp))
		ret = 0;

	return ret;
}
static ssize_t store_usage_enable(struct device *d,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int enable = 0;
	int i = 0;

#ifdef SOC_MODE
	struct platform_device *pdev  = container_of(d, struct platform_device, dev);
	struct bm_device_info *bmdi = (struct bm_device_info *)platform_get_drvdata(pdev);
#else
	struct pci_dev *pdev = container_of(d, struct pci_dev, dev);
	struct bm_device_info *bmdi = pci_get_drvdata(pdev);
#endif
	struct bm_chip_attr *cattr = NULL;

	cattr = &bmdi->c_attr;

	/* Check the validity of the parameters */
	if (-1 == check_enable_store(buf)) {
		pr_info("Parameter error! Parameter: 0 or 1\n");
		return -1;
	}

	sscanf(buf, "%d", &enable);
	if ((enable == 1) && (atomic_read(&cattr->timer_on) == 0)) {
		atomic_set(&cattr->timer_on, 1);
		for (i = 0; i < NPU_STAT_WINDOW_WIDTH; i++)
			cattr->npu_status[i] = 0;
		attr_timer_register(&cattr->attr_timer_npu, cattr, attr_timer_npu_handler,
				cattr->npu_timer_interval / NPU_STAT_WINDOW_WIDTH);
	} else if (enable == 0) {
		atomic_set(&cattr->timer_on, 0);
	}

	pr_info("Usage enable: %d\n", enable);

	return strnlen(buf, count);
}

static DEVICE_ATTR(npu_usage_enable, 0664, show_usage_enable, store_usage_enable);
static struct attribute *bm_npu_sysfs_entries[] = {
	&dev_attr_npu_usage.attr,
	&dev_attr_npu_usage_interval.attr,
	&dev_attr_npu_usage_enable.attr,
	NULL,
};

static struct attribute_group bm_npu_attribute_group = {
	.name = NULL,
	.attrs = bm_npu_sysfs_entries,
};

void bmdrv_thermal_init(struct bm_device_info *bmdi)
{
	int i = 0;

	for (i = 0; i < BM_THERMAL_WINDOW_WIDTH; i++)
		bmdi->c_attr.thermal_info.elapsed_temp[i] = 0;

	bmdi->c_attr.thermal_info.idx = 0;
	bmdi->c_attr.thermal_info.half_clk_tmp = 85;
	bmdi->c_attr.thermal_info.min_clk_tmp = 90;
}

void board_status_update(struct bm_device_info *bmdi, int cur_tmp, int cur_tpu_clk)
{
	if ((bmdi->cinfo.chip_id == 0x1684) && (bmdi->misc_info.pcie_soc_mode == 0)) {
		if ((cur_tpu_clk < bmdi->boot_info.tpu_min_clk)|| (cur_tpu_clk > bmdi->boot_info.tpu_max_clk)) {
			bmdi->status_pcie = 1;
		} else if (cur_tmp > 95) {
			bmdi->status_over_temp = 1;
		} else if ((cur_tpu_clk >= bmdi->boot_info.tpu_min_clk)
                         && (cur_tpu_clk <= bmdi->boot_info.tpu_max_clk) && (cur_tmp < 90)) {
			bmdi->status_pcie =0;
			bmdi->status_over_temp =0;
		}
	}
	if ((bmdi->status_over_temp) || (bmdi->status_pcie) || (bmdi->status_sync_api))
		bmdi->status = 1;
	else
		bmdi->status = 0;
}
void bmdrv_thermal_update_status(struct bm_device_info *bmdi, int cur_tmp)
{
	int avg_tmp = 0;
	int cur_tpu_clk = 0;
	int i = 0;
	struct bm_chip_attr *c_attr = &bmdi->c_attr;
	int new_led_status = c_attr->led_status;

	cur_tpu_clk = bmdrv_1684_clk_get_tpu_freq(bmdi);
	c_attr->thermal_info.elapsed_temp[bmdi->c_attr.thermal_info.idx] = cur_tmp;
	if (c_attr->thermal_info.idx++ >= BM_THERMAL_WINDOW_WIDTH - 1)
		c_attr->thermal_info.idx = 0;

	for (i = 0; i < BM_THERMAL_WINDOW_WIDTH; i++)
		avg_tmp += c_attr->thermal_info.elapsed_temp[i];

	avg_tmp = avg_tmp/BM_THERMAL_WINDOW_WIDTH;

	if (0 == bmdi->enable_dyn_freq) {
		if (cur_tpu_clk >= bmdi->boot_info.tpu_min_clk &&
			cur_tpu_clk < bmdi->boot_info.tpu_max_clk * 8 / 10) {
			new_led_status = LED_BLINK_THREE_TIMES_PER_S;
		} else if (cur_tpu_clk >= ((bmdi->boot_info.tpu_max_clk * 8) / 10) &&
			cur_tpu_clk < bmdi->boot_info.tpu_max_clk) {
			new_led_status = LED_BLINK_ONE_TIMES_PER_S;
		} else if (cur_tpu_clk == bmdi->boot_info.tpu_max_clk) {
			new_led_status = LED_BLINK_ONE_TIMES_PER_2S;
		}
	} else {
		if (avg_tmp > c_attr->thermal_info.min_clk_tmp
				&& cur_tpu_clk != bmdi->boot_info.tpu_min_clk) {
			pr_info("bmdrv_thermal_update_status cur_tpu_clk=%d cur_tmp = %d, \
				avg tmp = %d, change to min\n", cur_tpu_clk, cur_tmp, avg_tmp);
			bmdrv_clk_set_tpu_target_freq(bmdi, bmdi->boot_info.tpu_min_clk);
			new_led_status = LED_BLINK_THREE_TIMES_PER_S;
		} else if (avg_tmp < c_attr->thermal_info.half_clk_tmp
				&& avg_tmp > (c_attr->thermal_info.half_clk_tmp - 5)
				&& cur_tpu_clk == (bmdi->boot_info.tpu_min_clk)) {
			pr_info("bmdrv_thermal_update_status cur_tpu_clk=%d cur_tmp = %d, \
				avg tmp = %d, change to mid\n", cur_tpu_clk, cur_tmp, avg_tmp);
			bmdrv_clk_set_tpu_target_freq(bmdi, (bmdi->boot_info.tpu_max_clk * 8) / 10);
			new_led_status = LED_BLINK_ONE_TIMES_PER_S;
		} else if (avg_tmp > c_attr->thermal_info.half_clk_tmp
				&& avg_tmp < (c_attr->thermal_info.min_clk_tmp)
				&& cur_tpu_clk == (bmdi->boot_info.tpu_max_clk)) {
			pr_info("bmdrv_thermal_update_status cur_tpu_clk=%d cur_tmp = %d, \
				avg tmp = %d, change to mid\n", cur_tpu_clk, cur_tmp, avg_tmp);
			bmdrv_clk_set_tpu_target_freq(bmdi, (bmdi->boot_info.tpu_max_clk * 8) / 10);
			new_led_status = LED_BLINK_ONE_TIMES_PER_S;
		} else if (avg_tmp < (c_attr->thermal_info.half_clk_tmp - 5)
				&& cur_tpu_clk != bmdi->boot_info.tpu_max_clk) {
			pr_info("bmdrv_thermal_update_status cur_tmp = %d, avg tmp = %d, \
				change to max\n", cur_tmp, avg_tmp);
			bmdrv_clk_set_tpu_target_freq(bmdi, bmdi->boot_info.tpu_max_clk);
			new_led_status = LED_BLINK_ONE_TIMES_PER_2S;
		}
	}

	board_status_update(bmdi, cur_tmp, cur_tpu_clk);
	mutex_lock(&c_attr->attr_mutex);
	if (c_attr->bm_set_led_status &&
		new_led_status != c_attr->led_status &&
		c_attr->led_status != LED_ON &&
		c_attr->led_status != LED_OFF) {
		c_attr->bm_set_led_status(bmdi, new_led_status);
		c_attr->led_status = new_led_status;
	}
	mutex_unlock(&c_attr->attr_mutex);
}

static void bm_set_temp_position(struct bm_device_info *bmdi, u8 pos)
{
	if (bmdi->cinfo.chip_id == 0x1684)
		top_reg_write(bmdi, 0x01C, (0x1 << pos));
	else if (bmdi->cinfo.chip_id == 0x1682)
		top_reg_write(bmdi, 0x01C, (0x1 << pos));
}

/* tmp451 range mode */
static int bm_set_tmp451_range_mode(struct bm_device_info *bmdi)
{
	int ret;
	u8 cfg = 0;

	bm_set_temp_position(bmdi, 5);
	bm_i2c_set_target_addr(bmdi, 0x4c);
	ret = bm_i2c_read_byte(bmdi, 0x3, &cfg);
	ret = bm_i2c_write_byte(bmdi, 0x9, cfg | 4);
	return ret;
}

int bmdrv_card_attr_init(struct bm_device_info *bmdi)
{
	int ret = 0;
	int i = 0;
	struct bm_chip_attr *c_attr = &bmdi->c_attr;
#ifndef SOC_MODE
	int value = 0;
#endif

	c_attr->fan_speed = 100;
	c_attr->fan_rev_read = 0;
	c_attr->npu_cnt = 0;
	c_attr->npu_busy_cnt = 0;
	atomic_set(&c_attr->npu_utilization, 0);
	c_attr->npu_timer_interval = 500;
	c_attr->npu_busy_time_sum_ms = 0ULL;
	c_attr->npu_start_probe_time = 0ULL;
	c_attr->npu_status_idx = 0;
	for (i = 0; i < NPU_STAT_WINDOW_WIDTH; i++)
		c_attr->npu_status[i] = 0;
	atomic_set(&c_attr->timer_on, 0);
	mutex_init(&c_attr->attr_mutex);

	switch (bmdi->cinfo.chip_id) {
	case 0x1682:
#ifndef SOC_MODE
		c_attr->fan_control = true;
#else
		c_attr->fan_control = false;
#endif
		break;
	case 0x1684:
#ifndef SOC_MODE
		c_attr->bm_get_tpu_power = bm_read_vdd_tpu_power;
		c_attr->fan_control = bmdi->boot_info.fan_exist;
		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS ||
			BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H) {
			/* fix this later with bootinfo */
			c_attr->bm_set_led_status = set_led_status;
		}
		if (bmdi->boot_info.board_power_sensor_exist) {
			if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_EVB ||
				BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5)
				c_attr->bm_get_board_power = bm_read_sc5_power;
			else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS)
				c_attr->bm_get_board_power = bm_read_sc5_plus_power;
			else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
				c_attr->bm_get_board_power = bm_read_sc5_power;
		} else {
			c_attr->bm_get_board_power = NULL;
		}
		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H) {
			value = top_reg_read(bmdi, 0x470);
			value &= ~(0x1 << 4);
			value |= (0x1 << 4);
			top_reg_write(bmdi, 0x470, value);     /* Selector for FAN1 */
		} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5) {
			value = top_reg_read(bmdi, 0x46c);
			value &= ~(0x1 << 20);
			value |= (0x1 << 20);
			top_reg_write(bmdi, 0x46c, value);     /* Selector for FAN0 */
		}
#else
		c_attr->bm_get_tpu_power = NULL;
		c_attr->bm_get_board_power = NULL;
		c_attr->fan_control = false;
		c_attr->bm_get_chip_temp = NULL;
		c_attr->bm_get_board_temp = NULL;
#endif
		break;
	default:
		return -EINVAL;
	}

#ifndef SOC_MODE
	bmdrv_thermal_init(bmdi);
	c_attr->bm_get_fan_speed = bm_get_fan_speed;
	c_attr->bm_get_chip_temp = bm_read_tmp451_remote_temp;
	c_attr->bm_get_board_temp = bm_read_tmp451_local_temp;

	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SM5_S ||
			BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SM5_P) {
		c_attr->bm_get_chip_temp = bm_read_mcu_chip_temp;
		c_attr->bm_get_board_temp = bm_read_mcu_board_temp;
	} else {
		ret = bm_set_tmp451_range_mode(bmdi);
	}
#endif
	c_attr->bm_get_npu_util = bm_read_npu_util;

	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
static void attr_timer_temp_handler(struct timer_list *t)
{
	struct bm_chip_attr *c_attr = from_timer(c_attr, t, attr_timer_temp);
#else
static void attr_timer_temp_handler(unsigned long param)
{
	struct bm_chip_attr *c_attr = (struct bm_chip_attr *)param;
#endif
	schedule_work(&c_attr->attr_work);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
static void attr_timer_register(struct timer_list *tmr, struct bm_chip_attr *c_attr,
		void (*timer_handler)(struct timer_list *), u32 ms)
{
	timer_setup(tmr, timer_handler, 0);
	tmr->expires = jiffies + msecs_to_jiffies(ms);
	add_timer(tmr);
}
#else
static void attr_timer_register(struct timer_list *tmr, struct bm_chip_attr *c_attr,
		void (*timer_handler)(unsigned long), u32 ms)
{
	init_timer(tmr);
	tmr->function = timer_handler;
	tmr->expires = jiffies + msecs_to_jiffies(ms);
	tmr->data = (unsigned long)c_attr;
	add_timer(tmr);
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
static void attr_timer_npu_handler(struct timer_list *t)
{
	struct bm_chip_attr *c_attr = from_timer(c_attr, t, attr_timer_npu);
#else
static void attr_timer_npu_handler(unsigned long param)
{
	struct bm_chip_attr *c_attr = (struct bm_chip_attr *)param;
#endif
	struct bm_device_info *bmdi = container_of(c_attr, struct bm_device_info, c_attr);
	int i = 0;
	int npu_status_stat = 0;

	if (!bmdev_msgfifo_empty(bmdi, BM_MSGFIFO_CHANNEL_XPU)) {
		c_attr->npu_status[c_attr->npu_status_idx] = 1;
		c_attr->npu_busy_time_sum_ms += c_attr->npu_timer_interval/NPU_STAT_WINDOW_WIDTH;
	} else {
		c_attr->npu_status[c_attr->npu_status_idx] = 0;
	}

	c_attr->npu_start_probe_time += c_attr->npu_timer_interval/NPU_STAT_WINDOW_WIDTH;
	c_attr->npu_status_idx = (c_attr->npu_status_idx+1)%NPU_STAT_WINDOW_WIDTH;

	for (i = 0; i < NPU_STAT_WINDOW_WIDTH; i++)
		npu_status_stat += c_attr->npu_status[i];
	atomic_set(&c_attr->npu_utilization, npu_status_stat);

	if (atomic_read(&c_attr->timer_on) == 1) {
		attr_timer_register(&c_attr->attr_timer_npu, c_attr,
			attr_timer_npu_handler, c_attr->npu_timer_interval/NPU_STAT_WINDOW_WIDTH);
	}
}

static void bmdrv_fetch_attr(struct work_struct *work)
{
	struct bm_chip_attr *c_attr = container_of(work, struct bm_chip_attr, attr_work);
#ifndef SOC_MODE
	int rc = 0;
	u32 chip_temp = 0;
	struct bm_device_info *bmdi = container_of(c_attr, struct bm_device_info, c_attr);
	struct chip_info *cinfo = &bmdi->cinfo;

	if (!bmdi->boot_info.temp_sensor_exist)
		goto err_fetch;

	/* get chip temperature */
	mutex_lock(&c_attr->attr_mutex);
	rc = c_attr->bm_get_chip_temp(bmdi, &chip_temp);
	mutex_unlock(&c_attr->attr_mutex);
	if (rc) {
		dev_err(cinfo->device, "device chip temperature fetch failed %d\n", rc);
		goto err_fetch;
	}
	if (c_attr->fan_control)
		bmdrv_adjust_fan_speed(bmdi, chip_temp);

	mutex_lock(&bmdi->clk_reset_mutex);
	bmdrv_thermal_update_status(bmdi, chip_temp);
	mutex_unlock(&bmdi->clk_reset_mutex);
err_fetch:
#endif
	if (atomic_read(&c_attr->timer_on) == 1)
		attr_timer_register(&c_attr->attr_timer_temp, c_attr, attr_timer_temp_handler, 1000);
}

int reset_fan_speed(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return set_pwm_high(bmdi, 0x1);
	else
		return set_pwm_high(bmdi, 0x0);
#else
	return 0;
#endif
}

int bmdrv_enable_attr(struct bm_device_info *bmdi)
{
	struct bm_chip_attr *c_attr = &bmdi->c_attr;
	struct chip_info *cinfo = &bmdi->cinfo;
	int rc;

	if (c_attr->fan_control)
		reset_fan_speed(bmdi);

	rc = sysfs_create_group(&cinfo->device->kobj, &bm_npu_attribute_group);
	if (rc) {
		pr_err("create sysfs node failed\n");
		rc = -EINVAL;
		return rc;
	}

	INIT_WORK(&c_attr->attr_work, bmdrv_fetch_attr);
	atomic_set(&c_attr->timer_on, 1);
	attr_timer_register(&c_attr->attr_timer_temp, c_attr, attr_timer_temp_handler, 1000);
	attr_timer_register(&c_attr->attr_timer_npu, c_attr, attr_timer_npu_handler,
		c_attr->npu_timer_interval/NPU_STAT_WINDOW_WIDTH);
	return 0;
}

int bmdrv_disable_attr(struct bm_device_info *bmdi)
{
	struct bm_chip_attr *c_attr = &bmdi->c_attr;
	struct chip_info *cinfo = &bmdi->cinfo;

	cancel_work_sync(&c_attr->attr_work);
	atomic_set(&c_attr->timer_on, 0);
	if (del_timer_sync(&c_attr->attr_timer_temp) > 0)
		cancel_work_sync(&c_attr->attr_work);
	del_timer_sync(&c_attr->attr_timer_npu);
	if (c_attr->fan_control)
		reset_fan_speed(bmdi);
	sysfs_remove_group(&cinfo->device->kobj, &bm_npu_attribute_group);
	return 0;
}

/* the function set_fan_speed sets the fan running speed
 *parameter: u8 spd_level is an unsigned integer ranging from
 *	0 - 100; 0 means min speed and 100 means full speed
 */
int set_fan_speed(struct bm_device_info *bmdi, u16 spd_level)
{
#ifndef SOC_MODE
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return set_pwm_level(bmdi, FAN_PWM_PERIOD, spd_level, 0x1);
	else
		return set_pwm_level(bmdi, FAN_PWM_PERIOD, spd_level, 0x0);
#else
	return 0;
#endif
}

static int set_led_on(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	u32 reg_val = 0x0;

	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H) {
		reg_val = gpio_reg_read(bmdi, 0x4 + 0x800); //gpio78 for led
		reg_val |= 1 << 13;
		gpio_reg_write(bmdi, 0x4 + 0x800, reg_val);
		reg_val = gpio_reg_read(bmdi, 0x0 + 0x800);
		reg_val |= 1 << 13;
		gpio_reg_write(bmdi, 0x0 + 0x800, reg_val);
		return 0;
	} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS)
		return set_pwm_high(bmdi, 0);
	else
		return 0;

#else
	return 0;
#endif
}

static int set_led_off(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	u32 reg_val = 0x0;

	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H) {
		reg_val = gpio_reg_read(bmdi, 0x4 + 0x800);
		reg_val |= 1 << 13;
		gpio_reg_write(bmdi, 0x4 + 0x800, reg_val);
		reg_val = gpio_reg_read(bmdi, 0x0 + 0x800);
		reg_val &= ~(1 << 13);
		gpio_reg_write(bmdi, 0x0 + 0x800, reg_val);
		return 0;
	} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS)
		return set_pwm_low(bmdi, 0);
	else
		return 0;
#else
	return 0;
#endif
}

static int set_led_blink_1_per_2s(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS)
		return set_pwm_level(bmdi, LED_PWM_PERIOD*2, 50, 0);
	else
		return 0;
#else
	return 0;
#endif
}

static int set_led_blink_1_per_s(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS)
		return set_pwm_level(bmdi, LED_PWM_PERIOD, 25, 0);
	else
		return 0;
#else
	return 0;
#endif
}

static int set_led_blink_3_per_s(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS)
		return set_pwm_level(bmdi, LED_PWM_PERIOD / 3, 17, 0);
	else
		return 0;
#else
	return 0;
#endif
}

static int set_led_blink_fast(struct bm_device_info *bmdi)
{
#ifndef SOC_MODE
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS)
		return set_pwm_level(bmdi, LED_PWM_PERIOD / 2, 50, 0);
	else
		return 0;
#else
	return 0;
#endif
}

int set_led_status(struct bm_device_info *bmdi, int led_status)
{
	switch (led_status) {
	case LED_ON:
		return set_led_on(bmdi);
	case LED_OFF:
		return set_led_off(bmdi);
	case LED_BLINK_FAST:
		return set_led_blink_fast(bmdi);
	case LED_BLINK_ONE_TIMES_PER_S:
		return set_led_blink_1_per_s(bmdi);
	case LED_BLINK_ONE_TIMES_PER_2S:
		return set_led_blink_1_per_2s(bmdi);
	case LED_BLINK_THREE_TIMES_PER_S:
		return set_led_blink_3_per_s(bmdi);
	default:
		return -1;
	}
}

int set_ecc(struct bm_device_info *bmdi, int ecc_enable)
{
#ifdef SOC_MODE
	return 0;
#else
	struct bm_boot_info boot_info;

	if (bm_spi_flash_get_boot_info(bmdi, &boot_info))
		return -1;

	if (ecc_enable == boot_info.ddr_ecc_enable)
		return 0;

	boot_info.ddr_ecc_enable = ecc_enable;
	if (bm_spi_flash_update_boot_info(bmdi, &boot_info))
		return -1;
	return 0;
#endif
}

#ifndef SOC_MODE
int board_type_sc5_rev_to_duty(u16 fan_rev)
{
	u32 fan_duty = 0;

	if ((fan_rev > 0) && (fan_rev < 2000))
		fan_duty = 3;
	else if ((fan_rev >= 2000) && (fan_rev < 2520))
		fan_duty = (u32)(20*fan_rev-36130)/1000;
	else if ((fan_rev >= 2520) && (fan_rev < 2970))
		fan_duty = (u32)(22*fan_rev-41000)/1000;
	else if ((fan_rev >= 2970) && (fan_rev < 3990))
		fan_duty = (u32)(245*fan_rev-477940)/10000;
	else if ((fan_rev >= 3990) && (fan_rev < 4320))
		fan_duty = (u32)(303*fan_rev-709090)/10000;
	else if ((fan_rev >= 4320) && (fan_rev < 4590))
		fan_duty = (u32)(37*fan_rev-100000)/1000;
	else if ((fan_rev >= 4590) && (fan_rev < 5400))
		fan_duty = (u32)(4*fan_rev-11360)/100;
	else if (fan_rev >= 5400)
		fan_duty = 100;
	else if (fan_rev == 0)
		fan_duty = 0;
	return fan_duty;
}

int board_type_sc5h_rev_to_duty(u16 fan_rev)
{
	u32 fan_duty = 0;

	if ((fan_rev > 0) && (fan_rev <= 2000))
		fan_duty = (u32)(83*fan_rev+33333)/10000;
	else if ((fan_rev > 2000) && (fan_rev <= 4000))
		fan_duty = (u32)fan_rev/100;
	else if ((fan_rev > 4000) && (fan_rev <= 6400))
		fan_duty = (u32)(125*fan_rev-100000)/10000;
	else if ((fan_rev > 6400) && (fan_rev <= 7800))
		fan_duty = (u32)(143*fan_rev-214290)/10000;
	else if ((fan_rev > 7800) && (fan_rev <= 8400))
		fan_duty = (u32)(167*fan_rev-400000)/10000;
	else if (fan_rev > 8400)
		fan_duty = 100;
	else if (fan_rev == 0)
		fan_duty = 0;
	return fan_duty;
}

int bm_get_fixed_fan_speed(struct bm_device_info *bmdi, u32 temp)
{
	u16 fan_spd = 100;

	if (0 == bmdi->fixed_fan_speed) {
		if (temp > 61)
			fan_spd = 100;
		else if (temp <= 61 && temp > 35)
			fan_spd = 20 + (temp - 35) * 8 / 3;
		else if (temp <= 35 && temp > 20)
			fan_spd = 20;
		else if (temp <= 20)
			fan_spd = 10;
		return fan_spd;
	} else
		return bmdi->fixed_fan_speed;
}
#endif

void bmdrv_adjust_fan_speed(struct bm_device_info *bmdi, u32 temp)
{
#ifndef SOC_MODE
	struct bm_chip_attr *c_attr = &bmdi->c_attr;
	u16 fan_rev = 0;
	u32 fan_duty = 0;
	u32 fan_speed_set = 0;

	mutex_lock(&c_attr->attr_mutex);
	fan_speed_set = bm_get_fixed_fan_speed(bmdi, temp);
	if (c_attr->fan_speed != fan_speed_set) {
		if (set_fan_speed(bmdi, fan_speed_set) != 0)
			pr_err("bmdrv: set fan speed failed.\n");
	}
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5) {
		fan_rev = pwm_reg_read(bmdi, FREQ0DATA) * 30;
		c_attr->fan_rev_read = fan_rev;
		fan_duty = board_type_sc5_rev_to_duty(fan_rev);
	}

	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H) {
		fan_rev = pwm_reg_read(bmdi, FREQ1DATA) * 30;
		c_attr->fan_rev_read = fan_rev;
		fan_duty = board_type_sc5h_rev_to_duty(fan_rev);
	}

	if (fan_duty > 100)
		fan_duty = 100;
	c_attr->fan_speed = fan_duty;
	mutex_unlock(&c_attr->attr_mutex);
#endif
}

#ifndef SOC_MODE
int bm_get_fan_speed(struct bm_device_info *bmdi)
{
	struct bm_chip_attr *c_attr = &bmdi->c_attr;

	return c_attr->fan_speed;
}
#endif


int bm_read_npu_util(struct bm_device_info *bmdi)
{
	struct bm_chip_attr *c_attr = &bmdi->c_attr;
	int timer_on = 0;

	timer_on = atomic_read(&bmdi->c_attr.timer_on);
	if (timer_on)
		return atomic_read(&c_attr->npu_utilization);
	else
		return ATTR_NOTSUPPORTED_VALUE;
}

/* read local temperature (board) */
int bm_read_tmp451_local_temp(struct bm_device_info *bmdi, int *temp)
{
	u8 local_high = 0;
	int temps = 0;
	int ret;

	bm_i2c_set_target_addr(bmdi, 0x4c);
	ret = bm_i2c_read_byte(bmdi, 0, &local_high);
	temps = (local_high & 0xf) + ((local_high & 0xff) >> 4) * 16;
	temps -= 64;
	*temp = temps;
	return ret;
}

/* read remote temperature (chip) */
int bm_read_tmp451_remote_temp(struct bm_device_info *bmdi, int *temp)
{
	u8 local_high = 0;
	int temps = 0;
	int ret;

	bm_set_temp_position(bmdi, 5);
	bm_i2c_set_target_addr(bmdi, 0x4c);
	ret = bm_i2c_read_byte(bmdi, 0x1, &local_high);
	temps = (local_high & 0xf) + ((local_high & 0xff) >> 4) * 16;
	temps -= 64;
	if (ret)
		return ret;

	/* remote temperature is that the sensor inside our IC needs
	 * to be calibrated, the approximate deviation is 5℃
	 */
	*temp = temps - 5;
	PR_TRACE("remote temp = 0x%x\n", temps);
	return ret;
}

#ifndef SOC_MODE
/* chip power is a direct value;
 * it is fetched from isl68127
 * which is attached to i2c0 (smbus)
 */
static int bm_read_68127_power(struct bm_device_info *bmdi, int id, u32 *power)
{
	int ret;

	ret = bm_smbus_cmd_write_byte(bmdi, 0, id);
	if (ret) {
		pr_err("bmdrv smbus set cmd failed!\n");
		return ret;
	}
	ret = bm_smbus_cmd_read_hword(bmdi, 0x96, (u16 *)power);
	if (ret) {
		pr_err("bmdrv smbus read power value failed!\n");
		return ret;
	}
	PR_TRACE("bmdrv smbus id %x power %d W\n", id, *power);
	return ret;
}

static int bm_read_68127_voltage_out(struct bm_device_info *bmdi, int id, u32 *volt)
{
	int ret;

	ret = bm_smbus_cmd_write_byte(bmdi, 0, id);
	if (ret) {
		pr_err("bmdrv smbus set cmd failed!\n");
		return ret;
	}
	ret = bm_smbus_cmd_read_hword(bmdi, 0x8b, (u16 *)volt);
	if (ret) {
		pr_err("bmdrv smbus read voltage failed!\n");
		return ret;
	}
	PR_TRACE("bmdrv smbus id %x voltage %d mV\n", id, *volt);
	return ret;
}

static int bm_read_68127_current_out(struct bm_device_info *bmdi, int id, u32 *cur)
{
	int ret;

	ret = bm_smbus_cmd_write_byte(bmdi, 0, id);
	if (ret) {
		pr_err("bmdrv smbus set cmd failed!\n");
		return ret;
	}
	ret = bm_smbus_cmd_read_hword(bmdi, 0x8c, (u16 *)cur);
	if (ret) {
		pr_err("bmdrv smbus read current failed!\n");
		return ret;
	}
	*cur = (*cur) * 100;
	PR_TRACE("bmdrv smbus id %x current %d mA\n", id, *cur);
	return ret;
}

/* chip power is a direct value;
 * it is fetched from is pxc1331
 * which is attached to i2c0 (smbus)
 */
static int bm_read_1331_power(struct bm_device_info *bmdi, int id, u32 *power)
{
	int ret;

	ret = bm_smbus_cmd_write_byte(bmdi, 0, id);
	if (ret) {
		pr_err("bmdrv smbus set cmd failed!\n");
		return ret;
	}
	ret = bm_smbus_cmd_read_hword(bmdi, 0x2d, (u16 *)power);
	if (ret) {
		pr_err("bmdrv smbus read power value failed!\n");
		return ret;
	}
	*power = (*power) * 40 / 1000;
	PR_TRACE("bmdrv smbus id %x power %d W\n", id, *power);
	return ret;
}

static int bm_read_1331_voltage_out(struct bm_device_info *bmdi, int id, u32 *volt)
{
	int ret;

	ret = bm_smbus_cmd_write_byte(bmdi, 0, id);
	if (ret) {
		pr_err("bmdrv smbus set cmd failed!\n");
		return ret;
	}
	ret = bm_smbus_cmd_read_hword(bmdi, 0x1A, (u16 *)volt);
	if (ret) {
		pr_err("bmdrv smbus read voltage failed!\n");
		return ret;
	}
	*volt = (*volt) * 5 / 4;

	PR_TRACE("bmdrv smbus id %x voltage %d mV\n", id, *volt);
	return ret;
}

static int bm_read_1331_current_out(struct bm_device_info *bmdi, int id, u32 *cur)
{
	int ret;

	ret = bm_smbus_cmd_write_byte(bmdi, 0, id);
	if (ret) {
		pr_err("bmdrv smbus set cmd failed!\n");
		return ret;
	}
	ret = bm_smbus_cmd_read_hword(bmdi, 0x15, (u16 *)cur);
	if (ret) {
		pr_err("bmdrv smbus read current failed!\n");
		return ret;
	}
	*cur = (*cur) * 125 / 2;
	PR_TRACE("bmdrv smbus id %x current %d mA\n", id, *cur);
	return ret;
}

static int bm_read_1331_temp(struct bm_device_info *bmdi, int id, u32 *temp)
{
	int ret;

	ret = bm_smbus_cmd_write_byte(bmdi, 0, id);
	if (ret) {
		pr_err("bmdrv smbus set cmd failed!\n");
		return ret;
	}
	ret = bm_smbus_cmd_read_hword(bmdi, 0x29, (u16 *)temp);
	if (ret) {
		pr_err("bmdrv smbus read current failed!\n");
		return ret;
	}
	*temp = (*temp) / 8;
	PR_TRACE("bmdrv smbus id %x temp %d C\n", id, *temp);
	return ret;
}

int bm_read_vdd_tpu_voltage(struct bm_device_info *bmdi, u32 *volt)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return bm_read_1331_voltage_out(bmdi, 0x60, volt);
	else
		return bm_read_68127_voltage_out(bmdi, 0x0, volt);
}

int bm_read_vdd_tpu_current(struct bm_device_info *bmdi, u32 *cur)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return bm_read_1331_current_out(bmdi, 0x60, cur);
	else
		return bm_read_68127_current_out(bmdi, 0x0, cur);
}

int bm_read_vdd_tpu_mem_voltage(struct bm_device_info *bmdi, u32 *volt)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return bm_read_1331_voltage_out(bmdi, 0x62, volt);
	else
		return 0;
}

int bm_read_vdd_tpu_mem_current(struct bm_device_info *bmdi, u32 *cur)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return bm_read_1331_current_out(bmdi, 0x62, cur);
	else
		return 0;
}

int bm_read_vddc_voltage(struct bm_device_info *bmdi, u32 *volt)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return bm_read_1331_voltage_out(bmdi, 0x61, volt);
	else
		return bm_read_68127_voltage_out(bmdi, 0x1, volt);

}

int bm_read_vddc_current(struct bm_device_info *bmdi, u32 *cur)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return bm_read_1331_current_out(bmdi, 0x61, cur);
	else
		return bm_read_68127_current_out(bmdi, 0x1, cur);
}

int bm_read_vdd_tpu_power(struct bm_device_info *bmdi, u32 *tpu_power)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return  bm_read_1331_power(bmdi, 0x60, tpu_power);
	else
		return  bm_read_68127_power(bmdi, 0x0, tpu_power);
}

int bm_read_vdd_tpu_mem_power(struct bm_device_info *bmdi, u32 *tpu_mem_power)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return  bm_read_1331_power(bmdi, 0x62, tpu_mem_power);
	else
		return  0;
}

int bm_read_vddc_power(struct bm_device_info *bmdi, u32 *vddc_power)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return  bm_read_1331_power(bmdi, 0x61, vddc_power);
	else
		return 0;
}

int bm_read_vdd_pmu_tpu_temp(struct bm_device_info *bmdi, u32 *pmu_tpu_temp)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return  bm_read_1331_temp(bmdi, 0x60, pmu_tpu_temp);
	else
		return  0;
}

int bm_read_vdd_pmu_tpu_mem_temp(struct bm_device_info *bmdi, u32 *pmu_tpu_mem_temp)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return  bm_read_1331_temp(bmdi, 0x62, pmu_tpu_mem_temp);
	else
		return  0;
}

int bm_read_vddc_pmu_vddc_temp(struct bm_device_info *bmdi, u32 *pmu_vddc_temp)
{
	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H)
		return  bm_read_1331_temp(bmdi, 0x61, pmu_vddc_temp);
	else
		return 0;
}

int bm_read_mcu_current(struct bm_device_info *bmdi, u8 lo, u32 *cur)
{
	int ret = 0;
	u8 data = 0;
	u32 result = 0;

	ret = bm_mcu_read_reg(bmdi, lo, &data);
	if (ret)
		return ret;
	result = (u32)data;

	ret = bm_mcu_read_reg(bmdi, lo + 1, &data);
	if (ret)
		return ret;

	result |= (u32)data << 8;

	if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_EVB ||
		BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5) {
		if (BM1684_HW_VERSION(bmdi) < 4) {
			switch (lo) {
			case 0x28: // 12v atx
				result = (int)result * 2 * 1000 * 10 / 3 / 4096;
				break;
			case 0x2a: // vddio5
				result = (int)result * 2 * 1000 * 10 / 6 / 4096;
				break;
			case 0x2c: // vddio18
				result = (int)result * 2 * 1000 * 10 / 6 / 4096;
				break;
			case 0x2e: // vddio33
				result = (int)result * 2 * 1000 / 3 / 4096;
				break;
			case 0x30: // vdd_phy
				result = (int)result * 2 * 1000 * 10 / 8 / 4096;
				break;
			case 0x32: // vdd_pcie
				result = (int)result * 2 * 1000 * 10 / 6 / 4096;
				break;
			case 0x34: // vdd_tpu_mem
				result = (int)result * 2 * 1000 * 10 / 3 / 4096;
				break;
			case 0x36: // ddr_vddq
				result = (int)result * 2 * 1000 * 10 / 8 / 4096;
				break;
			case 0x38: // ddr_vddqlp
				result = (int)result * 2 * 1000 * 10 / 5 / 4096;
				break;
			default:
				break;
			}
		} else {
			switch (lo) {
			case 0x28:
				result = (int)result * 3 * 1000 / 4096;
				break;
			case 0x2a:
				result = (int)result * 6 * 1000 / 4096;
				break;
			case 0x2c:
				result = (int)result * 18 * 1000 / 15 / 4096;
				break;
			case 0x2e:
				result = (int)result * 18 * 1000 / 30 / 4096;
				break;
			case 0x30:
				result = (int)result * 18 * 1000 / 15 / 4096;
				break;
			case 0x32:
				result = (int)result * 18 * 1000 / 15 / 4096;
				break;
			case 0x34:
				result = (int)result * 6 * 1000 / 4096;
				break;
			case 0x36:
				result = (int)result * 18 * 1000 / 8 / 4096;
				break;
			case 0x38:
				result = (int)result * 18 * 1000 / 30 / 4096;
				break;
			default:
				break;
			}
		}
	} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_PLUS) {
		switch (lo) {
		case 0x28: /* 12v atx */
			result = (int)result * 1000 * 12 / 4096;
			break;
		case 0x2e: /* vddio33 */
			result = (int)result * 72 * 100 / 4096;
			break;
		default:
			break;
		}
	} else if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H) {
		switch (lo) {
		case 0x28: /* 12v atx */
			result = (int)result * 1000 * 6 / 4096;
			break;
		default:
			break;
		}
	}
	*cur = result;
	return ret;
}

int bm_read_mcu_voltage(struct bm_device_info *bmdi, u8 lo, u32 *volt)
{
	int ret = 0;
	u8 data = 0;
	u32 result = 0;

	switch (lo) {
	case 0x26: /* 12v atx */
		if (BM1684_BOARD_TYPE(bmdi) == BOARD_TYPE_SC5_H) {
			ret = bm_mcu_read_reg(bmdi, lo, &data);
			if (ret)
				return ret;
			result = (u32)data;

			ret = bm_mcu_read_reg(bmdi, lo + 1, &data);
			if (ret)
				return ret;
			result |= (u32)data << 8;

			result = (int)result * 18 * 11 * 1000 / (4096 * 10);
		} else {
			result = 12 * 1000;
		}
		break;
	case 0x2a: /* vddio5 */
		result = 5 * 1000;
		break;
	case 0x2c: /* vddio18 */
		result = 18 * 100;
		break;
	case 0x2e: /* vddio33 */
		result = 33 * 100;
		break;
	case 0x30: /* vdd_phy */
		result = 8 * 100;
		break;
	case 0x32: /* vdd_pcie */
		result = 8 * 100;
		break;
	case 0x34: /* vdd_tpu_mem */
		result = 7 * 100;
		break;
	case 0x36: /* ddr_vddq */
		result = 11 * 100;
		break;
	case 0x38: /* ddr_vddqlp */
		result = 6 * 100;
		break;
	default:
		result = 0;
		break;
	}
	*volt = result;
	return 0;
}

int bm_read_sc5_power(struct bm_device_info *bmdi, u32 *power)
{
	int ret;
	u32 volt, curr;

	/* read 12V atx value */
	ret = bm_read_mcu_voltage(bmdi, 0x26, &volt);
	if (ret) {
		pr_err("read mcu voltage failed!\n");
		return ret;
	}
	ret = bm_read_mcu_current(bmdi, 0x28, &curr);
	if (ret) {
		pr_err("read mcu current failed!\n");
		return ret;
	}
	*power = volt * curr / 1000 / 1000;
	return 0;
}

int bm_read_sc5_plus_power(struct bm_device_info *bmdi, u32 *power)
{
	int ret;
	u32 volt, curr;

	/* read 12V atx value */
	ret = bm_read_mcu_voltage(bmdi, 0x26, &volt);
	if (ret) {
		pr_err("read mcu voltage failed!\n");
		return ret;
	}
	ret = bm_read_mcu_current(bmdi, 0x28, &curr);
	if (ret) {
		pr_err("read mcu current failed!\n");
		return ret;
	}
	*power = volt * curr;

	volt = 0;
	curr = 0;
	/* read 3.3v value */
	ret = bm_read_mcu_voltage(bmdi, 0x2e, &volt);
	if (ret) {
		pr_err("read mcu voltage failed!\n");
		return ret;
	}
	ret = bm_read_mcu_current(bmdi, 0x2e, &curr);
	if (ret) {
		pr_err("read mcu current failed!\n");
		return ret;
	}
	*power += volt * curr;
	*power /= 1000 * 1000;
	return 0;
}

static int bm_read_mcu_temp(struct bm_device_info *bmdi, int id, int *temp)
{
	int ret;
	u8 data;

	ret = bm_mcu_read_reg(bmdi, id + 4, &data);
	if (ret)
		return ret;
	*temp = (int) (*(signed char *)(&data));
	return ret;
}

int bm_read_mcu_chip_temp(struct bm_device_info *bmdi, int *temp)
{
	return bm_read_mcu_temp(bmdi, 0, temp);
}

int bm_read_mcu_board_temp(struct bm_device_info *bmdi, int *temp)
{
	return bm_read_mcu_temp(bmdi, 1, temp);
}

static int bm_eeprom_write_unlock(struct bm_device_info *bmdi)
{
	int ret = 0x0;

	bm_i2c_set_target_addr(bmdi, 0x17);

	ret = bm_i2c_write_byte(bmdi, 0x60, 0x43);
	if (ret < 0)
		return -1;

	ret = bm_i2c_write_byte(bmdi, 0x60, 0x4b);
	if (ret < 0)
		return -1;

	return ret;
}

static int bm_eeprom_write_lock(struct bm_device_info *bmdi)
{
	int ret = 0x0;

	bm_i2c_set_target_addr(bmdi, 0x17);

	ret = bm_i2c_write_byte(bmdi, 0x60, 0x4c);
	if (ret < 0)
		return -1;

	ret = bm_i2c_write_byte(bmdi, 0x60, 0x4f);
	if (ret < 0)
		return -1;

	return ret;
}

static int bm_set_eeprom(struct bm_device_info *bmdi, u8 offset, char *data, int size)
{
	int ret, i;

	for (i = 0; i < size; i++) {
		ret = bm_set_eeprom_reg(bmdi, offset + i, data[i]);
		if (ret)
			return ret;
		udelay(400); /* delay large enough is needed */
	}
	return 0;
}

static int bm_get_eeprom(struct bm_device_info *bmdi, u8 offset, char *data, int size)
{
	int ret, i;

	for (i = 0; i < size; i++) {
		ret = bm_get_eeprom_reg(bmdi, offset + i, data + i);
		if (ret)
			return ret;
	}
	return 0;
}

static int bm_set_sn(struct bm_device_info *bmdi, char *sn)
{
	int ret = 0x0;

	ret = bm_eeprom_write_unlock(bmdi);
	if (ret < 0)
		return ret;

	ret = bm_set_eeprom(bmdi, 0, sn, 17);
	if (ret < 0)
		return ret;

	ret = bm_eeprom_write_lock(bmdi);
	if (ret < 0)
		return ret;

	return ret;
}

int bm_get_sn(struct bm_device_info *bmdi, char *sn)
{
	return bm_get_eeprom(bmdi, 0, sn, 17);
}

int bm_burning_info_sn(struct bm_device_info *bmdi, unsigned long arg)
{
	char sn[18];
	char sn_zero[18] = {0};
	int ret;
	struct bm_chip_attr *c_attr = &bmdi->c_attr;

	ret = copy_from_user(sn, (char __user *)arg, sizeof(sn));
	if (ret) {
		pr_err("copy SN from user failed!\n");
		return ret;
	}

	mutex_lock(&c_attr->attr_mutex);
	if (strncmp(sn, sn_zero, 17)) {
		/* set SN */
		ret = bm_set_sn(bmdi, sn);
		if (ret) {
			pr_err("bmdrv: set SN failed\n");
			mutex_unlock(&c_attr->attr_mutex);
			return ret;
		}
	} else {
		/* display SN */
		ret = bm_get_sn(bmdi, sn);
		if (ret) {
			pr_err("bmdrv: get SN failed\n");
			mutex_unlock(&c_attr->attr_mutex);
			return ret;
		}
		ret = copy_to_user((char __user *)arg, sn, sizeof(sn));
	}
	mutex_unlock(&c_attr->attr_mutex);
	return ret;
}

static int bm_set_mac(struct bm_device_info *bmdi, int id, unsigned char *mac)
{
	int ret = 0x0;

	ret = bm_eeprom_write_unlock(bmdi);
	if (ret < 0)
		return ret;

	bm_set_eeprom(bmdi, 0x20 + 0x20 * id, mac, 6);
	if (ret < 0)
		return ret;

	ret = bm_eeprom_write_lock(bmdi);
	if (ret < 0)
		return ret;

	return ret;
}

static int bm_get_mac(struct bm_device_info *bmdi, int id, unsigned char *mac)
{
	return bm_get_eeprom(bmdi, 0x20 + 0x20 * id, mac, 6);
}

int bm_burning_info_mac(struct bm_device_info *bmdi, int id, unsigned long arg)
{
	unsigned char mac_bytes[6];
	unsigned char mac_bytes_zero[6] = {0};
	int ret;
	struct bm_chip_attr *c_attr = &bmdi->c_attr;

	ret = copy_from_user(mac_bytes, (unsigned char __user *)arg, sizeof(mac_bytes));
	if (ret) {
		pr_err("copy MAC from user failed!\n");
		return ret;
	}
	mutex_lock(&c_attr->attr_mutex);
	if (memcmp(mac_bytes, mac_bytes_zero, sizeof(mac_bytes))) {
		/* set MAC */
		ret = bm_set_mac(bmdi, id, mac_bytes);
		if (ret) {
			pr_err("bmdrv: set MAC %d failed!n", id);
			mutex_unlock(&c_attr->attr_mutex);
			return ret;
		}
	} else {
		/* get MAC */
		ret = bm_get_mac(bmdi, id, mac_bytes);
		if (ret) {
			pr_err("bmdrv: get MAC %d failed!n", id);
			mutex_unlock(&c_attr->attr_mutex);
			return ret;
		}
		ret = copy_to_user((unsigned char __user *)arg, mac_bytes, sizeof(mac_bytes));
	}
	mutex_unlock(&c_attr->attr_mutex);
	return ret;
}

static int bm_set_board_type(struct bm_device_info *bmdi, char b_type)
{
	int ret = 0x0;

	ret = bm_eeprom_write_unlock(bmdi);
	if (ret < 0)
		return ret;

	ret = bm_set_eeprom(bmdi, 0x60, &b_type, 1);
	if (ret < 0)
		return ret;

	ret = bm_eeprom_write_lock(bmdi);
	if (ret < 0)
		return ret;

	return ret;
}

int bm_get_board_type(struct bm_device_info *bmdi, char *b_type)
{
	return bm_get_eeprom(bmdi, 0x60, b_type, 1);
}

int bm_burning_info_board_type(struct bm_device_info *bmdi, unsigned long arg)
{
	char b_byte;
	int ret;
	struct bm_chip_attr *c_attr = &bmdi->c_attr;

	ret = get_user(b_byte, (char __user *)arg);
	if (ret) {
		pr_err("copy board type from user failed!\n");
		return ret;
	}
	mutex_lock(&c_attr->attr_mutex);
	if (b_byte != -1) {
		/* set board byte */
		ret = bm_set_board_type(bmdi, b_byte);
		if (ret) {
			pr_err("bmdrv: set board type failed!\n");
			mutex_unlock(&c_attr->attr_mutex);
			return ret;
		}
	} else {
		/* get board type */
		ret = bm_get_board_type(bmdi, &b_byte);
		if (ret) {
			pr_err("bmdrv: get board type failed!\n");
			mutex_unlock(&c_attr->attr_mutex);
			return ret;
		}
		ret = put_user(b_byte, (char __user *)arg);
	}
	mutex_unlock(&c_attr->attr_mutex);
	return ret;
}

#endif
