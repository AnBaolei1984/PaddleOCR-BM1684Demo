#include "temp_def.h"
#include "bm_common.h"
#include "bm_io.h"
#include <linux/delay.h>
#include "bm1684_pcie.h"
#include "bm_memcpy.h"
#include "bm1684_lpddr4.h"
#include "bm1684_ddr.h"

/*
 * this file is for configuring the DDR sub-system for:
 *  - LPDDR4
 *  - non-inlineECC
 *  - No training, and uMCTL2 will do SDRAM initialization
 *  - Speedbin: 4267, Set-A
 *  - LOCKSTEP model
 *  - 2 DFI (POP) connection
 *  - 2 rank
 *  - 32 Gb per rank
 */

static  void opdelay(unsigned int times)
{
    udelay(times);
}

static  void dwc_phy_apb_write16(struct bm_device_info *bmdi, u32 base, u32 offset,  u32 value)
{
	ddr_phy_reg_write16(bmdi, base + PHY_OFFSET + (offset << 1), value);
}

static u16 dwc_phy_apb_read16(struct bm_device_info *bmdi, u32 base, u32 offset)
{
	return ddr_phy_reg_read16(bmdi, base + PHY_OFFSET + (offset << 1));
}

static int dwc_get_mail(struct bm_device_info *bmdi, u32 cfg_base, int mode)
{
	int mail;

	/* waiting a msg */
	while (dwc_phy_apb_read16(bmdi, cfg_base, UCT_SHANDOW_REG)
			& UCT_WRITE_PROT_SHADOW)
		;

	mail = dwc_phy_apb_read16(bmdi, cfg_base, UCT_WRITE_ONLY_SHADOW_REG);

	if (mode == STREAM_MSG)
		mail |= (dwc_phy_apb_read16(bmdi, cfg_base, UCT_DATWRITE_ONLY_SHADOW_REG) << 16);

	dwc_phy_apb_write16(bmdi, cfg_base, DCT_WRITE_PROT_REG, 0);

	while (!(dwc_phy_apb_read16(bmdi, cfg_base, UCT_SHANDOW_REG)
		& UCT_WRITE_PROT_SHADOW))
		;

	dwc_phy_apb_write16(bmdi, cfg_base, DCT_WRITE_PROT_REG, 1);

	return mail;
}

static void dwc_decode_stream_msg(struct bm_device_info *bmdi, u32 cfg_base, int train_2d)
{
	int i;
	int str_stream_index = dwc_get_mail(bmdi, cfg_base, STREAM_MSG);

	PR_TRACE("%s str_stream_index 0x%08x\n",
	train_2d ? "2D" : "1D", str_stream_index);
	i = 0;
	while (i < (str_stream_index & 0xffff)) {
		dwc_get_mail(bmdi, cfg_base, STREAM_MSG);
		i++;
	}
}

static void dwc_decode_major_msg(struct bm_device_info *bmdi, int msg)
{
	if (msg == FW_COMPLET_FAILED) {
		pr_info("dwc phy message major message type: 0x%x, %s\n", msg,
			msg == FW_COMPLET_SUCCESS ? "success"
				: msg == FW_COMPLET_FAILED ? "failed" : "unknown");
	}
}

void mask_train_result(struct bm_device_info *bmdi, u32 index, u32 phase, u32 result)
{
	bmdi->cinfo.ddr_failmap &= ~(1UL << ((index << 1) + phase));
	bmdi->cinfo.ddr_failmap |= ((result & 0x1) << ((index << 1) + phase));
}

static void dwc_train_status(struct bm_device_info *bmdi, u32 cfg_base,
		u32 train_2d, u32 result)
{
	u32 pass_or_fail = result == 0xff ? 1 : 0;
	u32 index = 0;

	index = cfg_base == DDR_CTRL0_A ? DDR_CTRL0_A_INDEX
	: cfg_base == DDR_CTRL0_B ? DDR_CTRL0_B_INDEX
	: cfg_base == DDR_CTRL1 ? DDR_CTRL1_INDEX
	: DDR_CTRL2_INDEX;

	mask_train_result(bmdi, index, train_2d, pass_or_fail);
}

