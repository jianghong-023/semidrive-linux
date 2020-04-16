/*
 * Copyright (c) 2019  Semidrive
 */

#define MB_MSG_PROTO_DFT        (0)
/* this msg is used for ROM code */
#define MB_MSG_PROTO_ROM        (1)
/* this msg is for rpmsg virtio */
#define MB_MSG_PROTO_RPMSG      (2)

/* used in mbox dts cell */
#define MB_DST_ADDR(uid)		((uid >> 8) & 0xff)
#define MB_RPROC_ID(uid )		(uid & 0xf)

typedef enum {
    IPCC_ADDR_INVALID           = 0x0,
    IPCC_ADDR_MB_TEST           = 0x05,
    IPCC_ADDR_VDSP_ANN          = 0x07,
    IPCC_ADDR_ECHO_TEST         = 0x08,
    IPCC_ADDR_RPMSG             = 0x10,
    IPCC_ADDR_RPMSG_TEST        = 0x20,
    IPCC_ADDR_DCF_BASE          = 0x30,
    IPCC_ADDR_MBOX_RAW          = 0x80,
    IPCC_ADDR_VDEV_BASE         = 0xb0,
    IPCC_ADDR_LEGACY            = 0xfe,
    IPCC_ADDR_MAX               = 0xff,
} sd_ipcc_addr_t;

typedef struct {
	u16 protocol: 4;
	u16 rproc   : 3;		/* processor id 0 ~ 7 */
	u16 priority: 1;
	u16 addr: 8;			/* receiver client address */
	u16 dat_len;
	u8 data[0];
} __attribute__((packed)) sd_msghdr_t;

#define MB_MSG_HDR_SZ		sizeof(sd_msghdr_t)

#define MB_MSG_INIT_RPMSG_HEAD(m, rproc, size, dest)	\
	mb_msg_init_head(m, rproc, MB_MSG_PROTO_RPMSG, true, size, dest)

inline static void mb_msg_init_head(sd_msghdr_t *msg, int rproc,
        int proto, bool priority, u8 size, u8 dest)
{
	msg->rproc = rproc;
	msg->protocol = proto;
	msg->priority = priority;
	msg->dat_len = size;	/* this size has included msghead */
	msg->addr = dest;
}

inline static u32 mb_msg_parse_packet_len(void *data)
{
	sd_msghdr_t *msg = (sd_msghdr_t *)data;

	return msg->dat_len;
}

inline static bool mb_msg_parse_packet_prio(void *data)
{
	sd_msghdr_t *msg = (sd_msghdr_t *)data;
	return msg->priority;
}

inline static int mb_msg_parse_packet_proto(void *data)
{
	sd_msghdr_t *msg = (sd_msghdr_t *)data;

	return msg->protocol;
}

inline static u8 *mb_msg_payload_ptr(void *data)
{
	sd_msghdr_t *msg = (sd_msghdr_t *)data;

	return msg->data;
}

inline static u32 mb_msg_payload_size(void *data)
{
	sd_msghdr_t *msg = (sd_msghdr_t *)data;

	return msg->dat_len - MB_MSG_HDR_SZ;
}

