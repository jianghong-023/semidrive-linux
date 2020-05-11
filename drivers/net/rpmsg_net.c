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

#define ETHERNET_ENDPOINT       60

struct rpmsg_eth_device {
	struct rpmsg_device *rpmsg_chnl;
	struct rpmsg_endpoint *ether_ept;
	struct net_device_stats stats;
	struct net_device *dev;
	struct rpmsg_channel_info chinfo;
	/* MTU is for rpmsg payload */
	int rpmsg_mtu;

	spinlock_t lock;
	struct sk_buff_head txqueue;
	struct work_struct tx_work;
};

struct rpmsg_net_device {
	struct rpmsg_device *rpmsg_chnl;
	struct net_device *ether_dev;
};

/*
 * The higher levels take care of making this non-reentrant (it's
 * called with bh's disabled).
 */
static netdev_tx_t rpmsg_eth_xmit(struct sk_buff *skb,
                            struct net_device *dev)
{
	struct rpmsg_eth_device *eth = netdev_priv(dev);

	spin_lock_bh(&eth->lock);

	skb_queue_tail(&eth->txqueue, skb);

	spin_unlock_bh(&eth->lock);

	schedule_work(&eth->tx_work);

	return NETDEV_TX_OK;
}


static void rpmsg_read_mac_addr(struct net_device *dev)
{
	int i=0;
	for (i = 0; i < ETH_ALEN; i++)
		dev->dev_addr[i] = 0;
	dev->dev_addr[0] = 0xAA;
	dev->dev_addr[1] = 0xBB;
	dev->dev_addr[2] = 0xCC;
	dev->dev_addr[3] = 3;
	dev->dev_addr[4] = 1;
	dev->dev_addr[5] = ETHERNET_ENDPOINT;
}

static int rpmsg_eth_open(struct net_device *ndev)
{
	rpmsg_read_mac_addr(ndev);
	netif_start_queue(ndev);

	return 0;
}


int rpmsg_eth_stop(struct net_device *dev)
{
	pr_info ("stop called\n");
	netif_stop_queue(dev);
	return 0;
}

/*
 * Configuration changes (passed on by ifconfig)
 */
int rpmsg_eth_config(struct net_device *dev, struct ifmap *map)
{
	if (dev->flags & IFF_UP) /* can't act on a running interface */
		return -EBUSY;

	/* ignore other fields */
	return 0;
}

struct net_device_stats *rpmsg_eth_stats(struct net_device *dev)
{
	struct rpmsg_eth_device *priv = netdev_priv(dev);
	return &priv->stats;
}

static u32 always_on(struct net_device *dev)
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
	.ndo_validate_addr  = eth_validate_addr,
	.ndo_get_stats      = rpmsg_eth_stats,
};

static int rpmsg_eth_rx_cb(struct rpmsg_device *rpdev, void *data,
                                        int len, void *priv, u32 src)
{
	struct rpmsg_net_device *rndev = dev_get_drvdata(&rpdev->dev);
    struct rpmsg_eth_device *eth = priv;
	struct sk_buff *skb;

	spin_lock_bh(&eth->lock);

	skb = netdev_alloc_skb_ip_align(rndev->ether_dev, len);
	if (!skb) {
		eth->stats.rx_errors++;
		eth->stats.rx_dropped += len;
		spin_unlock_bh(&eth->lock);
		pr_info ("rx msg dropped without skbuf\n");
		return -1;
	}
	memcpy(skb_put(skb, len), data, len);

	skb->protocol = eth_type_trans(skb, rndev->ether_dev);
	skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */
	eth->stats.rx_packets++;
	eth->stats.rx_bytes += len;
	netif_rx(skb);
	spin_unlock_bh(&eth->lock);

	pr_debug("rpmsg: net: RX msg len:%d\n", len);
//	print_hex_dump_bytes("rpmsg: net:", DUMP_PREFIX_ADDRESS, data, 48);

	return 0;
}

static void eth_xmit_work_handler(struct work_struct *work)
{
	struct rpmsg_eth_device *eth = container_of(work, struct rpmsg_eth_device,
	                    tx_work);
	struct sk_buff *skb;
	int len = 0;
	int err = 0;

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
		if (len > eth->rpmsg_mtu) {
			pr_err("ERROR: %s trim len > mtu : %d\n", __func__, len, eth->rpmsg_mtu);
			len = eth->rpmsg_mtu;
		}

		eth->stats.tx_packets++;
		eth->stats.tx_bytes += len;
		spin_unlock_bh(&eth->lock);

		err = rpmsg_trysendto(eth->ether_ept, skb->data, len, eth->chinfo.dst);
		if (err < 0)
			pr_err("ERROR: %s rc=%d no pkts\n", __func__, err);

		pr_debug("rpmsg: net: TX msg len:%d\n", len);

		dev_kfree_skb_any(skb);
	}
}

