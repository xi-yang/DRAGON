/*narb_summary.c*/

#include "zebra.h"
#include "prefix.h" 
#include "linklist.h"
#include "memory.h"
#include "thread.h"
#include "stream.h"
#include "log.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_te.h"
#include "ospfd/ospf_te_lsdb.h"
#include "ospfd/ospf_opaque.h"
#include "ospfd/ospf_api.h"
#include "ospf_apiclient.h"
#include "narb_apiserver.h"

#include "narb_summary.h"

/* global*/
struct narb_domain_info narb_domain_info;
int narb_topology_refresh_interval = OSPF_LSA_MAXAGE - OSPF_LS_REFRESH_TIME;


/*  Thread/timer function to originate domain summary LSA's periodically */
int
narb_refresher_originate_summary (struct thread *t)
{
  struct ospf_apiclient * oc = THREAD_ARG(t);
  int rc;

  /*
  static int delete_once = 1;
  if (delete_once);
    {
      flag_holdon = 1;
      delete_once = 0;
      narb_delete_summary();
      thread_add_timer (master, narb_refresher_originate_summary, oc, 5);
      return 0;
    }
  
  flag_holdon = 0;
  delete_once = 1;
  */

  flag_holdon = 1;
  narb_delete_summary(oc);  
  rc = narb_originate_summary(oc);
  flag_holdon = 0;
  return rc;
}

/* Function originating Router ID TE LSA*/
int 
narb_originate_router_id (struct ospf_apiclient * oc, struct router_id_info* router)
{
  int ret = -1;
  void *opaquedata;
  int opaquelen;
  u_char lsa_type = 10;
  u_char opaque_type = 1;

  opaquedata = (void *)ospf_te_router_addr_tlv_alloc(router->id); 
  opaquelen = ntohs(((struct te_tlv_header *)opaquedata)->length)
                + sizeof (struct te_tlv_header);

  router->opaque_id = narb_ospf_opaque_id();
  ret = ospf_apiclient_lsa_originate(oc, 
                  oc == oclient_inter? narb_domain_info.ospfd_inter.ori_if :
                  narb_domain_info.ospfd_intra.ori_if,
                  router->adv_id, /*$$$$ hacked*/
                  oc == oclient_inter? narb_domain_info.ospfd_inter.area :
                  narb_domain_info.ospfd_intra.area,
				    lsa_type, opaque_type, router->opaque_id,
				    opaquedata, opaquelen);
  XFREE(MTYPE_TMP, opaquedata);
  
  zlog_info ("ROUTER_ID (lsa-type[%d] opaque-type[%d]  \
opaque-id[%d]) originated through OSPFd at %s.", 
     lsa_type, opaque_type, router->opaque_id, oc == oclient_inter? 
     narb_domain_info.ospfd_inter.addr: narb_domain_info.ospfd_intra.addr);
  zlog_info ("\t ID = %X, ADV_ROUTER = %X: return code is %d.", 
    router->id, router->adv_id, ret);
  return ret;
}

