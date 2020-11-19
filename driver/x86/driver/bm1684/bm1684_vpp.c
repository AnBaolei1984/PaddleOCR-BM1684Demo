#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/reset.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/signal.h>
#else
#include <linux/sched.h>
#endif

#include "bm1684_cdma.h"
#include "bm_common.h"
#include "bm_irq.h"
#include "bm_memcpy.h"
#include "bm1684_irq.h"
#include "bm_gmem.h"
#include "bm_memcpy.h"

#define VPP_OK                             (0)
#define VPP_ERR                            (-1)
#define VPP_ERR_COPY_FROM_USER             (-2)
#define VPP_ERR_WRONG_CROPNUM              (-3)
#define VPP_ERR_INVALID_FD                 (-4)
#define VPP_ERR_INT_TIMEOUT                (-5)
#define VPP_ERR_INVALID_PA                 (-6)
#define VPP_ERR_INVALID_CMD                (-7)

#define VPP_ENOMEM                         (-12)
#define VPP_ERR_IDLE_BIT_MAP               (-256)
#define VPP_ERESTARTSYS                    (-512)

#define ALIGN16(x) ((x + 0xf) & (~0xf))

u64 ion_region[2] = {0, 0};
u64 npu_reserved[2] = {0, 0};
u64 vpu_reserved[2] = {0, 0};

#if 0
/*Positive number * 1024*/
/*negative number * 1024, then Complement code*/
YPbPr2RGB, BT601
Y = 0.299 R + 0.587 G + 0.114 B
U = -0.1687 R - 0.3313 G + 0.5 B + 128
V = 0.5 R - 0.4187 G - 0.0813 B + 128
R = Y + 1.4018863751529200 (Cr-128)
G = Y - 0.345806672214672 (Cb-128) - 0.714902851111154 (Cr-128)
B = Y + 1.77098255404941 (Cb-128)

YCbCr2RGB, BT601
Y = 16  + 0.257 * R + 0.504 * g + 0.098 * b
Cb = 128 - 0.148 * R - 0.291 * g + 0.439 * b
Cr = 128 + 0.439 * R - 0.368 * g - 0.071 * b
R = 1.164 * (Y - 16) + 1.596 * (Cr - 128)
G = 1.164 * (Y - 16) - 0.392 * (Cb - 128) - 0.812 * (Cr - 128)
B = 1.164 * (Y - 16) + 2.016 * (Cb - 128)
#endif

int csc_matrix_list[CSC_MAX][12] = {
//YCbCr2RGB,BT601
{0x000004a8, 0x00000000, 0x00000662, 0xfffc8450,
	0x000004a8, 0xfffffe6f, 0xfffffcc0, 0x00021e4d,
	0x000004a8, 0x00000812, 0x00000000, 0xfffbaca8 },
//YPbPr2RGB,BT601
{0x00000400, 0x00000000, 0x0000059b, 0xfffd322d,
	0x00000400, 0xfffffea0, 0xfffffd25, 0x00021dd6,
	0x00000400, 0x00000716, 0x00000000, 0xfffc74bc },
//RGB2YCbCr,BT601
{ 0x107, 0x204, 0x64, 0x4000,
	0xffffff68, 0xfffffed6, 0x1c2, 0x20000,
	0x1c2, 0xfffffe87, 0xffffffb7, 0x20000 },
//YCbCr2RGB,BT709
{ 0x000004a8, 0x00000000, 0x0000072c, 0xfffc1fa4,
	0x000004a8, 0xffffff26, 0xfffffdde, 0x0001338e,
	0x000004a8, 0x00000873, 0x00000000, 0xfffb7bec },
//RGB2YCbCr,BT709
{ 0x000000bb, 0x00000275, 0x0000003f, 0x00004000,
	0xffffff99, 0xfffffea5, 0x000001c2, 0x00020000,
	0x000001c2, 0xfffffe67, 0xffffffd7, 0x00020000 },
//RGB2YPbPr,BT601
{ 0x132, 0x259, 0x74, 0,
	0xffffff54, 0xfffffead, 0x200, 0x20000,
	0x200, 0xfffffe54, 0xffffffad, 0x20000 },
//YPbPr2RGB,BT709
{ 0x00000400, 0x00000000, 0x0000064d, 0xfffcd9be,
	0x00000400, 0xffffff40, 0xfffffe21, 0x00014fa1,
	0x00000400, 0x0000076c, 0x00000000, 0xfffc49ed },
//RGB2YPbPr,BT709
{ 0x000000da, 0x000002dc, 0x0000004a, 0,
	0xffffff8b, 0xfffffe75, 0x00000200, 0x00020000,
	0x00000200, 0xfffffe2f, 0xffffffd1, 0x00020000 },
};

static int __attribute__((unused)) check_ion_pa(unsigned long pa)
{
	int ret = VPP_OK;

	if (pa > 0)
	ret = ((pa >= ion_region[0]) && (pa <= (ion_region[0] + ion_region[1]))) ? VPP_OK : VPP_ERR;

	return ret;
}
static int __attribute__((unused)) check_vpu_pa(unsigned long pa)
{
	int ret = VPP_OK;

	if (pa > 0)
	ret = ((pa >= vpu_reserved[0]) && (pa <= (vpu_reserved[0] + vpu_reserved[1]))) ? VPP_OK : VPP_ERR;

	return ret;
}
static int __attribute__((unused)) check_npu_pa(unsigned long pa)
{
	int ret = VPP_OK;

	if (pa > 0)
	ret = ((pa >= npu_reserved[0]) && (pa <= (npu_reserved[0] + npu_reserved[1]))) ? VPP_OK : VPP_ERR;

	return ret;
}


