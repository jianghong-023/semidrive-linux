/*
 * clk-sec-pll.c
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
#include "clk-sec-pll.h"

/* rate = pllcfg / plldiv
 * pllcfg = 24000000 * (fbdiv+isint?0:(frac/2^24))/refdiv/postdiv
 */
/*if have more rate table item, please make sure the items are increment*/
struct sdrv_pll_rate_table pll3_rate_table[] = {
	{
		.refdiv = 2,
		.fbdiv = 150,
		.postdiv = {2, 1},
		.frac = 0,
		.div = {3, 12, 15, 5},
		.isint = true,
	},
};

struct sdrv_pll_rate_table pll4_rate_table[] = {
	{
		.refdiv = 3,
		.fbdiv = 250,
		.postdiv = {2, 1},
		.frac = 0,
		.div = {4, 6, 8, 10},
		.isint = true,
	},
};

struct sdrv_pll_rate_table pll5_rate_table[] = {
	{
		.refdiv = 3,
		.fbdiv = 199,
		.postdiv = {2, 1},
		.frac = 0,
		.div = {4, 6, 8, 10},
		.isint = true,
	},
};

struct sdrv_pll_rate_table pll6_rate_table[] = {
	{
		.refdiv = 2,
		.fbdiv = 122,
		.postdiv = {5, 1},
		.frac = 14763950,
		.div = {3, 4, 5, 6},
		.isint = false,
	},
};

struct sdrv_pll_rate_table pll7_rate_table[] = {
	{
		.refdiv = 2,
		.fbdiv = 90,
		.postdiv = {4, 1},
		.frac = 5315022,
		.div = {3, 4, 5, 6},
		.isint = false,
	},
};

static u64 sec_clkin_freq[SEC_CLKIN_MAX+1] = {
	24000000,
	2000000,
	24000000,
	32000,
	900000000,
	600000000,
	150000000,
	120000000,
	360000000,
	1000000000,
	500000000,
	333333333,
	250000000,
	200000000,
	796000000,
	398000000,
	265333333,
	199000000,
	159200000,
	294912000,
	491520000,
	368640000,
	294912000,
	245760000,
	270950400,
	361267200,
	270950400,
	216760320,
	180633600,
	207900000,
	207900000,
	207900000,
	207900000,
};

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
struct sdrv_cgu_pll_clk sec_pll_clk[SEC_CLKIN_MAX+1] = {
	SDRV_PLL_CLK(SEC, 0, 0, 0, 0),
	SDRV_PLL_CLK(SEC, 1, 0, 0, 0),
	SDRV_PLL_CLK(SEC, 2, 0, 0, 0),
	SDRV_PLL_CLK(SEC, 3, 0, 0, 0),
	SDRV_PLL_CLK_RATE_TABLE(SEC, 4, 0, pll3_rate_table),
	//SDRV_PLL_CLK_ROOT(SEC, 4, 0),
	SDRV_PLL_CLK_DIV(SEC, 5, 0, 4, PLL_DIVA),
	SDRV_PLL_CLK_DIV(SEC, 6, 0, 4, PLL_DIVB),
	SDRV_PLL_CLK_DIV(SEC, 7, 0, 4, PLL_DIVC),
	SDRV_PLL_CLK_DIV(SEC, 8, 0, 4, PLL_DIVD),
	SDRV_PLL_CLK_RATE_TABLE(SEC, 9, 0, pll4_rate_table),
	//SDRV_PLL_CLK_ROOT(SEC, 9, 0),
	SDRV_PLL_CLK_DIV(SEC, 10, 0, 9, PLL_DIVA),
	SDRV_PLL_CLK_DIV(SEC, 11, 0, 9, PLL_DIVB),
	SDRV_PLL_CLK_DIV(SEC, 12, 0, 9, PLL_DIVC),
	SDRV_PLL_CLK_DIV(SEC, 13, 0, 9, PLL_DIVD),
	SDRV_PLL_CLK_RATE_TABLE(SEC, 14, 0, pll5_rate_table),
	//SDRV_PLL_CLK_ROOT(SEC, 14, 0),
	SDRV_PLL_CLK_DIV(SEC, 15, 0, 14, PLL_DIVA),
	SDRV_PLL_CLK_DIV(SEC, 16, 0, 14, PLL_DIVB),
	SDRV_PLL_CLK_DIV(SEC, 17, 0, 14, PLL_DIVC),
	SDRV_PLL_CLK_DIV(SEC, 18, 0, 14, PLL_DIVD),
	SDRV_PLL_CLK_RATE_TABLE(SEC, 19, 0, pll6_rate_table),
	SDRV_PLL_CLK_DIV(SEC, 20, 0, 19, PLL_DIVA),
	SDRV_PLL_CLK_DIV(SEC, 21, 0, 19, PLL_DIVB),
	SDRV_PLL_CLK_DIV(SEC, 22, 0, 19, PLL_DIVC),
	SDRV_PLL_CLK_DIV(SEC, 23, 0, 19, PLL_DIVD),
	SDRV_PLL_CLK_RATE_TABLE(SEC, 24, 0, pll7_rate_table),
	SDRV_PLL_CLK_DIV(SEC, 25, 0, 24, PLL_DIVA),
	SDRV_PLL_CLK_DIV(SEC, 26, 0, 24, PLL_DIVB),
	SDRV_PLL_CLK_DIV(SEC, 27, 0, 24, PLL_DIVC),
	SDRV_PLL_CLK_DIV(SEC, 28, 0, 24, PLL_DIVD),
	SDRV_PLL_CLK(SEC, 29, 0, 0, 0),
	SDRV_PLL_CLK(SEC, 30, 0, 0, 0),
	SDRV_PLL_CLK(SEC, 31, 0, 0, 0),
	SDRV_PLL_CLK(SEC, 32, 0, 0, 0),
};

