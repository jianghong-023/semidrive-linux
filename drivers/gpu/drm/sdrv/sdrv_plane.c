#include "sdrv_plane.h"
#include "kunlun_drm_gem.h"

#include <drm/drm.h>
#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>

static int debug_layer = 0;
module_param(debug_layer, int, 0664);

LIST_HEAD(pipe_list_head);
#define to_kunlun_plane(x) container_of(x, struct kunlun_plane, base)
#define to_kunlun_crtc(x) container_of(x, struct kunlun_crtc, base)

#define FRAC_16_16(mult, div)    (((mult) << 16) / (div))
int ops_register(struct ops_entry *entry, struct list_head *head)
{
	struct ops_list *list;

	list = kzalloc(sizeof(struct ops_list), GFP_KERNEL);
	if (!list)
		return -ENOMEM;

	list->entry = entry;
	list_add(&list->head, head);

	return 0;
}

void *ops_attach(const char *str, struct list_head *head)
{
	struct ops_list *list;
	const char *ver;

	list_for_each_entry(list, head, head) {
		ver = list->entry->ver;

		if (!strcmp(str, ver))
			return list->entry->ops;
	}

	DRM_ERROR("attach disp ops %s failed\n", str);
	return NULL;
}

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

static int get_tile_hsize(uint32_t select)
{
	int value;
	switch (select) {
		case TILE_HSIZE_1:
			value = 1;
			break;
		case TILE_HSIZE_8:
			value = 8;
			break;
		case TILE_HSIZE_16:
			value = 16;
			break;
		case TILE_HSIZE_32:
			value = 32;
			break;
		case TILE_HSIZE_64:
			value = 64;
			break;
		case TILE_HSIZE_128:
			value = 128;
			break;
		default:
			value = 0;
			break;
	}
	return value;
}

static int get_tile_vsize(uint32_t select)
{
	int value;
	switch (select) {
		case TILE_VSIZE_1:
			value = 1;
			break;
		case TILE_VSIZE_2:
			value = 2;
			break;
		case TILE_VSIZE_4:
			value = 4;
			break;
		case TILE_VSIZE_8:
			value = 8;
			break;
		case TILE_VSIZE_16:
			value = 16;
			break;
		default:
			value = 0;
			break;
	}
	return value;
}

