/*
 *  Copyright (c) Semidrive
 */
/* 1366 x 768 */
#pragma once
#include <linux/types.h>
#include "dc.h"


#define DC_NUM 2

/* dc profile */
dc_profile_t dc_prop[DC_NUM] = {
    /* dc[0] */
    {
        .id       =  0,
        .irq      =  64,
        .tcon_irq =  65,
        .frame_w  =  800,
        .frame_h  =  600,
        .layer_n  =  2,
        .base_addr = 0x30440000,
    },
    /* dc[1] */
    {
        .id       =  1,
        .irq      =  66,
        .tcon_irq =  67,
        .frame_w  =  800,
        .frame_h  =  600,
        .layer_n  =  2,
        .base_addr = 0x30460000,
    },
};

/* dc registers initial values */
const uint32_t DC_CTRL_SW_RST                     =      0x0;
const uint32_t DC_CTRL_MS_MODE                    =      0x0;
const uint32_t DC_CTRL_SF_MODE                    =      0x0;
const uint32_t TCON_TRIG                          =      0x1;
const uint32_t DI_TRIG                            =      0x1;
const uint32_t FLC_TRIG                           =      0x1;
const uint32_t DC_FLC_UPDATE_FORCE                =      0x1;
const uint32_t TCON_UNDERRUN                      =      0x0;
const uint32_t DC_UNDERRUN                        =      0x0;
const uint32_t TCON_SOF                           =      0x0;
const uint32_t TCON_EOF                           =      0x0;
const uint32_t MLC                                =      0x1;
const uint32_t RLE                                =      0x0;
const uint32_t RDMA                               =      0x0;
const uint32_t SF_TCON_TRIG                          =      0x0;
const uint32_t SF_DI_TRIG                            =      0x1;
const uint32_t SF_FLC_TRIG                           =      0x1;
const uint32_t SF_TCON_UNDERRUN                      =      0x0;
const uint32_t SF_DC_UNDERRUN                        =      0x0;
const uint32_t SF_TCON_SOF                           =      0x0;
const uint32_t SF_TCON_EOF                           =      0x1;
const uint32_t SF_MLC                                =      0x1;
const uint32_t SF_RLE                                =      0x0;
const uint32_t SF_RDMA                               =      0x0;

const uint32_t RDMA_DFIFO_WML[4]                  =      { 0x50, 0x18, 0x18, 0x50};//{ 0xB0, 0x20, 0x20, 0xB0, 0x20, 0x20, 0x20 };
const uint32_t RDMA_DFIFO_DEPTH[4]                =      { 0xd, 0x3, 0x3, 0xd}; //{0x16, 0x4, 0x4, 0x16, 0x4,,0x4, 0x4}
const uint32_t RDMA_CFIFO_DEPTH[4]                =      { 0x6, 0x2, 0x2, 0x6};//{ 0xb, 0x2, 0x2, 0xb, 0x2, 0x2, 0x2 };
const uint32_t RDMA_CH_PRIO_SCHE[4]               =      { 0x4, 0x1, 0x1, 0x4};//{ 0x4, 0x1, 0x1, 0x4, 0x1, 0x1, 0x1 };
const uint32_t RDMA_CH_PRIO_P1[4]                 =      { 0x100, 0x80, 0x80, 0x100};//{ 0x100, 0x80, 0x80, 0x100, 0x80, 0x80, 0x80 };
const uint32_t RDMA_CH_PRIO_P0[4]                 =      { 0x40, 0x20, 0x20, 0x40};//{ 0x40, 0x20, 0x20, 0x40, 0x20, 0x20, 0x20 };
const uint32_t RDMA_BURST_MODE[4]                 =      { 0x0, 0x0, 0x0, 0x0};//, 0x0, 0x0, 0x0 };
const uint32_t RDMA_BURST_LEN[4]                  =      { 0x4, 0x4, 0x4, 0x4};//, 0x4, 0x4, 0x4};
const uint32_t RDMA_AXI_USER[4]                   =      { 0x0, 0x0, 0x0, 0x0};//, 0x0, 0x0, 0x0 };
const uint32_t RDMA_AXI_CTRL_PORT[4]              =      { 0x0, 0x0, 0x0, 0x0};//, 0x0, 0x0, 0x0 };
const uint32_t RDMA_AXI_CTRL_CACHE[4]             =      { 0x0, 0x0, 0x0, 0x0};//, 0x0, 0x0, 0x0 };
const uint32_t RDMA_CTRL_CFG_LOAD                 =      0x1;
const uint32_t RDMA_CTRL_ARB_SEL                  =      0x1;
//const uint32_t RDMA_CH_6                          =      0x1;
//const uint32_t RDMA_CH_5                          =      0x1;
//const uint32_t RDMA_CH_4                          =      0x1;
const uint32_t RDMA_CH_3                          =      0x1;
const uint32_t RDMA_CH_2                          =      0x1;
const uint32_t RDMA_CH_1                          =      0x1;
const uint32_t RDMA_CH_0                          =      0x1;

