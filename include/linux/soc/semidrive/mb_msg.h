/*
 * Copyright (c) 2018  Semidrive
 */

#define MB_MSG_PROTO_DFT        (0)
/* this msg is used for ROM code */
#define MB_MSG_PROTO_ROM        (1)
/* this msg is for rpmsg virtio */
#define MB_MSG_PROTO_RPMSG      (2)

typedef struct {
	u16 protocol: 4;
	u16 rproc   : 3;
	u16 priority: 1;
	u16 reserved: 8;
	u16 dat_len;
	u8 data[0];
} __attribute__((packed)) sd_msghdr_t;

#define MB_MSG_HDR_SZ       sizeof(sd_msghdr_t)

#define MB_MSG_INIT_RPMSG_HEAD(m, rproc, size)	\
	mb_msg_init_head(m, rproc, MB_MSG_PROTO_RPMSG, true, size)

inline static void mb_msg_init_head(sd_msghdr_t *msg, int rproc,
		int proto, bool priority, u8 size)
{
	msg->rproc = rproc;
	msg->protocol = proto;
	msg->priority = priority;
	msg->dat_len = size;
	msg->reserved = 0xbd;
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

