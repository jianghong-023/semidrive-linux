#ifndef __SDRV_G2D_CFG_H
#define __SDRV_G2D_CFG_H

#include <linux/types.h>

#if defined(USE_FREERTOS)
#include <sys/types.h>
#include <list.h>
#endif

#ifdef __YOCTO_G2D_TEST__
typedef    __u8    uint8_t;
typedef    __u16    uint16_t;
typedef    __u32    uint32_t;
typedef    unsigned long    uint64_t;
#endif

#define G2D_LAYER_MAX_NUM       6

#ifndef G2DLITE_API_USE
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
	UV_YUV444_RGB       = 0b00,
	UV_YUV422           = 0b01,
	UV_YUV440           = 0b10,
	UV_YUV420           = 0b11
} DATA_UV_MODE;

typedef enum {
	LINEAR_MODE             = 0b000,
	RLE_COMPR_MODE          = 0b001,
	GPU_RAW_TILE_MODE       = 0b010,
	GPU_CPS_TILE_MODE       = 0b011,
	VPU_RAW_TILE_MODE       = 0b100,
	VPU_CPS_TILE_MODE       = 0b101,
	VPU_RAW_TILE_988_MODE   = 0b110,
} DATA_MODE;

typedef enum {
	FMT_INTERLEAVED     = 0b00,
	FMT_MONOTONIC       = 0b01,
	FMT_SEMI_PLANAR     = 0b10,
	FMT_PLANAR          = 0b11
} FRM_BUF_STR_FMT;

typedef enum {
	ROT_DEFAULT         = 0b000,
	ROT_ROT             = 0b001,
	ROT_VFLIP           = 0b010,
	ROT_HFLIP           = 0b100
} ROT_TYPE;
#endif

#ifndef G2DLITE_API_USE
enum {
	BLEND_PIXEL_NONE = 0,
	BLEND_PIXEL_PREMULTI,
	BLEND_PIXEL_COVERAGE
};

typedef enum {
	ROTATION_TYPE_NONE    = 0b000,
	ROTATION_TYPE_ROT_90  = 0b001,
	ROTATION_TYPE_HFLIP   = 0b010,
	ROTATION_TYPE_VFLIP   = 0b100,
	ROTATION_TYPE_ROT_180 = ROTATION_TYPE_VFLIP | ROTATION_TYPE_HFLIP,
	ROTATION_TYPE_ROT_270 = ROTATION_TYPE_ROT_90 | ROTATION_TYPE_VFLIP | ROTATION_TYPE_HFLIP,
	ROTATION_TYPE_VF_90   = ROTATION_TYPE_VFLIP | ROTATION_TYPE_ROT_90,
	ROTATION_TYPE_HF_90   = ROTATION_TYPE_HFLIP | ROTATION_TYPE_ROT_90,
} rotation_type;
#endif

typedef enum {
    PD_NONE = 0,
    PD_SRC = 0x1,
    PD_DST = 0x2
} PD_LAYER_TYPE;

struct pix_comp {
	uint8_t bpa;
	uint8_t bpy;
	uint8_t bpu;
	uint8_t bpv;
	uint8_t swap;
	uint8_t endin;
	uint8_t uvswap;
	uint8_t is_yuv;
	uint8_t uvmode;
	uint8_t buf_fmt;
	const char *format_string;
};

struct tile_ctx {
	bool is_tile_mode;
	uint32_t data_mode;
	bool is_fbdc_cps;
	uint32_t tile_hsize;
	uint32_t tile_vsize;
	uint32_t tile_hsize_value;
	uint32_t tile_vsize_value;
	uint32_t fbdc_fmt;
	uint32_t x1;
	uint32_t y1;
	uint32_t x2;
	uint32_t y2;
	uint32_t width;
	uint32_t height;
};

struct g2d_buf {
#if defined(USE_FREERTOS)
	uint64_t vaddr;
#else
	int fd;
	uint64_t dma_addr;
	struct sg_table *sgt;
	struct dma_buf_attachment *attach;
	uint64_t vaddr;
	uint64_t size;
#endif

};


struct dpc_layer {
	uint8_t index; //plane index
	uint8_t enable;
	uint8_t nplanes;
	struct g2d_buf bufs[4];
	uint32_t addr_l[4];
	uint32_t addr_h[4];
	uint32_t pitch[4];
	uint32_t offsets[4];
	int16_t src_x;
	int16_t src_y;
	int16_t src_w;
	int16_t src_h;
	int16_t dst_x;
	int16_t dst_y;
	uint16_t dst_w;
	uint16_t dst_h;
	uint32_t format;
	uint32_t color_fmt;
	struct pix_comp comp;
	struct tile_ctx ctx;
	uint32_t alpha;
	uint32_t blend_mode;
	uint32_t rotation;
	uint32_t zpos;
	uint32_t xfbc;
	uint64_t modifier;
	uint32_t width;
	uint32_t height;
	uint32_t header_size_r;
	uint32_t header_size_y;
	uint32_t header_size_uv;
};

struct g2d_output_cfg{
	uint32_t width;
	uint32_t height;
	uint32_t fmt;
	uint64_t addr[4];
	uint32_t stride[4];
	uint32_t rotation;
	uint32_t color_fmt;
	uint32_t nplanes;
	struct g2d_buf bufs[4];
	uint32_t offsets[4];
};

struct g2d_bg_cfg {
	uint32_t en;
	uint32_t color;
	uint8_t g_alpha;
	uint8_t zorder;
	uint64_t aaddr;
	uint8_t bpa;
	uint32_t astride;
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
	struct g2d_buf abufs;
    PD_LAYER_TYPE pd_type;
};

struct g2d_input{
	unsigned char layer_num;
	struct g2d_bg_cfg bg_layer;
	struct dpc_layer layer[G2D_LAYER_MAX_NUM];
	struct g2d_output_cfg output;
	struct pix_comp out_comp;
	struct tile_ctx out_ctx;
	int layer_fences[G2D_LAYER_MAX_NUM];
	int release_fence;
};

struct g2d_pipe_capability {
	const uint32_t *formats;
	int nformats;
	int layer_type;
	int rotation;
	int scaling;
	int yuv;
	int yuv_fbc;
	int xfbc;
};

struct g2d_capability {
	int num_pipe;
	struct g2d_pipe_capability pipe_caps[G2D_LAYER_MAX_NUM];
};

#define G2D_COMMAND_BASE 0x00
#define G2D_IOCTL_BASE			'g'
#define G2D_IO(nr)			_IO(G2D_IOCTL_BASE,nr)
#define G2D_IOR(nr,type)		_IOR(G2D_IOCTL_BASE,nr,type)
#define G2D_IOW(nr,type)		_IOW(G2D_IOCTL_BASE,nr,type)
#define G2D_IOWR(nr,type)		_IOWR(G2D_IOCTL_BASE,nr,type)

#define G2D_IOCTL_GET_CAPABILITIES 		G2D_IOWR(G2D_COMMAND_BASE + 1, struct g2d_capability)
#define G2D_IOCTL_POST_CONFIG 		G2D_IOWR(G2D_COMMAND_BASE + 2, struct g2d_input)
#define G2D_IOCTL_FAST_COPY 		G2D_IOWR(G2D_COMMAND_BASE + 3, struct g2d_input)
#define G2D_IOCTL_FILL_RECT 		G2D_IOWR(G2D_COMMAND_BASE + 4, struct g2d_input)

#endif //__SDRV_G2D_CFG_H
