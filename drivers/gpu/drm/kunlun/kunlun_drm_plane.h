/*
 * kunlun_drm_plane.h
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef _KUNLUN_DRM_PLANE_H_
#define _KUNLUN_DRM_PLANE_H_
#include <drm/drm_plane.h>
#include "kunlun_drm_reg.h"

#define DU_PIPE_ELEM_SET(crtc, chn_base, elem, name, val) kunlun_reg_set(crtc->regs, \
		chn_base + elem->name.offset, elem->name.mask, elem->name.shift, val)

static const uint32_t csc_param[4][15] = {
	{/*YCbCr to RGB*/
		0x094F, 0x0000, 0x0CC4, 0x094F,
		0x3CE0, 0x397F, 0x094F, 0x1024,
		0x0000, 0x0000, 0x0000, 0x0000,
		0x0040, 0x0200, 0x0200
	},
	{/*YUV to RGB*/
		0x0800, 0x0000, 0x091E, 0x0800,
		0x7CD8, 0x7B5B, 0x0800, 0x1041,
		0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000
	},
	{/*RGB to YCbCr*/
		0x0000
	},
	{/*RGB to YUV*/
		0x0000
	}
};

/*Kunlun DC&DP layer IDs*/
enum {
	DC_SPIPE = BIT(0),
	DC_GPIPE = BIT(1),
	DP_GPIPE0 = BIT(2),
	DP_GPIPE1 = BIT(3),
	DP_SPIPE0 = BIT(4),
	DP_SPIPE1 = BIT(5),
};

/*Kunlun DC&DP layer comp swip*/
enum {
	SWAP_A_RGB          = 0b0000,
	SWAP_A_RBG          = 0b0001,
	SWAP_A_GBR          = 0b0010,
	SWAP_A_GRB          = 0b0011,
	SWAP_A_BGR          = 0b0100,
	SWAP_A_BRG          = 0b0101,
	SWAP_B_ARG          = 0b1000,
	SWAP_B_AGR          = 0b1001,
	SWAP_B_RGA          = 0b1010,
	SWAP_B_RAG          = 0b1011,
	SWAP_B_GRA          = 0b1100,
	SWAP_B_GAR          = 0b1101
};

/*Kunlun DC&DP layer formart uv mode*/
enum {
	UV_YUV444_RGB       = 0b00,
	UV_YUV422           = 0b01,
	UV_YUV440           = 0b10,
	UV_YUV420           = 0b11
};

/*Kunlun DC&DP layer formart memory mode*/
enum {
	LINEAR_MODE             = 0b000,
	RLE_COMPR_MODE          = 0b001,
	GPU_RAW_TILE_MODE       = 0b010,
	GPU_CPS_TILE_MODE       = 0b011,
	VPU_RAW_TILE_MODE       = 0b100,
	VPU_CPS_TILE_MODE       = 0b101,
	VPU_RAW_TILE_988_MODE   = 0b110,
};

/*Kunlun DC&DP layer formart planar mode*/
enum {
	FMT_INTERLEAVED     = 0b00,
	FMT_MONOTONIC       = 0b01,
	FMT_SEMI_PLANAR     = 0b10,
	FMT_PLANAR          = 0b11
};

/*Kunlun DC&DP layer formart rotation mode*/
enum {
	ROT_DEFAULT         = 0b000,
	ROT_ROT             = 0b001,
	ROT_VFLIP           = 0b010,
	ROT_HFLIP           = 0b100
};

struct kunlun_pipe_pix_comp {
	struct kunlun_du_reg bpv;
	struct kunlun_du_reg bpu;
	struct kunlun_du_reg bpy;
	struct kunlun_du_reg bpa;
};

struct kunlun_pipe_frm_ctrl {
	struct kunlun_du_reg endia_ctrl;
	struct kunlun_du_reg comp_swap;
	struct kunlun_du_reg rot;
	struct kunlun_du_reg rgb_yuv;
	struct kunlun_du_reg uv_swap;
	struct kunlun_du_reg uv_mode;
	struct kunlun_du_reg mode;
	struct kunlun_du_reg fmt;
};

