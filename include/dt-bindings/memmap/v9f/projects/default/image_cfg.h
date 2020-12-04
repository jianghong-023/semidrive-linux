
//*****************************************************************************
//
// WARNING: Automatically generated file, don't modify anymore!!!
//
// Copyright (c) 2019-2029 Semidrive Incorporated.  All rights reserved.
// Software License Agreement
//
//*****************************************************************************

#ifndef __IMAGE_CFG_H__
#define __IMAGE_CFG_H__

#define LOW32(x) ((x) & 0xffffffff)
#define HIGH32(x) ((x) >> 32)

#define BOARD_RAMDISK_OFFSET 0x8000000
#define BOARD_TARS_OFFSET 0x0
#define BOARD_KERNEL_OFFSET 0x280000
#define SEC_MEMBASE 0x30000000
#define SEC_MEMSIZE 0x600000
#define SYS_CFG_MEMBASE 0x30600000
#define SYS_CFG_MEMSIZE 0x200000
#define SAF_SEC_MEMBASE 0x30800000
#define SAF_SEC_MEMSIZE 0x80000
#define SEC_AP2_MEMBASE 0x30880000
#define SEC_AP2_MEMSIZE 0x80000
#define SAF_AP2_MEMBASE 0x30900000
#define SAF_AP2_MEMSIZE 0x1000000
#define AP2_ATF_MEMBASE 0x41900000
#define AP2_ATF_MEMSIZE 0x200000
#define SAF_MEMBASE 0x31B00000
#define SAF_MEMSIZE 0x2000000
#define AP2_REE_MEMBASE 0x43C00000
#define AP2_REE_MEMSIZE 0x3C500000
#define AP2_PRELOADER_MEMBASE 0x45C00000
#define AP2_PRELOADER_MEMSIZE 0x400000
#define AP2_BOOTLOADER_MEMBASE 0x48000000
#define AP2_BOOTLOADER_MEMSIZE 0x800000
#define AP2_KERNEL_MEMBASE 0x43E80000
#define AP2_KERNEL_MEMSIZE 0x3C300000
#define AP2_BOARD_RAMDISK_MEMBASE 0x4BC00000
#define AP2_BOARD_RAMDISK_MEMSIZE 0x2000000

#endif /* __IMAGE_CFG_H__*/
