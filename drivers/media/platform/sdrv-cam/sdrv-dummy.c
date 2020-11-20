/*
 * sdrv-dummy.c
 *
 * Semidrive platform v4l2 core file
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/media-bus-format.h>
#include <linux/media.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/videodev2.h>

#include <media/media-device.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-rect.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-sg.h>
#include <media/videobuf2-dma-contig.h>

#include <linux/mailbox_client.h>

#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ipcc.h>


#define SDRV_IMG_OFFSET				0x100
#define SDRV_IMG_REG_LEN			0x80

#define CSI_CORE_ENABLE				0x00
#define CSI_RESET				BIT(8)

#define CSI_INT_0				0x04
#define CSI_INT_1				0x08
#define CSI_STORE_DONE_MASK			0x0F
#define CSI_STORE_DONE_SHIFT			0
#define CSI_SDW_SET_MASK			0xF0
#define CSI_SDW_SET_SHIFT			4
#define CSI_CROP_ERR_MASK			0x0F
#define CSI_CROP_ERR_SHIFT			0
#define CSI_PIXEL_ERR_MASK			0xF0
#define CSI_PIXEL_ERR_SHIFT			4
#define CSI_OVERFLOW_MASK			0xF00
#define CSI_OVERFLOW_SHIFT			8
#define CSI_BUS_ERR_MASK			0xFF000
#define CSI_BUS_ERR_SHIFT			12
#define CSI_BT_ERR_MASK				0x100
#define CSI_BT_FATAL_MASK			0x200
#define CSI_BT_COF_MASK				0x400

#define SDRV_IMG_REG_BASE(i)        \
    (SDRV_IMG_OFFSET + (i) * (SDRV_IMG_REG_LEN))

#define SDRV_DUMMY_NAME "sdrv-dummy"

enum scs_op_type {
	SCS_OP_DEV_OPEN,
	SCS_OP_ENUM_FORMAT,
	SCS_OP_ENUM_FRAMESIZE,
	SCS_OP_GET_FORMAT,
	SCS_OP_SET_FORMAT,
	SCS_OP_QUEUE_SETUP,
	SCS_OP_GET_BUFINFO,
	SCS_OP_SET_BUFINFO,
	SCS_OP_QBUF,
	SCS_OP_STREAM_ON,
	SCS_OP_DQBUF,
	SCS_OP_STREAM_OFF,
};

struct scs_ioctl_cmd {
	u16 op;
	union {
		struct {
			u16 type;
			u16 width;
			u16 height;
			u32 pixelformat;
		} s_fmt;
		struct {
			u16 index;
			u32 len;
			u32 addr;
		} s_bufinfo;
		u8 data[12];
	} msg;
};

struct scs_ioctl_result {
	u16 op;
	union {
		/** used for get_version */
		struct {
			u32 fmt;
			u16 cnt;
		} fmt;
		/** used for get_config */
		struct {
			u16 type;
			u16 width;
			u16 height;
			u16 cnt;
		} fsz;
		struct {
			u16 type;
			u16 width;
			u16 height;
			u32 pixelformat;
			u16 result;
		} g_fmt;
		struct {
			u16 index;
			u32 len;
			u32 addr;
		} g_bufinfo;
		struct {
			u16 bufs;
			u16 planes;
			u32 size;
		} queue;
		u8 data[12];
	} msg;
};

struct csi_dummy {
	struct device *dev;

	struct mutex lock;
	struct mutex q_lock;
	spinlock_t buf_lock;

	struct mbox_client client;
	struct mbox_chan *mbox;

	struct v4l2_device v4l2_dev;
	struct media_device media_dev;
	struct v4l2_async_notifier notifier;
	struct v4l2_fwnode_endpoint vep;

	struct v4l2_format active_fmt;

	struct video_device vdev;
	struct vb2_queue queue;
	struct list_head buf_list;

	unsigned int mbus_type;

	struct list_head kstreams;
	unsigned int kstream_nums;

