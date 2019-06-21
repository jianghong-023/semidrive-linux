/*
 * kunlun_drm_crtc.c
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#include <drm/drm.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_of.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>

#include "kunlun_drm_crtc.h"
#include "kunlun_drm_dc_reg.h"
#include "kunlun_drm_dp_reg.h"
#include "kunlun_drm_drv.h"
#include "kunlun_drm_gem.h"

extern const struct kunlun_plane_data kunlun_dc_planes[];
extern const struct kunlun_plane_data kunlun_dp_planes[];

static const struct kunlun_irq_data dc_irq = {
	.int_mask = DU_REG(DC_INT_MSK, DC_INT_MASK, 0),
	.int_stat = DU_REG(DC_INT_ST, DC_INT_MASK, 0),
	.safe_int_mask = DU_REG(DC_SAFE_INT_MSK, DC_INT_MASK, 0),
	.rdma_int_mask = DU_REG(DC_RDMA_INT_MSK, DC_RDMA_INT_MASK, 0),
	.rdma_int_stat = DU_REG(DC_RDMA_INT_ST, DC_RDMA_INT_MASK, 0),
	.mlc_int_mask = DU_REG(MLC_INT_MSK, MLC_INT_MSK_MASK, 0),
	.mlc_int_mask = DU_REG(MLC_INT_ST, MLC_INT_ST_MASK, 0),
	.crc32_int_mask = DU_REG(CRC32_INT_MSK, CRC32_INT_MASK, 0),
	.crc32_int_mask = DU_REG(CRC32_INT_ST, CRC32_INT_MASK, 0),
};

static const struct kunlun_ctrl_data dc_ctrl = {
	.u.dc = {
		.reset = DU_REG(DC_CTRL, DC_CTRL_SW_RST_MASK, DC_CTRL_SW_RST_SHIFT),
		.urc = DU_REG(DC_CTRL, DC_CTRL_URC_MASK, DC_CTRL_URC_SHIFT),
		.mlc_dm = DU_REG(DC_CTRL, DC_CTRL_MLC_DM_MASK, DC_CTRL_MLC_DM_SHIFT),
		.ms = DU_REG(DC_CTRL, DC_CTRL_MS_MODE_MASK, DC_CTRL_MS_MODE_SHIFT),
		.sf = DU_REG(DC_CTRL, DC_CTRL_SF_MODE_MASK, DC_CTRL_SF_MODE_SHIFT),
		.crc_trig = DU_REG(DC_FLC_CTRL, CRC32_TRIG_MASK, CRC32_TRIG_SHIFT),
	},
	.di_trig = DU_REG(DC_FLC_CTRL, DI_TRIG_MASK, DI_TRIG_SHIFT),
	.flc_trig = DU_REG(DC_FLC_CTRL, FLC_TRIG_MASK, FLC_TRIG_SHIFT),
	.force_trig = DU_REG(DC_FLC_UPDATE, UPDATE_FORCE_MASK, UPDATE_FORCE_SHIFT),
	.sdma_en = DU_REG(SDMA_CTRL, SDMA_EN_MASK, SDMA_EN_SHIFT),
};

static const struct kunlun_rdma_channel rdma_chn = {
	.wml = DU_REG(RDMA_DFIFO_WML, RDMA_DFIFO_WML_MASK, RDMA_DFIFO_WML_SHIFT),
	.d_dep = DU_REG(RDMA_DFIFO_DEPTH, DFIFO_DEPTH_MASK, DFIFO_DEPTH_SHIFT),
	.c_dep = DU_REG(RDMA_CFIFO_DEPTH, CFIFO_DEPTH_MASK, CFIFO_DEPTH_SHIFT),
	.sche = DU_REG(RDMA_CH_PRIO, PRIO_SCHE_MASK, PRIO_SCHE_SHIFT),
	.prio1 = DU_REG(RDMA_CH_PRIO, PRIO_P1_MASK, PRIO_P1_SHIFT),
	.prio0 = DU_REG(RDMA_CH_PRIO, PRIO_P0_MASK, PRIO_P0_SHIFT),
	.burst_mode = DU_REG(RDMA_BURST, BURST_MODE_MASK, BURST_MODE_SHIFT),
	.burst_len = DU_REG(RDMA_BURST, BURST_LEN_MASK, BURST_LEN_SHIFT),
	.axi_user = DU_REG(RDMA_AXI_USER, AXI_USER_MASK, AXI_USER_SHIFT),
	.axi_prot = DU_REG(RDMA_AXI_CTRL, AXI_CTRL_PORT_MASK, AXI_CTRL_PORT_SHIFT),
	.axi_cache = DU_REG(RDMA_AXI_CTRL, AXI_CTRL_CACHE_MASK,
			AXI_CTRL_CACHE_SHIFT),
};

static const struct kunlun_rdma_data dc_rdma = {
	.num_chan = RDMA_DC_CHN_COUNT,
	.chan = &rdma_chn,
	.cfg_load = DU_REG(DC_RDMA_CTRL, CTRL_CFG_LOAD_MASK, CTRL_CFG_LOAD_SHIFT),
	.arb_sel = DU_REG(DC_RDMA_CTRL, CTRL_ARB_SEL_MASK, CTRL_ARB_SEL_SHIFT),
	.dfifo_full = DU_REG(DC_RDMA_DFIFO_FULL, RDMA_DC_CH_MASK, 0),
	.dfifo_empty = DU_REG(DC_RDMA_DFIFO_EMPTY, RDMA_DC_CH_MASK, 0),
	.cfifo_full = DU_REG(DC_RDMA_CFIFO_FULL, RDMA_DC_CH_MASK, 0),
	.cfifo_empty = DU_REG(DC_RDMA_CFIFO_EMPTY, RDMA_DC_CH_MASK, 0),
	.ch_idle = DU_REG(DC_RDMA_CH_IDLE, RDMA_DC_CH_MASK, 0),
};

static const struct kunlun_mlc_layer mlc_layer = {
	.prot_val = DU_REG(MLC_SF_CTRL, PROT_VAL_MASK, PROT_VAL_SHIFT),
	.vpos_prot_en = DU_REG(MLC_SF_CTRL, VPOS_PROT_EN_MASK, VPOS_PROT_EN_SHIFT),
	.slowdown_en = DU_REG(MLC_SF_CTRL, SLOWDOWN_EN_MASK, SLOWDOWN_EN_SHIFT),
	.aflu_psel = DU_REG(MLC_SF_CTRL, AFLU_PSEL_MASK, AFLU_PSEL_SHIFT),
	.aflu_en = DU_REG(MLC_SF_CTRL, AFLU_EN_MASK, AFLU_EN_SHIFT),
	.ckey_en = DU_REG(MLC_SF_CTRL, CKEY_EN_MASK, CKEY_EN_SHIFT),
	.g_alpha_en = DU_REG(MLC_SF_CTRL, G_ALPHA_EN_MASK, G_ALPHA_EN_SHIFT),
	.crop_en = DU_REG(MLC_SF_CTRL, CROP_EN_MASK, CROP_EN_SHIFT),
	.sf_en = DU_REG(MLC_SF_CTRL, MLC_SF_EN_MASK, MLC_SF_EN_SHIFT),
	.h_pos = DU_REG(MLC_SF_H_SPOS, SPOS_H_MASK, SPOS_H_SHIFT),
	.v_pos = DU_REG(MLC_SF_V_SPOS, SPOS_V_MASK, SPOS_V_SHIFT),
	.h_size = DU_REG(MLC_SF_SIZE, SIZE_H_MASK, SIZE_H_SHIFT),
	.v_size = DU_REG(MLC_SF_SIZE, SIZE_V_MASK, SIZE_V_SHIFT),
	.crop_h_start = DU_REG(MLC_SF_CROP_H_POS, CROP_START_MASK,
			CROP_START_SHIFT),
	.crop_h_end = DU_REG(MLC_SF_CROP_H_POS, CROP_END_MASK, CROP_END_SHIFT),
	.crop_v_start = DU_REG(MLC_SF_CROP_V_POS, CROP_START_MASK,
			CROP_START_SHIFT),
	.crop_v_end = DU_REG(MLC_SF_CROP_V_POS, CROP_END_MASK, CROP_END_SHIFT),
	.g_alpha = DU_REG(MLC_SF_G_ALPHA, G_ALPHA_A_MASK, G_ALPHA_A_SHIFT),
	.ckey_alpha = DU_REG(MLC_SF_CKEY_ALPHA, CKEY_ALPHA_A_MASK,
			CKEY_ALPHA_A_SHIFT),
	.ckey_r_up = DU_REG(MLC_SF_CKEY_R_LV, CKEY_LV_UP_MASK, CKEY_LV_UP_SHIFT),
	.ckey_r_dn = DU_REG(MLC_SF_CKEY_R_LV, CKEY_LV_DN_MASK, CKEY_LV_DN_SHIFT),
	.ckey_g_up = DU_REG(MLC_SF_CKEY_G_LV, CKEY_LV_UP_MASK, CKEY_LV_UP_SHIFT),
	.ckey_g_dn = DU_REG(MLC_SF_CKEY_G_LV, CKEY_LV_DN_MASK, CKEY_LV_DN_SHIFT),
	.ckey_b_up = DU_REG(MLC_SF_CKEY_B_LV, CKEY_LV_UP_MASK, CKEY_LV_UP_SHIFT),
	.ckey_b_dn = DU_REG(MLC_SF_CKEY_B_LV, CKEY_LV_DN_MASK, CKEY_LV_DN_SHIFT),
	.aflu_time = DU_REG(MLC_SF_AFLU_TIME, AFLU_TIMER_MASK, AFLU_TIMER_SHIFT),
};

static const struct kunlun_mlc_path mlc_path = {
	.alpha_bld_idx = DU_REG(MLC_PATH_CTRL, ALPHA_BLD_IDX_MASK,
			ALPHA_BLD_IDX_SHIFT),
	.layer_out_idx = DU_REG(MLC_PATH_CTRL, LAYER_OUT_IDX_MASK,
			LAYER_OUT_IDX_SHIFT),
};

static const struct kunlun_mlc_bg mlc_bg = {
	.bg_alpha = DU_REG(MLC_BG_CTRL, BG_A_MASK, BG_A_SHIFT),
	.aflu_en = DU_REG(MLC_BG_CTRL, BG_AFLU_EN_MASK, BG_AFLU_EN_SHIFT),
	.fstart_sel = DU_REG(MLC_BG_CTRL, FSTART_SEL_MASK, FSTART_SEL_SHIFT),
	.bg_a_sel = DU_REG(MLC_BG_CTRL, BG_A_SEL_MASK, BG_A_SEL_SHIFT),
	.bg_en = DU_REG(MLC_BG_CTRL, BG_EN_MASK, BG_EN_SHIFT),
	.alpha_bypass = DU_REG(MLC_BG_CTRL, ALPHA_BLD_BYPS_MASK,
			ALPHA_BLD_BYPS_SHIFT),
	.r_color = DU_REG(MLC_BG_COLOR, BG_COLOR_R_MASK, BG_COLOR_R_SHIFT),
	.g_color = DU_REG(MLC_BG_COLOR, BG_COLOR_G_MASK, BG_COLOR_G_SHIFT),
	.b_color = DU_REG(MLC_BG_COLOR, BG_COLOR_B_MASK, BG_COLOR_B_SHIFT),
	.aflu_time = DU_REG(MLC_BG_AFLU_TIME, BG_AFLU_TIMER_MASK,
			BG_AFLU_TIMER_SHIFT),
};

static const struct kunlun_mlc_data mlc_data = {
	.num_layer = MLC_LAYER_COUNT,
	.layer = &mlc_layer,
	.num_path = MLC_PATH_COUNT,
	.path = &mlc_path,
	.bg = &mlc_bg,
	.canvas_r = DU_REG(MLC_CANVAS_COLOR, CANVAS_COLOR_R_MASK,
			CANVAS_COLOR_R_SHIFT),
	.canvas_g = DU_REG(MLC_CANVAS_COLOR, CANVAS_COLOR_G_MASK,
			CANVAS_COLOR_G_SHIFT),
	.canvas_b = DU_REG(MLC_CANVAS_COLOR, CANVAS_COLOR_B_MASK,
			CANVAS_COLOR_B_SHIFT),
	.ratio = DU_REG(MLC_CLK_RATIO, CLK_RATIO_MASK, CLK_RATIO_SHIFT),
};

static const struct kunlun_tcon_kick_layer kick_layer = {
	.y_color = DU_REG(TCON_LAYER_KICK_COOR, LAYER_KICK_Y_MASK,
			LAYER_KICK_Y_SHIFT),
	.x_color = DU_REG(TCON_LAYER_KICK_COOR, LAYER_KICK_X_MASK,
			LAYER_KICK_X_SHIFT),
	.enable = DU_REG(TCON_LAYER_KICK_EN, LAYER_KICK_EN_MASK,
			LAYER_KICK_EN_SHIFT),
};

static const struct kunlun_tcon_csc tcon_csc = {
	.alpha = DU_REG(DC_CSC_CTRL, CSC_ALPHA_MASK, CSC_ALPHA_SHIFT),
	.sbup_conv = DU_REG(DC_CSC_CTRL, CSC_SBUP_CONV_MASK, CSC_SBUP_CONV_SHIFT),
	.bypass = DU_REG(DC_CSC_CTRL, CSC_BYPASS_MASK, CSC_BYPASS_SHIFT),
	.coef_a01 = DU_REG(DC_CSC_COEF1, COEF1_A01_MASK, COEF1_A01_SHIFT),
	.coef_a00 = DU_REG(DC_CSC_COEF1, COEF1_A00_MASK, COEF1_A00_SHIFT),
	.coef_a10 = DU_REG(DC_CSC_COEF2, COEF2_A10_MASK, COEF2_A10_SHIFT),
	.coef_a02 = DU_REG(DC_CSC_COEF2, COEF2_A02_MASK, COEF2_A02_SHIFT),
	.coef_a12 = DU_REG(DC_CSC_COEF3, COEF3_A12_MASK, COEF3_A12_SHIFT),
	.coef_a11 = DU_REG(DC_CSC_COEF3, COEF3_A11_MASK, COEF3_A11_SHIFT),
	.coef_a21 = DU_REG(DC_CSC_COEF4, COEF4_A21_MASK, COEF4_A21_SHIFT),
	.coef_a20 = DU_REG(DC_CSC_COEF4, COEF4_A20_MASK, COEF4_A20_SHIFT),
	.coef_b0 = DU_REG(DC_CSC_COEF5, COEF5_B0_MASK, COEF5_B0_SHIFT),
	.coef_a22 = DU_REG(DC_CSC_COEF5, COEF5_A22_MASK, COEF5_A22_SHIFT),
	.coef_b2 = DU_REG(DC_CSC_COEF6, COEF6_B2_MASK, COEF6_B2_SHIFT),
	.coef_b1 = DU_REG(DC_CSC_COEF6, COEF6_B1_MASK, COEF6_B1_SHIFT),
	.coef_c1 = DU_REG(DC_CSC_COEF7, COEF7_C1_MASK, COEF7_C1_SHIFT),
	.coef_c0 = DU_REG(DC_CSC_COEF7, COEF7_C0_MASK, COEF7_C0_SHIFT),
	.coef_c2 = DU_REG(DC_CSC_COEF8, COEF8_C2_MASK, COEF8_C2_SHIFT),
};

static const struct kunlun_tcon_gamma tcon_gamma = {
	.apb = DU_REG(GAMMA_CTRL, APB_RD_TO_MASK, APB_RD_TO_SHIFT),
	.bypass = DU_REG(GAMMA_CTRL, GMMA_BYPASS_MASK, GMMA_BYPASS_SHIFT),
};

static const struct kunlun_tcon_dither tcon_dither = {
	.v = DU_REG(DITHER_CTRL, V_DEP_MASK, V_DEP_SHIFT),
	.u = DU_REG(DITHER_CTRL, U_DEP_MASK, U_DEP_SHIFT),
	.y = DU_REG(DITHER_CTRL, Y_DEP_MASK, Y_DEP_SHIFT),
	.mode_12b = DU_REG(DITHER_CTRL, MODE_12_MASK, MODE_12_SHIFT),
	.spa_mode = DU_REG(DITHER_CTRL, SPA_LSB_EXP_MASK, SPA_LSB_EXP_SHIFT),
	.spa_1st = DU_REG(DITHER_CTRL, SPA_1ST_MASK, SPA_1ST_SHIFT),
	.spa_en = DU_REG(DITHER_CTRL, SPA_EN_MASK, SPA_EN_SHIFT),
	.tem_en = DU_REG(DITHER_CTRL, TEM_EN_MASK, TEM_EN_SHIFT),
	.bypass = DU_REG(DITHER_CTRL, DITHER_BYPASS_MASK, DITHER_BYPASS_SHIFT),
};

static const struct kunlun_tcon_crc32 tcon_crc32 = {
	.vsync_pol = DU_REG(CRC32_CTRL, CRC_VSYNC_POL_MASK, CRC_VSYNC_POL_SHIFT),
	.hsync_pol = DU_REG(CRC32_CTRL, CRC_HSYNC_POL_MASK, CRC_HSYNC_POL_SHIFT),
	.de_pol = DU_REG(CRC32_CTRL, CRC_DATA_EN_POL_MASK, CRC_DATA_EN_POL_SHIFT),
	.enable = DU_REG(CRC32_CTRL, GLOBAL_ENABLE_MASK, GLOBAL_ENABLE_SHIFT),
	.block_en = DU_REG(CRC32_BLOCK_CTRL0, BLOCK_ENABLE_MASK,
			BLOCK_ENABLE_SHIFT),
	.lock = DU_REG(CRC32_BLOCK_CTRL0, BLOCK_LOCK_MASK, BLOCK_LOCK_SHIFT),
	.start_y = DU_REG(CRC32_BLOCK_CTRL0, POS_START_Y_MASK, POS_START_Y_SHIFT),
	.start_x = DU_REG(CRC32_BLOCK_CTRL0, POS_START_X_MASK, POS_START_X_SHIFT),
	.end_y = DU_REG(CRC32_BLOCK_CTRL1, POS_END_Y_MASK, POS_END_Y_SHIFT),
	.end_x = DU_REG(CRC32_BLOCK_CTRL1, POS_END_X_MASK, POS_END_X_SHIFT),
	.expect = DU_REG(CRC32_BLOCK_EXPECT_DATA, EXPECT_DATA_MASK,
			EXPECT_DATA_SHIFT),
	.result = DU_REG(CRC32_BLOCK_EXPECT_DATA, RESULT_DATA_MASK,
			RESULT_DATA_SHIFT),
};

static const struct kunlun_tcon_data tcon_data = {
	.num_klayer = KICK_LAYER_COUNT,
	.klayer = &kick_layer,
	.num_csc = DC_CSC_COUNT,
	.csc = &tcon_csc,
	.gamma = &tcon_gamma,
	.dither = &tcon_dither,
	.num_crc32 = CRC_BLK_COUNT,
	.crc32 = &tcon_crc32,
	.trig = DU_REG(DC_FLC_CTRL, TCON_TRIG_MASK, TCON_TRIG_SHIFT),
	.hact = DU_REG(TCON_H_PARA_1, HACT_MASK, HACT_SHIFT),
	.htot = DU_REG(TCON_H_PARA_1, HTOL_MASK, HTOL_SHIFT),
	.hbp = DU_REG(TCON_H_PARA_2, HSBP_MASK, HSBP_SHIFT),
	.hsync = DU_REG(TCON_H_PARA_2, HSYNC_MASK, HSYNC_SHIFT),
	.vact = DU_REG(TCON_V_PARA_1, VACT_MASK, VACT_SHIFT),
	.vtot = DU_REG(TCON_V_PARA_1, VTOL_MASK, VTOL_SHIFT),
	.vbp = DU_REG(TCON_V_PARA_2, VSBP_MASK, VSBP_SHIFT),
	.vsync = DU_REG(TCON_V_PARA_2, VSYNC_MASK, VSYNC_SHIFT),
	.pix_scr = DU_REG(TCON_CTRL, PIX_SCR_MASK, PIX_SCR_SHIFT),
	.clk_en = DU_REG(TCON_CTRL, DSP_CLK_EN_MASK, DSP_CLK_EN_SHIFT),
	.clk_pol = DU_REG(TCON_CTRL, DSP_CLK_POL_MASK, DSP_CLK_POL_SHIFT),
	.de_pol = DU_REG(TCON_CTRL, DE_POL_MASK, DE_POL_SHITF),
	.vsync_pol = DU_REG(TCON_CTRL, VSYNC_POL_MASK, VSYNC_POL_SHIFT),
	.hsync_pol = DU_REG(TCON_CTRL, HSYNC_POL_MASK, HSYNC_POL_SHIFT),
	.enable = DU_REG(TCON_CTRL, EN_MASK, EN_SHIFT),
};

static const struct kunlun_irq_data dp_irq = {
	.int_mask = DU_REG(DP_INT_MSK, DP_INT_MASK, 0),
	.int_stat = DU_REG(DP_INT_ST, DP_INT_MASK, 0),
	.rdma_int_mask = DU_REG(DP_RDMA_INT_MSK, DP_RDMA_INT_MASK, 0),
	.rdma_int_stat = DU_REG(DP_RDMA_INT_ST, DP_RDMA_INT_MASK, 0),
	.mlc_int_mask = DU_REG(MLC_INT_MSK, MLC_INT_MSK_MASK, 0),
	.mlc_int_mask = DU_REG(MLC_INT_ST, MLC_INT_ST_MASK, 0),
};

static const struct kunlun_ctrl_data dp_ctrl = {
	.u.dp = {
		.reset = DU_REG(DP_CTRL, DP_CTRL_SW_RST_MASK, DP_CTRL_SW_RST_SHIFT),
		.re_type = DU_REG(DP_CTRL, DP_RE_RAM_TYPE_MASK, DP_RE_RAM_TYPE_SHIFT),
		.mlc_dm = DU_REG(DP_CTRL, DP_CTRL_MLC_DM_MASK, DP_CTRL_MLC_DM_SHIFT),
		.flc_kick = DU_REG(DP_CTRL, DP_FLC_KICK_SEL_MASK, DP_FLC_KICK_SEL_SHIFT),
		.enable = DU_REG(DP_CTRL, DP_EN_MASK, DP_EN_SHIFT),
		.h_size = DU_REG(DP_SIZE, DP_OUT_H_MASK, DP_OUT_H_SHIFT),
		.v_size = DU_REG(DP_SIZE, DP_OUT_V_MASK, DP_OUT_V_SHIFT),
	},
	.di_trig = DU_REG(DP_FLC_CTRL, DI_TRIG_MASK, DI_TRIG_SHIFT),
	.flc_trig = DU_REG(DP_FLC_CTRL, FLC_TRIG_MASK, FLC_TRIG_SHIFT),
	.force_trig = DU_REG(DP_FLC_UPDATE, UPDATE_FORCE_MASK, UPDATE_FORCE_SHIFT),
	.sdma_en = DU_REG(SDMA_CTRL, SDMA_EN_MASK, SDMA_EN_SHIFT),
};

static const struct kunlun_rdma_data dp_rdma = {
	.num_chan = RDMA_DP_CHN_COUNT,
	.chan = &rdma_chn,
	.cfg_load = DU_REG(DP_RDMA_CTRL, CTRL_CFG_LOAD_MASK, CTRL_CFG_LOAD_SHIFT),
	.arb_sel = DU_REG(DP_RDMA_CTRL, CTRL_ARB_SEL_MASK, CTRL_ARB_SEL_SHIFT),
	.dfifo_full = DU_REG(DP_RDMA_DFIFO_FULL, RDMA_DC_CH_MASK, 0),
	.dfifo_empty = DU_REG(DP_RDMA_DFIFO_EMPTY, RDMA_DC_CH_MASK, 0),
	.cfifo_full = DU_REG(DP_RDMA_CFIFO_FULL, RDMA_DC_CH_MASK, 0),
	.cfifo_empty = DU_REG(DP_RDMA_CFIFO_EMPTY, RDMA_DC_CH_MASK, 0),
	.ch_idle = DU_REG(DP_RDMA_CH_IDLE, RDMA_DC_CH_MASK, 0),
};

static const struct kunlun_fbdc_ctrl fbdc_ctrl = {
	.sw_rst = DU_REG(FBDC_CTRL, FBDC_CTRL_SW_RST_MASK, FBDC_CTRL_SW_RST_SHIFT),
	.inva_sw_en = DU_REG(FBDC_CTRL, FBDC_CTRL_INVA_SW_EN_MASK, FBDC_CTRL_INVA_SW_EN_SHIFT),
	.hdr_bypass = DU_REG(FBDC_CTRL, FBDC_CTRL_HDR_BYPASS_MASK, FBDC_CTRL_HDR_BYPASS_SHIFT),
	.mode_v3_1_en = DU_REG(FBDC_CTRL, FBDC_CTRL_MODE_V3_1_EN_MASK, FBDC_CTRL_MODE_V3_1_EN_SHIFT),
	.en = DU_REG(FBDC_CTRL, FBDC_CTRL_EN_MASK, FBDC_CTRL_EN_SHIFT),
};

static const struct kunlun_fbdc_cr_inval fbdc_cr_inval = {
	.requester_i = DU_REG(FBDC_CR_INVAL, FBDC_CR_INVAL_REQUESTER_I_MASK, FBDC_CR_INVAL_REQUESTER_I_SHIFT),
	.context_i = DU_REG(FBDC_CR_INVAL, FBDC_CR_INVAL_CONTEXT_I_MASK, FBDC_CR_INVAL_CONTEXT_I_SHIFT),
	.pengding_i = DU_REG(FBDC_CR_INVAL, FBDC_CR_INVAL_PENDING_I_MASK, FBDC_CR_INVAL_PENDING_I_SHIFT),
	.notify = DU_REG(FBDC_CR_INVAL, FBDC_CR_INVAL_NOTIFY_MASK, FBDC_CR_INVAL_NOTIFY_SHIFT),
	.override = DU_REG(FBDC_CR_INVAL, FBDC_CR_INVAL_OVERRIDE_MASK, FBDC_CR_INVAL_OVERRIDE_SHIFT),
	.requester_o = DU_REG(FBDC_CR_INVAL, FBDC_CR_INVAL_REQUESTER_O_MASK, FBDC_CR_INVAL_REQUESTER_O_SHIFT),
	.context_o = DU_REG(FBDC_CR_INVAL, FBDC_CR_INVAL_CONTEXT_O_MASK, FBDC_CR_INVAL_CONTEXT_O_SHIFT),
	.pengding_o = DU_REG(FBDC_CR_INVAL, FBDC_CR_INVAL_PENDING_O_MASK, FBDC_CR_INVAL_PENDING_O_SHIFT),
};

static const struct kunlun_fbdc_cr_val fbdc_cr_val = {
	.uv0 = DU_REG(FBDC_CR_VAL_0, FBDC_CR_VAL_0_UV0_MASK, FBDC_CR_VAL_0_UV0_SHIFT),
	.y0 = DU_REG(FBDC_CR_VAL_0, FBDC_CR_VAL_0_Y0_MASK, FBDC_CR_VAL_0_Y0_SHIFT),
	.uv1 = DU_REG(FBDC_CR_VAL_1, FBDC_CR_VAL_1_UV1_MASK, FBDC_CR_VAL_1_UV1_SHIFT),
	.y1 = DU_REG(FBDC_CR_VAL_1, FBDC_CR_VAL_1_Y1_MASK, FBDC_CR_VAL_1_Y1_SHIFT),
};

static const struct kunlun_fbdc_cr_ch0123 fbdc_cr_ch0123 = {
	.val0 = DU_REG(FBDC_CR_CH0123_0, FBDC_CR_CH0123_0_VAL0_MASK, FBDC_CR_CH0123_0_VAL0_SHIFT),
	.val1 = DU_REG(FBDC_CR_CH0123_1, FBDC_CR_CH0123_1_VAL1_MASK, FBDC_CR_CH0123_1_VAL1_SHIFT),
};

static const struct kunlun_fbdc_filter fbdc_filter = {
	.clear_3 = DU_REG(FBDC_CR_FILTER_CTRL, FBDC_CR_FILTER_CTRL_CLEAR_3_MASK, FBDC_CR_FILTER_CTRL_CLEAR_3_SHIFT),
	.clear_2 = DU_REG(FBDC_CR_FILTER_CTRL, FBDC_CR_FILTER_CTRL_CLEAR_2_MASK, FBDC_CR_FILTER_CTRL_CLEAR_2_SHIFT),
	.clear_1 = DU_REG(FBDC_CR_FILTER_CTRL, FBDC_CR_FILTER_CTRL_CLEAR_1_MASK, FBDC_CR_FILTER_CTRL_CLEAR_1_SHIFT),
	.clear_0 = DU_REG(FBDC_CR_FILTER_CTRL, FBDC_CR_FILTER_CTRL_CLEAR_0_MASK, FBDC_CR_FILTER_CTRL_CLEAR_0_SHIFT),
	.en = DU_REG(FBDC_CR_FILTER_CTRL, FBDC_CR_FILTER_CTRL_EN_MASK, FBDC_CR_FILTER_CTRL_EN_SHIFT),
	.status = DU_REG(FBDC_CR_FILTER_STATUS, FBDC_CR_FILTER_STATUS_MASK, FBDC_CR_FILTER_STATUS_SHIFT),
};

static const struct kunlun_fbdc_cr_core fbdc_cr_core = {
	.id_b = DU_REG(FBDC_CR_CORE_ID_0, FBDC_CR_CORE_ID_0_B_MASK, FBDC_CR_CORE_ID_0_B_SHIFT),
	.id_p = DU_REG(FBDC_CR_CORE_ID_0, FBDC_CR_CORE_ID_0_P_MASK, FBDC_CR_CORE_ID_0_P_SHIFT),
	.id_n = DU_REG(FBDC_CR_CORE_ID_1, FBDC_CR_CORE_ID_1_N_MASK, FBDC_CR_CORE_ID_1_N_SHIFT),
	.id_v = DU_REG(FBDC_CR_CORE_ID_1, FBDC_CR_CORE_ID_1_V_MASK, FBDC_CR_CORE_ID_1_V_SHIFT),
	.id_c = DU_REG(FBDC_CR_CORE_ID_2, FBDC_CR_CORE_ID_2_C_MASK, FBDC_CR_CORE_ID_2_C_SHIFT),
	.ip_clist = DU_REG(FBDC_CR_CORE_IP, FBDC_CR_CORE_IP_CHANGELIST_MASK, FBDC_CR_CORE_IP_CHANGELIST_SHIFT),
};

static const struct kunlun_fbdc_data fbdc_data = {
	.ctrl = &fbdc_ctrl,
	.cr_inval = &fbdc_cr_inval,
	.cr_val = &fbdc_cr_val,
	.cr_ch0123 = &fbdc_cr_ch0123,
	.filter = &fbdc_filter,
	.cr_core = &fbdc_cr_core,
};

static void kunlun_crtc_handle_vblank(struct kunlun_crtc *kcrtc);

static void kunlun_mlc_disable(struct kunlun_crtc *kcrtc)
{
	const struct kunlun_mlc_data *mlc = kcrtc->data->dc_data->mlc;
	void __iomem *regs = kcrtc->dc_regs;
	unsigned int i;

	for(i = 0; i < mlc->num_layer; i++)
		DU_MLC_LAYER_SET(regs, mlc, i, sf_en, 0);

}

static int kunlun_mlc_update_plane(struct kunlun_crtc *kcrtc, unsigned int mask)
{
	const struct kunlun_crtc_du_data *dc_data = kcrtc->data->dc_data;
	const struct kunlun_crtc_du_data *dp_data = kcrtc->data->dp_data;
	const struct kunlun_mlc_data *dc_mlc = dc_data->mlc;
	const struct kunlun_mlc_data *dp_mlc = dp_data->mlc;
	void __iomem *regs;
	unsigned int nums = 0;
	unsigned int dp_mask[2] = {0, 0};
	int i, j;

	if(!mask)
		return -EINVAL;

	if (kcrtc->num_dp_planes) {
		regs = kcrtc->dp_regs[0];
		for (i = 0; i < dp_data->num_planes; i++) {
			if (mask & (1 << i)) {
				DU_MLC_PATH_SET(regs, dp_mlc, i + 1 , alpha_bld_idx, i + 1);
				DU_MLC_PATH_SET(regs, dp_mlc, i + 1, layer_out_idx, i + 1);
			} else {
				DU_MLC_PATH_SET(regs, dp_mlc, i + 1, alpha_bld_idx, 0xF);
				DU_MLC_PATH_SET(regs, dp_mlc, i + 1, layer_out_idx, 0xF);
			}
			nums += (mask & (1 << i)) ? 1 : 0;
		}
		if (nums)
			dp_mask[0] = 1;

		DU_REG_SET(regs, dp_data->ctrl, di_trig, 1);
	}

	if (kcrtc->num_dp_planes > dp_data->num_planes) {
		regs = kcrtc->dp_regs[1];
		for (i = dp_data->num_planes, j = 0; i < kcrtc->num_dp_planes; i++, j++) {
			if (mask & (1 << i)) {
				DU_MLC_PATH_SET(regs, dp_mlc, j + 1, alpha_bld_idx, j + 1);
				DU_MLC_PATH_SET(regs, dp_mlc, j + 1, layer_out_idx, j + 1);
			} else {
				DU_MLC_PATH_SET(regs, dp_mlc, j + 1, alpha_bld_idx, 0xF);
				DU_MLC_PATH_SET(regs, dp_mlc, j + 1, layer_out_idx, 0xF);
			}
			nums += (mask & (1 << i)) ? 1 : 0;
		}
		if (nums)
			dp_mask[1] = 1;

		DU_REG_SET(regs, dp_data->ctrl, di_trig, 1);
	}

	for (i = kcrtc->num_dp_planes, j = 0; i < kcrtc->num_planes; i++, j++) {
		regs = kcrtc->dc_regs;
		if (mask & (1 << i)) {
			DU_MLC_PATH_SET(regs, dc_mlc, j + 1, alpha_bld_idx, j + 1);
			DU_MLC_PATH_SET(regs, dc_mlc, j + 1, layer_out_idx, j + 1);
		} else {
			DU_MLC_PATH_SET(regs, dc_mlc, j + 1, alpha_bld_idx, 0xF);
			DU_MLC_PATH_SET(regs, dc_mlc, j + 1, layer_out_idx, 0xF);
		}
	}

	if (kcrtc->dc_nums) {
		regs = kcrtc->dc_regs;

		if (dp_mask[0]) {
			DU_MLC_LAYER_SET(regs, dc_mlc, 2, sf_en, 1);
			DU_MLC_PATH_SET(regs, dc_mlc, 3, alpha_bld_idx, 3);
			DU_MLC_PATH_SET(regs, dc_mlc, 3, layer_out_idx, 3);
		} else {
			DU_MLC_LAYER_SET(regs, dc_mlc, 2, sf_en, 0);
			DU_MLC_PATH_SET(regs, dc_mlc, 3, alpha_bld_idx, 0xF);
			DU_MLC_PATH_SET(regs, dc_mlc, 3, layer_out_idx, 0xF);
		}

		if (dp_mask[1]) {
			DU_MLC_LAYER_SET(regs, dc_mlc, 3, sf_en, 1);
			DU_MLC_PATH_SET(regs, dc_mlc, 4, alpha_bld_idx, 4);
			DU_MLC_PATH_SET(regs, dc_mlc, 4, layer_out_idx, 4);
		} else {
			DU_MLC_LAYER_SET(regs, dc_mlc, 3, sf_en, 0);
			DU_MLC_PATH_SET(regs, dc_mlc, 4, alpha_bld_idx, 0xF);
			DU_MLC_PATH_SET(regs, dc_mlc, 4, layer_out_idx, 0xF);
		}

		DU_REG_SET(regs, dc_data->ctrl, di_trig, 1);
	}

	return 0;
}

static int kunlun_mlc_update_trig(struct kunlun_crtc *kcrtc, unsigned int mask)
{
	const struct kunlun_crtc_du_data *dc_data = kcrtc->data->dc_data;
	const struct kunlun_crtc_du_data *dp_data = kcrtc->data->dp_data;
	void __iomem *regs;
	if (kcrtc->num_dp_planes) {
		regs = kcrtc->dp_regs[0];
		DU_REG_SET(regs, dp_data->ctrl, flc_trig, 1);
	}

	if (kcrtc->num_dp_planes > dp_data->num_planes) {
		regs = kcrtc->dp_regs[1];
		DU_REG_SET(regs, dp_data->ctrl, flc_trig, 1);
	}

	if (kcrtc->dc_nums) {
		regs = kcrtc->dc_regs;
		DU_REG_SET(regs, dc_data->ctrl, flc_trig, 1);
	}

	return 0;
}

static void kunlun_dc_sw_reset(struct kunlun_crtc *kcrtc,
		const struct kunlun_ctrl_data *ctrl)
{
	void __iomem *dc_regs = kcrtc->dc_regs;

//	DC_CTRL_SET(dc_regs, ctrl, reset, 1);
//	usleep_range(1, 2);
//	DC_CTRL_SET(dc_regs, ctrl, reset, 0);
	DC_CTRL_SET(dc_regs, ctrl, urc, 1);
}

static void kunlun_dc_irq_init(struct kunlun_crtc *kcrtc,
		const struct kunlun_irq_data *irq_data)
{
	void __iomem *dc_regs = kcrtc->dc_regs;

	DU_IRQ_CLR(dc_regs, irq_data, int_mask, 0xFFFFFFF);
	DU_IRQ_CLR(dc_regs, irq_data, safe_int_mask, 0xFFFFFFF);
	DU_IRQ_CLR(dc_regs, irq_data, rdma_int_mask, 0x7F);
	DU_IRQ_CLR(dc_regs, irq_data, mlc_int_mask, 0x1FFF);
	DU_IRQ_CLR(dc_regs, irq_data, crc32_int_mask, 0xFFFF);
}

static void kunlun_tcon_init(struct kunlun_crtc *kcrtc,
		const struct kunlun_tcon_data *tcon_data)
{

	void __iomem *dc_regs = kcrtc->dc_regs;
	const struct kunlun_tcon_csc *csc = tcon_data->csc;
	const struct kunlun_tcon_gamma *gamma = tcon_data->gamma;

	DU_REG_SET(dc_regs, csc, bypass, 1);
	DU_REG_SET(dc_regs, gamma, bypass, 1);
}

static void kunlun_dc_rdma_init(struct kunlun_crtc *kcrtc,
		const struct kunlun_rdma_data *rdma)
{
	void __iomem *dc_regs = kcrtc->dc_regs;
	const struct kunlun_irq_data *irq_data = kcrtc->data->dc_data->irq;
	const uint16_t wml_cfg[] = {0x50, 0x50, 0x18, 0x18};
	const uint16_t d_depth_cfg[] = {0x0d, 0x03, 0x03, 0x0d};
	const uint16_t c_depth_cfg[] = {0x06, 0x02, 0x02, 0x06};
	const uint8_t sche_cfg[] = {0x04, 0x01, 0x01, 0x04};
	const uint8_t p1_cfg[] = {0x30, 0x20, 0x20, 0x30};
	const uint8_t p0_cfg[] = {0x10, 0x08, 0x08, 0x10};
	const uint8_t burst_mode_cfg[] = {0x0, 0x0, 0x0, 0x0};
	const uint8_t burst_len_cfg[] = {0x04, 0x04, 0x04, 0x04};
	unsigned long flags;
	int i;

	for(i = 0; i < rdma->num_chan; i++) {
		DU_RDMA_CHAN_SET(dc_regs, rdma, i, wml, wml_cfg[i]);
		DU_RDMA_CHAN_SET(dc_regs, rdma, i, d_dep, d_depth_cfg[i]);
		DU_RDMA_CHAN_SET(dc_regs, rdma, i, c_dep, c_depth_cfg[i]);
		DU_RDMA_CHAN_SET(dc_regs, rdma, i, sche, sche_cfg[i]);
		DU_RDMA_CHAN_SET(dc_regs, rdma, i, prio1, p1_cfg[i]);
		DU_RDMA_CHAN_SET(dc_regs, rdma, i, prio0, p0_cfg[i]);
		DU_RDMA_CHAN_SET(dc_regs, rdma, i, burst_mode, burst_mode_cfg[i]);
		DU_RDMA_CHAN_SET(dc_regs, rdma, i, burst_len, burst_len_cfg[i]);
		spin_lock_irqsave(&kcrtc->dc_irq_lock, flags);
		DU_IRQ_SET(dc_regs, irq_data, rdma_int_mask, i, 0);
		spin_unlock_irqrestore(&kcrtc->dc_irq_lock, flags);
	}

	spin_lock_irqsave(&kcrtc->dc_irq_lock, flags);
//	DU_IRQ_SET(dc_regs, irq_data, int_mask, RDMA_SHIFT, 0);
	spin_unlock_irqrestore(&kcrtc->dc_irq_lock, flags);

	DU_REG_SET(dc_regs, rdma, cfg_load, 1);
	DU_REG_SET(dc_regs, rdma, arb_sel, 1);
}

static int kunlun_mlc_set_timings(struct kunlun_crtc *kcrtc,
		struct drm_display_mode *mode)
{
	const struct kunlun_mlc_data *mlc = kcrtc->data->dc_data->mlc;
	void __iomem *regs = kcrtc->dc_regs;
	unsigned int i;

	for(i = 0; i < mlc->num_layer; i++) {
		DU_MLC_TIMING_SET(regs, mlc, i, h_size, mode->hdisplay);
		DU_MLC_TIMING_SET(regs, mlc, i, v_size, mode->vdisplay);
	}

	return 0;
}

static int kunlun_tcon_set_timings(struct kunlun_crtc *kcrtc,
		struct drm_display_mode *mode, u32 bus_flags)
{
	const struct kunlun_tcon_data *tcon = kcrtc->data->dc_data->tcon;
	void __iomem *regs = kcrtc->dc_regs;
	unsigned int i, tmp;

	DU_TIMING_REG_SET(regs, tcon, hact, mode->hdisplay);
	DU_TIMING_REG_SET(regs, tcon, htot, mode->htotal);

	tmp = mode->htotal - mode->hsync_start;
	DU_TIMING_REG_SET(regs, tcon, hbp, tmp);
	tmp = mode->hsync_end - mode->hsync_start;
	DU_TIMING_REG_SET(regs, tcon, hsync, tmp);

	DU_TIMING_REG_SET(regs, tcon, vact, mode->vdisplay);
	DU_TIMING_REG_SET(regs, tcon, vtot, mode->vtotal);

	tmp = mode->vtotal - mode->vsync_start;
	DU_TIMING_REG_SET(regs, tcon, vbp, tmp);
	tmp = mode->vsync_end - mode->vsync_start;
	DU_TIMING_REG_SET(regs, tcon, vsync, tmp);

	for(i = 0; i < tcon->num_klayer; i++) {
		if(i < 2)
			tmp = mode->vdisplay + 4;
		else
			tmp = mode->vdisplay + 5;
		if(tmp > mode->vtotal - 1)
			tmp = mode->vtotal -1;
		DU_TCON_KLAYER_REG_SET(regs, tcon, i, y_color, tmp);

		tmp = mode->htotal / 2 + i;
		if(tmp > mode->htotal - 1)
			tmp = mode->htotal - 1;
		DU_TCON_KLAYER_REG_SET(regs, tcon, i, x_color, tmp);
		DU_TCON_KLAYER_REG_SET(regs, tcon, i, enable, 1);
	}

	if(bus_flags & DRM_BUS_FLAG_PIXDATA_NEGEDGE)
		DU_REG_SET(regs, tcon, clk_pol, 1);
	if(bus_flags & DRM_BUS_FLAG_DE_LOW)
		DU_REG_SET(regs, tcon, de_pol, 1);

	if(mode->flags & DRM_MODE_FLAG_NVSYNC)
		DU_REG_SET(regs, tcon, vsync_pol, 1);
	if(mode->flags & DRM_MODE_FLAG_NHSYNC)
		DU_REG_SET(regs, tcon, hsync_pol, 1);

	DU_REG_SET(regs, tcon, clk_en, 1);
//	DU_REG_SET(regs, tcon, enable, 1);

	DU_REG_SET(regs, tcon, trig, 1);

	return 0;
}

static void kunlun_dc_mlc_init(struct kunlun_crtc *kcrtc,
		const struct kunlun_mlc_data *mlc)
{
	void __iomem *dc_regs = kcrtc->dc_regs;
	const struct kunlun_irq_data *irq_data = kcrtc->data->dc_data->irq;
	const uint8_t prot_val[] = {0x07, 0x07, 0x07, 0x07};
	const uint32_t aflu_time[] = {0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF};
	const uint8_t alpha_bld[] = {0xF, 0xF, 0xF, 0xF, 0xF};
	const uint8_t layer_out[] = {0xF, 0xF, 0xF, 0xF, 0xF};
	unsigned long flags;
	unsigned int i;

	for(i = 0; i < mlc->num_layer; i++) {
		DU_MLC_LAYER_SET(dc_regs, mlc, i, prot_val, prot_val[i]);
		DU_MLC_LAYER_SET(dc_regs, mlc, i, h_pos, 0);
		DU_MLC_LAYER_SET(dc_regs, mlc, i, v_pos, 0);
		DU_MLC_LAYER_SET(dc_regs, mlc, i, aflu_time, aflu_time[i]);
		DU_MLC_LAYER_SET(dc_regs, mlc, i, g_alpha, 0xFF);
		DU_MLC_LAYER_SET(dc_regs, mlc, i, g_alpha_en, 1);
	}

	for(i = 0; i < mlc->num_path; i++) {
		DU_MLC_PATH_SET(dc_regs, mlc, i, alpha_bld_idx, alpha_bld[i]);
		DU_MLC_PATH_SET(dc_regs, mlc, i, layer_out_idx, layer_out[i]);
		spin_lock_irqsave(&kcrtc->dc_irq_lock, flags);
		DU_IRQ_SET(dc_regs, irq_data, mlc_int_mask, i + MLC_FLU_L_0_SHIFT, 0);
		DU_IRQ_SET(dc_regs, irq_data, mlc_int_mask, i + MLC_E_L_0_SHIFT, 0);
		spin_unlock_irqrestore(&kcrtc->dc_irq_lock, flags);
	}

//	DU_IRQ_SET(dc_regs, irq_data, int_mask, MLC_SHIFT, 0);

	DU_REG_SET(dc_regs, mlc, ratio, 0xFFFF);
}

static void __iomem *kunlun_get_dp_regs(struct kunlun_crtc *kcrtc, int index)
{
	if ((index < 0) || (index > 2)) {
		pr_err("No support index = %d\n", index);
		return NULL;
	}

	return kcrtc->dp_regs[index];
}

static void kunlun_dp_set_size(struct kunlun_crtc *kcrtc,
		uint32_t width, uint32_t height, int index)
{
	const struct kunlun_ctrl_data *ctrl = kcrtc->data->dp_data->ctrl;
	void __iomem *dp_regs = kcrtc->dp_regs[index];

	DP_CTRL_SET(dp_regs, ctrl, h_size, width - 1);
	DP_CTRL_SET(dp_regs, ctrl, v_size, height - 1);
}

static void kunlun_dp_sw_reset(struct kunlun_crtc *kcrtc,
		const struct kunlun_ctrl_data *ctrl, int index)
{
	void __iomem *dp_regs = kcrtc->dp_regs[index];

	//	DP_CTRL_SET(dp_regs, ctrl, reset, 1);
	//	usleep_range(1, 2);
	//	DP_CTRL_SET(dp_regs, ctrl, reset, 0);
}

static void kunlun_dp_irq_init(struct kunlun_crtc *kcrtc,
		const struct kunlun_irq_data *irq_data, int index)
{
	void __iomem *dp_regs = kcrtc->dp_regs[index];

	DU_IRQ_CLR(dp_regs, irq_data, int_mask, 0x7F);
	DU_IRQ_CLR(dp_regs, irq_data, rdma_int_mask, 0x7F);
	DU_IRQ_CLR(dp_regs, irq_data, mlc_int_mask, 0x1FFF);
}

static void kunlun_dp_ctrl_init(struct kunlun_crtc *kcrtc,
		const struct kunlun_ctrl_data *ctrl, int index)
{
	void __iomem *dp_regs = kcrtc->dp_regs[index];

	DP_CTRL_SET(dp_regs, ctrl, re_type, 1);
	DP_CTRL_SET(dp_regs, ctrl, enable, 1);
}

static void kunlun_dp_rdma_init(struct kunlun_crtc *kcrtc,
		const struct kunlun_rdma_data *rdma, int index)
{
	void __iomem *dp_regs = kcrtc->dp_regs[index];
	const struct kunlun_irq_data *irq_data = kcrtc->data->dp_data->irq;
	/*
	const uint16_t wml_cfg[] = {128, 64, 64, 128, 64, 64, 96, 96, 64};
	const uint16_t d_depth_cfg[] = {16, 8, 8, 16, 8, 8, 12, 12, 8};
	const uint16_t c_depth_cfg[] = {32, 16, 16, 32, 16 , 16, 24, 24, 16};*/

	const uint16_t wml_cfg[] = {0x50, 0x28, 0x28, 0x50, 0x28, 0x28, 0x50, 0x28, 0x28};
	const uint16_t d_depth_cfg[] = {0x0a, 0x05, 0x05, 0x0a, 0x05, 0x05, 0x0a, 0x05, 0x05};
	const uint16_t c_depth_cfg[] = {0x05, 0x03, 0x03, 0x05, 0x03, 0x03, 0x05, 0x03, 0x03};

	const uint8_t sche_cfg[] = {0x04, 0x01, 0x01, 0x04, 0x01, 0x01, 0x04, 0x01, 0x01};
	const uint8_t p1_cfg[] = {0x30, 0x20, 0x20, 0x30, 0x20, 0x20, 0x30, 0x20, 0x20};
	const uint8_t p0_cfg[] = {0x10, 0x08, 0x08, 0x10, 0x08, 0x08, 0x10, 0x08, 0x08};
	const uint8_t burst_mode_cfg[] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
	const uint8_t burst_len_cfg[] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x4};
	unsigned long flags;
	int i;

	for(i = 0; i < rdma->num_chan; i++) {
		DU_RDMA_CHAN_SET(dp_regs, rdma, i, wml, wml_cfg[i]);
		DU_RDMA_CHAN_SET(dp_regs, rdma, i, d_dep, d_depth_cfg[i]);
		DU_RDMA_CHAN_SET(dp_regs, rdma, i, c_dep, c_depth_cfg[i]);
		DU_RDMA_CHAN_SET(dp_regs, rdma, i, sche, sche_cfg[i]);
		DU_RDMA_CHAN_SET(dp_regs, rdma, i, prio1, p1_cfg[i]);
		DU_RDMA_CHAN_SET(dp_regs, rdma, i, prio0, p0_cfg[i]);
		DU_RDMA_CHAN_SET(dp_regs, rdma, i, burst_mode, burst_mode_cfg[i]);
		DU_RDMA_CHAN_SET(dp_regs, rdma, i, burst_len, burst_len_cfg[i]);
		spin_lock_irqsave(&kcrtc->dp_irq_lock[index], flags);
		DU_IRQ_SET(dp_regs, irq_data, rdma_int_mask, i, 0);
		spin_unlock_irqrestore(&kcrtc->dp_irq_lock[index], flags);
	}

	spin_lock_irqsave(&kcrtc->dp_irq_lock[index], flags);
