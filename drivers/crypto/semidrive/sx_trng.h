/*
* Copyright (c) 2019 Semidrive Semiconductor.
* All rights reserved.
*
*/

#ifndef SX_TRNG_H
#define SX_TRNG_H

#include <ce_reg.h>
#include <ce.h>

typedef enum trng_test {
    TRNG_REP_TEST,      /**< Repetition count test. */
    TRNG_PROP_TEST,     /**< Adaptive proportion test. */
    TRNG_PREALM_TEST,   /**< AIS31 preliminary noise alarm */
    TRNG_ALM_TEST,      /**< AIS31 noise alarm. */
} trng_test_t;

typedef enum control_fsm_state {
    FSM_STATE_RESET = 0,
    FSM_STATE_STARTUP,
    FSM_STATE_IDLE_ON,
    FSM_STATE_IDLE_OFF,
    FSM_STATE_FILL_FIFO,
    FSM_STATE_ERROR
} control_fsm_state_t;

//RNG settings
#define RNG_CLKDIV            (0)
#define RNG_OFF_TIMER_VAL     (0)
#define RNG_FIFO_WAKEUP_LVL   (8)
#define RNG_INIT_WAIT_VAL     (512)
#define RNG_NB_128BIT_BLOCKS  (4)


/**
* @brief get random number from fifo
* CE1 FIFO depth: 16 words, CE2 FIFO depth: 32
* @param dst random number buffer
* @param size random number count
* @return CRYPTOLIB_SUCCESS if the TRNG was successfully configured for causing
*                          the test failure
*/
uint32_t sx_rng_get_trng(void __iomem *ce_base, uint8_t* dst, uint32_t size);

#endif /* SX_TRNG_H */
