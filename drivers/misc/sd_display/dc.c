/*
 *  Copyright (c) Semidrive
 */

#include <linux/printk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include "include/sd_display.h"

#include "include/dc_reg_cfg_fpgademo.h"
#include "include/dc_reg.h"
#include  "include/res/rgb888.h"

//extern uint32_t get_l_addr(addr_t addr);
//extern uint32_t get_h_addr(addr_t addr);

#define IS_RDMA(x)                       (RDMA_MASK & x) ? 1:0
#define IS_RLE(x)                        (RLE_MASK & x) ? 1:0
#define IS_MLC(x)                        (MLC_MASK & x) ? 1:0
#define IS_TCON_COMPLETE(x)              (TCON_EOF_MASK & x) ? 1:0
#define IS_TCON_UNDERRUN(x)              (TCON_UNDERRUN_MASK & x) ? 1:0
#define IS_DC_UNDERRUN(x)                (DC_UNDERRUN_MASK & x) ? 1:0

uint32_t dc_triggle_type = 0;

uint32_t path_zorder[5] = {0, 1, 2, 3, 4};

uint32_t csc_param[4][15] = {
    {/*YCbCr to RGB*/
        0x094F, 0x0000, 0x0CC4, 0x094F,
        0x3CE0, 0x397F, 0x094F, 0x1024,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0040, 0x0200, 0x0200
    },
    {/*YUV to RGB*/
        0x0800, 0x0000, 0x091E, 0x0800,
        0x7CD8, 0x7B5B, 0x0800, 0x1041,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000
    },
    {/*RGB to YCbCr*/
        0x0000
    },
    {/*RGB to YUV*/
        0x0000
    }
};

#if WITH_LL_DEBUG
#define dc_writel(val, reg) writel(val, reg);\
	pr_info("w(0x%lx, 0x%08x), r(0x%08x)\n", reg, val, readl(reg))
#else
#define dc_writel(val, reg) writel(val, reg)
#endif

static int dc_config_update(struct dc_device *dev);

uint32_t get_l_addr(uint64_t addr, uint8_t vm_flag)
{
    uint32_t addr_l;
    uint64_t baddr;

    if (vm_flag) {
        baddr = (uint64_t) virt_to_phys((void *)addr);
    } else {
        baddr = addr;
    }

    addr_l = (uint32_t)(baddr & 0x00000000FFFFFFFF);
    return addr_l;
}

uint32_t get_h_addr(uint64_t addr, uint8_t vm_flag)
{
    uint32_t addr_h;
    uint64_t baddr;

    if (vm_flag) {
        baddr = (uint64_t) virt_to_phys((void *)addr);
    } else {
        baddr = addr;
    }

    addr_h = (uint32_t)((baddr & 0xFFFFFFFF00000000) >> 32);
    return addr_h;
}

static void dc_clk_enable(struct dc_device *dev)
{
	pr_info("enable dc clks\n");
	/* pix clk */
	/* axi_clk */
	/* dsp_clk */
	/* dp_pix_clk */
}

static void dc_sw_reset(struct dc_device *dev)
{
	ulong reg = reg_addr(dev->reg_base, DC_CTRL);

	pr_info("dc_sw_reset, base=0x%lx, reg:0x%lx\n", dev->reg_base, reg);

	uint32_t val;
#if 0
	val = reg_value(1,
			readl(reg),
			DC_CTRL_SW_RST_SHIFT,
			DC_CTRL_SW_RST_MASK);

	dc_writel(val, reg);

	udelay(1);

	val = reg_value(0,
			readl(reg),
			DC_CTRL_SW_RST_SHIFT,
			DC_CTRL_SW_RST_MASK);

	dc_writel(val, reg);
#endif

	val = reg_value(1,
			readl(reg),
			DC_CTRL_UNDERRUN_CLR_MODE_SHIFT,
			DC_CTRL_UNDERRUN_CLR_MODE_MASK);

	dc_writel(val, reg);
}

#if 1
/* mask all interrupts */
static void dc_int_disable(struct dc_device *dev)
{
	ulong reg ;
	uint32_t val;

	pr_info("DC_INT disable\n");
	reg = reg_addr(dev->reg_base, DC_INT_MASK);
	val = readl(reg);
	val = reg_value(0xFFFFF, val, TCON_LAYER_KICK_SHIFT, TCON_LAYER_KICK_MASK);
	val = reg_value(1, val, SDMA_DONE_SHIFT, SDMA_DONE_MASK);
	val = reg_value(1, val, DC_UNDERRUN_SHIFT, DC_UNDERRUN_MASK);
	val = reg_value(1, val, TCON_UNDERRUN_SHIFT, TCON_UNDERRUN_MASK);
	val = reg_value(1, val, TCON_EOF_SHIFT, TCON_EOF_MASK);
	val = reg_value(1, val, TCON_SOF_SHIFT, TCON_SOF_MASK);
	val = reg_value(1, val, MLC_SHIFT, MLC_MASK);
	val = reg_value(1, val, RLE_SHIFT, RLE_MASK);
	val = reg_value(1, val, RDMA_SHIFT, RDMA_MASK);
	dc_writel(val, reg);

	pr_info("DC_SF_INT init\n");
	reg = reg_addr(dev->reg_base, DC_SF_INT_MASK);
	val = readl(reg);
	val = reg_value(0xFFFFF, val, TCON_LAYER_KICK_SHIFT, TCON_LAYER_KICK_MASK);
	val = reg_value(1, val, SDMA_DONE_SHIFT, SDMA_DONE_MASK);
	val = reg_value(1, val, DC_UNDERRUN_SHIFT, DC_UNDERRUN_MASK);
	val = reg_value(1, val, TCON_UNDERRUN_SHIFT, TCON_UNDERRUN_MASK);
	val = reg_value(1, val, TCON_EOF_SHIFT, TCON_EOF_MASK);
	val = reg_value(1, val, TCON_SOF_SHIFT, TCON_SOF_MASK);
	val = reg_value(1, val, MLC_SHIFT, MLC_MASK);
	val = reg_value(1, val, RLE_SHIFT, RLE_MASK);
	val = reg_value(1, val, RDMA_SHIFT, RDMA_MASK);
	dc_writel(val, reg);
}
#endif

static void dc_int_init(struct dc_device *dev)
{
	ulong reg ;
	uint32_t val;

	pr_info("DC_INT init\n");
	reg = reg_addr(dev->reg_base, DC_INT_MASK);
	val = readl(reg);
	val = reg_value(DC_UNDERRUN, val, DC_UNDERRUN_SHIFT, DC_UNDERRUN_MASK);
	val = reg_value(TCON_SOF, val, TCON_SOF_SHIFT, TCON_SOF_MASK);
	val = reg_value(TCON_EOF, val, TCON_EOF_SHIFT, TCON_EOF_MASK);
	val = reg_value(TCON_UNDERRUN, val, TCON_UNDERRUN_SHIFT, TCON_UNDERRUN_MASK);
	val = reg_value(RDMA, val, RDMA_SHIFT, RDMA_MASK);
	val = reg_value(RLE, val, RLE_SHIFT, RLE_MASK);
	val = reg_value(MLC, val, MLC_SHIFT, MLC_MASK);
	dc_writel(val, reg);

	pr_info("DC_SF_INT init\n");
	reg = reg_addr(dev->reg_base, DC_SF_INT_MASK);
	val = readl(reg);
	val = reg_value(SF_DC_UNDERRUN, val, DC_UNDERRUN_SHIFT, DC_UNDERRUN_MASK);
	val = reg_value(SF_TCON_SOF, val, TCON_SOF_SHIFT, TCON_SOF_MASK);
	val = reg_value(SF_TCON_EOF, val, TCON_EOF_SHIFT, TCON_EOF_MASK);
	val = reg_value(SF_TCON_UNDERRUN, val, TCON_UNDERRUN_SHIFT, TCON_UNDERRUN_MASK);
	val = reg_value(SF_RDMA, val, RDMA_SHIFT, RDMA_MASK);
	val = reg_value(SF_RLE, val, RLE_SHIFT, RLE_MASK);
	val = reg_value(SF_MLC, val, MLC_SHIFT, MLC_MASK);
	dc_writel(val, reg);
}


static void dc_tcon_init(struct dc_device *dev)
{
	ulong reg;
	uint32_t val;
	int i;

	pr_info("init TCON\n");
	/* TCON_H_PARA_1 */
	reg = reg_addr(dev->reg_base, TCON_H_PARA_1);
	val = readl(reg);
	val = reg_value(TCON_HACT, val, TCON_HACT_SHIFT, TCON_HACT_MASK);
	val = reg_value(TCON_HTOL, val, TCON_HTOL_SHIFT, TCON_HTOL_MASK);
	dc_writel(val, reg);

	/* TCON_H_PARA_2 */
	reg = reg_addr(dev->reg_base, TCON_H_PARA_2);
	val = readl(reg);
	val = reg_value(TCON_HSBP, val, TCON_HSBP_SHIFT, TCON_HSBP_MASK);
	val = reg_value(TCON_HSYNC, val, TCON_HSYNC_SHIFT, TCON_HSYNC_MASK);
	dc_writel(val, reg);

	/* TCON_V_PARA_1 */
	reg = reg_addr(dev->reg_base, TCON_V_PARA_1);
	val = readl(reg);
	val = reg_value(TCON_VACT, val, TCON_VACT_SHIFT, TCON_VACT_MASK);
	val = reg_value(TCON_VTOL, val, TCON_VTOL_SHIFT, TCON_VTOL_MASK);
	dc_writel(val, reg);

	/* TCON_V_PARA_2 */
	reg = reg_addr(dev->reg_base, TCON_V_PARA_2);
	val = readl(reg);
	val = reg_value(TCON_VSBP, val, TCON_VSBP_SHIFT, TCON_VSBP_MASK);
	val = reg_value(TCON_VSYNC, val, TCON_VSYNC_SHIFT, TCON_VSYNC_MASK);
	dc_writel(val, reg);

	/* TCON_CTRL */
	reg = reg_addr(dev->reg_base, TCON_CTRL);
	val = readl(reg);
	val = reg_value(TCON_PIX_SCR, val, TCON_PIX_SCR_SHIFT, TCON_PIX_SCR_MASK);
	val = reg_value(TCON_DSP_CLK_EN, val, TCON_DSP_CLK_EN_SHIFT, TCON_DSP_CLK_EN_MASK);
	val = reg_value(TCON_DSP_CLK_POL, val, TCON_DSP_CLK_POL_SHIFT, TCON_DSP_CLK_POL_MASK);
	val = reg_value(TCON_DE_POL, val, TCON_DE_POL_SHITF, TCON_DE_POL_MASK);
	val = reg_value(TCON_VSYNC_POL, val, TCON_VSYNC_POL_SHIFT, TCON_VSYNC_POL_MASK);
	val = reg_value(TCON_HSYNC_POL, val, TCON_HSYNC_POL_SHIFT, TCON_HSYNC_POL_MASK);
	dc_writel(val, reg);

	/* TCON_LAYER_KICK_COOR_i */
	for (i = 0; i < KICK_LAYER_COUNT; i++) {
		reg = reg_addr(dev->reg_base, TCON_LAYER_KICK_COOR_(i));
		val = readl(reg);
		val = reg_value(TCON_LAYER_KICK_Y[i], val, TCON_LAYER_KICK_Y_SHIFT, TCON_LAYER_KICK_Y_MASK);
		val = reg_value(TCON_LAYER_KICK_X[i], val, TCON_LAYER_KICK_X_SHIFT, TCON_LAYER_KICK_X_MASK);
		dc_writel(val, reg);
	}

	/* TCON_LAYER_KICK_EN_i */
	for (i = 0; i < KICK_LAYER_COUNT; i++) {
		reg = reg_addr(dev->reg_base, TCON_LAYER_KICK_EN_(i));
		val = readl(reg);
		val = reg_value(TCON_LAYER_KICK_EN[i], val, TCON_LAYER_KICK_EN_SHIFT, TCON_LAYER_KICK_EN_MASK);
		dc_writel(val, reg);
	}

	pr_info("TCON Trigger\n");
	reg = reg_addr(dev->reg_base, DC_FLC_CTRL);
	val = readl(reg);
	val =	 reg_value(TCON_TRIG, val, TCON_TRIG_SHIFT, TCON_TRIG_MASK);
	dc_writel(val, reg);
}

