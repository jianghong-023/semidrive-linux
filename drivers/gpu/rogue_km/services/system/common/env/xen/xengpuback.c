/*
* xengpuback.c
*
* Copyright (c) 2018 Semidrive Semiconductor.
* All rights reserved.
*
* Description: System abstraction layer .
*
* Revision History:
* -----------------
* 01, 07/25/2019 Lili create this file
*/

#define pr_fmt(fmt) "xen-gpuback: " fmt
#define VGSX_BAD_DEVICE_ID	-1

#include <linux/module.h>  /* Needed by all modules */
#include <linux/kernel.h>  /* Needed for KERN_ALERT */

#include <xen/xen.h>       /* We are doing something with Xen */
#include <xen/xenbus.h>
#include <xen/grant_table.h>
#include <xen/events.h>
#include <xen/interface/io/ring.h>
#include "img_types.h"
#include "xengpuback.h"
#include "vmm_pvz_server.h"
#include "xen_pvz_shbuf.h"
#include <asm/cacheflush.h>
#include "sysconfig.h"

int XenHostCreateConnection(IMG_UINT32 uiOSID);
void XenHostDestroyConnection(IMG_UINT32 uiOSID);


static void process_gpu_request(struct xen_gpuif *gpuif, const struct gpuif_request *req)
{
    u32 status = 0;
    u32 data = 0;
    IMG_UINT32 eType  = 0;
    IMG_UINT64 ui64FwSize  = 0;
    IMG_UINT64 ui64FwAddr  = 0;
    IMG_UINT64 ui64GpuSize = 0;
    IMG_UINT64 ui64GpuAddr = 0;
	IMG_UINT64 ui64PAddr   = 0;
    RING_IDX idx = gpuif->back_ring.rsp_prod_pvt;
	struct backend_info *be = gpuif->be;
    int notify;
	pr_info("enter %s, get cmd %d", __FUNCTION__, req->operation);
	struct gpuif_response * rsp = RING_GET_RESPONSE(&gpuif->back_ring, idx);

    switch (req->operation) {
        case GPUIF_OP_CREATE_PVZCONNECTION:
            status = XenHostCreateConnection(req->ui32OsID);
            break;
        case GPUIF_OP_DESTROY_PVZCONNECTION:
            XenHostDestroyConnection(req->ui32OsID);
            break;
        case GPUIF_OP_CREATE_DEVPHYSHEAPS:
        	{
			unsigned long flags;
			int ret;
			struct xen_pvz_shbuf_cfg buf_cfg;
            status = PvzServerCreateDevPhysHeaps(req->ui32OsID,
												req->ui32FuncID,
												req->ui32DevID,
												&eType,
												&ui64FwSize,
												&ui64FwAddr,
												&ui64GpuSize,
												&ui64GpuAddr);


			memset(&buf_cfg, 0, sizeof(buf_cfg));
			buf_cfg.xb_dev = gpuif->be->dev;
			buf_cfg.size = ui64FwSize;
			buf_cfg.addr = ui64FwAddr;
			buf_cfg.pages = NULL;
			gpuif->configaddr = phys_to_virt(ui64FwAddr);

#if defined(VMM_DEBUG)
			{
			const struct dma_map_ops *dma_ops;
			PVRSRV_DEVICE_CONFIG *pConfig = getDevConfig();
			struct device * pvrsrvdev = (struct device *)pConfig->pvOSDevice;
			memset(gpuif->configaddr, 0xDA,100);

			dma_ops = get_dma_ops(pvrsrvdev);
			dma_ops->sync_single_for_device(pvrsrvdev, ui64FwAddr, ui64FwSize, DMA_TO_DEVICE);
			dma_ops->sync_single_for_cpu(pvrsrvdev, ui64FwAddr, ui64FwSize, DMA_FROM_DEVICE);
			}
#endif

			VMM_DEBUG_PRINT("created Guest FW with OSID %d(domid %d) IPA 0x%x sz %llu, ret %d",
				req->ui32OsID, gpuif->domid, ui64FwAddr,
				ui64FwSize, status);
			gpuif->be->shbuf = xen_pvz_shbuf_alloc(&buf_cfg);
			if (IS_ERR(gpuif->be->shbuf)) {
				VMM_DEBUG_PRINT("create Guest FW with OSID %d(domid %d), ret %d",
					req->ui32OsID, gpuif->domid, gpuif->be->shbuf);
				break;
			}
			rsp->eType = eType;
			rsp->op.map_dev_heap.gref_directory =
				   xen_pvz_shbuf_get_dir_start(gpuif->be->shbuf);
			rsp->op.map_dev_heap.buffer_sz = ui64FwSize;
			VMM_DEBUG_PRINT("created Guest FW with gref_directory %d",
				rsp->op.map_dev_heap.gref_directory);

            break;
        	}
        case GPUIF_OP_DESTROY_DEVPHYSHEAPS:
            status = PvzServerDestroyDevPhysHeaps(req->ui32OsID,
                        					      req->ui32FuncID,
                        						  req->ui32DevID);
			xen_pvz_shbuf_free(gpuif->be->shbuf);
            break;
		case GPUIF_OP_MAP_DEVPHYSHEAPS:
			status = xdrv_map_dev_heap((struct xen_drvinfo *)gpuif->be,
				req->op.map_dev_heap.gref_directory,
				req->op.map_dev_heap.buffer_sz);
			if (!status) {
				const struct dma_map_ops *dma_ops;
				PVRSRV_DEVICE_CONFIG *pConfig = getDevConfig();
				struct device * pvrsrvdev = (struct device *)pConfig->pvOSDevice;
				ui64PAddr = gpuif->be->heap_obj->addr;
				VMM_DEBUG_PRINT("#########GPUIF_OP_MAP_DEVPHYSHEAPS,virt 0x%p host side:############", gpuif->be->heap_obj->vAddr);

				status = PvzServerMapDevPhysHeap(
					req->ui32OsID, req->ui32FuncID, req->ui32DevID,
					req->op.map_dev_heap.buffer_sz,
					ui64PAddr);
				if (status == PVRSRV_OK)
					gpuif->ui32DevID = req->ui32DevID;
				else
					xdrv_unmap_dev_heap((struct xen_drvinfo *)gpuif->be);

#if defined(VMM_DEBUG)
				if (gpuif->be->heap_obj->vAddr != NULL) {

					int i=0;
					int32_t *addr = (int32_t *)gpuif->be->heap_obj->vAddr;
					gpuif->configaddr = gpuif->be->heap_obj->vAddr;

					memset(addr, 0xCD,100);

					dma_ops = get_dma_ops(pvrsrvdev);
					dma_ops->sync_single_for_device(pvrsrvdev, ui64PAddr, 100, DMA_TO_DEVICE);
					dma_ops->sync_single_for_cpu(pvrsrvdev, ui64PAddr, 100, DMA_FROM_DEVICE);
				}
#endif

				VMM_DEBUG_PRINT("#########end of GPUIF_OP_MAP_DEVPHYSHEAPS,virt 0x%p host side:############", gpuif->be->heap_obj->vAddr);

			}
			VMM_DEBUG_PRINT("Mapping Guest FW with OSID %d (domid %d)IPA 0x%x sz %llu, ret %d",
				req->ui32OsID, gpuif->domid, ui64PAddr,
				req->op.map_dev_heap.buffer_sz, status);
			break;
		case GPUIF_OP_UNMAP_DEVPHYSHEAPS:
			if (gpuif->ui32DevID != req->ui32DevID) {
				status = -EINVAL;
				VMM_DEBUG_PRINT("Wrong device ID while unmapping: provided %d expected %d, osid %d, domain ID %d",
					req->ui32DevID, gpuif->ui32DevID,req->ui32OsID,	gpuif->domid);
			} else {
				status = PvzServerUnmapDevPhysHeap(
					req->ui32OsID, req->ui32FuncID, req->ui32DevID);

				gpuif->ui32DevID= VGSX_BAD_DEVICE_ID;
				VMM_DEBUG_PRINT("Unmapping Guest FW with OSID %d(domid %d), ret %d",
					req->ui32OsID, gpuif->domid, status);
			}
			xdrv_unmap_dev_heap((struct xen_drvinfo *)gpuif->be);
			rsp->status = status;
			break;
		case GPUIF_OP_DEBUG_DUMPCONFIG:
			if (gpuif->configaddr)
			{
				VMM_DEBUG_PRINT("#########GPUIF_OP_DEBUT_DUMPCONFIG,virt 0x%x host side:############", gpuif->configaddr);
				{
					int i=0;
					int32_t *addr = (int32_t *)gpuif->configaddr;

					for (i = 0; i< 100; i++)
					{
						VMM_DEBUG_PRINT("---- 0x%x ----", addr[i]);
					}
				}
				VMM_DEBUG_PRINT("#########end of GPUIF_OP_DEBUT_DUMPCONFIG,virt 0x%x host side:############", gpuif->configaddr);
			}
			break;
		default:
			break;
    }


	rsp->id = req->id;
	rsp->operation = req->operation;
	rsp->status = status;
	rsp->eType = eType;
	rsp->ui64FwPhysHeapSize = ui64FwSize,
	rsp->ui64FwPhysHeapAddr = ui64FwAddr,
	rsp->ui64GpuPhysHeapSize= ui64GpuSize,
	rsp->ui64GpuPhysHeapAddr= ui64GpuAddr,
  	gpuif->back_ring.rsp_prod_pvt = ++idx;

    VMM_DEBUG_PRINT("out %s, id %d, get cmd %d, rsp->status = %d",
		__FUNCTION__,rsp->id, req->operation, rsp->status);

	RING_PUSH_RESPONSES_AND_CHECK_NOTIFY(&gpuif->back_ring, notify);
	if (notify)
		notify_remote_via_irq(gpuif->gpuif_irq);
}


static bool xengpu_work_todo(struct xen_gpuif *gpuif)
{
 	if (likely(RING_HAS_UNCONSUMED_REQUESTS(&gpuif->back_ring)))
 		return 1;

 	return 0;
}

static void xengpu_action(struct xen_gpuif *gpuif)
{
	for (;;) {
		RING_IDX req_prod, req_cons;

		req_prod = gpuif->back_ring.sring->req_prod;
 		req_cons = gpuif->back_ring.req_cons;

  		/* Make sure we can see requests before we process them. */
  		rmb();

  		if (req_cons == req_prod)
  			break;

  		while (req_cons != req_prod) {
  			struct gpuif_request req;

  			RING_COPY_REQUEST(&gpuif->back_ring, req_cons, &req);
  			req_cons++;

  			process_gpu_request(gpuif, &req);
  		}

  		gpuif->back_ring.req_cons = req_cons;
  		gpuif->back_ring.sring->req_event = req_cons + 1;
  	}
}

irqreturn_t xen_gpuif_irq_fn(int irq, void *data)
{
    struct xen_gpuif *gpuif = data;

	while (xengpu_work_todo(gpuif))
		xengpu_action(gpuif);

	return IRQ_HANDLED;
}


