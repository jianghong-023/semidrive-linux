/*
 * Copyright (C) 2020 Semidrive Semiconductor
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/kfifo.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/ioctl.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>

/* net device info */
#include <linux/in.h>
#include <linux/netdevice.h>   /* struct device, and other headers */
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/ip.h>          /* struct iphdr */
#include <linux/tcp.h>         /* struct tcphdr */
#include <linux/skbuff.h>
#include <linux/in6.h>
#include <asm/checksum.h>
#include <net/arp.h>
#include <linux/if_ether.h>
#include <net/pkt_sched.h>

#define RPMSG_ETHERNET_EPT		60
#define RPMSG_ETHERNET_NAME		"rpmsg-net"

struct rpmsg_eth_device {
	struct rpmsg_device *rpmsg_chnl;
	struct rpmsg_endpoint *ether_ept;
	struct net_device_stats stats;
	struct net_device *dev;
	struct rpmsg_channel_info chinfo;
	/* MTU is for rpmsg payload */
	int rpmsg_mtu;

	/* ether frame fragment */
	bool frag;
	uint32_t frame_len;
	uint32_t frame_received;
	struct sk_buff *skb_stage;

	spinlock_t lock;
	struct sk_buff_head txqueue;
	struct work_struct tx_work;
};

struct rpmsg_net_device {
	struct rpmsg_device *rpmsg_chnl;
	struct net_device *ether_dev;
};


#define PL_TYPE_FRAG_INDICATOR	(1)
#define PL_TYPE_ENTIRE_PACK		(2)
#define PL_TYPE_FRAG_END		(4)

#define PL_MAGIC			(0x88)
struct rpmsg_eth_header {
	uint8_t type;
	uint8_t	magic;
	uint16_t len;
};

/*
 * The higher levels take care of making this non-reentrant (it's
 * called with bh's disabled).
 */
static netdev_tx_t rpmsg_eth_xmit(struct sk_buff *skb,
                            struct net_device *ndev)
{
	struct rpmsg_eth_device *eth = netdev_priv(ndev);

	spin_lock_bh(&eth->lock);

	skb_queue_tail(&eth->txqueue, skb);

	spin_unlock_bh(&eth->lock);

	schedule_work(&eth->tx_work);

	return NETDEV_TX_OK;
}

static int rpmsg_eth_open(struct net_device *ndev)
{
	netif_start_queue(ndev);

	return 0;
}


int rpmsg_eth_stop(struct net_device *ndev)
{
	pr_info ("stop called\n");
	netif_stop_queue(ndev);
	return 0;
}

/*
 * Configuration changes (passed on by ifconfig)
 */
int rpmsg_eth_config(struct net_device *ndev, struct ifmap *map)
{
	if (ndev->flags & IFF_UP) /* can't act on a running interface */
		return -EBUSY;

	/* ignore other fields */
	return 0;
}

static int rpmsg_eth_set_mac_address(struct net_device *ndev, void *addr)
{
	int ret = 0;

	ret = eth_mac_addr(ndev, addr);

	return ret;

}

struct net_device_stats *rpmsg_eth_stats(struct net_device *ndev)
{
	struct rpmsg_eth_device *priv = netdev_priv(ndev);
	return &priv->stats;
}

static u32 always_on(struct net_device *ndev)
{
	return 1;
}

static const struct ethtool_ops rpmsg_ethtool_ops = {
	.get_link           = always_on,
};

static const struct net_device_ops rpmsg_eth_ops = {
	.ndo_open           = rpmsg_eth_open,
	.ndo_stop           = rpmsg_eth_stop,
	.ndo_start_xmit     = rpmsg_eth_xmit,
	.ndo_set_config     = rpmsg_eth_config,
	.ndo_set_mac_address = rpmsg_eth_set_mac_address,
	.ndo_validate_addr  = eth_validate_addr,
	.ndo_get_stats      = rpmsg_eth_stats,
};

