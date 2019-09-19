/*
 * clk-soc-pll.c
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
#include "clk-soc-pll.h"
#include "clk-pll.h"
struct sdrv_pll_rate_table pll_cpu2_rate_table[] = {
	{
		.refdiv = 3,
		.fbdiv = 187,
		.postdiv = {1, 1},
		.frac = 0,
		.div = {2, 4, 0, 0},
		.isint = true,
	},
};

static u64 soc_clkin_freq[SOC_CLKIN_MAX+1] = {
	24000000,
	2000000,
	24000000,
	32000,
	1656000000,
	828000000,
	414000000,
	1328000000,
	664000000,
	332000000,
	1496000000,
	748000000,
	374000000,
	800000000,
	400000000,
	266666667,
	728000000,
	364000000,
	242666667,
	2664000000,
	666000000,
	532800000,
	748000000,
	374000000,
	249333333,
	1896000000,
	474000000,
	316000000,
	1000000000,
	500000000,
	333333333,
	500000000,
	399999999,
	1064000000,
	532000000,
	354666667,
};

struct sdrv_cgu_pll_clk soc_pll_clk[] = {
	SDRV_PLL_CLK(SOC, 0, 0, 0, 0),
	SDRV_PLL_CLK(SOC, 1, 0, 0, 0),
	SDRV_PLL_CLK(SOC, 2, 0, 0, 0),
	SDRV_PLL_CLK(SOC, 3, 0, 0, 0),
	SDRV_PLL_CLK_ROOT(SOC, 4, 0),
	SDRV_PLL_CLK_DIV(SOC, 5, 0, 4, PLL_DIVA),
	SDRV_PLL_CLK_DIV(SOC, 6, 0, 4, PLL_DIVB),
	SDRV_PLL_CLK_ROOT(SOC, 7, 0),
	SDRV_PLL_CLK_DIV(SOC, 8, 0, 7, PLL_DIVA),
	SDRV_PLL_CLK_DIV(SOC, 9, 0, 7, PLL_DIVB),
	//SDRV_PLL_CLK_RATE_TABLE(SOC, 10, 0, pll_cpu2_rate_table),
	SDRV_PLL_CLK_ROOT(SOC, 10, 0),
	SDRV_PLL_CLK_DIV(SOC, 11, 0, 10, PLL_DIVA),
	SDRV_PLL_CLK_DIV(SOC, 12, 0, 10, PLL_DIVB),
	SDRV_PLL_CLK_ROOT(SOC, 13, 0),
	SDRV_PLL_CLK_DIV(SOC, 14, 0, 13, PLL_DIVA),
	SDRV_PLL_CLK_DIV(SOC, 15, 0, 13, PLL_DIVB),
	SDRV_PLL_CLK_ROOT(SOC, 16, 0),
	SDRV_PLL_CLK_DIV(SOC, 17, 0, 16, PLL_DIVA),
	SDRV_PLL_CLK_DIV(SOC, 18, 0, 16, PLL_DIVB),
	SDRV_PLL_CLK_ROOT(SOC, 19, 0),
	SDRV_PLL_CLK_DIV(SOC, 20, 0, 19, PLL_DIVA),
	SDRV_PLL_CLK_DIV(SOC, 21, 0, 19, PLL_DIVB),
	SDRV_PLL_CLK_ROOT(SOC, 22, 0),
	SDRV_PLL_CLK_DIV(SOC, 23, 0, 22, PLL_DIVA),
	SDRV_PLL_CLK_DIV(SOC, 24, 0, 22, PLL_DIVB),
	SDRV_PLL_CLK_ROOT(SOC, 25, 0),
	SDRV_PLL_CLK_DIV(SOC, 26, 0, 25, PLL_DIVA),
	SDRV_PLL_CLK_DIV(SOC, 27, 0, 25, PLL_DIVB),
	SDRV_PLL_CLK_ROOT(SOC, 28, 0),
	SDRV_PLL_CLK_DIV(SOC, 29, 0, 28, PLL_DIVA),
	SDRV_PLL_CLK_DIV(SOC, 30, 0, 28, PLL_DIVB),
	SDRV_PLL_CLK_DIV_NO_PARENT(SOC, 31, 0, PLL_DIVA),
	SDRV_PLL_CLK_DIV_NO_PARENT(SOC, 32, 0, PLL_DIVA),
	SDRV_PLL_CLK_ROOT(SOC, 33, 0),
	SDRV_PLL_CLK_DIV(SOC, 34, 0, 33, PLL_DIVA),
	SDRV_PLL_CLK_DIV(SOC, 35, 0, 33, PLL_DIVA),
};

void sdrv_register_soc_plls(struct clk *clkbase[])
{
	int i = 0, size;
	void __iomem *reg_base = 0;

	/* PLL_CPU1A ROOT/DIVA/B */
	reg_base = sdrv_get_pll_regbase(soc_clk_src_names[SOC_CLKIN_4]);
	if (reg_base) {
		for (i = SOC_CLKIN_4; i <= SOC_CLKIN_6; i++)
			soc_pll_clk[i].reg_base = reg_base;
	}

	/* PLL_CPU1B ROOT/DIVA/B */
	reg_base = sdrv_get_pll_regbase(soc_clk_src_names[SOC_CLKIN_7]);
	if (reg_base) {
		for (i = SOC_CLKIN_7; i <= SOC_CLKIN_9; i++)
			soc_pll_clk[i].reg_base = reg_base;
	}

	/* PLL_CPU2 ROOT/DIVA/B */
	reg_base = sdrv_get_pll_regbase(soc_clk_src_names[SOC_CLKIN_10]);
	if (reg_base) {
		for (i = SOC_CLKIN_10; i <= SOC_CLKIN_12; i++)
			soc_pll_clk[i].reg_base = reg_base;
	}

	/* PLL_GPU1 ROOT/DIVA/B */
	reg_base = sdrv_get_pll_regbase(soc_clk_src_names[SOC_CLKIN_13]);
	if (reg_base) {
		for (i = SOC_CLKIN_13; i <= SOC_CLKIN_15; i++)
			soc_pll_clk[i].reg_base = reg_base;
	}

	/* PLL_GPU2 ROOT/DIVA/B */
	reg_base = sdrv_get_pll_regbase(soc_clk_src_names[SOC_CLKIN_16]);
	if (reg_base) {
		for (i = SOC_CLKIN_16; i <= SOC_CLKIN_18; i++)
			soc_pll_clk[i].reg_base = reg_base;
	}

	/* PLL_VPU ROOT/DIVA/B */
	sdrv_get_pll_regbase(soc_clk_src_names[SOC_CLKIN_19]);
	if (reg_base) {
		for (i = SOC_CLKIN_19; i <= SOC_CLKIN_21; i++)
			soc_pll_clk[i].reg_base = reg_base;
	}

	/* PLL_VSN ROOT/DIVA/B */
	reg_base = sdrv_get_pll_regbase(soc_clk_src_names[SOC_CLKIN_22]);
	if (reg_base) {
		for (i = SOC_CLKIN_22; i <= SOC_CLKIN_24; i++)
			soc_pll_clk[i].reg_base = reg_base;
	}

	/* PLL_HPI ROOT/DIVA/B */
	reg_base = sdrv_get_pll_regbase(soc_clk_src_names[SOC_CLKIN_25]);
	if (reg_base) {
		for (i = SOC_CLKIN_25; i <= SOC_CLKIN_27; i++)
			soc_pll_clk[i].reg_base = reg_base;
	}

	/* PLL_HIS ROOT/DIVA/B */
	reg_base = sdrv_get_pll_regbase(soc_clk_src_names[SOC_CLKIN_28]);
	if (reg_base) {
		for (i = SOC_CLKIN_28; i <= SOC_CLKIN_30; i++)
			soc_pll_clk[i].reg_base = reg_base;
	}

	/* PLL1 DIVA */
	reg_base = sdrv_get_pll_regbase(soc_clk_src_names[SOC_CLKIN_31]);
	if (reg_base)
		soc_pll_clk[SOC_CLKIN_31].reg_base = reg_base;

	/* PLL2 DIVA */
	reg_base = sdrv_get_pll_regbase(soc_clk_src_names[SOC_CLKIN_32]);
	if (reg_base)
		soc_pll_clk[SOC_CLKIN_32].reg_base = reg_base;

	/* PLL_DDR ROOT/DIVA/B */
	reg_base = sdrv_get_pll_regbase(soc_clk_src_names[SOC_CLKIN_33]);
	if (reg_base) {
		for (i = SOC_CLKIN_33; i <= SOC_CLKIN_35; i++)
			soc_pll_clk[i].reg_base = reg_base;
	}

	size = ARRAY_SIZE(soc_pll_clk);
	for (i = 0; i < size; i++)
		clkbase[soc_pll_clk[i].clk_id] = sdrv_register_pll(0, i, soc_pll_clk, size, NULL, soc_clk_src_names,
			soc_clkin_freq);

}

