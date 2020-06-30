/*
* xenipcfront.c
*
* Copyright (c) 2018 Semidrive Semiconductor.
* All rights reserved.
*
* Description: Xen IPC abstraction layer .
*
* Revision History:
* -----------------
* 01, 06/12/2020 Liuye create this file
*/
#include <linux/module.h>  /* Needed by all modules */
#include <linux/kernel.h>  /* Needed for KERN_ALERT */
#include <linux/semaphore.h>
#include <linux/sched.h>

#include <xen/xen.h>
#include <xen/xenbus.h>
#include <xen/page.h>
#include <xen/events.h>
#include <xen/grant_table.h>

#include "xenipc.h"

#define IPC_MAX_RING_SIZE	\
     	__CONST_RING_SIZE(xenipc, XEN_PAGE_SIZE * XENBUS_MAX_RING_GRANTS)
#define IPC_RING_SIZE	\
        __CONST_RING_SIZE(xenipc, XEN_PAGE_SIZE)
#define IPC_WAIT_RSP_TIMEOUT	MAX_SCHEDULE_TIMEOUT

struct xenipc_channel {
	spinlock_t ring_lock;
	struct xenipc_front_ring front_ring;
	grant_ref_t ring_ref;
	int evtchn;
	int irq;
};

typedef struct xenipc_frontend {
	struct xenbus_device    *xb_dev;
	struct xenipc_channel   *channel;
	struct xenipc_call_info *call_head;
	struct completion call_completion;
	spinlock_t calls_lock;
	bool inited;
	bool mapped;
} *xenipc_fe_handle_t;

struct xenipc_frontend gXenIpcInfo={0};
/* Only one front/back connection supported. */
static struct xenbus_device *xipc_fe_dev;
static atomic_t ipc_refcount;

/* first increment refcount, then proceed */
#define xipc_enter() {               \
	atomic_inc(&ipc_refcount);      \
}

/* first complete other operations, then decrement refcount */
#define xipc_exit() {                \
	atomic_dec(&ipc_refcount);      \
}

static int xenipc_rpc_call(struct xenipc_frontend *fe, struct xenipc_request *pReq)
{
	struct xenipc_channel *chan = fe->channel;
	struct xenipc_front_ring *ring = &(chan->front_ring);
	RING_IDX req_prod = ring->req_prod_pvt;
	struct xenipc_request *ring_req;
	int notify;
	uint32_t id = req_prod & (IPC_RING_SIZE - 1);;

	if (RING_FULL(ring))
	{
		return -EBUSY;
	}

	ring_req = RING_GET_REQUEST(ring, ring->req_prod_pvt);
	ring->req_prod_pvt++;

	memcpy(ring_req, pReq, sizeof(struct xenipc_request));
	ring_req->id          = id;
	pReq->id              = id;

	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(ring, notify);
	if (notify)
		notify_remote_via_irq(chan->irq);

	return 0;
}

static irqreturn_t xenipc_interrupt(int irq, void *dev_id)
{
	struct xenipc_respond *result;
	RING_IDX i, rp;
	struct xenbus_device *xbdev = (struct xenbus_device *)dev_id;
	struct xenipc_frontend *fe = dev_get_drvdata(&xbdev->dev);
	struct xenipc_channel *chan = fe->channel;
	struct xenipc_call_info *current_call = NULL;

again:
	spin_lock(&chan->ring_lock);
	rp = chan->front_ring.sring->rsp_prod;
	rmb(); /* Ensure we see queued responses up to 'rp'. */

	for (i = chan->front_ring.rsp_cons; i != rp; i++) {

		result = RING_GET_RESPONSE(&chan->front_ring, i);

		//spin_lock(&info->calls_lock);
		current_call = fe->call_head;
		do {
			if (!current_call)
				break;
			if (current_call->req->id == result->id)
				break;
			current_call = current_call->next;
		} while(current_call);

		if (current_call && (current_call->req->id == result->id)) {
			memcpy(current_call->rsp, result, sizeof(struct xenipc_respond));
			complete(&current_call->call_completed);
		}
		//spin_unlock(&info->calls_lock);
	}

	chan->front_ring.rsp_cons = i;

	if (i != chan->front_ring.req_prod_pvt) {
		int more_to_do;
		RING_FINAL_CHECK_FOR_RESPONSES(&chan->front_ring, more_to_do);
		if (more_to_do)
			goto again;
	} else
		chan->front_ring.sring->rsp_event = i + 1;

	spin_unlock(&chan->ring_lock);

	return IRQ_HANDLED;
}

