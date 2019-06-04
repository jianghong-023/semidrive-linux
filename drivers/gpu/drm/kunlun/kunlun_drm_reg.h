/*
 * kunlun_drm_reg.h
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef _KUNLUN_DRM_REG_H_
#define _KUNLUN_DRM_REG_H_
#include <linux/types.h>

struct kunlun_du_reg {
	uint16_t offset;
	uint16_t shift;
	uint32_t mask;
};

#define DU_REG(off, _mask, _shift)	\
	{ .offset = (off), .mask = (_mask), .shift = (_shift), }

#define DC_INT_MSK                       0x20
#define DC_INT_ST                        0x24
#define DC_INT_MASK                      0xFFFFFFF

#define RDMA_SHIFT                       0UL
#define RDMA_MASK                        1UL << RDMA_SHIFT

#define RLE_SHIFT                        1UL
#define RLE_MASK                         1UL << RLE_SHIFT

#define MLC_SHIFT                        2UL
#define MLC_MASK                         1UL << MLC_SHIFT

#define TCON_SOF_SHIFT                   3UL
#define TCON_SOF_MASK                    1UL << TCON_SOF_SHIFT

#define TCON_EOF_SHIFT                   4UL
#define TCON_EOF_MASK                    1UL << TCON_EOF_SHIFT

#define TCON_UNDERRUN_SHIFT              5UL
#define TCON_UNDERRUN_MASK               1UL << TCON_UNDERRUN_SHIFT

#define DC_UNDERRUN_SHIFT                6UL
#define DC_UNDERRUN_MASK                 1UL << DC_UNDERRUN_SHIFT

#define SDMA_DONE_SHIFT                  7UL
#define SDMA_DONE_MASK                   1UL << SDMA_DONE_SHIFT

#define TCON_LAYER_KICK_SHIFT            8UL
#define TCON_LAYER_KICK_MASK             0xFFFFF << TCON_LAYER_KICK_SHIFT

#define RDMA_INT_MSK                     0x1220
#define RDMA_INT_ST                      0x1224
#define RDMA_INT_MASK                    0x7F
#define RDMA_CH_6_SHIFT                  6UL
#define RDMA_CH_6_MASK                   1UL << RDMA_CH_6_SHIFT
#define RDMA_CH_5_SHIFT                  5UL
#define RDMA_CH_5_MASK                   1UL << RDMA_CH_5_SHIFT
#define RDMA_CH_4_SHIFT                  4UL
#define RDMA_CH_4_MASK                   1UL << RDMA_CH_4_SHIFT
#define RDMA_CH_3_SHIFT                  3UL
#define RDMA_CH_3_MASK                   1UL << RDMA_CH_3_SHIFT
#define RDMA_CH_2_SHIFT                  2UL
#define RDMA_CH_2_MASK                   1UL << RDMA_CH_2_SHIFT
#define RDMA_CH_1_SHIFT                  1UL
#define RDMA_CH_1_MASK                   1UL << RDMA_CH_1_SHIFT
#define RDMA_CH_0_SHIFT                  0UL
#define RDMA_CH_0_MASK                   1UL << RDMA_CH_0_SHIFT

#define DC_GP_CHN_BASE 0x2000
#define DC_SP_CHN_BASE 0x5000
#define DP_GP0_CHN_BASE 0x2000
#define DP_GP1_CHN_BASE 0x3000
#define DP_SP0_CHN_BASE 0x5000
#define DP_SP1_CHN_BASE 0x6000

#define GP_PIX_COMP                      0x0000
#define SP_PIX_COMP                      0x0000
#define BPV_SHIFT                        24UL
#define BPV_MASK                         0xF << BPV_SHIFT
#define BPU_SHIFT                        16UL
#define BPU_MASK                         0xF << BPU_SHIFT
#define BPY_SHIFT                        8UL
#define BPY_MASK                         0x1F << BPY_SHIFT
#define BPA_SHIFT                        0UL
#define BPA_MASK                         0xF << BPA_SHIFT

#define GP_FRM_CTRL                      0x0004
#define SP_FRM_CTRL                      0x0004
#define ENDIAN_CTRL_SHIFT                16UL
#define ENDIAN_CTRL_MASK                 0x7 << ENDIAN_CTRL_SHIFT
#define COMP_SWAP_SHIFT                  12UL
#define COMP_SWAP_MASK                   0xF << COMP_SWAP_SHIFT
#define ROT_SHIFT                        8UL
#define ROT_MASK                         0x7 << ROT_SHIFT
#define RGB_YUV_SHIFT                    7UL
#define RGB_YUV_MASK                     1UL << RGB_YUV_SHIFT
#define UV_SWAP_SHIFT                    6UL
#define UV_SWAP_MASK                     1UL << UV_SWAP_SHIFT
#define UV_MODE_SHIFT                    4UL
#define UV_MODE_MASK                     0x3 << UV_MODE_SHIFT
#define MODE_SHIFT                       2UL
#define MODE_MASK                        0x3 << MODE_SHIFT
#define FMT_SHIFT                        0UL
#define FMT_MASK                         0x3 << FMT_SHIFT

#define GP_FRM_SIZE                      0x0008
#define SP_FRM_SIZE                      0x0008
#define FRM_HEIGHT_SHIFT                 16UL
#define FRM_HEIGHT_MASK                  0xFFFF << FRM_HEIGHT_SHIFT
#define FRM_WIDTH_SHIFT                  0UL
#define FRM_WIDTH_MASK                   0xFFFF << FRM_WIDTH_SHIFT

#define GP_Y_BADDR_L                     0x000C
#define SP_Y_BADDR_L                     0x000C
#define GP_Y_BADDR_H                     0x0010
#define SP_Y_BADDR_H                     0x0010
#define GP_U_BADDR_L                     0x0014
#define GP_U_BADDR_H                     0x0018
#define GP_V_BADDR_L                     0x001C
#define GP_V_BADDR_H                     0x0020
#define BADDR_L_SHIFT                    0UL
#define BADDR_L_MASK                     0xFFFFFFFF << BADDR_L_SHIFT
#define BADDR_H_SHIFT                    0UL
#define BADDR_H_MASK                     0xFF << BADDR_H_SHIFT

#define GP_Y_STRIDE                      0x002C
#define SP_Y_STRIDE                      0x002C
#define GP_U_STRIDE                      0x0030
#define GP_V_STRIDE                      0x0034
#define STRIDE_SHIFT                     0UL
#define STRIDE_MASK                      0x3FFFFUL << STRIDE_SHIFT

#define GP_FRM_OFFSET                    0x0040
#define SP_FRM_OFFSET                    0x0040
#define FRM_Y_SHIFT                      16UL
#define FRM_Y_MASK                       0xFFFFUL << FRM_Y_SHIFT
#define FRM_X_SHIFT                      0UL
#define FRM_X_MASK                       0xFFFFUL << FRM_X_SHIFT

#define GP_YUVUP_CTRL                    0x0044
#define GP_YUVUP_EN_SHIFT                31UL
#define GP_YUVUP_EN_MASK                 0x1 << GP_YUVUP_EN_SHIFT
#define GP_YUVUP_VOFSET_SHIFT            6UL
#define GP_YUVUP_VOFSET_MASK             0x3 << GP_YUVUP_VOFSET_SHIFT
#define GP_YUVUP_HOFSET_SHIFT            4UL
#define GP_YUVUP_HOFSET_MASK             0x3 << GP_YUVUP_HOFSET_SHIFT
#define GP_YUVUP_FILTER_MODE_SHIFT       3UL
#define GP_YUVUP_FILTER_MODE_MASK        0x1 << GP_YUVUP_FILTER_MODE_SHIFT
#define GP_YUVUP_UPV_BYPASS_SHIFT        2UL
#define GP_YUVUP_UPV_BYPASS_MASK         0x1 << GP_YUVUP_UPV_BYPASS_SHIFT
#define GP_YUVUP_UPH_BYPASS_SHIFT        1UL
#define GP_YUVUP_UPH_BYPASS_MASK         0x1 << GP_YUVUP_UPH_BYPASS_SHIFT
#define GP_YUVUP_BYPASS_SHIFT            0UL
#define GP_YUVUP_BYPASS_MASK             0x1 << GP_YUVUP_BYPASS_SHIFT

#define GP_CSC_CTRL                      0x0200
#define GP_CSC_COEF1                     0x0204
#define GP_CSC_COEF2                     0x0208
#define GP_CSC_COEF3                     0x020C
#define GP_CSC_COEF4                     0x0210
#define GP_CSC_COEF5                     0x0214
#define GP_CSC_COEF6                     0x0218
#define GP_CSC_COEF7                     0x021C
#define GP_CSC_COEF8                     0x0220

#define GP_SWT_RST                       0x0E00
#define SP_SWT_RST                       0x0E00
#define SW_RST_SHIFT                     0UL
#define SW_RST_MASK                      0x1 << SW_RST_SHIFT

#define GP_SDW_TRIG                      0x0F00
#define SP_SDW_TRIG                      0x0F00
#define SDW_CTRL_TRIG_SHIFT              0UL
#define SDW_CTRL_TRIG_MASK               0x1 << SDW_CTRL_TRIG_SHIFT

#define SP_RLE_Y_LEN                     0x0100
#define RLE_Y_LEN_Y_SHIFT                0UL
#define RLE_Y_LEN_Y_MASK                 0xFFFFFF << RLE_Y_LEN_Y_SHIFT

#define SP_RLE_Y_CHECK_SUM               0x0110
#define RLE_Y_CHECK_SUM_Y_SHIFT          0UL
#define RLE_Y_CHECK_SUM_Y_MASK           0xFFFFFFFF << RLE_Y_CHECK_SUM_Y_SHIFT

#define SP_RLE_CTRL                      0x0120
#define RLE_DATA_SIZE_SHIFT              1UL
#define RLE_DATA_SIZE_MASK               0x3 << RLE_DATA_SIZE_SHIFT
#define RLE_EN_SHIFT                     0UL
#define RLE_EN_MASK                      0x1 << RLE_EN_SHIFT

#define SP_RLE_Y_CHECK_SUM_ST            0x0130
#define SP_RLE_U_CHECK_SUM_ST            0x0134
#define SP_RLE_V_CHECK_SUM_ST            0x0138
#define SP_RLE_A_CHECK_SUM_ST            0x013C
#define RLE_CHECK_SUM_SHIFT              0UL
#define RLE_CHECK_SUM_MASK               0xFFFFFFFF << RLE_CHECK_SUM_SHIFT

#define SP_RLE_INT_MASK                  0x0140
#define SP_RLE_INT_STATUS                0x0144
#define RLE_INT_V_ERR_SHIFT              3UL
#define RLE_INT_V_ERR_MASK               0x1 << RLE_INT_V_ERR_SHIFT
#define RLE_INT_U_ERR_SHIFT              2UL
#define RLE_INT_U_ERR_MASK               0x1 << RLE_INT_U_ERR_SHIFT
#define RLE_INT_Y_ERR_SHIFT              1UL
#define RLE_INT_Y_ERR_MASK               0x1 << RLE_INT_Y_ERR_SHIFT
#define RLE_INT_A_ERR_SHIFT              0UL
#define RLE_INT_A_ERR_MASK               0x1 << RLE_INT_A_ERR_SHIFT

#define SP_CLUT_A_CTRL                   0x0200
#define CLUT_HAS_ALPHA_SHIFT             18UL
#define CLUT_HAS_ALPHA_MASK              0x1 << CLUT_HAS_ALPHA_SHIFT
#define CLUT_A_Y_SEL_SHIFT               17UL
#define CLUT_A_Y_SEL_MASK                0x1 << CLUT_A_Y_SEL_SHIFT
#define CLUT_A_BYPASS_SHIFT              16UL
#define CLUT_A_BYPASS_MASK               0x1 << CLUT_A_BYPASS_SHIFT
#define CLUT_A_OFFSET_SHIFT              8UL
#define CLUT_A_OFFSET_MASK               0xFF << CLUT_A_OFFSET_SHIFT
#define CLUT_A_DEPTH_SHIFT               0UL
#define CLUT_A_DEPTH_MASK                0xF << CLUT_A_DEPTH_SHIFT

#define CLUT_Y_CTRL                      0x0204
#define CLUT_Y_BYPASS_SHIFT              16UL
#define CLUT_Y_BYPASS_MASK               0x1 << CLUT_Y_BYPASS_SHIFT
#define CLUT_Y_OFFSET_SHIFT              8UL
#define CLUT_Y_OFFSET_MASK               0xFF << CLUT_Y_OFFSET_SHIFT
#define CLUT_Y_DEPTH_SHIFT               0UL
#define CLUT_Y_DEPTH_MASK                0xF << CLUT_Y_DEPTH_SHIFT

#define CLUT_U_CTRL                      0x0208
#define CLUT_U_Y_SEL_SHIFT               17UL
#define CLUT_U_Y_SEL_MASK                0x1 << CLUT_U_Y_SEL_SHIFT
#define CLUT_U_BYPASS_SHIFT              16UL
#define CLUT_U_BYPASS_MASK               0x1 << CLUT_U_BYPASS_SHIFT
#define CLUT_U_OFFSET_SHIFT              8UL
#define CLUT_U_OFFSET_MASK               0xFF << CLUT_U_OFFSET_SHIFT
#define CLUT_U_DEPTH_SHIFT               0UL
#define CLUT_U_DEPTH_MASK                0xF << CLUT_U_DEPTH_SHIFT

#define CLUT_V_CTRL                      0x020c
#define CLUT_V_Y_SEL_SHIFT               17UL
#define CLUT_V_Y_SEL_MASK                0x1 << CLUT_V_Y_SEL_SHIFT
#define CLUT_V_BYPASS_SHIFT              16UL
#define CLUT_V_BYPASS_MASK               0x1 << CLUT_V_BYPASS_SHIFT
#define CLUT_V_OFFSET_SHIFT              8UL
#define CLUT_V_OFFSET_MASK               0xFF << CLUT_V_OFFSET_SHIFT
#define CLUT_V_DEPTH_SHIFT               0UL
#define CLUT_V_DEPTH_MASK                0xF << CLUT_V_DEPTH_SHIFT

#define CLUT_READ_CTRL                   0x0210
#define CLUT_APB_SEL_SHIFT               0UL
#define CLUT_APB_SEL_MASK                0x1 << CLUT_APB_SEL_SHIFT

#define CLUT_BADDRL                      0x0214
#define CLUT_BADDRL_SHIFT                0UL
#define CLIT_BADDRL_MASK                 0xFFFFFFFF << CLUT_BADDRL_SHIFT

#define CLUT_BADDRH                      0x0218
#define CLUT_BADDRH_SHIFT                0UL
#define CLIT_BADDRH_MASK                 0xFF << CLUT_BADDRH_SHIFT

#define CLUT_LOAD_CTRL                   0x021c
#define CLUT_LOAD_CTRL_EN_SHIFT          0UL
#define CLUT_LOAD_CTRL_EN_MASK           1 << CLUT_LOAD_CTRL_EN_SHIFT


#define MLC_INT_MSK                      0x7240
#define MLC_INT_MSK_MASK                 0x1FFF

#define MLC_INT_ST                       0x7244
#define MLC_INT_ST_MASK                  0xFFFFFFF

#define MLC_SLOWD_L_5_SHIFT       27UL
#define MLC_SLOWD_L_5_MASK        0x1 << MLC_SLOWD_L_5_SHIFT
#define MLC_SLOWD_L_4_SHIFT       26UL
#define MLC_SLOWD_L_4_MASK        0x1 << MLC_SLOWD_L_4_SHIFT
#define MLC_SLOWD_L_3_SHIFT       25U
#define MLC_SLOWD_L_3_MASK        0x1 << MLC_SLOWD_L_3_SHIFT
#define MLC_SLOWD_L_2_SHIFT       24UL
#define MLC_SLOWD_L_2_MASK        0x1 << MLC_SLOWD_L_2_SHIFT
#define MLC_SLOWD_L_1_SHIFT       23UL
#define MLC_SLOWD_L_1_MASK        0x1 << MLC_SLOWD_L_1_SHIFT
#define MLC_SLOWD_L_0_SHIFT       22UL
#define MLC_SLOWD_L_0_MASK        0x1 << MLC_SLOWD_L_0_SHIFT
#define MLC_CROP_E_L_5_SHIFT      21UL
#define MLC_CROP_E_L_5_MASK       0x1 << MLC_CROP_E_L_5_SHIFT
#define MLC_CROP_E_L_4_SHIFT      20UL
#define MLC_CROP_E_L_4_MASK       0x1 << MLC_CROP_E_L_4_SHIFT
#define MLC_CROP_E_L_3_SHIFT      19UL
#define MLC_CROP_E_L_3_MASK       0x1 << MLC_CROP_E_L_3_SHIFT
#define MLC_CROP_E_L_2_SHIFT      18UL
#define MLC_CROP_E_L_2_MASK       0x1 << MLC_CROP_E_L_2_SHIFT
#define MLC_CROP_E_L_1_SHIFT      17UL
#define MLC_CROP_E_L_1_MASK       0x1 << MLC_CROP_E_L_1_SHIFT
#define MLC_CROP_E_L_0_SHIFT      16UL
#define MLC_CROP_E_L_0_MASK       0x1 << MLC_CROP_E_L_0_SHIFT
#define MLC_E_L_5_SHIFT           12UL
#define MLC_E_L_5_MASK            0x1 << MLC_E_L_5_SHIFT
#define MLC_E_L_4_SHIFT           11UL
#define MLC_E_L_4_MASK            0x1 << MLC_E_L_4_SHIFT
#define MLC_E_L_3_SHIFT           10UL
#define MLC_E_L_3_MASK            0x1 << MLC_E_L_3_SHIFT
#define MLC_E_L_2_SHIFT           9UL
#define MLC_E_L_2_MASK            0x1 << MLC_E_L_2_SHIFT
#define MLC_E_L_1_SHIFT           8UL
#define MLC_E_L_1_MASK            0x1 << MLC_E_L_1_SHIFT
#define MLC_E_L_0_SHIFT           7UL
#define MLC_E_L_0_MASK            0x1 << MLC_E_L_0_SHIFT
#define MLC_FLU_L_5_SHIFT         6UL
#define MLC_FLU_L_5_MASK          0x1 << MLC_FLU_L_5_SHIFT
#define MLC_FLU_L_4_SHIFT         5UL
#define MLC_FLU_L_4_MASK          0x1 << MLC_FLU_L_4_SHIFT
#define MLC_FLU_L_3_SHIFT         4UL
#define MLC_FLU_L_3_MASK          0x1 << MLC_FLU_L_3_SHIFT
#define MLC_FLU_L_2_SHIFT         3UL
#define MLC_FLU_L_2_MASK          0x1 << MLC_FLU_L_2_SHIFT
#define MLC_FLU_L_1_SHIFT         2UL
#define MLC_FLU_L_1_MASK          0x1 << MLC_FLU_L_1_SHIFT
#define MLC_FLU_L_0_SHIFT         1UL
#define MLC_FLU_L_0_MASK          0x1 << MLC_FLU_L_0_SHIFT
#define MLC_FRM_END_SHIFT         0UL
#define MLC_FRM_END_MASK          0x1 << MLC_FRM_END_SHIFT


#define CRC32_INT_ST                0xE004
#define CRC32_INT_MSK               0xE008
#define CRC32_INT_MASK              0xFFFF

#define CRC_ERROR_7_SHIFT           15UL
#define CRC_ERROR_7_MASK            0x1 << CRC_ERROR_7_SHIFT
#define CRC_ERROR_6_SHIFT           14UL
#define CRC_ERROR_6_MASK            0x1 << CRC_ERROR_6_SHIFT
#define CRC_ERROR_5_SHIFT           13UL
#define CRC_ERROR_5_MASK            0x1 << CRC_ERROR_5_SHIFT
#define CRC_ERROR_4_SHIFT           12UL
#define CRC_ERROR_4_MASK            0x1 << CRC_ERROR_4_SHIFT
#define CRC_ERROR_3_SHIFT           11UL
#define CRC_ERROR_3_MASK            0x1 << CRC_ERROR_3_SHIFT
#define CRC_ERROR_2_SHIFT           10UL
#define CRC_ERROR_2_MASK            0x1 << CRC_ERROR_2_SHIFT
#define CRC_ERROR_1_SHIFT           9UL
#define CRC_ERROR_1_MASK            0x1 << CRC_ERROR_1_SHIFT
#define CRC_ERROR_0_SHIFT           8UL
#define CRC_ERROR_0_MASK            0x1 << CRC_ERROR_0_SHIFT
#define CRC_DONE_7_SHIFT            7UL
#define CRC_DONE_7_MASK             0x1 << CRC_DONE_7_SHIFT
#define CRC_DONE_6_SHIFT            6UL
#define CRC_DONE_6_MASK             0x1 << CRC_DONE_6_SHIFT
#define CRC_DONE_5_SHIFT            5UL
#define CRC_DONE_5_MASK             0x1 << CRC_DONE_5_SHIFT
#define CRC_DONE_4_SHIFT            4UL
#define CRC_DONE_4_MASK             0x1 << CRC_DONE_4_SHIFT
#define CRC_DONE_3_SHIFT            3UL
#define CRC_DONE_3_MASK             0x1 << CRC_DONE_3_SHIFT
#define CRC_DONE_2_SHIFT            2UL
#define CRC_DONE_2_MASK             0x1 << CRC_DONE_2_SHIFT
#define CRC_DONE_1_SHIFT            1UL
#define CRC_DONE_1_MASK             0x1 << CRC_DONE_1_SHIFT
#define CRC_DONE_0_SHIFT            0UL
#define CRC_DONE_0_MASK             0x1 << CRC_DONE_0_SHIFT


/* CTRL */
#define DC_CTRL                          0x0
#define DC_CTRL_SW_RST_SHIFT             31UL
#define DC_CTRL_SW_RST_MASK              1UL << DC_CTRL_SW_RST_SHIFT
#define DC_CTRL_MS_MODE_SHIFT            1UL
#define DC_CTRL_MS_MODE_MASK             0x1 << DC_CTRL_MS_MODE_SHIFT
#define DC_CTRL_SF_MODE_SHIFT            0UL
#define DC_CTRL_SF_MODE_MASK             1UL << DC_CTRL_SF_MODE_SHIFT
#define DC_CTRL_URC_SHIFT                3UL
#define DC_CTRL_URC_MASK                 1UL << DC_CTRL_URC_SHIFT
#define DC_CTRL_MLC_DM_SHIFT             2UL
#define DC_CTRL_MLC_DM_MASK              1UL << DC_CTRL_MLC_DM_SHIFT

