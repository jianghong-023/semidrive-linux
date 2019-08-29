/*************************************************************************/ /*!
@File           vmm_type_l4linux.c
@Title          Fiasco.OC L4LINUX VM manager type
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Fiasco.OC L4LINUX VM manager implementation
@License        Strictly Confidential.
*/ /**************************************************************************/
#if defined (LINUX)
#include <linux/module.h>
#endif

#include "pvrsrv.h"
#include "img_types.h"
#include "pvrsrv_error.h"
#include "rgxheapconfig.h"
#include "interrupt_support.h"

#include "vmm_impl.h"
#include "vz_physheap.h"
#include "vmm_pvz_server.h"
#include "vz_vm.h"

#if defined(CONFIG_L4)
#include <l4/sys/err.h>
#include <l4/sys/irq.h>
#include <l4/re/env.h>
#else
/* Suppress build failures for non-l4linux builds */
#define l4_cap_idx_t			int
#define l4re_env_get_cap(x)		0
#define l4_is_invalid_cap(x)	0
#define l4x_register_irq(x)		0
#define l4x_unregister_irq(x)	PVR_UNREFERENCED_PARAMETER(x)
#define l4_irq_trigger(x)		PVR_UNREFERENCED_PARAMETER(x)
#endif

/* Valid values for the TC_MEMORY_CONFIG configuration option */
#define TC_MEMORY_LOCAL			(1)
#define TC_MEMORY_HOST			(2)
#define TC_MEMORY_HYBRID		(3)

/*
	Fiasco.OC/L4Linux setup:
		- Defaults to static PVZ system setup
		- Use identical GPU/FW heap sizes for HOST/GUEST
			- 0MB <=(FW/HEAP)=> xMB <=(GPU/HEAP)=> yMB
				- xMB - 0MB = All OSID firmware heaps
				- yMB - xMB = All OSID graphics heaps

	Supported platforms:
		- ARM32 UMA platform device
			- Platform memory map:	[StartOfRam<=(CPU/RAM)=>512MB<=(GPU/RAM)=>1GB]
				- ARM32-VEXPRESS:   StartOfRam offset @ 0x60000000

		- x86 LMA PCI device
			- Local memory map:		[0<=(GPU/GRAM)=>1GB]
*/
#ifndef TC_MEMORY_CONFIG
	#if defined (CONFIG_ARM) && defined(CONFIG_ARCH_VEXPRESS)
		/* Default to 768 MiB offset @ 0x9000_0000, size 256 MiB */
		static IMG_UINT32 gui32SysL4lxDevMemPhysHeapBase = 0x90000000;
		static IMG_UINT32 gui32SysL4lxDevMemPhysHeapSize = 0x10000000;
	#else
		/* These should be specified during driver module load */
		static IMG_UINT32 gui32SysL4lxDevMemPhysHeapBase = 0x0;
		static IMG_UINT32 gui32SysL4lxDevMemPhysHeapSize = 0x0;
	#endif
#else
	/* Start of LMA card address */
	static IMG_UINT32 gui32SysL4lxDevMemPhysHeapBase = 0x0;
	static IMG_UINT32 gui32SysL4lxDevMemPhysHeapSize = 0x40000000;
	#if (TC_MEMORY_CONFIG != TC_MEMORY_LOCAL)
		#error "L4linux supports TC_MEMORY_CONFIG => TC_MEMORY_LOCAL only"
	#endif
#endif

/* If specified it override GPU devmem. auto-partitioning */
static IMG_UINT32 gui32SysL4lxDevMemFwPhysHeapBase = 0x0;

/* VZ bootstraping methods, defaults to host-origin (i.e. static) PVZ setup
   else it simulates a guest-origin (i.e. quasi-dynamic) PVZ setup */
static IMG_UINT32 gui32SysL4lxDevMemFwPhysHeapOrigin = 0x0;

#if defined (LINUX)
module_param_named(sys_l4lx_devmem_fw_physheap_origin,  gui32SysL4lxDevMemFwPhysHeapOrigin, uint, S_IRUGO | S_IWUSR);
module_param_named(sys_l4lx_devmem_fw_physheap_base,    gui32SysL4lxDevMemFwPhysHeapBase,   uint, S_IRUGO | S_IWUSR);
module_param_named(sys_l4lx_devmem_physheap_base,       gui32SysL4lxDevMemPhysHeapBase,     uint, S_IRUGO | S_IWUSR);
module_param_named(sys_l4lx_devmem_physheap_size,       gui32SysL4lxDevMemPhysHeapSize,     uint, S_IRUGO | S_IWUSR);
#endif

