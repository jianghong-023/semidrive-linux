/*
* xengpufront.c
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
#include <linux/module.h>  /* Needed by all modules */
#include <linux/kernel.h>  /* Needed for KERN_ALERT */
#include <linux/semaphore.h>
#include <linux/sched.h>

#include <xen/xen.h>       /* We are doing something with Xen */
#include <xen/xenbus.h>
#include <xen/page.h>
#include <xen/events.h>
#include <xen/grant_table.h>

#include "xengpu.h"

#define GRANT_INVALID_REF	0 
#define GPU_MAX_RING_SIZE	\
     	__CONST_RING_SIZE(gpuif, XEN_PAGE_SIZE * XENBUS_MAX_RING_GRANTS)
#define GPU_RING_SIZE	\
        __CONST_RING_SIZE(gpuif, XEN_PAGE_SIZE)
#define GPU_WAIT_RSP_TIMEOUT	MAX_SCHEDULE_TIMEOUT


struct gpufront_info {
    spinlock_t ring_lock;
    struct gpuif_front_ring front_ring;
    grant_ref_t ring_ref;
    int evtchn;
    int irq;
};

struct xengpu_info {
    spinlock_t calls_lock;
    struct gpufront_info *rinfo;
    struct xengpu_call_info *call_head;
};

struct xengpu_info * gXenGpuInfo = NULL;

static int gpuif_request(struct xengpu_info *info, struct gpuif_request *pReq)
{
    struct gpufront_info *rinfo = info->rinfo;
    struct gpuif_front_ring *ring = &(rinfo->front_ring);
    RING_IDX req_prod = ring->req_prod_pvt;
    struct gpuif_request *ring_req;
    int notify;
    uint32_t id = req_prod & (GPU_RING_SIZE - 1);;
    unsigned long flags;

    spin_lock_irqsave(&rinfo->ring_lock, flags);

    if (RING_FULL(&rinfo->front_ring))
    {
        spin_unlock_irqrestore(&rinfo->ring_lock, flags);
  		return -EBUSY;
    }

    ring_req = RING_GET_REQUEST(&(rinfo->front_ring), ring->req_prod_pvt);
    ring->req_prod_pvt++;

    memcpy(&ring_req, pReq, sizeof(struct gpuif_request));
    ring_req->id          = id;
    pReq->id              = id;

    RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(ring, notify);
    if (notify)
        notify_remote_via_irq(rinfo->irq);
    spin_unlock_irqrestore(&rinfo->ring_lock, flags);

    return 0;
}

static irqreturn_t gpuif_interrupt(int irq, void *dev_id)
{
    struct gpuif_response *bret;
    RING_IDX i, rp;
    unsigned long flags;
    struct xengpu_info *info = (struct xengpu_info *)dev_id;
    struct gpufront_info *rinfo = info->rinfo;
    struct xengpu_call_info *current_call;

again:
  	rp = rinfo->front_ring.sring->rsp_prod;
  	rmb(); /* Ensure we see queued responses up to 'rp'. */

  	for (i = rinfo->front_ring.rsp_cons; i != rp; i++) {
  		unsigned long id;

  		bret = RING_GET_RESPONSE(&rinfo->front_ring, i);

        spin_lock_irqsave(&info->calls_lock, flags);
        current_call = info->call_head;
        do {
            if (current_call->req->id == bret->id)
                break;
            current_call = current_call->next;
        } while(current_call);
        if (current_call->req->id == bret->id) {
            memcpy(&current_call->rsp, bret, sizeof(struct gpuif_response));
            up(&current_call->sem);
        }
        spin_unlock_irqrestore(&info->calls_lock, flags);
  	}

  	rinfo->front_ring.rsp_cons = i;

  	if (i != rinfo->front_ring.req_prod_pvt) {
  		int more_to_do;
  		RING_FINAL_CHECK_FOR_RESPONSES(&rinfo->front_ring, more_to_do);
  		if (more_to_do)
  			goto again;
  	} else
  		rinfo->front_ring.sring->rsp_event = i + 1;

  	spin_unlock_irqrestore(&rinfo->ring_lock, flags);

  	return IRQ_HANDLED;
}

int xengpu_do_request(struct gpuif_request *pReq, struct gpuif_response **rsp)
{
	int ret = 0;
    struct xengpu_call_info *call = kzalloc((sizeof(struct xengpu_call_info)), GFP_KERNEL);
    struct xengpu_call_info *current_call;
    unsigned long flags;

    if (gXenGpuInfo == NULL)
    {
        pr_err("xen gpu is not initialized correctly");
        return -EINVAL;
    }

    spin_lock_irqsave(&gXenGpuInfo->calls_lock, flags);
    current_call = gXenGpuInfo->call_head;
    while (current_call->next){
        current_call = current_call->next;
    }
    current_call->next = call;
    call->req = pReq;
    call->rsp = kzalloc((sizeof(struct gpuif_response)), GFP_KERNEL);
    sema_init(&call->sem, 1);
    spin_unlock_irqrestore(&gXenGpuInfo->calls_lock, flags);

    ret = gpuif_request(gXenGpuInfo, pReq);
    if (ret) {
        pr_err("xen gpu is not initialized correctly");
        return ret;
    }

    ret = down_timeout(&call->sem, GPU_WAIT_RSP_TIMEOUT);
    if (ret) {
        ret = -EINVAL;
        pr_err("%s: Timeout %d sec for request (cmd = %u)\n",
                __func__, (GPU_WAIT_RSP_TIMEOUT/HZ), pReq->operation);
        return ret;
    }

    spin_lock_irqsave(&gXenGpuInfo->calls_lock, flags);
    current_call = gXenGpuInfo->call_head;
    while (current_call->next != call){
        current_call = current_call->next;
    }
    current_call->next = call->next;
    spin_unlock_irqrestore(&gXenGpuInfo->calls_lock, flags);

    *rsp = call->rsp;
    kfree(call);

    return ret;
}
EXPORT_SYMBOL(xengpu_do_request);

