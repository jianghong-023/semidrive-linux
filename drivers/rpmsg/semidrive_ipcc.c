/*
 * Copyright (C) 2020 Semidrive Semiconductor, Inc.
 *
 * derived from the imx-rpmsg implementation.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/mailbox_client.h>
#include <linux/soc/semidrive/mb_msg.h>
#include "rpmsg_internal.h"

/* The feature bitmap for virtio rpmsg */
#define RPMSG_F_NS		0 /* RP supports name service notifications */
#define RPMSG_F_ECHO	1 /* RP supports echo test */
#define RPMSG_RESERVED_ADDRESSES	(1024)
#define RPMSG_NS_ADDR			(53)
#define RPMSG_ECHO_ADDR			(30)
#define RPMSG_TX_BUF_SIZE		(512)

/* DEBUG purpose */
#define CONFIG_IPCC_DUMP_HEX	(0)

struct rpmsg_ipcc_hdr {
	sd_msghdr_t mboxhdr;
	u32 src;
	u32 dst;
	u32 reserved;
	u16 len;
	u16 flags;
	u8 data[0];
} __packed;

struct rpmsg_ipcc_nsm {
	char name[RPMSG_NAME_SIZE];
	u32 addr;
	u32 flags;
} __packed;

enum rpmsg_ns_flags {
	RPMSG_NS_CREATE		= 0,
	RPMSG_NS_DESTROY	= 1,
};

struct rpmsg_ipcc_device {
	struct device *dev;
	int rproc;
	struct mbox_client client;
	struct mbox_chan *mbox;
	struct rpmsg_endpoint *ns_ept;
	struct rpmsg_endpoint *echo_ept;
	struct work_struct rx_work;
	struct idr endpoints;
	struct mutex endpoints_lock;

	wait_queue_head_t sendq;
	struct mutex tx_lock;
	unsigned int tx_buf_size;
	void *tx_buf_ptr;
	int tx_buf_txing;
	atomic_t sleepers;

	spinlock_t queue_lock;
	struct sk_buff_head queue;
};

struct rpmsg_ipcc_channel {
	struct rpmsg_device rpdev;

	struct rpmsg_ipcc_device *vrp;
};

#define to_rpmsg_ipcc_channel(_rpdev) \
	container_of(_rpdev, struct rpmsg_ipcc_channel, rpdev)

#define to_rpmsg_ipcc_device(_rpdev) \
	container_of(_rpdev, struct rpmsg_ipcc_device, rpdev)

static void rpmsg_ipcc_destroy_ept(struct rpmsg_endpoint *ept);
static int rpmsg_ipcc_send(struct rpmsg_endpoint *ept, void *data, int len);
static int rpmsg_ipcc_sendto(struct rpmsg_endpoint *ept, void *data, int len,
			       u32 dst);
static int rpmsg_ipcc_send_offchannel(struct rpmsg_endpoint *ept, u32 src,
					u32 dst, void *data, int len);
static int rpmsg_ipcc_trysend(struct rpmsg_endpoint *ept, void *data, int len);
static int rpmsg_ipcc_trysendto(struct rpmsg_endpoint *ept, void *data,
				  int len, u32 dst);
static int rpmsg_ipcc_trysend_offchannel(struct rpmsg_endpoint *ept, u32 src,
					   u32 dst, void *data, int len);
static unsigned int rpmsg_ipcc_poll(struct rpmsg_endpoint *ept,
				  struct file *filp, poll_table *wait);
static ssize_t rpmsg_ipcc_get_mtu(struct rpmsg_endpoint *ept);

static const struct rpmsg_endpoint_ops rpmsg_ipcc_endpoint_ops = {
	.destroy_ept = rpmsg_ipcc_destroy_ept,
	.send = rpmsg_ipcc_send,
	.sendto = rpmsg_ipcc_sendto,
	.send_offchannel = rpmsg_ipcc_send_offchannel,
	.trysend = rpmsg_ipcc_trysend,
	.trysendto = rpmsg_ipcc_trysendto,
	.trysend_offchannel = rpmsg_ipcc_trysend_offchannel,
	.poll = rpmsg_ipcc_poll,
	.get_mtu = rpmsg_ipcc_get_mtu,
};

static int ipcc_has_feature(struct rpmsg_ipcc_device *vrp, int feature)
{
    return 1;
}