/* Driver auto-partitions GPU/FW devmem. space (i.e. XXXPhysHeap{Base,Size}) for all OSIDs unless XXXFwPhysHeapBase is specified */
#define FWHEAP_BASE		 (gui32SysL4lxDevMemFwPhysHeapBase ? gui32SysL4lxDevMemFwPhysHeapBase : gui32SysL4lxDevMemPhysHeapBase)
#define VZ_FWHEAP_SIZE	 (RGX_FIRMWARE_RAW_HEAP_SIZE)
#define VZ_GPUHEAP_SIZE	 (gui32SysL4lxDevMemFwPhysHeapBase ? gui32SysL4lxDevMemPhysHeapSize : ((gui32SysL4lxDevMemPhysHeapSize-(VZ_FWHEAP_SIZE * RGXFW_NUM_OS))/RGXFW_NUM_OS))

static struct PvzIrqData {
	IMG_HANDLE hLISR;
	IMG_UINT32 irqnr;
	l4_cap_idx_t irqcap;
	IMG_CHAR pszIrqName[16];
	IMG_CHAR pszIsrName[16];
} gsPvzIrqData[RGXFW_NUM_OS - 1];

static struct PvzMisrData {
	IMG_HANDLE hMISR;
	IMG_UINT32 uiOSID[RGXFW_NUM_OS - 1];
} gsPvzMisrData;

static void L4xDestroyPvzConnection(void);

static void
GetDevPhysHeapAddrSize(IMG_UINT32 ui32OSID,
					   IMG_UINT64 *pui64FwHeapBase,
					   IMG_UINT64 *pui64GpuHeapBase)
{
	/* Linear sub-partitioning (i.e. OSID<0>(fw)+OSID<1>(fw)+..) of FW devmem. */
	*pui64FwHeapBase = FWHEAP_BASE+(VZ_FWHEAP_SIZE*ui32OSID);
	if (gui32SysL4lxDevMemFwPhysHeapBase)
	{
		/* Followed by (possibly) discontiguous GPU devmem. */
		*pui64GpuHeapBase = gui32SysL4lxDevMemPhysHeapBase;
	}
	else
	{
		/* Followed by contiguous linear sub-partitioning (i.e. OSID<0>(gpu)+OSID<1>(gpu)+...) of GPU devmem */
		*pui64GpuHeapBase = (FWHEAP_BASE+(VZ_FWHEAP_SIZE*RGXFW_NUM_OS))+(VZ_GPUHEAP_SIZE*ui32OSID);
	}
}

static PVRSRV_ERROR
L4xVmmpGetDevPhysHeapOrigin(PVRSRV_DEVICE_CONFIG *psDevConfig,
							PVRSRV_DEVICE_PHYS_HEAP eHeap,
							PVRSRV_DEVICE_PHYS_HEAP_ORIGIN *peOrigin)
{
	PVR_UNREFERENCED_PARAMETER(psDevConfig);
	PVR_UNREFERENCED_PARAMETER(eHeap);
	*peOrigin = gui32SysL4lxDevMemFwPhysHeapOrigin ?
					PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_GUEST :
					PVRSRV_DEVICE_PHYS_HEAP_ORIGIN_HOST;
	return PVRSRV_OK;
}

