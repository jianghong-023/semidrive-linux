#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_of.h>
#include <linux/kernel.h>
#include <linux/component.h>
#include <linux/dma-buf.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/soc/semidrive/ipcc.h>

#include "kunlun_drm_drv.h"
#include "sdrv_plane.h"
#include "sdrv_dpc.h"

extern const struct dpc_operation dc_r0p1_ops;
extern const struct dpc_operation dp_r0p1_ops;
extern const struct dpc_operation dp_dummy_ops;
extern const struct dpc_operation dp_rpcall_ops;
extern const struct dpc_operation dp_op_ops;

struct class *display_class;
struct kunlun_crtc *kcrtc_primary = NULL;

#define CONFIG_RECOVERY_ENABLED
#if 0
		display: display@0 {
			compatible = "semdrive,display-subsystem";
			sdriv,crtc = <&crtc0 &crtc1>;
			status = "disabled";
		};

		dp1: dp1@30440000 {
			compatible = "sdrv,dp";
			reg = <>;
			interrupts = <0 DP1_IRQ 4>;
			status = "disabled";
			mlc-select = <0>;
			sdrv,ip = "dp-r0p1";
		};

		dc1: dc1@30db0000 {
			compatible = "sdrv,dc";
			reg = <>;
			interrupts = <0 DC1_IRQ 4>;
			status = "disabled";
			sdrv,ip = "dc-r0p1";
			crtc0_out: port {
				crtc0_out_interface: endpoint@0 {
					remote-endpoint = <&parallel_in_crtc0>;
				};
			};
		};

		dp2: dp2@30460000 {
			compatible = "sdrv,dp";
			reg = <>;
			interrupts = <0 DP2_IRQ 4>;
			status = "disabled";
			sdrv,ip = "dp-r0p1";
		};

		dc2: dc2@30dc0000 {
			compatible = "sdrv,dc";
			reg = <>;
			interrupts = <0 DC2_IRQ 4>;
			status = "disabled";
			sdrv,ip = "dc-r0p1";


		};

		crtc0: crtc0@30440000 {
			compatible = "semidrive,sdrv-crtc";
			dpc-master = <&dp1 &dc1>;
			dpc-slave = <&dp2 &dc2>;
			display-mode = <3>; //1 single mode, 2: duplicate mode, 3: combine mode

			crtc0_out: port {
				crtc0_out_interface: endpoint@0 {
					remote-endpoint = <&parallel_in_crtc0>;
				};
			};
		};
#endif

static int dp_enable = 1;
module_param(dp_enable, int, 0660);
static DEFINE_MUTEX(udp_lock);

/*sysfs begin*/
#pragma pack(push)
#pragma pack(1)
typedef struct display_node_t
{
	uint8_t state : 3;
	uint8_t : 5;
}display_node_t;

#pragma pack(4)
typedef union disp_sync_t
{
	display_node_t args[4]; //four panels
	uint32_t val;
}disp_sync_args_t;
#pragma pack(pop)

#if 0
	DC_STAT_NOTINIT     = 0,	/* not initilized by remote cpu */
	DC_STAT_INITING     = 1,	/* on initilizing */
	DC_STAT_INITED      = 2,	/* initilize compilete, ready for display */
	DC_STAT_BOOTING     = 3,	/* during boot time splash screen */
	DC_STAT_CLOSING     = 4,	/* DC is going to close */
	DC_STAT_CLOSED      = 5,	/* DC is closed safely */
	DC_STAT_NORMAL      = 6,	/* DC is used by DRM */
	DC_STAT_MAX
#endif
void sdrv_crtc_dpms(struct drm_crtc *crtc, int mode);

void dump_registers(struct sdrv_dpc *dpc, int start, int end)
{
	int i, count;

	count = (end - start) / 4 + 1;
	count = (count + 3) / 4;
	pr_err("--[crtc-%d]-[dpc-type:%d]-[0x%04x~0x%04x]-----\n",
		dpc->crtc->base.index, dpc->dpc_type, start, end);
	for (i = 0; i < count; i++)
		pr_err("0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl(dpc->regs + start + 0x10 * i),
			readl(dpc->regs + start + 0x10 * i + 0x4),
			readl(dpc->regs + start + 0x10 * i + 0x8),
			readl(dpc->regs + start + 0x10 * i + 0xc));
}

static inline int check_avm_alive(struct kunlun_crtc *kcrtc) {
	disp_sync_args_t status;
	struct sdrv_dpc  *dpc = kcrtc->master;

	if (dpc->next || dpc->dpc_type == DPC_TYPE_DC || dp_enable)
		return 0;

	status.val = kcrtc->avm_dc_status;
	switch (status.args[kcrtc->id].state) {
		case DC_STAT_NOTINIT:
			return 0;
		case DC_STAT_INITING:
		case DC_STAT_INITED:
		case DC_STAT_BOOTING:
			return 1;
		case DC_STAT_CLOSING:
		case DC_STAT_CLOSED:
			return 0;
		default:
			return 0;
	}

	return 0;
}

static ssize_t fastavm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	dc_state_t status;
	struct kunlun_crtc *kcrtc = dev_get_drvdata(dev);
	mutex_lock(&kcrtc->refresh_mutex);
	ret = sd_get_dc_status(&status);
	mutex_unlock(&kcrtc->refresh_mutex);
	if (ret) {
		pr_err("[drm] sd_get_dc_status failed\n");
		kcrtc->avm_dc_status = DC_STAT_NOTINIT;
		ret = snprintf(buf, PAGE_SIZE, "sd_get_dc_status failed: 0x%x\n", kcrtc->avm_dc_status);
		return ret;
	}
	kcrtc->avm_dc_status = (int)status;
	ret = snprintf(buf, PAGE_SIZE, "new status: 0x%x, old status: 0x%x\n", (uint32_t)status, kcrtc->avm_dc_status);

	return ret;
}

