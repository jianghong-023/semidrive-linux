/*************************************************************************/ /*!
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#if defined(LINUX)

#include <asm/io.h>
#include <asm/page.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/dma-buf.h>
#include <../drivers/staging/android/ion/ion.h>
/* For MODULE_LICENSE */
#include "pvrmodule.h"

#include "dc_osfuncs.h"
#include "dc_semi.h"

#ifndef DC_SEMI_WIDTH
#define DC_SEMI_WIDTH 640
#endif

#ifndef DC_SEMI_HEIGHT
#define DC_SEMI_HEIGHT 480
#endif

#ifndef DC_SEMI_BIT_DEPTH
#define DC_SEMI_BIT_DEPTH 32
#endif

#ifndef DC_SEMI_MEMORY_LAYOUT
#define DC_SEMI_MEMORY_LAYOUT 0
#endif

#ifndef DC_SEMI_FBC_FORMAT
#define DC_SEMI_FBC_FORMAT 0
#endif

#ifndef DC_SEMI_REFRESH_RATE
#define DC_SEMI_REFRESH_RATE 60
#endif

#ifndef DC_SEMI_DPI
#define DC_SEMI_DPI 160
#endif


DC_SEMI_MODULE_PARAMETERS sModuleParams =
{
	.ui32Width = DC_SEMI_WIDTH,
	.ui32Height = DC_SEMI_HEIGHT,
	.ui32Depth = DC_SEMI_BIT_DEPTH,
	.ui32MemLayout = DC_SEMI_MEMORY_LAYOUT,
	.ui32FBCFormat = DC_SEMI_FBC_FORMAT,
	.ui32RefreshRate = DC_SEMI_REFRESH_RATE,
	.ui32XDpi = DC_SEMI_DPI,
	.ui32YDpi = DC_SEMI_DPI
};

module_param_named(width, 	sModuleParams.ui32Width, uint, S_IRUGO);
module_param_named(height, 	sModuleParams.ui32Height, uint, S_IRUGO);
module_param_named(depth, 	sModuleParams.ui32Depth, uint, S_IRUGO);
module_param_named(memlayout, 	sModuleParams.ui32MemLayout, uint, S_IRUGO);
module_param_named(fbcformat, 	sModuleParams.ui32FBCFormat, uint, S_IRUGO);
module_param_named(refreshrate, sModuleParams.ui32RefreshRate, uint, S_IRUGO);
module_param_named(xdpi,	sModuleParams.ui32XDpi, uint, S_IRUGO);
module_param_named(ydpi,	sModuleParams.ui32YDpi, uint, S_IRUGO);

const DC_SEMI_MODULE_PARAMETERS *DCSemiGetModuleParameters(void)
{
	return &sModuleParams;
}

IMG_HANDLE DCSemiAllocUncached(size_t uiSize)
{
	int fd = 0;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment * attach = NULL;

	fd = ion_alloc(uiSize, ION_HEAP_TYPE_DMA, 0);
	dmabuf = dma_buf_get(fd);
	//attach = dma_buf_attach(dmabuf, dev);
	//dma_buf_map_attachment();
	return dmabuf;
}

IMG_BOOL DCSemiFree(IMG_HANDLE pHandle, size_t uiSize)
{
	struct dma_buf *dmabuf = (struct dma_buf *)pHandle;
	PVR_UNREFERENCED_PARAMETER(uiSize);

	//dma_buf_unmap_attachment()
	//dma_buf_detach()
	dma_buf_put(dmabuf);
	
	return IMG_TRUE;
}

IMG_CPU_VIRTADDR DCSemiAcquire(IMG_HANDLE pHandle)
{
	struct dma_buf *dmabuf = (struct dma_buf *)pHandle;
	PVR_UNREFERENCED_PARAMETER(pHandle);
	dma_buf_begin_cpu_access(dmabuf, DMA_BIDIRECTIONAL);

	return dma_buf_kmap(dmabuf,0);
}

IMG_BOOL DCSemiRelease(IMG_HANDLE pHandle,IMG_HANDLE pAddr)
{
	struct dma_buf *dmabuf = (struct dma_buf *)pHandle;
	PVR_UNREFERENCED_PARAMETER(pHandle);

	dma_buf_kunmap(dmabuf,0, pAddr);
	dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	return IMG_TRUE;
}

//#define	VMALLOC_TO_PAGE_PHYS(vAddr) page_to_phys(vmalloc_to_page(vAddr))

PVRSRV_ERROR DCSemiLinAddrToDevPAddrs(IMG_CPU_VIRTADDR pvLinAddr,
					 IMG_DEV_PHYADDR *pasDevPAddr,
					 size_t uiSize)
{
	unsigned long ulPages = DC_OS_BYTES_TO_PAGES(uiSize);
	int i;

	for (i = 0; i < ulPages; i++)
	{
		//pasDevPAddr[i].uiAddr = VMALLOC_TO_PAGE_PHYS(pvLinAddr);
		pasDevPAddr[i].uiAddr = virt_to_phys(pvLinAddr);
		pvLinAddr += PAGE_SIZE;
	}

	return PVRSRV_OK;
}

static int __init dc_semi_init(void)
{
	if (DCSemiInit() != PVRSRV_OK)
	{
		return -ENODEV;
	}
	return 0;
}

/*****************************************************************************
 Function Name: DC_NOHW_Cleanup
 Description  : Remove the driver from the kernel.

                __exit places the function in a special memory section that
                the kernel frees once the function has been run.  Refer also
                to module_exit() macro call below.
*****************************************************************************/
static void __exit dc_semi_deinit(void)
{
	DCSemiDeInit();
}

module_init(dc_semi_init);
module_exit(dc_semi_deinit);

#endif /* defined(LINUX) */

