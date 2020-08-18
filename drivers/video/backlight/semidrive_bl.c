/* Copyright (c) 2020, Semidrive Semi.
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


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/soc/semidrive/ipcc.h>
#include <linux/soc/semidrive/xenipc.h>

struct rpmsg_bl_data {
	struct device	*dev;
	unsigned int	brightness;
	unsigned int	*levels;
	bool			enabled;
	unsigned int	scale;
};

struct platform_rpmsg_bl_data {
	unsigned int max_brightness;
	unsigned int dft_brightness;
	unsigned int lth_brightness;
	unsigned int *levels;
};

#define RPMSG_BL_DEV			(0)

int rpmsg_rpc_call_trace(int dev, struct rpc_req_msg *req, struct rpc_ret_msg *result);
int xenipc_rpc_trace(int dev, struct rpc_req_msg *req, struct rpc_ret_msg *result);

static int backlight_rpc_set_brightness(int brightness)
{
	struct rpc_ret_msg result = {0,};
	struct rpc_req_msg request = {0,};
	int ret = 0;

	request.cmd = MOD_RPC_REQ_SET_BRIGHT;
	request.param[0] = brightness;

	if (xen_domain()) {
		/* TODO: XEN domain IPC call */
		ret = xenipc_rpc_trace(RPMSG_BL_DEV, &request, &result);
	} else {
		ret = rpmsg_rpc_call_trace(RPMSG_BL_DEV, &request, &result);
	}

	return ret;
}

static void rpmsg_backlight_power_on(struct rpmsg_bl_data *pb, int brightness)
{
	if (brightness != pb->brightness) {
		backlight_rpc_set_brightness(brightness);
		pb->brightness = brightness;
		pb->enabled = !!brightness;
		dev_info(pb->dev, "%s\n", __func__);
	}
}

static void rpmsg_backlight_power_off(struct rpmsg_bl_data *pb)
{
	if (!pb->enabled)
		return;

	backlight_rpc_set_brightness(0);
	pb->brightness = 0;
	pb->enabled = false;
	dev_info(pb->dev, "%s\n", __func__);
}

static int rpmsg_backlight_update_status(struct backlight_device *bl)
{
	struct rpmsg_bl_data *pb = bl_get_data(bl);
	int brightness = bl->props.brightness;

	if (bl->props.power != FB_BLANK_UNBLANK ||
	    bl->props.fb_blank != FB_BLANK_UNBLANK ||
	    bl->props.state & BL_CORE_FBBLANK)
		brightness = 0;

	if (brightness > 0) {
		rpmsg_backlight_power_on(pb, brightness);
	} else
		rpmsg_backlight_power_off(pb);

	return 0;
}

static int rpmsg_backlight_check_fb(struct backlight_device *bl,
				  struct fb_info *info)
{
	return true;
}

static const struct backlight_ops rpmsg_backlight_ops = {
	.update_status	= rpmsg_backlight_update_status,
	.check_fb	= rpmsg_backlight_check_fb,
};

static int rpmsg_backlight_parse_dt(struct device *dev,
				  struct platform_rpmsg_bl_data *data)
{
	struct device_node *node = dev->of_node;
	struct property *prop;
	int length;
	u32 value;
	int ret;

	if (!node)
		return -ENODEV;

	memset(data, 0, sizeof(*data));

	/* determine the number of brightness levels */
	prop = of_find_property(node, "brightness-levels", &length);
	if (!prop)
		return -EINVAL;

	data->max_brightness = length / sizeof(u32);

	/* read brightness levels from DT property */
	if (data->max_brightness > 0) {
		size_t size = sizeof(*data->levels) * data->max_brightness;

		data->levels = devm_kzalloc(dev, size, GFP_KERNEL);
		if (!data->levels)
			return -ENOMEM;

		ret = of_property_read_u32_array(node, "brightness-levels",
						 data->levels,
						 data->max_brightness);
		if (ret < 0)
			return ret;

		ret = of_property_read_u32(node, "default-brightness-level",
					   &value);
		if (ret < 0)
			return ret;

		data->dft_brightness = value;
		data->max_brightness--;
	}

	return 0;
}

static const struct of_device_id rpmsg_backlight_of_match[] = {
	{ .compatible = "sd,rpmsg-bl" },
	{ }
};

MODULE_DEVICE_TABLE(of, rpmsg_backlight_of_match);

static int rpmsg_backlight_initial_power_state(const struct rpmsg_bl_data *pb)
{
	struct device_node *node = pb->dev->of_node;

	/* Not booted with device tree or no phandle link to the node */
	if (!node || !node->phandle)
		return FB_BLANK_UNBLANK;

	return FB_BLANK_UNBLANK;
}

static int rpmsg_backlight_probe(struct platform_device *pdev)
{
	struct platform_rpmsg_bl_data *data = dev_get_platdata(&pdev->dev);
	struct platform_rpmsg_bl_data defdata;
	struct backlight_properties props;
	struct backlight_device *bl;
	struct rpmsg_bl_data *pb;
	int ret;

	if (!data) {
		ret = rpmsg_backlight_parse_dt(&pdev->dev, &defdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to find platform data\n");
			return ret;
		}

		data = &defdata;
	}

	pb = devm_kzalloc(&pdev->dev, sizeof(*pb), GFP_KERNEL);
	if (!pb) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	if (data->levels) {
		unsigned int i;

		for (i = 0; i <= data->max_brightness; i++)
			if (data->levels[i] > pb->scale)
				pb->scale = data->levels[i];

		pb->levels = data->levels;
	} else
		pb->scale = data->max_brightness;

	pb->dev = &pdev->dev;
	pb->enabled = false;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = data->max_brightness;
	bl = backlight_device_register(dev_name(&pdev->dev), &pdev->dev, pb,
				       &rpmsg_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
	}

	bl->props.brightness = data->dft_brightness;
	bl->props.power = rpmsg_backlight_initial_power_state(pb);
	backlight_update_status(bl);

	platform_set_drvdata(pdev, bl);
	return 0;

err_alloc:

	return ret;
}

static int rpmsg_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct rpmsg_bl_data *pb = bl_get_data(bl);

	backlight_device_unregister(bl);
	rpmsg_backlight_power_off(pb);

	return 0;
}

static void rpmsg_backlight_shutdown(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct rpmsg_bl_data *pb = bl_get_data(bl);

	rpmsg_backlight_power_off(pb);
}

static struct platform_driver rpmsg_backlight_driver = {
	.driver		= {
		.name	= "rpmsg-bl",
		.of_match_table	= of_match_ptr(rpmsg_backlight_of_match),
	},
	.probe		= rpmsg_backlight_probe,
	.remove		= rpmsg_backlight_remove,
	.shutdown	= rpmsg_backlight_shutdown,
};

module_platform_driver(rpmsg_backlight_driver);


MODULE_DESCRIPTION("RPMSG based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ye.liu@semidrive.com");
MODULE_ALIAS("rpmsg:backlight");