static ssize_t fastavm_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	struct kunlun_crtc *kcrtc = dev_get_drvdata(dev);
	disp_sync_args_t status;

	ret = kstrtoint(buf, 16, &status.val);
	if (ret) {
		DRM_ERROR("Invalid input: %s\n", buf);
		return count;
	}

	PRINT("set avm_dc_status = 0x%x\n", (int) status.val);
	mutex_lock(&kcrtc->refresh_mutex);
	sd_set_dc_status((int) status.val);
	ret = sd_get_dc_status((dc_state_t *)&kcrtc->avm_dc_status);
	mutex_unlock(&kcrtc->refresh_mutex);
	if (ret) {
		DRM_ERROR("[drm] sd_get_dc_status failed\n");
		kcrtc->avm_dc_status = DC_STAT_NOTINIT;
	}

	return count;
}
static DEVICE_ATTR_RW(fastavm);


int fps(struct kunlun_crtc *kcrtc)
{
	u64 now;

	now = ktime_get_boot_ns() / 1000 / 1000;

	if (now - kcrtc->fi.last_time > 1000) // 取固定时间间隔为1秒
	{
		kcrtc->fi.fps = kcrtc->fi.frame_count;
		kcrtc->fi.frame_count = 0;
		kcrtc->fi.last_time = now;
	}
	return kcrtc->fi.fps;
}
static ssize_t fps_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	struct kunlun_crtc *kcrtc = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 16, &kcrtc->fi.hw_fps);
	if (ret) {
		DRM_ERROR("Invalid input: %s\n", buf);
		return count;
	}

	return count;
}

static ssize_t fps_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	dc_state_t status;
	struct kunlun_crtc *kcrtc = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "fps: %d\n", fps(kcrtc));

	return ret;
}
static DEVICE_ATTR_RW(fps);

static struct attribute *crtc_attrs[] = {
	&dev_attr_fastavm.attr,
	&dev_attr_fps.attr,
	NULL,
};

static const struct attribute_group crtc_group = {
	.attrs = crtc_attrs,
};

int kunlun_crtc_sysfs_init(struct device *dev)
{
	int rc;

	pr_info("[drm] %s: crtc %s sysfs init\n", __func__, dev_name(dev));
	rc = sysfs_create_group(&(dev->kobj), &crtc_group);
	if (rc)
		pr_err("create crtc attr node failed, rc=%d\n", rc);
	return rc;
}

EXPORT_SYMBOL(kunlun_crtc_sysfs_init);

static int compare_name(struct device_node *np, const char *name)
{
	return strstr(np->name, name) != NULL;
}

static ssize_t dispInfo_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	ssize_t count = 0;
	int i, j;
	struct sdrv_dpc *dpc = dev_get_drvdata(dev);
	struct kunlun_crtc *kcrtc = dpc->crtc;
	count += sprintf(buf + count, "%s: combine_mode[%d] num_pipe[%d]:\n", dpc->name, dpc->combine_mode, dpc->num_pipe);
	for (i = 0; i < dpc->num_pipe; i++) {
		struct sdrv_pipe *p = dpc->pipes[i];
		struct dpc_layer *layer = &(p->layer_data);
		count += sprintf(buf + count, "  %s: plane_type[%d] plane_enable[%d]\n", p->name, p->type, layer->enable);
		if (!layer->enable)
			continue;
		kcrtc->fi.hw_fps = 1;
		count += sprintf(buf + count, "    fps[%d] index[%d] formart[%c%c%c%c] alpha[%d] blend_mode[%d] rotation[%d]\n",
			fps(kcrtc), layer->index, layer->format & 0xff, (layer->format >> 8) & 0xff, (layer->format >> 16) & 0xff,
			(layer->format >> 24) & 0xff, layer->alpha, layer->blend_mode, layer->rotation);
		count += sprintf(buf + count, "    zpos[%d] xfbc[%d] header_size_r[%d] header_size_y[%d] header_size_uv[%d]\n",
			layer->zpos, layer->xfbc, layer->header_size_r, layer->header_size_y, layer->header_size_uv);
		count += sprintf(buf + count, "    src[%d %d %d %d] dst[%d %d %d %d] width[%d] height[%d]\n",
			layer->src_x, layer->src_y, layer->src_w, layer->src_h, layer->dst_x, layer->dst_y, layer->dst_w,
			layer->dst_h, layer->width, layer->height);
		count += sprintf(buf + count, "    nplanes[%d] :\n", layer->nplanes);
		for(j = 0; j < layer->nplanes; j++) {
			count += sprintf(buf + count, "      addr_l[0x%x] addr_h[0x%x] pitch[%d]\n",
				layer->addr_l[j], layer->addr_h[j], layer->pitch[j]);
		}
	}
	return count;
}

static DEVICE_ATTR_RO(dispInfo);

static struct attribute *disp_class_attrs[] = {
	&dev_attr_dispInfo.attr,
	NULL,
};

static const struct attribute_group disp_group = {
	.attrs = disp_class_attrs,
};

static const struct attribute_group *disp_groups[] = {
	&disp_group,
	NULL,
};

static int __init display_class_init(void)
{
	pr_info("display class register\n");
	if (display_class)
		return 0;
	display_class = class_create(THIS_MODULE, "display");
	if (IS_ERR(display_class)) {
		pr_err("[drm] Unable to create display class: %ld\n", PTR_ERR(display_class));
		return PTR_ERR(display_class);
	}

	display_class->dev_groups = disp_groups;

	return 0;
}

