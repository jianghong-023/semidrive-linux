/*
 * kunlun-core-kstream.h
 *
 * Semidrive kunlun platform v4l2 core file
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/media-bus-format.h>
#include <linux/media.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/videodev2.h>

#include <media/media-device.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-fwnode.h>

#include "kunlun-csi.h"

#define KUNLUN_IMG_OFFSET			0x100
#define KUNLUN_IMG_REG_LEN			0x80

#define CSI_CORE_ENABLE		0x00
#define CSI_RESET	BIT(8)

#define CSI_INT_0					0x04
#define CSI_INT_1					0x08
#define CSI_STORE_DONE_MASK			0x0F
#define CSI_STORE_DONE_SHIFT		0
#define CSI_SDW_SET_MASK			0xF0
#define CSI_SDW_SET_SHIFT			4
#define CSI_CROP_ERR_MASK			0x0F
#define CSI_CROP_ERR_SHIFT			0
#define CSI_PIXEL_ERR_MASK			0xF0
#define CSI_PIXEL_ERR_SHIFT			4
#define CSI_OVERFLOW_MASK			0xF00
#define CSI_OVERFLOW_SHIFT			8
#define CSI_BUS_ERR_MASK			0xFF000
#define CSI_BUS_ERR_SHIFT			12
#define CSI_BT_ERR_MASK				0x100
#define CSI_BT_FATAL_MASK			0x200
#define CSI_BT_COF_MASK				0x400

#define KUNLUN_IMG_REG_BASE(i)		\
	(KUNLUN_IMG_OFFSET + (i) * (KUNLUN_IMG_REG_LEN))


static int kstream_subdev_notifier_complete(struct v4l2_async_notifier *async)
{
	struct csi_core *csi = container_of(async, struct csi_core, notifier);
	struct v4l2_subdev *sd, *source_sd;
	struct kstream_device *kstream;
	struct media_entity *source, *sink;
	struct kstream_interface *interface;
	struct kstream_sensor *sensor;
	unsigned int kstream_sink_pad;
	int ret;

	list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
		interface = &kstream->interface;
		sensor = &kstream->sensor;
		sd = &kstream->subdev;
		source_sd = interface->subdev;

		source = &source_sd->entity;
		sink = &sd->entity;

		ret = kunlun_find_pad(sd, MEDIA_PAD_FL_SINK);
		kstream_sink_pad = ret < 0 ? 0 : ret;

		kstream->ops->init_interface(kstream);

		/* link interface --> csi stream module */
		ret = media_create_pad_link(source, interface->source_pad,
				sink, kstream_sink_pad,
				MEDIA_LNK_FL_IMMUTABLE | MEDIA_LNK_FL_ENABLED);
		if(ret < 0) {
			dev_err(kstream->dev, "Failed to link %s - %s entities: %d\n",
					source->name, sink->name, ret);
			return ret;
		}
		dev_info(kstream->dev, "Link %s -> %s\n", source->name, sink->name);

		sink = source;
		source_sd = sensor->subdev;
		source = &source_sd->entity;

		/* link sensor --> interface */
		ret = media_create_pad_link(source, sensor->source_pad,
				sink, interface->sink_pad,
				MEDIA_LNK_FL_IMMUTABLE | MEDIA_LNK_FL_ENABLED);
		if(ret < 0) {
			dev_err(kstream->dev, "Failed to link %s - %s entities: %d\n",
					source->name, sink->name, ret);
			return ret;
		}
		dev_info(kstream->dev, "Link %s -> %s\n", source->name, sink->name);
	}

	ret = v4l2_device_register_subdev_nodes(&csi->v4l2_dev);
	if(ret < 0)
		return ret;

	return media_device_register(&csi->media_dev);
}

static int kstream_interface_subdev_bound(struct v4l2_subdev *subdev,
		struct v4l2_async_subdev *asd)
{
	struct kstream_device *kstream =
		container_of(asd, struct kstream_device, interface.asd);
	int ret;

	kstream->interface.subdev = subdev;
	kstream->interface.mbus_type = kstream->core->mbus_type;
	ret = kunlun_find_pad(subdev, MEDIA_PAD_FL_SOURCE);
	if(ret < 0)
		return ret;
	kstream->interface.source_pad = ret;

	ret = kunlun_find_pad(subdev, MEDIA_PAD_FL_SINK);
	kstream->interface.sink_pad = ret < 0 ? 0 : ret;

	dev_dbg(kstream->dev, "Bound subdev %s source pad: %u sink pad: %u\n",
			subdev->name, kstream->interface.source_pad, kstream->interface.sink_pad);
	return 0;
}

