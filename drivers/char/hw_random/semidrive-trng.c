/*
 * Copyright (C) 2020 semidrive.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/hw_random.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/random.h>


#define TRNG_CE2_VCE2_NUM 1

#define CE_JUMP                     0x1000

#define APB_CE2_BASE (0x34001000)
#define CE_SOC_BASE_ADDR APB_CE2_BASE
#define CRYPTOENGINE_APB_AB0_BASE_ADDR CE_SOC_BASE_ADDR

#define TRNG_CONTROL_TRNG_FIFOWRITESTARTUP_FIELD_OFFSET 20
#define TRNG_CONTROL_TRNG_FIFOWRITESTARTUP_FIELD_SIZE 1
#define TRNG_CONTROL_TRNG_NB128BITBLOCKS_FIELD_OFFSET 16
#define TRNG_CONTROL_TRNG_NB128BITBLOCKS_FIELD_SIZE 4
#define TRNG_CONTROL_TRNG_AIS31TESTSEL_FIELD_OFFSET 15
#define TRNG_CONTROL_TRNG_AIS31TESTSEL_FIELD_SIZE 1
#define TRNG_CONTROL_TRNG_HEALTHTESTSEL_FIELD_OFFSET 14
#define TRNG_CONTROL_TRNG_HEALTHTESTSEL_FIELD_SIZE 1
#define TRNG_CONTROL_TRNG_AIS31BYPASS_FIELD_OFFSET 13
#define TRNG_CONTROL_TRNG_AIS31BYPASS_FIELD_SIZE 1
#define TRNG_CONTROL_TRNG_HEALTHTESTBYPASS_FIELD_OFFSET 12
#define TRNG_CONTROL_TRNG_HEALTHTESTBYPASS_FIELD_SIZE 1
#define TRNG_CONTROL_TRNG_FORCERUN_FIELD_OFFSET 11
#define TRNG_CONTROL_TRNG_FORCERUN_FIELD_SIZE 1
#define TRNG_CONTROL_TRNG_INTENALM_FIELD_OFFSET 10
#define TRNG_CONTROL_TRNG_INTENALM_FIELD_SIZE 1
#define TRNG_CONTROL_TRNG_INTENPRE_FIELD_OFFSET 9
#define TRNG_CONTROL_TRNG_INTENPRE_FIELD_SIZE 1
#define TRNG_CONTROL_TRNG_SOFTRESET_FIELD_OFFSET 8
#define TRNG_CONTROL_TRNG_SOFTRESET_FIELD_SIZE 1
#define TRNG_CONTROL_TRNG_INTENFULL_FIELD_OFFSET 7
#define TRNG_CONTROL_TRNG_INTENFULL_FIELD_SIZE 1
#define TRNG_CONTROL_TRNG_INTENPROP_FIELD_OFFSET 5
#define TRNG_CONTROL_TRNG_INTENPROP_FIELD_SIZE 1
#define TRNG_CONTROL_TRNG_INTENREP_FIELD_OFFSET 4
#define TRNG_CONTROL_TRNG_INTENREP_FIELD_SIZE 1
#define TRNG_CONTROL_TRNG_CONDBYPASS_FIELD_OFFSET 3
#define TRNG_CONTROL_TRNG_CONDBYPASS_FIELD_SIZE 1
#define TRNG_CONTROL_TRNG_TESTEN_FIELD_OFFSET 2
#define TRNG_CONTROL_TRNG_TESTEN_FIELD_SIZE 1
#define TRNG_CONTROL_TRNG_LFSREN_FIELD_OFFSET 1
#define TRNG_CONTROL_TRNG_LFSREN_FIELD_SIZE 1
#define TRNG_CONTROL_TRNG_ENABLE_FIELD_OFFSET 0
#define TRNG_CONTROL_TRNG_ENABLE_FIELD_SIZE 1

#define REG_CE_TRNG_CONTROL_OFFSET     (0x8100<<0)
#define REG_AP_APB_CRYPTOENGINE_TRNG_CONTROL (CRYPTOENGINE_APB_AB0_BASE_ADDR + (0x8100<<0))
#define REG_CE_TRNG_CONTROL                 (REG_AP_APB_CRYPTOENGINE_TRNG_CONTROL)
#define TRNG_FIFOWRITESTARTUP_SHIFT         TRNG_CONTROL_TRNG_FIFOWRITESTARTUP_FIELD_OFFSET
#define TRNG_FIFOWRITESTARTUP_MASK          1UL << TRNG_FIFOWRITESTARTUP_SHIFT
#define TRNG_NB128BITBLOCKS_SHIFT           TRNG_CONTROL_TRNG_NB128BITBLOCKS_FIELD_OFFSET
#define TRNG_NB128BITBLOCKS_MASK            0xF << TRNG_NB128BITBLOCKS_SHIFT
#define TRNG_AIS31TESTSEL_SHIFT             TRNG_CONTROL_TRNG_AIS31TESTSEL_FIELD_OFFSET
#define TRNG_AIS31TESTSEL_MASK              1UL << TRNG_AIS31TESTSEL_SHIFT
#define TRNG_HEALTHTESTSEL_SHIFT            TRNG_CONTROL_TRNG_HEALTHTESTSEL_FIELD_OFFSET
#define TRNG_HEALTHTESTSEL_MASK             1UL << TRNG_HEALTHTESTSEL_SHIFT
#define TRNG_AIS31BYPASS_SHIFT              TRNG_CONTROL_TRNG_AIS31BYPASS_FIELD_OFFSET
#define TRNG_AIS31BYPASS_MASK               1UL << TRNG_AIS31BYPASS_SHIFT
#define TRNG_HEALTHTESTBYPASS_SHIFT         TRNG_CONTROL_TRNG_HEALTHTESTBYPASS_FIELD_OFFSET
#define TRNG_HEALTHTESTBYPASS_MASK          1UL << TRNG_HEALTHTESTBYPASS_SHIFT
#define TRNG_FORCERUN_SHIFT                 TRNG_CONTROL_TRNG_FORCERUN_FIELD_OFFSET
#define TRNG_FORCERUN_MASK                  1UL << TRNG_FORCERUN_SHIFT
#define TRNG_INTENALM_SHIFT                 TRNG_CONTROL_TRNG_INTENALM_FIELD_OFFSET
#define TRNG_INTENALM_MASK                  1UL << TRNG_INTENALM_SHIFT
#define TRNG_INTENPRE_SHIFT                 TRNG_CONTROL_TRNG_INTENPRE_FIELD_OFFSET
#define TRNG_INTENPRE_MASK                  1UL << TRNG_INTENPRE_SHIFT
#define TRNG_SOFTRESET_SHIFT                TRNG_CONTROL_TRNG_SOFTRESET_FIELD_OFFSET
#define TRNG_SOFTRESET_MASK                 1UL << TRNG_SOFTRESET_SHIFT
#define TRNG_INTENFULL_SHIFT                TRNG_CONTROL_TRNG_INTENFULL_FIELD_OFFSET
#define TRNG_INTENFULL_MASK                 1UL << TRNG_INTENFULL_SHIFT
#define TRNG_INTENPROP_SHIFT                TRNG_CONTROL_TRNG_INTENPROP_FIELD_OFFSET
#define TRNG_INTENPROP_MASK                 1UL << TRNG_INTENPROP_SHIFT
#define TRNG_INTENREP_SHIFT                 TRNG_CONTROL_TRNG_INTENREP_FIELD_OFFSET
#define TRNG_INTENREP_MASK                  1UL << TRNG_INTENREP_SHIFT
#define TRNG_CONDBYPASS_SHIFT               TRNG_CONTROL_TRNG_CONDBYPASS_FIELD_OFFSET
#define TRNG_CONDBYPASS_MASK                1UL << TRNG_CONDBYPASS_SHIFT
#define TRNG_TESTEN_SHIFT                   TRNG_CONTROL_TRNG_TESTEN_FIELD_OFFSET
#define TRNG_TESTEN_MASK                    1UL << TRNG_TESTEN_SHIFT
#define TRNG_LFSREN_SHIFT                   TRNG_CONTROL_TRNG_LFSREN_FIELD_OFFSET
#define TRNG_LFSREN_MASK                    1UL << TRNG_LFSREN_SHIFT
#define TRNG_ENABLE_SHIFT                   TRNG_CONTROL_TRNG_ENABLE_FIELD_OFFSET
#define TRNG_ENABLE_MASK                    1UL << TRNG_ENABLE_SHIFT

#define REG_CE_TRNG_STATUS_OFFSET (0x8130<<0)
#define REG_AP_APB_CRYPTOENGINE_TRNG_STATUS (CRYPTOENGINE_APB_AB0_BASE_ADDR + (0x8130<<0))
#define TRNG_STATUS_TRNG_FIFOACCFAIL_FIELD_OFFSET 11
#define TRNG_STATUS_TRNG_FIFOACCFAIL_FIELD_SIZE 1
#define TRNG_STATUS_TRNG_STARTUPFAIL_FIELD_OFFSET 10
#define TRNG_STATUS_TRNG_STARTUPFAIL_FIELD_SIZE 1
#define TRNG_STATUS_TRNG_ALMINT_FIELD_OFFSET 9
#define TRNG_STATUS_TRNG_ALMINT_FIELD_SIZE 1
#define TRNG_STATUS_TRNG_PREINT_FIELD_OFFSET 8
#define TRNG_STATUS_TRNG_PREINT_FIELD_SIZE 1
#define TRNG_STATUS_TRNG_FULLINT_FIELD_OFFSET 7
#define TRNG_STATUS_TRNG_FULLINT_FIELD_SIZE 1
#define TRNG_STATUS_TRNG_PROPFAIL_FIELD_OFFSET 5
#define TRNG_STATUS_TRNG_PROPFAIL_FIELD_SIZE 1
#define TRNG_STATUS_TRNG_REPFAIL_FIELD_OFFSET 4
#define TRNG_STATUS_TRNG_REPFAIL_FIELD_SIZE 1
#define TRNG_STATUS_TRNG_STATE_FIELD_OFFSET 1
#define TRNG_STATUS_TRNG_STATE_FIELD_SIZE 3
#define TRNG_STATUS_TRNG_TESTDATABUSY_FIELD_OFFSET 0
#define TRNG_STATUS_TRNG_TESTDATABUSY_FIELD_SIZE 1

#define REG_CE_TRNG_STATUS                  (REG_AP_APB_CRYPTOENGINE_TRNG_STATUS)
#define TRNG_FIFOACCFAIL_SHIFT              TRNG_STATUS_TRNG_FIFOACCFAIL_FIELD_OFFSET
#define TRNG_FIFOACCFAIL_MASK               1UL << TRNG_FIFOACCFAIL_SHIFT
#define TRNG_STARTUPFAIL_SHIFT              TRNG_STATUS_TRNG_STARTUPFAIL_FIELD_OFFSET
#define TRNG_STARTUPFAIL_MASK               1UL << TRNG_STARTUPFAIL_SHIFT
#define TRNG_ALMINT_SHIFT                   TRNG_STATUS_TRNG_ALMINT_FIELD_OFFSET
#define TRNG_ALMINT_MASK                    1UL << TRNG_ALMINT_SHIFT
#define TRNG_PREINT_SHIFT                   TRNG_STATUS_TRNG_PREINT_FIELD_OFFSET
#define TRNG_PREINT_MASK                    1UL << TRNG_PREINT_SHIFT
#define TRNG_FULLINT_SHIFT                  TRNG_STATUS_TRNG_FULLINT_FIELD_OFFSET
#define TRNG_FULLINT_MASK                   1UL << TRNG_FULLINT_SHIFT
#define TRNG_PROPFAIL_SHIFT                 TRNG_STATUS_TRNG_PROPFAIL_FIELD_OFFSET
#define TRNG_PROPFAIL_MASK                  1UL << TRNG_PROPFAIL_SHIFT
#define TRNG_REPFAIL_SHIFT                  TRNG_STATUS_TRNG_REPFAIL_FIELD_OFFSET
#define TRNG_REPFAIL_MASK                   1UL << TRNG_REPFAIL_SHIFT
#define TRNG_STATE_SHIFT                    TRNG_STATUS_TRNG_STATE_FIELD_OFFSET
#define TRNG_STATE_MASK                     7UL << TRNG_STATE_SHIFT
#define TRNG_TESTDATABUSY_SHIFT             TRNG_STATUS_TRNG_TESTDATABUSY_FIELD_OFFSET
#define TRNG_TESTDATABUSY_MASK              1UL << TRNG_TESTDATABUSY_SHIFT

#define REG_AP_APB_CRYPTOENGINE_CE0_TRNG_STATUS_OFFSET (0x100<<0)
#define REG_AP_APB_CRYPTOENGINE_CE0_TRNG_NUM0_OFFSET (0x104<<0)

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_KEY0REG
// Register Offset : 0x8110
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_KEY0REG_OFFSET (0x8110<<0)

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_KEY1REG
// Register Offset : 0x8114
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_KEY1REG_OFFSET (0x8114<<0)

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_KEY2REG
// Register Offset : 0x8118
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_KEY2REG_OFFSET (0x8118<<0)

//--------------------------------------------------------------------------
// Register Name   : REG_AP_APB_CRYPTOENGINE_TRNG_KEY3REG
// Register Offset : 0x811c
// Description     :
//--------------------------------------------------------------------------
#define REG_CE_TRNG_KEY3REG_OFFSET  (0x811c<<0)

#define REG_CE_TRNG_SWOFFTMRVAL_OFFSET  (0x8140<<0)
#define REG_CE_TRNG_CLKDIV_OFFSET   (0x8144<<0)
#define REG_CE_TRNG_INITWAITVAL_OFFSET (0x8134<<0)
#define REG_CE_TRNG_CTRL_OFFSET (0x8030<<0)

#define CRYPTOLIB_SUCCESS 0
#define CRYPTOLIB_CRYPTO_ERR 6

//RNG settings
#define RNG_CLKDIV            (0)
#define RNG_OFF_TIMER_VAL     (0)
#define RNG_FIFO_WAKEUP_LVL   (8)
#define RNG_INIT_WAIT_VAL     (512)
#define RNG_NB_128BIT_BLOCKS  (4)

#define RUN_WITH_NON_INNER_STATUS 1 // trng init run with non inner status

#define CE_TRNG_FIFO_SIZE_VALUE  32

#define to_semidrive_rng(p)	container_of(p, struct semidrive_rng_data, rng)

typedef enum control_fsm_state {
    FSM_STATE_RESET = 0,
    FSM_STATE_STARTUP,
    FSM_STATE_IDLE_ON,
    FSM_STATE_IDLE_OFF,
    FSM_STATE_FILL_FIFO,
    FSM_STATE_ERROR
} control_fsm_state_t;

struct semidrive_rng_data {
	void __iomem *base;
	struct hwrng rng;
};

static bool rng_startup_failed = false;
static bool rng_needs_startup_chk = false;

static uint32_t reg_value(uint32_t val, uint32_t src, uint32_t shift, uint32_t mask)
{
  return (src & ~mask) | ((val << shift) & mask);
}

void trng_set_startup_chk_flag(void)
{
    rng_needs_startup_chk = true;
}

void trng_soft_reset(void __iomem *ce_ctrl_base)
{
    uint32_t read_value, value;

    pr_info("trng_soft_reset enter ");

    read_value = readl(ce_ctrl_base);
    value = reg_value(1, read_value, TRNG_SOFTRESET_SHIFT, TRNG_SOFTRESET_MASK);
    writel(value, ce_ctrl_base);

    value = reg_value(0, read_value, TRNG_SOFTRESET_SHIFT, TRNG_SOFTRESET_MASK);
    writel(value, ce_ctrl_base);

    trng_set_startup_chk_flag();
}

uint32_t trng_status_glb(void __iomem *ce_base, uint32_t bit_mask)
{
    return readl(ce_base + REG_CE_TRNG_STATUS_OFFSET) & bit_mask;
}

/**
* @brief Poll for the end of the BA431 startup tests and check result.
* @return CRYPTOLIB_SUCCESS if successful CRYPTOLIB_CRYPTO_ERR otherwise
*/
static uint32_t trng_wait_startup(void __iomem *ce_base)
{
    control_fsm_state_t fsm_state;

    /*Wait until startup is finished*/
    do {
        fsm_state = trng_status_glb(ce_base, TRNG_STATE_MASK);
    }
    while ((fsm_state == FSM_STATE_RESET) || (fsm_state == FSM_STATE_STARTUP));

    /*Check startup test result*/
    if (trng_status_glb(ce_base, TRNG_STARTUPFAIL_MASK)) {
        return CRYPTOLIB_CRYPTO_ERR;
    }

    return CRYPTOLIB_SUCCESS;
}

