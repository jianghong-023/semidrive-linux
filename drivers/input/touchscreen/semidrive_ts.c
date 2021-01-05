/*
 * Copyright (C) Semidrive Semiconductor Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/dmi.h>
#include <linux/firmware.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <asm/unaligned.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/mailbox_client.h>
#include <xen/xen.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ipcc.h>
#include <linux/soc/semidrive/xenipc.h>

struct semidrive_sts_data {
	struct platform_device *pdev;
	struct mbox_client client;
	struct mbox_chan *mbox;
	struct work_struct rx_work;
	struct input_dev *input_dev;
	int abs_x_max;
	int abs_y_max;
	int x_offset;
	int y_offset;
	bool swapped_x_y;
	bool inverted_x;
	bool inverted_y;
	unsigned int max_touch_num;
	u16 id;
	u16 version;
	u32 dev_type;	/* 0: main device, 1: aux device */
	u16 instance;
};

#define STS_MAX_HEIGHT		800
#define STS_MAX_WIDTH		1280
#define STS_CONTACT_SIZE		8
#define STS_MAX_CONTACTS		10
#define STS_BUFFER_STATUS_READY		BIT(7)

enum sts_op_type {
	STS_OP_GET_VERSION,
	STS_OP_GET_CONFIG,
	STS_OP_SET_CONFIG,
	STS_OP_RESET,
	STS_OP_SET_INITED,
};

/* Do not exceed 16 bytes so far */
struct sts_ioctl_cmd {
	u16 op;
	u16 instance;
	union {
		struct {
			u16 what;
			u16 why;
			u32 how;
		} s;
	} msg;
};

/* Do not exceed 16 bytes so far */
struct sts_ioctl_result {
	u16 op;
	u16 instance;
	union {
		/** used for get_version */
		struct {
			u16 version;
			u16 id;
			u16 dev_type;
		} v;
		/** used for get_config */
		struct {
			u16 abs_x_max;
			u16 abs_y_max;
			u16 touch_num;
			u16 x_offset;
			u16 y_offset;
		} gc;
		u8 data[12];
	} msg;
};

static int semidrive_read_input_report(struct semidrive_sts_data *ts, u8 *data)
{
	int touch_num;

	if (data[0] & STS_BUFFER_STATUS_READY) {
		touch_num = data[0] & 0x0f;
		if (touch_num > ts->max_touch_num) {
			return -EPROTO;
		}

		return touch_num;
	}

	return 0;
}

static void
semidrive_sts_report_touch(struct semidrive_sts_data *ts, u8 *coor_data)
{
	int id = coor_data[0] & 0x0F;
	int input_x = get_unaligned_le16(&coor_data[1]);
	int input_y = get_unaligned_le16(&coor_data[3]);
	int input_w = get_unaligned_le16(&coor_data[5]);

	//dev_err(&ts->pdev->dev, "(%d %d)\n", input_x, input_y);
	input_x -= ts->x_offset;
	input_y -= ts->y_offset;
	if ((input_x < 0) || (input_x > ts->abs_x_max))
		return;
	if ((input_y < 0) || (input_y > ts->abs_y_max))
		return;

	/* Inversions have to happen before axis swapping */
	if (ts->inverted_x)
		input_x = ts->abs_x_max - input_x;
	if (ts->inverted_y)
		input_y = ts->abs_y_max - input_y;
	if (ts->swapped_x_y)
		swap(input_x, input_y);

	input_mt_slot(ts->input_dev, id);
	input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_X, input_x);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, input_y);
	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, input_w);
	input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, input_w);

	dev_info(&ts->pdev->dev, "point (%d %d %d)\n", input_x, input_y, input_w);
}

/**
 * semidrive_process_events - Process incoming events
 *
 * @ts: our semidrive_sts_data pointer
 * @point_data: our pointe data
 *
 * Called when the IRQ is triggered. Read the current device state, and push
 * the input events to the user space.
 */
