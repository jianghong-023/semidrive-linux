/*
 * kunlun_drm_plane.c
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#include <drm/drm.h>
#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>

#include "kunlun_drm_drv.h"
#include "kunlun_drm_gem.h"
#include "kunlun_drm_crtc.h"
#include "kunlun_drm_dc_reg.h"
#include "kunlun_drm_dp_reg.h"

#define to_kunlun_plane(x) container_of(x, struct kunlun_plane, base)
#define to_kunlun_crtc(x) container_of(x, struct kunlun_crtc, base)

#define FRAC_16_16(mult, div)    (((mult) << 16) / (div))

static const struct kunlun_pipe_pix_comp gp_pix_comp = {
	.bpv = DU_REG(GP_PIX_COMP, BPV_MASK, BPV_SHIFT),
	.bpu = DU_REG(GP_PIX_COMP, BPU_MASK, BPU_SHIFT),
	.bpy = DU_REG(GP_PIX_COMP, BPY_MASK, BPY_SHIFT),
	.bpa = DU_REG(GP_PIX_COMP, BPA_MASK, BPA_SHIFT),
};
static const struct kunlun_pipe_pix_comp sp_pix_comp = {
	.bpv = DU_REG(SP_PIX_COMP, BPV_MASK, BPV_SHIFT),
	.bpu = DU_REG(SP_PIX_COMP, BPU_MASK, BPU_SHIFT),
	.bpy = DU_REG(SP_PIX_COMP, BPY_MASK, BPY_SHIFT),
	.bpa = DU_REG(SP_PIX_COMP, BPA_MASK, BPA_SHIFT),
};
static const struct kunlun_pipe_frm_ctrl gp_frm_ctrl = {
	.endia_ctrl = DU_REG(GP_FRM_CTRL, ENDIAN_CTRL_MASK, ENDIAN_CTRL_SHIFT),
	.comp_swap = DU_REG(GP_FRM_CTRL, COMP_SWAP_MASK, COMP_SWAP_SHIFT),
	.rot = DU_REG(GP_FRM_CTRL, ROT_MASK, ROT_SHIFT),
	.rgb_yuv = DU_REG(GP_FRM_CTRL, RGB_YUV_MASK, RGB_YUV_SHIFT),
	.uv_swap = DU_REG(GP_FRM_CTRL, UV_SWAP_MASK, UV_SWAP_SHIFT),
	.uv_mode = DU_REG(GP_FRM_CTRL, UV_MODE_MASK, UV_MODE_SHIFT),
	.mode = DU_REG(GP_FRM_CTRL, MODE_MASK, MODE_SHIFT),
	.fmt = DU_REG(GP_FRM_CTRL, FMT_MASK, FMT_SHIFT),
};

static const struct kunlun_pipe_frm_ctrl dp_gp_frm_ctrl = {
	.endia_ctrl = DU_REG(GP_FRM_CTRL, DP_ENDIAN_CTRL_MASK, DP_ENDIAN_CTRL_SHIFT),
	.comp_swap = DU_REG(GP_FRM_CTRL, DP_COMP_SWAP_MASK, DP_COMP_SWAP_SHIFT),
	.rot = DU_REG(GP_FRM_CTRL, DP_ROT_MASK, DP_ROT_SHIFT),
	.rgb_yuv = DU_REG(GP_FRM_CTRL, DP_RGB_YUV_MASK, DP_RGB_YUV_SHIFT),
	.uv_swap = DU_REG(GP_FRM_CTRL, DP_UV_SWAP_MASK, DP_UV_SWAP_SHIFT),
	.uv_mode = DU_REG(GP_FRM_CTRL, DP_UV_MODE_MASK, DP_UV_MODE_SHIFT),
	.mode = DU_REG(GP_FRM_CTRL, DP_MODE_MASK, DP_MODE_SHIFT),
	.fmt = DU_REG(GP_FRM_CTRL, DP_FMT_MASK, DP_FMT_SHIFT),
};

static const struct kunlun_pipe_frm_ctrl sp_frm_ctrl = {
	.endia_ctrl = DU_REG(SP_FRM_CTRL, ENDIAN_CTRL_MASK, ENDIAN_CTRL_SHIFT),
	.comp_swap = DU_REG(SP_FRM_CTRL, COMP_SWAP_MASK, COMP_SWAP_SHIFT),
	.rot = DU_REG(SP_FRM_CTRL, ROT_MASK, ROT_SHIFT),
	.rgb_yuv = DU_REG(SP_FRM_CTRL, RGB_YUV_MASK, RGB_YUV_SHIFT),
	.uv_swap = DU_REG(SP_FRM_CTRL, UV_SWAP_MASK, UV_SWAP_SHIFT),
	.uv_mode = DU_REG(SP_FRM_CTRL, UV_MODE_MASK, UV_MODE_SHIFT),
	.mode = DU_REG(SP_FRM_CTRL, MODE_MASK, MODE_SHIFT),
	.fmt = DU_REG(SP_FRM_CTRL, FMT_MASK, FMT_SHIFT),
};
static const struct kunlun_pipe_frm_data gp_frm_data = {
	.height = DU_REG(GP_FRM_SIZE, FRM_HEIGHT_MASK, FRM_HEIGHT_SHIFT),
	.width = DU_REG(GP_FRM_SIZE, FRM_WIDTH_MASK, FRM_WIDTH_SHIFT),
	.y_baddr_l = DU_REG(GP_Y_BADDR_L, BADDR_L_MASK, BADDR_L_SHIFT),
	.y_baddr_h = DU_REG(GP_Y_BADDR_H, BADDR_H_MASK, BADDR_H_SHIFT),
	.u_baddr_l = DU_REG(GP_U_BADDR_L, BADDR_L_MASK, BADDR_L_SHIFT),
	.u_baddr_h = DU_REG(GP_U_BADDR_H, BADDR_H_MASK, BADDR_H_SHIFT),
	.v_baddr_l = DU_REG(GP_V_BADDR_L, BADDR_L_MASK, BADDR_L_SHIFT),
	.v_baddr_h = DU_REG(GP_V_BADDR_H, BADDR_H_MASK, BADDR_H_SHIFT),
	.y_stride = DU_REG(GP_Y_STRIDE, STRIDE_MASK, STRIDE_SHIFT),
	.u_stride = DU_REG(GP_U_STRIDE, STRIDE_MASK, STRIDE_SHIFT),
	.v_stride = DU_REG(GP_V_STRIDE, STRIDE_MASK, STRIDE_SHIFT),
	.offset_x = DU_REG(GP_FRM_OFFSET, FRM_X_MASK, FRM_X_SHIFT),
	.offset_y = DU_REG(GP_FRM_OFFSET, FRM_Y_MASK, FRM_Y_SHIFT),
};
static const struct kunlun_pipe_frm_data sp_frm_data = {
	.height = DU_REG(SP_FRM_SIZE, FRM_HEIGHT_MASK, FRM_HEIGHT_SHIFT),
	.width = DU_REG(SP_FRM_SIZE, FRM_WIDTH_MASK, FRM_WIDTH_SHIFT),
	.y_baddr_l = DU_REG(SP_Y_BADDR_L, BADDR_L_MASK, BADDR_L_SHIFT),
	.y_baddr_h = DU_REG(SP_Y_BADDR_H, BADDR_H_MASK, BADDR_H_SHIFT),
	.y_stride = DU_REG(SP_Y_STRIDE, STRIDE_MASK, STRIDE_SHIFT),
	.offset_x = DU_REG(SP_FRM_OFFSET, FRM_X_MASK, FRM_X_SHIFT),
	.offset_y = DU_REG(SP_FRM_OFFSET, FRM_Y_MASK, FRM_Y_SHIFT),
};
static const struct kunlun_gp_yuvup_ctrl gp_yuvup_data = {
	.en = DU_REG(GP_YUVUP_CTRL, GP_YUVUP_EN_MASK, GP_YUVUP_EN_SHIFT),
	.vofset = DU_REG(GP_YUVUP_CTRL, GP_YUVUP_VOFSET_MASK, GP_YUVUP_VOFSET_SHIFT),
	.hofset = DU_REG(GP_YUVUP_CTRL, GP_YUVUP_HOFSET_MASK, GP_YUVUP_HOFSET_SHIFT),
	.filter_mode = DU_REG(GP_YUVUP_CTRL, GP_YUVUP_FILTER_MODE_MASK, GP_YUVUP_FILTER_MODE_SHIFT),
	.upv_bypass = DU_REG(GP_YUVUP_CTRL, GP_YUVUP_UPV_BYPASS_MASK, GP_YUVUP_UPV_BYPASS_SHIFT),
	.uph_bypass = DU_REG(GP_YUVUP_CTRL, GP_YUVUP_UPH_BYPASS_MASK, GP_YUVUP_UPH_BYPASS_SHIFT),
	.bypass = DU_REG(GP_YUVUP_CTRL, GP_YUVUP_BYPASS_MASK, GP_YUVUP_BYPASS_SHIFT),
};

static const struct kunlun_gp_crop_ctrl gp_crop_data = {
	.bypass = DU_REG(GP_CROP_CTRL, GP_CROP_BYPASS_MASK, GP_CROP_BYPASS_SHIFT),
	.start_x = DU_REG(GP_CROP_UL_POS, GP_CROP_UL_POS_X_MASK, GP_CROP_UL_POS_X_SHIFT),
	.start_y = DU_REG(GP_CROP_UL_POS, GP_CROP_UL_POS_Y_MASK, GP_CROP_UL_POS_Y_SHIFT),
	.vsize = DU_REG(GP_CROP_SIZE, GP_CROP_SIZE_V_MASK, GP_CROP_SIZE_V_SHIFT),
	.hsize = DU_REG(GP_CROP_SIZE, GP_CROP_SIZE_H_MASK, GP_CROP_SIZE_H_SHIFT),
	.err_st = DU_REG(GP_CROP_PAR_ERR, GP_CROP_PAR_STATUS_MASK, GP_CROP_PAR_STATUS_SHIFT),
};

static const struct kunlun_hscaler_data gp_hs_data = {
	.nor_para = DU_REG(GP_HS_CTRL, GP_HS_CTRL_NOR_PARA_MASK, GP_HS_CTRL_NOR_PARA_SHIFT),
	.apb_rd = DU_REG(GP_HS_CTRL, GP_HS_CTRL_APB_RD_MASK, GP_HS_CTRL_APB_RD_SHIFT),
	.filter_en_v = DU_REG(GP_HS_CTRL, GP_HS_CTRL_FILTER_EN_V_MASK, GP_HS_CTRL_FILTER_EN_V_SHIFT),
	.filter_en_u = DU_REG(GP_HS_CTRL, GP_HS_CTRL_FILTER_EN_U_MASK, GP_HS_CTRL_FILTER_EN_U_SHIFT),
	.filter_en_y = DU_REG(GP_HS_CTRL, GP_HS_CTRL_FILTER_EN_Y_MASK, GP_HS_CTRL_FILTER_EN_Y_SHIFT),
	.filter_en_a = DU_REG(GP_HS_CTRL, GP_HS_CTRL_FILTER_EN_A_MASK, GP_HS_CTRL_FILTER_EN_A_SHIFT),
	.pola = DU_REG(GP_HS_INI, GP_HS_INI_POLA_MASK, GP_HS_INI_POLA_SHIFT),
	.fra = DU_REG(GP_HS_INI, GP_HS_INI_FRA_MASK, GP_HS_INI_FRA_SHIFT),
	.ratio_int = DU_REG(GP_HS_RATIO, GP_HS_RATIO_INT_MASK, GP_HS_RATIO_INT_SHIFT),
	.ratio_fra = DU_REG(GP_HS_RATIO, GP_HS_RATIO_FRA_MASK, GP_HS_RATIO_FRA_SHIFT),
	.out_hsize = DU_REG(GP_HS_WIDTH, GP_HS_WIDTH_OUT_MASK, GP_HS_WIDTH_OUT_SHIFT),
};
static const struct kunlun_vscaler_data gp_vs_data = {
	.init_phase_o = DU_REG(GP_VS_CTRL, GP_VS_CTRL_INIT_PHASE_O_MASK, GP_VS_CTRL_INIT_PHASE_O_SHIFT),
	.init_pos_o = DU_REG(GP_VS_CTRL, GP_VS_CTRL_INIT_POS_O_MASK, GP_VS_CTRL_INIT_POS_O_SHIFT),
	.init_phase_e = DU_REG(GP_VS_CTRL, GP_VS_CTRL_INIT_PHASE_E_MASK, GP_VS_CTRL_INIT_PHASE_E_SHIFT),
	.init_pos_e = DU_REG(GP_VS_CTRL, GP_VS_CTRL_INIT_POS_E_MASK, GP_VS_CTRL_INIT_POS_E_SHIFT),
	.norm = DU_REG(GP_VS_CTRL, GP_VS_CTRL_NORM_MASK, GP_VS_CTRL_NORM_SHIFT),
	.parity = DU_REG(GP_VS_CTRL, GP_VS_CTRL_PARITY_MASK, GP_VS_CTRL_PARITY_SHIFT),
	.pxl_mode = DU_REG(GP_VS_CTRL, GP_VS_CTRL_PXL_MODE_MASK, GP_VS_CTRL_PXL_MODE_SHIFT),
	.vs_mode = DU_REG(GP_VS_CTRL, GP_VS_CTRL_VS_MODE_MASK, GP_VS_CTRL_VS_MODE_SHIFT),
	.out_vsize = DU_REG(GP_VS_RESV, GP_VS_RESV_VSIZE_MASK, GP_VS_RESV_VSIZE_SHIFT),
	.ratio_inc = DU_REG(GP_VS_INC, GP_VS_INC_INC_MASK, GP_VS_INC_INC_SHIFT),
};

static const struct kunlun_gp_scaler_data gp_scaler_data = {
	.hs_data = &gp_hs_data,
	.vs_data = &gp_vs_data,
};

static const struct kunlun_csc_ctrl gp_csc_data = {
	.alpha = DU_REG(GP_CSC_CTRL, CSC_ALPHA_MASK, CSC_ALPHA_SHIFT),
	.sbup_conv = DU_REG(GP_CSC_CTRL, CSC_SBUP_CONV_MASK, CSC_SBUP_CONV_SHIFT),
	.bypass = DU_REG(GP_CSC_CTRL, CSC_BYPASS_MASK, CSC_BYPASS_SHIFT),
	.a00 = DU_REG(GP_CSC_COEF1, COEF1_A00_MASK, COEF1_A00_SHIFT),
	.a01 = DU_REG(GP_CSC_COEF1, COEF1_A01_MASK, COEF1_A01_SHIFT),
	.a02 = DU_REG(GP_CSC_COEF2, COEF2_A02_MASK, COEF2_A02_SHIFT),
	.a10 = DU_REG(GP_CSC_COEF2, COEF2_A10_MASK, COEF2_A10_SHIFT),
	.a11 = DU_REG(GP_CSC_COEF3, COEF3_A11_MASK, COEF3_A11_SHIFT),
	.a12 = DU_REG(GP_CSC_COEF3, COEF3_A12_MASK, COEF3_A12_SHIFT),
	.a20 = DU_REG(GP_CSC_COEF4, COEF4_A20_MASK, COEF4_A20_SHIFT),
	.a21 = DU_REG(GP_CSC_COEF4, COEF4_A21_MASK, COEF4_A21_SHIFT),
	.a22 = DU_REG(GP_CSC_COEF5, COEF5_A22_MASK, COEF5_A22_SHIFT),
	.b0 = DU_REG(GP_CSC_COEF5, COEF5_B0_MASK, COEF5_B0_SHIFT),
	.b1 = DU_REG(GP_CSC_COEF6, COEF6_B1_MASK, COEF6_B1_SHIFT),
	.b2 = DU_REG(GP_CSC_COEF6, COEF6_B2_MASK, COEF6_B2_SHIFT),
	.c0 = DU_REG(GP_CSC_COEF7, COEF7_C0_MASK, COEF7_C0_SHIFT),
	.c1 = DU_REG(GP_CSC_COEF7, COEF7_C1_MASK, COEF7_C1_SHIFT),
	.c2 = DU_REG(GP_CSC_COEF8, COEF8_C2_MASK, COEF8_C2_SHIFT),
};

static const struct kunlun_gp_fbdc_ctrl dp_fbdc_data = {
	.m_id = DU_REG(GP_FBDC_CTRL, GP_FBDC_CTRL_M_ID_MASK, GP_FBDC_CTRL_M_ID_SHIFT),
	.context = DU_REG(GP_FBDC_CTRL, GP_FBDC_CTRL_CONTEXT_MASK, GP_FBDC_CTRL_CONTEXT_SHIFT),
	.hdr_encoding = DU_REG(GP_FBDC_CTRL, GP_FBDC_CTRL_HDR_ENCODING_MASK, GP_FBDC_CTRL_HDR_ENCODING_SHIFT),
	.twiddle_mode = DU_REG(GP_FBDC_CTRL, GP_FBDC_CTRL_TWIDDLE_MODE_MASK, GP_FBDC_CTRL_TWIDDLE_MODE_SHIFT),
	.meml = DU_REG(GP_FBDC_CTRL, GP_FBDC_CTRL_MEML_MASK, GP_FBDC_CTRL_MEML_SHIFT),
	.fmt = DU_REG(GP_FBDC_CTRL, GP_FBDC_CTRL_FMT_MASK, GP_FBDC_CTRL_FMT_SHIFT),
	.en = DU_REG(GP_FBDC_CTRL, GP_FBDC_CTRL_EN_MASK, GP_FBDC_CTRL_EN_SHIFT),
	.clear0_color = DU_REG(GP_FBDC_FBDC_CLEAR_0, GP_FBDC_FBDC_CLEAR_0_COLOR_MASK, GP_FBDC_FBDC_CLEAR_0_COLOR_SHIFT),
	.clear1_color = DU_REG(GP_FBDC_FBDC_CLEAR_1, GP_FBDC_FBDC_CLEAR_1_COLOR_MASK, GP_FBDC_FBDC_CLEAR_1_COLOR_SHIFT),
};

static const struct kunlun_rle_int_data rle_int_mask = {
	.v_err = DU_REG(SP_RLE_INT_MASK, RLE_INT_V_ERR_MASK, RLE_INT_V_ERR_SHIFT),
	.u_err = DU_REG(SP_RLE_INT_MASK, RLE_INT_U_ERR_MASK, RLE_INT_U_ERR_SHIFT),
	.y_err = DU_REG(SP_RLE_INT_MASK, RLE_INT_Y_ERR_MASK, RLE_INT_Y_ERR_SHIFT),
	.a_err = DU_REG(SP_RLE_INT_MASK, RLE_INT_A_ERR_MASK, RLE_INT_A_ERR_SHIFT),
};

static const struct kunlun_rle_int_data rle_int_status = {
	.v_err = DU_REG(SP_RLE_INT_STATUS, RLE_INT_V_ERR_MASK, RLE_INT_V_ERR_SHIFT),
	.u_err = DU_REG(SP_RLE_INT_STATUS, RLE_INT_V_ERR_MASK, RLE_INT_V_ERR_SHIFT),
	.y_err = DU_REG(SP_RLE_INT_STATUS, RLE_INT_V_ERR_MASK, RLE_INT_V_ERR_SHIFT),
	.a_err = DU_REG(SP_RLE_INT_STATUS, RLE_INT_V_ERR_MASK, RLE_INT_V_ERR_SHIFT),
};

static const struct kunlun_sp_rle_data sp_rle_data = {
	.length = DU_REG(SP_RLE_Y_LEN, RLE_Y_LEN_Y_MASK, RLE_Y_LEN_Y_SHIFT),
	.checksum = DU_REG(SP_RLE_Y_CHECK_SUM, RLE_Y_CHECK_SUM_Y_MASK, RLE_Y_CHECK_SUM_Y_SHIFT),
	.data_size = DU_REG(SP_RLE_CTRL, RLE_DATA_SIZE_MASK, RLE_DATA_SIZE_SHIFT),
	.rle_en = DU_REG(SP_RLE_CTRL, RLE_EN_MASK, RLE_EN_SHIFT),
	.y_checksum = DU_REG(SP_RLE_Y_CHECK_SUM_ST, RLE_CHECK_SUM_MASK, RLE_CHECK_SUM_SHIFT),
	.u_checksum = DU_REG(SP_RLE_U_CHECK_SUM_ST, RLE_CHECK_SUM_MASK, RLE_CHECK_SUM_SHIFT),
	.v_checksum = DU_REG(SP_RLE_V_CHECK_SUM_ST, RLE_CHECK_SUM_MASK, RLE_CHECK_SUM_SHIFT),
	.a_checksum = DU_REG(SP_RLE_A_CHECK_SUM_ST, RLE_CHECK_SUM_MASK, RLE_CHECK_SUM_SHIFT),
	.int_mask = &rle_int_mask,
	.int_status = &rle_int_status,
};
static const struct kunlun_clut_channel_a_data clut_a_data = {
	.has_alpha = DU_REG(SP_CLUT_A_CTRL, CLUT_HAS_ALPHA_MASK, CLUT_HAS_ALPHA_SHIFT),
	.a_y_sel = DU_REG(SP_CLUT_A_CTRL, CLUT_A_Y_SEL_MASK, CLUT_A_Y_SEL_SHIFT),
	.bypass = DU_REG(SP_CLUT_A_CTRL, CLUT_A_BYPASS_MASK, CLUT_A_BYPASS_SHIFT),
	.offset = DU_REG(SP_CLUT_A_CTRL, CLUT_A_OFFSET_MASK, CLUT_A_OFFSET_SHIFT),
	.depth = DU_REG(SP_CLUT_A_CTRL, CLUT_A_DEPTH_MASK, CLUT_A_DEPTH_SHIFT),
};
static const struct kunlun_clut_channel_y_data clut_y_data = {
	.bypass = DU_REG(CLUT_Y_CTRL, CLUT_Y_BYPASS_MASK, CLUT_Y_BYPASS_SHIFT),
	.offset = DU_REG(CLUT_Y_CTRL, CLUT_Y_OFFSET_MASK, CLUT_Y_OFFSET_SHIFT),
	.depth = DU_REG(CLUT_Y_CTRL, CLUT_Y_DEPTH_MASK, CLUT_Y_DEPTH_SHIFT),
};

static const struct kunlun_clut_channel_u_data clut_u_data = {
	.u_y_sel = DU_REG(CLUT_U_CTRL, CLUT_U_Y_SEL_MASK, CLUT_U_Y_SEL_SHIFT),
	.bypass = DU_REG(CLUT_U_CTRL, CLUT_U_BYPASS_MASK, CLUT_U_BYPASS_SHIFT),
	.offset = DU_REG(CLUT_U_CTRL, CLUT_U_OFFSET_MASK, CLUT_U_OFFSET_SHIFT),
	.depth = DU_REG(CLUT_U_CTRL, CLUT_U_DEPTH_MASK, CLUT_U_DEPTH_SHIFT),
};

static const struct kunlun_clut_channel_v_data clut_v_data = {
	.v_y_sel = DU_REG(CLUT_V_CTRL, CLUT_V_Y_SEL_MASK, CLUT_V_Y_SEL_SHIFT),
	.bypass = DU_REG(CLUT_V_CTRL, CLUT_V_BYPASS_MASK, CLUT_V_BYPASS_SHIFT),
	.offset = DU_REG(CLUT_V_CTRL, CLUT_V_OFFSET_MASK, CLUT_V_OFFSET_SHIFT),
	.depth = DU_REG(CLUT_V_CTRL, CLUT_V_DEPTH_MASK, CLUT_V_DEPTH_SHIFT),
};

static const struct kunlun_clut_read_ctrl clut_read_ctrl = {
	.v_sel = DU_REG(CLUT_READ_CTRL, CLUT_APB_SEL_MASK, CLUT_APB_SEL_SHIFT),
	.u_sel = DU_REG(CLUT_READ_CTRL, CLUT_APB_SEL_MASK, CLUT_APB_SEL_SHIFT),
	.y_sel = DU_REG(CLUT_READ_CTRL, CLUT_APB_SEL_MASK, CLUT_APB_SEL_SHIFT),
	.a_sel = DU_REG(CLUT_READ_CTRL, CLUT_APB_SEL_MASK, CLUT_APB_SEL_SHIFT),
};


static const struct kunlun_sp_clut_data sp_clut_data = {
	.a_data = &clut_a_data,
	.y_data = &clut_y_data,
	.u_data = &clut_u_data,
	.v_data = &clut_v_data,
	.read_ctrl = &clut_read_ctrl,
	.baddr_l = DU_REG(CLUT_BADDRL, CLIT_BADDRL_MASK, CLUT_BADDRL_SHIFT),
	.baddr_h = DU_REG(CLUT_BADDRH, CLIT_BADDRH_MASK, CLUT_BADDRH_SHIFT),
	.load_en = DU_REG(CLUT_LOAD_CTRL, CLUT_LOAD_CTRL_EN_MASK, CLUT_LOAD_CTRL_EN_SHIFT),
};

static const struct kunlun_gp_data dc_gp = {
	.pix_comp  = &gp_pix_comp,
	.frm_ctrl = &gp_frm_ctrl,
	.frm_info = &gp_frm_data,
	.yuvup_ctrl = &gp_yuvup_data,
	.csc_ctrl = &gp_csc_data,
	.sw_rst = DU_REG(GP_SWT_RST, SW_RST_MASK, SW_RST_SHIFT),
	.sdw_trig = DU_REG(GP_SDW_TRIG, SDW_CTRL_TRIG_MASK, SDW_CTRL_TRIG_SHIFT),
};

static const struct kunlun_sp_data dc_sp = {
	.pix_comp  = &sp_pix_comp,
	.frm_ctrl = &sp_frm_ctrl,
	.frm_info = &sp_frm_data,
	.rle_data = &sp_rle_data,
	.clut_data = &sp_clut_data,
	.sw_rst = DU_REG(SP_SWT_RST, SW_RST_MASK, SW_RST_SHIFT),
	.sdw_trig = DU_REG(SP_SDW_TRIG, SDW_CTRL_TRIG_MASK, SDW_CTRL_TRIG_SHIFT),
};

static const struct kunlun_gp_data dp_gp0 = {
	.pix_comp  = &gp_pix_comp,
	.frm_ctrl = &dp_gp_frm_ctrl,
	.frm_info = &gp_frm_data,
	.yuvup_ctrl = &gp_yuvup_data,
	.crop_ctrl = &gp_crop_data,
	.scaler = &gp_scaler_data,
	.csc_ctrl = &gp_csc_data,
	.fbdc_ctrl = &dp_fbdc_data,
	.sw_rst = DU_REG(GP_SWT_RST, SW_RST_MASK, SW_RST_SHIFT),
	.sdw_trig = DU_REG(GP_SDW_TRIG, SDW_CTRL_TRIG_MASK, SDW_CTRL_TRIG_SHIFT),
};

static const struct kunlun_gp_data dp_gp1 = {
	.pix_comp  = &gp_pix_comp,
	.frm_ctrl = &dp_gp_frm_ctrl,
	.frm_info = &gp_frm_data,
	.crop_ctrl = &gp_crop_data,
	.fbdc_ctrl = &dp_fbdc_data,
	.sw_rst = DU_REG(GP_SWT_RST, SW_RST_MASK, SW_RST_SHIFT),
	.sdw_trig = DU_REG(GP_SDW_TRIG, SDW_CTRL_TRIG_MASK, SDW_CTRL_TRIG_SHIFT),
};

const uint32_t sp_formats[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_BGRX8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_BGRA8888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_BGR565,
	DRM_FORMAT_XRGB4444,
	DRM_FORMAT_XBGR4444,
	DRM_FORMAT_BGRX4444,
	DRM_FORMAT_ARGB4444,
	DRM_FORMAT_ABGR4444,
	DRM_FORMAT_BGRA4444,
	DRM_FORMAT_XRGB1555,
	DRM_FORMAT_XBGR1555,
	DRM_FORMAT_BGRX5551,
	DRM_FORMAT_ARGB1555,
	DRM_FORMAT_ABGR1555,
	DRM_FORMAT_BGRA5551,
	DRM_FORMAT_XRGB2101010,
	DRM_FORMAT_XBGR2101010,
	DRM_FORMAT_BGRX1010102,
	DRM_FORMAT_ARGB2101010,
	DRM_FORMAT_ABGR2101010,
	DRM_FORMAT_BGRA1010102,
	DRM_FORMAT_R8
};
const uint32_t gp_lite_formats[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_BGRX8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_BGRA8888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_BGR565,
	DRM_FORMAT_XRGB4444,
	DRM_FORMAT_XBGR4444,
	DRM_FORMAT_BGRX4444,
	DRM_FORMAT_ARGB4444,
	DRM_FORMAT_ABGR4444,
	DRM_FORMAT_BGRA4444,
	DRM_FORMAT_XRGB1555,
	DRM_FORMAT_XBGR1555,
	DRM_FORMAT_BGRX5551,
	DRM_FORMAT_ARGB1555,
	DRM_FORMAT_ABGR1555,
	DRM_FORMAT_BGRA5551,
	DRM_FORMAT_XRGB2101010,
	DRM_FORMAT_XBGR2101010,
	DRM_FORMAT_BGRX1010102,
	DRM_FORMAT_ARGB2101010,
	DRM_FORMAT_ABGR2101010,
	DRM_FORMAT_BGRA1010102,
	DRM_FORMAT_R8,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_VYUY,
	DRM_FORMAT_AYUV,
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21,
	DRM_FORMAT_YUV420,
	DRM_FORMAT_YUV422,
	DRM_FORMAT_YUV444,
};

const uint32_t gp_big_formats[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_BGRX8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_BGRA8888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_BGR565,
	DRM_FORMAT_XRGB4444,
	DRM_FORMAT_XBGR4444,
	DRM_FORMAT_BGRX4444,
	DRM_FORMAT_ARGB4444,
	DRM_FORMAT_ABGR4444,
	DRM_FORMAT_BGRA4444,
	DRM_FORMAT_XRGB1555,
	DRM_FORMAT_XBGR1555,
	DRM_FORMAT_BGRX5551,
	DRM_FORMAT_ARGB1555,
	DRM_FORMAT_ABGR1555,
	DRM_FORMAT_BGRA5551,
	DRM_FORMAT_XRGB2101010,
	DRM_FORMAT_XBGR2101010,
	DRM_FORMAT_BGRX1010102,
	DRM_FORMAT_ARGB2101010,
	DRM_FORMAT_ABGR2101010,
	DRM_FORMAT_BGRA1010102,
	DRM_FORMAT_R8,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_VYUY,
	DRM_FORMAT_AYUV,
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21,
	DRM_FORMAT_YUV420,
	DRM_FORMAT_YUV422,
	DRM_FORMAT_YUV444,
};
const uint32_t gp_mid_formats[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_BGRX8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_BGRA8888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_BGR565,
	DRM_FORMAT_XRGB4444,
	DRM_FORMAT_XBGR4444,
	DRM_FORMAT_BGRX4444,
	DRM_FORMAT_ARGB4444,
	DRM_FORMAT_ABGR4444,
	DRM_FORMAT_BGRA4444,
	DRM_FORMAT_XRGB1555,
	DRM_FORMAT_XBGR1555,
	DRM_FORMAT_BGRX5551,
	DRM_FORMAT_ARGB1555,
	DRM_FORMAT_ABGR1555,
	DRM_FORMAT_BGRA5551,
	DRM_FORMAT_XRGB2101010,
	DRM_FORMAT_XBGR2101010,
	DRM_FORMAT_BGRX1010102,
	DRM_FORMAT_ARGB2101010,
	DRM_FORMAT_ABGR2101010,
	DRM_FORMAT_BGRA1010102,
	DRM_FORMAT_R8,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_VYUY,
	DRM_FORMAT_AYUV,
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21,
	DRM_FORMAT_YUV420,
	DRM_FORMAT_YUV422,
	DRM_FORMAT_YUV444,
};

static const struct kunlun_pipe_data sp_data = {
	.formats = sp_formats,
	.nformats = ARRAY_SIZE(sp_formats),
	.data.sp = &dc_sp,
};

static const struct kunlun_pipe_data gp_lite_data = {
	.formats = gp_lite_formats,
	.nformats = ARRAY_SIZE(gp_lite_formats),
	.data.gp = &dc_gp,
};

static const struct kunlun_pipe_data gp_big_data = {
	.formats = gp_big_formats,
	.nformats = ARRAY_SIZE(gp_big_formats),
	.data.gp = &dp_gp0,
};

static const struct kunlun_pipe_data gp_mid_data = {
	.formats = gp_mid_formats,
	.nformats = ARRAY_SIZE(gp_mid_formats),
	.data.gp = &dp_gp1,
};

const struct kunlun_plane_data kunlun_dc_planes[] = {
	{
		.chn_base = DC_SP_CHN_BASE,
		.id = DC_SPIPE,
		.type = DRM_PLANE_TYPE_PRIMARY,
		.hwpipe = &sp_data,
	},
	{
		.chn_base = DC_GP_CHN_BASE,
		.id = DC_GPIPE,
		.type = DRM_PLANE_TYPE_OVERLAY,
		.hwpipe = &gp_lite_data,
	}
};

const struct kunlun_plane_data kunlun_dp_planes[] = {
	{
		.chn_base = DP_GP0_CHN_BASE,
		.id = DP_GPIPE0,
		.type = DRM_PLANE_TYPE_PRIMARY,
		.hwpipe = &gp_big_data,
	},
	{
		.chn_base = DP_GP1_CHN_BASE,
		.id = DP_GPIPE1,
		.type = DRM_PLANE_TYPE_OVERLAY,
		.hwpipe = &gp_mid_data,
	},
	{
		.chn_base = DP_SP0_CHN_BASE,
		.id = DP_SPIPE0,
		.type = DRM_PLANE_TYPE_OVERLAY,
		.hwpipe = &sp_data,
	},
	{
		.chn_base = DP_SP1_CHN_BASE,
		.id = DP_SPIPE1,
		.type = DRM_PLANE_TYPE_OVERLAY,
		.hwpipe = &sp_data,
	}
};

static uint32_t get_l_addr(uint64_t addr)
{
	uint32_t addr_l;

	addr_l = (uint32_t)(addr & 0x00000000FFFFFFFF);
	return addr_l;
}

static uint32_t get_h_addr(uint64_t addr)
{
	uint32_t addr_h;

	addr_h = (uint32_t)((addr & 0xFFFFFFFF00000000) >> 32);
	return addr_h;
}

static int kunlun_plane_frm_comp_set(void __iomem *regs,
		uint32_t chn_base,
		const struct kunlun_pipe_pix_comp *comp,
		const struct kunlun_pipe_frm_ctrl *ctrl,
		const struct kunlun_gp_yuvup_ctrl *yuvup_ctrl,
		const struct kunlun_csc_ctrl *csc_ctrl,
		uint32_t format)
{
	uint8_t bpa, bpy, bpu, bpv;
	uint8_t swap;
	uint8_t uvswap = 0;
	uint8_t is_yuv = 0;
	uint8_t uvmode = 0;
	uint8_t buf_fmt = 0;
	char *format_string;

	switch (format) {
		case DRM_FORMAT_XRGB8888:
			bpa = bpy = bpu = bpv = 8;
			swap = SWAP_A_RGB;
			format_string = "DRM_FORMAT_XRGB8888";
			break;
		case DRM_FORMAT_XBGR8888:
			bpa = bpy = bpu = bpv = 8;
			swap = SWAP_A_RBG;
			format_string = "DRM_FORMAT_XBGR8888";
			break;
		case DRM_FORMAT_BGRX8888:
			bpa = bpy = bpu = bpv = 8;
			swap = SWAP_B_GRA;
			format_string = "DRM_FORMAT_BGRX8888";
			break;
		case DRM_FORMAT_ARGB8888:
			bpa = bpy = bpu = bpv = 8;
			swap = SWAP_A_RGB;
			format_string = "DRM_FORMAT_ARGB8888";
			break;
		case DRM_FORMAT_ABGR8888:
			bpa = bpy = bpu = bpv = 8;
			swap = SWAP_A_BGR;
			format_string = "DRM_FORMAT_ABGR8888";
			break;
		case DRM_FORMAT_BGRA8888:
			bpa = bpy = bpu = bpv = 8;
			swap = SWAP_B_GRA;
			format_string = "DRM_FORMAT_BGRA8888";
			break;
		case DRM_FORMAT_RGB888:
			bpa = 0;
			bpy = bpu = bpv = 8;
			swap = SWAP_A_RGB;
			format_string = "DRM_FORMAT_RGB888";
			break;
		case DRM_FORMAT_BGR888:
			bpa = 0;
			bpy = bpu = bpv = 8;
			swap = SWAP_A_BGR;
			format_string = "DRM_FORMAT_BGR888";
			break;
		case DRM_FORMAT_RGB565:
			bpa = 0;
			bpy = bpv = 5;
			bpu = 6;
			swap = SWAP_A_RGB;
			format_string = "DRM_FORMAT_RGB565";
			break;
		case DRM_FORMAT_BGR565:
			bpa = 0;
			bpy = bpv = 5;
			bpu = 6;
			swap = SWAP_A_BGR;
			format_string = "DRM_FORMAT_BGR565";
			break;
		case DRM_FORMAT_XRGB4444:
			bpa = bpy = bpu = bpv = 4;
			swap = SWAP_A_RGB;
			format_string = "DRM_FORMAT_XRGB4444";
			break;
		case DRM_FORMAT_XBGR4444:
			bpa = bpy = bpu = bpv = 4;
			swap = SWAP_A_BGR;
			format_string = "DRM_FORMAT_XBGR4444";
			break;
		case DRM_FORMAT_BGRX4444:
			bpa = bpy = bpu = bpv = 4;
			swap = SWAP_B_GRA;
			format_string = "DRM_FORMAT_BGRX4444";
			break;
		case DRM_FORMAT_ARGB4444:
			bpa = bpy = bpu = bpv = 4;
			swap = SWAP_A_RGB;
			format_string = "DRM_FORMAT_ARGB4444";
			break;
		case DRM_FORMAT_ABGR4444:
			bpa = bpy = bpu = bpv = 4;
			swap = SWAP_A_BGR;
			format_string = "DRM_FORMAT_ABGR4444";
			break;
		case DRM_FORMAT_BGRA4444:
			bpa = bpy = bpu = bpv = 4;
			swap = SWAP_B_GRA;
			format_string = "DRM_FORMAT_BGRA4444";
			break;
		case DRM_FORMAT_XRGB1555:
			bpa = 1;
			bpy = bpu = bpv = 5;
			swap = SWAP_A_RGB;
			format_string = "DRM_FORMAT_XRGB1555";
			break;
		case DRM_FORMAT_XBGR1555:
			bpa = 1;
			bpy = bpu = bpv = 5;
			swap = SWAP_A_BGR;
			format_string = "DRM_FORMAT_XBGR1555";
			break;
		case DRM_FORMAT_BGRX5551:
			bpa = 1;
			bpy = bpu = bpv = 5;
			swap = SWAP_B_GRA;
			format_string = "DRM_FORMAT_BGRX5551";
			break;
		case DRM_FORMAT_ARGB1555:
			bpa = 1;
			bpy = bpu = bpv = 5;
			swap = SWAP_A_RGB;
			format_string = "DRM_FORMAT_ARGB1555";
			break;
		case DRM_FORMAT_ABGR1555:
			bpa = 1;
			bpy = bpu = bpv = 5;
			swap = SWAP_A_BGR;
			format_string = "DRM_FORMAT_ABGR1555";
			break;
		case DRM_FORMAT_BGRA5551:
			bpa = 1;
			bpy = bpu = bpv = 5;
			swap = SWAP_B_GRA;
			format_string = "DRM_FORMAT_BGRA5551";
			break;
		case DRM_FORMAT_XRGB2101010:
			bpa = 2;
			bpy = bpu = bpv = 10;
			swap = SWAP_A_RGB;
			format_string = "DRM_FORMAT_XRGB2101010";
			break;
		case DRM_FORMAT_XBGR2101010:
			bpa = 2;
			bpy = bpu = bpv = 10;
			swap = SWAP_A_BGR;
			format_string = "DRM_FORMAT_XBGR2101010";
			break;
		case DRM_FORMAT_BGRX1010102:
			bpa = 2;
			bpy = bpu = bpv = 10;
			swap = SWAP_B_GRA;
			format_string = "DRM_FORMAT_BGRX1010102";
			break;
		case DRM_FORMAT_ARGB2101010:
			bpa = 2;
			bpy = bpu = bpv = 10;
			swap = SWAP_A_RGB;
			format_string = "DRM_FORMAT_ARGB2101010";
			break;
		case DRM_FORMAT_ABGR2101010:
			bpa = 2;
			bpy = bpu = bpv = 10;
			swap = SWAP_A_BGR;
			format_string = "DRM_FORMAT_ABGR2101010";
			break;
		case DRM_FORMAT_BGRA1010102:
			bpa = 2;
			bpy = bpu = bpv = 10;
			swap = SWAP_B_GRA;
			format_string = "DRM_FORMAT_BGRA1010102";
			break;
		case DRM_FORMAT_R8:
			bpy = 8;
			bpa = bpu = bpv = 0;
			swap = SWAP_A_RGB;
			format_string = "DRM_FORMAT_R8";
			break;
		case DRM_FORMAT_UYVY:
			bpa = bpv = 0;
			bpy = bpu = 8;
			swap = SWAP_A_RGB;
			is_yuv = 1;
			uvmode = UV_YUV422;
			format_string = "DRM_FORMAT_UYVY";
			break;
		case DRM_FORMAT_VYUY:
			bpa = bpv = 0;
			bpy = bpu = 8;
			swap = SWAP_A_RGB;
			uvswap = 1;
			is_yuv = 1;
			uvmode = UV_YUV422;
			format_string = "DRM_FORMAT_VYUY";
			break;
		case DRM_FORMAT_AYUV:
			bpa = bpy = bpu = bpv = 8;
			swap = SWAP_A_RGB;
			is_yuv = 1;
			format_string = "DRM_FORMAT_AYUV";
			break;
		case DRM_FORMAT_NV12:
			bpa = bpv = 0;
			bpy = bpu = 8;
			swap = SWAP_A_RGB;
			uvswap = 1;
			is_yuv = 1;
			uvmode = UV_YUV420;
			buf_fmt = FMT_SEMI_PLANAR;
			format_string = "DRM_FORMAT_NV12";
			break;
		case DRM_FORMAT_NV21:
			bpa = bpv = 0;
			bpy = bpu = 8;
			swap = SWAP_A_RGB;
			is_yuv = 1;
			uvmode = UV_YUV420;
			buf_fmt = FMT_SEMI_PLANAR;
			format_string = "DRM_FORMAT_NV21";
			break;
		case DRM_FORMAT_YUV420:
			bpa = 0;
			bpy = bpu = bpv = 8;
			swap = SWAP_A_RGB;
			is_yuv = 1;
			uvmode = UV_YUV420;
			buf_fmt = FMT_PLANAR;
			format_string = "DRM_FORMAT_YUV420";
			break;
		case DRM_FORMAT_YUV422:
			bpa = 0;
			bpy = bpu = bpv = 8;
			swap = SWAP_A_RGB;
			is_yuv = 1;
			uvmode = UV_YUV420;
			buf_fmt = FMT_PLANAR;
			format_string = "DRM_FORMAT_YUV422";
			break;
		case DRM_FORMAT_YUV444:
			bpa = 0;
			bpy = bpu = bpv = 8;
			swap = SWAP_A_RGB;
			is_yuv = 1;
			buf_fmt = FMT_PLANAR;
			format_string = "DRM_FORMAT_YUV444";
			break;

		default:
			DRM_ERROR("unsupported format[%08x]\n", format);
			return -EINVAL;
	}
	//pr_info("format = %s", format_string);
	/*config pixel components*/
	DU_PIPE_ELEM_SET(regs, chn_base, comp, bpv, bpv);
	DU_PIPE_ELEM_SET(regs, chn_base, comp, bpu, bpu);
	DU_PIPE_ELEM_SET(regs, chn_base, comp, bpy, bpy);
	DU_PIPE_ELEM_SET(regs, chn_base, comp, bpa, bpa);

	/*config component swap mode*/
	DU_PIPE_ELEM_SET(regs, chn_base, ctrl, comp_swap, swap);
	DU_PIPE_ELEM_SET(regs, chn_base, ctrl, rgb_yuv, is_yuv);
	DU_PIPE_ELEM_SET(regs, chn_base, ctrl, uv_swap, uvswap);
	DU_PIPE_ELEM_SET(regs, chn_base, ctrl, uv_mode, uvmode);
	DU_PIPE_ELEM_SET(regs, chn_base, ctrl, fmt, buf_fmt);

	if ((yuvup_ctrl != NULL) && (csc_ctrl != NULL)) {
		if (is_yuv) {
			DU_PIPE_ELEM_SET(regs, chn_base, yuvup_ctrl, bypass, 0);
			DU_PIPE_ELEM_SET(regs, chn_base, csc_ctrl, bypass, 0);
		} else {
			DU_PIPE_ELEM_SET(regs, chn_base, yuvup_ctrl, bypass, 1);
			DU_PIPE_ELEM_SET(regs, chn_base, csc_ctrl, bypass, 1);
		}
	}

	return 0;
}