	struct vb2_v4l2_buffer *vbuf_ready;	/* reg set */
	struct vb2_v4l2_buffer *vbuf_active;	/* data writing */
};

struct sdrv_dummy_buffer {
	struct vb2_v4l2_buffer vbuf;
	dma_addr_t paddr[3];
	struct list_head queue;
};

static int sdrv_dummy_ioctl(struct csi_dummy *csi, struct scs_ioctl_cmd *c,
			    struct scs_ioctl_result *data)
{
	struct rpc_ret_msg result = { };
	struct rpc_req_msg request;
	struct scs_ioctl_cmd *ctl =
	    DCF_RPC_PARAM(request, struct scs_ioctl_cmd);
	struct scs_ioctl_result *rs =
	    (struct scs_ioctl_result *)&result.result[0];
	int ret = 0;

	DCF_INIT_RPC_REQ(request, MOD_RPC_REQ_SCS_IOCTL);
	//ctl->op = command;
	memcpy(ctl, c, sizeof(*c));

	dev_info(csi->dev, "%s():\n", __func__);
	ret = semidrive_rpcall(&request, &result);
	if (ret < 0 || result.retcode < 0) {
		dev_err(csi->dev, "rpcall failed=%d %d\n", ret, result.retcode);
		goto fail_call;
	}

	memcpy(data, rs, sizeof(*rs));

fail_call:

	return ret;
}

static int sdrv_dummy_queue_setup(struct vb2_queue *q,
				  unsigned int *num_buffers,
				  unsigned int *num_planes,
				  unsigned int sizes[],
				  struct device *alloc_devs[])
{
	int ret;
	struct csi_dummy *csi = vb2_get_drv_priv(q);
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;

	struct v4l2_pix_format_mplane *format = &csi->active_fmt.fmt.pix_mp;

	op_ctl.op = SCS_OP_QUEUE_SETUP;

	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "queue setup failed: %d\n", ret);
		return ret;
	}
	*num_buffers = op_ret.msg.queue.bufs;
	*num_planes = format->num_planes = op_ret.msg.queue.planes;
	sizes[0] = format->plane_fmt[0].sizeimage = op_ret.msg.queue.size;

	dev_info(csi->dev, "*num_buffers=%d, *num_planes=%d, sizes[0]=%d\n",
		*num_buffers, *num_planes, sizes[0]);

	dev_info(csi->dev, "queue setup ok: %d\n", ret);

	return 0;
}

static int sdrv_dummy_buf_init(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct sdrv_dummy_buffer *kbuffer =
	    container_of(vbuf, struct sdrv_dummy_buffer, vbuf);

	unsigned int i;

	struct csi_dummy *csi = vb2_get_drv_priv(vb->vb2_queue);
	int ret;
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;
	const struct v4l2_pix_format_mplane *format =
	    &csi->active_fmt.fmt.pix_mp;

	for (i = 0; i < format->num_planes; i++) {
		kbuffer->paddr[i] = vb2_dma_contig_plane_dma_addr(vb, i);
		dev_info(csi->dev, "%s(): kbuffer->paddr[%d]=0x%x\n", __func__, i,
		       kbuffer->paddr[i]);
		if (!kbuffer->paddr[i])
			return -EFAULT;
	}

	op_ctl.op = SCS_OP_SET_BUFINFO;
	dev_info(csi->dev, "vb->index = %d\n", vb->index);
	op_ctl.msg.s_bufinfo.index = vb->index;
	op_ctl.msg.s_bufinfo.len = 1280 * 720 * 2;
	op_ctl.msg.s_bufinfo.addr = kbuffer->paddr[0];

	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "get_format failed: %d\n", ret);
		return ret;
	}
	dev_info(csi->dev, "set_bufinfo ok: %d\n", ret);

	return 0;

	return 0;
}

static int sdrv_dummy_buf_prepare(struct vb2_buffer *vb)
{
	return 0;
}

