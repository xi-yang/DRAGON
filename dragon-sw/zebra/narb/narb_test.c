/* Implementation of the NARB server  */

#include <zebra.h>
#include "prefix.h" 
#include "linklist.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_te.h"
#include "ospfd/ospf_opaque.h"
#include "ospfd/ospf_api.h"
#include "ospfd/ospf_dump.h" /* for ospf_lsa_header_dump */
#include "thread.h"
#include "stream.h"
#include "log.h"
#include "ospf_apiclient.h"
#include "narb_apiserver.h"

/* Local portnumber for async channel. Note that OSPF API library will also
   allocate a sync channel at ASYNCPORT+1. */
#define APP_NARB_PORT 2609
#define APP_CLIENT_PORT 2709

/* Master thread */
struct dragon_master *dmaster;
struct thread_master *master;

/* Global variables */
struct ospf_apiclient *oclient;

struct ospf_apiclient *oclient_inter;

struct ospf_apiclient *oclient_intra;
char **args;
char * narb_vty_addr = NULL;

int
app_handle_reply (struct thread *t)
{
  struct ospf_apiclient *cl;
  struct msg *msg;
  char addr[20];
  char *data;
  u_char hop_type;
  int i, count;
  struct msg_app2narb_request * msgbody;

  cl = THREAD_ARG (t);
  msg = msg_read(cl->fd_sync);

  if (!msg)
    {
      zlog_warn
      ("narb_test: read failed on fd=%d, closing connection", cl->fd_sync);

      exit(1);
    }

  data = STREAM_DATA(msg->s) + 4;
      
  switch (msg->hdr.msgtype)
    {
    case MSG_REPLY_ERO:
      count = (ntohs(msg->hdr.msglen) - 4) / 8;

      zlog_info("Request successful! ERO returned...");
      for (i = 0; i < count; i++)
        {
          hop_type = (*(data+8*i));
          hop_type >>=7;
          inet_ntop(AF_INET, data + 8*i +2, addr, 20);
          zlog_info("HOP-TYPE [%s]: %s", hop_type?"loose":"strict", addr);
        }
      sleep(2);
      /* Confirmation */

      msgbody = malloc(sizeof(struct msg_app2narb_request));
      msgbody->type = htons(TLV_TYPE_NARB_REQUEST);
      msgbody->length = htons(sizeof(struct msg_app2narb_request) - 4);
      inet_aton(args[2], &msgbody->src);
      inet_aton(args[3], &msgbody->dest);
      msgbody->encoding_type = 2;
      msgbody->switching_type = 51;
      msgbody->bandwidth = 500.0;
      msgbody->gpid = htons(8);
      msg = msg_new(MSG_APP_CONFIRM_EVENT, msgbody, ntohl(msg->hdr.msgseq), 
            sizeof(struct msg_app2narb_request));
      msg_write(cl->fd_sync, msg);

      sleep(3);
      msg = msg_new(MSG_APP_REMOVE_EVENT, msgbody, ntohl(msg->hdr.msgseq), 
            sizeof(struct msg_app2narb_request));
      msg_write(cl->fd_sync, msg);
      break;
    case MSG_REPLY_ERROR:
      {
        u_int16_t errcode = ntohs(*(u_int16_t *)data);
        zlog_info("Request process failed. Error code = %d.",  errcode);
      }
      break;
    case MSG_REPLY_REMOVE_CONFIRM:
      {
        zlog_info("NARB confirmation: The requested route has been removed..\n");
      }
      break;
    }

  thread_add_read(master, app_handle_reply, oclient, oclient->fd_sync);

  return 0;
}

int
app_request_inject (struct thread *t)
{
  struct ospf_apiclient *cl;
  struct msg_app2narb_request * msgbody;
  static int seqno = 1;
  float bandwidth = 500.0;
  struct msg *msg;

  msgbody = malloc(sizeof(struct msg_app2narb_request));

  msgbody->type = htons(TLV_TYPE_NARB_REQUEST);
  msgbody->length = htons(sizeof(struct msg_app2narb_request) - 4);
  inet_aton(args[2], &msgbody->src);
  inet_aton(args[3], &msgbody->dest);
  msgbody->bandwidth = bandwidth;
  msgbody->encoding_type = 2;
  msgbody->switching_type = 51;
  msgbody->gpid = htons(8);
  
  msg = msg_new(MSG_APP_REQUEST_EVENT, msgbody, seqno++, sizeof(struct msg_app2narb_request));
  
  cl = THREAD_ARG (t);

  msg_write(cl->fd_sync, msg);

  msg_free(msg);

  thread_add_read(master, app_handle_reply, oclient, oclient->fd_sync);
  
  return 0;
};