static struct sk_buff *
rpmsg_eth_alloc_skb(struct rpmsg_net_device *rndev, unsigned int length)
{
	struct net_device *ndev = rndev->ether_dev;
	struct rpmsg_eth_device *eth;
	struct sk_buff *skb;

	eth = netdev_priv(ndev);
	skb = netdev_alloc_skb_ip_align(ndev, eth->frame_len);
	if (!skb) {
		/* skb alloc fail, will drop contiguous fragment */
		eth->stats.rx_errors++;
		eth->stats.rx_dropped += eth->frame_len;
		return NULL;
	}

	return skb;
}

static int rpmsg_eth_rx_cb(struct rpmsg_device *rpdev, void *data,
                                        int len, void *priv, u32 src)
{
	struct rpmsg_net_device *rndev = dev_get_drvdata(&rpdev->dev);
	struct rpmsg_eth_device *eth = priv;
	struct net_device *ndev = eth->dev;
	struct rpmsg_eth_header *hdr;
	struct sk_buff *skb;

	if ((eth->frag == false) && len == sizeof(*hdr)) {
		hdr = (struct rpmsg_eth_header *)data;
		if (hdr->magic == PL_MAGIC) {
			eth->frag = true;
			eth->frame_len = hdr->len;
			eth->frame_received = 0;

			spin_lock_bh(&eth->lock);
			eth->skb_stage = rpmsg_eth_alloc_skb(rndev, eth->frame_len);
			if (!eth->skb_stage) {
				/* skb alloc fail, will drop contiguous fragment */
				spin_unlock_bh(&eth->lock);
				dev_err(&ndev->dev, "ERROR: skbuf, drop contiguous frags\n");
				return -ENOMEM;
			}

			spin_unlock_bh(&eth->lock);
			dev_dbg(&ndev->dev, "RX large frame len=%d\n", eth->frame_len);
			return 0;
		}
		/* Should not be here, fallback to normal frame */
		dev_err(&ndev->dev, "ERROR: wrong fragment header\n");
	}

	/* Receive contiguous fragments by rpmsg MTU */
	if ((eth->frag == true)) {
		if (eth->skb_stage) {
			/* Assembly fragments into one ether frame */
			if (eth->frame_received < eth->frame_len) {
				memcpy(skb_put(eth->skb_stage, len), data, len);
				eth->frame_received += len;
			}

			dev_dbg(&ndev->dev, "RX frame assembled=%d(=%d?), len=%d\n",
				eth->frame_received, eth->frame_len, len);
			/* last fragment to complete ether frame */
			if (eth->frame_received >= eth->frame_len) {
				eth->frag = false;
				skb = eth->skb_stage;
				eth->skb_stage = NULL;
				spin_lock_bh(&eth->lock);
				goto ether_frame_recieved;
			}
			return 0;
		}

		/* skb alloc fail previously, drop contiguous fragment */
		eth->frame_received += len;
		if (eth->frame_received >= eth->frame_len)
			eth->frag = false;
		return -EINVAL;
	}

	/* Rx single frame packet */
	spin_lock_bh(&eth->lock);
	eth->frame_len = len;

	skb = rpmsg_eth_alloc_skb(rndev, eth->frame_len);
	if (!skb) {
		spin_unlock_bh(&eth->lock);
		dev_err(&ndev->dev, "ERROR: skbuf, drop the frame\n");
		return -ENOMEM;
	}

	memcpy(skb_put(skb, len), data, len);

ether_frame_recieved:

	skb->protocol = eth_type_trans(skb, rndev->ether_dev);
	skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */
	eth->stats.rx_packets++;
	eth->stats.rx_bytes += eth->frame_len;
	netif_rx(skb);
	spin_unlock_bh(&eth->lock);

	dev_dbg(&ndev->dev, "RX frame len=%d\n", eth->frame_len);
	return 0;
}