//	DU_IRQ_SET(dp_regs, irq_data, int_mask, RDMA_SHIFT, 0);
	spin_unlock_irqrestore(&kcrtc->dp_irq_lock[index], flags);

	DU_REG_SET(dp_regs, rdma, cfg_load, 1);
	DU_REG_SET(dp_regs, rdma, arb_sel, 1);
}

static void kunlun_dp_mlc_init(struct kunlun_crtc *kcrtc,
		const struct kunlun_mlc_data *mlc, int index)
{
	void __iomem *dp_regs = kcrtc->dp_regs[index];
	const struct kunlun_irq_data *irq_data = kcrtc->data->dp_data->irq;
	const uint8_t prot_val[] = {0x07, 0x07, 0x07, 0x07};
	const uint32_t aflu_time[] = {0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF};
	const uint8_t alpha_bld[] = {0xF, 0xF, 0xF, 0xF, 0xF};
	const uint8_t layer_out[] = {0xF, 0xF, 0xF, 0xF, 0xF};
	const uint8_t fstart_val = 0x4;
	unsigned long flags;
	unsigned int i;

	for(i = 0; i < mlc->num_layer; i++) {
		DU_MLC_LAYER_SET(dp_regs, mlc, i, prot_val, prot_val[i]);
		DU_MLC_LAYER_SET(dp_regs, mlc, i, h_pos, 0);
		DU_MLC_LAYER_SET(dp_regs, mlc, i, v_pos, 0);
		DU_MLC_LAYER_SET(dp_regs, mlc, i, aflu_time, aflu_time[i]);
		DU_MLC_LAYER_SET(dp_regs, mlc, i, g_alpha, 0xFF);
		DU_MLC_LAYER_SET(dp_regs, mlc, i, g_alpha_en, 1);
	}

	for(i = 0; i < mlc->num_path; i++) {
		DU_MLC_PATH_SET(dp_regs, mlc, i, alpha_bld_idx, alpha_bld[i]);
		DU_MLC_PATH_SET(dp_regs, mlc, i, layer_out_idx, layer_out[i]);
		spin_lock_irqsave(&kcrtc->dp_irq_lock[index], flags);
		DU_IRQ_SET(dp_regs, irq_data, mlc_int_mask, i + MLC_FLU_L_0_SHIFT, 0);
		DU_IRQ_SET(dp_regs, irq_data, mlc_int_mask, i + MLC_E_L_0_SHIFT, 0);
		spin_unlock_irqrestore(&kcrtc->dp_irq_lock[index], flags);
	}

	DU_REG_SET(dp_regs, mlc->bg, fstart_sel, fstart_val);
//	DU_IRQ_SET(dp_regs, irq_data, int_mask, MLC_SHIFT, 0);

	DU_REG_SET(dp_regs, mlc, ratio, 0xFFFF);
}