static int gpufront_connect(struct xenbus_device *dev)
{
	struct gpuif_sring *sring;
	grant_ref_t gref;
	int err;
    struct gpufront_info *rinfo = kzalloc(sizeof(struct gpufront_info), GFP_KERNEL);
    struct xengpu_info *info    = kzalloc(sizeof(struct xengpu_info), GFP_KERNEL);

  	sring= (struct gpuif_sring *)get_zeroed_page(GFP_NOIO | __GFP_HIGH);
  	if (!sring) {
  		err = -ENOMEM;
  		xenbus_dev_fatal(dev, err, "allocating gpu ring page");
  		goto fail;
  	}
  	SHARED_RING_INIT(sring);
  	FRONT_RING_INIT(&rinfo->front_ring, sring, XEN_PAGE_SIZE);

  	err = xenbus_grant_ring(dev, sring, 1, &gref);
  	if (err < 0)
  		goto grant_ring_fail;
  	rinfo->ring_ref = gref;

  	err = xenbus_alloc_evtchn(dev, &rinfo->evtchn);
  	if (err < 0)
  		goto alloc_evtchn_fail;

  	err = bind_evtchn_to_irqhandler(rinfo->evtchn,
  					gpuif_interrupt,
  					0, dev->nodename, rinfo);
    if (err <= 0)
        goto bind_fail;
 	rinfo->irq = err;
    spin_lock_init(&rinfo->ring_lock);

    spin_lock_init(&info->calls_lock);
    info->rinfo = rinfo;

    dev_set_drvdata(&dev->dev, info);
    gXenGpuInfo = info;
  	return 0;

bind_fail:
    xenbus_free_evtchn(dev, rinfo->evtchn);
alloc_evtchn_fail:
  	gnttab_end_foreign_access_ref(rinfo->ring_ref, 0);
grant_ring_fail:
	free_page((unsigned long)sring);
fail:
    kfree(rinfo);
    kfree(info);
 	return err;
}

static void xengpu_disconnect_backend(struct gpufront_info *info)
{
    unsigned int i = 0;
    unsigned long flags;

    spin_lock_irqsave(&info->ring_lock, flags);

    if (info->irq)
        unbind_from_irqhandler(info->irq, info);
    info->evtchn = 0;
    info->irq = 0;

    gnttab_free_grant_references(info->ring_ref);

       /* End access and free the pages */
    gnttab_end_foreign_access(info->ring_ref, 0, (unsigned long)info->front_ring.sring);

    info->ring_ref = GRANT_INVALID_REF;
    spin_unlock_irqrestore(&info->ring_lock, flags);
}


/**
 * Callback received when the backend's state changes.
 */
static void gpu_backend_changed(struct xenbus_device *dev,
                               			  enum xenbus_state backend_state)
{
  	dev_dbg(&dev->dev, "blkfront:blkback_changed to state %d.\n", backend_state);

  	switch (backend_state) {
      	case XenbusStateInitialising:
      	case XenbusStateInitialised:
      	case XenbusStateReconfiguring:
      	case XenbusStateReconfigured:
      	case XenbusStateUnknown:
      		break;
      	case XenbusStateInitWait:
            if (dev->state != XenbusStateInitialising)
                break;
      		gpufront_connect(dev);
            xenbus_switch_state(dev, XenbusStateInitialised);
            break;
      	case XenbusStateConnected:
            xenbus_switch_state(dev, XenbusStateConnected);
      		break;

      	case XenbusStateClosed:
      		if (dev->state == XenbusStateClosed)
      			break;
      		/* fall through */
      	case XenbusStateClosing:
      		xenbus_frontend_closed(dev);
      		break;
      	}
}

static int xengpufront_remove(struct xenbus_device *xbdev){

    struct xengpu_info *info = dev_get_drvdata(&xbdev->dev);
    struct gpufront_info *rinfo = info->rinfo;
    dev_dbg(&xbdev->dev, "%s removed", xbdev->nodename);
    if (info == NULL)
        return 0;
    gXenGpuInfo = NULL;
    xengpu_disconnect_backend(rinfo);
    kfree(rinfo);
    kfree(info);
    return 0;
}


// The function is called on activation of the device
static int xengpufront_probe(struct xenbus_device *dev,
              const struct xenbus_device_id *id)
{
    printk(KERN_NOTICE "Probe called. We are good to go.\n");
    xenbus_switch_state(dev, XenbusStateInitialising);
    return 0;
}

// This defines the name of the devices the driver reacts to
static const struct xenbus_device_id xengpufront_ids[] = {
    { "xengpu"  },
    { ""  }
};

// We set up the callback functions
static struct xenbus_driver xengpufront_driver = {
    .ids  = xengpufront_ids,
    .probe = xengpufront_probe,
    .remove = xengpufront_remove,
    .otherend_changed = gpu_backend_changed,
};

// On loading this kernel module, we register as a frontend driver
static int __init xengpu_init(void)
{
    printk(KERN_NOTICE "enter xen gpu front driver!\n");
	if (!xen_domain())
		return -ENODEV;

    return xenbus_register_frontend(&xengpufront_driver);
}
module_init(xengpu_init);

// ...and on unload we unregister
static void __exit xengpu_exit(void)
{
    xenbus_unregister_driver(&xengpufront_driver);
    printk(KERN_ALERT "out xen gpu front driver!\n");
}
module_exit(xengpu_exit);