/* Function originating Link TE LSA*/
int
narb_originate_te_link (struct ospf_apiclient * oc, struct link_info* link)
{
  int ret = -1;
  void *opaquedata;
  int opaquelen;
  u_char lsa_type = 10;
  u_char opaque_type = 1;
  
  opaquedata = (void *)ospf_te_link_tlv_alloc(link->type, link->id); 
  if (LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_LOC_IF))
    {
      opaquedata = ospf_te_link_subtlv_append(opaquedata,
    		TE_LINK_SUBTLV_LCLIF_IPADDR, &link->loc_if);
    }
  if (LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_REM_IF))
    {
      opaquedata = ospf_te_link_subtlv_append(opaquedata,
    		TE_LINK_SUBTLV_RMTIF_IPADDR, &link->rem_if);
    }
  if (LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_MAX_BW))
    {
      opaquedata = ospf_te_link_subtlv_append(opaquedata,
    		TE_LINK_SUBTLV_MAX_BW, &link->max_bw);
    }
  if (LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_METRIC))
    {
      opaquedata = ospf_te_link_subtlv_append(opaquedata, 
    		TE_LINK_SUBTLV_TE_METRIC, &link->metric);
    }

  if (LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_MAX_RSV_BW))
    {
      opaquedata = ospf_te_link_subtlv_append(opaquedata, 
    		TE_LINK_SUBTLV_MAX_RSV_BW, &link->max_rsv_bw);
    }

  if (LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_UNRSV_BW))
    {
      opaquedata = ospf_te_link_subtlv_append(opaquedata,
    		TE_LINK_SUBTLV_UNRSV_BW, link->unrsv_bw);
    }

  if (LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_IFSW_CAP))
    {
      opaquedata = ospf_te_link_subtlv_append(opaquedata,
    		TE_LINK_SUBTLV_LINK_IFSWCAP, (void*)&link->ifswcap);
    }

  if (LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_RESV))
    {
      opaquedata = ospf_te_link_subtlv_append(opaquedata,
    		TE_LINK_SUBTLV_LINK_DRAGON_RESV, (void*)link->resvs);
    }

  opaquedata = ospf_te_link_subtlv_append(opaquedata,
    		TE_LINK_SUBTLV_LINK_DRAGON_DOMAIN_ID, (void*)&narb_domain_info.domain_id);
  
  opaquelen = ntohs(((struct te_tlv_header *)opaquedata)->length)
                + sizeof (struct te_tlv_header);
  link->opaque_id = narb_ospf_opaque_id();
  ret = ospf_apiclient_lsa_originate(oc,
                 oc == oclient_inter ? narb_domain_info.ospfd_inter.ori_if :
                 narb_domain_info.ospfd_intra.ori_if,
                 link->adv_id, /*$$$$ hacked*/
                 oc == oclient_inter ? narb_domain_info.ospfd_inter.area :
                 narb_domain_info.ospfd_intra.area,
				    lsa_type, opaque_type, link->opaque_id,
				    opaquedata, opaquelen);
  XFREE(MTYPE_TMP, opaquedata);
  
  zlog_info ("TE_LINK (lsa-type[%d] opaque-type[%d]  opaque-id[%d]) originated through OSPFd at %s.", 
     lsa_type, opaque_type, link->opaque_id, oc == oclient_inter ?
     narb_domain_info.ospfd_inter.addr : narb_domain_info.ospfd_intra.addr);
  zlog_info ("\t ID = %X, TYPE = %X, ADV_ROUTER = %X: return code is %d.", link->id, link->type, link->adv_id, ret);
  return ret;
}

/* Implementation of LSA origination using router_ids and te_links 
    in narb_domain_info. LSA's are originated to OSPFd. Note that 
    the function ospf_apiclient_lsa_originate has been hacked to use 
    different advertising router ids instead of using the id of OSPFd only*/
int
narb_originate_summary (struct ospf_apiclient * oc)
{
  int ret;
  
  listnode node;
 
  /* cancel the old originating thread*/
  if (oc->t_originator)
    {
      if (thread_timer_remain_second(oc->t_originator) > 0)
        thread_cancel(oc->t_originator);
      oc->t_originator = NULL;
    }

  /*Automatically probing/refreshing virtual te links using intRA-domain OSPFd CSPF requests*/
  narb_cleanup_probed_links(&narb_domain_info);
  narb_probe_virtual_links(&narb_domain_info);

  assert (narb_domain_info.router_ids);
  /*originate router-id LSA's*/
  for (node = listhead(narb_domain_info.router_ids); node; nextnode(node))
    {
      struct router_id_info * router = node->data;

      if (router->hide)
        continue;
      ret = narb_originate_router_id (oc, router);
    }
  
  assert (narb_domain_info.te_links);
  /*originate  te-link LSA's*/
  for (node = listhead(narb_domain_info.te_links); node; nextnode(node))
    {
      struct link_info * link = node->data;

      if (link->hide)
        continue;
      ret = narb_originate_te_link(oc, link);
    }

  /*refresher*/
  oc->t_originator = 
      thread_add_timer (master, narb_refresher_originate_summary, oc, 
          narb_topology_refresh_interval);
  return 0;
}

/* delete all router_id and te_link LSA's originated from narb_domain_info
    Note that the function ospf_apiclient_lsa_delete has been hacked
    to use different advertising router ids */

