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
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_ring.h>
#include <linux/mailbox_client.h>
#include <linux/sd_rpmsg.h>
#include <linux/soc/semidrive/mb_msg.h>

#define CONFIG_RPMSG_DUMP_HEX		(0)

enum sd_rpmsg_variants {
	X9HI,
	X9MID,
	X9ECO,
};
static enum sd_rpmsg_variants variant;

struct sd_virtdev {
	struct virtio_device vdev;
	unsigned int vring[2];
	struct virtqueue *vq[2];
	int base_vq_id;
	int num_of_vqs;
	struct notifier_block nb;
};

struct sd_rpmsg_vproc {
	char *rproc_name;
	int rproc_id;
	char *mbox_name;
	struct mbox_chan *mbox;
	struct mbox_client client;
	struct mutex lock;
	int vdev_nums;
#define MAX_VDEV_NUMS	4
	struct sd_virtdev ivdev[MAX_VDEV_NUMS];
};

struct sd_rpmsg_mbox {
	const char *name;
	struct blocking_notifier_head notifier;
};

static struct sd_rpmsg_mbox rpmsg_mbox = {
	.name	= "rp_sec",
};

struct sd_notify_msg {
	sd_msghdr_t msghead;
	__u16 device_type;
	__u16 vq_id;
} __attribute__((packed));

#define MAX_NUM 10	/* enlarge it if overflow happen */
static __u64 rproc_message[MAX_NUM];
static __u32 in_idx, out_idx;
static DEFINE_SPINLOCK(rpmsg_mb_lock);
static struct delayed_work rpmsg_work;

/*
 * For now, allocate 256 buffers of 512 bytes for each side. each buffer
 * will then have 16B for the msg header and 496B for the payload.
 * This will require a total space of 256KB for the buffers themselves, and
 * 3 pages for every vring (the size of the vring depends on the number of
 * buffers it supports).
 */
#define RPMSG_NUM_BUFS		(512)
#define RPMSG_BUF_SIZE		(512)
#define RPMSG_BUFS_SPACE	(RPMSG_NUM_BUFS * RPMSG_BUF_SIZE)

/*
 * The alignment between the consumer and producer parts of the vring.
 * Note: this is part of the "wire" protocol. If you change this, you need
 * to update your BIOS image as well
 */
#define RPMSG_VRING_ALIGN	(4096)

/* With 256 buffers, our vring will occupy 3 pages */
#define RPMSG_RING_SIZE	((DIV_ROUND_UP(vring_size(RPMSG_NUM_BUFS / 2, \
				RPMSG_VRING_ALIGN), PAGE_SIZE)) * PAGE_SIZE)

#define to_sd_virtdev(vd) container_of(vd, struct sd_virtdev, vdev)
#define to_sd_rpdev(vd, id) container_of(vd, struct sd_rpmsg_vproc, ivdev[id])

struct sd_rpmsg_vq_info {
	__u16 num;	/* number of entries in the virtio_ring */
	__u16 vq_id;	/* a globaly unique index of this virtqueue */
	void *addr;	/* address where we mapped the virtio ring */
	struct sd_rpmsg_vproc *rpdev;
};

static u64 sd_rpmsg_get_features(struct virtio_device *vdev)
{
	/* VIRTIO_RPMSG_F_NS has been made private */
	return 1 << 0;
}

static int sd_rpmsg_finalize_features(struct virtio_device *vdev)
{
	/* Give virtio_ring a chance to accept features */
	vring_transport_features(vdev);
	return 0;
}

/* kick the remote processor, and let it know which virtqueue to poke at */
static bool sd_rpmsg_notify(struct virtqueue *vq)
{
	struct sd_rpmsg_vq_info *rpvq = vq->priv;
	struct sd_notify_msg msg;
	struct sd_rpmsg_vproc *rpdev = rpvq->rpdev;
	int ret;

	MB_MSG_INIT_RPMSG_HEAD(&msg.msghead, rpdev->rproc_id,
			sizeof(struct sd_notify_msg), IPCC_ADDR_RPMSG);
	msg.vq_id = rpvq->vq_id;
	msg.device_type = 0x99;

	mutex_lock(&rpdev->lock);
	/* send the index of the triggered virtqueue as the mbox payload */
	ret = mbox_send_message(rpdev->mbox, &msg);
	mutex_unlock(&rpdev->lock);

	/* in case of TIMEOUT because remote proc may be off */
	if (ret < 0) {
		pr_info("%s rproc %s may be off\n", __func__, rpdev->rproc_name);
		return false;
	}
	return true;
}