int *get_zorders_from_pipe_mask(uint32_t mask)
{
	int i, n;
	static int zorders[8] = {0};
	int z = 0;
	for(i = 0, n = 0; i < 8; i++) {
		if (mask & (1 << i)) {
			n++;
			if (z == 0)
				z = i;
		}
	}

	for (i = 0; i < n; i++) {
		zorders[i] = z + i;
	}

	return zorders;
}

int _add_pipe(struct sdrv_dpc *dpc, int type, int hwid, const char *pipe_name, uint32_t offset)
{
	struct sdrv_pipe *p = NULL;

	p = devm_kzalloc(&dpc->dev, sizeof(struct sdrv_pipe), GFP_KERNEL);
	if (!p)
		return -ENOMEM;
	p->name = pipe_name;
	p->ops = (struct pipe_operation*)pipe_ops_attach(p->name);
	if (!p->ops) {
		DRM_ERROR("error ops attached\n");
		return -EINVAL;
	}

	p->regs = dpc->regs + offset;
	p->id = hwid;
	p->dpc = dpc;
	p->type = type;
	dpc->pipes[dpc->num_pipe] = p;
	dpc->pipe_mask |= (1 << hwid);
	dpc->num_pipe++;

	p->ops->init(p);

	return 0;
}

void kunlun_crtc_handle_vblank(struct kunlun_crtc *kcrtc)
{
	unsigned long flags;
	struct drm_crtc *crtc = &kcrtc->base;

	if (!crtc)
		return;
	drm_crtc_handle_vblank(crtc);

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	if(kcrtc->event) {
		drm_crtc_send_vblank_event(crtc, kcrtc->event);
		drm_crtc_vblank_put(crtc);
		kcrtc->event = NULL;
	}
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);

	if (kcrtc->fi.hw_fps == 1) {
		/* count the sw fps*/
		++kcrtc->fi.frame_count;
		fps(kcrtc);
	}
}

struct dp_dummy_data *dummy_datas[10];
static int num_dummy_data = 0;

int register_dummy_data(struct dp_dummy_data *dummy)
{
	int id = num_dummy_data;
	if (!dummy || id >= 10) {
		return -ENODEV;
	}
	DRM_INFO("register dummy data: %p\n", dummy);
	dummy_datas[id] = dummy;
	num_dummy_data++;
	return 0;
}

void unregister_dummy_data(struct dp_dummy_data *dummy)
{
	int id = num_dummy_data;
	if (!dummy) {
		return;
	}
	for (id = num_dummy_data -1; id >= 0; id--) {
		if (dummy_datas[id] == dummy) {
			dummy_datas[id] = 0;
			num_dummy_data--;
		}
	}
}

void call_dummy_vsync(void) {
	int i = 0;
	struct dp_dummy_data *d;
	struct kunlun_crtc *kcrtc;
	for (i = 0; i < num_dummy_data; i++) {
		d = dummy_datas[i];
		kcrtc = d->dpc->crtc;
		if (d->vsync_enabled) {
			kcrtc->evt_update = true;
			wake_up_interruptible_all(&kcrtc->wait_queue);
			drm_crtc_handle_vblank(&kcrtc->base);
		}
	}
}

irqreturn_t sdrv_irq_handler(int irq, void *data)
{
	struct sdrv_dpc *dpc = data;
	struct kunlun_crtc *kcrtc = dpc->crtc;
	uint32_t val;

	if (!kcrtc->du_inited) {
		pr_info_ratelimited("[drm] kcrtc %p, du_inited %d\n", kcrtc, kcrtc->du_inited);
		return IRQ_HANDLED;
	}

	val = kcrtc->master->ops->irq_handler(dpc);

	if(val & SDRV_TCON_EOF_MASK) {
#ifdef CONFIG_RECOVERY_ENABLED
		kcrtc->evt_update = true;
		wake_up_interruptible_all(&kcrtc->wait_queue);
		drm_crtc_handle_vblank(&kcrtc->base);

		if (kcrtc->fi.hw_fps == 1) {
			/* count the sw fps*/
			++kcrtc->fi.frame_count;
			fps(kcrtc);
		}
#else
		kunlun_crtc_handle_vblank(kcrtc);
#endif
		call_dummy_vsync();
	}

	if(val & SDRV_DC_UNDERRUN_MASK) {
		pr_err_ratelimited("under run.....\n");
	}

	return IRQ_HANDLED;
}

static void kunlun_crtc_atomic_begin(struct drm_crtc *crtc,
		struct drm_crtc_state *old_crtc_state)
{
	int i, j;
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);

	// mutex_lock(&kcrtc->refresh_mutex);
	//slave first
	for (i = 0; i < 2; i++) {
		struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;
		if (!dpc || !crtc->state->planes_changed)
			continue;

		for (j = 0; j< kcrtc->num_planes; j++) {

				struct sdrv_pipe *p = dpc->pipes[j];
				struct dpc_layer *layer = (struct dpc_layer *)&p->layer_data;

				memset(layer, 0, sizeof(struct dpc_layer));
		}
	}

}