static int sdrv_pix_comp(struct dpc_layer *layer)
{
	uint32_t format = layer->format;
	struct pix_comp *comp = &layer->comp;

	switch (format) {
		case DRM_FORMAT_XRGB8888:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->format_string = "DRM_FORMAT_XRGB8888";
			break;
		case DRM_FORMAT_XBGR8888:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_BGR;
			comp->format_string = "DRM_FORMAT_XBGR8888";
			break;
		case DRM_FORMAT_BGRX8888:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_B_GRA;
			comp->format_string = "DRM_FORMAT_BGRX8888";
			break;
		case DRM_FORMAT_ARGB8888:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->format_string = "DRM_FORMAT_ARGB8888";
			break;
		case DRM_FORMAT_ABGR8888:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_BGR;
			comp->format_string = "DRM_FORMAT_ABGR8888";
			break;
		case DRM_FORMAT_BGRA8888:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_B_GRA;
			comp->format_string = "DRM_FORMAT_BGRA8888";
			break;
		case DRM_FORMAT_RGB888:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->format_string = "DRM_FORMAT_RGB888";
			break;
		case DRM_FORMAT_BGR888:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_BGR;
			comp->format_string = "DRM_FORMAT_BGR888";
			break;
		case DRM_FORMAT_RGB565:
			comp->bpa = 0;
			comp->bpy = comp->bpv = 5;
			comp->bpu = 6;
			comp->swap = SWAP_A_RGB;
			comp->format_string = "DRM_FORMAT_RGB565";
			break;
		case DRM_FORMAT_BGR565:
			comp->bpa = 0;
			comp->bpy = comp->bpv = 5;
			comp->bpu = 6;
			comp->swap = SWAP_A_BGR;
			comp->format_string = "DRM_FORMAT_BGR565";
			break;
		case DRM_FORMAT_XRGB4444:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 4;
			comp->swap = SWAP_A_RGB;
			comp->format_string = "DRM_FORMAT_XRGB4444";
			break;
		case DRM_FORMAT_XBGR4444:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 4;
			comp->swap = SWAP_A_BGR;
			comp->format_string = "DRM_FORMAT_XBGR4444";
			break;
		case DRM_FORMAT_BGRX4444:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 4;
			comp->swap = SWAP_B_GRA;
			comp->format_string = "DRM_FORMAT_BGRX4444";
			break;
		case DRM_FORMAT_ARGB4444:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 4;
			comp->swap = SWAP_A_RGB;
			comp->format_string = "DRM_FORMAT_ARGB4444";
			break;
		case DRM_FORMAT_ABGR4444:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 4;
			comp->swap = SWAP_A_BGR;
			comp->format_string = "DRM_FORMAT_ABGR4444";
			break;
		case DRM_FORMAT_BGRA4444:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 4;
			comp->swap = SWAP_B_GRA;
			comp->format_string = "DRM_FORMAT_BGRA4444";
			break;
		case DRM_FORMAT_XRGB1555:
			comp->bpa = 1;
			comp->bpy = comp->bpu = comp->bpv = 5;
			comp->swap = SWAP_A_RGB;
			comp->format_string = "DRM_FORMAT_XRGB1555";
			break;
		case DRM_FORMAT_XBGR1555:
			comp->bpa = 1;
			comp->bpy = comp->bpu = comp->bpv = 5;
			comp->swap = SWAP_A_BGR;
			comp->format_string = "DRM_FORMAT_XBGR1555";
			break;
		case DRM_FORMAT_BGRX5551:
			comp->bpa = 1;
			comp->bpy = comp->bpu = comp->bpv = 5;
			comp->swap = SWAP_B_GRA;
			comp->format_string = "DRM_FORMAT_BGRX5551";
			break;
		case DRM_FORMAT_ARGB1555:
			comp->bpa = 1;
			comp->bpy = comp->bpu = comp->bpv = 5;
			comp->swap = SWAP_A_RGB;
			comp->format_string = "DRM_FORMAT_ARGB1555";
			break;
		case DRM_FORMAT_ABGR1555:
			comp->bpa = 1;
			comp->bpy = comp->bpu = comp->bpv = 5;
			comp->swap = SWAP_A_BGR;
			comp->format_string = "DRM_FORMAT_ABGR1555";
			break;
		case DRM_FORMAT_BGRA5551:
			comp->bpa = 1;
			comp->bpy = comp->bpu = comp->bpv = 5;
			comp->swap = SWAP_B_GRA;
			comp->format_string = "DRM_FORMAT_BGRA5551";
			break;
		case DRM_FORMAT_XRGB2101010:
			comp->bpa = 2;
			comp->bpy = comp->bpu = comp->bpv = 10;
			comp->swap = SWAP_A_RGB;
			comp->format_string = "DRM_FORMAT_XRGB2101010";
			break;
		case DRM_FORMAT_XBGR2101010:
			comp->bpa = 2;
			comp->bpy = comp->bpu = comp->bpv = 10;
			comp->swap = SWAP_A_BGR;
			comp->format_string = "DRM_FORMAT_XBGR2101010";
			break;
		case DRM_FORMAT_BGRX1010102:
			comp->bpa = 2;
			comp->bpy = comp->bpu = comp->bpv = 10;
			comp->swap = SWAP_B_GRA;
			comp->format_string = "DRM_FORMAT_BGRX1010102";
			break;
		case DRM_FORMAT_ARGB2101010:
			comp->bpa = 2;
			comp->bpy = comp->bpu = comp->bpv = 10;
			comp->swap = SWAP_A_RGB;
			comp->format_string = "DRM_FORMAT_ARGB2101010";
			break;
		case DRM_FORMAT_ABGR2101010:
			comp->bpa = 2;
			comp->bpy = comp->bpu = comp->bpv = 10;
			comp->swap = SWAP_A_BGR;
			comp->format_string = "DRM_FORMAT_ABGR2101010";
			break;
		case DRM_FORMAT_BGRA1010102:
			comp->bpa = 2;
			comp->bpy = comp->bpu = comp->bpv = 10;
			comp->swap = SWAP_B_GRA;
			comp->format_string = "DRM_FORMAT_BGRA1010102";
			break;
		case DRM_FORMAT_R8:
			comp->bpy = 8;
			comp->bpa = comp->bpu = comp->bpv = 0;
			comp->swap = SWAP_A_RGB;
			comp->format_string = "DRM_FORMAT_R8";
			break;

		case DRM_FORMAT_YUYV:
			comp->bpa = comp->bpv = 0;
			comp->bpy = comp->bpu = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->endin = 1;
			comp->uvmode = UV_YUV422;
			comp->format_string = "DRM_FORMAT_YUYV";
			break;

		case DRM_FORMAT_UYVY:
			comp->bpa = comp->bpv = 0;
			comp->bpy = comp->bpu = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->uvmode = UV_YUV422;
			comp->format_string = "DRM_FORMAT_UYVY";
			break;
		case DRM_FORMAT_VYUY:
			comp->bpa = comp->bpv = 0;
			comp->bpy = comp->bpu = 8;
			comp->swap = SWAP_A_RGB;
			comp->uvswap = 1;
			comp->is_yuv = 1;
			comp->uvmode = UV_YUV422;
			comp->format_string = "DRM_FORMAT_VYUY";
			break;
		case DRM_FORMAT_AYUV:
			comp->bpa = comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->format_string = "DRM_FORMAT_AYUV";
			break;
		case DRM_FORMAT_NV12:
			comp->bpa = comp->bpv = 0;
			comp->bpy = comp->bpu = 8;
			comp->swap = SWAP_A_RGB;
			comp->uvswap = 1;
			comp->is_yuv = 1;
			comp->uvmode = UV_YUV420;
			comp->buf_fmt = FMT_SEMI_PLANAR;
			comp->format_string = "DRM_FORMAT_NV12";
			break;
		case DRM_FORMAT_NV21:
			comp->bpa = comp->bpv = 0;
			comp->bpy = comp->bpu = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->uvmode = UV_YUV420;
			comp->buf_fmt = FMT_SEMI_PLANAR;
			comp->format_string = "DRM_FORMAT_NV21";
			break;
		case DRM_FORMAT_YUV420:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->uvmode = UV_YUV420;
			comp->buf_fmt = FMT_PLANAR;
			comp->format_string = "DRM_FORMAT_YUV420";
			break;
		case DRM_FORMAT_YVU420:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->uvswap = 1;
			comp->is_yuv = 1;
			comp->uvmode = UV_YUV420;
			comp->buf_fmt = FMT_PLANAR;
			comp->format_string = "DRM_FORMAT_YUV420";
			break;
		case DRM_FORMAT_YUV422:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->uvmode = UV_YUV422;
			comp->buf_fmt = FMT_PLANAR;
			comp->format_string = "DRM_FORMAT_YUV422";
			break;
		case DRM_FORMAT_YUV444:
			comp->bpa = 0;
			comp->bpy = comp->bpu = comp->bpv = 8;
			comp->swap = SWAP_A_RGB;
			comp->is_yuv = 1;
			comp->buf_fmt = FMT_PLANAR;
			comp->format_string = "DRM_FORMAT_YUV444";
			break;

		default:
			DRM_ERROR("unsupported format[%08x]\n", format);
			return -EINVAL;
	}
	return 0;
}