static int kstream_sensor_subdev_bound(struct v4l2_subdev *subdev,
		struct v4l2_async_subdev *asd)
{
	struct kstream_device *kstream =
		container_of(asd, struct kstream_device, sensor.asd);
	int ret;

	kstream->sensor.subdev = subdev;
	ret = kunlun_find_pad(subdev, MEDIA_PAD_FL_SOURCE);
	if(ret < 0)
		return ret;
	kstream->sensor.source_pad = ret;

	dev_dbg(kstream->dev, "Bound subdev %s source pad: %u\n",
			subdev->name, kstream->sensor.source_pad);
	return 0;
}

static int kstream_subdev_notifier_bound(struct v4l2_async_notifier *async,
		struct v4l2_subdev *subdev, struct v4l2_async_subdev *asd)
{
	switch(subdev->entity.function) {
		case MEDIA_ENT_F_IO_V4L:
			return kstream_interface_subdev_bound(subdev, asd);
			break;
		case MEDIA_ENT_F_CAM_SENSOR:
			return kstream_sensor_subdev_bound(subdev, asd);
			break;
		default:
			if(subdev->entity.flags)
				return kstream_sensor_subdev_bound(subdev, asd);
			break;
	}
	return 0;
}

static int kunlun_stream_register_device(struct csi_core *csi)
{
	struct kstream_device *kstream;
	int ret = 0;

	list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
		ret = kunlun_stream_register_entities(kstream, &csi->v4l2_dev);
		if(ret < 0)
			return ret;
	}
	return ret;
}

static int kunlun_stream_unregister_device(struct csi_core *csi)
{
	struct kstream_device *kstream;

	list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
		kunlun_stream_unregister_entities(kstream);
	}

	return 0;
}

static int kunlun_of_parse_ports(struct csi_core *csi)
{
	struct device_node *img_ep, *interface, *sensor, *intf_ep;
	struct device *dev = csi->dev;
	struct v4l2_async_notifier *notifier = &csi->notifier;
	struct of_endpoint ep;
	struct kstream_device *kstream;
	unsigned int size, i;
	int ret;

	for_each_endpoint_of_node(dev->of_node, img_ep) {
		if(csi->kstream_nums > KUNLUN_IMG_NUM) {
			dev_warn(dev, "Max support %d ips\n", KUNLUN_IMG_NUM);
			break;
		}

		kstream = devm_kzalloc(dev, sizeof(*kstream), GFP_KERNEL);
		if(!kstream) {
			dev_err(dev, "Failed to allocate memory for kstream device!\n");
			return -ENOMEM;
		}

		ret = of_graph_parse_endpoint(img_ep, &ep);

		if((ret < 0) || (ep.port >= KUNLUN_IMG_NUM)) {
			dev_err(dev, "Can't get port id\n");
			return ret;
		}

		kstream->id = ep.port;
		kstream->dev = csi->dev;
		kstream->core = csi;
		kstream->base = csi->base + KUNLUN_IMG_REG_BASE(kstream->id);
		csi->kstream_nums++;

		interface = of_graph_get_remote_port_parent(img_ep);
		if(!interface) {
			dev_dbg(dev, "Cannot get remote parent\n");
			continue;
		}

		kstream->interface.asd.match_type = V4L2_ASYNC_MATCH_FWNODE;
		kstream->interface.asd.match.fwnode.fwnode =
			of_fwnode_handle(interface);
		notifier->num_subdevs++;

		for_each_endpoint_of_node(interface, intf_ep) {
			sensor = of_graph_get_remote_port_parent(intf_ep);
			if(!sensor || sensor == dev->of_node) {
				of_node_put(sensor);
				continue;
			}

			kstream->sensor.asd.match_type = V4L2_ASYNC_MATCH_FWNODE;
			kstream->sensor.asd.match.fwnode.fwnode =
				of_fwnode_handle(sensor);
			of_node_put(sensor);
			notifier->num_subdevs++;
			kstream->enabled = true;
			list_add_tail(&kstream->csi_entry, &csi->kstreams);
		}
		of_node_put(interface);
	}

	if(!notifier->num_subdevs) {
		dev_err(dev, "No subdev found in %s", dev->of_node->name);
		return -EINVAL;
	}

	size = sizeof(*notifier->subdevs) * notifier->num_subdevs;
	notifier->subdevs = devm_kzalloc(dev, size, GFP_KERNEL);

	i = 0;
	list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
		notifier->subdevs[i++] = &kstream->interface.asd;
		notifier->subdevs[i++] = &kstream->sensor.asd;
	}

	return notifier->num_subdevs;
}