static void
semidrive_process_events(struct semidrive_sts_data *ts, u8 *point_data)
{
	int touch_num;
	int i;

	touch_num = semidrive_read_input_report(ts, point_data);
	if ((touch_num < 0) || (point_data[0] == 0))
		return;

	/*
	 * Bit 4 of the first byte reports the status of the capacitive
	 * Windows/Home button.
	 */
	input_report_key(ts->input_dev, KEY_LEFTMETA, point_data[0] & BIT(4));

	for (i = 0; i < touch_num; i++)
		semidrive_sts_report_touch(ts,
				&point_data[1 + STS_CONTACT_SIZE * i]);

	input_mt_sync_frame(ts->input_dev);
	input_sync(ts->input_dev);
}

static void semidrive_free_irq(struct semidrive_sts_data *ts)
{
	mbox_free_channel(ts->mbox);
}

static void sts_work_handler(struct work_struct *work)
{
	//TODO: send frontent to trigger irq by event channel
	pr_err("%s: Not implemented\n", __func__);
}

static void sts_coor_updated(struct mbox_client *client, void *mssg)
{
	struct semidrive_sts_data *ts = container_of(client,
						struct semidrive_sts_data, client);
	struct platform_device *pdev = ts->pdev;
	sd_msghdr_t *msghdr = mssg;

	if (!msghdr) {
		dev_err(&pdev->dev, "%s NULL mssg\n", __func__);
		return;
	}

	semidrive_process_events(ts, msghdr->data);

	return;
}

static int semidrive_request_irq(struct semidrive_sts_data *ts)
{
	struct platform_device *pdev = ts->pdev;
	struct mbox_client *client;
	int ret = 0;

	INIT_WORK(&ts->rx_work, sts_work_handler);

	client = &ts->client;
	/* Initialize the mbox channel used by touchscreen */
	client->dev = &pdev->dev;
	client->tx_done = NULL;
	client->rx_callback = sts_coor_updated;
	client->tx_block = true;
	client->tx_tout = 1000;
	client->knows_txdone = false;
	ts->mbox = mbox_request_channel(client, 0);
	if (IS_ERR(ts->mbox)) {
		ret = -EBUSY;
		dev_err(&pdev->dev, "mbox_request_channel failed: %ld\n",
				PTR_ERR(ts->mbox));
	}

	return ret;
}

static int semidrive_sts_ioctl(struct semidrive_sts_data *ts, int command,
								struct sts_ioctl_result *data)
{
	struct rpc_ret_msg result = {0,};
	struct rpc_req_msg request;
	struct sts_ioctl_cmd *ctl = DCF_RPC_PARAM(request, struct sts_ioctl_cmd);
	struct sts_ioctl_result *rs = (struct sts_ioctl_result *) &result.result[0];
	int ret = 0;

	DCF_INIT_RPC_REQ(request, MOD_RPC_REQ_STS_IOCTL);
	ctl->op = command;
	ctl->instance = ts->instance;
	dev_err(&ts->pdev->dev, "%s():[%d]\n", __func__, ctl->instance);
	ret = semidrive_rpcall(&request, &result);
	if (ret < 0 || result.retcode < 0) {
		dev_err(&ts->pdev->dev, "rpcall failed=%d %d\n", ret, result.retcode);
		goto fail_call;
	}

	memcpy(data, rs, sizeof(*rs));

fail_call:

	return ret;
}

/**
 * semidrive_reset - Reset device during power on
 *
 * @ts: semidrive_sts_data pointer
 */
static int semidrive_reset(struct semidrive_sts_data *ts)
{
	/* TODO: */
	return 0;
}

static int semidrive_set_inited(struct semidrive_sts_data *ts)
{
	struct sts_ioctl_result op_ret;

	semidrive_sts_ioctl(ts, STS_OP_SET_INITED, &op_ret);
	return 0;
}

