#ifndef _BM_UAPI_H_
#define _BM_UAPI_H_
#include <linux/types.h>
#include <bm_msg.h>

typedef struct bm_api {
	bm_api_id_t    api_id;
	u8 *api_addr;
	u32 api_size;
} bm_api_t;

typedef struct bm_api_ext {
	bm_api_id_t    api_id;
	u8 *api_addr;
	u32 api_size;
	u64 *api_handle;
} bm_api_ext_t;

typedef struct bm_api_data {
	bm_api_id_t    api_id;
	u64 api_handle;
	u64 data;
	s32 timeout;
} bm_api_data_t;

typedef struct bm_profile {
	u64 cdma_in_time;
	u64 cdma_in_counter;
	u64 cdma_out_time;
	u64 cdma_out_counter;
	u64 tpu_process_time;
	u64 sent_api_counter;
	u64 completed_api_counter;
} bm_profile_t;

struct bm_heap_stat {
	unsigned int mem_total;
	unsigned int mem_avail;
	unsigned int mem_used;
};

typedef struct bm_dev_stat {
	int mem_total;
	int mem_used;
	int tpu_util;
	int heap_num;
	struct bm_heap_stat heap_stat[4];
} bm_dev_stat_t;

/*
 * bm misc info
 */
#define BM_DRV_SOC_MODE 1
#define BM_DRV_PCIE_MODE 0
struct bm_misc_info {
	int pcie_soc_mode; /*0---pcie; 1---soc*/
	int ddr_ecc_enable; /*0---disable; 1---enable*/
	long long ddr0a_size;
	long long ddr0b_size;
	long long ddr1_size;
	long long ddr2_size;
	unsigned int chipid;
#define BM1682_CHIPID_BIT_MASK	(0X1 << 0)
#define BM1684_CHIPID_BIT_MASK	(0X1 << 1)
	unsigned long chipid_bit_mask;
	unsigned int driver_version;
	int domain_bdf; /*[31:16]-domin,[15:8]-bus_id,[7:3]-device_id,[2:0]-func_num*/
	int board_version; /*hardware board version [23:16]-mcu sw version, [15:8]-board type, [7:0]-hw version*/
	int a53_enable;
};


/*
 * bm boot info
 */
#define BOOT_INFO_VERSION       0xabcd
#define BOOT_INFO_VERSION_V1    0xabcd0001

struct boot_info_append_v1 {
	unsigned int a53_enable;
	unsigned long long heap2_size;
};

struct bm_boot_info {
	unsigned int deadbeef;
	unsigned int ddr_ecc_enable; /*0---disable; 1---enable*/
	unsigned long long ddr_0a_size;
	unsigned long long ddr_0b_size;
	unsigned long long ddr_1_size;
	unsigned long long ddr_2_size;
	unsigned int ddr_vendor_id;
	unsigned int ddr_mode;/*[31:16]-ddr_gmem_mode, [15:0]-ddr_power_mode*/
	unsigned int ddr_rank_mode;
	unsigned int tpu_max_clk;
	unsigned int tpu_min_clk;
	unsigned int temp_sensor_exist;
	unsigned int tpu_power_sensor_exist;
	unsigned int board_power_sensor_exist;
	unsigned int fan_exist;
	unsigned int max_board_power;
	unsigned int boot_info_version; /*[31:16]-0xabcd,[15:0]-version num*/
	union {
		struct boot_info_append_v1 append_v1;
	} append;
};

typedef enum {
	PERF_MONITOR_GDMA = 0,
	PERF_MONITOR_TPU = 1
} PERF_MONITOR_ID;

/*
* bm performace monitor
*/
struct bm_perf_monitor {
	long long buffer_start_addr;
	int buffer_size;
	PERF_MONITOR_ID monitor_id;
};

struct bm_reg {
	int reg_addr;
	int reg_value;
};

#define BMDEV_IOCTL_MAGIC  'p'
#define BMDEV_MEMCPY			_IOW('p', 0x00, unsigned long)