static void dc_rdma_init(struct dc_device *dev)
{
	ulong reg;
	uint32_t val;
	int i;

	pr_info("init RDMA\n");
	/* RDMA_DFIFO_WML_i */
	for (i = 0; i < CHN_COUNT; i++) {
		reg = reg_addr(dev->reg_base, RDMA_DFIFO_WML_(i));
		val = reg_value(RDMA_DFIFO_WML[i], readl(reg), RDMA_DFIFO_WML_SHIFT, RDMA_DFIFO_WML_MASK);
		dc_writel(val, reg);
		/* S_RDMA */
		reg = reg_addr(dev->reg_base, S_RDMA_DFIFO_WML_(i));
		dc_writel(val, reg);
	}

	/* RDMA_DFIFO_DEPTH_i */
	for (i = 0; i < CHN_COUNT; i++) {
		reg = reg_addr(dev->reg_base, RDMA_DFIFO_DEPTH_(i));
		val = reg_value(RDMA_DFIFO_DEPTH[i], readl(reg), RDMA_DFIFO_DEPTH_SHIFT, RDMA_DFIFO_DEPTH_MASK);
		dc_writel(val, reg);
		/* S_RDMA */
		reg = reg_addr(dev->reg_base, S_RDMA_DFIFO_DEPTH_(i));
		dc_writel(val, reg);
	}

	/* RDMA_CFIFO_DEPTH_i */
	for (i = 0; i < CHN_COUNT; i++) {
		reg = reg_addr(dev->reg_base, RDMA_CFIFO_DEPTH_(i));
		val = reg_value(RDMA_CFIFO_DEPTH[i], readl(reg), RDMA_CFIFO_DEPTH_SHIFT, RDMA_CFIFO_DEPTH_MASK);
		dc_writel(val, reg);
		/* S_RDMA */
		reg = reg_addr(dev->reg_base, S_RDMA_CFIFO_DEPTH_(i));
		dc_writel(val, reg);
	}

	/* RDMA_CH_PRIO_i */
	for (i = 0; i < CHN_COUNT; i++) {
		reg = reg_addr(dev->reg_base, RDMA_CH_PRIO_(i));
		val = readl(reg);
		val = reg_value(RDMA_CH_PRIO_SCHE[i], val, RDMA_CH_PRIO_SCHE_SHIFT, RDMA_CH_PRIO_SCHE_MASK);
		val = reg_value(RDMA_CH_PRIO_P1[i], val, RDMA_CH_PRIO_P1_SHIFT, RDMA_CH_PRIO_P1_MASK);
		val = reg_value(RDMA_CH_PRIO_P0[i], val, RDMA_CH_PRIO_P0_SHIFT, RDMA_CH_PRIO_P0_MASK);
		dc_writel(val, reg);
		/* S_RDMA */
		reg = reg_addr(dev->reg_base, S_RDMA_CH_PRIO_(i));
		dc_writel(val, reg);
	}

	/* RDMA_BURST_i */
	for (i = 0; i < CHN_COUNT; i++) {
		reg = reg_addr(dev->reg_base, RDMA_BURST_(i));
		val = readl(reg);
		val = reg_value(RDMA_BURST_MODE[i], val, RDMA_BURST_MODE_SHIFT, RDMA_BURST_MODE_MASK);
		val = reg_value(RDMA_BURST_LEN[i], val, RDMA_BURST_LEN_SHIFT, RDMA_BURST_LEN_MASK);
		dc_writel(val, reg);
		/* S_RDMA */
		reg = reg_addr(dev->reg_base, S_RDMA_BURST_(i));
		dc_writel(val, reg);
	}

	/* rdma_INT_MASK */
	do {
		reg = reg_addr(dev->reg_base, RDMA_INT_MASK);
		val = readl(reg);
		//val = reg_value(RDMA_CH_6, val, RDMA_CH_6_SHIFT, RDMA_CH_6_MASK);
		//val = reg_value(RDMA_CH_5, val, RDMA_CH_5_SHIFT, RDMA_CH_5_MASK);
		//val = reg_value(RDMA_CH_4, val, RDMA_CH_4_SHIFT, RDMA_CH_4_MASK);
		val = reg_value(RDMA_CH_3, val, RDMA_CH_3_SHIFT, RDMA_CH_3_MASK);
		val = reg_value(RDMA_CH_2, val, RDMA_CH_2_SHIFT, RDMA_CH_2_MASK);
		val = reg_value(RDMA_CH_1, val, RDMA_CH_1_SHIFT, RDMA_CH_1_MASK);
		val = reg_value(RDMA_CH_0, val, RDMA_CH_0_SHIFT, RDMA_CH_0_MASK);
		dc_writel(val, reg);
		/* S_RDMA */
		reg = reg_addr(dev->reg_base, S_RDMA_INT_MASK);
		dc_writel(val, reg);
	} while(0);

	/* RDMA Trigger */
	pr_info("write RDMA_CTRL.cfg_load\n");
	reg = reg_addr(dev->reg_base, RDMA_CTRL);
	val = readl(reg);
	val = reg_value(RDMA_CTRL_CFG_LOAD, val, RDMA_CTRL_CFG_LOAD_SHIFT, RDMA_CTRL_CFG_LOAD_MASK);
	val = reg_value(RDMA_CTRL_ARB_SEL, val, RDMA_CTRL_ARB_SEL_SHIFT, RDMA_CTRL_ARB_SEL_MASK);
	dc_writel(val, reg);
	/* S_RDMA */
	reg = reg_addr(dev->reg_base, S_RDMA_CTRL);
	dc_writel(val, reg);
}

static void sp_sdw_trig(struct dc_device *dev)
{
	ulong reg;
	uint32_t val;
	int i;

	for (i = 0; i < SP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, SP_SDW_CTRL_(i));
		val = readl(reg);
		val = reg_value(SP_SDW_CTRL_TRIG[i], val, SP_SDW_CTRL_TRIG_SHIFT, SP_SDW_CTRL_TRIG_MASK);
		dc_writel(val, reg);
	}
}

static void dc_sp_init(struct dc_device *dev)
{
	ulong reg;
	uint32_t val;
	int i;

	/* SP  */
	/* DC_SP_PIX_COMP_ */
	for (i = 0; i < SP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_SP_PIX_COMP_(i));
		val = readl(reg);
		val = reg_value(SP_BPV[i], val, BPV_SHIFT, BPV_MASK);
		val = reg_value(SP_BPU[i], val, BPU_SHIFT, BPU_MASK);
		val = reg_value(SP_BPY[i], val, BPY_SHIFT, BPY_MASK);
		val = reg_value(SP_BPA[i], val, BPA_SHIFT, BPA_MASK);
		dc_writel(val, reg);
	}
	/* DC_SP_FRM_CTRL_ */
	for (i = 0; i < SP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_SP_FRM_CTRL_(i));
		val = readl(reg);
		val = reg_value(SP_ENDIAN_CTRL[i], val, ENDIAN_CTRL_SHIFT, ENDIAN_CTRL_MASK);
		val = reg_value(SP_COMP_SWAP[i], val, COMP_SWAP_SHIFT, COMP_SWAP_MASK);
		val = reg_value(SP_ROT[i], val, ROT_SHIFT, ROT_MASK);
		val = reg_value(SP_RGB_YUV[i], val, RGB_YUV_SHIFT, RGB_YUV_MASK);
		val = reg_value(SP_UV_SWAP[i], val, UV_SWAP_SHIFT, UV_SWAP_MASK);
		val = reg_value(SP_UV_MODE[i], val, UV_MODE_SHIFT, UV_MODE_MASK);
		val = reg_value(SP_MODE[i], val, MODE_SHIFT, MODE_MASK);
		val = reg_value(SP_FMT[i], val, FMT_SHIFT, FMT_MASK);
		dc_writel(val, reg);
	}
	/* DC_SP_FRM_SIZE_ */
	for (i = 0; i < SP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_SP_FRM_SIZE_(i));
		val = readl(reg);
		val = reg_value(SP_FRM_HEIGHT[i], val, FRM_HEIGHT_SHIFT, FRM_HEIGHT_MASK);
		val = reg_value(SP_FRM_WIDTH[i], val, FRM_WIDTH_SHIFT, FRM_WIDTH_MASK);
		dc_writel(val, reg);
	}
	/* DC_SP_Y_BADDR_L_ */
	for (i = 0; i < SP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_SP_Y_BADDR_L_(i));
		val = readl(reg);
		val = reg_value(SP_BADDR_L_Y[i], val, BADDR_L_Y_SHIFT, BADDR_L_Y_MASK);
		dc_writel(val, reg);
	}
	/* DC_SP_Y_BADDR_H_ */
	for (i = 0; i < SP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_SP_Y_BADDR_H_(i));
		val = readl(reg);
		val = reg_value(SP_BADDR_H_Y[i], val, BADDR_H_Y_SHIFT, BADDR_H_Y_MASK);
		dc_writel(val, reg);
	}
	/* DC_SP_Y_STRIDE_ */
	for (i = 0; i < SP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_SP_Y_STRIDE_(i));
		val = readl(reg);
		val = reg_value(SP_STRIDE_Y[i], val, STRIDE_Y_SHIFT, STRIDE_Y_MASK);
		dc_writel(val, reg);
	}
	/* DC_SP_FRM_OFFSET_ */
	for (i = 0; i < SP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_SP_FRM_OFFSET_(i));
		val = readl(reg);
		val = reg_value(SP_FRM_Y[i], val, FRM_Y_SHIFT, FRM_Y_MASK);
		val = reg_value(SP_FRM_X[i], val, FRM_X_SHIFT, FRM_X_MASK);
		dc_writel(val, reg);
	}
}

static void dc_rle_init(struct dc_device *dev)
{
	ulong reg;
	uint32_t val;
	int i;

	/* RLE init*/
	/* RLE_Y_LEN_ */
	for (i = 0; i < SP_COUNT; i++) {
		reg = reg_addr(dev->reg_base, RLE_Y_LEN_(i));
		val = readl(reg);
		val = reg_value(RLE_Y_LEN_Y[i], val, RLE_Y_LEN_Y_SHIFT, RLE_Y_LEN_Y_MASK);
		dc_writel(val, reg);
	}
	/* RLE_Y_CHECK_SUM_ */
	for (i = 0; i < SP_COUNT; i++) {
		reg = reg_addr(dev->reg_base, RLE_Y_CHECK_SUM_(i));
		val = readl(reg);
		val = reg_value(RLE_Y_CHECK_SUM_Y[i], val, RLE_Y_CHECK_SUM_Y_SHIFT, RLE_Y_CHECK_SUM_Y_MASK);
		dc_writel(val, reg);
	}
	/*  RLE_Y_CHECK_SUM_ST_*/
	for (i = 0; i < SP_COUNT; i++) {
		reg = reg_addr(dev->reg_base, RLE_Y_CHECK_SUM_ST_(i));
		val = readl(reg);
		val = reg_value(RLE_Y_CHECK_SUM_ST_Y[i], val, RLE_Y_CHECK_SUM_Y_SHIFT, RLE_Y_CHECK_SUM_Y_MASK);
		dc_writel(val, reg);
	}
	/* RLE_INT_MASK_ */
	for (i = 0; i < SP_COUNT; i++) {
		reg = reg_addr(dev->reg_base, RLE_INT_MASK_(i));
		val = readl(reg);
		val = reg_value(RLE_INT_V_ERR[i], val, RLE_INT_V_ERR_SHIFT, RLE_INT_V_ERR_MASK);
		val = reg_value(RLE_INT_U_ERR[i], val, RLE_INT_U_ERR_SHIFT, RLE_INT_U_ERR_MASK);
		val = reg_value(RLE_INT_Y_ERR[i], val, RLE_INT_Y_ERR_SHIFT, RLE_INT_Y_ERR_MASK);
		val = reg_value(RLE_INT_A_ERR[i], val, RLE_INT_A_ERR_SHIFT, RLE_INT_A_ERR_MASK);
		dc_writel(val, reg);
	}
}

static void dc_clut_init(struct dc_device *dev)
{
	ulong reg;
	uint32_t val;
	int i;

	/* CLUT_A_CTRL_ */
	for (i = 0; i < SP_COUNT; i++) {
		reg = reg_addr(dev->reg_base, CLUT_A_CTRL_(i));
		val = readl(reg);
		val = reg_value(CLUT_A_Y_SEL[i], val, CLUT_A_Y_SEL_SHIFT, CLUT_A_Y_SEL_MASK);
		val = reg_value(CLUT_A_BYPASS[i], val, CLUT_A_BYPASS_SHIFT, CLUT_A_BYPASS_MASK);
		val = reg_value(CLUT_A_OFFSET[i], val, CLUT_A_OFFSET_SHIFT, CLUT_A_OFFSET_MASK);
		val = reg_value(CLUT_A_DEPTH[i], val, CLUT_A_DEPTH_SHIFT, CLUT_A_DEPTH_MASK);
		dc_writel(val, reg);
	}
	/* CLUT_Y_CTRL_ */
	for (i = 0; i < SP_COUNT; i++) {
		reg = reg_addr(dev->reg_base, CLUT_Y_CTRL_(i));
		val = readl(reg);
		val = reg_value(CLUT_Y_BYPASS[i], val, CLUT_Y_BYPASS_SHIFT, CLUT_Y_BYPASS_MASK);
		val = reg_value(CLUT_Y_OFFSET[i], val, CLUT_Y_OFFSET_SHIFT, CLUT_Y_OFFSET_MASK);
		val = reg_value(CLUT_Y_DEPTH[i], val, CLUT_Y_DEPTH_SHIFT, CLUT_Y_DEPTH_MASK);
		dc_writel(val, reg);
	}
	/* CLUT_U_CTRL_ */
	for (i = 0; i < SP_COUNT; i++) {
		reg = reg_addr(dev->reg_base, CLUT_U_CTRL_(i));
		val = readl(reg);
		val = reg_value(CLUT_U_Y_SEL[i], val, CLUT_U_Y_SEL_SHIFT, CLUT_U_Y_SEL_MASK);
		val = reg_value(CLUT_U_BYPASS[i], val, CLUT_U_BYPASS_SHIFT, CLUT_U_BYPASS_MASK);
		val = reg_value(CLUT_U_OFFSET[i], val, CLUT_U_OFFSET_SHIFT, CLUT_U_OFFSET_MASK);
		val = reg_value(CLUT_U_DEPTH[i], val, CLUT_U_DEPTH_SHIFT, CLUT_U_DEPTH_MASK);
		dc_writel(val, reg);
	}
	/* CLUT_V_CTRL_ */
	for (i = 0; i < SP_COUNT; i++) {
		reg = reg_addr(dev->reg_base, CLUT_V_CTRL_(i));
		val = readl(reg);
		val = reg_value(CLUT_V_Y_SEL[i], val, CLUT_V_Y_SEL_SHIFT, CLUT_V_Y_SEL_MASK);
		val = reg_value(CLUT_V_BYPASS[i], val, CLUT_V_BYPASS_SHIFT, CLUT_V_BYPASS_MASK);
		val = reg_value(CLUT_V_OFFSET[i], val, CLUT_V_OFFSET_SHIFT, CLUT_V_OFFSET_MASK);
		val = reg_value(CLUT_V_DEPTH[i], val, CLUT_V_DEPTH_SHIFT, CLUT_V_DEPTH_MASK);
		dc_writel(val, reg);
	}
	/* CLUT_READ_CTRL_ */
	for (i = 0; i < SP_COUNT; i++) {
		reg = reg_addr(dev->reg_base, CLUT_READ_CTRL_(i));
		val = readl(reg);
		val = reg_value(CLUT_APB_SEL[i], val, CLUT_APB_SEL_SHIFT, CLUT_APB_SEL_MASK);
		dc_writel(val, reg);
	}
}

