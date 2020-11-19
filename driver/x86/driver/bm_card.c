#include "bm_card.h"
#include "bm1684_card.h"
#include "bm_common.h"

static struct bm_card *g_bmcd[BM_MAX_CARD_NUM] = {NULL};

int bm_card_get_chip_num(struct bm_device_info *bmdi)
{
#ifdef SOC_MODE
	return 1;
#else
	if (bmdi->cinfo.chip_id == 0x1682)
		return 1;
	if (bmdi->cinfo.chip_id == 0x1684)
		return bm1684_card_get_chip_num(bmdi);
	else
		return 1;
#endif
}

static bool bm_chip_in_card(struct bm_card *bmcd, struct bm_device_info *bmdi)
{
	int dev_index = bmdi->dev_index;

	if (dev_index < 0)
		return false;
	if (dev_index > BM_MAX_CHIP_NUM)
		return false;

	if ((dev_index < bmcd->dev_start_index + bmcd->chip_num)
		&& (dev_index >= bmcd->dev_start_index))
		return true;
	else
		return false;
}

struct bm_card *bmdrv_card_get_bm_card(struct bm_device_info *bmdi)
{
	int i = 0;

	if (bmdi->bmcd != NULL)
		return bmdi->bmcd;

	for (i = 0; i < BM_MAX_CARD_NUM; i++) {
		if (g_bmcd[i] != NULL) {
			if (bm_chip_in_card(g_bmcd[i], bmdi) == true)
				return g_bmcd[i];
		} else {
			return NULL;
		}
	}
	return NULL;
}

static int bm_add_chip_to_card(struct bm_device_info *bmdi)
{
	int i = 0;
	struct bm_card *bmcd = NULL;

	if (bmdi->cinfo.chip_index == 0x0) {
		bmcd = kzalloc(sizeof(struct bm_card), GFP_KERNEL);
		if (!bmcd) {
			pr_err("bm card alloc fail\n");
			return -1;
		}
		for (i = 0; i < BM_MAX_CARD_NUM; i++) {
			if (g_bmcd[i] == NULL) {
				g_bmcd[i] = bmcd;
				g_bmcd[i]->card_index = i;
				g_bmcd[i]->chip_num = bm_card_get_chip_num(bmdi);
				g_bmcd[i]->dev_start_index = bmdi->dev_index;
				g_bmcd[i]->card_bmdi[0] = bmdi;
				bmdi->bmcd = g_bmcd[i];
				return 0;
			}
		}
		pr_err("card not find\n");
		return -1;
	} else {
		bmcd = bmdrv_card_get_bm_card(bmdi);
		if (bmcd == NULL) {
			pr_err("bmdrv_card_get_bm_card fail\n");
			return -1;
		}
		bmdi->bmcd = bmcd;
		bmcd->card_bmdi[bmdi->cinfo.chip_index] = bmdi;
	}
	return 0;
}

static int bm_remove_chip_from_card(struct bm_device_info *bmdi)
{
	int index = 0x0;

	if (bmdi->bmcd == NULL)
		return 0;

	index = bmdi->bmcd->card_index;
	if (g_bmcd[index] == NULL)
		return 0;

	g_bmcd[index]->card_bmdi[bmdi->cinfo.chip_index] = NULL;
	if (bmdi->cinfo.chip_index == 0x0) {
		kfree(g_bmcd[index]);
		g_bmcd[index] = NULL;
	}
	return 0;
}

int bmdrv_card_init(struct bm_device_info *bmdi)
{
	int ret = 0;

	ret = bm_add_chip_to_card(bmdi);
	if (ret < 0) {
		pr_err("add chip to card fail\n");
		return ret;
	}

	return ret;
}

int bmdrv_card_deinit(struct bm_device_info *bmdi)
{
	int ret = 0;

	ret = bm_remove_chip_from_card(bmdi);
	if (ret < 0) {
		pr_err("remove chip from card fail\n");
		return ret;
	}

	return ret;
}