/* FLC */
#define DC_FLC_CTRL                      0x04
#define CRC32_TRIG_SHIFT                 3UL
#define CRC32_TRIG_MASK                  1UL << CRC32_TRIG_SHIFT

#define TCON_TRIG_SHIFT                  2UL
#define TCON_TRIG_MASK                   1UL << TCON_TRIG_SHIFT

#define DI_TRIG_SHIFT                    1UL
#define DI_TRIG_MASK                     1UL << DI_TRIG_SHIFT

#define FLC_TRIG_SHIFT                   0UL
#define FLC_TRIG_MASK                    1 << FLC_TRIG_SHIFT

#define DC_FLC_UPDATE                    0x08
#define UPDATE_FORCE_SHIFT               0UL
#define UPDATE_FORCE_MASK                1UL << UPDATE_FORCE_SHIFT

#define DC_SDMA_CTRL                     0x10
#define SDMA_EN_SHIFT                    0
#define SDMA_EN_MASK                     1UL << SDMA_EN_SHIFT

/* RDMA */
#define RDMA_CHN_JMP 0x20
#define RDMA_DC_CHN_COUNT 4
#define RDMA_DFIFO_WML                    0x1000
#define RDMA_DFIFO_WML_SHIFT              0UL
#define RDMA_DFIFO_WML_MASK               (0xFFFF << RDMA_DFIFO_WML_SHIFT)