void
narb_delete_summary (struct ospf_apiclient * oc)
{
  int ret;

  u_char lsa_type = 10;
  u_char opaque_type = 1;

  listnode node;

  /* Cancel the thread that refresh the originated LSA's*/
  if (oc->t_originator)
  {
    if (thread_timer_remain_second(oc->t_originator) > 0)
      thread_cancel(oc->t_originator);
    oc->t_originator = NULL;
  }

  assert (narb_domain_info.router_ids);
  
  for (node = listhead(narb_domain_info.router_ids); node; nextnode(node))
    {
      struct router_id_info * router = node->data;

      if (router->hide)
        continue;
      ret = ospf_apiclient_lsa_delete (oc, 
                      router->adv_id, /* $$$$ hacked */
                      oc == oclient_inter? narb_domain_info.ospfd_inter.area : 
                      narb_domain_info.ospfd_intra.area, lsa_type,  
                      opaque_type, router->opaque_id);

      zlog_info ("ROUTER_ID (lsa-type[%d] opaque-type[%d]  opaque-id[%d]) deleted through OSPFd at %s.", 
         lsa_type, opaque_type, router->opaque_id, oc == oclient_inter? narb_domain_info.ospfd_inter.addr : 
                      narb_domain_info.ospfd_intra.addr);
      zlog_info ("\t ID = %X, ADV_ROUTER = %X: return code is %d.", router->id.s_addr, router->adv_id.s_addr, ret);
    }

  /*list_free(narb_domain_info.router_ids);
  narb_domain_info.router_ids = NULL;*/

  assert (narb_domain_info.te_links);
  for (node = listhead(narb_domain_info.te_links); node; nextnode(node))
    {
      struct link_info * link = node->data;

      if (link->hide)
        continue;
      ret = ospf_apiclient_lsa_delete (oc, 
                      link->adv_id, /* $$$$ hacked */
                      oc == oclient_inter? narb_domain_info.ospfd_inter.area : 
                      narb_domain_info.ospfd_intra.area, lsa_type,  
                      opaque_type, link->opaque_id);

      /* XFREE (MTYPE_TMP, link); */
      zlog_info ("LINK_ID (lsa-type[%d] opaque-type[%d]  opaque-id[%d]) deleted through OSPFd at %s.", 
         lsa_type, opaque_type, link->opaque_id, oc == oclient_inter? narb_domain_info.ospfd_inter.addr : 
                      narb_domain_info.ospfd_intra.addr);
      zlog_info ("\t ID = %X, ADV_ROUTER = %X: return code is %d.", link->id, link->adv_id, ret);
    }

  /* list_delete(narb_domain_info.te_links);
  narb_domain_info.te_links = NULL;*/

  /*empty the narb lsdb*/
}

struct if_narb_info * 
if_narb_lookup(list if_narb_table, struct in_addr if_addr)
{
  listnode node;
  listnode node_inner;
  struct if_narb_info * nodedata;
  struct in_addr * nodedata_inner;
  assert (if_narb_table);

  node = listhead(if_narb_table);
  while (node)
    {
      nodedata = getdata(node);
      assert (nodedata->if_addr_list);
      
      node_inner = listhead(nodedata->if_addr_list);
      while(node_inner)
        {
          nodedata_inner = getdata(node_inner);
          if (nodedata_inner->s_addr == if_addr.s_addr)
            return nodedata;
          nextnode(node_inner);
        }
      nextnode(node);
    }

  return NULL;
}