static void sdrv_dummy_buf_queue(struct vb2_buffer *vb)
{
	unsigned long flags;
	int ret;
	struct csi_dummy *csi = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct sdrv_dummy_buffer *kbuffer = container_of(vbuf,
							 struct
							 sdrv_dummy_buffer,
							 vbuf);

	dev_info(csi->dev, "%s(): csi->buf_list add vb->index=%d\n", __func__,
		vb->index);

	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;
	op_ctl.op = SCS_OP_QBUF;
	op_ctl.msg.s_bufinfo.index = vb->index;

	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "qbuf failed: %d\n", ret);
		return;
	}
	dev_info(csi->dev, "qbuf ok: %d, op_ret.msg.g_bufinfo.index=%d\n", ret,
		op_ret.msg.g_bufinfo.index);

	spin_lock_irqsave(&csi->buf_lock, flags);
	list_add_tail(&kbuffer->queue, &csi->buf_list);
	spin_unlock_irqrestore(&csi->buf_lock, flags);

}

static int sdrv_dummy_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct csi_dummy *csi = vb2_get_drv_priv(vq);
	int ret;
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;
	op_ctl.op = SCS_OP_STREAM_ON;

	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "stream on failed: %d\n", ret);
		return ret;
	}
	dev_info(csi->dev, "stream on ok: %d\n", ret);

}

static void sdrv_dummy_stop_streaming(struct vb2_queue *vq)
{
	struct csi_dummy *csi = vb2_get_drv_priv(vq);
	int ret;
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;
	struct sdrv_dummy_buffer *kbuf, *node;

	op_ctl.op = SCS_OP_STREAM_OFF;

	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "stream off failed: %d\n", ret);
		return;
	}
	dev_info(csi->dev, "stream off ok: %d\n", ret);

	list_for_each_entry_safe(kbuf, node, &csi->buf_list, queue) {
		dev_info(csi->dev, "list: kbuf->vbuf.vb2_buf.index=%d\n",
			kbuf->vbuf.vb2_buf.index);
		vb2_buffer_done(&kbuf->vbuf.vb2_buf, VB2_BUF_STATE_ERROR);
		list_del(&kbuf->queue);
	}

}

static const struct vb2_ops sdrv_dummy_video_vb2_q_ops = {
	.queue_setup = sdrv_dummy_queue_setup,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_init = sdrv_dummy_buf_init,
	.buf_prepare = sdrv_dummy_buf_prepare,
	.buf_queue = sdrv_dummy_buf_queue,
	.start_streaming = sdrv_dummy_start_streaming,
	.stop_streaming = sdrv_dummy_stop_streaming,
};

static int sdrv_dummy_video_open(struct file *file)
{
	struct csi_dummy *csi = video_drvdata(file);
	dev_info(csi->dev, "%s()\n", __func__);
	struct v4l2_fh *vfh;
	int ret;

	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;

	mutex_lock(&csi->lock);

	vfh = kzalloc(sizeof(*vfh), GFP_KERNEL);
	if (!vfh) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	v4l2_fh_init(vfh, &csi->vdev);
	v4l2_fh_add(vfh);

	file->private_data = vfh;

	mutex_unlock(&csi->lock);

	op_ctl.op = SCS_OP_DEV_OPEN;

	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "dev_open failed: %d\n", ret);
		goto err;
	}
	dev_info(csi->dev, "dev_open ok: %d\n", ret);

	return 0;

err:
	v4l2_fh_release(file);
err_alloc:
	mutex_unlock(&csi->lock);

	return ret;
}

static int sdrv_dummy_video_release(struct file *file)
{
	struct csi_dummy *csi = video_drvdata(file);

	vb2_fop_release(file);

	file->private_data = NULL;

	return 0;
}

static const struct v4l2_file_operations sdrv_dummy_vid_ops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = video_ioctl2,
	.open = sdrv_dummy_video_open,
	.release = sdrv_dummy_video_release,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
	.read = vb2_fop_read,
};

static int sdrv_dummy_video_querycap(struct file *file, void *fh,
				     struct v4l2_capability *cap)
{
	struct csi_dummy *csi = video_drvdata(file);