void dwc_read_msgblock_msg(struct bm_device_info *bmdi, u32 cfg_base, u32 train_2d)
{
	int mail;

loop:
	mail = dwc_get_mail(bmdi, cfg_base, MAJOR_MSG);
	if (mail == STREAM_MSG)
		dwc_decode_stream_msg(bmdi, cfg_base, train_2d);
	else
		dwc_decode_major_msg(bmdi, mail);
	if (mail != 0x7 && mail != 0xff)
		goto loop;

	if (mail == 0xff) {
		pr_err("Lpddr train failed, ddr: %s, phase: %d\n",
		cfg_base == DDR_CTRL0_A ? "0A"
		: cfg_base == DDR_CTRL0_B ? "0B"
		: cfg_base == DDR_CTRL1 ? "1"
		: "2", train_2d);
	}

	dwc_train_status(bmdi, cfg_base, train_2d, mail);
}

static inline void dwc_exec_fw(struct bm_device_info *bmdi, int cfg_base, u32 train_2d)
{

	dwc_phy_apb_write16(bmdi, cfg_base, 0xd0000, 0x1); // DWC_DDRPHYA_APBONLY0_MicroContMuxSel

	//1.  Reset the firmware microcontroller by writing the MicroReset CSR to set the StallToMicro and
	//	  ResetToMicro fields to 1 (all other fields should be zero).
	//	  Then rewrite the CSR so that only the StallToMicro remains set (all other fields should be zero).
	dwc_phy_apb_write16(bmdi, cfg_base, 0xd0000, 0x1); // DWC_DDRPHYA_APBONLY0_MicroContMuxSel
	dwc_phy_apb_write16(bmdi, cfg_base, 0xd0099, 0x9); // DWC_DDRPHYA_APBONLY0_MicroReset
	dwc_phy_apb_write16(bmdi, cfg_base, 0xd0099, 0x1); // DWC_DDRPHYA_APBONLY0_MicroReset

	// 2. Begin execution of the training firmware by setting the MicroReset CSR to 4'b0000.
	dwc_phy_apb_write16(bmdi, cfg_base, 0xd0099, 0x0); // DWC_DDRPHYA_APBONLY0_MicroReset

	// 3.   Wait for the training firmware to complete by following the procedure in "uCtrl Initialization and Mailbox Messaging"
	// [dwc_ddrphy_phyinit_userCustom_G_waitFwDone] Wait for the training firmware to complete.	Implement timeout fucntion or follow the procedure in "3.4 Running the firmware" of the Training Firmware Application Note to poll the Mailbox message.
	//dwc_ddrphy_phyinit_userCustom_G_waitFwDone ();

	dwc_read_msgblock_msg(bmdi, cfg_base, train_2d);

	// [dwc_ddrphy_phyinit_userCustom_G_waitFwDone] End of dwc_ddrphy_phyinit_userCustom_G_waitFwDone()
	// 4.   Halt the microcontroller."
	dwc_phy_apb_write16(bmdi, cfg_base, 0xd0099, 0x1); // DWC_DDRPHYA_APBONLY0_MicroReset
	// [dwc_ddrphy_phyinit_G_execFW] End of dwc_ddrphy_phyinit_G_execFW ()
}

