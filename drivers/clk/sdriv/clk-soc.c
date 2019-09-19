/*
 * clk-soc.c
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
#include "clk-soc.h"

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
#define UUU_M_SHIFT 12
#define UUU_M_WIDTH 4
#define UUU_N_SHIFT 8
#define UUU_N_WIDTH 4
#define UUU_P_SHIFT 4
#define UUU_P_WIDTH 4
#define UUU_Q_SHIFT 0
#define UUU_Q_WIDTH 4

//intern
enum SOC_CLKIN SOC_CLK_table_CPU1A[] = {
	SOC_CLKIN_0, SOC_CLKIN_2, SOC_CLKIN_31, SOC_CLKIN_32//, SOC_CLKIN_4
};

enum SOC_CLKIN SOC_CLK_table_CPU1A_0123[] = {
	SOC_CLK_CPU1A, SOC_CLKIN_4
};

enum SOC_CLKIN SOC_CLK_table_CPU1A_m[] = {
	SOC_CLK_CPU1A_M, SOC_CLKIN_4
};


enum SOC_CLKIN SOC_CLK_table_CPU1B[] = {
	SOC_CLKIN_0, SOC_CLKIN_2, SOC_CLKIN_31, SOC_CLKIN_32//, SOC_CLKIN_7
};

enum SOC_CLKIN SOC_CLK_table_CPU1B_0123[] = {
	SOC_CLK_CPU1B, SOC_CLKIN_7
};

enum SOC_CLKIN SOC_CLK_table_CPU1B_m[] = {
	SOC_CLK_CPU1B_M, SOC_CLKIN_7
};

enum SOC_CLKIN SOC_CLK_table_CPU2[] = {
	SOC_CLKIN_0, SOC_CLKIN_2, SOC_CLKIN_31, SOC_CLKIN_32//, SOC_CLKIN_10
};

enum SOC_CLKIN SOC_CLK_table_CPU2_0123[] = {
	SOC_CLK_CPU2, SOC_CLKIN_10
};

enum SOC_CLKIN SOC_CLK_table_CPU2_m[] = {
	SOC_CLK_CPU2_M, SOC_CLKIN_10
};

enum SOC_CLKIN SOC_CLK_table_GPU1[] = {
	SOC_CLKIN_0, SOC_CLKIN_2, SOC_CLKIN_31, SOC_CLKIN_32//, SOC_CLKIN_13
};

enum SOC_CLKIN SOC_CLK_table_GPU1_0123[] = {
	SOC_CLK_GPU1, SOC_CLKIN_13
};

enum SOC_CLKIN SOC_CLK_table_GPU1_m[] = {
	SOC_CLK_GPU1_M, SOC_CLKIN_13
};

enum SOC_CLKIN SOC_CLK_table_GPU2[] = {
	SOC_CLKIN_0, SOC_CLKIN_2, SOC_CLKIN_31, SOC_CLKIN_32//, SOC_CLKIN_16
};

enum SOC_CLKIN SOC_CLK_table_GPU2_0123[] = {
	SOC_CLK_GPU2, SOC_CLKIN_16
};

enum SOC_CLKIN SOC_CLK_table_GPU2_m[] = {
	SOC_CLK_GPU2_M, SOC_CLKIN_16
};

enum SOC_CLKIN SOC_CLK_table_VPU1[] = {
	SOC_CLKIN_0, SOC_CLKIN_2, SOC_CLKIN_31, SOC_CLKIN_32//, SOC_CLKIN_21
};

enum SOC_CLKIN SOC_CLK_table_VPU1_0123[] = {
	SOC_CLK_VPU1, SOC_CLKIN_21
};

enum SOC_CLKIN SOC_CLK_table_VPU1_m[] = {
	SOC_CLK_VPU1_M, SOC_CLKIN_21
};

enum SOC_CLKIN SOC_CLK_table_MJPEG[] = {
	SOC_CLKIN_0, SOC_CLKIN_2, SOC_CLKIN_31, SOC_CLKIN_32//, SOC_CLKIN_21
};

enum SOC_CLKIN SOC_CLK_table_MJPEG_0123[] = {
	SOC_CLK_MJPEG, SOC_CLKIN_21
};

enum SOC_CLKIN SOC_CLK_table_MJPEG_m[] = {
	SOC_CLK_MJPEG_M, SOC_CLKIN_21
};

enum SOC_CLKIN SOC_CLK_table_VPU_BUS[] = {
	SOC_CLKIN_0, SOC_CLKIN_2, SOC_CLKIN_31, SOC_CLKIN_32//, SOC_CLKIN_20
};

enum SOC_CLKIN SOC_CLK_table_VPU_BUS_0123[] = {
	SOC_CLK_VPU_BUS, SOC_CLKIN_20
};

enum SOC_CLKIN SOC_CLK_table_VPU_BUS_m[] = {
	SOC_CLK_VPU_BUS_M, SOC_CLKIN_20
};

enum SOC_CLKIN SOC_CLK_table_VSN_BUS[] = {
	SOC_CLKIN_0, SOC_CLKIN_2, SOC_CLKIN_31, SOC_CLKIN_32//, SOC_CLKIN_22
};

enum SOC_CLKIN SOC_CLK_table_VSN_BUS_0123[] = {
	SOC_CLK_VSN_BUS, SOC_CLKIN_22
};

enum SOC_CLKIN SOC_CLK_table_VSN_BUS_m[] = {
	SOC_CLK_VSN_BUS_M, SOC_CLKIN_22
};

enum SOC_CLKIN SOC_CLK_table_DDR[] = {
	SOC_CLKIN_0, SOC_CLKIN_2, SOC_CLKIN_31, SOC_CLKIN_32//, SOC_CLKIN_25
};

enum SOC_CLKIN SOC_CLK_table_DDR_0123[] = {
	SOC_CLK_DDR, SOC_CLKIN_33
};

enum SOC_CLKIN SOC_CLK_table_DDR_m[] = {
	SOC_CLK_DDR_M, SOC_CLKIN_33
};

enum SOC_CLKIN SOC_CLK_table_NOC_BUS[] = {
	SOC_CLKIN_0, SOC_CLKIN_2, SOC_CLKIN_31, SOC_CLKIN_32, SOC_CLKIN_25
};

enum SOC_CLKIN SOC_CLK_table_HIS_BUS[] = {
	SOC_CLKIN_0, SOC_CLKIN_2, SOC_CLKIN_31, SOC_CLKIN_32//, SOC_CLKIN_28
};

enum SOC_CLKIN SOC_CLK_table_HIS_BUS_0123[] = {
	SOC_CLK_HIS_BUS, SOC_CLKIN_28
};

enum SOC_CLKIN SOC_CLK_table_HIS_BUS_m[] = {
	SOC_CLK_HIS_BUS_M, SOC_CLKIN_28
};


#define SOC_OUT_CLK(id, _tablename, _type, _slice_id, div_shift, div_width) \
{	\
	.clk_id = SOC_CLK_ ##id,	\
	.type = _type,	\
	.slice_id = _slice_id,	\
	.n_parents = ARRAY_SIZE(SOC_CLK_table_ ##_tablename),	\
	.div = {	\
		.shift = div_shift,	\
		.width = div_width,	\
	},	\
	.mux_table = SOC_CLK_table_ ##_tablename,	\
}

#define SOC_OUT_MUX(id, _tablename, _type, _slice_id, _gate_id)	\
{	\
	.clk_id = SOC_CLK_ ##id,	\
	.type = _type,	\
	.slice_id = _slice_id,	\
	.gate_id = _gate_id,	\
	.n_parents = ARRAY_SIZE(SOC_CLK_table_ ##_tablename),	\
	.mux_table = SOC_CLK_table_ ##_tablename,	\
}

#define SOC_OUT_DIVIDER(id, p_id, _slice_id, _gate_id, div_shift, div_width)	\
{	\
	.clk_id = SOC_CLK_ ##id,	\
	.type = CLK_TYPE_UUU_DIVIDER,	\
	.slice_id = _slice_id,	\
	.gate_id = _gate_id,	\
	.parent_id = SOC_CLK_ ##p_id,	\
	.div = {	\
		.shift = div_shift,\
		.width = div_width,\
	},	\
}

#define SOC_OUT_GATE(id, p_id, _gate_id)	\
{	\
	.clk_id = SOC_CLK_ ##id,	\
	.type = CLK_TYPE_GATE,	\
	.gate_id = _gate_id,	\
	.parent_id = SOC_CLK_ ##p_id,	\
}


#define SOC_INTERN_CLK_IP(id, slice_id)	\
	SOC_OUT_CLK(id, id, CLK_TYPE_IP, slice_id, IP_DIV_SHIFT, IP_DIV_WIDTH)


#define SOC_INTERN_CLK_CORE(id, slice_id)	\
	SOC_OUT_CLK(id, id, CLK_TYPE_CORE, slice_id, CORE_DIV_SHIFT, CORE_DIV_WIDTH)

#define SOC_INTERN_CLK_BUS(id, slice_id)	\
	SOC_OUT_CLK(id, id, CLK_TYPE_BUS, slice_id, BUS_DIV_SHIFT, BUS_DIV_WIDTH)

#define SOC_OUT_CLK_UUU_MUX(id, table_name, slice_id)	\
	SOC_OUT_MUX(id, table_name, CLK_TYPE_UUU_MUX, slice_id, -1)

#define SOC_OUT_CLK_UUU_MUX2(id, table_name, slice_id, gate_id)	\
	SOC_OUT_MUX(id, table_name, CLK_TYPE_UUU_MUX2, slice_id, gate_id)

#define SOC_OUT_CLK_UUU_DIVIDER(id, p_id, slice_id, gate_id, div_shift, div_width)	\
	SOC_OUT_DIVIDER(id, p_id, slice_id, gate_id, div_shift, div_width)

#define SOC_OUT_CLK_GATE(id, p_id, slice_id)	\
	SOC_OUT_GATE(id, p_id, slice_id)

#define SOC_OUT_CLK_UUU_COMPOSITE(id, gateid0, gateid1, gateid2, gateid3)	\
	SOC_OUT_CLK_UUU_DIVIDER(id##_M, id##_MUX, id##_UUU_SLICE_TYPE_IDX, -1, UUU_M_SHIFT, UUU_M_WIDTH),	\
	SOC_OUT_CLK_UUU_MUX2(id##_0, id##_m, id##_UUU_SLICE_TYPE_IDX, gateid0),	\
	SOC_OUT_CLK_UUU_DIVIDER(id##_1, id##_MUX, id##_UUU_SLICE_TYPE_IDX, gateid1, UUU_N_SHIFT, UUU_N_WIDTH),	\
	SOC_OUT_CLK_UUU_DIVIDER(id##_2, id##_MUX, id##_UUU_SLICE_TYPE_IDX, gateid2, UUU_P_SHIFT, UUU_P_WIDTH),	\
	SOC_OUT_CLK_UUU_DIVIDER(id##_3, id##_MUX, id##_UUU_SLICE_TYPE_IDX, gateid3, UUU_Q_SHIFT, UUU_Q_WIDTH)

/* SOC_CLK_INTERN_MAX-SOC_CLK_INTERN_FIRST+1 */
static struct sdrv_cgu_out_clk intern_clk[] = {
	//internal clk
	SOC_INTERN_CLK_CORE(CPU1A, CPU1A_CORE_SLICE_TYPE_IDX),
	SOC_OUT_CLK_UUU_MUX(CPU1A_MUX, CPU1A_0123, CPU1A_UUU_SLICE_TYPE_IDX),
	SOC_INTERN_CLK_CORE(CPU1B, CPU1B_CORE_SLICE_TYPE_IDX),
	SOC_OUT_CLK_UUU_MUX(CPU1B_MUX, CPU1B_0123, CPU1B_UUU_SLICE_TYPE_IDX),
	SOC_INTERN_CLK_CORE(CPU2, CPU2_CORE_SLICE_TYPE_IDX),
	SOC_OUT_CLK_UUU_MUX(CPU2_MUX, CPU2_0123, CPU2_UUU_SLICE_TYPE_IDX),
	SOC_INTERN_CLK_CORE(GPU1, GPU1_CORE_SLICE_TYPE_IDX),
	SOC_OUT_CLK_UUU_MUX(GPU1_MUX, GPU1_0123, GPU1_UUU_SLICE_TYPE_IDX),
	SOC_INTERN_CLK_CORE(GPU2, GPU2_CORE_SLICE_TYPE_IDX),
	SOC_OUT_CLK_UUU_MUX(GPU2_MUX, GPU2_0123, GPU2_UUU_SLICE_TYPE_IDX),
	SOC_INTERN_CLK_IP(VPU1, VPU1_IP_SLICE_TYPE_IDX),
	SOC_OUT_CLK_UUU_MUX(VPU1_MUX, VPU1_0123, VPU1_UUU_SLICE_TYPE_IDX),
	SOC_INTERN_CLK_IP(MJPEG, MJPEG_IP_SLICE_TYPE_IDX),
	SOC_OUT_CLK_UUU_MUX(MJPEG_MUX, MJPEG_0123, MJPEG_UUU_SLICE_TYPE_IDX),
	SOC_INTERN_CLK_BUS(VPU_BUS, VPU_BUS_SLICE_TYPE_IDX),
	SOC_OUT_CLK_UUU_MUX(VPU_BUS_MUX, VPU_BUS_0123, VPU_BUS_UUU_SLICE_TYPE_IDX),
	SOC_INTERN_CLK_BUS(VSN_BUS, VSN_BUS_SLICE_TYPE_IDX),
	SOC_OUT_CLK_UUU_MUX(VSN_BUS_MUX, VSN_BUS_0123, VSN_BUS_UUU_SLICE_TYPE_IDX),
	SOC_INTERN_CLK_CORE(DDR, DDR_CORE_SLICE_TYPE_IDX),
	SOC_OUT_CLK_UUU_MUX(DDR_MUX, DDR_0123, DDR_UUU_SLICE_TYPE_IDX),
	SOC_INTERN_CLK_BUS(HIS_BUS, HIS_BUS_SLICE_TYPE_IDX),
	SOC_OUT_CLK_UUU_MUX(HIS_BUS_MUX, HIS_BUS_0123, HIS_BUS_UUU_SLICE_TYPE_IDX),
};
/*SOC_CLK_OUT_MAX-SOC_CLK_OUT_FIRST+1*/
static struct sdrv_cgu_out_clk out_clk[] = {
	SOC_OUT_CLK_UUU_COMPOSITE(CPU1A, -1, CKGEN_SOC_CKGATE_CPU1_INDEX, CKGEN_SOC_CKGATE_CPU1_INDEX, -1),
	SOC_OUT_CLK_GATE(CPU1A_0_CORE0, CPU1A_0, CKGEN_SOC_CKGATE_CPU1_CORE0_INDEX),
	SOC_OUT_CLK_GATE(CPU1A_0_CORE1, CPU1A_0, CKGEN_SOC_CKGATE_CPU1_CORE1_INDEX),
	SOC_OUT_CLK_GATE(CPU1A_0_CORE2, CPU1A_0, CKGEN_SOC_CKGATE_CPU1_CORE2_INDEX),
	SOC_OUT_CLK_GATE(CPU1A_0_CORE3, CPU1A_0, CKGEN_SOC_CKGATE_CPU1_CORE3_INDEX),
	SOC_OUT_CLK_GATE(CPU1A_0_CORE4, CPU1A_0, CKGEN_SOC_CKGATE_CPU1_CORE4_INDEX),
	SOC_OUT_CLK_GATE(CPU1A_0_CORE5, CPU1A_0, CKGEN_SOC_CKGATE_CPU1_CORE5_INDEX),

