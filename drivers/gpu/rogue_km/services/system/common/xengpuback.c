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


#include <linux/module.h>  /* Needed by all modules */
#include <linux/kernel.h>  /* Needed for KERN_ALERT */

#include <xen/xen.h>       /* We are doing something with Xen */
#include <xen/xenbus.h>
#include <xen/events.h>
#include <xen/interface/io/ring.h>
#include "img_types.h"
#include "xengpuback.h"
#include "vmm_pvz_server.h"

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
    RING_IDX idx = gpuif->back_ring.rsp_prod_pvt;
    int notify;

    switch (req->operation) {
        case GPUIF_OP_CREATE_PVZCONNECTION:
            status = XenHostCreateConnection(gpuif->domid);
            break;
        case GPUIF_OP_DESTROY_PVZCONNECTION:
            XenHostDestroyConnection(gpuif->domid);
            break;
        case GPUIF_OP_CREATE_DEVPHYSHEAPS:
            status = PvzServerCreateDevPhysHeaps(gpuif->domid,
                                                 req->ui32FuncID,
                                                 req->ui32DevID,
                   	                         &eType,
                    							 &ui64FwSize,
                    							 &ui64FwAddr,
                    							 &ui64GpuSize,
                    							 &ui64GpuAddr);
            break;
        case GPUIF_OP_DESTROY_DEVPHYSHEAPS:
            status = PvzServerDestroyDevPhysHeaps(gpuif->domid,
                        					      req->ui32FuncID,
                        						  req->ui32DevID);
            break;
        case GPUIF_OP_MAP_DEVPHYSHEAPS:
            status = PvzServerMapDevPhysHeap(gpuif->domid,
                        					 req->ui32FuncID,
                        					 req->ui32DevID,
                        					 req->ui64Size,
						                     req->ui64Addr);
            break;
        case GPUIF_OP_UNMAP_DEVPHYSHEAPS:
            status = PvzServerUnmapDevPhysHeap(gpuif->domid,
                        			           req->ui32FuncID,
                        				       req->ui32DevID);
            break;
    }

	struct gpuif_response rsp = {
		.id = req->id,
		.operation = req->operation,
		.status = status,
        .eType = eType,
        .ui64FwPhysHeapSize = ui64FwSize,
        .ui64FwPhysHeapAddr = ui64FwAddr,
        .ui64GpuPhysHeapSize= ui64GpuSize,
        .ui64GpuPhysHeapAddr= ui64GpuAddr,
	};

 	*RING_GET_RESPONSE(&gpuif->back_ring, idx) = rsp;
  	gpuif->back_ring.rsp_prod_pvt = ++idx;

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


