/*
 * kunlun_drm_dp_reg.h
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef _KUNLUN_DRM_DP_REG_H_
#define _KUNLUN_DRM_DP_REG_H_
#include "kunlun_drm_reg.h"

/* DP CTRL */
#define DP_CTRL                          0x0

#define DP_CTRL_SW_RST_SHIFT             31UL
#define DP_CTRL_SW_RST_MASK              1UL << DP_CTRL_SW_RST_SHIFT
#define DP_RE_RAM_TYPE_SHIFT             3UL
#define DP_RE_RAM_TYPE_MASK              0x3 << DP_RE_RAM_TYPE_SHIFT
#define DP_CTRL_MLC_DM_SHIFT             2UL
#define DP_CTRL_MLC_DM_MASK              1UL << DP_CTRL_MLC_DM_SHIFT
#define DP_FLC_KICK_SEL_SHIFT            1UL
#define DP_FLC_KICK_SEL_MASK             1UL << DP_FLC_KICK_SEL_SHIFT
#define DP_EN_SHIFT                      0UL
#define DP_EN_MASK                       1UL << DP_EN_SHIFT

/* DP FLC CTRL*/
#define DP_FLC_CTRL                      0x04

/* DP FLC UPDATE*/
#define DP_FLC_UPDATE                    0x08

/* DP SIZE*/
#define DP_SIZE                          0x0C

#define DP_OUT_V_SHIFT                   16UL
#define DP_OUT_V_MASK                    0xFFFF << DP_OUT_V_SHIFT
#define DP_OUT_H_SHIFT                   0UL
#define DP_OUT_H_MASK                    0xFFFF << DP_OUT_H_SHIFT

/*DP INIT MASK&&STATUS*/
#define DP_INT_MSK                       0x20
#define DP_INT_ST                        0x24
#define DP_INT_MASK                      0x7F

#define DP_RDMA_SHIFT                    0UL
#define DP_RDMA_MASK                     1UL << DP_RDMA_SHIFT

#define DP_RLE_SHIFT                     1UL
#define DP_RLE_MASK                      1UL << DP_RLE_SHIFT

#define DP_MLC_SHIFT                     2UL
#define DP_MLC_MASK                      1UL << DP_MLC_SHIFT

#define DP_RLE1_SHIFT                    3UL
#define DP_RLE1_MASK                     1UL << DP_RLE1_SHIFT

#define DP_SDMA_DONE_SHIFT               4UL
#define DP_SDMA_DONE_MASK                1UL << DP_SDMA_DONE_SHIFT

#define DP_SOF_SHIFT                     5UL
#define DP_SOF_MASK                      1UL << DP_SOF_SHIFT

#define DP_EOF_SHIFT                     6UL
#define DP_EOF_MASK                      1UL << DP_EOF_SHIFT

/*DC RDMA CHN*/
#define RDMA_DP_CHN_COUNT                9
/*DC RDMA CTRL*/
#define DP_RDMA_CTRL                     0x1400

#define DP_RDMA_DFIFO_FULL               0x1500
#define DP_RDMA_DFIFO_EMPTY              0x1504
#define DP_RDMA_CFIFO_FULL               0x1508
#define DP_RDMA_CFIFO_EMPTY              0x150C
#define DP_RDMA_CH_IDLE                  0x1510

#define RDMA_DP_CH_MASK                  0x3FFFF

#define DP_RDMA_INT_MSK                  0x1520
#define DP_RDMA_INT_ST                   0x1524

#define DP_RDMA_INT_MASK                 0x3FFFF

