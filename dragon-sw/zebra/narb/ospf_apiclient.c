/*************ospf_apiclient.c**********/
/* $$$$ hacked for being used by NARB*/
 
/*
 * Client side of OSPF API.
 * Copyright (C) 2001, 2002, 2003 Ralph Keller
 *
 * This file is part of GNU Zebra.
 * 
 * GNU Zebra is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

 #include <zebra.h>

#include <lib/version.h>
#include "getopt.h"
#include "thread.h"
#include "prefix.h"
#include "linklist.h"
#include "if.h"
#include "vector.h"
#include "vty.h"
#include "command.h"
#include "filter.h"
#include "stream.h"
#include "log.h"
#include "memory.h"
#include "table.h"

#include "ospfd/ospfd.h"

#include "ospfd/ospf_te.h"
#include "ospfd/ospf_te_lsa.h"
#include "ospfd/ospf_te_lsdb.h"

#include "ospfd/ospf_interface.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_opaque.h"
#include "ospfd/ospf_lsdb.h"
#include "ospfd/ospf_neighbor.h"
#include "ospfd/ospf_dump.h"
#include "ospfd/ospf_zebra.h"
#include "ospfd/ospf_api.h"

#include "ospf_apiclient.h"
#include "narb_apiserver.h"
#include "narb_summary.h"
#include "narb_rceapi.h"

/* Backlog for listen */
#define BACKLOG 5

extern int flag_holdon;
/* -----------------------------------------------------------
 * Forward declarations
 * -----------------------------------------------------------
 */

void ospf_apiclient_handle_reply (struct ospf_apiclient *oclient,
				  struct msg *msg);
void ospf_apiclient_handle_update_notify (struct ospf_apiclient *oclient,
					  struct msg *msg);
void ospf_apiclient_handle_delete_notify (struct ospf_apiclient *oclient,
					  struct msg *msg);

/* -----------------------------------------------------------
 * Followings are functions for connection management
 * -----------------------------------------------------------
 */
struct ospf_apiclient *
ospf_apiclient_connect (char *host, int syncport, int remote_port)
{
  struct sockaddr_in myaddr_sync;
  struct sockaddr_in myaddr_async;
  struct sockaddr_in peeraddr;
  struct hostent *hp;
  struct ospf_apiclient *new;
  int size = 0;
  int peeraddrlen;
  int async_server_sock;
  int fd1, fd2;
  int ret;
  int on = 1;

  /* There are two connections between the client and the server.
     First the client opens a connection for synchronous requests/replies 
     to the server. The server will accept this connection and
     as a reaction open a reverse connection channel for 
     asynchronous messages. */

  async_server_sock = socket (AF_INET, SOCK_STREAM, 0);
  if (async_server_sock < 0)
    {
      fprintf (stderr,
	       "ospf_apiclient_connect: creating async socket failed\n");
      return NULL;
    }

  /* Prepare socket for asynchronous messages */
  /* Initialize async address structure */
  memset (&myaddr_async, 0, sizeof (struct sockaddr_in));
  myaddr_async.sin_family = AF_INET;
  myaddr_async.sin_addr.s_addr = htonl (INADDR_ANY);
  myaddr_async.sin_port = htons (syncport+1);
  size = sizeof (struct sockaddr_in);
#ifdef HAVE_SIN_LEN
  myaddr_async.sin_len = size;
#endif /* HAVE_SIN_LEN */

  /* This is a server socket, reuse addr and port */
  ret = setsockopt (async_server_sock, SOL_SOCKET,
		    SO_REUSEADDR, (void *) &on, sizeof (on));
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: SO_REUSEADDR failed\n");
      close (async_server_sock);
      return NULL;
    }

#ifdef SO_REUSEPORT
  ret = setsockopt (async_server_sock, SOL_SOCKET, SO_REUSEPORT,
		    (void *) &on, sizeof (on));
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: SO_REUSEPORT failed\n");
      close (async_server_sock);
      return NULL;
    }
#endif /* SO_REUSEPORT */

  /* Bind socket to address structure */
  ret = bind (async_server_sock, (struct sockaddr *) &myaddr_async, size);
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: bind async socket failed\n");
      close (async_server_sock);
      return NULL;
    }

  /* Wait for reverse channel connection establishment from server */
  ret = listen (async_server_sock, BACKLOG);
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: listen: %s\n", strerror (errno));
      close (async_server_sock);
      return NULL;
    }

  /* Make connection for synchronous requests and connect to server */
  /* Resolve address of server */
  hp = gethostbyname (host);
  if (!hp)
    {
      fprintf (stderr, "ospf_apiclient_connect: no such host %s\n", host);
      close (async_server_sock);
      return NULL;
    }

  fd1 = socket (AF_INET, SOCK_STREAM, 0);
  if (fd1 < 0)
    {
      fprintf (stderr,
	       "ospf_apiclient_connect: creating sync socket failed\n");
      close (async_server_sock);
      return NULL;
    }

  /* Reuse addr and port */
  ret = setsockopt (fd1, SOL_SOCKET,
                    SO_REUSEADDR, (void *) &on, sizeof (on));
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: SO_REUSEADDR failed\n");
      close (async_server_sock);
      close (fd1);
      return NULL;
    }