static int sdrv_set_tile_ctx(struct dpc_layer *layer)
{
	struct tile_ctx *ctx = &layer->ctx;
	uint32_t format = layer->format;
	uint32_t x1, y1, x2, y2;

	switch (format) {
		case DRM_FORMAT_XRGB8888:
		case DRM_FORMAT_XBGR8888:
		case DRM_FORMAT_BGRX8888:
		case DRM_FORMAT_ARGB8888:
		case DRM_FORMAT_ABGR8888:
		case DRM_FORMAT_BGRA8888:
			switch (layer->modifier) {
				case DRM_FORMAT_MOD_SEMIDRIVE_8X8_TILE:
					ctx->tile_hsize = TILE_HSIZE_8;
					ctx->tile_vsize = TILE_VSIZE_8;
					ctx->data_mode = GPU_RAW_TILE_MODE;
				break;
				case DRM_FORMAT_MOD_SEMIDRIVE_16X4_TILE:
					ctx->tile_hsize = TILE_HSIZE_16;
					ctx->tile_vsize = TILE_VSIZE_4;
					ctx->data_mode = GPU_RAW_TILE_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_32X2_TILE:
					ctx->tile_hsize = TILE_HSIZE_32;
					ctx->tile_vsize = TILE_VSIZE_2;
					ctx->data_mode = GPU_RAW_TILE_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_8X8_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_8;
					ctx->tile_vsize = TILE_VSIZE_8;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_U8U8U8U8;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_16X4_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_16;
					ctx->tile_vsize = TILE_VSIZE_4;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_U8U8U8U8;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_32X2_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_32;
					ctx->tile_vsize = TILE_VSIZE_2;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_U8U8U8U8;
					break;
				default:
					ctx->data_mode = LINEAR_MODE;
					break;
			}
			break;
		case DRM_FORMAT_RGB565:
		case DRM_FORMAT_BGR565:
			switch (layer->modifier) {
				case DRM_FORMAT_MOD_SEMIDRIVE_16X8_TILE:
					ctx->tile_hsize = TILE_HSIZE_16;
					ctx->tile_vsize = TILE_VSIZE_8;
					ctx->data_mode = GPU_RAW_TILE_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_32X4_TILE:
					ctx->tile_hsize = TILE_HSIZE_32;
					ctx->tile_vsize = TILE_VSIZE_4;
					ctx->data_mode = GPU_RAW_TILE_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_64X2_TILE:
					ctx->tile_hsize = TILE_HSIZE_64;
					ctx->tile_vsize = TILE_VSIZE_2;
					ctx->data_mode = GPU_RAW_TILE_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_16X8_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_16;
					ctx->tile_vsize = TILE_VSIZE_8;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_R5G6B5;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_32X4_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_32;
					ctx->tile_vsize = TILE_VSIZE_4;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_R5G6B5;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_64X2_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_64;
					ctx->tile_vsize = TILE_VSIZE_2;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_R5G6B5;
					break;
				default:
					ctx->data_mode = LINEAR_MODE;
					break;
			}
			break;
		case DRM_FORMAT_R8:
			switch (layer->modifier) {
				case DRM_FORMAT_MOD_SEMIDRIVE_32X8_TILE:
					ctx->tile_hsize = TILE_HSIZE_32;
					ctx->tile_vsize = TILE_VSIZE_8;
					ctx->data_mode = GPU_RAW_TILE_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_64X4_TILE:
					ctx->tile_hsize = TILE_HSIZE_64;
					ctx->tile_vsize = TILE_VSIZE_4;
					ctx->data_mode = GPU_RAW_TILE_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_128X2_TILE:
					ctx->tile_hsize = TILE_HSIZE_128;
					ctx->tile_vsize = TILE_VSIZE_2;
					ctx->data_mode = GPU_RAW_TILE_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_32X8_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_32;
					ctx->tile_vsize = TILE_VSIZE_8;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_U8;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_64X4_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_64;
					ctx->tile_vsize = TILE_VSIZE_4;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_U8;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_128X2_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_128;
					ctx->tile_vsize = TILE_VSIZE_2;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_U8;
					break;
				default:
					ctx->data_mode = LINEAR_MODE;
					break;
			}
			break;
		case DRM_FORMAT_NV12:
		case DRM_FORMAT_NV21:
			switch (layer->modifier) {
				case DRM_FORMAT_MOD_SEMIDRIVE_CODA_16X16_TILE:
					ctx->tile_hsize = TILE_HSIZE_16;
					ctx->tile_vsize = TILE_VSIZE_16;
					ctx->data_mode = VPU_RAW_TILE_988_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_WAVE_32X8_TILE:
					ctx->tile_hsize = TILE_HSIZE_32;
					ctx->tile_vsize = TILE_VSIZE_8;
					ctx->data_mode = VPU_RAW_TILE_MODE;
					break;
				case DRM_FORMAT_MOD_SEMIDRIVE_WAVE_32X8_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_32;
					ctx->tile_vsize = TILE_VSIZE_8;
					ctx->data_mode = VPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_NV21;
					break;
				/*case DRM_FORMAT_MOD_SEMIDRIVE_WAVE_16X8_FBDC_TILE:
					ctx->tile_hsize = TILE_HSIZE_16;
					ctx->tile_vsize = TILE_VSIZE_8;
					ctx->data_mode = GPU_CPS_TILE_MODE;
					ctx->fbdc_fmt = FBDC_YUV420_16PACK;
					break;*/
				default:
					ctx->data_mode = LINEAR_MODE;
					break;
			}
			break;
		default:
			ctx->data_mode = LINEAR_MODE;
			break;
	}

	x1 = layer->src_x;
	y1 = layer->src_y;
	x2 = layer->src_x + layer->src_w;
	y2 = layer->src_y + layer->src_h;

	ctx->tile_hsize_value = get_tile_hsize(ctx->tile_hsize);
	ctx->tile_vsize_value = get_tile_vsize(ctx->tile_vsize);

	ctx->x1 = ALIGN_DOWN(x1, ctx->tile_hsize_value);
	ctx->y1 = ALIGN_DOWN(y1, ctx->tile_vsize_value);

	ctx->x2 = ALIGN(x2, ctx->tile_hsize_value);
	ctx->y2 = ALIGN(y2, ctx->tile_vsize_value);

	ctx->width = ctx->x2 - ctx->x1;
	ctx->height = ctx->y2 - ctx->y1;
	if ((format == DRM_FORMAT_NV12) || (format == DRM_FORMAT_NV21)) {
		if (ctx->width / ctx->tile_hsize_value % 2 == 1) {
			ctx->x2 += ctx->tile_hsize_value;
			ctx->width = ctx->x2 - ctx->x1;
		}
	}

	DRM_DEBUG("source (%d, %d - %d, %d) fbc  x2y2(%d, %d) - x1y1(%d, %d) =  width x height = %dx%d\n",
		x1, y1, x2, y2,
		ctx->x2, ctx->y2, ctx->x1, ctx->y1,
		ctx->width, ctx->height);

	if ((ctx->data_mode == GPU_RAW_TILE_MODE) ||
		(ctx->data_mode == GPU_CPS_TILE_MODE) ||
		(ctx->data_mode == VPU_RAW_TILE_MODE) ||
		(ctx->data_mode == VPU_CPS_TILE_MODE) ||
		(ctx->data_mode == VPU_RAW_TILE_988_MODE))
		ctx->is_tile_mode = true;
	else
		ctx->is_tile_mode = false;

	if ((ctx->data_mode == GPU_CPS_TILE_MODE) ||
		(ctx->data_mode == VPU_CPS_TILE_MODE))
		ctx->is_fbdc_cps = true;
	else
		ctx->is_fbdc_cps = false;

	return 0;
}

