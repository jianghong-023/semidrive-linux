/*
 * dp_r0p1.c
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_plane_helper.h>

#include "sdrv_plane.h"
#include "sdrv_dpc.h"

#define RDMA_JUMP           0x1000
#define GP0_JUMP            0x2000
#define GP1_JUMP            0x3000
#define SP0_JUMP            0x5000
#define SP1_JUMP            0x6000
#define MLC_JUMP            0x7000
#define AP_JUMP             0x9000
#define FBDC_JUMP           0xC000

#define TCON_SOF_MASK					BIT(5)
#define TCON_EOF_MASK					BIT(6)

#define DP_POTER_DUFF_ID	(5)

struct dp_reg {
	u32 dp_ctrl;
	u32 dp_flc_ctrl;
	u32 dp_flc_update;
	u32 dp_size;
	u32 sdma_ctrl;
	u32 reserved_0x14_0x1C[3];
	u32 dp_int_mask;
	u32 dp_int_status;
};

struct rdma_chn_reg {
	u32 rdma_dfifo_wml;
	u32 rdma_dfifo_depth;
	u32 rdma_cfifo_depth;
	union _rdma_ch_prio {
		u32 val;
		struct _rdma_ch_prio_ {
			u32 p0: 6;
			u32 reserved0: 2;
			u32 p1: 6;
			u32 reserved1: 2;
			u32 sche: 6;
			u32 reserved2: 10;
		} bits;
	} rdma_ch_prio;
	union _rdma_burst {
		u32 val;
		struct _rdma_burst_ {
			u32 len: 3;
			u32 mode: 1;
			u32 reserved: 28;
		} bits;
	} rdma_burst;
	u32 rdma_axi_user;
	u32 rdma_axi_ctrl;
	u32 rdmapres_wml;
};
struct dp_rdma_reg {
	struct rdma_chn_reg rdma_chn[9];
	u32 reserved_0x120_0x3fc[184];
	u32 rdma_ctrl;
	u32 reserved_0x404_0x4fc[63];
	u32 rdma_dfifo_full;
	u32 rdma_dfifo_empty;
	u32 rdma_cfifo_full;
	u32 rdma_cfifo_empty;
	u32 rdma_ch_idle;
	u32 reserved_0x514_0x51c[3];
	u32 rdma_int_mask;
	u32 rdma_int_status;
	u32 reserved_0x528_0x53c[6];
	u32 rdma_debug_ctrl;
	u32 rdma_debug_sta;
};

struct dp_gp_reg {
	u32 gp_pix_comp;
	u32 gp_frm_ctrl;
	u32 gp_frm_size;
	u32 gp_y_baddr_l;
	u32 gp_y_baddr_h;
	u32 gp_u_baddr_l;
	u32 gp_u_baddr_h;
	u32 gp_v_baddr_l;
	u32 gp_v_baddr_h;
	u32 reserved_0x24_0x28[2];
	u32 gp_y_stride;
	u32 gp_u_stride;
	u32 gp_v_stride;
	u32 gp_tile_ctrl;
	u32 reserved_0x3c;
	u32 gp_frm_offset;
	u32 gp_yuvup_ctrl;
	u32 reserved_0x48_0xfc[46];
	u32 gp_crop_ctrl;
	u32 gp_crop_ul_pos;
	u32 gp_crop_size;
	u32 reserved_0x10c;
	u32 gp_crop_par_err;
	u32 reserved_0x124_0x1fc[55];
	u32 gp_csc_ctrl;
	u32 gp_csc_coef1;
	u32 gp_csc_coef2;
	u32 gp_csc_coef3;
	u32 gp_csc_coef4;
	u32 gp_csc_coef5;
	u32 gp_csc_coef6;
	u32 gp_csc_coef7;
	u32 gp_csc_coef8;
	u32 reserved_0x224_0x2fc[55];
	u32 gp_hs_ctrl;
	u32 gp_hs_ini;
	u32 gp_hs_ratio;
	u32 gp_hs_width;
	u32 reserved_0x310_0x3fc[60];
	u32 gp_vs_ctrl;
	u32 gp_vs_resv;
	u32 gp_vs_inc;
	u32 reserved_0x40c;
	u32 gp_re_status;
	u32 reserved_0x414_0x4fc[59];
	u32 gp_fbdc_ctrl;
	u32 gp_fbdc_clear_0;
	u32 gp_fbdc_clear_1;
	u32 reserved_0x50c_0xdfc[573];
	u32 gp_sw_rst;
	u32 reserved_0xe04_0xefc[63];
	u32 gp_sdw_ctrl;
};

struct dp_sp_reg {
	u32 sp_pix_comp;
	u32 sp_frm_ctrl;
	u32 sp_frm_size;
	u32 sp_y_baddr_l;
	u32 sp_y_baddr_h;
	u32 reserved_0x14_0x28[6];
	u32 sp_y_stride;
	u32 reserved_0x30_0x3c[4];
	u32 sp_frm_offset;
	u32 reserved_0x44_0xfc[47];
	u32 rle_y_len;
	u32 reserved_0x104_0x10c[3];
	u32 rle_ctrl;
	u32 reserved_0x124_0x12c[3];
	u32 rle_y_check_sum_st;
	u32 rle_u_check_sum_st;
	u32 rle_v_check_sum_st;
	u32 rle_a_check_suem_st;
	u32 rle_int_mask;
	u32 rle_int_status;
	u32 reserved_0x148_0x1fc[46];
	u32 clut_a_ctrl;
	u32 clut_y_ctrl;
	u32 clut_u_ctrl;
	u32 clut_v_ctrl;
	u32 clut_read_ctrl;
	u32 clut_baddr_l;
	u32 clut_baddr_h;
	u32 clut_load_ctrl;
	u32 reserved_0x220_0xdfc[760];
	u32 sp_sw_rst;
	u32 reserved_0xe04_0xefc[63];
	u32 sp_sdw_ctrl;
};

struct mlc_sf_reg {
	union _mlc_sf_ctrl {
		u32 val;
		struct _mlc_sf_ctrl_ {
			u32 sf_en: 1;
			u32 crop_en: 1;
			u32 g_alpha_en: 1;
			u32 ckey_en: 1;
			u32 aflu_en: 1;
			u32 aflu_psel: 1;
			u32 slowdown_en: 1;
			u32 vpos_prot_en: 1;
			u32 prot_val: 6;
			u32 reserved: 18;
		} bits;
	} mlc_sf_ctrl;
	u32 mlc_sf_h_spos;
	u32 mlc_sf_v_spos;
	u32 mlc_sf_size;
	u32 mlc_sf_crop_h_pos;
	u32 mlc_sf_crop_v_pos;
	u32 mlc_sf_g_alpha;
	u32 mlc_sf_ckey_alpha;
	u32 mlc_sf_ckey_r_lv;
	u32 mlc_sf_ckey_g_lv;
	u32 mlc_sf_ckey_b_lv;
	u32 mlc_sf_aflu_time;
};

struct dp_mlc_reg {
	struct mlc_sf_reg mlc_sf[4];
	u32 reserved_0xc0_0x1fc[80];
	union _mlc_path_ctrl {
		u32 val;
		struct _mlc_path_ctrl_ {
			u32 layer_out_idx: 4;
			u32 pd_src_idx: 3;
			u32 reserved0: 1;
			u32 pd_des_idx: 3;
			u32 reserved1: 1;
			u32 pd_out_idx: 3;
			u32 reserved2: 1;
			u32 alpha_bld_idx: 4;
			u32 pd_mode: 5;
			u32 reserved3: 3;
			u32 pd_out_sel: 1;
			u32 pma_en: 1;
			u32 reserved4: 2;
		} bits;
	} mlc_path_ctrl[5];
	u32 mlc_bg_ctrl;
	u32 mlc_bg_color;
	u32 mlc_bg_aflu_time;
	u32 reserved_0x22c;
	u32 mlc_canvas_color;
	u32 mlc_clk_ratio;
	u32 reserved_0x238_0x23c[2];
	u32 mlc_int_mask;
	u32 mlc_int_status;
};

struct dp_ap_reg {
	u32 ap_pix_cmop;
	u32 ap_frm_ctrl;
	u32 ap_frm_size;
	u32 ap_baddr_l;
	u32 ap_baddr_h;
	u32 reserved_0x14_0x28[6];
	u32 ap_stride;
	u32 reserved_0x30_0x3c[4];
	u32 ap_frm_offset;
	u32 reserved_0x44_0xdfc[879];
	u32 ap_sw_rst;
	u32 reserved_0xe04_0xefc[63];
	u32 ap_sdw_ctrl;
};

struct fbdc_reg {
	union _fbdc_ctrl {
		u32 val;
		struct _fbdc_ctrl_ {
			u32 en: 1;
			u32 mode_v3_1_en: 1;
			u32 hdr_bypass: 1;
			u32 inva_sw_en: 1;
			u32 reserved: 27;
			u32 sw_rst: 1;
		} bits;
	} fbdc_ctrl;
	u32 fbdc_cr_inval;
	u32 fbdc_cr_val_0;
	u32 fbdc_cr_val_1;
	u32 fbdc_cr_ch0123_0;
	u32 fbdc_cr_ch0123_1;
	u32 fbdc_cr_filter_ctrl;
	u32 fbdc_cr_filter_status;
	u32 fbdc_cr_core_id_0;
	u32 fbdc_cr_core_id_1;
	u32 fbdc_cr_core_id_2;
	u32 fbdc_cr_core_ip;
};

#define REGISTER	volatile struct
typedef REGISTER dp_reg * dp_reg_t;

static uint32_t dp_irq_handler(struct sdrv_dpc *dpc)
{
	uint32_t sdrv_val = 0;

	sdrv_val |= SDRV_TCON_EOF_MASK;
	return sdrv_val;
}

static void dp_sw_reset(struct sdrv_dpc *dpc)
{

}


static void dp_vblank_enable(struct sdrv_dpc *dpc, bool enable)
{
	struct dp_dummy_data *dummy = (struct dp_dummy_data *)dpc->priv;
	if (dummy->vsync_enabled == enable)
		return;
	PRINT("vblank enable---");
	dummy->vsync_enabled = enable;
}

static void dp_enable(struct sdrv_dpc *dpc, bool enable)
{

}

static int dp_update(struct sdrv_dpc *dpc, struct dpc_layer layers[], u8 count)
{
#if 1

	int i, zpos, hwid;
	dp_reg_t reg = (dp_reg_t) dpc->regs;
	REGISTER dp_mlc_reg *mlc_reg = (REGISTER dp_mlc_reg *)(dpc->regs + MLC_JUMP);
	REGISTER fbdc_reg *freg = (REGISTER fbdc_reg *)(dpc->regs + FBDC_JUMP);
	struct dpc_layer *layer;
	bool is_fbdc_cps = false;
	volatile u32 tmp = 0;
	volatile uint32_t *flc_reg;
	int *z_orders = get_zorders_from_pipe_mask(dpc->pipe_mask);

	/*clear all mlc path*/
	for (i = 0; i < ARRAY_SIZE(mlc_reg->mlc_path_ctrl); i++) {
		if (dpc->pipe_mask & (1 << i))
			mlc_reg->mlc_path_ctrl[i].bits.layer_out_idx = 0xf;
		if (i == 0)
			mlc_reg->mlc_path_ctrl[i].val = 0xf777f;
	}

	for (i = 0; i < dpc->num_pipe; i++) {
		zpos = z_orders[i];
		mlc_reg->mlc_path_ctrl[zpos].bits.alpha_bld_idx = 0xf;
		mlc_reg->mlc_path_ctrl[zpos].bits.pma_en = 0;
	}


	for (i = 0; i < dpc->num_pipe; i++) {
		int delta = 0;
		layer = &dpc->pipes[i]->layer_data;

		if (!layer->enable)
			continue;
		zpos = layer->zpos;
		hwid = layer->index;
		DRM_DEBUG_DRIVER("dpc->name %s, layer->zpos %d, zpos: %d, hwid = %d\n", dpc->name, layer->zpos, zpos, hwid);
		if (layer->pd_type) {
			switch (layer->pd_type) {
				case PD_SRC:
					mlc_reg->mlc_path_ctrl[0].bits.pd_src_idx = hwid;
				break;
				case PD_DST:
					mlc_reg->mlc_path_ctrl[0].bits.pd_des_idx = hwid;
					mlc_reg->mlc_path_ctrl[0].bits.pd_mode = layer->pd_mode;
					mlc_reg->mlc_path_ctrl[0].bits.pd_out_idx = zpos;
					mlc_reg->mlc_path_ctrl[zpos].bits.alpha_bld_idx = DP_POTER_DUFF_ID;
				break;
				default:
				break;
			}
			/*set layer out idx*/
			mlc_reg->mlc_path_ctrl[hwid].bits.layer_out_idx = DP_POTER_DUFF_ID;

		} else {
			/*set layer out idx*/
			mlc_reg->mlc_path_ctrl[hwid].bits.layer_out_idx = zpos;
			/*set alpha bld idx*/
			mlc_reg->mlc_path_ctrl[zpos].bits.alpha_bld_idx = (hwid);
		}
		if (layer->blend_mode == DRM_MODE_BLEND_PREMULTI)
			tmp = 1;
		else
			tmp = 0;
		mlc_reg->mlc_path_ctrl[zpos].bits.pma_en = tmp;

		if (layer->ctx.is_fbdc_cps)
			is_fbdc_cps = true;
		// enable shadow ctrl
		flc_reg = (unsigned int *)(dpc->pipes[i]->regs + 0xf00);
		*flc_reg  = 1;
	}


	if (is_fbdc_cps) {
		tmp = freg->fbdc_cr_inval;
		tmp |= BIT(9);     //NOTIFY
		tmp &= ~(0xF << 4);//REQUESTER_O
		tmp |= 0x3 << 4;
		tmp |= 0x1;        //PENDING_O
		freg->fbdc_cr_inval = tmp;

		freg->fbdc_ctrl.val |= BIT(0); //en
	} else {
		freg->fbdc_ctrl.val &= ~ BIT(0); //en
	}

