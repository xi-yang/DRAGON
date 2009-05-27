/****************************************************************************

DRAGON Monitoring API Server source file dragon_mon_apiserver.h
Created by Xi Yang @ 08/11/2008
To be incorporated into GNU Zebra  - DRAGON extension

****************************************************************************/

#ifndef _DRAGON_MON_APISERVER_H
#define _DRAGON_MON_APISERVER_H

/* Server instance for each accepted client connection. */
struct mon_apiserver
{
  /* Socket for synchronous commands */
  int fd_sync;			
  struct sockaddr_in peer_sync;
  /* Fifo buffer for outgoing messages */
  list out_fifo;
  /* Identifier for the apiserver (using client UCID) */
  u_int32_t ucid;
  /* Read and write threads */
  struct thread *t_sync_read;
  struct thread *t_sync_write;
};


struct mon_api_msg_header
{
    u_int8_t type;
    u_int8_t action;
    u_int16_t length;
    u_int32_t ucid; /*  unique client id*/
    u_int32_t seqnum; /* sequence number, specified by requestor*/
    u_int32_t options; /* sequence number, specified by requestor*/
    u_int32_t chksum;   /* checksum for the above four 32-bit words*/
};

#define MON_API_MSG_CHKSUM(X) (((u_int32_t*)&X)[0] + ((u_int32_t*)&X)[1] + ((u_int32_t*)&X)[2] + ((u_int32_t*)&X)[3])

struct mon_api_msg
{
  struct mon_api_msg_header header;
  char* body;
};

struct _MON_LSP_Info
{
  struct in_addr source;
  struct in_addr destination;
  u_int16_t lsp_id;
  u_int16_t tunnel_id;
  u_int32_t status;
  u_int32_t time_sec;
};

struct _LSPService_Request
{
  struct in_addr source;
  u_int32_t src_lclid;
  struct in_addr destination;
  u_int32_t dest_lclid;
  u_int8_t  switching_type;
  u_int8_t  encoding_type;
  u_int16_t gpid;
  float bandwidth;
  u_int16_t vlan_tag;
  u_int16_t subnet_vtag_ingress;
  u_int16_t subnet_vtag_egress;
  u_int8_t reserved[2];
};

struct _PCE_Spec 
{
    char module_name[16];
    u_int32_t flags;
};

#define MON_API_MSGTYPE_LSPLIST		0x01 /*A list of LSP names */
#define MON_API_MSGTYPE_LSPINFO		0x02 /* LSP info at source VLSR */
#define MON_API_MSGTYPE_LSPERO		0x03 /* LSP ERO (and Subnet ERO if any) */
#define MON_API_MSGTYPE_NODELIST		0x04 /* Control plane (VLSR) node list */
#define MON_API_MSGTYPE_SWITCH 		0x10 /* Monitoring information for switch */
#define MON_API_MSGTYPE_CIRCUIT		0x20 /* Monitoring information for circuit */
#define MON_API_MSGTYPE_LSPPROV		0x30 /* LSP provisioning */

#define MON_API_ACTION_RTRV 	0x01 /* Information retrieval/query */
#define MON_API_ACTION_INSERT 	0x02 /* Insersion/Addition/Provisioning */
#define MON_API_ACTION_DELETE	0x03 /* Deleteon/Removal */
#define MON_API_ACTION_UPDATE	0x04 /* Uptate/Notification */
#define MON_API_ACTION_ACK 	0x05 /* Reply with acknowledgement */
#define MON_API_ACTION_DATA 	0x06 /* Reply/Ack with information data */
#define MON_API_ACTION_ERROR 	0x07 /* Reply with error code */

#define MON_TLV_GRI 			0x01
#define MON_TLV_SWITCH_INFO 	0x02
#define MON_TLV_CIRCUIT_INFO 	0x03
#define MON_TLV_NODE_LIST 		0x04
#define MON_TLV_LSP_INFO     	0x05
#define MON_TLV_LSP_ERO	 	0x06
#define MON_TLV_SUBNET_ERO	0x07
#define MON_TLV_IPv4_ADDR		0x08
#define MON_TLV_LSP_REQUEST	0x09
#define MON_TLV_PCE_SPEC		0x0a
#define MON_TLV_ERROR			0x0f

#define MON_SWITCH_OPTION_SUBNET 			0x0001
#define MON_SWITCH_OPTION_SUBNET_SRC 		0x0002
#define MON_SWITCH_OPTION_SUBNET_DEST		0x0004
#define MON_SWITCH_OPTION_SUBNET_SNC		0x0008
#define MON_SWITCH_OPTION_SUBNET_DTL		0x0010
#define MON_SWITCH_OPTION_SUBNET_TUNNEL	0x0020
#define MON_SWITCH_OPTION_CIRCUIT_SRC		0x1000
#define MON_SWITCH_OPTION_CIRCUIT_DEST		0x2000
#define MON_SWITCH_OPTION_SUBNET_TRANSIT	0x8000
#define MON_SWITCH_OPTION_ERROR 				0x10000

struct mon_api_msg* mon_api_msg_new(u_int8_t type, u_int8_t action, u_int16_t length, u_int32_t ucid, u_int32_t seqnum, u_int32_t options, void* body);
void mon_api_msg_free(struct mon_api_msg* msg);
struct mon_api_msg* mon_api_msg_read(int fd);
int mon_api_msg_write(int fd, struct mon_api_msg* msg);

#define MON_APISERVER_POST_MESSAGE(S, M) \
                listnode_add(S->out_fifo, M); \
                S->t_sync_write = thread_add_write (master, mon_apiserver_write, S, S->fd_sync);

unsigned short mon_apiserver_getport (void);
int mon_apiserver_init (void);
void mon_apiserver_term (void);
struct mon_apiserver *mon_apiserver_new (int fd_sync);
void mon_apiserver_free (struct mon_apiserver *apiserv);
int mon_apiserver_serv_sock_family (unsigned short port, int family);
int mon_apiserver_accept (struct thread *thread);
int mon_apiserver_read (struct thread *thread);
int mon_apiserver_handle_msg (struct mon_apiserver *apiserv, struct mon_api_msg *msg);
int mon_apiserver_write (struct thread *thread);
int mon_apiserver_send_reply (struct mon_apiserver *apiserv, u_int8_t type, u_int8_t action, struct _MON_Reply_Para* reply);
void mon_apiserver_send_ack(struct mon_apiserver* apiserv, u_int8_t type, u_int32_t seqnum);
void mon_apiserver_send_error(struct mon_apiserver* apiserv, u_int8_t type, u_int32_t seqnum, u_int32_t err_code);
struct lsp* dragon_find_lsp_by_griname(char* name);
int mon_apiserver_lsp_commit(char* lsp_gri, struct _LSPService_Request * lsp_req, int num_lsp_ero_nodes, struct _EROAbstractNode_Para* lsp_ero, 
    int num_subnet_ero_nodes, struct _EROAbstractNode_Para* subnet_ero, struct _PCE_Spec* pce_spec);
int mon_apiserver_lsp_delete(char* lsp_gri);

#endif
