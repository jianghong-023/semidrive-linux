/*
 * Copyright (C) 2015 Heiko Schocher <hs@denx.de>
 *
 * from:
 * drivers/gpu/drm/panel/panel-ld9040.c
 * ld9040 AMOLED LCD drm_panel driver.
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 * Derived from drivers/video/backlight/ld9040.c
 *
 * Andrzej Hajda <a.hajda@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <drm/drmP.h>
#include <drm/drm_panel.h>

#include <video/of_videomode.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>

struct kpanel {
	struct device *dev;
	struct drm_panel panel;
	struct videomode vm;
};

static inline struct kpanel *panel_to_kpanel(struct drm_panel *panel)
{
	return container_of(panel, struct kpanel, panel);
}

static int kpanel_display_on(struct kpanel *ctx)
{
	return 0;
}

static int kpanel_display_off(struct kpanel *ctx)
{
	return 0;
}

static int kpanel_power_on(struct kpanel *ctx)
{
	return kpanel_display_on(ctx);
}

static int kpanel_disable(struct drm_panel *panel)
{
	struct kpanel *ctx = panel_to_kpanel(panel);

	return kpanel_display_off(ctx);
}

static int kpanel_enable(struct drm_panel *panel)
{
	struct kpanel *ctx = panel_to_kpanel(panel);

	return kpanel_power_on(ctx);
}

static const struct drm_display_mode default_mode = {
	.clock = 74250,
	.hdisplay = 1280,
	.hsync_start = 1280 + 70,
	.hsync_end = 1280 + 70 + 40,
	.htotal = 1280 + 70 + 40 + 260,
	.vdisplay = 720,
	.vsync_start = 720 + 20,
	.vsync_end = 720 + 20 + 5,
	.vtotal = 720 + 20 + 5 + 5,
	.vrefresh = 60,
};

static int kpanel_get_modes(struct drm_panel *panel)
{
	struct drm_connector *connector = panel->connector;
	struct kpanel *ctx = panel_to_kpanel(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_create(connector->dev);
	if (!mode) {
		dev_err(ctx->dev, "failed to create a new display mode\n");
		return -ENOMEM;
	}

	drm_display_mode_from_videomode(&ctx->vm, mode);
	drm_bus_flags_from_videomode(&ctx->vm, &connector->display_info.bus_flags);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = 61;
	connector->display_info.height_mm = 103;

	return 1;
}

static const struct drm_panel_funcs kpanel_drm_funcs = {
	.disable = kpanel_disable,
	.enable = kpanel_enable,
	.get_modes = kpanel_get_modes,
};

static int kpanel_probe(struct platform_device *pdev)
{
	struct kpanel *ctx;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int ret;

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ret = of_get_videomode(np, &ctx->vm, OF_USE_NATIVE_MODE);
	if(ret < 0)
		return ret;

	drm_panel_init(&ctx->panel);
	ctx->dev = dev;
	ctx->panel.dev = dev;
	ctx->panel.funcs = &kpanel_drm_funcs;

	platform_set_drvdata(pdev, ctx);
	return drm_panel_add(&ctx->panel);
}

static int kpanel_remove(struct platform_device *pdev)
{
	struct kpanel *ctx = platform_get_drvdata(pdev);

	kpanel_display_off(ctx);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id kpanel_of_match[] = {
	{ .compatible = "semidrive,panel-simple" },
	{ }
};
MODULE_DEVICE_TABLE(of, kpanel_of_match);

static struct platform_driver panel_lvds_driver = {
	.probe		= kpanel_probe,
	.remove		= kpanel_remove,
	.driver		= {
		.name	= "kunlun-panel-simple",
		.of_match_table = kpanel_of_match,
	},
};

module_platform_driver(panel_lvds_driver);

MODULE_AUTHOR("Heiko Schocher <hs@denx.de>");
MODULE_DESCRIPTION("kpanel LCD Driver");
MODULE_LICENSE("GPL v2");