	strlcpy(cap->driver, SDRV_DUMMY_NAME, sizeof(cap->driver));
	strlcpy(cap->card, SDRV_DUMMY_NAME, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
		 dev_name(csi->dev));

	return 0;
}

struct sdrv_dummy_pix_format {
	u32 pixfmt;
	u32 mbus_code;
};

struct sdrv_dummy_pix_format pixfmts[] = {
	{
	 .pixfmt = V4L2_PIX_FMT_YUYV,	/* YUYV */
	 .mbus_code = MEDIA_BUS_FMT_YUYV8_2X8,
	 },
	{
	 .pixfmt = V4L2_PIX_FMT_UYVY,	/* UYVY */
	 .mbus_code = MEDIA_BUS_FMT_UYVY8_2X8,
	 },
};

static int sdrv_dummy_video_enum_fmt(struct file *file, void *priv,
				     struct v4l2_fmtdesc *f)
{
	struct csi_dummy *csi = video_drvdata(file);

	int ret;
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;
	op_ctl.op = SCS_OP_ENUM_FORMAT;

	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "enum_format failed: %d\n", ret);
		return ret;
	}
	dev_info(csi->dev, "enum_format ok: %d, op_ret.msg.fmt.fmt=0x%x\n", ret,
		op_ret.msg.fmt.fmt);

	if (f->index < op_ret.msg.fmt.cnt)
		f->pixelformat = op_ret.msg.fmt.fmt;
	else
		return -EINVAL;

	return 0;
}

static int sdrv_dummy_video_g_fmt(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	struct csi_dummy *csi = video_drvdata(file);

	int ret;
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;

	struct v4l2_format *af = &csi->active_fmt;
/*	u32 pad;
	int ret = 0;
	struct v4l2_subdev_format format = {
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};*/

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	if (af->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
	    af->fmt.pix_mp.width &&
	    af->fmt.pix_mp.height && af->fmt.pix_mp.num_planes) {
		*f = *af;
		return 0;
	}

	op_ctl.op = SCS_OP_GET_FORMAT;

	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "get_format failed: %d\n", ret);
		return ret;
	}
	dev_info(csi->dev,
		"get_format ok: %d, op_ret.msg.g_fmt.type=%d, op_ret.msg.g_fmt.pixelformat=0x%x\n",
		ret, op_ret.msg.g_fmt.type, op_ret.msg.g_fmt.pixelformat);

	af->type = op_ret.msg.g_fmt.type;
	if (af->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		af->fmt.pix_mp.width = op_ret.msg.g_fmt.width;
		af->fmt.pix_mp.height = op_ret.msg.g_fmt.height;
		af->fmt.pix_mp.pixelformat = op_ret.msg.g_fmt.pixelformat;
	}

	*f = *af;

	return 0;
}

static int sdrv_dummy_video_s_fmt(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	struct csi_dummy *csi = video_drvdata(file);

	int ret;
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;
	op_ctl.op = SCS_OP_SET_FORMAT;

	op_ctl.msg.s_fmt.type = f->type;
	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		op_ctl.msg.s_fmt.width = f->fmt.pix_mp.width;
		op_ctl.msg.s_fmt.height = f->fmt.pix_mp.height;
		op_ctl.msg.s_fmt.pixelformat = f->fmt.pix_mp.pixelformat;
	}
	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "set_format failed: %d\n", ret);
		return ret;
	}
	if (op_ret.msg.g_fmt.pixelformat != f->fmt.pix_mp.pixelformat) {
		dev_err(csi->dev, "set_format: 0x%x failed!\n",
			f->fmt.pix_mp.pixelformat);
		return -1;
	}
	dev_info(csi->dev, "set_format ok: %d, op_ret.msg.gs_fmt.width=%d\n",
		ret, op_ret.msg.g_fmt.width);

	csi->active_fmt = *f;

	return 0;
}

