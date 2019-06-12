/*
 * kunlun_drm_dc_reg.h
 *
 * Semidrive kunlun platform drm driver
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef _KUNLUN_DRM_DC_REG_H_
#define _KUNLUN_DRM_DC_REG_H_
#include "kunlun_drm_reg.h"

/* DC CTRL */
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

/* DC FLC CTRL*/
#define DC_FLC_CTRL                      0x04
#define DC_SAFE_FLC_CTRL                 0x100

#define CRC32_TRIG_SHIFT                 3UL
#define CRC32_TRIG_MASK                  1UL << CRC32_TRIG_SHIFT
#define TCON_TRIG_SHIFT                  2UL
#define TCON_TRIG_MASK                   1UL << TCON_TRIG_SHIFT

/*DC FLC UPDATE*/
#define DC_FLC_UPDATE                    0x08

/*DC INIT MASK&&STATUS*/
#define DC_INT_MSK                       0x20
#define DC_INT_ST                        0x24
#define DC_SAFE_INT_MSK                  0x120
#define DC_SAFE_INT_ST                   0x124

#define DC_INT_MASK                      0xFFFFFFF

#define DC_RDMA_SHIFT                    0UL
#define DC_RDMA_MASK                     1UL << DC_RDMA_SHIFT

#define DC_RLE_SHIFT                     1UL
#define DC_RLE_MASK                      1UL << DC_RLE_SHIFT

#define DC_MLC_SHIFT                     2UL
#define DC_MLC_MASK                      1UL << DC_MLC_SHIFT

#define TCON_SOF_SHIFT                   3UL
#define TCON_SOF_MASK                    1UL << TCON_SOF_SHIFT

#define TCON_EOF_SHIFT                   4UL
#define TCON_EOF_MASK                    1UL << TCON_EOF_SHIFT

#define TCON_UNDERRUN_SHIFT              5UL
#define TCON_UNDERRUN_MASK               1UL << TCON_UNDERRUN_SHIFT

#define DC_UNDERRUN_SHIFT                6UL
#define DC_UNDERRUN_MASK                 1UL << DC_UNDERRUN_SHIFT

#define DC_SDMA_DONE_SHIFT               7UL
#define DC_SDMA_DONE_MASK                1UL << DC_SDMA_DONE_SHIFT

#define TCON_LAYER_KICK_SHIFT            8UL
#define TCON_LAYER_KICK_MASK             0xFFFFF << TCON_LAYER_KICK_SHIFT

/*DC RDMA CHN*/
#define RDMA_DC_CHN_COUNT                4
#define SAFE_RDMA_DC_CHN_COUNT           1
/*DC RDMA CTRL*/
#define DC_RDMA_CTRL                     0x1100

#define DC_RDMA_DFIFO_FULL               0x1200
#define DC_RDMA_DFIFO_EMPTY              0x1204
#define DC_RDMA_CFIFO_FULL               0x1208
#define DC_RDMA_CFIFO_EMPTY              0x120C
#define DC_RDMA_CH_IDLE                  0x1210

#define RDMA_DC_CH_MASK                  0x7F

#define DC_RDMA_INT_MSK                  0x1220
#define DC_RDMA_INT_ST                   0x1224

#define DC_RDMA_INT_MASK                 0x7F
#define DC_RDMA_CH_6_SHIFT               6UL
#define DC_RDMA_CH_6_MASK                1UL << DC_RDMA_CH_6_SHIFT
#define DC_RDMA_CH_5_SHIFT               5UL
#define DC_RDMA_CH_5_MASK                1UL << DC_RDMA_CH_5_SHIFT
#define DC_RDMA_CH_4_SHIFT               4UL
#define DC_RDMA_CH_4_MASK                1UL << DC_RDMA_CH_4_SHIFT
#define DC_RDMA_CH_3_SHIFT               3UL
#define DC_RDMA_CH_3_MASK                1UL << DC_RDMA_CH_3_SHIFT
#define DC_RDMA_CH_2_SHIFT               2UL
#define DC_RDMA_CH_2_MASK                1UL << DC_RDMA_CH_2_SHIFT
#define DC_RDMA_CH_1_SHIFT               1UL
#define DC_RDMA_CH_1_MASK                1UL << DC_RDMA_CH_1_SHIFT
#define DC_RDMA_CH_0_SHIFT               0UL
#define DC_RDMA_CH_0_MASK                1UL << DC_RDMA_CH_0_SHIFT



