/*
* vmm_type_xen.c
*
* Copyright (c) 2018 Semidrive Semiconductor.
* All rights reserved.
*
* Description: System abstraction layer .
*
* Revision History:
* -----------------
* 01, 07/14/2019 Lili create this file
*/
#include "pvrsrv.h"
#include "img_types.h"
#include "pvrsrv_error.h"
#include "rgxheapconfig.h"

#include "vmm_impl.h"
#include "vmm_pvz_server.h"
#include "xengpu.h"
#include "xengpufront.h"
#include "sysconfig.h"

//#define SMMU_SUPPORTED 1

int xengpu_xenbus_init(void);
void xengpu_xenbus_fini(void);

static PVRSRV_ERROR
XenVMMDebugDumpConfig(IMG_UINT32 ui32FuncID,
						  IMG_UINT32 ui32DevID)
{
   PVR_UNREFERENCED_PARAMETER(ui32FuncID);
   PVR_UNREFERENCED_PARAMETER(ui32DevID);
   VMM_DEBUG_PRINT("enter %s, funcID(%d), DevID(%d)",
	   __FUNCTION__,
	   ui32FuncID,
	   ui32DevID);

#ifdef VMM_DEBUG
   /* call PvzServerDestroyDevPhysHeaps through hypervisor */
   struct gpuif_request request;
   struct gpuif_response *rsp=NULL;
   PVRSRV_ERROR ret = PVRSRV_OK;
   PVRSRV_DEVICE_CONFIG *pConfig = getDevConfig();
   XENGPU_REQUEST_INIT(&request);

   request.operation  = GPUIF_OP_DEBUG_DUMPCONFIG;
   request.ui32FuncID = ui32FuncID;
   request.ui32DevID  = ui32DevID;
   request.ui32OsID = pConfig->ui32OsId;
   ret = xengpu_do_request(&request, &rsp);
   if ((!ret) && (rsp != NULL)) {
	   ret					 = rsp->status;
   }
   kfree(rsp);
   return ret;
#else
   return PVRSRV_ERROR_NOT_IMPLEMENTED;
#endif
}

static PVRSRV_ERROR
XenVMMCreateDevConfig(IMG_UINT32 ui32FuncID,
					   IMG_UINT32 ui32DevID,
					   IMG_UINT32 *pui32IRQ,
					   IMG_UINT32 *pui32RegsSize,
					   IMG_UINT64 *pui64RegsCpuPBase)
{
    PVR_UNREFERENCED_PARAMETER(ui32FuncID);
    PVR_UNREFERENCED_PARAMETER(ui32DevID);
    PVR_UNREFERENCED_PARAMETER(pui32IRQ);
    PVR_UNREFERENCED_PARAMETER(pui32RegsSize);
    PVR_UNREFERENCED_PARAMETER(pui64RegsCpuPBase);
    /* provided in dts, so do nothing here */
    VMM_DEBUG_PRINT("%s, funcID(%d), DevID(%d) do nothing\n",
        __FUNCTION__,
        ui32FuncID,
        ui32DevID);
    return PVRSRV_ERROR_NOT_IMPLEMENTED;
}

static PVRSRV_ERROR
XenVMMDestroyDevConfig(IMG_UINT32 ui32FuncID,
						IMG_UINT32 ui32DevID)
{
    PVR_UNREFERENCED_PARAMETER(ui32FuncID);
    PVR_UNREFERENCED_PARAMETER(ui32DevID);
    /* provided in dts, so do nothing here */
    VMM_DEBUG_PRINT("%s, funcID(%d), DevID(%d) do nothing",
        __FUNCTION__,
        ui32FuncID,
        ui32DevID);
    return PVRSRV_ERROR_NOT_IMPLEMENTED;
}