static int sdrv_dummy_video_try_fmt(struct file *file,
				    void *fh, struct v4l2_format *f)
{
	struct csi_dummy *csi = video_drvdata(file);
	int ret;
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;
	op_ctl.op = SCS_OP_SET_FORMAT;

	op_ctl.msg.s_fmt.type = f->type;
	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		op_ctl.msg.s_fmt.width = f->fmt.pix_mp.width;
		op_ctl.msg.s_fmt.height = f->fmt.pix_mp.height;
		op_ctl.msg.s_fmt.pixelformat = f->fmt.pix_mp.pixelformat;
	}
	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "set_format failed: %d\n", ret);
		return ret;
	}
	if (op_ret.msg.g_fmt.pixelformat != f->fmt.pix_mp.pixelformat) {
		dev_err(csi->dev, "set_format: 0x%x failed!\n",
			f->fmt.pix_mp.pixelformat);
		return -1;
	}
	dev_info(csi->dev, "set_format ok: %d, op_ret.msg.gs_fmt.width=%d\n",
		ret, op_ret.msg.g_fmt.width);

	csi->active_fmt = *f;

	return 0;
}

static int sdrv_dummy_g_selection(struct file *file, void *fh,
				  struct v4l2_selection *s)
{
	return 0;
}

static int sdrv_dummy_s_selection(struct file *file, void *fh,
				  struct v4l2_selection *s)
{
	return 0;
}

static int sdrv_dummy_cropcap(struct file *file, void *fh,
			      struct v4l2_cropcap *crop)
{
	return 0;
}

static int sdrv_dummy_enum_input(struct file *file, void *fh,
				 struct v4l2_input *i)
{
	return 0;
}

static int sdrv_dummy_g_input(struct file *file, void *fh, unsigned int *i)
{
	return 0;
}

static int sdrv_dummy_s_input(struct file *file, void *fh, unsigned int i)
{
	return 0;
}

static int sdrv_dummy_querystd(struct file *file, void *fh, v4l2_std_id * a)
{
	return 0;
}

static int sdrv_dummy_g_std(struct file *file, void *fh, v4l2_std_id * a)
{
	return 0;
}

static int sdrv_dummy_s_std(struct file *file, void *fh, v4l2_std_id a)
{
	return 0;
}

static int sdrv_dummy_g_parm(struct file *file, void *fh,
			     struct v4l2_streamparm *a)
{
	return 0;
}

static int sdrv_dummy_s_parm(struct file *file, void *fh,
			     struct v4l2_streamparm *a)
{
	return 0;
}

static int sdrv_dummy_enum_framesizes(struct file *file, void *fh,
				      struct v4l2_frmsizeenum *fsize)
{
	struct csi_dummy *csi = video_drvdata(file);

	int ret;
	struct scs_ioctl_cmd op_ctl;
	struct scs_ioctl_result op_ret;
	op_ctl.op = SCS_OP_ENUM_FRAMESIZE;

	ret = sdrv_dummy_ioctl(csi, &op_ctl, &op_ret);
	if (ret < 0) {
		dev_err(csi->dev, "enum_framesize failed: %d\n", ret);
		return ret;
	}
	dev_info(csi->dev, "enum_framesize ok: %d, op_ret.msg.fsz (%d, %d-%d)\n",
		ret, op_ret.msg.fsz.type, op_ret.msg.fsz.width,
		op_ret.msg.fsz.height);

	if (fsize->index < op_ret.msg.fsz.type) {
		fsize->type = op_ret.msg.fsz.type;
		if (fsize->type == V4L2_FRMSIZE_TYPE_DISCRETE) {
			fsize->discrete.width = op_ret.msg.fsz.width;
			fsize->discrete.height = op_ret.msg.fsz.height;
		}
	} else {
		return -EINVAL;
	}

	return 0;
}

static int sdrv_dummy_enum_frameintervals(struct file *file, void *fh,
					  struct v4l2_frmivalenum *fival)
{
	return 0;
}

static long sdrv_dummy_vidioc_default(struct file *file, void *fh,
				      bool valid_prio, unsigned int cmd,
				      void *arg)
{
	return 0;
}

