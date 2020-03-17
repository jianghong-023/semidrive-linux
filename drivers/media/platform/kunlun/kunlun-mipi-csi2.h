/*
 * kunlun-mipi-csi2.h
 *
 * Semidrive kunlun platform mipi csi2 header file
 *
 * Copyright (C) 2019, Semidrive  Communications Inc.
 *
 * This file is licensed under a dual GPLv2 or X11 license.
 */

#ifndef SD_KUNLUN_MIPI_CSI2_H
#define SD_KUNLUN_MIPI_CSI2_H

#include <linux/clk.h>
#include <linux/media-bus-format.h>
#include <linux/media.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/slab.h>
#include <linux/videodev2.h>

#include <media/media-device.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-fwnode.h>
#include <linux/phy/phy.h>
//#include <media/v4l2-dv-timings.h>
//#include <linux/io.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include "kunlun-csi.h"
#define KUNLUN_MIPI_CSI2_NAME           "kunlun-mipi-csi2"

#define KUNLUN_MIPI_CSI2_VC0_PAD_SINK       0
#define KUNLUN_MIPI_CSI2_VC1_PAD_SINK       1
#define KUNLUN_MIPI_CSI2_VC2_PAD_SINK       2
#define KUNLUN_MIPI_CSI2_VC3_PAD_SINK       3
#define KUNLUN_MIPI_CSI2_VC0_PAD_SRC        4
#define KUNLUN_MIPI_CSI2_VC1_PAD_SRC        5
#define KUNLUN_MIPI_CSI2_VC2_PAD_SRC        6
#define KUNLUN_MIPI_CSI2_VC3_PAD_SRC        7
#define KUNLUN_MIPI_CSI2_PAD_NUM        8


#define CSI_MAX_ENTITIES    2

#define KUNLUN_MIPI_CSI2_IPI_NUM        4


/* data type */
#define DT_YUV420_LE_8_O 0x18
#define DT_YUV420_10_O 0x19
#define DT_YUV420_LE_8_E 0x1C
#define DT_YUV420_10_E 0x1D
#define DT_YUV422_8 0x1E
#define DT_YUV422_10 0x1F
#define DT_RGB444 0x20
#define DT_RGB555 0x21
#define DT_RGB565 0x22
#define DT_RGB666 0x23
#define DT_RGB888 0x24
#define DT_RAW8 0x2A
#define DT_RAW10 0x2B

/* ipi mode */
#define IPI_MODE_ENABLE 1<<24
#define IPI_MODE_CUT_THROUGH 1<<16

/* ipi advance features */
#define IPI_ADV_SYNC_LEGACCY 1<<24
#define IPI_ADV_USE_EMBEDDED 1<<21
#define IPI_ADV_USE_BLANKING 1<<20
#define IPI_ADV_USE_NULL 1<<19
#define IPI_ADV_USE_LINE_START 1<<18
#define IPI_ADV_USE_VIDEO 1<<17
#define IPI_ADV_SEL_LINE_EVENT 1<<16


/** @short DWC MIPI CSI-2 register addresses*/
enum register_addresses {
    R_CSI2_VERSION = 0x00,
    R_CSI2_N_LANES = 0x04,
    R_CSI2_CTRL_RESETN = 0x08,
    R_CSI2_INTERRUPT = 0x0C,
    R_CSI2_DATA_IDS_1 = 0x10,
    R_CSI2_DATA_IDS_2 = 0x14,
    R_CSI2_INT_ST_AP_MAIN = 0x2C,

    R_CSI2_DPHY_SHUTDOWNZ = 0x40,
    R_CSI2_DPHY_RSTZ = 0x44,
    R_CSI2_DPHY_RX = 0x48,
    R_CSI2_DPHY_STOPSTATE = 0x4C,
    R_CSI2_DPHY_TST_CTRL0 = 0x50,
    R_CSI2_DPHY_TST_CTRL1 = 0x54,

