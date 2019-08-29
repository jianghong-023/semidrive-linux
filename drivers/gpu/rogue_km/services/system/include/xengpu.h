
#ifndef __XEN_PUBLIC_IO_GPUIF_H__
#define __XEN_PUBLIC_IO_GPUIF_H__

#include <xen/interface/io/ring.h>
#include <xen/interface/grant_table.h>

#include <linux/string.h>
#include <linux/semaphore.h>
#include <linux/sched.h>

/*
 * REQUEST CODES.
 */
#define GPUIF_OP_CREATE_PVZCONNECTION             0
#define GPUIF_OP_DESTROY_PVZCONNECTION            1
#define GPUIF_OP_CREATE_DEVPHYSHEAPS              2
#define GPUIF_OP_DESTROY_DEVPHYSHEAPS             3
#define GPUIF_OP_MAP_DEVPHYSHEAPS                 4
#define GPUIF_OP_UNMAP_DEVPHYSHEAPS               5

#define XEN_GPUIF_STATUS_SUCCESS           0
#define XEN_GPUIF_STATUS_NOT_SUPPORTED     1
#define XEN_GPUIF_STATUS_INVALID_PARAMETER 2
#define XEN_GPUIF_STATUS_BUFFER_OVERFLOW   3


struct gpuif_request {
	uint64_t        id;
	uint8_t  operation;    /* GPUIF_OP_???                         */
    uint32_t ui32FuncID;
    uint32_t ui32DevID;
    uint64_t ui64Size;
    uint64_t ui64Addr;
};

struct gpuif_response {
	uint64_t        id;              /* copied from request */
	uint8_t         operation;       /* copied from request */
	int16_t         status;          /* GPUIF_RSP_???       */
    /*below fields are only used for XenVMMCreateDevPhysHeaps */
    uint32_t        eType;
    uint64_t        ui64FwPhysHeapSize;
    uint64_t        ui64FwPhysHeapAddr;
    uint64_t        ui64GpuPhysHeapSize;
    uint64_t        ui64GpuPhysHeapAddr;
};

struct xengpu_call_info {
    struct gpuif_request  *req;
    struct gpuif_response *rsp;
    struct xengpu_call_info *next;
    struct semaphore sem;
};


DEFINE_RING_TYPES(gpuif, struct gpuif_request, struct gpuif_response);


#define XENGPU_REQUEST_INIT(_r) \
    memset((void *)_r, 0, sizeof(struct gpuif_request))

#endif /* __XEN_PUBLIC_IO_GPUIF_H__ */