static void kunlun_dp_fbdc_init(struct kunlun_crtc *kcrtc,
		const struct kunlun_fbdc_data *fbdc, int index)
{
	const struct kunlun_fbdc_ctrl *ctrl = kcrtc->data->dp_data->fbdcs->ctrl;
	void __iomem *dp_regs = kcrtc->dp_regs[index];

	DU_REG_SET(dp_regs, ctrl, en, 0);
}
static int kunlun_crtc_init(struct kunlun_crtc *kcrtc)
{
	const struct kunlun_crtc_du_data *dc_data = kcrtc->data->dc_data;
	const struct kunlun_crtc_du_data *dp_data = kcrtc->data->dp_data;
	int i;
	/*init dc*/
	if (kcrtc->dc_nums) {
		kunlun_dc_sw_reset(kcrtc, dc_data->ctrl);
		kunlun_dc_irq_init(kcrtc, dc_data->irq);
		kunlun_dc_rdma_init(kcrtc, dc_data->rdma);
		kunlun_tcon_init(kcrtc, dc_data->tcon);
		kunlun_dc_mlc_init(kcrtc, dc_data->mlc);
	}

	for (i = 0; i < kcrtc->dp_nums; i++) {
		kunlun_dp_sw_reset(kcrtc, dp_data->ctrl, i);
		kunlun_dp_irq_init(kcrtc, dp_data->irq, i);
		kunlun_dp_ctrl_init(kcrtc, dp_data->ctrl, i);
		kunlun_dp_rdma_init(kcrtc, dp_data->rdma, i);
		kunlun_dp_mlc_init(kcrtc, dp_data->mlc, i);
		kunlun_dp_fbdc_init(kcrtc, dp_data->fbdcs, i);
	}

	return 0;
}