	SOC_OUT_CLK_UUU_COMPOSITE(CPU1B, CKGEN_SOC_CKGATE_CPU1_INDEX, -1, -1, -1),
	SOC_OUT_CLK_UUU_COMPOSITE(CPU2, CKGEN_SOC_CKGATE_CPU2_INDEX, CKGEN_SOC_CKGATE_CPU2_INDEX, CKGEN_SOC_CKGATE_CPU2_INDEX, -1),
	SOC_OUT_CLK_UUU_COMPOSITE(GPU1, CKGEN_SOC_CKGATE_GPU1_INDEX, CKGEN_SOC_CKGATE_GPU1_INDEX, CKGEN_SOC_CKGATE_PLL_GPU1_INDEX, -1),
	SOC_OUT_CLK_UUU_COMPOSITE(GPU2, CKGEN_SOC_CKGATE_GPU2_INDEX, CKGEN_SOC_CKGATE_GPU2_INDEX, CKGEN_SOC_CKGATE_PLL_GPU2_INDEX, -1),
	SOC_OUT_CLK_UUU_COMPOSITE(VPU1, CKGEN_SOC_CKGATE_VPU1_INDEX, -1, -1, -1),
	SOC_OUT_CLK_UUU_COMPOSITE(MJPEG, CKGEN_SOC_CKGATE_MJPEG_INDEX, -1, -1, -1),
	SOC_OUT_CLK_UUU_COMPOSITE(VPU_BUS, -1, -1, -1, -1),