static void dc_mlc_init(struct dc_device *dev)
{
	ulong reg;
	uint32_t val;
	int i;

	/* MLC_SF_CTRL_ */
	for (i = 0; i < MLC_LAYER_COUNT; i++) {
		reg = reg_addr(dev->reg_base, MLC_SF_CTRL_(i));
		val = readl(reg);
		val = reg_value(MLC_SF_PROT_VAL[i], val, MLC_SF_PROT_VAL_SHIFT, MLC_SF_PROT_VAL_MASK);
		val = reg_value(MLC_SF_VPOS_PROT_EN[i], val, MLC_SF_VPOS_PROT_EN_SHIFT, MLC_SF_VPOS_PROT_EN_MASK);
		val = reg_value(MLC_SF_SLOWDOWN_EN[i], val, MLC_SF_SLOWDOWN_EN_SHIFT, MLC_SF_SLOWDOWN_EN_MASK);
		val = reg_value(MLC_SF_AFLU_PSEL[i], val, MLC_SF_AFLU_PSEL_SHIFT, MLC_SF_AFLU_PSEL_MASK);
		val = reg_value(MLC_SF_AFLU_EN[i], val, MLC_SF_AFLU_EN_SHIFT, MLC_SF_AFLU_EN_MASK);
		val = reg_value(MLC_SF_CKEY_EN[i], val, MLC_SF_CKEY_EN_SHIFT, MLC_SF_CKEY_EN_MASK);
		val = reg_value(MLC_SF_G_ALPHA_EN[i], val, MLC_SF_G_ALPHA_EN_SHIFT, MLC_SF_G_ALPHA_EN_MASK);
		val = reg_value(MLC_SF_CROP_EN[i], val, MLC_SF_CROP_EN_SHIFT, MLC_SF_CROP_EN_MASK);
		val = reg_value(MLC_SF_EN[i], val, MLC_SF_EN_SHIFT, MLC_SF_EN_MASK);
		dc_writel(val, reg);
		/* S MLC */
		reg = reg_addr(dev->reg_base, MLC_SF_CTRL_S_(i));
		dc_writel(val, reg);
	}
	/* MLC_SF_H_SPOS_ */
	for (i = 0; i < MLC_LAYER_COUNT; i++) {
		reg = reg_addr(dev->reg_base, MLC_SF_H_SPOS_(i));
		val = readl(reg);
		val = reg_value(MLC_SF_H_SPOS_H[i], val, MLC_SF_H_SPOS_H_SHIFT, MLC_SF_H_SPOS_H_MASK);
		dc_writel(val, reg);
		/* S MLC */
		reg = reg_addr(dev->reg_base, MLC_SF_H_SPOS_S_(i));
		dc_writel(val, reg);
	}
	/* MLC_SF_V_SPOS_ */
	for (i = 0; i < MLC_LAYER_COUNT; i++) {
		reg = reg_addr(dev->reg_base, MLC_SF_V_SPOS_(i));
		val = readl(reg);
		val = reg_value(MLC_SF_V_SPOS_V[i], val, MLC_SF_V_SPOS_V_SHIFT, MLC_SF_V_SPOS_V_MASK);
		dc_writel(val, reg);
		/* S MLC */
		reg = reg_addr(dev->reg_base, MLC_SF_V_SPOS_S_(i));
		dc_writel(val, reg);
	}
	/* MLC_SF_SIZE_ */
	for (i = 0; i < MLC_LAYER_COUNT; i++) {
		reg = reg_addr(dev->reg_base, MLC_SF_SIZE_(i));
		val = readl(reg);
		val = reg_value(MLC_SF_SIZE_V[i], val, MLC_SF_SIZE_V_SHIFT, MLC_SF_SIZE_V_MASK);
		val = reg_value(MLC_SF_SIZE_H[i], val, MLC_SF_SIZE_H_SHIFT, MLC_SF_SIZE_H_MASK);
		dc_writel(val, reg);
		/* S MLC */
		reg = reg_addr(dev->reg_base, MLC_SF_SIZE_S_(i));
		dc_writel(val, reg);
	}
	/* MLC_SF_CROP_H_POS_ */
	for (i = 0; i < MLC_LAYER_COUNT; i++) {
		reg = reg_addr(dev->reg_base, MLC_SF_CROP_H_POS_(i));
		val = readl(reg);
		val = reg_value(MLC_SF_CROP_END[i], val, MLC_SF_CROP_END_SHIFT, MLC_SF_CROP_END_MASK);
		val = reg_value(MLC_SF_CROP_START[i], val, MLC_SF_CROP_START_SHIFT, MLC_SF_CROP_START_MASK);
		dc_writel(val, reg);
		/* S MLC */
		reg = reg_addr(dev->reg_base, MLC_SF_CROP_H_POS_S_(i));
		dc_writel(val, reg);
	}
	/* MLC_SF_CROP_V_POS_ */
	for (i = 0; i < MLC_LAYER_COUNT; i++) {
		reg = reg_addr(dev->reg_base, MLC_SF_CROP_V_POS_(i));
		val = readl(reg);
		val = reg_value(MLC_SF_CROP_END[i], val, MLC_SF_CROP_END_SHIFT, MLC_SF_CROP_END_MASK);
		val = reg_value(MLC_SF_CROP_START[i], val, MLC_SF_CROP_START_SHIFT, MLC_SF_CROP_START_MASK);
		dc_writel(val, reg);
		/* S MLC */
		reg = reg_addr(dev->reg_base, MLC_SF_CROP_V_POS_S_(i));
		dc_writel(val, reg);
	}
	/* MLC_SF_G_ALPHA_ */
	for (i = 0; i < MLC_LAYER_COUNT; i++) {
		reg = reg_addr(dev->reg_base, MLC_SF_G_ALPHA_(i));
		val = readl(reg);
		val = reg_value(MLC_SF_G_ALPHA_A[i], val, MLC_SF_G_ALPHA_A_SHIFT, MLC_SF_G_ALPHA_A_MASK);
		dc_writel(val, reg);
		/* S MLC */
		reg = reg_addr(dev->reg_base, MLC_SF_G_ALPHA_S_(i));
		dc_writel(val, reg);
	}
	/* MLC_SF_CKEY_ALPHA_ */
	for (i = 0; i < MLC_LAYER_COUNT; i++) {
		reg = reg_addr(dev->reg_base, MLC_SF_CKEY_ALPHA_(i));
		val = readl(reg);
		val = reg_value(MLC_SF_CKEY_ALPHA_A[i], val, MLC_SF_CKEY_ALPHA_A_SHIFT, MLC_SF_CKEY_ALPHA_A_MASK);
		dc_writel(val, reg);
		/* S MLC */
		reg = reg_addr(dev->reg_base, MLC_SF_CKEY_ALPHA_S_(i));
		dc_writel(val, reg);
	}
	/* MLC_SF_CKEY_R_LV_ */
	for (i = 0; i < MLC_LAYER_COUNT; i++) {
		reg = reg_addr(dev->reg_base, MLC_SF_CKEY_R_LV_(i));
		val = readl(reg);
		val = reg_value(MLC_SF_CKEY_LV_UP[i], val, MLC_SF_CKEY_LV_UP_SHIFT, MLC_SF_CKEY_LV_UP_MASK);
		val = reg_value(MLC_SF_CKEY_LV_DN[i], val, MLC_SF_CKEY_LV_DN_SHIFT, MLC_SF_CKEY_LV_DN_MASK);
		dc_writel(val, reg);
		/* S MLC */
		reg = reg_addr(dev->reg_base, MLC_SF_CKEY_R_LV_S_(i));
		dc_writel(val, reg);
	}
	/* MLC_SF_CKEY_G_LV_ */
	for (i = 0; i < MLC_LAYER_COUNT; i++) {
		reg = reg_addr(dev->reg_base, MLC_SF_CKEY_G_LV_(i));
		val = readl(reg);
		val = reg_value(MLC_SF_CKEY_LV_UP[i], val, MLC_SF_CKEY_LV_UP_SHIFT, MLC_SF_CKEY_LV_UP_MASK);
		val = reg_value(MLC_SF_CKEY_LV_DN[i], val, MLC_SF_CKEY_LV_DN_SHIFT, MLC_SF_CKEY_LV_DN_MASK);
		dc_writel(val, reg);
		/* S MLC */
		reg = reg_addr(dev->reg_base, MLC_SF_CKEY_G_LV_S_(i));
		dc_writel(val, reg);
	}
	/* MLC_SF_CKEY_B_LV_ */
	for (i = 0; i < MLC_LAYER_COUNT; i++) {
		reg = reg_addr(dev->reg_base, MLC_SF_CKEY_B_LV_(i));
		val = readl(reg);
		val = reg_value(MLC_SF_CKEY_LV_UP[i], val, MLC_SF_CKEY_LV_UP_SHIFT, MLC_SF_CKEY_LV_UP_MASK);
		val = reg_value(MLC_SF_CKEY_LV_DN[i], val, MLC_SF_CKEY_LV_DN_SHIFT, MLC_SF_CKEY_LV_DN_MASK);
		dc_writel(val, reg);
		/* S MLC */
		reg = reg_addr(dev->reg_base, MLC_SF_CKEY_B_LV_S_(i));
		dc_writel(val, reg);
	}
	/* MLC_SF_AFLU_TIME_*/
	for (i = 0; i < MLC_LAYER_COUNT; i++) {
		reg = reg_addr(dev->reg_base, MLC_SF_AFLU_TIME_(i));
		val = readl(reg);
		val = reg_value(MLC_SF_AFLU_TIMER[i], val, MLC_SF_AFLU_TIMER_SHIFT, MLC_SF_AFLU_TIMER_MASK);
		dc_writel(val, reg);
		/* S MLC */
		reg = reg_addr(dev->reg_base, MLC_SF_AFLU_TIME_S_(i));
		dc_writel(val, reg);
	}
	/* MLC_PATH_CTRL_ */
	for (i = 0; i < MLC_PATH_COUNT; i++) {
		reg = reg_addr(dev->reg_base, MLC_PATH_CTRL_(i));
		val = readl(reg);
		val = reg_value(ALPHA_BLD_IDX[i], val, ALPHA_BLD_IDX_SHIFT, ALPHA_BLD_IDX_MASK);
		val = reg_value(LAYER_OUT_IDX[i], val, LAYER_OUT_IDX_SHIFT, LAYER_OUT_IDX_MASK);
		dc_writel(val, reg);
		/* S MLC */
		reg = reg_addr(dev->reg_base, MLC_PATH_CTRL_S_(i));
		dc_writel(val, reg);
	}

	/* MLC_BG_CTRL */
	reg = reg_addr(dev->reg_base, MLC_BG_CTRL);
	val = readl(reg);
	val = reg_value(BG_A, val, BG_A_SHIFT, BG_A_MASK);
	val = reg_value(AFLU_EN, val, AFLU_EN_SHIFT, AFLU_EN_MASK);
	val = reg_value(FSTART_SEL, val, FSTART_SEL_SHIFT, FSTART_SEL_MASK);
	val = reg_value(BG_A_SEL, val, BG_A_SEL_SHIFT, BG_A_SEL_MASK);
	val = reg_value(BG_EN, val, BG_EN_SHIFT, BG_EN_MASK);
	val = reg_value(ALPHA_BLD_BYPS, val, ALPHA_BLD_BYPS_SHIFT, ALPHA_BLD_BYPS_MASK);
	dc_writel(val, reg);
	/* S MLC */
	reg = reg_addr(dev->reg_base, MLC_BG_CTRL_S);
	dc_writel(val, reg);

	/* MLC_BG_COLOR */
	reg = reg_addr(dev->reg_base, MLC_BG_COLOR);
	val = readl(reg);
	val = reg_value(BG_COLOR_R, val, BG_COLOR_R_SHIFT, BG_COLOR_R_MASK);
	val = reg_value(BG_COLOR_G, val, BG_COLOR_G_SHIFT, BG_COLOR_G_MASK);
	val = reg_value(BG_COLOR_B, val, BG_COLOR_B_SHIFT, BG_COLOR_B_MASK);
	dc_writel(val, reg);
	/* S MLC */
	reg = reg_addr(dev->reg_base, MLC_BG_COLOR_S);
	dc_writel(val, reg);

	/* MLC_BG_AFLU_TIME */
	reg = reg_addr(dev->reg_base, MLC_BG_AFLU_TIME);
	val = readl(reg);
	val = reg_value(MLC_BG_AFLU_TIMER, val, MLC_BG_AFLU_TIMER_SHIFT, MLC_BG_AFLU_TIMER_MASK);
	dc_writel(val, reg);
	/* S MLC */
	reg = reg_addr(dev->reg_base, MLC_BG_AFLU_TIME_S);
	dc_writel(val, reg);

	/* MLC_CANVAS_COLOR */
	reg = reg_addr(dev->reg_base, MLC_CANVAS_COLOR);
	val = readl(reg);
	val = reg_value(CANVAS_COLOR_R, val, CANVAS_COLOR_R_SHIFT, CANVAS_COLOR_R_MASK);
	val = reg_value(CANVAS_COLOR_G, val, CANVAS_COLOR_G_SHIFT, CANVAS_COLOR_G_MASK);
	val = reg_value(CANVAS_COLOR_B, val, CANVAS_COLOR_B_SHIFT, CANVAS_COLOR_B_MASK);
	dc_writel(val, reg);
	/* S MLC */
	reg = reg_addr(dev->reg_base, MLC_CANVAS_COLOR_S);
	dc_writel(val, reg);

	/* MLC_CLK_RATIO */
	reg = reg_addr(dev->reg_base, MLC_CLK_RATIO);
	val = readl(reg);
	val = reg_value(MLC_CLK_RATIO_V, val, MLC_CLK_RATIO_SHIFT, MLC_CLK_RATIO_MASK);
	dc_writel(val, reg);
	/* S MLC */
	reg = reg_addr(dev->reg_base, MLC_CLK_RATIO_S);
	dc_writel(val, reg);

	/* MLC_INT_MASK */
	reg = reg_addr(dev->reg_base, MLC_INT_MASK);
	val = readl(reg);
	val = reg_value(MLC_MASK_ERR_L_5, val, MLC_MASK_ERR_L_5_SHIFT, MLC_MASK_ERR_L_5_MASK);
	val = reg_value(MLC_MASK_ERR_L_4, val, MLC_MASK_ERR_L_4_SHIFT, MLC_MASK_ERR_L_4_MASK);
	val = reg_value(MLC_MASK_ERR_L_3, val, MLC_MASK_ERR_L_3_SHIFT, MLC_MASK_ERR_L_3_MASK);
	val = reg_value(MLC_MASK_ERR_L_2, val, MLC_MASK_ERR_L_2_SHIFT, MLC_MASK_ERR_L_2_MASK);
	val = reg_value(MLC_MASK_ERR_L_1, val, MLC_MASK_ERR_L_1_SHIFT, MLC_MASK_ERR_L_1_MASK);
	val = reg_value(MLC_MASK_ERR_L_0, val, MLC_MASK_ERR_L_0_SHIFT, MLC_MASK_ERR_L_0_MASK);
	val = reg_value(MLC_MASK_FLU_L_5, val, MLC_MASK_FLU_L_5_SHIFT, MLC_MASK_FLU_L_5_MASK);
	val = reg_value(MLC_MASK_FLU_L_4, val, MLC_MASK_FLU_L_4_SHIFT, MLC_MASK_FLU_L_4_MASK);
	val = reg_value(MLC_MASK_FLU_L_3, val, MLC_MASK_FLU_L_3_SHIFT, MLC_MASK_FLU_L_3_MASK);
	val = reg_value(MLC_MASK_FLU_L_2, val, MLC_MASK_FLU_L_2_SHIFT, MLC_MASK_FLU_L_2_MASK);
	val = reg_value(MLC_MASK_FLU_L_1, val, MLC_MASK_FLU_L_1_SHIFT, MLC_MASK_FLU_L_1_MASK);
	val = reg_value(MLC_MASK_FLU_L_0, val, MLC_MASK_FLU_L_0_SHIFT, MLC_MASK_FLU_L_0_MASK);
	val = reg_value(MLC_MASK_FRM_END, val, MLC_MASK_FRM_END_SHIFT, MLC_MASK_FRM_END_MASK);
	dc_writel(val, reg);
	/* S MLC */
	reg = reg_addr(dev->reg_base, MLC_INT_MASK_S);
	dc_writel(val, reg);
}

