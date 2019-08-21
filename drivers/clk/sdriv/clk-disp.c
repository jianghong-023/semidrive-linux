/*
 * clk-disp.c
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
#include "clk-disp.h"

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
//intern
/*csi 1/2/3/SPARE1/dc5_alt_dsp/disp_bus*/
enum DISP_CLKIN DISP_CLK_table_MIPI_CSI123_PIX[] = {
	DISP_CLKIN_0, DISP_CLKIN_1, DISP_CLKIN_2, DISP_CLKIN_3,
	DISP_CLKIN_6, DISP_CLKIN_4
};

enum DISP_CLKIN DISP_CLK_table_DP123_DC12345[] = {
	DISP_CLKIN_0, DISP_CLKIN_1, DISP_CLKIN_2, DISP_CLKIN_3,
	DISP_CLKIN_5, DISP_CLKIN_4
};

enum DISP_CLKIN DISP_CLK_table_EXT_AUD1234[] = {
	DISP_CLKIN_0, DISP_CLKIN_1, DISP_CLKIN_2, DISP_CLKIN_3,
	DISP_CLKIN_7, DISP_CLKIN_10, DISP_CLKIN_13, DISP_CLKIN_16
};


#define SDRV_OUT_CLK(id, _tablename, _type, _slice_id, div_shift, div_width) \
{	\
	.clk_id = DISP_CLK_ ##id, \
	.type = _type,	\
	.slice_id = _slice_id, \
	.n_parents = ARRAY_SIZE(DISP_CLK_table_ ##_tablename),	\
	.div = {	\
		.shift = div_shift,	\
		.width = div_width,	\
	},	\
	.mux_table = DISP_CLK_table_ ##_tablename,	\
}

#define SDRV_OUT_GATE(id, p_id, _gate_id)	\
{	\
	.clk_id = DISP_CLK_ ##id,	\
	.type = CLK_TYPE_GATE,	\
	.gate_id = _gate_id,	\
	.parent_id = DISP_CLK_ ##p_id,	\
}

#define SDRV_INTERN_CLK_IP(id, table_name, slice_id) \
	SDRV_OUT_CLK(id, table_name, CLK_TYPE_IP, slice_id, IP_DIV_SHIFT, IP_DIV_WIDTH)


#define SDRV_INTERN_CLK_CORE(id, slice_id) \
	SDRV_OUT_CLK(id, id, CLK_TYPE_CORE, slice_id, CORE_DIV_SHIFT, CORE_DIV_WIDTH)

#define SDRV_INTERN_CLK_BUS(id, table_name, slice_id) \
		SDRV_OUT_CLK(id, table_name, CLK_TYPE_BUS, slice_id, BUS_DIV_SHIFT, BUS_DIV_WIDTH)

#define SDRV_OUT_CLK_GATE(id, p_id, gate_id)	\
		SDRV_OUT_GATE(id, p_id, gate_id)


/* SEC_CLK_INTERN_MAX-SEC_CLK_INTERN_FIRST+1 */
static struct sdrv_cgu_out_clk intern_clk[] = {
	//internal clk
	SDRV_INTERN_CLK_IP(MIPI_CSI1_PIX, MIPI_CSI123_PIX, MIPI_CSI1_PIX_IP_SLICE_TYPE_IDX),
	SDRV_INTERN_CLK_IP(MIPI_CSI2_PIX, MIPI_CSI123_PIX, MIPI_CSI2_PIX_IP_SLICE_TYPE_IDX),
	SDRV_INTERN_CLK_IP(MIPI_CSI3_PIX, MIPI_CSI123_PIX, MIPI_CSI3_PIX_IP_SLICE_TYPE_IDX),
};
/*SEC_CLK_OUT_MAX-SEC_CLK_OUT_FIRST+1*/
static struct sdrv_cgu_out_clk out_clk[] = {
	SDRV_OUT_GATE(MIPI_CSI1, MIPI_CSI1_PIX, CKGEN_DISP_CKGATE_MIPI_CSI1_INDEX),
	SDRV_OUT_GATE(CSI1, MIPI_CSI1_PIX, CKGEN_DISP_CKGATE_CSI1_INDEX),
	SDRV_OUT_GATE(MIPI_CSI2, MIPI_CSI2_PIX, CKGEN_DISP_CKGATE_MIPI_CSI2_INDEX),
	SDRV_OUT_GATE(CSI2, MIPI_CSI2_PIX, CKGEN_DISP_CKGATE_CSI2_INDEX),
	SDRV_OUT_GATE(MIPI_CSI3, MIPI_CSI3_PIX, CKGEN_DISP_CKGATE_MIPI_CSI3_INDEX),
	SDRV_OUT_GATE(CSI3, MIPI_CSI3_PIX, CKGEN_DISP_CKGATE_CSI3_INDEX),

	SDRV_INTERN_CLK_IP(DP1, DP123_DC12345, DP1_IP_SLICE_TYPE_IDX),
	SDRV_INTERN_CLK_IP(DP2, DP123_DC12345, DP2_IP_SLICE_TYPE_IDX),
	SDRV_INTERN_CLK_IP(DP3, DP123_DC12345, DP3_IP_SLICE_TYPE_IDX),

	SDRV_INTERN_CLK_IP(DC5, DP123_DC12345, DC5_IP_SLICE_TYPE_IDX),
	SDRV_INTERN_CLK_IP(DC1, DP123_DC12345, DC1_IP_SLICE_TYPE_IDX),
	SDRV_INTERN_CLK_IP(DC2, DP123_DC12345, DC2_IP_SLICE_TYPE_IDX),
	SDRV_INTERN_CLK_IP(DC3, DP123_DC12345, DC3_IP_SLICE_TYPE_IDX),
	SDRV_INTERN_CLK_IP(DC4, DP123_DC12345, DC4_IP_SLICE_TYPE_IDX),

	SDRV_INTERN_CLK_IP(SPARE1, MIPI_CSI123_PIX, SPARE1_IP_SLICE_TYPE_IDX),
	SDRV_INTERN_CLK_IP(DC5_ALT_DSP, MIPI_CSI123_PIX, DC5_ALT_DSP_IP_SLICE_TYPE_IDX),
	SDRV_INTERN_CLK_BUS(DISP_BUS, MIPI_CSI123_PIX, DISP_BUS_SLICE_TYPE_IDX),

	SDRV_INTERN_CLK_IP(EXT_AUD1, EXT_AUD1234, EXT_AUD1_IP_SLICE_TYPE_IDX),
	SDRV_INTERN_CLK_IP(EXT_AUD2, EXT_AUD1234, EXT_AUD2_IP_SLICE_TYPE_IDX),
	SDRV_INTERN_CLK_IP(EXT_AUD3, EXT_AUD1234, EXT_AUD3_IP_SLICE_TYPE_IDX),
	SDRV_INTERN_CLK_IP(EXT_AUD4, EXT_AUD1234, EXT_AUD4_IP_SLICE_TYPE_IDX),

};

static struct clk *clk_base[DISP_CLK_OUT_MAX+1];
static struct clk_onecell_data clk_base_data = {
	.clks = clk_base,
	.clk_num = (DISP_CLK_OUT_MAX+1),
};

static void __init sdrv_disp_clk_init(struct device_node *np)
{
	void __iomem *disp_reg_base;
	int i;

	disp_reg_base = of_iomap(np, 0);
	if (!disp_reg_base) {
		pr_warn("%s: failed to map address range\n", __func__);
		return;
	}
	pr_debug("reg base %p\n", disp_reg_base);
	sdrv_register_disp_plls();

	for (i = 0; i < ARRAY_SIZE(intern_clk); i++)
		clk_base[intern_clk[i].clk_id] = sdrv_register_out_composite(np,
			disp_reg_base, &intern_clk[i], disp_clk_src_names);

	for (i = 0; i < ARRAY_SIZE(out_clk); i++)
		clk_base[out_clk[i].clk_id] = sdrv_register_out_composite(np,
			disp_reg_base, &out_clk[i], disp_clk_src_names);

	of_clk_add_provider(np, of_clk_src_onecell_get, &clk_base_data);
}

CLK_OF_DECLARE(sdrv_clk, "sd,sdrv-cgu-disp", sdrv_disp_clk_init);

