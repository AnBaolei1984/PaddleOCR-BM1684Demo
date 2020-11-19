#ifndef _BM_FW_H_
#define _BM_FW_H_

#define LAST_INI_REG_VAL	 0x76125438

struct bm_device_info;

enum fw_downlod_stage {
	FW_START = 0,
	DDR_INIT_DONE = 1,
	DL_DDR_IMG_DONE	= 2
};

typedef struct bm_firmware_desc {
	unsigned int *itcm_fw;	//bytes
	int itcmfw_size;
	unsigned int *ddr_fw;
	int ddrfw_size;		//bytes
} bm_fw_desc, *pbm_fw_desc;

int bmdrv_fw_load(struct bm_device_info *bmdi, struct file *file, pbm_fw_desc fw);
void bmdrv_fw_unload(struct bm_device_info *bmdi);

#endif