#define DP_RDMA_CH_17_SHIFT              17UL
#define DP_RDMA_CH_17_MASK               1UL << DP_RDMA_CH_17_SHIFT
#define DP_RDMA_CH_16_SHIFT              16UL
#define DP_RDMA_CH_16_MASK               1UL << DP_RDMA_CH_16_SHIFT
#define DP_RDMA_CH_15_SHIFT              15UL
#define DP_RDMA_CH_15_MASK               1UL << DP_RDMA_CH_15_SHIFT
#define DP_RDMA_CH_14_SHIFT              14UL
#define DP_RDMA_CH_14_MASK               1UL << DP_RDMA_CH_14_SHIFT
#define DP_RDMA_CH_13_SHIFT              13UL
#define DP_RDMA_CH_13_MASK               1UL << DP_RDMA_CH_13_SHIFT
#define DP_RDMA_CH_12_SHIFT              12UL
#define DP_RDMA_CH_12_MASK               1UL << DP_RDMA_CH_12_SHIFT
#define DP_RDMA_CH_11_SHIFT              11UL
#define DP_RDMA_CH_11_MASK               1UL << DP_RDMA_CH_11_SHIFT
#define DP_RDMA_CH_10_SHIFT              10UL
#define DP_RDMA_CH_10_MASK               1UL << DP_RDMA_CH_10_SHIFT
#define DP_RDMA_CH_9_SHIFT               9UL
#define DP_RDMA_CH_9_MASK                1UL << DP_RDMA_CH_9_SHIFT
#define DP_RDMA_CH_8_SHIFT               8UL
#define DP_RDMA_CH_8_MASK                1UL << DP_RDMA_CH_8_SHIFT
#define DP_RDMA_CH_7_SHIFT               7UL
#define DP_RDMA_CH_7_MASK                1UL << DP_RDMA_CH_7_SHIFT
#define DP_RDMA_CH_6_SHIFT               6UL
#define DP_RDMA_CH_6_MASK                1UL << DP_RDMA_CH_6_SHIFT
#define DP_RDMA_CH_5_SHIFT               5UL
#define DP_RDMA_CH_5_MASK                1UL << DP_RDMA_CH_5_SHIFT
#define DP_RDMA_CH_4_SHIFT               4UL
#define DP_RDMA_CH_4_MASK                1UL << DP_RDMA_CH_4_SHIFT
#define DP_RDMA_CH_3_SHIFT               3UL
#define DP_RDMA_CH_3_MASK                1UL << DP_RDMA_CH_3_SHIFT
#define DP_RDMA_CH_2_SHIFT               2UL
#define DP_RDMA_CH_2_MASK                1UL << DP_RDMA_CH_2_SHIFT
#define DP_RDMA_CH_1_SHIFT               1UL
#define DP_RDMA_CH_1_MASK                1UL << DP_RDMA_CH_1_SHIFT
#define DP_RDMA_CH_0_SHIFT               0UL
#define DP_RDMA_CH_0_MASK                1UL << DP_RDMA_CH_0_SHIFT


/*DP PIPES*/
#define DP_GP0_CHN_BASE 0x2000
#define DP_GP1_CHN_BASE 0x3000
#define DP_SP0_CHN_BASE 0x5000
#define DP_SP1_CHN_BASE 0x6000

#define DP_ENDIAN_CTRL_SHIFT             16UL
#define DP_ENDIAN_CTRL_MASK              0x7 << DP_ENDIAN_CTRL_SHIFT
#define DP_COMP_SWAP_SHIFT               12UL
#define DP_COMP_SWAP_MASK                0xF << DP_COMP_SWAP_SHIFT
#define DP_ROT_SHIFT                     9UL
#define DP_ROT_MASK                      0x7 << DP_ROT_SHIFT
#define DP_RGB_YUV_SHIFT                 8UL
#define DP_RGB_YUV_MASK                  1UL << DP_RGB_YUV_SHIFT
#define DP_UV_SWAP_SHIFT                 7UL
#define DP_UV_SWAP_MASK                  1UL << DP_UV_SWAP_SHIFT
#define DP_UV_MODE_SHIFT                 5UL
#define DP_UV_MODE_MASK                  0x3 << DP_UV_MODE_SHIFT
#define DP_MODE_SHIFT                    2UL
#define DP_MODE_MASK                     0x7 << DP_MODE_SHIFT
#define DP_FMT_SHIFT                     0UL
#define DP_FMT_MASK                      0x3 << DP_FMT_SHIFT