const uint32_t RDMA_CFIFO_DEP                     =      0x0;
const uint32_t RDMA_DFIFO_DEP                     =      0x0;

const uint32_t GP_BPV[2]                          =      { 0x8, 0x8 };
const uint32_t GP_BPU[2]                          =      { 0x8, 0x8 };
const uint32_t GP_BPY[2]                          =      { 0x8, 0x8 };
const uint32_t GP_BPA[2]                          =      { 0x0, 0x0 };
const uint32_t GP_ENDIAN_CTRL[2]                  =      { 0x0, 0x0 };
const uint32_t GP_COMP_SWAP[2]                    =      { 0x0, 0x0 };
const uint32_t GP_ROT[2]                          =      { 0x0, 0x0 };
const uint32_t GP_RGB_YUV[2]                      =      { 0x0, 0x0 };
const uint32_t GP_UV_SWAP[2]                      =      { 0x0, 0x0 };
const uint32_t GP_UV_MODE[2]                      =      { 0x0, 0x0 };
const uint32_t GP_MODE[2]                         =      { 0x0, 0x0 };
const uint32_t GP_FMT[2]                          =      { 0x0, 0x0 };
#if 0
const uint32_t GP_FRM_HEIGHT[2]                   =      { 719, 719 };
const uint32_t GP_FRM_WIDTH[2]                    =      { 1279, 1279 };
#endif
const uint32_t GP_FRM_HEIGHT[2]                   =      { 599, 599 };
const uint32_t GP_FRM_WIDTH[2]                    =      { 799, 799 };
const uint32_t GP_BADDR_L_Y[2]                    =      { 0x1000000, 0x1000000 };
const uint32_t GP_BADDR_H_Y[2]                    =      { 0x0, 0x0 };
const uint32_t GP_BADDR_L_U[2]                    =      { 0x0, 0x0 };
const uint32_t GP_BADDR_H_U[2]                    =      { 0x0, 0x0 };
const uint32_t GP_BADDR_L_V[2]                    =      { 0x0, 0x0 };
const uint32_t GP_BADDR_H_V[2]                    =      { 0x0, 0x0 };
#if 1
const uint32_t GP_STRIDE_Y[2]                     =      { 0xeff, 0xeff };
#endif
//const uint32_t GP_STRIDE_Y[2]                     =      { 0x95f, 0x95f };
const uint32_t GP_STRIDE_U[2]                     =      { 0x0, 0x0 };
const uint32_t GP_STRIDE_V[2]                     =      { 0x0, 0x0 };
const uint32_t GP_FRM_Y[2]                        =      { 0x0, 0x0 };
const uint32_t GP_FRM_X[2]                        =      { 0x0, 0x0 };
const uint32_t DC_GP_YUVUP_EN[2]                  =      { 0x0, 0x0 };
const uint32_t DC_GP_YUVUP_VOFSET[2]              =      { 0x0, 0x0 };
const uint32_t DC_GP_YUVUP_HOFSET[2]              =      { 0x0, 0x0 };
const uint32_t DC_GP_YUVUP_FILTER_MODE[2]         =      { 0x0, 0x0 };
const uint32_t DC_GP_YUVUP_UPV_BYPASS[2]          =      { 0x0, 0x0 };
const uint32_t DC_GP_YUVUP_UPH_BYPASS[2]          =      { 0x0, 0x0 };
const uint32_t DC_GP_YUVUP_BYPASS[2]              =      { 0x1, 0x1 };
const uint32_t GP_CSC_ALPHA[2]                    =      { 0x0, 0x0 };
const uint32_t GP_CSC_SBUP_CONV[2]                =      { 0x0, 0x0 };
const uint32_t GP_CSC_BYPASS[2]                   =      { 0x1, 0x1 };
const uint32_t GP_CSC_COEF1_A01[2]                =      { 0x0, 0x0 };
const uint32_t GP_CSC_COEF1_A00[2]                =      { 0x0, 0x0 };
const uint32_t GP_CSC_COEF2_A10[2]                =      { 0x0, 0x0 };
const uint32_t GP_CSC_COEF2_A02[2]                =      { 0x0, 0x0 };
const uint32_t GP_CSC_COEF3_A12[2]                =      { 0x0, 0x0 };
const uint32_t GP_CSC_COEF3_A11[2]                =      { 0x0, 0x0 };
const uint32_t GP_CSC_COEF4_A21[2]                =      { 0x0, 0x0 };
const uint32_t GP_CSC_COEF4_A20[2]                =      { 0x0, 0x0 };
const uint32_t GP_CSC_COEF5_B0[2]                 =      { 0x0, 0x0 };
const uint32_t GP_CSC_COEF5_A22[2]                =      { 0x0, 0x0 };
const uint32_t GP_CSC_COEF6_B2[2]                 =      { 0x0, 0x0 };
const uint32_t GP_CSC_COEF6_B1[2]                 =      { 0x0, 0x0 };
const uint32_t GP_CSC_COEF7_C1[2]                 =      { 0x0, 0x0 };
const uint32_t GP_CSC_COEF7_C0[2]                 =      { 0x0, 0x0 };
const uint32_t GP_CSC_COEF8_C2[2]                 =      { 0x0, 0x0 };
const uint32_t GP_SDW_CTRL_TRIG[2]                =      { 0x1, 0x1 };