static int ddr_training(struct bm_device_info *bmdi, u32 cfg_base, u32 train)
{
	u32 i, train_addr;

	if (train == T1D) {
		//2 load 1d-imem
		train_addr = TARIN_IMEM_OFFSET;
		//Programming MemResetL to 0x2
		dwc_phy_apb_write16(bmdi, cfg_base, MEM_RSETE_L, 0x2);
		dwc_phy_apb_write16(bmdi, cfg_base, MICRO_CONT_MUX_SEL, 0);
		for (i = 0; i < ARRAY_SIZE(lpddr4x_train1d_imem); i++, train_addr++)
			dwc_phy_apb_write16(bmdi, cfg_base, train_addr, lpddr4x_train1d_imem[i]);
		dwc_phy_apb_write16(bmdi, cfg_base, MICRO_CONT_MUX_SEL, 1);

		//3 set dfi clock
		PR_TRACE("phy load dmem\n");

		//4 load 1d-dmem
		train_addr = TARIN_DMEM_OFFSET;
		dwc_phy_apb_write16(bmdi, cfg_base, MICRO_CONT_MUX_SEL, 0);
		for (i = 0; i < ARRAY_SIZE(lpddr4x_train1d_dmem); i++, train_addr++)
			dwc_phy_apb_write16(bmdi, cfg_base, train_addr, lpddr4x_train1d_dmem[i]);
		dwc_phy_apb_write16(bmdi, cfg_base, MICRO_CONT_MUX_SEL, 1);

		PR_TRACE("phy run firmware\n");

		dwc_exec_fw(bmdi, cfg_base, T1D);

	} else {
		//6 load 2d-imem
		train_addr = TARIN_IMEM_OFFSET;
		dwc_phy_apb_write16(bmdi, cfg_base, MEM_RSETE_L, 0x2);
		dwc_phy_apb_write16(bmdi, cfg_base, MICRO_CONT_MUX_SEL, 0);
		for (i = 0; i < ARRAY_SIZE(lpddr4x_train2d_imem); i++, train_addr++)
			dwc_phy_apb_write16(bmdi, cfg_base, train_addr, lpddr4x_train2d_imem[i]);
		dwc_phy_apb_write16(bmdi, cfg_base, MICRO_CONT_MUX_SEL, 1);

		//7 load 2d-dmem
		train_addr = TARIN_DMEM_OFFSET;
		dwc_phy_apb_write16(bmdi, cfg_base, MICRO_CONT_MUX_SEL, 0);
		for (i = 0; i < ARRAY_SIZE(lpddr4x_train2d_dmem); i++, train_addr++)
			dwc_phy_apb_write16(bmdi, cfg_base, train_addr, lpddr4x_train2d_dmem[i]);
		dwc_phy_apb_write16(bmdi, cfg_base, MICRO_CONT_MUX_SEL, 1);

		dwc_exec_fw(bmdi, cfg_base, T2D);
	}

	return 0;
}

static int ddr_phy_enter_mission(struct bm_device_info *bmdi, u32 cfg_base, u32 ddr_index)
{
	u32 i;
	u32 read_data;

	//5 set dfi clock highest
	PR_TRACE("phy load pie\n");

	dwc_phy_apb_write16(bmdi, cfg_base, MICRO_CONT_MUX_SEL, 0);
	for (i = 0; i < ARRAY_SIZE(phy_pie_data); i++)
		dwc_phy_apb_write16(bmdi, cfg_base, phy_pie_data[i].offset, phy_pie_data[i].value);
	dwc_phy_apb_write16(bmdi, cfg_base, MICRO_CONT_MUX_SEL, 1);

	PR_TRACE("phy run firmware done\n");


	PR_TRACE("Wait for impedance calibration\n");

	// Wait for impedance calibration to complete one round of calbiation by polling CalBusy CSR to be 0
	do {
		read_data =
		    dwc_phy_apb_read16(bmdi, cfg_base, 0x00020097);
	} while ((read_data & 0x00000001) != 0);
	// phy is ready for initial dfi_init_start request
	// set umctl2 to tigger dfi_init_start
	bm_write32(bmdi, cfg_base + 0x000001b0, 0x00000060);

	// wait for dfi_init_complete to be 1
	do {
		read_data = bm_read32(bmdi, cfg_base + 0x000001bc);
	} while ((read_data & 0x00000001) != 1);

	// deassert dfi_init_start, and enable the act on dfi_init_complete
	bm_write32(bmdi, cfg_base + 0x000001b0, 0x00000041);

	// dynamic group0 wakeup
	bm_write32(bmdi, cfg_base + 0x320, 0x00000001);
	do {
		read_data = bm_read32(bmdi, cfg_base + 0x324);
	} while ((read_data & 0x00000001) != 1);

	// waiting controller in normal mode
	do {
		read_data = bm_read32(bmdi, cfg_base + 0x004);
	} while ((read_data & 0x00000001) != 1);

	// set selfref_en, and en_dfi_dram_clk_disable again, recover it, in reset_dut_do_training in ddrc_env
	bm_write32(bmdi, cfg_base + 0x00000030, 0x00000000);

	return 0;
}