    R_CSI2_IPI_MODE = 0x80,
    R_CSI2_IPI_VCID = 0x84,
    R_CSI2_IPI_DATA_TYPE = 0x88,
    R_CSI2_IPI_MEM_FLUSH = 0x8C,
    R_CSI2_IPI_HSA_TIME = 0x90,
    R_CSI2_IPI_HBP_TIME = 0x94,
    R_CSI2_IPI_HSD_TIME = 0x98,
    R_CSI2_IPI_HLINE_TIME = 0x9C,
    R_CSI2_IPI_SOFTRSTN = 0xA0,
    R_CSI2_IPI_ADV_FEATURES = 0xAC,
    R_CSI2_IPI_VSA_LINES = 0xB0,
    R_CSI2_IPI_VBP_LINES = 0xB4,
    R_CSI2_IPI_VFP_LINES = 0xB8,
    R_CSI2_IPI_VACTIVE_LINES = 0xBC,
    R_CSI2_PHY_CAL = 0Xcc,
    R_CSI2_INT_PHY_FATAL = 0xe0,
    R_CSI2_MASK_INT_PHY_FATAL = 0xe4,
    R_CSI2_FORCE_INT_PHY_FATAL = 0xe8,
    R_CSI2_INT_PKT_FATAL = 0xf0,
    R_CSI2_MASK_INT_PKT_FATAL = 0xf4,
    R_CSI2_FORCE_INT_PKT_FATAL = 0xf8,
    R_CSI2_INT_FRAME_FATAL = 0x100,
    R_CSI2_MASK_INT_FRAME_FATAL = 0x104,
    R_CSI2_FORCE_INT_FRAME_FATAL = 0x108,
    R_CSI2_INT_PHY = 0x110,
    R_CSI2_MASK_INT_PHY = 0x114,
    R_CSI2_FORCE_INT_PHY = 0x118,
    R_CSI2_INT_PKT = 0x120,
    R_CSI2_MASK_INT_PKT = 0x124,
    R_CSI2_FORCE_INT_PKT = 0x128,
    R_CSI2_INT_LINE = 0x130,
    R_CSI2_MASK_INT_LINE = 0x134,
    R_CSI2_FORCE_INT_LINE = 0x138,
    R_CSI2_INT_IPI = 0x140,
    R_CSI2_MASK_INT_IPI = 0x144,
    R_CSI2_FORCE_INT_IPI = 0x148,

    R_CSI2_IPI2_MODE = 0x200,
    R_CSI2_IPI2_VCID = 0x204,
    R_CSI2_IPI2_DATA_TYPE = 0X208,
    R_CSI2_IPI2_MEM_FLUSH = 0x20c,
    R_CSI2_IPI2_HSA_TIME = 0X210,
    R_CSI2_IPI2_HBP_TIME = 0X214,
    R_CSI2_IPI2_HSD_TIME = 0X218,
    R_CSI2_IPI2_ADV_FEATURES = 0X21C,

    R_CSI2_IPI3_MODE = 0X220,
    R_CSI2_IPI3_VCID = 0X224,
    R_CSI2_IPI3_DATA_TYPE = 0X228,
    R_CSI2_IPI3_MEM_FLUSH = 0X22C,
    R_CSI2_IPI3_HSA_TIME = 0X230,
    R_CSI2_IPI3_HBP_TIME = 0X234,
    R_CSI2_IPI3_HSD_TIME = 0X238,
    R_CSI2_IPI3_ADV_FEATURES = 0X23C,

    R_CSI2_IPI4_MODE = 0X240,
    R_CSI2_IPI4_VCID = 0X244,
    R_CSI2_IPI4_DATA_TYPE = 0X248,
    R_CSI2_IPI4_MEM_FLUSH = 0X24C,
    R_CSI2_IPI4_HSA_TIME = 0X250,
    R_CSI2_IPI4_HBP_TIME = 0X254,
    R_CSI2_IPI4_HSD_TIME = 0X258,
    R_CSI2_IPI4_ADV_FEATURES = 0X25C,

    R_CSI2_IPI_RAM_ERR_LOG_AP = 0X2E0,
    R_CSI2_IPI_RAM_ERR_ADDR_AP = 0X2E4,
    R_CSI2_IPI2_RAM_ERR_LOG_AP = 0X2E8,
    R_CSI2_IPI2_RAM_ERR_ADDR_AP = 0X2EC,
    R_CSI2_IPI3_RAM_ERR_LOG_AP = 0X2F0,
    R_CSI2_IPI3_RAM_ERR_ADDR_AP = 0X2F4,
    R_CSI2_IPI4_RAM_ERR_LOG_AP = 0X2F8,
    R_CSI2_IPI4_RAM_ERR_ADDR_AP = 0X2FC,

