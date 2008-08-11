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

struct mon_api_msg* mon_api_msg_new(u_int8_t type, u_int8_t action, u_int16_t length, u_int32_t ucid, u_int32_t seqnum, u_int32_t options, void* body);
void mon_api_msg_free(struct mon_api_msg* msg);
struct mon_api_msg* mon_api_msg_read(int fd);
int mon_api_msg_write(int fd, struct mon_api_msg* msg);

struct ospf_apiserver;

unsigned short mon_apiserver_getport (void);
int mon_apiserver_init (void);
void mon_apiserver_term (void);
struct mon_apiserver *mon_apiserver_new (int fd_sync);
void mon_apiserver_free (struct mon_apiserver *apiserv);
int mon_apiserver_serv_sock_family (unsigned short port, int family);
int mon_apiserver_accept (struct thread *thread);
int mon_apiserver_read (struct thread *thread);
int mon_apiserver_handle_msg (struct mon_apiserver *apiserv, struct mon_api_msg *msg);
int mon_apiserver_sync_write (struct thread *thread);
int mon_apiserver_send_reply (struct mon_apiserver *apiserv, u_int32_t seqnr, struct _MON_Reply_Para* reply);

#endif