void trng_wait_ready(void __iomem *ce_base, uint32_t vce_id)
{
    int i = 0;
    uint32_t fsm_state;

    do {
        fsm_state = readl((ce_base + REG_AP_APB_CRYPTOENGINE_CE0_TRNG_STATUS_OFFSET) + CE_JUMP * vce_id) & 0x1;
        i++;

        if (0 == i % 50) {
            return;
        }
    }
    while (!fsm_state);
}

uint32_t trng_init(void __iomem *ce_base)
{
    uint32_t status = 0;
    uint32_t key[4];
    uint32_t value;
    void __iomem *ce_ctrl_base;

    pr_info("trng_init enter ce_base=0x%x",ce_base);

    /*Soft reset*/
    ce_ctrl_base = ce_base + REG_CE_TRNG_CONTROL_OFFSET;

    trng_soft_reset(ce_ctrl_base);

    if (RNG_OFF_TIMER_VAL < 0) {
        value = readl(ce_ctrl_base);
        value = reg_value(1, value, TRNG_FORCERUN_SHIFT, TRNG_FORCERUN_MASK);
        writel(value, ce_ctrl_base);
    }
    else {
        writel(RNG_OFF_TIMER_VAL, ce_base + REG_CE_TRNG_SWOFFTMRVAL_OFFSET);
    }
    /*REG_CE_TRNG_CLKDIV*/
    writel(RNG_CLKDIV, ce_base + REG_CE_TRNG_CLKDIV_OFFSET);
    /*REG_CE_TRNG_INITWAITVAL*/
    writel(RNG_INIT_WAIT_VAL, ce_base + REG_CE_TRNG_INITWAITVAL_OFFSET);

    /*Enable NDRNG*/
    value = readl(ce_ctrl_base);
    value = reg_value(RNG_NB_128BIT_BLOCKS, value, TRNG_NB128BITBLOCKS_SHIFT, TRNG_NB128BITBLOCKS_MASK);
    value = reg_value(1, value, TRNG_ENABLE_SHIFT, TRNG_ENABLE_MASK);
    writel(value, ce_ctrl_base);

    /*Check startup tests*/
    status = trng_wait_startup(ce_base);
    if (status) {
        rng_startup_failed = true;
        return CRYPTOLIB_CRYPTO_ERR;
    }

    /*Program random key for the conditioning function*/
    /*REG_CE_TRNG_KEY0REG 0x8110 REG_CE_TRNG_KEY1RE 0x8114 GREG_CE_TRNG_KEY2REG REG_CE_TRNG_KEY3REG*/

    key[0] = readl(ce_ctrl_base + 0x80);
    key[1] = readl(ce_ctrl_base + 0x84);
    key[2] = readl(ce_ctrl_base + 0x88);
    key[3] = readl(ce_ctrl_base + 0x8c);
    writel(key[0], ce_base+REG_CE_TRNG_KEY0REG_OFFSET);
    writel(key[1], ce_base+REG_CE_TRNG_KEY1REG_OFFSET);
    writel(key[2], ce_base+REG_CE_TRNG_KEY2REG_OFFSET);
    writel(key[3], ce_base+REG_CE_TRNG_KEY3REG_OFFSET);

    /*Soft reset to flush FIFO*/
    trng_soft_reset(ce_ctrl_base);

    /*Enable interrupts for health tests (repetition and adaptive proportion tests) & AIS31 test failures */
    //ba431_enable_health_test_irq();
    value = readl(ce_ctrl_base);
    value = reg_value(1, value, TRNG_ENABLE_SHIFT, TRNG_ENABLE_MASK);
    value = reg_value(0, value, TRNG_LFSREN_SHIFT, TRNG_LFSREN_MASK);
    value = reg_value(0, value, TRNG_TESTEN_SHIFT, TRNG_TESTEN_MASK);
    value = reg_value(0, value, TRNG_CONDBYPASS_SHIFT, TRNG_CONDBYPASS_MASK);
    value = reg_value(0, value, TRNG_INTENREP_SHIFT, TRNG_INTENREP_MASK);
    value = reg_value(1, value, TRNG_INTENPROP_SHIFT, TRNG_INTENPROP_MASK);
    value = reg_value(1, value, TRNG_INTENFULL_SHIFT, TRNG_INTENFULL_MASK);
    value = reg_value(1, value, TRNG_INTENPRE_SHIFT, TRNG_INTENPRE_MASK);
    value = reg_value(1, value, TRNG_INTENALM_SHIFT, TRNG_INTENALM_MASK);
    value = reg_value(0, value, TRNG_FORCERUN_SHIFT, TRNG_FORCERUN_MASK);
    value = reg_value(0, value, TRNG_HEALTHTESTBYPASS_SHIFT, TRNG_HEALTHTESTBYPASS_MASK);
    value = reg_value(0, value, TRNG_AIS31BYPASS_SHIFT, TRNG_AIS31BYPASS_MASK);
    value = reg_value(0, value, TRNG_HEALTHTESTSEL_SHIFT, TRNG_HEALTHTESTSEL_MASK);
    value = reg_value(0, value, TRNG_AIS31TESTSEL_SHIFT, TRNG_AIS31TESTSEL_MASK);
    value = reg_value(0x4, value, TRNG_NB128BITBLOCKS_SHIFT, TRNG_NB128BITBLOCKS_MASK);
    value = reg_value(1, value, TRNG_FIFOWRITESTARTUP_SHIFT, TRNG_FIFOWRITESTARTUP_MASK);
    writel(value, ce_ctrl_base);

    return CRYPTOLIB_SUCCESS;
}

