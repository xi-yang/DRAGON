/****************************************************************************
narb_apiserver.c

Provides routing and domain information servirces to ASTDL speaking app clients.
Each client communicates with a unique instance of narb_server throgh a TCP-socket.

****************************************************************************/
#include <zebra.h>

#include <lib/version.h>
#include "getopt.h"
#include "thread.h"
#include "prefix.h"
#include "sockunion.h"
#include "linklist.h"
#include "if.h"
#include "vector.h"
#include "vty.h"
#include "command.h"
#include "filter.h"
#include "stream.h"
#include "hash.h"
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
#include "narb_summary.h"
#include "narb_config.h"
#include "narb_apiserver.h"
#include "narb_rceapi.h"

/* List of all active app->narb request connections. */
list narb_apiserver_list;

/* Common server port, on which narb_server listen to and accept incoming 
  app connections. It can be set to aother number using '-p'  in command line.*/
unsigned int NARB_API_SYNC_PORT = 2609;

int flag_holdon = 1;

/*By default, NARB should return all-strict-hop ERO upon routing request,
    The default routing mode is reconfigurable by NARB CLI.*/
int flag_routing_mode = RT_MODE_ALL_STRICT_ONLY;
int single_domain_mode = 0;

/* Init narb server: allocate initial data structures and start a
 * server socket to listen to and accpet app connection requests. */
int
narb_apiserver_init(void)
{
  union sockunion su;
  unsigned int port;
  int accept_sock;
  int rc = -1;

  /* Create new socket for synchronous messages. */
  memset (&su, 0, sizeof (union sockunion));
  su.sa.sa_family = AF_INET;

  /* Make new socket */
  accept_sock = sockunion_stream_socket (&su);
  if (accept_sock < 0)
    return accept_sock;

  /* This is a server, so reuse address and port */
  sockopt_reuseaddr (accept_sock);

  /* Bind socket to address and given port. */
  port = narb_apiserver_getport();
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
      zlog_warn ("narb_apiserver_init: listen: %s",
                 strerror (errno));
      close (accept_sock);	/* Close socket */
      return rc;
    }

  /* Schedule new thread that handles accepted connections. */
  narb_apiserver_event (NARB_APISERVER_ACCEPT, accept_sock, NULL);

  /* Initialize list that keeps track of all connections. */
  narb_apiserver_list = list_new ();

  return rc;
}


/*return the narb_server port, an obsolete function.*/
unsigned short
narb_apiserver_getport (void)
{
  struct servent *sp = getservbyname ("narb", "tcp");

  return sp ? ntohs (sp->s_port) : NARB_API_SYNC_PORT;
}


/*********************************************************
       Functions for NARB-APP communications
 *********************************************************/

/* Accept connection request from ASTDL applications. For each
   accepted connection, a unique instance of narb_apiserver 
   is created to handle requests/response for that application. */
int
narb_apiserver_accept (struct thread *thread)
{
  int accept_sock;
  int new_sync_sock;
  union sockunion su;
  struct narb_apiserver *narb_apiserv;
  struct sockaddr_in peer_sync;
  int peerlen;
  int ret;

  accept_sock = THREAD_FD (thread);

  /* Keep hearing on socket for further connections. */
  narb_apiserver_event (NARB_APISERVER_ACCEPT, accept_sock, NULL);

  memset (&su, 0, sizeof (union sockunion));

  /* Accept connection for synchronous messages */
  new_sync_sock = sockunion_accept (accept_sock, &su);
  if (new_sync_sock < 0)
    {
      zlog_warn ("narb_apiserver_accept: accept: %s", strerror (errno));
      return -1;
    }

  memset(&peer_sync, 0, sizeof(struct sockaddr_in));
  peerlen = sizeof (struct sockaddr_in);
  ret = getpeername (new_sync_sock, (struct sockaddr *)&peer_sync, &peerlen);
  if (ret < 0)
    {
      zlog_warn ("narb_apiserver_accept: getpeername: %s", strerror (errno));
      close (new_sync_sock);
      return -1;
    }
  
  /* Allocate new server-side connection structure */
  narb_apiserv = narb_apiserver_new (new_sync_sock);

  /* Add to active connection list */
  listnode_add (narb_apiserver_list, narb_apiserv);

  narb_apiserv->peer_sync = peer_sync;
  
  /* And add read threads for new connection */
  narb_apiserver_event (NARB_APISERVER_SYNC_READ, new_sync_sock, narb_apiserv);

  zlog_warn ("NARB_APISERVER: New narb_apiserv(%p), total#(%d)", narb_apiserv, narb_apiserver_list->count);

  return 0;
}

/* Create a new narb_apiserver. Memory allocated within function. */
struct narb_apiserver *
narb_apiserver_new (int fd_sync)
{
  static int id = 1;
  struct narb_apiserver *new =
    XMALLOC (MTYPE_NARB_APISERVER, sizeof (struct narb_apiserver));

  new->apiserver_id = id++;
  
  new->fd_sync = fd_sync;

  /* Initialize storage for tracking the ERO's sent to applications*/
  new->app_req_list = list_new();

  /* Initialize fifo for outgoing narb->app messages  */
  new->out_sync_fifo = msg_fifo_new ();
  new->t_sync_read = NULL;
  new->t_sync_write = NULL;
  new->routing_mode = flag_routing_mode;

  return new;
}

/* Read incoming app->narb messages*/
int
narb_apiserver_read (struct thread *thread)
{
  struct narb_apiserver *narb_apiserv;
  struct msg *msg;
  int fd;
  int rc = -1;
  enum narb_apiserver_event event;

  narb_apiserv = THREAD_ARG (thread);
  fd = THREAD_FD (thread);

  if (fd ==   narb_apiserv->fd_sync)
    {
      event = NARB_APISERVER_SYNC_READ;
        narb_apiserv->t_sync_read = NULL;
    }
  else
    {
      zlog_warn ("  narb_apiserver_read: Unknown fd(%d)", fd);
      listnode_delete(narb_apiserver_list, narb_apiserv);
      narb_apiserver_free (narb_apiserv);
      goto out;
    }

  /* Read message from fd. */
  msg = msg_read (fd);
  if (msg == NULL)
    {
      zlog_warn
	("narb_apiserver_read: read failed on fd=%d, closing connection", fd);

      /* Perform cleanup. */
      listnode_delete (narb_apiserver_list, narb_apiserv);
      narb_apiserver_free (narb_apiserv);
      goto out;
    }

  narb_apiserver_msg_print (msg);

  /* Dispatch to corresponding message handler. */
  rc = narb_apiserver_handle_msg (narb_apiserv, msg);

  /* Prepare for next message, add read thread. */
  narb_apiserver_event (event, fd, narb_apiserv);

  /*free the memory allocated in msg_read */
  msg_free (msg);

out:
  return rc;
}

/*Write outgoing narb->app messages.
  narb<->app communications is synchronous.*/
int
narb_apiserver_sync_write (struct thread *thread)
{
  struct narb_apiserver *narb_apiserv;
  struct msg *msg;
  int fd;
  int rc = -1;

  narb_apiserv = THREAD_ARG (thread);
  assert (narb_apiserv);
  fd = THREAD_FD (thread);

  narb_apiserv->t_sync_write = NULL;

  /* Sanity check */
  if (fd != narb_apiserv->fd_sync)
    {
      zlog_warn ("narb_apiserver_sync_write: Unknown fd=%d", fd);
      goto out;
    }

  /* Check whether there is really a message in the fifo. */
  msg = msg_fifo_pop (narb_apiserv->out_sync_fifo);
  if (!msg)
    {
      zlog_warn ("NARB_APISERVER: narb_apiserver_sync_write: No message in Sync-FIFO?");
      return 0;
    }

  narb_apiserver_msg_print (msg);

  rc = msg_write (fd, msg);

  /* Once a message is dequeued, it should be freed anyway. */
  msg_free (msg);

  if (rc < 0)
    {
      zlog_warn
        ("narb_apiserver_sync_write: write failed on fd=%d", fd);
      goto out;
    }


  /* If more messages are in sync message fifo, schedule write thread. */
  if (msg_fifo_head (narb_apiserv->out_sync_fifo))
    {
      narb_apiserver_event (NARB_APISERVER_SYNC_WRITE, narb_apiserv->fd_sync,
                            narb_apiserv);
    }
  
 out:

  if (rc < 0)
  {
      /* Perform cleanup and disconnect with peer */
      listnode_delete(narb_apiserver_list, narb_apiserv);
      narb_apiserver_free (narb_apiserv);
    }

  return rc;
}


/* Scheduling a thread to handle a narb server event */
void
narb_apiserver_event (enum narb_apiserver_event event, int fd,
		      struct narb_apiserver *narb_apiserv)
{
  struct thread *apiserver_serv_thread;

  switch (event)
    {
    case NARB_APISERVER_ACCEPT:
      apiserver_serv_thread =
	thread_add_read (master, narb_apiserver_accept, narb_apiserv, fd);
      break;
    case NARB_APISERVER_SYNC_READ:
      narb_apiserv->t_sync_read =
	thread_add_read (master, narb_apiserver_read, narb_apiserv, fd);
      break;
    case NARB_APISERVER_SYNC_WRITE:
      if (!narb_apiserv->t_sync_write)
        {
          narb_apiserv->t_sync_write =
          thread_add_write (master, narb_apiserver_sync_write, narb_apiserv, fd);
        }
      break;
    }
}