static void __ept_release(struct kref *kref)
{
	struct rpmsg_endpoint *ept = container_of(kref, struct rpmsg_endpoint,
						  refcount);
	/*
	 * At this point no one holds a reference to ept anymore,
	 * so we can directly free it
	 */
	kfree(ept);
}

/* for more info, see below documentation of rpmsg_create_ept() */
static struct rpmsg_endpoint *__rpmsg_create_ept(struct rpmsg_ipcc_device *vrp,
						 struct rpmsg_device *rpdev,
						 rpmsg_rx_cb_t cb,
						 void *priv, u32 addr)
{
	int id_min, id_max, id;
	struct rpmsg_endpoint *ept;
	struct device *dev = &rpdev->dev;

	ept = kzalloc(sizeof(*ept), GFP_KERNEL);
	if (!ept)
		return NULL;

	kref_init(&ept->refcount);
	mutex_init(&ept->cb_lock);

	ept->rpdev = rpdev;
	ept->cb = cb;
	ept->priv = priv;
	ept->ops = &rpmsg_ipcc_endpoint_ops;

	/* do we need to allocate a local address ? */
	if (addr == RPMSG_ADDR_ANY) {
		id_min = RPMSG_RESERVED_ADDRESSES;
		id_max = 0;
	} else {
		id_min = addr;
		id_max = addr + 1;
	}

	mutex_lock(&vrp->endpoints_lock);

	/* bind the endpoint to an rpmsg address (and allocate one if needed) */
	id = idr_alloc(&vrp->endpoints, ept, id_min, id_max, GFP_KERNEL);
	if (id < 0) {
		dev_err(dev, "idr_alloc failed: %d\n", id);
		goto free_ept;
	}
	ept->addr = id;

	mutex_unlock(&vrp->endpoints_lock);

	return ept;

free_ept:
	mutex_unlock(&vrp->endpoints_lock);
	kref_put(&ept->refcount, __ept_release);
	return NULL;
}

static struct rpmsg_endpoint *rpmsg_ipcc_create_ept(struct rpmsg_device *rpdev,
						      rpmsg_rx_cb_t cb,
						      void *priv,
						      struct rpmsg_channel_info chinfo)
{
	struct rpmsg_ipcc_channel *vch = to_rpmsg_ipcc_channel(rpdev);
	struct rpmsg_ipcc_device *vrp = vch->vrp;

	return __rpmsg_create_ept(vrp, rpdev, cb, priv, chinfo.src);
}

/**
 * __rpmsg_destroy_ept() - destroy an existing rpmsg endpoint
 * @vrp: virtproc which owns this ept
 * @ept: endpoing to destroy
 *
 * An internal function which destroy an ept without assuming it is
 * bound to an rpmsg channel. This is needed for handling the internal
 * name service endpoint, which isn't bound to an rpmsg channel.
 * See also __rpmsg_create_ept().
 */
static void
__rpmsg_destroy_ept(struct rpmsg_ipcc_device *vrp, struct rpmsg_endpoint *ept)
{
	/* make sure new inbound messages can't find this ept anymore */
	mutex_lock(&vrp->endpoints_lock);
	idr_remove(&vrp->endpoints, ept->addr);
	mutex_unlock(&vrp->endpoints_lock);

	/* make sure in-flight inbound messages won't invoke cb anymore */
	mutex_lock(&ept->cb_lock);
	ept->cb = NULL;
	mutex_unlock(&ept->cb_lock);

	kref_put(&ept->refcount, __ept_release);
}

static void rpmsg_ipcc_destroy_ept(struct rpmsg_endpoint *ept)
{
	struct rpmsg_ipcc_channel *vch = to_rpmsg_ipcc_channel(ept->rpdev);
	struct rpmsg_ipcc_device *vrp = vch->vrp;

	__rpmsg_destroy_ept(vrp, ept);
}

static int rpmsg_ipcc_announce_create(struct rpmsg_device *rpdev)
{
	struct rpmsg_ipcc_channel *vch = to_rpmsg_ipcc_channel(rpdev);
	struct rpmsg_ipcc_device *vrp = vch->vrp;
	struct device *dev = &rpdev->dev;
	int err = 0;

	/* need to tell remote processor's name service about this channel ? */
	if (rpdev->announce && rpdev->ept &&
	    ipcc_has_feature(vrp, RPMSG_F_NS)) {
		struct rpmsg_ipcc_nsm nsm;

//		MB_MSG_INIT_RPMSG_HEAD(&nsm.mboxhdr, vrp->rproc, sizeof(nsm), 0);
		strncpy(nsm.name, rpdev->id.name, RPMSG_NAME_SIZE);
		nsm.addr = rpdev->ept->addr;
		nsm.flags = RPMSG_NS_CREATE;

		err = rpmsg_sendto(rpdev->ept, &nsm, sizeof(nsm), RPMSG_NS_ADDR);
		if (err)
			dev_err(dev, "failed to announce service %d\n", err);
	}

	return err;
}