const uint32_t SP_BPV[2]                          =      { 0x8, 0x8 };
const uint32_t SP_BPU[2]                          =      { 0x8, 0x8 };
const uint32_t SP_BPY[2]                          =      { 0x8, 0x8 };
const uint32_t SP_BPA[2]                          =      { 0x8, 0x8 };
const uint32_t SP_ENDIAN_CTRL[2]                  =      { 0x0, 0x0 };
const uint32_t SP_COMP_SWAP[2]                    =      { 0x0, 0x0 };
const uint32_t SP_ROT[2]                          =      { 0x0, 0x0 };
const uint32_t SP_RGB_YUV[2]                      =      { 0x0, 0x0 };
const uint32_t SP_UV_SWAP[2]                      =      { 0x0, 0x0 };
const uint32_t SP_UV_MODE[2]                      =      { 0x0, 0x0 };
const uint32_t SP_MODE[2]                         =      { 0x0, 0x0 };
const uint32_t SP_FMT[2]                          =      { 0x0, 0x0 };
const uint32_t SP_FRM_HEIGHT[2]                   =      { 719, 719 };
const uint32_t SP_FRM_WIDTH[2]                    =      { 1279, 1279 };
const uint32_t SP_BADDR_L_Y[2]                    =      { 0x2000000, 0x2000000 };
const uint32_t SP_BADDR_H_Y[2]                    =      { 0x0, 0x0 };
const uint32_t SP_STRIDE_Y[2]                     =      { 0x13FF, 0x13FF };
const uint32_t SP_STRIDE_U[2]                     =      { 0x0, 0x0 };
const uint32_t SP_STRIDE_V[2]                     =      { 0x0, 0x0 };
const uint32_t SP_FRM_Y[2]                        =      { 0x0, 0x0 };
const uint32_t SP_FRM_X[2]                        =      { 0x0, 0x0 };

