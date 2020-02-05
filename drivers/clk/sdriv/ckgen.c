/*
 * ckgen.c
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
#include "ckgen_reg.h"
#include "ckgen.h"

#define MAX_POLL_COUNT 1000
/*
 * IP clk slice
 * clkin0-7 -> CG -> pre_div(3 bits) -> post_div(6bits)
 *
 * clkin0 by default
 */
void ckgen_ip_slice_cfg(void __iomem *base, u32 slice_id,
		u32 src_sel, u32 pre_div, u32 post_div)
{
	void __iomem *ctl_addr = base +
		SOC_CKGEN_REG_MAP(CKGEN_IP_SLICE_CTL_OFF(slice_id));
	u32 ctl = clk_readl(ctl_addr);
	int count = MAX_POLL_COUNT;

	/* 1> Set pre_en to 1'b0 to turn off the clock */
	ctl &= ~BM_CKGEN_IP_SLICE_CTL_CG_EN;
	clk_writel(ctl, ctl_addr);

	while ((clk_readl(ctl_addr) & (BM_CKGEN_IP_SLICE_CTL_CG_EN_STATUS)) && count--)
		cpu_relax();

	/* 2> Change pre_mux_sel to select clock source */
	ctl &= ~FM_CKGEN_IP_SLICE_CTL_CLK_SRC_SEL;
	ctl |= FV_CKGEN_IP_SLICE_CTL_CLK_SRC_SEL(src_sel);
	clk_writel(ctl, ctl_addr);

	/* 3> Set pre_en to 1'b1 to turn on the clock */
	ctl |= BM_CKGEN_IP_SLICE_CTL_CG_EN;
	clk_writel(ctl, ctl_addr);
	count = MAX_POLL_COUNT;
	while (!(clk_readl(ctl_addr) & (BM_CKGEN_IP_SLICE_CTL_CG_EN_STATUS)) && count--)
		cpu_relax();

	/* 4>	a. Set pre_div_num */
	ctl &= ~FM_CKGEN_IP_SLICE_CTL_PRE_DIV_NUM;
	ctl |= FV_CKGEN_IP_SLICE_CTL_PRE_DIV_NUM(pre_div);
	clk_writel(ctl, ctl_addr);
	count = MAX_POLL_COUNT;
	/*		b. Check the corresponding busy bit. */
	while ((clk_readl(ctl_addr) & (BM_CKGEN_IP_SLICE_CTL_PRE_BUSY)) && count--)
		cpu_relax();
	if (count == 0)
		pr_err("polling fail: ip pre div busy bit\n");
	/* 4>	a. Set post_div_num */
	ctl &= ~FM_CKGEN_IP_SLICE_CTL_POST_DIV_NUM;
	ctl |= FV_CKGEN_IP_SLICE_CTL_POST_DIV_NUM(post_div);
	clk_writel(ctl, ctl_addr);
	/*		b. Check the corresponding busy bit. */
	count = MAX_POLL_COUNT;
	while ((clk_readl(ctl_addr) & (BM_CKGEN_IP_SLICE_CTL_POST_BUSY)) && count--)
		cpu_relax();
	if (count == 0)
		pr_err("polling fail: ip post div busy bit\n");
}

/*
 * Bus clk slice
 *
 * clk_in0-7 -> CG -> pre_div_a(3 bit) -> |\
 *                                          | |--> post_div
 * clk_in0-7 -> CG -> pre_div_b(3 bit) -> |/
 *                                          pre_a_b_sel
 *
 * a_clk_in0 by default.
 *
 * SW program sequence to change from a to b.
 */
