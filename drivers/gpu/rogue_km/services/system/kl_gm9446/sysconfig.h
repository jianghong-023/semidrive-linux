/* 
* sysconfig.h
* 
* Copyright (c) 2018 Semidrive Semiconductor. 
* All rights reserved. 
* 
* Description: System abstraction layer . 
* 
* Revision History: 
* ----------------- 
* 011, 01/14/2019 Lili create this file 
*/

#include "pvrsrv_device.h"
#include "rgxdevice.h"

#if !defined(__SYSCCONFIG_H__)
#define __SYSCCONFIG_H__


#define RGX_KUNLUN_CORE_CLOCK_SPEED 800000000 //800mHz
#define SYS_RGX_ACTIVE_POWER_LATENCY_MS (100)

/* BIF Tiling mode configuration */
static RGXFWIF_BIFTILINGMODE geBIFTilingMode = RGXFWIF_BIFTILINGMODE_256x16;

/* default BIF tiling heap x-stride configurations. */
static IMG_UINT32 gauiBIFTilingHeapXStrides[RGXFWIF_NUM_BIF_TILING_CONFIGS] =
{
	0, /* BIF tiling heap 1 x-stride */
	1, /* BIF tiling heap 2 x-stride */
	2, /* BIF tiling heap 3 x-stride */
	3  /* BIF tiling heap 4 x-stride */
};

/*****************************************************************************
 * system specific data structures
 *****************************************************************************/
 
#endif	/* __SYSCCONFIG_H__ */

