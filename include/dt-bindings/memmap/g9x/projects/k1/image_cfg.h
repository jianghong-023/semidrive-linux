
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
#define SAF_MEMBASE 0x30000000
#define SAF_MEMSIZE 0x2000000
#define SEC_MEMBASE 0x32000000
#define SEC_MEMSIZE 0x600000
#define SYS_CFG_MEMBASE 0x32600000
#define SYS_CFG_MEMSIZE 0x200000
#define SDPE_MEMBASE 0x32800000
#define SDPE_MEMSIZE 0x100000
#define SAF_SEC_MEMBASE 0x32900000
#define SAF_SEC_MEMSIZE 0x80000
#define SAF_SDPE_MEMBASE 0x32980000
#define SAF_SDPE_MEMSIZE 0x80000
#define SAF_AP2_MEMBASE 0x32A00000
#define SAF_AP2_MEMSIZE 0x80000
#define SDPE_AP2_MEMBASE 0x32A80000
#define SDPE_AP2_MEMSIZE 0x80000
#define SEC_AP2_MEMBASE 0x32B00000
#define SEC_AP2_MEMSIZE 0x100000
#define SEC_SDPE_MEMBASE 0x32C00000
#define SEC_SDPE_MEMSIZE 0x200000
#define AP2_ATF_MEMBASE 0x42E00000
#define AP2_ATF_MEMSIZE 0x200000
#define AP2_REE_MEMBASE 0x43000000
#define AP2_REE_MEMSIZE 0x3D000000
#define AP2_PRELOADER_MEMBASE 0x45000000
#define AP2_PRELOADER_MEMSIZE 0x400000
#define AP2_BOOTLOADER_MEMBASE 0x47400000
#define AP2_BOOTLOADER_MEMSIZE 0x800000
#define AP2_KERNEL_MEMBASE 0x43280000
#define AP2_KERNEL_MEMSIZE 0x3CE00000
#define AP2_BOARD_RAMDISK_MEMBASE 0x4B000000
#define AP2_BOARD_RAMDISK_MEMSIZE 0x2000000

#endif /* __IMAGE_CFG_H__*/