const uint32_t RLE_Y_LEN_Y[2]                     =      { 0x0, 0x0 };
const uint32_t RLE_U_LEN_U[2]                     =      { 0x0, 0x0 };
const uint32_t RLE_V_LEN_V[2]                     =      { 0x0, 0x0 };
const uint32_t RLE_A_LEN_A[2]                     =      { 0x0, 0x0 };
const uint32_t RLE_Y_CHECK_SUM_Y[2]               =      { 0x0, 0x0 };
const uint32_t RLE_U_CHECK_SUM_U[2]               =      { 0x0, 0x0 };
const uint32_t RLE_V_CHECK_SUM_V[2]               =      { 0x0, 0x0 };
const uint32_t RLE_A_CHECK_SUM_A[2]               =      { 0x0, 0x0 };
const uint32_t RLE_DATA_SIZE[2]                   =      { 0x0, 0x0 };
const uint32_t RLE_EN[2]                          =      { 0x0, 0x0 };
const uint32_t RLE_Y_CHECK_SUM_ST_Y[2]               =      { 0x0, 0x0 };
const uint32_t RLE_U_CHECK_SUM_ST_U[2]               =      { 0x0, 0x0 };
const uint32_t RLE_V_CHECK_SUM_ST_V[2]               =      { 0x0, 0x0 };
const uint32_t RLE_A_CHECK_SUM_ST_A[2]               =      { 0x0, 0x0 };
const uint32_t RLE_INT_V_ERR[2]                   =      { 0x0, 0x0 };
const uint32_t RLE_INT_U_ERR[2]                   =      { 0x0, 0x0 };
const uint32_t RLE_INT_Y_ERR[2]                   =      { 0x0, 0x0 };
const uint32_t RLE_INT_A_ERR[2]                   =      { 0x0, 0x0 };
const uint32_t CLUT_A_Y_SEL[2]                    =      { 0x0, 0x0 };
const uint32_t CLUT_A_BYPASS[2]                   =      { 0x1, 0x1 };
const uint32_t CLUT_A_OFFSET[2]                   =      { 0x0, 0x0 };
const uint32_t CLUT_A_DEPTH[2]                    =      { 0x0, 0x0 };
const uint32_t CLUT_Y_BYPASS[2]                   =      { 0x1, 0x1 };
const uint32_t CLUT_Y_OFFSET[2]                   =      { 0x0, 0x0 };
const uint32_t CLUT_Y_DEPTH[2]                    =      { 0x0, 0x0 };
const uint32_t CLUT_U_Y_SEL[2]                    =      { 0x0, 0x0 };
const uint32_t CLUT_U_BYPASS[2]                   =      { 0x1, 0x1 };
const uint32_t CLUT_U_OFFSET[2]                   =      { 0x0, 0x0 };
const uint32_t CLUT_U_DEPTH[2]                    =      { 0x0, 0x0 };
const uint32_t CLUT_V_Y_SEL[2]                    =      { 0x0, 0x0 };
const uint32_t CLUT_V_BYPASS[2]                   =      { 0x1, 0x1 };
const uint32_t CLUT_V_OFFSET[2]                   =      { 0x0, 0x0 };
const uint32_t CLUT_V_DEPTH[2]                    =      { 0x0, 0x0 };
const uint32_t CLUT_APB_SEL[2]                    =      { 0x0, 0x0 };
const uint32_t SP_SDW_CTRL_TRIG[2]                =      { 0x0, 0x0 };

