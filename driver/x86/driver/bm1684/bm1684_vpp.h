#ifndef _VPP_K_H_
#define _VPP_K_H_
//#include "../bm_common.h"

/*vpp global control*/
#define VPP_VERSION      (0x000)
#define VPP_CONTROL0 (0x004)
#define VPP_CONTROL1       (0x008)
#define VPP_STATUS  (0x00C)
#define VPP_INT_EN    (0x010)
#define VPP_INT_CLEAR  (0x014)
#define VPP_INT_STATUS    (0x018)
#define VPP_INT_RAW_STATUS    (0x01c)
#define VPP_CMD_BASE    (0x020)
#define VPP_CMD_BASE_EXT    (0x024)

/*Tile GDI2AXI control*/
#define VPP_GDI_XY2_CAS_0    (0x030)
#define VPP_GDI_XY2_CAS_1    (0x034)
#define VPP_GDI_XY2_CAS_2    (0x038)
#define VPP_GDI_XY2_CAS_3    (0x03c)
#define VPP_GDI_XY2_CAS_4    (0x040)
#define VPP_GDI_XY2_CAS_5    (0x044)
#define VPP_GDI_XY2_CAS_6    (0x048)
#define VPP_GDI_XY2_CAS_7    (0x04c)
#define VPP_GDI_XY2_CAS_8    (0x050)
#define VPP_GDI_XY2_CAS_9    (0x054)
#define VPP_GDI_XY2_CAS_10    (0x058)
#define VPP_GDI_XY2_CAS_11    (0x05c)
#define VPP_GDI_XY2_CAS_12    (0x060)
#define VPP_GDI_XY2_CAS_13    (0x064)
#define VPP_GDI_XY2_CAS_14    (0x068)
#define VPP_GDI_XY2_CAS_15    (0x06c)
#define VPP_GDI_XY2_BA_0    (0x070)
#define VPP_GDI_XY2_BA_1    (0x074)
#define VPP_GDI_XY2_BA_2    (0x078)
#define VPP_GDI_XY2_BA_3    (0x07c)
#define VPP_GDI_XY2_RAS_0    (0x080)
#define VPP_GDI_XY2_RAS_1    (0x084)
#define VPP_GDI_XY2_RAS_2    (0x088)
#define VPP_GDI_XY2_RAS_3    (0x08c)
#define VPP_GDI_XY2_RAS_4    (0x090)
#define VPP_GDI_XY2_RAS_5    (0x094)
#define VPP_GDI_XY2_RAS_6    (0x098)
#define VPP_GDI_XY2_RAS_7    (0x09c)
#define VPP_GDI_XY2_RAS_8    (0x0a0)
#define VPP_GDI_XY2_RAS_9    (0x0a4)
#define VPP_GDI_XY2_RAS_10    (0x0a8)
#define VPP_GDI_XY2_RAS_11    (0x0ac)
#define VPP_GDI_XY2_RAS_12    (0x0b0)
#define VPP_GDI_XY2_RAS_13    (0x0b4)
#define VPP_GDI_XY2_RAS_14    (0x0b8)
#define VPP_GDI_XY2_RAS_15    (0x0bc)
#define VPP_GDI_RBC2_AXI_0    (0x0c0)
#define VPP_GDI_RBC2_AXI_1    (0x0c4)
#define VPP_GDI_RBC2_AXI_2    (0x0c8)
#define VPP_GDI_RBC2_AXI_3    (0x0cc)
#define VPP_GDI_RBC2_AXI_4    (0x0d0)
#define VPP_GDI_RBC2_AXI_5    (0x0d4)
#define VPP_GDI_RBC2_AXI_6    (0x0d8)
#define VPP_GDI_RBC2_AXI_7    (0x0dc)
#define VPP_GDI_RBC2_AXI_8    (0x0e0)
#define VPP_GDI_RBC2_AXI_9    (0x0e4)
#define VPP_GDI_RBC2_AXI_10    (0x0e8)
#define VPP_GDI_RBC2_AXI_11    (0x0ec)
#define VPP_GDI_RBC2_AXI_12    (0x0f0)
#define VPP_GDI_RBC2_AXI_13    (0x0f4)
#define VPP_GDI_RBC2_AXI_14    (0x0f8)
#define VPP_GDI_RBC2_AXI_15    (0x0fc)
#define VPP_GDI_RBC2_AXI_16    (0x0100)
#define VPP_GDI_RBC2_AXI_17    (0x0104)
#define VPP_GDI_RBC2_AXI_18    (0x0108)
#define VPP_GDI_RBC2_AXI_19    (0x010c)
#define VPP_GDI_RBC2_AXI_20    (0x0110)
#define VPP_GDI_RBC2_AXI_21    (0x0114)
#define VPP_GDI_RBC2_AXI_22    (0x0118)
#define VPP_GDI_RBC2_AXI_23    (0x011c)
#define VPP_GDI_RBC2_AXI_24    (0x0120)
#define VPP_GDI_RBC2_AXI_25    (0x0124)
#define VPP_GDI_RBC2_AXI_26    (0x0128)
#define VPP_GDI_RBC2_AXI_27    (0x012c)
#define VPP_GDI_RBC2_AXI_28    (0x0130)
#define VPP_GDI_RBC2_AXI_29    (0x0134)
#define VPP_GDI_RBC2_AXI_30    (0x0138)
#define VPP_GDI_RBC2_AXI_31    (0x013c)
#define VPP_GDI_XY2_RBC_CONFIG    (0x0140)
#define VPP_GDI_TILEDBUF_BASE    (0x0144)