static void gp_sdw_trig(struct dc_device *dev)
{
	ulong reg;
	uint32_t val;
	int i;

	for (i = 0; i < GP_COUNT; i++) {
		reg = reg_addr(dev->reg_base, GP_SDW_CTRL_(i));
		val = reg_value(GP_SDW_CTRL_TRIG[i], readl(reg), GP_SDW_CTRL_TRIG_SHIFT, GP_SDW_CTRL_TRIG_MASK);
		dc_writel(val, reg);
	}
}

static void dc_gp_init(struct dc_device *dev)
{
	ulong reg;
	uint32_t val;
	pr_info("init GP register\n");
	int i;

	/* DC_GP_PIX_CIMP_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_GP_PIX_CIMP_(i));
		val = readl(reg);
		val = reg_value(GP_BPV[i], val, BPV_SHIFT, BPV_MASK);
		val = reg_value(GP_BPU[i], val, BPU_SHIFT, BPU_MASK);
		val = reg_value(GP_BPY[i], val, BPY_SHIFT, BPY_MASK);
		val = reg_value(GP_BPA[i], val, BPA_SHIFT, BPA_MASK);
		dc_writel(val, reg);
	}
	/* DC_GP_FRM_CTRL_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_GP_FRM_CTRL_(i));
		val = readl(reg);
		val = reg_value(GP_ENDIAN_CTRL[i], val, ENDIAN_CTRL_SHIFT, ENDIAN_CTRL_MASK);
		val = reg_value(GP_COMP_SWAP[i], val, COMP_SWAP_SHIFT, COMP_SWAP_MASK);
		val = reg_value(GP_ROT[i], val, ROT_SHIFT, ROT_MASK);
		val = reg_value(GP_RGB_YUV[i], val, RGB_YUV_SHIFT, RGB_YUV_MASK);
		val = reg_value(GP_UV_SWAP[i], val, UV_SWAP_SHIFT, UV_SWAP_MASK);
		val = reg_value(GP_UV_MODE[i], val, UV_MODE_SHIFT, UV_MODE_MASK);
		val = reg_value(GP_MODE[i], val, MODE_SHIFT, MODE_MASK);
		val = reg_value(GP_FMT[i], val, FMT_SHIFT, FMT_MASK);
		dc_writel(val, reg);
	}
	/* DC_GP_FRM_SIZE_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_GP_FRM_SIZE_(i));
		val = readl(reg);
		val = reg_value(GP_FRM_HEIGHT[i], val, FRM_HEIGHT_SHIFT, FRM_HEIGHT_MASK);
		val = reg_value(GP_FRM_WIDTH[i], val, FRM_WIDTH_SHIFT, FRM_WIDTH_MASK);
		dc_writel(val, reg);
	}

	/* DC_GP_Y_BADDR_L_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_GP_Y_BADDR_L_(i));
		val = readl(reg);
		val = reg_value(get_l_addr(image_rgb888.y, 1), val, BADDR_L_Y_SHIFT, BADDR_L_Y_MASK);
		dc_writel(val, reg);
	}
	/* DC_GP_Y_BADDR_H_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_GP_Y_BADDR_H_(i));
		val = readl(reg);
		val = reg_value(get_h_addr(image_rgb888.y, 1), val, BADDR_H_Y_SHIFT, BADDR_H_Y_MASK);
		dc_writel(val, reg);
	}
	/* DC_GP_U_BADDR_L_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_GP_U_BADDR_L_(i));
		val = readl(reg);
		val = reg_value(GP_BADDR_L_U[i], val, BADDR_L_U_SHIFT, BADDR_L_U_MASK);
		dc_writel(val, reg);
	}
	/* DC_GP_U_BADDR_H_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_GP_U_BADDR_H_(i));
		val = readl(reg);
		val = reg_value(GP_BADDR_H_U[i], val, BADDR_H_U_SHIFT, BADDR_H_U_MASK);
		dc_writel(val, reg);
	}
	/* DC_GP_V_BADDR_L_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_GP_V_BADDR_L_(i));
		val = readl(reg);
		val = reg_value(GP_BADDR_L_V[i], val, BADDR_L_V_SHIFT, BADDR_L_V_MASK);
		dc_writel(val, reg);
	}
	/* DC_GP_V_BADDR_H_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_GP_V_BADDR_H_(i));
		val = readl(reg);
		val = reg_value(GP_BADDR_H_V[i], val, BADDR_H_V_SHIFT, BADDR_H_V_MASK);
		dc_writel(val, reg);
	}
	/* DC_GP_Y_STRIDE_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_GP_Y_STRIDE_(i));
		val = readl(reg);
		val = reg_value(GP_STRIDE_Y[i], val, STRIDE_Y_SHIFT, STRIDE_Y_MASK);
		dc_writel(val, reg);
	}
	/* DC_GP_U_STRIDE_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_GP_U_STRIDE_(i));
		val = readl(reg);
		val = reg_value(GP_STRIDE_U[i], val, STRIDE_U_SHIFT, STRIDE_U_MASK);
		dc_writel(val, reg);
	}
	/* DC_GP_V_STRIDE_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_GP_V_STRIDE_(i));
		val = readl(reg);
		val = reg_value(GP_STRIDE_V[i], val, STRIDE_V_SHIFT, STRIDE_V_MASK);
		dc_writel(val, reg);
	}
	/* DC_GP_FRM_OFFSET_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_GP_FRM_OFFSET_(i));
		val = readl(reg);
		val = reg_value(GP_FRM_Y[i], val, FRM_Y_SHIFT, FRM_Y_MASK);
		val = reg_value(GP_FRM_X[i], val, FRM_X_SHIFT, FRM_X_MASK);
		dc_writel(val, reg);
	}
	/* DC_GP_YUVUP_CTRL_i */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, DC_GP_YUVUP_CTRL_(i));
		val = readl(reg);
		val = reg_value(DC_GP_YUVUP_EN[i], val, DC_GP_YUVUP_EN_SHIFT, DC_GP_YUVUP_EN_MASK);
		val = reg_value(DC_GP_YUVUP_VOFSET[i], val, DC_GP_YUVUP_VOFSET_SHIFT, DC_GP_YUVUP_VOFSET_MASK);
		val = reg_value(DC_GP_YUVUP_HOFSET[i], val, DC_GP_YUVUP_HOFSET_SHIFT, DC_GP_YUVUP_HOFSET_MASK);
		val = reg_value(DC_GP_YUVUP_FILTER_MODE[i], val, DC_GP_YUVUP_FILTER_MODE_SHIFT, DC_GP_YUVUP_FILTER_MODE_MASK);
		val = reg_value(DC_GP_YUVUP_UPV_BYPASS[i], val, DC_GP_YUVUP_UPV_BYPASS_SHIFT, DC_GP_YUVUP_UPV_BYPASS_MASK);
		val = reg_value(DC_GP_YUVUP_UPH_BYPASS[i], val, DC_GP_YUVUP_UPH_BYPASS_SHIFT, DC_GP_YUVUP_UPH_BYPASS_MASK);
		val = reg_value(DC_GP_YUVUP_BYPASS[i], val, DC_GP_YUVUP_BYPASS_SHIFT, DC_GP_YUVUP_BYPASS_MASK);
		dc_writel(val, reg);
	}

	/* GP_CSC */
	/* GP_CSC_CTRL_i */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, GP_CSC_CTRL_(i));
		val = readl(reg);
		val = reg_value(GP_CSC_ALPHA[i], val, GP_CSC_ALPHA_SHIFT, GP_CSC_ALPHA_MASK);
		val = reg_value(GP_CSC_SBUP_CONV[i], val, GP_CSC_SBUP_CONV_SHIFT, GP_CSC_SBUP_CONV_MASK);
		val = reg_value(GP_CSC_BYPASS[i], val, GP_CSC_BYPASS_SHIFT, GP_CSC_BYPASS_MASK);
		dc_writel(val, reg);
	}

	/* GP_CSC_COEF1_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, GP_CSC_COEF1_(i));
		val = readl(reg);
		val = reg_value(GP_CSC_COEF1_A01[i], val, GP_CSC_COEF1_A01_SHIFT, GP_CSC_COEF1_A01_MASK);
		val = reg_value(GP_CSC_COEF1_A00[i], val, GP_CSC_COEF1_A00_SHIFT, GP_CSC_COEF1_A00_MASK);
		dc_writel(val, reg);
	}

	/* GP_CSC_COEF2_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, GP_CSC_COEF2_(i));
		val = readl(reg);
		val = reg_value(GP_CSC_COEF2_A10[i], val, GP_CSC_COEF2_A10_SHIFT, GP_CSC_COEF2_A10_MASK);
		val = reg_value(GP_CSC_COEF2_A02[i], val, GP_CSC_COEF2_A02_SHIFT, GP_CSC_COEF2_A02_MASK);
		dc_writel(val, reg);
	}
	/* GP_CSC_COEF3_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, GP_CSC_COEF3_(i));
		val = readl(reg);
		val = reg_value(GP_CSC_COEF3_A12[i], val, GP_CSC_COEF3_A12_SHIFT, GP_CSC_COEF3_A12_MASK);
		val = reg_value(GP_CSC_COEF3_A11[i], val, GP_CSC_COEF3_A11_SHIFT, GP_CSC_COEF3_A11_MASK);
		dc_writel(val, reg);
	}
	/* GP_CSC_COEF4_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, GP_CSC_COEF4_(i));
		val = readl(reg);
		val = reg_value(GP_CSC_COEF4_A21[i], val, GP_CSC_COEF4_A21_SHIFT, GP_CSC_COEF4_A21_MASK);
		val = reg_value(GP_CSC_COEF4_A20[i], val, GP_CSC_COEF4_A20_SHIFT, GP_CSC_COEF4_A20_MASK);
		dc_writel(val, reg);
	}
	/* GP_CSC_COEF5_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, GP_CSC_COEF5_(i));
		val = readl(reg);
		val = reg_value(GP_CSC_COEF5_B0[i], val, GP_CSC_COEF5_B0_SHIFT, GP_CSC_COEF5_B0_MASK);
		val = reg_value(GP_CSC_COEF5_A22[i], val, GP_CSC_COEF5_A22_SHIFT, GP_CSC_COEF5_A22_MASK);
		dc_writel(val, reg);
	}
	/* GP_CSC_COEF6_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, GP_CSC_COEF6_(i));
		val = readl(reg);
		val = reg_value(GP_CSC_COEF6_B2[i], val, GP_CSC_COEF6_B2_SHIFT, GP_CSC_COEF6_B2_MASK);
		val = reg_value(GP_CSC_COEF6_B1[i], val, GP_CSC_COEF6_B1_SHIFT, GP_CSC_COEF6_B1_MASK);
		dc_writel(val, reg);
	}
	/* GP_CSC_COEF7_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, GP_CSC_COEF7_(i));
		val = readl(reg);
		val = reg_value(GP_CSC_COEF7_C1[i], val, GP_CSC_COEF7_C1_SHIFT, GP_CSC_COEF7_C1_MASK);
		val = reg_value(GP_CSC_COEF7_C0[i], val, GP_CSC_COEF7_C0_SHIFT, GP_CSC_COEF7_C0_MASK);
		dc_writel(val, reg);
	}
	/* GP_CSC_COEF8_ */
	for (i = 0; i < GP_COUNT; i++ ) {
		reg = reg_addr(dev->reg_base, GP_CSC_COEF8_(i));
		val = readl(reg);
		val = reg_value(GP_CSC_COEF8_C2[i], val, GP_CSC_COEF8_C2_SHIFT, GP_CSC_COEF8_C2_MASK);
		dc_writel(val, reg);
	}
}

