/*
 * Copyright (C) 2020 Semidriver Semiconductor Inc.
 * License terms:  GNU General Public License (GPL), version 2
 */


#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <xen/xen.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ipcc.h>
#include <linux/soc/semidrive/xenipc.h>

#define RPMSG_DC_STATUS_DEV			(0)

int rpmsg_rpc_call_trace(int dev, struct rpc_req_msg *req, struct rpc_ret_msg *result);
int xenipc_rpc_trace(int dev, struct rpc_req_msg *req, struct rpc_ret_msg *result);

int semidrive_rpcall(struct rpc_req_msg *req, struct rpc_ret_msg *result)
{
	int ret = 0;

	if (xen_domain()) {
		/* TODO: XEN domain IPC call */
		ret = xenipc_rpc_trace(RPMSG_DC_STATUS_DEV, req, result);
	} else {
		ret = rpmsg_rpc_call_trace(RPMSG_DC_STATUS_DEV, req, result);
	}

	return ret;
}
EXPORT_SYMBOL(semidrive_rpcall);

MODULE_AUTHOR("ye.liu@semidrive.com");
MODULE_ALIAS("rpmsg:rpcall");
MODULE_DESCRIPTION("Semidrive RPC kernel helper");
MODULE_LICENSE("GPL v2");