static int rpmsg_ipcc_announce_destroy(struct rpmsg_device *rpdev)
{
	struct rpmsg_ipcc_channel *vch = to_rpmsg_ipcc_channel(rpdev);
	struct rpmsg_ipcc_device *vrp = vch->vrp;
	struct device *dev = &rpdev->dev;
	int err = 0;

	/* tell remote processor's name service we're removing this channel */
	if (rpdev->announce && rpdev->ept &&
	    ipcc_has_feature(vrp, RPMSG_F_NS)) {
		struct rpmsg_ipcc_nsm nsm;

//		MB_MSG_INIT_RPMSG_HEAD(&nsm.mboxhdr, vrp->rproc, sizeof(nsm), 0);
		strncpy(nsm.name, rpdev->id.name, RPMSG_NAME_SIZE);
		nsm.addr = rpdev->ept->addr;
		nsm.flags = RPMSG_NS_DESTROY;

		err = rpmsg_sendto(rpdev->ept, &nsm, sizeof(nsm), RPMSG_NS_ADDR);
		if (err)
			dev_err(dev, "failed to announce service %d\n", err);
	}

	return err;
}

static const struct rpmsg_device_ops rpmsg_ipcc_devops = {
	.create_ept = rpmsg_ipcc_create_ept,
	.announce_create = rpmsg_ipcc_announce_create,
	.announce_destroy = rpmsg_ipcc_announce_destroy,
};

static void rpmsg_ipcc_release_device(struct device *dev)
{
	struct rpmsg_device *rpdev = to_rpmsg_device(dev);

	if (rpdev)
		kfree(rpdev);
}

/*
 * create an rpmsg channel using its name and address info.
 * this function will be used to create both static and dynamic
 * channels.
 */
static struct rpmsg_device *rpmsg_create_channel(struct rpmsg_ipcc_device *vrp,
						 struct rpmsg_channel_info *chinfo)
{
	struct rpmsg_ipcc_channel *vch;
	struct rpmsg_device *rpdev;
	struct device *tmp, *dev = vrp->dev;
	int ret;

	/* make sure a similar channel doesn't already exist */
	tmp = rpmsg_find_device(dev, chinfo);
	if (tmp) {
		/* decrement the matched device's refcount back */
		put_device(tmp);
		dev_err(dev, "channel %s:%x:%x already exist\n",
				chinfo->name, chinfo->src, chinfo->dst);
		return NULL;
	}

	vch = kzalloc(sizeof(*vch), GFP_KERNEL);
	if (!vch)
		return NULL;

	/* Link the channel to our vrp */
	vch->vrp = vrp;

	/* Assign public information to the rpmsg_device */
	rpdev = &vch->rpdev;
	rpdev->src = chinfo->src;
	rpdev->dst = chinfo->dst;
	rpdev->ops = &rpmsg_ipcc_devops;

	/*
	 * rpmsg server channels has predefined local address (for now),
	 * and their existence needs to be announced remotely
	 */
	rpdev->announce = rpdev->src != RPMSG_ADDR_ANY;

	strncpy(rpdev->id.name, chinfo->name, RPMSG_NAME_SIZE);

	rpdev->dev.parent = vrp->dev;
	rpdev->dev.release = rpmsg_ipcc_release_device;
	ret = rpmsg_register_device(rpdev);
	if (ret)
		return NULL;

	return rpdev;
}

/* super simple buffer "allocator" that is just enough for now */
static void *__get_tx_buf(struct rpmsg_ipcc_device *vrp)
{
	void *ret = NULL;

	/* support multiple concurrent senders */
	mutex_lock(&vrp->tx_lock);

	/*
	 * we only one tx buffer so far, if theres
	 */
	if (vrp->tx_buf_txing == 0) {
		ret = vrp->tx_buf_ptr;
		vrp->tx_buf_txing = 1;
	}

	mutex_unlock(&vrp->tx_lock);

	return ret;
}