static void vpp_reg_write(int core_id, struct bm_device_info *bmdi, unsigned int val, unsigned int offset)
{
	if (core_id == 0)
		vpp0_reg_write(bmdi, offset, val);
	else
		vpp1_reg_write(bmdi, offset, val);
}

static unsigned int vpp_reg_read(int core_id, struct bm_device_info *bmdi, unsigned int offset)
{
	unsigned int ret;

	if (core_id == 0)
		ret = vpp0_reg_read(bmdi, offset);
	else
		ret = vpp1_reg_read(bmdi, offset);
	return ret;
}

void vpp_clear_int(struct bm_device_info *bmdi, int core_id)
{

	if (core_id == 0)
		vpp0_reg_write(bmdi, VPP_INT_CLEAR, 0xffffffff);
	else
		vpp1_reg_write(bmdi, VPP_INT_CLEAR, 0xffffffff);
}

irqreturn_t vpp0_irq_handler(struct bm_device_info *bmdi)
{
//	pr_info("vpp0 irq handler\n");
	bmdi->vppdrvctx.got_event_vpp[0] = 1;
	wake_up(&bmdi->vppdrvctx.wq_vpp[0]);

	return IRQ_HANDLED;
}
irqreturn_t vpp1_irq_handler(struct bm_device_info *bmdi)
{
//	pr_info("vpp1 irq handler\n");
	bmdi->vppdrvctx.got_event_vpp[1] = 1;
	wake_up(&bmdi->vppdrvctx.wq_vpp[1]);

	return IRQ_HANDLED;
}

static void vpp_check_hw_idle(int core_id, struct bm_device_info *bmdi)
{
	int status, raw_status;
	static int j = 1;

	for (;;) {

		status = (vpp_reg_read(core_id, bmdi, VPP_STATUS) >> 8) & 0x1;
		if (status != 0x01) {
			j++;
			if (j == 2)
				pr_info("vpp status %d, vpp is busy!!!!,sysid %d, current->pid  %d,current->tgid  %d, dev_index  %d\n",
				status, core_id, current->pid, current->tgid, bmdi->dev_index);
		} else {
			j = 0;
			break;
		}
	}
	for (;;) {

		raw_status = vpp_reg_read(core_id, bmdi, VPP_INT_RAW_STATUS);
		if (raw_status != 0x0) {
			vpp_reg_write(core_id, bmdi, 0xffffffff, VPP_INT_CLEAR);
			raw_status = vpp_reg_read(core_id, bmdi, VPP_INT_RAW_STATUS);
			pr_info("vpp raw_status: 0x%08x, dev_index  %d\n", raw_status, bmdi->dev_index);
		} else {
			break;
		}
	}
}

static void vpp_setup_desc(unsigned char core_id, struct bm_device_info *bmdi,
	struct vpp_batch *batch, u64 *vpp_desc_pa)
{
	if ((core_id != 0) && (core_id != 1))
		pr_info("core_id err in vpp_setup_desc, core_id %d, dev_index  %d\n", core_id, bmdi->dev_index);
	/*check vpp hw idle*/
	vpp_check_hw_idle(core_id, bmdi);

	/*set cmd list addr*/
	vpp_reg_write(core_id, bmdi, (unsigned int)(vpp_desc_pa[0] & 0xffffffff), VPP_CMD_BASE);
	vpp_reg_write(core_id, bmdi, (unsigned int)((vpp_desc_pa[0] >> 32) & 0x7), VPP_CMD_BASE_EXT);
#if 0
	u32 cmd_base;
	u32 cmd_base_ext;
	cmd_base = vpp_reg_read(core_id, bmdi, VPP_CMD_BASE);
	cmd_base_ext = vpp_reg_read(core_id, bmdi, VPP_CMD_BASE_EXT);
#endif

	/*set vpp0 and vpp1 as cmd list mode and interrupt mode*/
	vpp_reg_write(core_id, bmdi, 0x00040004, VPP_CONTROL0);//cmd list
	vpp_reg_write(core_id, bmdi, 0x03, VPP_INT_EN);//interruput mode : vpp_in

	/*start vpp hw work*/
	vpp_reg_write(core_id, bmdi, 0x00010001, VPP_CONTROL0);
}