/*vpp descriptor for DMA CMD LIST*/
#define VPP_DES_HEAD    (0x0300)
#define VPP_NEXT_CMD_BASE    (0x0304)
#define VPP_NEXT_CMD_BASE_EXT    (0x0308)
#define VPP_PADDING_CTRL   (0x030c)


#define VPP_CSC_COE_00   (0x0310)
#define VPP_CSC_COE_01   (0x0314)
#define VPP_CSC_COE_02   (0x0318)
#define VPP_CSC_ADD_0     (0x031c)
#define VPP_CSC_COE_10   (0x0320)
#define VPP_CSC_COE_11   (0x0324)
#define VPP_CSC_COE_12   (0x0328)
#define VPP_CSC_ADD_1     (0x032c)
#define VPP_CSC_COE_20   (0x0330)
#define VPP_CSC_COE_21   (0x0334)
#define VPP_CSC_COE_22   (0x0338)
#define VPP_CSC_ADD_2     (0x033c)


#define VPP_SRC_CTRL           (0x0340)
#define VPP_SRC_SIZE           (0x0344)
#define VPP_SRC_CROP_ST        (0x0348)
#define VPP_SRC_CROP_SIZE      (0x034c)
#define VPP_SRC_RY_BASE        (0x0350)
#define VPP_SRC_GU_BASE        (0x0354)
#define VPP_SRC_BV_BASE        (0x0358)
#define VPP_SRC_STRIDE         (0x035c)
#define VPP_SRC_RY_BASE_EXT    (0x0360)
#define VPP_SRC_GU_BASE_EXT    (0x0364)
#define VPP_SRC_BV_BASE_EXT    (0x0368)


#define VPP_DST_CTRL              (0x0370)
#define VPP_DST_SIZE              (0x0374)
#define VPP_DST_CROP_ST           (0x0378)
#define VPP_DST_CROP_SIZE         (0x037c)
#define VPP_DST_RY_BASE           (0x0380)
#define VPP_DST_GU_BASE           (0x0384)
#define VPP_DST_BV_BASE           (0x0388)
#define VPP_DST_STRIDE            (0x038c)
#define VPP_DST_RY_BASE_EXT       (0x0390)
#define VPP_DST_GU_BASE_EXT       (0x0394)
#define VPP_DST_BV_BASE_EXT       (0x0398)
#define VPP_DST_PADDING_HEIGHT    (0x039c)

#define VPP_SCL_CTRL   (0x03a0)
#define VPP_SCL_INIT   (0x03a4)
#define VPP_SCL_X   (0x03a8)
#define VPP_SCL_Y   (0x03ac)