#define GP_TILE_CTRL                     0x0038
#define GP_TILE_TR_REQ_CNT_FI_SHIFT      25UL
#define GP_TILE_TR_REQ_CNT_FI_MASK       0x1 << GP_TILE_TR_REQ_CNT_FI_SHIFT
#define GP_TILE_TR_REQ_CNT_SHIFT         16UL
#define GP_TILE_TR_REQ_CNT_MASK          0x1FF << GP_TILE_TR_REQ_CNT_SHIFT
#define GP_TILE_TR_BUF_WML_SHIFT         12UL
#define GP_TILE_TR_BUF_WML_MASK          0xF << GP_TILE_TR_BUF_WML_SHIFT
#define GP_TILE_CTRL_RE_FIEO_BYPS_SHIFT  9UL
#define GP_TILE_CTRL_RE_FIEO_BYPS_MASK   1 << GP_TILE_CTRL_RE_FIEO_BYPS_SHIFT
#define GP_TILE_CTRL_RE_MODE_SHIFT       8UL
#define GP_TILE_CTRL_RE_MODE_MASK        1 << GP_TILE_CTRL_RE_MODE_SHIFT
#define GP_TILE_CTRL_VSIZE_SHIFT         4UL
#define GP_TILE_CTRL_VSIZE_MASK          0x7 << GP_TILE_CTRL_VSIZE_SHIFT
#define GP_TILE_CTRL_HSIZE_SHIFT         0UL
#define GP_TILE_CTRL_HSIZE_MASK          0x7 << GP_TILE_CTRL_HSIZE_SHIFT

#define GP_CROP_CTRL                     0x0100
#define GP_CROP_BYPASS_SHIFT             0UL
#define GP_CROP_BYPASS_MASK              0x1 << GP_CROP_BYPASS_SHIFT

#define GP_CROP_UL_POS                   0x0104
#define GP_CROP_UL_POS_Y_SHIFT           16UL
#define GP_CROP_UL_POS_Y_MASK            0xFFFF << GP_CROP_UL_POS_Y_SHIFT
#define GP_CROP_UL_POS_X_SHIFT           0UL
#define GP_CROP_UL_POS_X_MASK            0xFFFF << GP_CROP_UL_POS_X_SHIFT

#define GP_CROP_SIZE                     0x0108
#define GP_CROP_SIZE_V_SHIFT             16UL
#define GP_CROP_SIZE_V_MASK              0xFFFF << GP_CROP_SIZE_V_SHIFT
#define GP_CROP_SIZE_H_SHIFT             0UL
#define GP_CROP_SIZE_H_MASK              0xFFFF << GP_CROP_SIZE_H_SHIFT

#define GP_CROP_PAR_ERR                  0x0120
#define GP_CROP_PAR_STATUS_SHIFT         0UL
#define GP_CROP_PAR_STATUS_MASK          0x1 << GP_CROP_PAR_STATUS_SHIFT

#define GP_HS_CTRL                       0x0300
#define GP_HS_CTRL_NOR_PARA_SHIFT        8UL
#define GP_HS_CTRL_NOR_PARA_MASK         0xF << GP_HS_CTRL_NOR_PARA_SHIFT
#define GP_HS_CTRL_APB_RD_SHIFT          4UL
#define GP_HS_CTRL_APB_RD_MASK           1 << GP_HS_CTRL_APB_RD_SHIFT
#define GP_HS_CTRL_FILTER_EN_V_SHIFT     3UL
#define GP_HS_CTRL_FILTER_EN_V_MASK      1 << GP_HS_CTRL_FILTER_EN_V_SHIFT
#define GP_HS_CTRL_FILTER_EN_U_SHIFT     2UL
#define GP_HS_CTRL_FILTER_EN_U_MASK      1 << GP_HS_CTRL_FILTER_EN_U_SHIFT
#define GP_HS_CTRL_FILTER_EN_Y_SHIFT     1UL
#define GP_HS_CTRL_FILTER_EN_Y_MASK      1 << GP_HS_CTRL_FILTER_EN_Y_SHIFT
#define GP_HS_CTRL_FILTER_EN_A_SHIFT     0UL
#define GP_HS_CTRL_FILTER_EN_A_MASK      1 << GP_HS_CTRL_FILTER_EN_A_SHIFT

#define GP_HS_INI                        0x0304
#define GP_HS_INI_POLA_SHIFT             19UL
#define GP_HS_INI_POLA_MASK              1 << GP_HS_INI_POLA_SHIFT
#define GP_HS_INI_FRA_SHIFT              0
#define GP_HS_INI_FRA_MASK               0x7FFFF << GP_HS_INI_FRA_SHIFT

