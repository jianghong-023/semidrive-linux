
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

/*
 * I2C status value
 */
typedef enum {
	I2C_STAT_NOTINIT     = 0,	/* not initilized by remote cpu */
	I2C_STAT_INITING     = 1,	/* on initilizing */
	I2C_STAT_INITED      = 2,	/* initilize compilete, ready for i2c */
	I2C_STAT_BOOTING     = 3,	/* during boot time splash screen */
	I2C_STAT_CLOSING     = 4,	/* I2C is going to close */
	I2C_STAT_CLOSED      = 5,	/* I2C is closed safely */
	I2C_STAT_NORMAL      = 6,	/* I2C is used by linux */
	I2C_STAT_MAX         = I2C_STAT_NORMAL,
} i2c_state_t;

#define SD_CLUSTER_EPT	(70)
#define SD_IVI_EPT	(71)
#define SD_SSYSTEM_EPT	(72)
#define SD_EARLYAPP_EPT	(80)

#define SYS_RPC_REQ_BASE			(0x2000)
#define SYS_RPC_REQ_SET_PROPERTY	(SYS_RPC_REQ_BASE + 0)
#define SYS_RPC_REQ_GET_PROPERTY	(SYS_RPC_REQ_BASE + 1)
/* Here defined backlight RPC cmd */
#define MOD_RPC_REQ_BASE			(0x3000)
#define MOD_RPC_REQ_SET_BRIGHT		(MOD_RPC_REQ_BASE + 8)
#define MOD_RPC_REQ_GET_BRIGHT		(MOD_RPC_REQ_BASE + 9)

int sd_close_dc(bool is_block);

bool sd_is_dc_closed(void);

int sd_wait_dc_init(unsigned int timeout_ms);

bool sd_is_dc_inited(void);

int sd_set_dc_status(dc_state_t val);

int sd_get_dc_status(dc_state_t *val);

int sd_kick_vdsp(void);

int sd_get_i2c_status(i2c_state_t *val);
int sd_set_i2c_status(i2c_state_t val);

typedef void(*vdsp_isr_callback)(void *ctx, void *mssg);
int sd_connect_vdsp(void *hwctx, vdsp_isr_callback isr_cb);

#endif //__IPCC_HEAD_FILE__