#define VPP_SCL_Y_COE_P0C0   (0x03b0)
#define VPP_SCL_Y_COE_P0C2   (0x03b4)
#define VPP_SCL_Y_COE_P1C0   (0x03b8)
#define VPP_SCL_Y_COE_P1C2   (0x03bc)
#define VPP_SCL_Y_COE_P2C0   (0x03c0)
#define VPP_SCL_Y_COE_P2C2   (0x03c4)
#define VPP_SCL_Y_COE_P3C0   (0x03c8)
#define VPP_SCL_Y_COE_P3C2   (0x03cc)
#define VPP_SCL_Y_COE_P4C0   (0x03d0)
#define VPP_SCL_Y_COE_P4C2   (0x03d4)
#define VPP_SCL_Y_COE_P5C0   (0x03d8)
#define VPP_SCL_Y_COE_P5C2   (0x03dc)
#define VPP_SCL_Y_COE_P6C0   (0x03e0)
#define VPP_SCL_Y_COE_P6C2   (0x03e4)
#define VPP_SCL_Y_COE_P7C0   (0x03e8)
#define VPP_SCL_Y_COE_P7C2   (0x03ec)
#define VPP_SCL_Y_COE_P8C0   (0x03f0)
#define VPP_SCL_Y_COE_P8C2   (0x03f4)
#define VPP_SCL_Y_COE_P9C0   (0x03f8)
#define VPP_SCL_Y_COE_P9C2   (0x03fc)
#define VPP_SCL_Y_COE_PAC0   (0x0400)
#define VPP_SCL_Y_COE_PAC2   (0x0404)
#define VPP_SCL_Y_COE_PBC0   (0x0408)
#define VPP_SCL_Y_COE_PBC2   (0x040c)
#define VPP_SCL_Y_COE_PCC0   (0x0410)
#define VPP_SCL_Y_COE_PCC2   (0x0414)
#define VPP_SCL_Y_COE_PDC0   (0x0418)
#define VPP_SCL_Y_COE_PDC2   (0x041c)
#define VPP_SCL_Y_COE_PEC0   (0x0420)
#define VPP_SCL_Y_COE_PEC2   (0x0424)
#define VPP_SCL_Y_COE_PFC0   (0x0428)
#define VPP_SCL_Y_COE_PFC2   (0x042c)

#define VPP_SCL_X_COE_P0C0   (0x0430)
#define VPP_SCL_X_COE_P0C2   (0x0434)
#define VPP_SCL_X_COE_P0C4   (0x0438)
#define VPP_SCL_X_COE_P0C6   (0x043c)
#define VPP_SCL_X_COE_P1C0   (0x0440)
#define VPP_SCL_X_COE_P1C2   (0x0444)
#define VPP_SCL_X_COE_P1C4   (0x0448)
#define VPP_SCL_X_COE_P1C6   (0x044c)
#define VPP_SCL_X_COE_P2C0   (0x0450)
#define VPP_SCL_X_COE_P2C2   (0x0454)
#define VPP_SCL_X_COE_P2C4   (0x0458)
#define VPP_SCL_X_COE_P2C6   (0x045c)
#define VPP_SCL_X_COE_P3C0   (0x0460)
#define VPP_SCL_X_COE_P3C2   (0x0464)
#define VPP_SCL_X_COE_P3C4   (0x0468)
#define VPP_SCL_X_COE_P3C6   (0x046c)
#define VPP_SCL_X_COE_P4C0   (0x0470)
#define VPP_SCL_X_COE_P4C2   (0x0474)
#define VPP_SCL_X_COE_P4C4   (0x0478)
#define VPP_SCL_X_COE_P4C6   (0x047c)
#define VPP_SCL_X_COE_P5C0   (0x0480)
#define VPP_SCL_X_COE_P5C2   (0x0484)
#define VPP_SCL_X_COE_P5C4   (0x0488)
#define VPP_SCL_X_COE_P5C6   (0x048c)
#define VPP_SCL_X_COE_P6C0   (0x0490)
#define VPP_SCL_X_COE_P6C2   (0x0494)
#define VPP_SCL_X_COE_P6C4   (0x0498)
#define VPP_SCL_X_COE_P6C6   (0x049c)
#define VPP_SCL_X_COE_P7C0   (0x04a0)
#define VPP_SCL_X_COE_P7C2   (0x04a4)
#define VPP_SCL_X_COE_P7C4   (0x04a8)
#define VPP_SCL_X_COE_P7C6   (0x04ac)
#define VPP_SCL_X_COE_P8C0   (0x04b0)
#define VPP_SCL_X_COE_P8C2   (0x04b4)
#define VPP_SCL_X_COE_P8C4   (0x04b8)
#define VPP_SCL_X_COE_P8C6   (0x04bc)
#define VPP_SCL_X_COE_P9C0   (0x04c0)
#define VPP_SCL_X_COE_P9C2   (0x04c4)
#define VPP_SCL_X_COE_P9C4   (0x04c8)
#define VPP_SCL_X_COE_P9C6   (0x04cc)
#define VPP_SCL_X_COE_PAC0   (0x04d0)
#define VPP_SCL_X_COE_PAC2   (0x04d4)
#define VPP_SCL_X_COE_PAC4   (0x04d8)
#define VPP_SCL_X_COE_PAC6   (0x04dc)
#define VPP_SCL_X_COE_PBC0   (0x04e0)
#define VPP_SCL_X_COE_PBC2   (0x04e4)
#define VPP_SCL_X_COE_PBC4   (0x04e8)
#define VPP_SCL_X_COE_PBC6   (0x04ec)
#define VPP_SCL_X_COE_PCC0   (0x04f0)
#define VPP_SCL_X_COE_PCC2   (0x04f4)
#define VPP_SCL_X_COE_PCC4   (0x04f8)
#define VPP_SCL_X_COE_PCC6   (0x04fc)
#define VPP_SCL_X_COE_PDC0   (0x0500)
#define VPP_SCL_X_COE_PDC2   (0x0504)
#define VPP_SCL_X_COE_PDC4   (0x0508)
#define VPP_SCL_X_COE_PDC6   (0x050c)
#define VPP_SCL_X_COE_PEC0   (0x0510)
#define VPP_SCL_X_COE_PEC2   (0x0514)
#define VPP_SCL_X_COE_PEC4   (0x0518)
#define VPP_SCL_X_COE_PEC6   (0x051c)
#define VPP_SCL_X_COE_PFC0   (0x0520)
#define VPP_SCL_X_COE_PFC2   (0x0524)
#define VPP_SCL_X_COE_PFC4   (0x0528)
#define VPP_SCL_X_COE_PFC6   (0x052c)
//color space bm1684 support
#define YUV420        0
#define YOnly         1
#define RGB24         2
#define ARGB32        3
#define YUV422        4