static int kunlun_plane_size_check(struct drm_crtc_state *crtc_state,
				struct drm_plane_state *state)
{
	uint32_t src_w, src_h;
	uint32_t crtc_x, crtc_y;
	uint32_t dst_w, dst_h;
	uint32_t crtc_hdisplay, crtc_vdisplay;
	struct drm_rect *src = &state->src;
	int x1, y1, x2, y2;
	int dx = 0, dy = 0;

	src_w = (drm_rect_width(src) >> 16);
	src_h = (drm_rect_height(src) >> 16);
	crtc_x = state->crtc_x;
	crtc_y = state->crtc_y;

	crtc_hdisplay = crtc_state->mode.crtc_hdisplay;
	crtc_vdisplay = crtc_state->mode.crtc_vdisplay;
	dst_w = drm_rect_width(&state->dst);
	dst_h = drm_rect_height(&state->dst);

	if (((dst_w + crtc_x) > crtc_hdisplay) || ((dst_h + crtc_y) > crtc_vdisplay) ) {
		DRM_DEBUG("size check failed: (%d, %d ++ %d, %d)\n", dst_w, dst_h, crtc_x, crtc_y);
		return -EINVAL;
	}

	// 2:Down/Up sampling the Max or rotation operation, output Hsize 2560 Pixel
	if ((src_w != dst_w) && (dst_w >= 2560))
		return -EINVAL;

