/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/
#include "sx_trng.h"
#include "sx_errors.h"

#define LOCAL_TRACE 0 //close local trace 1->0

#define RUN_WITH_NON_INNER_STATUS 1 // trng init run with non inner status

static bool rng_startup_failed = false;
static bool rng_needs_startup_chk = false;

void sx_trng_soft_reset(void)
{
    uint32_t read_value, value;

    read_value = readl(_ioaddr(REG_CE_TRNG_CONTROL));
    value = reg_value(1, read_value, TRNG_SOFTRESET_SHIFT, TRNG_SOFTRESET_MASK);
    //LTRACEF("trng_soft_reset val = 1 reg=%x, value=%x.\n", REG_CE_TRNG_CONTROL, value);
    writel(value, _ioaddr(REG_CE_TRNG_CONTROL));

    value = reg_value(0, read_value, TRNG_SOFTRESET_SHIFT, TRNG_SOFTRESET_MASK);
    //LTRACEF("trng_soft_reset val = 0 reg=%x, value=%x.\n", REG_CE_TRNG_CONTROL, value);
    writel(value, _ioaddr(REG_CE_TRNG_CONTROL));
}

uint32_t sx_trng_status_glb(uint32_t bit_mask)
{
    //LTRACEF("trng_status_glb REG_CE_TRNG_STATUS value=0x%x.\n", readl(_ioaddr(REG_CE_TRNG_STATUS)));
    return readl(_ioaddr(REG_CE_TRNG_STATUS)) & bit_mask;
}

/**
* @brief Verify the conditioning function of the BA431 against test patterns.
* @return CRYPTOLIB_SUCCESS if successful CRYPTOLIB_CRYPTO_ERR otherwise
*/
static uint32_t trng_conditioning_test(void)
{
    const uint32_t test_data[16] = {0xE1BCC06B, 0x9199452A, 0x1A7434E1, 0x25199E7F,
                                    0x578A2DAE, 0x9CAC031E, 0xAC6FB79E, 0x518EAF45,
                                    0x461CC830, 0x11E45CA3, 0x19C1FBE5, 0xEF520A1A,
                                    0x45249FF6, 0x179B4FDF, 0x7B412BAD, 0x10376CE6
                                   };
    const uint32_t known_answer[4] = {0xA1CAF13F, 0x09AC1F68, 0x30CA0E12, 0xA7E18675};
    const uint32_t test_key[4] = {0x16157E2B, 0xA6D2AE28, 0x8815F7AB, 0x3C4FCF09};

    uint32_t i;
    uint32_t error = 0;
    uint32_t read_value, value;

    /*Soft reset*/
    sx_trng_soft_reset();

    /*Enable test mode*/
    read_value = readl(_ioaddr(REG_CE_TRNG_CONTROL));
    value = reg_value(4, read_value, TRNG_NB128BITBLOCKS_SHIFT, TRNG_NB128BITBLOCKS_MASK);
    value = reg_value(1, value, TRNG_AIS31BYPASS_SHIFT, TRNG_AIS31BYPASS_MASK);
    value = reg_value(1, value, TRNG_HEALTHTESTBYPASS_SHIFT, TRNG_HEALTHTESTBYPASS_MASK);
    value = reg_value(1, value, TRNG_TESTEN_SHIFT, TRNG_TESTEN_MASK);
    writel(value, _ioaddr(REG_CE_TRNG_CONTROL));

    /*Write key*/
    writel(test_key[0], _ioaddr(REG_CE_TRNG_KEY0REG));
    writel(test_key[1], _ioaddr(REG_CE_TRNG_KEY1REG));
    writel(test_key[2], _ioaddr(REG_CE_TRNG_KEY2REG));
    writel(test_key[3], _ioaddr(REG_CE_TRNG_KEY3REG));

    /*Write test data input*/
    for (i = 0; i < sizeof(test_data) / 4; i++) {
        while (sx_trng_status_glb(TRNG_TESTDATABUSY_MASK));

        writel(test_data[i], _ioaddr(REG_CE_TRNG_TESTDATA));
    }

    //LTRACEF("write test data finish.\n");

    /*Wait for conditioning test to complete --> wait for return data to appear in FIFO*/
    while (readl(_ioaddr(REG_CE_TRNG_FIFOLEVEL)) < 4);

    /*Clear control register*/
    writel(0, _ioaddr(REG_CE_TRNG_CONTROL));

    /*Compare results to known answer*/
    for (i = 0; i < sizeof(known_answer) / 4; i++) {
        error |= readl(_ioaddr(REG_CE_TRNG_CONTROL + 0x80 + i * 0x4)) ^ known_answer[i];
    }

    if (error) {
        //LTRACEF("finish error: %d.\n", error);
        return CRYPTOLIB_CRYPTO_ERR;
    }

    return CRYPTOLIB_SUCCESS;
}

