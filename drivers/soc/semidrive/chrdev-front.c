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
#include <linux/io.h>
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
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/soc/semidrive/ipcc.h>
#include <uapi/linux/rpmsg_socket.h>
#include <uapi/linux/can.h>
#include <uapi/linux/can/raw.h>
#include <net/busy_poll.h>

#include "uapi/dcf-ioctl.h"
#define CONFIG_DCF_ENDPOINT (0)

union dcf_ioctl_arg {
	struct dcf_usercontext	context;
	struct dcf_ioc_setproperty setprop;
};

struct dcf_front_device {
	const char *name;
	umode_t mode;
	const struct file_operations *fops;
	fmode_t fmode;
	int myaddr;

	int minor;
	struct device *dev;
	struct platform_device *pdev;
	struct mutex ept_lock;
	struct list_head users;
	atomic_t user_count;
	struct socket *sock;
};

struct dcf_user {
	struct list_head list_node;
	struct dcf_front_device *fdev;
	u8 user_id;
	u8 user_id_other;
	spinlock_t queue_lock;
	struct sk_buff_head queue;
	wait_queue_head_t readq;
	int pid;
};

/*!< CAN Frame Type(0:DATA or 1:REMOTE). */
#define RPMSG_CAN_FLAG_TYPE		(1 << 29)
/*!< CAN FD or classic frame? 0: classic or 1: canfd */
#define RPMSG_CAN_FLAG_CANFD	(1 << 30)
/*!< CAN Frame Identifier(0: STD or 1: EXT format). */
#define RPMSG_CAN_FLAG_FMT		(1 << 31)
struct rpmsg_can_frame {
	canid_t can_id;  /* 32 bit CAN_ID + FORMAT/CANFD/TYPE flags */
	__u8    len;     /* frame payload length in byte */
	__u8    data[CANFD_MAX_DLEN];
}__attribute__((packed));

#define RPMSG_CAN_MTU	(sizeof(struct rpmsg_can_frame))

static long vircan_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	pr_warn_once("%s: not supported\n", __func__);
	return -ENOTSUPP;
}

/*
 * Special canid for time test, store the sys count (uint32)from/to can driver
 * into payload of the canframe at offset 4~7 and store the timestamp of
 * linux vircan driver in offset 0~3
 * sys counter is a global clock counter with 3MHz frequency.
 */
#define CAN_ID_TIMESTAMP_TEST	(1)
uint32_t semidrive_get_syscntr(void);

