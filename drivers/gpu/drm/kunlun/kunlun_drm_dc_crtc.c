/*
 * kunlun_drm_dc_crtc.c
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <drm/drm.h>
#include <drm/drmP.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_crtc_helper.h>
#include <linux/of_device.h>

#include "kunlun_drm_drv.h"
#include "kunlun_drm_gem.h"
#include "kunlun_drm_crtc.h"
#include "kunlun_drm_reg.h"

extern const struct kunlun_plane_data kunlun_dc_planes[];

static const struct kunlun_irq_data dc_irq = {
	.int_mask = DU_REG(DC_INT_MSK, DC_INT_MASK, 0),
	.int_stat = DU_REG(DC_INT_ST, DC_INT_MASK, 0),
	.safe_int_mask = DU_REG(DC_SAFE_INT_MSK, DC_INT_MASK, 0),
	.rdma_int_mask = DU_REG(RDMA_INT_MSK, RDMA_INT_MASK, 0),
	.rdma_int_stat = DU_REG(RDMA_INT_ST, RDMA_INT_MASK, 0),
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
	.sdma_en = DU_REG(DC_SDMA_CTRL, SDMA_EN_MASK, SDMA_EN_SHIFT),
};

const struct kunlun_rdma_channel rdma_chn = {
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
	.cfg_load = DU_REG(RDMA_CTRL, CTRL_CFG_LOAD_MASK, CTRL_CFG_LOAD_SHIFT),
	.arb_sel = DU_REG(RDMA_CTRL, CTRL_ARB_SEL_MASK, CTRL_ARB_SEL_SHIFT),
	.dfifo_full = DU_REG(RDMA_DFIFO_FULL, RDMA_DC_CH_MASK, 0),
	.dfifo_empty = DU_REG(RDMA_DFIFO_EMPTY, RDMA_DC_CH_MASK, 0),
	.cfifo_full = DU_REG(RDMA_CFIFO_FULL, RDMA_DC_CH_MASK, 0),
	.cfifo_empty = DU_REG(RDMA_CFIFO_EMPTY, RDMA_DC_CH_MASK, 0),
	.ch_idle = DU_REG(RDMA_CH_IDLE, RDMA_DC_CH_MASK, 0),
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

const struct kunlun_mlc_data mlc_data = {
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

void kunlun_mlc_disable(struct kunlun_crtc *kcrtc)
{
	const struct kunlun_crtc_data *data = kcrtc->data;
	const struct kunlun_mlc_data *mlc = data->mlc;
	unsigned int i;

	for(i = 0; i < mlc->num_layer; i++)
		DU_MLC_LAYER_SET(kcrtc, mlc, i, sf_en, 0);

}

int kunlun_mlc_update_plane(struct kunlun_crtc *kcrtc, unsigned int mask)
{
	const struct kunlun_crtc_data *data = kcrtc->data;
	const struct kunlun_mlc_data *mlc = data->mlc;
	const struct kunlun_ctrl_data *ctrl = data->ctrl;
	unsigned int i;

	if(!mask)
		return -EINVAL;

	for (i = 0; i < mlc->num_path; i++) {
		if (mask & (1 << i)) {
			DU_MLC_PATH_SET(kcrtc, mlc, i+1, alpha_bld_idx, 1);
			DU_MLC_PATH_SET(kcrtc, mlc, i+1, layer_out_idx, 1);
		} else {
			DU_MLC_PATH_SET(kcrtc, mlc, i+1, alpha_bld_idx, 0xF);
			DU_MLC_PATH_SET(kcrtc, mlc, i+1, layer_out_idx, 0xF);
		}

	}

	DU_REG_SET(kcrtc, ctrl, di_trig, 1);

	return 0;
}

int kunlun_mlc_update_trig(struct kunlun_crtc *kcrtc)
{
	const struct kunlun_crtc_data *data = kcrtc->data;
	const struct kunlun_ctrl_data *ctrl = data->ctrl;

	DU_REG_SET(kcrtc, ctrl, flc_trig, 1);
	return 0;
}

static void kunlun_dc_sw_reset(struct kunlun_crtc *dc,
		const struct kunlun_ctrl_data *ctrl)
{
//	DC_CTRL_SET(dc, ctrl, reset, 1);
//	usleep_range(1, 2);
//	DC_CTRL_SET(dc, ctrl, reset, 0);
	DC_CTRL_SET(dc, ctrl, urc, 1);
}

void kunlun_irq_init(struct kunlun_crtc *kcrtc,
		const struct kunlun_irq_data *irq_data)
{
	DU_IRQ_CLR(kcrtc, irq_data, int_mask, 0xFFFFFFF); 
	DU_IRQ_CLR(kcrtc, irq_data, safe_int_mask, 0xFFFFFFF); 
	DU_IRQ_CLR(kcrtc, irq_data, rdma_int_mask, 0x7F); 
	DU_IRQ_CLR(kcrtc, irq_data, mlc_int_mask, 0x1FFF); 
	DU_IRQ_CLR(kcrtc, irq_data, crc32_int_mask, 0xFFFF); 
}

void kunlun_tcon_init(struct kunlun_crtc *kcrtc,
		const struct kunlun_tcon_data *tcon_data)
{
	const struct kunlun_tcon_csc *csc = tcon_data->csc;
	const struct kunlun_tcon_gamma *gamma = tcon_data->gamma;

	DU_REG_SET(kcrtc, csc, bypass, 1);
	DU_REG_SET(kcrtc, gamma, bypass, 1);
}

void kunlun_rdma_init(struct kunlun_crtc *kcrtc,
		const struct kunlun_rdma_data *rdma)
{
	const struct kunlun_irq_data *irq_data = kcrtc->data->irq;
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
		DU_RDMA_CHAN_SET(kcrtc, rdma, i, wml, wml_cfg[i]);
		DU_RDMA_CHAN_SET(kcrtc, rdma, i, d_dep, d_depth_cfg[i]);
		DU_RDMA_CHAN_SET(kcrtc, rdma, i, c_dep, c_depth_cfg[i]);
		DU_RDMA_CHAN_SET(kcrtc, rdma, i, sche, sche_cfg[i]);
		DU_RDMA_CHAN_SET(kcrtc, rdma, i, prio1, p1_cfg[i]);
		DU_RDMA_CHAN_SET(kcrtc, rdma, i, prio0, p0_cfg[i]);
		DU_RDMA_CHAN_SET(kcrtc, rdma, i, burst_mode, burst_mode_cfg[i]);
		DU_RDMA_CHAN_SET(kcrtc, rdma, i, burst_len, burst_len_cfg[i]);
		spin_lock_irqsave(&kcrtc->irq_lock, flags);
		DU_IRQ_SET(kcrtc, irq_data, rdma_int_mask, i, 0);
		spin_unlock_irqrestore(&kcrtc->irq_lock, flags);
	}

	spin_lock_irqsave(&kcrtc->irq_lock, flags);
//	DU_IRQ_SET(kcrtc, irq_data, int_mask, RDMA_SHIFT, 0);
	spin_unlock_irqrestore(&kcrtc->irq_lock, flags);

	DU_REG_SET(kcrtc, rdma, cfg_load, 1);
	DU_REG_SET(kcrtc, rdma, arb_sel, 1);
}

int kunlun_mlc_set_timings(struct kunlun_crtc *kcrtc,
		struct drm_display_mode *mode)
{
	const struct kunlun_crtc_data *data = kcrtc->data;
	const struct kunlun_mlc_data *mlc = data->mlc;
	unsigned int i;

	for(i = 0; i < mlc->num_layer; i++) {
		DU_MLC_TIMING_SET(kcrtc, mlc, i, h_size, mode->hdisplay);
		DU_MLC_TIMING_SET(kcrtc, mlc, i, v_size, mode->vdisplay);
	}

	return 0;
}

int kunlun_tcon_set_timings(struct kunlun_crtc *kcrtc,
		struct drm_display_mode *mode, u32 bus_flags)
{
	const struct kunlun_crtc_data *data = kcrtc->data;
	const struct kunlun_tcon_data *tcon = data->tcon;
	unsigned int i, tmp;

	DU_TIMING_REG_SET(kcrtc, tcon, hact, mode->hdisplay);
	DU_TIMING_REG_SET(kcrtc, tcon, htot, mode->htotal);

	tmp = mode->htotal - mode->hsync_start;
	DU_TIMING_REG_SET(kcrtc, tcon, hbp, tmp);
	tmp = mode->hsync_end - mode->hsync_start;
	DU_TIMING_REG_SET(kcrtc, tcon, hsync, tmp);

	DU_TIMING_REG_SET(kcrtc, tcon, vact, mode->vdisplay);
	DU_TIMING_REG_SET(kcrtc, tcon, vtot, mode->vtotal);

	tmp = mode->vtotal - mode->vsync_start;
	DU_TIMING_REG_SET(kcrtc, tcon, vbp, tmp);
	tmp = mode->vsync_end - mode->vsync_start;
	DU_TIMING_REG_SET(kcrtc, tcon, vsync, tmp);

	for(i = 0; i < tcon->num_klayer; i++) {
		if(i < 2)
			tmp = mode->vdisplay + 4;
		else
			tmp = mode->vdisplay + 5;
		if(tmp > mode->vtotal - 1)
			tmp = mode->vtotal -1;
		DU_TCON_KLAYER_REG_SET(kcrtc, tcon, i, y_color, tmp);

		tmp = mode->htotal / 2 + i;
		if(tmp > mode->htotal - 1)
			tmp = mode->htotal - 1;
		DU_TCON_KLAYER_REG_SET(kcrtc, tcon, i, x_color, tmp);
		DU_TCON_KLAYER_REG_SET(kcrtc, tcon, i, enable, 1);
	}

	if(bus_flags & DRM_BUS_FLAG_PIXDATA_NEGEDGE)
		DU_REG_SET(kcrtc, tcon, clk_pol, 1);
	if(bus_flags & DRM_BUS_FLAG_DE_LOW)
		DU_REG_SET(kcrtc, tcon, de_pol, 1);

	if(mode->flags & DRM_MODE_FLAG_NVSYNC)
		DU_REG_SET(kcrtc, tcon, vsync_pol, 1);
	if(mode->flags & DRM_MODE_FLAG_NHSYNC)
		DU_REG_SET(kcrtc, tcon, hsync_pol, 1);

	DU_REG_SET(kcrtc, tcon, clk_en, 1);
//	DU_REG_SET(kcrtc, tcon, enable, 1);

	DU_REG_SET(kcrtc, tcon, trig, 1);

	return 0;
}

void kunlun_mlc_init(struct kunlun_crtc *kcrtc,
		const struct kunlun_mlc_data *mlc)
{
	const struct kunlun_irq_data *irq_data = kcrtc->data->irq;
	const uint8_t prot_val[] = {0x07, 0x07, 0x07, 0x07};
	const uint32_t aflu_time[] = {0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF};
	const uint8_t alpha_bld[] = {0xF, 0xF, 0xF, 0xF, 0xF};
	const uint8_t layer_out[] = {0xF, 0xF, 0xF, 0xF, 0xF};
	unsigned long flags;
	unsigned int i;

	for(i = 0; i < mlc->num_layer; i++) {
		DU_MLC_LAYER_SET(kcrtc, mlc, i, prot_val, prot_val[i]);
		DU_MLC_LAYER_SET(kcrtc, mlc, i, h_pos, 0);
		DU_MLC_LAYER_SET(kcrtc, mlc, i, v_pos, 0);
		DU_MLC_LAYER_SET(kcrtc, mlc, i, aflu_time, aflu_time[i]);
		DU_MLC_LAYER_SET(kcrtc, mlc, i, g_alpha, 0xFF);
		DU_MLC_LAYER_SET(kcrtc, mlc, i, g_alpha_en, 1);
	}

	for(i = 0; i < mlc->num_path; i++) {
		DU_MLC_PATH_SET(kcrtc, mlc, i, alpha_bld_idx, alpha_bld[i]);
		DU_MLC_PATH_SET(kcrtc, mlc, i, layer_out_idx, layer_out[i]);
		spin_lock_irqsave(&kcrtc->irq_lock, flags);
		DU_IRQ_SET(kcrtc, irq_data, mlc_int_mask, i + MLC_FLU_L_0_SHIFT, 0);
		DU_IRQ_SET(kcrtc, irq_data, mlc_int_mask, i + MLC_E_L_0_SHIFT, 0);
		spin_unlock_irqrestore(&kcrtc->irq_lock, flags);
	}

//	DU_IRQ_SET(kcrtc, irq_data, int_mask, MLC_SHIFT, 0);

	DU_REG_SET(kcrtc, mlc, ratio, 0xFFFF);
}

static int kunlun_dc_crtc_init(struct kunlun_crtc *dc)
{
	const struct kunlun_crtc_data *dc_data = dc->data;

	kunlun_dc_sw_reset(dc, dc_data->ctrl);

	kunlun_irq_init(dc, dc_data->irq);

	kunlun_rdma_init(dc, dc_data->rdma);

	kunlun_tcon_init(dc, dc_data->tcon);

	kunlun_mlc_init(dc, dc_data->mlc);

	return 0;
}

static irqreturn_t dc_irq_handler(int irq, void *data)
{
	struct kunlun_crtc *kcrtc = data;
	const struct kunlun_irq_data *irq_data = kcrtc->data->irq;
	unsigned long flags;
	uint32_t val;

	spin_lock_irqsave(&kcrtc->irq_lock, flags);
	val = DU_IRQ_GET(kcrtc, irq_data, int_stat);
	DU_IRQ_CLR(kcrtc, irq_data, int_stat, val);
	spin_unlock_irqrestore(&kcrtc->irq_lock, flags);

	if(val & TCON_EOF_MASK)
		kunlun_crtc_handle_vblank(kcrtc);

//	if(val & DC_UNDERRUN_MASK)
//		schedule_work(&kcrtc->underrun_work);

	if(val & MLC_MASK)
		schedule_work(&kcrtc->mlc_int_work);

	if(val & RDMA_MASK)
		schedule_work(&kcrtc->rdma_int_work);

	return IRQ_HANDLED;
}

static void kunlun_dc_underrun(struct work_struct *work)
{
	struct kunlun_crtc *kcrtc =
		container_of(work, struct kunlun_crtc, underrun_work);

	DRM_DEV_ERROR(kcrtc->dev, "DC UNDERRUN!\n");
}

void kunlun_mlc_int_error(struct work_struct *work)
{
	struct kunlun_crtc *kcrtc =
		container_of(work, struct kunlun_crtc, mlc_int_work);
	const struct kunlun_irq_data *irq = kcrtc->data->irq;
	uint32_t val;

	val = DU_IRQ_GET(kcrtc, irq, mlc_int_stat);
	DRM_DEV_ERROR(kcrtc->dev, "MLC ERROR: 0x%x\n", val);
}

void kunlun_rdma_int_error(struct work_struct *work)
{
	struct kunlun_crtc *kcrtc =
		container_of(work, struct kunlun_crtc, rdma_int_work);
	const struct kunlun_irq_data *irq = kcrtc->data->irq;
	uint32_t val;

	val = DU_IRQ_GET(kcrtc, irq, rdma_int_stat);
	DRM_DEV_ERROR(kcrtc->dev, "RDMA ERROR: 0x%x\n", val);
}

static int kunlun_dc_crtc_bind(struct device *dev, struct device *master,
		void *data)
{
	struct drm_device *drm = data;
	struct kunlun_drm_data *kdrm = drm->dev_private;
	const struct kunlun_crtc_data *dc_data;
	struct kunlun_crtc *dc;
	int ret;

	dc_data = of_device_get_match_data(dev);
	if(!dc_data)
		return -ENODEV;

	dc = devm_kzalloc(dev, sizeof(*dc), GFP_KERNEL);
	if(!dc)
		return -ENOMEM;

	dc->dev = dev;
	dc->drm = drm;
	dc->data = dc_data;
	dev_set_drvdata(dev, dc);

	ret = kunlun_crtc_get_resource(dc);
	if(ret)
		goto err_out;

	ret = kunlun_dc_crtc_init(dc);
	if(ret) {
		dev_err(dev, "Failed init DC device: %d\n", ret);
		goto err_planes_fini;
	}

	ret = kunlun_crtc_planes_init(dc);
	if(ret)
		goto err_out;

	ret = devm_request_irq(dev, dc->irq, dc_irq_handler,
			IRQF_SHARED, dev_name(dev), dc);
	if(ret) {
		DRM_DEV_ERROR(dev, "Failed to request DC IRQ: %d\n", dc->irq);
		goto err_dc_crtc_fini;
	}

//	disable_irq(dc->irq);

	INIT_WORK(&dc->underrun_work, kunlun_dc_underrun);
	INIT_WORK(&dc->mlc_int_work, kunlun_mlc_int_error);
	INIT_WORK(&dc->rdma_int_work, kunlun_rdma_int_error);

	kdrm->crtcs[kdrm->num_crtcs] = &dc->base;
	kdrm->num_crtcs++;

	return 0;

err_dc_crtc_fini:
err_planes_fini:
	kunlun_crtc_planes_fini(dc);
err_out:
	return ret;
}

static void kunlun_dc_crtc_unbind(struct device *dev,
		struct device *master, void *data)
{
	struct kunlun_crtc *dc = dev_get_drvdata(dev);

	kunlun_crtc_planes_fini(dc);
}

static const struct component_ops kunlun_dc_crtc_ops = {
	.bind = kunlun_dc_crtc_bind,
	.unbind = kunlun_dc_crtc_unbind,
};

static int kunlun_dc_vblank_enable(struct kunlun_crtc *kcrtc, bool enable)
{
	uint32_t val;
	const struct kunlun_irq_data *irq_data = kcrtc->data->irq;

	val = enable ? 0 : 1;

	DU_IRQ_SET(kcrtc, irq_data, int_mask, TCON_EOF_SHIFT, val);

	return 0;
}

static int kunlun_dc_enable(struct kunlun_crtc *kcrtc, bool enable)
{
	uint32_t val;
	const struct kunlun_ctrl_data *ctrl = kcrtc->data->ctrl;
	const struct kunlun_tcon_data *tcon = kcrtc->data->tcon;

	if(enable) {
		DU_REG_SET(kcrtc, ctrl, di_trig, 1);
		DU_REG_SET(kcrtc, ctrl, flc_trig, 1);
		DU_REG_SET(kcrtc, ctrl, force_trig, 1);
		val = 1;
	} else {
		kunlun_mlc_disable(kcrtc);
		val = 0;
	}

	DU_REG_SET(kcrtc, tcon, enable, val);
	return 0;
}

static const struct kunlun_du_ops kunlun_dc_ops = {
	.vblank_enable = kunlun_dc_vblank_enable,
	.du_enable = kunlun_dc_enable,
};

static const struct kunlun_crtc_data kunlun_dc_data = {
	.irq = &dc_irq,
	.ctrl = &dc_ctrl,
	.rdma = &dc_rdma,
	.mlc = &mlc_data,
	.tcon = &tcon_data,
	.planes = kunlun_dc_planes,
	.num_planes = 2,
	.cmpt_ops = &kunlun_dc_crtc_ops,
	.du_ops = &kunlun_dc_ops,
};

static const struct of_device_id kunlun_dc_of_table[] = {
	{.compatible = "semidrive,kunlun-dc", .data = &kunlun_dc_data},
	{},
};

static struct platform_driver kunlun_dc_crtc_driver = {
	.probe = kunlun_drm_crtc_probe,
	.remove = kunlun_drm_crtc_remove,
	.driver = {
		.name = "kunlun-dc",
		.of_match_table = kunlun_dc_of_table,
	},
};
module_platform_driver(kunlun_dc_crtc_driver);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive kunlun dc CRTC");
MODULE_LICENSE("GPL");