#define RDMA_DFIFO_DEPTH                  0x1004
#define DFIFO_DEPTH_SHIFT                 0UL
#define DFIFO_DEPTH_MASK                  (0xFFFF << DFIFO_DEPTH_SHIFT)

#define RDMA_CFIFO_DEPTH                  0x1008
#define CFIFO_DEPTH_SHIFT                 0UL
#define CFIFO_DEPTH_MASK                  (0xFFFF << CFIFO_DEPTH_SHIFT)

#define RDMA_CH_PRIO                      0x100C
#define PRIO_SCHE_SHIFT                   16UL
#define PRIO_SCHE_MASK                    0x3f << PRIO_SCHE_SHIFT
#define PRIO_P1_SHIFT                     8UL
#define PRIO_P1_MASK                      0x3f << PRIO_P1_SHIFT
#define PRIO_P0_SHIFT                     0UL
#define PRIO_P0_MASK                      0x3f << PRIO_P0_SHIFT

#define RDMA_BURST                        0x1010
#define BURST_MODE_SHIFT                  3UL
#define BURST_MODE_MASK                   0x1UL << BURST_MODE_SHIFT
#define BURST_LEN_SHIFT                   0UL
#define BURST_LEN_MASK                    0x7UL << BURST_LEN_SHIFT

