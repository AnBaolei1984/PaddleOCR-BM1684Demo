#include <linux/delay.h>
#include "bm_common.h"
#include "bm1682_ddr.h"

#define UART0_CLK_IN_HZ		25000000

static inline int ddr_poll(struct bm_device_info *bmdi, u32 ddr_ctl, u32 reg_offset, u32 exp_data, u32 msk)
{
	u32 tmp_val;
	int flag = 1;

	while (flag) {
		tmp_val = ddr_reg_read_enh(bmdi, ddr_ctl, reg_offset);
		if ((tmp_val & ~msk) == (exp_data & ~msk))
			flag = 0;
	}
	return flag;
}
static inline int ddr_check(struct bm_device_info *bmdi, u32 ddr_ctl, u32 reg_offset, u32 exp_data, u32 msk)
{
	u32 tmp_val;

	tmp_val = ddr_reg_read_enh(bmdi, ddr_ctl, reg_offset);
	if ((tmp_val & ~msk) == (exp_data & ~msk))
		return 0;
	else
		return 1;
}

#ifdef HIGH_DENSITY_SERVER //dual chip or high density server board
  static struct ddr_timing {
    u32 wdqs;
    u32 dqs_coarse;
    u32 dqsl_fine;
    u32 dqsh_fine;
  } ddr_para[2][4] = {
    {{0x00001000, 0x0e010005, 0xdfb63832, 0xa082513b},   //1600 channel_0
     {0x00001000, 0x68010045, 0x2af2a098, 0xd5ae6c4f},   //2133
     {0x00001000, 0x95010055, 0x4f11d4c8, 0xefc47a59},   //2400
     {0x00001000, 0xc3014055, 0xa0d49591, 0x6fabd080}},  //2666
    {{0x00001000, 0xbc000000, 0x77729895, 0xd2cbfbfd},   //1600 channel_1
     {0x00001000, 0xfa005500, 0x9e98cbc7, 0x180f4e51},   //2133
     {0x00001000, 0x1a015500, 0xb2abe4e0, 0x3b31787b},   //2400
     {0x00001000, 0x39015500, 0xc6befef9, 0x5f53a2a5}}}; //2666
#else //single chip board
  static struct ddr_timing {
    u32 wdqs;
    u32 dqs_coarse;
    u32 dqsl_fine;
    u32 dqsh_fine;
  } ddr_para[2][4] = {
    {{0x00001000, 0xf9000005, 0xd8c21516, 0x946e3e23},   //1600 channel_0
     {0x00001000, 0x4d010055, 0x20037173, 0xc692522e},   //2133
     {0x00001000, 0x86010055, 0x5230b0b3, 0xe6ab6136},   //2400
     {0x00001000, 0xa0014055, 0xa0d49591, 0x6fabd080}},  //2666
    {{0x00001000, 0xa4000000, 0x4b497771, 0xc4b3e4e5},   //1600 channel_1
     {0x00001000, 0xda004500, 0x64629f97, 0x06ef3031},   //2133
     {0x00001000, 0xff005500, 0x7373bbb1, 0x33196666},   //2400
     {0x00001000, 0x12015500, 0x7d7ac7bd, 0x80f0f080}}}; //2666
#endif

static u8 calc_ddr_para_id(u32 freq)
{
	u8 id = (freq - 1600) / 250;
	if (id == 0)
		return 0;
	return (id - 1);
}

void bm1682_disable_ddr_interleave(struct bm_device_info *bmdi)
{
 	u32 data = top_reg_read(bmdi, 0x1c0);
	top_reg_write(bmdi, 0x1c0, (data & (~(1<<4))) | 0x2);
}

void bm1682_enable_ddr_interleave(struct bm_device_info *bmdi)
{
	u32 data = top_reg_read(bmdi, 0x1c0);
	top_reg_write(bmdi, 0x1c0, (data | (1<<4)) | 0x2);
}

void disable_local_mem_early_resp(struct bm_device_info *bmdi)
{
   //Disable the early write for easier simulation
	u32 data = top_reg_read(bmdi, 0x8);
	top_reg_write(bmdi, 0x8, data | (1<<9));
}