static bool kunlun_format_global_alpha(uint32_t format)
{
	int i;
	for (i = 0; i < sizeof(format); i++) {
		char tmp = (format >> (8 * i)) & 0xff;

		if (tmp == 'A')
			return false;
	}

	return true;
}

static int get_du_mlc_sf_index(uint32_t layer_id)
{
	uint32_t i;
	switch (layer_id) {
		case DC_SPIPE:
		case DP_GPIPE0:
			i = 0;
			break;
		case DC_GPIPE:
		case DP_GPIPE1:
			i = 1;
			break;
		case DP_SPIPE0:
			i = 2;
			break;
		case DP_SPIPE1:
			i = 3;
			break;
		default:
			DRM_ERROR("unsupported layer id [%d]\n", layer_id);
			return -EINVAL;
	}
	return i;
}

static void kunlun_spipe_init(const struct kunlun_sp_data *sp_data,
		void __iomem *regs, uint32_t chn_base)
{
	DU_PIPE_ELEM_SET(regs, chn_base, sp_data->clut_data->a_data, bypass, 1);
	DU_PIPE_ELEM_SET(regs, chn_base, sp_data->clut_data->y_data, bypass, 1);
	DU_PIPE_ELEM_SET(regs, chn_base, sp_data->clut_data->u_data, bypass, 1);
	DU_PIPE_ELEM_SET(regs, chn_base, sp_data->clut_data->v_data, bypass, 1);
	DU_PIPE_ELEM_SET(regs, chn_base, sp_data, sdw_trig, 1);
}