static void kunlun_crtc_atomic_flush(struct drm_crtc *crtc,
		struct drm_crtc_state *old_crtc_state)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	struct kunlun_crtc_state *state = to_kunlun_crtc_state(crtc->state);
	int ret;
	int i, j;
	// if(WARN_ON(kcrtc->enabled))
	// 	return;

	if (!state->base.active)
		goto OUT;

	if (!kcrtc->recover_done)
		goto OUT;

	state->plane_mask = crtc->state->plane_mask;

	if(!state->plane_mask) {
		DRM_INFO("state->plane mask is null: 0x%x\n", state->plane_mask);
		goto OUT;
	}

	//slave first
	for (i = 0; i < 2; i++) {
		u8 count = 0;
		struct dpc_layer *layers[16];
		struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;

		if (!dpc)
			continue;

		for (j = 0; j< kcrtc->num_planes; j++) {
			u8 plane_enabled =
				state->plane_mask & (1 << drm_plane_index(&kcrtc->planes[j].base))? 1: 0;
			struct sdrv_pipe *p = dpc->pipes[j];
			if (p->layer_data.enable != plane_enabled) {
				pr_debug("id %d this layer[%d] should enabled? %d : %d  plane_mask 0x%x : 0x%x\n",
					kcrtc->planes[j].base.base.id, j, plane_enabled,
					p->layer_data.enable, state->plane_mask,
					(1 << drm_plane_index(&kcrtc->planes[j].base)));
			}
 			if (plane_enabled) count++;
			layers[j] = &p->layer_data;
		}

		struct drm_display_mode *mode = &crtc->state->adjusted_mode;
		struct kunlun_crtc_state *state = to_kunlun_crtc_state(crtc->state);
		if (dpc && dpc->ops->modeset) {
			dpc->ops->modeset(dpc, mode, state->bus_flags);
			if (dpc->next && dpc->next->ops->modeset) {
				dpc->next->ops->mlc_select(dpc->next, dpc->mlc_select);
				dpc->next->ops->modeset(dpc->next, mode, state->bus_flags);
			}
		}

		ret = dpc->ops->update(dpc, *layers, count);

		if (ret) {
			DRM_ERROR("update plane[%d] failed\n", i);
			goto OUT;
		}

		dpc->ops->flush(dpc);
		if (dpc->next)
			dpc->next->ops->flush(dpc->next);
	}

#ifdef CONFIG_RECOVERY_ENABLED

	/*wait vysnc*/
	if (old_crtc_state->active) {
		kcrtc->evt_update = false;
		ret = wait_event_interruptible_timeout(kcrtc->wait_queue, kcrtc->evt_update, msecs_to_jiffies(500));

		if (!ret) {
			/* timeout*/
			DRM_ERROR("crtc[%d] wait for vsync timeout!\n", crtc->index);

			for (i = 0; i < 2; i++) {
				struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;

				if (!dpc)
					continue;

				if (dpc->ops->dump) {
					dpc->ops->dump(dpc);
					if (dpc->next && dpc->next->ops->dump)
						dpc->next->ops->dump(dpc->next);
				}
			}
			kcrtc->recover_done = false;
			kthread_queue_work(&kcrtc->kworker, &kcrtc->recovery);
			goto OUT;
		}

		for (i = 0; i < 2; i++) {
			struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;

			if (!dpc)
				continue;

			if (dpc->ops->update_done) {
				ret = dpc->ops->update_done(dpc);
				if (ret) {
					if (dpc->next && dpc->next->ops->dump)
						dpc->next->ops->dump(dpc->next);
					kcrtc->recover_done = false;
					kthread_queue_work(&kcrtc->kworker, &kcrtc->recovery);
					goto OUT;
				}
			}
		}
	}

OUT:
	spin_lock_irq(&crtc->dev->event_lock);
	if (crtc->state->event) {
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		crtc->state->event = NULL;
	}
	spin_unlock_irq(&crtc->dev->event_lock);
#else
OUT:
	spin_lock_irq(&crtc->dev->event_lock);
	if (crtc->state->event && old_crtc_state->active) {
		WARN_ON(drm_crtc_vblank_get(crtc));
		WARN_ON(kcrtc->event);

		kcrtc->event = crtc->state->event;
		crtc->state->event = NULL;
	}
	spin_unlock_irq(&crtc->dev->event_lock);
#endif

	if (kcrtc->fi.hw_fps == 0) {
		/* count the sw fps*/
		++kcrtc->fi.frame_count;
		fps(kcrtc);
	}
	// mutex_unlock(&kcrtc->refresh_mutex);
	return;
}

static void kunlun_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	struct kunlun_crtc_state *state = to_kunlun_crtc_state(crtc->state);
	struct drm_display_mode *mode = &crtc->state->adjusted_mode;
	int i;

	DRM_DEV_INFO(kcrtc->dev,
			"set mode: %d:\"%s\" %d %d %d %d %d %d %d %d %d %d 0x%x 0x%x",
			mode->base.id, mode->name, mode->vrefresh, mode->clock,
			mode->hdisplay, mode->hsync_start, mode->hsync_end, mode->htotal,
			mode->vdisplay, mode->vsync_start, mode->vsync_end, mode->vtotal,
			mode->type, mode->flags);

	state->plane_mask |= crtc->state->plane_mask;

	//slave first
	for (i = 0; i < 2; i++) {
		struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;
		if (dpc && dpc->ops->modeset) {
			dpc->ops->modeset(dpc, mode, state->bus_flags);
			if (dpc->next && dpc->next->ops->modeset) {
				dpc->next->ops->mlc_select(dpc->next, dpc->mlc_select);
				dpc->next->ops->modeset(dpc->next, mode, state->bus_flags);
			}

		}

	}
}

static void kunlun_crtc_atomic_enable(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	int i = 0;

	//if(WARN_ON(kcrtc->enabled))
	//	return;

	//slave first
	for (i = 0; i < 2; i++) {
		struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;
		if (!kcrtc->enable_status && dpc) {
			dpc->ops->enable(dpc, true);
			if (dpc->next)
				dpc->next->ops->enable(dpc->next, true);
			kcrtc->enable_status = true;
			if (kcrtc->irq)
				enable_irq(kcrtc->irq);
		}
	}

	drm_crtc_vblank_on(crtc);
	WARN_ON(drm_crtc_vblank_get(crtc));
#ifndef CONFIG_RECOVERY_ENABLED
	if (crtc->state->event) {
		WARN_ON(kcrtc->event);

		kcrtc->event = crtc->state->event;
		crtc->state->event = NULL;
	}
#endif
}

