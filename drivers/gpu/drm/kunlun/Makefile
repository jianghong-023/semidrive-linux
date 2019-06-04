# SPDX-License-Identifier: GPL-2.0
kunlun-drm-objs := \
	kunlun_drm_drv.o \
	kunlun_drm_fb.o \
	kunlun_drm_gem.o

kunlun-drm-$(CONFIG_DRM_FBDEV_EMULATION) += kunlun_drm_fbdev.o

obj-$(CONFIG_DRM_KUNLUN_RGB) += kunlun_drm_parallel.o

obj-$(CONFIG_DRM_KUNLUN) += kunlun-drm.o

kunlun-drm-crtc-objs := \
	kunlun_drm_crtc.o \
	kunlun_drm_plane.o

kunlun-drm-crtc-$(CONFIG_DRM_KUNLUN_DC) += kunlun_drm_dc_crtc.o

obj-$(CONFIG_DRM_KUNLUN_CRTC) += kunlun-drm-crtc.o
