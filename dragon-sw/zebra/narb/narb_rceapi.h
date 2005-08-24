#ifndef __NARB_RCEAPI_H__
#define __NARB_RCEAPI_H__

#include <zebra.h>

#define API_MAX_MSG_SIZE 6000
extern char* RCE_HOST_ADDR;
extern int RCE_API_PORT;
extern int rce_api_sock;


struct rce_api_msg_header
{
    u_char msgtype;
    u_char action;
    u_int16_t msglen;
    u_int32_t ucid;
    u_int32_t msgseq;
    u_int32_t chksum;
    u_int32_t msgtag[2];
};

struct rce_api_msg
{
    struct rce_api_msg_header hdr;
    char* body;
};

/* TLV's wrapped in the LSP querry message, indicated
    in msgta[0]
*/
#define LSP_TLV_NARB_REQ    0x01

/* LSP computation options in msgtag[0] */
#define LSP_OPT_STRICT  ((u_int32_t)(0x01 << 16)) //otherwise LSP_OPT_LOOSE
#define LSP_OPT_PREFERRED ((u_int32_t)(0x02 << 16)) //otherwise LSP_OPT_ONLY
#define LSP_OPT_MRN 0x04 << 16  //otherwise SINGLE_REGION
#define LSP_OPT_BIDIRECTIONAL ((u_int32_t)(0x10 << 16)) //otherwise UNIDIRECTIONAL

/*combinations of options*/
#define LSP_OPT_LOOSE_ONLY 0
#define LSP_OPT_STRICT_ONLY LSP_OPT_STRICT
#define LSP_OPT_LOOSE_PREFERRED LSP_OPT_PREFERRED
#define LSP_OPT_STRICT_PREFERRED (LSP_OPT_STRICT | LSP_OPT_PREFERRED)

enum rce_api_msg_type
{
    MSG_LSP = 0x01,
    MSG_LSA = 0x02,
    MSG_AAA = 0x03,
    MSG_RM = 0x04,
    MSG_CTRL = 0x05
};

enum rce_api_action
{
    ACT_NOOP = 0x00,
    ACT_QUERY = 0x01,
    ACT_INSERT = 0x02,
    ACT_DELETE = 0x03,
    ACT_UPDATE = 0x04,
    ACT_ACK = 0x05,
    ACT_ACKDATA = 0x06,
    ACT_ERROR = 0x07
};


#define MSG_CHKSUM(X) (((u_int32_t*)&X)[0] + ((u_int32_t*)&X)[1] + ((u_int32_t*)&X)[2])

extern void rce_api_msg_delete (struct rce_api_msg* msg);

extern int narb_rceapi_connect (char *host, int port);

extern struct rce_api_msg * narb_rceapi_read (int fd);

extern int narb_rceapi_send (int fd, struct rce_api_msg *msg);

extern int narb_rceapi_msghandle (struct rce_api_msg*msg);

extern struct rce_api_msg * rce_api_msg_new (u_char msgtype, u_char action, void *msgbody, u_int32_t ucid, u_int32_t seqnum, u_int16_t msglen);

#endif
