# SPDX-License-Identifier: GPL-2.0
ccflags-y += -g -O1 -DKO_VERSION=\"$(shell date +%Y-%m-%d-%H:%M:%S)\"
ccflags-y += -I$(obj) -I$(obj)/semidrive_linked
LINKED_DIR=$(KBUILD_SRC)/$(src)/semidrive_linked

sdrv-drm-objs := \
  drm_drv.o \
  drm_fb.o \
  drm_gem.o \
  drm_memblock.o

dsi-objs := \
	dsi/dwc_mipi_dsi_host.o \
	dsi/sdrv-mipi-dsi.o

test = $(shell if [ -d "$(LINKED_DIR)" ]; then echo "exist"; else echo "noexist"; fi)

ifeq ("$(test)", "noexist")
$(info  "semidrm noexist $(LINKED_DIR)")
	ifeq ($(CONFIG_FTRACE),y)
		obj-$(CONFIG_DRM_SEMIDRIVE) += semidrive_linked_eng.o
	else
		obj-$(CONFIG_DRM_SEMIDRIVE) += semidrive_linked_usr.o
	endif
else
$(info  "semidrm exist $(LINKED_DIR)")
obj-m += semidrive_linked.o

semidrive_linked-objs := \
	semidrive_linked/dp_r0p1.o \
	semidrive_linked/dc_r0p1.o \
	semidrive_linked/planes/gpipe_dp.o \
	semidrive_linked/planes/gpipe_dp_s.o \
	semidrive_linked/planes/gpipe_dc.o \
	semidrive_linked/planes/spipe.o \
	semidrive_linked/planes/apipe.o \
	semidrive_linked/parallel_interface.o \
	semidrive_linked/lvds_interface.o

drm-crtc-objs:= \
	semidrive_linked/dp_r0p1.o \
	semidrive_linked/dc_r0p1.o \
	semidrive_linked/planes/gpipe_dp.o \
	semidrive_linked/planes/gpipe_dp_s.o \
	semidrive_linked/planes/gpipe_dc.o \
	semidrive_linked/planes/spipe.o \
	semidrive_linked/planes/apipe.o \
	semidrive_linked/parallel_interface.o \
	semidrive_linked/lvds_interface.o
endif

drm-crtc-objs += \
  sdrv_dpc.o \
  sdrv_plane.o \
  sdrv_rpcall.o \
  dp_dummy.o \
  dp_op.o \
  dp_rpcall.o

obj-$(CONFIG_DRM_SEMIDRIVE) += drm-crtc.o drm_fbdev.o
obj-$(CONFIG_DRM_SEMIDRIVE) += drm_connector.o
obj-$(CONFIG_DRM_SEMIDRIVE) += sdrv-drm.o
obj-$(CONFIG_DRM_SEMIDRIVE) += dsi.o