/**
* @brief Poll for the end of the BA431 startup tests and check result.
* @return CRYPTOLIB_SUCCESS if successful CRYPTOLIB_CRYPTO_ERR otherwise
*/
static uint32_t trng_wait_startup(void)
{
    control_fsm_state_t fsm_state;

    /*Wait until startup is finished*/
    do {
        fsm_state = sx_trng_status_glb(TRNG_STATE_MASK);
    }
    while ((fsm_state == FSM_STATE_RESET) || (fsm_state == FSM_STATE_STARTUP));

    /*Check startup test result*/
    if (sx_trng_status_glb(TRNG_STARTUPFAIL_MASK)) {
        return CRYPTOLIB_CRYPTO_ERR;
    }

    return CRYPTOLIB_SUCCESS;
}

void sx_trng_wait_ready(uint32_t vce_id)
{
    int i = 0;
    uint32_t fsm_state;

    do {
        //LTRACEF("readl(REG_TRNG_STATUS_CE_(vce_id)) value=0x%x.\n", readl(_ioaddr(REG_TRNG_STATUS_CE_(vce_id))));
        fsm_state = readl(_ioaddr(REG_TRNG_STATUS_CE_(vce_id))) & 0x1;
        i++;

        if (0 == i % 50) {
            //LTRACEF("check ready times: %d\n", i);
            return;
        }
    }
    while (!fsm_state);
}

void trng_get_rand(uint32_t vce_id, uint8_t* dst, uint32_t size)
{
    uint32_t rng_value;
    uint32_t i;

    //LTRACEF("trng_get_rand vce_id =%d, dst=%p, size=%d.\n", vce_id, dst, size);
    if (size % 4) {
        return;
    }

#if AIS31_ENABLED

    if (sx_trng_status_glb(BA431_STAT_MASK_PREALM_INT)) {
        CRYPTOLIB_PRINTF("Preliminary noise alarm detected.\n");
        trng_soft_reset();
    }

#endif
    //LTRACEF("trng_get_rand rng_needs_startup_chk=%d.\n", rng_needs_startup_chk);
    if (rng_needs_startup_chk) {
        trng_wait_startup();
        rng_needs_startup_chk = false;
    }

    sx_trng_wait_ready(vce_id);

    for (i = 0; i < size;) {
        //((uint32_t)(dst + i)) = readl(REG_TRNG_NUM0_CE_(vce_id) + (i % 8));
        rng_value = readl(_ioaddr(REG_TRNG_NUM0_CE_(vce_id)) + (i % 8));
        //LTRACEF("trng_get_rand rng_value=%x.\n", rng_value);
        memcpy(dst + i, (void*)(&rng_value), 4);
        i = i + 4;
    }
}



uint32_t trng_get_hrng(uint32_t vce_id)
{
    return readl(_ioaddr(REG_HRNG_NUM_CE_(vce_id)));
}

uint32_t rng_get_prng(uint32_t vce_id, uint8_t* dst, uint32_t size)
{
    uint32_t ret = 0;
    uint32_t rng_value;

    if (dst == NULL) {
        return ret;
    }

    rng_value = readl(_ioaddr(REG_HRNG_NUM_CE_(vce_id)));

    memcpy(dst, (void*)(&rng_value), 4);

    ret = 1;
    return ret;
}

uint32_t sx_rng_get_trng(void __iomem *ce_base, uint8_t* dst, uint32_t size)
{
	uint32_t ret = 0;
	uint32_t rng_value = 0;
	uint32_t i = 0;
	uint32_t index = 0;
	uint32_t index_max = 0;
	uint32_t cp_left = 0;
	unsigned long baddr;

	pr_info("sx_rng_get_trng enter dst=%p size=%d", dst, size);

	baddr = ce_base + (0x114<<0);

	index = 0;
	index_max = size>>2; //4 byte one cp
	cp_left = size&0x3;

	for (i = 0; i < index_max; i++) {
			rng_value = readl(baddr);
			pr_info("sx_rng_get_trng rng_value=%x.\n", rng_value);
			memcpy(dst+index, (void *)(&rng_value), sizeof(rng_value));
			index = index+4;
			ret = index;
	}

	if(cp_left > 0){
		rng_value = readl(baddr);
		pr_info("sx_rng_get_trng last %d rng_value=%x.\n", cp_left, rng_value);
		memcpy(dst+index, (void *)(&rng_value), cp_left);
		ret = index+cp_left;
	}

	return ret;
}