static void __mbox_enable_tx_cb(struct rpmsg_ipcc_device *vrp)
{

}

static void __mbox_disable_tx_cb(struct rpmsg_ipcc_device *vrp)
{

}

static void __upref_sleepers(struct rpmsg_ipcc_device *vrp)
{
	/* support multiple concurrent senders */
	mutex_lock(&vrp->tx_lock);

	/* are we the first sleeping context waiting for tx buffers ? */
	if (atomic_inc_return(&vrp->sleepers) == 1)
		/* enable "tx-complete" interrupts before dozing off */
		__mbox_enable_tx_cb(vrp);

	mutex_unlock(&vrp->tx_lock);
}

static void __downref_sleepers(struct rpmsg_ipcc_device *vrp)
{
	/* support multiple concurrent senders */
	mutex_lock(&vrp->tx_lock);

	/* are we the last sleeping context waiting for tx buffers ? */
	if (atomic_dec_and_test(&vrp->sleepers))
		/* disable "tx-complete" interrupts */
		__mbox_disable_tx_cb(vrp);

	mutex_unlock(&vrp->tx_lock);
}

static int __send_offchannel_raw(struct rpmsg_ipcc_device *vrp,
                  u32 src, u32 dst, void *data, int len, bool wait)
{
	struct device *dev = vrp->dev;
	struct rpmsg_ipcc_hdr *msg;
	int err = 0;

	/* bcasting isn't allowed */
	if (src == RPMSG_ADDR_ANY || dst == RPMSG_ADDR_ANY) {
		dev_err(dev, "invalid addr (src 0x%x, dst 0x%x)\n", src, dst);
		return -EINVAL;
	}

	/*
	* We currently use fixed-sized buffers, and therefore the payload
	* length is limited.
	*
	* One of the possible improvements here is either to support
	* user-provided buffers (and then we can also support zero-copy
	* messaging), or to improve the buffer allocator, to support
	* variable-length buffer sizes.
	*/
	if (len > vrp->tx_buf_size - sizeof(struct rpmsg_ipcc_hdr)) {
		dev_err(dev, "message is too big (%d)\n", len);
		return -EMSGSIZE;
	}

	/* grab a buffer */
	msg = __get_tx_buf(vrp);
	if (!msg && !wait)
		return -ENOMEM;

	/* no free buffer ? wait for one (but bail after 15 seconds) */
	while (!msg) {
		/* enable "tx-complete" interrupts, if not already enabled */
		__upref_sleepers(vrp);

		/*
		* sleep until a free buffer is available or 15 secs elapse.
		* the timeout period is not configurable because there's
		* little point in asking drivers to specify that.
		* if later this happens to be required, it'd be easy to add.
		*/
		err = wait_event_interruptible_timeout(vrp->sendq,
				(msg = __get_tx_buf(vrp)),
				msecs_to_jiffies(15000));

		/* disable "tx-complete" interrupts if we're the last sleeper */
		__downref_sleepers(vrp);

		/* timeout ? */
		if (!err) {
			dev_err(dev, "timeout waiting for a tx buffer\n");
			return -ERESTARTSYS;
		}
	}

	MB_MSG_INIT_RPMSG_HEAD(&msg->mboxhdr, vrp->rproc, len + sizeof(*msg), 0);
	msg->len = len;
	msg->flags = 0;
	msg->src = src;
	msg->dst = dst;
	msg->reserved = 0;
	memcpy(msg->data, data, len);

	dev_dbg(dev, "TX From 0x%x, To 0x%x, Len %d, Flags %d, Reserved %d\n",
			msg->src, msg->dst, msg->len, msg->flags, msg->reserved);

#if CONFIG_IPCC_DUMP_HEX
	print_hex_dump_bytes("rpmsg_ipcc TX: ", DUMP_PREFIX_NONE,
			msg, sizeof(*msg) + msg->len);
#endif

	mutex_lock(&vrp->tx_lock);

	err = mbox_send_message(vrp->mbox, msg);

	vrp->tx_buf_txing = 0;
	mutex_unlock(&vrp->tx_lock);

	if (err < 0)
		return err;

	return 0;
}

static int rpmsg_ipcc_send_offchannel_raw(struct rpmsg_device *rpdev,
                  u32 src, u32 dst,
                  void *data, int len, bool wait)
{
	struct rpmsg_ipcc_channel *vch = to_rpmsg_ipcc_channel(rpdev);
	struct rpmsg_ipcc_device *vrp = vch->vrp;

	return __send_offchannel_raw(vrp, src, dst, data, len, true);
}

