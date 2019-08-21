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
#include "clk.h"
#include "clk-soc-pll.h"
#include "clk-pll.h"

static u64 soc_clkin_freq[SOC_CLKIN_MAX+1] = {
	24000000,
	2000000,
	24000000,
	32000,
	1800000000,
	900000000,
	450000000,
	1504000000,
	752000000,
	376000000,
	1600000000,
	800000000,
	400000000,
	800000000,
	400000000,
	266666667,
	800000000,
	400000000,
	266666667,
	2664000000,
	666000000,
	532800000,
	800000000,
	400000000,
	266666667,
	1064000000,
	532000000,
	354666667,
	1000000000,
	500000000,
	333333333,
	1000000000,
	799999999,
};

#define SOC_PLL_CLK(id, base, div_shift, div_width) \
{	\
	.clk_id = SOC_CLKIN_ ##id,	\
	.reg_base = #base,	\
}

struct sdrv_cgu_pll_clk soc_pll_clk[] = {
	SOC_PLL_CLK(0, 0, 0, 0),
	SOC_PLL_CLK(1, 0, 0, 0),
	SOC_PLL_CLK(2, 0, 0, 0),
	SOC_PLL_CLK(3, 0, 0, 0),
	SOC_PLL_CLK(4, 0, 0, 0),
	SOC_PLL_CLK(5, 0, 0, 0),
	SOC_PLL_CLK(6, 0, 0, 0),
	SOC_PLL_CLK(7, 0, 0, 0),
	SOC_PLL_CLK(8, 0, 0, 0),
	SOC_PLL_CLK(9, 0, 0, 0),
	SOC_PLL_CLK(10, 0, 0, 0),
	SOC_PLL_CLK(11, 0, 0, 0),
	SOC_PLL_CLK(12, 0, 0, 0),
	SOC_PLL_CLK(13, 0, 0, 0),
	SOC_PLL_CLK(14, 0, 0, 0),
	SOC_PLL_CLK(15, 0, 0, 0),
	SOC_PLL_CLK(16, 0, 0, 0),
	SOC_PLL_CLK(17, 0, 0, 0),
	SOC_PLL_CLK(18, 0, 0, 0),
	SOC_PLL_CLK(19, 0, 0, 0),
	SOC_PLL_CLK(20, 0, 0, 0),
	SOC_PLL_CLK(21, 0, 0, 0),
	SOC_PLL_CLK(22, 0, 0, 0),
	SOC_PLL_CLK(23, 0, 0, 0),
	SOC_PLL_CLK(24, 0, 0, 0),
	SOC_PLL_CLK(25, 0, 0, 0),
	SOC_PLL_CLK(26, 0, 0, 0),
	SOC_PLL_CLK(27, 0, 0, 0),
	SOC_PLL_CLK(28, 0, 0, 0),
	SOC_PLL_CLK(29, 0, 0, 0),
	SOC_PLL_CLK(30, 0, 0, 0),
	SOC_PLL_CLK(31, 0, 0, 0),
	SOC_PLL_CLK(32, 0, 0, 0),
};


void sdrv_register_soc_plls(void)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(soc_pll_clk); i++)
		sdrv_register_pll(0, &soc_pll_clk[i], NULL, soc_clk_src_names,
			soc_clkin_freq);

}