void enable_local_mem_early_resp(struct bm_device_info *bmdi)
{
   //enable the early write for perfomance
	u32 data = top_reg_read(bmdi, 0x8);
	top_reg_write(bmdi, 0x8, data & (~(1<<9)));
}

void set_ddr_interleave_size(struct bm_device_info *bmdi, u32 size)
{
  /*
  int i;
  int bit_sum = 0;
  for(i = 0; i < 8; i++){
    bit_sum += (size >> i) & 0x1;
  }
  */
	u32 data = top_reg_read(bmdi, 0x8);
	top_reg_write(bmdi, 0x8, (data & 0xffffff00) | size);
}

static void get_ddr_freq(struct bm_device_info *bmdi, u32 *ddr_freq)
{
	// fix me later
	if (CONFIG_DDR_CLK) {
		*ddr_freq = CONFIG_DDR_CLK;
	}
}

static void ddr4_init(struct bm_device_info *bmdi, u32 ddr_ctl)
{
	u32 rdata;
	int i;

	ddr_reg_write_enh(bmdi, ddr_ctl, 0x7fc, 0x60000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x100, 0x022a0092);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x104, 0x00002002);   //change bit[6]=0 reg_data_exchange_mode=0
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x108, 0x1a7a0063);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x10c, 0x1a7a0063);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x110, 0x1a7a0063);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x114, 0x1a7a0063);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x118, 0x20000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x11c, 0x20000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x120, 0x20000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x124, 0x20000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x128, 0x00000008);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x12c, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x130, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x134, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x138, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x13c, 0x20000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x140, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x144, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x148, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x14c, 0x00003080);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x150, 0xf400f000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x154, 0x00000020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x158, 0x07700001);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x15c, 0x04000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x160, 0x00000400);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x164, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x168, 0x000c0004);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x16c, 0x00bb0200);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x170, 0x000c0006);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x174, 0x000f0000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x178, 0x0012c10c);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x17c, 0x06080606);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x180, 0x40001100);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x184, 0x0c400a28);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x188, 0x00001000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x18c, 0x05701016);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x190, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x19c, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1a0, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1a4, 0x00002020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1a8, 0x80002020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1ac, 0x00000001);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1cc, 0x01010101);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1d0, 0x01010101);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1d4, 0x08080808);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1d8, 0x08080808);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x820, 0x80800180);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x688, 0x80808080);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x68c, 0x80808080);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x690, 0x80808080);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x694, 0x80808080);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6f8, 0x90909090);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6fc, 0x90909090);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x800, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x824, 0x00000090);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x194, 0x0111ffe0);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x150, 0x05000080);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x198, 0x08060000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1b0, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1b4, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1b8, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1bc, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1c0, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1c4, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1c8, 0x000aff2c);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1dc, 0x00080000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1e0, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1e4, 0xaa55aa55);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1e8, 0x55aa55aa);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1ec, 0xaaaa5555);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1f0, 0x5555aaaa);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1f4, 0xaa55aa55);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1f8, 0x55aa55aa);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1fc, 0xaaaa5555);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x600, 0x5555aaaa);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x604, 0xaa55aa55);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x608, 0x55aa55aa);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x60c, 0xaaaa5555);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x610, 0x5555aaaa);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x614, 0xaa55aa55);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x618, 0x55aa55aa);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x61c, 0xaaaa5555);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x620, 0x5555aaaa);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x624, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x628, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x62c, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x630, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x634, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x638, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x63c, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x640, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x644, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x648, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x64c, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x650, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x654, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x658, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x65c, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x660, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x664, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x668, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x66c, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x670, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x698, 0x3f3f0800);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x69c, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6a0, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6a4, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6a8, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6ac, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6b0, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6b4, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6b8, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6bc, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6c0, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6c4, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6c8, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6cc, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6d0, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6d4, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6d8, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6dc, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6e0, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6e4, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6e8, 0x00003f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x804, 0x00000800);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x828, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x82c, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x830, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x834, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x838, 0x3f3f3f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x83c, 0x00003f3f);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x000, 0x00113001);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x004, 0x0008001e);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x008, 0x0000144e);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x00c, 0x00010c11);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x030, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x040, 0x741c180b);  //cpu_write32(offset + 0x50000040, 0x751c180b) ->  cpu_write32(offset + 0x50000040, 0x741c180b)
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x044, 0x0a01d328);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x048, 0x00142b14);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x04c, 0x060e0926);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x050, 0x4000e30e);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x054, 0x00000020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x05c, 0x01440000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x060, 0x00000807);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x064, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x068, 0x00001400);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x090, 0x12839483);  //col addr convt change
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x094, 0x3ff6b16a);  //col addr convt change
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x098, 0x144d2450);  //row addr convt
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x09c, 0x19617595);  //row addr convt
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x0a0, 0x1e75c6da);  //row addr convt

	//ddr_reg_write_enh(bmdi, ddr_ctl, 0a4,0x0003ffff);  //row addr convt
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x0a4, (0x3f << 12)|(0x3f<<6)|(0x1f)) ;  //row addr convt
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x0a8, 0x0003f3ce);  //bank addr convt change bk[1:0] = addr[15:14]
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x0ac, 0x00ffffc6);  //bg/rank addr convt addr[6] = bg
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x7fc,0x60000001);
	//This can not work on palladium
	//polling(offset + 0x500002c0,0x80000001,0xfffffffe);

	ddr_check(bmdi, ddr_ctl, 0x130, 0x0, 0x0);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x130, 0x20000000);
	//This can not work on palladium
	//polling(offset + 0x500002c0,0x80000001,0xfffffffe);

	ddr_reg_write_enh(bmdi, ddr_ctl, 0x130, 0x00000000);

	ddr_poll(bmdi, ddr_ctl, 0x004, 0x80000000, 0x7fffffff);
	ddr_poll(bmdi, ddr_ctl, 0x004, 0xc008001e, 0x0);//read_chk
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x004, 0xc0080016);
	//[ddrc_tb.u_cfg_model][65510 ns] Release FORCE_RESETN_LOW.
	ddr_poll(bmdi, ddr_ctl, 0x004, 0xc0080016, 0x0);//read_chk
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x004, 0xc0080012);
	for (i=0; i<=20; i++) {
		ddr_check(bmdi, ddr_ctl, 0x130, 0x0, 0x0);
	}

	ddr_reg_write_enh(bmdi, ddr_ctl, 0x058, 0x00000032);
	ddr_poll(bmdi, ddr_ctl, 0x058, 0x00000000, 0xfffffffd);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x058, 0x00c00062);
	ddr_poll(bmdi, ddr_ctl, 0x058,0x00000000, 0xfffffffd);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x058, 0x00400052);
	ddr_poll(bmdi, ddr_ctl, 0x058, 0x00000000, 0xfffffffd);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x058, 0x00000042);
	ddr_poll(bmdi, ddr_ctl, 0x058, 0x00000000, 0xfffffffd);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x058, 0x00020022);
	ddr_poll(bmdi, ddr_ctl, 0x058, 0x00000000, 0xfffffffd);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x058, 0x00001012);
	ddr_poll(bmdi, ddr_ctl, 0x058, 0x00000000, 0xfffffffd);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x058, 0x00b70002);
	ddr_poll(bmdi, ddr_ctl, 0x058, 0x00000000, 0xfffffffd);
	ddr_poll(bmdi, ddr_ctl, 0x068, 0x00001400, 0x0);//read_chk
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x068, 0x00003400);
	ddr_poll(bmdi, ddr_ctl, 0x068, 0x00000000, 0xffffdfff);
	ddr_poll(bmdi, ddr_ctl, 0x004, 0xc0080012, 0x0);//read_chk
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x004, 0xc0080012);
	ddr_poll(bmdi, ddr_ctl, 0x004, 0xc0080012, 0x0);//read_chk
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x004, 0xc0080010);
	rdata = ddr_reg_read_enh(bmdi, ddr_ctl, 0x4);

	if ((rdata & 0x00000002) /*[1]*/ == 0x00000002 /*1'b1*/) {
	//	cpu_write32(0xffffff10,0x0B0B0B0B);
	}
}