static const struct v4l2_ioctl_ops sdrv_dummy_vid_ioctl_ops = {
	.vidioc_querycap = sdrv_dummy_video_querycap,
	.vidioc_enum_fmt_vid_cap_mplane = sdrv_dummy_video_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = sdrv_dummy_video_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane = sdrv_dummy_video_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane = sdrv_dummy_video_try_fmt,

	.vidioc_g_selection = sdrv_dummy_g_selection,
	.vidioc_s_selection = sdrv_dummy_s_selection,

	.vidioc_cropcap = sdrv_dummy_cropcap,

	.vidioc_enum_input = sdrv_dummy_enum_input,
	.vidioc_g_input = sdrv_dummy_g_input,
	.vidioc_s_input = sdrv_dummy_s_input,

	.vidioc_querystd = sdrv_dummy_querystd,
	.vidioc_g_std = sdrv_dummy_g_std,
	.vidioc_s_std = sdrv_dummy_s_std,

	.vidioc_g_parm = sdrv_dummy_g_parm,
	.vidioc_s_parm = sdrv_dummy_s_parm,

	.vidioc_enum_framesizes = sdrv_dummy_enum_framesizes,
	.vidioc_enum_frameintervals = sdrv_dummy_enum_frameintervals,

	.vidioc_default = sdrv_dummy_vidioc_default,

	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
};

static void sdrv_dummy_release(struct video_device *vdev)
{
	struct csi_dummy *csi = video_get_drvdata(vdev);

	mutex_destroy(&csi->q_lock);
	mutex_destroy(&csi->lock);
}

int sdrv_dummy_video_register(	/*struct kstream_video *video,
				   struct v4l2_device *v4l2_dev */ struct
				     video_device *vdev)
{
	struct csi_dummy *csi = container_of(vdev,
					     struct csi_dummy, vdev);
	int ret;

	struct vb2_queue *q;
	q = &csi->queue;

	mutex_init(&csi->lock);
	mutex_init(&csi->q_lock);

	INIT_LIST_HEAD(&csi->buf_list);
	spin_lock_init(&csi->buf_lock);

	q->drv_priv = csi;
	q->mem_ops = &vb2_dma_contig_memops;
	q->ops = &sdrv_dummy_video_vb2_q_ops;
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->io_modes = VB2_DMABUF | VB2_MMAP | VB2_READ | VB2_USERPTR;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->buf_struct_size = sizeof(struct sdrv_dummy_buffer);
	q->dev = csi->dev;
	q->lock = &csi->q_lock;
	ret = vb2_queue_init(q);
	if (ret < 0) {
		dev_err(csi->dev, "Failed to init vb2 queue: %d\n", ret);
		goto error_vb2_init;
	}

	vdev->fops = &sdrv_dummy_vid_ops;
	vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING |
	    V4L2_CAP_READWRITE;
	vdev->ioctl_ops = &sdrv_dummy_vid_ioctl_ops;
	vdev->release = sdrv_dummy_release;
	vdev->v4l2_dev = &csi->v4l2_dev;
	vdev->vfl_dir = VFL_DIR_RX;
	vdev->queue = q;
	snprintf(vdev->name, ARRAY_SIZE(vdev->name), "sdrv-kstream-0");
	ret = video_register_device(vdev, VFL_TYPE_GRABBER, -1);
	if (ret < 0) {
		dev_err(csi->dev, "Failed to register video device: %d\n", ret);
		goto error_video_register;
	}

	video_set_drvdata(vdev, csi);

	return ret;

error_video_register:
	mutex_destroy(&csi->lock);
	mutex_destroy(&csi->q_lock);
	vb2_queue_release(vdev->queue);
error_vb2_init:

	return ret;
}