void sdrv_register_sec_plls(struct clk *clkbase[])
{
	int i = 0, size;
	void __iomem *reg_base = 0;

	/* PLL3 ROOT/DIVA/B/C/D */
	reg_base = sdrv_get_pll_regbase(sec_clk_src_names[SEC_CLKIN_4]);
	if (reg_base) {
		for (i = SEC_CLKIN_4; i <= SEC_CLKIN_8; i++)
			sec_pll_clk[i].reg_base = reg_base;
	}

	/* PLL4 ROOT/DIVA/B/C/D */
	reg_base = sdrv_get_pll_regbase(sec_clk_src_names[SEC_CLKIN_9]);
	if (reg_base) {
		for (i = SEC_CLKIN_9; i <= SEC_CLKIN_13; i++)
			sec_pll_clk[i].reg_base = reg_base;
	}

	/* PLL5 ROOT/DIVA/B/C/D */
	reg_base = sdrv_get_pll_regbase(sec_clk_src_names[SEC_CLKIN_14]);
	if (reg_base) {
		for (i = SEC_CLKIN_14; i <= SEC_CLKIN_18; i++)
			sec_pll_clk[i].reg_base = reg_base;
	}


	/* PLL6 ROOT/DIVA/B/C/D */
	reg_base = sdrv_get_pll_regbase(sec_clk_src_names[SEC_CLKIN_19]);
	if (reg_base) {
		for (i = SEC_CLKIN_19; i <= SEC_CLKIN_23; i++)
			sec_pll_clk[i].reg_base = reg_base;
	}

	/* PLL7 ROOT/DIVA/B/C/D */
	reg_base = sdrv_get_pll_regbase(sec_clk_src_names[SEC_CLKIN_24]);
	if (reg_base) {
		for (i = SEC_CLKIN_24; i <= SEC_CLKIN_28; i++)
			sec_pll_clk[i].reg_base = reg_base;
	}

	size = ARRAY_SIZE(sec_pll_clk);
	for (i = 0; i < size; i++)
		clkbase[sec_pll_clk[i].clk_id] = sdrv_register_pll(0, i, sec_pll_clk, size, NULL, sec_clk_src_names, sec_clkin_freq);

}