static void kunlun_gpipe_init(const struct kunlun_gp_data *gp_data,
		void __iomem *regs, uint32_t chn_base, int type)
{
	if (type == DC_GPIPE || type == DP_GPIPE0) {
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->yuvup_ctrl, bypass, 1);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->csc_ctrl, bypass, 1);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->csc_ctrl, a00, csc_param[0][0]);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->csc_ctrl, a01, csc_param[0][1]);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->csc_ctrl, a02, csc_param[0][2]);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->csc_ctrl, a10, csc_param[0][3]);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->csc_ctrl, a11, csc_param[0][4]);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->csc_ctrl, a12, csc_param[0][5]);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->csc_ctrl, a20, csc_param[0][6]);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->csc_ctrl, a21, csc_param[0][7]);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->csc_ctrl, a22, csc_param[0][8]);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->csc_ctrl, b0, csc_param[0][9]);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->csc_ctrl, b1, csc_param[0][10]);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->csc_ctrl, b2, csc_param[0][11]);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->csc_ctrl, c0, csc_param[0][12]);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->csc_ctrl, c1, csc_param[0][13]);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->csc_ctrl, c2, csc_param[0][14]);
	}
	if (type == DP_GPIPE0) {
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->crop_ctrl, bypass, 1);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->scaler->hs_data, nor_para, 0);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->scaler->vs_data, norm, 0);
	}

	if ((type == DP_GPIPE0) || (type == DP_GPIPE1)) {
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->fbdc_ctrl, en, 0);
	}

	DU_PIPE_ELEM_SET(regs, chn_base, gp_data, sdw_trig, 1);
}