/* Dispatching a narb->app message to corresponding function accoring to its type. */
int
narb_apiserver_handle_msg (struct narb_apiserver *apiserv, struct msg *msg)
{
  int rc;

  switch (msg->hdr.msgtype)
    {
    case MSG_APP_REQUEST_EVENT:
      rc = narb_apiserver_handle_app_request_event (apiserv, msg);
      break;
    case MSG_APP_REMOVE_EVENT:
      rc = narb_apiserver_handle_app_remove_event (apiserv, msg);
      break;
    case MSG_APP_CONFIRM_EVENT:
      rc = narb_apiserver_handle_app_confirm_event (apiserv, msg);
      break;
    default:
      zlog_warn ("narb_apiserver_handle_msg: Unknown message type: %d",
		 msg->hdr.msgtype);
      rc = -1;
    }
  return rc;
}


struct msg_narb_cspf_reply *
narb_rceapi_query_lsp (int *p_sock, u_int32_t options, u_int32_t ucid, u_int32_t seqnum, struct msg_narb_cspf_request *cspf_req)
{
  struct rce_api_msg *rce_msg;
  struct msg_narb_cspf_reply * rmsg = NULL;
  int sock;
  
  assert(p_sock);
  sock = *p_sock;
  rce_msg = rce_api_msg_new(MSG_LSP, ACT_QUERY, &cspf_req->app_req_data, ucid, seqnum, ntohs(sizeof(cspf_req->app_req_data)));
  rce_msg->hdr.msgtag[0] = options | LSP_TLV_NARB_REQ | LSP_OPT_MRN | LSP_OPT_BIDIRECTIONAL;
  
  if (narb_rceapi_send(sock, rce_msg) < 0)
  {
    zlog_err("Send MSG to RCE failed\n");
    *p_sock = -1;
    return NULL;
  }

  rce_msg = narb_rceapi_read(sock);

  if (!rce_msg)
  {
    zlog_err("Read MSG from RCE failed\n");
    *p_sock = -1;
    return NULL;
  }
  else
  {
      rmsg = XMALLOC(MTYPE_TMP, sizeof(struct msg_narb_cspf_reply) + ntohs(rce_msg->hdr.msglen) - TLV_HDR_SIZE);
      rmsg->app_seqnum = cspf_req->app_seqnum; 
      rmsg->src.s_addr = cspf_req->app_req_data.src.s_addr;
      rmsg->dest.s_addr = cspf_req->app_req_data.dest.s_addr;
      rmsg->narb_apiserv_id = cspf_req->narb_apiserv_id;
      memcpy ((char*)&rmsg->tlv, rce_msg->body, ntohs(rce_msg->hdr.msglen));
  }

  XFREE(MTYPE_TMP, cspf_req);
  return rmsg;
}

/* Function handling the app->narb route reqeust */
int
narb_apiserver_handle_app_request_event (struct narb_apiserver *apiserv,
                struct msg *msg)
{
  struct app_req_super_data *superdata;
  struct msg_app2narb_request * app_req;
  
  int length;
  void * tlv_data;
  struct msg * rmsg;
  u_int32_t app_seqnum = msg->hdr.msgseq;
  struct in_addr *p_addr;

  app_req = (struct msg_app2narb_request *)STREAM_DATA(msg->s);

  zlog_info ("narb_apiserver_handle_app_request_event: called");

  /* check if the request has already existed in the app_req_list of this narb_apiserver*/
  superdata = app_req_list_lookup (apiserv->app_req_list, app_seqnum);
  
  /* if not, construct super data for the new request. */
  if (!superdata)
    {
      superdata = XMALLOC(MTYPE_TMP, sizeof(struct app_req_super_data));
      memset(superdata, 0, sizeof(struct app_req_super_data));

      app_seqnum = superdata->app_seqnum = ntohl(msg->hdr.msgseq);
      
      superdata->src = app_req->src;
      if ((p_addr = narb_lookup_router_by_if_addr(&app_req->src, NULL)) != NULL)
        {
          app_req->src = superdata->src = *p_addr;
        }

      superdata->dest = app_req->dest;
      if ((p_addr = narb_lookup_router_by_if_addr(NULL, &app_req->dest)) != NULL)
        {
          app_req->dest = superdata->dest = *p_addr;
        }

      superdata->encoding_type = app_req->encoding_type;
      superdata->switching_type = app_req->switching_type;
      superdata->bandwidth = app_req->bandwidth;

      /*this is a brandnew request or a retransmission but without ERO available*/
      superdata->state = APP_REQ_STATE_REQ;
      /* neigher ero_inter nor ero_intra has been obtained from ospfd's*/
      superdata->ero_state = ERO_NONE;
      
      listnode_add(apiserv->app_req_list, superdata);
    }

  if (superdata->src.s_addr ==  superdata->dest.s_addr)
    {
      narb_apiserver_error_reply(apiserv, app_seqnum, NARB_ERROR_NO_DEST);
      return -1;
    }

  if (flag_holdon)
    {
      narb_apiserver_error_reply(apiserv, app_seqnum, NARB_ERROR_JUST_HOLDON);
      return -1;
    }

  switch (superdata->state)
    {
    case APP_REQ_STATE_REQ:
      if (superdata->req_retran_counter == 0) /*new request*/
        {
          if (oclient_inter && oclient_inter->neighbor_count > 0 && !single_domain_mode)
          {
          /* Check if source ip is in domain_summary_info->router_ids. */
          listnode node = listhead(narb_domain_info.router_ids);
          while (node)
            {
              if (((struct router_id_info *)node->data)->id.s_addr == superdata->src.s_addr)
                break;
              nextnode(node);
            }

          /* If source ip is not in domain_summary_info->router_ids or not in narb->lsdb*/
          if (!node || !narb_lsdb_router_lookup_by_addr(oclient_inter->lsdb, superdata->src)) 
            {
              narb_apiserver_error_reply(apiserv, app_seqnum, NARB_ERROR_NO_SRC);
            }
          else
            {
              /* Check if destination ip is in domain_summary_info->router_ids. */
              node = listhead(narb_domain_info.router_ids);
              while (node)
                {
                  if (((struct router_id_info *)node->data)->id.s_addr == superdata->dest.s_addr)
                    break;
                  nextnode(node);
                }
              
               /* If destination ip is not in domain_summary_info->router_ids */
              if (!node)
                {
                    /*Check if the destination is in the 'big' network with external domains*/
                    if (!narb_lsdb_router_lookup_by_addr(oclient_inter->lsdb, superdata->dest))
                      { /*if destination is not in any external domain. Send back error message.*/
                        narb_apiserver_error_reply(apiserv, app_seqnum, NARB_ERROR_NO_DEST);  
                      }
                    else 
                      {/* otherwise destination does exist in external doamin. Send an inter-domain request */
                        narb_apiserver_send_cspf_request (oclient_inter, apiserv->apiserver_id, 
                            app_seqnum, app_req);
                      }
                }
              else /* If both src and dest are in domain_summary_info->router_ids*/
                {
                  /* still send an intER-domain request fist, which will derive an intRA-domain request later*/
                  narb_apiserver_send_cspf_request (oclient_inter, apiserv->apiserver_id, 
                        app_seqnum, app_req);
                }
            }
          }
          else if (oclient_intra)
          {
            narb_apiserver_send_cspf_request (oclient_intra, apiserv->apiserver_id, app_seqnum, app_req);
          }
        }
      else   /* this request is a re-transmission*/
        {
          if (superdata->req_retran_counter > MAX_REQ_RETRAN)
            {
              /* if having been retransmitted two many times, return an error msg.*/
              rmsg = narb_new_msg_reply_error(app_seqnum, NARB_ERROR_EXCEED_MAX_RETRAN);
              msg_fifo_push (apiserv->out_sync_fifo, rmsg);
              narb_apiserver_event(NARB_APISERVER_SYNC_WRITE, apiserv->fd_sync, apiserv);
            }
          else   
            {
              /* Repeat CSPF request. Note that if the repeated CSPF request results in 
                  repeated reply. The returned inter-domain ERO will be re-merged with intra-domain ERO. */
             if (oclient_inter && oclient_inter->neighbor_count > 0 && !single_domain_mode)
                narb_apiserver_send_cspf_request (oclient_inter, apiserv->apiserver_id, app_seqnum, app_req);
              else if (oclient_intra)
                narb_apiserver_send_cspf_request (oclient_intra, apiserv->apiserver_id, app_seqnum, app_req);
            }
        }
      superdata->req_retran_counter++;
      break;
      
    case APP_REQ_STATE_ERO_VALID:
      /* This request has been received before and ERO has been constructed.
          We simply re-send a ERO reply */
      if (superdata->req_retran_counter > MAX_REQ_RETRAN)
        {
          rmsg = narb_new_msg_reply_error(app_seqnum, NARB_ERROR_EXCEED_MAX_RETRAN);
        }
      else  if ((!superdata->ero_inter ||!superdata->ero_inter->count) && 
                      (!superdata->ero_intra ||!superdata->ero_intra->count))
        {
          rmsg = narb_new_msg_reply_error(app_seqnum, NARB_ERROR_NO_ROUTE);
        }
      else
        {
          list merged_ero = narb_merge_ero(superdata->ero_inter, superdata->ero_intra);
          tlv_data = narb_apiserver_construct_ero_tlv(&length, merged_ero);
          ero_free(merged_ero);
          rmsg = narb_new_msg_reply_ero(app_seqnum, tlv_data, length);
        }

      msg_fifo_push (apiserv->out_sync_fifo, rmsg);
      narb_apiserver_event(NARB_APISERVER_SYNC_WRITE, apiserv->fd_sync, apiserv);
      
      superdata->req_retran_counter++;
      break;

    case APP_REQ_STATE_CONFIRMED:
        /*Do nothing, simply discard the request */
        ;
      break;
    default:

      zlog_warn("Invalid app. req.f state [%d]", superdata->state);
    }
  
  return 0;
}