#ifdef SO_REUSEPORT
  ret = setsockopt (fd1, SOL_SOCKET, SO_REUSEPORT,
                    (void *) &on, sizeof (on));
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: SO_REUSEPORT failed\n");
      close (async_server_sock);
      close (fd1);
      return NULL;
    }
#endif /* SO_REUSEPORT */


  /* Bind sync socket to address structure. This is needed since we
     want the sync port number on a fixed port number. The reverse
     async channel will be at this port+1 */

  memset (&myaddr_sync, 0, sizeof (struct sockaddr_in));
  myaddr_sync.sin_family = AF_INET;
  myaddr_sync.sin_port = htons (syncport);
#ifdef HAVE_SIN_LEN
  myaddr_sync.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_SIN_LEN */

  ret = bind (fd1, (struct sockaddr *) &myaddr_sync, size);
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: bind sync socket failed\n");
      close (async_server_sock);
      close (fd1);
      return NULL;
    }

  /* Prepare address structure for connect */
  memcpy (&myaddr_sync.sin_addr, hp->h_addr, hp->h_length);
  myaddr_sync.sin_family = AF_INET;
  myaddr_sync.sin_port = htons(remote_port);
#ifdef HAVE_SIN_LEN
  myaddr_sync.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_SIN_LEN */

  /* Now establish synchronous channel with OSPF daemon */
  ret = connect (fd1, (struct sockaddr *) &myaddr_sync,
		 sizeof (struct sockaddr_in));
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: sync connect failed\n");
      close (async_server_sock);
      close (fd1);
      return NULL;
    }

  /* Accept reverse connection */
  peeraddrlen = sizeof (struct sockaddr_in);
  memset (&peeraddr, 0, peeraddrlen);

  fd2 =
    accept (async_server_sock, (struct sockaddr *) &peeraddr, &peeraddrlen);
  if (fd2 < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: accept async failed\n");
      close (async_server_sock);
      close (fd1);
      return NULL;
    }

  /* Server socket is not needed anymore since we are not accepting more 
     connections */
  close (async_server_sock);

  /* Create new client-side instance */
  new = XMALLOC (MTYPE_OSPF_APICLIENT, sizeof (struct ospf_apiclient));
  memset (new, 0, sizeof (struct ospf_apiclient));

  /* Initialize socket descriptors for sync and async channels */
  new->fd_sync = fd1;
  new->fd_async = fd2;
  new->out_sync_fifo = msg_fifo_new();
  new->lsdb = list_new();

  new->t_async_read = 
    thread_add_read(master, ospf_narb_read, new, fd2);
  return new;
}

int
ospf_apiclient_close (struct ospf_apiclient *oclient)
{
  listnode node;
  struct ospf_lsa *lsa;
  
  if (oclient->fd_sync >= 0)
    {
      close (oclient->fd_sync);
    }

  if (oclient->fd_async >= 0)
    {
      close (oclient->fd_async);
    }
  if (oclient->lsdb)
    {
      node = listhead(oclient->lsdb);
      while(node)
        {
          lsa = getdata(node);
          if (lsa)
            {
              lsa->lock = 0;
              ospf_lsa_free(lsa);
            }
          nextnode(node);
        }
      list_delete(oclient->lsdb);
    }
  if (oclient->out_sync_fifo)
    {
      msg_fifo_free(oclient->out_sync_fifo);
    }
  if (oclient->t_async_read)
    {
      thread_cancel(oclient->t_async_read);
      oclient->t_async_read = NULL;
    }
  /* Free client structure */
  XFREE (MTYPE_OSPF_APICLIENT, oclient);
  return 0;
}

/* -----------------------------------------------------------
 * Followings are functions to send a request to OSPFd
 * -----------------------------------------------------------
 */

/* Send synchronous request, wait for reply */
int
ospf_apiclient_send_request (struct ospf_apiclient *oclient, struct msg *msg)
{
  u_int32_t reqseq;
  struct msg_reply *msgreply;
  int rc;

  /* NB: Given "msg" is freed inside this function. */

  /* Remember the sequence number of the request */
  reqseq = ntohl (msg->hdr.msgseq);

  /* Write message to OSPFd */
  rc = msg_write (oclient->fd_sync, msg);
  msg_free (msg);

  if (rc < 0)
    {
      return -1;
    }

  /* Synchronous write/read. Wait for reply */
  msg = msg_read (oclient->fd_sync);
  if (!msg)
    return -1;

  assert (msg->hdr.msgtype == MSG_REPLY);
  assert (ntohl (msg->hdr.msgseq) == reqseq);

  msgreply = (struct msg_reply *) STREAM_DATA (msg->s);
  rc = msgreply->errcode;
  msg_free (msg);

  return rc;
}


/* -----------------------------------------------------------
 * Helper functions
 * -----------------------------------------------------------
 */

u_int32_t
ospf_apiclient_get_seqnr (void)
{
  static u_int32_t seqnr = MIN_SEQ;
  u_int32_t tmp;

  tmp = seqnr;
  /* Increment sequence number */
  if (seqnr < MAX_SEQ)
    {
      seqnr++;
    }
  else
    {
      seqnr = MIN_SEQ;
    }
  return tmp;
}