void kunlun_planes_init(struct kunlun_crtc *kcrtc)
{
	const struct kunlun_crtc_du_data *dc_data = kcrtc->data->dc_data;
	const struct kunlun_crtc_du_data *dp_data = kcrtc->data->dp_data;
	const struct kunlun_sp_data *sp_data;
	const struct kunlun_gp_data *gp_data;
	void __iomem *regs;
	uint32_t chn_base;
	int layer_id;
	int i;

	for (i = 0; i < kcrtc->num_planes; i++) {
		if (kcrtc->ctrl_unit) {
			regs = kcrtc->dp_regs;
			layer_id = dp_data->planes[i].id;
			chn_base = dp_data->planes[i].chn_base;
			if ((layer_id == DP_SPIPE0) || (layer_id == DP_SPIPE1)) {
				sp_data = dp_data->planes[i].hwpipe->data.sp;

				kunlun_spipe_init(sp_data, regs, chn_base);
			}

			if ((layer_id == DP_GPIPE0) || (layer_id == DP_GPIPE1)) {
				gp_data = dp_data->planes[i].hwpipe->data.gp;

				kunlun_gpipe_init(gp_data, regs, chn_base, layer_id);
			}
		} else {
			regs = kcrtc->dc_regs;
			layer_id = dc_data->planes[i].id;
			chn_base = dc_data->planes[i].chn_base;
			if (layer_id == DC_SPIPE) {
				sp_data = dc_data->planes[i].hwpipe->data.sp;

				kunlun_spipe_init(sp_data, regs, chn_base);
			}

			if (layer_id == DC_GPIPE) {
				gp_data = dc_data->planes[i].hwpipe->data.gp;

				kunlun_gpipe_init(gp_data, regs, chn_base, layer_id);
			}
		}
	}
}

