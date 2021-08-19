/*
 * Copyright (C) 2020 Semidrive Semiconductor, Inc.
 *
 * derived from the imx-rpmsg implementation.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/rpmsg.h>

struct rpmsg_device *
rpmsg_ipcc_request_channel(struct platform_device *client, int index, const char *dev_name);

static int dcf_virnet_probe(struct platform_device *pdev)
{
	struct rpmsg_device *newch;
	const char *dev_name;
	int ret = 0;

	ret = device_property_read_string(&pdev->dev, "name", &dev_name);
	if (ret < 0)
		return ret;

	newch = rpmsg_ipcc_request_channel(pdev, 0, dev_name);

	return 0;
}

static int dcf_virnet_remove(struct platform_device *pdev)
{
	/* Not supported */
	return 0;
}

static const struct of_device_id dcf_virnet_of_ids[] = {
	{ .compatible = "sd,rpmsg-netdev"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, dcf_virnet_of_ids);

static struct platform_driver dcf_virnet_driver = {
	.probe = dcf_virnet_probe,
	.remove = dcf_virnet_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "dcf-virnet",
		.of_match_table = dcf_virnet_of_ids,
	},
};

static int __init dcf_virnet_init(void)
{
	int ret;

	ret = platform_driver_register(&dcf_virnet_driver);
	if (ret)
		pr_err("Unable to initialize virtual netdev\n");
	else
		pr_info("Semidrive virtual netdev is registered.\n");

	return ret;
}
module_init(dcf_virnet_init);

static void __exit dcf_virnet_exit(void)
{
	platform_driver_unregister(&dcf_virnet_driver);
}
module_exit(dcf_virnet_exit);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive RPMSG-based Virtual Net Device");
MODULE_LICENSE("GPL v2");