static int ddr_phy_init(struct bm_device_info *bmdi, u32 cfg_base, u32 ddr_index)
{
	u32 i;
	u32 read_data;

	// 1. Assert the core_ddrc_rstn and areset_n resets (ddrc reset, and axi reset)
	// 2. assert the preset
	// 3. start the clocks (pclk, core_ddrc_clk, aclk_n)
	// 4. Deassert presetn once the clocks are active and stable
	// 5. allow 128 cycle for synchronization of presetn to core_ddrc_core_clk and aclk domain and to
	//    permit initialization of end logic
	// 6. Initialize the registers
	// 7. Deassert the resets

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	//
	// uMCTL2 DDRC Controller Initialization
	//
	//////////////////////////////////////////////////////////////////////////////////////////////////////
	//
	// Start Clocks
	// TODO:
	//
	// Assert core_ddrc_rstn, areset_n
	read_data = bm_read32(bmdi, 0x50010c00);	//bit 3:6 -> ddr0a/ddr0b/ddr1/ddr2 soft-reset
	read_data = read_data & (~(1 << (3 + ddr_index)));
	bm_write32(bmdi, 0x50010c00, read_data);
	opdelay(50);
	// assert preset
	read_data = bm_read32(bmdi, 0x50010c00);	//bit 7:10 -> ddr0a/ddr0b/ddr1/ddr2 apb-reset
	bm_write32(bmdi, 0x50010c00, read_data & ~(1 << (7 + ddr_index)));

	opdelay(100);
	// Assert pwrokin, bit[11:14]
	//bit 11:14 -> ddr0a/ddr0b/ddr1/ddr2 pwrokin
	read_data |= 1 << (11 + ddr_index);
	bm_write32(bmdi, 0x50010c00, read_data);
	opdelay(100);
	// Deassert preset
	read_data |= 1 << (7 + ddr_index);
	bm_write32(bmdi, 0x50010c00, read_data);
	opdelay(100);
	//
	// wait 128 cycle of pclk at least
	// TODO:
	opdelay(1000);

	for (i = 0; i < ARRAY_SIZE(cfg_data); i++)
		bm_write32(bmdi, cfg_base + cfg_data[i].offset, cfg_data[i].value);

	// make sure the write is done
	// in ddrc_env, it just wait 10 clock to make sure write is done,
	// and then release core_ddrc_rstn
	// TODO:
	opdelay(100);
	//
	// the DDR initialization if done by here ////
	//
	// Dessert the resets: axi reset and core_ddrc_rstn
	// TODO:
	read_data = bm_read32(bmdi, 0x50010c00);	//bit 3:6 -> ddr0a/ddr0b/ddr1/ddr2 soft-reset
	read_data = read_data | (1 << (3 + ddr_index));
	bm_write32(bmdi, 0x50010c00, read_data);
	opdelay(50);

	// enable the reigster access for dynamic register
	// APB_QUASI_DYN_G0_SLEEP
	bm_write32(bmdi, cfg_base + 0x00000320, 0x00000000);

	// set dfi_init_complete_en, for phy initialization
	bm_write32(bmdi, cfg_base + 0x000001b0, 0x00000040);

	//DDR_PHY_ENABLE
	//0 pin swap
	PR_TRACE("phy pin swap\n");
	dwc_phy_apb_write16(bmdi, cfg_base, MICRO_CONT_MUX_SEL, 0);
	if (ddr_index == 0) {
		for (i = 0; i < ARRAY_SIZE(pin_swap_0); i++)
			dwc_phy_apb_write16(bmdi, cfg_base, pin_swap_0[i].offset, pin_swap_0[i].value);
	} else if (ddr_index == 1) {
		for (i = 0; i < ARRAY_SIZE(pin_swap_1); i++)
			dwc_phy_apb_write16(bmdi, cfg_base, pin_swap_1[i].offset, pin_swap_1[i].value);
	} else if (ddr_index == 2) {
		for (i = 0; i < ARRAY_SIZE(pin_swap_2); i++)
			dwc_phy_apb_write16(bmdi, cfg_base, pin_swap_2[i].offset, pin_swap_2[i].value);
	} else if (ddr_index == 3) {
		for (i = 0; i < ARRAY_SIZE(pin_swap_3); i++)
			dwc_phy_apb_write16(bmdi, cfg_base, pin_swap_3[i].offset, pin_swap_3[i].value);
	}
	PR_TRACE("phy pll\n");

	PR_TRACE("phy initial\n");
	//1 phy initial
	for (i = 0; i < ARRAY_SIZE(phy_initial_data); i++)
		dwc_phy_apb_write16(bmdi, cfg_base, phy_initial_data[i].offset, phy_initial_data[i].value);

	dwc_phy_apb_write16(bmdi, cfg_base, 0x200ca, 0x124);
	dwc_phy_apb_write16(bmdi, cfg_base, 0x200c7, 0x20);
//	dwc_phy_apb_write16(bmdi, cfg_base, 0x200c5, 0xa);
	dwc_phy_apb_write16(bmdi, cfg_base, 0x200cc, 0x17f);
	PR_TRACE("phy initial\n");

	return 0;
}