static void kunlun_crtc_atomic_disable(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state)
{
	//struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	//int i = 0;

	// if(WARN_ON(!kcrtc->enabled))
	//	return;
	// if (kcrtc->irq)
	//disable_irq(kcrtc->irq);

	spin_lock_irq(&crtc->dev->event_lock);
	if (crtc->state->event) {
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		crtc->state->event = NULL;
	}
	spin_unlock_irq(&crtc->dev->event_lock);

	drm_crtc_vblank_off(crtc);

	//slave first
	/*for (i = 0; i < 2; i++) {
		struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;
		if (dpc)
			dpc->ops->enable(dpc, false);
	}

	kcrtc->enabled = false;*/

}


static const struct drm_crtc_helper_funcs kunlun_crtc_helper_funcs = {
	.dpms = sdrv_crtc_dpms,
	.atomic_flush = kunlun_crtc_atomic_flush,
	.atomic_begin = kunlun_crtc_atomic_begin,
	.mode_set_nofb = kunlun_crtc_mode_set_nofb,
	.atomic_enable = kunlun_crtc_atomic_enable,
	.atomic_disable = kunlun_crtc_atomic_disable,
};

static void kunlun_crtc_destroy(struct drm_crtc *crtc)
{
	drm_crtc_cleanup(crtc);
}

static void kunlun_crtc_reset(struct drm_crtc *crtc)
{
	if(crtc->state)
		__drm_atomic_helper_crtc_destroy_state(crtc->state);
	kfree(crtc->state);

	crtc->state = kzalloc(sizeof(struct kunlun_crtc_state), GFP_KERNEL);
	if(crtc->state)
		crtc->state->crtc = crtc;
}

static struct drm_crtc_state *
kunlun_crtc_duplicate_state(struct drm_crtc *crtc)
{
	struct kunlun_crtc_state *kunlun_state, *current_state;

	current_state = to_kunlun_crtc_state(crtc->state);

	kunlun_state = kzalloc(sizeof(struct kunlun_crtc_state), GFP_KERNEL);
	if(!kunlun_state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &kunlun_state->base);

	kunlun_state->plane_mask = current_state->plane_mask;

	return &kunlun_state->base;
}

static void kunlun_crtc_destroy_state(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	struct kunlun_crtc_state *s = to_kunlun_crtc_state(state);

	__drm_atomic_helper_crtc_destroy_state(state);

	kfree(s);
}

static int kunlun_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	struct sdrv_dpc *dpc;

	if(!kcrtc->vblank_enable)
			return -EPERM;

	dpc = kcrtc->master;
	dpc->ops->vblank_enable(dpc, true);

	return 0;
}
void sdrv_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	DRM_INFO("kcrtc name %s set dpms %d\n", kcrtc->master->name, mode);
}

static void kunlun_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	struct sdrv_dpc *dpc;

	if(!kcrtc->vblank_enable)
			return;

	dpc = kcrtc->master;
	dpc->ops->vblank_enable(dpc, false);
}

static const struct drm_crtc_funcs kunlun_crtc_funcs = {
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.destroy = kunlun_crtc_destroy,
	.reset = kunlun_crtc_reset,
	.atomic_duplicate_state = kunlun_crtc_duplicate_state,
	.atomic_destroy_state = kunlun_crtc_destroy_state,
	.enable_vblank = kunlun_crtc_enable_vblank,
	.disable_vblank = kunlun_crtc_disable_vblank,
//	.set_crc_source = kunlun_crtc_set_crc_source,
};


static int sdrv_dpc_device_create(struct sdrv_dpc *dpc,
				struct device *parent, struct device_node *np)
{
	int err;

	if (!parent) {
		DRM_ERROR("parent device is null\n");
		return -1;
	}

	dpc->dev.class = display_class;
	dpc->dev.parent = parent;
	dpc->dev.of_node = np;
	dev_set_name(&dpc->dev, "%s", np->name);
	dev_set_drvdata(&dpc->dev, dpc);

	err = device_register(&dpc->dev);
	if (err)
		DRM_ERROR("dpc device register failed\n");

	return err;
}

static int sdrv_dpc_get_resource(struct sdrv_dpc *dpc, struct device_node *np)
{
	struct resource res;
	const char *str;
	char *temp_str;
	int irq_num;
	int ret;

	if (!dpc) {
		pr_err("dpc pointer is null\n");
		return -EINVAL;
	}

	if (!of_property_read_string(np, "sdrv,ip", &str)) {
		dpc->name = str;
	} else
		DRM_WARN("sdrv,ip can not found\n");

	if (compare_name(np, "dummy"))
		dpc->dpc_type = DPC_TYPE_DUMMY;

	DRM_INFO("dpc->dpc_type = %d\n", dpc->dpc_type);
	if (dpc->dpc_type >= DPC_TYPE_DUMMY)
		return 0;


	/*Only Linux Os, we always need dc.*/
	if (of_address_to_resource(np, 0, &res)) {
		DRM_ERROR("parse dt base address failed\n");
		return -ENODEV;
	}
	DRM_INFO("got %s res 0x%lx\n", dpc->name, (unsigned long)res.start);

	dpc->regs = devm_ioremap_nocache(&dpc->dev, res.start, resource_size(&res));
	if(IS_ERR_OR_NULL(dpc->regs)) {
		DRM_ERROR("Cannot find dpc regs 001\n");
		return PTR_ERR(dpc->regs);
	}



	irq_num = irq_of_parse_and_map(np, 0);
	if (!irq_num) {
		DRM_ERROR("error: dpc parse irq num failed\n");
		return -EINVAL;
	}
	DRM_INFO("dpc irq_num = %d\n", irq_num);
	dpc->irq = irq_num;

	ret = of_property_read_u32(np, "mlc-select", &dpc->mlc_select);
	if (ret) {
		dpc->mlc_select = 0;
	}

	return 0;
}