static ssize_t vircan_read(struct file *filp, char __user *buf,
				 size_t len, loff_t *f_pos)
{
	struct dcf_front_device *fdev = filp->private_data;
	struct rpmsg_can_frame *vcanframe;
	struct socket *sock = fdev->sock;
	struct canfd_frame frame = {0, };
	unsigned int msg_flags = 0;
	struct msghdr msg;
	struct kvec vec[2];
	void *kbuf;
	int ret, rev_len = 0;

	kbuf = kmalloc(RPMSG_CAN_MTU, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iter.type = ITER_KVEC|READ;
	msg.msg_iter.count = RPMSG_CAN_MTU;
	vec[0].iov_base = kbuf;
	vec[0].iov_len = RPMSG_CAN_MTU;
	msg.msg_iter.kvec = vec;
	msg.msg_iter.nr_segs = 1;

	if (filp->f_flags & O_NONBLOCK)
		msg_flags = MSG_DONTWAIT;

	ret = sock_recvmsg(sock, &msg, msg_flags);
	if (ret > 0) {
		/* reserve flag and padding data */
		vcanframe = (struct rpmsg_can_frame *)kbuf;

		if (vcanframe->can_id & RPMSG_CAN_FLAG_FMT) {
			frame.can_id = vcanframe->can_id & CAN_EFF_MASK;
			frame.can_id |= CAN_EFF_FLAG;
		} else {
			frame.can_id = vcanframe->can_id & CAN_SFF_MASK;
		}

		if (vcanframe->can_id & RPMSG_CAN_FLAG_TYPE)
			frame.can_id |= CAN_RTR_FLAG;

		if (vcanframe->can_id & RPMSG_CAN_FLAG_CANFD)
			rev_len = min_t(size_t, len, CANFD_MTU);
		else
			rev_len = min_t(size_t, len, CAN_MTU);

		frame.len = vcanframe->len;
		memcpy(&frame.data[0], &vcanframe->data[0], vcanframe->len);

		if ((frame.can_id & CAN_SFF_MASK) == CAN_ID_TIMESTAMP_TEST) {
			u32 temp = semidrive_get_syscntr();
			if (temp)
				memcpy(&frame.data[0], &temp, sizeof(temp));
		}

		ret = copy_to_user(buf, &frame, rev_len);
//		print_hex_dump_bytes("vircan_rx: ", DUMP_PREFIX_ADDRESS, kbuf, vcanframe->len + sizeof(canid_t));
	}

	kfree(kbuf);

	dev_dbg(fdev->dev, "%s recv %d bytes\n", __func__, rev_len);

	return ret < 0 ? ret : rev_len;
}

static ssize_t vircan_write(struct file *filp, const char __user *buf,
				  size_t len, loff_t *f_pos)
{
	struct dcf_front_device *fdev = filp->private_data;
	struct socket *sock = fdev->sock;
	struct rpmsg_can_frame vcanframe = {0,};
	struct canfd_frame *frame;
	struct msghdr msg;
	struct kvec vec[2];
	void *kbuf;
	int ret = 0;

	if (len > CANFD_MTU) {
		dev_err(fdev->dev, "invalid canframe size=%ld\n", len);
		return -EBUSY;
	}

	kbuf = memdup_user(buf, len);
	if (IS_ERR(kbuf))
		return PTR_ERR(kbuf);

	frame = kbuf;
	if (frame->len > CANFD_MAX_DLEN) {
		dev_err(fdev->dev, "invalid data length=%d\n", frame->len);
		goto free_kbuf;
	}

	if (mutex_lock_interruptible(&fdev->ept_lock)) {
		ret = -ERESTARTSYS;
		goto free_kbuf;
	}

	memset(&msg, 0, sizeof(msg));
	if (filp->f_flags & O_NONBLOCK)
		msg.msg_flags |= MSG_DONTWAIT;

	if (frame->can_id & CAN_EFF_FLAG) {
		vcanframe.can_id = frame->can_id & CAN_EFF_MASK;
		vcanframe.can_id |= RPMSG_CAN_FLAG_FMT;
	} else
		vcanframe.can_id = frame->can_id & CAN_SFF_MASK;

	if (frame->can_id & CAN_RTR_FLAG)
		vcanframe.can_id |= RPMSG_CAN_FLAG_TYPE;

	if (frame->len > CAN_MAX_DLEN)
		vcanframe.can_id |= RPMSG_CAN_FLAG_CANFD;

	vcanframe.len = frame->len;
	memcpy(&vcanframe.data[0], &frame->data[0], frame->len);

	if ((frame->can_id & CAN_SFF_MASK) == CAN_ID_TIMESTAMP_TEST) {
		u32 temp = semidrive_get_syscntr();
		if (temp)
			memcpy(&vcanframe.data[0], &temp, sizeof(temp));
	}
	msg.msg_iter.type = ITER_KVEC|WRITE;
	msg.msg_iter.count = RPMSG_CAN_MTU;
	vec[0].iov_base = &vcanframe;
	vec[0].iov_len = RPMSG_CAN_MTU;
	msg.msg_iter.kvec = vec;
	msg.msg_iter.nr_segs = 1;

	ret = sock_sendmsg(sock, &msg);

	mutex_unlock(&fdev->ept_lock);
	dev_dbg(fdev->dev, "%s sent %d bytes\n", __func__, ret);

free_kbuf:
	kfree(kbuf);
	return ret < 0 ? ret : len;
}


enum sts_op_type {
	CP_OP_GET_VERSION = 1,
	CP_OP_GET_CONFIG,
	CP_OP_START,
	CP_OP_STOP,
};

/* Do not exceed 32 bytes so far */
struct canctl_cmd {
	u16 op;
	u16 tag;
	union {
		struct {
			u16 what;
		} s;
	} msg;
};

/* Do not exceed 16 bytes so far */
struct canctl_result {
	u16 op;
	u16 tag;
	union {
		/** used for get_version */
		struct {
			u16 version;
			u16 id;
			u16 vendor;
		} v;
		/** used for get_config */
		struct {
			u8 baudrate;
			u8 canfd_support;
			u8 instances;
			u8 padding;
		} cfg;
		u8 data[12];
	} msg;
};

static int vircan_if_control(struct dcf_front_device *candev, int command,
	struct canctl_result *data)
{
	struct rpc_ret_msg result = {0,};
	struct rpc_req_msg request;
	struct canctl_cmd *ctl = DCF_RPC_PARAM(request, struct canctl_cmd);
	struct canctl_result *rs = (struct canctl_result *) &result.result[0];
	int ret = 0;

	DCF_INIT_RPC_REQ(request, MOD_RPC_REQ_CP_IOCTL);
	ctl->op = command;
	ctl->tag = candev->myaddr;
	ret = semidrive_rpcall(&request, &result);
	if (ret < 0 || result.retcode < 0) {
		dev_err(candev->dev, "rpcall failed=%d %d\n", ret, result.retcode);
		goto fail_call;
	}

	memcpy(data, rs, sizeof(*rs));

fail_call:

	return ret;
}

static int vircan_if_start(struct dcf_front_device *candev)
{
	struct canctl_result op_ret;

	return vircan_if_control(candev, CP_OP_START, &op_ret);
}

static int vircan_if_stop(struct dcf_front_device *candev)
{
	struct canctl_result op_ret;

	return vircan_if_control(candev, CP_OP_STOP, &op_ret);
}

static int vircan_if_getconfig(struct dcf_front_device *candev)
{
	struct canctl_result op_ret;
	int ret;

	ret = vircan_if_control(candev, CP_OP_GET_CONFIG, &op_ret);
	if (ret == 0) {
		dev_info(candev->dev, "baud: %d, canfd: %d, if#: %d\n",
			op_ret.msg.cfg.baudrate, op_ret.msg.cfg.canfd_support,
			op_ret.msg.cfg.instances);
	}

	return ret;
}

static int vircan_if_getversion(struct dcf_front_device *candev)
{
	struct canctl_result op_ret;
	int ret;

	ret = vircan_if_control(candev, CP_OP_GET_VERSION, &op_ret);
	if (ret == 0)
		dev_info(candev->dev, "id: %d, version: %04x, vendor: %d\n",
				op_ret.msg.v.id, op_ret.msg.v.version, op_ret.msg.v.vendor);

	return ret;
}

/* This device support single user */
static int vircan_open(struct inode *inode, struct file *filp)
{
	struct dcf_front_device *fdev = filp->private_data;
	struct device *dev = fdev->dev;
	struct socket *sock = NULL;
	struct sockaddr_rpmsg sa;
	int ret;

	if (atomic_read(&fdev->user_count)) {
		dev_err(dev, "already open ept:%d\n", fdev->myaddr);
		return -EBUSY;
	}

	ret = vircan_if_getversion(fdev);
	if (ret < 0) {
		dev_err(dev, "dev:%d no response\n", fdev->myaddr);
		return -EAGAIN;
	}

	atomic_inc(&fdev->user_count);
	mutex_init(&fdev->ept_lock);
	INIT_LIST_HEAD(&fdev->users);

	get_device(dev);

	sa.family = AF_RPMSG;
	sa.vproc_id = 0;	/* default safety core */
	sa.addr = fdev->myaddr;
	ret = sock_create(sa.family, SOCK_SEQPACKET, 0, &sock);
	if (ret < 0)
		goto out;

	ret = kernel_connect(sock, (struct sockaddr *)&sa, sizeof(sa), 0);
	if (ret < 0)
		goto out;

	fdev->sock = sock;

	vircan_if_getconfig(fdev);
	vircan_if_start(fdev);
	dev_info(dev, "device open ept:%d\n", fdev->myaddr);

	return 0;

out:
	if (sock)
		sock_release(sock);

	atomic_dec(&fdev->user_count);
	put_device(dev);

	dev_err(dev, "open ept:%d fail:%d\n", fdev->myaddr, ret);

	return ret;
}

static int vircan_release(struct inode *inode, struct file *filp)
{
	struct dcf_front_device *fdev = filp->private_data;
	struct socket *sock = fdev->sock;
	struct device *dev = fdev->dev;

	vircan_if_stop(fdev);
	sock_release(sock);
	atomic_dec(&fdev->user_count);
	dev_info(dev, "device close\n");

	put_device(dev);

	return 0;
}

static unsigned int vircan_poll(struct file *filp, poll_table *wait)
{
	struct dcf_front_device *fdev = filp->private_data;
	struct socket *sock = fdev->sock;
	unsigned int busy_flag = 0;

	if (sk_can_busy_loop(sock->sk)) {
		/* this socket can poll_ll so tell the system call */
		busy_flag = POLL_BUSY_LOOP;

		/* once, only if requested by syscall */
		if (wait && (wait->_key & POLL_BUSY_LOOP))
			sk_busy_loop(sock->sk, 1);
	}

	return busy_flag | sock->ops->poll(filp, sock, wait);
}

static int validate_ioctl_arg(unsigned int cmd, union dcf_ioctl_arg *arg)
{
	int ret = 0;

	switch (cmd) {
	case DCF_IOC_SET_USER_CTX:

		break;
	default:
		break;
	}

	return ret ? -EINVAL : 0;
}

/* fix up the cases where the ioctl direction bits are incorrect */
static unsigned int dcf_ioctl_dir(unsigned int cmd)
{
	return _IOC_DIR(cmd);
}

long dcf_file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct dcf_user *user = filp->private_data;
	union dcf_ioctl_arg data;
	unsigned int dir;
	int ret = 0;

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
	case DCF_IOC_SET_USER_CTX:
	{
		struct dcf_usercontext *ctx = &data.context;
		if (ctx->user_id >= DCF_USER_ANY)
			return -ERANGE;

		if (user->user_id != DCF_USER_ANY)
			return -EEXIST;

		user->user_id = ctx->user_id;
		user->user_id_other = ctx->user_id_other;

		break;
	}
	case DCF_IOC_SET_PROPERTY:
	{
		struct dcf_ioc_setproperty *setprop = &data.setprop;

        semidrive_set_property(setprop->property_id, setprop->property_value);
		break;
	}
	case DCF_IOC_GET_PROPERTY:
	{
		struct dcf_ioc_setproperty *setprop = &data.setprop;

        semidrive_get_property(setprop->property_id, &setprop->property_value);
		break;
	}

	default:
		return -ENOTTY;
	}

	if (dir & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
	}
	return ret;
}