static PVRSRV_ERROR
XenVMMCreateDevPhysHeaps(IMG_UINT32 ui32FuncID,
						  IMG_UINT32 ui32DevID,
						  IMG_UINT32 *peType,
						  IMG_UINT64 *pui64FwPhysHeapSize,
						  IMG_UINT64 *pui64FwPhysHeapAddr,
						  IMG_UINT64 *pui64GpuPhysHeapSize,
						  IMG_UINT64 *pui64GpuPhysHeapAddr)
{

	PVR_UNREFERENCED_PARAMETER(ui32FuncID);
	PVR_UNREFERENCED_PARAMETER(ui32DevID);
	PVR_UNREFERENCED_PARAMETER(peType);
	PVR_UNREFERENCED_PARAMETER(pui64FwPhysHeapSize);
	PVR_UNREFERENCED_PARAMETER(pui64FwPhysHeapAddr);
	PVR_UNREFERENCED_PARAMETER(pui64GpuPhysHeapSize);
	PVR_UNREFERENCED_PARAMETER(pui64GpuPhysHeapAddr);
    VMM_DEBUG_PRINT("enter %s, funcID(%d), DevID(%d)",
        __FUNCTION__,
        ui32FuncID,
        ui32DevID);
#ifdef SMMU_SUPPORTED
    /* call PvzServerCreateDevPhysHeaps through hypervisor */
    struct gpuif_request request;
    struct gpuif_response *rsp = NULL;
    PVRSRV_ERROR ret = PVRSRV_OK;
	PVRSRV_DEVICE_CONFIG *pConfig = getDevConfig();
	PHYS_HEAP_CONFIG *psPhysHeapConfig;
	struct device * pvrsrvdev = (struct device *)pConfig->pvOSDevice;

    XENGPU_REQUEST_INIT(&request);

    request.operation  = GPUIF_OP_CREATE_DEVPHYSHEAPS;
    request.ui32FuncID = ui32FuncID;
    request.ui32DevID  = ui32DevID;
	request.ui32OsID = pConfig->ui32OsId;
    ret = xengpu_do_request(&request, &rsp);
    if ((!ret) && (rsp != NULL)) {
        ret                   = rsp->status;
        *peType               = rsp->eType;
        *pui64FwPhysHeapSize  = rsp->ui64FwPhysHeapSize;
        *pui64FwPhysHeapAddr  = rsp->ui64FwPhysHeapAddr;
        *pui64GpuPhysHeapSize = rsp->ui64GpuPhysHeapSize;
        *pui64GpuPhysHeapAddr = rsp->ui64GpuPhysHeapAddr;
    }
	kfree(rsp);

#if defined(VMM_DEBUG)
	{
		int i=0;
		int32_t *addr = phys_to_virt(*pui64FwPhysHeapAddr);
		const struct dma_map_ops *dma_ops;

		for (i = 0; i< 100; i++)
		{
			VMM_DEBUG_PRINT("---- 0x%x ----", addr[i]);
		}

		memset(addr, 0xAB,100);

		dma_ops = get_dma_ops(pvrsrvdev);
		dma_ops->sync_single_for_device(pvrsrvdev, *pui64FwPhysHeapAddr, *pui64FwPhysHeapSize, DMA_TO_DEVICE);
		dma_ops->sync_single_for_cpu(pvrsrvdev, *pui64FwPhysHeapAddr, *pui64FwPhysHeapSize, DMA_FROM_DEVICE);
	}

	XenVMMDebugDumpConfig(ui32FuncID, ui32DevID);
#endif

	psPhysHeapConfig = SysVzGetPhysHeapConfig(pConfig, PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL);
	if (psPhysHeapConfig->pasRegions == NULL)
	{
		psPhysHeapConfig->pasRegions = OSAllocZMem(sizeof(PHYS_HEAP_REGION));

		PVR_ASSERT(! psPhysHeapConfig->bDynAlloc);
		psPhysHeapConfig->bDynAlloc = IMG_TRUE;
		psPhysHeapConfig->ui32NumOfRegions++;
	}

	if (psPhysHeapConfig->pasRegions[0].hPrivData == NULL)
	{
		psPhysHeapConfig->pasRegions[0].hPrivData = getDMAallocinfo();
	}
	return ret;
#else
    return PVRSRV_ERROR_NOT_IMPLEMENTED;
#endif
}

