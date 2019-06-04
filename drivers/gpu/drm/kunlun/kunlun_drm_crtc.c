/*
 * kunlun_drm_crtc.c
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#include <drm/drm.h>
#include <drm/drmP.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_of.h>
#include <linux/of_device.h>

#include "kunlun_drm_drv.h"
#include "kunlun_drm_gem.h"
#include "kunlun_drm_crtc.h"

int kunlun_mlc_update_plane(struct kunlun_crtc *kcrtc,
		unsigned int mask);
int kunlun_mlc_update_trig(struct kunlun_crtc *kcrtc);

static void kunlun_crtc_atomic_begin(struct drm_crtc *crtc,
		struct drm_crtc_state *old_crtc_state)
{

}

static void kunlun_crtc_atomic_flush(struct drm_crtc *crtc,
		struct drm_crtc_state *old_crtc_state)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	struct kunlun_crtc_state *state = to_kunlun_crtc_state(crtc->state);
	struct kunlun_crtc_state *old_state;
	int ret;

//	if(WARN_ON(kcrtc->enabled))
//		return;

	if(!state->base.active)
		return;

	spin_lock_irq(&crtc->dev->event_lock);
	if (crtc->state->event && old_crtc_state->active) {
		WARN_ON(drm_crtc_vblank_get(crtc));
		WARN_ON(kcrtc->event);

		kcrtc->event = crtc->state->event;
		crtc->state->event = NULL;
	}
	spin_unlock_irq(&crtc->dev->event_lock);

	old_state = to_kunlun_crtc_state(old_crtc_state);
//	if(state->plane_mask != old_state->plane_mask) {
		ret = kunlun_mlc_update_plane(kcrtc, state->plane_mask);
		if(ret)
			return;
//	}

	ret = kunlun_mlc_update_trig(kcrtc);
	if(ret)
		return;
}

static void kunlun_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	const struct kunlun_crtc_data *data = kcrtc->data;
	struct kunlun_crtc_state *state = to_kunlun_crtc_state(crtc->state);
	struct drm_display_mode *mode = &crtc->state->adjusted_mode;

	DRM_DEV_INFO(kcrtc->dev,
			"set mode: %d:\"%s\" %d %d %d %d %d %d %d %d %d %d 0x%x 0x%x",
			mode->base.id, mode->name, mode->vrefresh, mode->clock,
			mode->hdisplay, mode->hsync_start, mode->hsync_end, mode->htotal,
			mode->vdisplay, mode->vsync_start, mode->vsync_end, mode->vtotal,
			mode->type, mode->flags);

	state->plane_mask |= crtc->state->plane_mask;
	kunlun_mlc_set_timings(kcrtc, mode);

	if(data->tcon)
		kunlun_tcon_set_timings(kcrtc, mode, state->bus_flags);
}

static void kunlun_crtc_atomic_enable(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	const struct kunlun_crtc_data *data = kcrtc->data;
	int ret;

	if(WARN_ON(kcrtc->enabled))
		return;

	ret = data->du_ops->du_enable(kcrtc, true);
	if(ret)
		return;

//	enable_irq(kcrtc->irq);
	drm_crtc_vblank_on(crtc);
	WARN_ON(drm_crtc_vblank_get(crtc));
	if (crtc->state->event) {
		WARN_ON(kcrtc->event);

		kcrtc->event = crtc->state->event;
		crtc->state->event = NULL;
	}
	kcrtc->enabled = true;
}

static void kunlun_crtc_atomic_disable(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	const struct kunlun_crtc_data *data = kcrtc->data;

	if(WARN_ON(!kcrtc->enabled))
		return;

//	disable_irq(kcrtc->irq);

	spin_lock_irq(&crtc->dev->event_lock);
	if (crtc->state->event) {
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		crtc->state->event = NULL;
	}
	spin_unlock_irq(&crtc->dev->event_lock);

	drm_crtc_vblank_off(crtc);

	data->du_ops->du_enable(kcrtc, false);

	kcrtc->enabled = false;
}

static const struct drm_crtc_helper_funcs kunlun_crtc_helper_funcs = {
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
	const struct kunlun_crtc_data *data = kcrtc->data;

	if(!kcrtc->vblank_enable)
			return -EPERM;

	return data->du_ops->vblank_enable(kcrtc, true);
}

static void kunlun_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct kunlun_crtc *kcrtc = to_kunlun_crtc(crtc);
	const struct kunlun_crtc_data *data = kcrtc->data;

	if(!kcrtc->vblank_enable)
			return ;

	data->du_ops->vblank_enable(kcrtc, false);
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

void kunlun_crtc_handle_vblank(struct kunlun_crtc *kcrtc)
{
	unsigned long flags;
	struct drm_crtc *crtc = &kcrtc->base;

	drm_crtc_handle_vblank(crtc);

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	if(kcrtc->event) {
		drm_crtc_send_vblank_event(crtc, kcrtc->event);
		drm_crtc_vblank_put(crtc);
		kcrtc->event = NULL;
	}
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}

void kunlun_crtc_planes_fini(struct kunlun_crtc *kcrtc)
{
	unsigned int size;

	kunlun_planes_fini(kcrtc);

	drm_crtc_cleanup(&kcrtc->base);

	size = kcrtc->num_planes * sizeof(struct kunlun_plane);
	memset(kcrtc->planes, 0, size);
	kcrtc->num_planes = 0;
}

int kunlun_crtc_planes_init(struct kunlun_crtc *kcrtc)
{
	const struct kunlun_crtc_data *data = kcrtc->data;
	struct drm_plane *primary = NULL, *cursor = NULL;
	struct kunlun_plane *plane;
	unsigned int size;
	int ret;
	int i;

	size = data->num_planes * sizeof(struct kunlun_plane);
	kcrtc->planes = devm_kzalloc(kcrtc->dev, size, GFP_KERNEL);
	if(!kcrtc->planes)
		return -ENOMEM;

	for(i = 0; i < data->num_planes; i++) {
		plane = &kcrtc->planes[i];
		plane->data = &data->planes[i];
		//plane->mlc_plane.base = ;
		//plane->mlc_plane.layer = data->mlc->mlc_layer;
		plane->kcrtc = kcrtc;
	}
	kcrtc->num_planes = data->num_planes;

	kunlun_planes_init(kcrtc, data);

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

	return 0;

err_crtc_cleanup:
	drm_crtc_cleanup(&kcrtc->base);
err_planes_fini:
	kunlun_planes_fini(kcrtc);
	memset(kcrtc->planes, 0, size);
	kcrtc->num_planes = 0;
	return ret;
}

int kunlun_crtc_get_resource(struct kunlun_crtc *kcrtc)
{
	struct device *dev = kcrtc->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct device_node *port;
	struct resource *res;
	int irq;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	kcrtc->regs = devm_ioremap_resource(dev, res);
	if(IS_ERR(kcrtc->regs)) {
		dev_err(dev, "Cannot find regs\n");
		return PTR_ERR(kcrtc->regs);
	}

	irq = platform_get_irq(pdev, 0);
	if(irq < 0) {
		dev_err(dev, "Cannot find irq for DC\n");
		return irq;
	}
	kcrtc->irq = (unsigned int)irq;

	spin_lock_init(&kcrtc->irq_lock);

	if(of_property_read_bool(dev->of_node,"vblank-disable"))
		kcrtc->vblank_enable = false;
	else
		kcrtc->vblank_enable = true;

	port = of_get_child_by_name(dev->of_node, "port");
	if (!port) {
		DRM_DEV_ERROR(dev, "no port node found in %pOF\n", dev->of_node);
		return -ENOENT;
	}
	kcrtc->base.port = port;

	return 0;
}

int kunlun_drm_crtc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct kunlun_crtc_data *crtc_data;

	crtc_data = of_device_get_match_data(dev);
	if(!crtc_data || !crtc_data->cmpt_ops)
		return -ENODEV;

	return component_add(dev, crtc_data->cmpt_ops);
}

int kunlun_drm_crtc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct kunlun_crtc_data *crtc_data;

	crtc_data = of_device_get_match_data(dev);
	if(!crtc_data || !crtc_data->cmpt_ops)
		return -ENODEV;

	component_del(dev, crtc_data->cmpt_ops);
	return 0;
}