struct ospf_apiclient *
narb_apiclient_connect (char *host, int syncport)
{
  struct sockaddr_in myaddr_sync;
  struct hostent *hp;
  struct ospf_apiclient *new;
  int size = 0;
  int fd1;
  int ret;
  int on = 1;


  /* Make connection for synchronous requests and connect to server */
  /* Resolve address of server */
  hp = gethostbyname (host);
  if (!hp)
    {
      fprintf (stderr, "ospf_apiclient_connect: no such host %s\n", host);
      return NULL;
    }

  fd1 = socket (AF_INET, SOCK_STREAM, 0);
  if (fd1 < 0)
    {
      fprintf (stderr,
	       "ospf_apiclient_connect: creating sync socket failed\n");
      return NULL;
    }


  /* Reuse addr and port */
  ret = setsockopt (fd1, SOL_SOCKET,
                    SO_REUSEADDR, (void *) &on, sizeof (on));
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: SO_REUSEADDR failed\n");
      close (fd1);
      return NULL;
    }

#ifdef SO_REUSEPORT
  ret = setsockopt (fd1, SOL_SOCKET, SO_REUSEPORT,
                    (void *) &on, sizeof (on));
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: SO_REUSEPORT failed\n");
      close (fd1);
      return NULL;
    }
#endif /* SO_REUSEPORT */


  /* Bind sync socket to address structure. This is needed since we
     want the sync port number on a fixed port number. The reverse
     async channel will be at this port+1 */

  memset (&myaddr_sync, 0, sizeof (struct sockaddr_in));
  myaddr_sync.sin_family = AF_INET;
  myaddr_sync.sin_port = htons (APP_CLIENT_PORT);
#ifdef HAVE_SIN_LEN
  myaddr_sync.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_SIN_LEN */
  size = sizeof (struct sockaddr_in);

  ret = bind (fd1, (struct sockaddr *) &myaddr_sync, size);
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: bind sync socket failed\n");
      close (fd1);
      return NULL;
    }

  /* Prepare address structure for connect */
  memcpy (&myaddr_sync.sin_addr, hp->h_addr, hp->h_length);
  myaddr_sync.sin_family = AF_INET;
  myaddr_sync.sin_port = htons(syncport);
#ifdef HAVE_SIN_LEN
  myaddr_sync.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_SIN_LEN */

  /* Now establish synchronous channel with OSPF daemon */
  ret = connect (fd1, (struct sockaddr *) &myaddr_sync,
		 sizeof (struct sockaddr_in));
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: sync connect failed\n");
      close (fd1);
      return NULL;
    }

  /* Create new client-side instance */
  new = malloc (sizeof (struct ospf_apiclient));
  memset (new, 0, sizeof (struct ospf_apiclient));

  /* Initialize socket descriptors for sync and async channels */
  new->fd_sync = fd1;
  new->fd_async = -1;

  return new;
}

/*
int
narb_apiclient_close (struct ospf_apiclient *oclient)
{

  if (oclient->fd_sync >= 0)
    {
      close (oclient->fd_sync);
    }


  XFREE (MTYPE_OSPF_APICLIENT, oclient);
  return 0;
}
*/

int
main (int argc, char *argv[])
{
  struct thread thread;
  args = argv;

  if (argc != 4)
    {
      printf("Usage: %s narb-host src-ip dest-ip\n", args[0]);
      exit(1);
    }
  
/* alloc narbserv and init data structures for every field of (struct narb_server_type) */
  
  master = thread_master_create ();

  /* Open connection to OSPF daemon */
  oclient = narb_apiclient_connect (args[1], APP_NARB_PORT);
  if (!oclient)
    {
      printf ("Connecting to OSPF daemon on %s failed!\n",
	      args[1]);
      exit (1);
    }

  thread_add_write(master, app_request_inject, oclient, oclient->fd_sync);

  /* Run loop for the threads */
  while (1)
    {
      thread_fetch (master, &thread);
      thread_call (&thread);
    }

  /* Never reached */
  return 0;
}

