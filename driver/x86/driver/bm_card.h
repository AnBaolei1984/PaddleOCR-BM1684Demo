#ifndef SOC_MODE
#define BM_MAX_CARD_NUM	                32
#define BM_MAX_CHIP_NUM_PER_CARD	8
#define BM_MAX_CHIP_NUM                 128
#else
#define BM_MAX_CARD_NUM	                1
#define BM_MAX_CHIP_NUM_PER_CARD	1
#define BM_MAX_CHIP_NUM                 1
#endif

struct bm_card {
	int card_index;
	int chip_num;
	int dev_start_index;
	struct bm_device_info *card_bmdi[BM_MAX_CHIP_NUM_PER_CARD];
};

int bmdrv_card_init(struct bm_device_info *bmdi);
int bmdrv_card_deinit(struct bm_device_info *bmdi);
struct bm_card *bmdrv_card_get_bm_card(struct bm_device_info *bmdi);