int ddr_init_palladium(struct bm_device_info *bmdi, u32 ddr_freq)
{
	ddr4_init(bmdi, 0);
	ddr4_init(bmdi, 1);
	return 0;
}

static inline void refresh_time(struct bm_device_info *bmdi, u32 ddr_ctl, u32 val)
{
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x8, val);
}

static inline void ecc_enable(struct bm_device_info *bmdi, u32 ddr_ctl, u8 flag)
{
	u32 data = ddr_reg_read_enh(bmdi, ddr_ctl, 0x0);
	if (flag) {
		ddr_reg_write_enh(bmdi, ddr_ctl, 0x0, (data | 0xc00));
	} else {
		ddr_reg_write_enh(bmdi, ddr_ctl, 0x0, (data & 0xfffff3ff));
	}
}

static inline void odt_odi(struct bm_device_info *bmdi, u32 ddr_ctl, u32 val)
{
  //phy odt-odi configure
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x130, val);
}

static inline void write_eye_train_offset(struct bm_device_info *bmdi, u32 ddr_ctl, u32 wdq)
{
  //write eye training offset value, delay dq(for margin)
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x804, wdq);
}

static inline void read_vref(struct bm_device_info *bmdi, u32 ddr_ctl, u32 dbs, u32 db8)
{
  //mmio_write_32(offset + 0x5000067c,0x59000000); //for odt=120ohm
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x67c, db8); //for odt=60ohm, solve ecc error debug
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x680, dbs);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x684, dbs);
}