void dwc_phy_enter_mission(struct bm_device_info *bmdi)
{
	ddr_phy_enter_mission(bmdi, DDR_CTRL0_A, 0);
	ddr_phy_enter_mission(bmdi, DDR_CTRL0_B, 1);
	ddr_phy_enter_mission(bmdi, DDR_CTRL1, 2);
	ddr_phy_enter_mission(bmdi, DDR_CTRL2, 3);
}

static struct pll_controll freq_table[] = {
	{FREQ_800M, 1, 2, 2, 32}, //800M
	{FREQ_2400M, 1, 2, 1, 48}, //2400M
	{FREQ_3200M, 1, 1, 1, 32}, //3200M
	{FREQ_3600M, 1, 1, 1, 36}, //3600M
	{FREQ_4000M, 1, 1, 1, 40}, //4000M
	{FREQ_4266M, 1, 3, 1, 128}, //4266M
};

static  void pll_setting(struct bm_device_info *bmdi, u32 freq)
{
	u32 value, i;
	u32 refdiv, post1div, post2div, fbdiv;

	if (!freq)
		return;

	for (i = 0; i < ARRAY_SIZE(freq_table); i++)
		if (freq_table[i].freq == freq)
			goto do_set;

	PR_TRACE("No support freq\n");
	return;

do_set:
	refdiv = freq_table[i].refdiv;
	post1div = freq_table[i].post1div;
	post2div = freq_table[i].post2div;
	fbdiv = freq_table[i].fbdiv;


	//dpll0
	value = bm_read32(bmdi, TOP_BASE + PLL_EN_OFFSET);
	bm_write32(bmdi, TOP_BASE + PLL_EN_OFFSET, value & ~(0x1 << 4));
	bm_write32(bmdi, TOP_BASE + DPLL0_OFFSET,
					refdiv | fbdiv << 16 | post1div << 8 | post2div << 12);

	while (!((bm_read32(bmdi, TOP_BASE + PLL_STATUS_OFFSET) >> 12) & 0x1))
		;

	while ((bm_read32(bmdi, TOP_BASE + PLL_STATUS_OFFSET) >> 4) & 0x1)
		;

	bm_write32(bmdi, TOP_BASE + PLL_EN_OFFSET, value | (0x1 << 4));

	//dpll1
	value = bm_read32(bmdi, TOP_BASE + PLL_EN_OFFSET);
	bm_write32(bmdi, TOP_BASE + PLL_EN_OFFSET, value & ~(0x1 << 5));
	bm_write32(bmdi, TOP_BASE + DPLL1_OFFSET,
					refdiv | fbdiv << 16 | post1div << 8 | post2div << 12);

	while (!((bm_read32(bmdi, TOP_BASE + PLL_STATUS_OFFSET) >> 13) & 0x1))
		;

	while ((bm_read32(bmdi, TOP_BASE + PLL_STATUS_OFFSET) >> 5) & 0x1)
		;

	bm_write32(bmdi, TOP_BASE + PLL_EN_OFFSET, value | (0x3 << 5));
}