	// 1:When input format yuv 420/4422, the Max.Input HSIZE 2560 Pixel
	switch (state->fb->format->format) {
		case DRM_FORMAT_YUV420:
		case DRM_FORMAT_YUV422:
		case DRM_FORMAT_NV12:
		case DRM_FORMAT_NV21:
			if (src_w >= 2560)
				return -EINVAL;
			break;
	}

	if (!state->fb->modifier)
		return 0;

	/*checking tile and ifbc state*/
	// 3:When FBDC format the Max. Input HSIZE 2560 Pixel
	if (src_w >= 2560)
		return -EINVAL;

#if 1
	x1 = src->x1 >> 16;
	x2 = src->x2 >> 16;
	y1 = src->y1 >> 16;
	y2 = src->y2 >> 16;

	/*check tiled block size*/
	dx += x1 % 8;
	dx += x2 % 8;
	dy += y1 % 8;
	dy += y2 % 8;

	if (((src_w + crtc_x + dx) > crtc_hdisplay) || ((src_h + crtc_y + dy) > crtc_vdisplay)) {
		DRM_DEBUG("mlc crop size over than mlc display: format 0x%x (%d) " DRM_RECT_FP_FMT " , fb widthx height %dx%d\n",
			state->fb->format->format, state->fb->modifier? 1: 0, DRM_RECT_FP_ARG(src),
			state->fb->width, state->fb->height);
		return -EINVAL;
	}
#endif

	return 0;
}

static int position_dest(int start, int end, struct drm_rect *rect, struct drm_rect *out_dest)
{
	int enable = 0;
	int x1, x2;

	if ((rect->x1 >= end) || (rect->x2 <= start)) {
		enable = 0;
	} else 	if (start < rect->x1 &&  rect->x2 < end) {
		enable = 1;
		out_dest->x1 = rect->x1 - start;
		out_dest->x2 = rect->x2 - start;
	} else {
		enable = 1;
		x1 = rect->x1 < start? start: rect->x1;
		x2 = rect->x2 > end? end: rect->x2;

		out_dest->x1 = x1 - start;
		out_dest->x2 = x2 - start;
	}

	return enable;
}