#define GP_HS_RATIO                      0x0308
#define GP_HS_RATIO_INT_SHIFT            19UL
#define GP_HS_RATIO_INT_MASK             0x7 << GP_HS_RATIO_INT_SHIFT
#define GP_HS_RATIO_FRA_SHIFT            0UL
#define GP_HS_RATIO_FRA_MASK             0x7FFFF << GP_HS_RATIO_FRA_SHIFT

#define GP_HS_WIDTH                      0x030C
#define GP_HS_WIDTH_OUT_SHIFT            0UL
#define GP_HS_WIDTH_OUT_MASK             0xFFFF << GP_HS_WIDTH_OUT_SHIFT

#define GP_VS_CTRL                       0x0400
#define GP_VS_CTRL_INIT_PHASE_O_SHIFT    18UL
#define GP_VS_CTRL_INIT_PHASE_O_MASK     0x3F << GP_VS_CTRL_INIT_PHASE_O_SHIFT
#define GP_VS_CTRL_INIT_POS_O_SHIFT      16UL
#define GP_VS_CTRL_INIT_POS_O_MASK       0x3 << GP_VS_CTRL_INIT_POS_O_SHIFT
#define GP_VS_CTRL_INIT_PHASE_E_SHIFT    10UL
#define GP_VS_CTRL_INIT_PHASE_E_MASK     0x3F << GP_VS_CTRL_INIT_PHASE_E_SHIFT
#define GP_VS_CTRL_INIT_POS_E_SHIFT      8UL
#define GP_VS_CTRL_INIT_POS_E_MASK       0x3 << GP_VS_CTRL_INIT_POS_E_SHIFT
#define GP_VS_CTRL_NORM_SHIFT            4UL
#define GP_VS_CTRL_NORM_MASK             0xF << GP_VS_CTRL_NORM_SHIFT
#define GP_VS_CTRL_PARITY_SHIFT          3UL
#define GP_VS_CTRL_PARITY_MASK           1 << GP_VS_CTRL_PARITY_SHIFT
#define GP_VS_CTRL_PXL_MODE_SHIFT        2UL
#define GP_VS_CTRL_PXL_MODE_MASK         1 << GP_VS_CTRL_PXL_MODE_SHIFT
#define GP_VS_CTRL_VS_MODE_SHIFT         0UL
#define GP_VS_CTRL_VS_MODE_MASK          0x3 << GP_VS_CTRL_VS_MODE_SHIFT

#define GP_VS_RESV                       0x0404
#define GP_VS_RESV_VSIZE_SHIFT           0UL
#define GP_VS_RESV_VSIZE_MASK            0xFFFF << GP_VS_RESV_VSIZE_SHIFT

#define GP_VS_INC                        0x0408
#define GP_VS_INC_INC_SHIFT              0UL
#define GP_VS_INC_INC_MASK               0x1FFFFF << GP_VS_INC_INC_SHIFT

#define GP_RE_STATUS                     0x0410
#define GP_RE_STATUS_V_FRAME_END_SHIFT   2UL
#define GP_RE_STATUS_V_FRAME_END_MASK    1 << GP_RE_STATUS_V_FRAME_END_SHIFT
#define GP_RE_STATUS_U_FRAME_END_SHIFT   1UL
#define GP_RE_STATUS_U_FRAME_END_MASK    1 << GP_RE_STATUS_U_FRAME_END_SHIFT
#define GP_RE_STATUS_Y_FRAME_END_SHIFT   0UL
#define GP_RE_STATUS_Y_FRAME_END_MASK    1 << GP_RE_STATUS_Y_FRAME_END_SHIFT