const uint32_t MLC_SF_PROT_VAL[4]                 =      { 0x7, 0x7, 0x7, 0x7 };
const uint32_t MLC_SF_VPOS_PROT_EN[4]             =      { 0x0, 0x0, 0x0, 0x0};
const uint32_t MLC_SF_SLOWDOWN_EN[4]              =      { 0x0, 0x0, 0x0, 0x0 };
const uint32_t MLC_SF_AFLU_PSEL[4]                =      { 0x0, 0x0, 0x0, 0x0 };
const uint32_t MLC_SF_AFLU_EN[4]                  =      { 0x0, 0x0, 0x0, 0x0 };
const uint32_t MLC_SF_CKEY_EN[4]                  =      { 0x0, 0x0, 0x0, 0x0 };
const uint32_t MLC_SF_G_ALPHA_EN[4]               =      { 0x0, 0x1, 0x0, 0x1 };
const uint32_t MLC_SF_CROP_EN[4]                  =      { 0x0, 0x0, 0x0, 0x0 };
const uint32_t MLC_SF_EN[4]                       =      { 0x0, 0x1, 0x0, 0x0 };
const uint32_t MLC_SF_H_SPOS_H[4]                 =      { 0x0, 0x0, 0x0, 0x0 };
const uint32_t MLC_SF_V_SPOS_V[4]                 =      { 0x0, 0x0, 0x0, 0x0 };
#if 0
const uint32_t MLC_SF_SIZE_V[4]                   =      { 719, 719, 719, 719 };
const uint32_t MLC_SF_SIZE_H[4]                   =      { 1279, 1279, 1279, 1279 };
#endif
const uint32_t MLC_SF_SIZE_V[4]                   =      { 599, 599, 599, 599 };
const uint32_t MLC_SF_SIZE_H[4]                   =      { 799, 799, 799, 799 };
const uint32_t MLC_SF_CROP_END[4]                 =      { 0x0, 0x0, 0x0, 0x0 };
const uint32_t MLC_SF_CROP_START[4]               =      { 0x0, 0x0, 0x0, 0x0 };
const uint32_t MLC_SF_G_ALPHA_A[4]                =      { 0x0, 0xFF, 0x0, 0xFF};
const uint32_t MLC_SF_CKEY_ALPHA_A[4]             =      { 0x0, 0x0, 0x0, 0x0 };
const uint32_t MLC_SF_CKEY_LV_UP[4]               =      { 0x0, 0x0, 0x0, 0x0 };
const uint32_t MLC_SF_CKEY_LV_DN[4]               =      { 0x0, 0x0, 0x0, 0x0 };
const uint32_t MLC_SF_AFLU_TIMER[4]               =      { 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF };
const uint32_t ALPHA_BLD_IDX[5]                   =      { 0xF, 0xF, 0x2, 0xF, 0xF};
const uint32_t LAYER_OUT_IDX[5]                   =      { 0xF, 0xF, 0x2, 0xF, 0xF};
const uint32_t BG_A                               =      0x0;
const uint32_t AFLU_EN                            =      0x0;
const uint32_t FSTART_SEL                         =      0x0;
const uint32_t BG_A_SEL                           =      0x0;
const uint32_t BG_EN                              =      0x0;
const uint32_t ALPHA_BLD_BYPS                     =      0x0;
const uint32_t BG_COLOR_R                         =      0x3ff;
const uint32_t BG_COLOR_G                         =      0x00;
const uint32_t BG_COLOR_B                         =      0x00;
const uint32_t MLC_BG_AFLU_TIMER                  =      0x0;
const uint32_t CANVAS_COLOR_R                     =      0x00;
const uint32_t CANVAS_COLOR_G                     =      0x00;
const uint32_t CANVAS_COLOR_B                     =      0x3ff;
const uint32_t MLC_CLK_RATIO_V                    =      0xFFFF;

const uint32_t MLC_MASK_ERR_L_5                   =      0x0;
const uint32_t MLC_MASK_ERR_L_4                   =      0x0;
const uint32_t MLC_MASK_ERR_L_3                   =      0x0;
const uint32_t MLC_MASK_ERR_L_2                   =      0x0;
const uint32_t MLC_MASK_ERR_L_1                   =      0x0;
const uint32_t MLC_MASK_ERR_L_0                   =      0x0;
const uint32_t MLC_MASK_FLU_L_5                   =      0x0;
const uint32_t MLC_MASK_FLU_L_4                   =      0x0;
const uint32_t MLC_MASK_FLU_L_3                   =      0x0;
const uint32_t MLC_MASK_FLU_L_2                   =      0x0;
const uint32_t MLC_MASK_FLU_L_1                   =      0x0;
const uint32_t MLC_MASK_FLU_L_0                   =      0x0;
const uint32_t MLC_MASK_FRM_END                   =      0x0;