#define VPP_CROP_NUM_MAX (256)
#define VPP_CORE_MAX (2)

typedef struct vpp_drv_context {
	struct semaphore vpp_core_sem;
	unsigned long vpp_idle_bit_map;
	wait_queue_head_t  wq_vpp[VPP_CORE_MAX];
	int got_event_vpp[VPP_CORE_MAX];

} vpp_drv_context_t;

struct vpp_descriptor {
	unsigned int des_head;
	unsigned int next_cmd_base;
	unsigned int map_conv_off_base_y;
	unsigned int map_conv_off_base_c;
	unsigned int csc_coe00;
	unsigned int csc_coe01;
	unsigned int csc_coe02;
	unsigned int csc_add0;
	unsigned int csc_coe10;
	unsigned int csc_coe11;
	unsigned int csc_coe12;
	unsigned int csc_add1;
	unsigned int csc_coe20;
	unsigned int csc_coe21;
	unsigned int csc_coe22;
	unsigned int csc_add2;
	unsigned int src_ctrl;
	unsigned int map_conv_off_stride;
	unsigned int src_crop_st;
	unsigned int src_crop_size;
	unsigned int src_ry_base;
	unsigned int src_gu_base;
	unsigned int src_bv_base;
	unsigned int src_stride;
	unsigned int src_ry_base_ext;
	unsigned int src_gu_base_ext;
	unsigned int src_bv_base_ext;
	unsigned int rsv;
	unsigned int dst_ctrl;
	unsigned int dst_size;
	unsigned int dst_crop_st;
	unsigned int dst_crop_size;
	unsigned int dst_ry_base;
	unsigned int dst_gu_base;
	unsigned int dst_bv_base;
	unsigned int dst_stride;
	unsigned int dst_ry_base_ext;
	unsigned int dst_gu_base_ext;
	unsigned int dst_bv_base_ext;
	unsigned int dst_padding_height;
	unsigned int scl_ctrl;
	unsigned int scl_int;
	unsigned int scl_x;
	unsigned int scl_y;
};