#define RDMA_AXI_USER                     0x1014
#define AXI_USER_SHIFT                    0UL
#define AXI_USER_MASK                     0xFFFFF << AXI_USER_SHIFT

#define RDMA_AXI_CTRL                     0x1018
#define AXI_CTRL_PORT_SHIFT               4UL
#define AXI_CTRL_PORT_MASK                0x3 << AXI_CTRL_PORT_SHIFT
#define AXI_CTRL_CACHE_SHIFT              0UL
#define AXI_CTRL_CACHE_MASK               0xF << AXI_CTRL_CACHE_SHIFT

#define RDMA_CTRL                        0x1400
#define CTRL_CFG_LOAD_SHIFT              1UL
#define CTRL_CFG_LOAD_MASK               0x1UL << CTRL_CFG_LOAD_SHIFT
#define CTRL_ARB_SEL_SHIFT               0UL
#define CTRL_ARB_SEL_MASK                0x1UL << CTRL_ARB_SEL_SHIFT

#define RDMA_DFIFO_FULL                  0x1500
#define RDMA_DFIFO_EMPTY                 0x1504
#define RDMA_CFIFO_FULL                  0x1508
#define RDMA_CFIFO_EMPTY                 0x150C
#define RDMA_CH_IDLE                     0x1510
#define RDMA_DC_CH_MASK                     ((1 << RDMA_DC_CHN_COUNT) - 1)