static void dump_layer(const char *func, struct dpc_layer *layer) {
	if (debug_layer) {
		PRINT("[dump] %s *ENABLE = %d*,  format: 0x%x  source (%d, %d, %d, %d) => dest (%d, %d, %d, %d)\n",
				func, layer->enable, layer->format,
				layer->src_x, layer->src_y, layer->src_w, layer->src_h,
				layer->dst_x, layer->dst_y, layer->dst_w, layer->dst_h);
	}
}
#define MAX(a, b) (a) > (b)? (a): (b)
int sdrv_layer_config(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	struct drm_crtc *crtc = state->crtc;
	struct kunlun_plane *klp = to_kunlun_plane(plane);
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(state->crtc);
	struct drm_framebuffer *fb = state->fb;
	struct drm_rect *src = &state->src;
//	struct drm_rect *dest = &state->dst;
	struct drm_display_mode *mode = &crtc->state->adjusted_mode;
	struct drm_gem_object *obj;
	struct kunlun_gem_object *gem_obj;

	dma_addr_t dma_addr;
	struct dpc_layer *layer;
	int i, j;
	int case_num = 0;
	int half_width = mode->hdisplay / 2;
	struct drm_rect source;
	struct drm_rect display;

	source.x1 = src->x1 >> 16;
	source.y1 = src->y1 >> 16;
	source.x2 = src->x2 >> 16;
	source.y2 = src->y2 >> 16;

	display.x1 = state->crtc_x;
	display.y1 = state->crtc_y;
	display.x2 = display.x1 + state->crtc_w;
	display.y2 = display.y1 + state->crtc_h;

	//case 1
	if (source.x2 <= half_width) {
		case_num = 1;
	}else if (source.x1 >= half_width) {
		case_num = 2;
	} else if (source.x1 <half_width && source.x2 > half_width){
		case_num = 3;
	} else {
		DRM_ERROR("error case_num : source x1 %d, x2 %d, y1 %d, y2 %d\n",
			source.x1, source.x2, source.y1, source.y2);
	}

	// one plane maybe contain 2 pipe for combine mode.
	for (i = 0;i < klp->num_pipe; i++) {
		int start, end;
		struct sdrv_pipe *p = klp->pipes[i];

		layer = (struct dpc_layer *)&p->layer_data;
		layer->index = p->id;
		layer->zpos = state->zpos;
		layer->alpha = klp->alpha;
		layer->blend_mode = klp->blend_mode;
		layer->format = fb->format->format;
		layer->nplanes = fb->format->num_planes;
		layer->modifier = fb->modifier;

		start = i == 0? 0: half_width;
		end = i == 0? half_width: half_width *2;
		layer->src_x = source.x1;
		layer->src_y = source.y1;
		layer->src_w = source.x2 - source.x1;
		layer->src_h =source.y2 - source.y1;

		layer->dst_x = display.x1;
		layer->dst_y = display.y1;
		layer->dst_w = display.x2 - display.x1;
		layer->dst_h = display.y2 - display.y1;
		layer->enable = 1;
		// porter-duff
		layer->pd_type = klp->pd_type;
		layer->pd_mode = klp->pd_mode;
		// PRINT("plane id = <%d>, index = %d\n", plane->base.id, drm_plane_index(plane));
		if (kcrtc->combine_mode == 3) {
			struct drm_rect temp;
			if (position_dest(start, end, &display, &temp)) {
				layer->dst_x = temp.x1;
				layer->dst_w = temp.x2 - temp.x1;
				if (start == 0) {
					layer->src_x = source.x1;
					layer->src_w = layer->dst_w;
				} else {
					layer->src_x = source.x2 - layer->dst_w;
					layer->src_w = layer->dst_w;
				}
				layer->enable = 1;
			} else {
				// not use this pipe
				layer->enable = 0;
			}

			layer->src_y = source.y1;
			layer->src_h =source.y2 - source.y1;

			layer->dst_y = display.y1;
			layer->dst_h = layer->src_h;
		}
		dump_layer(__func__, layer);
		for (j = 0; j < layer->nplanes; j++) {
			obj = drm_gem_fb_get_obj(fb, j);
			gem_obj = to_kunlun_obj(obj);

			dma_addr = gem_obj->paddr + fb->offsets[j];
			layer->addr_l[j] = get_l_addr(dma_addr);
			layer->addr_h[j] = get_h_addr(dma_addr);
			layer->pitch[j] = fb->pitches[j];
		}
		sdrv_pix_comp(layer);

		if (layer->modifier)
			sdrv_set_tile_ctx(layer);
	}

	return 0;
}