static PVRSRV_ERROR
XenVMMDestroyDevPhysHeaps(IMG_UINT32 ui32FuncID,
						   IMG_UINT32 ui32DevID)
{
	PVR_UNREFERENCED_PARAMETER(ui32FuncID);
	PVR_UNREFERENCED_PARAMETER(ui32DevID);
    VMM_DEBUG_PRINT("enter %s, funcID(%d), DevID(%d)",
        __FUNCTION__,
        ui32FuncID,
        ui32DevID);

#ifdef SMMU_SUPPORTED
    /* call PvzServerDestroyDevPhysHeaps through hypervisor */
    struct gpuif_request request;
    struct gpuif_response *rsp=NULL;
    PVRSRV_ERROR ret = PVRSRV_OK;
	PVRSRV_DEVICE_CONFIG *pConfig = getDevConfig();
	PHYS_HEAP_CONFIG *psPhysHeapConfig;

    XENGPU_REQUEST_INIT(&request);
	request.ui32OsID = pConfig->ui32OsId;
    request.operation  = GPUIF_OP_DESTROY_DEVPHYSHEAPS;
    request.ui32FuncID = ui32FuncID;
    request.ui32DevID  = ui32DevID;
    ret = xengpu_do_request(&request, &rsp);
    if ((!ret) && (rsp != NULL)) {
        ret                   = rsp->status;
	}
	kfree(rsp);

	psPhysHeapConfig = SysVzGetPhysHeapConfig(pConfig, PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL);
	if (psPhysHeapConfig->pasRegions != NULL)
	{
		OSFreeMem(psPhysHeapConfig->pasRegions);
		psPhysHeapConfig->bDynAlloc = IMG_FALSE;
		psPhysHeapConfig->ui32NumOfRegions--;
	}

	return ret;
#else
    return PVRSRV_ERROR_NOT_IMPLEMENTED;
#endif
}

static PVRSRV_ERROR
XenVMMMapDevPhysHeap(IMG_UINT32 ui32FuncID,
					  IMG_UINT32 ui32DevID,
					  IMG_UINT64 ui64Size,
					  IMG_UINT64 ui64Addr)
{
	int i;

	PVR_UNREFERENCED_PARAMETER(i);
	PVR_UNREFERENCED_PARAMETER(ui32FuncID);
	PVR_UNREFERENCED_PARAMETER(ui32DevID);
	PVR_UNREFERENCED_PARAMETER(ui64Size);
	PVR_UNREFERENCED_PARAMETER(ui64Addr);
	VMM_DEBUG_PRINT("enter %s, funcID(%d), DevID(%d), ui64Size(%d), ui64Addr(0x%x)",
			__FUNCTION__,
			ui32FuncID,
			ui32DevID,
			ui64Size,
			ui64Addr);
#ifndef SMMU_SUPPORTED
	/* call PvzServerMapDevPhysHeap through hypervisor */
	int ret;

#if defined(VMM_DEBUG)
	PVRSRV_DEVICE_CONFIG *pConfig = getDevConfig();
	struct device * pvrsrvdev = (struct device *)pConfig->pvOSDevice;
	const struct dma_map_ops *dma_ops;
	PHYS_HEAP_CONFIG * psPhysHeapConfig = SysVzGetPhysHeapConfig(pConfig,PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL);
	DMA_ALLOC *psDmaAlloc = (DMA_ALLOC *)psPhysHeapConfig->pasRegions[0].hPrivData;
	IMG_UINT64 *start = psDmaAlloc->pvVirtAddr;
	memset(start, 0xAB,100);
	dma_ops = get_dma_ops(pvrsrvdev);
	dma_ops->sync_single_for_device(pvrsrvdev, ui64Addr, 100, DMA_BIDIRECTIONAL );
	dma_ops->sync_single_for_cpu(pvrsrvdev, ui64Addr, 100, DMA_BIDIRECTIONAL );
	VMM_DEBUG_PRINT("#########set 0x%x to virt 0x%p############", *start, start);
#endif

	ret = gsx_front_map(ui32FuncID, ui32DevID, pConfig->ui32OsId, ui64Size, ui64Addr);
	if (!ret) {
		VMM_DEBUG_PRINT("%s:%d, ret=%d", __FUNCTION__, __LINE__, ret);
	}

#if defined(VMM_DEBUG)
	VMM_DEBUG_PRINT("#########return to guest begin############", *start, start);
	for (i = 0; i< 100; i++)
	{
		VMM_DEBUG_PRINT("---- 0x%x ----", start[i]);
	}
	VMM_DEBUG_PRINT("#########finish guest############", *start, start);
#endif

	return ret;
#else
	return PVRSRV_ERROR_NOT_IMPLEMENTED;
#endif
}