static void dc_csc_init(struct dc_device *dev)
{
	ulong reg;
	uint32_t val;
	int i;

	/* DC_CSC_CTRL_ */
	for (i = 0 ; i < DC_CSC_COUNT; i++) {
		reg = reg_addr(dev->reg_base, DC_CSC_CTRL_(i));
		val = readl(reg);
		val = reg_value(DC_CSC_ALPHA[i], val, DC_CSC_ALPHA_SHIFT, DC_CSC_ALPHA_MASK);
		val = reg_value(DC_CSC_SBUP_CONV[i], val, DC_CSC_SBUP_CONV_SHIFT, DC_CSC_SBUP_CONV_MASK);
		val = reg_value(DC_CSC_BYPASS[i], val, DC_CSC_BYPASS_SHIFT, DC_CSC_BYPASS_MASK);
		dc_writel(val, reg);
	}
	/* DC_CSC_COEF1_ */
	for (i = 0 ; i < DC_CSC_COUNT; i++) {
		reg = reg_addr(dev->reg_base, DC_CSC_COEF1_(i));
		val = readl(reg);
		val = reg_value(DC_CSC_COEF1_A01[i], val, DC_CSC_COEF1_A01_SHIFT, DC_CSC_COEF1_A01_MASK);
		val = reg_value(DC_CSC_COEF1_A00[i], val, DC_CSC_COEF1_A00_SHIFT, DC_CSC_COEF1_A00_MASK);
		dc_writel(val, reg);
	}
	/* DC_CSC_COEF2_ */
	for (i = 0 ; i < DC_CSC_COUNT; i++) {
		reg = reg_addr(dev->reg_base, DC_CSC_COEF2_(i));
		val = readl(reg);
		val = reg_value(DC_CSC_COEF2_A10[i], val, DC_CSC_COEF2_A10_SHIFT, DC_CSC_COEF2_A10_MASK);
		val = reg_value(DC_CSC_COEF2_A02[i], val, DC_CSC_COEF2_A02_SHIFT, DC_CSC_COEF2_A02_MASK);
		dc_writel(val, reg);
	}
	/* DC_CSC_COEF3_ */
	for (i = 0 ; i < DC_CSC_COUNT; i++) {
		reg = reg_addr(dev->reg_base, DC_CSC_COEF3_(i));
		val = readl(reg);
		val = reg_value(DC_CSC_COEF3_A12[i], val, DC_CSC_COEF3_A12_SHIFT, DC_CSC_COEF3_A12_MASK);
		val = reg_value(DC_CSC_COEF3_A11[i], val, DC_CSC_COEF3_A11_SHIFT, DC_CSC_COEF3_A11_MASK);
		dc_writel(val, reg);
	}
	/* DC_CSC_COEF4_ */
	for (i = 0 ; i < DC_CSC_COUNT; i++) {
		reg = reg_addr(dev->reg_base, DC_CSC_COEF4_(i));
		val = readl(reg);
		val = reg_value(DC_CSC_COEF4_A21[i], val, DC_CSC_COEF4_A21_SHIFT, DC_CSC_COEF4_A21_MASK);
		val = reg_value(DC_CSC_COEF4_A20[i], val, DC_CSC_COEF4_A20_SHIFT, DC_CSC_COEF4_A20_MASK);
		dc_writel(val, reg);
	}
	/* DC_CSC_COEF5_ */
	for (i = 0 ; i < DC_CSC_COUNT; i++) {
		reg = reg_addr(dev->reg_base, DC_CSC_COEF5_(i));
		val = readl(reg);
		val = reg_value(DC_CSC_COEF5_B0[i], val, DC_CSC_COEF5_B0_SHIFT, DC_CSC_COEF5_B0_MASK);
		val = reg_value(DC_CSC_COEF5_A22[i], val, DC_CSC_COEF5_A22_SHIFT, DC_CSC_COEF5_A22_MASK);
		dc_writel(val, reg);
	}
	/* DC_CSC_COEF6_ */
	for (i = 0 ; i < DC_CSC_COUNT; i++) {
		reg = reg_addr(dev->reg_base, DC_CSC_COEF6_(i));
		val = readl(reg);
		val = reg_value(DC_CSC_COEF6_B2[i], val, DC_CSC_COEF6_B2_SHIFT, DC_CSC_COEF6_B2_MASK);
		val = reg_value(DC_CSC_COEF6_B1[i], val, DC_CSC_COEF6_B1_SHIFT, DC_CSC_COEF6_B1_MASK);
		dc_writel(val, reg);
	}
	/* DC_CSC_COEF7_ */
	for (i = 0 ; i < DC_CSC_COUNT; i++) {
		reg = reg_addr(dev->reg_base, DC_CSC_COEF7_(i));
		val = readl(reg);
		val = reg_value(DC_CSC_COEF7_C1[i], val, DC_CSC_COEF7_C1_SHIFT, DC_CSC_COEF7_C1_MASK);
		val = reg_value(DC_CSC_COEF7_C0[i], val, DC_CSC_COEF7_C0_SHIFT, DC_CSC_COEF7_C0_MASK);
		dc_writel(val, reg);
	}
	/* DC_CSC_COEF8_ */
	for (i = 0 ; i < DC_CSC_COUNT; i++) {
		reg = reg_addr(dev->reg_base, DC_CSC_COEF8_(i));
		val = readl(reg);
		val = reg_value(DC_CSC_COEF8_C2[i], val, DC_CSC_COEF8_C2_SHIFT, DC_CSC_COEF8_C2_MASK);
		dc_writel(val, reg);
	}
}

static void dc_gamma_init(struct dc_device *dev)
{
	ulong reg;
	uint32_t val;

	/* GAMMA_CTRL */
	reg = reg_addr(dev->reg_base, GAMMA_CTRL);
	val = readl(reg);
	val = reg_value(GAMMA_APB_RD_TO, val, GAMMA_APB_RD_TO_SHIFT, GAMMA_APB_RD_TO_MASK);
	val = reg_value(GMMA_BYPASS, val, GMMA_BYPASS_SHIFT, GMMA_BYPASS_MASK);
	dc_writel(val, reg);
}

static void dc_dither_init(struct dc_device *dev)
{
	ulong reg;
	uint32_t val;

	/* DITHER_CTRL */
	reg = reg_addr(dev->reg_base, DITHER_CTRL);
	val = readl(reg);
	val = reg_value(DITHER_V_DEP, val, DITHER_V_DEP_SHIFT, DITHER_V_DEP_MASK);
	val = reg_value(DITHER_U_DEP, val, DITHER_U_DEP_SHIFT, DITHER_U_DEP_MASK);
	val = reg_value(DITHER_Y_DEP, val, DITHER_Y_DEP_SHIFT, DITHER_Y_DEP_MASK);
	val = reg_value(DITHER_MODE_12, val, DITHER_MODE_12_SHIFT, DITHER_MODE_12_MASK);
	val = reg_value(DITHER_SPA_LSB_EXP_MODE, val, DITHER_SPA_LSB_EXP_MODE_SHIFT, DITHER_SPA_LSB_EXP_MODE_MASK);
	val = reg_value(DITHER_SPA_1ST, val, DITHER_SPA_1ST_SHIFT, DITHER_SPA_1ST_MASK);
	val = reg_value(DITHER_SPA_EN, val, DITHER_SPA_EN_SHIFT, DITHER_SPA_EN_MASK);
	val = reg_value(DITHER_TEM_EN, val, DITHER_TEM_EN_SHIFT, DITHER_TEM_EN_MASK);
	val = reg_value(DITHER_BYPASS, val, DITHER_BYPASS_SHIFT, DITHER_BYPASS_MASK);
	dc_writel(val, reg);
}


static void set_di_flc_trig(struct dc_device *dev)
{
	ulong reg;
	uint32_t val;

	reg = reg_addr(dev->reg_base, DC_FLC_CTRL);
	val = readl(reg);
	val = reg_value(DI_TRIG, val, DI_TRIG_SHIFT, DI_TRIG_MASK);
	val = reg_value(FLC_TRIG, val, FLC_TRIG_SHIFT, FLC_TRIG_MASK);
	dc_writel(val, reg);
}

static void dc_flc_trig(struct dc_device *dev)
{
	ulong reg;
	uint32_t val;

	reg = reg_addr(dev->reg_base, DC_FLC_CTRL);
	val = readl(reg);
	val = reg_value(FLC_TRIG, val, FLC_TRIG_SHIFT, FLC_TRIG_MASK);
	dc_writel(val, reg);
}

static uint32_t get_flc_trig(struct dc_device *dev)
{
	ulong reg;
	uint32_t val;

	reg = reg_addr(dev->reg_base, DC_FLC_CTRL);
	val = readl(reg);
	return val & FLC_TRIG_MASK;
}

static void set_tcon_ctrl_en(struct dc_device *dev)
{
	ulong reg;
	uint32_t val;

	reg = reg_addr(dev->reg_base, TCON_CTRL);
	val = readl(reg);
	val = reg_value(TCON_EN, val, TCON_EN_SHIFT, TCON_EN_MASK);
	dc_writel(val, reg);
}

static void set_flc_update_force(struct dc_device *dev)
{
	ulong reg;
	uint32_t val;

	reg = reg_addr(dev->reg_base, DC_FLC_UPDATE);
	val = readl(reg);
	val = reg_value(DC_FLC_UPDATE_FORCE, val, DC_FLC_UPDATE_FORCE_SHIFT, DC_FLC_UPDATE_FORCE_MASK);
	dc_writel(val, reg);
}

static int dc_init(struct dc_device *dev)
{

	dc_int_disable(dev);

	dc_sw_reset(dev);

	/* RDMA */
	dc_rdma_init(dev);

	/* TCON */
	dc_tcon_init(dev);

	/* GP */
	dc_gp_init(dev);

	/* GP Trigger */
	gp_sdw_trig(dev);

	/* SP */
	dc_sp_init(dev);

	/* RLE */
	dc_rle_init(dev);

	/* CLUT */
	dc_clut_init(dev);

	/* SP Trigger */
	sp_sdw_trig(dev);

	/* MLC */
	dc_mlc_init(dev);

	/* CSC */
	dc_csc_init(dev);

	/* GAMMA */
	dc_gamma_init(dev);

	/* DITHER */
	dc_dither_init(dev);

	/* DI FLC Trigger */
	set_di_flc_trig(dev);

	/* DC FLC UPDATE FORCE */
	set_flc_update_force(dev);
	pr_info("set_flc_update_force done \n");
	udelay(1000);
	pr_info("set_tcon_ctrl_en \n");
	/* TCON ENABLE */
	set_tcon_ctrl_en(dev);

	/* DC and DC_SF INT  */
	//dc_int_init(dev);

	return 0;
}

static int dc_config_done(struct dc_device *dev)
{
	int err = 0;
	dc_config_t *config = &dev->config;

	if (config->type == DC_NO_UPDATE) {
	}

	if (config->type & DC_GP_UPDATE) {
		gp_sdw_trig(dev);
		config->type &= ~DC_GP_UPDATE;
	}

	if (config->type & DC_SP_UPDATE) {
		sp_sdw_trig(dev);
		config->type &= ~DC_SP_UPDATE;
	}

	if (config->type & DC_BG_UPDATE) {
		set_di_flc_trig(dev);
		config->type &= ~DC_BG_UPDATE;
	}

	return err;
}

static int dc_check(struct dc_device *dev)
{
	int err = 0;
	dev->status = get_flc_trig(dev);
	if (dev->status != 0) {
		dev->status = -1;
		err = -1;
	}
	pr_info("%s: flc_trig = %d\n", __func__, dev->status);
	return err;
}

static int dc_sp_update(struct dc_device *dev)
{
	sp_sdw_trig(dev);
	return 0;
}

static int dc_gp_update(struct dc_device *dev)
{
	gp_sdw_trig(dev);
	return 0;
}


static int dc_reset(struct dc_device *dev)
{
	dc_sw_reset(dev);
	return 0;
}