bool trng_startup_failed(void)
{
    return rng_startup_failed;
}

uint32_t rng_get_trng(void __iomem *ce_base, uint32_t vce_id, uint8_t* dst, uint32_t size)
{
    uint32_t ret = 0;
    uint32_t rng_value;
    uint32_t i;

    pr_info("rng_get_trng enter size=%d", size);

    if (size % 4) {
        return ret;
    }

    if (rng_needs_startup_chk) {
        trng_wait_startup(ce_base);
        rng_needs_startup_chk = false;
    }

    trng_wait_ready(ce_base, vce_id);

    for (i = 0; i < size;) {
        if (i % 8 == 0) {
            trng_wait_ready(ce_base, vce_id);
            pr_info("reg%x value = %x.\n", REG_CE_TRNG_CONTROL,readl(ce_base + REG_CE_TRNG_CONTROL_OFFSET));
            pr_info("reg(%x) value = %x.\n",REG_CE_TRNG_STATUS, readl(ce_base + REG_CE_TRNG_STATUS_OFFSET));
            if (readl(ce_base + REG_CE_TRNG_STATUS_OFFSET) & 0x100) {
                writel(readl(ce_base + REG_CE_TRNG_STATUS_OFFSET) & 0xFFFFFEFF, ce_base + REG_CE_TRNG_STATUS_OFFSET);
            }
            pr_info("after readl(%x) value = %x.\n",REG_CE_TRNG_STATUS, readl(ce_base + REG_CE_TRNG_STATUS_OFFSET));
			writel(0x1, ce_base + REG_CE_TRNG_CTRL_OFFSET);
        }

        rng_value = readl(((ce_base + REG_AP_APB_CRYPTOENGINE_CE0_TRNG_NUM0_OFFSET) + CE_JUMP * vce_id) + (i % 8));
		pr_info("trng_get_rand rng_value=%x.\n", rng_value);
        memcpy(dst + i, (void*)(&rng_value), 4);
        i = i + 4;
    }

    ret = i;
    return ret;
}

