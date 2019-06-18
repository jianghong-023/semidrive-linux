/*
 * kunlun_drm_drv.c
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_of.h>
#include <linux/pm_runtime.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/component.h>
#include <linux/console.h>
#include <linux/iommu.h>
#include <linux/of_platform.h>

#include "kunlun_drm_drv.h"
#include "kunlun_drm_fbdev.h"
#include "kunlun_drm_gem.h"



#define KUNLUN_DRM_DRIVER_NAME		"semidrive"
#define KUNLUN_DRM_DRIVER_DESC		"semidrive Soc DRM"
#define KUNLUN_DRM_DRIVER_DATE		"20190424"
#define KUNLUN_DRM_DRIVER_MAJOR		1
#define KUNLUN_DRM_DRIVER_MINOR		0

extern const struct drm_framebuffer_funcs kunlun_drm_fb_funcs;

static struct drm_framebuffer *
kunlun_gem_fb_create(struct drm_device *drm,
		struct drm_file *file_priv, const struct drm_mode_fb_cmd2 *cmd)
{
	return drm_gem_fb_create_with_funcs(drm, file_priv, cmd,
			&kunlun_drm_fb_funcs);
}

static void kunlun_drm_output_poll_changed(struct drm_device *drm)
{
	struct kunlun_drm_data *kdrm = drm->dev_private;

	drm_fb_helper_hotplug_event(kdrm->fb_helper);
}

static const struct drm_mode_config_funcs kunlun_mode_config_funcs = {
	.fb_create = kunlun_gem_fb_create,
	.output_poll_changed = kunlun_drm_output_poll_changed,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static void kunlun_drm_lastclose(struct drm_device *drm)
{
	struct kunlun_drm_data *kdrm = drm->dev_private;
	if(kdrm->fb_helper)
		drm_fb_helper_restore_fbdev_mode_unlocked(kdrm->fb_helper);
}

static const struct file_operations kunlun_drm_driver_fops = {
	.owner				= THIS_MODULE,
	.open				= drm_open,
	.mmap				= kunlun_drm_mmap,
	.poll				= drm_poll,
	.read				= drm_read,
	.unlocked_ioctl		= drm_ioctl,
	.compat_ioctl		= drm_compat_ioctl,
	.release			= drm_release,
};

static struct drm_driver kunlun_drm_driver = {
	.driver_features			= DRIVER_GEM | DRIVER_PRIME |
			DRIVER_RENDER | DRIVER_ATOMIC | DRIVER_MODESET,
	.lastclose					= kunlun_drm_lastclose,
	.gem_vm_ops					= &drm_gem_cma_vm_ops,
	.gem_free_object_unlocked	= kunlun_gem_free_object,
	.dumb_create				= kunlun_gem_dumb_create,
	.prime_handle_to_fd			= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle			= drm_gem_prime_fd_to_handle,
	.gem_prime_import			= drm_gem_prime_import,
	.gem_prime_export			= drm_gem_prime_export,
	.gem_prime_get_sg_table		= kunlun_gem_prime_get_sg_table,
	.gem_prime_import_sg_table	= kunlun_gem_prime_import_sg_table,
	.gem_prime_vmap				= kunlun_gem_prime_vmap,
	.gem_prime_vunmap			= kunlun_gem_prime_vunmap,
	.gem_prime_mmap				= kunlun_gem_prime_mmap,
	.fops 						= &kunlun_drm_driver_fops,
	.name						= KUNLUN_DRM_DRIVER_NAME,
	.desc						= KUNLUN_DRM_DRIVER_DESC,
	.date						= KUNLUN_DRM_DRIVER_DATE,
	.major						= KUNLUN_DRM_DRIVER_MAJOR,
	.minor						= KUNLUN_DRM_DRIVER_MINOR,
};

static int compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static int kunlun_drm_init_iommu(struct drm_device *drm)
{
	struct kunlun_drm_data *kdrm = drm->dev_private;
	struct device *dev = drm->dev;
	struct device_node *iommu;
	struct iommu_domain_geometry *geometry;
	u64 start, end;
	int ret = 0;

	kdrm->iommu_enable = false;
	iommu = of_parse_phandle(dev->of_node, "iommus", 0);
	if(!iommu) {
		DRM_DEV_DEBUG(dev, "iommu disabled\n");
		return ret;
	}

	kdrm->mm = kzalloc(sizeof(*kdrm->mm), GFP_KERNEL);
	if(!kdrm->mm) {
		ret = -ENOMEM;
		goto err_domain;
	}

	kdrm->domain = iommu_domain_alloc(&platform_bus_type);
	if(!kdrm->domain) {
		ret = -ENOMEM;
		goto err_free_mm;;
	}

	geometry = &kdrm->domain->geometry;
	start = geometry->aperture_start;
	end = geometry->aperture_end;

	DRM_DEV_DEBUG(dev, "IOMMU context initialized: %#llx - %#llx\n",
			start, end);
	drm_mm_init(kdrm->mm, start, end - start + 1);
	mutex_init(&kdrm->mm_lock);

	kdrm->iommu_enable = true;

	of_node_put(iommu);
	return ret;

err_free_mm:
	kfree(kdrm->mm);
err_domain:
	of_node_put(iommu);
	return ret;
}

static void kunlun_drm_iommu_cleanup(struct drm_device *drm)
{
	struct kunlun_drm_data *kdrm = drm->dev_private;

	if(!kdrm->iommu_enable)
		return;

	drm_mm_takedown(kdrm->mm);
	iommu_domain_free(kdrm->domain);
	kfree(kdrm->mm);
}

static int kunlun_drm_bind(struct device *dev)
{
	struct drm_device *drm;
	struct kunlun_drm_data *kdrm;
	int ret;

	drm = drm_dev_alloc(&kunlun_drm_driver, dev);
	if(IS_ERR(drm)) {
		dev_err(dev, "failed to allocate drm_device\n");
		return PTR_ERR(drm);
	}

	kdrm = devm_kzalloc(dev, sizeof(*kdrm), GFP_KERNEL);
	if(!kdrm) {
		ret = -ENOMEM;
		goto err_unref;
	}

	drm->dev_private = kdrm;
	kdrm->drm = drm;

	drm->irq_enabled = true;

	drm_mode_config_init(drm);

	drm->mode_config.min_width = 0;
	drm->mode_config.min_height = 0;
	drm->mode_config.max_width = 4096;
	drm->mode_config.max_height = 4096;

	drm->mode_config.funcs = &kunlun_mode_config_funcs;

	ret = kunlun_drm_init_iommu(drm);
	if(ret)
		goto err_config_cleanup;

	ret = component_bind_all(dev, drm);
	if(ret) {
		dev_err(dev, "failed to bind component\n");
		goto err_iommu_cleanup;
	}

	dev_set_drvdata(dev, drm);

	if(!kdrm->num_crtcs) {
		dev_err(dev, "error crtc numbers\n");
		goto err_unbind;
	}

	ret = drm_vblank_init(drm, kdrm->num_crtcs);
	if(ret) {
		goto err_unbind;
	}

	drm_mode_config_reset(drm);

	ret = kunlun_drm_fbdev_init(drm);
	if(ret) {
		goto err_unbind;
	}

	drm_kms_helper_poll_init(drm);

	ret = drm_dev_register(drm, 0);
	if(ret)
		goto err_helper;

	return ret;

err_helper:
	drm_kms_helper_poll_fini(drm);
	kunlun_drm_fbdev_fini(drm);
err_unbind:
	dev_set_drvdata(dev, NULL);
	component_unbind_all(dev, drm);
err_iommu_cleanup:
	kunlun_drm_iommu_cleanup(drm);
err_config_cleanup:
	drm_mode_config_cleanup(drm);
err_unref:
	drm_dev_unref(drm);

	return ret;
}

static void kunlun_drm_unbind(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	drm_dev_unregister(drm);

	drm_kms_helper_poll_fini(drm);

	kunlun_drm_fbdev_fini(drm);

	dev_set_drvdata(dev, NULL);

	component_unbind_all(dev, drm);

	kunlun_drm_iommu_cleanup(drm);

	drm_mode_config_cleanup(drm);

	drm_dev_unref(drm);
}

static const struct component_master_ops kunlun_drm_ops = {
	.bind = kunlun_drm_bind,
	.unbind = kunlun_drm_unbind,
};

static int add_components_dsd(struct device *master, struct device_node *np,
		struct component_match **matchptr)
{
	struct device_node *ep_node, *intf;

	for_each_endpoint_of_node(np, ep_node) {
		intf = of_graph_get_remote_port_parent(ep_node);
		if(!intf)
			continue;
		
		drm_of_component_match_add(master, matchptr, compare_of, intf);
		DRM_DEV_DEBUG(master, "Add component %s", intf->name);
		of_node_put(intf);
	}

	return 0;
}

static int add_display_components(struct device *dev,
		struct component_match **matchptr)
{
	struct device_node *np;
	const struct kunlun_drm_device_info *kdd_info;
	int ret;

	kdd_info = of_device_get_match_data(dev);
	if(!kdd_info || !kdd_info->match)
		return -ENODEV;

	ret = of_platform_populate(dev->of_node, NULL, NULL, dev);
	if(ret) {
		DRM_DEV_ERROR(dev, "failed to populate dp/dc devices\n");
		return ret;
	}

	for_each_child_of_node(dev->of_node, np) {
		if(!kdd_info->match(np)) {
			DRM_DEV_INFO(dev, "OF node %s not match\n", np->name);
			continue;
		}

		drm_of_component_match_add(dev, matchptr, compare_of, np);
		DRM_DEV_INFO(dev, "Add component %s", np->name);

		ret = add_components_dsd(dev, np, matchptr);
		if(ret) {
			goto err_depopulate;
		}
	}

	return ret;

err_depopulate:
	of_platform_depopulate(dev);
	return ret;
}

static int kunlun_drm_probe(struct platform_device *pdev)
{
	struct component_match *match = NULL;
	int ret;

	ret = add_display_components(&pdev->dev, &match);
	if(ret)
		return ret;

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(40));
	if(ret)
		return ret;

	if(!match)
		return -EINVAL;

	return component_master_add_with_match(&pdev->dev,
			&kunlun_drm_ops, match);
}

static int kunlun_drm_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &kunlun_drm_ops);
	of_platform_depopulate(&pdev->dev);

	return 0;
}

static int compare_name_dp(struct device_node *np)
{
	return strstr(np->name, "dp") != NULL;
}

static int compare_name_dc(struct device_node *np)
{
	return strstr(np->name, "dc") != NULL;
}

static const struct kunlun_drm_device_info kunlun_dps_info = {
	.type = KUNLUN_DRM_DP,
	.match = compare_name_dp,
};

static const struct kunlun_drm_device_info kunlun_dcs_info = {
	.type = KUNLUN_DRM_DC,
	.match = compare_name_dc,
};

static const struct of_device_id kunlun_drm_of_table[] = {
	{.compatible = "semdrive,dp-subsystem", .data = &kunlun_dps_info },
	{.compatible = "semdrive,dc-subsystem", .data = &kunlun_dcs_info },
	{},
};
MODULE_DEVICE_TABLE(of, kunlun_drm_of_table);

static struct platform_driver kunlun_drm_platform_driver = {
	.probe = kunlun_drm_probe,
	.remove = kunlun_drm_remove,
	.driver = {
		.name = "kunlun-drm",
		.of_match_table = kunlun_drm_of_table,
	},
};
module_platform_driver(kunlun_drm_platform_driver);

MODULE_AUTHOR("Semidrive Semiconductor");
MODULE_DESCRIPTION("Semidrive Kunlun DRM driver");
MODULE_LICENSE("GPL");