static irqreturn_t dc_irq_handler(int irq, void *data)
{
	struct kunlun_crtc *kcrtc = data;
	const struct kunlun_irq_data *irq_data = kcrtc->data->dc_data->irq;
	void __iomem *regs = kcrtc->dc_regs;
	unsigned long flags;
	uint32_t val;

	spin_lock_irqsave(&kcrtc->dc_irq_lock, flags);
	val = DU_IRQ_GET(regs, irq_data, int_stat);
	DU_IRQ_CLR(regs, irq_data, int_stat, val);
	spin_unlock_irqrestore(&kcrtc->dc_irq_lock, flags);

	if(val & TCON_EOF_MASK)
		kunlun_crtc_handle_vblank(kcrtc);

//	if(val & DC_UNDERRUN_MASK)
//		schedule_work(&kcrtc->underrun_work);

	if(val & DC_MLC_MASK)
		schedule_work(&kcrtc->mlc_int_work);

	if(val & DC_RDMA_MASK)
		schedule_work(&kcrtc->rdma_int_work);

	return IRQ_HANDLED;
}

static void kunlun_dc_underrun(struct work_struct *work)
{
	struct kunlun_crtc *kcrtc =
		container_of(work, struct kunlun_crtc, underrun_work);

	DRM_DEV_ERROR(kcrtc->dev, "DC UNDERRUN!\n");
}