static int sdrv_custom_layer_config(struct post_layer *p, struct dpc_layer *layer)
{

	if (!p) {
		DRM_ERROR("post_layer pointer invalid\n");
		return -EINVAL;
	}
	// one plane maybe contain 2 pipe for combine mode.
	layer->index = p->index;
	layer->zpos = p->zpos;
	layer->alpha =  p->alpha;
	layer->blend_mode = p->blend_mode;
	layer->format = p->format;
	layer->nplanes = 1;
	layer->modifier = p->modifier;

	layer->src_x = p->src_x;
	layer->src_y =  p->src_y;
	layer->src_w = p->src_w;
	layer->src_h = p->src_h;

	layer->dst_x = p->dst_x;
	layer->dst_y = p->dst_y;
	layer->dst_w = p->dst_w;
	layer->dst_h = p->dst_h;
	layer->enable = 1;
	// porter-duff
	layer->pd_type = p->pd_type;
	layer->pd_mode = p->pd_mode;
	// PRINT("plane id = <%d>, index = %d\n", plane->base.id, drm_plane_index(plane));

	dump_layer(__func__, layer);

	layer->addr_l[0] = layer->addr_l[0];
	layer->addr_h[0] = p->addr_h[0];
	layer->pitch[0] = p->pitch[0];

	sdrv_pix_comp(layer);

	if (layer->modifier)
		sdrv_set_tile_ctx(layer);

	return 0;
}

