/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include "sram_conf.h"
#include "ce.h"
#include "ce_reg.h"

#define CE_HEAP_MEM_NODE_ITEM_NUM_MAX    	32
#define CE_HEAP_SIZE_OF_PAGE             	4096
#define CE_HEAP_MALLOC_MAX_SIZE      		512
#define CE_HEAP_CAPACITY_OF_PAGE         	(CE_HEAP_SIZE_OF_PAGE/CE_HEAP_MALLOC_MAX_SIZE)
#define CE_HEAP_PAGE_NUM                 	(CE_HEAP_MEM_NODE_ITEM_NUM_MAX/CE_HEAP_CAPACITY_OF_PAGE)

/** Helper to build a ::block_t from a pair of data address and size */

struct ce_heap {
	size_t item_num;
	size_t ce_capacity_of_page;
	size_t ce_page_num;
	bool is_init;
	uint8_t *ce_pageaddr[CE_HEAP_PAGE_NUM];
	struct mem_node mem_node_list[CE_HEAP_MEM_NODE_ITEM_NUM_MAX];
};

uint32_t enter_ce_heap_init_time = 0;
static DEFINE_SPINLOCK(ce_heap_lock);
static struct ce_heap inheap;

uint32_t ce_inheap_init(void)
{
	inheap.item_num = CE_HEAP_MEM_NODE_ITEM_NUM_MAX;
	inheap.ce_capacity_of_page = CE_HEAP_CAPACITY_OF_PAGE;
	inheap.ce_page_num = CE_HEAP_PAGE_NUM;
	inheap.is_init = 1;
	uint8_t i, j;
	enter_ce_heap_init_time++;
	pr_info(" enter ce_inheap_init \n");

	for (i = 0; i < inheap.ce_page_num ; i++) {
		inheap.ce_pageaddr[i] = (uint8_t *)get_zeroed_page(GFP_DMA);

		if (inheap.ce_pageaddr[i] == NULL) {
			pr_err("get_zeroed_page is wrong in %s, %s,", __func__, __LINE__);

			for (j = 0; j < i ; j++) {
				free_pages((unsigned long)inheap.ce_pageaddr[j], 0);
				inheap.ce_pageaddr[i] = NULL;
			}
			inheap.is_init = 0;
			return 1;
		}
	}

	for (i = 0; i < inheap.item_num; i++) {
		inheap.mem_node_list[i].size = CE_HEAP_MALLOC_MAX_SIZE;
		inheap.mem_node_list[i].is_used =  0xff;
		inheap.mem_node_list[i].ptr = NULL;
	}

	return 0;
}

struct mem_node *ce_malloc(size_t size)
{
	uint8_t i = 0;
	uint32_t status = 0;
	spin_lock_saved_state_t states;

	if (unlikely(size > CE_HEAP_MALLOC_MAX_SIZE)) {
		pr_err("apply space is too big\n");
		return NULL;
	}

	struct mem_node *return_node = NULL;

	if (unlikely(inheap.is_init == 0)) {
		status = ce_inheap_init();

		if (status) {
			pr_err("%s, %d, status is %d\n", __func__, __LINE__, status);
			return status;
		}
	}

	spin_lock_irqsave(&ce_heap_lock, states);

	for (i = 0; i < inheap.item_num; i++) {
		if (inheap.mem_node_list[i].is_used == 0xff) {
			inheap.mem_node_list[i].is_used = i;
			inheap.mem_node_list[i].ptr = inheap.ce_pageaddr[i / inheap.ce_capacity_of_page]
									+ CE_HEAP_MALLOC_MAX_SIZE * (i % inheap.ce_capacity_of_page);
			return_node = &(inheap.mem_node_list[i]);
			//pr_info("%d is be used\n",i);
			break;
		}
	}

	if (unlikely(i  ==  inheap.item_num)) {
		pr_err("one page is lack\n");
		spin_unlock_irqrestore(&ce_heap_lock, states);
		return NULL;
	}

	memset(return_node->ptr, 0, CE_HEAP_MALLOC_MAX_SIZE);
	spin_unlock_irqrestore(&ce_heap_lock, states);
	return return_node;
	//malloc();
}

void ce_free(struct mem_node *mem_node)
{
	spin_lock_saved_state_t states;
	uint8_t i;
	spin_lock_irqsave(&ce_heap_lock, states);

	if (likely((mem_node != NULL) && (mem_node->is_used != 0xff))) {
		//pr_info("%d is be free\n",mem_node->is_used);
		mem_node->is_used = 0xff;
		mem_node->ptr = NULL;
	}

	spin_unlock_irqrestore(&ce_heap_lock, states);
}

void ce_inheap_free(void)
{
	uint8_t i;
	pr_info("enter_ce_heap_init_time is %d\n", enter_ce_heap_init_time);
	enter_ce_heap_init_time = 0;

	if (inheap.is_init == 1) {
		inheap.is_init = 0;

		for (i = 0; i < inheap.ce_page_num ; i++) {
			free_pages((unsigned long)inheap.ce_pageaddr[i], 0);
			inheap.ce_pageaddr[i] = NULL;
		}

		pr_info("page is be free\n");
	}
}

uint8_t g_ce_int_arg[VCE_COUNT];
bool g_ce_inited = false;
volatile uint32_t g_int_flag = 0;

