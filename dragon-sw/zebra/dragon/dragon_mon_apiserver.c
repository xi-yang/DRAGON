/****************************************************************************

DRAGON Monitoring API Server source file dragon_mon_apiserver.h
Created by Xi Yang @ 08/11/2008
To be incorporated into GNU Zebra  - DRAGON extension

****************************************************************************/
#include <zebra.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "thread.h"
#include "linklist.h"
#include "memory.h"
#include "buffer.h"
#include "sockunion.h"
#include "network.h"
#include "log.h"
#include "dragond.h"
#include "dragon_mon_apiserver.h"

int MON_APISERVER_PORT = 2616;
extern struct dragon_master dmaster;


/* -----------------------------------------------------------
 * Functions for monitoring API messages
 * -----------------------------------------------------------
 */

struct mon_api_msg* mon_api_msg_new(u_int8_t type, u_int8_t action, u_int16_t length, u_int32_t ucid, u_int32_t seqnum, u_int32_t options, void* body)
{
  struct mon_api_msg *msg = XMALLOC(MTYPE_TMP, sizeof(struct mon_api_msg));
  msg->header.type = type;
  msg->header.action = action;
  msg->header.length = htons(length);
  msg->header.ucid = htonl(ucid);
  msg->header.seqnum = htonl(seqnum);
  msg->header.options = htonl(options);
  msg->header.chksum = MON_API_MSG_CHKSUM(msg->header);
  msg->body = XMALLOC(MTYPE_TMP, length);
  memcpy(msg->body, body, length);
  return msg;
}

void mon_api_msg_free(struct mon_api_msg* msg)
{
  assert (msg);
  if (msg->body)
    XFREE(MTYPE_TMP, msg->body);
  XFREE(MTYPE_TMP, msg);
}

struct mon_api_msg * mon_api_msg_read (int fd)
{
  struct mon_api_msg *msg;
  struct mon_api_msg_header hdr;
  char buf[DRAGON_MAX_PACKET_SIZE];
  int bodylen;
  int rlen;

  /* Read message header */
  rlen = readn (fd, (char *) &hdr, sizeof (struct mon_api_msg_header));

  if (rlen < 0)
    {
      zlog_warn ("mon_msg_read: readn %s", strerror (errno));
      return NULL;
    }
  else if (rlen == 0)
    {
      zlog_warn ("msg_read: Connection closed by peer");
      return NULL;
    }
  else if (rlen != sizeof (struct mon_api_msg_header))
    {
      zlog_warn ("msg_read: Cannot read message header!");
      return NULL;
    }

  /* Checksum verifycation */
  if (hdr.chksum != MON_API_MSG_CHKSUM(hdr))
    {
      zlog_warn ("mon_msg_read: MON_API_MSG_CHKSUM verifycation failed");
      return NULL;
    }

  /* Determine body length. */
  bodylen = ntohs (hdr.length);
  if (bodylen > 0)
    {

      /* Read message body */
      rlen = readn (fd, buf, bodylen);
      if (rlen < 0)
	{
	  zlog_warn ("mon_msg_read: readn %s", strerror (errno));
	  return NULL;
	}
      else if (rlen == 0)
	{
	  zlog_warn ("mon_msg_read: Connection closed by peer");
	  return NULL;
	}
      else if (rlen != bodylen)
	{
	  zlog_warn ("mon_msg_read: Cannot read message body!");
	  return NULL;
	}
    }

  /* Allocate new message */
  msg = mon_api_msg_new (hdr.type, hdr.action, ntohs(hdr.length), ntohl(hdr.ucid), ntohl(hdr.seqnum), ntohl(hdr.options), buf);

  return msg;
}