static void dwc_pre_setting(struct bm_device_info *bmdi, u32 rank, u32 freq)
{
	int i, count;
	struct dwc_precfg_freq *cfg_freq = NULL;
	struct dwc_precfg_rank *cfg_rank = NULL;
	struct init_data *cfg_ecc = NULL;
	u32 ecc_array_size = 0;

	for (i = 0; i < ARRAY_SIZE(pre_cfg_rank); i++) {
		if (rank == pre_cfg_rank[i].rank_num)
			cfg_rank = &pre_cfg_rank[i];
	}

	for (i = 0; i < ARRAY_SIZE(pre_cfg_freq); i++) {
		if (freq == pre_cfg_freq[i].frequency)
			cfg_freq = &pre_cfg_freq[i];
	}

	if (!cfg_freq || !cfg_rank)
		pr_err("lpddr4/4x can't get configuration\n");

	//override phy
	for (count = 0; count < ARRAY_SIZE(cfg_freq->phy_freq_data); count++) {
		for (i = 0; i < ARRAY_SIZE(phy_initial_data); i++)
			if (phy_initial_data[i].offset == cfg_freq->phy_freq_data[count].offset) {
				phy_initial_data[i].value = cfg_freq->phy_freq_data[count].value;
				break;
			}
	}

	//override pie
	for (count = 0; count < ARRAY_SIZE(cfg_freq->pie_freq_data); count++) {
		for (i = 0; i < ARRAY_SIZE(phy_pie_data); i++)
			if (phy_pie_data[i].offset == cfg_freq->pie_freq_data[count].offset) {
				phy_pie_data[i].value = cfg_freq->pie_freq_data[count].value;
				break;
			}
	}

	//override train 1d
	for (count = 0; count < ARRAY_SIZE(cfg_freq->train1d_dmem_freq_data); count++) {
		i = cfg_freq->train1d_dmem_freq_data[count].offset;
		lpddr4x_train1d_dmem[i] = cfg_freq->train1d_dmem_freq_data[count].value;
	}

	//override train 2d
	for (count = 0; count < ARRAY_SIZE(cfg_freq->train2d_dmem_freq_data); count++) {
		i = cfg_freq->train2d_dmem_freq_data[count].offset;
		lpddr4x_train2d_dmem[i] = cfg_freq->train2d_dmem_freq_data[count].value;
	}

	//override train 1d rank
	for (count = 0; count < ARRAY_SIZE(cfg_rank->train1d_dmem_rank_data); count++) {
		i = cfg_rank->train1d_dmem_rank_data[count].offset;
		lpddr4x_train1d_dmem[i] = cfg_rank->train1d_dmem_rank_data[count].value;
	}

	//override train 2d rank
	for (count = 0; count < ARRAY_SIZE(cfg_rank->train2d_dmem_rank_data); count++) {
		i = cfg_rank->train2d_dmem_rank_data[count].offset;
		lpddr4x_train2d_dmem[i] = cfg_rank->train2d_dmem_rank_data[count].value;
	}

	//override cfg rank
	cfg_data[0].value = cfg_rank->cfg_rank;

	if (bmdi->boot_info.ddr_ecc_enable == 0x1) {
		if (rank == RANK1) {
			cfg_ecc = cfg_ecc_1rank;
			ecc_array_size = ARRAY_SIZE(cfg_ecc_1rank);
		} else if (rank == RANK2) {
			cfg_ecc = cfg_ecc_2rank;
			ecc_array_size = ARRAY_SIZE(cfg_ecc_2rank);
		}
	} else {
		cfg_ecc = cfg_ecc_off;
		ecc_array_size = ARRAY_SIZE(cfg_ecc_off);
	}

	for (count = 0; count < ecc_array_size; count++) {
		for (i = 0; i < ARRAY_SIZE(cfg_data); i++) {
			if (cfg_data[i].offset == cfg_ecc[count].offset) {
				cfg_data[i].value = cfg_ecc[count].value;
				break;
			}
		}
	}

	/* lpddr4x/4 override cfg,
	 * default config is lpddr4x, if choose lpddr4 mode
	 * use lpddr4 mrs setting override lpddr4x's.
	 */
	if ((bmdi->boot_info.ddr_mode & 0xffff) == LPDDR4) {
		struct msg_mrs *mrs;
		u8 *data, *chan_a_offset, *chan_b_offset;
		u32 size = 0;

		if (cfg_rank->rank_num == RANK1)
			mrs = &lpddr4_1rank;
		else
			mrs = &lpddr4_2rank;

		size = &mrs->mr22 - &mrs->mr1 + 1;
		chan_a_offset = (u8 *)&lpddr4x_train1d_dmem
				+ mrs->train_chan_a;
		chan_b_offset = (u8 *)&lpddr4x_train1d_dmem
				+ mrs->train_chan_b;

		//override train 1d mrs
		data = chan_a_offset;
		memcpy(data, &mrs->mr1, size);
		memcpy(data + size, &mrs->mr1, size);

		data = chan_b_offset;
		memcpy(data, &mrs->mr1, size);
		memcpy(data + size, &mrs->mr1, size);

		//override train 2d mrs
		chan_a_offset = (u8 *)&lpddr4x_train2d_dmem
				+ mrs->train_chan_a;
		chan_b_offset = (u8 *)&lpddr4x_train2d_dmem
				+ mrs->train_chan_b;

		data = chan_a_offset;
		memcpy(data, &mrs->mr1, size);
		memcpy(data + size, &mrs->mr1, size);

		data = chan_b_offset;
		memcpy(data, &mrs->mr1, size);
		memcpy(data + size, &mrs->mr1, size);
	}
}

