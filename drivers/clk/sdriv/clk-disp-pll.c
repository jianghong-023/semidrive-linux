/*
 * clk-disp-pll.c
 *
 *
 * Copyright(c); 2018 Semidrive
 *
 * Author: Alex Chen <qing.chen@semidrive.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/reboot.h>
#include <linux/rational.h>
#include "clk.h"
#include "clk-pll.h"
#include "clk-disp-pll.h"

/**
 * Register a clock branch.
 * Most clock branches have a form like
 *
 * src1 --|--\
 *        |M |--[GATE]-[DIV]-
 * src2 --|--/
 *
 * sometimes without one of those components.
 */
static u64 disp_clkin_freq[DISP_CLKIN_MAX+1] = {
	24000000,
	2000000,
	24000000,
	32000,
	2000000000,
	400000000,
	1000000000,
	2079000000,
	693000000,
	415800000,
	2079000000,
	693000000,
	415800000,
	2079000000,
	693000000,
	415800000,
	2079000000,
	693000000,
	415800000,
};

#define SDRV_PLL_CLK(id, base, div_shift, div_width) \
{	\
	.clk_id = DISP_CLKIN_ ##id,	\
	.reg_base = #base,	\
}

struct sdrv_cgu_pll_clk disp_pll_clk[DISP_CLKIN_MAX+1] = {
	SDRV_PLL_CLK(0, 0, 0, 0),
	SDRV_PLL_CLK(1, 0, 0, 0),
	SDRV_PLL_CLK(2, 0, 0, 0),
	SDRV_PLL_CLK(3, 0, 0, 0),
	SDRV_PLL_CLK(4, 0, 0, 0),
	SDRV_PLL_CLK(5, 0, 0, 0),
	SDRV_PLL_CLK(6, 0, 0, 0),
	SDRV_PLL_CLK(7, 0, 0, 0),
	SDRV_PLL_CLK(8, 0, 0, 0),
	SDRV_PLL_CLK(9, 0, 0, 0),
	SDRV_PLL_CLK(10, 0, 0, 0),
	SDRV_PLL_CLK(11, 0, 0, 0),
	SDRV_PLL_CLK(12, 0, 0, 0),
	SDRV_PLL_CLK(13, 0, 0, 0),
	SDRV_PLL_CLK(14, 0, 0, 0),
	SDRV_PLL_CLK(15, 0, 0, 0),
	SDRV_PLL_CLK(16, 0, 0, 0),
	SDRV_PLL_CLK(17, 0, 0, 0),
	SDRV_PLL_CLK(18, 0, 0, 0),
};

void sdrv_register_disp_plls(void)
{
	int i = 0;

	for (i = DISP_CLKIN_0; i <= DISP_CLKIN_MAX; i++)
		sdrv_register_pll(0, &disp_pll_clk[i], NULL, disp_clk_src_names, disp_clkin_freq);

}