int sdrv_custom_plane_update(struct drm_device *dev, struct sdrv_post_config *post)
{
	int i;
	struct drm_plane *plane;
	struct drm_crtc *crtc = NULL;
	struct kunlun_plane *klp;

	for (i = 0; i < post->num_buffer; i++) {
		struct post_layer *layer;
		layer = &post->layers[i];
		plane = drm_plane_find(dev, layer->plane_id);
		klp = to_kunlun_plane(plane);
		crtc = drm_crtc_find(dev, layer->crtc_id);
		/*
		 * can't update plane when du is disabled.
		 */
		if (WARN_ON(!crtc))
			return -EINVAL;


		for (i = 0;i < klp->num_pipe; i++) {
			struct sdrv_pipe *p = klp->pipes[i];
			sdrv_custom_layer_config(layer, &p->layer_data);

			p->ops->update(p, &p->layer_data);
		}

		klp->pd_type = PD_NONE;
		klp->pd_mode = -1;

		if (klp->plane_status != PLANE_ENABLE)
			klp->plane_status = PLANE_ENABLE;
	}

	/**
		crtc update
	*/
	if (crtc) {
		int ret;
		struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
		struct sdrv_dpc *dpc = kcrtc->master;

		ret = dpc->ops->update(dpc, NULL, 0);

		if (ret) {
			DRM_ERROR("update plane[%d] failed\n", i);
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL(sdrv_custom_plane_update);

static int kunlun_plane_atomic_check(struct drm_plane *plane,
				struct drm_plane_state *state)
{
	int ret;
	struct drm_crtc *crtc = state->crtc;
	struct drm_crtc_state *crtc_state;
	struct drm_framebuffer *fb = state->fb;
	struct kunlun_plane *klp = to_kunlun_plane(plane);
	struct drm_rect clip;
	int min_scale = DRM_PLANE_HELPER_NO_SCALING;
	int max_scale = DRM_PLANE_HELPER_NO_SCALING;
	/*scaling feature not supported*/
#if 1
	if (klp->cap->scaling) {
		min_scale =  FRAC_16_16(1, 1);
		max_scale = FRAC_16_16(2, 1);
	}
#endif

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
	if (ret) {
		DRM_DEBUG("scaling feature may not work\n");
		return ret;
	}

	if (!state->visible)
		return -EINVAL;

	ret = kunlun_plane_size_check(crtc_state, state);
	if (ret)
		return ret;

#if 0
	sdrv_layer_config(plane, state);

	for (i = 0; i < klp->num_pipe; i++) {
		struct sdrv_pipe *p = klp->pipes[i];
		struct sdrv_dpc *dpc = p->dpc;
		struct dpc_layer *layer;

		layer = (struct dpc_layer *) (&p->layer_data);
		dump_layer(__func__, layer);
		if (p->ops && p->ops->check)
			ret |= p->ops->check(p, layer);
	}
#endif

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
	int i;
	/*
	 * can't update plane when du is disabled.
	 */
	if (WARN_ON(!crtc))
		return;

	if (!klp->kcrtc->recover_done)
		return;

	if (!state->visible) {
		kunlun_plane_atomic_disable(plane, old_state);
		return;
	}

	sdrv_layer_config(plane, state);
	for (i = 0;i < klp->num_pipe; i++) {
		struct sdrv_pipe *p = klp->pipes[i];
		p->ops->update(p, &p->layer_data);
	}

	klp->pd_type = PD_NONE;
	klp->pd_mode = -1;

	if (klp->plane_status != PLANE_ENABLE)
		klp->plane_status = PLANE_ENABLE;
}

static void kunlun_plane_atomic_disable(struct drm_plane *plane,
					 struct drm_plane_state *old_state)
{
	struct kunlun_plane *klp = to_kunlun_plane(plane);
	struct kunlun_crtc *kcrtc = klp->kcrtc;
	int i;

	if (!old_state->crtc)
		return;

	if (!klp->kcrtc->recover_done)
		return;

	for (i = 0;i < klp->num_pipe; i++) {
		struct sdrv_pipe *p = klp->pipes[i];
		/* disable dummy plane */
		if (kcrtc->master->dpc_type >= DPC_TYPE_DUMMY) {
			PRINT("plane %d disabled\n", plane->base.id);
			sdrv_rpcall_start(false);
		}
		p->ops->disable(p);
	}

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

	if (klp->cap->rotation) {
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

static uint32_t kunlun_plane_capabilities_property(struct pipe_capability *cap)
{
	uint32_t value = 0;

	value = BIT(PROP_PLANE_CAP_RGB);
	if (cap->yuv)
		value |= BIT(PROP_PLANE_CAP_YUV);
	if (cap->yuv_fbc)
		value |= BIT(PROP_PLANE_CAP_YUV_FBC);
	if (cap->xfbc)
		value |= BIT(PROP_PLANE_CAP_XFBC);
	if (cap->rotation)
		value |= BIT(PROP_PLANE_CAP_ROTATION);
	if (cap->scaling)
		value |= BIT(PROP_PLANE_CAP_SCALING);

	return value;
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

	if (klp->cap->xfbc)
		INSTALL_RANGE_PROPERTY(fbdc_hsize_y, FBDC_HSIZE_Y, 0, UINT_MAX, 0);

	if (klp->cap->yuv_fbc)
		INSTALL_RANGE_PROPERTY(fbdc_hsize_uv, FBDC_HSIZE_UV, 0, UINT_MAX, 0);

	klp->cap_mask = kunlun_plane_capabilities_property(klp->cap);
	INSTALL_RANGE_PROPERTY(cap_mask, CAP_MASK, 0, UINT_MAX, klp->cap_mask);
	INSTALL_RANGE_PROPERTY(pd_type, PD_TYPE, 0, 2, 0);
	INSTALL_RANGE_PROPERTY(pd_mode, PD_MODE, 0, 24, 0);
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

	if (klp->cap->xfbc)
		SET_PROPERTY(fbdc_hsize_y, FBDC_HSIZE_Y, uint32_t);

	if (klp->cap->yuv_fbc)
		SET_PROPERTY(fbdc_hsize_uv, FBDC_HSIZE_UV, uint32_t);

	SET_PROPERTY(cap_mask, CAP_MASK, uint32_t);
	SET_PROPERTY(pd_type, PD_TYPE, int);
	SET_PROPERTY(pd_mode, PD_MODE, int);
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

	if (klp->cap->xfbc)
		GET_PROPERTY(fbdc_hsize_y, FBDC_HSIZE_Y, uint32_t);

	if (klp->cap->yuv_fbc)
		GET_PROPERTY(fbdc_hsize_uv, FBDC_HSIZE_UV, uint32_t);

	GET_PROPERTY(cap_mask, CAP_MASK, uint32_t);
	GET_PROPERTY(pd_type, PD_TYPE, int);
	GET_PROPERTY(pd_mode, PD_MODE, int);
	pr_err("Invalid property\n");
	ret = -EINVAL;
done:
	return ret;
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

static struct drm_plane_funcs kunlun_plane_funcs = {
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
	struct kunlun_plane *plane;
	int ret;
	int i;

	for(i = 0; i < kcrtc->num_planes; i++) {
		plane = &kcrtc->planes[i];

		if (plane->type != DRM_PLANE_TYPE_PRIMARY &&
				plane->type != DRM_PLANE_TYPE_CURSOR)
			continue;

		ret = drm_universal_plane_init(kcrtc->drm, &plane->base,
				0, &kunlun_plane_funcs, plane->cap->formats,
				plane->cap->nformats, NULL,
				plane->type, NULL);
		if(ret) {
			DRM_DEV_ERROR(kcrtc->dev, "failed to init plane %d\n", ret);
			goto err_planes_fini;
		}

		drm_plane_helper_add(&plane->base, &kunlun_plane_helper_funcs);

		kunlun_plane_install_properties(&plane->base, &plane->base.base);

		if(plane->type == DRM_PLANE_TYPE_PRIMARY)
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
	struct kunlun_plane *plane;
	int ret;
	int i;

	for(i = 0; i < kcrtc->num_planes; i++) {
		plane = &kcrtc->planes[i];

		if(plane->type != DRM_PLANE_TYPE_OVERLAY)
			continue;

		ret = drm_universal_plane_init(kcrtc->drm, &plane->base,
				(1 << drm_crtc_index(&kcrtc->base)),
				&kunlun_plane_funcs, plane->cap->formats,
				plane->cap->nformats, NULL,
				plane->type, NULL);
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