static int vpp_prepare_cmd_list(struct bm_device_info *bmdi, struct vpp_batch *batch,
	u64 *vpp_desc_pa)
{
	int idx, srcUV = 0, dstUV = 0, ret = VPP_OK;
	int input_format, output_format, input_plannar, output_plannar;
	int src_csc_en = 0;
	int csc_mode = YCbCr2RGB_BT601;
	int scale_enable = 1;   //v3:0   v4:1
	unsigned long dst_addr;
	unsigned long dst_addr0;
	unsigned long dst_addr1;
	unsigned long dst_addr2;
	struct vpp_descriptor *pdes_vpp[VPP_CROP_NUM_MAX];

	//pr_info("in vpp_prepare_cmd_list\n");

	for (idx = 0; idx < batch->num; idx++) {

		struct vpp_cmd *cmd = (batch->cmd + idx);

		if (((upper_32_bits(cmd->src_addr0) & 0x7) != 0x3) &&
			((upper_32_bits(cmd->src_addr0) & 0x7) != 0x4)) {
			pr_err("pa,cmd->src_addr0  0x%lx, input must be on ddr1 or ddr2\n", cmd->src_addr0);
			ret = VPP_ERR_INVALID_PA;
		}

		pdes_vpp[idx] = (struct vpp_descriptor *)kzalloc(sizeof(struct vpp_descriptor), GFP_USER);
		if (pdes_vpp[idx] == NULL) {
			ret = -VPP_ENOMEM;
			pr_info("pdes_vpp[%d] is NULL, dev_index  %d\n", idx, bmdi->dev_index);
			return ret;
		}

		input_format = cmd->src_format;
		input_plannar = cmd->src_plannar;
		output_format = cmd->dst_format;
		output_plannar = cmd->dst_plannar;

		src_csc_en = cmd->src_csc_en;

		if (cmd->src_uv_stride == 0) {
			srcUV = (((input_format == YUV420) || (input_format == YUV422)) && (input_plannar == 1))
				? (cmd->src_stride/2) : (cmd->src_stride);
		} else {
			srcUV = cmd->src_uv_stride;
		}
		if (cmd->dst_uv_stride == 0) {
			dstUV = (((output_format == YUV420) || (output_format == YUV422)) && (output_plannar == 1))
				? (cmd->dst_stride/2) : (cmd->dst_stride);
		} else {
			dstUV = cmd->dst_uv_stride;
		}

		if ((cmd->csc_type >= 0) && (cmd->csc_type < CSC_MAX))
			csc_mode = cmd->csc_type;

		/*  the first crop*/
		if (idx == (batch->num - 1)) {
			pdes_vpp[idx]->des_head = 0x300 + idx;
			pdes_vpp[idx]->next_cmd_base = 0x0;
		} else {
			pdes_vpp[idx]->des_head = 0x200 + idx;
			pdes_vpp[idx]->next_cmd_base = (unsigned int)(vpp_desc_pa[idx + 1] & 0xffffffff);
			pdes_vpp[idx]->des_head |= (unsigned int)(((vpp_desc_pa[idx + 1] >> 32) & 0x7) << 16);
		}

		if (cmd->csc_type == CSC_USER_DEFINED) {
			pdes_vpp[idx]->csc_coe00 = cmd->matrix.csc_coe00 & 0x1fff;
			pdes_vpp[idx]->csc_coe01 = cmd->matrix.csc_coe01  & 0x1fff;
			pdes_vpp[idx]->csc_coe02 = cmd->matrix.csc_coe02  & 0x1fff;
			pdes_vpp[idx]->csc_add0  = cmd->matrix.csc_add0  & 0x1fffff;
			pdes_vpp[idx]->csc_coe10 = cmd->matrix.csc_coe10  & 0x1fff;
			pdes_vpp[idx]->csc_coe11 = cmd->matrix.csc_coe11  & 0x1fff;
			pdes_vpp[idx]->csc_coe12 = cmd->matrix.csc_coe12  & 0x1fff;
			pdes_vpp[idx]->csc_add1  = cmd->matrix.csc_add1  & 0x1fffff;
			pdes_vpp[idx]->csc_coe20 = cmd->matrix.csc_coe20  & 0x1fff;
			pdes_vpp[idx]->csc_coe21 = cmd->matrix.csc_coe21  & 0x1fff;
			pdes_vpp[idx]->csc_coe22 = cmd->matrix.csc_coe22 & 0x1fff;
			pdes_vpp[idx]->csc_add2  = cmd->matrix.csc_add2 & 0x1fffff;
		} else {
		/*not change*/
			pdes_vpp[idx]->csc_coe00 = csc_matrix_list[csc_mode][0]  & 0x1fff;
			pdes_vpp[idx]->csc_coe01 = csc_matrix_list[csc_mode][1]  & 0x1fff;
			pdes_vpp[idx]->csc_coe02 = csc_matrix_list[csc_mode][2]  & 0x1fff;
			pdes_vpp[idx]->csc_add0  = csc_matrix_list[csc_mode][3]  & 0x1fffff;
			pdes_vpp[idx]->csc_coe10 = csc_matrix_list[csc_mode][4]  & 0x1fff;
			pdes_vpp[idx]->csc_coe11 = csc_matrix_list[csc_mode][5]  & 0x1fff;
			pdes_vpp[idx]->csc_coe12 = csc_matrix_list[csc_mode][6]  & 0x1fff;
			pdes_vpp[idx]->csc_add1  = csc_matrix_list[csc_mode][7]  & 0x1fffff;
			pdes_vpp[idx]->csc_coe20 = csc_matrix_list[csc_mode][8]  & 0x1fff;
			pdes_vpp[idx]->csc_coe21 = csc_matrix_list[csc_mode][9]  & 0x1fff;
			pdes_vpp[idx]->csc_coe22 = csc_matrix_list[csc_mode][10] & 0x1fff;
			pdes_vpp[idx]->csc_add2  = csc_matrix_list[csc_mode][11] & 0x1fffff;
		}

		/*src crop parameter*/
		pdes_vpp[idx]->src_ctrl = ((src_csc_en & 0x1) << 11)
			| ((cmd->src_plannar & 0x1) << 8)
			| ((cmd->src_endian & 0x1) << 5)
			| ((cmd->src_endian_a & 0x1) << 4)
			| (cmd->src_format & 0xf);

		pdes_vpp[idx]->src_crop_st = (cmd->src_axisX << 16) | cmd->src_axisY;
		pdes_vpp[idx]->src_crop_size = (cmd->src_cropW << 16) | cmd->src_cropH;
		pdes_vpp[idx]->src_stride = (cmd->src_stride << 16) | (srcUV & 0xffff);

		if ((cmd->src_fd0 == 0) && (cmd->src_addr0 != 0)) {/*src addr is pa*/
			pdes_vpp[idx]->src_ry_base = (unsigned int)(cmd->src_addr0 & 0xffffffff);
			pdes_vpp[idx]->src_gu_base = (unsigned int)(cmd->src_addr1 & 0xffffffff);
			pdes_vpp[idx]->src_bv_base = (unsigned int)(cmd->src_addr2 & 0xffffffff);

			pdes_vpp[idx]->src_ry_base_ext = (unsigned int)((cmd->src_addr0 >> 32) & 0x7);
			pdes_vpp[idx]->src_gu_base_ext = (unsigned int)((cmd->src_addr1 >> 32) & 0x7);
			pdes_vpp[idx]->src_bv_base_ext = (unsigned int)((cmd->src_addr2 >> 32) & 0x7);
		} else {/*src addr is not pa*/
			pr_err("%s, src addr0 is not physical addr\n", __func__);
		}

		/*dst crop parameter*/
		pdes_vpp[idx]->dst_ctrl = ((cmd->dst_plannar & 0x1) << 8)
			| ((cmd->dst_endian & 0x1) << 5)
			| ((cmd->dst_endian_a & 0x1) << 4)
			| (cmd->dst_format & 0xf);

		pdes_vpp[idx]->dst_crop_st = (cmd->dst_axisX << 16) | cmd->dst_axisY;
		pdes_vpp[idx]->dst_crop_size = (cmd->dst_cropW << 16) | cmd->dst_cropH;
		pdes_vpp[idx]->dst_stride = (cmd->dst_stride << 16) | (dstUV & 0xffff);

		if ((cmd->dst_fd0 == 0) && (cmd->dst_addr0 != 0)) {/*dst addr is pa*/
			/*if dst fmr is rgbp and only channel 0 addr is valid, The data for the three channels*/
			/* are b, g, r; not the original order:r, g, b*/
			if ((output_format == RGB24) && ((cmd->dst_plannar & 0x1) == 0x1)
				&& (cmd->dst_addr1 == 0) && (cmd->dst_addr2 == 0)) {
				dst_addr = cmd->dst_addr0;
				dst_addr0 = dst_addr + 2 * cmd->dst_cropH * ALIGN16(cmd->dst_cropW);
				dst_addr1 = dst_addr + cmd->dst_cropH * ALIGN16(cmd->dst_cropW);
				dst_addr2 = dst_addr;

				pdes_vpp[idx]->dst_ry_base = lower_32_bits(dst_addr0);
				pdes_vpp[idx]->dst_gu_base = lower_32_bits(dst_addr1);
				pdes_vpp[idx]->dst_bv_base = lower_32_bits(dst_addr2);

				pdes_vpp[idx]->dst_ry_base_ext = (unsigned int)(upper_32_bits(dst_addr0) & 0x7);
				pdes_vpp[idx]->dst_gu_base_ext = (unsigned int)(upper_32_bits(dst_addr1) & 0x7);
				pdes_vpp[idx]->dst_bv_base_ext = (unsigned int)(upper_32_bits(dst_addr2) & 0x7);
			} else {
				pdes_vpp[idx]->dst_ry_base = (unsigned int)(cmd->dst_addr0 & 0xffffffff);
				pdes_vpp[idx]->dst_gu_base = (unsigned int)(cmd->dst_addr1 & 0xffffffff);
				pdes_vpp[idx]->dst_bv_base = (unsigned int)(cmd->dst_addr2 & 0xffffffff);

				pdes_vpp[idx]->dst_ry_base_ext = (unsigned int)((cmd->dst_addr0 >> 32) & 0x7);
				pdes_vpp[idx]->dst_gu_base_ext = (unsigned int)((cmd->dst_addr1 >> 32) & 0x7);
				pdes_vpp[idx]->dst_bv_base_ext = (unsigned int)((cmd->dst_addr2 >> 32) & 0x7);
			}
		} else {/*dst addr is not pa*/
			pr_err("%s, line : %d, dst addr0 is not physical addr\n", __func__, __LINE__);
		}

		/*scl_ctrl parameter*/
		pdes_vpp[idx]->scl_ctrl = ((cmd->ver_filter_sel & 0xf) << 12)
			| ((cmd->hor_filter_sel & 0xf) << 8)
			| (scale_enable & 0x1);
		pdes_vpp[idx]->scl_int = ((cmd->scale_y_init & 0x3fff) << 16)
			| (cmd->scale_x_init & 0x3fff);

		pdes_vpp[idx]->scl_x = (unsigned int)(cmd->src_cropW * 16384 / cmd->dst_cropW);
		pdes_vpp[idx]->scl_y = (unsigned int)(cmd->src_cropH * 16384 / cmd->dst_cropH);

		if (cmd->mapcon_enable == 1) {
			u64 comp_base_y = 0, comp_base_c = 0, offset_base_y = 0,
				offset_base_c = 0, map_conv_off_base_y = 0;
			u64 map_conv_off_base_c = 0, map_comp_base_y = 0, map_comp_base_c = 0;
			u32 frm_h = 0, height = 0, map_pic_height_y = 0, map_pic_height_c = 0, frm_w = 0,
				comp_stride_y = 0, comp_stride_c = 0;

			if ((cmd->src_fd0 == 0) && (cmd->src_addr0 != 0)) {
				offset_base_y = cmd->src_addr0;
				comp_base_y = cmd->src_addr1;
				offset_base_c = cmd->src_addr2;
				comp_base_c = cmd->src_addr3;
			} else {
				pr_err("%s, line : %d, src addr0 is not physical addr\n", __func__, __LINE__);
			}

			frm_h = cmd->rows;
			height = ((frm_h + 15) / 16) * 4 * 16;
			map_conv_off_base_y = offset_base_y + (cmd->src_axisX / 256) * height * 2 +
				(cmd->src_axisY / 4) * 2 * 16;
			pdes_vpp[idx]->map_conv_off_base_y = (u32)(map_conv_off_base_y & 0xfffffff0);

			map_conv_off_base_c = offset_base_c + (cmd->src_axisX / 2 / 256) * height * 2 +
				(cmd->src_axisY / 2 / 2) * 2 * 16;
			pdes_vpp[idx]->map_conv_off_base_c = (u32)(map_conv_off_base_c & 0xfffffff0);

			pdes_vpp[idx]->src_ctrl =
				((1 & 0x1) << 12) | (0x0 << 20) | (0x0 << 16) | pdes_vpp[idx]->src_ctrl;//little endian
//((1 & 0x1) << 12) | (0xf << 20) | (0xf << 16) | pdes_vpp[idx]->src_ctrl;//big endian

			map_pic_height_y = ALIGN(frm_h, 16) & 0x3fff;
			map_pic_height_c = ALIGN(frm_h / 2, 8) & 0x3fff;
			pdes_vpp[idx]->map_conv_off_stride = (map_pic_height_y << 16) | map_pic_height_c;
			pdes_vpp[idx]->src_crop_st = ((cmd->src_axisX & 0x1fff) << 16) | (cmd->src_axisY & 0x1fff);
			pdes_vpp[idx]->src_crop_size = ((cmd->src_cropW & 0x3fff) << 16) | (cmd->src_cropH & 0x3fff);
			frm_w = cmd->cols;
			comp_stride_y = ALIGN(ALIGN(frm_w, 16) * 4, 32);
			map_comp_base_y = comp_base_y + (cmd->src_axisY / 4)*comp_stride_y;
			pdes_vpp[idx]->src_ry_base = (u32)(map_comp_base_y & 0xfffffff0);

			comp_stride_c = ALIGN(ALIGN(frm_w / 2, 16) * 4, 32);
			map_comp_base_c = comp_base_c + (cmd->src_axisY / 2 / 2) * comp_stride_c;
			pdes_vpp[idx]->src_gu_base = (u32)(map_comp_base_c & 0xfffffff0);
			pdes_vpp[idx]->src_stride = ((comp_stride_y & 0xfffffff0) << 16) | (comp_stride_c & 0xfffffff0);

			pdes_vpp[idx]->src_ry_base_ext =
				(u32)((((map_conv_off_base_y >> 32) & 0x7) << 8) | ((map_comp_base_y >> 32) & 0x7));
			pdes_vpp[idx]->src_gu_base_ext =
				(u32)((((map_conv_off_base_c >> 32) & 0x7) << 8) | ((map_comp_base_c >> 32) & 0x7));

		}
		//dump_des(batch, pdes, vpp_desc_pa);
		bmdev_memcpy_s2d_internal(bmdi, vpp_desc_pa[idx], (void *)pdes_vpp[idx], sizeof(struct vpp_descriptor));
		kfree(pdes_vpp[idx]);
	}

	return ret;
}