static int kunlun_of_parse_core(struct platform_device *pdev,
		struct csi_core *csi)
{
	struct resource *res;
	struct device *dev = csi->dev;
	struct fwnode_handle *fwnode;
	int ret;
	const char *mbus;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	csi->base = devm_ioremap_resource(dev, res);
	if(IS_ERR(csi->base)) {
		dev_err(dev, "Could not get csi reg map\n");
		return PTR_ERR(csi->base);
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if(!res) {
		dev_err(dev, "Missing IRQ\n");
		return -EINVAL;
	}

	csi->irq = res->start;

	ret = of_property_read_string(dev->of_node, "mbus-type", &mbus);
	if(ret < 0) {
		dev_err(dev, "Missing mbus-type\n");
		return -EINVAL;
	}

	if(strcmp(mbus, "parallel") == 0)
		csi->mbus_type = V4L2_MBUS_PARALLEL;
	else if(strcmp(mbus, "bt656") == 0)
		csi->mbus_type = V4L2_MBUS_BT656;
	else if(strcmp(mbus, "mipi-csi2") == 0)
		csi->mbus_type = V4L2_MBUS_CSI2;
	else if(strcmp(mbus, "dc2csi-1") == 0)
		csi->mbus_type = KUNLUN_MBUS_DC2CSI1;
	else if(strcmp(mbus, "dc2csi-2") == 0)
		csi->mbus_type = KUNLUN_MBUS_DC2CSI2;
	else
		dev_err(dev, "Unknow mbus-type\n");

	fwnode = dev_fwnode(dev);
	ret = v4l2_fwnode_endpoint_parse(fwnode, &csi->vep);
	fwnode_handle_put(fwnode);
	if(ret) {
		dev_dbg(dev, "v4l2 fwnode not found!\n");
	} else if(csi->mbus_type == V4L2_MBUS_BT656
			|| csi->mbus_type == V4L2_MBUS_PARALLEL) {
		dev_dbg(dev, "bus_flags: %08X\n", csi->vep.bus.parallel.flags);
	}

	return ret;
}

static void kunlun_init_core(struct csi_core *csi)
{
	kcsi_writel(csi->base, CSI_CORE_ENABLE, CSI_RESET);
	usleep_range(2, 10);
	kcsi_clr(csi->base, CSI_CORE_ENABLE, CSI_RESET);
}

static const struct media_device_ops kunlun_media_ops = {
	.link_notify = v4l2_pipeline_link_notify,
};

static void kunlun_csi_frm_done_isr(struct csi_core *csi, u32 val)
{
	struct kstream_device *kstream;
	unsigned int ids = (val & CSI_STORE_DONE_MASK) >> CSI_STORE_DONE_SHIFT;

	list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
		if(kstream->state != RUNNING && kstream->state != IDLE)
			continue;
		if(!((1 << kstream->id) & ids))
			continue;

		kstream->ops->frm_done_isr(kstream);
		csi->ints.frm_cnt++;
	}
}

static void kunlun_csi_img_update_isr(struct csi_core *csi, u32 val)
{
	struct kstream_device *kstream;
	unsigned int ids = (val & CSI_SDW_SET_MASK) >> CSI_SDW_SET_SHIFT;

	list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
		if(kstream->state != RUNNING && kstream->state != IDLE)
			continue;
		if(!((1 << kstream->id) & ids))
			continue;

		kstream->ops->img_update_isr(kstream);
	}
}

static void kunlun_csi_bt_err_isr(struct csi_core *csi, u32 val)
{
	if(csi->mbus_type != V4L2_MBUS_BT656)
		return;

	if(val & CSI_BT_ERR_MASK)
		csi->ints.bt_err++;

	if(val & CSI_BT_FATAL_MASK)
		csi->ints.bt_fatal++;

	if(val & CSI_BT_COF_MASK)
		csi->ints.bt_cof++;
}

static void kunlun_csi_int1_isr(struct csi_core *csi, u32 val)
{
	struct kstream_device *kstream;

	list_for_each_entry(kstream, &csi->kstreams, csi_entry) {
		if(kstream->state != RUNNING && kstream->state != IDLE)
			continue;

		if((1 << kstream->id) &
				((val & CSI_CROP_ERR_MASK) >> CSI_CROP_ERR_SHIFT))
			csi->ints.crop_err++;

		if((1 << kstream->id) &
				((val & CSI_PIXEL_ERR_MASK) >> CSI_PIXEL_ERR_SHIFT))
			csi->ints.pixel_err++;

		if((1 << kstream->id) &
				((val & CSI_OVERFLOW_MASK) >> CSI_OVERFLOW_SHIFT))
			csi->ints.overflow++;

		if((3 << kstream->id) &
				((val & CSI_BUS_ERR_MASK) >> CSI_BUS_ERR_SHIFT))
			csi->ints.bus_err++;
	}
}