static inline void set_fabric_interleave_mask(struct bm_device_info *bmdi, u32 size)
{
	u32 data = top_reg_read(bmdi, 0x8);
	top_reg_write(bmdi, 0x8, (data & 0xffffff00) | size);
}

//mem MRS configure(MR0~MR6);;mem odt
static inline void mem_init_asic_bm1682(struct bm_device_info *bmdi, u32 ddr_ctl)
{
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x58, 0x00000032);
	ddr_poll(bmdi, ddr_ctl, 0x58, 0x00000000, 0xfffffffd);

	ddr_reg_write_enh(bmdi, ddr_ctl, 0x58, 0x00c59062);
	ddr_poll(bmdi, ddr_ctl, 0x58, 0x00000000, 0xfffffffd);

	ddr_reg_write_enh(bmdi, ddr_ctl, 0x58, 0x00440052);  //disable read dbi
	ddr_poll(bmdi, ddr_ctl, 0x58, 0x00000000, 0xfffffffd);

	ddr_reg_write_enh(bmdi, ddr_ctl, 0x58, 0x00000042);
	ddr_poll(bmdi, ddr_ctl, 0x58, 0x00000000, 0xfffffffd);

  //mmio_write_32(offset + 0x50000058,0x00820022);//odt=80ohm
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x58, 0x00020022);
	ddr_poll(bmdi, ddr_ctl, 0x58, 0x00000000, 0xfffffffd);

	ddr_reg_write_enh(bmdi, ddr_ctl, 0x58, 0x00101012);//odt=60ohm odt=34ohm
	ddr_poll(bmdi, ddr_ctl, 0x58, 0x00000000, 0xfffffffd);

	ddr_reg_write_enh(bmdi, ddr_ctl, 0x58, 0x00b40002);
	ddr_poll(bmdi, ddr_ctl, 0x58, 0x00000000, 0xfffffffd);
}

