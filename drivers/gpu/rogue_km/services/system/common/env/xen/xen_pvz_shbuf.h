/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 * Based on xen_drm_front_shbuf.h (Xen para-virtual DRM device)
 *
 * Copyright (C) 2016-2018 EPAM Systems Inc.
 *
 * Author: Oleksandr Andrushchenko <oleksandr_andrushchenko@epam.com>
 */

#ifndef __XEN_PVZ_FRONT_SHBUF_H_
#define __XEN_PVZ_FRONT_SHBUF_H_

#include <linux/kernel.h>
#include <xen/grant_table.h>
#include "xengpu.h"
#include "dma_support.h"

struct xen_pvz_shbuf {
	/*
	 * number of references granted for the backend use:
	 *  - for allocated/imported buffers this holds number of grant
	 *    references for the page directory and pages of the buffer
	 */
	int num_grefs;
	grant_ref_t *grefs;
	unsigned char *directory;

	/*
	 * there are 2 ways to provide backing storage for this shared buffer:
	 * either pages or base address. if buffer created from address then we own
	 * the pages and must free those ourselves on closure
	 */
	int num_pages;
	struct page **pages;
	uint64_t addr;

	struct xenbus_device *xb_dev;
};

struct xen_pvz_shbuf_cfg {
	struct xenbus_device *xb_dev;
	size_t size;
	uint64_t addr;
	struct page **pages;
};

struct dev_heap_object {
	uint64_t addr;
	uint64_t vAddr;
	grant_handle_t *map_handles;

	/* these are pages from Xen balloon for allocated Guest FW heap */
	uint32_t num_pages;
	struct page **pages;
};

struct xen_drvinfo {
	struct xenbus_device *xb_dev;
	struct xen_pvz_shbuf *shbuf;
	struct dev_heap_object *heap_obj;
	DMA_ALLOC *psDmaAlloc;
};

struct xen_pvz_shbuf *xen_pvz_shbuf_alloc(
		struct xen_pvz_shbuf_cfg *cfg);

#define xen_page_to_vaddr(page) \
				((phys_addr_t)pfn_to_kaddr(page_to_xen_pfn(page)))


grant_ref_t xen_pvz_shbuf_get_dir_start(struct xen_pvz_shbuf *buf);

void xen_pvz_shbuf_free(struct xen_pvz_shbuf *buf);
int xdrv_map_dev_heap(struct xen_drvinfo *drv_info,
		grant_ref_t gref_directory, uint64_t buffer_sz);
int xdrv_unmap_dev_heap(struct xen_drvinfo *drv_info);
#endif /* __XEN_PVZ_FRONT_SHBUF_H_ */
