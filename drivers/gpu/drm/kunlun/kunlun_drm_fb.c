/*
 * kunlun_drm_fb.c
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
#include <drm/drm_gem_framebuffer_helper.h>

#include "kunlun_drm_drv.h"
#include "kunlun_drm_gem.h"
#include "kunlun_drm_fb.h"
#include "kunlun_drm_fbdev.h"

static void kunlun_gem_fb_destroy(struct drm_framebuffer *fb)
{
	int i;
	struct kunlun_gem_object *kg_obj;
	struct drm_gem_object *obj;

	for(i = 0; i < 4; i++) {
		obj = fb->obj[i];
		if(!obj)
			continue;

		kg_obj = to_kunlun_obj(obj);

		if(kg_obj->pages && kg_obj->vaddr)
			vunmap(kg_obj->vaddr);

		drm_gem_object_put_unlocked(obj);
	}

err_fb_cleanup:
	drm_framebuffer_cleanup(fb);
	kfree(fb);
}

const struct drm_framebuffer_funcs kunlun_drm_fb_funcs = {
	.destroy = kunlun_gem_fb_destroy,
	.create_handle = drm_gem_fb_create_handle,
};

static struct drm_framebuffer *
kunlun_gem_fb_alloc(struct drm_device *drm,
		const struct drm_mode_fb_cmd2 *cmd,
		struct drm_gem_object **obj, unsigned int num_planes)
{
	struct drm_framebuffer *fb;
	int ret, i;

	fb = kzalloc(sizeof(*fb), GFP_KERNEL);
	if(!fb)
		return ERR_PTR(-ENOMEM);

	drm_helper_mode_fill_fb_struct(drm, fb, cmd);

	for(i = 0; i < num_planes; i++)
		fb->obj[i] = obj[i];

	ret = drm_framebuffer_init(drm, fb, &kunlun_drm_fb_funcs);
	if(ret) {
		DRM_DEV_ERROR(drm->dev, "Failed to init framebuffer: %d\n",
				ret);
		kfree(fb);
		return ERR_PTR(ret);
	}

	return fb;
}

struct drm_framebuffer *kunlun_drm_fbdev_fb_alloc(struct drm_device *drm,
		struct drm_fb_helper_surface_size *sizes,
		unsigned int pitch_align, struct drm_gem_object *obj)
{
	return drm_gem_fbdev_fb_create(drm, sizes, pitch_align, obj,
			&kunlun_drm_fb_funcs);
}