const uint32_t MLC_S_SLOWD_L_5                    =      0x0;
const uint32_t MLC_S_SLOWD_L_4                    =      0x0;
const uint32_t MLC_S_SLOWD_L_3                    =      0x0;
const uint32_t MLC_S_SLOWD_L_2                    =      0x0;
const uint32_t MLC_S_SLOWD_L_1                    =      0x0;
const uint32_t MLC_S_SLOWD_L_0                    =      0x0;
const uint32_t MLC_S_CROP_E_L_5                   =      0x0;
const uint32_t MLC_S_CROP_E_L_4                   =      0x0;
const uint32_t MLC_S_CROP_E_L_3                   =      0x0;
const uint32_t MLC_S_CROP_E_L_2                   =      0x0;
const uint32_t MLC_S_CROP_E_L_1                   =      0x0;
const uint32_t MLC_S_CROP_E_L_0                   =      0x0;
const uint32_t MLC_S_E_L_5                        =      0x0;
const uint32_t MLC_S_E_L_4                        =      0x0;
const uint32_t MLC_S_E_L_3                        =      0x0;
const uint32_t MLC_S_E_L_2                        =      0x0;
const uint32_t MLC_S_E_L_1                        =      0x0;
const uint32_t MLC_S_E_L_0                        =      0x0;
const uint32_t MLC_S_FLU_L_5                      =      0x0;
const uint32_t MLC_S_FLU_L_4                      =      0x0;
const uint32_t MLC_S_FLU_L_3                      =      0x0;
const uint32_t MLC_S_FLU_L_2                      =      0x0;
const uint32_t MLC_S_FLU_L_1                      =      0x0;
const uint32_t MLC_S_FLU_L_0                      =      0x0;
const uint32_t MLC_S_FRM_END                      =      0x0;
#if 0
const uint32_t TCON_HACT                          =      1279;
const uint32_t TCON_HTOL                          =      1649;
const uint32_t TCON_HSBP                          =      259;
const uint32_t TCON_HSYNC                         =      39;
const uint32_t TCON_VACT                          =      719;
const uint32_t TCON_VTOL                          =      749;
const uint32_t TCON_VSBP                          =      24;
const uint32_t TCON_VSYNC                         =      4;
#endif
const uint32_t TCON_HACT                          =      799;
const uint32_t TCON_HTOL                          =      1055;
const uint32_t TCON_HSBP                          =      215;
const uint32_t TCON_HSYNC                         =      127;
const uint32_t TCON_VACT                          =      599;
const uint32_t TCON_VTOL                          =      627;
const uint32_t TCON_VSBP                          =      26;
const uint32_t TCON_VSYNC                         =      3;
const uint32_t TCON_PIX_SCR                       =      0x0;
const uint32_t TCON_DSP_CLK_EN                    =      0x1;
const uint32_t TCON_DSP_CLK_POL                   =      0x0;
const uint32_t TCON_DE_POL                        =      0x0;
const uint32_t TCON_VSYNC_POL                     =      0x0;
const uint32_t TCON_HSYNC_POL                     =      0x0;
const uint32_t TCON_EN                            =      0x1;
#if 0
const uint32_t TCON_LAYER_KICK_Y[18]              =      { 0x2d4, 0x2d4, 0x2d5, 0x2d5, 0x2d5, 0x2d5, 0x2d5, 0x2d5, 0x2d5, 0x2ed, 0x2d5, 0x2d5, 0x2d5, 0x2d5, 0x2d5, 0x2d5, 0x2d5, 0x2d5};
const uint32_t TCON_LAYER_KICK_X[18]              =      { 0x556, 0x580, 0x576, 0x586, 0x2d5, 0x2d5, 0x2d5, 0x2d5, 0x2d5, 0x258, 0x2d5, 0x2d5, 0x2d5, 0x2d5, 0x2d5, 0x2d5, 0x2d5, 0x2d5};
#endif
const uint32_t TCON_LAYER_KICK_Y[18]              =      { 0x259, 0x25a, 0x25b, 0x25b, 0x25c, 0x25c, 0x25c, 0x25c, 0x25c, 0x25c, 0x25c, 0x25c, 0x25c, 0x25c, 0x25c, 0x25c, 0x25c, 0x25c};
const uint32_t TCON_LAYER_KICK_X[18]              =      { 0x322, 0x323, 0x324, 0x324, 0x325, 0x325, 0x325, 0x325, 0x325, 0x325, 0x325, 0x325, 0x325, 0x325, 0x325, 0x325, 0x325, 0x325};
const uint32_t TCON_LAYER_KICK_EN[18]             =      { 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1};