#define GP_FBDC_CTRL                     0x0500
#define GP_FBDC_CTRL_M_ID_SHIFT          14UL
#define GP_FBDC_CTRL_M_ID_MASK           0x3 << GP_FBDC_CTRL_M_ID_SHIFT
#define GP_FBDC_CTRL_CONTEXT_SHIFT       11UL
#define GP_FBDC_CTRL_CONTEXT_MASK        0x7 << GP_FBDC_CTRL_CONTEXT_SHIFT
#define GP_FBDC_CTRL_HDR_ENCODING_SHIFT  10UL
#define GP_FBDC_CTRL_HDR_ENCODING_MASK   1 << GP_FBDC_CTRL_HDR_ENCODING_SHIFT
#define GP_FBDC_CTRL_TWIDDLE_MODE_SHIFT  9UL
#define GP_FBDC_CTRL_TWIDDLE_MODE_MASK   1 << GP_FBDC_CTRL_TWIDDLE_MODE_SHIFT
#define GP_FBDC_CTRL_MEML_SHIFT          8UL
#define GP_FBDC_CTRL_MEML_MASK           1 << GP_FBDC_CTRL_MEML_SHIFT
#define GP_FBDC_CTRL_FMT_SHIFT           1UL
#define GP_FBDC_CTRL_FMT_MASK            0x7F << GP_FBDC_CTRL_FMT_SHIFT
#define GP_FBDC_CTRL_EN_SHIFT            0UL
#define GP_FBDC_CTRL_EN_MASK             1 << GP_FBDC_CTRL_EN_SHIFT

#define GP_FBDC_FBDC_CLEAR_0             0x0504
#define GP_FBDC_FBDC_CLEAR_0_COLOR_SHIFT 0UL
#define GP_FBDC_FBDC_CLEAR_0_COLOR_MASK  0xFFFFFFFF << GP_FBDC_FBDC_CLEAR_0_COLOR_SHIFT

#define GP_FBDC_FBDC_CLEAR_1             0x0508
#define GP_FBDC_FBDC_CLEAR_1_COLOR_SHIFT 0UL
#define GP_FBDC_FBDC_CLEAR_1_COLOR_MASK  0xFFFFFFFF << GP_FBDC_FBDC_CLEAR_1_COLOR_SHIFT


/*DP A-pipe*/
#define DP_AP_CHN_BASE 0x9000

#define AP_PIX_COMP                      0x0000
#define AP_PIX_COMP_BPA_SHIFT            0UL
#define AP_PIX_COMP_BPA_MASK             0xF << AP_PIX_COMP_BPA_SHIFT

#define AP_FRM_CTRL                      0x0004
#define AP_FRM_CTRL_ENDIAN_CTRL_SHIFT    16UL
#define AP_FRM_CTRL_ENDIAN_CTRL_MASK     0x7 << AP_FRM_CTRL_ENDIAN_CTRL_SHIFT
#define AP_FRM_CTRL_ROT_SHIFT            8UL
#define AP_FRM_CTRL_ROT_MASK             0x7 << AP_FRM_CTRL_ROT_SHIFT
#define AP_FRM_CTRL_FAST_CP_MODE_SHIFT   0UL
#define AP_FRM_CTRL_FAST_CP_MODE_MASK    0x1 << AP_FRM_CTRL_FAST_CP_MODE_SHIFT

#define AP_FRM_SIZE                      0x0008
#define AP_FRM_SIZE_HEIGHT_SHIFT         16UL
#define AP_FRM_SIZE_HEIGHT_MASK          0xFFFF << AP_FRM_SIZE_HEIGHT_SHIFT
#define AP_FRM_SIZE_WIDTH_SHIFT          0UL
#define AP_FRM_SIZE_WIDTH_MASK           0xFFFF << AP_FRM_SIZE_WIDTH_SHIFT

#define AP_BADDR_L                       0x000C
#define AP_BADDR_L_SHIFT                 0UL
#define AP_BADDR_L_MASK                  0xFFFFFFFF << AP_BADDR_L_SHIFT

#define AP_BADDR_H                       0x0010
#define AP_BADDR_H_SHIFT                 0UL
#define AP_BADDR_H_MASK                  0xFFFFFFFF << AP_BADDR_H_SHIFT

#define AP_STRIDE                        0x002C
#define AP_STRIDE_SHIFT                  0UL
#define AP_STRIDE_MASK                   0x3FFFF << AP_STRIDE_SHIFT

#define AP_FRM_OFFSET                    0x0040
#define AP_FRM_OFFSET_Y_SHIFT            16UL
#define AP_FRM_OFFSET_Y_MASK             0xFFFF << AP_FRM_OFFSET_Y_SHIFT
#define AP_FRM_OFFSET_X_SHIFT            0UL
#define AP_FRM_OFFSET_X_MASK             0xFFFF << AP_FRM_OFFSET_X_SHIFT