static int kunlun_plane_format_check(struct drm_plane *plane,
				uint32_t format)
{
	uint32_t i;
	struct kunlun_plane *klp = to_kunlun_plane(plane);
	const struct kunlun_plane_data *kpd = klp->data;

	for (i = 0; i < kpd->hwpipe->nformats; i++) {
		if (format == kpd->hwpipe->formats[i])
			break;
	}
	if (i == kpd->hwpipe->nformats) {
		DRM_ERROR("unsupported format[%08x]\n", format);
		return -EINVAL;
	}

	return 0;
}

static int kunlun_plane_atomic_check(struct drm_plane *plane,
				struct drm_plane_state *state)
{
	int ret;
	struct drm_crtc *crtc = state->crtc;
	struct drm_crtc_state *crtc_state;
	struct drm_framebuffer *fb = state->fb;
	struct kunlun_plane *klp = to_kunlun_plane(plane);
	const struct kunlun_plane_data *kpd = klp->data;
	struct drm_rect clip;
	int min_scale = DRM_PLANE_HELPER_NO_SCALING;
	int max_scale = DRM_PLANE_HELPER_NO_SCALING;

	if (kpd->id ==  DP_GPIPE0) {
		min_scale = FRAC_16_16(1, 8);
		max_scale = FRAC_16_16(8, 1);
	}

