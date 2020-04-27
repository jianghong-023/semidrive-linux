/*
 * Copyright (C) 2020 Semidriver Semiconductor Inc.
 * License terms:  GNU General Public License (GPL), version 2
 */


#include <linux/module.h>
#include <linux/rpmsg.h>
#include <linux/soc/semidrive/mb_msg.h>

#include "ipcc_rpmsg.h"

#define RPMSG_TEST_TIMEOUT 		1000
#define RPMSG_SANITY_TAG 		0xdeadbeef

#define RPMSG_RPC_REQ_PING               (0x1000)
#define RPMSG_RPC_ACK_PING               (RPMSG_RPC_REQ_PING+1)
#define RPMSG_RPC_REQ_GETTIMEOFDAY       (0x1002)
#define RPMSG_RPC_ACK_GETTIMEOFDAY       (RPMSG_RPC_REQ_GETTIMEOFDAY+1)

#define RPMSG_RPC_MAX_PARAMS     (8)
#define RPMSG_RPC_MAX_RESULT     (4)

struct rpc_req_msg {
    u32 cmd;
    u32 cksum;
    u32 param[RPMSG_RPC_MAX_PARAMS];
} ;

struct rpc_ret_msg {
    u32 ack;
    u32 retcode;
    u32 result[RPMSG_RPC_MAX_RESULT];
};

struct rpmsg_rpc_device {

	struct device *dev;
	struct mutex lock;
	struct delayed_work delay_work;
	struct rpmsg_device *rpmsg_device;
	struct rpmsg_endpoint *channel;

	int cb_err;
	struct completion done;
	struct rpc_ret_msg result;
};

int rpmsg_rpc_call(struct rpmsg_rpc_device *rpcdev, struct rpc_req_msg *req, struct rpc_ret_msg *result, int timeout)
{
	struct rpmsg_device *rpmsg_device = rpcdev->rpmsg_device;
	int ret;

	if (!rpmsg_device) {
		dev_err(rpcdev->dev, "failed to open, rpmsg is not initialized\n");
		return -EINVAL;
	}

	init_completion(&rpcdev->done);

	/* TODO: we need to calculate checksum before sending */

	/* send it */
	ret = rpmsg_send(rpmsg_device->ept, req, sizeof(*req));
	if (ret) {
		dev_err(rpcdev->dev, "failed to rpmsg_send %d %d\n", ret, req->cmd);
		goto err;
	}

	/* wait for acknowledge */
	if (!wait_for_completion_timeout
	    (&rpcdev->done, msecs_to_jiffies(timeout))) {
		dev_err(rpcdev->dev, "failed to call RPC %d\n", req->cmd);
		ret = -ETIMEDOUT;
		goto err;
	}

	/* command completed, check error */
	if (rpcdev->result.retcode) {
		dev_err(rpcdev->dev, "return failure code: %d \n", rpcdev->result.retcode);
		ret = -EIO;
		goto err;
	}

	if (result)
		memcpy(result, &rpcdev->result, sizeof(*result));

    dev_info(rpcdev->dev, "succeed to call RPC %d\n", req->cmd);

err:
    return ret;
}

int rpmsg_rpc_ping(struct rpmsg_rpc_device *rpcdev)
{
    struct rpc_ret_msg result = {0,};
    struct rpc_req_msg request = {0,};
    int ret;

    request.cmd = RPMSG_RPC_REQ_PING;
    request.cksum = RPMSG_SANITY_TAG;
    ret = rpmsg_rpc_call(rpcdev, &request, &result, RPMSG_TEST_TIMEOUT);
    if (ret < 0) {
        dev_err(rpcdev->dev, "rpc: call_func:%x fail ret: %d\n", request.cmd, ret);
        return ret;
    }

    ret = result.retcode;
    dev_dbg(rpcdev->dev, "rpc: get result: %x %d %x %x %x\n", result.ack, ret,
                result.result[0], result.result[1], result.result[2]);
    WARN_ON(result.ack != RPMSG_RPC_ACK_PING);
    WARN_ON(ret);

    if (memcmp((char*)request.param, (char*)result.result, sizeof(result.result)))
        dev_warn(rpcdev->dev, "rpc: echo not match\n");

    return ret;
}

