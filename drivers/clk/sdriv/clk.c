/*
 * clk.c
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
#include "ckgen.h"

typedef union {
	struct  {
		u32 cg_en_a:1;
		u32 src_sel_a:3;
		u32 reserved1:5;
		u32 a_b_sel:1;
		u32 post_div_num:6;
		u32 cg_en_b:1;
		u32 src_sel_b:3;
		u32 reserved2:7;
		u32 cg_en_b_status:1;
		u32 cg_en_a_status:1;
		u32 reserved3:2;
		u32 post_busy:1;
	};

	u32 val;
} soc_core_ctl;
typedef union {

struct   {
	u32 cg_en_a:1;
	u32 src_sel_a:3;
	u32 pre_div_num_a:3;
	u32 reserved1:2;
	u32 a_b_sel:1;
	u32 post_div_num:6;
	u32 cg_en_b:1;
	u32 src_sel_b:3;
	u32 pre_div_num_b:3;
	u32 reserved2:4;
	u32 cg_en_b_status:1;
	u32 cg_en_a_status:1;
	u32 pre_busy_b:1;
	u32 pre_busy_a:1;
	u32 post_busy:1;
};
u32 val;
} soc_bus_ctl;
typedef union {

struct  {
	u32 cg_en:1;
	u32 src_sel:3;
	u32 pre_div_num:3;
	u32 reserved1:3;
	u32 post_div_num:6;
	u32 reserved2:12;
	u32 cg_en_status:1;
	u32 reserved3:1;
	u32 pre_busy:1;
	u32 post_busy:1;
};
u32 val;
} soc_ip_ctl;


typedef union {
	struct  {
		u32 q_div:4;
		u32 p_div:4;
		u32 n_div:4;
		u32 m_div:4;
		u32 uuu_sel0:1;
		u32 uuu_sel1:1;
		u32 reserved1:2;
		u32 uuu_gating_ack:1;
		u32 reserved2:6;
		u32 dbg_div:4;
		u32 dbg_gating_en:1;
	};

	u32 val;
} soc_uuu;

typedef union {

struct  {
	u32 sw_gating_en:1;
	u32 sw_gating_dis:1;	//0, means enable, 1 means disable.
	u32 reserved:29;
	u32 ckgen_gating_lock:1;
};
u32 val;
} soc_lp_gate_en;


static u32 get_soc_core_parent(void __iomem *base, u8 slice_id)
{
	u32 val;
	void __iomem *reg = base + SOC_CKGEN_REG_MAP(CKGEN_CORE_SLICE_CTL_OFF(slice_id));
	soc_core_ctl ctl;

	ctl.val = clk_readl(reg);
	if (ctl.a_b_sel == 0)
		val = ctl.src_sel_a;
	else
		val = ctl.src_sel_b;
	pr_debug("%s id 0x%x, index %d\n", __func__, slice_id, val);
	return val;
}

static u32 set_soc_core_parent(void __iomem *base, u32 slice_id, u32 index)
{
	void __iomem *reg = base + SOC_CKGEN_REG_MAP(CKGEN_CORE_SLICE_CTL_OFF(slice_id));
	soc_core_ctl ctl;

	ctl.val = clk_readl(reg);
	pr_debug("%s id 0x%x, index %d\n", __func__, slice_id, index);
	ckgen_core_slice_cfg(base, slice_id, !ctl.a_b_sel, index);

	ckgen_core_slice_switch(base, slice_id);
	return 0;
}


static u32 get_soc_bus_parent(void __iomem *base, u8 slice_id)
{
	u32 val;
	void __iomem *reg = base + SOC_CKGEN_REG_MAP(CKGEN_BUS_SLICE_CTL_OFF(slice_id));
	soc_bus_ctl ctl;

	ctl.val = clk_readl(reg);

	if (ctl.a_b_sel == 0)
		val = ctl.src_sel_a;
	else
		val = ctl.src_sel_b;
	pr_debug("%s id 0x%x, index %d\n", __func__, slice_id, val);
	return val;
}

static u32 set_soc_bus_parent(void __iomem *base, u32 slice_id, u32 index)
{
	void __iomem *reg = base + SOC_CKGEN_REG_MAP(CKGEN_BUS_SLICE_CTL_OFF(slice_id));
	soc_bus_ctl ctl;

	ctl.val = clk_readl(reg);
	pr_debug("%s id 0x%x, index %d\n", __func__, slice_id, index);

	if (ctl.a_b_sel)
		ckgen_bus_slice_cfg(base, slice_id, !ctl.a_b_sel, index, ctl.pre_div_num_b);
	else
		ckgen_bus_slice_cfg(base, slice_id, !ctl.a_b_sel, index, ctl.pre_div_num_a);
	ckgen_bus_slice_switch(base, slice_id);
	return 0;
}

static u32 get_soc_ip_parent(void __iomem *base, u8 slice_id)
{
	u32 val;
	void __iomem *reg = base + SOC_CKGEN_REG_MAP(CKGEN_IP_SLICE_CTL_OFF(slice_id));
	soc_ip_ctl ctl;

	ctl.val = clk_readl(reg);

	val = ctl.src_sel;
	pr_debug("%s id 0x%x, index %d\n", __func__, slice_id, val);
	return val;
}

static u32 set_soc_ip_parent(void __iomem *base, u32 slice_id, u32 index)
{
	void __iomem *reg = base + SOC_CKGEN_REG_MAP(CKGEN_IP_SLICE_CTL_OFF(slice_id));
	soc_ip_ctl ctl;

	ctl.val = clk_readl(reg);
	pr_debug("%s id 0x%x, index %d\n", __func__, slice_id, index);
	ckgen_ip_slice_cfg(base, slice_id, index, ctl.pre_div_num, ctl.post_div_num);
	return 0;
}

static u32 get_soc_uuu_sel0_parent(void __iomem *base, u8 slice_id)
{
	u32 val;
	void __iomem *reg = base + SOC_CKGEN_REG_MAP(CKGEN_UUU_SLICE_OFF(slice_id));
	soc_uuu ctl;

	ctl.val = clk_readl(reg);
	val = ctl.uuu_sel0;
	pr_debug("%s id 0x%x, index %d\n", __func__, slice_id, val);
	return val;
}

static u32 set_soc_uuu_sel0_parent(void __iomem *base, u32 slice_id, u32 index)
{
	void __iomem *reg = base + SOC_CKGEN_REG_MAP(CKGEN_UUU_SLICE_OFF(slice_id));
	soc_uuu ctl;

	ctl.val = clk_readl(reg);
	pr_debug("%s id 0x%x, index %d\n", __func__, slice_id, index);
	ctl.uuu_sel0 = index;
	clk_writel(ctl.val, reg);
	return 0;
}

static u32 get_soc_uuu_sel1_parent(void __iomem *base, u8 slice_id)
{
	u32 val;
	void __iomem *reg = base + SOC_CKGEN_REG_MAP(CKGEN_UUU_SLICE_OFF(slice_id));
	soc_uuu ctl;

	ctl.val = clk_readl(reg);
	val = ctl.uuu_sel1;
	pr_debug("%s id 0x%x, index %d\n", __func__, slice_id, val);
	return val;
}

static u32 set_soc_uuu_sel1_parent(void __iomem *base, u32 slice_id, u32 index)
{
	void __iomem *reg = base + SOC_CKGEN_REG_MAP(CKGEN_UUU_SLICE_OFF(slice_id));
	soc_uuu ctl;

	ctl.val = clk_readl(reg);
	pr_debug("%s id 0x%x, index %d\n", __func__, slice_id, index);
	ctl.uuu_sel1 = index;
	clk_writel(ctl.val, reg);
	return 0;
}

static u8 sdclk_mux_get_parent(struct clk_hw *hw)
{
	struct sdrv_cgu_out_clk *clk = mux_to_sdrv_cgu_out_clk(hw);
	int num_parents = clk_hw_get_num_parents(hw);
	u32 val;

	if (clk->type == CLK_TYPE_CORE) {
		val = get_soc_core_parent(clk->base, clk->slice_id);
	} else if (clk->type == CLK_TYPE_BUS) {
		val = get_soc_bus_parent(clk->base, clk->slice_id);
	} else if (clk->type == CLK_TYPE_IP) {
		val = get_soc_ip_parent(clk->base, clk->slice_id);
	} else if (clk->type == CLK_TYPE_UUU_MUX) {//UUU MUX
		val = get_soc_uuu_sel0_parent(clk->base, clk->slice_id);
	} else if (clk->type == CLK_TYPE_UUU_MUX2) {//UUU MUX
		val = get_soc_uuu_sel1_parent(clk->base, clk->slice_id);
	} else {//UUU n/p/q and gate only have one parent
		return 0;
	}

	if (clk->mux_table) {
		int i;

		for (i = 0; i < num_parents; i++)
			if (i == val)
				return i;
		return -EINVAL;
	}
	if (val >= num_parents)
		return -EINVAL;
	return val;
}

static int sdclk_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct sdrv_cgu_out_clk *clk = mux_to_sdrv_cgu_out_clk(hw);
	u32 val;
	unsigned long flags = 0;

	if (clk->mux_table)
		index = index;

	if (clk->lock)
		spin_lock_irqsave(clk->lock, flags);
	else
		__acquire(clk->lock);

	if (clk->type == CLK_TYPE_CORE) {
		val = set_soc_core_parent(clk->base, clk->slice_id, index);
	} else if (clk->type == CLK_TYPE_BUS) {
		val = set_soc_bus_parent(clk->base, clk->slice_id, index);
	} else if (clk->type == CLK_TYPE_IP) {
		val = set_soc_ip_parent(clk->base, clk->slice_id, index);
	} else if (clk->type == CLK_TYPE_UUU_MUX) {//UUU MUX
		val = set_soc_uuu_sel0_parent(clk->base, clk->slice_id, index);
	} else if (clk->type == CLK_TYPE_UUU_MUX2) {//UUU MUX2
		val = set_soc_uuu_sel1_parent(clk->base, clk->slice_id, index);
	} else {//UUU and gate only have one parent
		return 0;
	}

	if (clk->lock)
		spin_unlock_irqrestore(clk->lock, flags);
	else
		__release(clk->lock);

	return 0;
}
static int sdclk_mux_determine_rate(struct clk_hw *hw,
				struct clk_rate_request *req)
{
	return clk_mux_determine_rate_flags(hw, req, CLK_MUX_ROUND_CLOSEST);
}

const struct clk_ops sdclk_mux_ops = {
	.get_parent = sdclk_mux_get_parent,
	.set_parent = sdclk_mux_set_parent,
	.determine_rate = sdclk_mux_determine_rate,
};


static int sdclk_gate_enable(struct clk_hw *hw)
{
	struct sdrv_cgu_out_clk *clk = gate_to_sdrv_cgu_out_clk(hw);
	u32 __iomem *reg = 0;
	unsigned long uninitialized_var(flags);

	if (clk->lock)
		spin_lock_irqsave(clk->lock, flags);
	else
		__acquire(clk->lock);


	pr_debug("%s name %s\n", __func__, clk->name);
	if (clk->type == CLK_TYPE_CORE)
	{
		soc_core_ctl ctl;

		reg = clk->base + SOC_CKGEN_REG_MAP(CKGEN_CORE_SLICE_CTL_OFF(clk->slice_id));
		ctl.val = clk_readl(reg);
		if (ctl.a_b_sel == 0) //A
			ctl.cg_en_a = 1;
		else
			ctl.cg_en_b = 1;
		clk_writel(ctl.val, reg);
	} else if (clk->type == CLK_TYPE_BUS) {
		soc_bus_ctl ctl;

		reg = clk->base + SOC_CKGEN_REG_MAP(CKGEN_BUS_SLICE_CTL_OFF(clk->slice_id));
		ctl.val = clk_readl(reg);
		if (ctl.a_b_sel == 0) //A
			ctl.cg_en_a = 1;
		else
			ctl.cg_en_b = 1;

		clk_writel(ctl.val, reg);
	} else if (clk->type == CLK_TYPE_IP) {
		soc_ip_ctl ctl;

		reg = clk->base + SOC_CKGEN_REG_MAP(CKGEN_IP_SLICE_CTL_OFF(clk->slice_id));
		ctl.val = clk_readl(reg);
		ctl.cg_en = 1;
		clk_writel(ctl.val, reg);
	} else if (clk->type == CLK_TYPE_UUU_DIVIDER || clk->type == CLK_TYPE_UUU_MUX2
			|| clk->type == CLK_TYPE_GATE) {
		if (clk->gate_id != -1) {
			soc_lp_gate_en gate;

			reg = clk->base + SOC_CKGEN_REG_MAP(CKGEN_LP_GATING_EN_OFF(clk->gate_id));//get gating id?
			gate.val = clk_readl(reg);
			gate.sw_gating_dis = 0;
			clk_writel(gate.val, reg);
		}
	}

	if (clk->lock)
		spin_unlock_irqrestore(clk->lock, flags);
	else
		__release(clk->lock);

	return 0;
}

static void sdclk_gate_disable(struct clk_hw *hw)
{
	struct sdrv_cgu_out_clk *clk = gate_to_sdrv_cgu_out_clk(hw);
	u32 __iomem *reg = 0;
	unsigned long uninitialized_var(flags);

	if (clk->lock)
		spin_lock_irqsave(clk->lock, flags);
	else
		__acquire(clk->lock);


	pr_debug("%s name %s\n", __func__, clk->name);
	if (clk->type == CLK_TYPE_CORE) {
		soc_core_ctl ctl;

		reg = clk->base + SOC_CKGEN_REG_MAP(CKGEN_CORE_SLICE_CTL_OFF(clk->slice_id));
		ctl.val = clk_readl(reg);
		if (ctl.a_b_sel == 0) //A
			ctl.cg_en_a = 0;
		else
			ctl.cg_en_b = 0;

		clk_writel(ctl.val, reg);
	} else if (clk->type == CLK_TYPE_BUS) {
		soc_bus_ctl ctl;

		reg = clk->base + SOC_CKGEN_REG_MAP(CKGEN_BUS_SLICE_CTL_OFF(clk->slice_id));
		ctl.val = clk_readl(reg);
		if (ctl.a_b_sel == 0) //A
			ctl.cg_en_a = 0;
		else
			ctl.cg_en_b = 0;

		clk_writel(ctl.val, reg);
	} else if (clk->type == CLK_TYPE_IP) {
		soc_ip_ctl ctl;

		reg = clk->base + SOC_CKGEN_REG_MAP(CKGEN_IP_SLICE_CTL_OFF(clk->slice_id));
		ctl.val = clk_readl(reg);
		ctl.cg_en = 0;
		clk_writel(ctl.val, reg);
	} else if (clk->type == CLK_TYPE_UUU_DIVIDER || clk->type == CLK_TYPE_UUU_MUX2
			|| clk->type == CLK_TYPE_GATE) {
		if (clk->gate_id != -1) {
			soc_lp_gate_en gate;

			reg = clk->base + SOC_CKGEN_REG_MAP(CKGEN_LP_GATING_EN_OFF(clk->gate_id));//get gating id?
			gate.val = clk_readl(reg);
			gate.sw_gating_dis = 1;
			clk_writel(gate.val, reg);
		}
	}

	if (clk->lock)
		spin_unlock_irqrestore(clk->lock, flags);
	else
		__release(clk->lock);

}

int sdclk_gate_is_enabled(struct clk_hw *hw)
{
	struct sdrv_cgu_out_clk *clk = gate_to_sdrv_cgu_out_clk(hw);
	void __iomem *reg = 0;
	int ret = 0;
	unsigned long uninitialized_var(flags);

	pr_debug("%s name %s\n", __func__, clk->name);
	if (clk->lock)
		spin_lock_irqsave(clk->lock, flags);
	else
		__acquire(clk->lock);
	if (clk->type == CLK_TYPE_CORE) {
		soc_core_ctl ctl;

		reg = clk->base + SOC_CKGEN_REG_MAP(CKGEN_CORE_SLICE_CTL_OFF(clk->slice_id));
		ctl.val = clk_readl(reg);
		if (ctl.a_b_sel == 0) //A
			ret = ctl.cg_en_a;
		else
			ret = ctl.cg_en_b;
	} else if (clk->type == CLK_TYPE_BUS) {
		soc_bus_ctl ctl;

		reg = clk->base + SOC_CKGEN_REG_MAP(CKGEN_BUS_SLICE_CTL_OFF(clk->slice_id));
		ctl.val = clk_readl(reg);
		if (ctl.a_b_sel == 0) //A
			ret = ctl.cg_en_a;
		else
			ret = ctl.cg_en_b;
	} else if (clk->type == CLK_TYPE_IP) {
		soc_ip_ctl ctl;

		reg = clk->base + SOC_CKGEN_REG_MAP(CKGEN_IP_SLICE_CTL_OFF(clk->slice_id));
		ctl.val = clk_readl(reg);
		ret = ctl.cg_en;
	} else if (clk->type == CLK_TYPE_UUU_DIVIDER || clk->type == CLK_TYPE_UUU_MUX2
		|| clk->type == CLK_TYPE_GATE) {
		if (clk->gate_id != -1) {
			soc_lp_gate_en gate;

			reg = clk->base + SOC_CKGEN_REG_MAP(CKGEN_LP_GATING_EN_OFF(clk->gate_id));//get gating id?
			gate.val = clk_readl(reg);
			ret = !gate.sw_gating_dis;
		} else {
			ret = 1;
		}
	}
	if (clk->lock)
		spin_unlock_irqrestore(clk->lock, flags);
	else
		__release(clk->lock);
	return ret;
}

const struct clk_ops sdclk_gate_ops = {
	.enable = sdclk_gate_enable,
	.disable = sdclk_gate_disable,
	.is_enabled = sdclk_gate_is_enabled,
};

static unsigned long sdclk_divider_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	return clk_divider_ops.recalc_rate(hw, parent_rate);
}

static long sdclk_divider_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *prate)
{
	return clk_divider_ops.round_rate(hw, rate, prate);
}

static int sdclk_divider_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	//struct sdrv_cgu_out_clk *clk = div_to_sdrv_cgu_out_clk(hw);
	pr_debug("%s name %s rate %ld prate %ld\n", __func__, clk_hw_get_name(hw), rate, parent_rate);
	return clk_divider_ops.set_rate(hw, rate, parent_rate);
}


const struct clk_ops sdclk_divider_ops = {
	.recalc_rate = sdclk_divider_recalc_rate,
	.round_rate = sdclk_divider_round_rate,
	.set_rate = sdclk_divider_set_rate,
};

static void sdrv_fill_parent_names(const char **parent, u32 *id, int size, const char *global_clk_names[])
{
	int i;

	for (i = 0; i < size; i++)
		parent[i] = global_clk_names[id[i]];
}

struct clk *sdrv_register_out_composite(struct device_node *np, void __iomem *base, struct sdrv_cgu_out_clk *clk, const char *global_clk_names[])
{
	const char *parents[MAX_PARENT_NUM];
	u32 ctl_offset = 0;
	struct clk *ret = NULL;

	clk->name = global_clk_names[clk->clk_id];
	clk->base = base;
	clk->lock = kmalloc(sizeof(spinlock_t), GFP_KERNEL);
	spin_lock_init(clk->lock);

	switch (clk->type) {
	case CLK_TYPE_CORE:
		ctl_offset = SOC_CKGEN_REG_MAP(CKGEN_CORE_SLICE_CTL_OFF(clk->slice_id));
		break;
	case CLK_TYPE_BUS:
		ctl_offset = SOC_CKGEN_REG_MAP(CKGEN_BUS_SLICE_CTL_OFF(clk->slice_id));
		break;
	case CLK_TYPE_IP:
		ctl_offset = SOC_CKGEN_REG_MAP(CKGEN_IP_SLICE_CTL_OFF(clk->slice_id));
		break;
	case CLK_TYPE_UUU_MUX2:
	case CLK_TYPE_UUU_MUX:
	case CLK_TYPE_UUU_DIVIDER:
		ctl_offset = SOC_CKGEN_REG_MAP(CKGEN_UUU_SLICE_OFF(clk->slice_id));
		break;
	case CLK_TYPE_GATE:
	default:
		break;
	}

	if (clk->type != CLK_TYPE_UUU_MUX && clk->type != CLK_TYPE_UUU_MUX2
				&& clk->type != CLK_TYPE_GATE) {
		clk->div.reg = base + ctl_offset;
		clk->div.lock = clk->lock;
		clk->div.flags = CLK_DIVIDER_ROUND_CLOSEST;
	}

	pr_debug("register clk:%s\n", clk->name);
	if (clk->type == CLK_TYPE_UUU_MUX) {
		sdrv_fill_parent_names(parents, clk->mux_table, clk->n_parents, global_clk_names);
		ret = clk_register_composite(NULL, clk->name, parents, clk->n_parents,
					&clk->mux_hw, &sdclk_mux_ops,
					NULL, NULL,
					NULL, NULL, CLK_IGNORE_UNUSED|CLK_SET_RATE_PARENT);
	} else if (clk->type == CLK_TYPE_UUU_MUX2) {
		sdrv_fill_parent_names(parents, clk->mux_table, clk->n_parents, global_clk_names);
		ret = clk_register_composite(NULL, clk->name, parents, clk->n_parents,
					&clk->mux_hw, &sdclk_mux_ops,
					NULL, NULL,
					&clk->gate_hw, &sdclk_gate_ops, CLK_IGNORE_UNUSED|CLK_SET_RATE_PARENT);
	} else if (clk->type == CLK_TYPE_UUU_DIVIDER) {
		parents[0] = global_clk_names[clk->parent_id];
		ret = clk_register_composite(NULL, clk->name, parents, 1,
					&clk->mux_hw, &sdclk_mux_ops,
					&clk->div.hw, &sdclk_divider_ops,
					&clk->gate_hw, &sdclk_gate_ops, CLK_IGNORE_UNUSED|CLK_SET_RATE_PARENT);
	} else if (clk->type == CLK_TYPE_GATE) {
		if (clk->parent_id != -1) {
			parents[0] = global_clk_names[clk->parent_id];
			ret = clk_register_composite(NULL, clk->name, parents, 1,
						&clk->mux_hw, &sdclk_mux_ops,
						NULL, NULL,
						&clk->gate_hw, &sdclk_gate_ops, CLK_IGNORE_UNUSED|CLK_SET_RATE_PARENT);
		} else {
			ret = clk_register_composite(NULL, clk->name, NULL, 0,
						NULL, NULL,
						NULL, NULL,
						&clk->gate_hw, &sdclk_gate_ops, CLK_IGNORE_UNUSED);
		}
	} else {
		sdrv_fill_parent_names(parents, clk->mux_table, clk->n_parents, global_clk_names);
		ret = clk_register_composite(NULL, clk->name, parents, clk->n_parents,
					&clk->mux_hw, &sdclk_mux_ops,
					&clk->div.hw, &sdclk_divider_ops,
					&clk->gate_hw, &sdclk_gate_ops, CLK_IGNORE_UNUSED);
	}

	return ret;
}