static int vpp_get_core_id(struct bm_device_info *bmdi, char *core)
{
	if (bmdi->vppdrvctx.vpp_idle_bit_map >= 3) {
		pr_err("[fatal err!]take sem, but two vpp core are busy, vpp_idle_bit_map = %ld\n", bmdi->vppdrvctx.vpp_idle_bit_map);
		return VPP_ERR_IDLE_BIT_MAP;
	}

	if (test_and_set_bit(0, &bmdi->vppdrvctx.vpp_idle_bit_map) == 0) {
		*core = 0;
	} else if (test_and_set_bit(1, &bmdi->vppdrvctx.vpp_idle_bit_map) == 0) {
		*core = 1;
	} else {
		pr_err("[fatal err!]Abnormal status, vpp_idle_bit_map = %ld\n", bmdi->vppdrvctx.vpp_idle_bit_map);
		return VPP_ERR_IDLE_BIT_MAP;
	}
	return VPP_OK;
}

static int vpp_free_core_id(struct bm_device_info *bmdi, char core_id)
{
	if ((core_id != 0) && (core_id != 1)) {
		pr_err("vpp abnormal status, vpp_idle_bit_map = %ld, core_id is %d\n", bmdi->vppdrvctx.vpp_idle_bit_map, core_id);
		return VPP_ERR_IDLE_BIT_MAP;
	}

	clear_bit(core_id, &bmdi->vppdrvctx.vpp_idle_bit_map);
	return VPP_OK;
}

