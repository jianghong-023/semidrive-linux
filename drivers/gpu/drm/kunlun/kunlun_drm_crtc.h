/*
 * kunlun_drm_crtc.h
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef _KUNLUN_DRM_CRTC_H_
#define _KUNLUN_DRM_CRTC_H_
#include "kunlun_drm_plane.h"
#include "kunlun_drm_reg.h"

static inline void kunlun_reg_set(void __iomem *base,
		uint32_t offset, uint32_t mask, uint32_t shift, uint32_t val);

#define DU_REG_SET(base, du, name, val) kunlun_reg_set(base, \
		du->name.offset, du->name.mask, du->name.shift, val)

#define DC_CTRL_SET(base, du, name, val) kunlun_reg_set(base, \
		du->u.dc.name.offset, du->u.dc.name.mask, du->u.dc.name.shift, val)
#define DP_CTRL_SET(base, du, name, val) kunlun_reg_set(base, \
		du->u.dp.name.offset, du->u.dp.name.mask, du->u.dp.name.shift, val)

#define DU_RDMA_CHAN_SET(base, rdma, i, name, val) kunlun_reg_set(base, \
		rdma->chan->name.offset + (i) * RDMA_CHN_JMP, \
		rdma->chan->name.mask, rdma->chan->name.shift, val);

#define DU_TIMING_REG_SET(base, du, name, val) kunlun_reg_set(base, \
		du->name.offset, du->name.mask, du->name.shift, val - 1)

#define DU_TCON_KLAYER_REG_SET(base, tcon, i, name, val) \
	kunlun_reg_set(base, \
			tcon->klayer->name.offset + (i) * KICK_LAYER_JMP, \
			tcon->klayer->name.mask, tcon->klayer->name.shift, val)

#define DU_MLC_LAYER_SET(base, mlc, i, name, val) kunlun_reg_set(base, \
		mlc->layer->name.offset + (i) * MLC_LAYER_JMP, \
		mlc->layer->name.mask, mlc->layer->name.shift, val)

#define DU_MLC_TIMING_SET(base, mlc, i, name, val) kunlun_reg_set(base, \
		mlc->layer->name.offset + (i) * MLC_LAYER_JMP, \
		mlc->layer->name.mask, mlc->layer->name.shift, val - 1)

#define DU_MLC_PATH_SET(base, mlc, i, name, val) kunlun_reg_set(base, \
		mlc->path->name.offset + (i) * MLC_PATH_JMP, \
		mlc->path->name.mask, mlc->path->name.shift, val)

#define DU_IRQ_SET(base, reg, name, i, val) kunlun_reg_set(base, \
			reg->name.offset, 1 << (i), (i), val)

#define DU_REG_GET(base, du, name) kunlun_reg_get(base, \
			du->name.offset, du->name.mask, du->name.shift)

#define DU_IRQ_GET(base, reg, name) kunlun_reg_get(base, \
		reg->name.offset, 0xFFFFFFFF, 0)

#define DU_IRQ_CLR(base, reg, name, val) kunlun_reg_writel(base, \
		reg->name.offset, val)

enum {
	CRTC_DC,
	CRTC_DP0,
	CRTC_DP1
};

enum {
	PLANE_DISABLE,
	PLANE_ENABLE
};

struct kunlun_irq_data {
	struct kunlun_du_reg int_mask;
	struct kunlun_du_reg int_stat;
	struct kunlun_du_reg safe_int_mask;
	struct kunlun_du_reg rdma_int_mask;
	struct kunlun_du_reg rdma_int_stat;
	struct kunlun_du_reg mlc_int_mask;
	struct kunlun_du_reg mlc_int_stat;
	struct kunlun_du_reg crc32_int_mask;
	struct kunlun_du_reg crc32_int_stat;
};

struct kunlun_dc_ctrl {
	struct kunlun_du_reg reset;
	struct kunlun_du_reg urc;
	struct kunlun_du_reg mlc_dm;
	struct kunlun_du_reg ms;
	struct kunlun_du_reg sf;
	struct kunlun_du_reg gamma;
	struct kunlun_du_reg crc_trig;
};

struct kunlun_dp_ctrl {
	struct kunlun_du_reg reset;
	struct kunlun_du_reg re_type;
	struct kunlun_du_reg mlc_dm;
	struct kunlun_du_reg flc_kick;
	struct kunlun_du_reg enable;
	struct kunlun_du_reg h_size;
	struct kunlun_du_reg v_size;
};

struct kunlun_ctrl_data {
	union {
		struct kunlun_dc_ctrl dc;
		struct kunlun_dp_ctrl dp;
	} u;
	struct kunlun_du_reg di_trig;
	struct kunlun_du_reg flc_trig;
	struct kunlun_du_reg force_trig;
	struct kunlun_du_reg sdma_en;
};

struct kunlun_rdma_channel {
	struct kunlun_du_reg wml;
	struct kunlun_du_reg d_dep;
	struct kunlun_du_reg c_dep;
	struct kunlun_du_reg sche;
	struct kunlun_du_reg prio1;
	struct kunlun_du_reg prio0;
	struct kunlun_du_reg burst_mode;
	struct kunlun_du_reg burst_len;
	struct kunlun_du_reg axi_user;
	struct kunlun_du_reg axi_prot;
	struct kunlun_du_reg axi_cache;
	struct kunlun_du_reg th_down;
	struct kunlun_du_reg th_up;
};

struct kunlun_rdma_data {
	unsigned int num_chan;
	const struct kunlun_rdma_channel *chan;
	struct kunlun_du_reg cfg_load;
	struct kunlun_du_reg arb_sel;
	struct kunlun_du_reg dfifo_full;
	struct kunlun_du_reg dfifo_empty;
	struct kunlun_du_reg cfifo_full;
	struct kunlun_du_reg cfifo_empty;
	struct kunlun_du_reg ch_idle;
};

struct kunlun_mlc_layer {
	struct kunlun_du_reg prot_val;
	struct kunlun_du_reg vpos_prot_en;
	struct kunlun_du_reg slowdown_en;
	struct kunlun_du_reg aflu_psel;
	struct kunlun_du_reg aflu_en;
	struct kunlun_du_reg ckey_en;
	struct kunlun_du_reg g_alpha_en;
	struct kunlun_du_reg crop_en;
	struct kunlun_du_reg sf_en;
	struct kunlun_du_reg h_pos;
	struct kunlun_du_reg v_pos;
	struct kunlun_du_reg h_size;
	struct kunlun_du_reg v_size;
	struct kunlun_du_reg crop_h_start;
	struct kunlun_du_reg crop_h_end;
	struct kunlun_du_reg crop_v_start;
	struct kunlun_du_reg crop_v_end;
	struct kunlun_du_reg g_alpha;
	struct kunlun_du_reg ckey_alpha;
	struct kunlun_du_reg ckey_r_up;
	struct kunlun_du_reg ckey_r_dn;
	struct kunlun_du_reg ckey_g_up;
	struct kunlun_du_reg ckey_g_dn;
	struct kunlun_du_reg ckey_b_up;
	struct kunlun_du_reg ckey_b_dn;
	struct kunlun_du_reg aflu_time;
};

struct kunlun_mlc_bg {
	struct kunlun_du_reg bg_alpha;
	struct kunlun_du_reg aflu_en;
	struct kunlun_du_reg fstart_sel;
	struct kunlun_du_reg bg_a_sel;
	struct kunlun_du_reg bg_en;
	struct kunlun_du_reg alpha_bypass;
	struct kunlun_du_reg r_color;
	struct kunlun_du_reg g_color;
	struct kunlun_du_reg b_color;
	struct kunlun_du_reg aflu_time;
};

struct kunlun_mlc_path {
	struct kunlun_du_reg alpha_bld_idx;
	struct kunlun_du_reg layer_out_idx;
};

struct kunlun_mlc_data {
	unsigned int num_layer;
	const struct kunlun_mlc_layer *layer;
	unsigned int num_path;
	const struct kunlun_mlc_path *path;
	const struct kunlun_mlc_bg *bg;
	struct kunlun_du_reg canvas_r;
	struct kunlun_du_reg canvas_g;
	struct kunlun_du_reg canvas_b;
	struct kunlun_du_reg ratio;
};

struct kunlun_tcon_kick_layer {
	struct kunlun_du_reg y_color;
	struct kunlun_du_reg x_color;
	struct kunlun_du_reg enable;
};

struct kunlun_tcon_post_ch {
	struct kunlun_du_reg out_mode;
	struct kunlun_du_reg out_scr;
	struct kunlun_du_reg out_pol;
	struct kunlun_du_reg out_en;
	struct kunlun_du_reg pos_off;
	struct kunlun_du_reg pos_on;
};

struct kunlun_tcon_csc {
	struct kunlun_du_reg alpha;
	struct kunlun_du_reg sbup_conv;
	struct kunlun_du_reg bypass;
	struct kunlun_du_reg coef_a01;
	struct kunlun_du_reg coef_a00;
	struct kunlun_du_reg coef_a10;
	struct kunlun_du_reg coef_a02;
	struct kunlun_du_reg coef_a12;
	struct kunlun_du_reg coef_a11;
	struct kunlun_du_reg coef_a21;
	struct kunlun_du_reg coef_a20;
	struct kunlun_du_reg coef_b0;
	struct kunlun_du_reg coef_a22;
	struct kunlun_du_reg coef_b2;
	struct kunlun_du_reg coef_b1;
	struct kunlun_du_reg coef_c1;
	struct kunlun_du_reg coef_c0;
	struct kunlun_du_reg coef_c2;
};

struct kunlun_tcon_gamma {
	struct kunlun_du_reg apb;
	struct kunlun_du_reg bypass;
};

struct kunlun_tcon_dither {
	struct kunlun_du_reg v;
	struct kunlun_du_reg u;
	struct kunlun_du_reg y;
	struct kunlun_du_reg mode_12b;
	struct kunlun_du_reg spa_mode;
	struct kunlun_du_reg spa_1st;
	struct kunlun_du_reg spa_en;
	struct kunlun_du_reg tem_en;
	struct kunlun_du_reg bypass;
};

struct kunlun_tcon_crc32 {
	struct kunlun_du_reg vsync_pol;
	struct kunlun_du_reg hsync_pol;
	struct kunlun_du_reg de_pol;
	struct kunlun_du_reg block_en;
	struct kunlun_du_reg enable;
	struct kunlun_du_reg lock;
	struct kunlun_du_reg start_y;
	struct kunlun_du_reg start_x;
	struct kunlun_du_reg end_y;
	struct kunlun_du_reg end_x;
	struct kunlun_du_reg expect;
	struct kunlun_du_reg result;
};

struct kunlun_tcon_data {
	unsigned int num_klayer;
	const struct kunlun_tcon_kick_layer *klayer;
	unsigned int num_pchn;
	const struct kunlun_tcon_post_ch *pchn;
	unsigned int num_csc;
	const struct kunlun_tcon_csc *csc;
	const struct kunlun_tcon_gamma *gamma;
	const struct kunlun_tcon_dither *dither;
	unsigned int num_crc32;
	const struct kunlun_tcon_crc32 *crc32;
	struct kunlun_du_reg trig;
	struct kunlun_du_reg hact;
	struct kunlun_du_reg htot;
	struct kunlun_du_reg hbp;
	struct kunlun_du_reg hsync;
	struct kunlun_du_reg vact;
	struct kunlun_du_reg vtot;
	struct kunlun_du_reg vbp;
	struct kunlun_du_reg vsync;
	struct kunlun_du_reg pix_scr;
	struct kunlun_du_reg clk_en;
	struct kunlun_du_reg clk_pol;
	struct kunlun_du_reg de_pol;
	struct kunlun_du_reg vsync_pol;
	struct kunlun_du_reg hsync_pol;
	struct kunlun_du_reg enable;
};

struct kunlun_fbdc_ctrl {
	struct kunlun_du_reg sw_rst;
	struct kunlun_du_reg inva_sw_en;
	struct kunlun_du_reg hdr_bypass;
	struct kunlun_du_reg mode_v3_1_en;
	struct kunlun_du_reg en;
};

struct kunlun_fbdc_cr_inval {
	struct kunlun_du_reg requester_i;
	struct kunlun_du_reg context_i;
	struct kunlun_du_reg pengding_i;
	struct kunlun_du_reg notify;
	struct kunlun_du_reg override;
	struct kunlun_du_reg requester_o;
	struct kunlun_du_reg context_o;
	struct kunlun_du_reg pengding_o;
};

struct kunlun_fbdc_cr_val {
	struct kunlun_du_reg uv0;
	struct kunlun_du_reg y0;
	struct kunlun_du_reg uv1;
	struct kunlun_du_reg y1;
};

struct kunlun_fbdc_cr_ch0123 {
	struct kunlun_du_reg val0;
	struct kunlun_du_reg val1;
};

struct kunlun_fbdc_filter {
	struct kunlun_du_reg clear_3;
	struct kunlun_du_reg clear_2;
	struct kunlun_du_reg clear_1;
	struct kunlun_du_reg clear_0;
	struct kunlun_du_reg en;
	struct kunlun_du_reg status;
};

struct kunlun_fbdc_cr_core {
	struct kunlun_du_reg id_b;
	struct kunlun_du_reg id_p;
	struct kunlun_du_reg id_n;
	struct kunlun_du_reg id_v;
	struct kunlun_du_reg id_c;
	struct kunlun_du_reg ip_clist;
};

struct kunlun_fbdc_data {
	const struct kunlun_fbdc_ctrl *ctrl;
	const struct kunlun_fbdc_cr_inval *cr_inval;
	const struct kunlun_fbdc_cr_val *cr_val;
	const struct kunlun_fbdc_cr_ch0123 *cr_ch0123;
	const struct kunlun_fbdc_filter *filter;
	const struct kunlun_fbdc_cr_core *cr_core;
};

struct kunlun_crtc;
struct kunlun_du_ops {
	int (*vblank_enable)(struct kunlun_crtc *kcrtc, bool enable);
	int (*du_enable)(struct kunlun_crtc *kcrtc, bool enable);
};
struct kunlun_plane {
	struct drm_plane base;
	const struct kunlun_plane_data *data;
	struct kunlun_crtc *kcrtc;
	uint8_t du_owner;
	uint8_t plane_status;
};

struct kunlun_crtc_du_data {
	const struct kunlun_irq_data *irq;
	const struct kunlun_ctrl_data *ctrl;
	const struct kunlun_rdma_data *rdma;
	const struct kunlun_mlc_data *mlc;
	const struct kunlun_tcon_data *tcon;
	const struct kunlun_plane_data *planes;
	const struct kunlun_fbdc_data *fbdcs;
	unsigned int num_planes;
};

struct kunlun_crtc_data {
	const struct kunlun_crtc_du_data *dc_data;
	const struct kunlun_crtc_du_data *dp_data;
	const struct component_ops *cmpt_ops;
	const struct kunlun_du_ops *du_ops;
};

struct kunlun_crtc {
	struct device *dev;
	struct drm_crtc base;
	struct drm_device *drm;
	const struct kunlun_crtc_data *data;

	int dc_nums;
	int dp_nums;
	void __iomem *dc_regs;
	void __iomem *dp_regs[2];
	unsigned int irq;

	spinlock_t dc_irq_lock;
	spinlock_t dp_irq_lock[2];

	unsigned int num_planes;
	unsigned int num_dc_planes;
	unsigned int num_dp_planes;
	struct kunlun_plane *planes;

	bool enabled;
	bool vblank_enable;
	bool pending;
	struct drm_pending_vblank_event *event;

	struct work_struct underrun_work;
	struct work_struct mlc_int_work;
	struct work_struct rdma_int_work;
};

#define to_kunlun_crtc(x) container_of(x, struct kunlun_crtc, base)

struct kunlun_crtc_state {
	struct drm_crtc_state base;
	unsigned int plane_mask;
	unsigned int bus_format;
	unsigned int bus_flags;
};

#define to_kunlun_crtc_state(s) \
	container_of(s, struct kunlun_crtc_state, base)

static inline void
kunlun_reg_writel(void __iomem *base, uint32_t offset, uint32_t val)
{
	writel(val, base + offset);
	//pr_info("val = 0x%08x, read:0x%08x = 0x%08x\n", val, base + offset, readl(base+offset));
}

static inline uint32_t kunlun_reg_readl(void __iomem *base, uint32_t offset)
{
	return readl(base + offset);
}

static inline void kunlun_reg_set(void __iomem *base,
		uint32_t offset, uint32_t mask, uint32_t shift, uint32_t val)
{
	uint32_t reg;

	if(!mask)
		return;

	reg = kunlun_reg_readl(base, offset);
	reg &= ~mask;
	reg |= (val << shift) & mask;
	kunlun_reg_writel(base, offset, reg);
}

static inline uint32_t kunlun_reg_get(void __iomem *base,
		uint32_t offset, uint32_t mask, uint32_t shift)
{
	uint32_t reg;

	reg = kunlun_reg_readl(base, offset);
	reg &= mask;
	reg >>= shift;

	return reg;
}

extern void kunlun_planes_init(struct kunlun_crtc *kcrtc);
extern void kunlun_planes_fini(struct kunlun_crtc *kcrtc);
extern int kunlun_planes_init_primary(struct kunlun_crtc *kcrtc,
		struct drm_plane **primary, struct drm_plane **cursor);
extern int kunlun_planes_init_overlay(struct kunlun_crtc *kcrtc);
#endif