struct kunlun_pipe_frm_data {
	struct kunlun_du_reg height;
	struct kunlun_du_reg width;
	struct kunlun_du_reg y_baddr_l;
	struct kunlun_du_reg y_baddr_h;
	struct kunlun_du_reg u_baddr_l;
	struct kunlun_du_reg u_baddr_h;
	struct kunlun_du_reg v_baddr_l;
	struct kunlun_du_reg v_baddr_h;
	struct kunlun_du_reg y_stride;
	struct kunlun_du_reg u_stride;
	struct kunlun_du_reg v_stride;
	struct kunlun_du_reg offset_x;
	struct kunlun_du_reg offset_y;
};

struct kunlun_gp_tile_ctrl {
	struct kunlun_du_reg tr_req_cnt;
	struct kunlun_du_reg re_buf_wml;
	struct kunlun_du_reg re_fifo_byps;
	struct kunlun_du_reg re_mode;
	struct kunlun_du_reg vsize;
	struct kunlun_du_reg hsize;
};

struct kunlun_gp_yuvup_ctrl {
	struct kunlun_du_reg en;
	struct kunlun_du_reg vofset;
	struct kunlun_du_reg hofset;
	struct kunlun_du_reg filter_mode;
	struct kunlun_du_reg upv_bypass;
	struct kunlun_du_reg uph_bypass;
	struct kunlun_du_reg bypass;
};

struct kunlun_gp_crop_ctrl {
	struct kunlun_du_reg bypass;
	struct kunlun_du_reg start_x;
	struct kunlun_du_reg start_y;
	struct kunlun_du_reg vsize;
	struct kunlun_du_reg hsize;
	struct kunlun_du_reg err_st;
};

struct kunlun_csc_ctrl {
	struct kunlun_du_reg alpha;
	struct kunlun_du_reg sbup_conv;
	struct kunlun_du_reg bypass;
	struct kunlun_du_reg a00;
	struct kunlun_du_reg a01;
	struct kunlun_du_reg a02;
	struct kunlun_du_reg a10;
	struct kunlun_du_reg a11;
	struct kunlun_du_reg a12;
	struct kunlun_du_reg a20;
	struct kunlun_du_reg a21;
	struct kunlun_du_reg a22;
	struct kunlun_du_reg b0;
	struct kunlun_du_reg b1;
	struct kunlun_du_reg b2;
	struct kunlun_du_reg c0;
	struct kunlun_du_reg c1;
	struct kunlun_du_reg c2;
};

struct kunlun_hscaler_data {
	struct kunlun_du_reg nor_para;
	struct kunlun_du_reg apb_rd;
	struct kunlun_du_reg filter_en_v;
	struct kunlun_du_reg filter_en_u;
	struct kunlun_du_reg filter_en_y;
	struct kunlun_du_reg filter_en_a;
	struct kunlun_du_reg pola;
	struct kunlun_du_reg fra;
	struct kunlun_du_reg ratio_int;
	struct kunlun_du_reg ratio_fra;
	struct kunlun_du_reg out_hsize;
};

struct kunlun_vscaler_data {
	struct kunlun_du_reg init_phase_o;
	struct kunlun_du_reg init_pos_o;
	struct kunlun_du_reg init_phase_e;
	struct kunlun_du_reg init_pos_e;
	struct kunlun_du_reg norm;
	struct kunlun_du_reg parity;
	struct kunlun_du_reg pxl_mode;
	struct kunlun_du_reg vs_mode;
	struct kunlun_du_reg out_vsize;
	struct kunlun_du_reg ratio_inc;
};

struct kunlun_gp_scaler_data {
	struct kunlun_hscaler_data *hs_data;
	struct kunlun_vscaler_data *vs_data;
};

struct kunlun_gp_re_status {
	struct kunlun_du_reg v_frame_end;
	struct kunlun_du_reg u_frame_end;
	struct kunlun_du_reg y_frame_end;
};

struct kunlun_gp_fbdc_ctrl {
	struct kunlun_du_reg m_id;
	struct kunlun_du_reg context;
	struct kunlun_du_reg hdr_encoding;
	struct kunlun_du_reg twiddle_mode;
	struct kunlun_du_reg meml;
	struct kunlun_du_reg fmt;
	struct kunlun_du_reg en;
	struct kunlun_du_reg clear0_color;
	struct kunlun_du_reg clear1_color;
};