static int sdrv_dpc_init(struct sdrv_dpc *dpc, struct device_node *np)
{
	int i, ret;
	struct device *pdev;
	const struct sdrv_dpc_data *data;

	if (!dpc)
		return -EINVAL;
	pdev = dpc->crtc->dev;

	dpc->dpc_type = compare_name(np, "dp") ? DPC_TYPE_DP: DPC_TYPE_DC;

	sdrv_dpc_device_create(dpc, pdev, np);

	ret = sdrv_dpc_get_resource(dpc, np);
	if (ret) {
		DRM_ERROR("dpc get resource failed\n");
		return -EINVAL;
	}

	data = of_device_get_match_data(pdev);
	for (i = 0; i < 16; i++){
		if (!strcmp(dpc->name, data[i].version)) {
			dpc->ops = data[i].ops;
			DRM_INFO("%s ops[%d] attached\n", dpc->name, i);
			break;
		}
	}

	if (dpc->ops == NULL) {
		DRM_ERROR("core ops attach failed, have checked %d times\n", i);
		return -1;
	}
	dpc->num_pipe = 0;
	// dpc init
	dpc->ops->init(dpc);

	return 0;
}

static void sdrv_dpc_unit(struct sdrv_dpc *dpc)
{
	if (!dpc)
		return;
	if (dpc->ops->uninit)
		dpc->ops->uninit(dpc);
}

static void kunlun_crtc_recovery(struct kthread_work *work)
{
	struct kunlun_crtc *kcrtc = container_of(work, struct kunlun_crtc, recovery);
	int i;

	DRM_INFO("recovery crtc-%d start\n", kcrtc->base.index);

	if (kcrtc->master) {
		if (kcrtc->irq)
			disable_irq(kcrtc->irq);
		kcrtc->master->ops->init(kcrtc->master);
		if (kcrtc->master->next)
			kcrtc->master->next->ops->init(kcrtc->master->next);
	}

	if (kcrtc->slave) {
		kcrtc->slave->ops->init(kcrtc->slave);
		if (kcrtc->slave->next)
			kcrtc->slave->next->ops->init(kcrtc->slave->next);
	}

	for (i = 0; i < 2; i++) {
		struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;
			if (dpc) {
				dpc->ops->enable(dpc, true);
				if (dpc->next)
					dpc->next->ops->enable(dpc->next, true);
			}
	}
	kcrtc->recover_done = true;
	if (kcrtc->irq)
		enable_irq(kcrtc->irq);
	kunlun_crtc_enable_vblank(&kcrtc->base);

	DRM_INFO("recovery crtc-%d end\n", kcrtc->base.index);
}

