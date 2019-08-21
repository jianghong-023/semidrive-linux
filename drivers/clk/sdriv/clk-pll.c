/*
 * clk-pll.c
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

#define to_sdrv_cgu_pll(_hw) container_of(_hw, struct sdrv_cgu_pll_clk, hw)

static void sdrv_pll_get_params(struct sdrv_cgu_pll_clk *pll,
					struct sdrv_pll_rate_table *rate)
{
}

static unsigned long sdrv_pll_recalc_rate(struct clk_hw *hw,
						     unsigned long prate)
{
	struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
	struct sdrv_pll_rate_table cur;
	u64 rate64 = prate;

	sdrv_pll_get_params(pll, &cur);
	rate64 = pll->clkin_freq[pll->clk_id];

	return (unsigned long)rate64;
}

static int sdrv_pll_enable(struct clk_hw *hw)
{
	//struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
	//enable
	//wait pll locked
	return 0;
}

static void sdrv_pll_disable(struct clk_hw *hw)
{
	//struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
	//disable
}

static int sdrv_pll_is_enabled(struct clk_hw *hw)
{
	//struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
	return true;
}

static long sdrv_pll_round_rate(struct clk_hw *hw,
		unsigned long drate, unsigned long *prate)
{
	struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
	const struct sdrv_pll_rate_table *rate_table = pll->rate_table;
	int i;

	for (i = 0; i < pll->rate_count; i++) {
		if (drate <= rate_table[i].rate)
			return rate_table[i].rate;
	}

	return rate_table[i - 1].rate;
}

static int sdrv_pll_set_rate(struct clk_hw *hw, unsigned long drate,
		unsigned long prate)
{
	//struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
	//const struct sdrv_pll_rate_table *rate;

	return 0;
}

static void sdrv_pll_init(struct clk_hw *hw)
{
	//struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
}

static const struct clk_ops sdrv_pll_clk_norate_ops = {
	.recalc_rate = sdrv_pll_recalc_rate,
	.enable = sdrv_pll_enable,
	.disable = sdrv_pll_disable,
	.is_enabled = sdrv_pll_is_enabled,
};

static const struct clk_ops sdrv_pll_clk_ops = {
	.recalc_rate = sdrv_pll_recalc_rate,
	.round_rate = sdrv_pll_round_rate,
	.set_rate = sdrv_pll_set_rate,
	.enable = sdrv_pll_enable,
	.disable = sdrv_pll_disable,
	.is_enabled = sdrv_pll_is_enabled,
	.init = sdrv_pll_init,
};
struct clk *sdrv_register_pll(void __iomem *base, struct sdrv_cgu_pll_clk *clk,
		struct sdrv_pll_rate_table *rate_table, const char *clk_name[],
		u64 *clkin_freq)
{
	const char *name = clk_name[clk->clk_id];
	struct clk_init_data init;
	struct sdrv_cgu_pll_clk *pll = clk;
	struct clk *pll_clk;

	pll->clkin_freq = clkin_freq;
	pll->reg_base = base;
	/* now create the actual pll */
	init.name = name;

	/* keep all plls untouched for now */
	init.flags = CLK_IGNORE_UNUSED;

	init.num_parents = 0;

	if (rate_table) {
		int len;

		/* find count of rates in rate_table */
		for (len = 0; rate_table[len].rate != 0; )
			len++;

		pll->rate_count = len;
		pll->rate_table = kmemdup(rate_table,
					pll->rate_count *
					sizeof(struct sdrv_pll_rate_table),
					GFP_KERNEL);
		WARN(!pll->rate_table,
			"%s: could not allocate rate table for %s\n",
			__func__, name);
	}

	if (!pll->rate_table)
		init.ops = &sdrv_pll_clk_norate_ops;
	else
		init.ops = &sdrv_pll_clk_ops;


	pll->hw.init = &init;

	pll_clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(pll_clk)) {
		pr_err("%s: failed to register pll clock %s : %ld\n",
			__func__, name, PTR_ERR(pll_clk));
	}

	return pll_clk;
}