/**
 * semidrive_read_config - Read the embedded configuration of the panel
 *
 * @ts: our semidrive_sts_data pointer
 *
 * Must be called during probe
 */
static void semidrive_read_config(struct semidrive_sts_data *ts)
{
	struct sts_ioctl_result op_ret;
	int ret;

	ret = semidrive_sts_ioctl(ts, STS_OP_GET_CONFIG, &op_ret);
	dev_info(&ts->pdev->dev, "%s: ret=%d\n", __func__, ret);
	if (ret) {
		goto apply_default;
	}

	ts->abs_x_max = op_ret.msg.gc.abs_x_max;
	ts->abs_y_max = op_ret.msg.gc.abs_y_max;
	if (ts->swapped_x_y)
		swap(ts->abs_x_max, ts->abs_y_max);
	ts->x_offset = op_ret.msg.gc.x_offset;
	ts->y_offset = op_ret.msg.gc.y_offset;
	ts->max_touch_num = op_ret.msg.gc.touch_num;
	if (!ts->abs_x_max || !ts->abs_y_max || !ts->max_touch_num) {
		goto apply_default;
	}
	dev_err(&ts->pdev->dev, "(%d-%d)[%d]\n", ts->abs_x_max, ts->abs_y_max, ts->max_touch_num);

	return;

apply_default:
	dev_err(&ts->pdev->dev, "Invalid config, using defaults\n");
	ts->abs_x_max = STS_MAX_WIDTH;
	ts->abs_y_max = STS_MAX_HEIGHT;
	if (ts->swapped_x_y)
		swap(ts->abs_x_max, ts->abs_y_max);
	ts->max_touch_num = STS_MAX_CONTACTS;
	ts->x_offset = 0;
	ts->y_offset = 0;
	dev_err(&ts->pdev->dev, "(%d-%d)[%d]\n", ts->abs_x_max, ts->abs_y_max, ts->max_touch_num);
}

/**
 * semidrive_read_version - Read safe touchscreen version
 *
 * @ts: our semidrive_sts_data pointer
 */
static int semidrive_read_version(struct semidrive_sts_data *ts)
{
	int ret;
	struct sts_ioctl_result op_ret;

	ret = semidrive_sts_ioctl(ts, STS_OP_GET_VERSION, &op_ret);
	if (ret < 0) {
		ts->id = 0x1001;
		dev_err(&ts->pdev->dev, "read version failed: %d\n", ret);
		return ret;
	}

	ts->version = op_ret.msg.v.version;
	ts->id = op_ret.msg.v.id;
	ts->dev_type = op_ret.msg.v.dev_type;

	dev_err(&ts->pdev->dev, "ID %d, version: %04x, type: %d\n", ts->id, ts->version, ts->dev_type);

	return 0;
}

/**
 * semidrive_register_input_dev - Allocate, populate and register the input device
 *
 * @ts: our semidrive_sts_data pointer
 *
 * Must be called during probe
 */
static int semidrive_register_input_dev(struct semidrive_sts_data *ts)
{
	int ret = 0;

	ts->input_dev = devm_input_allocate_device(&ts->pdev->dev);
	if (!ts->input_dev) {
		dev_err(&ts->pdev->dev, "Failed to allocate input device.");
		return -ENOMEM;
	}

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
			     0, ts->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
			     0, ts->abs_y_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);

	input_mt_init_slots(ts->input_dev, ts->max_touch_num,
			    INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);

	ts->input_dev->name = "Semidrive Safe TouchScreen";
	if (ts->dev_type == 1)
		ts->input_dev->phys = "input/ts_aux";
	else
		ts->input_dev->phys = "input/ts_main";
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->id.vendor = 0x0416;
	ts->input_dev->id.product = ts->id;
	ts->input_dev->id.version = ts->version;

	/* Capacitive Windows/Home button on some devices */
	input_set_capability(ts->input_dev, EV_KEY, KEY_LEFTMETA);
	ret = input_register_device(ts->input_dev);
	if (ret) {
		dev_err(&ts->pdev->dev, "Failed to register input device: %d", ret);
	}

	return ret;
}