/* MLC */
#define MLC_LAYER_JMP              0x30
#define MLC_LAYER_COUNT            4UL
#define MLC_PATH_JMP               0x4
#define MLC_PATH_COUNT             5UL

#define MLC_SF_CTRL                0x7000
#define PROT_VAL_SHIFT             8UL
#define PROT_VAL_MASK              0x3F << PROT_VAL_SHIFT
#define VPOS_PROT_EN_SHIFT         7UL
#define VPOS_PROT_EN_MASK          0x1 << VPOS_PROT_EN_SHIFT
#define SLOWDOWN_EN_SHIFT          6UL
#define SLOWDOWN_EN_MASK           0x1 << SLOWDOWN_EN_SHIFT
#define AFLU_PSEL_SHIFT            5UL
#define AFLU_PSEL_MASK             0x1 << AFLU_PSEL_SHIFT
#define AFLU_EN_SHIFT              4UL
#define AFLU_EN_MASK               0x1 << AFLU_EN_SHIFT
#define CKEY_EN_SHIFT              3UL
#define CKEY_EN_MASK               0x1 << CKEY_EN_SHIFT
#define G_ALPHA_EN_SHIFT           2UL
#define G_ALPHA_EN_MASK            0x1 << G_ALPHA_EN_SHIFT
#define CROP_EN_SHIFT              1UL
#define CROP_EN_MASK               0x1 << CROP_EN_SHIFT
#define MLC_SF_EN_SHIFT            0UL
#define MLC_SF_EN_MASK             0x1 << MLC_SF_EN_SHIFT

