#below configs come from .config
override PVR_SYSTEM := kl_gm9446
override PVR_SYSTEM_CLEAN := 9446
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

ifeq ($(CONFIG_BUFFER_SYNC),y)
override SUPPORT_BUFFER_SYNC := 1
endif

override PVR_BUILD_TYPE := release
override BUILD := release

override KERNEL_COMPONENTS := pvrsrvkm_9446
override SUPPORT_GPUTRACE_EVENTS := 1
override SUPPORT_ION := 1

override HOST_PRIMARY_ARCH := host_x86_64
override HOST_32BIT_ARCH := host_i386
override HOST_FORCE_32BIT := -m32
override TARGET_PRIMARY_ARCH := target_aarch64
override TARGET_SECONDARY_ARCH :=
override TARGET_ALL_ARCH := target_aarch64
override TARGET_FORCE_32BIT :=
override METAG_VERSION_NEEDED := 2.8.1.0.3
override MIPS_VERSION_NEEDED := 2014.07-1
override KERNEL_COMPONENTS := srvkm
override PVRSRV_MODNAME := pvrsrvkm_9226
override PVRSYNC_MODNAME := pvr_sync
override PVR_BUILD_TYPE := release
override SUPPORT_RGX := 1
override PVR_SYSTEM := kl_gm9226
override PVR_LOADER :=
override BUILD := release
override DEBUGLINK := 1
override SUPPORT_PHYSMEM_TEST := 1
override COMPRESS_DEBUG_SECTIONS := 1
override VMM_TYPE := stub
override undefine PDUMP
override OPTIM := -O2
override RGX_TIMECORR_CLOCK := sched
override PDVFS_COM_HOST := 1
override PDVFS_COM_AP := 2
override PDVFS_COM_PMC := 3
override PDVFS_COM := PDVFS_COM_HOST
override PVR_GPIO_MODE_GENERAL := 1
override PVR_GPIO_MODE_POWMON_PIN := 2
override PVR_GPIO_MODE := PVR_GPIO_MODE_GENERAL
override PVR_HANDLE_BACKEND := idr
override SUPPORT_DMABUF_BRIDGE := 1
override undefine SUPPORT_SERVER_SYNC
override SUPPORT_SERVER_SYNC_IMPL := 1
override SUPPORT_NATIVE_FENCE_SYNC := 1
override PVR_USE_FENCE_SYNC_MODEL := 1
override SUPPORT_DMA_FENCE := 1
override SUPPORT_ANDROID_PLATFORM := 1
override SUPPORT_ION := 1
override RK_INIT_V2 := 1