/* constructing an ERO message from the ero list and replying to app client. */
void
narb_apiserver_ero_reply (struct narb_apiserver *apiserv, u_int32_t app_seqnum, list ero)
{
  int length;
  void * tlv_data;
  struct msg * rmsg;
    
  tlv_data = narb_apiserver_construct_ero_tlv(&length, ero);
  /*ero_free(ero); */
  rmsg = narb_new_msg_reply_ero(app_seqnum, tlv_data, length);

  if (!rmsg)
    {
      rmsg = narb_new_msg_reply_error(app_seqnum, NARB_ERROR_NO_ROUTE);
    }

  msg_fifo_push (apiserv->out_sync_fifo, rmsg);
  narb_apiserver_event(NARB_APISERVER_SYNC_WRITE, apiserv->fd_sync, apiserv);
}

/* constructing an narb_error message from the ero list and replying to app client. */
void
narb_apiserver_error_reply (struct narb_apiserver *apiserv, 
          u_int32_t app_seqnum, u_int32_t errcode)
{
  struct msg * rmsg;                  
  rmsg = narb_new_msg_reply_error(app_seqnum, errcode);
  msg_fifo_push (apiserv->out_sync_fifo, rmsg);
  narb_apiserver_event(NARB_APISERVER_SYNC_WRITE, apiserv->fd_sync, apiserv);
}

/* function handling app->narb confirmation message, which confirms 
    that a RSVP path has been set up*/
int
narb_apiserver_handle_app_confirm_event (struct narb_apiserver *apiserv,
                struct msg * msg)
{
  int ret;
  struct app_req_super_data *app_req_superdata;
  listnode ero_node;
  struct link_info *link;
  struct ero_subobj *subobj;
  struct ero_subobj *subobj_next;
  u_int32_t app_seqnum = ntohl(msg->hdr.msgseq);

  zlog_info ("narb_apiserver_handle_app_confirm_event: called");
  zlog_info ("Route has been set up...\n");

  if (!oclient_inter)
    return -1;

  /*check if this request is still maintained in app_req_list of this narb_apiserver*/
  if (!(app_req_superdata = app_req_list_lookup(apiserv->app_req_list, app_seqnum)))
    {
      zlog_warn ("narb_apiserver_handle_app_confirm_event: \
The app request (seqnum = %d) no longer exists.", app_seqnum);
      return -1;
    }
  
  /*Change superdata->state into CONFIRMED*/
  app_req_superdata->state = APP_REQ_STATE_CONFIRMED;

  /*TODO: $$$$ update TE-LSDB database in NARB */
  /*TODO: $$$$ originate TE-LSA's (Updates) to iner-domain control plane*/
  /*$$$$ temp code, for lamdba switching only. In lamdba switching we delete 
      the Opaque Link TE LSA's corresponding to subobjects on the ero_inter list*/
  if (!app_req_superdata->ero_inter)
    return -1;
  
  ero_node = listhead(app_req_superdata->ero_inter);
  while (ero_node)
    {
      subobj = (struct ero_subobj *)ero_node->data;
      if (ero_node->next)
        {
          subobj_next = (struct ero_subobj *)ero_node->next->data;

      	  link = narb_lookup_link_by_if_addr(narb_domain_info.te_links, 
                &subobj->addr, &subobj_next->addr);
          /*
          lsa = narb_lookup_lsa_by_if_addr (oclient_inter->lsdb, 
                  &subobj->addr, &subobj_next->addr);
          */
        }
      else
        link = NULL;
        /*lsa = NULL;*/

      nextnode(ero_node);

      /*if (!lsa)*/
      if (!link)
        continue;

      /* delete the LSA */
      ret = ospf_apiclient_lsa_delete (oclient_inter,
                      link->adv_id,
                      narb_domain_info.ospfd_inter.area, 
                      10, 1, link->opaque_id);

      zlog_info ("LINK TE LSA [%d] deleted through OSPFd at %s: \
ID = %X, ADV_ROUTER = %X: return code is %d.",
         link->adv_id, narb_domain_info.ospfd_inter.addr, 
         link->opaque_id, link->adv_id, ret);

      link->hide = 1;
    }

  /*$$$$*/
  /* Should forward the confirmation if loose hop leads to a peering narb, which is alive */
  
  /*Sending conrimation message to next-domain NARB (recursive)*/
  /* connect to the next-domain narb */
  if (!app_req_superdata->rec_narb || app_req_superdata->rec_narb_fd <= 0) 
  {
    zlog_warn("No path confirmation relayed to next-domain NARB." );
  }
  else
  {
    /* relaying confirmation message */
    msg_write(app_req_superdata->rec_narb_fd, msg);
    narb_apiserver_msg_print (msg);
  }
  return 0;
}

/* function handling app->narb removal message, which notifies 
    that a RSVP path has been torn down*/
int
narb_apiserver_handle_app_remove_event (struct narb_apiserver *apiserv,
                struct msg *msg)
{
  int ret = -1;
  struct app_req_super_data *app_req_superdata;
  struct msg * rmsg;
  listnode ero_node;
  struct ero_subobj *subobj;
  struct ero_subobj *subobj_next;
  struct link_info *link;
  u_int32_t app_seqnum = ntohl(msg->hdr.msgseq);

  if (!oclient_inter)
    return -1;

  zlog_info ("narb_apiserver_handle_app_remove_event: called");
  zlog_info ("Route removed...A deletion confirmation has been sent...\n");

  if (!(app_req_superdata = app_req_list_lookup(apiserv->app_req_list, app_seqnum)))
    {
      zlog_warn ("narb_apiserver_handle_app_remove_event: \
The app request (seqnum = %d) no longer exists.", app_seqnum);
      return -1;
    }
  
  /*TODO: $$$$ update TE database */
  /*TODO: $$$$ originate TE-LSA's (Updates)*/
  /*$$$$ temp code, for lamdba switching only. In lamdba switching we re-originate 
      the Opaque Link TE LSA's on the ero_inter list*/
  if (!app_req_superdata->ero_inter)
    return -1;
  
  ero_node = listhead(app_req_superdata->ero_inter);
  while (ero_node)
    {
      subobj = (struct ero_subobj *)ero_node->data;
      if (ero_node->next)
        {
          subobj_next = (struct ero_subobj *)ero_node->next->data;

      	  link = narb_lookup_link_by_if_addr(narb_domain_info.te_links, 
             &subobj->addr, &subobj_next->addr);
        }
      else
        link = NULL;

      nextnode(ero_node);

      if (!link)
        continue;

      if(link->hide == 0)
        continue;

      link->hide = 0; 
      ret = narb_originate_te_link(oclient_inter, link);
    }

  /* reply to app client to confirm that a confirmation message has been received */
  rmsg = narb_new_msg_reply_remove_confirm (app_seqnum);
  assert (rmsg);
  msg_fifo_push (apiserv->out_sync_fifo, rmsg);
  narb_apiserver_event(NARB_APISERVER_SYNC_WRITE, apiserv->fd_sync, apiserv);

  /*Remove app request and related data from the app_req_list*/
  if (app_req_superdata->ero_inter)
    {
      ero_free(app_req_superdata->ero_inter);
      app_req_superdata->ero_inter = NULL;
    }
  if (app_req_superdata->ero_intra)
    {
      ero_free(app_req_superdata->ero_intra);
      app_req_superdata->ero_intra = NULL;
    }
  
  listnode_delete(apiserv->app_req_list, app_req_superdata);

  /*Sending removal message to next-domain NARB (recursive)*/

  /* connect to the next-domain narb */
  if (!app_req_superdata->rec_narb || app_req_superdata->rec_narb_fd <= 0)
  {
    zlog_warn("No path removal relayed to next-domain NARB.");
  }
  else
  {
    /* relaying confirmation message */
    msg_write(app_req_superdata->rec_narb_fd, msg);
    narb_apiserver_msg_print (msg);
    close(app_req_superdata->rec_narb_fd);
  }

  XFREE(MTYPE_TMP, app_req_superdata);

  return 0;
}

/* Auxiliary function constructing ERO tlv from an ero list */
void *
narb_apiserver_construct_ero_tlv (int *len,  list ero)
{
  listnode node;
  struct te_tlv_header * tlv;
  int length;
  int offset;
  
  int subobj_size = sizeof(struct ipv4_prefix_subobj);

/* just temp code for testing ...
  struct ero_subobj * subobj = XMALLOC(MTYPE_TMP, sizeof(struct ero_subobj));
  inet_aton("140.173.4.217", &subobj->addr);
  subobj->hop_type = ERO_TYPE_STRICT_HOP;
  subobj->prefix_len = 32;
  list_add_node_prev(ero, ero->head, subobj);

  subobj = XMALLOC(MTYPE_TMP, sizeof(struct ero_subobj));
  inet_aton("140.173.4.218", &subobj->addr);
  subobj->hop_type = ERO_TYPE_STRICT_HOP;
  subobj->prefix_len = 32;
  list_add_node_prev(ero, ero->head, subobj);
*/

  assert(ero);
  
  if (!ero->count)
    return NULL;

  /*make header*/
  length =  sizeof (struct te_tlv_header) + ero->count * subobj_size;
  tlv = XMALLOC(MTYPE_TMP, length);
  tlv->length = htons(length - sizeof (struct te_tlv_header));
  tlv->type = htons(TLV_TYPE_NARB_ERO);

  /*make body*/
  offset = sizeof (struct te_tlv_header);
  for (node = listhead(ero); node; nextnode(node), offset+=subobj_size )
    {
      struct ipv4_prefix_subobj * subobj = 
                      (struct ipv4_prefix_subobj *)((char *)tlv + offset);
      struct ero_subobj * ero_subobj = node->data;
      subobj->l_and_type = L_AND_TYPE(ero_subobj->hop_type, 0x01);
      memcpy(subobj->addr, &ero_subobj->addr, 4);
      subobj->length = sizeof(struct ipv4_prefix_subobj);
      subobj->prefix_len = 32;
      subobj->resvd = 0;
    }

  *len = length;
  return (void *)tlv;
}