#define BMDEV_ALLOC_GMEM		_IOWR('p', 0x10, unsigned long)
#define BMDEV_FREE_GMEM			_IOW('p', 0x11, unsigned long)
#define BMDEV_TOTAL_GMEM		_IOWR('p', 0x12, unsigned long)
#define BMDEV_AVAIL_GMEM		_IOWR('p', 0x13, unsigned long)
#define BMDEV_REQUEST_ARM_RESERVED	_IOWR('p', 0x14, unsigned long)
#define BMDEV_RELEASE_ARM_RESERVED	_IOW('p',  0x15, unsigned long)
#define BMDEV_MAP_GMEM			_IOWR('p', 0x16, unsigned long) //not used, map in mmap
#define BMDEV_INVALIDATE_GMEM		_IOWR('p', 0x17, unsigned long)
#define BMDEV_FLUSH_GMEM		_IOWR('p', 0x18, unsigned long)
#define BMDEV_ALLOC_GMEM_ION		_IOW('p', 0x19, unsigned long)

#define BMDEV_SEND_API			_IOW('p', 0x20, unsigned long)
#define BMDEV_THREAD_SYNC_API		_IOW('p', 0x21, unsigned long)
#define BMDEV_DEVICE_SYNC_API		_IOW('p', 0x23, unsigned long)
#define BMDEV_HANDLE_SYNC_API		_IOW('p', 0x27, unsigned long)
#define BMDEV_SEND_API_EXT		_IOW('p', 0x28, unsigned long)
#define BMDEV_QUERY_API_RESULT		_IOW('p', 0x29, unsigned long)

#define BMDEV_GET_MISC_INFO		_IOWR('p', 0x30, unsigned long)
#define BMDEV_UPDATE_FW_A9		_IOW('p',  0x31, unsigned long)
#define BMDEV_GET_PROFILE		_IOWR('p', 0x32, unsigned long)
#define BMDEV_PROGRAM_A53               _IOWR('p', 0x33, unsigned long)
#define BMDEV_GET_BOOT_INFO		_IOWR('p', 0x34, unsigned long)
#define BMDEV_UPDATE_BOOT_INFO		_IOWR('p', 0x35, unsigned long)
#define BMDEV_SN                        _IOWR('p', 0x36, unsigned long)
#define BMDEV_MAC0                      _IOWR('p', 0x37, unsigned long)
#define BMDEV_MAC1                      _IOWR('p', 0x38, unsigned long)
#define BMDEV_BOARD_TYPE                _IOWR('p', 0x39, unsigned long)
#define BMDEV_PROGRAM_MCU               _IOWR('p', 0x3a, unsigned long)
#define BMDEV_CHECKSUM_MCU              _IOWR('p', 0x3b, unsigned long)
#define BMDEV_SET_REG                   _IOWR('p', 0x3c, unsigned long)
#define BMDEV_GET_REG                   _IOWR('p', 0x3d, unsigned long)
#define BMDEV_GET_DEV_STAT              _IOWR('p', 0x3e, unsigned long)

#define BMDEV_TRACE_ENABLE		_IOW('p',  0x40, unsigned long)
#define BMDEV_TRACE_DISABLE		_IOW('p',  0x41, unsigned long)
#define BMDEV_TRACEITEM_NUMBER		_IOWR('p', 0x42, unsigned long)
#define BMDEV_TRACE_DUMP		_IOWR('p', 0x43, unsigned long)
#define BMDEV_TRACE_DUMP_ALL		_IOWR('p', 0x44, unsigned long)
#define BMDEV_ENABLE_PERF_MONITOR       _IOWR('p', 0x45, unsigned long)
#define BMDEV_DISABLE_PERF_MONITOR      _IOWR('p', 0x46, unsigned long)
#define BMDEV_GET_DEVICE_TIME           _IOWR('p', 0x47, unsigned long)

#define BMDEV_SET_TPU_DIVIDER		_IOWR('p', 0x50, unsigned long)
#define BMDEV_SET_MODULE_RESET		_IOWR('p', 0x51, unsigned long)
#define BMDEV_SET_TPU_FREQ		_IOWR('p', 0x52, unsigned long)
#define BMDEV_GET_TPU_FREQ		_IOWR('p', 0x53, unsigned long)