const uint32_t TCON_UNDERRUN_S                    =      0x0;

const uint32_t DC_CSC_ALPHA[2]                    =      { 0x0, 0x0 };
const uint32_t DC_CSC_SBUP_CONV[2]                =      { 0x0, 0x0 };
const uint32_t DC_CSC_BYPASS[2]                   =      { 0x1, 0x1 };
const uint32_t DC_CSC_COEF1_A01[2]                =      { 0x0, 0x0 };
const uint32_t DC_CSC_COEF1_A00[2]                =      { 0x0, 0x0 };
const uint32_t DC_CSC_COEF2_A10[2]                =      { 0x0, 0x0 };
const uint32_t DC_CSC_COEF2_A02[2]                =      { 0x0, 0x0 };
const uint32_t DC_CSC_COEF3_A12[2]                =      { 0x0, 0x0 };
const uint32_t DC_CSC_COEF3_A11[2]                =      { 0x0, 0x0 };
const uint32_t DC_CSC_COEF4_A21[2]                =      { 0x0, 0x0 };
const uint32_t DC_CSC_COEF4_A20[2]                =      { 0x0, 0x0 };
const uint32_t DC_CSC_COEF5_B0[2]                 =      { 0x0, 0x0 };
const uint32_t DC_CSC_COEF5_A22[2]                =      { 0x0, 0x0 };
const uint32_t DC_CSC_COEF6_B2[2]                 =      { 0x0, 0x0 };
const uint32_t DC_CSC_COEF6_B1[2]                 =      { 0x0, 0x0 };
const uint32_t DC_CSC_COEF7_C1[2]                 =      { 0x0, 0x0 };
const uint32_t DC_CSC_COEF7_C0[2]                 =      { 0x0, 0x0 };
const uint32_t DC_CSC_COEF8_C2[2]                 =      { 0x0, 0x0 };

const uint32_t GAMMA_APB_RD_TO                    =      0x0;
const uint32_t GMMA_BYPASS                        =      0x1;
const uint32_t DITHER_V_DEP                       =      0x0;
const uint32_t DITHER_U_DEP                       =      0x0;
const uint32_t DITHER_Y_DEP                       =      0x0;
const uint32_t DITHER_MODE_12                     =      0x1;
const uint32_t DITHER_SPA_LSB_EXP_MODE            =      0x0;
const uint32_t DITHER_SPA_1ST                     =      0x0;
const uint32_t DITHER_SPA_EN                      =      0x0;
const uint32_t DITHER_TEM_EN                      =      0x0;
const uint32_t DITHER_BYPASS                      =      0x1;

const uint32_t CRC32_VSYNC_POL                    =      0x0;
const uint32_t CRC32_HSYNC_POL                    =      0x0;
const uint32_t CRC32_DATA_EN_POL                  =      0x0;
const uint32_t CRC32_GLOBAL_ENABLE                =      0x0;
const uint32_t CRC_ERROR                          =      0x0;
const uint32_t CRC_DONE                           =      0x0;

const uint32_t CRC32_BLOCK_ENABLE[8]              =      { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
const uint32_t CRC32_POS_START_Y[8]               =      { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
const uint32_t CRC32_POS_START_X[8]               =      { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
const uint32_t CRC32_POS_END_Y[8]                 =      { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
const uint32_t CRC32_POS_END_X[8]                 =      { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
const uint32_t CRC32_EXPECT_DATA[8]               =      { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
const uint32_t CRC32_RESULT_DATA[8]               =      { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
