/*
 * Copyright (c) Semidrive
 */
#ifndef _SD_DISPLY_H
#define _SD_DISPLY_H

#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/irqreturn.h>

#define LAYER_MAX_NUM 2

/* register update TYPEs */
#define DC_NO_UPDATE              0x0000
#define DC_GP_UPDATE              0x0001
#define DC_SP_UPDATE              0x0002
#define DC_DP_UPDATE              0x0004
#define DC_RDMA_UPDATE            0x0008
#define DC_MLC_UPDATE             0x0010
#define DC_GAMMA_UPDATE           0x0020
#define DC_DITHER_UPDATE          0x0040
#define DC_CSC_UPDATE             0x0080
#define DC_INT_UPDATE             0x0100
#define DC_RLE                    0x0200
#define DC_BG_UPDATE              0x0400
#define DC_CRC_UPDATE			  0x0800

typedef enum {
	DC_SP = 0,
	DC_GP,
	DISP_LAYER_MAX
} LAYER_ID;

typedef enum {
	DATA_YUV420,
	DATA_NV12,
	DATA_NV21,
	DATA_YUV422,
	DATA_YUV440,
	DATA_YUV444,
	DATA_AYUV4444,
	DATA_ARGB8888,
	DATA_ARGB2101010,
	DATA_RGB888,
	DATA_RGB666,
	DATA_RGB565,
	DATA_ARGB1555,
	DATA_ARGB4444,
	DATA_YONLY1,
	DATA_YONLY2,
	DATA_YONLY3,
	DATA_YONLY4,
	DATA_YONLY5,
	DATA_YONLY6,
	DATA_YONLY7,
	DATA_YONLY8,
	DATA_YONLY9,
	DATA_YONLY10,
	DATA_YUV420_10_16PACK
} DATA_FORMAT;

typedef enum {
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
} COMP_SWAP_MODE;

typedef enum {
	TILE_HSIZE_1    = 0b000,
	TILE_HSIZE_8    = 0b001,
	TILE_HSIZE_16   = 0b010,
	TILE_HSIZE_32   = 0b011,
	TILE_HSIZE_64   = 0b100,
	TILE_HSIZE_128  = 0b101,
} TILE_HSIZE;

typedef enum {
	TILE_VSIZE_1	= 0b000,
	TILE_VSIZE_2	= 0b001,
	TILE_VSIZE_4	= 0b010,
	TILE_VSIZE_8	= 0b011,
	TILE_VSIZE_16	= 0b100,
} TILE_VSIZE;

typedef enum {
	pixel_alpha = 0,
	global_alpha
} ALPHA_MODE;

typedef enum {
	ROT_DEFAULT         = 0b000,
	ROT_ROT             = 0b001,
	ROT_VFLIP           = 0b010,
	ROT_HFLIP           = 0b100
} ROT_TYPE;

typedef enum {
	UV_YUV444_RGB       = 0b00,
	UV_YUV422           = 0b01,
	UV_YUV440           = 0b10,
	UV_YUV420           = 0b11
} DATA_UV_MODE;

typedef enum {
	LINEAR_MODE 			= 0b000,
	RLE_COMPR_MODE			= 0b001,
	GPU_RAW_TILE_MODE		= 0b010,
	GPU_CPS_TILE_MODE		= 0b011,
	VPU_RAW_TILE_MODE		= 0b100,
	VPU_CPS_TILE_MODE		= 0b101,
	VPU_RAW_TILE_988_MODE	= 0b110,
} DATA_MODE;

typedef enum {
	FMT_INTERLEAVED     = 0b00,
	FMT_MONOTONIC       = 0b01,
	FMT_SEMI_PLANAR     = 0b10,
	FMT_PLANAR          = 0b11
} FRM_BUF_STR_FMT;

typedef enum {
	FBDC_U8U8U8U8 = 0xc,
	FBDC_U16      = 0x9,
	FBDC_R5G6B5   = 0x5,
	FBDC_U8       = 0x0,
	FBDC_NV21     = 0x37,
	FBDC_YUV420_16PACK = 0x65
} FBDC_FMT;

typedef struct mlc_sf_ckey {
	u8 ckey_en;
	uint32_t a;
	uint32_t r_dn;
	uint32_t r_up;
	uint32_t g_dn;
	uint32_t g_up;
	uint32_t b_dn;
	uint32_t b_up;
} mlc_sf_ckey;

