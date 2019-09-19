/*
 * clk-pll.h
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

#include "clk.h"
#include "sc_pfpll.h"
#ifndef __CLK_PLL_H
#define       __CLK_PLL_H

#define SDRV_PLL_CLK(clkprefix, id, base, div_shift, div_width) \
{	\
	.clk_id = clkprefix ## _CLKIN_ ##id,	\
	.reg_base = base,	\
	.parent_id = -1,	\
}
#define SDRV_PLL_CLK_RATE_TABLE(clkprefix, id, base, _rate_table) \
{	\
	.clk_id = clkprefix ## _CLKIN_ ##id,	\
	.reg_base = base,	\
	.rate_table = _rate_table,	\
	.rate_count = ARRAY_SIZE(_rate_table),	\
	.parent_id = -1,	\
	.div = PLL_ROOT,	\
}
#define SDRV_PLL_CLK_ROOT(clkprefix, id, base) \
{	\
	.clk_id = clkprefix ## _CLKIN_ ##id,	\
	.reg_base = base,	\
	.rate_table = NULL,	\
	.rate_count = 0,	\
	.parent_id = -1,	\
	.div = PLL_ROOT,	\
}

#define SDRV_PLL_CLK_DIV(clkprefix, id, base, _parent, _div) \
{	\
	.clk_id = clkprefix ## _CLKIN_ ##id,	\
	.reg_base = base,	\
	.parent_id = clkprefix ## _CLKIN_ ##_parent,	\
	.div = _div,	\
}

#define SDRV_PLL_CLK_DIV_NO_PARENT(clkprefix, id, base, _div) \
{	\
	.clk_id = clkprefix ## _CLKIN_ ##id,	\
	.reg_base = base,	\
	.parent_id = -1,	\
	.div = _div,	\
}

enum rootdiv {
	PLL_ROOT,
	PLL_DIVA,
	PLL_DIVB,
	PLL_DIVC,
	PLL_DIVD,
};

struct sdrv_pll_rate_table {
	unsigned int refdiv;
	unsigned int fbdiv;
	unsigned int postdiv[2];
	unsigned long frac;
	int div[4];
	int isint;
};
struct sdrv_cgu_pll_clk {
	u8 clk_id;
	int parent_id;
	const char *name;
	enum rootdiv div;/*0: root; 1~4: div a,b,c,d*/
	struct clk_hw hw;
	void __iomem *reg_base;
	const struct sdrv_pll_rate_table *rate_table;
	unsigned int rate_count;
	spinlock_t *lock;
	u64 *clkin_freq;
	const struct clk_ops *pll_ops;
	unsigned long min_rate;
	unsigned long max_rate;
	struct notifier_block clk_nb;
};
#define nb_to_sdrv_cgu_pll_clk(nb) container_of(nb, struct sdrv_cgu_pll_clk, clk_nb)
struct clk *sdrv_register_pll(void __iomem *base, int index,
	struct sdrv_cgu_pll_clk *clk_table, int num,
	struct sdrv_pll_rate_table *rate_table, const char *clk_name[],
	u64 *clkin_freq);
void __iomem *sdrv_get_pll_regbase(const char *name);
#endif