static void rpmsg_netdev_setup(struct net_device *dev)
{
	dev->header_ops		= &eth_header_ops;
	dev->type		= ARPHRD_ETHER;
	dev->hard_header_len 	= ETH_HLEN;
	dev->min_header_len	= ETH_HLEN;
	dev->mtu		= ETH_DATA_LEN; /* override later: actually not ethnet */
	dev->min_mtu		= ETH_MIN_MTU;
	dev->max_mtu		= ETH_DATA_LEN;
	dev->addr_len		= ETH_ALEN;
	dev->tx_queue_len	= DEFAULT_TX_QUEUE_LEN;
	dev->flags		= IFF_BROADCAST|IFF_MULTICAST;
	dev->priv_flags		|= IFF_TX_SKB_SHARING;

	eth_broadcast_addr(dev->broadcast);
}

int rpmsg_net_create_etherdev(struct rpmsg_net_device *rndev)
{
	int result, ret = -ENOMEM;
	struct rpmsg_eth_device *eth;
	struct net_device *dev;

	dev = alloc_netdev(sizeof(struct rpmsg_eth_device),
                             "rpm%d", NET_NAME_UNKNOWN, rpmsg_netdev_setup);
	if (dev ==NULL) {
		pr_err("ERROR: %s %d\n", __func__, __LINE__);
		return ret;
	}

	dev->ethtool_ops = &rpmsg_ethtool_ops;
	dev->netdev_ops  = &rpmsg_eth_ops;

	eth = netdev_priv(dev);
	memset(eth, 0, sizeof(*eth));

	pr_debug("INFO: %s %d\n", __func__, __LINE__);
	eth->dev = dev;
	rpmsg_read_mac_addr(dev);

	eth->rpmsg_chnl = rndev->rpmsg_chnl;
	eth->chinfo.src = ETHERNET_ENDPOINT;
	eth->chinfo.dst = ETHERNET_ENDPOINT;
	snprintf(eth->chinfo.name, RPMSG_NAME_SIZE, "rpmsg-net-%d", eth->chinfo.src);
	rndev->ether_dev = dev;

	spin_lock_init(&eth->lock);
	skb_queue_head_init(&eth->txqueue);
	INIT_WORK(&eth->tx_work, eth_xmit_work_handler);

	eth->ether_ept = rpmsg_create_ept(eth->rpmsg_chnl, rpmsg_eth_rx_cb,
					eth, eth->chinfo);
	if (!eth->ether_ept) {
		pr_err("%s Failed to create endpoint\n",  __func__);
		goto out;
	}
	eth->rpmsg_mtu = rpmsg_get_mtu(eth->ether_ept);
	dev->mtu = eth->rpmsg_mtu - ETH_HLEN;

	result = register_netdev(dev);
	if (result) {
		pr_err("%s: Failed to register netdev\n", __func__);

	} else {
		ret = 0;
		pr_debug("%s: done!\n", __func__);
	}

	return 0;

out:
	if(dev) {
		free_netdev(dev);
	}

	pr_err("%s Failed!\n", __func__);
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
		dev_err(&rpdev->dev, "Failed to allocate memory for rpmsg user dev.\n");
		return -ENOMEM;
	}

	dev_info(&rpdev->dev, "new channel: 0x%x -> 0x%x!\n",
	         rpdev->src, rpdev->dst);

	memset(rndev, 0x0, sizeof(struct rpmsg_net_device));
	rndev->rpmsg_chnl = rpdev;

	dev_set_drvdata(&rpdev->dev, rndev);
	dev_dbg(&rpdev->dev, "%s\n", __func__);

	if (rpmsg_net_create_etherdev(rndev)) {
		dev_err(&rpdev->dev, "Failed: rpmsg_net\n");
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
	{ .name = "rpmsg-net" },
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

module_init(rpmsg_net_init);
module_exit(rpmsg_net_exit);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("RPMsg-based Network Device Driver");
MODULE_LICENSE("GPL v2");