/* -----------------------------------------------------------
 * API to access OSPF daemon by client applications.
 * -----------------------------------------------------------
 */

/*
 * Synchronous request to register opaque type.
 */
int
ospf_apiclient_register_opaque_type (struct ospf_apiclient *cl,
				     u_char ltype, u_char otype)
{
  struct msg *msg;
  int rc;

  /* just put 1 as a sequence number. */
  msg = new_msg_register_opaque_type (ospf_apiclient_get_seqnr (),
				      ltype, otype);
  if (!msg)
    {
      fprintf (stderr, "new_msg_register_opaque_type failed\n");
      return -1;
    }

  rc = ospf_apiclient_send_request (cl, msg);
  return rc;
}

/* 
 * Synchronous request to synchronize with OSPF's LSDB.
 * Two steps required: register_event in order to get
 * dynamic updates and LSDB_Sync.
 */
int
ospf_apiclient_sync_lsdb (struct ospf_apiclient *oclient)
{
  struct msg *msg;
  int rc;
  struct lsa_filter_type filter;

  filter.typemask = 0x200;	/* Type-10 Opauqe LSAs */
  filter.origin = ANY_ORIGIN;
  filter.num_areas = 0;		/* all Areas. */

  msg = new_msg_register_event (ospf_apiclient_get_seqnr (), &filter);
  if (!msg)
    {
      fprintf (stderr, "new_msg_register_event failed\n");
       return -1;
    }
  rc = ospf_apiclient_send_request (oclient, msg);

  if (rc != 0)
    goto out;

  msg = new_msg_sync_lsdb (ospf_apiclient_get_seqnr (), &filter);
  if (!msg)
    {
      fprintf (stderr, "new_msg_sync_lsdb failed\n");
      return -1;
    }
  rc = ospf_apiclient_send_request (oclient, msg);

  if (rc != 0)
    goto out;

  /*Additional code to count the ospfd neighbors periodically. May be
    put into a separate function (timer) later.*/
  msg = msg_new (MSG_NEIGHBOR_COUNT_REQUEST, NULL, ospf_apiclient_get_seqnr (), 0);
  rc = ospf_apiclient_send_request (oclient, msg);
  
out:
  return rc;
}

/* 
 * Synchronous request to originate or update an LSA.
 */

/*Hacked $$$$ a new argument, struct in_addr adv_id, is passed and processed*/
int
ospf_apiclient_lsa_originate (struct ospf_apiclient *oclient,
			      struct in_addr ifaddr, struct in_addr adv_id, struct in_addr area_id, 
			      u_char lsa_type, u_char opaque_type, u_int32_t opaque_id,
			      void *opaquedata, int opaquelen)
{
  struct msg *msg;
  int rc;
  u_char buf[OSPF_MAX_LSA_SIZE];
  struct lsa_header *lsah;
  u_int32_t tmp;


  /* We can only originate opaque LSAs */
  if (!IS_OPAQUE_LSA (lsa_type))
    {
      fprintf (stderr, "Cannot originate non-opaque LSA type %d\n", lsa_type);
      return OSPF_API_ILLEGALLSATYPE;
    }

  /* Make a new LSA from parameters */
  lsah = (struct lsa_header *) buf;
  lsah->ls_age = 0;
  lsah->options = 0;
  lsah->type = lsa_type;

  tmp = SET_OPAQUE_LSID (opaque_type, opaque_id);
  lsah->id.s_addr = htonl (tmp);
  lsah->adv_router = adv_id;
  lsah->ls_age = 0;
  lsah->ls_seqnum = 0;
  lsah->checksum = 0;
  lsah->length = htons (sizeof (struct lsa_header) + opaquelen);

  memcpy (((u_char *) lsah) + sizeof (struct lsa_header), opaquedata,
	  opaquelen);

  msg = new_msg_originate_request (ospf_apiclient_get_seqnr (),
				   ifaddr, area_id, lsah);
  if (!msg)
    {
      fprintf (stderr, "new_msg_originate_request failed\n");
      return OSPF_API_NOMEMORY;
    }

  rc = ospf_apiclient_send_request (oclient, msg);
  return rc;
}

/*Hacked $$$$ a new argument, struct in_addr adv_id, is passed and processed*/
int
ospf_apiclient_lsa_delete (struct ospf_apiclient *oclient, 
              struct in_addr adv_router,
			   struct in_addr area_id, u_char lsa_type,
			   u_char opaque_type, u_int32_t opaque_id)
{
  struct msg *msg;
  int rc;

  /* Only opaque LSA can be deleted */
  if (!IS_OPAQUE_LSA (lsa_type))
    {
      fprintf (stderr, "Cannot delete non-opaque LSA type %d\n", lsa_type);
      return OSPF_API_ILLEGALLSATYPE;
    }

  /* opaque_id is in host byte order and will be converted
   * to network byte order by new_msg_delete_request */
  msg = new_msg_delete_request (ospf_apiclient_get_seqnr (),
              adv_router,
				area_id, lsa_type, opaque_type, opaque_id);

  rc = ospf_apiclient_send_request (oclient, msg);
  return rc;
}

/* -----------------------------------------------------------
 * Followings are handlers for messages from OSPF daemon
 * -----------------------------------------------------------
 */
