/*
 * kunlun-csi.h
 *
 * Semidrive kunlun platform csi header file
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef SD_KUNLUN_CSI_H
#define SD_KUNLUN_CSI_H

#include <linux/types.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/media-entity.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <linux/device.h>

#define KUNLUN_IMG_NUM	4

#define KUNLUN_IMG_PAD_SINK		0
#define KUNLUN_IMG_PAD_SRC		1
#define KUNLUN_IMG_PAD_NUM		2

#define KUNLUN_IMG_NAME "kunlun-kstream"

#define KUNLUN_IMG_X_MAX		4096
#define KUNLUN_IMG_Y_MAX		4096

#define IMG_MAX_NUM_PLANES		3
#define IMG_HW_NUM_PLANES		(IMG_MAX_NUM_PLANES - 1)

#define KUNLUN_MBUS_DC2CSI1		0x101
#define KUNLUN_MBUS_DC2CSI2		0x102

struct csi_core;
struct kstream_device;

struct kstream_buffer {
	struct vb2_v4l2_buffer vbuf;
	dma_addr_t paddr[3];
	struct list_head queue;
};

struct kstream_mbus_format {
	u32 code;
	u8 pix_odd;
	u8 pix_even;
	bool fc_dual;
};

struct kstream_pix_format {
	u32 pixfmt;
	u32 mbus_code;
	u8 bpp[3];
	u8 pack_uv_odd;
	u8 pack_uv_even;
	u8 pack_pix_odd;
	u8 pack_pix_even;
	u32 split[2];
	u32 pack[2];
};


enum kstream_state {
	STOPPED = 0,
	INITING,
	INITIALED,
	RUNNING,
	IDLE,
	STOPPING,
};

struct kstream_sensor {
	struct v4l2_async_subdev asd;
	struct v4l2_subdev *subdev;
	unsigned int source_pad;
};

struct kstream_interface {
	unsigned int mbus_type;
	struct v4l2_async_subdev asd;
	struct v4l2_subdev *subdev;
	unsigned int sink_pad;
	unsigned int source_pad;
};

struct kstream_video {
	struct device *dev;
	struct csi_core *core;
	struct vb2_queue queue;
	struct video_device vdev;
	struct media_pad pad;
	struct v4l2_format active_fmt;
	struct list_head buf_list;
	unsigned int sequence;
	u8 id;	/* stream id */
	struct mutex lock;
	struct mutex q_lock;
	spinlock_t buf_lock;

	struct vb2_v4l2_buffer *vbuf_ready; /* reg set */
	struct vb2_v4l2_buffer *vbuf_active;  /* data writing */
};

struct kstream_ops {
	void (*init_interface)(struct kstream_device *kstream);
	void (*frm_done_isr)(struct kstream_device *kstream);
	void (*img_update_isr)(struct kstream_device *kstream);
};

struct kstream_device {
	u8 id;
	struct device *dev;
	void __iomem *base;
	struct list_head csi_entry;
	struct csi_core *core;
	struct v4l2_subdev subdev;
	struct media_pad pads[KUNLUN_IMG_PAD_NUM];

	struct v4l2_mbus_framefmt mbus_fmt[KUNLUN_IMG_PAD_NUM];
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_rect crop;

	struct kstream_video video;
	struct kstream_interface interface;
	struct kstream_sensor sensor;
	enum kstream_state state;
	bool enabled;
	bool iommu_enable;

	struct kstream_ops *ops;
};

struct csi_int_stat {
	u64 frm_cnt;
	u32 bt_err;
	u32 bt_fatal;
	u32 bt_cof;
	u32 crop_err;
	u32 pixel_err;
	u32 overflow;
	u32 bus_err;
};

struct csi_core {
	struct device *dev;
	void __iomem *base;
	u32 irq;
	u32 host_id;	/* host ip id */
	struct mutex lock;

	struct v4l2_device v4l2_dev;
	struct media_device media_dev;
	struct v4l2_async_notifier notifier;
	struct v4l2_fwnode_endpoint vep;

	unsigned int  mbus_type;
	unsigned int mipi_stream_num;

	struct list_head kstreams;
	unsigned int kstream_nums;

	struct csi_int_stat ints;
	atomic_t ref_count;
};

int kunlun_stream_register_entities(struct kstream_device *kstream,
		struct v4l2_device *v4l2_dev);
int kunlun_stream_unregister_entities(struct kstream_device *kstream);

int kunlun_stream_video_register(struct kstream_video *video,
		struct v4l2_device *v4l2_dev);

void kunlun_stream_video_unregister(struct kstream_video *video);

const struct kstream_pix_format *
kstream_get_kpfmt_by_mbus_code(unsigned int mbus_code);

const struct kstream_pix_format *
kstream_get_kpfmt_by_index(unsigned int index);

const struct kstream_pix_format *
kstream_get_kpfmt_by_fmt(unsigned int pixelformat);

static inline u32 kcsi_readl(void __iomem *base, u32 addr)
{
	return readl_relaxed(base + addr);
}

static inline void kcsi_writel(void __iomem *base, u32 addr, u32 val)
{
	writel_relaxed(val, base + addr);
}

static inline void kcsi_clr(void __iomem *base, u32 addr, u32 clr)
{
	kcsi_writel(base, addr, kcsi_readl(base, addr) & ~clr);
}

static inline void kcsi_set(void __iomem *base, u32 addr, u32 set)
{
	kcsi_writel(base, addr, kcsi_readl(base, addr) | set);
}

static inline void kcsi_clr_and_set(void __iomem *base, u32 addr,
		u32 clr, u32 set)
{
	u32 reg;

	reg = kcsi_readl(base, addr);
	reg &= ~clr;
	reg |= set;
	kcsi_writel(base, addr, reg);
}

static inline int kunlun_find_pad(struct v4l2_subdev *sd, int direction)
{
	unsigned int pad;

	for(pad = 0; pad < sd->entity.num_pads; pad++)
		if(sd->entity.pads[pad].flags & direction)
			return pad;
	return -EINVAL;
}

#endif