static int kunlun_crtc_bind(struct device *dev, struct device *master,
		void *data)
{
	struct device_node *np;
	struct device_node *port;
	struct drm_device *drm = data;
	struct kunlun_drm_data *kdrm = drm->dev_private;
	struct kunlun_crtc *kcrtc;
	struct drm_plane *primary = NULL, *cursor = NULL;
	struct sdrv_dpc *dpc;
	u32 temp;
	int i, ret;
	struct task_struct *task;

	kcrtc = dev_get_drvdata(dev);
	kcrtc->drm = drm;

	// check combine mode of_property_read_u32
	if (!of_property_read_u32(dev->of_node, "display-mode", &temp))
		kcrtc->combine_mode = temp;
	else
		kcrtc->combine_mode = 1;

	for (i = 0; ; i++) {
		np = of_parse_phandle(dev->of_node, "dpc-master", i);
		if (!np)
			break;
		if(!of_device_is_available(np)) {
			DRM_DEV_INFO(dev, "OF node %s not available or match\n", np->name);
			continue;
		}
		dpc = (struct sdrv_dpc *)devm_kzalloc(dev, sizeof(struct sdrv_dpc), GFP_KERNEL);
		if (!dpc) {
			DRM_ERROR("kzalloc dpc failed\n");
			return -ENOMEM;
		}
		dpc->crtc = kcrtc;
		DRM_DEV_INFO(dev, "Add component %s", np->name);
		dpc->combine_mode = kcrtc->combine_mode;
		dpc->is_master = true;
		dpc->inited = false;

		if (!kcrtc->master)
			kcrtc->master = dpc;
		else
			kcrtc->master->next = dpc;

		sdrv_dpc_init(dpc, np);
	}
	if (!kcrtc->master) {
		DRM_ERROR("dpc-master is not specified, dts maybe not match\n");
		return -EINVAL;
	}

	if (kcrtc->combine_mode > 1) {
		for (i = 0; ; i++) {
			np = of_parse_phandle(dev->of_node, "dpc-slave", i);
			if (!np)
				break;
			//TODO: check the master and slave dpc type??
			if(!of_device_is_available(np)) {
				DRM_DEV_INFO(dev, "OF node %s not available or match\n", np->name);
				continue;
			}
			dpc = (struct sdrv_dpc *)devm_kzalloc(dev, sizeof(struct sdrv_dpc), GFP_KERNEL);
			if (!dpc) {
				DRM_ERROR("kzalloc dpc failed\n");
				return -ENOMEM;
			}
			dpc->crtc = kcrtc;
			DRM_DEV_INFO(dev, "Add component %s", np->name);
			dpc->combine_mode = kcrtc->combine_mode;
			dpc->is_master = false;
			dpc->inited = false;

			if (!kcrtc->slave)
				kcrtc->slave = dpc;
			else
				kcrtc->slave->next = dpc;

			sdrv_dpc_init(dpc, np);
		}

		if (kcrtc->slave->num_pipe != kcrtc->master->num_pipe) {
			DRM_ERROR("The pipes number of master(%d) and slave(%d) is not match!\n",
				kcrtc->master->num_pipe, kcrtc->slave->num_pipe);
			goto fail_dpc_init;
		}

		/* binding master and slave pipes in combine mode */
		for (i = 0; i < kcrtc->master->num_pipe; i++) {
			kcrtc->master->pipes[i]->next = kcrtc->slave->pipes[i];
		}
	}
	mutex_init(&kcrtc->refresh_mutex);

	//irq_set_status_flags(irq_num, IRQ_NOAUTOEN);
	kcrtc->irq = kcrtc->master->irq;
	if (kcrtc->master->dpc_type < DPC_TYPE_DUMMY) {
		irq_set_status_flags(kcrtc->irq, IRQ_NOAUTOEN);
		ret = devm_request_irq(dev, kcrtc->irq, sdrv_irq_handler,
				0, dev_name(dev), kcrtc->master); //IRQF_SHARED
		if(ret) {
			DRM_DEV_ERROR(dev, "Failed to request DC IRQ: %d\n", kcrtc->irq);
			goto fail_dpc_init;
		}
	}

	port = of_get_child_by_name(dev->of_node, "port");
	if (!port) {
		DRM_DEV_ERROR(dev, "no port node found in %pOF\n", dev->of_node);
		return -ENOENT;
	}
	kcrtc->base.port = port;
	// plane init
	kcrtc->num_planes = kcrtc->master->num_pipe;

	kcrtc->planes = devm_kzalloc(kcrtc->dev, kcrtc->num_planes * sizeof(struct kunlun_plane), GFP_KERNEL);
	if(!kcrtc->planes)
		return -ENOMEM;
	DDBG(kcrtc->master->num_pipe);
	for (i = 0; i < kcrtc->num_planes; i++) {
		kcrtc->planes[i].type = kcrtc->master->pipes[i]->type;
		kcrtc->planes[i].cap = kcrtc->master->pipes[i]->cap;
		kcrtc->planes[i].kcrtc = kcrtc;

		if (kcrtc->combine_mode > 1) {
			kcrtc->planes[i].pipes[0] = kcrtc->master->pipes[i];
			kcrtc->planes[i].pipes[1] = kcrtc->slave->pipes[i];
			kcrtc->planes[i].num_pipe = 2;
		} else { //only master
			kcrtc->planes[i].pipes[0] = kcrtc->master->pipes[i];
			kcrtc->planes[i].num_pipe = 1;
		}
	}

	ret = kunlun_planes_init_primary(kcrtc, &primary, &cursor);
	if(ret)
		goto err_planes_fini;
	if(!primary) {
		DRM_DEV_ERROR(kcrtc->dev, "CRTC cannot find primary plane\n");
		goto err_planes_fini;
	}

	ret = drm_crtc_init_with_planes(kcrtc->drm, &kcrtc->base,
			primary, cursor, &kunlun_crtc_funcs, NULL);
	if(ret)
		goto err_planes_fini;

	drm_crtc_helper_add(&kcrtc->base, &kunlun_crtc_helper_funcs);

	ret = kunlun_planes_init_overlay(kcrtc);
	if(ret)
		goto err_crtc_cleanup;

	kunlun_crtc_sysfs_init(dev);


	kdrm->crtcs[kdrm->num_crtcs] = &kcrtc->base;
	kdrm->num_crtcs++;
	kcrtc->vblank_enable = true;
	kcrtc->du_inited = true;
	kcrtc->enable_status = false;
	kcrtc->recover_done = true;

	init_waitqueue_head(&kcrtc->wait_queue);
	kthread_init_worker(&kcrtc->kworker);
	kthread_init_work(&kcrtc->recovery, kunlun_crtc_recovery);
	if (!kcrtc_primary)
		kcrtc_primary = kcrtc;

	task = kthread_run(kthread_worker_fn, &kcrtc->kworker, "crtc[%d]-worker", kcrtc->id);
	if (task)
		kcrtc->work_thread = task;
	else
		return -EINVAL;

	return 0;
err_crtc_cleanup:
	drm_crtc_cleanup(&kcrtc->base);
err_planes_fini:
	kunlun_planes_fini(kcrtc);
fail_dpc_init:
	sdrv_dpc_unit(kcrtc->slave);
	sdrv_dpc_unit(kcrtc->slave->next);
	sdrv_dpc_unit(kcrtc->master);
	sdrv_dpc_unit(kcrtc->master->next);
	return -1;
}

static void kunlun_crtc_unbind(struct device *dev,
		struct device *master, void *data)
{
	// struct kunlun_crtc *crtc = dev_get_drvdata(dev);

	// kunlun_crtc_planes_fini(crtc);
}
static const struct component_ops kunlun_crtc_ops = {
	.bind = kunlun_crtc_bind,
	.unbind = kunlun_crtc_unbind,
};

static int kunlun_drm_crtc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct kunlun_crtc *kcrtc;

	kcrtc = (struct kunlun_crtc *)devm_kzalloc(&pdev->dev, sizeof(struct kunlun_crtc), GFP_KERNEL);
	if (!kcrtc)
		return -ENOMEM;

	kcrtc->dev = dev;
	platform_set_drvdata(pdev, kcrtc);

	if (!display_class) {
		// ugly..
		pipe_ops_register(&spipe_entry);
		pipe_ops_register(&gpipe_dc_entry);
		pipe_ops_register(&gpipe1_dp_entry);
		pipe_ops_register(&gpipe0_dp_entry);
		pipe_ops_register(&apipe_entry);
	}
	display_class_init();

	return component_add(dev, &kunlun_crtc_ops);
}

