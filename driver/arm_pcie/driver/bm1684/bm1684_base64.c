#include <linux/ioctl.h>
#include "bm_common.h"

static unsigned long int base64_compute_dstlen(uint64_t len, bool enc)
{
	int pad_count;
	unsigned long roundup;

	/* encode */
	if (enc)
		return (len + 2) / 3 * 4;
	/* decode */
	if (len == 0)
		return 0;

	pad_count = 0;
	roundup = len / 4 * 3;

	return roundup;
}

int base64_prepare(struct bm_device_info *base64_bmdi, struct ce_base base)
{
	int i;
	struct ce_desc *desc;
	uint32_t *ptr0;
	uint32_t *ptr1;

	bm_write32(base64_bmdi, 0x58004000, (16 << 16) | (16 << 24));
	bm_write32(base64_bmdi, 0x58004004, 0xffffffff);
	bm_write32(base64_bmdi, 0x58004010, 0xffffffff);

	desc = kmalloc(sizeof(struct ce_desc), GFP_KERNEL);
	memset(desc, 0x00, sizeof(struct ce_desc));
	desc->ctrl = 0x01882007;
	desc->alg = base.direction ?  1:0;
	desc->next = 0; /* no next descriptor */
	desc->src = (uint64_t)base.src;
	desc->dst = (uint64_t)base.dst;
	desc->len = base.len;
	desc->dstlen = base64_compute_dstlen(base.len, base.direction);

	ptr0 = (uint32_t *)desc;
	ptr1 = (uint32_t *)0x58004080;

	for (i = 0; i < 22; ++i)
		bm_write32(base64_bmdi, (uint64_t)(void *)(ptr1 + i), *(ptr0 + i));
	kfree(desc);

	return 0;
}

int base64_start(struct bm_device_info *base64_bmdi)
{
	uint32_t tmp;
	int i = 0;

	tmp = bm_read32(base64_bmdi, 0x58004000) | 0x00000001;
	while (bm_read32(base64_bmdi, 0x58004000) & 0x00000001) {
		i++;
		if (i > 10000)
			break;
	}

	bm_write32(base64_bmdi, 0x58004000, tmp);

	while (bm_read32(base64_bmdi, 0x58004000) & 0x00000001)
		;

	return 0;
}