static int sd_rpmsg_callback(struct notifier_block *this,
					unsigned long index, void *data)
{
	struct sd_virtdev *virdev;
	__u64 msg_data = (__u64) data;
	struct sd_notify_msg *msg = (struct sd_notify_msg*) &msg_data;
	__u16 vq_id;

#if CONFIG_RPMSG_DUMP_HEX
 	print_hex_dump_bytes("rpmsg : cb: ", DUMP_PREFIX_ADDRESS,
 				msg, 8);
#endif
	virdev = container_of(this, struct sd_virtdev, nb);
	/* ignore vq indices which are clearly not for us */
	vq_id = msg->vq_id;
	if (vq_id < virdev->base_vq_id || vq_id > virdev->base_vq_id + 1) {
		pr_err("vq_id: 0x%x is invalid\n", vq_id);
		return NOTIFY_DONE;
	}

	vq_id -= virdev->base_vq_id;

	pr_info("%s vqid: %d rproc: %d nvqs: %d\n", __func__, msg->vq_id,
				msg->msghead.rproc, virdev->num_of_vqs);

	/*
	 * Currently both PENDING_MSG and explicit-virtqueue-index
	 * messaging are supported.
	 * Whatever approach is taken, at this point 'mu_msg' contains
	 * the index of the vring which was just triggered.
	 */
	if (vq_id < virdev->num_of_vqs) {
		vring_interrupt(vq_id, virdev->vq[vq_id]);
	}
	return NOTIFY_DONE;
}

int sd_rpmsg_register_nb(const char *name, struct notifier_block *nb)
{
	if ((name == NULL) || (nb == NULL))
		return -EINVAL;

	if (!strcmp(rpmsg_mbox.name, name))
		blocking_notifier_chain_register(&(rpmsg_mbox.notifier), nb);
	else
		return -ENOENT;

	return 0;
}

int sd_rpmsg_unregister_nb(const char *name, struct notifier_block *nb)
{
	if ((name == NULL) || (nb == NULL))
		return -EINVAL;

	if (!strcmp(rpmsg_mbox.name, name))
		blocking_notifier_chain_unregister(&(rpmsg_mbox.notifier),
				nb);
	else
		return -ENOENT;

	return 0;
}

static struct virtqueue *rp_find_vq(struct virtio_device *vdev,
				    unsigned int index,
				    void (*callback)(struct virtqueue *vq),
				    const char *name, bool ctx)
{
	struct sd_virtdev *virdev = to_sd_virtdev(vdev);
	struct sd_rpmsg_vproc *rpdev = to_sd_rpdev(virdev,
						     virdev->base_vq_id / 2);
	struct sd_rpmsg_vq_info *rpvq;
	struct virtqueue *vq;
	int err;

	rpvq = kmalloc(sizeof(*rpvq), GFP_KERNEL);
	if (!rpvq)
		return ERR_PTR(-ENOMEM);

	/* ioremap'ing normal memory, so we cast away sparse's complaints */
	rpvq->addr = (__force void *) ioremap_nocache(virdev->vring[index],
							RPMSG_RING_SIZE);
	if (!rpvq->addr) {
		err = -ENOMEM;
		goto free_rpvq;
	}

	memset_io(rpvq->addr, 0, RPMSG_RING_SIZE);

	pr_debug("vring%d: phys 0x%x, virt 0x%p\n", index, virdev->vring[index],
					rpvq->addr);

	vq = vring_new_virtqueue(index, RPMSG_NUM_BUFS / 2, RPMSG_VRING_ALIGN,
			vdev, true, ctx, rpvq->addr, sd_rpmsg_notify, callback,
			name);
	if (!vq) {
		pr_err("vring_new_virtqueue failed\n");
		err = -ENOMEM;
		goto unmap_vring;
	}

	virdev->vq[index] = vq;
	vq->priv = rpvq;
	/* system-wide unique id for this virtqueue */
	rpvq->vq_id = virdev->base_vq_id + index;
	rpvq->rpdev = rpdev;
	mutex_init(&rpdev->lock);

	return vq;

unmap_vring:
	/* iounmap normal memory, so make sparse happy */
	iounmap((__force void __iomem *) rpvq->addr);
free_rpvq:
	kfree(rpvq);
	return ERR_PTR(err);
}

