//--=========================================================================--
//  This file is a part of VPU Reference API project
//-----------------------------------------------------------------------------
//
//       This confidential and proprietary software may be used only
//     as authorized by a licensing agreement from Chips&Media Inc.
//     In the event of publication, the following notice is applicable:
//
//            (C) COPYRIGHT 2006 - 2011  CHIPS&MEDIA INC.
//                      ALL RIGHTS RESERVED
//
//       The entire notice above must be reproduced on all authorized
//       copies.
//      This file should be modified by some customers according to their SOC configuration.
//--=========================================================================--

#ifndef _WAVE_CONFIG_H_
#define _WAVE_CONFIG_H_

#define WAVE412_CODE                    0x4120

#define PRODUCT_CODE_WAVE412(x) (x == WAVE412_CODE)
#define MAX_INST_HANDLE_SIZE            48              /* DO NOT CHANGE THIS VALUE */
#define MAX_NUM_INSTANCE                4
#define MAX_NUM_VPU_CORE                1
#define MAX_NUM_VCORE                   1

/************************************************************************/
/* VPU COMMON MEMORY                                                    */
/************************************************************************/
#define SIZE_COMMON                     (4*1024*1024)


/* other config add following */

/* virtualization  for wave412 */

/* smmu config for wave412*/

#endif  /* _WAVE_CONFIG_H_ */

