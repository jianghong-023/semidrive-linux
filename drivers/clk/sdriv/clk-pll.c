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
#include <linux/of_address.h>
#include "clk.h"
#include "clk-pll.h"

#define DEBUG_PLL 0

#define to_sdrv_cgu_pll(_hw) container_of(_hw, struct sdrv_cgu_pll_clk, hw)
/*2^24*/
#define FRACM (16777216)
static unsigned long sdrv_pll_calcurate_pll_cfg(const struct sdrv_pll_rate_table *rate)
{
	unsigned long tmp = 0;
	unsigned long freq = 0;

	if (rate->isint == 0)
		tmp = rate->frac*(1000000)/(FRACM);

	freq = (tmp + (rate->fbdiv*(1000000)))*(SOC_PLL_REFCLK_FREQ/(1000000))/rate->refdiv;
	return freq;
}
static unsigned long sdrv_pll_get_curr_freq(struct sdrv_cgu_pll_clk *pll, const struct sdrv_pll_rate_table *cur)
{
	unsigned long pll_cfg = sdrv_pll_calcurate_pll_cfg(cur);

	if (pll->div == PLL_DIVA)
		return pll_cfg/(cur->div[0]+1);
	else if (pll->div == PLL_DIVB)
		return pll_cfg/(cur->div[1]+1);
	else if (pll->div == PLL_DIVC)
		return pll_cfg/(cur->div[2]+1);
	else if (pll->div == PLL_DIVD)
		return pll_cfg/(cur->div[3]+1);
	else //root
		return pll_cfg/cur->postdiv[0]/cur->postdiv[1];
}
static void sdrv_pll_get_params(struct sdrv_cgu_pll_clk *pll,
					struct sdrv_pll_rate_table *rate)
{
	sc_pfpll_getparams(pll->reg_base, &rate->fbdiv, &rate->refdiv,
		&rate->postdiv[0], &rate->postdiv[1], &rate->frac,
		&rate->div[0], &rate->div[1], &rate->div[2], &rate->div[3], &rate->isint);
#if DEBUG_PLL
	pr_err("pll %s reg_base 0x%lx fbdiv %d, refdiv %d, postdiv1 %d postdiv2 %d frac %ld, diva %d, divb %d divc %d divd %d, isint %d\n",
		clk_hw_get_name(&pll->hw), pll->reg_base,
		rate->fbdiv, rate->refdiv, rate->postdiv[0], rate->postdiv[1], rate->frac,
		rate->div[0], rate->div[1], rate->div[2], rate->div[3], rate->isint);
#endif
}

static void sdrv_pll_set_params(struct sdrv_cgu_pll_clk *pll,
					const struct sdrv_pll_rate_table *rate)
{
	sc_pfpll_setparams(pll->reg_base, rate->fbdiv, rate->refdiv,
		rate->postdiv[0], rate->postdiv[1], rate->frac,
		rate->div[0], rate->div[1], rate->div[2], rate->div[3], rate->isint);
}


static unsigned long sdrv_pll_recalc_rate(struct clk_hw *hw,
						     unsigned long prate)
{
	struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
	struct sdrv_pll_rate_table cur;
	u64 rate64 = prate;

	if (pll->reg_base == NULL) {
		rate64 = pll->clkin_freq[pll->clk_id];
	} else {
		sdrv_pll_get_params(pll, &cur);
		rate64 = sdrv_pll_get_curr_freq(pll, &cur);
	}

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
	unsigned long rate = 0;

	for (i = 0; i < pll->rate_count; i++) {
		rate = sdrv_pll_get_curr_freq(pll, &rate_table[i]);
		if (drate <= rate)
			return rate;
	}

	return rate;
}

static int sdrv_pll_set_rate(struct clk_hw *hw, unsigned long drate,
		unsigned long prate)
{
	struct sdrv_cgu_pll_clk *pll = to_sdrv_cgu_pll(hw);
	const struct sdrv_pll_rate_table *rate_table = pll->rate_table;
	int i;
	unsigned long rate, flags = 0;

	if (pll->lock)
		spin_lock_irqsave(pll->lock, flags);
	else
		__acquire(pll->lock);


	for (i = 0; i < pll->rate_count; i++) {
		rate = sdrv_pll_get_curr_freq(pll, &rate_table[i]);
		if (drate <= rate) {
			sdrv_pll_set_params(pll, &rate_table[i]);
				if (pll->lock)
					spin_unlock_irqrestore(pll->lock, flags);
				else
					__release(pll->lock);
			return 0;
		}
	}

	sdrv_pll_set_params(pll, &rate_table[i-1]);

	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);
	else
		__release(pll->lock);

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

