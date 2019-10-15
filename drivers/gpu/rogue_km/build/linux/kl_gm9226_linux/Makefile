########################################################################### ###
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Strictly Confidential.
### ###########################################################################

# If a TARGET_PRODUCT is specified but not a TARGET_DEVICE, try to
# derive the TARGET_DEVICE from TARGET_PRODUCT.
#
ifeq ($(TARGET_DEVICE),)
override TARGET_DEVICE := $(TARGET_PRODUCT)
endif

## to do !!!!need confirm this
#export CLANG_PATH=$(ANDROID_ROOT)/prebuilts/clang/host/linux-x86/clang-4691093/bin/
export CLANG=$(CLANG_PATH)/clang
export CLANG_CXX=$(CLANG_PATH)/clang++
#export CROSS_COMPILE=aarch64-linux-android-
export ARCH=arm64
export MIPS_ELF_ROOT=/tool/mips-mti-elf/mips-mti-elf/2014.07-1
#export PLATFORM_RELEASE=Pie
#export CROSS_COMPILE_SECONDARY=arm-linux-androideabi-
#export MULTIARCH=1

export GLSLC=$(ANDROID_ROOT)/prebuilts/shader-tools/linux-x86_64/glslc
export EXCLUDED_APIS=camerahal cldnn dnn nnhal composerhal memtrackhal openrl renderscript sensorhal hwperftools vulkan opencl
## end

include ../config/preconfig.mk
include ../config/window_system.mk

PVR_SYSTEM := kl_gm9226



KERNEL_COMPONENTS := srvkm

 # Heap names should be defined as per the labels used in the kernel.
 # Kernels >=4.12, support heap query so we use heap names 
 # to get the correct heap id.
 # For kernels <4.12, we use heap_id_mask defined in this Makefile.
 # Heap names defined here are for reference only.
ION_DEFAULT_HEAP_NAME := \"system\"
ION_DEFAULT_HEAP_ID_MASK := (1 << ION_HEAP_TYPE_SYSTEM)
ION_FALLBACK_HEAP_NAME := \"carveout\"
ION_FALLBACK_HEAP_ID_MASK := (1 << ION_HEAP_TYPE_CARVEOUT)

ifeq ($(SUPPORT_KMS),1)
 PVR_DRM_MODESET_DRIVER_NAME := semidrive
 PVR_DRM_MODESET_MODULE_NAME := dumb
# DISPLAY_CONTROLLER := drm_nulldisp
else
 ifeq ($(SUPPORT_DISPLAY_CLASS),1)
  DISPLAY_CONTROLLER := dc_semi
 endif
endif

KERNEL_COMPONENTS += $(DISPLAY_CONTROLLER)

PDUMP_STREAMBUF_SIZE_MB := 10

DEVICE_MEMSETCPY_ALIGN_IN_BYTES := 16

PVR_LDM_PLATFORM_PRE_REGISTERED := 1

RGX_TIMECORR_CLOCK := sched
CACHEFLUSH_NO_KMRBF_USING_UMVA ?= 1
include ../config/core.mk
-include ../common/lws.mk
include ../common/3rdparty.mk
