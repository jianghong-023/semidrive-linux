/*
 * sc_pfpll.c
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
#include "sc_pfpll.h"
#include "sc_pfpll_reg.h"
#include <linux/clk.h>
#include <linux/clk-provider.h>

#define FBDIV_MIN   16
#define FBDIV_MAX   640
#define FBDIV_MIN_IN_FRAC_MODE  20
#define FBDIV_MAX_IN_FRAC_MODE  320
#define REFDIV_MIN  1
#define REFDIV_MAX  63
#define POSTDIV1_MIN    1
#define POSTDIV1_MAX    7
#define POSTDIV2_MIN    1
#define POSTDIV2_MAX    7

#define VCO_FREQ_MIN    (800*1000*1000U)
#define VCO_FREQ_MAX    (3200*1000*1000U)

/*
 * PLLs sourced from external OSC (24MHz) in ATB
 *
 * The PLLs involved in ATB are in integer mode.
 */

/*
 * Fout = (Fref/RefDiv)*(FbDiv+(Frac/2^24))/PostDiv
 *  PostDiv = PostDiv1 * PostDiv2
 *
 * Something shall be followed:
 *  1) PostDiv1 >= PostDiv2
 *  2) 800MHz <= VCO_FREQ <= 3200MHz
 *  3) 16 <= FBDIV <= 640 for INT mode
 *  4) 20 <= FBDIV <= 320 for Frac mode
 *
 *  There are four divs (A/B/C/D, in pll wrapper). Their default values are
 *  properly set per integration requirement. Usually, there is no need for
 *  ATB to change them.
 */
/**
 * @para
 *      freq    the frequency on which PLL to be programmed to run, in Hz.
 */