/*
 * ddr0a/ddr0b share a hardware reset signal,
 * ddr1/ddr2 share another.
 * the initial sequency should be 0a->0b, 1->2
 * so we put 0a/0b 1/2 in 2 groups.
 */
static void ddr_group_init(struct bm_device_info *bmdi, u32 group)
{
	if (group == GROUP0) {
		//phy init
		ddr_phy_init(bmdi, DDR_CTRL0_A, 0);
		ddr_phy_init(bmdi, DDR_CTRL0_B, 1);
		//1d train
		ddr_training(bmdi, DDR_CTRL0_A, 0);
		ddr_training(bmdi, DDR_CTRL0_B, 0);
		//2d train
		ddr_training(bmdi, DDR_CTRL0_A, 1);
		ddr_training(bmdi, DDR_CTRL0_B, 1);
	} else {
		ddr_phy_init(bmdi, DDR_CTRL1, 2);
		ddr_phy_init(bmdi, DDR_CTRL2, 3);

		ddr_training(bmdi, DDR_CTRL1, 0);
		ddr_training(bmdi, DDR_CTRL2, 0);

		ddr_training(bmdi, DDR_CTRL1, 1);
		ddr_training(bmdi, DDR_CTRL2, 1);
	}
}

static int ddr_memset_by_cdma(struct bm_device_info *bmdi, u64 start_addr, u64 end_addr)
{
	int i = 0;
	bm_cdma_arg cdma_arg;
	struct bm_memcpy_info *memcpy_info = &bmdi->memcpy_info;
	u32 size = memcpy_info->stagemem_s2d.size;
	int num = (end_addr - start_addr)/size;

	PR_TRACE("ecc memset start_addr = 0x%llx, end_addr = 0x%llx, num = 0x%x, size = 0x%x\n",
		start_addr, end_addr, num, size);
	mutex_lock(&memcpy_info->stagemem_s2d.stage_mutex);
	memset(memcpy_info->stagemem_s2d.v_addr, 0, size);
	for (i = 0; i <= num; i++) {
		bmdev_construct_cdma_arg(&cdma_arg,
		memcpy_info->stagemem_s2d.p_addr & 0xffffffffff,
		start_addr + size * i,
		size, HOST2CHIP,
		false, false);
		if (memcpy_info->bm_cdma_transfer(bmdi, NULL, &cdma_arg, true)) {
			pr_err("ddr memeset src = 0x%llx, dst = 0x%llx fail\n",
			start_addr, end_addr);
			mutex_unlock(&memcpy_info->stagemem_s2d.stage_mutex);
			return -EBUSY;
		}
	}
	mutex_unlock(&memcpy_info->stagemem_s2d.stage_mutex);

	return 0;
}

