# SPDX-License-Identifier: GPL-2.0
ccflags-y += -g -O1 -DKO_VERSION=\"$(shell date +%Y-%m-%d-%H:%M:%S)\"
ccflags-y += -I$(obj)

kunlun-drm-objs := \
  kunlun_drm_drv.o \
  kunlun_drm_fb.o \
  kunlun_drm_gem.o \
  kunlun_drm_memblock.o

sdrvdrm-y := \
	dsi/dwc_mipi_dsi_host.o \
	dsi/sdrv-mipi-dsi.o

kunlun-drm-crtc-objs := \
  sdrv_dpc.o \
  sdrv_plane.o \
  sdrv_rpcall.o \
  dp_dummy.o \
  dp_op.o \
  dp_rpcall.o

obj-y += semidrive_linked/
obj-y += kunlun-drm-crtc.o kunlun_drm_fbdev.o
obj-y += kunlun_drm_parallel.o
obj-y += kunlun-drm.o
obj-y += sdrvdrm.o