static void sdrv_dummy_notify(struct mbox_client *client, void *mssg)
{
	struct vb2_v4l2_buffer *vbuf;
	struct csi_dummy *csi = container_of(client,
					     struct csi_dummy, client);
	dev_info(csi->dev, "%s\n", __func__);

	sd_msghdr_t *msghdr = mssg;

	if (!msghdr) {
		dev_err(csi->dev, "%s NULL mssg\n", __func__);
		return;
	}
	dev_info(csi->dev, "dat_len=%d, data[0]=%d, data[1]=%d\n",
		msghdr->dat_len, msghdr->data[0], msghdr->data[1]);

/*	if (csi->vbuf_active == NULL) {
		dev_err(csi->dev, "%s vbuf_active is null\n", __func__);
		//return;
	}*/

	unsigned long flags;

	spin_lock_irqsave(&csi->buf_lock, flags);

	struct sdrv_dummy_buffer *kbuf;

	list_for_each_entry(kbuf, &csi->buf_list, queue) {
		dev_info(csi->dev, "kbuf->vbuf.vb2_buf.index=%d\n",
			kbuf->vbuf.vb2_buf.index);
		if (kbuf->vbuf.vb2_buf.index == msghdr->data[1]) {
			vbuf = &kbuf->vbuf;
			vbuf->vb2_buf.timestamp = ktime_get_ns();
			vb2_buffer_done(&vbuf->vb2_buf, VB2_BUF_STATE_DONE);

			list_del(&kbuf->queue);
			break;
		}
	}

	spin_unlock_irqrestore(&csi->buf_lock, flags);
	dev_info(csi->dev, "%s\n", __func__);

	return;
}

static int sdrv_dummy_request_irq(struct csi_dummy *csi)
{
	struct mbox_client *client;
	int ret = 0;

	client = &csi->client;
	/* Initialize the mbox channel used by touchscreen */
	client->dev = csi->dev;
	client->tx_done = NULL;
	client->rx_callback = sdrv_dummy_notify;
	client->tx_block = true;
	client->tx_tout = 1000;
	client->knows_txdone = false;
	csi->mbox = mbox_request_channel(client, 0);
	if (IS_ERR(csi->mbox)) {
		ret = -EBUSY;
		dev_err(csi->dev, "mbox_request_channel failed: %ld\n",
			PTR_ERR(csi->mbox));
	}

	return ret;
}

static int sdrv_dummy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct csi_dummy *csi;
	int ret;
	dev_info(dev, "%s()-110: .\n", __func__);

	csi = devm_kzalloc(dev, sizeof(*csi), GFP_KERNEL);
	if (!csi)
		return -ENOMEM;

	csi->dev = dev;
	platform_set_drvdata(pdev, csi);

	ret = v4l2_device_register(csi->dev, &csi->v4l2_dev);
	if (ret < 0)
		goto err_register_v4l2;

	ret = sdrv_dummy_video_register(&csi->vdev);
	if (ret < 0)
		goto err_register_video;

	ret = sdrv_dummy_request_irq(csi);
	if (ret < 0)
		goto err_register_video;

	dev_info(dev, "%s(): exit.\n", __func__);
	return 0;

err_register_video:
	video_unregister_device(&csi->vdev);
err_register_v4l2:
	v4l2_device_unregister(&csi->v4l2_dev);

	return ret;
}

static int sdrv_dummy_remove(struct platform_device *pdev)
{
	struct csi_dummy *csi;

	csi = platform_get_drvdata(pdev);

	video_unregister_device(&csi->vdev);

	v4l2_device_unregister(&csi->v4l2_dev);

	return 0;
}

static const struct of_device_id sdrv_dummy_dt_match[] = {
	{.compatible = "sd,sdrv_dummy"},
	{},
};

static struct platform_driver sdrv_dummy_driver = {
	.probe = sdrv_dummy_probe,
	.remove = sdrv_dummy_remove,
	.driver = {
		   .name = "sdrv-dummy",
		   .of_match_table = sdrv_dummy_dt_match,
		   },
};

module_platform_driver(sdrv_dummy_driver);

MODULE_ALIAS("platform:sdrv-dummy");
MODULE_DESCRIPTION("Semidrive Camera driver");
MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_LICENSE("GPL v2");