static PVRSRV_ERROR
XenVMMUnmapDevPhysHeap(IMG_UINT32 ui32FuncID,
						IMG_UINT32 ui32DevID)
{
	PVR_UNREFERENCED_PARAMETER(ui32FuncID);
	PVR_UNREFERENCED_PARAMETER(ui32DevID);
	VMM_DEBUG_PRINT("enter %s, funcID(%d), DevID(%d)",
		__FUNCTION__,
		ui32FuncID,
		ui32DevID);
#ifndef SMMU_SUPPORTED
	/* call PvzServerUnmapDevPhysHeap through hypervisor */
	struct gpuif_request request;
	struct gpuif_response *rsp = NULL;
	PVRSRV_DEVICE_CONFIG *pConfig = getDevConfig();

	PVRSRV_ERROR ret = PVRSRV_OK;

	ret = gsx_front_unmap(ui32FuncID, ui32DevID, pConfig->ui32OsId);
	if (!ret) {
		VMM_DEBUG_PRINT("%s:%d, ret=%d", __FUNCTION__, __LINE__, ret);
	}

	return ret;
#else
    return PVRSRV_ERROR_NOT_IMPLEMENTED;
#endif
}

static PVRSRV_ERROR
XenVMMGetDevPhysHeapOrigin(PVRSRV_DEVICE_CONFIG *psDevConfig,
							PVRSRV_DEVICE_PHYS_HEAP eHeapType,
							PVRSRV_DEVICE_PHYS_HEAP_ORIGIN *peOrigin)
{
    PVR_UNREFERENCED_PARAMETER(psDevConfig);
    PVR_UNREFERENCED_PARAMETER(eHeapType);
    VMM_DEBUG_PRINT("enter %s, psDevConfig(%p), eHeapType(%d)",
        __FUNCTION__,
        psDevConfig,
        eHeapType);

#ifdef SMMU_SUPPORTED
    if (eHeapType == PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL) {
        *peOrigin = PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_HOST;
    } else {
        *peOrigin = PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_GUEST;
    }
#else
    *peOrigin = PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_GUEST;
#endif

	return PVRSRV_OK;
}

static PVRSRV_ERROR
XenVMMGetDevPhysHeapAddrSize(PVRSRV_DEVICE_CONFIG *psDevConfig,
							PVRSRV_DEVICE_PHYS_HEAP eHeapType,
							IMG_UINT64 *pui64Size,
							IMG_UINT64 *pui64Addr)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	*pui64Size = 0;
	*pui64Addr = 0;
	PVR_UNREFERENCED_PARAMETER(psDevConfig);
	PVR_UNREFERENCED_PARAMETER(eHeapType);
	VMM_DEBUG_PRINT("enter %s, psDevConfig(%p), eHeapType(%d)",
		__FUNCTION__,
		psDevConfig,
		eHeapType);