	SOC_OUT_CLK_GATE(VPU_BUS_0_VPU1, VPU_BUS_0, CKGEN_SOC_CKGATE_VPU1_INDEX),//vpu1
	SOC_OUT_CLK_GATE(VPU_BUS_0_VPU2, VPU_BUS_0, CKGEN_SOC_CKGATE_VPU2_INDEX),//vpu2
	SOC_OUT_CLK_GATE(VPU_BUS_0_MJPEG, VPU_BUS_0, CKGEN_SOC_CKGATE_MJPEG_INDEX),//mjpeg

	SOC_OUT_CLK_GATE(VPU_BUS_1_VPU1, VPU_BUS_1, CKGEN_SOC_CKGATE_VPU1_INDEX),//vpu1
	SOC_OUT_CLK_GATE(VPU_BUS_1_VPU2, VPU_BUS_1, CKGEN_SOC_CKGATE_VPU2_INDEX),//vpu2
	SOC_OUT_CLK_GATE(VPU_BUS_1_MJPEG, VPU_BUS_1, CKGEN_SOC_CKGATE_MJPEG_INDEX),//mjpeg

	SOC_OUT_CLK_UUU_COMPOSITE(VSN_BUS, -1, -1, -1, -1),
	SOC_OUT_CLK_GATE(VSN_BUS_0_VDSP, VSN_BUS_0, CKGEN_SOC_CKGATE_VDSP_INDEX),//vdsp
	SOC_OUT_CLK_GATE(VSN_BUS_0_BIPC_VDSP, VSN_BUS_0, CKGEN_SOC_CKGATE_BIPC_VDSP_INDEX),//bipc vdsp