int xenipc_do_request(int dev, struct xenipc_request *pReq, struct xenipc_respond *rsp)
{
	int ret = 0;
	struct xenipc_call_info *call;
	struct xenipc_call_info *current_call;
	unsigned long flags;
	xenipc_fe_handle_t hRpc = &gXenIpcInfo;

	VMM_DEBUG_PRINT("%s:%d, enter!", __FUNCTION__, __LINE__);

	if (hRpc->inited == false) {
		pr_err("xen ipc is not initialized correctly\n");
		return -EINVAL;
	}

	call = kzalloc((sizeof(struct xenipc_call_info)), GFP_KERNEL);
	if (!call) {
		pr_err("xen ipc has no memory\n");
		return -ENOMEM;
	}

	spin_lock_irqsave(&hRpc->calls_lock, flags);
	init_completion(&call->call_completed);
	current_call = hRpc->call_head;
	if (!current_call) {
		hRpc->call_head = call;
	} else {
		while (current_call->next){
			current_call = current_call->next;
		}
		current_call->next = call;
	}
	call->req = pReq;
	call->rsp = rsp;
	VMM_DEBUG_PRINT("xenipc calling(cmd=%d)", pReq->cmd);

	ret = xenipc_rpc_call(hRpc, pReq);
	if (ret) {
		pr_err("xenipc failled to call\n");
		return ret;
	}
	VMM_DEBUG_PRINT("xenipc cmd %d is called, pReq->id = %lld!", pReq->cmd, pReq->id);
	spin_unlock_irqrestore(&hRpc->calls_lock, flags);

	/* wait for the backend to connect */
	if (wait_for_completion_interruptible(&call->call_completed) < 0)
		return -ETIMEDOUT;

	VMM_DEBUG_PRINT("xenipc cmd %d respond!", pReq->cmd);

	spin_lock_irqsave(&hRpc->calls_lock, flags);
	current_call = hRpc->call_head;
	if ( current_call == NULL ) {
		// do nothing
	} else if ( current_call == call ){
		hRpc->call_head = current_call->next;
	} else {
		do{
			if (current_call->next == call) break;
			current_call = current_call->next;
		} while (current_call != NULL);

		if (current_call->next == call)
			current_call->next = current_call->next->next;
	}

	spin_unlock_irqrestore(&hRpc->calls_lock, flags);
	kfree(call);

	return ret;
}

#define XENIPC_SANITY_TAG		(0x20201010)
int xenipc_rpc_trace(int dev, struct rpc_req_msg *req, struct rpc_ret_msg *result)
{
	int ret;
	struct xenipc_request xreq;
	struct xenipc_respond xrsp;

	xreq.cmd = req->cmd;
	xreq.cksum = XENIPC_SANITY_TAG;	//TODO: add real checksum
	memcpy(xreq.param, req->param, sizeof(req->param));

	ret = xenipc_do_request(dev, &xreq, &xrsp);

	if (result) {
		result->ack = xrsp.ack;
		result->retcode = xrsp.retcode;
		memcpy(result->result, xrsp.result, sizeof(xrsp.result));
	}

	return ret;
}
EXPORT_SYMBOL(xenipc_do_request);

static int talk_to_backend_ring(struct xenbus_device *dev, struct xenipc_channel *priv)
{
	struct xenbus_transaction xbt;
	const char *message = NULL;
	int ret;

again:
	ret = xenbus_transaction_start(&xbt);
	if (ret) {
		xenbus_dev_fatal(dev, ret, "starting transaction");
		return ret;
	}

	ret = xenbus_printf(xbt, dev->nodename,
				"ipc-ring-ref", "%u", priv->ring_ref);
	if (ret) {
		message = "writing ring-ref";
		goto abort_transaction;
	}

	ret = xenbus_printf(xbt, dev->nodename, "event-channel-ipc", "%u",
				priv->evtchn);
	if (ret) {
		message = "writing event-channel";
		goto abort_transaction;
	}

	ret = xenbus_transaction_end(xbt, 0);
	if (ret == -EAGAIN)
		goto again;

	if (ret) {
		xenbus_dev_fatal(dev, ret, "completing transaction");
		return ret;
	}

	return 0;

abort_transaction:
	xenbus_transaction_end(xbt, 1);
	if (message)
	        xenbus_dev_error(dev, ret, "%s", message);

	return ret;
}