/*
*
* offset: ddrc channel
* wdq: tune dq
* dqs_coarse, dqsl_fine, dqsh_fine: tune dqs
*/
//ddrc inital sequence
static void ddr_init_asic_bm1682(struct bm_device_info *bmdi, u32 ddr_ctl, u32 wdq,
	       	u32 dqs_coarse, u32 dqsl_fine, u32 dqsh_fine)
{
	u32 data;

	/* disable write buffer */
	data = ddr_reg_read_enh(bmdi, ddr_ctl, 0x8c);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x8c, (data | (1 << 17)));

	ddr_reg_read_enh(bmdi, ddr_ctl, 0x4);
  //mmio_write_32(offset + 0x50000004, (data & 0x3ff7fffd));
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x4, 0x14);

	ddr_reg_write_enh(bmdi, ddr_ctl, 0x7fc, 0x60000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x100, 0x00000000);
  //phy mode set
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x104, 0x00003062);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x108, 0x1a7a0063);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x10c, 0x1a7a0063);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x110, 0x1a7a0063);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x114, 0x1a7a0063);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x118, 0x20000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x11c, 0x20000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x120, 0x20000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x124, 0x20000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x128, 0x00000008);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x12c, 0x00000000);
  //phy odt-odi configure
	odt_odi(bmdi, ddr_ctl, 0x00077400); //odt=60ohm, odi=34ohm
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x134, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x138, 0x00000000);
  //first CA training command following CKE low
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x13c, 0x20000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x140, 0x00007f00);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x144, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x148, 0x00003f00);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x14c, 0x00003080);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x150, 0xfc00f000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x154, 0x00000220);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x158, 0x0b400201);
  //mmio_write_32(offset +, 0x158,0x0b400101);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x15c, 0x04800000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x160, 0x00000c59);
  //mmio_write_32(offset +, 0x160,0x00000c64);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x164, 0x00000000);
  //168~184 ddr timing registers
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x168, 0x000c0004);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x16c, 0x00800200);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x170, 0x000c000a);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x174, 0x208dc000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x178, 0x5161c10c);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x17c, 0x06080606);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x180, 0xb7801000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x184, 0x0c400c30);
  //MLB CA training DRAM timing setting
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x188, 0x00001000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x18c, 0x05701016);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x190, 0x00000000);
  //PHY CA per bit deskewing manual mode setting
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x19c, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1a0, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1a4, 0x00002020);
  //MLB CA training manual mode fine delay setting
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1a8, 0x80002020);
  //MLB CA training manual mode coarse delay setting
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1ac, 0x00000001);
  //db0~db7 DLL read leveling gate 1/2T delay value
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1cc, 0x01010101);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1d0, 0x01010101);
  //db0~db7 fine-tune phase delay setting
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1d4, 0x80808080);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1d8, 0x80808080);
  //read Vref (db0~8)
	read_vref(bmdi, ddr_ctl, 0x4e4e4e4e, 0x57000000);

	ddr_reg_write_enh(bmdi, ddr_ctl, 0x820, 0x80800180);
  //db0~db7 DLL read eye dqs
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x688, 0x80808080);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x68c, 0x80808080);
  //db0~db7 DLL read eye dqsn
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x690, 0x80808080);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x694, 0x80808080);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6f8, 0x80808080);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6fc, 0x80808080);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x800, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x824, 0x00000090);

  //ddr training configure
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x194, 0xff2112e0);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x198, 0x08060000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1b0, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1b4, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1b8, 0x00000000);
  //write leveling configure
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1bc, dqs_coarse);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1c0, dqsl_fine);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1c4, dqsh_fine);

	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1c8, 0x000aff2c);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1dc, 0x00080000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1e0, 0x00000000);
  //LPDDR4 MR read golden data 0~3
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1e4, 0xaa55aa55);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1e8, 0x55aa55aa);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1ec, 0xaaaa5555);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1f0, 0x5555aaaa);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1f4, 0xaa55aa55);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1f8, 0x55aa55aa);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x1fc, 0xaaaa5555);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x600, 0x5555aaaa);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x604, 0xaa55aa55);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x608, 0x55aa55aa);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x60c, 0xaaaa5555);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x610, 0x5555aaaa);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x614, 0xaa55aa55);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x618, 0x55aa55aa);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x61c, 0xaaaa5555);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x620, 0x5555aaaa);
  //PHY db0~db7 read path dqs/dm/dq0~dq7 per bit deskewing manual mode setting
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x624, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x628, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x62c, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x630, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x634, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x638, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x63c, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x640, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x644, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x648, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x64c, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x650, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x654, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x658, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x65c, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x660, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x664, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x668, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x66c, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x670, 0x20202020);
  //PHY db0~db7 dqs/dm/dq0~7 per bit deskew ing manual mode setting
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x698, 0x20200820);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x69c, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6a0, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6a4, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6a8, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6ac, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6b0, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6b4, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6b8, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6bc, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6c0, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6c4, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6c8, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6cc, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6d0, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6d4, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6d8, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6dc, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6e0, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6e4, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6e8, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x6f4, 0x00000019);
  //wdq delay configure
	write_eye_train_offset(bmdi, ddr_ctl, wdq);
  //INFO("MLB write data eye training:0x%08x\n", wdq);
  //user pattern for MLB training
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x810, 0x11224488);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x814, 0x11224488);

  //PHY db8 read/write path dqs/dm/dq0~7 per bit deskewing manual mode setting
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x828, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x82c, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x830, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x834, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x838, 0x20202020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x83c, 0x00002020);
  //disable ecc bit[11:10]
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x000, 0x00113001);

	data = ddr_reg_read_enh(bmdi, ddr_ctl, 0x4);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x004, (data | 0x1c));
  //ddr refresh configure
  	refresh_time(bmdi, ddr_ctl, 0x0000144e);//for 2667mbps

	ddr_reg_write_enh(bmdi, ddr_ctl, 0x00c, 0x00010c10);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x030, 0x00000000);
  //timing parameter setting
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x040, 0x741c180b);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x044, 0x0a01d328);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x048, 0x00142b14);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x04c, 0x060e0926);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x050, 0x4000e30e);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x054, 0x00000020);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x05c, 0x01440000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x060, 0x00000807);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x064, 0x00000000);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x068, 0x00001400);
#ifdef DDR_PERF_ENABLE
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x090, 0x12839483);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x094, 0x3ff6b16a);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x098, 0x144d2450);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x09c, 0x19617595);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x0a0, 0x1e75c6da);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x0a4, 0x0003ffdf);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x0a8, 0x0003f3ce);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x0ac, 0x00ffffc6);
#else
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x090, 0x10731483);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x094, 0x3ff62d49);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x098, 0x144d2450);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x09c, 0x19617595);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x0a0, 0x1e75c6da);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x0a4, 0x0003ffdf);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x0a8, 0x0003f38d);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x0ac, 0x00ffffcf);
#endif
  // delay 500 us for Enable ddr controller
  //udelay(600);
  /*int wait_val=0x10*1024*1024;
  while(wait_val--);
  */
	udelay(500);

	data = ddr_reg_read_enh(bmdi, ddr_ctl, 0x4);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x4, (data | 0x00080000));

  // polling dfi_init_complete
	ddr_poll(bmdi, ddr_ctl, 0x4, 0x80000000, 0x7fffffff);
  //INFO("poll 0x004, expdata:bit[31]-> 1, success\n");

	data = ddr_reg_read_enh(bmdi, ddr_ctl, 0x4);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x4, (data & 0xfffffff3));
}