static int dc_gp_config(struct dc_device *dev, layer_info *sf_info)
{
	uint32_t val;
	uint64_t reg;
	uint32_t bpv, bpu, bpy, bpa;
	uint32_t rgb_yuv;
	uint32_t y_addr_l, y_addr_h, u_addr_l, u_addr_h, v_addr_l, v_addr_h;
	uint32_t y_stride = 0;
	uint32_t u_stride = 0;
	uint32_t v_stride = 0;
	uint32_t csc_bypass = 1;
	uint32_t csc_mode = 0;//0:ycbcr to rgb, 1: yuv to rgb, 2: rgb to ycbcr, 3: rgb to yuv
	uint32_t uv_swap = 0;
	uint32_t yuv_up_en = 0;
	layer_info *info = sf_info;

	/*1.para prepare*/
	switch (info->frm_format) {
		case DATA_RGB565:
			bpa = 0;
			bpu = 6;
			bpv = bpy = 5;
			rgb_yuv = 0;
			y_stride = info->width * info->bpp - 1;
			break;
		case DATA_RGB666:
			bpa = 0;
			bpy = bpu = bpv = 6;
			rgb_yuv = 0;
			y_stride = (info->width * 18 + 7) / 8 - 1;
			break;
		case DATA_ARGB8888:
			bpa = bpy = bpu = bpv = 8;
			rgb_yuv = 0;
			y_stride = info->width * info->bpp - 1;
			break;
		case DATA_RGB888:
			bpa = 0;
			bpy = bpu = bpv = 8;
			rgb_yuv = 0;
			y_stride = info->width * info->bpp - 1;
			break;
		case DATA_ARGB4444:
			bpa = bpy = bpu = bpv = 4;
			rgb_yuv = 0;
			y_stride = info->width * info->bpp - 1;
			break;
		case DATA_ARGB1555:
			bpa = 1;
			bpy = bpu = bpv = 5;
			rgb_yuv = 0;
			y_stride = info->width * info->bpp - 1;
			break;
		case DATA_ARGB2101010:
			bpa = 2;
			bpy = bpu = bpv = 10;
			rgb_yuv = 0;
			y_stride = info->width * info->bpp - 1;
			break;
		case DATA_YUV444:
			bpa = 0;
			bpy = bpu = bpv = 8;
			rgb_yuv = 1;
			csc_bypass = 0;
			csc_mode = 0;
			if (info->frm_buf_str_fmt == FMT_INTERLEAVED) {
				y_stride = info->width * info->bpp - 1;
			}
			if (info->frm_buf_str_fmt == FMT_SEMI_PLANAR) {
				y_stride = info->width * info->bpp - 1;
				u_stride = info->width * info->bpp - 1;
			}
			if (info->frm_buf_str_fmt == FMT_PLANAR) {
				y_stride = info->width * info->bpp - 1;
				u_stride = info->width * info->bpp - 1;
				v_stride = info->width * info->bpp - 1;
			}
			break;
		case DATA_AYUV4444:
			bpy = bpu = bpv = bpa = 8;
			rgb_yuv = 1;
			csc_bypass = 0;
			csc_mode = 0;
			y_stride = info->width * info->bpp - 1;
			break;
		case DATA_YUV420:
			bpa = bpv = 0;
			bpy = bpu = 8;
			rgb_yuv = 1;
			csc_bypass = 0;
			csc_mode = 0;
			yuv_up_en = 1;
			if (info->frm_buf_str_fmt == FMT_PLANAR) {
				bpv = 8;
				y_stride = info->width - 1;
				u_stride = (info->width + 1) / 2 - 1;
				v_stride = (info->width + 1) / 2 - 1;
			}
			break;
		case DATA_YUV422:
			bpa = bpv = 0;
			bpy = bpu = 8;
			rgb_yuv = 1;
			csc_bypass = 0;
			csc_mode = 0;
			yuv_up_en = 1;
			if (info->frm_buf_str_fmt == FMT_PLANAR) {
				bpv = 8;
				y_stride = info->width - 1;
				u_stride = (info->width + 1) / 2 - 1;
				v_stride = (info->width + 1) / 2 - 1;
			}
			if (info->frm_buf_str_fmt == FMT_INTERLEAVED) {
				y_stride = info->width * 2 - 1;
			}
			if (info->frm_buf_str_fmt == FMT_SEMI_PLANAR) {
				y_stride = info->width - 1;
				u_stride = info->width - 1;
			}
			break;
		case DATA_YUV440:
			bpa = bpv = 0;
			bpy = bpu = 8;
			rgb_yuv = 1;
			csc_bypass = 0;
			csc_mode = 0;
			yuv_up_en = 1;
			if (info->frm_buf_str_fmt == FMT_SEMI_PLANAR) {
				y_stride = info->width - 1;
				u_stride = info->width * 2 - 1;
			}
			break;
		case DATA_NV12:
			bpa = bpv = 0;
			bpy = bpu = 8;
			rgb_yuv = 1;
			csc_bypass = 0;
			csc_mode = 0;
			uv_swap = 1;
			yuv_up_en = 1;
			y_stride = info->width - 1;
			u_stride = info->width - 1;
			break;
		case DATA_NV21:
			bpa = bpv = 0;
			bpy = bpu = 8;
			rgb_yuv = 1;
			csc_bypass = 0;
			csc_mode = 0;
			yuv_up_en = 1;
			y_stride = info->width - 1;
			u_stride = info->width - 1;
			break;
		case DATA_YUV420_10_16PACK:
			bpa = bpv = 0;
			bpy = bpu = 10;
			rgb_yuv = 1;
			csc_bypass = 0;
			csc_mode = 0;
			yuv_up_en = 1;
			y_stride = info->width * 2 - 1;
			u_stride = info->width * 2 - 1;
			break;
		case DATA_YONLY1:
			bpa = bpv = bpu = 0;
			bpy = 1;
			rgb_yuv = 0;
			y_stride = (info->width + 7) / 8 - 1;
			if (info->frm_buf_str_fmt == FMT_MONOTONIC) {
				rgb_yuv = 1;
				csc_bypass = 0;
				csc_mode = 1;
			}
			break;
		case DATA_YONLY2:
			bpa = bpv = bpu = 0;
			bpy = 2;
			rgb_yuv = 0;
			y_stride = (info->width + 3) / 4 - 1;
			if (info->frm_buf_str_fmt == FMT_MONOTONIC) {
				rgb_yuv = 1;
				csc_bypass = 0;
				csc_mode = 1;
			}
			break;
		case DATA_YONLY4:
			bpa = bpv = bpu = 0;
			bpy = 4;
			rgb_yuv = 0;
			y_stride = (info->width + 1) / 2 - 1;
			if (info->frm_buf_str_fmt == FMT_MONOTONIC) {
				rgb_yuv = 1;
				csc_bypass = 0;
				csc_mode = 1;
			}
			break;
		case DATA_YONLY6:
			bpa = bpv = bpu = 0;
			bpy = 6;
			rgb_yuv = 0;
			y_stride = (info->width * 6 + 7) / 8 - 1;
			if (info->frm_buf_str_fmt == FMT_MONOTONIC) {
				rgb_yuv = 1;
				csc_bypass = 0;
				csc_mode = 1;
			}
			break;
		case DATA_YONLY8:
			bpa = bpv = bpu = 0;
			bpy = 8;
			rgb_yuv = 0;
			y_stride = info->width - 1;
			if (info->frm_buf_str_fmt == FMT_MONOTONIC) {
				rgb_yuv = 1;
				csc_bypass = 0;
				csc_mode = 1;
			}
			break;
		default:
			pr_err("%s  Not support this foramt!\n",__func__);
			return -1;
	}

	y_addr_l = get_l_addr(info->frame_addr_y, 0);
	y_addr_h = get_h_addr(info->frame_addr_y, 0);
	u_addr_l = get_l_addr(info->frame_addr_u, 0);
	u_addr_h = get_h_addr(info->frame_addr_u, 0);
	v_addr_l = get_l_addr(info->frame_addr_v, 0);
	v_addr_h = get_h_addr(info->frame_addr_v, 0);

	/*2.set g-pipe common regs*/
	/* DC_GP_PIX_CIMP_ */
	reg = reg_addr(dev->reg_base, DC_GP_PIX_CIMP_(info->pipe_index));
	val = readl(reg);
	val = reg_value(bpv, val, BPV_SHIFT, BPV_MASK);
	val = reg_value(bpu, val, BPU_SHIFT, BPU_MASK);
	val = reg_value(bpy, val, BPY_SHIFT, BPY_MASK);
	val = reg_value(bpa, val, BPA_SHIFT, BPA_MASK);
	dc_writel(val, reg);

	/* DC_GP_FRM_CTRL_ */
	reg = reg_addr(dev->reg_base, DC_GP_FRM_CTRL_(info->pipe_index));
	val = readl(reg);
	val = reg_value(info->endian_mode, val, ENDIAN_CTRL_SHIFT, ENDIAN_CTRL_MASK);
	val = reg_value(info->swap_mode, val, COMP_SWAP_SHIFT, COMP_SWAP_MASK);
	val = reg_value(info->rot_type, val, ROT_SHIFT, ROT_MASK);
	val = reg_value(rgb_yuv, val, RGB_YUV_SHIFT, RGB_YUV_MASK);
	val = reg_value(uv_swap, val, UV_SWAP_SHIFT, UV_SWAP_MASK);
	val = reg_value(info->data_uv_mode, val, UV_MODE_SHIFT, UV_MODE_MASK);
	val = reg_value(info->data_mode, val, MODE_SHIFT, MODE_MASK);
	val = reg_value(info->frm_buf_str_fmt, val, FMT_SHIFT, FMT_MASK);
	dc_writel(val, reg);

	/* DC_GP_FRM_SIZE_ */
	reg = reg_addr(dev->reg_base, DC_GP_FRM_SIZE_(info->pipe_index));
	val = readl(reg);
	val = reg_value(info->height - 1, val, FRM_HEIGHT_SHIFT, FRM_HEIGHT_MASK);
	val = reg_value(info->width - 1, val, FRM_WIDTH_SHIFT, FRM_WIDTH_MASK);
	dc_writel(val, reg);

	/* DC_GP_Y_BADDR_L_ */
	reg = reg_addr(dev->reg_base, DC_GP_Y_BADDR_L_(info->pipe_index));
	val = readl(reg);
	val = reg_value(y_addr_l, val, BADDR_L_Y_SHIFT, BADDR_L_Y_MASK);
	dc_writel(val, reg);

	/* DC_GP_Y_BADDR_H_ */
	reg = reg_addr(dev->reg_base, DC_GP_Y_BADDR_H_(info->pipe_index));
	val = readl(reg);
	val = reg_value(y_addr_h, val, BADDR_H_Y_SHIFT, BADDR_H_Y_MASK);
	dc_writel(val, reg);

	/* DC_GP_U_BADDR_L_ */
	reg = reg_addr(dev->reg_base, DC_GP_U_BADDR_L_(info->pipe_index));
	val = readl(reg);
	val = reg_value(u_addr_l, val, BADDR_L_U_SHIFT, BADDR_L_U_MASK);
	dc_writel(val, reg);

	/* DC_GP_U_BADDR_H_ */
	reg = reg_addr(dev->reg_base, DC_GP_U_BADDR_H_(info->pipe_index));
	val = readl(reg);
	val = reg_value(u_addr_h, val, BADDR_H_U_SHIFT, BADDR_H_U_MASK);
	dc_writel(val, reg);

	/* DC_GP_V_BADDR_L_ */
	reg = reg_addr(dev->reg_base, DC_GP_V_BADDR_L_(info->pipe_index));
	val = readl(reg);
	val = reg_value(v_addr_l, val, BADDR_L_V_SHIFT, BADDR_L_V_MASK);
	dc_writel(val, reg);

	/* DC_GP_V_BADDR_H_ */
	reg = reg_addr(dev->reg_base, DC_GP_V_BADDR_H_(info->pipe_index));
	val = readl(reg);
	val = reg_value(v_addr_h, val, BADDR_H_V_SHIFT, BADDR_H_V_MASK);
	dc_writel(val, reg);

	/* DC_GP_Y_STRIDE_ */
	reg = reg_addr(dev->reg_base, DC_GP_Y_STRIDE_(info->pipe_index));
	val = readl(reg);
	val = reg_value(info->y_stride ?:y_stride, val, STRIDE_Y_SHIFT, STRIDE_Y_MASK);
	dc_writel(val, reg);

	/* DC_GP_U_STRIDE_ */
	reg = reg_addr(dev->reg_base, DC_GP_U_STRIDE_(info->pipe_index));
	val = readl(reg);
	val = reg_value(info->u_stride ?:u_stride, val, STRIDE_U_SHIFT, STRIDE_U_MASK);
	dc_writel(val, reg);

	/* DC_GP_V_STRIDE_ */
	reg = reg_addr(dev->reg_base, DC_GP_V_STRIDE_(info->pipe_index));
	val = readl(reg);
	val = reg_value(info->v_stride ?:v_stride, val, STRIDE_V_SHIFT, STRIDE_V_MASK);
	dc_writel(val, reg);

	/* DC_GP_FRM_OFFSET_ */
	reg = reg_addr(dev->reg_base, DC_GP_FRM_OFFSET_(info->pipe_index));
	val = readl(reg);
	val = reg_value(info->offset_y, val, FRM_Y_SHIFT, FRM_Y_MASK);
	val = reg_value(info->offset_x, val, FRM_X_SHIFT, FRM_X_MASK);
	dc_writel(val, reg);

	/*3.set g-pipe yuv up regs*/
	/* DC_GP_YUVUP_CTRL_ */
	reg = reg_addr(dev->reg_base, DC_GP_YUVUP_CTRL_(info->pipe_index));
	val = readl(reg);
	val = reg_value(yuv_up_en, val, DC_GP_YUVUP_EN_SHIFT, DC_GP_YUVUP_EN_MASK);
	//val = reg_value(DC_GP_YUVUP_CTRL_UPV_BYPASS, val, DC_GP_YUVUP_UPV_BYPASS_SHIFT, DC_GP_YUVUP_UPV_BYPASS_MASK);
	//val = reg_value(DC_GP_YUVUP_CTRL_UPH_BYPASS, val, DC_GP_YUVUP_UPH_BYPASS_SHIFT, DC_GP_YUVUP_UPH_BYPASS_MASK);
	val = reg_value((yuv_up_en ? 0 : 1), val, DC_GP_YUVUP_BYPASS_SHIFT, DC_GP_YUVUP_BYPASS_MASK);
	dc_writel(val, reg);

	/*4.set g-pipe csc regs*/
	/* GP_CSC_CTRL_ */
	reg = reg_addr(dev->reg_base, GP_CSC_CTRL_(info->pipe_index));
	val = readl(reg);
	val = reg_value(csc_bypass, val, GP_CSC_BYPASS_SHIFT, GP_CSC_BYPASS_MASK);
	dc_writel(val, reg);

	if (!csc_bypass) {
		/* GP_CSC_COEF1 */
		reg = reg_addr(dev->reg_base, GP_CSC_COEF1_(info->pipe_index));
		val = readl(reg);
		val = reg_value(csc_param[csc_mode][1], val, GP_CSC_COEF1_A01_SHIFT, GP_CSC_COEF1_A01_MASK);
		val = reg_value(csc_param[csc_mode][0], val, GP_CSC_COEF1_A00_SHIFT, GP_CSC_COEF1_A00_MASK);
		dc_writel(val, reg);

		/* GP_CSC_COEF2 */
		reg = reg_addr(dev->reg_base, GP_CSC_COEF2_(info->pipe_index));
		val = readl(reg);
		val = reg_value(csc_param[csc_mode][3], val, GP_CSC_COEF2_A10_SHIFT, GP_CSC_COEF2_A10_MASK);
		val = reg_value(csc_param[csc_mode][2], val, GP_CSC_COEF2_A02_SHIFT, GP_CSC_COEF2_A02_MASK);
		dc_writel(val, reg);

		/* GP_CSC_COEF3 */
		reg = reg_addr(dev->reg_base, GP_CSC_COEF3_(info->pipe_index));
		val = readl(reg);
		val = reg_value(csc_param[csc_mode][5], val, GP_CSC_COEF3_A12_SHIFT, GP_CSC_COEF3_A12_MASK);
		val = reg_value(csc_param[csc_mode][4], val, GP_CSC_COEF3_A11_SHIFT, GP_CSC_COEF3_A11_MASK);
		dc_writel(val, reg);

		/* GP_CSC_COEF4 */
		reg = reg_addr(dev->reg_base, GP_CSC_COEF4_(info->pipe_index));
		val = readl(reg);
		val = reg_value(csc_param[csc_mode][7], val, GP_CSC_COEF4_A21_SHIFT, GP_CSC_COEF4_A21_MASK);
		val = reg_value(csc_param[csc_mode][6], val, GP_CSC_COEF4_A20_SHIFT, GP_CSC_COEF4_A20_MASK);
		dc_writel(val, reg);

		/* GP_CSC_COEF5 */
		reg = reg_addr(dev->reg_base, GP_CSC_COEF5_(info->pipe_index));
		val = readl(reg);
		val = reg_value(csc_param[csc_mode][9], val, GP_CSC_COEF5_B0_SHIFT, GP_CSC_COEF5_B0_MASK);
		val = reg_value(csc_param[csc_mode][8], val, GP_CSC_COEF5_A22_SHIFT, GP_CSC_COEF5_A22_MASK);
		dc_writel(val, reg);

		/* GP_CSC_COEF6 */
		reg = reg_addr(dev->reg_base, GP_CSC_COEF6_(info->pipe_index));
		val = readl(reg);
		val = reg_value(csc_param[csc_mode][11], val, GP_CSC_COEF6_B2_SHIFT, GP_CSC_COEF6_B2_MASK);
		val = reg_value(csc_param[csc_mode][10], val, GP_CSC_COEF6_B1_SHIFT, GP_CSC_COEF6_B1_MASK);
		dc_writel(val, reg);

		/* GP_CSC_COEF7 */
		reg = reg_addr(dev->reg_base, GP_CSC_COEF7_(info->pipe_index));
		val = readl(reg);
		val = reg_value(csc_param[csc_mode][13], val, GP_CSC_COEF7_C1_SHIFT, GP_CSC_COEF7_C1_MASK);
		val = reg_value(csc_param[csc_mode][12], val, GP_CSC_COEF7_C0_SHIFT, GP_CSC_COEF7_C0_MASK);
		dc_writel(val, reg);

		/* GP_CSC_COEF8 */
		reg = reg_addr(dev->reg_base, GP_CSC_COEF8_(info->pipe_index));
		val = readl(reg);
		val = reg_value(csc_param[csc_mode][14], val, GP_CSC_COEF8_C2_SHIFT, GP_CSC_COEF8_C2_MASK);
		dc_writel(val, reg);
	}
	dc_triggle_type |= DC_GP_UPDATE;

	return 0;
}
static int dc_sp_config(struct dc_device *dev, layer_info *sf_info)
{
	uint32_t val;
	uint64_t reg;
	uint32_t bpv, bpu, bpy, bpa;
	uint32_t rgb_yuv = 0;
	uint32_t y_addr_l, y_addr_h;
	uint32_t y_stride = 0;
	layer_info *info = sf_info;

	/*1.para prepare*/
	switch (info->frm_format) {
		case DATA_RGB565:
			bpa = 0;
			bpu = 6;
			bpv = bpy = 5;
			y_stride = info->width * info->bpp - 1;
			break;
		case DATA_RGB666:
			bpa = 0;
			bpy = bpu = bpv = 6;
			y_stride = (info->width * 18 + 7) / 8 - 1;
			break;
		case DATA_ARGB8888:
			bpa = bpy = bpu = bpv = 8;
			y_stride = info->width * info->bpp - 1;
			break;
		case DATA_RGB888:
			bpa = 0;
			bpy = bpu = bpv = 8;
			y_stride = info->width * info->bpp - 1;
			break;
		case DATA_ARGB4444:
			bpa = bpy = bpu = bpv = 4;
			y_stride = info->width * info->bpp - 1;
			break;
		case DATA_ARGB1555:
			bpa = 1;
			bpy = bpu = bpv = 5;
			y_stride = info->width * info->bpp - 1;
			break;
		case DATA_ARGB2101010:
			bpa = 2;
			bpy = bpu = bpv = 10;
			y_stride = info->width * info->bpp - 1;
			break;
		case DATA_YONLY1:
			bpa = bpv = bpu = 0;
			bpy = 1;
			rgb_yuv = 1;
			y_stride = (info->width + 7) / 8 - 1;
			break;
		case DATA_YONLY2:
			bpa = bpv = bpu = 0;
			bpy = 2;
			rgb_yuv = 1;
			y_stride = (info->width + 3) / 4 - 1;
			break;
		case DATA_YONLY4:
			bpa = bpv = bpu = 0;
			bpy = 4;
			rgb_yuv = 1;
			y_stride = (info->width + 1) / 2 - 1;
			break;
		case DATA_YONLY6:
			bpa = bpv = bpu = 0;
			bpy = 6;
			rgb_yuv = 1;
			y_stride = (info->width * 6 + 7) / 8 - 1;
			break;
		case DATA_YONLY8:
			bpa = bpv = bpu = 0;
			bpy = 8;
			rgb_yuv = 1;
			y_stride = info->width - 1;
			break;
		default:
			pr_err("%s  Not support this foramt!\n",__func__);
			return -1;
	}

	y_addr_l = get_l_addr(info->frame_addr_y, 0);
	y_addr_h = get_h_addr(info->frame_addr_y, 0);

	/*2.set s-pipe common regs*/
	/* DC_SP_PIX_COMP_ */
	reg = reg_addr(dev->reg_base, DC_SP_PIX_COMP_(info->pipe_index));
	val = readl(reg);
	val = reg_value(bpv, val, BPV_SHIFT, BPV_MASK);
	val = reg_value(bpu, val, BPU_SHIFT, BPU_MASK);
	val = reg_value(bpy, val, BPY_SHIFT, BPY_MASK);
	val = reg_value(bpa, val, BPA_SHIFT, BPA_MASK);
	dc_writel(val, reg);

	/* DC_SP_FRM_CTRL_ */
	reg = reg_addr(dev->reg_base, DC_SP_FRM_CTRL_(info->pipe_index));
	val = readl(reg);
	val = reg_value(info->endian_mode, val, ENDIAN_CTRL_SHIFT, ENDIAN_CTRL_MASK);
	val = reg_value(info->swap_mode, val, COMP_SWAP_SHIFT, COMP_SWAP_MASK);
	val = reg_value(info->rot_type, val, ROT_SHIFT, ROT_MASK);
	val = reg_value(rgb_yuv, val, RGB_YUV_SHIFT, RGB_YUV_MASK);
	val = reg_value(info->data_mode, val, MODE_SHIFT, MODE_MASK);
	val = reg_value(info->frm_buf_str_fmt, val, FMT_SHIFT, FMT_MASK);
	dc_writel(val, reg);

	/* DC_SP_FRM_SIZE_ */
	reg = reg_addr(dev->reg_base, DC_SP_FRM_SIZE_(info->pipe_index));
	val = readl(reg);
	val = reg_value(info->height - 1, val, FRM_HEIGHT_SHIFT, FRM_HEIGHT_MASK);
	val = reg_value(info->width - 1, val, FRM_WIDTH_SHIFT, FRM_WIDTH_MASK);
	dc_writel(val, reg);

	/* DC_SP_Y_BADDR_L_ */
	reg = reg_addr(dev->reg_base, DC_SP_Y_BADDR_L_(info->pipe_index));
	val = readl(reg);
	val = reg_value(y_addr_l, val, BADDR_L_Y_SHIFT, BADDR_L_Y_MASK);
	dc_writel(val, reg);

	/* DC_SP_Y_BADDR_H_ */
	reg = reg_addr(dev->reg_base, DC_SP_Y_BADDR_H_(info->pipe_index));
	val = readl(reg);
	val = reg_value(y_addr_h, val, BADDR_H_Y_SHIFT, BADDR_H_Y_MASK);
	dc_writel(val, reg);

	/*DC_SP_Y_STRIDE_*/
	reg = reg_addr(dev->reg_base, DC_SP_Y_STRIDE_(info->pipe_index));
	val = readl(reg);
	val = reg_value(info->y_stride ?:y_stride, val,  STRIDE_Y_SHIFT, STRIDE_Y_MASK);
	dc_writel(val, reg);

	/* DC_SP_FRM_OFFSET_ */
	reg = reg_addr(dev->reg_base, DC_SP_FRM_OFFSET_(info->pipe_index));
	val = readl(reg);
	val = reg_value(info->offset_y, val, FRM_Y_SHIFT, FRM_Y_MASK);
	val = reg_value(info->offset_x, val, FRM_X_SHIFT, FRM_X_MASK);
	dc_writel(val, reg);

	/*3.set s-pipe rle regs*/
	if (info->data_mode == RLE_COMPR_MODE) {
		/* RLE_Y_LEN_ */
		reg = reg_addr(dev->reg_base, RLE_Y_LEN_(info->pipe_index));
		val = reg_value(info->rle.rle_y_len, 0, RLE_Y_LEN_Y_SHIFT, RLE_Y_LEN_Y_MASK);
		dc_writel(val, reg);

		/* RLE_Y_CHECK_SUM_ */
		reg = reg_addr(dev->reg_base, RLE_Y_CHECK_SUM_(info->pipe_index));
		val = reg_value(info->rle.rle_y_checksum, 0, RLE_Y_CHECK_SUM_Y_SHIFT, RLE_Y_CHECK_SUM_Y_MASK);
		dc_writel(val, reg);

		/*RLE_CTRL_*/
		reg = reg_addr(dev->reg_base, RLE_CTRL_(info->pipe_index));
		val = reg_value(info->rle.rle_data_size, 0, RLE_DATA_SIZE_SHIFT, RLE_DATA_SIZE_MASK);
		val = reg_value(1, val, RLE_EN_SHIFT, RLE_EN_MASK);
		dc_writel(val, reg);
	}
	else {
		/*RLE_CTRL_*/
		reg = reg_addr(dev->reg_base, RLE_CTRL_(info->pipe_index));
		val = readl(reg);
		val = reg_value(0, val, RLE_EN_SHIFT, RLE_EN_MASK);
		dc_writel(val, reg);
	}

	/*3.set s-pipe clut regs*/
	if (info->clut_en) {
		/* CLUT_A_CTRL_ */
		reg = reg_addr(dev->reg_base, CLUT_A_CTRL_(info->pipe_index));
		if (bpa) {
			val = reg_value(0, 0, CLUT_A_BYPASS_SHIFT, CLUT_A_BYPASS_MASK);
			val = reg_value(bpa, val, CLUT_A_DEPTH_SHIFT, CLUT_A_DEPTH_MASK);
		}
		else {
			val = reg_value(1, 0, CLUT_A_BYPASS_SHIFT, CLUT_A_BYPASS_MASK);
		}
		dc_writel(val, reg);

		/* CLUT_Y_CTRL_ */
		reg = reg_addr(dev->reg_base, CLUT_Y_CTRL_(info->pipe_index));
		if (bpy) {
			val = reg_value(0, 0, CLUT_Y_BYPASS_SHIFT, CLUT_Y_BYPASS_MASK);
			val = reg_value(bpy, val, CLUT_Y_DEPTH_SHIFT, CLUT_Y_DEPTH_MASK);
		}
		else {
			val = reg_value(1, 0, CLUT_Y_BYPASS_SHIFT, CLUT_Y_BYPASS_MASK);
		}
		dc_writel(val, reg);

		/* CLUT_U_CTRL_ */
		reg = reg_addr(dev->reg_base, CLUT_U_CTRL_(info->pipe_index));
		if (bpu) {
			val = reg_value(0, 0, CLUT_U_BYPASS_SHIFT, CLUT_U_BYPASS_MASK);
			val = reg_value(bpu, val, CLUT_U_DEPTH_SHIFT, CLUT_U_DEPTH_MASK);
		}
		else {
			val = reg_value(1, 0, CLUT_U_DEPTH_SHIFT, CLUT_U_DEPTH_MASK);
		}
		dc_writel(val, reg);

		/* CLUT_V_CTRL_ */
		reg = reg_addr(dev->reg_base, CLUT_V_CTRL_(info->pipe_index));
		if (bpv) {
			val = reg_value(0, 0, CLUT_V_BYPASS_SHIFT, CLUT_V_BYPASS_MASK);
			val = reg_value(bpv, val, CLUT_V_DEPTH_SHIFT, CLUT_V_DEPTH_MASK);
		}
		else {
			val = reg_value(1, 0, CLUT_V_BYPASS_SHIFT, CLUT_V_BYPASS_MASK);
		}
		dc_writel(val, reg);
	}
	else {
		/* CLUT_A_CTRL_ */
		reg = reg_addr(dev->reg_base, CLUT_A_CTRL_(info->pipe_index));
		val = reg_value(1, 0, CLUT_A_BYPASS_SHIFT, CLUT_A_BYPASS_MASK);
		dc_writel(val, reg);

		/* CLUT_Y_CTRL_ */
		reg = reg_addr(dev->reg_base, CLUT_Y_CTRL_(info->pipe_index));
		val = reg_value(1, 0, CLUT_Y_BYPASS_SHIFT, CLUT_Y_BYPASS_MASK);
		dc_writel(val, reg);

		/* CLUT_U_CTRL_ */
		reg = reg_addr(dev->reg_base, CLUT_U_CTRL_(info->pipe_index));
		val = reg_value(1, 0, CLUT_U_BYPASS_SHIFT, CLUT_U_BYPASS_MASK);
		dc_writel(val, reg);

		/* CLUT_V_CTRL_ */
		reg = reg_addr(dev->reg_base, CLUT_V_CTRL_(info->pipe_index));
		val = reg_value(1, 0, CLUT_V_BYPASS_SHIFT, CLUT_V_BYPASS_MASK);
		dc_writel(val, reg);
	}
	dc_triggle_type |= DC_SP_UPDATE;

	return 0;
}