/*$$$$ NOT USED IN NARB*/
void
ospf_apiclient_handle_ready (struct ospf_apiclient *oclient, struct msg *msg)
{
  struct msg_ready_notify *r;
  r = (struct msg_ready_notify *) STREAM_DATA (msg->s);

  zlog_warn("ospf_apiclient_handle_ready is called");

  /* Invoke registered callback function. */
  if (oclient->ready_notify)
    {
      (oclient->ready_notify) (r->lsa_type, r->opaque_type, r->addr);
    }
}

/*$$$$ NOT USED IN NARB*/
void
ospf_apiclient_handle_new_if (struct ospf_apiclient *oclient, struct msg *msg)
{
  struct msg_new_if *n;
  n = (struct msg_new_if *) STREAM_DATA (msg->s);

  zlog_warn("msg_new_if called");

  /* Invoke registered callback function. */
  if (oclient->new_if)
    {
      (oclient->new_if) (n->ifaddr, n->area_id);
    }
}
/*$$$$ NOT USED IN NARB*/
void
ospf_apiclient_handle_del_if (struct ospf_apiclient *oclient, struct msg *msg)
{
  struct msg_del_if *d;
  d = (struct msg_del_if *) STREAM_DATA (msg->s);

  zlog_warn("msg_del_if called");

  /* Invoke registered callback function. */
  if (oclient->del_if)
    {
      (oclient->del_if) (d->ifaddr);
    }
}
/*$$$$ NOT USED IN NARB*/
void
ospf_apiclient_handle_ism_change (struct ospf_apiclient *oclient,
				  struct msg *msg)
{
  struct msg_ism_change *m;
  m = (struct msg_ism_change *) STREAM_DATA (msg->s);

  zlog_warn("ospf_apiclient_ism_change is called");

  /* Invoke registered callback function. */
  if (oclient->ism_change)
    {
      (oclient->ism_change) (m->ifaddr, m->area_id, m->status);
    }

}
/*$$$$ NOT USED IN NARB*/
void
ospf_apiclient_handle_nsm_change (struct ospf_apiclient *oclient,
				  struct msg *msg)
{
  struct msg_nsm_change *m;
  m = (struct msg_nsm_change *) STREAM_DATA (msg->s);

  zlog_warn("ospf_apiclient_nsm_change is called");

  /* Invoke registered callback function. */
  if (oclient->nsm_change)
    {
      (oclient->nsm_change) (m->ifaddr, m->nbraddr, m->router_id, m->status);
    }
}

struct ospf_lsa *
narb_lsdb_lookup (list lsdb, struct ospf_lsa *lsa)
{
  listnode node;
  struct ospf_lsa *nodedata;

  node = listhead(lsdb);
  while (node)
    {
      nodedata = getdata(node);
      if (nodedata->data->adv_router.s_addr == lsa->data->adv_router.s_addr
          && nodedata->data->id.s_addr == lsa->data->id.s_addr)
          return nodedata;
      nextnode(node);
    }
  return NULL;
  
}

  
/* After sync_lsdb message is sent to OSPFd, this function is called in an async
  manner. Each time an LSA is returned. If it has existed replace the old one in
  NARB LSDB. Otherwise, add it into NARB LSDB.***
  ***  Interval of automatic-sync between NARB and OSPFd = 30 secs*/
void
ospf_apiclient_handle_lsa_update (struct ospf_apiclient *oclient,
				  struct msg *msg)
{
  struct msg_lsa_change_notify *cn;
  struct lsa_header *lsa;
  struct ospf_lsa *old, *new;
  struct link_info *link;
  int lsalen;
  struct rce_api_msg * rce_msg;
  static int rce_seq_num = 1;

  cn = (struct msg_lsa_change_notify *) STREAM_DATA (msg->s);

  /* Extract LSA from message */
  lsalen = ntohs (cn->data.length);
  lsa = XMALLOC (MTYPE_OSPF_APICLIENT, lsalen);
  if (!lsa)
    {
      zlog_warn("LSA update: Cannot allocate memory for LSA\n");
      return;
    }
  memcpy (lsa, &(cn->data), lsalen);

  /* construct the new lsa*/
  new = ospf_lsa_new();
  new->data = lsa;
  new->te_lsa_type = LINK_TE_LSA;
  new = ospf_te_lsa_parse(new);
  
  assert (oclient == oclient_inter ||oclient == oclient_intra);

  old = narb_lsdb_lookup (oclient->lsdb, new);

  /* discard old TE-LSA from TE-LSDB */
  if (old)
    {
       /* Turn on refreshment for this TE link if it's originated by this NARB*/
      if (old->tepara_ptr)
        if(old->tepara_ptr->p_lclif_ipaddr && old->tepara_ptr->p_rmtif_ipaddr)
          {
            link = narb_lookup_link_by_if_addr(narb_domain_info.te_links, 
              &old->tepara_ptr->p_lclif_ipaddr->value, 
              &old->tepara_ptr->p_rmtif_ipaddr->value);
            if (link)
              link->hide = 0;
          }

      /* delete the old LSA from NARB LSDB */
      SET_FLAG(old->flags, OSPF_LSA_DISCARD);
      old->lock = 1;
      listnode_delete(oclient->lsdb, old);
      old->lock = 0;
      ospf_lsa_free(old);
    }
  
