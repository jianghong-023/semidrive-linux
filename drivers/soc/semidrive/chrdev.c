/*
 * Copyright (c) 2020, Semidriver Semiconductor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/rpmsg.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <linux/soc/semidrive/ipcc.h>

union dcf_ioctl_arg {
	int a;
	int b;
};

struct dcf_device {
	const char *name;
	umode_t mode;
	const struct file_operations *fops;
	fmode_t fmode;
	int myaddr;

	int minor;
	struct device *dev;
	struct rpmsg_device *rpdev;
	struct mutex ept_lock;

	spinlock_t queue_lock;
	struct sk_buff_head queue;
	wait_queue_head_t readq;
};

static int validate_ioctl_arg(unsigned int cmd, union dcf_ioctl_arg *arg)
{
	int ret = 0;

	switch (cmd) {
	case 1:

		break;
	default:
		break;
	}

	return ret ? -EINVAL : 0;
}

/* fix up the cases where the ioctl direction bits are incorrect */
static unsigned int dcf_ioctl_dir(unsigned int cmd)
{
	switch (cmd) {
	default:
		return _IOC_DIR(cmd);
	}
}

long ioctl_dcf(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned int dir;
	union dcf_ioctl_arg data;

	dir = dcf_ioctl_dir(cmd);

	if (_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;

	/*
	 * The copy_from_user is unconditional here for both read and write
	 * to do the validate. If there is no write for the ioctl, the
	 * buffer is cleared
	 */
	if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	ret = validate_ioctl_arg(cmd, &data);
	if (ret) {
		pr_warn_once("%s: ioctl validate failed\n", __func__);
		return ret;
	}

	if (!(dir & _IOC_WRITE))
		memset(&data, 0, sizeof(data));

	switch (cmd) {
	case 1:
	{
		break;
	}
	case 2:

		break;
	default:
		return -ENOTTY;
	}

	if (dir & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
	}
	return ret;
}

static ssize_t read_dcf(struct file *filp, char __user *buf,
				 size_t len, loff_t *f_pos)
{
	struct dcf_device *eptdev = filp->private_data;
	struct rpmsg_device *rpdev = eptdev->rpdev;
	unsigned long flags;
	struct sk_buff *skb;
	int use;

	if (!rpdev->ept)
		return -EPIPE;

	spin_lock_irqsave(&eptdev->queue_lock, flags);

	/* Wait for data in the queue */
	if (skb_queue_empty(&eptdev->queue)) {
		spin_unlock_irqrestore(&eptdev->queue_lock, flags);

		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		/* Wait until we get data or the endpoint goes away */
		if (wait_event_interruptible(eptdev->readq,
						 !skb_queue_empty(&eptdev->queue) ||
						 !rpdev->ept))
			return -ERESTARTSYS;

		/* We lost the endpoint while waiting */
		if (!rpdev->ept)
			return -EPIPE;

		spin_lock_irqsave(&eptdev->queue_lock, flags);
	}

	skb = skb_dequeue(&eptdev->queue);
	spin_unlock_irqrestore(&eptdev->queue_lock, flags);
	if (!skb)
		return -EFAULT;

	use = min_t(size_t, len, skb->len);
	if (copy_to_user(buf, skb->data, use))
		use = -EFAULT;

	kfree_skb(skb);

	return use;
}


static ssize_t write_dcf(struct file *filp, const char __user *buf,
				  size_t len, loff_t *f_pos)
{
	struct dcf_device *eptdev = filp->private_data;
	struct rpmsg_device *rpdev = eptdev->rpdev;
	void *kbuf;
	int ret;

	kbuf = memdup_user(buf, len);
	if (IS_ERR(kbuf))
		return PTR_ERR(kbuf);

	if (mutex_lock_interruptible(&eptdev->ept_lock)) {
		ret = -ERESTARTSYS;
		goto free_kbuf;
	}

	if (!rpdev->ept) {
		ret = -EPIPE;
		goto unlock_eptdev;
	}

	if (filp->f_flags & O_NONBLOCK)
		ret = rpmsg_trysend(rpdev->ept, kbuf, len);
	else
		ret = rpmsg_send(rpdev->ept, kbuf, len);

unlock_eptdev:
	mutex_unlock(&eptdev->ept_lock);

free_kbuf:
	kfree(kbuf);
	return ret < 0 ? ret : len;
}

static unsigned int poll_dcf(struct file *filp, poll_table *wait)
{
	struct dcf_device *eptdev = filp->private_data;
	struct rpmsg_device *rpdev = eptdev->rpdev;
	unsigned int mask = 0;

	if (!rpdev->ept)
		return POLLERR;

	poll_wait(filp, &eptdev->readq, wait);

	if (!skb_queue_empty(&eptdev->queue))
		mask |= POLLIN | POLLRDNORM;

	mask |= rpmsg_poll(rpdev->ept, filp, wait);

	return mask;
}

static int dcf_eptdev_cb(struct rpmsg_device *rpdev, void *buf, int len,
			void *priv, u32 addr)
{
	struct dcf_device *eptdev = priv;
	struct sk_buff *skb;

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	skb_put_data(skb, buf, len);

	spin_lock(&eptdev->queue_lock);
	skb_queue_tail(&eptdev->queue, skb);
	spin_unlock(&eptdev->queue_lock);

	/* wake up any blocking processes, waiting for new data */
	wake_up_interruptible(&eptdev->readq);

	return 0;
}

static int open_dcf(struct inode *inode, struct file *filp)
{
	struct dcf_device *eptdev = filp->private_data;
	struct rpmsg_device *rpdev = eptdev->rpdev;
	struct device *dev = eptdev->dev;
	struct rpmsg_channel_info chinfo;

	get_device(dev);
	mutex_init(&eptdev->ept_lock);
	spin_lock_init(&eptdev->queue_lock);
	skb_queue_head_init(&eptdev->queue);
	init_waitqueue_head(&eptdev->readq);

	if (rpdev->ept) {
		dev_err(dev, "Already exist\n");
		return -EEXIST;
	}

	strncpy(chinfo.name, eptdev->name, RPMSG_NAME_SIZE);
	chinfo.src = eptdev->myaddr;
	chinfo.dst = RPMSG_ADDR_ANY;

	rpdev->ept = rpmsg_create_ept(rpdev, dcf_eptdev_cb, eptdev, chinfo);
	if (!rpdev->ept) {
		dev_err(dev, "failed to create endpoint\n");
		put_device(dev);
		return -ENOMEM;
	}
	rpdev->src = rpdev->ept->addr;

	dev_info(dev, "open %s %d->%d \n", chinfo.name, chinfo.src, chinfo.dst);

	return 0;
}

static int release_dcf(struct inode *inode, struct file *filp)
{
	struct dcf_device *eptdev = filp->private_data;
	struct rpmsg_device *rpdev = eptdev->rpdev;
	struct device *dev = eptdev->dev;
	struct sk_buff *skb;

	/* Close the endpoint, if it's not already destroyed by the parent */
	mutex_lock(&eptdev->ept_lock);
	if (rpdev->ept) {
			rpmsg_destroy_ept(rpdev->ept);
			rpdev->ept = NULL;
	}
	mutex_unlock(&eptdev->ept_lock);

	/* Discard all SKBs */
	while (!skb_queue_empty(&eptdev->queue)) {
		skb = skb_dequeue(&eptdev->queue);
		kfree_skb(skb);
	}

	put_device(dev);

	dev_info(dev, "released!\n");

	return 0;
}

static const struct file_operations __maybe_unused cluster_fops = {
	.owner   = THIS_MODULE,
	.read    = read_dcf,
	.write   = write_dcf,
	.unlocked_ioctl = ioctl_dcf,
	.poll    = poll_dcf,
	.open    = open_dcf,
	.release = release_dcf,
	.llseek	 = no_llseek,
};

static const struct file_operations __maybe_unused avm_fops = {
	.llseek		= no_llseek,
	.read		= read_dcf,
	.write		= write_dcf,
	.unlocked_ioctl = ioctl_dcf,
	.open		= open_dcf,
	.release	= release_dcf,
};

static const struct file_operations __maybe_unused sec_fops = {
	.llseek		= no_llseek,
	.read		= read_dcf,
	.write		= write_dcf,
	.unlocked_ioctl = ioctl_dcf,
	.open		= open_dcf,
	.release	= release_dcf,
};

static struct dcf_device devlist[] = {
	 [1] = { "cluster",  0666, &cluster_fops, FMODE_UNSIGNED_OFFSET, SD_CLUSTER_EPT},
	 [2] = { "earlyapp", 0666, &avm_fops,     FMODE_UNSIGNED_OFFSET, SD_EARLYAPP_EPT},
	 [3] = { "ssystem",  0600, &sec_fops,     FMODE_UNSIGNED_OFFSET, SD_SSYSTEM_EPT},
};

static struct rpmsg_device_id rpmsg_bridge_id_table[] = {
	{.name = "rpmsg-cluster"},
	{.name = "rpmsg-earlyapp"},
	{.name = "rpmsg-ssystem"},
	{},
};

static dev_t dcf_major;
static struct class *dcf_class;

static int dcf_chrdev_open(struct inode *inode, struct file *filp)
{
	int minor;
	const struct dcf_device *dev;

	minor = iminor(inode);
	if (minor >= ARRAY_SIZE(devlist))
		return -ENXIO;

	dev = &devlist[minor];
	if (!dev->fops)
		return -ENXIO;

	filp->f_mode |= dev->fmode;
	filp->private_data = (void*)dev;
	replace_fops(filp, dev->fops);

	if (dev->fops->open)
		return dev->fops->open(inode, filp);

	return 0;
}

static const struct file_operations dcf_chrdev_fops = {
	.open = dcf_chrdev_open,
	.llseek = noop_llseek,
};

static char *dcf_devnode(struct device *dev, umode_t *mode)
{
	if (mode && devlist[MINOR(dev->devt)].mode)
		*mode = devlist[MINOR(dev->devt)].mode;
	return NULL;
}

static int rpmsg_bridge_probe(struct rpmsg_device *rpdev)
{
	struct dcf_device *dev = NULL;
	int minor;
	char *pstr;

	for (minor = 1; minor < ARRAY_SIZE(devlist); minor++) {
		pstr = strnstr(rpdev->id.name, devlist[minor].name, RPMSG_NAME_SIZE);
		if (pstr) {
			dev = &devlist[minor];
			dev->rpdev = rpdev;
			dev->minor = minor;
			dev->dev = device_create(dcf_class, &rpdev->dev, MKDEV(MAJOR(dcf_major), minor),
						  dev, devlist[minor].name);

			dev_info(dev->dev, "device created!\n");
			return 0;
		}
	}

	return -ENODEV;
}

static void rpmsg_bridge_remove(struct rpmsg_device *rpdev)
{
	struct dcf_device *dev = dev_get_drvdata(&rpdev->dev);

	if (dev->dev)
		device_destroy(dcf_class, dev->dev->devt);

	dev_set_drvdata(&rpdev->dev, NULL);
	dev->rpdev = NULL;
}

static struct rpmsg_driver rpmsg_bridge_driver = {
	.probe = rpmsg_bridge_probe,
	.remove = rpmsg_bridge_remove,
	.id_table = rpmsg_bridge_id_table,
	.drv = {
		.name = "sd,rpmsg-br"
	},
};

static int __init dcf_char_init(void)
{
	int major;
	int ret;

	major = register_chrdev(0, "dcf", &dcf_chrdev_fops);
	if (major < 0) {
		pr_err("dcf: failed to register char dev region\n");
		return major;
	}

	dcf_major = MKDEV(major, 0);
	dcf_class = class_create(THIS_MODULE, "dcf");
	if (IS_ERR(dcf_class)) {
		unregister_chrdev(major, "dcf");
		return PTR_ERR(dcf_class);
	}

	dcf_class->devnode = dcf_devnode;

	if (xen_domain()) {
		/* TODO: XEN environment use vRPC as transportation */

	} else {
		/* Native environment use rpmsg channel as transportation */
		ret = register_rpmsg_driver(&rpmsg_bridge_driver);
		if (ret < 0) {
			pr_err("dcf: failed to register rpmsg driver\n");
			class_destroy(dcf_class);
			unregister_chrdev(major, "dcf");
		}
	}

	return 0;
}

static void __exit dcf_char_exit(void)
{
	unregister_rpmsg_driver(&rpmsg_bridge_driver);
	class_destroy(dcf_class);
	unregister_chrdev(dcf_major, "dcf");
}

module_init(dcf_char_init);
module_exit(dcf_char_exit);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_ALIAS("dcf:chrdev");
MODULE_DESCRIPTION("Semidrive domain-communication-framework driver");
MODULE_LICENSE("GPL v2");