	SOC_OUT_CLK_GATE(VSN_BUS_1_NOC_VSP, VSN_BUS_1, CKGEN_SOC_CKGATE_NOC_VSN_INDEX),
	SOC_OUT_CLK_GATE(VSN_BUS_1_PLL_VSP, VSN_BUS_1, CKGEN_SOC_CKGATE_PLL_VSN_INDEX),
	SOC_OUT_CLK_GATE(VSN_BUS_1_BIPC_VDSP, VSN_BUS_1, CKGEN_SOC_CKGATE_BIPC_VDSP_INDEX),
	SOC_OUT_CLK_GATE(VSN_BUS_1_EIC_VDSP, VSN_BUS_1, CKGEN_SOC_CKGATE_EIC_VSN_INDEX),

	SOC_OUT_CLK_UUU_COMPOSITE(DDR, -1, -1, -1, -1),

	SOC_INTERN_CLK_BUS(NOC_BUS, NOC_BUS_SLICE_TYPE_IDX),

	SOC_OUT_CLK_UUU_COMPOSITE(HIS_BUS, -1, CKGEN_SOC_CKGATE_PCIE_REF_INDEX, -1, -1),
	SOC_OUT_CLK_GATE(HIS_BUS_2_NOC_HIS, HIS_BUS_2, CKGEN_SOC_CKGATE_NOC_HIS_INDEX),
	SOC_OUT_CLK_GATE(HIS_BUS_2_PCIE2, HIS_BUS_2, CKGEN_SOC_CKGATE_PCIEX2_INDEX),
	SOC_OUT_CLK_GATE(HIS_BUS_2_PCIE1, HIS_BUS_2, CKGEN_SOC_CKGATE_PCIEX1_INDEX),
	SOC_OUT_CLK_GATE(HIS_BUS_2_USB1, HIS_BUS_2, CKGEN_SOC_CKGATE_USB1_INDEX),
	SOC_OUT_CLK_GATE(HIS_BUS_2_USB2, HIS_BUS_2, CKGEN_SOC_CKGATE_USB2_INDEX),
	SOC_OUT_CLK_GATE(HIS_BUS_3_NOC_HIS, HIS_BUS_3, CKGEN_SOC_CKGATE_NOC_HIS_INDEX),
	SOC_OUT_CLK_GATE(HIS_BUS_3_PCIE2, HIS_BUS_3, CKGEN_SOC_CKGATE_PCIEX2_INDEX),
	SOC_OUT_CLK_GATE(HIS_BUS_3_PCIE1, HIS_BUS_3, CKGEN_SOC_CKGATE_PCIEX1_INDEX),
	SOC_OUT_CLK_GATE(HIS_BUS_3_USB1, HIS_BUS_3, CKGEN_SOC_CKGATE_USB1_INDEX),
	SOC_OUT_CLK_GATE(HIS_BUS_3_USB2, HIS_BUS_3, CKGEN_SOC_CKGATE_USB2_INDEX),
	SOC_OUT_CLK_GATE(HIS_BUS_3_PCIE_PHY, HIS_BUS_3, CKGEN_SOC_CKGATE_PCIEPHY_INDEX),

};