#define MLC_SF_H_SPOS              0x7004
#define SPOS_H_SHIFT               0UL
#define SPOS_H_MASK                0x1FFFF << SPOS_H_SHIFT

#define MLC_SF_V_SPOS              0x7008
#define SPOS_V_SHIFT               0UL
#define SPOS_V_MASK                0x1FFFF << SPOS_V_SHIFT

#define MLC_SF_SIZE                0x700C
#define SIZE_V_SHIFT               16UL
#define SIZE_V_MASK                0xFFFF << SIZE_V_SHIFT
#define SIZE_H_SHIFT               0UL
#define SIZE_H_MASK                0xFFFF << SIZE_H_SHIFT

#define MLC_SF_CROP_H_POS          0x7010
#define MLC_SF_CROP_V_POS          0x7014
#define CROP_END_SHIFT             16UL
#define CROP_END_MASK              0xFFFF << CROP_END_SHIFT
#define CROP_START_SHIFT           0UL
#define CROP_START_MASK            0xFFFF << CROP_START_SHIFT

#define MLC_SF_G_ALPHA             0x7018
#define G_ALPHA_A_SHIFT            0UL
#define G_ALPHA_A_MASK             0xFF << G_ALPHA_A_SHIFT

#define MLC_SF_CKEY_ALPHA          0x701C
#define CKEY_ALPHA_A_SHIFT         0UL
#define CKEY_ALPHA_A_MASK          0xFF << CKEY_ALPHA_A_SHIFT

#define MLC_SF_CKEY_R_LV           0x7020
#define MLC_SF_CKEY_G_LV           0x7024
#define MLC_SF_CKEY_B_LV           0x7028
#define CKEY_LV_UP_SHIFT           16UL
#define CKEY_LV_UP_MASK            0x3FF << CKEY_LV_UP_SHIFT
#define CKEY_LV_DN_SHIFT           0UL
#define CKEY_LV_DN_MASK            0x3FF << CKEY_LV_DN_SHIFT

#define MLC_SF_AFLU_TIME           0x702C
#define AFLU_TIMER_SHIFT           0UL
#define AFLU_TIMER_MASK            0xFFFFFFFF << AFLU_TIMER_SHIFT

#define MLC_PATH_CTRL              0x7200
#define ALPHA_BLD_IDX_SHIFT        16UL
#define ALPHA_BLD_IDX_MASK         0xF << ALPHA_BLD_IDX_SHIFT
#define LAYER_OUT_IDX_SHIFT        0UL
#define LAYER_OUT_IDX_MASK         0xF << LAYER_OUT_IDX_SHIFT

#define MLC_BG_CTRL                0x7220
#define BG_A_SHIFT                 8UL
#define BG_A_MASK                  0xFF << BG_A_SHIFT
#define BG_AFLU_EN_SHIFT           7UL
#define BG_AFLU_EN_MASK            0x1 << BG_AFLU_EN_SHIFT
#define FSTART_SEL_SHIFT           4UL
#define FSTART_SEL_MASK            0x7 << FSTART_SEL_SHIFT
#define BG_A_SEL_SHIFT             2UL
#define BG_A_SEL_MASK              0x1 << BG_A_SEL_SHIFT
#define BG_EN_SHIFT                1UL
#define BG_EN_MASK                 0x1 << BG_EN_SHIFT
#define ALPHA_BLD_BYPS_SHIFT       0UL
#define ALPHA_BLD_BYPS_MASK        0x1 << ALPHA_BLD_BYPS_SHIFT

#define MLC_BG_COLOR               0x7224
#define BG_COLOR_R_SHIFT           20UL
#define BG_COLOR_R_MASK            0x3FF << BG_COLOR_R_SHIFT
#define BG_COLOR_G_SHIFT           10UL
#define BG_COLOR_G_MASK            0x3FF << BG_COLOR_G_SHIFT
#define BG_COLOR_B_SHIFT           0UL
#define BG_COLOR_B_MASK            0x3FF << BG_COLOR_B_SHIFT