static void eth_xmit_work_handler(struct work_struct *work)
{
	struct rpmsg_eth_device *eth = container_of(work, struct rpmsg_eth_device,
	                    tx_work);
	struct net_device *ndev = eth->dev;
	struct sk_buff *skb;
	int len = 0, send_bytes, cc;
	int err = 0;
	struct rpmsg_eth_header frhdr;

	for (;;) {
		spin_lock_bh(&eth->lock);
		/* check for data in the queue */
		if (skb_queue_empty(&eth->txqueue)) {
			spin_unlock_bh(&eth->lock);
			break;
		}

		skb = skb_dequeue(&eth->txqueue);
		if (!skb) {
			spin_unlock_bh(&eth->lock);
			break;
		}

		len = skb->len;
		eth->stats.tx_packets++;
		eth->stats.tx_bytes += len;
		spin_unlock_bh(&eth->lock);
		dev_dbg(&ndev->dev, "TX frame len=%d\n", len);

		if (len > eth->rpmsg_mtu) {
			frhdr.type = PL_TYPE_FRAG_INDICATOR;
			frhdr.magic = PL_MAGIC;
			frhdr.len = len;
			cc = 0;

			err = rpmsg_trysendto(eth->ether_ept, &frhdr, sizeof(frhdr), eth->chinfo.dst);
			if (err < 0)
				dev_err(&ndev->dev, "ERROR: %d rpmsg_trysendto\n", err);

			dev_dbg(&ndev->dev, "TX large frame len=%d\n", len);

			while (len) {
				if (len > eth->rpmsg_mtu)
					send_bytes = eth->rpmsg_mtu;
				else
					send_bytes = len;

				err = rpmsg_trysendto(eth->ether_ept, skb->data + cc, send_bytes, eth->chinfo.dst);
				if (err < 0)
					dev_err(&ndev->dev, "ERROR: rc=%d cc=%d len=%d\n", err, cc, send_bytes);

				len -= send_bytes;
				cc += send_bytes;
				dev_dbg(&ndev->dev, "TX cc=%d, send=%d left=%d\n", cc, send_bytes, len);

			}
		} else {
			err = rpmsg_trysendto(eth->ether_ept, skb->data, len, eth->chinfo.dst);
			if (err < 0)
				dev_err(&ndev->dev, "ERROR: rc=%d no len=%d\n", err, len);
		}

		dev_kfree_skb_any(skb);
	}
}

static struct device_type rpmsg_eth_type = {
	.name = "rpmsg",
};

static void rpmsg_netdev_setup(struct net_device *ndev)
{
	SET_NETDEV_DEVTYPE(ndev, &rpmsg_eth_type);
	ndev->header_ops		= &eth_header_ops;
	ndev->type		= ARPHRD_ETHER;
	ndev->hard_header_len 	= ETH_HLEN;
	ndev->min_header_len	= ETH_HLEN;
	ndev->mtu		= ETH_DATA_LEN; /* override later: actually not ethnet */
	ndev->min_mtu		= ETH_MIN_MTU;
	ndev->max_mtu		= ETH_DATA_LEN;
	ndev->addr_len		= ETH_ALEN;
	ndev->tx_queue_len	= DEFAULT_TX_QUEUE_LEN;
	ndev->flags		= IFF_BROADCAST|IFF_MULTICAST;
	ndev->priv_flags		|= IFF_TX_SKB_SHARING;

	eth_broadcast_addr(ndev->broadcast);
}