static const unsigned int GDI_XY2[3][16] = {
{ 0x1010, 0x1111, 0x1212, 0x1313,
	0x0303, 0x0404, 0x0505, 0x0606,
	0x0707, 0x1415, 0x4040, 0x4040,
	0x4040, 0x4040, 0x4040, 0x4040 },
{ 0x0888, 0x0989, 0x1594, 0x4040,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000 },
{ 0x0a0a, 0x1616, 0x1717, 0x1818,
	0x1919, 0x1a1a, 0x1b1b, 0x1c1c,
	0x1d1d, 0x1e1e, 0x1f1f, 0x4040,
	0x4040, 0x4040, 0x4040, 0x4040 } };

static const unsigned int GDI_RBC2_AXI[32] = {
	0x0c30, 0x0c30, 0x0c30, 0x0000,
	0x0041, 0x0082, 0x00c3, 0x0104,
	0x0145, 0x0186, 0x01c7, 0x0208,
	0x0249, 0x0410, 0x0451, 0x0492,
	0x0820, 0x0861, 0x08a2, 0x08e3,
	0x0924, 0x0965, 0x09a6, 0x09e7,
	0x0a28, 0x0a69, 0x0aaa, 0x0aeb,
	0x0b2c, 0x0b6d, 0x0bae, 0x0bef };

struct csc_matrix {
	int csc_coe00;
	int csc_coe01;
	int csc_coe02;
	int csc_add0;
	int csc_coe10;
	int csc_coe11;
	int csc_coe12;
	int csc_add1;
	int csc_coe20;
	int csc_coe21;
	int csc_coe22;
	int csc_add2;
};

struct vpp_cmd {
	int src_format;
	int src_stride;
	int src_endian;
	int src_endian_a;
	int src_plannar;
	int src_fd0;
	int src_fd1;
	int src_fd2;
	unsigned long src_addr0;
	unsigned long src_addr1;
	unsigned long src_addr2;
	unsigned short src_axisX;
	unsigned short src_axisY;
	unsigned short src_cropW;
	unsigned short src_cropH;

	int dst_format;
	int dst_stride;
	int dst_endian;
	int dst_endian_a;
	int dst_plannar;
	int dst_fd0;
	int dst_fd1;
	int dst_fd2;
	unsigned long dst_addr0;
	unsigned long dst_addr1;
	unsigned long dst_addr2;
	unsigned short dst_axisX;
	unsigned short dst_axisY;
	unsigned short dst_cropW;
	unsigned short dst_cropH;

	int src_csc_en;
	int hor_filter_sel;
	int ver_filter_sel;
	int scale_x_init;
	int scale_y_init;
	int csc_type;

	int mapcon_enable;
	int src_fd3;
	unsigned long src_addr3;
	int cols;
	int rows;
	int src_uv_stride;
	int dst_uv_stride;
	struct csc_matrix matrix;
};

struct vpp_batch {
	int num;
	struct vpp_cmd *cmd;
};

struct vpp_batch_stack {
	int num;
	struct vpp_cmd  cmd[VPP_CROP_NUM_MAX];
};

enum _csc_coe_type {
	YCbCr2RGB_BT601  = 0,
	YPbPr2RGB_BT601  = 1,
	RGB2YCbCr_BT601  = 2,
	YCbCr2RGB_BT709  = 3,
	RGB2YCbCr_BT709  = 4,
	RGB2YPbPr_BT601  = 5,
	YPbPr2RGB_BT709  = 6,
	RGB2YPbPr_BT709  = 7,
	CSC_MAX,
	CSC_USER_DEFINED
};
struct bm_vpp_buf {
	struct device *dev;
	/* dmabuf related */
	int dma_fd;
	dma_addr_t dma_addr;
	enum dma_data_direction dma_dir;
	struct dma_buf *dma_buf;
	struct dma_buf_attachment *dma_attach;
	struct sg_table *dma_sgt;
};

int trigger_vpp(struct bm_device_info *bmdi, unsigned long arg);
void bm_vpp_request_irq(struct bm_device_info *bmdi);
void bm_vpp_free_irq(struct bm_device_info *bmdi);
int vpp_init(struct bm_device_info *bmdi);
void vpp_exit(struct bm_device_info *bmdi);
extern int bmdev_memcpy_d2s(struct bm_device_info *bmdi,struct file *file,
		void __user *dst, u64 src, u32 size,
		bool intr, bm_cdma_iommu_mode cdma_iommu_mode);
#endif