#define MLC_BG_AFLU_TIME           0x7228
#define BG_AFLU_TIMER_SHIFT        0
#define BG_AFLU_TIMER_MASK         0xFFFFFFFF << BG_AFLU_TIMER_SHIFT

#define MLC_CANVAS_COLOR           0x7230
#define CANVAS_COLOR_R_SHIFT       20UL
#define CANVAS_COLOR_R_MASK        0x3FF << CANVAS_COLOR_R_SHIFT
#define CANVAS_COLOR_G_SHIFT       10UL
#define CANVAS_COLOR_G_MASK        0x3FF << CANVAS_COLOR_G_SHIFT
#define CANVAS_COLOR_B_SHIFT       0UL
#define CANVAS_COLOR_B_MASK        0x3FF << CANVAS_COLOR_B_SHIFT

#define MLC_CLK_RATIO              0x7234
#define CLK_RATIO_SHIFT            0UL
#define CLK_RATIO_MASK             0xFFFF << CLK_RATIO_SHIFT


/* TCON */
#define KICK_LAYER_JMP              0x8
#define KICK_LAYER_COUNT            18UL

#define TCON_H_PARA_1               0x9000
#define HACT_SHIFT                  16UL
#define HACT_MASK                   0xFFFF << HACT_SHIFT
#define HTOL_SHIFT                  0UL
#define HTOL_MASK                   0xFFFF << HTOL_SHIFT

#define TCON_H_PARA_2               0x9004
#define HSBP_SHIFT                  16UL
#define HSBP_MASK                   0xFFFF << HSBP_SHIFT
#define HSYNC_SHIFT                 0UL
#define HSYNC_MASK                  0xFFFF << HSYNC_SHIFT

#define TCON_V_PARA_1               0x9008
#define VACT_SHIFT                  16UL
#define VACT_MASK                   0xFFFF << VACT_SHIFT
#define VTOL_SHIFT                  0UL
#define VTOL_MASK                   0xFFFF << VTOL_SHIFT

#define TCON_V_PARA_2               0x900C
#define VSBP_SHIFT                  16UL
#define VSBP_MASK                   0xFFFF << VSBP_SHIFT
#define VSYNC_SHIFT                 0UL
#define VSYNC_MASK                  0xFFFF << VSYNC_SHIFT


#define TCON_CTRL                   0x9010
#define PIX_SCR_SHIFT               6UL
#define PIX_SCR_MASK                0x3 << PIX_SCR_SHIFT
#define DSP_CLK_EN_SHIFT            5UL
#define DSP_CLK_EN_MASK             0x1 << DSP_CLK_EN_SHIFT
#define DSP_CLK_POL_SHIFT           4UL
#define DSP_CLK_POL_MASK            0x1 << DSP_CLK_POL_SHIFT
#define DE_POL_SHITF                3UL
#define DE_POL_MASK                 0x1 << DE_POL_SHITF
#define VSYNC_POL_SHIFT             2UL
#define VSYNC_POL_MASK              0x1 << VSYNC_POL_SHIFT
#define HSYNC_POL_SHIFT             1UL
#define HSYNC_POL_MASK              0x1 << HSYNC_POL_SHIFT
#define EN_SHIFT                    0UL
#define EN_MASK                     0x1 << EN_SHIFT

#define TCON_LAYER_KICK_COOR        0x9020
#define LAYER_KICK_Y_SHIFT          16UL
#define LAYER_KICK_Y_MASK           0xFFFF << LAYER_KICK_Y_SHIFT
#define LAYER_KICK_X_SHIFT          0UL
#define LAYER_KICK_X_MASK           0xFFFF << LAYER_KICK_X_SHIFT

#define TCON_LAYER_KICK_EN          0x9024
#define LAYER_KICK_EN_SHIFT         0UL
#define LAYER_KICK_EN_MASK          0x1 << LAYER_KICK_EN_SHIFT

#define TCON_UNDERRUN_CNT           0x9100
#define UNDERRUN_S_SHIFT            0
#define UNDERRUN_S_MASK             0xFFFFFFFF << UNDERRUN_S_SHIFT


/* DC_CSC */
#define DC_CSC_JMP                  0x1000
#define DC_CSC_COUNT                2UL
#define DC_CSC_CTRL                 0xA000
#define CSC_ALPHA_SHIFT             2UL
#define CSC_ALPHA_MASK              0x1 << CSC_ALPHA_SHIFT
#define CSC_SBUP_CONV_SHIFT         1UL
#define CSC_SBUP_CONV_MASK          0x1 << CSC_SBUP_CONV_SHIFT
#define CSC_BYPASS_SHIFT            0UL
#define CSC_BYPASS_MASK             0x1 << CSC_BYPASS_SHIFT

#define DC_CSC_COEF1                0xA004
#define COEF1_A01_SHIFT             16UL
#define COEF1_A01_MASK              0x3FFF << COEF1_A01_SHIFT
#define COEF1_A00_SHIFT             0UL
#define COEF1_A00_MASK              0x3FFF << COEF1_A00_SHIFT

#define DC_CSC_COEF2                0xA008
#define COEF2_A10_SHIFT             16UL
#define COEF2_A10_MASK              0x3FFF << COEF2_A10_SHIFT
#define COEF2_A02_SHIFT             0UL
#define COEF2_A02_MASK              0x3FFF << COEF2_A02_SHIFT

#define DC_CSC_COEF3                0xA00C
#define COEF3_A12_SHIFT             16UL
#define COEF3_A12_MASK              0x3FFF << COEF3_A12_SHIFT
#define COEF3_A11_SHIFT             0
#define COEF3_A11_MASK              0x3FFF << COEF3_A11_SHIFT

