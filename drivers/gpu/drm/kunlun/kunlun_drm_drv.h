/*
 * kunlun_drm_drv.h
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef _KUNLUN_DRM_DRV_H_
#define _KUNLUN_DRM_DRV_H_
#include <drm/drm_fb_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_gem.h>

#include <linux/module.h>
#include <linux/component.h>


#define MAX_CRTC			8

struct kunlun_drm_device_info {
	int (*match)(struct device_node *np);
};

struct kunlun_drm_data {
	struct drm_device *drm;

	struct drm_fb_helper *fb_helper;

	bool iommu_enable;
	struct iommu_domain *domain;
	struct drm_mm *mm;
	struct mutex mm_lock;

	unsigned int num_crtcs;
	struct drm_crtc *crtcs[MAX_CRTC];
};

#endif