__maybe_unused static void dump_des(struct vpp_batch *batch, struct vpp_descriptor **pdes_vpp, dma_addr_t *des_paddr)
{
	int idx = 0;

//	pr_info("bmdi->dev_index   is      %d\n", bmdi->dev_index);
	pr_info("batch->num   is      %d\n", batch->num);
	for (idx = 0; idx < batch->num; idx++) {
	pr_info("des_paddr[%d]   0x%llx\n", idx, des_paddr[idx]);
	pr_info("pdes_vpp[%d]->des_head   0x%x\n", idx, pdes_vpp[idx]->des_head);
	pr_info("pdes_vpp[%d]->next_cmd_base   0x%x\n", idx, pdes_vpp[idx]->next_cmd_base);
	pr_info("pdes_vpp[%d]->map_conv_off_base_y   0x%x\n", idx, pdes_vpp[idx]->map_conv_off_base_y);
	pr_info("pdes_vpp[%d]->map_conv_off_base_c   0x%x\n", idx, pdes_vpp[idx]->map_conv_off_base_c);
	pr_info("pdes_vpp[%d]->src_ctrl   0x%x\n", idx, pdes_vpp[idx]->src_ctrl);
	pr_info("pdes_vpp[%d]->map_conv_off_stride   0x%x\n", idx, pdes_vpp[idx]->map_conv_off_stride);
	pr_info("pdes_vpp[%d]->src_crop_st   0x%x\n", idx, pdes_vpp[idx]->src_crop_st);
	pr_info("pdes_vpp[%d]->src_crop_size   0x%x\n", idx, pdes_vpp[idx]->src_crop_size);
	pr_info("pdes_vpp[%d]->src_ry_base   0x%x\n", idx, pdes_vpp[idx]->src_ry_base);
	pr_info("pdes_vpp[%d]->src_gu_base   0x%x\n", idx, pdes_vpp[idx]->src_gu_base);
	pr_info("pdes_vpp[%d]->src_bv_base   0x%x\n", idx, pdes_vpp[idx]->src_bv_base);
	pr_info("pdes_vpp[%d]->src_stride   0x%x\n", idx, pdes_vpp[idx]->src_stride);
	pr_info("pdes_vpp[%d]->src_ry_base_ext   0x%x\n", idx, pdes_vpp[idx]->src_ry_base_ext);
	pr_info("pdes_vpp[%d]->src_gu_base_ext   0x%x\n", idx, pdes_vpp[idx]->src_gu_base_ext);
	pr_info("pdes_vpp[%d]->src_bv_base_ext   0x%x\n", idx, pdes_vpp[idx]->src_bv_base_ext);
	pr_info("pdes_vpp[%d]->dst_ctrl   0x%x\n", idx, pdes_vpp[idx]->dst_ctrl);
	pr_info("pdes_vpp[%d]->dst_crop_st   0x%x\n", idx, pdes_vpp[idx]->dst_crop_st);
	pr_info("pdes_vpp[%d]->dst_crop_size   0x%x\n", idx, pdes_vpp[idx]->dst_crop_size);
	pr_info("pdes_vpp[%d]->dst_ry_base   0x%x\n", idx, pdes_vpp[idx]->dst_ry_base);
	pr_info("pdes_vpp[%d]->dst_gu_base   0x%x\n", idx, pdes_vpp[idx]->dst_gu_base);
	pr_info("pdes_vpp[%d]->dst_bv_base   0x%x\n", idx, pdes_vpp[idx]->dst_bv_base);
	pr_info("pdes_vpp[%d]->dst_stride   0x%x\n", idx, pdes_vpp[idx]->dst_stride);
	pr_info("pdes_vpp[%d]->dst_ry_base_ext   0x%x\n", idx, pdes_vpp[idx]->dst_ry_base_ext);
	pr_info("pdes_vpp[%d]->dst_gu_base_ext   0x%x\n", idx, pdes_vpp[idx]->dst_gu_base_ext);
	pr_info("pdes_vpp[%d]->dst_bv_base_ext   0x%x\n", idx, pdes_vpp[idx]->dst_bv_base_ext);
	pr_info("pdes_vpp[%d]->scl_ctrl   0x%x\n", idx, pdes_vpp[idx]->scl_ctrl);
	pr_info("pdes_vpp[%d]->scl_int   0x%x\n", idx, pdes_vpp[idx]->scl_int);
	pr_info("pdes_vpp[%d]->scl_x   0x%x\n", idx, pdes_vpp[idx]->scl_x);
	pr_info("pdes_vpp[%d]->scl_y   0x%x\n", idx, pdes_vpp[idx]->scl_y);
	}
}

