/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/
#include <sram_conf.h>
#include <ce.h>
#include <ce_reg.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>

#define CE_MEM_NODE_ITEM_NUM_MAX 6

#define CE_MEM_NODE_DIG_MAX 64
#define CE_MEM_NODE_HASH_PAD_MAX 256
#define CE_MEM_NODE_RSA_MAX 512
#define CE_MEM_NODE_RSA_MAX_EXT 516

/** Helper to build a ::block_t from a pair of data address and size */

#define LOCAL_TRACE 0 //close local trace 1->0 

uint8_t __attribute__((aligned(CACHE_LINE))) sx_hash_64[ROUNDUP(CE_MEM_NODE_DIG_MAX, CACHE_LINE)];
uint8_t __attribute__((aligned(CACHE_LINE))) sx_hash_padding_256[ROUNDUP(CE_MEM_NODE_HASH_PAD_MAX, CACHE_LINE)];
uint8_t __attribute__((aligned(CACHE_LINE))) sx_msg_padding_512[ROUNDUP(CE_MEM_NODE_RSA_MAX, CACHE_LINE)];
uint8_t __attribute__((aligned(CACHE_LINE))) sx_rev_cpy_buf_512[ROUNDUP(CE_MEM_NODE_RSA_MAX, CACHE_LINE)];
uint8_t __attribute__((aligned(CACHE_LINE))) sx_dbMask_512[ROUNDUP(CE_MEM_NODE_RSA_MAX, CACHE_LINE)];
uint8_t __attribute__((aligned(CACHE_LINE))) sx_msg_padding_ext_516[ROUNDUP(CE_MEM_NODE_RSA_MAX_EXT, CACHE_LINE)];

struct ce_heap {
    size_t item_num;
    size_t used_num;
    size_t used_num_max;
    struct mem_node mem_node_list[CE_MEM_NODE_ITEM_NUM_MAX];
};
static DEFINE_SPINLOCK(ce_heap_lock);
// ce_heap static vars
static struct ce_heap inheap = {
    .item_num = CE_MEM_NODE_ITEM_NUM_MAX,
    .used_num = 0,
    .used_num_max = 0,
    .mem_node_list[0].size = CE_MEM_NODE_DIG_MAX,
    .mem_node_list[0].is_used = 0,
    .mem_node_list[0].ptr = sx_hash_64,
    .mem_node_list[1].size = CE_MEM_NODE_HASH_PAD_MAX,
    .mem_node_list[1].is_used = 0,
    .mem_node_list[1].ptr = sx_hash_padding_256,
    .mem_node_list[2].size = CE_MEM_NODE_RSA_MAX,
    .mem_node_list[2].is_used = 0,
    .mem_node_list[2].ptr = sx_msg_padding_512,
    .mem_node_list[3].size = CE_MEM_NODE_RSA_MAX,
    .mem_node_list[3].is_used = 0,
    .mem_node_list[3].ptr = sx_dbMask_512,
    .mem_node_list[4].size = CE_MEM_NODE_RSA_MAX,
    .mem_node_list[4].is_used = 0,
    .mem_node_list[4].ptr = sx_rev_cpy_buf_512,
    .mem_node_list[5].size = CE_MEM_NODE_RSA_MAX_EXT,
    .mem_node_list[5].is_used = 0,
    .mem_node_list[5].ptr = sx_msg_padding_ext_516,
};

//event_t g_ce_signal[VCE_COUNT];
uint8_t g_ce_int_arg[VCE_COUNT];
//event_t g_trng_signal;
bool g_ce_inited = false;
volatile uint32_t g_int_flag = 0;

#define APB_SYS_CNT_RO_BASE (0x31400000)

#define RECODE_COUNT_NUM_MAX 256
typedef struct cout_info_ce
{
    uint32_t cout_value;
    uint32_t cout_num;
}cout_info_ce_t;

typedef struct recode_info_ce
{
    cout_info_ce_t cout_info[RECODE_COUNT_NUM_MAX];
    uint32_t r_index;
}recode_info_ce_t;

recode_info_ce_t recode_info_ce_a = {0};

void __iomem *sys_cnt_base_ce = NULL;

void of_set_sys_cnt_ce(int i){
	
    if(recode_info_ce_a.r_index == RECODE_COUNT_NUM_MAX){
        return;
    }
	if(sys_cnt_base_ce == NULL){
		sys_cnt_base_ce = ioremap(APB_SYS_CNT_RO_BASE, 0x1000);
	}
    recode_info_ce_a.cout_info[recode_info_ce_a.r_index].cout_value = (readl(sys_cnt_base_ce))/3;
    recode_info_ce_a.cout_info[recode_info_ce_a.r_index].cout_num = i;
    recode_info_ce_a.r_index++;
}

uint32_t of_get_sys_cnt_ce(void){
	uint32_t ret;

	if(sys_cnt_base_ce == NULL){
		sys_cnt_base_ce = ioremap(APB_SYS_CNT_RO_BASE, 0x1000);
	}
    ret = (readl(sys_cnt_base_ce))/3;
    return ret;
}

struct mem_node * ce_malloc(size_t size){
    uint8_t i = 0;
    spin_lock_saved_state_t states;
    struct mem_node * return_node = NULL;

    spin_lock_irqsave(&ce_heap_lock, states);

    for(i = 0; i < inheap.item_num; i++){
        if((inheap.mem_node_list[i].size >= size)&&(inheap.mem_node_list[i].is_used == 0)){
            inheap.mem_node_list[i].is_used = 1;
            inheap.used_num++;
            if(inheap.used_num > inheap.used_num_max){
                inheap.used_num_max = inheap.used_num;
            }
            return_node = &(inheap.mem_node_list[i]);
            break;
        }
    }
    spin_unlock_irqrestore(&ce_heap_lock, states);
    return return_node;
    //malloc();
}

void ce_free(struct mem_node * mem_node){
    spin_lock_saved_state_t states;

    spin_lock_irqsave(&ce_heap_lock, states);
    if((mem_node != NULL)&&(mem_node->is_used == 1)){
        mem_node->is_used = 0;
        inheap.used_num--;
    }
    spin_unlock_irqrestore(&ce_heap_lock, states);
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

    if (g_ce_inited) return 0;

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

void printf_binary(const char* info, const void* content, uint32_t content_len)
{
    pr_debug("%s: \n", info);
    uint32_t i = 0;

    pr_debug("%p ", content);
    for (; i < content_len; i++) {
        pr_debug("0x%02x ", *((uint8_t*)(content) + i));

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

void clean_cache_block(block_t* data, uint32_t ce_id)
{
    if((data == NULL)||(data->len == 0)){
        return;
    }

    switch(data->addr_type){
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

void invalidate_cache_block(block_t* data, uint32_t ce_id)
{
    if((data == NULL)||(data->len == 0)){
        return;
    }

    switch(data->addr_type){
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