#endif
	return 0;
}

static void dp_flush(struct sdrv_dpc *dpc)
{
	int i;
	dp_reg_t reg = (dp_reg_t) dpc->regs;

#if 0
	// di trig
	reg->dp_flc_ctrl |= BIT(1);

	reg->dp_flc_ctrl |= BIT(0);
#endif
}

static void dp_init(struct sdrv_dpc *dpc)
{
	struct dp_dummy_data *dummy;
	// bottom to top
	if (!dpc->inited) {
		dpc->inited = true;
		_add_pipe(dpc, DRM_PLANE_TYPE_OVERLAY, 1, "gpipe0_dp", GP0_JUMP);
		_add_pipe(dpc, DRM_PLANE_TYPE_PRIMARY, 2, "gpipe1_dp", GP1_JUMP);
		//_add_pipe(dpc, DRM_PLANE_TYPE_OVERLAY, 3, "spipe", SP0_JUMP);
		//_add_pipe(dpc, DRM_PLANE_TYPE_PRIMARY, 4, "spipe", SP1_JUMP);
	}

	dummy = devm_kzalloc(&dpc->dev, sizeof(struct dp_dummy_data), GFP_KERNEL);
	if (!dummy) {
		DRM_ERROR("kalloc dummy failed\n");
		return;
	}
	dpc->priv = dummy;
	dummy->dpc = dpc;
	dummy->vsync_enabled = true;

	register_dummy_data(dummy);
	PRINT("add %d pipes for dp\n", dpc->num_pipe);
}

static int dp_modeset(struct sdrv_dpc *dpc, struct drm_display_mode *mode, u32 bus_flags)
{
	return 0;
}

static void dp_uninit(struct sdrv_dpc *dpc)
{
}

static void dp_shutdown(struct sdrv_dpc *dpc)
{
}

static int dp_update_done(struct sdrv_dpc *dpc)
{
	return 0;
}

static void dp_dump_registers(struct sdrv_dpc *dpc)
{

}

const struct dpc_operation dp_op_ops = {
	.init = dp_init,
	.uninit = dp_uninit,
	.irq_handler = dp_irq_handler,
	.vblank_enable = dp_vblank_enable,
	.enable = dp_enable,
	.update = dp_update,
	.flush = dp_flush,
	.sw_reset = dp_sw_reset,
	.modeset = dp_modeset,
	.shutdown = dp_shutdown,
	.update_done = dp_update_done,
	.dump = dp_dump_registers,
};