static void kunlun_mlc_int_error(struct work_struct *work)
{
	struct kunlun_crtc *kcrtc =
		container_of(work, struct kunlun_crtc, mlc_int_work);
	const struct kunlun_irq_data *irq = kcrtc->data->dc_data->irq;
	void __iomem *regs = kcrtc->dc_regs;
	uint32_t val;

	val = DU_IRQ_GET(regs, irq, mlc_int_stat);
	DRM_DEV_ERROR(kcrtc->dev, "MLC ERROR: 0x%x\n", val);
}

static void kunlun_rdma_int_error(struct work_struct *work)
{
	struct kunlun_crtc *kcrtc =
		container_of(work, struct kunlun_crtc, rdma_int_work);
	const struct kunlun_irq_data *irq = kcrtc->data->dc_data->irq;
	void __iomem *regs = kcrtc->dc_regs;
	uint32_t val;

	val = DU_IRQ_GET(regs, irq, rdma_int_stat);
	DRM_DEV_ERROR(kcrtc->dev, "RDMA ERROR: 0x%x\n", val);
}

static void kunlun_crtc_atomic_begin(struct drm_crtc *crtc,
		struct drm_crtc_state *old_crtc_state)
{

}

static void kunlun_crtc_atomic_flush(struct drm_crtc *crtc,
		struct drm_crtc_state *old_crtc_state)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	struct kunlun_crtc_state *state = to_kunlun_crtc_state(crtc->state);
	struct kunlun_crtc_state *old_state;
	int ret;