int rpmsg_rpc_gettimeofday(struct rpmsg_rpc_device *rpcdev)
{
    struct rpc_ret_msg result;
    struct rpc_req_msg request;
    int ret;

    request.cmd = RPMSG_RPC_REQ_GETTIMEOFDAY;
    request.cksum = RPMSG_SANITY_TAG;
    ret = rpmsg_rpc_call(rpcdev, &request, &result, RPMSG_TEST_TIMEOUT);
    if (ret < 0) {
        dev_err(rpcdev->dev, "rpc: call-func:%x fail ret: %d\n", request.cmd, ret);
        return ret;
    }
    ret = result.retcode;
    dev_dbg(rpcdev->dev, "rpc: get result: %x %d %x %x %x\n", result.ack, ret,
                result.result[0], result.result[1], result.result[2]);
    WARN_ON(result.ack != RPMSG_RPC_ACK_GETTIMEOFDAY);
    WARN_ON(ret);
    return ret;
}

static int rpmsg_rpcdev_cb(struct rpmsg_device *rpdev,
						void *data, int len, void *priv, u32 src)
{
	struct rpmsg_rpc_device *rpcdev = dev_get_drvdata(&rpdev->dev);
	struct rpc_ret_msg *result;

	/* sanity check */
	if (!rpdev) {
		dev_err(NULL, "rpdev is NULL\n");
		return -EINVAL;
	}

	if (!data || !len) {
		dev_err(&rpdev->dev,
			"unexpected empty message received from src=%d\n", src);
		return -EINVAL;
	}

	if (len != sizeof(*result)) {
		dev_err(&rpdev->dev,
			"unexpected message length received from src=%d (received %d bytes while %zu bytes expected)\n",
			len, src, sizeof(*result));
		return -EINVAL;
	}

	memcpy(&rpcdev->result, data, sizeof(*result));

	/*
	 * all is fine,
	 * update status & complete command
	 */
	complete(&rpcdev->done);

	return 0;
}

static void rpc_work_handler(struct work_struct *work)
{
	struct rpmsg_rpc_device *rpcdev = container_of(to_delayed_work(work),
					 struct rpmsg_rpc_device, delay_work);

	rpmsg_rpc_ping(rpcdev);
	rpmsg_rpc_gettimeofday(rpcdev);

}

static int rpmsg_rpcdev_probe(struct rpmsg_device *rpdev)
{
	struct rpmsg_rpc_device *rpcdev;

	rpcdev = devm_kzalloc(&rpdev->dev, sizeof(*rpcdev), GFP_KERNEL);
	if (!rpcdev)
		return -ENOMEM;

	rpcdev->channel = rpdev->ept;
	rpcdev->dev = &rpdev->dev;

	rpcdev->rpmsg_device = rpdev;
	dev_set_drvdata(&rpdev->dev, rpcdev);

	/* just async call RPC */
	INIT_DELAYED_WORK(&rpcdev->delay_work, rpc_work_handler);
	schedule_delayed_work(&rpcdev->delay_work, msecs_to_jiffies(1000));

	/* TODO: add kthread to serve RPC request */
	dev_info(&rpdev->dev, "Semidrive Rpmsg RPC device probe!\n");

	return 0;
}

static void rpmsg_rpcdev_remove(struct rpmsg_device *rpdev)
{
	struct rpmsg_rpc_device *rpcdev = dev_get_drvdata(&rpdev->dev);

	rpcdev->rpmsg_device = NULL;
	dev_set_drvdata(&rpdev->dev, NULL);
}

static struct rpmsg_device_id rpmsg_rpcdev_id_table[] = {
	{.name = "rpmsg-ipcc-rpc"},
	{},
};

static struct rpmsg_driver rpmsg_rpcdev_driver = {
	.probe = rpmsg_rpcdev_probe,
	.remove = rpmsg_rpcdev_remove,
	.callback = rpmsg_rpcdev_cb,
	.id_table = rpmsg_rpcdev_id_table,
	.drv = {
		.name = "rpmsg-rpc"
	},
};

static int rpmsg_rpcdev_init(void)
{
	int ret;

	ret = register_rpmsg_driver(&rpmsg_rpcdev_driver);
	if (ret < 0) {
		pr_err("rpmsg-test: failed to register rpmsg driver\n");
	}

	pr_err("Semidrive Rpmsg driver for RPC device registered\n");

	return ret;
}
subsys_initcall(rpmsg_rpcdev_init);

static void rpmsg_rpcdev_exit(void)
{
	unregister_rpmsg_driver(&rpmsg_rpcdev_driver);
}
module_exit(rpmsg_rpcdev_exit);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_ALIAS("rpmsg:rpcdev");
MODULE_DESCRIPTION("Semidrive Rpmsg RPC kernel mode driver");
MODULE_LICENSE("GPL v2");