#define APB_SYS_CNT_RO_BASE (0x31400000)

#define RECODE_COUNT_NUM_MAX 256
typedef struct cout_info_ce {
	uint32_t cout_value;
	uint32_t cout_num;
} cout_info_ce_t;

typedef struct recode_info_ce {
	cout_info_ce_t cout_info[RECODE_COUNT_NUM_MAX];
	uint32_t r_index;
} recode_info_ce_t;

recode_info_ce_t recode_info_ce_a = {0};

void __iomem *sys_cnt_base_ce = NULL;

void of_set_sys_cnt_ce(int i)
{

	if (recode_info_ce_a.r_index == RECODE_COUNT_NUM_MAX) {
		return;
	}

	if (sys_cnt_base_ce == NULL) {
		sys_cnt_base_ce = ioremap(APB_SYS_CNT_RO_BASE, 0x1000);
	}

	recode_info_ce_a.cout_info[recode_info_ce_a.r_index].cout_value = (readl(sys_cnt_base_ce)) / 3;
	recode_info_ce_a.cout_info[recode_info_ce_a.r_index].cout_num = i;
	recode_info_ce_a.r_index++;
}

uint32_t of_get_sys_cnt_ce(void)
{
	uint32_t ret;

	if (sys_cnt_base_ce == NULL) {
		sys_cnt_base_ce = ioremap(APB_SYS_CNT_RO_BASE, 0x1000);
	}

	ret = (readl(sys_cnt_base_ce)) / 3;
	return ret;
}



addr_t addr_switch_to_ce(uint32_t vce_id, ce_addr_type_t addr_type_s, addr_t addr)
{
	addr_t addr_switch = addr;

	if (EXT_MEM == addr_type_s) {
		addr_switch = addr;
	}
	else if (SRAM_PUB == addr_type_s || SRAM_SEC == addr_type_s) {
		addr_switch = get_sram_base(vce_id);
		addr_switch = ~addr_switch & addr;
	}

	return addr_switch;
}

int32_t ce_init(uint32_t vce_id)
{
	uint32_t int_base;

	return 0;
}

int32_t ce_globle_init(void)
{

	if (g_ce_inited) {
		return 0;
	}

	pr_debug("ce_globle_init enter\n");

	g_ce_inited = true;

	return 0;
}

uint32_t switch_addr_type(ce_addr_type_t addr_type_s)
{
	uint32_t addr_type_d;

	switch (addr_type_s) {
		case SRAM_PUB:
			addr_type_d = 0x1;
			break;

		case SRAM_SEC:
			addr_type_d = 0x3;
			break;

		case KEY_INT:
			addr_type_d = 0x4;
			break;

		case EXT_MEM:
			addr_type_d = 0x0;
			break;

		case PKE_INTERNAL:
			addr_type_d = 0x2;
			break;

		default:
			addr_type_d = 0x0;
	}

	return addr_type_d;
}

void printf_binary(const char *info, const void *content, uint32_t content_len)
{
	pr_debug("%s: \n", info);
	uint32_t i = 0;

	pr_debug("%p ", content);

	for (; i < content_len; i++) {
		pr_debug("0x%02x ", *((uint8_t *)(content) + i));

		if (0 != i && 0 == (i + 1) % 16) {
			pr_debug("\n");
		}
	}

	if (!(i % 16)) {
		pr_debug("\n");
	}
}

uint32_t reg_value(uint32_t val, uint32_t src, uint32_t shift, uint32_t mask)
{
	return (src & ~mask) | ((val << shift) & mask);
}

void clean_cache_block(block_t *data, uint32_t ce_id)
{
	if ((data == NULL) || (data->len == 0)) {
		return;
	}

	switch (data->addr_type) {
		case EXT_MEM:
			//dma_cache_sync(g_sm2.dev, (addr_t)data->addr, data->len, DMA_TO_DEVICE);
			__flush_dcache_area((addr_t)data->addr, data->len);
			//dma_sync_single_for_cpu(g_sm2.dev, __pa((addr_t)data->addr), data->len, DMA_TO_DEVICE);
			break;

		case SRAM_PUB:
			//dma_sync_single_for_cpu(g_sm2.dev, (addr_t)data->addr+sram_addr(data->addr_type, ce_id), data->len, DMA_TO_DEVICE);
			break;

		default:
			return;
	}
}

void invalidate_cache_block(block_t *data, uint32_t ce_id)
{
	if ((data == NULL) || (data->len == 0)) {
		return;
	}

	switch (data->addr_type) {
		case EXT_MEM:
			__inval_dcache_area((addr_t)data->addr, data->len);
			//dma_sync_single_for_device(g_sm2.dev, __pa((addr_t)data->addr), data->len, DMA_TO_DEVICE);
			break;

		case SRAM_PUB:
			//dma_sync_single_for_device(g_sm2.dev, (addr_t)data->addr+sram_addr(data->addr_type, ce_id), data->len, DMA_TO_DEVICE);
			break;

		default:
			return;
	}
}

void clean_cache(addr_t start, uint32_t size)
{
	// arch_clean_cache_range(start, size);
}

void flush_cache(addr_t start, uint32_t size)
{
	__inval_dcache_area(start, size);
	//dma_sync_single_for_device(g_sm2.dev, __pa(start), size, DMA_TO_DEVICE);
}