static int dcf_file_open_detail(struct inode *inode, struct file *filp)
{
	struct dcf_front_device *fdev = filp->private_data;
	struct device *dev = fdev->dev;
	struct dcf_user *user;

	get_device(dev);

	if (!atomic_read(&fdev->user_count)) {
		mutex_init(&fdev->ept_lock);
		INIT_LIST_HEAD(&fdev->users);

		/*TODO: create endpoint below */
	}

	user = kzalloc(sizeof(struct dcf_user), GFP_KERNEL);
	if (!user) {
		dev_err(dev, "failed to create endpoint\n");
		put_device(dev);
		return -ENOMEM;
	}

	user->user_id = DCF_USER_ANY;	/* Not assigned */
	user->user_id_other = DCF_USER_ANY;
	user->fdev = fdev;
	user->pid = current->group_leader->pid;
	spin_lock_init(&user->queue_lock);
	skb_queue_head_init(&user->queue);
	init_waitqueue_head(&user->readq);
	list_add(&user->list_node, &fdev->users);
	filp->private_data = user;
	atomic_inc(&fdev->user_count);
	dev_info(dev, "user=%d pid=%d opened\n", user->user_id, user->pid);

	return 0;
}

static int dcf_file_release(struct inode *inode, struct file *filp)
{
	struct dcf_user *user = filp->private_data;
	struct dcf_front_device *fdev = user->fdev;
	struct device *dev = fdev->dev;
	struct sk_buff *skb;

	list_del(&user->list_node);

	/* Discard all SKBs */
	spin_lock(&user->queue_lock);
	while (!skb_queue_empty(&user->queue)) {
		skb = skb_dequeue(&user->queue);
		kfree_skb(skb);
	}
	spin_unlock(&user->queue_lock);
	dev_info(dev, "user=%d pid=%d released!\n", user->user_id, user->pid);

	if (atomic_dec_and_test(&fdev->user_count)) {
		/* Close the endpoint, if it's not already destroyed by the parent */
		mutex_lock(&fdev->ept_lock);

		/*TODO: Destroy endpoint below */

		mutex_unlock(&fdev->ept_lock);
		dev_info(dev, "last user=%d pid=%d released!\n", user->user_id, user->pid);
	}

	kfree(user);
	put_device(dev);

	return 0;
}