void sc_pfpll_setparams(void __iomem *base, unsigned int fbdiv, unsigned int refdiv,
	unsigned int postdiv1, unsigned int postdiv2, unsigned long frac,
	int diva, int divb, int divc, int divd, int isint)
{
	void __iomem *b = base;
	u32 ctrl = clk_readl(b + PLL_CTRL_OFF);
	u32 div = clk_readl(b + PLL_DIV_OFF);
	u32 v = 0;
	u32 freq_vco = 0;

	WARN_ON(!((refdiv >= REFDIV_MIN) && (refdiv <= REFDIV_MAX)));
	WARN_ON(!((fbdiv >= FBDIV_MIN) && (fbdiv <= FBDIV_MAX)));

	freq_vco = (SOC_PLL_REFCLK_FREQ/refdiv) * fbdiv;
	WARN_ON(!((freq_vco >= VCO_FREQ_MIN) && (freq_vco <= VCO_FREQ_MAX)));

/* disable pll, gate-off outputs, power-down postdiv */
/* the clock gates need to be off to protect the divs behind them from
 * being malfunctioned by glitch.
 * Once div been feed (i.e, the vco is stable), the div value can be
 * updated on the fly.
 */
	ctrl &= ~BM_PLL_CTRL_PLLEN;
	ctrl &= ~(BM_PLL_CTRL_PLL_DIVD_CG_EN | BM_PLL_CTRL_PLL_DIVC_CG_EN
		| BM_PLL_CTRL_PLL_DIVB_CG_EN | BM_PLL_CTRL_PLL_DIVA_CG_EN
		| BM_PLL_CTRL_PLLPOSTCG_EN | BM_PLL_CTRL_FOUTPOSTDIVEN
		| BM_PLL_CTRL_BYPASS);
	clk_writel(ctrl, b + PLL_CTRL_OFF);
	clk_writel(fbdiv, b + PLL_FBDIV_OFF);
/* update refdiv. */
	div &= ~FM_PLL_DIV_REFDIV;
	div |= FV_PLL_DIV_REFDIV(refdiv);
	clk_writel(div, b + PLL_DIV_OFF);
	/*  frac */
	if (isint) {
/* Integer mode. */
		ctrl = clk_readl(b + PLL_CTRL_OFF);
		ctrl &= ~BM_PLL_CTRL_INT_MODE;
		ctrl |= 1 << BS_PLL_CTRL_INT_MODE;
		clk_writel(ctrl, b + PLL_CTRL_OFF);
	} else {
/* Fractional mode. */
		ctrl = clk_readl(b + PLL_CTRL_OFF);
		ctrl &= ~BM_PLL_CTRL_INT_MODE;
		clk_writel(ctrl, b + PLL_CTRL_OFF);

/* Configure fbdiv fractional part.*/
		v = clk_readl(b + PLL_FRAC_OFF);
		v &= ~FM_PLL_FRAC_FRAC;
		v |= FV_PLL_FRAC_FRAC(frac);
		clk_writel(v, b + PLL_FRAC_OFF);
	}

/* enable PLL, VCO starting... */
	ctrl |= BM_PLL_CTRL_PLLEN;
	clk_writel(ctrl, b + PLL_CTRL_OFF);
/* max lock time: 250 input clock cycles for freqency lock and 500 cycles
 * for phase lock. For fref=25MHz, REFDIV=1, locktime is 5.0us and 10us
 */
	while (!(clk_readl(b + PLL_CTRL_OFF) & BM_PLL_CTRL_LOCK));
	/* power up and update post div */
	ctrl |= BM_PLL_CTRL_FOUTPOSTDIVEN;
	clk_writel(ctrl, b + PLL_CTRL_OFF);
	div &= ~(FM_PLL_DIV_POSTDIV1 | FM_PLL_DIV_POSTDIV2);
	div |= FV_PLL_DIV_POSTDIV1(postdiv1) | FV_PLL_DIV_POSTDIV2(postdiv2);
	clk_writel(div, b + PLL_DIV_OFF);

	v = clk_readl(b + PLL_OUT_DIV_1_OFF);
	v &= ~(FM_PLL_OUT_DIV_1_DIV_NUM_A | FM_PLL_OUT_DIV_1_DIV_NUM_B);
	v |= FV_PLL_OUT_DIV_1_DIV_NUM_A(diva) | FV_PLL_OUT_DIV_1_DIV_NUM_B(divb);
	clk_writel(v, b + PLL_OUT_DIV_1_OFF);

	v = clk_readl(b + PLL_OUT_DIV_2_OFF);
	v &= ~(FM_PLL_OUT_DIV_2_DIV_NUM_C | FM_PLL_OUT_DIV_2_DIV_NUM_D);
	v |= FV_PLL_OUT_DIV_2_DIV_NUM_C(divc) | FV_PLL_OUT_DIV_2_DIV_NUM_D(divd);
	clk_writel(v, b + PLL_OUT_DIV_2_OFF);

/*No need to touch divA/B/C/D which default
 * value had been selected properly when being integarated.
 */
/* these DIVs and CGs is outside sc pll IP, they are in a wrapper */
	ctrl |= BM_PLL_CTRL_PLL_DIVD_CG_EN | BM_PLL_CTRL_PLL_DIVC_CG_EN
		| BM_PLL_CTRL_PLL_DIVB_CG_EN | BM_PLL_CTRL_PLL_DIVA_CG_EN
		| BM_PLL_CTRL_PLLPOSTCG_EN;
	clk_writel(ctrl, b + PLL_CTRL_OFF);
}

void sc_pfpll_getparams(void __iomem *base, unsigned int *fbdiv, unsigned int *refdiv,
	unsigned int *postdiv1, unsigned int *postdiv2, unsigned long *frac,
	int *diva, int *divb, int *divc, int *divd, int *isint)
{
	void __iomem *b = base;
	u32 v = clk_readl(b + PLL_CTRL_OFF);

	*isint = ((v & BM_PLL_CTRL_INT_MODE) >> BS_PLL_CTRL_INT_MODE);

	v = clk_readl(b + PLL_FBDIV_OFF);
	*fbdiv = GFV_PLL_FBDIV_FBDIV(v);

	v = clk_readl(b + PLL_DIV_OFF);
	*refdiv = GFV_PLL_DIV_REFDIV(v);
	*postdiv1 = GFV_PLL_DIV_POSTDIV1(v);
	*postdiv2 = GFV_PLL_DIV_POSTDIV2(v);

	v = clk_readl(b + PLL_FRAC_OFF);
	*frac = GFV_PLL_FRAC_FRAC(v);

	v = clk_readl(b + PLL_OUT_DIV_1_OFF);
	*diva = GFV_PLL_OUT_DIV_1_DIV_NUM_A(v);
	*divb = GFV_PLL_OUT_DIV_1_DIV_NUM_B(v);

	v = clk_readl(b + PLL_OUT_DIV_2_OFF);
	*divc = GFV_PLL_OUT_DIV_2_DIV_NUM_C(v);
	*divd = GFV_PLL_OUT_DIV_2_DIV_NUM_D(v);

}