static void top_reg_init_asic_bm1682(struct bm_device_info *bmdi)
{
  //read_chk(0x50008008,0x00000001, 0x00);
  //spi-flash address remap: located at 0x4400_0000
  //u32 data = mmio_read_32(0x50008008);
  //mmio_write_32(0x50008008, data | (0x1 << 11));

#ifdef DDR_DEBUG_IO
  enable_debug_io(0x2);
#endif

  //set fabric interleave mask(1kB)
	set_fabric_interleave_mask(bmdi, 0x01);

#if 0
#ifndef DDR_INTERLEAVE_ENABLE //none-interleave
  INFO("ddr interleave disable\n");
  //ddr0 addr configure
  mmio_write_32(0x50008180,0x0);
  mmio_write_32(0x50008184,0x1);
  mmio_write_32(0x50008188,0xffffffff);
  mmio_write_32(0x5000818c,0x1);
  //ddr1 addr configure
  mmio_write_32(0x50008190,0x0);
  mmio_write_32(0x50008194,0x2);
  mmio_write_32(0x50008198,0xffffffff);
  mmio_write_32(0x5000819c,0x2);

  //none-interleave bit[4]=0
  mmio_write_32(0x500081c0,0x02);
#else
  mmio_write_32(0x500081c0,0x12);
  INFO("ddr interleave enable\n");
#endif
#endif
}

