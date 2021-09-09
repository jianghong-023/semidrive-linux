/*
 * dpc.h
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#ifndef _sdrv_dpc_H_
#define _sdrv_dpc_H_

#include <linux/delay.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#include <video/videomode.h>
#include <linux/ktime.h>
#include <linux/kthread.h>
#include <linux/wait.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_vblank.h>
#include <uapi/drm/drm_mode.h>
#include <drm/drm_atomic.h>

#define ENTRY() pr_info_ratelimited("[drm] <%d> %20s enter\n", __LINE__, __func__)
#define PRINT(fmt, args...) pr_info("[drm] <%d> [%20s] " fmt, __LINE__, __func__, ##args)
// #define PRINT(fmt, args...)
#define DDBG(x) PRINT(#x " -> %d\n", x)
#define XDBG(x)	PRINT(#x " -> 0x%x\n", x)
#define PDBG(x)	PRINT(#x " -> %p\n", x)

#define SDRV_RDMA_MASK			BIT(0)
#define SDRV_RLE_MASK			BIT(1)
#define SDRV_MLC_MASK			BIT(2)
#define SDRV_RLE1_MASK			BIT(3)
#define SDRV_SDMA_DONE_MASK		BIT(4)
#define SDRV_TCON_SOF_MASK		BIT(5)
#define SDRV_TCON_EOF_MASK 		BIT(6)
#define SDRV_TCON_UNDERRUN_MASK	BIT(7)
#define	SDRV_DC_UNDERRUN_MASK	BIT(8)
#define	SDRV_LAYER_KICK_MASK	BIT(9)

#define MLC_JUMP            0x7000
#define S_MLC_JUMP          0x8000

struct pix_comp {
	u8 bpa;
	u8 bpy;
	u8 bpu;
	u8 bpv;
	u8 swap;
	u8 endin;
	u8 uvswap;
	u8 is_yuv;
	u8 uvmode;
	u8 buf_fmt;
	char *format_string;
};

enum {
	DPC_TYPE_DP = 0,
	DPC_TYPE_DC,
	DPC_TYPE_DUMMY,
	DPC_TYPE_RPCALL
};

enum {
	DELAY_POST_NONE = 0,
	DELAY_POST_START = 1,
	DELAY_POST_END
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

typedef enum {
    PD_NONE = 0,
    PD_SRC = 0b01,
    PD_DST = 0b10
} PD_LAYER_TYPE;

typedef enum {
    CLEAR = 0,
    SRC,
    DST,
    SRC_OVER,
    DST_OVER,
    SRC_IN,
    DST_IN,
    SRC_OUT,
    DST_OUT,
    SRC_ATOP,
    DST_ATOP,
    XOR,
    DARKEN,
    LIGHTEN,
    MULTIPLY,
    SCREEN,
    ADD,
    OVERLAY,
    SRC_SUB,
    DES_SUB
} pd_mode_t;

struct pd_t {
    u32 en;
    u32 zorder;
    pd_mode_t mode;
    u8 alpha_need;
};

struct dpc_layer {
	u8 index; //plane index
	u8 enable;
	u8 nplanes;
	u32 addr_l[4];
	u32 addr_h[4];
	u32 pitch[4];
	s16 src_x;
	s16 src_y;
	s16 src_w;
	s16 src_h;
	s16 dst_x;
	s16 dst_y;
	u16 dst_w;
	u16 dst_h;
	u32 format;
	struct pix_comp comp;
	struct tile_ctx ctx;
	u32 alpha;
	u32 blend_mode;
	u32 rotation;
	u32 zpos;
	u32 xfbc;
	u64 modifier;
	u32 width;
	u32 height;
	u32 header_size_r;
	u32 header_size_y;
	u32 header_size_uv;
	int pd_type;
	int pd_mode;
};

/* custom post config start*/
typedef struct drm_buffer_t {
    uint32_t size;
    uint64_t phys_addr;
    uint64_t base;
    uint64_t virt_addr;
    uint64_t dma_addr;
    uint64_t attachment;
    uint64_t sgt;
    int32_t	 buf_handle;
} drm_buffer_t;

struct post_layer {
	u8 crtc_id;
	u8 plane_id;
	u8 index; //plane index
	u8 enable;
	u8 nplanes;
	u32 addr_l[4];
	u32 addr_h[4];
	u32 pitch[4];
	s16 src_x;
	s16 src_y;
	s16 src_w;
	s16 src_h;
	s16 dst_x;
	s16 dst_y;
	u16 dst_w;
	u16 dst_h;
	u32 format;

	u32 alpha;
	u32 blend_mode;
	u32 rotation;
	u32 zpos;
	u32 xfbc;
	u64 modifier;
	u32 width;
	u32 height;