  /* Insert new TE-LSA into TE-LSDB */
  listnode_add(oclient->lsdb, new);

  //$$$$ Wrap the lsa into RCE_MSG and send it to RCE
 /*
 if (rce_api_sock < 0)
   {
      rce_api_sock = narb_rceapi_connect(RCE_HOST_ADDR, RCE_API_PORT);
      if (rce_api_sock < 0)
        {
            zlog_err("Cannot connect to RCE\n");
            return;
        }
    }
   rce_msg = rce_api_msg_new(MSG_LSA, ACT_UPDATE, new->data, rce_seq_num++, ntohs(new->data->length));
   rce_msg->hdr.msgtag[0] = 0xffffffff;
   if (narb_rceapi_send(rce_api_sock, rce_msg) < 0)
      rce_api_sock = -1;
   else
    {
      rce_msg = narb_rceapi_read (rce_api_sock);
      if (!rce_msg)
        rce_api_sock = -1;
      else
        narb_rceapi_msghandle (rce_msg);
    }
*/
}

void
ospf_apiclient_handle_lsa_delete (struct ospf_apiclient *oclient,
				  struct msg *msg)
{
  struct msg_lsa_change_notify *cn;
  struct ospf_lsa *new, *old;
  struct lsa_header *data;
  struct link_info *link;
  int lsalen;

 cn = (struct msg_lsa_change_notify *) STREAM_DATA (msg->s);

  new =  ospf_lsa_new();
  
  /* Extract LSA from message */
  lsalen = ntohs (cn->data.length);
  data = XMALLOC (MTYPE_OSPF_APICLIENT, lsalen);
  if (!data)
    {
     zlog_warn("LSA delete: Cannot allocate memory for LSA\n");
      return;
    }
  memcpy (data, &(cn->data), lsalen);

  new->data = data;

  new->te_lsa_type = LINK_TE_LSA;
    
  old = narb_lsdb_lookup(oclient->lsdb, new);
  /*in the case of delete, no te_lsa_parse is needed*/
  
  if (old)   
    {
       /* Turn off refreshment for this TE link if it's originated by this NARB.*/
      if (old->tepara_ptr)
        if(old->tepara_ptr->p_lclif_ipaddr && old->tepara_ptr->p_rmtif_ipaddr)
          {
            link = narb_lookup_link_by_if_addr(narb_domain_info.te_links, 
              &old->tepara_ptr->p_lclif_ipaddr->value, 
              &old->tepara_ptr->p_rmtif_ipaddr->value);
            if (link)
              link->hide = 1;
          }

      /*delete the lsa from NARB LSDB */
      listnode_delete(oclient->lsdb, old);
      old->lock = 0;
      ospf_lsa_free(old);
    }
 
  /* free memory allocated by ospf apiclient library */
  XFREE (MTYPE_OSPF_APICLIENT, data);
  XFREE (MTYPE_OSPF_LSA, new);
}

/*For now, we only handle MSG_LSA_UPDATE_NOTIFY message.*/
void
ospf_apiclient_msghandle (struct ospf_apiclient *oclient, struct msg *msg)
{
  /* Call message handler function. */
  switch (msg->hdr.msgtype)
    {
    /*
    case MSG_READY_NOTIFY:
      ospf_apiclient_handle_ready (oclient, msg);
      break;
   case MSG_ISM_CHANGE:
      ospf_apiclient_handle_ism_change (oclient, msg);
      break;
    case MSG_NSM_CHANGE:
      ospf_apiclient_handle_nsm_change (oclient, msg);
      break;
    case MSG_NEW_IF:
      ospf_apiclient_handle_new_if (oclient, msg);
      break;
    case MSG_DEL_IF:
      ospf_apiclient_handle_del_if (oclient, msg);
      break;
    */
    case MSG_LSA_UPDATE_NOTIFY:
      ospf_apiclient_handle_lsa_update (oclient, msg);
      break;
    case MSG_LSA_DELETE_NOTIFY:
      ospf_apiclient_handle_lsa_delete (oclient, msg);
      break;
    case MSG_NEIGHBOR_COUNT:
      ospf_apiclient_handle_neighbor_count (oclient, msg);
      break;
      /*
    default:
      fprintf (stderr, "ospf_apiclient_read: Unknown message type: %d\n",
	       msg->hdr.msgtype);
      break;
      */
    }
}


/* -----------------------------------------------------------
 * Asynchronous OSPFd->NARB message handling
 * -----------------------------------------------------------
 */
int
ospf_apiclient_handle_async (struct ospf_apiclient *oclient)
{
  struct msg *msg;

  /* Get a message */
  msg = msg_read (oclient->fd_async);

  if (!msg)
    {
      /* Connection broke down */
      return -1;
    }

  /* Handle message */
  ospf_apiclient_msghandle (oclient, msg);

  /* Don't forget to free this message */
  msg_free (msg);

  return 0;
}



/* ---------------------------------------------------------
 * Functions for constructing Opauqe TE TLVs for NARB<->OSPFd communication
 * ---------------------------------------------------------
 */

/* Constructing Router_ID TLV
   Memory for tlv data allocated within function*/