#define DC_CSC_COEF4                0xA010
#define COEF4_A21_SHIFT             16UL
#define COEF4_A21_MASK              0x3FFF << COEF4_A21_SHIFT
#define COEF4_A20_SHIFT             0UL
#define COEF4_A20_MASK              0x3FFF << COEF4_A20_SHIFT

#define DC_CSC_COEF5                0xA014
#define COEF5_B0_SHIFT              16UL
#define COEF5_B0_MASK               0x3FFF << COEF5_B0_SHIFT
#define COEF5_A22_SHIFT             0UL
#define COEF5_A22_MASK              0x3FFF << COEF5_A22_SHIFT

#define DC_CSC_COEF6                0xA018
#define COEF6_B2_SHIFT              16UL
#define COEF6_B2_MASK               0x3FFF << COEF6_B2_SHIFT
#define COEF6_B1_SHIFT              0UL
#define COEF6_B1_MASK               0x3FFF << COEF6_B1_SHIFT

#define DC_CSC_COEF7                0xA01C
#define COEF7_C1_SHIFT              16UL
#define COEF7_C1_MASK               0x3FF << COEF7_C1_SHIFT
#define COEF7_C0_SHIFT              0UL
#define COEF7_C0_MASK               0x3FF << COEF7_C0_SHIFT

#define DC_CSC_COEF8                0xA020
#define COEF8_C2_SHIFT              0UL
#define COEF8_C2_MASK               0x3FF << COEF8_C2_SHIFT

#define GAMMA_CTRL                  0xC000
#define APB_RD_TO_SHIFT             8UL
#define APB_RD_TO_MASK              0xFF << APB_RD_TO_SHIFT
#define GMMA_BYPASS_SHIFT           0UL
#define GMMA_BYPASS_MASK            0x1 << GMMA_BYPASS_SHIFT

#define DITHER_CTRL                 0xC004
#define V_DEP_SHIFT                 16UL
#define V_DEP_MASK                  0xF << V_DEP_SHIFT
#define U_DEP_SHIFT                 12UL
#define U_DEP_MASK                  0xF << U_DEP_SHIFT
#define Y_DEP_SHIFT                 8UL
#define Y_DEP_MASK                  0xF << Y_DEP_SHIFT
#define MODE_12_SHIFT               6UL
#define MODE_12_MASK                0x1 << MODE_12_SHIFT
#define SPA_LSB_EXP_SHIFT	        4UL
#define SPA_LSB_EXP_MASK	        0x3 << SPA_LSB_EXP_SHIFT
#define SPA_1ST_SHIFT		        3UL
#define SPA_1ST_MASK			    0x3 << SPA_1ST_SHIFT
#define SPA_EN_SHIFT                2UL
#define SPA_EN_MASK                 0x1 << SPA_EN_SHIFT
#define TEM_EN_SHIFT                1UL
#define TEM_EN_MASK    		        0x1 << TEM_EN_SHIFT
#define DITHER_BYPASS_SHIFT         0UL
#define DITHER_BYPASS_MASK          0x1 << DITHER_BYPASS_SHIFT

/* CRC32 */
#define CRC_BLK_JMP                 0x4
#define CRC_BLK_COUNT               8UL

#define CRC32_CTRL                  0xE004
#define CRC_VSYNC_POL_SHIFT         9UL
#define CRC_VSYNC_POL_MASK          0x1 << CRC_VSYNC_POL_SHIFT
#define CRC_HSYNC_POL_SHIFT         8UL
#define CRC_HSYNC_POL_MASK          0x1 << CRC_HSYNC_POL_SHIFT
#define CRC_DATA_EN_POL_SHIFT       7UL
#define CRC_DATA_EN_POL_MASK        0x1 << CRC_DATA_EN_POL_SHIFT
#define GLOBAL_ENABLE_SHIFT         0UL
#define GLOBAL_ENABLE_MASK          0x1 << GLOBAL_ENABLE_SHIFT

#define CRC32_BLOCK_CTRL0           0xE010
#define BLOCK_ENABLE_SHIFT          31UL
#define BLOCK_ENABLE_MASK           0x1 << BLOCK_ENABLE_SHIFT
#define BLOCK_LOCK_SHIFT            30UL
#define BLOCK_LOCK_MASK             0X1 << BLOCK_LOCK_SHIFT
#define POS_START_Y_SHIFT           16UL
#define POS_START_Y_MASK            0x3FFF << POS_START_Y_SHIFT
#define POS_START_X_SHIFT           0UL
#define POS_START_X_MASK            0x3FFF << POS_START_X_SHIFT

#define CRC32_BLOCK_CTRL1           0xE014
#define POS_END_Y_SHIFT             16UL
#define POS_END_Y_MASK              0x3FFF << POS_END_Y_SHIFT
#define POS_END_X_SHIFT             0UL
#define POS_END_X_MASK              0x3FFF << POS_END_X_SHIFT

#define CRC32_BLOCK_EXPECT_DATA     0xE018
#define EXPECT_DATA_SHIFT           0UL
#define EXPECT_DATA_MASK            0xFFFFFFFF << EXPECT_DATA_SHIFT

#define CRC32_BLOCK_RESULT_DATA     0xE01C
#define RESULT_DATA_SHIFT           0UL
#define RESULT_DATA_MASK            0xFFFFFFFF << RESULT_DATA_SHIFT

#endif