/**
 * semidrive_configure_dev - Finish device initialization
 *
 * @ts: our semidrive_sts_data pointer
 *
 * Must be called from probe to finish initialization of the device.
 * Contains the common initialization code for both devices that
 * declare gpio pins and devices that do not. It is either called
 * directly from probe or from request_firmware_wait callback.
 */
static int semidrive_configure_dev(struct semidrive_sts_data *ts)
{
	struct platform_device *pdev = ts->pdev;
	int ret = 0;

	ts->swapped_x_y = device_property_read_bool(&pdev->dev,
						    "touchscreen-swapped-x-y");
	ts->inverted_x = device_property_read_bool(&pdev->dev,
						   "touchscreen-inverted-x");
	ts->inverted_y = device_property_read_bool(&pdev->dev,
						   "touchscreen-inverted-y");

	semidrive_read_config(ts);

	ret = semidrive_register_input_dev(ts);
	if (ret)
		return ret;

	ret = semidrive_request_irq(ts);

	return ret;
}

static int semidrive_sts_probe(struct platform_device *pdev)
{
	struct semidrive_sts_data *ts;
	int ret;

	ts = devm_kzalloc(&pdev->dev, sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	ts->pdev = pdev;
	platform_set_drvdata(pdev, ts);

	/* Read instance from dts */
	ret = of_property_read_u16(pdev->dev.of_node, "touchscreen-id", &ts->instance);
	if (ret < 0) {
		dev_err(&pdev->dev, "Missing touchscreen id, use 0\n");
		ts->instance = 0;
	}

	ret = semidrive_reset(ts);
	if (ret) {
		dev_err(&pdev->dev, "reset failed: %d\n", ret);
		goto err;
	}

	ret = semidrive_read_version(ts);
	if (ret) {
		dev_err(&pdev->dev, "Read version failed %d\n", ret);
		goto err;
	}

	ret = semidrive_configure_dev(ts);
	if (ret) {
		dev_err(&pdev->dev, "configure dev failed %d\n", ret);
		goto err;
	}

	ret = semidrive_set_inited(ts);
	if (ret) {
		dev_err(&pdev->dev, "set init failed %d\n", ret);
		goto err;
	}

	dev_err(&pdev->dev, "%s(): done ok\n", __func__);

	return 0;

err:
	kfree(ts);
	return ret;
}

static int semidrive_sts_remove(struct platform_device *pdev)
{
	struct semidrive_sts_data *ts = platform_get_drvdata(pdev);

	semidrive_free_irq(ts);

	return 0;
}

static int __maybe_unused semidrive_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused semidrive_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(semidrive_pm_ops, semidrive_suspend, semidrive_resume);

static const struct platform_device_id semidrive_sts_id[] = {
	{ "sd,safetouch", 0 },
	{ }
};
MODULE_DEVICE_TABLE(platform, semidrive_sts_id);

static const struct of_device_id semidrive_of_match[] = {
	{ .compatible = "sd,safetouch" },
	{ }
};
MODULE_DEVICE_TABLE(of, semidrive_of_match);

static struct platform_driver semidrive_sts_driver = {
	.probe = semidrive_sts_probe,
	.remove = semidrive_sts_remove,
	.id_table = semidrive_sts_id,
	.driver = {
		.name = "semidrive,safets",
		.of_match_table = of_match_ptr(semidrive_of_match),
		.pm = &semidrive_pm_ops,
	},
};
module_platform_driver(semidrive_sts_driver);

MODULE_AUTHOR("<ye.liu@semidrive.com>");
MODULE_DESCRIPTION("Semidrive Safe TouchScreen driver");
MODULE_LICENSE("GPL v2");