struct te_tlv_header *
ospf_te_router_addr_tlv_alloc(struct in_addr addr)
{
  struct te_tlv_header * tlv_header = malloc(sizeof(struct te_tlv_header)
  				+ sizeof(struct in_addr));
  tlv_header->type = TE_TLV_ROUTER_ADDR;
  tlv_header->type = htons(tlv_header->type);
  tlv_header->length = sizeof(struct in_addr);
  tlv_header->length = htons(tlv_header->length);
  memcpy((void *)tlv_header + sizeof(struct te_tlv_header), (void *)&addr,
  				sizeof(struct in_addr));
  return tlv_header;
}

/* Constructing TE Link TLV
  Memory for tlv data allocated within function*/
struct te_tlv_header *
ospf_te_link_tlv_alloc(u_char type, struct in_addr addr)
{
  struct te_tlv_header * tlv_header;
  struct te_link_subtlv_link_type link_type;
  struct te_link_subtlv_link_id link_id;
  int length = sizeof (struct te_link_subtlv_link_type)
                        + sizeof(struct te_link_subtlv_link_id);
  
  tlv_header = malloc(sizeof(struct te_tlv_header) + length);
  tlv_header->type = TE_TLV_LINK;
  tlv_header->type = htons(tlv_header->type);
  tlv_header->length = htons(length);

  memset(&link_type, 0, sizeof(struct te_link_subtlv_link_type));
  link_type.header.type = TE_LINK_SUBTLV_LINK_TYPE;
  link_type.header.type = htons(link_type.header.type);
  link_type.header.length = 1;
  link_type.header.length = htons(link_type.header.length);
  link_type.link_type.value = type;
  memcpy((void *)tlv_header + sizeof(struct te_tlv_header), 
              (void *)&link_type, sizeof(struct te_link_subtlv_link_type));

  link_id.header.type = TE_LINK_SUBTLV_LINK_ID;
  link_id.header.type = htons(link_id.header.type);
  link_id.header.length = sizeof(struct in_addr);
  link_id.header.length = htons(link_id.header.length);
  link_id.value = addr;
  memcpy((void *)tlv_header + sizeof(struct te_tlv_header)
                      + sizeof(struct te_link_subtlv_link_type), 
              (void *)&link_id, 
              sizeof(struct te_link_subtlv_link_id));

  return tlv_header;
}

void htonf_mbps(float * src, float * dest)
{
    u_int32_t * p = (u_int32_t*)dest;
    *dest =(*src)*1000000/8;
    *p = htonl(*p);
}

/* Appending optional TE parameter (sub-TLV) to TE Link TLV.
   Memory of the tlv data re-allocated within function*/