//	if(WARN_ON(kcrtc->enabled))
//		return;

	if(!state->base.active)
		return;

	spin_lock_irq(&crtc->dev->event_lock);
	if (crtc->state->event && old_crtc_state->active) {
		WARN_ON(drm_crtc_vblank_get(crtc));
		WARN_ON(kcrtc->event);

		kcrtc->event = crtc->state->event;
		crtc->state->event = NULL;
	}
	spin_unlock_irq(&crtc->dev->event_lock);

	old_state = to_kunlun_crtc_state(old_crtc_state);
	state->plane_mask = crtc->state->plane_mask;
	ret = kunlun_mlc_update_plane(kcrtc, state->plane_mask);
	if(ret)
		return;

	ret = kunlun_mlc_update_trig(kcrtc, state->plane_mask);
	if(ret)
		return;
}

static void kunlun_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	const struct kunlun_crtc_du_data *dc_data = kcrtc->data->dc_data;
	struct kunlun_crtc_state *state = to_kunlun_crtc_state(crtc->state);
	struct drm_display_mode *mode = &crtc->state->adjusted_mode;
	int i;

	DRM_DEV_INFO(kcrtc->dev,
			"set mode: %d:\"%s\" %d %d %d %d %d %d %d %d %d %d 0x%x 0x%x",
			mode->base.id, mode->name, mode->vrefresh, mode->clock,
			mode->hdisplay, mode->hsync_start, mode->hsync_end, mode->htotal,
			mode->vdisplay, mode->vsync_start, mode->vsync_end, mode->vtotal,
			mode->type, mode->flags);

	state->plane_mask |= crtc->state->plane_mask;
	kunlun_mlc_set_timings(kcrtc, mode);

	if (dc_data->tcon)
		kunlun_tcon_set_timings(kcrtc, mode, state->bus_flags);

	/*set dp out size*/
	for (i = 0; i < kcrtc->dp_nums; i++) {
		kunlun_dp_set_size(kcrtc, mode->hdisplay, mode->vdisplay, i);
	}
}