struct link_info*
narb_cspf_probe(struct svc_probe *svc_probe, struct router_id_info *router)
{
  int rc = -1;
  struct msg * msg;
  u_int32_t seqnum;
  struct msg_narb_cspf_reply * rmsg;  
  struct te_tlv_header * tlv;
  struct link_info *link;
  int len;
  struct in_addr *router_ip;
  struct msg_narb_cspf_request * cspf_req = XMALLOC(MTYPE_TMP, sizeof (struct msg_narb_cspf_request));

  memset(cspf_req, 0, sizeof(struct msg_narb_cspf_request));
  
  /* messages to OSPFd_intra and OSPFd_inter use different area id*/
  cspf_req->area_id = narb_domain_info.ospfd_intra.area;
  cspf_req->app_req_data.src.s_addr = svc_probe->router->id.s_addr;
  cspf_req->app_req_data.dest.s_addr = router->id.s_addr;
  cspf_req->app_req_data.switching_type = svc_probe->service->sw_type;
  cspf_req->app_req_data.encoding_type = svc_probe->service->enc_type;
  cspf_req->app_req_data.bandwidth = svc_probe->service->max_bw;
  seqnum = ospf_apiclient_get_seqnr();
  msg = msg_new (MSG_NARB_CSPF_REQUEST, cspf_req, seqnum, sizeof(struct msg_narb_cspf_request));
  XFREE(MTYPE_TMP, cspf_req);
  
  if (!msg)
    {
      zlog_warn ("NARB_SUMMARY: narb_cspf_probe: No message in Sync-FIFO?");
      return NULL;
    }

  narb_apiserver_msg_print (msg);

  /*write cspf request message*/
  rc = msg_write (oclient_intra->fd_sync, msg);
  msg_free (msg);

  if (rc < 0)
    {
      zlog_warn
        ("narb_cspf_probe: write failed on fd=%d", oclient_intra->fd_sync);
      return NULL;
    }

  /* Synchronous TCP socket write and read. Wait for reply */
  msg = msg_read (oclient_intra->fd_sync);
  if (!msg)
    return NULL;
  assert (msg->hdr.msgtype == MSG_NARB_CSPF_REPLY);
  rmsg = (struct msg_narb_cspf_reply *) STREAM_DATA (msg->s);
  tlv = &rmsg->tlv;
  len = ntohs(tlv->length);
  if (ntohs(tlv->type) == TLV_TYPE_NARB_ERO)
    {
      link = XMALLOC(MTYPE_TMP, sizeof(struct link_info));
      memset(link, 0, sizeof(struct link_info));
      link->adv_id.s_addr = svc_probe->router->id.s_addr;
      link->id.s_addr = router->id.s_addr;
      link->type = 1;
      router_ip  = (struct in_addr *)((char *)tlv + sizeof(struct te_tlv_header));
      link->loc_if.s_addr = router_ip->s_addr;
      SET_LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_LOC_IF);
      router_ip  = (struct in_addr *)((char *)tlv 
                        + len + sizeof(struct te_tlv_header) - sizeof(struct in_addr));
      link->rem_if.s_addr = router_ip->s_addr;
      SET_LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_REM_IF);
      link->ifswcap.switching_cap = (u_char)svc_probe->service->sw_type;
      link->ifswcap.encoding = (u_char)svc_probe->service->enc_type;
      SET_LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_IFSW_CAP);
      link->max_bw = link->max_rsv_bw  = svc_probe->service->max_bw;
      SET_LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_MAX_BW);
      SET_LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_MAX_RSV_BW);
      if (svc_probe->router->type == RT_TYPE_HOST || router->type == RT_TYPE_HOST)
        link->metric = HOST_BORDER_METRIC;
      else if (svc_probe->router->type == RT_TYPE_BORDER && router->type == RT_TYPE_BORDER)
        link->metric = BORDER_BORDER_METRIC;
      else
        link->metric = 1;
      SET_LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_METRIC);
      
      /*free message when handling done*/
      msg_free (msg);
      return link;
    }

  /*free message when handling done*/
  msg_free (msg);
  return NULL;
}

void
narb_probe_virtual_links(struct narb_domain_info *p_domain_info)
{
  listnode probe_node, router_node;
  struct svc_probe *svc_probe;
  struct router_id_info *router;
  struct link_info *link;

  assert(p_domain_info->svc_probes);

  /* ensure that oclient_intra is alive */
  if (!oclient_intra || !ospf_apiclient_alive(oclient_intra))
    {
      zlog_warn("narb_probe_virtual_links failed: intRA-domain OSPFd is dead!");
      return;
    }
    
  probe_node = listhead(p_domain_info->svc_probes);
  while(probe_node)
    {
      svc_probe = getdata(probe_node);
      router_node = listhead(p_domain_info->router_ids);
      while(router_node)
        {
          router = getdata(router_node);
          if ( (svc_probe->router->type == RT_TYPE_HOST && router->type == RT_TYPE_BORDER)
            || (svc_probe->router->type == RT_TYPE_BORDER && router != svc_probe->router) )
            {
              link = narb_cspf_probe(svc_probe, router);
              if (link)
                {
                  listnode_add(p_domain_info->te_links, link);
                  SET_LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_AUTO_PROBED);
                }
            }
          nextnode(router_node);
        }
      nextnode(probe_node);
    }
}

void
narb_cleanup_probed_links(struct narb_domain_info *p_domain_info)
{
  listnode node;
  struct link_info *link;

  node = listhead(p_domain_info->te_links);

  while (node)
    {
      link = getdata(node);
      if (LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_AUTO_PROBED))
        {
          nextnode(node);
          listnode_delete(p_domain_info->te_links, link);
          XFREE(MTYPE_TMP, link);
        }
      else
        {
          nextnode(node);
        }
    }
}
