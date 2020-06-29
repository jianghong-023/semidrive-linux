
/*
 * Copyright (c) 2019  Semidrive
 */

#ifndef __IPCC_HEAD_FILE__
#define __IPCC_HEAD_FILE__

#define RPMSG_RPC_MAX_PARAMS     (8)
#define RPMSG_RPC_MAX_RESULT     (4)

struct rpc_req_msg {
	u32 cmd;
	u32 cksum;
	u32 param[RPMSG_RPC_MAX_PARAMS];
};

struct rpc_ret_msg {
	u32 ack;
	u32 retcode;
	u32 result[RPMSG_RPC_MAX_RESULT];
};

/*
 * Display DC status value
 */
typedef enum {
	DC_STAT_NOTINIT     = 0,	/* not initilized by remote cpu */
	DC_STAT_INITING     = 1,	/* on initilizing */
	DC_STAT_INITED      = 2,	/* initilize compilete, ready for display */
	DC_STAT_BOOTING     = 3,	/* during boot time splash screen */
	DC_STAT_CLOSING     = 4,	/* DC is going to close */
	DC_STAT_CLOSED      = 5,	/* DC is closed safely */
	DC_STAT_NORMAL      = 6,	/* DC is used by DRM */
	DC_STAT_MAX         = DC_STAT_NORMAL,
} dc_state_t;

int sd_close_dc(bool is_block);

bool sd_is_dc_closed(void);

int sd_wait_dc_init(unsigned int timeout_ms);

bool sd_is_dc_inited(void);

int sd_set_dc_status(dc_state_t val);

int sd_get_dc_status(dc_state_t *val);

int sd_kick_vdsp(void);

typedef void(*vdsp_isr_callback)(void *ctx, void *mssg);
int sd_connect_vdsp(void *hwctx, vdsp_isr_callback isr_cb);

#endif //__IPCC_HEAD_FILE__