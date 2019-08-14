#include <xen/xenbus.h>
#include <xen/page.h>

#include "xengpu.h"

struct xen_gpuif {
        /* Unique identifier for this interface. */
        domid_t                 domid;
        unsigned int            handle;
        /* Back pointer to the backend_info. */
        struct backend_info     *be;
        struct gpuif_back_ring back_ring;
    unsigned int gpuif_irq;
};

struct backend_info {
        struct xenbus_device    *dev;
        struct xen_gpuif        *gpuif;
        struct xenbus_watch     backend_watch;
        unsigned                major;
        unsigned                minor;
        char                    *mode;
};