/* creating a narb MSG_REPLY_ERO message using the ERO tlv as message body*/
struct msg *
narb_new_msg_reply_ero (u_int32_t seqnr, void * ero_tlv, int length)
{
  struct msg *msg;

  if (length ==0 || !ero_tlv)
     return NULL;
  
  msg = msg_new (MSG_REPLY_ERO, ero_tlv, seqnr, length);
  XFREE(MTYPE_TMP, ero_tlv);
  
  return msg;
}

/* creating a narb MSG_REPLY_REMOVE_CONFIRM message*/
struct msg *
narb_new_msg_reply_remove_confirm (u_int32_t seqnr)
{
  struct msg *msg;

  msg = msg_new (MSG_REPLY_REMOVE_CONFIRM, NULL, seqnr, 0);

  return msg;
}

/* creating a narb TLV_TYPE_NARB_ERROR_CODE message*/
/* note that error code is 16 bits long */
struct msg *
narb_new_msg_reply_error (u_int32_t seqnr, u_int16_t errorcode)
{
  struct msg *msg;

  struct te_tlv_header * tlv = XMALLOC(MTYPE_TMP, 8);
  tlv->type = htons(TLV_TYPE_NARB_ERROR_CODE); 
  tlv->length = htons(8);
  * (u_int16_t *)((char *)tlv + 4) = htons(errorcode);
  * (u_int16_t *)((char *)tlv + 6) = 0;
  
  msg = msg_new (MSG_REPLY_ERROR, tlv, seqnr, 8);
  XFREE(MTYPE_TMP, tlv);

  return msg;
}

/* free an ero list */
void 
ero_free(list ero)
{
  listnode node;

  for (node = listhead (ero); node; nextnode(node))
    {
      XFREE(MTYPE_ERO, node->data);
    }
  list_delete(ero);
}

/* Free an instance of narb server. */
void
narb_apiserver_free (struct narb_apiserver *apiserv)
{
  listnode node;

  /* Cancel read and write threads. */
  if (apiserv->t_sync_read)
    {
      thread_cancel (apiserv->t_sync_read);
    }
  if (apiserv->t_sync_write)
    {
      thread_cancel (apiserv->t_sync_write);
    }
  
  /* Close connection. */
  if (apiserv->fd_sync > 0)
    {
      close (apiserv->fd_sync);
    }
  
  /* Delete all super data */
  for (node = listhead(apiserv->app_req_list); node; nextnode(node))
    {
      if (((struct app_req_super_data *)node->data)->ero_inter)
      {
        ero_free(((struct app_req_super_data *)node->data)->ero_inter);
      }
      if (((struct app_req_super_data *)node->data)->ero_intra)
      {
        ero_free(((struct app_req_super_data *)node->data)->ero_intra);
      }

      if(((struct app_req_super_data *)node->data)->rec_narb_fd > 0)
        close(((struct app_req_super_data *)node->data)->rec_narb_fd);

      XFREE(MTYPE_TMP, node->data);
    }
  
  list_delete(apiserv->app_req_list);
  
  /* Free fifos */
  msg_fifo_flush(apiserv->out_sync_fifo);
  msg_fifo_free (apiserv->out_sync_fifo);

  /* Remove from the list of active clients. */
  listnode_delete (narb_apiserver_list, apiserv);

  zlog_info ("NARB_APISERVER: Delete narb_apiserv(%p), total#(%d)", apiserv, narb_apiserver_list->count);

  /* And free instance. */
  XFREE (MTYPE_NARB_APISERVER, apiserv);
}

/*free all narb_apiserver instances on narb_apiserver_list */
void
narb_apiserver_free_all (void)
{
  listnode node;
  int i = 1;
  for (node = listhead(narb_apiserver_list); node; nextnode (node))
    {
      if (i++ > narb_apiserver_list->count)
        return;
      narb_apiserver_free (node->data);
    }
  list_delete(narb_apiserver_list);
  zlog_info("NARB is down. Good bye!");
  zlog_info("...Please wait for five seconds before restart so that OSPFd can finish cleanning up ...");
}

/* searching for a request data record on app_req_list using msg sequence number*/
struct app_req_super_data * 
app_req_list_lookup (list app_req_list, u_int32_t seqnum)
{
  listnode node;

  assert (app_req_list);
  
  for (node = listhead(app_req_list); node; nextnode(node))
    {
      if (((struct app_req_super_data *)node->data)->app_seqnum == seqnum)
        return (struct app_req_super_data *)node->data;
    }
  return NULL;
}

/* printing the narb<->app messages */
void
narb_apiserver_msg_print (struct msg *msg)
{
  if (!msg)
    {
      zlog_warn ("narb_apiserver: msg=NULL!\n");
      return;
    }

  zlog_info
    ("API-msg [%s]: type(%d),len(%d),seq(%lu),data(%p),size(%lu)",
     narb_apiserver_typename (msg->hdr.msgtype), msg->hdr.msgtype, 
     ntohs (msg->hdr.msglen), (unsigned long) ntohl (msg->hdr.msgseq),
     STREAM_DATA (msg->s), STREAM_SIZE (msg->s));

  return;
}

/* work with narb_apiserver_msg_print */
const char *
narb_apiserver_typename (int msgtype)
{
  struct nametab { 
    int value;
    const char *name;} NameTab[] = {
    { MSG_APP_REQUEST_EVENT,   "MSG_APP_REQUEST",   },
    { MSG_APP_REMOVE_EVENT, "MSG_APP_REMOVE", },
    { MSG_APP_CONFIRM_EVENT,        "MSG_APP_CONFIRM",         },
    { MSG_REPLY_ERO,             "MSG_REPLY_ERO",              },
    { MSG_REPLY_ERROR,     "MSG_REPLY_ERROR",      },
    { MSG_REPLY_REMOVE_CONFIRM,     "MSG_REPLY_REMOVE_CONFIRM",      },
    { MSG_NARB_CSPF_REQUEST,     "MSG_NARB_CSPF_REQUEST",      },
    { MSG_NARB_CSPF_REPLY,     "MSG_NARB_CSPF_REQUEST",      },
  };

  int i, n = sizeof (NameTab) / sizeof (NameTab[0]);
  const char *name = NULL;

  for (i = 0; i < n; i++)
    {
      if (NameTab[i].value == msgtype)
        {
          name = NameTab[i].name;
          break;
        }
    }

  return name ? name : "?UNKNOWN_MSG_TYPE?";
}


/*********************************************************
       Functions for NARB-OSPFd communications
 *********************************************************/

/* options for IntER-domain CSPF == LSP_OPT_LOOSE_ONLY & LSP_TLV_NARB_REQ
 * options for IntRA-domain CSPF == LSP_OPT_STRICT_ONLY & LSP_TLV_NARB_REQ
 */
struct msg_narb_cspf_reply *
narb_ospf_cspf_request (int fd, u_int32_t seqnum, struct msg_narb_cspf_request *cspf_req)
{
  struct msg *msg;
  int rc;
  
  msg = msg_new (MSG_NARB_CSPF_REQUEST, cspf_req, seqnum, sizeof(struct msg_narb_cspf_request));
  XFREE(MTYPE_TMP, cspf_req);

  if (!msg)
    {
      zlog_warn ("NARB_APISERVER: narb_cspf_req_write: No message in Sync-FIFO?");
      return NULL;
    }

  narb_apiserver_msg_print (msg);

  /*write cspf request message*/
  rc = msg_write (fd, msg);
  msg_free (msg);

  if (rc < 0)
    {
      zlog_warn
        ("narb_cspf_req_write: write failed on fd=%d", fd);
      return NULL;
    }

  /* Synchronous TCP socket write and read. Wait for reply */
  msg = msg_read (fd);
  if (!msg)
    return NULL;

  assert (msg->hdr.msgtype == MSG_NARB_CSPF_REPLY);
  assert (ntohl (msg->hdr.msgseq) == seqnum);

  return (struct msg_narb_cspf_reply *) STREAM_DATA (msg->s);
}


/* constructing a CSPF request and scheduling a write thread (on ospf_sock) to
    send this request. The CSPF request is uniquely identified by the combination of 
    narb_apiserver_id and app_seqnum 
    Note that this function can send requests to both OSPFd-inter and OSPFd-intra
    depending on which  ospf_apiclient (ospfd_inter or ospfd_intra) is used. */
int
narb_apiserver_send_cspf_request (struct ospf_apiclient * oc, int narb_apiserv_id, 
                u_int32_t app_seqnum, void *data)
{
  int rc = -1;
  u_int32_t seqnum;
  struct msg_narb_cspf_request * cspf_req = XMALLOC(MTYPE_TMP, sizeof (struct msg_narb_cspf_request));
  struct msg_narb_cspf_reply * rmsg;
 
  /* constructing CSPF request msg */
  cspf_req->narb_apiserv_id = htonl(narb_apiserv_id);
  cspf_req->app_seqnum = htonl(app_seqnum);
  
  /* messages to OSPFd_intra and OSPFd_inter use different area id*/
  if (oc == oclient_inter)
    cspf_req->area_id = narb_domain_info.ospfd_inter.area;
  else
    cspf_req->area_id = narb_domain_info.ospfd_intra.area;
  cspf_req->app_req_data = *(struct msg_app2narb_request *)data;
  
  seqnum = ospf_apiclient_get_seqnr();


  if (RCE_HOST_ADDR == NULL)
    {
      rmsg = narb_ospf_cspf_request (oc->fd_sync, seqnum, cspf_req);
    }
  else
    {
      if (rce_api_sock < 0)
        {
      	rce_api_sock = narb_rceapi_connect(RCE_HOST_ADDR, RCE_API_PORT);
            if (rce_api_sock < 0)
            {
                zlog_err("Cannot connect to RCE\n");
                return -1;
            }
        }
      rmsg = narb_rceapi_query_lsp (&rce_api_sock, 
        ((oc == oclient_inter) ? LSP_OPT_LOOSE_ONLY : LSP_OPT_STRICT_ONLY), narb_apiserv_id, seqnum, cspf_req);
      if (!rmsg) /* retry to ensure to reconnect if pipe is broken*/
        {
          if (rce_api_sock < 0)
            {
          	rce_api_sock = narb_rceapi_connect(RCE_HOST_ADDR, RCE_API_PORT);
                if (rce_api_sock < 0)
                {
                    zlog_err("Cannot connect to RCE\n");
                    return -1;
                }
            }
          rmsg = narb_rceapi_query_lsp (&rce_api_sock, 
            (oc == oclient_inter ? LSP_OPT_LOOSE_ONLY : LSP_OPT_STRICT_ONLY), narb_apiserv_id, seqnum, cspf_req);
        }      
    }
  
  /*we handle the reply from OSPFd_inter and OSPFd_intra using separate 
    functions*/
  if (!rmsg)
    {
      return rc;
    }

  if (oc == oclient_inter)
    rc = narb_oclient_handle_cspf_reply_inter (rmsg);
  else
    rc = narb_oclient_handle_cspf_reply_intra (rmsg);

  return rc;
}

