/*
 * clk.h
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

#ifndef __CLK__H__
#define __CLK__H__

#define MAX_PARENT_NUM 10
#include "ckgate.h"
#define IP_PREDIV_SHIFT 4
#define IP_PREDIV_WIDTH 3
#define IP_PREDIV_BUSYSHIFT 30
#define IP_PREDIV_BUSYWIDTH 1
#define IP_PREDIV_EXPECT 0
#define IP_POSTDIV_SHIFT 10
#define IP_POSTDIV_WIDTH 6
#define IP_POSTDIV_BUSYSHIFT 31
#define IP_POSTDIV_BUSYWIDTH 1
#define IP_POSTDIV_EXPECT 0

#define CORE_DIV_SHIFT 10
#define CORE_DIV_WIDTH 6
#define CORE_DIV_BUSYSHIFT 31
#define CORE_DIV_BUSYWIDTH 1
#define CORE_DIV_EXPECT 0

#define BUS_POSTDIV_SHIFT 10
#define BUS_POSTDIV_WIDTH 6
#define BUS_POSTDIV_BUSYSHIFT 31
#define BUS_POSTDIV_BUSYWIDTH 1
#define BUS_POSTDIV_EXPECT 0

enum CLK_TYPE {
	CLK_TYPE_CORE,
	CLK_TYPE_BUS,
	CLK_TYPE_IP,
	CLK_TYPE_IP_POST,
	CLK_TYPE_UUU_MUX,
	CLK_TYPE_UUU_MUX2,
	CLK_TYPE_UUU_DIVIDER,
	CLK_TYPE_GATE,
};

struct sdrv_cgu_out_clk {
	int clk_id;
	u8 n_parents;
	const char *name;
	enum CLK_TYPE type;
	u32 slice_id;
	int gate_id;
	void __iomem	*base;
	spinlock_t	*lock;
	//divider
	int parent_id;
	//mux
	u8 mux_shift;
	u8 mux_width;
	//composite
	u32 *mux_table;
	struct clk_divider      div;
	struct clk_hw mux_hw;
	struct clk_hw gate_hw;
	unsigned long min_rate;
	unsigned long max_rate;
	struct notifier_block clk_nb;
	int busywidth;	//0 means no busy bit
	int busyshift;
	int expect;
};
#define mux_to_sdrv_cgu_out_clk(_hw) container_of(_hw, struct sdrv_cgu_out_clk, mux_hw)
#define gate_to_sdrv_cgu_out_clk(_hw) container_of(_hw, struct sdrv_cgu_out_clk, gate_hw)
#define div_to_sdrv_cgu_out_clk(_hw) container_of(_hw, struct sdrv_cgu_out_clk, div.hw)
#define nb_to_sdrv_cgu_out_clk(nb) container_of(nb, struct sdrv_cgu_out_clk, clk_nb)

struct clk *sdrv_register_out_composite(struct device_node *np, void __iomem *base, struct sdrv_cgu_out_clk *clk, const char *global_clk_names[]);
int sdrv_get_clk_min_rate(const char *name, u32 *min);
int sdrv_get_clk_max_rate(const char *name, u32 *max);
#endif