static void kunlun_crtc_atomic_enable(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	const struct kunlun_crtc_data *data = kcrtc->data;
	int ret;

	if(WARN_ON(kcrtc->enabled))
		return;

	ret = data->du_ops->du_enable(kcrtc, true);
	if(ret)
		return;

//	enable_irq(kcrtc->irq);
	drm_crtc_vblank_on(crtc);
	WARN_ON(drm_crtc_vblank_get(crtc));
	if (crtc->state->event) {
		WARN_ON(kcrtc->event);

		kcrtc->event = crtc->state->event;
		crtc->state->event = NULL;
	}
	kcrtc->enabled = true;
}

static void kunlun_crtc_atomic_disable(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	const struct kunlun_crtc_data *data = kcrtc->data;

	if(WARN_ON(!kcrtc->enabled))
		return;

//	disable_irq(kcrtc->irq);

	spin_lock_irq(&crtc->dev->event_lock);
	if (crtc->state->event) {
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		crtc->state->event = NULL;
	}
	spin_unlock_irq(&crtc->dev->event_lock);

	drm_crtc_vblank_off(crtc);

	data->du_ops->du_enable(kcrtc, false);

	kcrtc->enabled = false;
}

static const struct drm_crtc_helper_funcs kunlun_crtc_helper_funcs = {
	.atomic_flush = kunlun_crtc_atomic_flush,
	.atomic_begin = kunlun_crtc_atomic_begin,
	.mode_set_nofb = kunlun_crtc_mode_set_nofb,
	.atomic_enable = kunlun_crtc_atomic_enable,
	.atomic_disable = kunlun_crtc_atomic_disable,
};

static void kunlun_crtc_destroy(struct drm_crtc *crtc)
{
	drm_crtc_cleanup(crtc);
}

static void kunlun_crtc_reset(struct drm_crtc *crtc)
{
	if(crtc->state)
		__drm_atomic_helper_crtc_destroy_state(crtc->state);
	kfree(crtc->state);

	crtc->state = kzalloc(sizeof(struct kunlun_crtc_state), GFP_KERNEL);
	if(crtc->state)
		crtc->state->crtc = crtc;
}

static struct drm_crtc_state *
kunlun_crtc_duplicate_state(struct drm_crtc *crtc)
{
	struct kunlun_crtc_state *kunlun_state, *current_state;

	current_state = to_kunlun_crtc_state(crtc->state);

	kunlun_state = kzalloc(sizeof(struct kunlun_crtc_state), GFP_KERNEL);
	if(!kunlun_state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &kunlun_state->base);

	kunlun_state->plane_mask = current_state->plane_mask;

	return &kunlun_state->base;
}

static void kunlun_crtc_destroy_state(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	struct kunlun_crtc_state *s = to_kunlun_crtc_state(state);

	__drm_atomic_helper_crtc_destroy_state(state);

	kfree(s);
}

static int kunlun_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	const struct kunlun_crtc_data *data = kcrtc->data;

	if(!kcrtc->vblank_enable)
			return -EPERM;

	return data->du_ops->vblank_enable(kcrtc, true);
}

static void kunlun_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	const struct kunlun_crtc_data *data = kcrtc->data;

	if(!kcrtc->vblank_enable)
			return ;

	data->du_ops->vblank_enable(kcrtc, false);
}

static const struct drm_crtc_funcs kunlun_crtc_funcs = {
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.destroy = kunlun_crtc_destroy,
	.reset = kunlun_crtc_reset,
	.atomic_duplicate_state = kunlun_crtc_duplicate_state,
	.atomic_destroy_state = kunlun_crtc_destroy_state,
	.enable_vblank = kunlun_crtc_enable_vblank,
	.disable_vblank = kunlun_crtc_disable_vblank,
//	.set_crc_source = kunlun_crtc_set_crc_source,
};

static void kunlun_crtc_handle_vblank(struct kunlun_crtc *kcrtc)
{
	unsigned long flags;
	struct drm_crtc *crtc = &kcrtc->base;

	drm_crtc_handle_vblank(crtc);

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	if(kcrtc->event) {
		drm_crtc_send_vblank_event(crtc, kcrtc->event);
		drm_crtc_vblank_put(crtc);
		kcrtc->event = NULL;
	}
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}

static void kunlun_crtc_planes_fini(struct kunlun_crtc *kcrtc)
{
	unsigned int size;

	kunlun_planes_fini(kcrtc);

	drm_crtc_cleanup(&kcrtc->base);

	size = kcrtc->num_planes * sizeof(struct kunlun_plane);
	memset(kcrtc->planes, 0, size);
	kcrtc->num_planes = 0;
}

static int kunlun_crtc_planes_init(struct kunlun_crtc *kcrtc)
{
	const struct kunlun_crtc_du_data *dc_data = kcrtc->data->dc_data;
	const struct kunlun_crtc_du_data *dp_data = kcrtc->data->dp_data;
	struct drm_plane *primary = NULL, *cursor = NULL;
	struct kunlun_plane *plane;
	unsigned int size;
	unsigned int dc_planes_nums = 0;
	unsigned int dp_planes_nums = 0;
	unsigned int planes_nums;
	int ret;
	int i, j;

	if (kcrtc->dc_nums) {
		dc_planes_nums = dc_data->num_planes;
	}

	for (i = 0; i < kcrtc->dp_nums; i++) {
		dp_planes_nums += dp_data->num_planes;
	}

	planes_nums = dc_planes_nums + dp_planes_nums;
	size = planes_nums * sizeof(struct kunlun_plane);
	kcrtc->planes = devm_kzalloc(kcrtc->dev, size, GFP_KERNEL);
	if(!kcrtc->planes)
		return -ENOMEM;

	for (i = 0; i < dp_planes_nums; i++) {
		if (i < dp_data->num_planes) {
			j = i;
		} else {
			j = i - dp_data->num_planes;
		}
		plane = &kcrtc->planes[i];
		plane->data = &dp_data->planes[j];
		//plane->mlc_plane.base = ;
		//plane->mlc_plane.layer = data->mlc->mlc_layer;
		plane->kcrtc = kcrtc;

		plane->du_owner = (i < dp_data->num_planes) ? CRTC_DP0 : CRTC_DP1;
	}

	for (i = dp_planes_nums; i < planes_nums; i++) {
		plane = &kcrtc->planes[i];
		plane->data = &dc_data->planes[i - dp_planes_nums];
		plane->kcrtc = kcrtc;
		plane->du_owner = CRTC_DC;
	}
	kcrtc->num_dc_planes = dc_planes_nums;
	kcrtc->num_dp_planes = dp_planes_nums;
	kcrtc->num_planes = planes_nums;

	kunlun_planes_init(kcrtc);

	ret = kunlun_planes_init_primary(kcrtc, &primary, &cursor);
	if(ret)
		goto err_planes_fini;
	if(!primary) {
		DRM_DEV_ERROR(kcrtc->dev, "CRTC cannot find primary plane\n");
		goto err_planes_fini;
	}

	ret = drm_crtc_init_with_planes(kcrtc->drm, &kcrtc->base,
			primary, cursor, &kunlun_crtc_funcs, NULL);
	if(ret)
		goto err_planes_fini;

	drm_crtc_helper_add(&kcrtc->base, &kunlun_crtc_helper_funcs);

	ret = kunlun_planes_init_overlay(kcrtc);
	if(ret)
		goto err_crtc_cleanup;

	return 0;

err_crtc_cleanup:
	drm_crtc_cleanup(&kcrtc->base);
err_planes_fini:
	kunlun_planes_fini(kcrtc);
	memset(kcrtc->planes, 0, size);
	kcrtc->num_planes = 0;
	return ret;
}