/*searching for a instance of narb_apiserver from the narb_apiserver_list
  using the unique narb_apiserver_id*/
struct narb_apiserver *
narb_apiserver_list_lookup(u_int32_t id)
{
  listnode node;

  node = listhead(narb_apiserver_list);
  while (node)
    {
      if (((struct narb_apiserver *)node->data)->apiserver_id == id)
        return (struct narb_apiserver *)node->data;
      nextnode(node);
    }
  return NULL;
}

  /*handling the CSPF reply message from OSPFd_inter */
int 
narb_oclient_handle_cspf_reply_inter (struct msg_narb_cspf_reply * msg_reply)
{
  list ero_list;
  struct narb_apiserver *apiserv;
  struct app_req_super_data * app_req_superdata;
  struct msg_app2narb_request *req_intra;
  struct in_addr dest_intra;
  struct in_addr *p_addr;
  int ret = -1;    
  struct ero_subobj  *ero_subobj;
  u_int32_t narb_apiserv_id =  ntohl(msg_reply->narb_apiserv_id);
  u_int32_t  app_seqnum = ntohl(msg_reply->app_seqnum);
  struct te_tlv_header * tlv = &msg_reply->tlv;
 
  /* looking for the apiserver using narb_apiserv_id*/
  apiserv = narb_apiserver_list_lookup(narb_apiserv_id);
  if (!apiserv)
    {
      zlog_warn("narb_oclient_handle_cspf_reply_inter: narb_apiserver_list_lookup(%d) failed!", narb_apiserv_id);
      return -1;
    }

  /* looking for the request data*/    
  app_req_superdata = app_req_list_lookup (apiserv->app_req_list, app_seqnum);
  if (!app_req_superdata)
    {
      zlog_warn("narb_oclient_handle_cspf_reply_inter: app_req_list_lookup (apiserv = %d, seqnum = %d) failed!", 
                            narb_apiserv_id, app_seqnum);
      return -1;
    }
 
  switch (ntohs(tlv->type))
    {
    case TLV_TYPE_NARB_ERO:
      {
        /*Valid ERO returned:
          CSPF_inter only return Loose hop ERO's. For now it only consists of 
          a sequence of IPv4 addresses.  We may need a (address, interface-id)
          style ERO later.*/
        struct ero_subobj *router_ip;
        int len = ntohs(tlv->length);
        assert( len > 0 && len%sizeof(struct ero_subobj) == 0);
        /* fetch ip's from the message and put them into an ero list*/
        router_ip  = (struct in_addr *)((char *)tlv + sizeof(struct te_tlv_header));
        ero_list = list_new();
        for (; len > 0 ;router_ip++, len -= sizeof(struct ero_subobj))
          {
            ero_subobj = XMALLOC(MTYPE_TMP, sizeof(struct ero_subobj));
            *ero_subobj = *router_ip;
            listnode_add(ero_list, ero_subobj);
          }

        /* check if inter-domain ERO has already existed.*/
        if (app_req_superdata->ero_inter)
          {
            /* remove the old inter-domain ERO */
            ero_free(app_req_superdata->ero_inter);
          }
        
        /* add new inter-domain ERO list */
        app_req_superdata->ero_inter = ero_list;

         /* if no inter- nor intra- ERO*/
        if (app_req_superdata->ero_state == ERO_NONE)
          {
            /* so we have inter-domain ERO now*/
            app_req_superdata->ero_state = ERO_INTER_ONLY;

            /* find the current domain's gateway in this inter-domain ERO*/
            dest_intra = narb_lookup_domain_gateway (
                app_req_superdata->ero_inter,
                narb_domain_info.inter_domain_te_links,
                narb_domain_info.te_links);
          
            if(dest_intra.s_addr == 0)
              { /* if no expandable segment in inter-domain ERO, return the ero_iter directly */
                if (apiserv->routing_mode == RT_MODE_ALL_LOOSE_ALLOWED)
                  {
                    narb_apiserver_ero_reply (apiserv, app_seqnum, app_req_superdata->ero_inter);
                    ret = 0;
                  }
                else
                  {
                    narb_apiserver_error_reply(apiserv, app_seqnum, NARB_ERROR_NO_ROUTE);
                    assert (app_req_superdata->ero_inter);
                    ero_free(app_req_superdata->ero_inter);
                    app_req_superdata->ero_inter = NULL;
                    ret = -1;
                  }
              }
            else if (dest_intra.s_addr ==app_req_superdata->src.s_addr)
              {
                /* ero_list == app_req_superdata->ero_inter*/
                assert(ero_list->count >= 2);
                ero_subobj = ero_list->head->data;
                ero_subobj->hop_type = ERO_TYPE_STRICT_HOP;           
                ero_subobj = ero_list->head->next->data;
                ero_subobj->hop_type = ERO_TYPE_STRICT_HOP;

                if (apiserv->routing_mode == RT_MODE_MIXED_PREFERRED || apiserv->routing_mode == RT_MODE_ALL_LOOSE_ALLOWED)
                  {
                    narb_apiserver_ero_reply (apiserv, app_seqnum, app_req_superdata->ero_inter);
                    app_req_superdata->ero_state = ERO_INTER_INTRA;
                    app_req_superdata->state = APP_REQ_STATE_ERO_VALID;
                    ret = 0;
                  }
                else 
                  {
                    if (narb_apiserver_recursive_routing (apiserv, app_req_superdata, app_req_superdata->ero_inter) == 0)
                    {/* merged ero or error code returned within the function */
                      app_req_superdata->ero_state = ERO_INTER_INTRA;
                      app_req_superdata->state = APP_REQ_STATE_ERO_VALID;
                      ret = 0;
                    }
                    else
                    {
                      narb_apiserver_error_reply (apiserv, app_seqnum, NARB_ERROR_NO_ROUTE);
                      assert (app_req_superdata->ero_inter);
                      ero_free(app_req_superdata->ero_inter);
                      app_req_superdata->ero_inter = NULL;
                    }
                  }
              }
            else
              {
                /* otherwise the first segement is expandable, construct an 
                    intra-domain route request for the expandable segment*/
                req_intra = XMALLOC(MTYPE_TMP, sizeof (struct msg_app2narb_request));
                memset(req_intra, 0, sizeof (struct msg_app2narb_request));
                req_intra->length = htons(sizeof(struct msg_app2narb_request) - 4);
                req_intra->src = app_req_superdata->src;
                if ((p_addr = narb_lookup_router_by_if_addr(&req_intra->src, NULL)) != NULL)
                  {
                    req_intra->src = *p_addr;
                  }
                req_intra->dest = dest_intra;
                if ((p_addr = narb_lookup_router_by_if_addr(NULL, &req_intra->dest)) != NULL)
                  {
                    req_intra->dest = *p_addr;
                  }

                req_intra->bandwidth = app_req_superdata->bandwidth;
                req_intra->encoding_type = app_req_superdata->encoding_type;
                req_intra->switching_type = app_req_superdata->switching_type;
                
                /*send intra-domain CSPF request to expand the segment between
                  req_intra->src and req_intra->des*/
                if ((ret = narb_apiserver_send_cspf_request (oclient_intra, narb_apiserv_id, app_seqnum, req_intra)) < 0)
                  {
                    if (apiserv->routing_mode == RT_MODE_ALL_LOOSE_ALLOWED)
                      {
                        narb_apiserver_ero_reply (apiserv, app_seqnum, app_req_superdata->ero_inter);
                        app_req_superdata->ero_state = ERO_INTER_INTRA;
                        app_req_superdata->state = APP_REQ_STATE_ERO_VALID;
                        ret = 0;
                      }
                    else
                      {
                        if (ret == -2)
                            narb_apiserver_error_reply(apiserv, app_seqnum, NARB_ERROR_INTERNAL);
                        assert (app_req_superdata->ero_inter);
                        ero_free(app_req_superdata->ero_inter);
                        app_req_superdata->ero_inter = NULL;
                        if (app_req_superdata->ero_intra)
                          {
                            ero_free(app_req_superdata->ero_intra);
                            app_req_superdata->ero_intra = NULL;
                          }
                        app_req_superdata->ero_state = ERO_NONE;
                      }
                  }
                else
                  {
                    ; /* do nothing */
                  }
                XFREE(MTYPE_TMP, req_intra);
              }
          }
        else  if (app_req_superdata->ero_state == ERO_INTRA_ONLY ||
                      app_req_superdata->ero_state == ERO_INTER_INTRA)
          {
            /* (app_req_superdata->ero_state == ERO_INTRA_ONLY) is an optional scenario
              for future implementation. It implies that we can send inter- and intra- CSPF
              request in parallel. In the current implementation, we send inter- CSPF first. Only
              if the inter-CSPF request is replied with a valid ERO, we send the intra- CSPF.
              Therefore, no ERO_INTRA_ONLY case for now. ERO_INTER_INTRA is possible if
              retranmssion of request is carried out.*/
            
            /* merge inter-domain and intra-doamin ERO lists*/
            ero_list = 
              narb_merge_ero(app_req_superdata->ero_inter, app_req_superdata->ero_intra);
            
            if (!ero_list)
              {

                if (apiserv->routing_mode == RT_MODE_ALL_LOOSE_ALLOWED)
                  {
                    narb_apiserver_ero_reply (apiserv, app_seqnum, app_req_superdata->ero_inter);
                    app_req_superdata->ero_state = ERO_INTER_INTRA;
                    app_req_superdata->state = APP_REQ_STATE_ERO_VALID;
                    ret = 0;
                  }
                else
                  {
                    /* if the two ERO cannot be merged
                        construct error msg and send back to app client */
                    narb_apiserver_error_reply (apiserv, app_seqnum, NARB_ERROR_NO_ROUTE);
                    assert (app_req_superdata->ero_inter);
                    ero_free(app_req_superdata->ero_inter);
                    app_req_superdata->ero_inter = NULL;
                    assert (app_req_superdata->ero_intra);
                    ero_free(app_req_superdata->ero_intra);
                    app_req_superdata->ero_intra = NULL;
                    app_req_superdata->ero_state = ERO_NONE;
                  }
               }
            else
              {
                /* otherwise, reply to app client with merged/expanded ERO */
                narb_apiserver_ero_reply (apiserv, app_seqnum, ero_list);
                ero_free(ero_list);
                app_req_superdata->ero_state = ERO_INTER_INTRA;
                app_req_superdata->state = APP_REQ_STATE_ERO_VALID;
                ret = 0;
              }
          }
      }
      break;    
    case TLV_TYPE_NARB_ERROR_CODE: 
      /* someting wrong with the CSPF request*/
      {
        u_int32_t errcode = ntohl(*(u_int32_t *)((char *)tlv + sizeof(struct te_tlv_header)));
        switch (errcode)
          {
          case CSPF_ERROR_UNKNOWN_SRC:          /*No such a source */
            {
                narb_apiserver_error_reply(apiserv, app_seqnum, NARB_ERROR_NO_SRC);
                break;
            }
          case CSPF_ERROR_UNKNOWN_DEST:        /*No such a destination */
            {
                narb_apiserver_error_reply(apiserv, app_seqnum, NARB_ERROR_NO_DEST);
                break;
            }
          case CSPF_ERROR_NO_ROUTE:                 /*No route found */
            {
              narb_apiserver_error_reply(apiserv, app_seqnum, NARB_ERROR_NO_ROUTE);
            }
            break;
          } /* end of switch (errcode) */
        }

      if (app_req_superdata->ero_intra)
        {
          ero_free(app_req_superdata->ero_intra);
          app_req_superdata->ero_intra = NULL;
        }

      break;
    }/* end of switch (tlv->type) */

  return ret;
}