void vpp_dump(struct vpp_batch *batch)
{
	struct vpp_cmd *cmd ;
	int i;
	for (i = 0; i < batch->num; i++) {
		cmd = (batch->cmd + i);
		pr_info("batch->num    is  %d      \n", batch->num);
		pr_info("cmd id %d, cmd->src_format  0x%x\n", i, cmd->src_format);
		pr_info("cmd id %d, cmd->src_stride  0x%x\n", i, cmd->src_stride);
		pr_info("cmd id %d, cmd->src_uv_stride  0x%x\n", i, cmd->src_uv_stride);
		pr_info("cmd id %d, cmd->src_endian  0x%x\n", i, cmd->src_endian);
		pr_info("cmd id %d, cmd->src_endian_a  0x%x\n", i, cmd->src_endian_a);
		pr_info("cmd id %d, cmd->src_plannar  0x%x\n", i, cmd->src_plannar);
		pr_info("cmd id %d, cmd->src_fd0  0x%x\n", i, cmd->src_fd0);
		pr_info("cmd id %d, cmd->src_fd1  0x%x\n", i, cmd->src_fd1);
		pr_info("cmd id %d, cmd->src_fd2  0x%x\n", i, cmd->src_fd2);
		pr_info("cmd id %d, cmd->src_addr0  0x%lx\n", i, cmd->src_addr0);
		pr_info("cmd id %d, cmd->src_addr1  0x%lx\n", i, cmd->src_addr1);
		pr_info("cmd id %d, cmd->src_addr2  0x%lx\n", i, cmd->src_addr2);
		pr_info("cmd id %d, cmd->src_axisX  0x%x\n", i, cmd->src_axisX);
		pr_info("cmd id %d, cmd->src_axisY  0x%x\n", i, cmd->src_axisY);
		pr_info("cmd id %d, cmd->src_cropW  0x%x\n", i, cmd->src_cropW);
		pr_info("cmd id %d, cmd->src_cropH  0x%x\n", i, cmd->src_cropH);
		pr_info("cmd id %d, cmd->dst_format  0x%x\n", i, cmd->dst_format);
		pr_info("cmd id %d, cmd->dst_stride  0x%x\n", i, cmd->dst_stride);
		pr_info("cmd id %d, cmd->dst_uv_stride  0x%x\n", i, cmd->dst_uv_stride);
		pr_info("cmd id %d, cmd->dst_endian  0x%x\n", i, cmd->dst_endian);
		pr_info("cmd id %d, cmd->dst_endian_a  0x%x\n", i, cmd->dst_endian_a);
		pr_info("cmd id %d, cmd->dst_plannar  0x%x\n", i, cmd->dst_plannar);
		pr_info("cmd id %d, cmd->dst_fd0  0x%x\n", i, cmd->dst_fd0);
		pr_info("cmd id %d, cmd->dst_fd1  0x%x\n", i, cmd->dst_fd1);
		pr_info("cmd id %d, cmd->dst_fd2  0x%x\n", i, cmd->dst_fd2);
		pr_info("cmd id %d, cmd->dst_addr0  0x%lx\n", i, cmd->dst_addr0);
		pr_info("cmd id %d, cmd->dst_addr1  0x%lx\n", i, cmd->dst_addr1);
		pr_info("cmd id %d, cmd->dst_addr2  0x%lx\n", i, cmd->dst_addr2);
		pr_info("cmd id %d, cmd->dst_axisX  0x%x\n", i, cmd->dst_axisX);
		pr_info("cmd id %d, cmd->dst_axisY  0x%x\n", i, cmd->dst_axisY);
		pr_info("cmd id %d, cmd->dst_cropW  0x%x\n", i, cmd->dst_cropW);
		pr_info("cmd id %d, cmd->dst_cropH  0x%x\n", i, cmd->dst_cropH);
		pr_info("cmd id %d, cmd->src_csc_en  0x%x\n", i, cmd->src_csc_en);
		pr_info("cmd id %d, cmd->hor_filter_sel  0x%x\n", i, cmd->hor_filter_sel);
		pr_info("cmd id %d, cmd->ver_filter_sel  0x%x\n", i, cmd->ver_filter_sel);
		pr_info("cmd id %d, cmd->scale_x_init  0x%x\n", i, cmd->scale_x_init);
		pr_info("cmd id %d, cmd->scale_y_init  0x%x\n", i, cmd->scale_y_init);
		pr_info("cmd id %d, cmd->csc_type  0x%x\n", i, cmd->csc_type);
		pr_info("cmd id %d, cmd->mapcon_enable  0x%x\n", i, cmd->mapcon_enable);
		pr_info("cmd id %d, cmd->src_fd3  0x%x\n", i, cmd->src_fd3);
		pr_info("cmd id %d, cmd->src_addr3  0x%lx\n", i, cmd->src_addr3);
		pr_info("cmd id %d, cmd->cols  0x%x\n", i, cmd->cols);
		pr_info("cmd id %d, cmd->rows  0x%x\n", i, cmd->rows);
	}
    return;
}