int rpmsg_net_create_etherdev(struct rpmsg_net_device *rndev)
{
	struct device *dev = &rndev->rpmsg_chnl->dev;
	struct rpmsg_eth_device *eth;
	struct net_device *ndev;
	int ret = -ENOMEM;

	ndev = alloc_netdev(sizeof(struct rpmsg_eth_device),
                             "eth%d", NET_NAME_UNKNOWN, rpmsg_netdev_setup);
	if (ndev == NULL) {
		dev_err(dev, "ERROR: alloc_netdev\n");
		return ret;
	}

	ndev->ethtool_ops = &rpmsg_ethtool_ops;
	ndev->netdev_ops  = &rpmsg_eth_ops;
	SET_NETDEV_DEV(ndev, dev);

	eth = netdev_priv(ndev);
	memset(eth, 0, sizeof(*eth));

	eth->dev = ndev;
	eth_hw_addr_random(ndev);

	eth->rpmsg_chnl = rndev->rpmsg_chnl;
	eth->chinfo.src = RPMSG_ETHERNET_EPT;
	eth->chinfo.dst = RPMSG_ETHERNET_EPT;
	snprintf(eth->chinfo.name, RPMSG_NAME_SIZE, "rpmsg-net-%d", eth->chinfo.src);
	rndev->ether_dev = ndev;

	spin_lock_init(&eth->lock);
	skb_queue_head_init(&eth->txqueue);
	INIT_WORK(&eth->tx_work, eth_xmit_work_handler);

	eth->ether_ept = rpmsg_create_ept(eth->rpmsg_chnl, rpmsg_eth_rx_cb,
					eth, eth->chinfo);
	if (!eth->ether_ept) {
		dev_err(dev, "ERROR: rpmsg_create_ept\n");
		goto out;
	}
	eth->rpmsg_mtu = rpmsg_get_mtu(eth->ether_ept);
//	ndev->mtu = eth->rpmsg_mtu - ETH_HLEN;
	eth->frag = false;

	ret = register_netdev(ndev);
	if (ret < 0 ) {
		dev_err(dev, "ERROR: register netdev\n");
		goto out;
	}

	dev_info(&ndev->dev, "created mtu(%d,%d)\n", eth->rpmsg_mtu, ndev->mtu);
	return ret;

out:
	if(ndev) {
		free_netdev(ndev);
	}

	dev_info(dev, "ERROR: Failed!\n");
	return ret;
}

static int rpmsg_net_device_cb(struct rpmsg_device *rpdev, void *data,
        int len, void *priv, u32 src)
{
	dev_info(&rpdev->dev, "%s: called\n",  __func__);
	return 0;
}

static int rpmsg_net_device_probe(struct rpmsg_device *rpdev)
{
	struct rpmsg_net_device *rndev;

	rndev = devm_kzalloc(&rpdev->dev, sizeof(struct rpmsg_net_device),
	                     GFP_KERNEL);
	if (!rndev) {
		dev_err(&rpdev->dev, "Failed to allocate memory for rpmsg netdev\n");
		return -ENOMEM;
	}

	dev_info(&rpdev->dev, "new channel!\n");

	memset(rndev, 0x0, sizeof(struct rpmsg_net_device));
	rndev->rpmsg_chnl = rpdev;

	dev_set_drvdata(&rpdev->dev, rndev);

	if (rpmsg_net_create_etherdev(rndev)) {
		goto error2;
	}

	return 0;

error2:
	return -ENODEV;
}

static void rpmsg_net_device_remove(struct rpmsg_device *rpdev)
{
	struct rpmsg_net_device *rndev = dev_get_drvdata(&rpdev->dev);

	if (rndev->ether_dev) {
		free_netdev(rndev->ether_dev);
	}
}

static struct rpmsg_device_id rpmsg_net_driver_id_table[] =
{
	{ .name = RPMSG_ETHERNET_NAME },
	{},
};

static struct rpmsg_driver rpmsg_net_driver =
{
	.drv.name = "rpmsg_netdrv",
	.drv.owner = THIS_MODULE,
	.id_table = rpmsg_net_driver_id_table,
	.probe = rpmsg_net_device_probe,
	.remove = rpmsg_net_device_remove,
	.callback = rpmsg_net_device_cb,
};

static int __init rpmsg_net_init(void)
{
	int err = 0;

	err = register_rpmsg_driver(&rpmsg_net_driver);
	if (err != 0) {
		pr_err("%s: Failed, rc=%d\n", __func__, err);
	}

	return err;
}

static void __exit rpmsg_net_exit(void)
{
	unregister_rpmsg_driver(&rpmsg_net_driver);
}

late_initcall(rpmsg_net_init);
module_exit(rpmsg_net_exit);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("RPMsg-based Network Device Driver");
MODULE_LICENSE("GPL v2");

