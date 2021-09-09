/*
 * dpc.h
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#ifndef _sdrv_plane_H_
#define _sdrv_plane_H_

#include <linux/delay.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#include <video/videomode.h>

#include "sdrv_dpc.h"

static const u32 primary_fmts[] = {
	DRM_FORMAT_XRGB8888, DRM_FORMAT_XBGR8888,
	DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888, DRM_FORMAT_BGRA8888,
	DRM_FORMAT_RGBX8888, DRM_FORMAT_BGRX8888,
	DRM_FORMAT_RGB565, DRM_FORMAT_BGR565,
	DRM_FORMAT_NV12, DRM_FORMAT_NV21,
	DRM_FORMAT_NV16, DRM_FORMAT_NV61,
	DRM_FORMAT_YUV420, DRM_FORMAT_YVU420,
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
	SWAP_A_RGB			= 0b0000,
	SWAP_A_RBG			= 0b0001,
	SWAP_A_GBR			= 0b0010,
	SWAP_A_GRB			= 0b0011,
	SWAP_A_BGR			= 0b0100,
	SWAP_A_BRG			= 0b0101,
	SWAP_B_ARG			= 0b1000,
	SWAP_B_AGR			= 0b1001,
	SWAP_B_RGA			= 0b1010,
	SWAP_B_RAG			= 0b1011,
	SWAP_B_GRA			= 0b1100,
	SWAP_B_GAR			= 0b1101
};

/*Kunlun DC&DP layer format uv mode*/
enum {
	UV_YUV444_RGB       = 0b00,
	UV_YUV422           = 0b01,
	UV_YUV440           = 0b10,
	UV_YUV420           = 0b11
};

/*Kunlun DC&DP layer format memory mode*/
enum {
	LINEAR_MODE             = 0b000,
	RLE_COMPR_MODE          = 0b001,
	GPU_RAW_TILE_MODE       = 0b010,
	GPU_CPS_TILE_MODE       = 0b011,
	VPU_RAW_TILE_MODE       = 0b100,
	VPU_CPS_TILE_MODE       = 0b101,
	VPU_RAW_TILE_988_MODE   = 0b110,
};

/*Kunlun DC&DP layer format planar mode*/
enum {
	FMT_INTERLEAVED     = 0b00,
	FMT_MONOTONIC       = 0b01,
	FMT_SEMI_PLANAR     = 0b10,
	FMT_PLANAR          = 0b11
};

/*Kunlun DC&DP layer format rotation mode*/
enum {
	ROT_DEFAULT         = 0b000,
	ROT_ROT             = 0b001,
	ROT_VFLIP           = 0b010,
	ROT_HFLIP           = 0b100
};

/*Kunlun DP layer format TILE vsize*/
enum {
	TILE_VSIZE_1       = 0b000,
	TILE_VSIZE_2       = 0b001,
	TILE_VSIZE_4       = 0b010,
	TILE_VSIZE_8       = 0b011,
	TILE_VSIZE_16      = 0b100,
};

/*Kunlun DP layer format TILE hsize*/
enum {
	TILE_HSIZE_1       = 0b000,
	TILE_HSIZE_8       = 0b001,
	TILE_HSIZE_16      = 0b010,
	TILE_HSIZE_32      = 0b011,
	TILE_HSIZE_64      = 0b100,
	TILE_HSIZE_128     = 0b101,
};

/**/
enum {
	FBDC_U8U8U8U8      = 0xc,
	FBDC_U16           = 0x9,
	FBDC_R5G6B5        = 0x5,
	FBDC_U8            = 0x0,
	FBDC_NV21          = 0x37,
	FBDC_YUV420_16PACK = 0x65
};
enum kunlun_plane_property {
	PLANE_PROP_ALPHA,
	PLANE_PROP_BLEND_MODE,
	PLANE_PROP_FBDC_HSIZE_Y,
	PLANE_PROP_FBDC_HSIZE_UV,
	PLANE_PROP_CAP_MASK,
	PLANE_PROP_PD_TYPE,
	PLANE_PROP_PD_MODE,
	PLANE_PROP_MAX_NUM
};
enum {
	DRM_MODE_BLEND_PIXEL_NONE = 0,
	DRM_MODE_BLEND_PREMULTI,
	DRM_MODE_BLEND_COVERAGE
};
enum {
	PLANE_DISABLE,
	PLANE_ENABLE
};

enum {
	PROP_PLANE_CAP_RGB = 0,
	PROP_PLANE_CAP_YUV,
	PROP_PLANE_CAP_XFBC,
	PROP_PLANE_CAP_YUV_FBC,
	PROP_PLANE_CAP_ROTATION,
	PROP_PLANE_CAP_SCALING,
};

struct pipe_capability {
	const u32 *formats;
	int nformats;
	int layer_type;
	int rotation;
	int scaling;
	int yuv;
	int yuv_fbc;
	int xfbc;
};

struct pipe_operation {
	int (*init)(struct sdrv_pipe *pipe);
	int (*update)(struct sdrv_pipe *pipe, struct dpc_layer *layer);
	int (*check)(struct sdrv_pipe *pipe, struct dpc_layer *layer);
	int (*disable)(struct sdrv_pipe *pipe);
	int (*sw_reset)(struct sdrv_pipe *pipe);
	int (*trigger)(struct sdrv_pipe *pipe);
};

struct sdrv_pipe {
	int id; // the ordered id from 0
	struct sdrv_dpc *dpc;
	enum drm_plane_type type;
	void __iomem *regs;
	const char *name;
	struct pipe_operation *ops;
	struct pipe_capability *cap;
	struct dpc_layer layer_data;
	struct sdrv_pipe *next;
};

struct kunlun_plane {
	struct pipe_capability *cap;
	struct kunlun_crtc *kcrtc;
	uint8_t plane_status;
	struct drm_property *plane_property[PLANE_PROP_MAX_NUM];
	uint8_t alpha;
	uint8_t blend_mode;
	uint32_t fbdc_hsize_y;
	uint32_t fbdc_hsize_uv;
	uint32_t cap_mask;
	int pd_type;
	int pd_mode;
	struct sdrv_pipe *pipes[2];
	int num_pipe;
	enum drm_plane_type type;
	struct drm_plane base;
};

struct ops_entry {
	const char *ver;
	void *ops;
};

struct ops_list {
	struct list_head head;
	struct ops_entry *entry;
};

extern struct list_head pipe_list_head;
int ops_register(struct ops_entry *entry, struct list_head *head);
void *ops_attach(const char *str, struct list_head *head);

#define pipe_ops_register(entry)	ops_register(entry, &pipe_list_head)
#define pipe_ops_attach(str)	ops_attach(str, &pipe_list_head)

extern struct ops_entry apipe_entry;
extern struct ops_entry spipe_entry;
extern struct ops_entry gpipe_dc_entry;
extern struct ops_entry gpipe1_dp_entry;
extern struct ops_entry gpipe0_dp_entry;

int kunlun_planes_init_primary(struct kunlun_crtc *kcrtc,
		struct drm_plane **primary, struct drm_plane **cursor);
int kunlun_planes_init_overlay(struct kunlun_crtc *kcrtc);
void kunlun_planes_fini(struct kunlun_crtc *kcrtc);
#endif //_sdrv_plane_H_