	if (!crtc || !fb)
		return 0;

	crtc_state = drm_atomic_get_existing_crtc_state(state->state, crtc);
	if (WARN_ON(!crtc_state))
		return -EINVAL;

	clip.x1 = 0;
	clip.y1 = 0;
	clip.x2 = crtc_state->adjusted_mode.hdisplay;
	clip.y2 = crtc_state->adjusted_mode.vdisplay;

	ret = drm_plane_helper_check_state(state, &clip,
					min_scale, max_scale,
					true, true);
	if (ret)
		return ret;

	if (!state->visible)
		return -EINVAL;

	ret = kunlun_plane_format_check(plane, fb->format->format);
	if (ret)
		return ret;

	return 0;
}

static void kunlun_plane_atomic_disable(struct drm_plane *plane,
					 struct drm_plane_state *old_state);
static void kunlun_plane_atomic_update(struct drm_plane *plane,
		struct drm_plane_state *old_state)
{
	struct drm_plane_state *state = plane->state;
	struct drm_crtc *crtc = state->crtc;
	struct kunlun_plane *klp = to_kunlun_plane(plane);
//	const struct kunlun_plane_data *kpd = klp->data;
	uint32_t chn_base = klp->data->chn_base;
	uint32_t layer_id = klp->data->id;
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(state->crtc);
	struct drm_framebuffer *fb = state->fb;
	struct drm_rect *src = &state->src;
//	struct drm_rect *dest = &state->dst;
	struct drm_gem_object *obj;
	struct kunlun_gem_object *gem_obj;
	uint32_t pitch, format;
	dma_addr_t dma_addr;
	uint32_t addr_l, addr_h;
	uint32_t src_w, src_h;
	uint32_t off_x, off_y;
	uint32_t crtc_x, crtc_y;
	uint32_t nplanes;
	const struct kunlun_sp_data *sp_data = NULL;
	const struct kunlun_gp_data *gp_data = NULL;
	const struct kunlun_pipe_pix_comp *pix_comp;
	const struct kunlun_pipe_frm_ctrl *frm_ctrl;
	const struct kunlun_pipe_frm_data *frm_data;
	const struct kunlun_gp_yuvup_ctrl *yuvup_ctrl = NULL;
	const struct kunlun_csc_ctrl *csc_ctrl = NULL;
	const struct kunlun_mlc_data *dc_mlc = kcrtc->data->dc_data->mlc;
	const struct kunlun_mlc_data *dp_mlc = kcrtc->data->dp_data->mlc;
	struct kunlun_mlc_data *mlc;
	void __iomem *regs;
	int i;
	/*
	 * can't update plane when du is disabled.
	 */
	if (WARN_ON(!crtc))
		return;