static irqreturn_t kunlun_csi_isr(int irq, void *dev)
{
	struct csi_core *csi = dev;
	u32 value0, value1;

	value0 = kcsi_readl(csi->base, CSI_INT_0);
	value1 = kcsi_readl(csi->base, CSI_INT_1);

	kcsi_writel(csi->base, CSI_INT_0, value0);
	kcsi_writel(csi->base, CSI_INT_1, value1);

	if(value0 & CSI_STORE_DONE_MASK)
		kunlun_csi_frm_done_isr(csi, value0);

	if(value0 & CSI_SDW_SET_MASK)
		kunlun_csi_img_update_isr(csi, value0);

	if(value0 & (CSI_BT_ERR_MASK | CSI_BT_FATAL_MASK | CSI_BT_COF_MASK))
		kunlun_csi_bt_err_isr(csi, value0);

	if(value1)
		kunlun_csi_int1_isr(csi, value1);

	return IRQ_HANDLED;
}

static int kunlun_csi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct csi_core *csi;
	int ret;

	csi = devm_kzalloc(dev, sizeof(*csi), GFP_KERNEL);
	if(!csi)
		return -ENOMEM;

	csi->dev = dev;
	atomic_set(&csi->ref_count, 0);
	platform_set_drvdata(pdev, csi);
	INIT_LIST_HEAD(&csi->kstreams);

	ret = kunlun_of_parse_core(pdev, csi);
	if(ret < 0)
		return ret;

	kunlun_init_core(csi);

	ret = kunlun_of_parse_ports(csi);
	if(ret < 0)
		return ret;

	csi->media_dev.dev = csi->dev;
	strlcpy(csi->media_dev.model, "Kunlun camera module",
			sizeof(csi->media_dev.model));
	csi->media_dev.ops = &kunlun_media_ops;
	media_device_init(&csi->media_dev);

	csi->v4l2_dev.mdev = &csi->media_dev;
	ret = v4l2_device_register(csi->dev, &csi->v4l2_dev);
	if(ret < 0) {
		dev_err(dev, "Failed to register V4L2 device: %d\n", ret);
		return ret;
	}

	ret = kunlun_stream_register_device(csi);
	if(ret < 0)
		goto err_register_entities;

	csi->notifier.bound = kstream_subdev_notifier_bound;
	csi->notifier.complete = kstream_subdev_notifier_complete;
	ret = v4l2_async_notifier_register(&csi->v4l2_dev, &csi->notifier);
	if(ret < 0) {
		dev_err(dev, "Failed to register async subdev node: %d\n", ret);
		goto err_register_subdevs;
	}

	ret = devm_request_irq(dev, csi->irq, kunlun_csi_isr,
			0, dev_name(dev), csi);
	if(ret < 0) {
		dev_err(dev, "Request IRQ failed: %d\n", ret);
		goto err_register_subdevs;;
	}

	dev_info(dev, "CSI driver probed.\n");
	return 0;

err_register_subdevs:
	kunlun_stream_unregister_device(csi);
err_register_entities:
	v4l2_device_unregister(&csi->v4l2_dev);

	return ret;
}

static int kunlun_csi_remove(struct platform_device *pdev)
{
	struct csi_core *csi;

	csi = platform_get_drvdata(pdev);

	v4l2_async_notifier_unregister(&csi->notifier);

	kunlun_stream_unregister_device(csi);

	WARN_ON(atomic_read(&csi->ref_count));

	v4l2_device_unregister(&csi->v4l2_dev);

	media_device_unregister(&csi->media_dev);

	media_device_cleanup(&csi->media_dev);

	return 0;
}

static const struct of_device_id kunlun_csi_dt_match[] = {
	{ .compatible = "semdrive,kunlun-csi"},
	{  },
};

static struct platform_driver kunlun_csi_driver = {
	.probe = kunlun_csi_probe,
	.remove = kunlun_csi_remove,
	.driver = {
		.name = "kunlun-csi",
		.of_match_table = kunlun_csi_dt_match,
	},
};

module_platform_driver(kunlun_csi_driver);

MODULE_ALIAS("platform:kunlun-csi");
MODULE_DESCRIPTION("Semidrive Camera driver");
MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_LICENSE("GPL v2");