uint32_t trng_get_rand_by_fifo(void __iomem *ce_base, uint8_t * dst, uint32_t size)
{
    uint32_t rng_value;
    unsigned long baddr;
    uint32_t status;
    uint32_t i,j;
    uint32_t size_time;
    uint32_t size_left;

    if (!dst || !size) {
        return 0;
    }

    if (size % 4) {
        return 0;
    }

    status = readl(ce_base+REG_CE_TRNG_STATUS_OFFSET);

    while (0x0 != (status & 0x300)) {
        pr_info("trng_get_rand_by_fifo status=0x%x.\n", status);
        trng_soft_reset(ce_base + REG_CE_TRNG_CONTROL_OFFSET);
        status = readl(ce_base+REG_CE_TRNG_STATUS_OFFSET);
    }
    size_time = size/CE_TRNG_FIFO_SIZE_VALUE;
    size_left = size%CE_TRNG_FIFO_SIZE_VALUE;

    pr_info("trng_get_rand_by_fifo time=0x%x, left=0x%x.\n", size_time,size_left);

    if (0x0 != (status & 0x80)) {

        baddr = ce_base + REG_CE_TRNG_CONTROL_OFFSET + 0x80;

        for (i = 0; i < size_time; i++) {
            for (j = 0; j < CE_TRNG_FIFO_SIZE_VALUE; ) {
                rng_value = readl(baddr+j*4);
                pr_info("trng_get_rand_by_fifo rng_value=%x.\n", rng_value);
                memcpy(dst+i*CE_TRNG_FIFO_SIZE_VALUE+j*4, (void*)(&rng_value), 4);
                j = j+4;
            }

        }
        if(size_left > 0){
            for (j = 0; j < size_left; ) {
                rng_value = readl(baddr+j*4);
                pr_info("trng_get_rand_by_fifo rng_value=%x.\n", rng_value);
                memcpy(dst+size_time*CE_TRNG_FIFO_SIZE_VALUE+j*4, (void*)(&rng_value), 4);
                j = j+4;
            }
        }
        return size;
    }

    pr_info("trng_get_rand_by_fifo fifo is null staus 0x%x.\n", status);

    return 0;
}