static int rpmsg_ipcc_send(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct rpmsg_device *rpdev = ept->rpdev;
	u32 src = ept->addr, dst = rpdev->dst;

	return rpmsg_ipcc_send_offchannel_raw(rpdev, src, dst, data, len, true);
}

static int rpmsg_ipcc_sendto(struct rpmsg_endpoint *ept, void *data, int len,
                u32 dst)
{
	struct rpmsg_device *rpdev = ept->rpdev;
	u32 src = ept->addr;

	return rpmsg_ipcc_send_offchannel_raw(rpdev, src, dst, data, len, true);
}

static int rpmsg_ipcc_send_offchannel(struct rpmsg_endpoint *ept, u32 src,
                 u32 dst, void *data, int len)
{
	struct rpmsg_device *rpdev = ept->rpdev;

	return rpmsg_ipcc_send_offchannel_raw(rpdev, src, dst, data, len, true);
}

static int rpmsg_ipcc_trysend(struct rpmsg_endpoint *ept, void *data, int len)
{
    struct rpmsg_device *rpdev = ept->rpdev;
    u32 src = ept->addr, dst = rpdev->dst;

    return rpmsg_ipcc_send_offchannel_raw(rpdev, src, dst, data, len, false);
}

static int rpmsg_ipcc_trysendto(struct rpmsg_endpoint *ept, void *data,
               int len, u32 dst)
{
    struct rpmsg_device *rpdev = ept->rpdev;
    u32 src = ept->addr;

    return rpmsg_ipcc_send_offchannel_raw(rpdev, src, dst, data, len, false);
}

static int rpmsg_ipcc_trysend_offchannel(struct rpmsg_endpoint *ept, u32 src,
                    u32 dst, void *data, int len)
{
    struct rpmsg_device *rpdev = ept->rpdev;

    return rpmsg_ipcc_send_offchannel_raw(rpdev, src, dst, data, len, false);
}

static unsigned int rpmsg_ipcc_poll(struct rpmsg_endpoint *ept,
				  struct file *filp, poll_table *wait)
{
	struct rpmsg_ipcc_channel *vch = to_rpmsg_ipcc_channel(ept->rpdev);
	struct rpmsg_ipcc_device *vrp = vch->vrp;
	unsigned int mask = 0;

	poll_wait(filp, &vrp->sendq, wait);

	mutex_lock(&vrp->tx_lock);

	if (!vrp->tx_buf_txing)
		mask |= POLLOUT | POLLWRNORM;

	mutex_unlock(&vrp->tx_lock);

	return mask;
}


static ssize_t rpmsg_ipcc_get_mtu(struct rpmsg_endpoint *ept)
{
	struct rpmsg_ipcc_channel *vch = to_rpmsg_ipcc_channel(ept->rpdev);
	struct rpmsg_ipcc_device *vrp = vch->vrp;

	return vrp->tx_buf_size - sizeof(struct rpmsg_ipcc_hdr);
}
static int rpmsg_ipcc_rx(struct rpmsg_ipcc_device *vrp,
			     unsigned char *data, unsigned int len)
{
	struct device *dev = vrp->dev;
	struct rpmsg_endpoint *ept;
	struct rpmsg_ipcc_hdr *msg = (struct rpmsg_ipcc_hdr *) data;

	dev_dbg(dev, "From: 0x%x, To: 0x%x, Len: %d, Flags: %d, Reserved: %d\n",
		msg->src, msg->dst, msg->len, msg->flags, msg->reserved);
#if CONFIG_IPCC_DUMP_HEX
	print_hex_dump_bytes("rpmsg_ipcc RX: ", DUMP_PREFIX_ADDRESS,
			 msg, sizeof(*msg) + msg->len);
#endif
	/*
	 * We currently use fixed-sized buffers, so trivially sanitize
	 * the reported payload length.
	 */
	if (len > vrp->tx_buf_size ||
	    msg->len > (len - sizeof(struct rpmsg_ipcc_hdr))) {
		dev_warn(dev, "inbound msg too big: (%d, %d)\n", len, msg->len);
		return -EINVAL;
	}

	/* use the dst addr to fetch the callback of the appropriate user */
	mutex_lock(&vrp->endpoints_lock);

	ept = idr_find(&vrp->endpoints, msg->dst);

	/* let's make sure no one deallocates ept while we use it */
	if (ept)
		kref_get(&ept->refcount);

	mutex_unlock(&vrp->endpoints_lock);

	if (ept) {
		/* make sure ept->cb doesn't go away while we use it */
		mutex_lock(&ept->cb_lock);

		if (ept->cb)
			ept->cb(ept->rpdev, msg->data, msg->len, ept->priv,
				msg->src);

		mutex_unlock(&ept->cb_lock);

		/* farewell, ept, we don't need you anymore */
		kref_put(&ept->refcount, __ept_release);
	} else
		dev_warn(dev, "msg received with no recipient\n");

	/* TODO: return the buffer back to memory pool */

	return 0;
}