static void sd_rpmsg_del_vqs(struct virtio_device *vdev)
{
	struct virtqueue *vq, *n;
	struct sd_virtdev *virdev = to_sd_virtdev(vdev);
	struct sd_rpmsg_vproc *rpdev = to_sd_rpdev(virdev,
						     virdev->base_vq_id / 2);

	list_for_each_entry_safe(vq, n, &vdev->vqs, list) {
		struct sd_rpmsg_vq_info *rpvq = vq->priv;

		iounmap(rpvq->addr);
		vring_del_virtqueue(vq);
		kfree(rpvq);
	}

	if (&virdev->nb)
		sd_rpmsg_unregister_nb((const char *)rpdev->rproc_name,
				&virdev->nb);
}

static int sd_rpmsg_find_vqs(struct virtio_device *vdev, unsigned int nvqs,
				struct virtqueue *vqs[],
				vq_callback_t *callbacks[],
				const char * const names[],
				const bool * ctx,
				struct irq_affinity *desc)
{
	struct sd_virtdev *virdev = to_sd_virtdev(vdev);
	struct sd_rpmsg_vproc *rpdev = to_sd_rpdev(virdev,
						     virdev->base_vq_id / 2);
	int i, err;

	/* we maintain two virtqueues per remote processor (for RX and TX) */
	if (nvqs != 2)
		return -EINVAL;

	for (i = 0; i < nvqs; ++i) {
		vqs[i] = rp_find_vq(vdev, i, callbacks[i], names[i], ctx ? ctx[i] : false);
		if (IS_ERR(vqs[i])) {
			err = PTR_ERR(vqs[i]);
			goto error;
		}
	}

	virdev->num_of_vqs = nvqs;

	virdev->nb.notifier_call = sd_rpmsg_callback;
	sd_rpmsg_register_nb((const char *)rpdev->rproc_name, &virdev->nb);

	return 0;

error:
	sd_rpmsg_del_vqs(vdev);
	return err;
}

static void sd_rpmsg_reset(struct virtio_device *vdev)
{
	dev_dbg(&vdev->dev, "reset !\n");
}

static u8 sd_rpmsg_get_status(struct virtio_device *vdev)
{
	return 0;
}

static void sd_rpmsg_set_status(struct virtio_device *vdev, u8 status)
{
	dev_dbg(&vdev->dev, "%s new status: %d\n", __func__, status);
}

static void sd_rpmsg_vproc_release(struct device *dev)
{
	/* this handler is provided so driver core doesn't yell at us */
}

static struct virtio_config_ops sd_rpmsg_config_ops = {
	.get_features	= sd_rpmsg_get_features,
	.finalize_features = sd_rpmsg_finalize_features,
	.find_vqs	= sd_rpmsg_find_vqs,
	.del_vqs	= sd_rpmsg_del_vqs,
	.reset		= sd_rpmsg_reset,
	.set_status	= sd_rpmsg_set_status,
	.get_status	= sd_rpmsg_get_status,
};

static struct sd_rpmsg_vproc sd_rpmsg_vprocs[] = {
	{
		.rproc_name	= "rp_sec",
		.rproc_id	= 1,
	}
};

static const struct of_device_id sd_rpmsg_dt_ids[] = {
	{ .compatible = "sd,rpmsg-x9",  .data = (void *)X9HI, },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sd_rpmsg_dt_ids);

/* contains pool of descriptors and two circular buffers */
#ifndef VRING_SIZE
#define VRING_SIZE (0x8000)
#endif