#define AP_SW_RST                        0x0E00
#define AP_SW_RST_SHIFT                  0UL
#define AP_SW_RST_MASK                   0x1 << AP_SW_RST_SHIFT

#define AP_SDW_CTRL                      0x0F00
#define AP_SDW_CTRL_TRIG_SHIFT           0UL
#define AP_SDW_CTRL_TRIG_MASK            0x1 << AP_SDW_CTRL_TRIG_SHIFT

/*DP FBDC*/
#define FBDC_CTRL                        0xC000
#define FBDC_CTRL_SW_RST_SHIFT           31UL
#define FBDC_CTRL_SW_RST_MASK            0x1 << FBDC_CTRL_SW_RST_SHIFT
#define FBDC_CTRL_INVA_SW_EN_SHIFT       3UL
#define FBDC_CTRL_INVA_SW_EN_MASK        0x1 << FBDC_CTRL_INVA_SW_EN_SHIFT
#define FBDC_CTRL_HDR_BYPASS_SHIFT       2UL
#define FBDC_CTRL_HDR_BYPASS_MASK        0x1 << FBDC_CTRL_HDR_BYPASS_SHIFT
#define FBDC_CTRL_MODE_V3_1_EN_SHIFT     1UL
#define FBDC_CTRL_MODE_V3_1_EN_MASK      0x1 << FBDC_CTRL_MODE_V3_1_EN_SHIFT
#define FBDC_CTRL_EN_SHIFT               0UL
#define FBDC_CTRL_EN_MASK                0x1 << FBDC_CTRL_EN_SHIFT

#define FBDC_CR_INVAL                    0xC004
#define FBDC_CR_INVAL_REQUESTER_I_SHIFT  20UL
#define FBDC_CR_INVAL_REQUESTER_I_MASK   0xF << FBDC_CR_INVAL_REQUESTER_I_SHIFT
#define FBDC_CR_INVAL_CONTEXT_I_SHIFT    17UL
#define FBDC_CR_INVAL_CONTEXT_I_MASK     0x7 << FBDC_CR_INVAL_CONTEXT_I_SHIFT
#define FBDC_CR_INVAL_PENDING_I_SHIFT    16UL
#define FBDC_CR_INVAL_PENDING_I_MASK     0x1 << FBDC_CR_INVAL_PENDING_I_SHIFT
#define FBDC_CR_INVAL_NOTIFY_SHIFT       9UL
#define FBDC_CR_INVAL_NOTIFY_MASK        0x1 << FBDC_CR_INVAL_NOTIFY_SHIFT
#define FBDC_CR_INVAL_OVERRIDE_SHIFT     8UL
#define FBDC_CR_INVAL_OVERRIDE_MASK      0x1 << FBDC_CR_INVAL_OVERRIDE_SHIFT
#define FBDC_CR_INVAL_REQUESTER_O_SHIFT  4UL
#define FBDC_CR_INVAL_REQUESTER_O_MASK   0xF << FBDC_CR_INVAL_REQUESTER_O_SHIFT
#define FBDC_CR_INVAL_CONTEXT_O_SHIFT    3UL
#define FBDC_CR_INVAL_CONTEXT_O_MASK     0x7 << FBDC_CR_INVAL_CONTEXT_O_SHIFT
#define FBDC_CR_INVAL_PENDING_O_SHIFT    0
#define FBDC_CR_INVAL_PENDING_O_MASK     0x1 << FBDC_CR_INVAL_PENDING_O_SHIFT

#define FBDC_CR_VAL_0                    0xC008
#define FBDC_CR_VAL_0_UV0_SHIFT          16UL
#define FBDC_CR_VAL_0_UV0_MASK           0x3FF << FBDC_CR_VAL_0_UV0_SHIFT
#define FBDC_CR_VAL_0_Y0_SHIFT           0UL
#define FBDC_CR_VAL_0_Y0_MASK            0x3FF << FBDC_CR_VAL_0_Y0_SHIFT