static int kunlun_drm_crtc_remove(struct platform_device *pdev)
{
	int i;
	struct device *dev = &pdev->dev;
	struct kunlun_crtc *kcrtc = platform_get_drvdata(pdev);

	for (i = 0; i < 2; i++) {
		struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;
		if (!dpc)
			continue;
		if (dpc->ops->shutdown) {
			dpc->ops->shutdown(dpc);
			if (dpc->next && dpc->next->ops->shutdown) {
				dpc->next->ops->shutdown(dpc->next);
			}
		}
	}

	component_del(dev, &kunlun_crtc_ops);
	return 0;
}

static void kunlun_drm_crtc_shutdown(struct platform_device *pdev)
{
	int i;
	struct kunlun_crtc *kcrtc = platform_get_drvdata(pdev);

	for (i = 0; i < 2; i++) {
		struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;
		if (!dpc)
			continue;
		if (dpc->ops->shutdown)
			dpc->ops->shutdown(dpc);
	}
}

#ifdef CONFIG_PM_SLEEP
static int kunlun_crtc_sys_suspend(struct device *dev)
{
	int i = 0;
	struct kunlun_crtc *kcrtc = dev_get_drvdata(dev);
	struct drm_crtc *crtc = &kcrtc->base;
	struct drm_device *drm = crtc->dev;

	DRM_INFO("kunlun_crtc_sys_suspend enter !\n");

	if (!drm)
		return 0;

	drm_kms_helper_poll_disable(drm);
	DRM_INFO("save drm state !\n");
	kcrtc->state = drm_atomic_helper_suspend(drm);
	if (IS_ERR(kcrtc->state)) {
		drm_kms_helper_poll_enable(drm);
		DRM_ERROR("drm atomic helper suspend return error !\n");
		return PTR_ERR(kcrtc->state);
	}

	for (i = 0; i < 2; i++) {
		struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;

		if (!dpc)
			continue;

		DRM_INFO("[crtc-%d]-[dpc-name:%s]-[dpc-type:%d]\n", crtc->index, dpc->name, dpc->dpc_type);
		if (dpc->ops->shutdown) {
			dpc->ops->shutdown(dpc);
			if (dpc->next && dpc->next->ops->shutdown) {
				dpc->next->ops->shutdown(dpc->next);
			}
		}
	}

	DRM_INFO("kunlun_crtc_sys_suspend out !\n");

	return 0;
}

static int kunlun_crtc_sys_resume(struct device *dev)
{
	int i;
	struct kunlun_crtc *kcrtc = dev_get_drvdata(dev);
	struct drm_crtc *crtc = &kcrtc->base;
	struct drm_device *drm = crtc->dev;

	DRM_INFO("kunlun_crtc_sys_resume enter !\n");

	kcrtc->recover_done = false;

	if (kcrtc->master) {
		if (kcrtc->irq)
			disable_irq(kcrtc->irq);
		kcrtc->master->ops->init(kcrtc->master);
		if (kcrtc->master->next)
			kcrtc->master->next->ops->init(kcrtc->master->next);
	}

	if (kcrtc->slave) {
		kcrtc->slave->ops->init(kcrtc->slave);
		if (kcrtc->slave->next)
			kcrtc->slave->next->ops->init(kcrtc->slave->next);
	}

	for (i = 0; i < 2; i++) {
		struct sdrv_dpc *dpc = i % 2? kcrtc->master: kcrtc->slave;
			if (dpc) {
				DRM_INFO("[crtc-%d]-[dpc-type:%d]-[dpc-name:%s]\n", kcrtc->base.index, dpc->dpc_type, dpc->name);
				dpc->ops->enable(dpc, true);
				if (dpc->next)
					dpc->next->ops->enable(dpc->next, true);
			}
	}

	kcrtc->recover_done = true;
	if (kcrtc->irq)
		enable_irq(kcrtc->irq);
	kunlun_crtc_enable_vblank(&kcrtc->base);

	if (!drm)
		return 0;

	DRM_INFO("pop drm state to atomtic!\n");
	drm_atomic_helper_resume(drm, kcrtc->state);
	drm_kms_helper_poll_enable(drm);

	DRM_INFO("kunlun_crtc_sys_resume out !\n");
	return 0;
}

#endif //end #ifdef CONFIG_PM_SLEEP

static const struct dev_pm_ops kunlun_crtc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(kunlun_crtc_sys_suspend,
				kunlun_crtc_sys_resume)
};


struct sdrv_dpc_data dpc_data[] = {
	{.version = "dc-r0p1", .ops = &dc_r0p1_ops},
	{.version = "dp-r0p1", .ops = &dp_r0p1_ops},
	{.version = "dp-dummy", .ops = &dp_dummy_ops},
	{.version = "dp-rpcall", .ops = &dp_rpcall_ops},
	{.version = "dp-op", .ops = &dp_op_ops},
	{},
};

static const struct of_device_id kunlun_crtc_of_table[] = {
	{.compatible = "semidrive,kunlun-crtc", .data = dpc_data},
	{},
};

static struct platform_driver kunlun_crtc_driver = {
	.probe = kunlun_drm_crtc_probe,
	.remove = kunlun_drm_crtc_remove,
	.shutdown = kunlun_drm_crtc_shutdown,
	.driver = {
		.name = "kunlun-crtc",
		.of_match_table = kunlun_crtc_of_table,
		.pm = &kunlun_crtc_pm_ops,
	},
};
module_platform_driver(kunlun_crtc_driver);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive kunlun dc CRTC");
MODULE_LICENSE("GPL");