/*handling the CSPF reply message from OSPFd_intra */
int 
narb_oclient_handle_cspf_reply_intra (struct msg_narb_cspf_reply * msg_reply)
{
  list ero_intra_list;
  int len;
  listnode node;
  struct narb_apiserver *apiserv;
  struct app_req_super_data * app_req_superdata;
  int ret = -1;

  struct ero_subobj  *ero_subobj;
  u_int32_t narb_apiserv_id =  ntohl(msg_reply->narb_apiserv_id);
  u_int32_t  app_seqnum = ntohl(msg_reply->app_seqnum);
  struct te_tlv_header * tlv = &msg_reply->tlv;

  /* looking for the apiserver using narb_apiserv_id*/
  apiserv = narb_apiserver_list_lookup(narb_apiserv_id);
  if (!apiserv)
    {
      zlog_warn("narb_oclient_handle_cspf_reply_intra: narb_apiserver_list_lookup(%d) failed!", narb_apiserv_id);
      return -2;
    }

  /* looking for the request superdata  */    
  app_req_superdata = app_req_list_lookup (apiserv->app_req_list, app_seqnum);
  if (!app_req_superdata)
    {
      zlog_warn("narb_oclient_handle_cspf_reply_intra: app_req_list_lookup (apiserv = %d, seqnum = %d) failed!", 
                            narb_apiserv_id, app_seqnum);
      return -2;
    }

  
  switch (ntohs(tlv->type))
    {
    case TLV_TYPE_NARB_ERO:
      {
        /*Valid intra-domain ERO returned:
          CSPF_inter only return Loose hop ERO's. For now it only consists of 
          a sequence of IPv4 addresses.  We may need a (address, interface-id)
          style ERO later.*/
        struct ero_subobj *router_ip;
        len = ntohs(tlv->length);
        assert( len >0 && len % sizeof(struct ero_subobj) == 0);
        router_ip  = (struct in_addr *)((char *)tlv + sizeof(struct te_tlv_header));
        ero_intra_list = list_new();
        for (; len > 0 ;router_ip++, len -= sizeof(struct ero_subobj))
          {
            ero_subobj = XMALLOC(MTYPE_TMP, sizeof(struct ero_subobj));
            *ero_subobj = *router_ip;
            listnode_add(ero_intra_list, ero_subobj);
          }

        if (app_req_superdata->ero_intra)
          {
            ero_free(app_req_superdata->ero_intra);
          }
        app_req_superdata->ero_intra = ero_intra_list;

         /* if no ERO exists, we now have an intra-domain ERO */
        if (app_req_superdata->ero_state == ERO_NONE || 
             app_req_superdata->ero_state == ERO_INTRA_ONLY)
          {
            int reply_with_intra_only = 0;
            struct in_addr last = ((struct ero_subobj*)ero_intra_list->tail->data)->addr;
            
            assert (app_req_superdata->src.s_addr == msg_reply->src.s_addr);
            assert (app_req_superdata->dest.s_addr == msg_reply->dest.s_addr);

            app_req_superdata->ero_state = ERO_INTRA_ONLY;

            if (oclient_inter && oclient_inter->neighbor_count > 0 && !single_domain_mode)
              {
                /* Check if last-hop is an interface in domain_summary_info->te_links. */
                node = listhead(narb_domain_info.te_links);
                while (node)
                  {
                    if (((struct link_info *)node->data)->rem_if.s_addr == last.s_addr)
                      break;
                    nextnode(node);
                  }
                 if (node)
                   reply_with_intra_only = 1;
                }
              else
                {
                  reply_with_intra_only = 1;
                }

              if (reply_with_intra_only) /* An intra-domain request is replied with a valid intra-domain ERO*/
                {
                  narb_apiserver_ero_reply (apiserv, app_seqnum, app_req_superdata->ero_intra);
                  app_req_superdata->ero_state = ERO_INTRA_ONLY;
                  app_req_superdata->state = APP_REQ_STATE_ERO_VALID;
                  ret = 0;
                }
              else
                narb_apiserver_error_reply (apiserv, app_seqnum, NARB_ERROR_NO_ROUTE);
          }
        else  if (app_req_superdata->ero_state == ERO_INTER_ONLY ||
                    app_req_superdata->ero_state == ERO_INTER_INTRA)
          {
            /* if we alreday have an inter-domain ERO merge intra-domain 
                and inter-domain ERO; if we have an intra-doamin ERO too, 
                replace it with the new one. */
            list merged_ero
                = narb_merge_ero(app_req_superdata->ero_inter,  app_req_superdata->ero_intra);

            if (!merged_ero)
              {

                if (apiserv->routing_mode == RT_MODE_ALL_LOOSE_ALLOWED)
                  {
                    narb_apiserver_ero_reply (apiserv, app_seqnum, app_req_superdata->ero_inter);
                    app_req_superdata->ero_state = ERO_INTER_INTRA;
                    app_req_superdata->state = APP_REQ_STATE_ERO_VALID;
                    ret = 0;
                  }
                else
                  {
                    /* Construct error msg and schedule to write */
                    narb_apiserver_error_reply (apiserv, app_seqnum, NARB_ERROR_NO_ROUTE);
                    assert (app_req_superdata->ero_intra);
                    ero_free(app_req_superdata->ero_intra);
                    app_req_superdata->ero_intra = NULL;
                    if (app_req_superdata->ero_state == ERO_INTER_INTRA)
                       app_req_superdata->ero_state = ERO_INTER_ONLY; 
                    /* merging failed, something wrong with the intra or inter ERO*/
                    app_req_superdata->ero_state = ERO_NONE;
                  }
              }
            else /*intra- and inter-ero's merged succesfully */
              {
                if (apiserv->routing_mode == RT_MODE_MIXED_PREFERRED)
                  {
                    narb_apiserver_ero_reply (apiserv, app_seqnum, merged_ero);
                    app_req_superdata->ero_state = ERO_INTER_INTRA;
                    app_req_superdata->state = APP_REQ_STATE_ERO_VALID;
                    ret = 0;
                    break;
                  }

                /* if the last subobj is loose hop, use the last strict subobj and the last loose subobj 
                 as src and dest. Compose and send a recursive requst to the NARB of next domain.*/
                if (((struct ero_subobj*)merged_ero->tail->data)->hop_type == ERO_TYPE_LOOSE_HOP)
                  {
                    /* merged ero or error code returned within the function */
                    if (narb_apiserver_recursive_routing (apiserv, app_req_superdata, merged_ero) == 0)
                      {
                        app_req_superdata->state = APP_REQ_STATE_ERO_VALID;
                        ret = 0;
                      }
                    else if (apiserv->routing_mode == RT_MODE_MIXED_ALLOWED)
                      {
                        narb_apiserver_ero_reply (apiserv, app_seqnum, merged_ero);
                        app_req_superdata->state = APP_REQ_STATE_ERO_VALID;
                        ret = 0;
                      }
                    else
                      {
                        /*narb_apiserver_error_reply (apiserv, app_seqnum, NARB_ERROR_NO_ROUTE);*/
                        assert (app_req_superdata->ero_intra);
                        ero_free(app_req_superdata->ero_intra);
                        app_req_superdata->ero_intra = NULL;
                        /* merging failed, something wrong with the intra or inter ERO*/
                        app_req_superdata->ero_state = ERO_NONE;
                      }
                  }
                else
                  {
                    /*reply with a merged/expanded ERO */
                    app_req_superdata->ero_state = ERO_INTER_INTRA;
                    narb_apiserver_ero_reply (apiserv, app_seqnum, merged_ero);
                    app_req_superdata->state = APP_REQ_STATE_ERO_VALID;
                    ret = 0;
                  }

                  ero_free(merged_ero);
              }
          }  
      }
      break;
    case TLV_TYPE_NARB_ERROR_CODE:
      /*somewrong with the intra-domain CSPF request*/
      {
        u_int32_t errcode = ntohl(*(u_int32_t *)((char *)tlv + sizeof(struct te_tlv_header)));
        if (apiserv->routing_mode != RT_MODE_ALL_LOOSE_ALLOWED)
          {
            switch (errcode)
              {
              case CSPF_ERROR_UNKNOWN_SRC:          /*No such a source */
                {
                    narb_apiserver_error_reply(apiserv, app_seqnum, NARB_ERROR_NO_SRC);
                    break;
                }
              case CSPF_ERROR_UNKNOWN_DEST:        /*No such a destination */
                {
                    narb_apiserver_error_reply(apiserv, app_seqnum, NARB_ERROR_NO_DEST);
                    break;
                }
              case CSPF_ERROR_NO_ROUTE:                 /*No route found */
                {
                  narb_apiserver_error_reply(apiserv, app_seqnum, NARB_ERROR_NO_ROUTE);
                }
                break;
              } /* end of switch (errcode) */
          }
      }

      if (app_req_superdata->ero_intra)
        {
          ero_free(app_req_superdata->ero_intra);
          app_req_superdata->ero_intra = NULL;
        }
      break;
    }/* end of switch (tlv->type) */

  return ret;
}