#define SAFE_RDMA_CTRL                   0x1400
#define SAFE_RDMA_DFIFO_FULL             0x1500
#define SAFE_RDMA_DFIFO_EMPTY            0x1504
#define SAFE_RDMA_CFIFO_FULL             0x1508
#define SAFE_RDMA_CFIFO_EMPTY            0x150C
#define SAFE_RDMA_CH_IDLE                0x1510

#define SAFE_RDMA_DC_CH_MASK             0x7F

#define SAFE_RDMA_INT_MSK                0x1220
#define SAFE_RDMA_INT_ST                 0x1224

/*DC PIPES*/
#define DC_GP_CHN_BASE 0x2000
#define DC_SP_CHN_BASE 0x5000

/* TCON */
#define KICK_LAYER_JMP              0x8
#define KICK_LAYER_COUNT            19UL

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

/*TCON POST CTRL*/
#define POST_KICK_LAYER_JMP         0x4
#define POST_KICK_LAYER_COUNT       4UL

#define TCON_POST_CTRL              0x9400
#define POST_CTRL_EN_SHIFT          1UL
#define POST_CTRL_EN_MASK           1UL << POST_CTRL_EN_SHIFT
#define POST_CTRL_SYNC_SCR_SHIFT    0UL
#define POST_CTRL_SYNC_SCR_MASK     1UL << POST_CTRL_SYNC_SCR_SHIFT

#define TCON_POST_CH_CTRL           0x9410
#define POST_CH_OUT_MODE_SHIFT      3UL
#define POST_CH_OUT_MODE_MASK       1UL << POST_CH_OUT_MODE_SHIFT
#define POST_CH_OUT_SCR_SHIFT       2UL
#define POST_CH_OUT_SCR_MASK        1UL << POST_CH_OUT_SCR_SHIFT
#define POST_CH_OUT_POL_SHIFT       1UL
#define POST_CH_OUT_POL_MASK        1UL << POST_CH_OUT_POL_SHIFT
#define POST_CH_OUT_EN_SHIFT        0UL
#define POST_CH_OUT_EN_MASK         1UL << POST_CH_OUT_EN_SHIFT

#define TCON_POST_POS               0x9430
#define POST_POS_OFF_SHIFT          16UL
#define POST_POS_OFF_MASK           0xFFFF << POST_POS_OFF_SHIFT
#define POST_POS_ON_SHIFT           0UL
#define POST_POS_ON_MASK            0xFFFF << POST_POS_ON_SHIFT

/* DC_CSC */
#define DC_CSC_JMP                  0x1000
#define DC_CSC_COUNT                2UL

#define DC_CSC_CTRL                 0xA000
#define DC_CSC_COEF1                0xA004
#define DC_CSC_COEF2                0xA008
#define DC_CSC_COEF3                0xA00C
#define DC_CSC_COEF4                0xA010
#define DC_CSC_COEF5                0xA014
#define DC_CSC_COEF6                0xA018
#define DC_CSC_COEF7                0xA01C
#define DC_CSC_COEF8                0xA020

/*GAMMA*/
#define GAMMA_CTRL                  0xC000
#define APB_RD_TO_SHIFT             8UL
#define APB_RD_TO_MASK              0xFF << APB_RD_TO_SHIFT
#define GMMA_BYPASS_SHIFT           0UL
#define GMMA_BYPASS_MASK            0x1 << GMMA_BYPASS_SHIFT

/*DITHER*/
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



/*DC CRC32*/
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