void ckgen_bus_slice_cfg(void __iomem *base, u32 slice_id,
		u32 path, u32 src_sel, u32 pre_div)
{
	void __iomem *ctl_addr = base +
		SOC_CKGEN_REG_MAP(CKGEN_BUS_SLICE_CTL_OFF(slice_id));
	u32 ctl = clk_readl(ctl_addr);
	int count = MAX_POLL_COUNT;

	if (path == PATH_A) {
		/* B is being selected now */
		WARN_ON(!(ctl & BM_CKGEN_BUS_SLICE_CTL_A_B_SEL));
		/* 1> Set pre_en to 1'b0 to turn off path. */
		ctl &= ~BM_CKGEN_BUS_SLICE_CTL_CG_EN_A;
		clk_writel(ctl, ctl_addr);
		while ((clk_readl(ctl_addr) & (BM_CKGEN_BUS_SLICE_CTL_CG_EN_A_STATUS)) && count--)
			cpu_relax();
		/* 2> Change pre_mux_sel to select clock source.*/
		ctl &= ~FM_CKGEN_BUS_SLICE_CTL_CLK_SRC_SEL_A;
		ctl |= FV_CKGEN_BUS_SLICE_CTL_CLK_SRC_SEL_A(src_sel);
		clk_writel(ctl, ctl_addr);
		/* 3> Set pre_en to 1'b1 to turn on path. */
		ctl |= BM_CKGEN_BUS_SLICE_CTL_CG_EN_A;
		clk_writel(ctl, ctl_addr);
		count = MAX_POLL_COUNT;
		while (!(clk_readl(ctl_addr) & (BM_CKGEN_BUS_SLICE_CTL_CG_EN_A_STATUS)) && count--)
			cpu_relax();
		/* 4>	a. Set pre_div_num */
		ctl &= ~FM_CKGEN_BUS_SLICE_CTL_PRE_DIV_NUM_A;
		ctl |= FV_CKGEN_BUS_SLICE_CTL_PRE_DIV_NUM_A(pre_div);
		clk_writel(ctl, ctl_addr);
		/*		b. Check the corresponding busy bit. */
		count = MAX_POLL_COUNT;
		while ((clk_readl(ctl_addr) & BM_CKGEN_BUS_SLICE_CTL_PRE_BUSY_A) && count--)
			cpu_relax();
		if (count == 0)
			pr_err("polling fail: bus pre div busy bit\n");
	} else {
		/* A is being selected now */
		WARN_ON((ctl & BM_CKGEN_BUS_SLICE_CTL_A_B_SEL));
		/* 1> Set pre_en to 1'b0 to turn off path. */
		ctl &= ~BM_CKGEN_BUS_SLICE_CTL_CG_EN_B;
		clk_writel(ctl, ctl_addr);
		count = MAX_POLL_COUNT;
		while ((clk_readl(ctl_addr) & (BM_CKGEN_BUS_SLICE_CTL_CG_EN_B_STATUS)) && count--)
			cpu_relax();
		/* 2> Change pre_mux_sel to select clock source.*/
		ctl &= ~FM_CKGEN_BUS_SLICE_CTL_CLK_SRC_SEL_B;
		ctl |= FV_CKGEN_BUS_SLICE_CTL_CLK_SRC_SEL_B(src_sel);
		clk_writel(ctl, ctl_addr);
		/* 3> Set pre_en to 1'b1 to turn on path. */
		ctl |= BM_CKGEN_BUS_SLICE_CTL_CG_EN_B;
		clk_writel(ctl, ctl_addr);
		count = MAX_POLL_COUNT;
		while (!(clk_readl(ctl_addr) & (BM_CKGEN_BUS_SLICE_CTL_CG_EN_B_STATUS)) && count--)
			cpu_relax();
		/* 4>	a. Set pre_div_num */
		ctl &= ~FM_CKGEN_BUS_SLICE_CTL_PRE_DIV_NUM_B;
		ctl |= FV_CKGEN_BUS_SLICE_CTL_PRE_DIV_NUM_B(pre_div);
		clk_writel(ctl, ctl_addr);
		/*		b. Check the corresponding busy bit. */
		count = MAX_POLL_COUNT;
		while ((clk_readl(ctl_addr) & BM_CKGEN_BUS_SLICE_CTL_PRE_BUSY_B) && count--)
			cpu_relax();
		if (count == 0)
			pr_err("polling fail: bus pre div b busy bit\n");
	}
}

