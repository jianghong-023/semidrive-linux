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
#include <linux/of_address.h>
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
	2079000000,
	415800000,
	1039500000,
	2079000000,
	1039500000,
	415800000,
	2079000000,
	1039500000,
	415800000,
	2079000000,
	1039500000,
	415800000,
	2079000000,
	1039500000,
	415800000,
};

struct sdrv_cgu_pll_clk disp_pll_clk[DISP_CLKIN_MAX+1] = {
	SDRV_PLL_CLK(DISP, 0, 0, 0, 0),
	SDRV_PLL_CLK(DISP, 1, 0, 0, 0),
	SDRV_PLL_CLK(DISP, 2, 0, 0, 0),
	SDRV_PLL_CLK(DISP, 3, 0, 0, 0),
	SDRV_PLL_CLK_ROOT(DISP, 4, 0),
	SDRV_PLL_CLK_DIV(DISP, 5, 0, 4, PLL_DIVA),
	SDRV_PLL_CLK_DIV(DISP, 6, 0, 4, PLL_DIVB),
	SDRV_PLL_CLK_ROOT(DISP, 7, 0),
	SDRV_PLL_CLK_DIV(DISP, 8, 0, 7, PLL_DIVA),
	SDRV_PLL_CLK_DIV(DISP, 9, 0, 7, PLL_DIVB),
	SDRV_PLL_CLK_ROOT(DISP, 10, 0),
	SDRV_PLL_CLK_DIV(DISP, 11, 0, 10, PLL_DIVA),
	SDRV_PLL_CLK_DIV(DISP, 12, 0, 10, PLL_DIVB),
	SDRV_PLL_CLK_ROOT(DISP, 13, 0),
	SDRV_PLL_CLK_DIV(DISP, 14, 0, 13, PLL_DIVA),
	SDRV_PLL_CLK_DIV(DISP, 15, 0, 13, PLL_DIVB),
	SDRV_PLL_CLK_ROOT(DISP, 16, 0),
	SDRV_PLL_CLK_DIV(DISP, 17, 0, 16, PLL_DIVA),
	SDRV_PLL_CLK_DIV(DISP, 18, 0, 16, PLL_DIVB),
};

void sdrv_register_disp_plls(struct clk *clkbase[])
{
	int i = 0, size;
	void __iomem *reg_base = 0;

	/* PLL_DISP ROOT/DIVA/B */
	reg_base = sdrv_get_pll_regbase(disp_clk_src_names[DISP_CLKIN_4]);
	if (reg_base) {
		for (i = DISP_CLKIN_4; i <= DISP_CLKIN_6; i++)
			disp_pll_clk[i].reg_base = reg_base;
	}

	/* PLL_LVDS1 ROOT/DIVA/B */
	reg_base = sdrv_get_pll_regbase(disp_clk_src_names[DISP_CLKIN_7]);
	if (reg_base) {
		for (i = DISP_CLKIN_7; i <= DISP_CLKIN_9; i++)
			disp_pll_clk[i].reg_base = reg_base;
	}

	/* PLL_LVDS2 ROOT/DIVA/B */
	reg_base = sdrv_get_pll_regbase(disp_clk_src_names[DISP_CLKIN_10]);
	if (reg_base) {
		for (i = DISP_CLKIN_10; i <= DISP_CLKIN_12; i++)
			disp_pll_clk[i].reg_base = reg_base;
	}

	/* PLL_LVDS3 ROOT/DIVA/B */
	reg_base = sdrv_get_pll_regbase(disp_clk_src_names[DISP_CLKIN_13]);
	if (reg_base) {
		for (i = DISP_CLKIN_13; i <= DISP_CLKIN_15; i++)
			disp_pll_clk[i].reg_base = reg_base;
	}

	/* PLL_LVDS4 ROOT/DIVA/B */
	reg_base = sdrv_get_pll_regbase(disp_clk_src_names[DISP_CLKIN_16]);
	if (reg_base) {
		for (i = DISP_CLKIN_16; i <= DISP_CLKIN_18; i++)
			disp_pll_clk[i].reg_base = reg_base;
	}

	size = ARRAY_SIZE(disp_pll_clk);
	for (i = 0; i < size; i++)
		clkbase[disp_pll_clk[i].clk_id] = sdrv_register_pll(0, i, disp_pll_clk, size, NULL, disp_clk_src_names, disp_clkin_freq);

}

