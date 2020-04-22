#below configs come from .config
override PVR_SYSTEM := kl_gm9226
override PVR_SYSTEM_CLEAN := 9226
ifeq ($(CONFIG_VMM_TYPE),"stub")
override VMM_TYPE := stub
else
override VMM_TYPE := xen
endif
ifneq ($(CONFIG_PVRSRV_VZ_NUM_OSID),)
override PVRSRV_VZ_NUM_OSID := $(CONFIG_PVRSRV_VZ_NUM_OSID)
else
override PVRSRV_VZ_NUM_OSID := 0
endif

ifneq ($(CONFIG_ANDROID),)
override PVR_BUILD_DIR := $(PVR_SYSTEM)_android
override TARGET_SECONDARY_ARCH := target_armv7-a
override TARGET_ALL_ARCH :=  target_aarch64 target_armv7-a
override PVRSRV_MODULE_BASEDIR := /vendor/modules/
override KERNEL_CROSS_COMPILE := aarch64-linux-android-
override DEBUGLINK := 1
override COMPRESS_DEBUG_SECTIONS := 1
override OPTIM := -O2
override undefine SUPPORT_SERVER_SYNC
override SUPPORT_ANDROID_PLATFORM := 1
override RK_INIT_V2 := 1
override KERNELDIR=$(ANDROID_ROOT)/kernel
override ANDROID := 1
else
override PVR_BUILD_DIR := $(PVR_SYSTEM)_linux
override TARGET_SECONDARY_ARCH :=
override TARGET_ALL_ARCH :=  target_aarch64
override KERNELDIR := $(PWD)/../../../
override KERNEL_ID := 4.14.61
override PVRSRV_MODULE_BASEDIR := /lib/modules/4.14.61/extra/
override KERNEL_CROSS_COMPILE := aarch64-linux-gnu-
override undefine SUPPORT_DISPLAY_CLASS
override SUPPORT_BUFFER_SYNC := 1
override undefine ANDROID
endif

ifeq ($(CONFIG_BUFFER_SYNC),y)
override SUPPORT_BUFFER_SYNC := 1
endif

ifeq ($(CONFIG_PVR_KMD_DEBUG),y)
override PVR_BUILD_TYPE := debug
override BUILD := debug
else
override PVR_BUILD_TYPE := release
override BUILD := release
endif

override KERNEL_COMPONENTS := pvrsrvkm_9226
override SUPPORT_GPUTRACE_EVENTS := 1
override SUPPORT_ION := 1
override HOST_PRIMARY_ARCH := host_x86_64
override HOST_32BIT_ARCH := host_i386
override HOST_FORCE_32BIT := -m32
override TARGET_PRIMARY_ARCH := target_aarch64

override TARGET_FORCE_32BIT :=
override TARGET_OS :=
override METAG_VERSION_NEEDED := 2.8.1.0.3
override MIPS_VERSION_NEEDED := 2014.07-1

override PVRSRV_MODNAME := pvrsrvkm_9226
override PVRSYNC_MODNAME := pvr_sync
override SUPPORT_RGX := 1
override PVR_LOADER :=
override PVR_SERVICES_DEBUG := 1
override undefine PDUMP
override RGX_TIMECORR_CLOCK := sched
override PDVFS_COM_HOST := 1
override PDVFS_COM_AP := 2
override PDVFS_COM_PMC := 3
override PDVFS_COM := PDVFS_COM_HOST
override PVR_GPIO_MODE_GENERAL := 1
override PVR_GPIO_MODE_POWMON_PIN := 2
override PVR_GPIO_MODE_POWMON_WO_PIN := 3
override PVR_GPIO_MODE := PVR_GPIO_MODE_GENERAL
override PVR_HANDLE_BACKEND := idr
override SUPPORT_DEVICEMEMHISTORY_BRIDGE := 1
override SUPPORT_SYNCTRACKING_BRIDGE := 1
override PVR_RI_DEBUG := 1
override SUPPORT_PAGE_FAULT_DEBUG := 1
override SUPPORT_NATIVE_FENCE_SYNC := 1
override PVR_USE_FENCE_SYNC_MODEL := 1
override SUPPORT_DMA_FENCE := 1