static struct clk *clk_base[SOC_CLK_OUT_MAX+1];
static struct clk_onecell_data clk_base_data = {
	.clks = clk_base,
	.clk_num = (SOC_CLK_OUT_MAX+1),
};

static void __init sdrv_clk_init(struct device_node *np)
{
	void __iomem *soc_reg_base;
	int i;

	soc_reg_base = of_iomap(np, 0);
	if (!soc_reg_base) {
		pr_warn("%s: failed to map address range\n", __func__);
		return;
	}
	pr_debug("reg base %p\n", soc_reg_base);
	sdrv_register_soc_plls(clk_base);

	for (i = 0; i < ARRAY_SIZE(intern_clk); i++)
		clk_base[intern_clk[i].clk_id] = sdrv_register_out_composite(np,
			soc_reg_base, &intern_clk[i], soc_clk_src_names);

	for (i = 0; i < ARRAY_SIZE(out_clk); i++)
		clk_base[out_clk[i].clk_id] = sdrv_register_out_composite(np,
			soc_reg_base, &out_clk[i], soc_clk_src_names);

	of_clk_add_provider(np, of_clk_src_onecell_get, &clk_base_data);
}

CLK_OF_DECLARE(sdrv_clk, "sd,sdrv-cgu-soc", sdrv_clk_init);

