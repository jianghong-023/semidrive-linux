#include <xen/xenbus.h>
#include <xen/page.h>

#include "xengpu.h"
#include "xen_pvz_shbuf.h"

struct xen_gpuif {
    /* Unique identifier for this interface. */
    domid_t                 domid;
    uint32_t                ui32DevID;
    unsigned int            handle;
    /* Back pointer to the backend_info. */
    struct backend_info     *be;
    struct gpuif_back_ring  back_ring;
	unsigned int gpuif_irq;
	void * configaddr;
};

struct backend_info {
    struct xenbus_device    *dev;
	struct xen_pvz_shbuf    *shbuf;
	struct dev_heap_object  *heap_obj;
	DMA_ALLOC *psDmaAlloc;

    struct xen_gpuif        *gpuif;
    unsigned                major;
    unsigned                minor;
    char                    *mode;
};