void __iomem *sdrv_get_pll_regbase(const char *name)
{
	void __iomem *reg_base = NULL;
	struct device_node *np = NULL;

	np = of_find_node_by_name(NULL, name);
	if (np != NULL) {
		if (of_device_is_available(np))
			reg_base = of_iomap(np, 0);
		of_node_put(np);
	}
	return reg_base;
}

static struct sdrv_cgu_pll_clk *sdrv_get_pll_by_id(int clk_id, struct sdrv_cgu_pll_clk *clk_table, int num)
{
	int i;

	for (i = 0; i < num; i++)
		if (clk_table[i].clk_id == clk_id)
			return &clk_table[i];
	return NULL;
}
static int sdrv_pll_pre_rate_change(struct sdrv_cgu_pll_clk *clk,
					   struct clk_notifier_data *ndata)
{
	int ret = 0;

	if (ndata->new_rate < clk->min_rate
			|| ndata->new_rate > clk->max_rate) {
		pr_err("want change clk %s freq from %ld to %ld, not allowd, min %ld max %ld\n",
			clk->name, ndata->old_rate, ndata->new_rate,
			clk->min_rate, clk->max_rate);
		ret = -EINVAL;
	}
	return ret;
}

static int sdrv_pll_post_rate_change(struct sdrv_cgu_pll_clk *clk,
					  struct clk_notifier_data *ndata)
{
	return 0;
}

static int sdrv_pll_notifier_cb(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct clk_notifier_data *ndata = data;
	struct sdrv_cgu_pll_clk *clk = nb_to_sdrv_cgu_pll_clk(nb);
	int ret = 0;

	pr_debug("%s: event %lu, old_rate %lu, new_rate: %lu\n",
		 __func__, event, ndata->old_rate, ndata->new_rate);
	if (event == PRE_RATE_CHANGE)
		ret = sdrv_pll_pre_rate_change(clk, ndata);
	else if (event == POST_RATE_CHANGE)
		ret = sdrv_pll_post_rate_change(clk, ndata);

	return notifier_from_errno(ret);
}

struct clk *sdrv_register_pll(void __iomem *base, int index,
		struct sdrv_cgu_pll_clk *clk_table, int num,
		struct sdrv_pll_rate_table *rate_table, const char *clk_name[],
		u64 *clkin_freq)
{
	struct sdrv_cgu_pll_clk *clk = &clk_table[index];
	const char *name = clk_name[clk->clk_id];
	const char *pll_parents[1];
	struct clk_init_data init;
	struct sdrv_cgu_pll_clk *pll = clk;
	struct clk *pll_clk;
	u32 min_freq = 0, max_freq = 0;

	pll->name = name;
	pll->clkin_freq = clkin_freq;
	if (base)
		pll->reg_base = base;
	/* now create the actual pll */
	init.name = name;

	/* keep all plls untouched for now */
	init.flags = CLK_IGNORE_UNUSED;

	if (pll->parent_id != -1) {
		struct sdrv_cgu_pll_clk *p_clk = sdrv_get_pll_by_id(pll->parent_id, clk_table, num);
		//init.flags |= CLK_SET_RATE_PARENT;
		pll_parents[0] = clk_name[pll->parent_id];
		init.parent_names = pll_parents;
		init.num_parents = 1;
		if (pll->reg_base && pll->reg_base == p_clk->reg_base)
			pll->lock = p_clk->lock;
	} else {
		init.num_parents = 0;
		if (pll->reg_base) {
			pll->lock = kmalloc(sizeof(spinlock_t), GFP_KERNEL);
			if (pll->lock)
				spin_lock_init(clk->lock);
		} else {
			pll->lock = NULL;
		}
	}

	if (rate_table) {
		int len;

		/* find count of rates in rate_table */
		for (len = 0; rate_table[len].refdiv != 0; )
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
#if DEBUG_PLL
	pr_err("pll %s\n", name);
#endif
	pll_clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(pll_clk)) {
		pr_err("%s: failed to register pll clock %s : %ld\n",
			__func__, name, PTR_ERR(pll_clk));
		return NULL;
	}

	if (sdrv_get_clk_min_rate(name, &min_freq) == 0) {
		clk_set_min_rate(pll_clk, min_freq);
		pll->min_rate = min_freq;
	} else {
		pll->min_rate = 0;
	}
	if (sdrv_get_clk_max_rate(name, &max_freq) == 0) {
		clk_set_max_rate(pll_clk, max_freq);
		pll->max_rate = max_freq;
	} else {
		pll->max_rate = ULONG_MAX;
	}
	clk->clk_nb.notifier_call = sdrv_pll_notifier_cb;
	if (clk_notifier_register(pll_clk, &pll->clk_nb)) {
		pr_err("%s: failed to register clock notifier for %s\n",
				__func__, pll->name);
	}
	return pll_clk;
}