struct te_tlv_header *
ospf_te_link_subtlv_append(struct te_tlv_header * tlv_header,
		u_int16_t type, void *value)
{
  struct te_tlv_header *tlv_header_appended;
  char buf[OSPF_MAX_LSA_SIZE];
  int tlv_size = sizeof(struct te_tlv_header) + ntohs(tlv_header->length);
  int sub_tlv_size;

  switch (type)
    {
      case TE_LINK_SUBTLV_LCLIF_IPADDR:
        {
          struct te_link_subtlv_lclif_ipaddr *lclif_ipaddr
            = (struct te_link_subtlv_lclif_ipaddr *)(buf + tlv_size);
          
          lclif_ipaddr->header.type = htons(TE_LINK_SUBTLV_LCLIF_IPADDR);
          sub_tlv_size = sizeof (struct te_link_subtlv_lclif_ipaddr);
          lclif_ipaddr->header.length = htons(sizeof(struct in_addr));
          lclif_ipaddr->value = *((struct in_addr*)value);
          break;
        }
      case TE_LINK_SUBTLV_RMTIF_IPADDR:
        {
          struct te_link_subtlv_rmtif_ipaddr *rmtif_ipaddr 
            = (struct te_link_subtlv_rmtif_ipaddr *)(buf + tlv_size);
          
          rmtif_ipaddr->header.type = htons(TE_LINK_SUBTLV_RMTIF_IPADDR);
          sub_tlv_size = sizeof (struct te_link_subtlv_rmtif_ipaddr);
          rmtif_ipaddr->header.length = htons(sizeof(struct in_addr));
          rmtif_ipaddr->value = *((struct in_addr*)value);
          break;
        }
      case TE_LINK_SUBTLV_TE_METRIC:
          {
          struct te_link_subtlv_te_metric *metric 
            = (struct te_link_subtlv_te_metric *)(buf + tlv_size);
          
          metric->header.type = htons(TE_LINK_SUBTLV_TE_METRIC);
          sub_tlv_size = sizeof (struct te_link_subtlv_te_metric);
          metric->header.length = htons(sizeof(u_int32_t));
          metric->value = htonl(*((u_int32_t*)value));
	      break;
        }
      case TE_LINK_SUBTLV_MAX_BW:
          {
          struct te_link_subtlv_max_bw *max_bw 
            = (struct te_link_subtlv_max_bw *)(buf + tlv_size);
          
          max_bw->header.type = htons(TE_LINK_SUBTLV_MAX_BW);
          sub_tlv_size = sizeof (struct te_link_subtlv_max_bw);
          max_bw->header.length = htons(sizeof(float));
          htonf_mbps(value, &max_bw->value);
	      break;
        }
      case TE_LINK_SUBTLV_MAX_RSV_BW:
        {
          struct te_link_subtlv_max_rsv_bw *max_rsv_bw 
            = (struct te_link_subtlv_max_rsv_bw *)(buf + tlv_size);
          
          max_rsv_bw->header.type = htons(TE_LINK_SUBTLV_MAX_RSV_BW);
          sub_tlv_size = sizeof (struct te_link_subtlv_max_rsv_bw);
          max_rsv_bw->header.length = htons(sizeof(float));
          htonf_mbps(value, &max_rsv_bw->value);
	      break;
        }
      case TE_LINK_SUBTLV_UNRSV_BW:
        {
          struct te_link_subtlv_unrsv_bw *max_unrsv_bw 
            = (struct te_link_subtlv_unrsv_bw *)(buf + tlv_size);
          int i;
          
          max_unrsv_bw->header.type = htons(TE_LINK_SUBTLV_UNRSV_BW);
          sub_tlv_size = sizeof (struct te_link_subtlv_unrsv_bw);
          max_unrsv_bw->header.length = htons(sizeof(float) * 8);
          for (i = 0; i < 8; i++)
            {
              htonf_mbps((float *)value + i, (float *)(&(max_unrsv_bw->value)) + i);
            }
	      break;
        }
      case TE_LINK_SUBTLV_RSC_CLSCLR:
        {
	       struct te_link_subtlv_rsc_clsclr *rsc_clsclr 
            = (struct te_link_subtlv_rsc_clsclr *)(buf + tlv_size);
          
          rsc_clsclr->header.type = htons(TE_LINK_SUBTLV_RSC_CLSCLR);
          sub_tlv_size = sizeof (struct te_link_subtlv_rsc_clsclr);
          rsc_clsclr->header.length = htons(sizeof(u_int32_t));
          rsc_clsclr->value = htonl(*((u_int32_t *)value));
	       break;
        }  
      case TE_LINK_SUBTLV_LINK_LCRMT_ID:
        {
          struct te_link_subtlv_link_lcrmt_id *lcrmt_id 
            = (struct te_link_subtlv_link_lcrmt_id *)(buf + tlv_size);

          lcrmt_id->header.type = htons(TE_LINK_SUBTLV_LINK_LCRMT_ID);
          sub_tlv_size = sizeof (struct te_link_subtlv_link_lcrmt_id);
          lcrmt_id->header.length = htons(sizeof(u_int32_t) * 2);
          lcrmt_id->link_local_id = htonl(*((u_int32_t *)value));
          lcrmt_id->link_remote_id = htonl(*((u_int32_t *)value + 1));
          break;
        }
      case TE_LINK_SUBTLV_LINK_PROTYPE: 
        {
          struct te_link_subtlv_link_protype *protype 
            = (struct te_link_subtlv_link_protype *)(buf + tlv_size);

          protype->header.type = htons(TE_LINK_SUBTLV_LINK_PROTYPE);
          sub_tlv_size = sizeof (struct te_link_subtlv_link_protype);
          protype->header.length = htons(1);
          memset(&(protype->value4), 0, 4);
          protype->value4.value = *((u_char *)value);
          break;
        }
      case TE_LINK_SUBTLV_LINK_IFSWCAP:
        {
          int i; float x;
          struct te_link_subtlv_link_ifswcap *ifswcap 
                  = (struct te_link_subtlv_link_ifswcap *)(buf + tlv_size);

          ifswcap->header.type = htons(TE_LINK_SUBTLV_LINK_IFSWCAP);
          sub_tlv_size = sizeof (struct te_link_subtlv_link_ifswcap);
          ifswcap->header.length = htons(sizeof(struct link_ifswcap_type));
          memcpy((void *)&ifswcap->link_ifswcap_data, value, 
                    sizeof(struct link_ifswcap_type));
          for (i = 0; i < 8; i++)
            {
              htonf_mbps(ifswcap->link_ifswcap_data.max_lsp_bw_at_priority + i, &x);
              ifswcap->link_ifswcap_data.max_lsp_bw_at_priority[i] = x;
            }
          break;
        }
      case TE_LINK_SUBTLV_LINK_SRLG:
        {
          listnode node;
          list srlg_list = (list)value;
          int sub_tlv_length = 0;
          struct te_tlv_header *header
            = (struct te_tlv_header *)(buf + tlv_size);

          header->type = htons(TE_LINK_SUBTLV_LINK_SRLG);
          for (node = listhead(srlg_list); node; nextnode(node))
            {
              u_int32_t* data = getdata(node);
              *data = htonl(*data);
              memcpy(buf + tlv_size + sizeof(struct te_tlv_header) 
                        + sub_tlv_length, data, 4);
              sub_tlv_length += 4;
            }
          sub_tlv_size = sizeof(struct te_tlv_header) + sub_tlv_length;
          header->length = htons(sub_tlv_length);
          memcpy((void *)(buf + tlv_size), (void *)header,
                    sizeof(struct te_tlv_header));
          break;
        }
      case TE_LINK_SUBTLV_LINK_DRAGON_RESV:
        {
          struct te_tlv_header *resv_header = (buf + tlv_size);
          struct reservation *resv = (struct reservation *)(buf + tlv_size+TLV_HDR_SIZE);
          list list_resvs = (list)value;
          listnode node = listhead(list_resvs);
          resv_header->type = htons(TE_LINK_SUBTLV_LINK_DRAGON_RESV);
          resv_header->length = htons(list_resvs->count*RESV_SIZE);
          sub_tlv_size = TLV_HDR_SIZE + list_resvs->count*RESV_SIZE;
          while (node)
            {
              if (node->data)
                  memcpy((void *)(resv++), node->data, sizeof(struct reservation));
              nextnode(node);
            }
          break;
        }
      case TE_LINK_SUBTLV_LINK_DRAGON_DOMAIN_ID:
        {
          struct te_link_domain_id *domain_id 
            = (struct te_link_domain_id *)(buf + tlv_size);
          domain_id->header.type = htons(TE_LINK_SUBTLV_LINK_DRAGON_DOMAIN_ID);
          domain_id->header.length = htons(sizeof(u_int32_t));
          sub_tlv_size = sizeof (struct te_link_domain_id);
          domain_id->value = *(u_int32_t*)value;
         break;
      }       
      default:
	       zlog_info("ospf_te_link_subtlv_append: Unrecognized subtlv type-%d", type);
	       return tlv_header;
    }

  tlv_header->length = htons(ntohs(tlv_header->length) + sub_tlv_size);
  memcpy(buf, tlv_header, tlv_size);
  free(tlv_header);

  tlv_header_appended = malloc(tlv_size + sub_tlv_size);
  memcpy((void *)tlv_header_appended, (void *)buf, tlv_size + sub_tlv_size);

  return tlv_header_appended;
}