static int set_vring_phy_buf(struct platform_device *pdev,
		       struct sd_rpmsg_vproc *rpdev, int index)
{
	struct resource *res;
	resource_size_t size;
	unsigned int start, end;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		size = resource_size(res);
		start = res->start;
		end = res->start + size;
		rpdev->ivdev[0].vring[0] = start; /* rx vring first */
		rpdev->ivdev[0].vring[1] = start + VRING_SIZE; /* tx vring */
	} else {
		pr_err("%s: no rpmsg mem resource!\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static void rpmsg_work_handler(struct work_struct *work)
{
	__u64 message;
	unsigned long flags;

	spin_lock_irqsave(&rpmsg_mb_lock, flags);
	/* handle all incoming mbox message */
	while (in_idx != out_idx) {
		message = rproc_message[out_idx % MAX_NUM];
		spin_unlock_irqrestore(&rpmsg_mb_lock, flags);

		blocking_notifier_call_chain(&(rpmsg_mbox.notifier), 8,
						(void *)message);

		spin_lock_irqsave(&rpmsg_mb_lock, flags);
		rproc_message[out_idx % MAX_NUM] = 0;
		out_idx++;
	}
	spin_unlock_irqrestore(&rpmsg_mb_lock, flags);
}

static void sd_rpmsg_mbox_callback(struct mbox_client *client, void *mssg)
{
	struct sd_rpmsg_vproc *rpdev = container_of(client, struct sd_rpmsg_vproc,
						client);
	struct device *dev = client->dev;
	__u64 message;
	unsigned long flags;

	pr_debug("rpmsg cb msg from: %s\n", rpdev->rproc_name);

	memcpy(&message, mssg, 8);
	spin_lock_irqsave(&rpmsg_mb_lock, flags);
	/* get message from receive buffer */
	rproc_message[in_idx % MAX_NUM] = message;
	in_idx++;
	/*
	 * Too many mu message not be handled in timely, can enlarge
	 * MAX_NUM
	 */
	if (in_idx == out_idx) {
		spin_unlock_irqrestore(&rpmsg_mb_lock, flags);
		pr_err("mbox msg overflow!\n");
		return;
	}
	spin_unlock_irqrestore(&rpmsg_mb_lock, flags);

	schedule_delayed_work(&rpmsg_work, 0);

	return;
}

static int sd_rpmsg_probe(struct platform_device *pdev)
{
	int i, j, ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;

	variant = (enum sd_rpmsg_variants)of_device_get_match_data(dev);

	INIT_DELAYED_WORK(&rpmsg_work, rpmsg_work_handler);

	BLOCKING_INIT_NOTIFIER_HEAD(&(rpmsg_mbox.notifier));

	pr_info("Semidrive rpmsg is initializing!\n");

	for (i = 0; i < ARRAY_SIZE(sd_rpmsg_vprocs); i++) {
		struct sd_rpmsg_vproc *rpdev = &sd_rpmsg_vprocs[i];
		struct mbox_client *client = &rpdev->client;

		/* Initialize the mbox unit used by rpmsg */
		client->dev = dev;
		client->tx_done = NULL;
		client->rx_callback = sd_rpmsg_mbox_callback;
		client->tx_block = true;
		client->tx_tout = 1000;
		client->knows_txdone = false;

		rpdev->mbox = mbox_request_channel_byname(client, rpdev->rproc_name);
		if (IS_ERR(rpdev->mbox)) {
			ret = -EBUSY;
			pr_err("mbox_request_channel failed: %ld\n",
				PTR_ERR(rpdev->mbox));
			return ret;
		}

		rpdev->vdev_nums = 1;
		if (!strcmp(rpdev->rproc_name, "rp_sec")) {
			ret = set_vring_phy_buf(pdev, rpdev, 0);
			if (ret) {
				pr_err("No vring buffer.\n");
				return -ENOMEM;
			}
		} else {
			pr_err("No remote processor.\n");
			return -ENODEV;
		}

		if (of_reserved_mem_device_init_by_idx(dev, np, 0)) {
			pr_err("dev doesn't have specific DMA pool.\n");
			ret = -ENOMEM;
			return ret;
		}

		for (j = 0; j < rpdev->vdev_nums; j++) {
			pr_info("%s rpdev%d vdev%d: vring0 0x%x, vring1 0x%x\n",
				 __func__, i, rpdev->vdev_nums,
				 rpdev->ivdev[j].vring[0],
				 rpdev->ivdev[j].vring[1]);
			rpdev->ivdev[j].vdev.id.device = VIRTIO_ID_RPMSG;
			rpdev->ivdev[j].vdev.config = &sd_rpmsg_config_ops;
			rpdev->ivdev[j].vdev.dev.parent = &pdev->dev;
			rpdev->ivdev[j].vdev.dev.release = sd_rpmsg_vproc_release;
			rpdev->ivdev[j].base_vq_id = j * 2;

			ret = register_virtio_device(&rpdev->ivdev[j].vdev);
			if (ret) {
				pr_err("%s failed to register rpdev: %d\n",
						__func__, ret);
				return ret;
			}
		}
	}

	platform_set_drvdata(pdev, sd_rpmsg_vprocs);

	return ret;
}

static struct platform_driver sd_rpmsg_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "sd-rpmsg",
		   .of_match_table = sd_rpmsg_dt_ids,
		   },
	.probe = sd_rpmsg_probe,
};

static int __init sd_rpmsg_init(void)
{
	int ret;

	ret = platform_driver_register(&sd_rpmsg_driver);
	if (ret)
		pr_err("Unable to initialize rpmsg driver\n");
	else
		pr_info("Semidrive rpmsg driver is registered.\n");

	return ret;
}

subsys_initcall(sd_rpmsg_init);

MODULE_AUTHOR("Semidrive Semiconductor, Inc.");
MODULE_DESCRIPTION("X9 rpmsg virtio device");
MODULE_LICENSE("GPL v2");