typedef struct mlc_sf_info {
	mlc_sf_ckey sf_ckey;
	u8 g_alpha_en;
	uint32_t g_alpha_value;
	u8 aflu_en;
	u8 aflu_sel;
	uint32_t aflu_timer;
	u8 slowdown_en;
	u8 vpos_prot_en;
	uint32_t prot_val;
	uint32_t z_order;
} mlc_sf_info;


typedef struct layer_crop {
	u8 crop_en;
	uint32_t h_pos_start;
	uint32_t h_pos_end;
	uint32_t v_pos_start;
	uint32_t v_pos_end;
	uint32_t hsize;
	uint32_t vsize;
} layer_crop;

typedef struct tile_info {
	uint32_t bpp;
	TILE_HSIZE hsize;
	TILE_VSIZE vsize;
} tile_info;

typedef struct scaler_info {
	uint32_t scaler_mode; // 0: NONE, 1: HS, 2: VS, 3: HS+VS
	uint32_t hs_width;
	uint32_t vs_height;
} scaler_info;

typedef struct rle_info {
	uint32_t rle_y_len;
	uint32_t rle_y_checksum;
	uint32_t rle_data_size;
} rle_info;

typedef struct bg_info {
	uint32_t bg_en;
	uint32_t bg_a_select;
	uint32_t bg_a_value;
	uint32_t bg_color;
	uint32_t a_pipe_bpa;
	uint64_t a_pipe_addr_a;
} bg_info;

typedef struct layer_info {
	uint32_t layer_dirty;
	uint32_t pipe_index;//index of indenpent device(dc,dp)
	uint32_t layer_enable;
	uint32_t width;
	uint32_t height;
	uint32_t bpp;
	uint32_t frm_format;
	uint64_t frame_addr_y;
	uint64_t frame_addr_u;
	uint64_t frame_addr_v;
	uint32_t y_stride;
	uint32_t u_stride;
	uint32_t v_stride;
	/*for memory access offset*/
	uint32_t offset_x;
	uint32_t offset_y;
	/*for lcd display position*/
	uint32_t pos_x;
	uint32_t pos_y;
	uint32_t clut_en;
	/*crop*/
	layer_crop crop_info;
	tile_info tile;
	scaler_info hvs_info;
	rle_info rle;
	uint32_t swap_mode;
	uint32_t endian_mode;
	uint32_t alpha_mode;
	uint32_t rot_type;
	uint32_t data_uv_mode;
	uint32_t data_mode;
	uint32_t frm_buf_str_fmt;
	/*mlc*/
	mlc_sf_info sf_info;
	uint32_t fbdc_fmt;
} layer_info;

struct disp_info {
	uint32_t dev_id;//disp0,disp1,disp2,disp3.
	uint32_t flip;
	bg_info backgroud;
	uint32_t canvas_color;
	layer_info layers[LAYER_MAX_NUM];//dc-sp,dc-gp,dp-gp0,dp-gp1,dp-sp0,dp-sp1.
};


struct dc_device;

typedef struct dc_config_t {
	uint32_t type;
	uint8_t id;
	ulong reg;
	uint32_t val;
} dc_config_t;


struct dc_ops {
	int (*init)(struct dc_device *dev);
	int (*reset)(struct dc_device *dev);
	int (*config)(struct dc_device *dev, struct disp_info *dpu_info);
	int (*config_done)(struct dc_device *dev);
	int (*check)(struct dc_device *dev);
	enum irqreturn (*irq_handler)(int irq, void *dev);
	void (*vsync_set)(struct dc_device *dev, int32_t enable);
};

struct dc_device {
	uint8_t id;
	ulong reg_base;
	uint32_t irq;
	uint32_t tcon_irq;
	struct dc_config_t config;
	const struct dc_ops *ops;
	uint32_t status;
};

typedef enum sd_display_stat_t {
	SD_READY = 0,
	SD_BUSY,
	SD_INVALID,
	SD_UNKNOWN,
} sd_display_stat_t;

struct sd_display_res {
	sd_display_stat_t state;
	uint8_t id;

	uint32_t layer_n;
	struct dc_device dc;
	struct list_head todo;
	atomic_t ready;
	wait_queue_head_t go;
#ifdef SD_NEED_REGIST_DEV
	struct miscdevice misc_dev;
#endif
	struct work_struct irq_queue;
	uint8_t disp_thread_status;
};
#endif
