/*
 * kunlun-mipi-csi2.h
 *
 * Semidrive kunlun platform mipi csi2 header file
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef SD_KUNLUN_MIPI_CSI2_H
#define SD_KUNLUN_MIPI_CSI2_H

#define KUNLUN_CSI2_PAD_SINK	0
#define KUNLUN_CSI2_PAD_SRC		1
#define KUNLUN_CSI2_PAD_NUM		2

struct mipi_csi2 {
	u8 id;
	void __iomem *base;
	struct v4l2_subdev subdev;
	struct media_pad pads[KUN_CSI2_PAD_NUM];
};

#endif