static void __rx_work_handler(struct work_struct *work)
{
	struct rpmsg_ipcc_device *vrp = container_of(work, struct rpmsg_ipcc_device,
	                    rx_work);
	struct sk_buff *skb;
	unsigned long flags;

	/* exhaust the rx skb queue for schedule work may lost */
	for (;;) {
		spin_lock_irqsave(&vrp->queue_lock, flags);

		/* check for data in the queue */
		if (skb_queue_empty(&vrp->queue)) {
			spin_unlock_irqrestore(&vrp->queue_lock, flags);
				return;
		}

		skb = skb_dequeue(&vrp->queue);
		spin_unlock_irqrestore(&vrp->queue_lock, flags);
		if (!skb)
			return;

		rpmsg_ipcc_rx(vrp, skb->data, skb->len);

		kfree_skb(skb);
	}
}

static void __mbox_rx_cb(struct mbox_client *client, void *mssg)
{
	struct rpmsg_ipcc_device *vrp = container_of(client, struct rpmsg_ipcc_device,
						client);
	struct device *dev = client->dev;
	struct sk_buff *skb;
	sd_msghdr_t *msghdr = mssg;
	int len;
	void *tmp;

	if (!msghdr) {
		dev_err(dev, "%s NULL mssg\n", __func__);
		return;
	}

	len = msghdr->dat_len;
	dev_dbg(dev, "msghdr %p (%x %d %d %d)\n", msghdr, msghdr->addr, len,
						 msghdr->protocol, msghdr->rproc);

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return;

	tmp = skb_put(skb, len);
	memcpy_fromio(tmp, mssg, len);

	spin_lock(&vrp->queue_lock);
	skb_queue_tail(&vrp->queue, skb);
	spin_unlock(&vrp->queue_lock);

	schedule_work(&vrp->rx_work);

	return;
}

/*
 * This is invoked whenever the remote processor completed processing
 * a TX msg we just sent it, and the buffer is put back to the used ring.
 *
 * Normally, though, we suppress this "tx complete" interrupt in order to
 * avoid the incurred overhead.
 */
static void __mbox_tx_done(struct mbox_client *client, void *mssg, int r)
{
	struct rpmsg_ipcc_device *vrp = container_of(client, struct rpmsg_ipcc_device, client);

	dev_dbg(vrp->dev, "%s\n", __func__);

	/* wake up potential senders that are waiting for a tx buffer */
	wake_up_interruptible(&vrp->sendq);
}

/* invoked when a name service announcement arrives */
static int rpmsg_ipcc_ns_cb(struct rpmsg_device *rpdev, void *data, int len,
		       void *priv, u32 src)
{
	struct rpmsg_ipcc_nsm *msg = data;
	struct rpmsg_device *newch;
	struct rpmsg_channel_info chinfo;
	struct rpmsg_ipcc_device *vrp = priv;
	struct device *dev = vrp->dev;
	int ret;

#if defined(CONFIG_DYNAMIC_DEBUG)
	dynamic_hex_dump("NS announcement: ", DUMP_PREFIX_NONE, 16, 1,
			 data, len, true);
#endif

	dev_info(dev, "rpmsg_ipcc_ns_cb in\n");

	if (len != sizeof(*msg)) {
		dev_err(dev, "malformed ns msg (%d)\n", len);
		return -EINVAL;
	}

	/*
	 * the name service ept does _not_ belong to a real rpmsg channel,
	 * and is handled by the rpmsg bus itself.
	 * for sanity reasons, make sure a valid rpdev has _not_ sneaked
	 * in somehow.
	 */
	if (rpdev) {
		dev_err(dev, "anomaly: ns ept has an rpdev handle\n");
		return -EINVAL;
	}

	/* don't trust the remote processor for null terminating the name */
	msg->name[RPMSG_NAME_SIZE - 1] = '\0';

	dev_info(dev, "%sing channel %s addr 0x%x\n",
		 msg->flags & RPMSG_NS_DESTROY ? "destroy" : "creat",
		 msg->name, msg->addr);

	strncpy(chinfo.name, msg->name, sizeof(chinfo.name));
	chinfo.src = RPMSG_ADDR_ANY;
	chinfo.dst = msg->addr;

	if (msg->flags & RPMSG_NS_DESTROY) {
		ret = rpmsg_unregister_device(vrp->dev, &chinfo);
		if (ret)
			dev_err(dev, "rpmsg_destroy_channel failed: %d\n", ret);
	} else {
		newch = rpmsg_create_channel(vrp, &chinfo);
		if (!newch)
			dev_err(dev, "rpmsg_create_channel failed\n");
	}

	return 0;
}

