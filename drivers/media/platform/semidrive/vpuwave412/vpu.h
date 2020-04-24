//--=========================================================================--
//  This file is linux device driver for VPU.
//-----------------------------------------------------------------------------
//
//       This confidential and proprietary software may be used only
//     as authorized by a licensing agreement from Chips&Media Inc.
//     In the event of publication, the following notice is applicable:
//
//            (C) COPYRIGHT 2006 - 2015  CHIPS&MEDIA INC.
//                      ALL RIGHTS RESERVED
//
//       The entire notice above must be reproduced on all authorized
//       copies.
//
//--=========================================================================--

#ifndef __VPU_DRV_H__
#define __VPU_DRV_H__

#include <linux/fs.h>
#include <linux/types.h>

#define USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY

#define VDI_IOCTL_MAGIC  'V'
#define VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY  _IO(VDI_IOCTL_MAGIC, 0)
#define VDI_IOCTL_FREE_PHYSICALMEMORY       _IO(VDI_IOCTL_MAGIC, 1)
#define VDI_IOCTL_WAIT_INTERRUPT            _IO(VDI_IOCTL_MAGIC, 2)
#define VDI_IOCTL_SET_CLOCK_GATE            _IO(VDI_IOCTL_MAGIC, 3)
#define VDI_IOCTL_RESET                     _IO(VDI_IOCTL_MAGIC, 4)
#define VDI_IOCTL_GET_INSTANCE_POOL         _IO(VDI_IOCTL_MAGIC, 5)
#define VDI_IOCTL_GET_COMMON_MEMORY         _IO(VDI_IOCTL_MAGIC, 6)
#define VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO _IO(VDI_IOCTL_MAGIC, 8)
#define VDI_IOCTL_OPEN_INSTANCE             _IO(VDI_IOCTL_MAGIC, 9)
#define VDI_IOCTL_CLOSE_INSTANCE            _IO(VDI_IOCTL_MAGIC, 10)
#define VDI_IOCTL_GET_INSTANCE_NUM          _IO(VDI_IOCTL_MAGIC, 11)
#define VDI_IOCTL_GET_REGISTER_INFO         _IO(VDI_IOCTL_MAGIC, 12)
#define VDI_IOCTL_DEVICE_MEMORY_MAP         _IO(VDI_IOCTL_MAGIC, 13)
#define VDI_IOCTL_DEVICE_MEMORY_UNMAP       _IO(VDI_IOCTL_MAGIC, 14)
#define VDI_IOCTL_DEVICE_SRAM_CFG           _IO(VDI_IOCTL_MAGIC, 15)
#define VDI_IOCTL_DEVICE_GET_SRAM_INFO      _IO(VDI_IOCTL_MAGIC, 16)

/**
 *@Brief Description of buffer memory allocated by driver
 *@param size
 *@param phys_add phy address in kernel
 *@param base   virt address in kernel
 *@param virt_addr virt address in user space
 *@param dam_addr device memory address
 *@param attachment  for vpu map dma device
 *@param buf_handle  dam buffer handle
 */
typedef struct vpudrv_buffer_t {
    uint32_t size;
    uint64_t phys_addr;
    uint64_t base;
    uint64_t virt_addr;
    uint64_t dma_addr;
    uint64_t attachment;
    uint64_t sgt;
    int32_t	 buf_handle;
} vpudrv_buffer_t;

typedef struct vpu_bit_firmware_info_t {
	uint32_t size;				/* size of this structure*/
	uint32_t core_idx;
	uint64_t reg_base_offset;
	uint16_t bit_code[512];
} vpu_bit_firmware_info_t;

typedef struct vpudrv_inst_info_t {
	uint32_t core_idx;
	uint32_t inst_idx;
	int32_t inst_open_count;	   /* for output only*/
} vpudrv_inst_info_t;

typedef struct vpudrv_intr_info_t {
	uint32_t timeout;
	int32_t	 intr_reason;
} vpudrv_intr_info_t;

#endif
