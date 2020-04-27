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

#ifndef _CODA_CONFIG_H_
#define _CODA_CONFIG_H_

#define CODA980_CODE                    0x9800

#define PRODUCT_CODE_CODA988(x) (x == CODA980_CODE)
#define MAX_INST_HANDLE_SIZE            48              /* DO NOT CHANGE THIS VALUE */
#define MAX_NUM_INSTANCE                4
#define MAX_NUM_VPU_CORE                1
#define MAX_NUM_VCORE                   1

/************************************************************************/
/* VPU COMMON MEMORY                                                    */
/************************************************************************/
#define COMMAND_QUEUE_DEPTH             4
#define ONE_TASKBUF_SIZE_FOR_CQ         (8*1024*1024)   /* upto 8Kx4K, need 8Mbyte per task*/
#define SIZE_COMMON                     (2*1024*1024)

/* other config add following */

/* virtualization  for coda988 */

/* smmu config for coda988 */

#endif  /* _CODA_CONFIG_H_ */