static const struct file_operations __maybe_unused vircan_fops = {
	.llseek		= no_llseek,
	.read		= vircan_read,
	.write		= vircan_write,
	.unlocked_ioctl = vircan_ioctl,
	.poll		= vircan_poll,
	.open		= vircan_open,
	.release	= vircan_release,
};

static const struct file_operations property_fops = {
	.owner   = THIS_MODULE,
	.unlocked_ioctl = dcf_file_ioctl,
	.open    = dcf_file_open_detail,
	.release = dcf_file_release,
	.llseek	 = no_llseek,
};

static struct dcf_front_device devlist[] = {
	[1] = { "property", 0600, &property_fops,FMODE_UNSIGNED_OFFSET, SD_PROPERTY_EPT},
	[2] = { "vircan",   0600, &vircan_fops,  FMODE_UNSIGNED_OFFSET, SD_VIRCAN_EPT},
	[3] = { "vircan",   0600, &vircan_fops,  FMODE_UNSIGNED_OFFSET, SD_VIRCAN_EPT_C},
};

static const struct of_device_id dcf_front_of_ids[] = {
	{ .compatible = "dcf,property", .data = (void *)SD_PROPERTY_EPT},
	{ .compatible = "dcf,vircan0",  .data = (void *)SD_VIRCAN_EPT},
	{ .compatible = "dcf,vircan1",  .data = (void *)SD_VIRCAN_EPT_C},
	{ }
};