	if (!state->visible) {
		kunlun_plane_atomic_disable(plane, old_state);
		return;
	}

	if (kcrtc->ctrl_unit)
		regs = kcrtc->dp_regs;
	else
		regs = kcrtc->dc_regs;
	if (!regs) {
		pr_err("This plane's regs is NULL\n");
		return;
	}

	format = fb->format->format;
	nplanes = fb->format->num_planes;
	/*src values are in Q16 fixed point, convert to integer:*/
	off_x = src->x1 >> 16;
	off_y = src->y1 >> 16;
	src_w = (drm_rect_width(src) >> 16) - 1;
	src_h = (drm_rect_height(src) >> 16) - 1;

	crtc_x = state->crtc_x;
	crtc_y = state->crtc_y;

	obj = drm_gem_fb_get_obj(fb, 0);
	gem_obj = to_kunlun_obj(obj);

	dma_addr = gem_obj->paddr + fb->offsets[0];
	addr_l = get_l_addr(dma_addr);
	addr_h = get_h_addr(dma_addr);
	pitch = fb->pitches[0] - 1;

	if ((layer_id == DC_SPIPE) ||
		(layer_id == DP_SPIPE0) ||
		(layer_id == DP_SPIPE1)) {
		sp_data = klp->data->hwpipe->data.sp;
		pix_comp = sp_data->pix_comp;
		frm_ctrl = sp_data->frm_ctrl;
		frm_data = sp_data->frm_info;
	} else {
		gp_data = klp->data->hwpipe->data.gp;
		pix_comp = gp_data->pix_comp;
		frm_ctrl = gp_data->frm_ctrl;
		frm_data = gp_data->frm_info;
		yuvup_ctrl = gp_data->yuvup_ctrl;
		csc_ctrl = gp_data->csc_ctrl;
	}

	kunlun_plane_frm_comp_set(regs, chn_base, pix_comp, frm_ctrl,
			yuvup_ctrl, csc_ctrl, format);
	DU_PIPE_ELEM_SET(regs, chn_base, frm_data, height, src_h);
	DU_PIPE_ELEM_SET(regs, chn_base, frm_data, width, src_w);
	DU_PIPE_ELEM_SET(regs, chn_base, frm_data, y_baddr_l, addr_l);
	DU_PIPE_ELEM_SET(regs, chn_base, frm_data, y_baddr_h, addr_h);
	DU_PIPE_ELEM_SET(regs, chn_base, frm_data, y_stride, pitch);
	DU_PIPE_ELEM_SET(regs, chn_base, frm_data, offset_x, off_x);
	DU_PIPE_ELEM_SET(regs, chn_base, frm_data, offset_y, off_y);

	if (nplanes > 1) {
		obj = drm_gem_fb_get_obj(fb, 1);
		gem_obj = to_kunlun_obj(obj);

		dma_addr = gem_obj->paddr + fb->offsets[1];
		addr_l = get_l_addr(dma_addr);
		addr_h = get_h_addr(dma_addr);
		pitch = fb->pitches[1] - 1;

		DU_PIPE_ELEM_SET(regs, chn_base, frm_data, u_baddr_l, addr_l);
		DU_PIPE_ELEM_SET(regs, chn_base, frm_data, u_baddr_h, addr_h);
		DU_PIPE_ELEM_SET(regs, chn_base, frm_data, u_stride, pitch);
	}

	if (nplanes > 2) {
		obj = drm_gem_fb_get_obj(fb, 2);
		gem_obj = to_kunlun_obj(obj);

		dma_addr = gem_obj->paddr + fb->offsets[2];
		addr_l = get_l_addr(dma_addr);
		addr_h = get_h_addr(dma_addr);
		pitch = fb->pitches[2] - 1;

		DU_PIPE_ELEM_SET(regs, chn_base, frm_data, v_baddr_l, addr_l);
		DU_PIPE_ELEM_SET(regs, chn_base, frm_data, v_baddr_h, addr_h);
		DU_PIPE_ELEM_SET(regs, chn_base, frm_data, v_stride, pitch);
	}

	if (layer_id == DP_GPIPE0) {
		/*hscaler*/
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->scaler->hs_data, out_hsize, src_w);
		/*crop*/
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->crop_ctrl, hsize, src_w);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->crop_ctrl, vsize, src_h);
		/*vscaler*/
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->scaler->vs_data, out_vsize, src_h);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data, sdw_trig, 1);
	} else if (layer_id == DP_GPIPE1) {
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->crop_ctrl, hsize, src_w);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data->crop_ctrl, vsize, src_h);
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data, sdw_trig, 1);
	} else if (layer_id == DC_GPIPE) {
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data, sdw_trig, 1);
	} else {
		DU_PIPE_ELEM_SET(regs, chn_base, sp_data, sdw_trig, 1);
	}

	if (kcrtc->ctrl_unit)
		mlc = dp_mlc;
	else
		mlc = dc_mlc;

	i = get_du_mlc_sf_index(layer_id);
	if (i < 0)
		return;
	DU_MLC_LAYER_SET(regs, mlc, i, h_pos, crtc_x);
	DU_MLC_LAYER_SET(regs, mlc, i, v_pos, crtc_y);
	DU_MLC_LAYER_SET(regs, mlc, i, h_size, src_w);
	DU_MLC_LAYER_SET(regs, mlc, i, v_size, src_h);

	/*set the 'enable layer' bit*/
	DU_MLC_LAYER_SET(regs, mlc, i, sf_en, 1);

	if (klp->plane_status != PLANE_ENABLE)
		klp->plane_status = PLANE_ENABLE;
}

static void kunlun_plane_atomic_disable(struct drm_plane *plane,
					 struct drm_plane_state *old_state)
{
	struct kunlun_plane *klp = to_kunlun_plane(plane);
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(old_state->crtc);
	const struct kunlun_mlc_data *dc_mlc = kcrtc->data->dc_data->mlc;
	const struct kunlun_mlc_data *dp_mlc = kcrtc->data->dp_data->mlc;
	const struct kunlun_sp_data *sp_data;
	const struct kunlun_gp_data *gp_data;
	uint32_t chn_base = klp->data->chn_base;
	int layer_id = klp->data->id;
	void __iomem *regs;
	int i;

	if (!old_state->crtc)
		return;

	if (kcrtc->ctrl_unit)
		regs = kcrtc->dp_regs;
	else
		regs = kcrtc->dc_regs;

	if (!regs) {
		pr_err("This plane's regs is NULL\n");
		return;
	}

	if ((layer_id == DC_SPIPE) ||
		(layer_id == DP_SPIPE0) ||
		(layer_id == DP_SPIPE1)) {
		sp_data = klp->data->hwpipe->data.sp;
		DU_PIPE_ELEM_SET(regs, chn_base, sp_data, sdw_trig, 1);
	} else {
		gp_data = klp->data->hwpipe->data.gp;
		DU_PIPE_ELEM_SET(regs, chn_base, gp_data, sdw_trig, 1);
	}

	i = get_du_mlc_sf_index(layer_id);
	if (i < 0)
		return;
	/*clear the 'enable layer' bit*/
	if (kcrtc->ctrl_unit)
		DU_MLC_LAYER_SET(regs, dp_mlc, i, sf_en, 0);
	else
		DU_MLC_LAYER_SET(regs, dc_mlc, i, sf_en, 0);

	if (klp->plane_status != PLANE_DISABLE)
		klp->plane_status = PLANE_DISABLE;
}