static int dc_mlc_config(struct dc_device *dev, struct disp_info *dpu_info)
{
	uint32_t val, mlc_sf_index, mlc_path_index, i;
	uint64_t reg;
	struct disp_info *info = dpu_info;

	for (i = DC_SP; i <= DC_GP; i++) {
		layer_info layer = info->layers[i];
		mlc_sf_index = i;
		mlc_path_index = i + 1;
		if (layer.layer_dirty) {
			/* MLC_SF_CTRL_ */
			reg = reg_addr(dev->reg_base, MLC_SF_CTRL_(mlc_sf_index));
			val = readl(reg);
			val = reg_value(layer.sf_info.g_alpha_en, val, MLC_SF_G_ALPHA_EN_SHIFT, MLC_SF_G_ALPHA_EN_MASK);
			val = reg_value(layer.layer_enable, val, MLC_SF_EN_SHIFT, MLC_SF_EN_MASK);
			dc_writel(val, reg);

			/* MLC_SF_H_SPOS_ */
			reg = reg_addr(dev->reg_base, MLC_SF_H_SPOS_(mlc_sf_index));
			val = readl(reg);
			val = reg_value(layer.pos_y, val, MLC_SF_H_SPOS_H_SHIFT, MLC_SF_H_SPOS_H_MASK);
			dc_writel(val, reg);

			/* MLC_SF_V_SPOS_ */
			reg = reg_addr(dev->reg_base, MLC_SF_V_SPOS_(mlc_sf_index));
			val = readl(reg);
			val = reg_value(layer.pos_x, val, MLC_SF_V_SPOS_V_SHIFT, MLC_SF_V_SPOS_V_MASK);
			dc_writel(val, reg);

			/* MLC_SF_SIZE_ */
			reg = reg_addr(dev->reg_base, MLC_SF_SIZE_(mlc_sf_index));
			val = readl(reg);
			val = reg_value(layer.height - 1, val, MLC_SF_SIZE_V_SHIFT, MLC_SF_SIZE_V_MASK);
			val = reg_value(layer.width - 1, val, MLC_SF_SIZE_H_SHIFT, MLC_SF_SIZE_H_MASK);
			dc_writel(val, reg);

			/* MLC_SF_G_ALPHA_ */
			reg = reg_addr(dev->reg_base, MLC_SF_G_ALPHA_(mlc_sf_index));
			val = readl(reg);
			val = reg_value(layer.sf_info.g_alpha_value, val, MLC_SF_G_ALPHA_A_SHIFT, MLC_SF_G_ALPHA_A_MASK);
			dc_writel(val, reg);

			/* MLC_PATH_CTRL_ */
			if (layer.layer_enable) {
				reg = reg_addr(dev->reg_base, MLC_PATH_CTRL_(mlc_path_index));
				val = readl(reg);
				val = reg_value(path_zorder[layer.sf_info.z_order], val, LAYER_OUT_IDX_SHIFT, LAYER_OUT_IDX_MASK);
				dc_writel(val, reg);
				reg = reg_addr(dev->reg_base, MLC_PATH_CTRL_(path_zorder[layer.sf_info.z_order]));
				val = readl(reg);
				val = reg_value(mlc_path_index, val, ALPHA_BLD_IDX_SHIFT, ALPHA_BLD_IDX_MASK);
				dc_writel(val, reg);
			}
			else {
				reg = reg_addr(dev->reg_base, MLC_PATH_CTRL_(mlc_path_index));
				val = readl(reg);
				val = reg_value(0xf, val, LAYER_OUT_IDX_SHIFT, LAYER_OUT_IDX_MASK);
				dc_writel(val, reg);
				reg = reg_addr(dev->reg_base, MLC_PATH_CTRL_(path_zorder[layer.sf_info.z_order]));
				val = readl(reg);
				val = reg_value(0xf, val, ALPHA_BLD_IDX_SHIFT, ALPHA_BLD_IDX_MASK);
				dc_writel(val, reg);
			}
		}
	}
#if 0
	for (int i = DP_GP0; i <= DP_SP1; i++) {
		layer_info layer = info->layers[i];
		mlc_sf_index = 2;
		mlc_path_index = 3;
		if (layer.layer_enable) {
			/* MLC_SF_CTRL_ */
			reg = reg_addr(dev->reg_base, MLC_SF_CTRL_(mlc_sf_index));
			val = readl(reg);
			val = reg_value(1, val, MLC_SF_EN_SHIFT, MLC_SF_EN_MASK);
			dc_writel(val, reg);

			/* MLC_PATH_CTRL_ */
			reg = reg_addr(dev->reg_base, MLC_PATH_CTRL_(mlc_path_index));
			val = readl(reg);
			val = reg_value(path_zorder[3], val, LAYER_OUT_IDX_SHIFT, LAYER_OUT_IDX_MASK);
			dc_writel(val, reg);
			reg = reg_addr(dev->reg_base, MLC_PATH_CTRL_(path_zorder[3]));
			val = readl(reg);
			val = reg_value(mlc_path_index, val, ALPHA_BLD_IDX_SHIFT, ALPHA_BLD_IDX_MASK);
			dc_writel(val, reg);

			break;
		}
		else {
			/* MLC_SF_CTRL_ */
			reg = reg_addr(dev->reg_base, MLC_SF_CTRL_(mlc_sf_index));
			val = readl(reg);
			val = reg_value(0, val, MLC_SF_EN_SHIFT, MLC_SF_EN_MASK);
			dc_writel(val, reg);

			/* MLC_PATH_CTRL_ */
			reg = reg_addr(dev->reg_base, MLC_PATH_CTRL_(mlc_path_index));
			val = readl(reg);
			val = reg_value(0xf, val, LAYER_OUT_IDX_SHIFT, LAYER_OUT_IDX_MASK);
			dc_writel(val, reg);
			reg = reg_addr(dev->reg_base, MLC_PATH_CTRL_(path_zorder[3]));
			val = readl(reg);
			val = reg_value(0xf, val, ALPHA_BLD_IDX_SHIFT, ALPHA_BLD_IDX_MASK);
			dc_writel(val, reg);
		}
	}
#endif
	/* MLC_PATH_CTRL(0) */
	if (info->backgroud.bg_en) {
		reg = reg_addr(dev->reg_base, MLC_PATH_CTRL_(0));
		val = readl(reg);
		val = reg_value(0, val, ALPHA_BLD_IDX_SHIFT, ALPHA_BLD_IDX_MASK);
		val = reg_value(0, val, LAYER_OUT_IDX_SHIFT, LAYER_OUT_IDX_MASK);
		dc_writel(val, reg);
	}
	else {
		reg = reg_addr(dev->reg_base, MLC_PATH_CTRL_(0));
		val = readl(reg);
		val = reg_value(0xf, val, ALPHA_BLD_IDX_SHIFT, ALPHA_BLD_IDX_MASK);
		val = reg_value(0xf, val, LAYER_OUT_IDX_SHIFT, LAYER_OUT_IDX_MASK);
		dc_writel(val, reg);
	}

	/* MLC_BG_CTRL */
	reg = reg_addr(dev->reg_base, MLC_BG_CTRL);
	val = readl(reg);
	val = reg_value(info->backgroud.bg_a_value, val, BG_A_SHIFT, BG_A_MASK);
	val = reg_value(info->backgroud.bg_a_select, val, BG_A_SEL_SHIFT, BG_A_SEL_MASK);
	val = reg_value(info->backgroud.bg_en, val, BG_EN_SHIFT, BG_EN_MASK);
	dc_writel(val, reg);

	/* MLC_BG_COLOR */
	reg = reg_addr(dev->reg_base, MLC_BG_COLOR);
	dc_writel(val, reg);

	/* MLC_CANVAS_COLOR */
	reg = reg_addr(dev->reg_base, MLC_CANVAS_COLOR);
	dc_writel(val, reg);

	dc_triggle_type |= DC_MLC_UPDATE;
	return 0;
}