void ckgen_bus_slice_postdiv_update(void __iomem *base, u32 slice_id,
		u32 post_div)
{
	void __iomem *ctl_addr = base +
		SOC_CKGEN_REG_MAP(CKGEN_BUS_SLICE_CTL_OFF(slice_id));
	u32 ctl = clk_readl(ctl_addr);
	int count = MAX_POLL_COUNT;

	/* 6>	a. Change post_div_num */
	ctl &= ~FM_CKGEN_BUS_SLICE_CTL_POST_DIV_NUM;
	ctl |= FV_CKGEN_BUS_SLICE_CTL_POST_DIV_NUM(post_div);
	clk_writel(ctl, ctl_addr);
	/* b. Check the corresponding busy bit. */
	while ((clk_readl(ctl_addr) & BM_CKGEN_BUS_SLICE_CTL_POST_BUSY) && count--)
		cpu_relax();
	if (count == 0)
			pr_err("polling fail: bus post div b busy bit\n");
}

void ckgen_bus_slice_switch(void __iomem *base, u32 slice_id)
{
	void __iomem *ctl_addr = base +
			SOC_CKGEN_REG_MAP(CKGEN_BUS_SLICE_CTL_OFF(slice_id));
	u32 ctl = clk_readl(ctl_addr);

	/* 5> toggle pre_a_b_sel. This mux is glitch-less.
	 * Note: Both clock from path a and be should be active when switching.
	 */
	if (ctl & BM_CKGEN_BUS_SLICE_CTL_A_B_SEL)
		ctl &= ~BM_CKGEN_BUS_SLICE_CTL_A_B_SEL;
	else
		ctl |= BM_CKGEN_BUS_SLICE_CTL_A_B_SEL;

	clk_writel(ctl, ctl_addr);
}

/*
 * Core clock slice
 * Derived from Bus clk slice without pre_div.
 *
 * a_clk_in0 by default
 */

void ckgen_core_slice_cfg(void __iomem *base, u32 slice_id, u32 path,
		u32 src_sel)
{
	void __iomem *ctl_addr = base +
		SOC_CKGEN_REG_MAP(CKGEN_CORE_SLICE_CTL_OFF(slice_id));
	u32 ctl = clk_readl(ctl_addr);
	int count = MAX_POLL_COUNT;

	if (path == PATH_A) {
		/* B is being selected now */
		WARN_ON(!(ctl & BM_CKGEN_CORE_SLICE_CTL_A_B_SEL));
		/* 1> Set pre_en to 1'b0 to turn off path. */
		ctl &= ~BM_CKGEN_CORE_SLICE_CTL_CG_EN_A;
		clk_writel(ctl, ctl_addr);
		while ((clk_readl(ctl_addr) & (BM_CKGEN_CORE_SLICE_CTL_CG_EN_A_STATUS)) && count--)
			cpu_relax();
		/* 2> Change pre_mux_sel to select clock source.*/
		ctl &= ~FM_CKGEN_CORE_SLICE_CTL_CLK_SRC_SEL_A;
		ctl |= FV_CKGEN_CORE_SLICE_CTL_CLK_SRC_SEL_A(src_sel);
		clk_writel(ctl, ctl_addr);
		/* 3> Set pre_en to 1'b1 to turn on path. */
		ctl |= BM_CKGEN_CORE_SLICE_CTL_CG_EN_A;
		clk_writel(ctl, ctl_addr);
		count = MAX_POLL_COUNT;
		while (!(clk_readl(ctl_addr) & (BM_CKGEN_CORE_SLICE_CTL_CG_EN_A_STATUS)) && count--)
			cpu_relax();
	} else {
		/* A is being selected now */
		WARN_ON((ctl & BM_CKGEN_CORE_SLICE_CTL_A_B_SEL));
		/* 1> Set pre_en to 1'b0 to turn off path. */
		ctl &= ~BM_CKGEN_CORE_SLICE_CTL_CG_EN_B;
		clk_writel(ctl, ctl_addr);
		while ((clk_readl(ctl_addr) & (BM_CKGEN_CORE_SLICE_CTL_CG_EN_B_STATUS)) && count--)
			cpu_relax();
		/* 2> Change pre_mux_sel to select clock source.*/
		ctl &= ~FM_CKGEN_CORE_SLICE_CTL_CLK_SRC_SEL_B;
		ctl |= FV_CKGEN_CORE_SLICE_CTL_CLK_SRC_SEL_B(src_sel);
		clk_writel(ctl, ctl_addr);
		/* 3> Set pre_en to 1'b1 to turn on path. */
		ctl |= BM_CKGEN_CORE_SLICE_CTL_CG_EN_B;
		clk_writel(ctl, ctl_addr);
		count = MAX_POLL_COUNT;
		while (!(clk_readl(ctl_addr) & (BM_CKGEN_CORE_SLICE_CTL_CG_EN_A_STATUS)) && count--)
			cpu_relax();
	}
}