int vpp_handle_setup(struct bm_device_info *bmdi, struct vpp_batch *batch)
{
	int ret = VPP_OK, ret1 = VPP_ERR, idx = 0;
	u64 *vpp_desc_pa;
	unsigned char core_id = 0xFF;


	struct reserved_mem_info *resmem_info = &bmdi->gmem_info.resmem_info;

	//pr_info("in vpp_handle_setup\n");
	if (down_interruptible(&bmdi->vppdrvctx.vpp_core_sem)) {
		pr_info("down_interruptible id interrupted, dev_index %d\n", bmdi->dev_index);
		return VPP_ERESTARTSYS;
	}
	ret = vpp_get_core_id(bmdi, &core_id);
	if (ret != VPP_OK) {
		up(&bmdi->vppdrvctx.vpp_core_sem);
		return VPP_ERR;
	}

	//pr_info("resmem_info->vpp_addr: 0x%llx", resmem_info->vpp_addr);
	vpp_desc_pa = (u64 *)kzalloc(VPP_CROP_NUM_MAX * sizeof(u64), GFP_KERNEL);
	if (vpp_desc_pa == NULL) {
		ret = VPP_ENOMEM;
		pr_info("vpp_desc_pa is NULL, dev_index  %d\n", bmdi->dev_index);
		return ret;
	}

	for (idx = 0; idx < batch->num; idx++)
		vpp_desc_pa[idx] = resmem_info->vpp_addr + core_id * (512 << 10) + ((1 << 10) * idx);

	if (core_id == 0) {
		ret = vpp_prepare_cmd_list(bmdi, batch, vpp_desc_pa);
		if (ret == VPP_OK) {
			vpp_setup_desc(core_id, bmdi, batch, vpp_desc_pa);
			ret1 = wait_event_timeout(bmdi->vppdrvctx.wq_vpp[0], bmdi->vppdrvctx.got_event_vpp[0], 10*HZ);
			if (ret1 == 0) {
				pr_info("ret  %d,core_id %d,current->pid %d,current->tgid %d,vpp_idle_bit_map %ld, dev_index  %d, vpp_desc_pa  %p\n",
					ret1, core_id, current->pid, current->tgid, bmdi->vppdrvctx.vpp_idle_bit_map, bmdi->dev_index, vpp_desc_pa);
				vpp_dump(batch);
				ret = VPP_ERR_INT_TIMEOUT;
		//		vpp_hardware_reset(core_id, vdev);
			}
			bmdi->vppdrvctx.got_event_vpp[0] = 0;
		}
	} else if (core_id == 1) {
		ret = vpp_prepare_cmd_list(bmdi, batch, vpp_desc_pa);
		if (ret == VPP_OK) {
			vpp_setup_desc(core_id, bmdi, batch, vpp_desc_pa);
			ret1 = wait_event_timeout(bmdi->vppdrvctx.wq_vpp[1], bmdi->vppdrvctx.got_event_vpp[1], 10*HZ);
			if (ret1 == 0) {
				pr_info("ret  %d,core_id %d,current->pid %d,current->tgid %d,vpp_idle_bit_map %ld, dev_index  %d, vpp_desc_pa  %p\n",
					ret1, core_id, current->pid, current->tgid, bmdi->vppdrvctx.vpp_idle_bit_map, bmdi->dev_index, vpp_desc_pa);
				vpp_dump(batch);
				ret = VPP_ERR_INT_TIMEOUT;
		//		vpp_hardware_reset(core_id, vdev);
			}
			bmdi->vppdrvctx.got_event_vpp[1] = 0;
		}
	} else {
		pr_err("vpp abnormal status, vpp_idle_bit_map = %ld, core_id is %d, dev_index  %d\n",
			bmdi->vppdrvctx.vpp_idle_bit_map, core_id, bmdi->dev_index);
		ret = VPP_ERR_IDLE_BIT_MAP;
	}

	kfree(vpp_desc_pa);
	ret |= vpp_free_core_id(bmdi, core_id);
	up(&bmdi->vppdrvctx.vpp_core_sem);

	if (signal_pending(current)) {
		ret |= VPP_ERESTARTSYS;
		pr_err("signal_pending ret=%d,current->pid %d,current->tgid %d,vpp_idle_bit_map %ld, dev_index %d\n",
			ret, current->pid, current->tgid, bmdi->vppdrvctx.vpp_idle_bit_map, bmdi->dev_index);
	}

	return ret;
}