static int semidrive_rng_init(struct hwrng *rng)
{
	struct semidrive_rng_data *priv = to_semidrive_rng(rng);
	pr_info("semidrive_rng_init enter ");
	trng_init(priv->base);
	return 0;
}

static void semidrive_rng_cleanup(struct hwrng *rng)
{
    struct semidrive_rng_data *priv = to_semidrive_rng(rng);
    pr_info("semidrive_rng_cleanup enter ");
	trng_soft_reset(priv->base+REG_CE_TRNG_CONTROL_OFFSET);
}

static int semidrive_rng_read(struct hwrng *rng, void *buf, size_t max, bool wait)
{
    int ret = 0;
    struct semidrive_rng_data *priv = to_semidrive_rng(rng);
	pr_info("semidrive_rng_read enter size =0x%x",max);

	//ret = rng_get_trng(priv->base, TRNG_CE2_VCE2_NUM, buf, max);
    ret = trng_get_rand_by_fifo(priv->base, buf, max);

	pr_info("semidrive_rng_read out ");
	return ret;
}

static int semidrive_rng_probe(struct platform_device *pdev)
{
	struct semidrive_rng_data *rng;
	struct resource *res;
	int ret;

	pr_info("semidrive_rng_probe enter ");

	rng = devm_kzalloc(&pdev->dev, sizeof(*rng), GFP_KERNEL);
	if (!rng)
		return -ENOMEM;

	platform_set_drvdata(pdev, rng);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rng->base = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(rng->base))
		return PTR_ERR(rng->base);

	rng->rng.name = pdev->name;
	rng->rng.init = semidrive_rng_init;
	rng->rng.cleanup = semidrive_rng_cleanup;
	rng->rng.read = semidrive_rng_read;
	rng->rng.quality = 1000;

	ret = devm_hwrng_register(&pdev->dev, &rng->rng);
	if (ret) {
		dev_err(&pdev->dev, "failed to register hwrng\n");
		return ret;
	}

	return 0;
}

static const struct of_device_id semidrive_rng_match[] = {
	{ .compatible = "semidrive,silex-rng" },
	{ }
};

MODULE_DEVICE_TABLE(of, semidrive_rng_match);

static struct platform_driver semidrive_rng_driver = {
	.probe		= semidrive_rng_probe,
	.driver		= {
		.name	= "semidrive-rng",
		.of_match_table = of_match_ptr(semidrive_rng_match),
	},
};

module_platform_driver(semidrive_rng_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("semidrive <semidrive@semidrive.com>");
MODULE_DESCRIPTION("semidrive random number generator driver");