void ckgen_core_slice_postdiv_update(void __iomem *base, u32 slice_id,
		u32 post_div)
{
	void __iomem *ctl_addr = base +
			SOC_CKGEN_REG_MAP(CKGEN_CORE_SLICE_CTL_OFF(slice_id));
	u32 ctl = clk_readl(ctl_addr);
	int count = MAX_POLL_COUNT;

	/* 6>	a. Change post_div_num */
	ctl &= ~FM_CKGEN_CORE_SLICE_CTL_POST_DIV_NUM;
	ctl |= FV_CKGEN_CORE_SLICE_CTL_POST_DIV_NUM(post_div);
	clk_writel(ctl, ctl_addr);
	/*		b. Check the corresponding busy bit. */
	while ((clk_readl(ctl_addr) & BM_CKGEN_CORE_SLICE_CTL_POST_BUSY) && count--)
		cpu_relax();
	if (count == 0)
		pr_err("polling fail: core post div busy bit\n");
}

void ckgen_core_slice_switch(void __iomem *base, u32 slice_id)
{
	void __iomem *ctl_addr = base +
			SOC_CKGEN_REG_MAP(CKGEN_CORE_SLICE_CTL_OFF(slice_id));
	u32 ctl = clk_readl(ctl_addr);

	/* 5> toggle pre_a_b_sel. This mux is glitch-less.
	 * Note: Both clock from path a and be should be active when switching.
	 */
	if (ctl & BM_CKGEN_CORE_SLICE_CTL_A_B_SEL)
		ctl &= ~BM_CKGEN_CORE_SLICE_CTL_A_B_SEL;
	else
		ctl |= BM_CKGEN_CORE_SLICE_CTL_A_B_SEL;

	clk_writel(ctl, ctl_addr);
}

void ckgen_cg_en(void __iomem *base, u32 id)
{
	void __iomem *a = base + SOC_CKGEN_REG_MAP(CKGEN_LP_GATING_EN_OFF(id));
	u32 v = clk_readl(a);

	v &= ~BM_CKGEN_LP_GATING_EN_SW_GATING_DIS;
	clk_writel(v, a);
	/* SW_GATING_EN (force_en actually) is for debug purpose */
}

void ckgen_cg_dis(void __iomem *base, u32 id)
{
	void __iomem *a = base + SOC_CKGEN_REG_MAP(CKGEN_LP_GATING_EN_OFF(id));
	u32 v = readl(a);

	v |= BM_CKGEN_LP_GATING_EN_SW_GATING_DIS;
	clk_writel(v, a);
}

void ckgen_uuu_slice_cfg(void __iomem *base, u32 slice_id, u32 src_sel,
		u32 div_mnpq)
{
	void __iomem *a = base + SOC_CKGEN_REG_MAP(CKGEN_UUU_SLICE_OFF(slice_id));
	u32 v = clk_readl(a);

	WARN_ON(!((src_sel == UUU_SEL_CKGEN_SOC) || (src_sel == UUU_SEL_PLL)));

	v &= FM_CKGEN_UUU_SLICE_UUU_SEL;
	v |= FV_CKGEN_UUU_SLICE_UUU_SEL(src_sel);
	v &= ~(FM_CKGEN_UUU_SLICE_M_DIV_NUM | FM_CKGEN_UUU_SLICE_N_DIV_NUM
			| FM_CKGEN_UUU_SLICE_P_DIV_NUM | FM_CKGEN_UUU_SLICE_Q_DIV_NUM);
	v |= div_mnpq;
	clk_writel(v, a);
}
u32 ckgen_uuu_get_slice_cfg(void __iomem *base, u32 slice_id)
{
	void __iomem *a = base + SOC_CKGEN_REG_MAP(CKGEN_UUU_SLICE_OFF(slice_id));
	u32 v = clk_readl(a);
	return v;
}