/*recursive routing procedure*/
int
narb_apiserver_recursive_routing (struct narb_apiserver *apiserv,  
    struct app_req_super_data *app_req_superdata, list ero)
{
  listnode node;
  struct in_addr rec_src, rec_dest;
  struct msg_app2narb_request * msgbody;
  struct msg *msg;
  struct if_narb_info * if_narb;
  int narb_fd;
  int ret = -1;

  /* get source ip of the recursice request*/
  node = listhead(ero);
  while (node)
    {
      if (((struct ero_subobj*)node->data)->hop_type == ERO_TYPE_LOOSE_HOP)
        break;
      nextnode(node);
    }
  
    assert(node->prev);
    rec_src = ((struct ero_subobj*)node->prev->data)->addr;
    rec_dest = ((struct ero_subobj*)ero->tail->data)->addr;
    
  /* look up the next-domain narb associated with the source ip address */
  if_narb = if_narb_lookup(narb_domain_info.if_narb_table, rec_src);
  /* disconnect if an old connection exists */
  if (app_req_superdata->rec_narb_fd)
    {
      close(app_req_superdata->rec_narb_fd);
      app_req_superdata->rec_narb_fd = 0;
    }

  /* connect to the next-domain narb */
  if (!if_narb || !(narb_fd = narb_connect_peer(if_narb->addr, if_narb->port)))
    {  
      narb_apiserver_error_reply(apiserv, ntohl(msg->hdr.msgseq), NARB_ERROR_INTERNAL);
      return -1;
    }

  /* compose request message */
  msgbody = malloc(sizeof(struct msg_app2narb_request));
  memset(msgbody, 0, sizeof(struct msg_app2narb_request));
  msgbody->type = htons(TLV_TYPE_NARB_REQUEST);
  msgbody->length = htons(sizeof(struct msg_app2narb_request) - 4);
  msgbody->src = rec_src;
  msgbody->dest = rec_dest;
  msgbody->bandwidth = app_req_superdata->bandwidth;
  msgbody->encoding_type = app_req_superdata->encoding_type;
  msgbody->switching_type = app_req_superdata->switching_type;
  msg = msg_new(MSG_APP_REQUEST_EVENT, msgbody, app_req_superdata->app_seqnum,
        sizeof(struct msg_app2narb_request));
  XFREE(MTYPE_TMP, msgbody);
  
  /* send request message */
  msg_write(narb_fd, msg);
  narb_apiserver_msg_print (msg);
  msg_free(msg);

  /* sync read */
  msg = msg_read(narb_fd);
  if (!msg)
    {
      zlog_warn
      ("narb recursive routing: read failed on fd=%d, closing connection", narb_fd);
       narb_apiserver_error_reply(apiserv, ntohl(msg->hdr.msgseq), NARB_ERROR_INTERNAL);
      return -1;
    }

  /* store the next-domain narb information*/
  app_req_superdata->rec_narb = if_narb;
  app_req_superdata->rec_narb_fd = narb_fd;

  /*reply with a merged/expanded ERO */
  app_req_superdata->ero_state = ERO_INTER_INTRA;
  /* handle reply message. Free msg and ero within the function. */
  ret = narb_apiserver_handle_recursive_routing_reply (apiserv, msg, ero);
  
  return ret;
}

  /*handling the ERO reply or error message from neighboring NARB */
int
narb_apiserver_handle_recursive_routing_reply (struct narb_apiserver *apiserv,
                  struct msg *msg, list ero)
{
  char * data;
  u_char hop_type;
  int i, count;
  struct ipv4_prefix_subobj * subobj;
  struct ero_subobj * ero_subobj;
  list rec_ero = NULL;
  list merged_ero;
  int ret = -1;
  
  data = STREAM_DATA(msg->s) + 4;
      
  switch (msg->hdr.msgtype)
    {
    case MSG_REPLY_ERO:
      count = (ntohs(msg->hdr.msglen) - 4) / 8;
      if (count)
        rec_ero = list_new();
      
      for (i = 0; i < count; i++)
        {
          subobj = (struct ipv4_prefix_subobj *)(data+8*i);
          hop_type = subobj->l_and_type >> 7;
          assert(hop_type == ERO_TYPE_STRICT_HOP);

          ero_subobj = XMALLOC(MTYPE_TMP, sizeof(struct ero_subobj));
          memset(ero_subobj, 0, sizeof(struct ero_subobj));
          memcpy(&ero_subobj->addr, &subobj->addr, sizeof(struct in_addr));
          ero_subobj->hop_type = hop_type;
          ero_subobj->prefix_len = 32;
          listnode_add(rec_ero, ero_subobj);
        }

      if(rec_ero)
        {
          merged_ero  = narb_merge_ero(ero, rec_ero);
          ero_free(rec_ero);
          
          if (merged_ero)
            {
              /* merged_ero freed within the function*/
              narb_apiserver_ero_reply(apiserv, ntohl(msg->hdr.msgseq), merged_ero);
              ero_free(merged_ero);
              ret = 0;
            }      
          else if (apiserv->routing_mode == RT_MODE_MIXED_ALLOWED)
            {
              narb_apiserver_ero_reply(apiserv, ntohl(msg->hdr.msgseq), ero);
              ret = 0;
            }
          else
            {
              narb_apiserver_error_reply(apiserv, ntohl(msg->hdr.msgseq), NARB_ERROR_NO_ROUTE);
            }
        }
      break;
    case MSG_REPLY_ERROR:
      {
        u_int16_t errcode = ntohs(*(u_int16_t *)data);
        narb_apiserver_error_reply (apiserv, ntohl(msg->hdr.msgseq), errcode);
      }
      break;
    /*
    case MSG_REPLY_REMOVE_CONFIRM:
      {
        zlog_info("NARB confirmation: The requested route has been removed..\n");
      }
      break;
    */
    }

  msg_free(msg);
  
  return ret;
}
      
/*auxiliary function mergeing inter-domain and inter-domain ERO's
   a merged/expaned ERO list is created and returned. 
   Memory allocated within function.*/