int bm1684_lpddr4_init(struct bm_device_info *bmdi)
{
	int reg_val = 0;
	int chip_mode = top_reg_read(bmdi, 0x4) & 0x3;
	u32 freq = 0;
	u32 rank = 0;
	u32 fail_count = 0;

	if (chip_mode == NORMAL_MODE)
		freq = FREQ_2400M;
	else
		freq = FREQ_4000M;
	switch (bmdi->boot_info.ddr_rank_mode) {
	case 0x1:
		rank = GROUP_RANK(RANK1, RANK1);
		break;
	case 0x2:
		rank = GROUP_RANK(RANK2, RANK2);
		break;
	case 0x3:
		rank = GROUP_RANK(RANK1, RANK2);
		break;
	default:
		pr_err("unknow rank type for DDR init\n");
		return -1;
	}

	reg_val = top_reg_read(bmdi, REG_TOP_SOFT_RST0);
	reg_val |= BIT_MASK_TOP_SOFT_RST0_DDRC;
	top_reg_write(bmdi, REG_TOP_SOFT_RST0, reg_val);

#ifdef DDR_INTERLEAVE_ENABLE
	// interleave is enabled by default
#else
	//disable 4K interleave
	bm1684_disable_ddr_interleave(bmdi);
	pr_info("LPDDR interleave disabled\n");
#endif

	pll_setting(bmdi, freq);

start:
	dwc_pre_setting(bmdi, GROUP0_RANK(rank), freq);
	ddr_group_init(bmdi, GROUP0);

	if (GROUP0_RANK(rank) != GROUP1_RANK(rank))
		dwc_pre_setting(bmdi, GROUP1_RANK(rank), freq);
	ddr_group_init(bmdi, GROUP1);
	if (bmdi->cinfo.ddr_failmap) {
		bmdi->cinfo.ddr_failmap = 0;
		fail_count++;
		pr_err("Lpddr train %d times still fail, retry!\n", fail_count);
		if (fail_count > 20) {
			pr_err("lpddr train fail\n");
			return -1;
		}
		goto start;
	}

	dwc_phy_enter_mission(bmdi);
	pr_info("lpddr%s(rank: %d + %d, mode: %s) init done\n",
		(bmdi->boot_info.ddr_mode & 0xffff) == LPDDR4 ? "4" : "4x",
		GROUP0_RANK(rank), GROUP1_RANK(rank),
		chip_mode == NORMAL_MODE ? "normal 2400MHz":"fast 4000MHz");
	msleep(1000);

	if (bmdi->boot_info.ddr_ecc_enable == 0x1) {
		ddr_memset_by_cdma(bmdi, 0x100000000,
			(bmdi->boot_info.ddr_0a_size + bmdi->boot_info.ddr_0b_size)/8*7 + 0x100000000);
		ddr_memset_by_cdma(bmdi, 0x300000000, (bmdi->boot_info.ddr_1_size)/8*7 + 0x300000000);
		ddr_memset_by_cdma(bmdi, 0x400000000, (bmdi->boot_info.ddr_1_size)/8*7 + 0x400000000);
		pr_info("cdma write ddr for ecc done\n");
	}

	return 0;
}