//ddr BIST(Build-in self test)
static int ddr_self_test_pattern(struct bm_device_info *bmdi, u32 ddr_ctl)
{
	u32 data;
	data = ddr_reg_read_enh(bmdi, ddr_ctl, 0x104);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x104, (data & 0xffffffbf));

	//enable BIST
	data = ddr_reg_read_enh(bmdi, ddr_ctl, 0x04);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x04, (data | 0x00000001));
	// release SDII grant capability
	data = ddr_reg_read_enh(bmdi, ddr_ctl, 0x04);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x04, (data & ~0x00000002));
	data = ddr_reg_read_enh(bmdi, ddr_ctl, 0x04);
	if ((data & ~0xfffffffd) == 0x02) {
		pr_err("bmdrv: release SDII grant capability, FORCE_SDII_NO_GNT should be 0!\n");
		return -1;
	}

	//sdii count
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x700, 0x00000040);
	//start bank...
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x704,
		((0x7) | (0x3 << 4) | (0x0 << 8) | (0xf << 12) | (0x1 << 16) |
		(0x0 << 20) | (0x0 << 21) | (0x0 << 27) | (0x1 << 28) | (0x0 << 29) | (0x0 << 30)));
	//start column
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x708, (0xfc0 | (0x3fff3 << 12)));
	ddr_reg_read_enh(bmdi, ddr_ctl, 0x708);

	//start BIST
	data = ddr_reg_read_enh(bmdi, ddr_ctl, 0x71c);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x71c, (data | 0x00000001));
	ddr_reg_read_enh(bmdi, ddr_ctl, 0x71c);

	//poll: wait for BIST test done
	ddr_poll(bmdi, ddr_ctl, 0x71c, 0x00000000, 0xfffffffe);

	data = ddr_reg_read_enh(bmdi, ddr_ctl, 0x71c);
	if ((data & ~0xffffffef) == 0x10) {
		pr_info("bmdrv: [ddr_%04x BIST] success!\n", ddr_ctl);
	} else {
		pr_info("bmdrv: [ddr_%04x BIST] failed!\n", ddr_ctl);
		data = ddr_reg_read_enh(bmdi, ddr_ctl, 0x4);
		ddr_reg_write_enh(bmdi, ddr_ctl, 0x4, (data & 0xfffffffe));
		return -1;
	}
	data = ddr_reg_read_enh(bmdi, ddr_ctl, 0x4);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x4, (data & 0xfffffffe));
#ifdef DDR_ECC_ENABLE
	ecc_enable(bmdi, ddr_ctl, 1);//enable ecc
#else
	ecc_enable(bmdi, ddr_ctl, 0);
#endif
	return 0;
}

// calculate ddr pll div parameters
static void calc_freq_div(u32 freq, u32 *ref_div, u32 *fb_div)
{
	u32 Mb = 1000 * 1000;
	u32 ref_clock = UART0_CLK_IN_HZ * 2 / Mb;

	switch(freq) {
	case 2133: //fine
		*ref_div = 0x3;
		*fb_div = 0x20;
	break;
	case 2666: //fine
		*ref_div = 0x3;
		*fb_div = 0x28;
	break;
	default: //coarse
		*ref_div = 0x2;
		*fb_div = freq / (ref_clock / (*ref_div) * 4);
	break;
	}
}

//configure ddr freq for each channel after ddr initialization
void ddr_freq_configure(struct bm_device_info *bmdi, u32 ddr_ctl, u32 freq)
{
	u32 ref_div = 0, fb_div = 0;
	u32 data;
	calc_freq_div(freq, &ref_div, &fb_div);

	//stop controller;; enter self-refresh
	data = ddr_reg_read_enh(bmdi, ddr_ctl, 0x4);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x4, (data | 0x10002));
	ddr_poll(bmdi, ddr_ctl, 0x4, 0x00000000, 0xfffeffff);

	data = ref_div | (0x1 << 8) | (0x1 << 12) | (fb_div << 16) | (0x00 << 24);
	switch (ddr_ctl) {
	case 0x0:
		top_reg_write(bmdi, 0x0f4, data);
	break;
	case 0x1:
		top_reg_write(bmdi, 0x0f8, data);
	break;
	default:
	break;
	}
	//check clock status, bit: 20~16, ddr3,ddr2,ddr1,mpll,spll
	data = top_reg_read(bmdi, 0x100);
	while ((data & 0x040001) != 0) {
		data = top_reg_read(bmdi, 0x100);
	}
	//exit self-refresh
	data = ddr_reg_read_enh(bmdi, ddr_ctl, 0x4);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x4, (data | 0x20000));
	ddr_poll(bmdi, ddr_ctl, 0x4, 0x00000000, 0xfffdffff);
	//restart the controller
	data = ddr_reg_read_enh(bmdi, ddr_ctl, 0x4);
	ddr_reg_write_enh(bmdi, ddr_ctl, 0x4, (data & 0xfffffffd));
}