list
narb_merge_ero (list ero_inter, list ero_intra)
{
  listnode node_inter, node_intra, src_tmp, dest_tmp;
  struct in_addr src_intra, dest_intra;
  struct ero_subobj* nodedata;
  struct in_addr *router_id1, *router_id2;
  list merged_ero;
  
  assert (ero_inter);
  assert (ero_intra);

  src_intra = ((struct ero_subobj *)ero_intra->head->data)->addr;
  dest_intra = ((struct ero_subobj *)ero_intra->tail->data)->addr;

  /*locate src_intra on the ero_inter list (condition: both ip's are if_addr's 
  originating from the same router)*/
  router_id1 = narb_lookup_router_by_if_addr (&src_intra, NULL);
  if (!router_id1)
    return NULL;
  
  node_inter = listhead(ero_inter);
  while (node_inter)
    {
      router_id2 = narb_lookup_router_by_if_addr 
                        (&((struct ero_subobj *)node_inter->data)->addr, NULL);
      if (!router_id2)
        {
          nextnode(node_inter);
          continue;
        }
      if (router_id1->s_addr == router_id2->s_addr)
        break;
      nextnode(node_inter);
    }
  if (!node_inter)  /*src_intra must exist in ero_inter list */
    return NULL;
  if (node_inter->prev && node_inter->next)
    nextnode(node_inter);
  src_tmp = node_inter;

  /*locate dest_intra on the ero_inter list (condition: both ip's are if_addr's 
  terminating on the same router)*/
  router_id1 = narb_lookup_router_by_if_addr (NULL, &dest_intra);
  if (!router_id1)
    return NULL;

  nextnode(node_inter);
  while (node_inter)
    {
      router_id2 = narb_lookup_router_by_if_addr 
                        (NULL, &((struct ero_subobj *)node_inter->data)->addr);
      if (!router_id2)
        {
          nextnode(node_inter);
          continue;
        }
      if (router_id1->s_addr == router_id2->s_addr)
        break;
      nextnode(node_inter);
    }
  if (!node_inter)  /*dest_intra must exist in ero_inter list */
    return NULL;
  dest_tmp = node_inter;

  merged_ero = list_new();
  
  node_inter = listhead(ero_inter);

  /* add nodes in ero_inter before ero_intra->head */
  while (node_inter !=src_tmp)
    {
      assert(node_inter);
      nodedata = XMALLOC(MTYPE_TMP, sizeof(struct ero_subobj));
      *nodedata = *((struct ero_subobj*)node_inter->data);
      listnode_add(merged_ero, nodedata);
      node_inter = node_inter->next;
    } 
  
  /* add nodes in ero_intra */
  node_intra = ero_intra->head;
  while (node_intra)
    {
      struct ero_subobj* nodedata;
      assert(node_intra);
      nodedata = XMALLOC(MTYPE_TMP, sizeof(struct ero_subobj));
      *nodedata = *((struct ero_subobj*)node_intra->data);
      listnode_add(merged_ero, nodedata);
      node_intra = node_intra->next;
    }

  /* add nodes in ero_inter after ero_intra->tail */
  node_inter = dest_tmp->next;
  nodedata = getdata(ero_intra->head);
  /* inter-domain link is forced to be strict-hop if the first hop is strict*/
  if (node_inter && nodedata->hop_type == ERO_TYPE_STRICT_HOP)
  {
    ((struct ero_subobj*)node_inter->data)->hop_type = ERO_TYPE_STRICT_HOP;
    if (node_inter->next)
      ((struct ero_subobj*)node_inter->next->data)->hop_type 
           = ERO_TYPE_STRICT_HOP;
  }
  
  while (node_inter)
    {
      struct ero_subobj* nodedata;
      assert(node_inter);
      nodedata = XMALLOC(MTYPE_TMP, sizeof(struct ero_subobj));
      *nodedata = *((struct ero_subobj*)node_inter->data);
      listnode_add(merged_ero, nodedata);
      node_inter = node_inter->next;
    } 

  return merged_ero;
}

/* This function is used exclusively for looking up an inte-domain
   link, whose id we assume is unique. */
struct link_info * 
narb_lookup_link_by_id (list links, struct in_addr *id)
{
  listnode node;
  struct link_info * link;

  assert(links);
  node = listhead(links);
  while(node)
  {
    link = (struct link_info *)node->data;
    if (link->id.s_addr == id->s_addr)
      {
        return link;
      }
    nextnode(node);
  }
  return NULL;
}

/* Looking for the current domain gateway in the inter-domain ERO list.
    An IP is the current domain gateway, if it advertising an inter-domain
    TE link, whose ID (IPv4) exists in the inter-domain ERO.*/
struct in_addr
narb_lookup_domain_gateway (list ero_inter, list inter_links, list all_links)
{
  struct ero_subobj *subobj;
  struct in_addr * te_link_id;
  struct link_info *link;
  struct in_addr ret = {0};
  listnode node_ero;
  listnode node_link = listhead(inter_links);

  /* loop though all inter-domain TE links */
  while(node_link)
    {
      te_link_id = (struct in_addr *)node_link->data;
      nextnode(node_link);
      
      link = narb_lookup_link_by_id(all_links, te_link_id);
      if (!link)
        continue;
      
      /* loop though all IPv4 sub ERO objs in the inter-domain ERO */
      node_ero = listhead(ero_inter);
      while(node_ero)
        {
          subobj = (struct ero_subobj *)node_ero->data;
          if (subobj->addr.s_addr == link->loc_if.s_addr)
            return link->adv_id;
          nextnode(node_ero);
        }
    }
  
  subobj = getdata(ero_inter->tail);  
  link = narb_lookup_link_by_if_addr(all_links, NULL, &subobj->addr);
  if (link)
    {
      return link->id;
    }
  return ret;
}

struct in_addr *
narb_lookup_router_by_if_addr (struct in_addr *lcl_if, struct in_addr *rmt_if)
{
  struct ospf_lsa * lsa;

  if (!oclient_inter || !oclient_inter->lsdb)
    return NULL;

  lsa = narb_lookup_lsa_by_if_addr (oclient_inter->lsdb, lcl_if, rmt_if);

  if (lcl_if)
  {
    if (lsa && lsa->data)
      return &lsa->data->adv_router;
  }
  else
  {
    if (lsa && lsa->tepara_ptr && lsa->tepara_ptr->p_link_id)
      return &lsa->tepara_ptr->p_link_id->value;
  }

  return NULL;
}



/* Look for a link with speficied local and/or remote interface address. 
    Assuming that each interface address is unique. */
struct link_info * 
narb_lookup_link_by_if_addr (list links, struct in_addr *lcl_if, struct in_addr *rmt_if)
{
  listnode node;
  struct link_info * link;

  assert(links);
  node = listhead(links);

  if (lcl_if)
    {
      while(node)
      {
        link = (struct link_info *)node->data;
        nextnode(node);
        
        if (link->loc_if.s_addr== lcl_if->s_addr)
          {
            if (rmt_if && link->rem_if.s_addr != rmt_if->s_addr)
              continue;
            else
              return link;
          }

      }
    }
  else if (rmt_if) /* lcl_if == NULL */
    {
      while(node)
      {
        link = (struct link_info *)node->data;
        nextnode(node);
        
        if (link->rem_if.s_addr== rmt_if->s_addr)
          {
            if (lcl_if && link->loc_if.s_addr != lcl_if->s_addr)
              continue;
            else
              return link;
          }
      }
    }

  return NULL;
}

/* Look for a link with speficied local and/or remote interface address in LSDB*/ 
struct ospf_lsa * 
narb_lookup_lsa_by_if_addr (list lsdb, struct in_addr *lcl_if, struct in_addr *rmt_if)
{
  listnode node = listhead(lsdb);
  struct ospf_lsa *lsa;

  while (node)
  {
    lsa = getdata(node);

    nextnode(node);

    /* lsa should have been parsed when entering the narb lsdb*/
    
    if (!lsa)
      continue;

    if (lsa->te_lsa_type!= LINK_TE_LSA)
      continue;

    if (!lsa->tepara_ptr)
      continue;

    if (!lsa->tepara_ptr->p_lclif_ipaddr)
      continue;
    
    if (lcl_if && lcl_if->s_addr != lsa->tepara_ptr->p_lclif_ipaddr->value.s_addr)
      continue;
    else if (!rmt_if)
      return lsa;
    
    if (!lsa->tepara_ptr->p_rmtif_ipaddr)
      continue;

    if (rmt_if && rmt_if->s_addr != lsa->tepara_ptr->p_rmtif_ipaddr->value.s_addr)
      continue;

    return lsa;
  }

  return NULL;
}

/* Connecting to a peering NARB */
int 
narb_connect_peer(char * host, int port)
{
    struct sockaddr_in addr;
    struct hostent *hp;
    int fd;
    int ret;
    int size;
    int on = 1;
	
    hp = gethostbyname (host);
      if (!hp)
    {
      fprintf (stderr, "ospf_apiclient_connect: no such host %s\n", host);
      return (-1);
    }

    fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
      {
	  zlog_warn("narb_connect_peer: socket(): %s", strerror (errno));
	  return (-1);
      }

                                                                                
  /* Reuse addr and port */
  ret = setsockopt (fd, SOL_SOCKET,
                    SO_REUSEADDR, (void *) &on, sizeof (on));
  if (ret < 0)
    {
      fprintf (stderr, "narb_connect_peer: SO_REUSEADDR failed\n");
      close (fd);
      return (-1);
    }

#ifdef SO_REUSEPORT
  ret = setsockopt (fd, SOL_SOCKET, SO_REUSEPORT,
                    (void *) &on, sizeof (on));
  if (ret < 0)
    {
      fprintf (stderr, "narb_connect_peer: SO_REUSEPORT failed\n");
      close (fd);
      return (-1);
    }
#endif /* SO_REUSEPORT */

  /* Bind sync socket to address structure. This is needed since we
     want the sync port number on a fixed port number. The reverse
     async channel will be at this port+1 */
                                                                                
  memset (&addr, 0, sizeof (struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons (port+100);
#ifdef HAVE_SIN_LEN
  addr.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_SIN_LEN */
  size = sizeof (struct sockaddr_in);
                                                                                
  ret = bind (fd, (struct sockaddr *) &addr, size);
  if (ret < 0)
    {
      fprintf (stderr, "narb_connect_peer: bind sync socket failed\n");
      close (fd);
      return (-1);
    }
                                                                                
  /* Prepare address structure for connect */
  memcpy (&addr.sin_addr, hp->h_addr, hp->h_length);
  addr.sin_family = AF_INET;
  addr.sin_port = htons (port);
#ifdef HAVE_SIN_LEN
  addr.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_SIN_LEN */
 
  /* Now establish synchronous channel with OSPF daemon */
  ret = connect (fd, (struct sockaddr *) &addr,
                 sizeof (struct sockaddr_in));
  if (ret < 0)
    {
      zlog_warn("narb_connect_peer: connect(): %s", strerror (errno));
      close (fd);
      return (-1);
    }

  return fd;
}

