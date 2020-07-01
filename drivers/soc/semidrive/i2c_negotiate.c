/*
 * Copyright (C) 2020 Semidriver Semiconductor Inc.
 * License terms:  GNU General Public License (GPL), version 2
 */

#include <linux/module.h>
#include <linux/rpmsg.h>
#include <xen/xen.h>
#include <xen/xenbus.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ipcc.h>
#include "ipcc_rpmsg.h"
#include "xenipc.h"

#define CONFIG_I2C_NEGOTIABLE		(1)

#if CONFIG_I2C_NEGOTIABLE

#define SYS_PROP_I2C_STATUS			(7)
#define RPMSG_RPC_I2C_TIMEOUT		(500)
#define RPMSG_I2C_STATUS_DEV			(0)

int rpmsg_rpc_call_trace(int dev, struct rpc_req_msg *req, struct rpc_ret_msg *result);
int xenipc_rpc_trace(int dev, struct rpc_req_msg *req, struct rpc_ret_msg *result);

static int rpc_get_i2c_status(i2c_state_t *val)
{
	struct rpc_ret_msg result;
	struct rpc_req_msg request;
	int ret = 0;

	request.cmd = SYS_RPC_REQ_GET_PROPERTY;
	request.param[0] = SYS_PROP_I2C_STATUS;

	if (xen_domain()) {
		/* TODO: XEN domain IPC call */
		ret = xenipc_rpc_trace(RPMSG_I2C_STATUS_DEV, &request, &result);
	} else {
		ret = rpmsg_rpc_call_trace(RPMSG_I2C_STATUS_DEV, &request, &result);
	}

	if (!ret && val) {
		*val = result.result[0];
	}

	return ret;
}

static int rpc_set_i2c_status(i2c_state_t val, bool block)
{
	struct rpc_ret_msg result;
	struct rpc_req_msg request;
	int ret = 0;

	request.cmd = SYS_RPC_REQ_SET_PROPERTY;
	request.param[0] = SYS_PROP_I2C_STATUS;
	request.param[1] = val;
	request.param[2] = block;

	if (xen_domain()) {
		/* TODO: XEN domain IPC call */
		ret = xenipc_rpc_trace(RPMSG_I2C_STATUS_DEV, &request, &result);
	} else {
		ret = rpmsg_rpc_call_trace(RPMSG_I2C_STATUS_DEV, &request, &result);
	}

	return ret;
}

int sd_get_i2c_status(i2c_state_t *val)
{
	return rpc_get_i2c_status(val);
}
EXPORT_SYMBOL(sd_get_i2c_status);

int sd_set_i2c_status(i2c_state_t val)
{
	return rpc_set_i2c_status(val, true);
}
EXPORT_SYMBOL(sd_set_i2c_status);

#endif	//CONFIG_I2C_NEGOTIABLE


