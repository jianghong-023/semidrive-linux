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
#include "clk.h"
#include "clk-pll.h"
#include "clk-sec-pll.h"
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
	983040000,
	655360000,
	491520000,
	393216000,
	327680000,
	882000000,
	588000000,
	441000000,
	352800000,
	294000000,
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


#define SDRV_PLL_CLK(id, base, div_shift, div_width) \
{	\
	.clk_id = SEC_CLKIN_ ##id,	\
	.reg_base = #base,	\
}

struct sdrv_cgu_pll_clk sec_pll_clk[SEC_CLKIN_MAX+1] = {
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
	SDRV_PLL_CLK(19, 0, 0, 0),
	SDRV_PLL_CLK(20, 0, 0, 0),
	SDRV_PLL_CLK(21, 0, 0, 0),
	SDRV_PLL_CLK(22, 0, 0, 0),
	SDRV_PLL_CLK(23, 0, 0, 0),
	SDRV_PLL_CLK(24, 0, 0, 0),
	SDRV_PLL_CLK(25, 0, 0, 0),
	SDRV_PLL_CLK(26, 0, 0, 0),
	SDRV_PLL_CLK(27, 0, 0, 0),
	SDRV_PLL_CLK(28, 0, 0, 0),
	SDRV_PLL_CLK(29, 0, 0, 0),
	SDRV_PLL_CLK(30, 0, 0, 0),
	SDRV_PLL_CLK(31, 0, 0, 0),
	SDRV_PLL_CLK(32, 0, 0, 0),
};

void sdrv_register_sec_plls(void)
{
	int i = 0;

	for (i = SEC_CLKIN_0; i <= SEC_CLKIN_MAX; i++)
		sdrv_register_pll(0, &sec_pll_clk[i], NULL, sec_clk_src_names, sec_clkin_freq);

}