/* Handling asynchronous messages coming in from the OSPFd API server.
  In particular, sync_lsdb function is called here. */
int 
ospf_narb_read (struct thread *thread)
{
  struct ospf_apiclient *oc;
  int fd;
  int ret;

  oc = THREAD_ARG (thread);
  fd = THREAD_FD (thread);

  /* Handle asynchronous message */
  ret = ospf_apiclient_handle_async (oc);
  if (ret < 0) {
    zlog_warn ("ospf_narb_read: Async connection closed, exiting...");
    oc->disconnected = 1;
    oc->t_async_read = NULL;

    if (oc->t_originator)
      {
        if (thread_timer_remain_second(oc->t_originator) > 0)
          thread_cancel(oc->t_originator);
        oc->t_originator = NULL;
      }

    if (oc->t_sync_lsdb)
      {
        if (thread_timer_remain_second(oc->t_sync_lsdb) > 0)
          thread_cancel(oc->t_sync_lsdb);
        oc->t_sync_lsdb = NULL;
      }

    return ret;
  }

  /* Reschedule read thread */
  oc->t_async_read = 
      thread_add_read (master, ospf_narb_read, oc, fd);

  return 0;
}

void
ospf_apiclient_handle_neighbor_count (struct ospf_apiclient *oclient,
				  struct msg *msg)
{
  struct msg_neighbor_count *m;
  m = (struct msg_neighbor_count *) STREAM_DATA (msg->s);

  if (oclient)
        oclient->neighbor_count = m->count;
}

/* Printing header of LSA returned from sync_lsdb (for debugging)*/
static int 
ospf_narb_dump_lsdb (struct thread *thread)
{
  struct ospf_apiclient *oc = THREAD_ARG(thread);
  if (!oclient_inter)
    return -1;

  zlog_info ("ospf_narb_dump_lsdb....... %d records...... (with %d neighbor(s))", 
      oc->lsdb->count, oc->neighbor_count);

  return 0;
}

/* Timer thread function to sync LSDB between OSPFd and NARB periodically*/
int 
ospf_narb_sync_lsdb (struct thread *thread)
{
  struct ospf_apiclient *oc = THREAD_ARG(thread);
  
  int rc =  ospf_apiclient_sync_lsdb(oc);
  thread_add_timer (master, ospf_narb_dump_lsdb, oc, 5);
  
  flag_holdon = 0;

  /* Sync periodically */
  oc->t_sync_lsdb =
    thread_add_timer (master, ospf_narb_sync_lsdb, oc, OSPF_NARB_SYNC_LSDB_INVERVAL);

  return rc;
}

/* Looking for an LSA in NARB LSDB */
struct ospf_lsa *
narb_lsdb_router_lookup_by_addr (list lsdb, struct in_addr addr)
{
  listnode node;
  struct ospf_lsa *lsa;
  struct lsa_header *header;
  struct te_tlv_header *tlv;
  u_int16_t type;
  struct in_addr id;

  if (!lsdb->count)
    return NULL;
  
  node = listhead(lsdb);
  while (node)
    {
      lsa = getdata(node);
      nextnode(node);
      if (!lsa)
        continue;
      
      header = lsa->data;
     /* assert(ntohs(header->length) >= sizeof(struct lsa_header) + 8);*/
      tlv = (struct te_tlv_header *)((char *)header + sizeof(struct lsa_header));
      type = ntohs(tlv->type);
      if (type == ROUTER_ID_TE_LSA)
        {
          id =  *(struct in_addr *)((char *)tlv + sizeof(struct te_tlv_header));
          if (id.s_addr == addr.s_addr)
            return lsa;
        }
    }
  
  return NULL;
}