static dev_t dcf_major;
static struct class *dcf_class;

static int dcf_file_open(struct inode *inode, struct file *filp)
{
	int minor;
	const struct dcf_front_device *dev;

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

static const struct file_operations dcf_file_fops = {
	.open = dcf_file_open,
	.llseek = noop_llseek,
};

static char *dcf_devnode(struct device *dev, umode_t *mode)
{
	if (mode && devlist[MINOR(dev->devt)].mode)
		*mode = devlist[MINOR(dev->devt)].mode;
	return NULL;
}

static int dcf_front_probe(struct platform_device *pdev)
{
	struct dcf_front_device *dev = NULL;
	int minor;

	for (minor = 1; minor < ARRAY_SIZE(devlist); minor++) {
		if (devlist[minor].myaddr == (long)of_device_get_match_data(&pdev->dev)) {
			dev = &devlist[minor];
			if (dev->dev) {
				pr_err("Error already open device %s\n", dev->name);
				return -EALREADY;
			}
			dev->pdev = pdev;
			dev->minor = minor;
			dev->dev = device_create(dcf_class, &pdev->dev, MKDEV(MAJOR(dcf_major), minor),
						  dev, devlist[minor].name);

			if (IS_ERR(dev->dev)) {
				pr_err("Error creating device %s\n", dev->name);
				return PTR_ERR(dev->dev);
			}
			dev_set_drvdata(&pdev->dev, dev);
			dev_info(&pdev->dev, "device created minor:%d\n", dev->minor);
			return 0;
		}
	}

	return -ENODEV;
}

static int dcf_front_remove(struct platform_device *pdev)
{
	struct dcf_front_device *dev = dev_get_drvdata(&pdev->dev);

	if (dev->dev)
		device_destroy(dcf_class, dev->dev->devt);

	dev_set_drvdata(&pdev->dev, NULL);
	dev->pdev = NULL;

	return 0;
}

MODULE_DEVICE_TABLE(of, dcf_front_of_ids);

static struct platform_driver dcf_front_driver = {
	.probe = dcf_front_probe,
	.remove = dcf_front_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sd,dcfv",
		.of_match_table = dcf_front_of_ids,
	},
};

static int __init dcf_front_init(void)
{
	int major;
	int ret;

	pr_info("initialize dcf front driver!\n");

	major = register_chrdev(0, "dcfv", &dcf_file_fops);
	if (major < 0) {
		pr_err("dcf: failed to register char dev region\n");
		return major;
	}

	dcf_major = MKDEV(major, 0);
	dcf_class = class_create(THIS_MODULE, "dcfv");
	if (IS_ERR(dcf_class)) {
		unregister_chrdev(major, "dcfv");
		return PTR_ERR(dcf_class);
	}

	dcf_class->devnode = dcf_devnode;
	/* Native environment use rpmsg channel as transportation */
	ret = platform_driver_register(&dcf_front_driver);
	if (ret < 0) {
		pr_err("dcf: failed to register rpmsg driver\n");
		class_destroy(dcf_class);
		unregister_chrdev(major, "dcfv");
	}

	return 0;
}

static void __exit dcf_front_fini(void)
{

	pr_info("unregister dcf front driver!\n");

	platform_driver_unregister(&dcf_front_driver);
	class_destroy(dcf_class);
	unregister_chrdev(dcf_major, "dcf");
}

module_init(dcf_front_init);
module_exit(dcf_front_fini);

MODULE_AUTHOR("<ye.liu@semidrive.com>");
MODULE_ALIAS("dcf:chrdev");
MODULE_DESCRIPTION("Semidrive DCF User chardev driver");
MODULE_LICENSE("GPL v2");