/* invoked when a echo request arrives */
static int rpmsg_ipcc_echo_cb(struct rpmsg_device *rpdev, void *data, int len,
		       void *priv, u32 src)
{
	struct rpmsg_ipcc_device *vrp = priv;
	struct device *dev = vrp->dev;
	struct rpmsg_ipcc_hdr *msg;
	void *msg_ptr;
	int err = 0;

	dev_info(dev, "%s in\n", __func__);
	if (len == 0) {
		dev_err(dev, "malformed echo msg (%d)\n", len);
		return -EINVAL;
	}

	if (rpdev) {
		dev_err(dev, "anomaly: echo ept has an rpdev handle\n");
		return -EINVAL;
	}

	msg_ptr = kzalloc(sizeof(*msg) + len, GFP_ATOMIC);
	if (!msg_ptr)
		return -ENOMEM;

	msg = msg_ptr;
	MB_MSG_INIT_RPMSG_HEAD(&msg->mboxhdr, vrp->rproc, sizeof(*msg) + len, 0);
	msg->src = RPMSG_ECHO_ADDR;
	msg->dst = src;
	msg->len = len;

	err = __send_offchannel_raw(vrp, RPMSG_ECHO_ADDR, src, data, len, true);
	if (err)
		dev_err(dev, "failed to echo service %d\n", err);

	kfree(msg_ptr);
	return 0;
}