int trigger_vpp(struct bm_device_info *bmdi, unsigned long arg)
{
	struct vpp_batch batch, batch_tmp;
	int ret = 0;

//	pr_info("[VPPDRV] start trigger_vpp\n");

	ret = copy_from_user(&batch_tmp, (void *)arg, sizeof(batch_tmp));
	if (ret != 0) {
		pr_err("trigger_vpp copy_from_user wrong,the number of bytes not copied %d, sizeof(batch_tmp) total need %lu, dev_index  %d\n",
			ret, sizeof(batch_tmp), bmdi->dev_index);
		ret = VPP_ERR_COPY_FROM_USER;
		return ret;
	}

	batch = batch_tmp;

	if ((batch.num <= 0) || (batch.num > VPP_CROP_NUM_MAX)) {
		pr_err("wrong num, batch.num  %d, dev_index  %d\n", batch.num, bmdi->dev_index);
		ret = VPP_ERR_WRONG_CROPNUM;
		return ret;
	}

	batch.cmd = kzalloc(batch.num * (sizeof(struct vpp_cmd)), GFP_KERNEL);
	if (batch.cmd == NULL) {
		ret = VPP_ENOMEM;
		pr_info("batch_cmd is NULL, dev_index  %d\n", bmdi->dev_index);
		return ret;
	}

	ret = copy_from_user(batch.cmd, ((void *)batch_tmp.cmd), (batch.num * sizeof(struct vpp_cmd)));
	if (ret != 0) {
		pr_err("trigger_vpp copy_from_user wrong,the number of bytes not copied %d,batch.num %d, single vpp_cmd %lu, total need %lu\n",
			ret, batch.num, sizeof(struct vpp_cmd), (batch.num * sizeof(struct vpp_cmd)));
		kfree(batch.cmd);
		ret = VPP_ERR_COPY_FROM_USER;
		return ret;
	}

	ret = vpp_handle_setup(bmdi, &batch);
	if ((ret != VPP_OK) && (ret != VPP_ERESTARTSYS))
		pr_err("trigger_vpp ,vpp_handle_setup wrong, ret %d, line  %d, dev_index  %d\n", ret, __LINE__, bmdi->dev_index);

	kfree(batch.cmd);
	return ret;
}

int vpp_init(struct bm_device_info *bmdi)
{
	int i;
	sema_init(&bmdi->vppdrvctx.vpp_core_sem, VPP_CORE_MAX);
	bmdi->vppdrvctx.vpp_idle_bit_map = 0;
	for (i = 0; i < VPP_CORE_MAX; i++)
		init_waitqueue_head(&bmdi->vppdrvctx.wq_vpp[i]);
	return 0;
}

void vpp_exit(struct bm_device_info *bmdi)
{
}

static void bmdrv_vpp0_irq_handler(struct bm_device_info *bmdi)
{
	vpp_clear_int(bmdi, 0);
	vpp0_irq_handler(bmdi);
}

static void bmdrv_vpp1_irq_handler(struct bm_device_info *bmdi)
{
	vpp_clear_int(bmdi, 1);
	vpp1_irq_handler(bmdi);
}

void bm_vpp_request_irq(struct bm_device_info *bmdi)
{
	bmdrv_submodule_request_irq(bmdi, VPP0_IRQ_ID, bmdrv_vpp0_irq_handler);
	bmdrv_submodule_request_irq(bmdi, VPP1_IRQ_ID, bmdrv_vpp1_irq_handler);
}

void bm_vpp_free_irq(struct bm_device_info *bmdi)
{
	bmdrv_submodule_free_irq(bmdi, VPP0_IRQ_ID);
	bmdrv_submodule_free_irq(bmdi, VPP1_IRQ_ID);
}

