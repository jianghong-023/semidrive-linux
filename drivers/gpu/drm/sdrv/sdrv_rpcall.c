/*
 * Copyright (C) 2020 Semidriver Semiconductor Inc.
 * License terms:  GNU General Public License (GPL), version 2
 */

#include <linux/module.h>
#include <linux/rpmsg.h>
#include <xen/xen.h>
#include <xen/xenbus.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ipcc.h>
#include <linux/soc/semidrive/xenipc.h>
#include "sdrv_dpc.h"
#include "sdrv_plane.h"
#define SYS_PROP_DC_STATUS			(4)

// part of display module
struct sdrv_disp_control {
	struct drm_crtc *crtc;

};

struct disp_frame_info {
	u32 addr_l;
	u32 addr_h;
	u32 format;
	u16 width;
	u16 height;
	u16 pos_x;
	u16 pos_y;
	u16 pitch;
	u16 mask_id;
}__attribute__ ((packed));

/* Do not exceed 32 bytes so far */
struct disp_ioctl_cmd {
	u32 op;
	union {
		u8 data[28];
		struct disp_frame_info fi;
	} u;
};

struct disp_ioctl_ret {
	u32 op;
	union {
		u8 data[8];
	} u;
};

#define DISP_CMD_START		_IOWR('d', 0, struct disp_frame_info)
#define DISP_CMD_SET_FRAMEINFO		_IOWR('d', 1, struct disp_frame_info)
#define DISP_CMD_SHARING_WITH_MASK		_IOWR('d', 2, struct disp_frame_info)
#define DISP_CMD_CLEAR		_IOWR('d', 3, struct disp_frame_info)



#define FILL_IOC_DATA(ctl, arg, size) \
	memcpy(&ctl->u.data[0], arg, size)

static int sdrv_disp_ioctl(struct sdrv_disp_control *disp, u32 command,
								void *arg)
{
	struct rpc_ret_msg result = {0,};
	struct rpc_req_msg request;
	struct disp_ioctl_cmd *ctl = DCF_RPC_PARAM(request, struct disp_ioctl_cmd);
	unsigned int size = _IOC_SIZE(command);
	int ret = 0;

	DCF_INIT_RPC_REQ(request, MOD_RPC_REQ_DC_IOCTL);
	ctl->op = command;
	switch(command) {
	case DISP_CMD_SET_FRAMEINFO:
		if (size != sizeof(struct disp_frame_info)) {
			ret = -EINVAL;
			goto fail_call;
		}

		FILL_IOC_DATA(ctl, arg, size);

	    break;
	case DISP_CMD_SHARING_WITH_MASK:
		if (size != sizeof(struct disp_frame_info)) {
			ret = -EINVAL;
			goto fail_call;
		}

		FILL_IOC_DATA(ctl, arg, size);
		break;

	case DISP_CMD_START:
	case DISP_CMD_CLEAR:
		PRINT("got cmd 0x%x ioctl\n", command);
		break;
	default:
		break;
	}


	ret = semidrive_rpcall(&request, &result);
	if (ret < 0 || result.retcode < 0) {
		goto fail_call;
	}

	return 0;

fail_call:

	return ret;
}

int sdrv_rpcall_start(bool enable) {
	int ret;
	struct sdrv_disp_control disp;
	int cmd = enable? DISP_CMD_START: DISP_CMD_CLEAR;

	ret = sdrv_disp_ioctl(&disp, cmd, (void*)NULL);
	if (ret < 0) {
		PRINT("ioctl failed=%d\n", ret);
	}

	return ret;
}

int sdrv_set_frameinfo(struct sdrv_dpc *dpc, struct dpc_layer layers[], u8 count)
{
	struct sdrv_disp_control disp;
	struct disp_frame_info fi;
	struct dpc_layer *layer = NULL;
	bool is_fbdc_cps = false;
	int i;
	int ret;

	if (count != 1){
		DRM_ERROR("rpcall display only support single layer, input %d layers", count);
		return -EINVAL;
	}

	for (i = 0; i < dpc->num_pipe; i++) {
		layer = &dpc->pipes[i]->layer_data;

		if (!layer->enable)
			continue;

		if (layer->ctx.is_fbdc_cps)
			is_fbdc_cps = true;
	}

	if (is_fbdc_cps) {
		DRM_ERROR("fbdc layer cannot support rpcall display");
		return -EINVAL;
	}

	if (!layer) {
		DRM_ERROR("layer is null pointer");
		return -1;
	}

	fi.width = layer->src_w;
	fi.height = layer->src_h;
	fi.pitch = layer->pitch[0];
	fi.addr_h = layer->addr_h[0];
	fi.addr_l = layer->addr_l[0];
	fi.pos_x = layer->dst_x;
	fi.pos_y = layer->dst_y;
	fi.format = layer->format;
	fi.mask_id = 0;

	DRM_DEBUG("format[%c%c%c%c]\n",
		layer->format & 0xff, (layer->format >> 8) & 0xff, (layer->format >> 16) & 0xff,(layer->format >> 24) & 0xff);
	DRM_DEBUG("w:%d h:%d pitch:%d addrh:0x%x addrl:0x%x posx:%d posy:%d format:%x mk:%d\n",
		fi.width, fi.height, fi.pitch, fi.addr_h, fi.addr_l, fi.pos_x, fi.pos_y, fi.format, fi.mask_id);

	if (fi.width <= 0 || fi.height <= 0 || fi.format != DRM_FORMAT_ABGR8888) {
		return 0;
	}

	ret = sdrv_disp_ioctl(&disp, DISP_CMD_SHARING_WITH_MASK, (void*)&fi);
	if (ret < 0) {
		PRINT("ioctl failed=%d\n", ret);
	}

	return ret;
}

