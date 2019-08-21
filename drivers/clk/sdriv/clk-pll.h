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
#ifndef __CLK_PLL_H
#define       __CLK_PLL_H

struct sdrv_pll_rate_table {
	unsigned long rate;
	unsigned int fbdiv;
	unsigned int postdiv1;
	unsigned int refdiv;
	unsigned int postdiv2;
};
struct sdrv_cgu_pll_clk {
	u8 clk_id;
	struct clk_hw hw;
	void __iomem *reg_base;
	const struct sdrv_pll_rate_table *rate_table;
	unsigned int rate_count;
	spinlock_t *lock;
	u64 *clkin_freq;
	const struct clk_ops *pll_ops;
};

struct clk *sdrv_register_pll(void __iomem *base, struct sdrv_cgu_pll_clk *clk,
	struct sdrv_pll_rate_table *rate_table, const char *clk_name[],
	u64 *clkin_freq);
#endif