static int rpmsg_ipcc_probe(struct platform_device *pdev)
{
	struct rpmsg_ipcc_device *vrp;
	struct mbox_client *client;
	int ret = 0;

	vrp = kzalloc(sizeof(struct rpmsg_ipcc_device), GFP_KERNEL);
	if (!vrp) {
		dev_err(&pdev->dev, "no memory to probe\n");
		return -ENOMEM;
	}

	vrp->tx_buf_size = RPMSG_TX_BUF_SIZE;
	vrp->tx_buf_ptr = kzalloc(vrp->tx_buf_size, GFP_KERNEL);
	if (IS_ERR(vrp->tx_buf_ptr)) {
		dev_err(&pdev->dev, "tx buf malloc failed: %ld\n", PTR_ERR(vrp->tx_buf_ptr));
		ret = -ENOMEM;
		goto fail_out;
	}

	idr_init(&vrp->endpoints);
	mutex_init(&vrp->endpoints_lock);
	mutex_init(&vrp->tx_lock);
	init_waitqueue_head(&vrp->sendq);
	INIT_WORK(&vrp->rx_work, __rx_work_handler);

	spin_lock_init(&vrp->queue_lock);
	skb_queue_head_init(&vrp->queue);

	client = &vrp->client;
	/* Initialize the mbox unit used by rpmsg */
	client->dev = &pdev->dev;
	client->tx_done = __mbox_tx_done;
	client->rx_callback = __mbox_rx_cb;
	client->tx_block = true;
	client->tx_tout = 1000;
	client->knows_txdone = false;
	vrp->mbox = mbox_request_channel(client, 0);
	if (IS_ERR(vrp->mbox)) {
		ret = -EBUSY;
		dev_err(&pdev->dev, "mbox_request_channel failed: %ld\n", PTR_ERR(vrp->mbox));
		goto fail_out;
	}
	vrp->dev = &pdev->dev;
	platform_set_drvdata(pdev, vrp);

	/* if supported by the remote processor, enable the name service */
	if (ipcc_has_feature(vrp, RPMSG_F_NS)) {
		/* a dedicated endpoint handles the name service msgs */
		vrp->ns_ept = __rpmsg_create_ept(vrp, NULL, rpmsg_ipcc_ns_cb,
						vrp, RPMSG_NS_ADDR);
		if (!vrp->ns_ept) {
			dev_err(&pdev->dev, "failed to create the ns ept\n");
			ret = -ENOMEM;
		}
	}

	/* if supported by the remote processor, enable the echo service */
	if (ipcc_has_feature(vrp, RPMSG_F_ECHO)) {
		/* a dedicated endpoint handles the echo msgs */
		vrp->echo_ept = __rpmsg_create_ept(vrp, NULL, rpmsg_ipcc_echo_cb,
						vrp, RPMSG_ECHO_ADDR);
		if (!vrp->echo_ept) {
			dev_err(&pdev->dev, "failed to create the echo ept\n");
			ret = -ENOMEM;
		}
	}

	/* need to tell remote processor's name service about this channel ? */
	if (ipcc_has_feature(vrp, RPMSG_F_NS)) {
		struct rpmsg_ipcc_nsm nsm;

//		MB_MSG_INIT_RPMSG_HEAD(&nsm.mboxhdr, vrp->rproc, sizeof(nsm), 0);
		strncpy(nsm.name, "rpmsg-ipcc-echo", RPMSG_NAME_SIZE);
		nsm.addr = RPMSG_ECHO_ADDR;
		nsm.flags = RPMSG_NS_CREATE;
		ret = __send_offchannel_raw(vrp, RPMSG_ECHO_ADDR, RPMSG_NS_ADDR, &nsm, sizeof(nsm), true);
		if (ret)
			dev_err(&pdev->dev, "failed to announce ns service %d\n", ret);
	}

	return ret;

fail_out:
	if (vrp->mbox)
		mbox_free_channel(vrp->mbox);

	if (vrp->tx_buf_ptr)
		kfree(vrp->tx_buf_ptr);

	if (vrp)
		kfree(vrp);

	return ret;
}

static int rpmsg_ipcc_remove_device(struct device *dev, void *data)
{
	device_unregister(dev);

	return 0;
}

/*
 * Shut down all clients by making sure that each edge stops processing
 * events and scanning for new channels, then call destroy on the devices.
 */
static int rpmsg_ipcc_remove(struct platform_device *pdev)
{
	int ret;
	struct rpmsg_ipcc_device *vrp = platform_get_drvdata(pdev);
	struct sk_buff *skb;

	if (!vrp)
		return -ENODEV;

	ret = device_for_each_child(vrp->dev, NULL, rpmsg_ipcc_remove_device);
	if (ret)
		dev_warn(vrp->dev, "can't remove rpmsg device: %d\n", ret);

	if (vrp->ns_ept)
		__rpmsg_destroy_ept(vrp, vrp->ns_ept);

	/* Discard all SKBs */
	while (!skb_queue_empty(&vrp->queue)) {
		skb = skb_dequeue(&vrp->queue);
		kfree_skb(skb);
	}

	if (vrp->mbox)
		mbox_free_channel(vrp->mbox);

	idr_destroy(&vrp->endpoints);

	kfree(vrp);

	return ret;
}

static const struct of_device_id rpmsg_ipcc_of_ids[] = {
	{ .compatible = "sd,rpmsg-ipcc"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rpmsg_ipcc_of_ids);

static struct platform_driver rpmsg_ipcc_driver = {
	.probe = rpmsg_ipcc_probe,
	.remove = rpmsg_ipcc_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sd,rpmsg-ipcc",
		.of_match_table = rpmsg_ipcc_of_ids,
	},
};

static int __init rpmsg_ipcc_init(void)
{
	int ret;

	ret = platform_driver_register(&rpmsg_ipcc_driver);
	if (ret)
		pr_err("Unable to initialize rpmsg ipcc\n");
	else
		pr_info("Semidrive rpmsg ipcc is registered.\n");

	return ret;
}

arch_initcall(rpmsg_ipcc_init);

static void __exit rpmsg_ipcc_exit(void)
{
	platform_driver_unregister(&rpmsg_ipcc_driver);
}
module_exit(rpmsg_ipcc_exit);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive rpmsg IPCC Driver");
MODULE_LICENSE("GPL v2");