struct kunlun_rle_int_data {
	struct kunlun_du_reg v_err;
	struct kunlun_du_reg u_err;
	struct kunlun_du_reg y_err;
	struct kunlun_du_reg a_err;
};
struct kunlun_sp_rle_data {
	struct kunlun_du_reg length;
	struct kunlun_du_reg checksum;
	struct kunlun_du_reg data_size;
	struct kunlun_du_reg rle_en;
	struct kunlun_du_reg y_checksum;
	struct kunlun_du_reg u_checksum;
	struct kunlun_du_reg v_checksum;
	struct kunlun_du_reg a_checksum;
	const struct kunlun_rle_int_data *int_mask;
	const struct kunlun_rle_int_data *int_status;
};

struct kunlun_clut_channel_a_data {
	struct kunlun_du_reg has_alpha;
	struct kunlun_du_reg a_y_sel;
	struct kunlun_du_reg bypass;
	struct kunlun_du_reg offset;
	struct kunlun_du_reg depth;
};

struct kunlun_clut_channel_y_data {
	struct kunlun_du_reg bypass;
	struct kunlun_du_reg offset;
	struct kunlun_du_reg depth;
};

struct kunlun_clut_channel_u_data {
	struct kunlun_du_reg u_y_sel;
	struct kunlun_du_reg bypass;
	struct kunlun_du_reg offset;
	struct kunlun_du_reg depth;
};

struct kunlun_clut_channel_v_data {
	struct kunlun_du_reg v_y_sel;
	struct kunlun_du_reg bypass;
	struct kunlun_du_reg offset;
	struct kunlun_du_reg depth;
};

struct kunlun_clut_read_ctrl {
	struct kunlun_du_reg v_sel;
	struct kunlun_du_reg u_sel;
	struct kunlun_du_reg y_sel;
	struct kunlun_du_reg a_sel;
};
struct kunlun_sp_clut_data {
	const struct kunlun_clut_channel_a_data *a_data;
	const struct kunlun_clut_channel_y_data *y_data;
	const struct kunlun_clut_channel_u_data *u_data;
	const struct kunlun_clut_channel_v_data *v_data;
	const struct kunlun_clut_read_ctrl *read_ctrl;
	struct kunlun_du_reg baddr_l;
	struct kunlun_du_reg baddr_h;
	struct kunlun_du_reg load_en;
};

struct kunlun_gp_data {
	const struct kunlun_pipe_pix_comp *pix_comp;
	const struct kunlun_pipe_frm_ctrl *frm_ctrl;
	const struct kunlun_pipe_frm_data *frm_info;
	const struct kunlun_gp_tile_ctrl *tile_ctrl;
	const struct kunlun_gp_yuvup_ctrl *yuvup_ctrl;
	const struct kunlun_gp_crop_ctrl *crop_ctrl;
	const struct kunlun_csc_ctrl *csc_ctrl;
	const struct kunlun_gp_scaler_data *scaler;
	const struct kunlun_gp_re_status *re_st;
	const struct kunlun_gp_fbdc_ctrl *fbdc_ctrl;
	struct kunlun_du_reg sw_rst;
	struct kunlun_du_reg sdw_trig;
};

struct kunlun_sp_data {
	const struct kunlun_pipe_pix_comp *pix_comp;
	const struct kunlun_pipe_frm_ctrl *frm_ctrl;
	const struct kunlun_pipe_frm_data *frm_info;
	const struct kunlun_sp_rle_data *rle_data;
	const struct kunlun_sp_clut_data *clut_data;
	struct kunlun_du_reg sw_rst;
	struct kunlun_du_reg sdw_trig;
};

struct kunlun_pipe_data {
	const uint32_t *formats;
	uint32_t nformats;
	union {
		const struct kunlun_sp_data *sp;
		const struct kunlun_gp_data *gp;
	} data;
};

struct kunlun_plane_data {
	uint32_t chn_base;
	uint32_t id; /*layer ID*/
	enum drm_plane_type type;
	const struct kunlun_pipe_data *hwpipe;
};
#endif