static PVRSRV_ERROR
L4xVmmGetDevPhysHeapAddrSize(PVRSRV_DEVICE_CONFIG *psDevConfig,
							 PVRSRV_DEVICE_PHYS_HEAP eHeap,
							 IMG_UINT64 *pui64Size,
							 IMG_UINT64 *pui64Addr)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	IMG_UINT64 ui64FwHeapBase, ui64GpuHeapBase;
	PVR_UNREFERENCED_PARAMETER(psDevConfig);

	if (! gui32SysL4lxDevMemPhysHeapSize)
	{
		PVR_DPF((PVR_DBG_ERROR,"Invalid device memory mapping spec.\n"));
		return PVRSRV_ERROR_NOT_IMPLEMENTED;
	}
	else if (! gui32SysL4lxDevMemPhysHeapBase)
	{
		gui32SysL4lxDevMemPhysHeapBase = psDevConfig->pasPhysHeaps[0].pasRegions[0].sCardBase.uiAddr;
		gui32SysL4lxDevMemPhysHeapSize = psDevConfig->pasPhysHeaps[0].pasRegions[0].uiSize;
	}

	GetDevPhysHeapAddrSize(PVRSRV_VZ_DRIVER_OSID, &ui64FwHeapBase, &ui64GpuHeapBase);

	/* Ensure these values are 4 KiB aligned, else Fiasco/L4Linux proceeds to map these then falls-over */
	if ((ui64FwHeapBase & ui64GpuHeapBase & VZ_FWHEAP_SIZE & VZ_GPUHEAP_SIZE) & (IMG_UINT64)((1 < 12) - 1))
	{
		PVR_DPF((PVR_DBG_ERROR,"Invalid devmem mmio/sz specification, not 4K algined\n"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	switch (eHeap)
	{
		case PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL:
			*pui64Size = VZ_FWHEAP_SIZE;
			*pui64Addr = ui64FwHeapBase;
			break;

		case PVRSRV_DEVICE_PHYS_HEAP_GPU_LOCAL:
			*pui64Size = VZ_GPUHEAP_SIZE;
			*pui64Addr = ui64GpuHeapBase;
			break;

		default:
			*pui64Size = 0;
			*pui64Addr = 0;
			eError = PVRSRV_ERROR_NOT_IMPLEMENTED;
			PVR_ASSERT(0);
			break;
	}

	return eError;
}

static PVRSRV_ERROR
L4xVmmCreateDevConfig(IMG_UINT32 ui32FuncID,
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
	return PVRSRV_ERROR_NOT_IMPLEMENTED;
}

static PVRSRV_ERROR
L4xVmmDestroyDevConfig(IMG_UINT32 ui32FuncID,
					   IMG_UINT32 ui32DevID)
{
	PVR_UNREFERENCED_PARAMETER(ui32FuncID);
	PVR_UNREFERENCED_PARAMETER(ui32DevID);
	return PVRSRV_ERROR_NOT_IMPLEMENTED;
}

static PVRSRV_ERROR
L4xVmmCreateDevPhysHeaps(IMG_UINT32 ui32FuncID,
						 IMG_UINT32 ui32DevID,
						 IMG_UINT32 *peType,
						 IMG_UINT64 *pui64FwSize,
						 IMG_UINT64 *pui64FwAddr,
						 IMG_UINT64 *pui64GpuSize,
						 IMG_UINT64 *pui64GpuAddr)
{
	PVR_UNREFERENCED_PARAMETER(ui32FuncID);
	PVR_UNREFERENCED_PARAMETER(ui32DevID);
	PVR_UNREFERENCED_PARAMETER(peType);
	PVR_UNREFERENCED_PARAMETER(pui64FwSize);
	PVR_UNREFERENCED_PARAMETER(pui64FwAddr);
	PVR_UNREFERENCED_PARAMETER(pui64GpuSize);
	PVR_UNREFERENCED_PARAMETER(pui64GpuAddr);
	return PVRSRV_ERROR_NOT_IMPLEMENTED;
}

static PVRSRV_ERROR
L4xVmmDestroyDevPhysHeaps(IMG_UINT32 ui32FuncID,
						  IMG_UINT32 ui32DevID)
{
	PVR_UNREFERENCED_PARAMETER(ui32FuncID);
	PVR_UNREFERENCED_PARAMETER(ui32DevID);
	return PVRSRV_ERROR_NOT_IMPLEMENTED;
}

static PVRSRV_ERROR
L4xVmmMapDevPhysHeap(IMG_UINT32 ui32FuncID,
					 IMG_UINT32 ui32DevID,
					 IMG_UINT64 ui64Size,
					 IMG_UINT64 ui64Addr)
{
	PVR_UNREFERENCED_PARAMETER(ui32FuncID);
	PVR_UNREFERENCED_PARAMETER(ui32DevID);
	PVR_UNREFERENCED_PARAMETER(ui64Size);
	PVR_UNREFERENCED_PARAMETER(ui64Addr);

	if (gui32SysL4lxDevMemFwPhysHeapOrigin)
	{
		/* 
			In a real dynamic PVZ setup, the RAM region backing
			the guest driver firmware heap is allocated from its
			VM GuestOS via a DMA/CMA allocation or with other means.
			These values are then communicated to the host driver
			via an hypercall in this function therefore the address
			of this heap cannot be known apriori by the host driver.
			The advantage of this setup is that no carving-out of
			a dedicated piece of memory for GPU firmware use is
			required by the systems integrator as said memory is
			dynamically obtained by the guest driver at runtime.

			In a quasi-dynamic PVZ setup, the address of the guest
			driver firmware heap is know apriori by the host driver.
			Therefore no actual hypercall to host driver is needed
			here so we OK this call. When the guest driver is loaded,
			it notifies the host and the host then maps the guest
			driver firmware heap. This setup is usefull for setups
			where hypercalls do not carry any data payload but merely
			acts as signals so when the guest driver comes online it
			only has to notify the host driver which then proceeds
			to map that driver heap into the firmware.
		*/
		return PVRSRV_OK;
	}

	/*
		In a static PVZ setup, the address of both the host and all
		guest driver firmware heap is known apriori by host driver
		so no hypercall mechanism is required in this setup. All
		the firmware heap for all drivers in initialized by the
		host when it's loaded. All drivers know their own dedicated
		firmware heap apriori; for all drivers, the correct firmware
		base address corresponding to it's own driver firmware heap
		is returned in sConfigFuncTab->pfnGetDevPhysHeapAddrSize().
	*/
	return PVRSRV_ERROR_NOT_IMPLEMENTED;
}

static PVRSRV_ERROR
L4xVmmUnmapDevPhysHeap(IMG_UINT32 ui32FuncID,
					   IMG_UINT32 ui32DevID)
{
	PVR_UNREFERENCED_PARAMETER(ui32FuncID);
	PVR_UNREFERENCED_PARAMETER(ui32DevID);

	if (gui32SysL4lxDevMemFwPhysHeapOrigin)
	{
		return PVRSRV_OK;
	}

	return PVRSRV_ERROR_NOT_IMPLEMENTED;
}

static VMM_PVZ_CONNECTION gsL4xVmmPvz =
{
	.sHostFuncTab = {
		/* pfnCreateDevConfig */
		&L4xVmmCreateDevConfig,

		/* pfnDestroyDevConfig */
		&L4xVmmDestroyDevConfig,

		/* pfnCreateDevPhysHeaps */
		&L4xVmmCreateDevPhysHeaps,

		/* pfnDestroyDevPhysHeaps */
		&L4xVmmDestroyDevPhysHeaps,

		/* pfnMapDevPhysHeap */
		&L4xVmmMapDevPhysHeap,

		/* pfnUnmapDevPhysHeap */
		&L4xVmmUnmapDevPhysHeap
	},

	.sGuestFuncTab = {
		/* pfnCreateDevConfig */
		&PvzServerCreateDevConfig,

		/* pfnDestroyDevConfig */
		&PvzServerDestroyDevConfig,

		/* pfnCreateDevPhysHeaps */
		&PvzServerCreateDevPhysHeaps,

		/* pfnDestroyDevPhysHeaps */
		PvzServerDestroyDevPhysHeaps,

		/* pfnMapDevPhysHeap */
		&PvzServerMapDevPhysHeap,

		/* pfnUnmapDevPhysHeap */
		&PvzServerUnmapDevPhysHeap
	},

	.sConfigFuncTab = {
		/* pfnGetDevPhysHeapOrigin */
		&L4xVmmpGetDevPhysHeapOrigin,

		/* pfnGetDevPhysHeapAddrSize */
		&L4xVmmGetDevPhysHeapAddrSize
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

static void _TriggerPvzConnection(l4_cap_idx_t irqcap)
{
	unsigned long flags;
	static DEFINE_SPINLOCK(lock);

	/* Notify that this OSID has transitioned on/offline */
	spin_lock_irqsave(&lock, flags);
	l4_irq_trigger(irqcap);
	spin_unlock_irqrestore(&lock, flags);
}

static void L4x_MISRHandler_PvzConnection(void *pvData)
{
	struct PvzMisrData *psMisrData = pvData;
	IMG_UINT32 uiOSID;
	IMG_UINT32 uiIdx;

	for (uiOSID = 1; uiOSID < RGXFW_NUM_OS; uiOSID ++)
	{
		uiIdx = uiOSID - 1;
		if (psMisrData->uiOSID[uiIdx] == uiOSID)
		{
			if (PVRSRV_OK != gsL4xVmmPvz.sVmmFuncTab.pfnOnVmOnline(uiOSID, 0))
			{
				PVR_DPF((PVR_DBG_ERROR, "OnVmOnline failed for OSID%d", uiOSID));
			}
			else
			{
				PVR_LOG(("Guest OSID%d driver is online", uiOSID));
			}

			if (gui32SysL4lxDevMemFwPhysHeapOrigin)
			{
				IMG_UINT64 ui64FwHeapBase, ui64GpuHeapBase;

				GetDevPhysHeapAddrSize(uiOSID, &ui64FwHeapBase, &ui64GpuHeapBase);

				PVR_LOG(("Initializing guest OSID%d driver firmware heap @ 0x%p", uiOSID, (void*)(uintptr_t)ui64FwHeapBase));

				if (gsL4xVmmPvz.sGuestFuncTab.pfnMapDevPhysHeap(uiOSID,
																PVZ_BRIDGE_MAPDEVICEPHYSHEAP,
																0,
																VZ_FWHEAP_SIZE,
																ui64FwHeapBase) != PVRSRV_OK)
				{
					PVR_LOG(("Guest OSID%d driver firmware heap initialization failed", uiOSID));
				}
				else
				{
					PVR_LOG(("Guest OSID%d firmware heap initialized", uiOSID));
				}
			}
		}
		else if (psMisrData->uiOSID[uiIdx] == 0)
		{
			if (gui32SysL4lxDevMemFwPhysHeapOrigin)
			{
				if (gsL4xVmmPvz.sGuestFuncTab.pfnUnmapDevPhysHeap(uiOSID,
																  PVZ_BRIDGE_UNMAPDEVICEPHYSHEAP,
																  0) != PVRSRV_OK)
				{
					PVR_LOG(("Guest OSID%d driver firmware heap deinitialization failed", uiOSID));
				}
				else
				{
					PVR_LOG(("Guest OSID%d firmware heap deinitialized", uiOSID));
				}
			}

			if (PVRSRV_OK != gsL4xVmmPvz.sVmmFuncTab.pfnOnVmOffline(uiOSID))
			{
				PVR_DPF((PVR_DBG_ERROR, "OnVmOffline failed for OSID%d", uiOSID));
			}
			else
			{
				PVR_LOG(("Guest OSID%d driver is offline", uiOSID));
			}
		}
	}
}

static IMG_BOOL L4xPvzConnectionLISR(void *pvData)
{
	struct PvzMisrData *psMisrData = &gsPvzMisrData;
	struct PvzIrqData *psIrqData = &gsPvzIrqData[0];
	IMG_UINT32 uiOSID;
	IMG_UINT32 uiIdx;

	for (uiOSID = 1; uiOSID < RGXFW_NUM_OS; uiOSID ++)
	{
		uiIdx = uiOSID - 1;
		if (&psIrqData[uiIdx] == pvData)
		{
			/* For now, use simple state machine protocol */
			if (psMisrData->uiOSID[uiIdx] == uiOSID)
			{
				/* Guest driver in VM is offline */
				psMisrData->uiOSID[uiIdx] = 0;
			}
			else
			{
				/* Guest driver in VM is online */
				psMisrData->uiOSID[uiIdx] = uiOSID;
			}
			OSScheduleMISR(psMisrData->hMISR);
		}
	}

	return IMG_TRUE;
}

static PVRSRV_ERROR L4xCreatePvzConnection(void)
{
	IMG_UINT32 uiOSID;
	IMG_BOOL bRequireOsidOnline = IMG_FALSE;

	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		l4_cap_idx_t irqcap;

		/* Get environment capability for PVZ/IRQ */
		irqcap = l4re_env_get_cap("pvzirq");
		if (l4_is_invalid_cap(irqcap))
		{
			bRequireOsidOnline = IMG_TRUE;

			PVR_LOG(("Could not find pvz connection"));
			PVR_LOG(("Unable to notify host of guest online event"));

			goto exit;
		}
		else
		{
			/* Notify host that guest has come online */
			_TriggerPvzConnection(irqcap);
		}
	}
	else
	{
		/* This pvz misr get notified whenever guest comes online */
		if (PVRSRV_OK != OSInstallMISR(&gsPvzMisrData.hMISR,
									   L4x_MISRHandler_PvzConnection,
									   &gsPvzMisrData))
		{
			bRequireOsidOnline = IMG_TRUE;

			PVR_LOG(("Could not create pvz thread object"));

			goto exit;
		}

		/* Create abstract virtual IRQ for each guest OSID */
		for (uiOSID = 1; uiOSID < RGXFW_NUM_OS; uiOSID ++)
		{
			IMG_UINT32 uiIdx = uiOSID - 1;

			/* Each OSID has its own corresponding IRQ capability */
			OSSNPrintf(gsPvzIrqData[uiIdx].pszIrqName,
					   sizeof(gsPvzIrqData[uiIdx].pszIrqName),
					   "pvzirq%d",
					   uiOSID);
			OSSNPrintf(gsPvzIrqData[uiIdx].pszIsrName,
					   sizeof(gsPvzIrqData[uiIdx].pszIsrName),
					   "l4pvz%d",
					   uiOSID);

			/* Get a free capability slot for the PVZ/IRQ capability for this OSID */
			gsPvzIrqData[uiIdx].irqcap = l4re_env_get_cap(gsPvzIrqData[uiIdx].pszIrqName);
			if (l4_is_invalid_cap(gsPvzIrqData[uiIdx].irqcap))
			{
				bRequireOsidOnline = IMG_TRUE;

				PVR_LOG(("Could not find a PVZ/IRQ capability [%s] for OSID%d",
						gsPvzIrqData[uiIdx].pszIrqName,
						uiOSID));

				if (uiOSID == (RGXFW_NUM_OS - 1))
				{
					goto exit;
				}

				continue;
			}

			/* Register IRQ capability with l4linux, obtain corresponding IRQ number */
			gsPvzIrqData[uiIdx].irqnr = l4x_register_irq(gsPvzIrqData[uiIdx].irqcap);
			if (gsPvzIrqData[uiIdx].irqnr < 0)
			{
				bRequireOsidOnline = IMG_TRUE;

				PVR_LOG(("Could not register OSID%d IRQ%d with kernel",
						gsPvzIrqData[uiIdx].irqnr,
						uiOSID));

				continue;
			}

			/* pvz IRQ number now known to l4linux kernel, install ISR for it */
			if (PVRSRV_OK != OSInstallSystemLISR(&gsPvzIrqData[uiIdx].hLISR,
												 gsPvzIrqData[uiIdx].irqnr,
												 gsPvzIrqData[uiIdx].pszIsrName,
												 L4xPvzConnectionLISR,
												 &gsPvzIrqData[uiIdx],
												 0))
			{
				bRequireOsidOnline = IMG_TRUE;

				PVR_LOG(("Could not install system LISR for OSID%d PVZ/IRQ",
						uiOSID));

				continue;
			}
		}

		if (bRequireOsidOnline == IMG_TRUE)
		{
			PVR_LOG(("Manual pvrdebug -osonline #osid,1 required"));
		}
	}

exit:
	return PVRSRV_OK;
}

static void L4xDestroyPvzConnection(void)
{
	if (PVRSRV_VZ_MODE_IS(DRIVER_MODE_GUEST))
	{
		l4_cap_idx_t irqcap;

		/* Get environment capability for PVZ/IRQ */
		irqcap = l4re_env_get_cap("pvzirq");
		if (l4_is_invalid_cap(irqcap))
		{
			PVR_DPF((PVR_DBG_ERROR,"Could not find pvz connection"));
			PVR_DPF((PVR_DBG_ERROR,"Unable to notify host of guest offline event"));
			return;
		}

		_TriggerPvzConnection(irqcap);
	}
	else
	{
		IMG_UINT32 uiOSID;
		IMG_UINT32 uiIdx;

		for (uiOSID = 1; uiOSID < RGXFW_NUM_OS; uiOSID ++)
		{
			uiIdx = uiOSID - 1;

			if (gsPvzIrqData[uiIdx].hLISR)
			{
				OSUninstallSystemLISR(gsPvzIrqData[uiIdx].hLISR);
				gsPvzIrqData[uiIdx].hLISR = NULL;
			}

			if (gsPvzIrqData[uiIdx].irqnr)
			{
				l4x_unregister_irq(gsPvzIrqData[uiIdx].irqnr);
				gsPvzIrqData[uiIdx].irqnr = 0;
			}
		}

		if (gsPvzMisrData.hMISR)
		{
			OSUninstallMISR(gsPvzMisrData.hMISR);
			gsPvzMisrData.hMISR = NULL;
		}
	}
}

PVRSRV_ERROR VMMCreatePvzConnection(VMM_PVZ_CONNECTION **psPvzConnection)
{
	PVR_ASSERT(psPvzConnection);
	*psPvzConnection = &gsL4xVmmPvz;
	return L4xCreatePvzConnection();
}

void VMMDestroyPvzConnection(VMM_PVZ_CONNECTION *psPvzConnection)
{
	PVR_ASSERT(psPvzConnection == &gsL4xVmmPvz);
	L4xDestroyPvzConnection();
}

/******************************************************************************
 End of file (vmm_type_l4linux.c)
******************************************************************************/