static int kunlun_crtc_get_resource(struct kunlun_crtc *kcrtc)
{
	struct device *dev = kcrtc->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct device_node *port;
	struct resource *regs;
	int irq;
	int ret;

	ret = of_property_read_u32(dev->of_node, "dc_nums", &kcrtc->dc_nums);
	if (ret) {
		dev_err(dev, "dc_nums is not specified\n");
		return -EINVAL;
	}
	/*
	ret = of_property_read_u32(dev->of_node, "dp_nums", &kcrtc->dp_nums);
	if (ret) {
		dev_err(dev, "dp_nums is not specified\n");
		return -EINVAL;
	}*/

	if ((kcrtc->dc_nums == 0) && (kcrtc->dp_nums == 0)) {
		dev_err(dev, "crtc have no hardware support!\n");
		return -EINVAL;
	}

	if (kcrtc->dc_nums) {
		regs = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dc-base");
		kcrtc->dc_regs = devm_ioremap_resource(dev, regs);
		if(IS_ERR(kcrtc->dc_regs)) {
			dev_err(dev, "Cannot find dc regs\n");
			return PTR_ERR(kcrtc->dc_regs);
		}
	}

	if (kcrtc->dp_nums) {
		regs = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dp0-base");
		kcrtc->dp_regs[0] = devm_ioremap_resource(dev, regs);
		if(IS_ERR(kcrtc->dp_regs[0])) {
			dev_err(dev, "Cannot find dp0 regs\n");
			return PTR_ERR(kcrtc->dp_regs[0]);
		}
	}

	if (kcrtc->dp_nums == 2) {
		regs = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dp1-base");
		kcrtc->dp_regs[1] = devm_ioremap_resource(dev, regs);
		if(IS_ERR(kcrtc->dp_regs[1])) {
			dev_err(dev, "Cannot find dp1 regs\n");
			return PTR_ERR(kcrtc->dp_regs[1]);
		}
	}

	irq = platform_get_irq(pdev, 0);
	if(irq < 0) {
		dev_err(dev, "Cannot find irq for DC\n");
		return irq;
	}
	kcrtc->irq = (unsigned int)irq;

	spin_lock_init(&kcrtc->dc_irq_lock);
	spin_lock_init(&kcrtc->dp_irq_lock[0]);
	spin_lock_init(&kcrtc->dp_irq_lock[1]);

	if(of_property_read_bool(dev->of_node,"vblank-disable"))
		kcrtc->vblank_enable = false;
	else
		kcrtc->vblank_enable = true;

	port = of_get_child_by_name(dev->of_node, "port");
	if (!port) {
		DRM_DEV_ERROR(dev, "no port node found in %pOF\n", dev->of_node);
		return -ENOENT;
	}
	kcrtc->base.port = port;

	DRM_DEV_INFO(dev, "dc:0x%p, dp0:0x%p, dp1:0x%p, IRQ: %d, vblank_enable: %x\n",
		kcrtc->dc_regs, kcrtc->dp_regs[0], kcrtc->dp_regs[1], kcrtc->irq,
		kcrtc->vblank_enable);

	return 0;
}

static int kunlun_crtc_bind(struct device *dev, struct device *master,
		void *data)
{
	struct drm_device *drm = data;
	struct kunlun_drm_data *kdrm = drm->dev_private;
	const struct kunlun_crtc_data *crtc_data;
	struct kunlun_crtc *crtc;
	int ret;

	crtc_data = of_device_get_match_data(dev);
	if(!crtc_data)
		return -ENODEV;

	crtc = devm_kzalloc(dev, sizeof(*crtc), GFP_KERNEL);
	if(!crtc)
		return -ENOMEM;

	crtc->dev = dev;
	crtc->drm = drm;
	crtc->data = crtc_data;
	dev_set_drvdata(dev, crtc);

	ret = kunlun_crtc_get_resource(crtc);
	if(ret)
		goto err_out;

	ret = kunlun_crtc_init(crtc);
	if(ret) {
		dev_err(dev, "Failed init CRTC device: %d\n", ret);
		goto err_planes_fini;
	}

	ret = kunlun_crtc_planes_init(crtc);
	if(ret)
		goto err_out;

	ret = devm_request_irq(dev, crtc->irq, dc_irq_handler,
			IRQF_SHARED, dev_name(dev), crtc);
	if(ret) {
		DRM_DEV_ERROR(dev, "Failed to request DC IRQ: %d\n", crtc->irq);
		goto err_dc_crtc_fini;
	}

//	disable_irq(dc->irq);

	INIT_WORK(&crtc->underrun_work, kunlun_dc_underrun);
	INIT_WORK(&crtc->mlc_int_work, kunlun_mlc_int_error);
	INIT_WORK(&crtc->rdma_int_work, kunlun_rdma_int_error);

	kdrm->crtcs[kdrm->num_crtcs] = &crtc->base;
	kdrm->num_crtcs++;

	return 0;

err_dc_crtc_fini:
err_planes_fini:
	kunlun_crtc_planes_fini(crtc);
err_out:
	return ret;
}

static void kunlun_crtc_unbind(struct device *dev,
		struct device *master, void *data)
{
	struct kunlun_crtc *crtc = dev_get_drvdata(dev);

	kunlun_crtc_planes_fini(crtc);
}

static const struct component_ops kunlun_crtc_ops = {
	.bind = kunlun_crtc_bind,
	.unbind = kunlun_crtc_unbind,
};

static int kunlun_dc_vblank_enable(struct kunlun_crtc *kcrtc, bool enable)
{
	uint32_t val;
	void __iomem *regs = kcrtc->dc_regs;
	const struct kunlun_irq_data *irq_data = kcrtc->data->dc_data->irq;

	val = enable ? 0 : 1;

	DU_IRQ_SET(regs, irq_data, int_mask, TCON_EOF_SHIFT, val);

	return 0;
}

static int kunlun_dc_enable(struct kunlun_crtc *kcrtc, bool enable)
{
	uint32_t val;
	const struct kunlun_ctrl_data *ctrl = kcrtc->data->dc_data->ctrl;
	const struct kunlun_tcon_data *tcon = kcrtc->data->dc_data->tcon;
	void __iomem *regs = kcrtc->dc_regs;

	if(enable) {
		DU_REG_SET(regs, ctrl, di_trig, 1);
		DU_REG_SET(regs, ctrl, flc_trig, 1);
		DU_REG_SET(regs, ctrl, force_trig, 1);
		val = 1;
	} else {
		kunlun_mlc_disable(kcrtc);
		val = 0;
	}

	DU_REG_SET(regs, tcon, enable, val);
	return 0;
}

static int kunlun_drm_crtc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct kunlun_crtc_data *crtc_data;

	crtc_data = of_device_get_match_data(dev);
	if(!crtc_data || !crtc_data->cmpt_ops)
		return -ENODEV;

	return component_add(dev, crtc_data->cmpt_ops);
}

static int kunlun_drm_crtc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct kunlun_crtc_data *crtc_data;

	crtc_data = of_device_get_match_data(dev);
	if(!crtc_data || !crtc_data->cmpt_ops)
		return -ENODEV;

	component_del(dev, crtc_data->cmpt_ops);
	return 0;
}


static const struct kunlun_du_ops kunlun_crtc_du_ops = {
	.vblank_enable = kunlun_dc_vblank_enable,
	.du_enable = kunlun_dc_enable,
};

static const struct kunlun_crtc_du_data kunlun_dc_data = {
	.irq = &dc_irq,
	.ctrl = &dc_ctrl,
	.rdma = &dc_rdma,
	.mlc = &mlc_data,
	.tcon = &tcon_data,
	.planes = kunlun_dc_planes,
	.num_planes = 2,
};

static const struct kunlun_crtc_du_data kunlun_dp_data = {
	.irq = &dp_irq,
	.ctrl = &dp_ctrl,
	.rdma = &dp_rdma,
	.mlc = &mlc_data,
	.planes = kunlun_dp_planes,
	.fbdcs = &fbdc_data,
	.num_planes = 4,
};

static const struct kunlun_crtc_data crtc_data = {
	.dc_data = &kunlun_dc_data,
	.dp_data = &kunlun_dp_data,
	.cmpt_ops = &kunlun_crtc_ops,
	.du_ops = &kunlun_crtc_du_ops,
};

static const struct of_device_id kunlun_crtc_of_table[] = {
	{.compatible = "semidrive,kunlun-crtc", .data = &crtc_data},
	{},
};

static struct platform_driver kunlun_crtc_driver = {
	.probe = kunlun_drm_crtc_probe,
	.remove = kunlun_drm_crtc_remove,
	.driver = {
		.name = "kunlun-crtc",
		.of_match_table = kunlun_crtc_of_table,
	},
};
module_platform_driver(kunlun_crtc_driver);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive kunlun dc CRTC");
MODULE_LICENSE("GPL");

