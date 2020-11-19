#include <linux/pci.h>
#include "bm_io.h"
#include "bm_pcie.h"
#include "bm_common.h"

void bm1682_map_bar(struct bm_device_info *bmdi, struct pci_dev *pdev)
{
}

void bm1682_unmap_bar(struct bm_bar_info *bari)
{
}

void bm1682_pcie_calculate_cdma_max_payload(struct bm_device_info *bmdi)
{
}

static const struct bm_bar_info bm1682_bar_layout[] = {
    {
        .bar0_len = 0x400000,
        .bar0_dev_start = 0x2000000,
        .bar1_len = 0x1000000,
        .bar1_dev_start = 0x50000000,
        .bar2_len = 0x1000000,
        .bar2_dev_start = 0x60000000,
    },
    {
        .bar0_len = 0x4000000,
        .bar0_dev_start = 0x0,
        .bar1_len = 0x10000000,
        .bar1_dev_start = 0x50000000,
        .bar2_len = 0x10000,
        .bar2_dev_start = 0x60000000,
    },
};

int bm1682_setup_bar_dev_layout(struct bm_bar_info *bar_info, const struct bm_bar_info *bar_layout)
{
    int i;
    int layout_cnt;
    if (NULL == bar_layout) {
	    bar_layout = bm1682_bar_layout;
	    layout_cnt = sizeof(bm1682_bar_layout)/sizeof(struct bm_bar_info);
    } else {
	    layout_cnt = 1;
    }

    for (i = 0; i < layout_cnt; i++) {
        if (bar_layout->bar0_len == bar_info->bar0_len &&
            bar_layout->bar1_len == bar_info->bar1_len &&
            bar_layout->bar2_len == bar_info->bar2_len) {
            bar_info->bar0_dev_start = bar_layout->bar0_dev_start;
            bar_info->bar1_dev_start = bar_layout->bar1_dev_start;
            bar_info->bar2_dev_start = bar_layout->bar2_dev_start;
            return 0;
        }
	bar_layout++;
    }
    /* Not find */
    return -1;
}