//configure all ddr pll into initialization
static void ddr_freq_init(struct bm_device_info *bmdi, u32 freq)
{
	u32 ref_div = 0, fb_div = 0;
	u32 data;
	calc_freq_div(freq, &ref_div, &fb_div);

	data = ref_div | (0x1 << 8) | (0x1 << 12) | (fb_div << 16) | (0x00 << 24);
	top_reg_write(bmdi, 0x0f4, data);
	top_reg_write(bmdi, 0x0f8, data);

	//check clock status, bit18(ddr1) bit0(ddr0),
	data = top_reg_read(bmdi, 0x100);
	while ((data & 0x040001) != 0) {
		data = top_reg_read(bmdi, 0x100);
	}
}

static void ddr_soft_reset(struct bm_device_info *bmdi)
{
	u32 data;
	data = top_reg_read(bmdi, 0x14);
	top_reg_write(bmdi, 0x14, (data & 0xffffe3ff));
	udelay(100);
	data = top_reg_read(bmdi, 0x14);
	top_reg_write(bmdi, 0x14, (data | 0x00001c00));
}

int ddr_init(struct bm_device_info *bmdi, u32 freq)
{
	int ret;
	u8 id;
	//close gate
	top_reg_write(bmdi, 0x0, 0xf7ffffff);

	//ddr freq configure(unit:Mbps)
	ddr_freq_init(bmdi, freq);

	//open gate
	top_reg_write(bmdi, 0x0, 0xffffffff);

	ddr_soft_reset(bmdi);

	top_reg_init_asic_bm1682(bmdi);

	id = calc_ddr_para_id(freq);
	pr_info("bmdrv : ddrc0_init...\n");
	//ddrc inital sequence and dqs optimizing(coarse, (dqs0~3)fine , (dqs4~7)fine)
	ddr_init_asic_bm1682(bmdi, 0x0, ddr_para[0][id].wdqs, ddr_para[0][id].dqs_coarse,
		       ddr_para[0][id].dqsl_fine, ddr_para[0][id].dqsh_fine);
	//INFO("ddr0 dram init...\n");
	mem_init_asic_bm1682(bmdi, 0x0);
	//INFO("ddr0 bist...\n");
 	ret = ddr_self_test_pattern(bmdi, 0x0);
	if (ret) {
		pr_err("bmdrv: ddr self test failed!\n");
		return ret;
	}
	//INFO("ddrc0_init end...\n");
#ifdef DUMP_DDRC_REG
	dump_reg(0x50000000, 0x1000);
#endif

	pr_info("bmdrv : ddrc1 init...\n");
	ddr_init_asic_bm1682(bmdi, 0x1, ddr_para[1][id].wdqs, ddr_para[1][id].dqs_coarse,
		       ddr_para[1][id].dqsl_fine, ddr_para[1][id].dqsh_fine);
	//INFO("ddr1 dram init...\n");
	mem_init_asic_bm1682(bmdi, 0x1);
	//INFO("ddr1 bist...\n");
	ret = ddr_self_test_pattern(bmdi, 0x1);
	if (ret) {
		pr_err("bmdrv: ddr self test failed!\n");
		return ret;
	}
	//INFO("ddrc1 init end...\n");
#ifdef DUMP_DDRC_REG
	dump_reg(0x50001000, 0x1000);
#endif
	return ret;
}

int bm1682_ddr_top_init(struct bm_device_info *bmdi)
{
	int ret;
	u32 ddr_freq = 0;

	get_ddr_freq(bmdi, &ddr_freq);
	bm1682_disable_ddr_interleave(bmdi);

#ifdef LOCAL_MEM_EARLY_RESP
	enable_local_mem_early_resp(bmdi);
#else
	disable_local_mem_early_resp(bmdi);
#endif

	if (bmdi->cinfo.platform == PALLADIUM)
		ret = ddr_init_palladium(bmdi, ddr_freq);
	else
		ret = ddr_init(bmdi, ddr_freq);
	if (ret) {
		pr_err("bmdrv : init ddr failed\n");
	}
	return ret;
}