#ifdef SMMU_SUPPORTED
	switch (eHeapType)
	{
		case PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL:
		{
			/* host allocate this heap*/
			/*should return heap Addr/Size value pairs obtained
			 in sHostFuncTab->pfnCreateDevPhysHeaps().*/
    		if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
			{
				PHYS_HEAP_CONFIG *psPhysHeapConfig;

				psPhysHeapConfig = SysVzGetPhysHeapConfig(psDevConfig, eHeapType);
				if (psPhysHeapConfig->pasRegions != NULL) {
					*pui64Addr = psPhysHeapConfig->pasRegions[0].sStartAddr.uiAddr;
					*pui64Size = psPhysHeapConfig->pasRegions[0].uiSize;
				} else {
					*pui64Size = RGX_FIRMWARE_RAW_HEAP_SIZE;
				}
			} else {
				*pui64Size = RGX_FIRMWARE_RAW_HEAP_SIZE;
			}
			break;
		}
		case PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL:
			/* for gpu heap, guest allocate this heap and uma is using, return 0/0*/
			break;
		default:
			eError = PVRSRV_ERROR_NOT_IMPLEMENTED;
			PVR_ASSERT(0);
			break;
	}
#else
	switch (eHeapType)
	{
		case PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL:
			/* host allocate this heap and dma is using*/
			*pui64Size = RGX_FIRMWARE_RAW_HEAP_SIZE;
			break;
		case PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL:
			/* for gpu heap, guest allocate this heap and uma is using, return 0/0*/
			break;
		default:
			eError = PVRSRV_ERROR_NOT_IMPLEMENTED;
			PVR_ASSERT(0);
			break;
	}

#endif
	VMM_DEBUG_PRINT("out %s, *pui64Addr(%p), *pui64Size(%d)",
		__FUNCTION__,
		*pui64Addr,
		*pui64Size);
	return eError;
}

static VMM_PVZ_CONNECTION gsXenVmmPvz =
{
	.sHostFuncTab = {
		/* pfnCreateDevConfig */
		&XenVMMCreateDevConfig,

		/* pfnDestroyDevConfig */
		&XenVMMDestroyDevConfig,

		/* pfnCreateDevPhysHeaps */
		&XenVMMCreateDevPhysHeaps,

		/* pfnDestroyDevPhysHeaps */
		&XenVMMDestroyDevPhysHeaps,

		/* pfnMapDevPhysHeap */
		&XenVMMMapDevPhysHeap,

		/* pfnUnmapDevPhysHeap */
		&XenVMMUnmapDevPhysHeap,

		&XenVMMDebugDumpConfig
	},

	.sGuestFuncTab = {
		/* pfnCreateDevConfig */
		&PvzServerCreateDevConfig,

		/* pfnDestroyDevConfig */
		&PvzServerDestroyDevConfig,

		/* pfnCreateDevPhysHeaps */
		&PvzServerCreateDevPhysHeaps,

		/* pfnDestroyDevPhysHeaps */
		&PvzServerDestroyDevPhysHeaps,

		/* pfnMapDevPhysHeap */
		&PvzServerMapDevPhysHeap,

		/* pfnUnmapDevPhysHeap */
		&PvzServerUnmapDevPhysHeap
	},

	.sConfigFuncTab = {
		/* pfnGetDevPhysHeapOrigin */
		&XenVMMGetDevPhysHeapOrigin,

		/* pfnGetDevPhysHeapAddrSize */
		&XenVMMGetDevPhysHeapAddrSize
	},

	.sVmmFuncTab = {
		/* pfnOnVmOnline */
		&PvzServerOnVmOnline,

		/* pfnOnVmOffline */
		&PvzServerOnVmOffline,

		/* pfnVMMConfigure */
		&PvzServerVMMConfigure
	}
};