static void dc_gp_sdw_triggle(struct dc_device *dev)
{
	uint64_t reg;

	reg = reg_addr(dev->reg_base, GP_SDW_CTRL_(0));
	dc_writel(0x1, reg);
}

static void dc_sp_sdw_triggle(struct dc_device *dev)
{
	uint64_t reg;

	reg = reg_addr(dev->reg_base, SP_SDW_CTRL_(0));
	dc_writel(0x1, reg);
}

static void dc_flc_di_triggle(struct dc_device *dev)
{
	uint64_t reg;
	uint32_t val;

	reg = reg_addr(dev->reg_base, DC_FLC_CTRL);
	val = readl(reg);
	val = reg_value(0x1, val, DI_TRIG_SHIFT, DI_TRIG_MASK);
	dc_writel(val, reg);
}

static void dc_flc_crc32_triggle(struct dc_device *dev)
{
	uint64_t reg;
	uint32_t val;

	reg = reg_addr(dev->reg_base, DC_FLC_CTRL);
	val = readl(reg);
	val = reg_value(0x1, val, CRC32_TRIG_SHIFT, CRC32_TRIG_MASK);
	dc_writel(val, reg);
}

static void dc_flc_triggle(struct dc_device *dev)
{
	uint64_t reg;
	uint32_t val;
	uint32_t cnt = 0;

	while (1) {
		reg = reg_addr(dev->reg_base, DC_FLC_CTRL);
		val = readl(reg);
		if (!(FLC_TRIG_SHIFT & val)) {
			val = reg_value(0x1, val, FLC_TRIG_SHIFT, FLC_TRIG_MASK);
			dc_writel(val, reg);
			break;
		}
		udelay(100);
		cnt++;
		if (cnt > 500) {
		    pr_err("%s:triggle overtime!\n", __func__);
		    break;
		}
	}
}


static void dc_triggle(struct dc_device *dev, uint32_t type)
{
	if (type & DC_GP_UPDATE) {
		dc_gp_sdw_triggle(dev);
	}

	if (type & DC_SP_UPDATE) {
		dc_sp_sdw_triggle(dev);
	}

	if (type & (DC_MLC_UPDATE |
				DC_GAMMA_UPDATE |
				DC_DITHER_UPDATE |
				DC_CSC_UPDATE)) {
		dc_flc_di_triggle(dev);
	}

	if (type & DC_CRC_UPDATE) {
		dc_flc_crc32_triggle(dev);
	}

	udelay(10);
	dc_flc_triggle(dev);
	dc_triggle_type = 0;
}

static int dc_config(struct dc_device *dev, struct disp_info *dpu_info)
{
	int ret = 0;
	struct disp_info *info = dpu_info;

	if (info->layers[DC_SP].layer_dirty) {
		if (info->layers[DC_SP].layer_enable) {
			ret = dc_sp_config(dev, &info->layers[DC_SP]);
			if (ret < 0) {
				pr_err("%s dc s-pipe failed!\n",__func__);
			return -1;
			}
		}
	}

	if (info->layers[DC_GP].layer_dirty) {
		if (info->layers[DC_GP].layer_enable) {
			ret = dc_gp_config(dev, &info->layers[DC_GP]);
			if (ret < 0) {
				pr_err("%s  dc g-pipe failed!\n",__func__);
				return -1;
			}
		}
	}

	dc_mlc_config(dev, info);
	dc_triggle(dev, dc_triggle_type);
	return 0;
}

static enum irqreturn dc_irq_handler(int irq, void *data)
{
	uint32_t val;
	uint64_t reg;
	struct sd_display_res *disp = data;
	struct dc_device *dev = &disp->dc;

	reg = reg_addr(dev->reg_base, DC_INT_STATUS);
	val = readl(reg);
	dc_writel(val, reg);
	if (val & BIT_AP_APB_DC_DC_INT_STATUS_TCON_EOF) {
		schedule_work(&disp->irq_queue);
	}

	return IRQ_HANDLED;
}

static void dc_vsync_irq_set(struct dc_device *dev, int32_t enable)
{
	uint32_t val;
	uint64_t reg;

	reg = reg_addr(dev->reg_base, DC_INT_MASK);
	if (enable) {
		val = reg_value(0, 0xFFFFFFFF, TCON_EOF_SHIFT, TCON_EOF_MASK);
		dc_writel(val, reg);
	} else {
        val = reg_value(1, 0xFFFFFFFF, TCON_EOF_SHIFT, TCON_EOF_MASK);
		dc_writel(val, reg);
	}
}

static const struct dc_ops ops = {
	.init = dc_init,
	.reset = dc_reset,
	.config = dc_config,
	.config_done = dc_config_done,
	.check =  dc_check,
	.irq_handler = dc_irq_handler,
	.vsync_set = dc_vsync_irq_set,
};

int dc_setup(struct dc_device *dev)
{
	int err;

	pr_info("%s (id=%d, base=0x%lx, irq=%d)\n", __func__, dev->id, dev->reg_base, dev->irq);

	err = dc_init(dev);

	dev->ops = &ops;

	return err;
}

int get_dc_num(void)
{
	return DC_NUM;
}

dc_profile_t *get_dc_prop(uint8_t id)
{
	return &dc_prop[id];
}