#define FBDC_CR_VAL_1                    0xC00C
#define FBDC_CR_VAL_1_UV1_SHIFT          16UL
#define FBDC_CR_VAL_1_UV1_MASK           0x3FF << FBDC_CR_VAL_1_UV1_SHIFT
#define FBDC_CR_VAL_1_Y1_SHIFT           0UL
#define FBDC_CR_VAL_1_Y1_MASK            0x3FF << FBDC_CR_VAL_1_Y1_SHIFT

#define FBDC_CR_CH0123_0                 0xC010
#define FBDC_CR_CH0123_0_VAL0_SHIFT      0UL
#define FBDC_CR_CH0123_0_VAL0_MASK       0xFFFFFFFF << FBDC_CR_CH0123_0_VAL0_SHIFT

#define FBDC_CR_CH0123_1                 0xC014
#define FBDC_CR_CH0123_1_VAL1_SHIFT      0UL
#define FBDC_CR_CH0123_1_VAL1_MASK       0xFFFFFFFF << FBDC_CR_CH0123_1_VAL1_SHIFT

#define FBDC_CR_FILTER_CTRL                 0xC018
#define FBDC_CR_FILTER_CTRL_CLEAR_3_SHIFT   7UL
#define FBDC_CR_FILTER_CTRL_CLEAR_3_MASK    0x1 << FBDC_CR_FILTER_CTRL_CLEAR_3_SHIFT
#define FBDC_CR_FILTER_CTRL_CLEAR_2_SHIFT   6UL
#define FBDC_CR_FILTER_CTRL_CLEAR_2_MASK    0x1 << FBDC_CR_FILTER_CTRL_CLEAR_2_SHIFT
#define FBDC_CR_FILTER_CTRL_CLEAR_1_SHIFT   5UL
#define FBDC_CR_FILTER_CTRL_CLEAR_1_MASK    0x1 << FBDC_CR_FILTER_CTRL_CLEAR_1_SHIFT
#define FBDC_CR_FILTER_CTRL_CLEAR_0_SHIFT   4UL
#define FBDC_CR_FILTER_CTRL_CLEAR_0_MASK    0x1 << FBDC_CR_FILTER_CTRL_CLEAR_0_SHIFT
#define FBDC_CR_FILTER_CTRL_EN_SHIFT        0UL
#define FBDC_CR_FILTER_CTRL_EN_MASK         0x1 << FBDC_CR_FILTER_CTRL_EN_SHIFT

#define FBDC_CR_FILTER_STATUS             0xC01C
#define FBDC_CR_FILTER_STATUS_SHIFT       0UL
#define FBDC_CR_FILTER_STATUS_MASK        0xF << FBDC_CR_FILTER_STATUS_SHIFT

#define FBDC_CR_CORE_ID_0                 0xC020
#define FBDC_CR_CORE_ID_0_B_SHIFT         16UL
#define FBDC_CR_CORE_ID_0_B_MASK          0xFFFF << FBDC_CR_CORE_ID_0_B_SHIFT
#define FBDC_CR_CORE_ID_0_P_SHIFT         0UL
#define FBDC_CR_CORE_ID_0_P_MASK          0xFFFF << FBDC_CR_CORE_ID_0_P_SHIFT

#define FBDC_CR_CORE_ID_1                 0xC024
#define FBDC_CR_CORE_ID_1_N_SHIFT         16UL
#define FBDC_CR_CORE_ID_1_N_MASK          0xFFFF << FBDC_CR_CORE_ID_1_N_SHIFT
#define FBDC_CR_CORE_ID_1_V_SHIFT         0UL
#define FBDC_CR_CORE_ID_1_V_MASK          0xFFFF << FBDC_CR_CORE_ID_1_V_SHIFT

#define FBDC_CR_CORE_ID_2                 0xC028
#define FBDC_CR_CORE_ID_2_C_SHIFT         0UL
#define FBDC_CR_CORE_ID_2_C_MASK          0xFFFF << FBDC_CR_CORE_ID_2_C_SHIFT

#define FBDC_CR_CORE_IP                   0xC02C
#define FBDC_CR_CORE_IP_CHANGELIST_SHIFT  0UL
#define FBDC_CR_CORE_IP_CHANGELIST_MASK   0xFFFFFFFF << FBDC_CR_CORE_IP_CHANGELIST_SHIFT

#endif