static void kunlun_plane_destroy(struct drm_plane *plane)
{
	drm_plane_cleanup(plane);
}

static void kunlun_plane_install_rotation_property(struct drm_plane *plane)
{
	struct kunlun_plane *klp = to_kunlun_plane(plane);
	uint32_t layer_type = klp->data->id;

	if ((layer_type == DP_GPIPE0) || (layer_type == DP_GPIPE1)) {
		drm_plane_create_rotation_property(plane,
						DRM_MODE_ROTATE_0,
						DRM_MODE_ROTATE_0 |
						DRM_MODE_ROTATE_90 |
						DRM_MODE_ROTATE_180 |
						DRM_MODE_ROTATE_270 |
						DRM_MODE_REFLECT_X |
						DRM_MODE_REFLECT_Y);
	}
}

static void kunlun_plane_install_zpos_property(struct drm_plane *plane)
{
	struct kunlun_plane *klp = to_kunlun_plane(plane);
	uint32_t num_planes = klp->kcrtc->num_planes;

	drm_plane_create_zpos_property(plane, 0, 0, num_planes - 1);
}

static void kunlun_plane_install_properties(struct drm_plane *plane,
				struct drm_mode_object *obj)
{
	struct drm_device *dev = plane->dev;
	struct kunlun_plane *klp = to_kunlun_plane(plane);
	struct drm_property *prop;

	static const struct drm_prop_enum_list blend_mode_prop_enum_list[] = {
		{ DRM_MODE_BLEND_PIXEL_NONE, "None" },
		{ DRM_MODE_BLEND_PREMULTI, "Pre-multiplied" },
		{ DRM_MODE_BLEND_COVERAGE, "Coverage" },
	};

#define INSTALL_PROPERTY(name, NAME, init_val, fnc, ...) do { \
		prop = klp->plane_property[PLANE_PROP_##NAME]; \
		if (!prop) { \
			prop = drm_property_##fnc(dev, 0, #name, \
					##__VA_ARGS__); \
			if (!prop) { \
				pr_err("Create property %s failed\n", #name); \
				return; \
			} \
			klp->plane_property[PLANE_PROP_##NAME] = prop; \
		} \
		drm_object_attach_property(&plane->base, prop, init_val); \
	} while (0)

#define INSTALL_RANGE_PROPERTY(name, NAME, min, max, init_val) \
		INSTALL_PROPERTY(name, NAME, init_val, \
				create_range, min, max)

#define INSTALL_ENUM_PROPERTY(name, NAME, init_val) \
		INSTALL_PROPERTY(name, NAME, init_val, \
				create_enum, name##_prop_enum_list, \
				ARRAY_SIZE(name##_prop_enum_list))

	INSTALL_RANGE_PROPERTY(alpha, ALPHA, 0, 255, 255);
	INSTALL_ENUM_PROPERTY(blend_mode, BLEND_MODE, DRM_MODE_BLEND_PIXEL_NONE);

	kunlun_plane_install_rotation_property(plane);
	kunlun_plane_install_zpos_property(plane);

#undef INSTALL_RANGE_PROPERTY
#undef INSTALL_ENUM_PROPERTY
#undef INSTALL_PROPERTY
}

static int kunlun_plane_atomic_set_property(struct drm_plane *plane,
				struct drm_plane_state *state, struct drm_property *property,
				uint64_t val)
{
	struct kunlun_plane *klp = to_kunlun_plane(plane);
	int ret = 0;

#define SET_PROPERTY(name, NAME, type) do { \
		if (klp->plane_property[PLANE_PROP_##NAME] == property) { \
			klp->name = (type)val; \
			goto done; \
		} \
	} while (0)

	SET_PROPERTY(alpha, ALPHA, uint8_t);
	SET_PROPERTY(blend_mode, BLEND_MODE, uint8_t);

	pr_err("Invalid property\n");
	ret = -EINVAL;
done:
	return ret;
#undef SET_PROPERTY
}

static int kunlun_plane_atomic_get_property(struct drm_plane *plane,
				const struct drm_plane_state *state,
				struct drm_property *property, uint64_t *val)
{
	struct kunlun_plane *klp = to_kunlun_plane(plane);
	int ret = 0;

#define GET_PROPERTY(name, NAME, type) do { \
		if (klp->plane_property[PLANE_PROP_##NAME] == property) { \
			*val = klp->name; \
			goto done; \
		} \
	} while (0)

	GET_PROPERTY(alpha, ALPHA, uint8_t);
	GET_PROPERTY(blend_mode, BLEND_MODE, uint8_t);

	pr_err("Invalid property\n");
	ret = -EINVAL;
done:
	return ret;
#undef GET_PROPERTY
}

static void kunlun_plane_reset(struct drm_plane *plane)
{
	struct kunlun_plane *klp = to_kunlun_plane(plane);

	klp->alpha = 0xFF;
	drm_atomic_helper_plane_reset(plane);
}

static const struct drm_plane_helper_funcs kunlun_plane_helper_funcs = {
	.atomic_check = kunlun_plane_atomic_check,
	.atomic_update = kunlun_plane_atomic_update,
	.atomic_disable = kunlun_plane_atomic_disable,
};

static const struct drm_plane_funcs kunlun_plane_funcs = {
	.update_plane	= drm_atomic_helper_update_plane,
	.disable_plane	= drm_atomic_helper_disable_plane,
	.destroy = kunlun_plane_destroy,
	.atomic_set_property = kunlun_plane_atomic_set_property,
	.atomic_get_property = kunlun_plane_atomic_get_property,
	.reset = kunlun_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};

void kunlun_planes_fini(struct kunlun_crtc *kcrtc)
{
	struct drm_plane *plane, *tmp;

	list_for_each_entry_safe(plane, tmp,
			&kcrtc->drm->mode_config.plane_list, head)
		drm_plane_cleanup(plane);
}

int kunlun_planes_init_primary(struct kunlun_crtc *kcrtc,
		struct drm_plane **primary, struct drm_plane **cursor)
{
	const struct kunlun_crtc_du_data *dc_data = kcrtc->data->dc_data;
	const struct kunlun_crtc_du_data *dp_data = kcrtc->data->dp_data;
	const struct kunlun_plane_data *plane_data;
	struct kunlun_plane *plane;
	int ret;
	int i;

	for(i = 0; i < kcrtc->num_planes; i++) {
		plane = &kcrtc->planes[i];
		if (kcrtc->ctrl_unit)
			plane_data = &dp_data->planes[i];
		else
			plane_data = &dc_data->planes[i];

		if (plane_data->type != DRM_PLANE_TYPE_PRIMARY &&
				plane_data->type != DRM_PLANE_TYPE_CURSOR)
			continue;

		ret = drm_universal_plane_init(kcrtc->drm, &plane->base,
				0, &kunlun_plane_funcs, plane_data->hwpipe->formats,
				plane_data->hwpipe->nformats, NULL,
				plane_data->type, NULL);
		if(ret) {
			DRM_DEV_ERROR(kcrtc->dev, "failed to init plane %d\n", ret);
			goto err_planes_fini;
		}

		drm_plane_helper_add(&plane->base, &kunlun_plane_helper_funcs);

		kunlun_plane_install_properties(&plane->base, &plane->base.base);

		if(plane_data->type == DRM_PLANE_TYPE_PRIMARY)
			*primary = &plane->base;
		else
			*cursor = &plane->base;
	}

	return 0;

err_planes_fini:
	kunlun_planes_fini(kcrtc);
	return ret;
}

int kunlun_planes_init_overlay(struct kunlun_crtc *kcrtc)
{
	const struct kunlun_crtc_du_data *dc_data = kcrtc->data->dc_data;
	const struct kunlun_crtc_du_data *dp_data = kcrtc->data->dp_data;
	const struct kunlun_plane_data *plane_data;
	struct kunlun_plane *plane;
	int index;
	int ret;
	int i, j;

	for(i = 0; i < kcrtc->num_planes; i++) {
		plane = &kcrtc->planes[i];
		if (kcrtc->ctrl_unit)
			plane_data = &dp_data->planes[i];
		else
			plane_data = &dc_data->planes[i];

		if(plane_data->type != DRM_PLANE_TYPE_OVERLAY)
			continue;

		ret = drm_universal_plane_init(kcrtc->drm, &plane->base,
				(1 << drm_crtc_index(&kcrtc->base)),
				&kunlun_plane_funcs, plane_data->hwpipe->formats,
				plane_data->hwpipe->nformats, NULL,
				plane_data->type, NULL);
		if(ret) {
			DRM_DEV_ERROR(kcrtc->dev, "failed to init plane %d\n", ret);
			goto err_planes_fini;
		}

		drm_plane_helper_add(&plane->base, &kunlun_plane_helper_funcs);

		kunlun_plane_install_properties(&plane->base, &plane->base.base);
	}

	return 0;

err_planes_fini:
	kunlun_planes_fini(kcrtc);
	return ret;
}