int mon_api_msg_write (int fd, struct mon_api_msg *msg)
{
  u_char buf[DRAGON_MAX_PACKET_SIZE];
  int l;
  int wlen;

  assert (msg);

  /* Length of message including header */
  l = sizeof (struct mon_api_msg_header) + ntohs (msg->header.length);

  /* Make contiguous memory buffer for message */
  memcpy (buf, &msg->header, sizeof (struct mon_api_msg_header));
  memcpy(buf, msg->body, ntohs (msg->header.length));
  wlen = writen (fd, (char*)buf, l);
  if (wlen < 0)
    {
      zlog_warn ("msg_write: writen %s", strerror (errno));
      return -1;
    }
  else if (wlen == 0)
    {
      zlog_warn ("msg_write: Connection closed by peer");
      return -1;
    }
  else if (wlen != l)
    {
      zlog_warn ("msg_write: Cannot write API message");
      return -1;
    }
  return 0;
}

/* -----------------------------------------------------------
 * Monitoring API server functions
 * -----------------------------------------------------------
 */

unsigned short mon_apiserver_getport (void)
{
  struct servent *sp = getservbyname ("dragon_monapi", "tcp");
  return sp ? ntohs (sp->s_port) : MON_APISERVER_PORT;
}

int mon_apiserver_serv_sock_family (unsigned short port, int family)
{
  union sockunion su;
  int accept_sock;
  int rc;

  memset (&su, 0, sizeof (union sockunion));
  su.sa.sa_family = family;

  /* Make new socket */
  accept_sock = sockunion_stream_socket (&su);
  if (accept_sock < 0)
    return accept_sock;

  /* This is a server, so reuse address and port */
  sockopt_reuseaddr (accept_sock);
  sockopt_reuseport (accept_sock);

  /* Bind socket to address and given port. */
  rc = sockunion_bind (accept_sock, &su, port, NULL);
  if (rc < 0)
    {
      close (accept_sock);	/* Close socket */
      return rc;
    }

  /* Listen socket under queue length 3. */
  rc = listen (accept_sock, 3);
  if (rc < 0)
    {
      zlog_warn ("mon_apiserver_serv_sock_family: listen: %s", strerror (errno));
      close (accept_sock);	/* Close socket */
      return rc;
    }
  return accept_sock;
}

int mon_apiserver_init (void)
{
  int fd;
  int rc = -1;

  /* Create new socket for synchronous messages. */
  fd = mon_apiserver_serv_sock_family (MON_APISERVER_PORT, AF_INET);

  if (fd < 0)
    goto out;

  /* Schedule new thread that handles accepted connections. */
  thread_add_read (master, mon_apiserver_accept, NULL, fd);

  /* Initialize list that keeps track of all connections. */
  dmaster.mon_apiserver_list = list_new ();

  rc = 0;

out:
  return rc;
}

void mon_apiserver_term (void)
{
  listnode node;

  /* Free all client instances */
  for (node = listhead (dmaster.mon_apiserver_list); node; nextnode (node))
    {
      struct mon_apiserver *apiserv =
	(struct mon_apiserver *) getdata (node);
      mon_apiserver_free (apiserv);
    }

  /* Free client list itself */
  list_delete (dmaster.mon_apiserver_list);
  dmaster.mon_apiserver_list = NULL;
}

struct mon_apiserver *mon_apiserver_new (int fd)
{
  struct mon_apiserver *apiserv = XMALLOC (MTYPE_TMP, sizeof (struct mon_apiserver));

  memset(apiserv, 0, sizeof(struct mon_apiserver));
  apiserv->fd_sync = fd;
  apiserv->out_fifo = list_new ();
  apiserv->t_sync_read = NULL;
  apiserv->t_sync_write = NULL;

  return apiserv;
}

void fifo_free (list fifo)
{
  listnode node;
  struct msg* msg;

  LIST_LOOP(fifo, msg, node)
    {
      XFREE(MTYPE_TMP, msg);
    }

  list_delete(fifo);
}