/* this function is running on host */
void XenHostDestroyConnection(IMG_UINT32 uiOSID)
{
    VMM_DEBUG_PRINT("enter %s, uiOSID(%d)",
        __FUNCTION__,
        uiOSID);

    if (PVRSRV_OK != gsXenVmmPvz.sVmmFuncTab.pfnOnVmOffline(uiOSID))
    {
        PVR_DPF((PVR_DBG_ERROR, "OnVmOffline failed for OSID%d", uiOSID));
    }
    else
    {
        PVR_LOG(("Guest OSID%d driver is offline", uiOSID));
    }

}
EXPORT_SYMBOL(XenHostDestroyConnection);

/* this function is running on host */
PVRSRV_ERROR XenHostCreateConnection(IMG_UINT32 uiOSID)
{
    VMM_DEBUG_PRINT("enter %s, uiOSID(%d)",
        __FUNCTION__,
        uiOSID);

	if (PVRSRV_OK != gsXenVmmPvz.sVmmFuncTab.pfnOnVmOnline(uiOSID, 0))
	{
		PVR_DPF((PVR_DBG_ERROR, "OnVmOnline failed for OSID%d", uiOSID));
	}
	else
	{
		PVR_LOG(("Guest OSID%d driver is online", uiOSID));
	}

	return PVRSRV_OK;
}
EXPORT_SYMBOL(XenHostCreateConnection);

/* this function is running on guest */
static PVRSRV_ERROR XenCreatePvzConnection(void)
{
	IMG_UINT32 uiOSID;
	IMG_BOOL bRequireOsidOnline = IMG_FALSE;
	PVRSRV_ERROR ret = PVRSRV_OK;

	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		struct gpuif_request request;
		struct gpuif_response *rsp = NULL;
		PVRSRV_DEVICE_CONFIG *pConfig = getDevConfig();
		XENGPU_REQUEST_INIT(&request);

		ret = xengpu_init();
                if (ret != PVRSRV_OK) {
                        pr_err("%s:%d,xengpu_init return 0x%x", __FUNCTION__, __LINE__, ret);
                        return ret;
                }
		request.operation = GPUIF_OP_CREATE_PVZCONNECTION;
		request.ui32OsID = pConfig->ui32OsId;
		ret = xengpu_do_request(&request, &rsp);
		if (rsp != NULL) {
			ret = rsp->status;
			kfree(rsp);
		}
	} else {
		xengpu_xenbus_init();
	}

	return ret;
}

/* this function is running on guest */
static void XenDestroyPvzConnection(void)
{
	PVRSRV_ERROR ret = PVRSRV_OK;
	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		struct gpuif_request request;
		struct gpuif_response *rsp = NULL;
		PVRSRV_DEVICE_CONFIG *pConfig = getDevConfig();
		XENGPU_REQUEST_INIT(&request);

		request.operation = GPUIF_OP_DESTROY_PVZCONNECTION;
		request.ui32OsID = pConfig->ui32OsId;
		ret = xengpu_do_request(&request, &rsp);
		if (rsp != NULL) {
			ret = rsp->status;
			kfree(rsp);
		}
		xengpu_exit();
	} else {
		xengpu_xenbus_fini();
	}
}

PVRSRV_ERROR VMMCreatePvzConnection(VMM_PVZ_CONNECTION **psPvzConnection)
{
	PVR_LOGR_IF_FALSE((NULL != psPvzConnection), "VMMCreatePvzConnection", PVRSRV_ERROR_INVALID_PARAMS);
    VMM_DEBUG_PRINT("enter %s", __FUNCTION__);
    *psPvzConnection = &gsXenVmmPvz;
    return XenCreatePvzConnection();
}

void VMMDestroyPvzConnection(VMM_PVZ_CONNECTION *psPvzConnection)
{
	PVR_LOG_IF_FALSE((NULL != psPvzConnection), "VMMDestroyPvzConnection");
    VMM_DEBUG_PRINT("enter %s", __FUNCTION__);
    return XenDestroyPvzConnection();
}

/******************************************************************************
 End of file (vmm_type_xen.c)
******************************************************************************/

