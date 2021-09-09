/*
 * Copyright (C) 2013 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/dma-buf.h>
#include <linux/highmem.h>
//#include <linux/memblock.h>
#include <linux/slab.h>

#include <drm/drmP.h>
#include "drm_drv.h"

struct drm_mb_pdata {
	phys_addr_t base;
	size_t size;
};

static struct sg_table *drm_mb_map(struct dma_buf_attachment *attach,
		enum dma_data_direction direction)
{
	struct drm_mb_pdata *pdata = attach->dmabuf->priv;
	struct sg_table *table;
	int ret;

	table = kzalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret < 0)
		goto err_alloc;

	sg_dma_address(table->sgl) = pdata->base;
	sg_dma_len(table->sgl) = pdata->size;

	return table;

err_alloc:
	kfree(table);
	return ERR_PTR(ret);
}

static void drm_mb_unmap(struct dma_buf_attachment *attach,
		struct sg_table *table, enum dma_data_direction direction)
{

	sg_free_table(table);
	kfree(table);
}

static void drm_mb_release(struct dma_buf *buf)
{
	struct drm_mb_pdata *pdata = buf->priv;

	kfree(pdata);
}

static void *drm_mb_do_kmap(struct dma_buf *buf, unsigned long pgoffset,
		bool atomic)
{
	struct drm_mb_pdata *pdata = buf->priv;
	unsigned long pfn = PFN_DOWN(pdata->base) + pgoffset;
	struct page *page = pfn_to_page(pfn);

	if (atomic)
		return kmap_atomic(page);
	else
		return kmap(page);
}

static void *drm_mb_kmap_atomic(struct dma_buf *buf,
		unsigned long pgoffset)
{
	return drm_mb_do_kmap(buf, pgoffset, true);
}

static void drm_mb_kunmap_atomic(struct dma_buf *buf,
		unsigned long pgoffset, void *vaddr)
{
	kunmap_atomic(vaddr);
}

static void *drm_mb_kmap(struct dma_buf *buf, unsigned long pgoffset)
{
	return drm_mb_do_kmap(buf, pgoffset, false);
}

static void drm_mb_kunmap(struct dma_buf *buf, unsigned long pgoffset,
		void *vaddr)
{
	kunmap(vaddr);
}

static int drm_mb_mmap(struct dma_buf *buf, struct vm_area_struct *vma)
{
	struct drm_mb_pdata *pdata = buf->priv;

	return remap_pfn_range(vma, vma->vm_start, PFN_DOWN(pdata->base),
			vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

struct dma_buf_ops drm_mb_ops = {
	.map_dma_buf = drm_mb_map,
	.unmap_dma_buf = drm_mb_unmap,
	.release = drm_mb_release,
	.map_atomic = drm_mb_kmap_atomic,
	.unmap_atomic = drm_mb_kunmap_atomic,
	.map = drm_mb_kmap,
	.unmap = drm_mb_kunmap,
	.mmap = drm_mb_mmap,
};

/**
 * drm_mb_export - export a memblock reserved area as a dma-buf
 *
 * @base: base physical address
 * @size: memblock size
 * @flags: mode flags for the dma-buf's file
 *
 * @base and @size must be page-aligned.
 *
 * Returns a dma-buf on success or ERR_PTR(-errno) on failure.
 */
struct dma_buf *drm_mb_export(phys_addr_t base, size_t size, int flags)
{
	struct drm_mb_pdata *pdata;
	struct dma_buf *buf;

	if (PAGE_ALIGN(base) != base || PAGE_ALIGN(size) != size)
		return ERR_PTR(-EINVAL);

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->base = base;

	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	exp_info.ops = &drm_mb_ops;
	exp_info.size = size;
	exp_info.flags = flags;
	exp_info.priv = pdata;

	buf = dma_buf_export(&exp_info);
	if (IS_ERR(buf))
		kfree(pdata);

	return buf;
}
EXPORT_SYMBOL(drm_mb_export);