	int pd_type;
	int pd_mode;
};

struct sdrv_post_config {
	int num_buffer;
	drm_buffer_t buffers[4];
	struct post_layer layers[4];
};

/* custom post config end*/

/* Do not exceed 16 bytes so far */
struct dc_ioctl_cmd {
	u32 op;
	union {
		struct {
			u32 addr_l;
			u32 addr_h;
			u32 width;
			u32 height;
			int format;
		} s;
	} msg;
};

struct dpc_operation;
struct sdrv_pipe;
struct kunlun_crtc;
struct kunlun_plane;


struct sdrv_dpc {

	struct kunlun_crtc *crtc;
	struct sdrv_dpc *next;

	int inited;
	int is_master; //master or slave in combine mode
	int mlc_select;
	int dpc_type;/*0:dc, 1:dp, 2: dummy*/
	void __iomem *regs;
	uint32_t irq;
	spinlock_t irq_lock;

	const char *name;
	int combine_mode;

	const struct dpc_operation *ops;
	struct sdrv_pipe *pipes[16];
	int pipe_mask;
	int num_pipe;
	void *priv;
	struct pd_t pd_info;

	struct clk *pclk;
	int pclk_div;

	struct device dev;
	struct work_struct vsync_work;
	struct work_struct underrun_work;
	struct work_struct mlc_int_work;
	struct work_struct rdma_int_work;
};

struct dpc_operation {
	void (*init)(struct sdrv_dpc *dpc);
	void (*uninit)(struct sdrv_dpc *dpc);
	uint32_t (*irq_handler)(struct sdrv_dpc *dpc);
	void (*vblank_enable)(struct sdrv_dpc *dpc, bool enable);
	void (*enable)(struct sdrv_dpc *dpc, bool enable);
	int (*update)(struct sdrv_dpc *dpc, struct dpc_layer layers[], u8 count);
	void (*flush)(struct sdrv_dpc *dpc);
	int (*modeset)(struct sdrv_dpc *dpc, struct drm_display_mode *mode, u32 bus_flags);
	void (*sw_reset)(struct sdrv_dpc *dpc);
	void (*mlc_select) (struct sdrv_dpc *dpc, int mlc_select);
	void (*shutdown)(struct sdrv_dpc *dpc);
	int (*update_done)(struct sdrv_dpc *dpc);
	void (*dump)(struct sdrv_dpc *dpc);
};
extern struct sdrv_dpc *dpc_primary;

struct sdrv_dpc_data {
	const char *version;
	const struct dpc_operation* ops;
};

struct dp_dummy_data {
	int dummy_id;
	bool vsync_enabled;
	struct sdrv_dpc *dpc;
};
int register_dummy_data(struct dp_dummy_data *dummy);

struct fps_info {
	int hw_fps;
	int fps;
	int frame_count;
	u64 last_time;
};

struct kunlun_crtc {
	struct device *dev;
	struct drm_crtc base;
	struct drm_device *drm;
	struct sdrv_dpc *dpcs;
	int dpc_mask;
	struct sdrv_dpc *master;
	struct sdrv_dpc *slave;
	int combine_mode; // is dc combine mode?

	unsigned int num_planes;
	struct kunlun_plane *planes;

	int ctrl_unit;/*0:dc, 1:dp*/
	int mlc_select;/*0:dc third layer, 1: dc fouth layer*/
	unsigned int irq;
	struct fps_info fi;

	spinlock_t irq_lock;

	bool enabled;
	bool enable_status;
	bool vblank_enable;
	bool pending;
	int avm_dc_status;
	int du_inited;
	int posted;
	int id;
	struct mutex refresh_mutex;
	struct drm_pending_vblank_event *event;

	struct kthread_worker kworker;
	struct kthread_work recovery;
	struct task_struct *work_thread;
	bool evt_update;
	bool recover_done;
	wait_queue_head_t wait_queue;
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

int *get_zorders_from_pipe_mask(uint32_t mask);
int _add_pipe(struct sdrv_dpc *dpc, int type, int hwid, const char *pipe_name, uint32_t offset);
irqreturn_t sdrv_irq_handler(int irq, void *data);

int sdrv_set_frameinfo(struct sdrv_dpc *dpc, struct dpc_layer layers[], u8 count);
int sdrv_rpcall_start(bool enable);


void kunlun_crtc_handle_vblank(struct kunlun_crtc *kcrtc);
void dump_registers(struct sdrv_dpc *dpc, int start, int end);
int kunlun_crtc_sys_suspend(struct drm_crtc *crtc);
int kunlun_crtc_sys_resume(struct drm_crtc *crtc);
#endif //_sdrv_dpc_H_