void mon_apiserver_free (struct mon_apiserver *apiserv)
{
  /* Cancel read and write threads. */
  if (apiserv->t_sync_read)
    {
      thread_cancel (apiserv->t_sync_read);
    }
  if (apiserv->t_sync_write)
    {
      thread_cancel (apiserv->t_sync_write);
    }

  /* Close connections to client. */
  if (apiserv->fd_sync > 0)
    {
      close (apiserv->fd_sync);
    }

  fifo_free (apiserv->out_fifo);

  /* Remove from the list of active clients. */
  listnode_delete (dmaster.mon_apiserver_list, apiserv);

  /* And free instance. */
  XFREE (MTYPE_TMP, apiserv);
}

int mon_apiserver_accept (struct thread *thread)
{
  int accept_sock;
  int new_sync_sock;
  union sockunion su;
  struct mon_apiserver *apiserv;
  struct sockaddr_in peer_sync;
  int peerlen;
  /* list funclist; */
  /* int registered; */
  int ret = 0;

  /* THREAD_ARG (thread) is NULL */
  accept_sock = THREAD_FD (thread);

  /* Keep hearing on socket for further connections. */
  thread_add_read (master, mon_apiserver_accept, apiserv, accept_sock);

  memset (&su, 0, sizeof (union sockunion));
  /* Accept connection for synchronous messages */
  new_sync_sock = sockunion_accept (accept_sock, &su);
  if (new_sync_sock < 0)
    {
      zlog_warn ("mon_apiserver_accept: accept: %s", strerror (errno));
      return -1;
    }

  /* Get port address and port number of peer to make reverse connection.
     The reverse channel uses the port number of the peer port+1. */

  memset(&peer_sync, 0, sizeof(struct sockaddr_in));
  peerlen = sizeof (struct sockaddr_in);

  ret = getpeername (new_sync_sock, (struct sockaddr *)&peer_sync, (socklen_t*)&peerlen);
  if (ret < 0)
    {
      zlog_warn ("mon_apiserver_accept: getpeername: %s", strerror (errno));
      close (new_sync_sock);
      return -1;
    }

  /* Allocate new server-side connection structure */
  apiserv = mon_apiserver_new (new_sync_sock);

  /* Add to active connection list */
  listnode_add (dmaster.mon_apiserver_list, apiserv);
  apiserv->peer_sync = peer_sync;

  /* And add read threads for new connection */
  /* Keep hearing on socket for further connections. */
  thread_add_read (master, mon_apiserver_read, apiserv, new_sync_sock);

  return 0;
}

int mon_apiserver_read (struct thread *thread)
{
  struct mon_apiserver *apiserv;
  struct mon_api_msg*msg;
  int fd;
  int rc = -1;

  apiserv = THREAD_ARG (thread);
  fd = THREAD_FD (thread);

  if (fd == apiserv->fd_sync)
    {
      apiserv->t_sync_read = NULL;
    }
  else
    {
      zlog_warn ("mon_apiserver_read: Unknown fd(%d)", fd);
      mon_apiserver_free (apiserv);
      goto out;
    }

  /* Read message from fd. */
  msg = mon_api_msg_read (fd);
  if (msg == NULL)
    {
      zlog_warn ("mon_apiserver_read: read failed on fd=%d, closing connection", fd);

      /* Perform cleanup. */
      mon_apiserver_free (apiserv);
      goto out;
    }

  /* Dispatch to corresponding message handler. */
  rc = mon_apiserver_handle_msg (apiserv, msg);

  /* Prepare for next message, add read thread. */
  thread_add_read (master, mon_apiserver_read, apiserv, fd);

  mon_api_msg_free (msg);

out:
  return rc;

}

int mon_apiserver_handle_msg (struct mon_apiserver *apiserv, struct mon_api_msg *msg)
{

}

int mon_apiserver_sync_write (struct thread *thread)
{

}

int mon_apiserver_send_reply (struct mon_apiserver *apiserv, u_int32_t seqnum, struct _MON_Reply_Para* reply)
{

}