    R_CSI2_SCRAMBLING = 0X300,
    R_CSI2_SCRAMBLING_SEED1 = 0X304,
    R_CSI2_SCRAMBLING_SEED2 = 0X308,
    R_CSI2_SCRAMBLING_SEED3 = 0X30C,
    R_CSI2_SCRAMBLING_SEED4 = 0x310,
    R_CSI2_N_SYNC = 0X340,
    R_CSI2_ERR_INJ_CTRL_AP = 0X344,
    R_CSI2_ERR_INJ_CHK_MSK_AP = 0X348,
    R_CSI2_ERR_INJ_DH32_MSK_AP = 0X34C,
    R_CSI2_ERR_INJ_DL32_MSK_AP = 0X350,
    R_CSI2_ERR_INJ_ST_AP = 0X354,
    R_CSI2_ST_FAP_PHY_FATAL = 0X360,
    R_CSI2_INT_MSK_FAP_PHY_FATAL = 0X364,
    R_CSI2_INT_FORCE_FAP_PHY_FATAL = 0X368,
    R_CSI2_INT_ST_FAP_PKT_FATAL = 0X370,
    R_CSI2_INT_MSK_FAP_PKT_FATAL = 0X374,
    R_CSI2_INT_FORCE_FAP_PKT_FATAL = 0X378,
    R_CSI2_INT_ST_FAP_FRAME_FATAL = 0X380,
    R_CSI2_INT_MSK_FAP_FRAME_FATAL = 0X384,
    R_CSI2_INT_FORCE_FAP_FRAME_FATAL = 0X388,
    R_CSI2_INT_ST_FAP_PHY = 0X390,
    R_CSI2_INT_MSK_FAP_PHY = 0X394,
    R_CSI2_INT_FORCE_FAP_PHY = 0X398,
    R_CSI2_INT_ST_FAP_PKT = 0X3A0,
    R_CSI2_INT_MSK_FAP_PKT = 0X3A4,
    R_CSI2_INT_FORCE_FAP_PKT = 0X3A8,
    R_CSI2_INT_ST_FAP_LINE = 0X3B0,
    R_CSI2_INT_MSK_FAP_LINE = 0X3B4,
    R_CSI2_INT_FORCE_FAP_LINE = 0X3B8,
    R_CSI2_INT_ST_FAP_IPI = 0X3C0,
    R_CSI2_INT_MSK_FAP_IPI = 0X3C4,
    R_CSI2_INT_FORCE_FAP_IPI = 0X3C8,
    R_CSI2_INT_ST_FAP_IPI2 = 0X3D0,
    R_CSI2_INT_MSK_FAP_IPI2 = 0X3D4,
    R_CSI2_INT_FORCE_FAP_IPI2 = 0X3D8,
    R_CSI2_INT_ST_FAP_IPI3 = 0X3E0,
    R_CSI2_INT_MSK_FAP_IPI3 = 0X3E4,
    R_CSI2_INT_FORCE_FAP_IPI3 = 0X3E8,
    R_CSI2_INT_ST_FAP_IPI4 = 0X3F0,
    R_CSI2_INT_MSK_FAP_IPI4 = 0X3F4,
    R_CSI2_INT_FORCE_FAP_IPI4 = 0X3F8,
};

/** @short Interrupt Masks */
enum interrupt_type {
    CSI2_INT_PHY_FATAL = 1 << 0,
    CSI2_INT_PKT_FATAL = 1 << 1,
    CSI2_INT_FRAME_FATAL = 1 << 2,
    CSI2_INT_PHY = 1 << 16,
    CSI2_INT_PKT = 1 << 17,
    CSI2_INT_LINE = 1 << 18,
    CSI2_INT_IPI = 1 << 19,
};

/**
 * @short Format template
 */
struct mipi_fmt {
    const char *name;
    u32 code;
    u8 depth;
};

struct csi_hw {

    uint32_t num_lanes;
    uint32_t output_type;   //IPI = 0; IDI = 1; BOTH = 2
    /*IPI Info */
    uint32_t ipi_mode;
    uint32_t ipi_color_mode;
    uint32_t ipi_auto_flush;
    uint32_t virtual_ch;

    uint32_t hsa;
    uint32_t hbp;
    uint32_t hsd;
    uint32_t htotal;

    uint32_t vsa;
    uint32_t vbp;
    uint32_t vfp;
    uint32_t vactive;

    uint32_t adv_feature;
};

struct kunlun_csi_mipi_csi2 {
    u32 id;
    //void __iomem *base;
    struct device *dev;
    struct v4l2_subdev subdev;

    struct mutex lock;
    spinlock_t slock;

    struct media_pad pads[KUNLUN_MIPI_CSI2_PAD_NUM];
    struct platform_device *pdev;
    u8 index;


    /** Store current format */
    const struct mipi_fmt *fmt;
    struct v4l2_mbus_framefmt format;

    /** Device Tree Information */
    void __iomem *base_address;
    void __iomem *dispmux_address;
    uint32_t ctrl_irq_number;

    uint32_t lanerate;
    uint32_t lane_count;

    struct csi_hw hw;

    struct phy *phy;
};


struct phy_freqrange {
    int index;
    int range_l;
    int range_h;
};

#endif