static int xenipc_front_connect(struct xenbus_device *dev)
{
	struct xenipc_sring *sring;
	grant_ref_t gref;
	int err;
	struct xenipc_channel *chan;
	struct xenipc_frontend *fe    = &gXenIpcInfo;

	chan = kzalloc(sizeof(struct xenipc_channel), GFP_KERNEL);
	if (!chan) {
		err = -ENOMEM;
		xenbus_dev_fatal(dev, err, "allocating ipc channel");
		goto fail;
	}

	sring= (struct xenipc_sring *)get_zeroed_page(GFP_NOIO | __GFP_HIGH);
	if (!sring) {
		err = -ENOMEM;
		xenbus_dev_fatal(dev, err, "allocating ipc ring page");
		goto fail;
	}
	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&chan->front_ring, sring, XEN_PAGE_SIZE);

	err = xenbus_grant_ring(dev, sring, 1, &gref);
	if (err < 0)
		goto grant_ring_fail;
	chan->ring_ref = gref;

	err = xenbus_alloc_evtchn(dev, &chan->evtchn);
	if (err < 0)
		goto alloc_evtchn_fail;

	err = bind_evtchn_to_irqhandler(chan->evtchn, xenipc_interrupt,
					0, "ipcfront", dev);
	if (err <= 0)
		goto bind_fail;

	chan->irq = err;
	spin_lock_init(&chan->ring_lock);
	spin_lock_init(&fe->calls_lock);
	fe->channel = chan;

	err = talk_to_backend_ring(dev, chan);
	if (err <0)
		goto bind_fail;

	dev_set_drvdata(&dev->dev, fe);
	dev_info(&dev->dev, "out %s:%d", __FUNCTION__, __LINE__);

	return 0;

bind_fail:
	xenbus_free_evtchn(dev, chan->evtchn);
alloc_evtchn_fail:
	gnttab_end_foreign_access_ref(chan->ring_ref, 0);
grant_ring_fail:
	free_page((unsigned long)sring);
fail:

	if (chan)
		kfree(chan);

	dev_info(&dev->dev, "out %s, err = %d", __FUNCTION__, err);

	return err;
}

static void xenipc_disconnect_backend(struct xenipc_channel *chan)
{
	unsigned long flags;

	spin_lock_irqsave(&chan->ring_lock, flags);

	if (chan->irq)
		unbind_from_irqhandler(chan->irq, chan);
	chan->evtchn = 0;
	chan->irq = 0;

	gnttab_free_grant_references(chan->ring_ref);

	/* End access and free the pages */
	gnttab_end_foreign_access(chan->ring_ref, 0, (unsigned long)chan->front_ring.sring);

	chan->ring_ref = GRANT_INVALID_REF;
	spin_unlock_irqrestore(&chan->ring_lock, flags);
}

/**
 * Callback received when the backend's state changes.
 */
static void xenipc_backend_changed(struct xenbus_device *dev,
				enum xenbus_state backend_state)
{
	dev_info(&dev->dev, "%s %p backend(%s) this end(%s)\n",
			 __func__,
			 dev,
			 xenbus_strstate(backend_state),
			 xenbus_strstate(dev->state));

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

		xenipc_front_connect(dev);
		xenbus_switch_state(dev, XenbusStateInitialised);
    	break;
	case XenbusStateConnected:
		xenbus_switch_state(dev, XenbusStateConnected);
		gXenIpcInfo.inited = true;
		complete(&gXenIpcInfo.call_completion);
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

static int xenipc_front_remove(struct xenbus_device *xbdev)
{
	struct xenipc_frontend *fe = dev_get_drvdata(&xbdev->dev);
	struct xenipc_channel *chan = fe->channel;

	dev_info(&xbdev->dev, "%s removed", xbdev->nodename);
	if (fe == NULL)
	    return 0;

	gXenIpcInfo.inited = false;
	xenipc_disconnect_backend(chan);
	kfree(chan);
	kfree(fe);
	return 0;
}

// The function is called on activation of the device
static int xenipc_front_probe(struct xenbus_device *dev,
              const struct xenbus_device_id *id)
{
	dev_info(&dev->dev, "Probe called. We are good to go.\n");
	gXenIpcInfo.xb_dev = dev;
	xipc_fe_dev = dev;

	xenbus_switch_state(dev, XenbusStateInitialising);
	return 0;
}

static const struct xenbus_device_id xenipc_fe_ids[] = {
	{ "vrpc"  },
	{ ""  }
};

static struct xenbus_driver xenipc_front_driver = {
	.name = "ipcfront",
	.ids  = xenipc_fe_ids,
	.probe = xenipc_front_probe,
	.remove = xenipc_front_remove,
	.otherend_changed = xenipc_backend_changed,
};

static int __init xenipc_fe_init(void)
{
	int ret;

	if (!xen_domain() || xen_initial_domain())
		return -ENODEV;

	pr_info("enter xen ipc front driver!\n");

	init_completion(&gXenIpcInfo.call_completion);
	ret = xenbus_register_frontend(&xenipc_front_driver);
	if (ret < 0)
		pr_err("Failed to initialize frontend driver, ret %d", ret);

	/* wait for the backend to connect */
//	if (wait_for_completion_interruptible(&gXenIpcInfo.call_completion) < 0)
//		return -ETIMEDOUT;

	return 0;
}

static void __exit xenipc_fe_fini(void)
{
	if (!xen_domain() || !xen_initial_domain())
		return;

	pr_info("unregister xen ipc front driver!\n");
	xenbus_unregister_driver(&xenipc_front_driver);
}

module_init(xenipc_fe_init);

module_exit(xenipc_fe_fini);