#define BMDEV_TRIGGER_VPP               _IOWR('p', 0x60, unsigned long)

#define BMDEV_BASE64_PREPARE            _IOWR('p', 0x70, unsigned long)
#define BMDEV_BASE64_START              _IOWR('p', 0x71, unsigned long)
#define BMDEV_BASE64_CODEC              _IOWR('p', 0x72, unsigned long)

#define BMDEV_I2C_READ_SLAVE            _IOWR('p', 0x73, unsigned long)
#define BMDEV_I2C_WRITE_SLAVE           _IOWR('p', 0x74, unsigned long)

#define BMDEV_TRIGGER_BMCPU             _IOWR('p', 0x80, unsigned long)

#define BMCTL_GET_DEV_CNT		_IOR('q', 0x0, unsigned long)
#define BMCTL_GET_SMI_ATTR		_IOWR('q', 0x01, unsigned long)
#define BMCTL_SET_LED			_IOWR('q', 0x02, unsigned long)
#define BMCTL_TEST_I2C1			_IOWR('q', 0x9, unsigned long) // test
#define BMCTL_SET_ECC			_IOWR('q', 0x03, unsigned long)
#define BMCTL_GET_PROC_GMEM             _IOWR('q', 0x04, unsigned long)
#define BMCTL_DEV_RECOVERY             _IOWR('q', 0x05, unsigned long)
#define BMCTL_GET_DRIVER_VERSION       _IOR('q', 0x06, unsigned long)

struct bm_smi_attr {
	int dev_id;
	int chip_id;
	int chip_mode;  /*0---pcie; 1---soc*/
	int domain_bdf;
	int status;
	int card_index;
	int chip_index_of_card;

	int mem_used;
	int mem_total;
	int tpu_util;

	int board_temp;
	int chip_temp;
	int board_power;
	int tpu_power;
	int fan_speed;

	int vdd_tpu_volt;
	int vdd_tpu_curr;
	int atx12v_curr;

	int tpu_min_clock;
	int tpu_max_clock;
	int tpu_current_clock;
	int board_max_power;

	int ecc_enable;
	int ecc_correct_num;

	char sn[18];
	char board_type[5];

	/*if or not to display board endline and board attr*/
	int board_endline;
	int board_attr;
};

struct bm_smi_proc_gmem {
	int dev_id;
	pid_t pid[128];
	u64 gmem_used[128];
	int proc_cnt;
};

/*
* definations used by base64
*/

struct ce_desc {
	uint32_t     ctrl;
	uint32_t     alg;
	uint64_t     next;
	uint64_t     src;
	uint64_t     dst;
	uint64_t     len;
	union {
		uint8_t      key[32];
		uint64_t     dstlen; //destination amount, only used in base64
	};
	uint8_t      iv[16];
};

struct ce_base {
	uint64_t     src;
	uint64_t     dst;
	uint64_t     len;
	bool direction;
};

struct ce_reg {
	uint32_t ctrl; //control
	uint32_t intr_enable; //interrupt mask
	uint32_t desc_low; //descriptor base low 32bits
	uint32_t desc_high; //descriptor base high 32bits
	uint32_t intr_raw; //interrupt raw, write 1 to clear
	uint32_t se_key_valid; //secure key valid, read only
	uint32_t cur_desc_low; //current reading descriptor
	uint32_t cur_desc_high; //current reading descriptor
	uint32_t r1[24]; //reserved
	uint32_t desc[22]; //PIO mode descriptor register
	uint32_t r2[10]; //reserved
	uint32_t key[24];		//keys
	uint32_t r3[8]; //reserved
	uint32_t iv[12]; //iv
	uint32_t r4[4]; //reserved
	uint32_t sha_param[8]; //sha parameter
};

//for i2c
struct bm_i2c_param {
	int i2c_slave_addr;
	int i2c_index;
	int i2c_cmd;
	int value;
	int op;
};

#endif /* _BM_UAPI_H_ */
