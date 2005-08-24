/************** narb_vty.c ***************/

#include <zebra.h>

#include <sys/un.h>
#include <setjmp.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/stat.h>
 #include <fcntl.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "command.h"
#include "memory.h"
#include "vector.h"
#include "vty.h"
#include "prefix.h"
#include "thread.h"
#include "table.h"
#include "log.h"
#include "linklist.h"
#include "stream.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_te.h"
#include "ospfd/ospf_te_lsdb.h"
#include "ospfd/ospf_opaque.h"
#include "ospfd/ospf_api.h"
#include "ospf_apiclient.h"

#include "ospf_apiclient.h"
#include "narb_vty.h"
#include "narb_summary.h"
#include "narb_config.h"
#include "narb_apiserver.h"
#include "narb_rceapi.h"
#include "dragon/dragond.h"

char narb_prompt[50] = "%s(narb)# ";
char topology_prompt[50] = "%s(narb-topology)# ";
char te_link_prompt[50] = "%s(narb-update-link)# ";
char * vty_password = NULL;

extern struct thread_master * master;
extern struct dragon_master dmaster;
extern struct narb_domain_info narb_domain_info;
extern struct ospf_apiclient * oclient_inter;
extern struct ospf_apiclient * oclient_intra;

extern struct cmd_node config_node;
extern struct cmd_node auth_node;
extern struct cmd_node view_node;
extern struct cmd_node lsp_node;

extern vector cmdvec;
extern struct host host;
extern char *dragon_motd;
extern vector Vvty_serv_thread;
extern vector vtyvec;
extern char *vty_cwd;

extern struct cmd_element config_write_terminal_cmd;
extern struct cmd_element config_write_file_cmd;
extern struct cmd_element config_write_cmd;

extern int flag_routing_mode;
extern int single_domain_mode;
extern unsigned int NARB_API_SYNC_PORT;

int
ospf_apiclient_alive (struct ospf_apiclient *oc)
{
  if (!oc)
    return 0;
  
  return !oc->disconnected;
}

struct cmd_node narb_node =
{
  NARB_NODE,
  narb_prompt,
};

struct cmd_node topology_node =
{
  TOPOLOGY_NODE,
  topology_prompt,
};

struct cmd_node te_link_node =
{
  TE_LINK_NODE,
  te_link_prompt,
};

struct link_info * link_to_update = NULL;

DEFUN (narb_show_topology,
       narb_show_topology_cmd,
        "show topology (local|global)",
       SHOW_STR
       "Show pre-configued topology\n")
{
  listnode node;
  struct router_id_info * router;
  struct link_info * link;
  char addr_buf1[20], addr_buf2[20], addr_buf3[20], addr_buf4[20];
  
  if (strcmp(argv[0], "local") == 0)
    {
    
      assert (narb_domain_info.router_ids);
      assert (narb_domain_info.te_links);
      assert (narb_domain_info.inter_domain_te_links);
      
      vty_out (vty, "%s\t A total of %d Router ID's  in domain summary %s", VTY_NEWLINE,
          narb_domain_info.router_ids->count, VTY_NEWLINE);
      node = listhead(narb_domain_info.router_ids);
      while (node)
        {
          router = (struct router_id_info*)node->data;
          strcpy (addr_buf1, inet_ntoa (router->id));
          strcpy (addr_buf2,  inet_ntoa (router->adv_id));
          vty_out (vty, "Opaue ID (%d): id (%s), adv_router (%s)%s", router->opaque_id,
                addr_buf1, addr_buf2, VTY_NEWLINE);
           nextnode(node);
        }

      vty_out (vty, "%s\t   A total of %d TE Links  in domain summary %s", VTY_NEWLINE,
            narb_domain_info.te_links->count, VTY_NEWLINE);
      node = listhead(narb_domain_info.te_links);
      while (node)
        {
          link = (struct link_info*)node->data;
          strcpy (addr_buf1, inet_ntoa (link->id));
          strcpy (addr_buf2, inet_ntoa (link->adv_id));
          strcpy (addr_buf3, inet_ntoa (link->loc_if));
          strcpy (addr_buf4, inet_ntoa (link->rem_if));

          vty_out (vty, "Opaue ID (%d): {adv_router (%s)->link_id (%s)} {lcl_if(%s)->rmt_if(%s)} \
{metric (%d), sw_cap (%s), encoding (%s)}%s",
                link->opaque_id, addr_buf2, addr_buf1, addr_buf3, addr_buf4,
                link->metric, value_to_string(&str_val_conv_swcap, (u_int32_t)link->ifswcap.switching_cap), 
                value_to_string(&str_val_conv_encoding, (u_int32_t)link->ifswcap.encoding), VTY_NEWLINE);
           nextnode(node);
        }

      vty_out (vty, "%s \t   %d inter-domain TE Links  in domain summary %s", VTY_NEWLINE,
              narb_domain_info.inter_domain_te_links->count, VTY_NEWLINE);
      node = listhead(narb_domain_info.inter_domain_te_links);
      while (node)
        {
          vty_out (vty, "Inter-domain TE Link ID (%s) %s", inet_ntoa(*(struct in_addr*)node->data), 
                VTY_NEWLINE);
           nextnode(node);
        }
      vty_out (vty, "\t  \t .......The End.......%s %s", VTY_NEWLINE,VTY_NEWLINE);
  }
  else
    {
      if (!oclient_inter)
        {
          vty_out(vty, "\t Warning: intERdomain OSPF apiclient is down%s", VTY_NEWLINE);
          return CMD_WARNING;
        }

      vty_out (vty, "\t A total of %d records found in NARB-LSDB %s", 
        (int)oclient_inter->lsdb->count,  VTY_NEWLINE);
      
      node = listhead(oclient_inter->lsdb);
      while (node)
        {
          u_int16_t type;
          
          struct lsa_header *header;
          struct te_tlv_header *tlv;
          struct ospf_lsa *lsa = getdata(node);
          
          nextnode(node);
          if (!lsa)
            continue;
          
          header = lsa->data;
          tlv = (struct te_tlv_header *)((char *)header + sizeof(struct lsa_header));
          type = ntohs(tlv->type);
          
          switch (type)
            {
            case ROUTER_ID_TE_LSA:
              vty_out (vty, "Router ID TE LSA: ");
              break;
            case LINK_TE_LSA:
              vty_out (vty, "Link TE LSA:      ");
              break;
            default:
              vty_out (vty, "Unknown LSA:   ");
            }

          strcpy (addr_buf1, inet_ntoa (header->id));
          strcpy (addr_buf2,  inet_ntoa (header->adv_router));
          vty_out (vty, "Opaque ID (%s), Adv_router (%s), Length (%d) %s", addr_buf1, 
                addr_buf2, ntohs (header->length), VTY_NEWLINE);
        }
      vty_out (vty, "\t  \t .......The End.......%s %s", VTY_NEWLINE,VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

DEFUN (narb_use_topology,
       narb_use_topology_cmd,
        "use topology FILE",
       SHOW_STR
       "Using a different topology file for configuration\n"
       "Path of the topology file\n")
{
  int ret;
  vector vline;


  vline = cmd_make_strvec ("delete topology");
  ret = cmd_execute_command_strict (vline, vty, NULL);
  cmd_free_strvec(vline);

  assert(argv[0]);
  narb_read_config(argv[0], NULL, NULL, &narb_domain_info);

  vline = cmd_make_strvec ("connect ospfd interdomain");
  ret = cmd_execute_command_strict (vline, vty, NULL);
  cmd_free_strvec(vline);

  vline = cmd_make_strvec ("connect ospfd intradomain");
  ret = cmd_execute_command_strict (vline, vty, NULL);
  cmd_free_strvec(vline);
  
  vline = cmd_make_strvec ("originate topology");
  ret = cmd_execute_command_strict (vline, vty, NULL);
  cmd_free_strvec(vline);
  
  return CMD_SUCCESS;
}

/* Trying connection to protocol daemon. */
int
module_connect (char * host, int port)
{
  int ret;
  struct hostent *hp;
  int sock;
  struct sockaddr_in addr;
  /*struct timeval timeout;*/

  hp = gethostbyname (host);
  if (!hp)
      return -1;

  sock = socket (AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
      return -1;

  memset (&addr, 0, sizeof (struct sockaddr_in));
  addr.sin_family = AF_INET;
  memcpy (&addr.sin_addr, hp->h_addr, hp->h_length);
  addr.sin_port = htons(port);
  ret = connect (sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
  close (sock);
  return ret;
}


DEFUN (narb_configure,
       narb_configure_cmd,
        "configure narb",
       "Configure system parameters\n")
{
  vty->node = NARB_NODE;
  return CMD_SUCCESS;
}

DEFUN (narb_show_module_status,
       narb_show_module_status_cmd,
       "show module",
       SHOW_STR
       "Show status of software modules\n")
{
  int connected;
  int alive;
  listnode node;
  struct if_narb_info *if_narb;
  
  /* print header */
  vty_out (vty, "                  **NARB Module Status** %s%s", VTY_NEWLINE, VTY_NEWLINE);
  vty_out (vty, "Module                 IP/Port              Status         Connection%s", VTY_NEWLINE);

  vty_out (vty, "intER-domain OSPFd   %s/%-13d ",
        narb_domain_info.ospfd_inter.addr, narb_domain_info.ospfd_inter.port);
  connected = module_connect(narb_domain_info.ospfd_inter.addr,
        narb_domain_info.ospfd_inter.port);
  if (connected == 0)
    {
       alive = ospf_apiclient_alive (oclient_inter);
       if (alive)
         vty_out (vty, "online       connected %s", VTY_NEWLINE);
       else
         vty_out (vty, "online       disconnected %s", VTY_NEWLINE);
    }
  else
    {
    	vty_out (vty, "offline      disconnected %s", VTY_NEWLINE);
    }

  vty_out (vty, "intRA-domain OSPFd   %s/%-13d ",
        narb_domain_info.ospfd_intra.addr, narb_domain_info.ospfd_intra.port);
  connected = module_connect(narb_domain_info.ospfd_intra.addr, 
        narb_domain_info.ospfd_intra.port);
  if (connected == 0)
    {
       alive = ospf_apiclient_alive (oclient_intra);
       if (alive)
         vty_out (vty, "online       connected %s", VTY_NEWLINE);
       else
         vty_out (vty, "online       disconnected %s", VTY_NEWLINE);
    }
  else
    {
    	vty_out (vty, "offline      disconnected %s", VTY_NEWLINE);
    }

  if (RCE_HOST_ADDR != NULL)
    {
      vty_out (vty, "resource comp engine  %s/%-13d ",
        RCE_HOST_ADDR, RCE_API_PORT);
        connected = module_connect(RCE_HOST_ADDR, RCE_API_PORT);
      if (connected == 0)
        {
           if (rce_api_sock > 0)
             vty_out (vty, "online       connected %s", VTY_NEWLINE);
           else
             vty_out (vty, "online       disconnected %s", VTY_NEWLINE);
        }
      else
        {
        	vty_out (vty, "offline      disconnected %s", VTY_NEWLINE);
        }
    }

  if (narb_domain_info.if_narb_table != NULL)
    {
      node = listhead(narb_domain_info.if_narb_table);
      while(node)
        {
          char ip[20];
          listnode node_inner;
          struct in_addr *if_addr;
          if_narb = (struct if_narb_info*)getdata(node);
          
          vty_out (vty, "Next-domain  NARB    %s/%-13d ",
              if_narb->addr, if_narb->port);
          connected = module_connect(if_narb->addr, if_narb->port);
           if (connected == 0)
             vty_out (vty, "online");
           else
             vty_out (vty, "offline");
           nextnode(node);
           vty_out(vty, "       via %s %s",
             inet_ntop(AF_INET, if_narb->if_addr_list->head->data, ip, 20), VTY_NEWLINE);
           
           node_inner = if_narb->if_addr_list->head->next;
           while(node_inner)
            {
              if_addr = getdata(node_inner);
              vty_out(vty, "\t\t\t\t\t\talso     via %s %s",
                inet_ntop(AF_INET, if_addr, ip, 20), VTY_NEWLINE);
              
              nextnode(node_inner);
            }
      	}
    }
  return CMD_SUCCESS;
}

DEFUN (narb_set_ospfd_interdomain,
       narb_set_ospfd_interdomain_cmd,
        "set ospfd interdomain HOST  LCL_PORT RMT_PORT ORI_IF AREA",
        SHOW_STR
       "Configure interdomain ospfd parameters\n"
       "continue... \n"
       "Host of the OSPFd\n"
       "The local sync port on the NARB host\n"
       "The apiserver port on the OSPFd host\n"
       "The interface address on OSPFd through which LSA's are originated\n"
       "OSPF area ID")
{
  strcpy(narb_domain_info.ospfd_inter.addr, argv[0]);
  narb_domain_info.ospfd_inter.localport = atoi(argv[1]);
  narb_domain_info.ospfd_inter.port = atoi(argv[2]);
  inet_aton(argv[3], &narb_domain_info.ospfd_inter.ori_if);  
  inet_aton(argv[4], &narb_domain_info.ospfd_inter.area);  
  return CMD_SUCCESS;
}

DEFUN (narb_set_peer,
       narb_set_peer_cmd,
        "set narb-peer HOST port PORT via IF_ADDR",
        SHOW_STR
       "Configure peering NARB\n"
       "Host of the peering NARB\n"
       "The port on the NARB host\n"
       "The interface that the NARB is associaed with\n")
{
  struct in_addr if_addr;
  inet_aton(argv[2], &if_addr);
  if_narb_add(narb_domain_info.if_narb_table, argv[0], 
        atoi(argv[1]), if_addr);
  
  return CMD_SUCCESS;
}

DEFUN (narb_set_routing_mode,
       narb_set_routing_mode_cmd,
        "set routing-mode (all-strict-only|mixed-allowed|mixed-preferred|all-loose-allowed)",
        SHOW_STR
       "Configure NARB routing mode\n")
{
  if(strcmp(argv[0], "all-strict-only") == 0)
    flag_routing_mode = RT_MODE_ALL_STRICT_ONLY;
  else if(strcmp(argv[0], "mixed-allowed") == 0)
    flag_routing_mode = RT_MODE_MIXED_ALLOWED;
  else if(strcmp(argv[0], "mixed-preferred") == 0)
    flag_routing_mode = RT_MODE_MIXED_PREFERRED;
  else if(strcmp(argv[0], "all-loose-allowed") == 0)
    flag_routing_mode = RT_MODE_ALL_LOOSE_ALLOWED;
  vty_out(vty, "Routing mode changed succesfully. %s\
The new mode will be effective for new requests.%s\
However, it does not apply to requests in progress. %s", VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE);
  
  return CMD_SUCCESS;
}

DEFUN (narb_set_working_mode,
       narb_set_working_mode_cmd,
        "set working-mode (single-domain-mode|multi-domain-mode)",
        SHOW_STR
       "Configure NARB working mode\n")
{
  if(strcmp(argv[0], "single-domain-mode") == 0)
    single_domain_mode = 1;
  else if (strcmp(argv[0], "multi-domain-mode") == 0)
    single_domain_mode = 0;  
  
  return CMD_SUCCESS;
}

DEFUN (narb_set_ospfd_intradomain,
       narb_set_ospfd_intradomain_cmd,
        "set ospfd intradomain HOST LCL_PORT RMT_PORT ORI_IF AREA",
        SHOW_STR
       "Configure intradomain ospfd parameters\n"
       "continue... \n"
       "Host of the OSPFd\n"
       "The local sync port on the NARB host\n"
       "The apiserver port on the OSPFd host\n"
       "The interface address on OSPFd through which LSA's are originated\n"
       "OSPF area ID")
{
  strcpy(narb_domain_info.ospfd_intra.addr, argv[0]);
  narb_domain_info.ospfd_intra.localport = atoi(argv[1]);
  narb_domain_info.ospfd_intra.port = atoi(argv[2]);
  inet_aton(argv[3], &narb_domain_info.ospfd_intra.ori_if);  
  inet_aton(argv[4], &narb_domain_info.ospfd_intra.area);  
  return CMD_SUCCESS;
}

DEFUN (narb_connect_ospfd,
       narb_connect_ospfd_cmd,
        "connect ospfd (interdomain | intradomain)",
       "(Re)connect to an OSPFd\n")
{
  int ret;
  
  if (strcmp(argv[0], "interdomain") ==0)
    {
      if (oclient_inter)
        {
          vty_out(vty, "IntER-domain OSPFd apiclient has already existed.\
%s Please disconnect before reconnecting.%s", VTY_NEWLINE, VTY_NEWLINE);
          /* ospf_apiclient_close(oclient_inter);*/
          return CMD_WARNING;
        }

      oclient_inter = ospf_apiclient_connect(narb_domain_info.ospfd_inter.addr, 
            narb_domain_info.ospfd_inter.localport, narb_domain_info.ospfd_inter.port);
      if (!oclient_inter)
        {
          vty_out(vty, "\t Warning: connecting to intER-domain OSPFd failed!%s", VTY_NEWLINE);
          return CMD_WARNING;
        }
      
      /* register opaque type with OSPFd_inter for originating opaque LSA's*/
      ret = ospf_apiclient_register_opaque_type (oclient_inter, 10, 1);
      if (ret < 0)
        {
          perror("ospf_apiclient_register_opaque_type[10, 1] (inter) failed !");
        }

      /* originate LSA's to summary the domain topology. A timer thread is 
          started inside to refresh the domain-summary LSA's periodically.*/
      narb_originate_summary(oclient_inter);

      /* start a timer thread to fetch back oapque LSA's from the LSDB of 
          inter-domain OSPFd*/
      if (oclient_inter->t_sync_lsdb)
        {
          thread_cancel(oclient_inter->t_sync_lsdb);
          oclient_inter->t_sync_lsdb = NULL;
        }
      oclient_inter->t_sync_lsdb =
          thread_add_timer (master, ospf_narb_sync_lsdb, 
                oclient_inter, OSPF_NARB_SYNC_LSDB_INVERVAL);      
    }
  else 
    {
      if (oclient_intra)
        {
          vty_out(vty, "IntRA-domain OSPFd apiclient has already existed.\
%s Please disconnect before reconnecting.%s", VTY_NEWLINE, VTY_NEWLINE);
          /*ospf_apiclient_close(oclient_intra);*/
          return CMD_WARNING;
        }
      
      oclient_intra = ospf_apiclient_connect(narb_domain_info.ospfd_intra.addr, 
            narb_domain_info.ospfd_intra.localport, narb_domain_info.ospfd_intra.port);
      if (!oclient_intra)
        {
          vty_out(vty, "\t Warning: connecting to intRA-domain OSPFd failed!%s", VTY_NEWLINE);
          return CMD_WARNING;
        }
    }
  
  return CMD_SUCCESS;
}

DEFUN (narb_disconnect_ospfd,
       narb_disconnect_ospfd_cmd,
        "disconnect ospfd (interdomain | intradomain)",
       "Disconnect to an OSPFd\n")
{
  if (strcmp(argv[0], "interdomain") ==0)
    {
      if (oclient_inter)
        {
          if (oclient_inter->t_originator)
            {
              thread_cancel(oclient_inter->t_originator);
              oclient_inter->t_originator = NULL;
            }
          if (oclient_inter->t_sync_lsdb)
            {
              thread_cancel(oclient_inter->t_sync_lsdb);
              oclient_inter->t_sync_lsdb = NULL;
            }
          if (oclient_inter->t_async_read)
            {
              thread_cancel(oclient_inter->t_async_read);
              oclient_inter->t_async_read = NULL;
            }

          ospf_apiclient_close(oclient_inter);
          oclient_inter = NULL;
        }
    }
  else 
    {
      if (oclient_intra)
        {
          if (oclient_intra->t_async_read)
            {
              thread_cancel(oclient_intra->t_async_read);
              oclient_intra->t_async_read = NULL;
            }

          ospf_apiclient_close(oclient_intra);
          oclient_intra = NULL;
        }
    }
  
  return CMD_SUCCESS;
}

char rce_host_addr[80];
DEFUN (narb_set_rce,
       narb_set_rce_cmd,
        "set rce HOST PORT",
        SHOW_STR
       "Configure resource computation engine (RCE)\n"
       "Host of the RCE\n"
       "The port on the RCE\n")
{
  strcpy(rce_host_addr, argv[0]);
  if (RCE_HOST_ADDR == NULL)
      RCE_HOST_ADDR = rce_host_addr;
  RCE_API_PORT= atoi(argv[1]);
  if (rce_api_sock > 0)
    close(rce_api_sock);

  rce_api_sock = narb_rceapi_connect(RCE_HOST_ADDR, RCE_API_PORT);
  if (rce_api_sock < 0)
  {
      vty_out(vty, "Cannot connect to RCE %s:%d.%s", RCE_HOST_ADDR, RCE_API_PORT, VTY_NEWLINE);
      return CMD_WARNING;
  }
  else
  {
      vty_out(vty, "Connection to RCE %s:%d has been set up.%s", RCE_HOST_ADDR, RCE_API_PORT, VTY_NEWLINE);
  }

  return CMD_SUCCESS;
}

DEFUN (narb_delete_rce,
       narb_delete_rce_cmd,
        "delete rce",
        SHOW_STR
       "delete resource computation engine (RCE)\n")
{
  if (rce_api_sock > 0)
  {
      close (rce_api_sock);
      rce_api_sock = -1;
  }

  RCE_HOST_ADDR = NULL;

  return CMD_SUCCESS;
}

DEFUN (narb_show_rce,
       narb_show_rce_cmd,
        "show rce",
        SHOW_STR
       "Show configuration of  resource computation engine (RCE)\n")
{
  if (RCE_HOST_ADDR == NULL)
    {
      vty_out(vty, "RCE not configured. %s", VTY_NEWLINE);
    }
  else
    {
      if (rce_api_sock < 0)
      {
          vty_out(vty, "RCE set on %s:%d (disconnected).%s", RCE_HOST_ADDR, RCE_API_PORT, VTY_NEWLINE);
      }
      else
      {
          vty_out(vty, "RCE set on %s:%d (connected).%s", RCE_HOST_ADDR, RCE_API_PORT, VTY_NEWLINE);
      }
    }
  return CMD_SUCCESS;
}

DEFUN (narb_set_holdon,
       narb_set_holdon_cmd,
        "set holdon (yes|no)",
        SHOW_STR
       "Force narb holdon status to be set or released. \n"
       "yes: holdon; no: released\n")
{
  if (strcmp(argv[0], "yes") == 0)
    {
        flag_holdon = 1;
        vty_out(vty, "NARB HOLDON status forced.%s",VTY_NEWLINE);
    }
  else
    {
        flag_holdon = 0;
        vty_out(vty, "NARB HOLDON status released.%s",VTY_NEWLINE);
    }

  return CMD_SUCCESS;
}


DEFUN (narb_configure_end,
       narb_configure_end_cmd,
        "exit",
       "Exit from configuration node\n")
{
  vty->node = VIEW_NODE;
  return CMD_SUCCESS;
}

DEFUN (narb_topology,
       narb_topology_cmd,
        "topology",
       "Configure abstract topologies\n")
{
  vty->node = TOPOLOGY_NODE;
  return CMD_SUCCESS;
}


DEFUN (narb_topology_delete,
       narb_topology_delete_cmd,
        "delete topology",
       "Delete the currently originated topology\n")
{
  vty_out(vty, "Deleting topology...%s", VTY_NEWLINE);
  if (!oclient_inter || !ospf_apiclient_alive(oclient_inter))
    {
      vty_out(vty, "intER-domain OSPFd is not available!%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  narb_delete_summary(oclient_inter);

  return CMD_SUCCESS;
}

DEFUN (narb_topology_originate,
       narb_topology_originate_cmd,
        "originate topology",
       "Originate or refresh the currently configured topology\n")
{
  vty_out(vty, "Originating/Refreshing topology...%s", VTY_NEWLINE);
  if (!oclient_inter || !ospf_apiclient_alive(oclient_inter))
    {
      vty_out(vty, "intER-domain OSPFd is not available!%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  narb_delete_summary(oclient_inter);
  narb_originate_summary(oclient_inter);

  return CMD_SUCCESS;
}

extern int narb_topology_refresh_interval;

DEFUN (narb_set_topology_refresh_interval,
       narb_set_topology_refresh_interval_cmd,
        "set refresh-interval SECONDS",
        "Set the reconfigurable refresh interval\n")
{
  int interval = atoi(argv[0]);

  if (interval < 60 || interval > 3500)
    {
      vty_out(vty, "The  interval value must be between 60 and 3500 (seconds) %s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  narb_topology_refresh_interval = interval;

  if (oclient_inter)
    {
      /* cancel the old refresh thread and do refresh now */
      narb_delete_summary(oclient_inter);
      narb_originate_summary(oclient_inter);
    }

  return CMD_SUCCESS;
}


DEFUN (narb_topology_end,
       narb_topology_end_cmd,
        "exit",
        "Exiting topology configuration mode\n")
{
  vty->node = VIEW_NODE;
  return CMD_SUCCESS;
}

DEFUN (narb_update_link,
       narb_update_link_cmd,
        "update link LCL_IF_ADDR RMT_IF_ADDR",
       "Update TE link parameters\n")
{
  struct in_addr lcl_if, rmt_if;
  struct link_info * link;

  inet_aton(argv[0], &lcl_if);
  inet_aton(argv[1], &rmt_if);

  link = narb_lookup_link_by_if_addr(narb_domain_info.te_links, &lcl_if, &rmt_if);

  if (link)
    {
      link_to_update = XMALLOC(MTYPE_TMP, sizeof(struct link_info));
      memcpy(link_to_update, link, sizeof(struct link_info));
      link_to_update->opaque_id = narb_ospf_opaque_id();
      
      vty->node = TE_LINK_NODE;
    }
  else
    {
      vty_out(vty, "TE-Link {%s->%s} not found (not originated in this domain?)!%s", argv[0], argv[1], VTY_NEWLINE);
      return CMD_WARNING;
    }
  
  return CMD_SUCCESS;
}

DEFUN (narb_set_link,
       narb_set_link_cmd,
        "set (link_id | adv_router |metric | lcl_if |rmt_if) VALUE",
       "Set TE link parameters\n")
{
  assert (link_to_update);

  if (strcmp(argv[0], "link_id") == 0)
    {
      inet_aton(argv[1], &link_to_update->id);
    }
  else if (strcmp(argv[0], "adv_router") == 0)
    {
      inet_aton(argv[1], &link_to_update->adv_id);
    }
  else if (strcmp(argv[0], "metric") == 0)
    {
      link_to_update->metric = atoi(argv[1]);
      SET_LINK_PARA_FLAG(link_to_update->info_flag, LINK_PARA_FLAG_METRIC);
    }
  else if (strcmp(argv[0], "lcl_if") == 0)
    {
      inet_aton(argv[1], &link_to_update->loc_if);
      SET_LINK_PARA_FLAG(link_to_update->info_flag, LINK_PARA_FLAG_LOC_IF);
    }
  else if (strcmp(argv[0], "rmt_if") == 0)
    {
      inet_aton(argv[1], &link_to_update->rem_if);
      SET_LINK_PARA_FLAG(link_to_update->info_flag, LINK_PARA_FLAG_REM_IF);
    }

  return CMD_SUCCESS;  
}

DEFUN (narb_set_link_sw,
       narb_set_link_sw_cmd,
        "set sw_capability (lsc|tdm|psc1|psc2|psc3|psc4) encoding (lambda|ethernet|packet|sdh)",
       "Set TE link interface switching capability\n")
{
  assert (link_to_update);

  link_to_update->ifswcap.switching_cap = string_to_value(&str_val_conv_swcap, argv[0]);
  link_to_update->ifswcap.encoding = string_to_value(&str_val_conv_encoding, argv[1]);
  SET_LINK_PARA_FLAG(link_to_update->info_flag, LINK_PARA_FLAG_IFSW_CAP);

  return CMD_SUCCESS;  
}


void print_link_info (struct vty *vty, struct link_info *link)
{
  char addr_buf1[20], addr_buf2[20], addr_buf3[20], addr_buf4[3];

  assert (link);
  
  strcpy (addr_buf1, inet_ntoa (link->id));
  strcpy (addr_buf2, inet_ntoa (link->adv_id));
  strcpy (addr_buf3, inet_ntoa (link->loc_if));
  strcpy (addr_buf4, inet_ntoa (link->rem_if));

  vty_out (vty, "TE-LINK (OpqID %d):  {adv_router (%s)->id (%s)} %s", link->opaque_id,
                addr_buf2, addr_buf1, VTY_NEWLINE);
  vty_out (vty, "\t Interfaces: {lcl_if (%s)->rmt_if (%s)} %s",
                addr_buf3, addr_buf4, VTY_NEWLINE);
  vty_out (vty, "\t \t {metric = %d, sw_cap = %s, encoding = %s} %s", 
                link->metric, value_to_string(&str_val_conv_swcap, (u_int32_t)link->ifswcap.switching_cap),
                value_to_string(&str_val_conv_encoding, (u_int32_t)link->ifswcap.encoding), VTY_NEWLINE);
}
      
DEFUN (narb_show_link,
       narb_show_link_cmd,
        "show link (updated | original)",
       "Show TE link parameters\n")
{
  struct link_info * link;
  
  assert (link_to_update);

  if (strcmp(argv[0], "updated") == 0)
    {
      /*show info of link_to_update*/
      print_link_info (vty, link_to_update);
    }
  else
    {
      link = narb_lookup_link_by_if_addr(narb_domain_info.te_links, 
              &link_to_update->loc_if, &link_to_update->rem_if);
      assert (link);
      /*show info of link*/
      print_link_info (vty, link);
    }

  return CMD_SUCCESS;  
}
  
DEFUN (narb_update_link_cancel,
       narb_update_link_cancel_cmd,
        "cancel",
       "Cancel updating TE link parameters\n")
{
  if (link_to_update)
    {
      XFREE(MTYPE_TMP, link_to_update);
      link_to_update = NULL;
    }
  
  vty->node = TOPOLOGY_NODE;
  return CMD_SUCCESS;
}

DEFUN (narb_update_link_commit,
       narb_update_link_commit_cmd,
        "commit",
       "Commit updating TE link parameters\n")
{
  struct link_info * link;

  assert (link_to_update);

  if (!oclient_inter || !ospf_apiclient_alive (oclient_inter))
    {
      vty_out(vty, "IntER-domain OSPFd connection is dead now.%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  else
    {
      vty_out(vty, "Sending update to intER-domain OSPFd.%s", VTY_NEWLINE);
    }

  link = narb_lookup_link_by_if_addr(narb_domain_info.te_links, 
            &link_to_update->loc_if, &link_to_update->rem_if);

  ospf_apiclient_lsa_delete(oclient_inter, link->adv_id, narb_domain_info.ospfd_inter.area,
      10, 1, link->opaque_id);

  narb_originate_te_link(oclient_inter, link_to_update);

  assert (link);

  memcpy(link, link_to_update, sizeof(struct link_info));
  XFREE(MTYPE_TMP, link_to_update);
  link_to_update = NULL;

  vty->node = TOPOLOGY_NODE;
  return CMD_SUCCESS;
}

static struct narb_domain_info * p_intra_domain_info = NULL;

DEFUN (narb_test_originate_intradomain_topology,
       narb_test_originate_intradomain_topology_cmd,
        "test originate FILE",
        "intRA-domain topology operation, "
        "a testing feature\n"
       "Originate the manually configued intRA-domain topology\n")
{
  listnode node;
  vector vline;

  if (!oclient_intra | !ospf_apiclient_alive(oclient_intra))
    {
      vty_out(vty, "The intRA-domain OSPF API client is dead!%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (p_intra_domain_info)
    {
      vline = cmd_make_strvec ("intradomain topology delete");

      vty_out(vty, "Deleting the old topology%s", VTY_NEWLINE);
      cmd_execute_command_strict (vline, vty, NULL);
    }

  p_intra_domain_info = XMALLOC(MTYPE_TMP, sizeof(struct narb_domain_info));
  
  if (narb_read_config(argv[0], NULL, NULL, p_intra_domain_info) < 0)
    {
      XFREE(MTYPE_TMP, p_intra_domain_info);
      p_intra_domain_info = NULL;
      vty_out(vty, "Topology file (%s) doesn'e exist! %s", argv[0], VTY_NEWLINE);
      return CMD_WARNING;
    }

  /*originate router-id LSA's*/
  vty_out(vty, "Originating %d Router-id TE LSA's %s", 
            p_intra_domain_info->router_ids->count, VTY_NEWLINE);
  for (node = listhead(p_intra_domain_info->router_ids); node; nextnode(node))
    {
      struct router_id_info * router = node->data;

      if (router->hide)
        continue;
      narb_originate_router_id (oclient_intra, router);
    }
  
  /*originate  te-link LSA's*/
  vty_out(vty, "Originating %d Link TE LSA's %s", 
            p_intra_domain_info->te_links->count, VTY_NEWLINE);
  for (node = listhead(p_intra_domain_info->te_links); node; nextnode(node))
    {
      struct link_info * link = node->data;

      if (link->hide)
        continue;
      narb_originate_te_link(oclient_intra, link);
    }

  return CMD_SUCCESS;
}


DEFUN (narb_test_delete_intradomain_topology,
       narb_test_delete_intradomain_topology_cmd,
        "test delete",
        "intRA-domain topology operation, "
        "a testing feature\n"
        "Delete the manually originated intRA-domain topology\n")
{
  listnode node;

  if (!oclient_intra | !ospf_apiclient_alive(oclient_intra))
    {
      vty_out(vty, "The intRA-domain OSPF API client is dead!%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (!p_intra_domain_info)
    {
      vty_out(vty, "No intRA-domain topology to be deleted!%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  /*delete router-id LSA's*/
  vty_out(vty, "Deleting %d Router-id TE LSA's %s", 
            p_intra_domain_info->router_ids->count, VTY_NEWLINE);

  for (node = listhead(p_intra_domain_info->router_ids); node; nextnode(node))
    {
      struct router_id_info * router = node->data;

      ospf_apiclient_lsa_delete (oclient_intra, 
                      router->adv_id,
                      narb_domain_info.ospfd_intra.area, 10,
                      1, router->opaque_id);
    }
  
  /*delete link TE LSA's*/
  vty_out(vty, "Deleting %d Link TE LSA's %s", 
            p_intra_domain_info->te_links->count, VTY_NEWLINE);
  for (node = listhead(p_intra_domain_info->te_links); node; nextnode(node))
    {
      struct link_info * link = node->data;

      ospf_apiclient_lsa_delete (oclient_intra, 
                      link->adv_id,
                      narb_domain_info.ospfd_intra.area, 10,
                      1, link->opaque_id);
    }

  /* Destroy the intra_domain_info data structure*/
  for (node = listhead(p_intra_domain_info->router_ids); node; nextnode(node))
      XFREE(MTYPE_TMP, node->data);
  list_free(p_intra_domain_info->router_ids);

  for (node = listhead(p_intra_domain_info->te_links); node; nextnode(node))
      XFREE(MTYPE_TMP, node->data);
  list_free(p_intra_domain_info->te_links);

  list_free(p_intra_domain_info->if_narb_table);
  list_free(p_intra_domain_info->svc_probes);
  list_free(p_intra_domain_info->inter_domain_te_links);

  XFREE(MTYPE_TMP, p_intra_domain_info);
  p_intra_domain_info = NULL;
  
  return CMD_SUCCESS;
}

DEFUN (narb_test_delete_link,
       narb_test_delete_link_cmd,
        "test delete link LCL_IF_ADDR RMT_IF_ADDR",
       "Delete a TE link in global scale\n")
{
  struct in_addr lcl_if, rmt_if;
  struct ospf_lsa * lsa;  

  if (!oclient_inter || !ospf_apiclient_alive(oclient_inter))
    {
      vty_out(vty, "IntER-domain OSPFd is down.%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  inet_aton(argv[0], &lcl_if);
  inet_aton(argv[1], &rmt_if);

  lsa = narb_lookup_lsa_by_if_addr(oclient_inter->lsdb, &lcl_if, &rmt_if);

  if (lsa)
    {

      vty_out(vty, "Deleting LSA id (%x) adv_router (%x) %s", lsa->data->id.s_addr, lsa->data->adv_router.s_addr, VTY_NEWLINE);
      ospf_apiclient_lsa_delete(oclient_inter, lsa->data->adv_router, 
        narb_domain_info.ospfd_inter.area, 10, 1, ntohl(lsa->data->id.s_addr));
    }
  else
    {
      vty_out(vty, "TE-Link {%s->%s} not found %s", argv[0], argv[1], VTY_NEWLINE);
      return CMD_WARNING;
    }
  
  return CMD_SUCCESS;
}

/*DRAGON RSVP client functions*/
extern int  dragon_edit_lsp_cmd (struct cmd_element *, struct vty *, int, char **);
extern int  dragon_show_lsp_cmd (struct cmd_element *, struct vty *, int, char **);
extern int  dragon_show_lsp_all_cmd (struct cmd_element *, struct vty *, int, char **);
extern int  dragon_commit_lsp_sender_cmd (struct cmd_element *, struct vty *, int, char **);
extern int  dragon_commit_lsp_default_cmd (struct cmd_element *, struct vty *, int, char **);
extern int dragon_delete_lsp_cmd (struct cmd_element *, struct vty *, int, char **);
extern int dragon_set_lsp_name_cmd (struct cmd_element *, struct vty *, int, char **);
extern int dragon_set_lsp_ip_cmd (struct cmd_element *, struct vty *, int, char **);
extern int dragon_set_lsp_sw_cmd (struct cmd_element *, struct vty *, int, char **);
extern int dragon_set_lsp_dir_cmd (struct cmd_element *, struct vty *, int, char **);
extern int dragon_set_label_set_cmd (struct cmd_element *, struct vty *, int, char **);
extern int dragon_master_init();

#if 0
DEFUN (narb_commit_lsp,
       narb_commit_lsp_cmd,
       "commit lsp NAME",
       "Commit a GMPLS label switching path (LSP)\n"
       "Label switching path\n"
       "LSP name\n")
{
  struct listnode *node;
  struct lsp *lsp = NULL;
  int found;
  struct msg *rmsg;
  char *data = NULL;
  struct dragon_tlv_header *tlvh;

  found = 0;
  LIST_LOOP(dmaster.dragon_lsp_table, lsp , node)
  {
  	if (strcmp((lsp->common.SessionAttribute_Para)->sessionName, argv[0])==0)
  	{
  		if (lsp->status != LSP_EDIT)
  		{
			vty_out (vty, "The LSP %s is not in edit state. %s", argv[0], VTY_NEWLINE);
			return CMD_WARNING;
  		}
		else
		{
			found = 1;
			break;
		}
  	}
  }
  if (!found)
  {
  	vty_out (vty, "Unable to find LSP %s. %s", argv[0], VTY_NEWLINE);
	return CMD_WARNING;
  }
  if (!is_mandated_params_set_for_lsp(lsp))
  {
  	vty_out (vty, "Mandated parameter not set for lsp %s. %s", argv[0], VTY_NEWLINE);
	return CMD_WARNING;
  }

  /* Assign a unique sequence number */
  lsp->seqno = dragon_assign_seqno();

  /*create a socket connect to self */
  /*create and send a CSPF route request */
  rmsg = narb_self_route_request (lsp);

  if (rmsg)
      data = STREAM_DATA(rmsg->s);
  else 
      return CMD_ERR_INCOMPLETE;
  
  if (rmsg->hdr.msgtype == MSG_REPLY_ERROR)
    {
      u_int16_t errcode = ntohs(*(u_int16_t *)data);
      vty_out(vty, "Route request failed (error code : %d)%s", errcode, VTY_NEWLINE);
      msg_free(rmsg);
      return CMD_WARNING;
    }
  else if (rmsg->hdr.msgtype == MSG_REPLY_ERO)
    {
      tlvh = DTLV_HDR_TOP(STREAM_DATA(rmsg->s));
      if (lsp->common.EROAbstractNode_Para)
        XFREE(MTYPE_OSPF_DRAGON, lsp->common.EROAbstractNode_Para);
      lsp->common.EROAbstractNode_Para = dragon_narb_topo_rsp_proc_ero(tlvh, &lsp->common.ERONodeNumber);
      
      /*start PATH Message */
      /* call RSVPD to set up the path */
      zInitRsvpPathRequest(dmaster.api, &lsp->common, 1);

      /*$$$ no retry timer ? ... */

      /* Set commit flag */
      lsp->status = LSP_COMMIT; 
      msg_free(rmsg);
    }
  else 
    {
      vty_out(vty, "Unknown response message%s", VTY_NEWLINE);
      msg_free(rmsg);
      return CMD_ERR_INCOMPLETE;
    }

  return CMD_SUCCESS;
}

struct msg *
narb_self_route_request ( struct lsp *lsp)
{
  struct msg *msg;
  struct msg_app2narb_request * msgbody;
  struct narb_apiserver *narb_apiserv;

  /* compose request message */
  msgbody = malloc(sizeof(struct msg_app2narb_request));
  memset(msgbody, 0, sizeof(struct msg_app2narb_request));
  msgbody->type = htons(TLV_TYPE_NARB_REQUEST);
  msgbody->length = htons(sizeof(struct msg_app2narb_request) - 4);
  msgbody->src = lsp->common.Session_Para.srcAddr;
  msgbody->dest = lsp->common.Session_Para.destAddr;
  msgbody->encoding_type = lsp->common.LabelRequest_Para.data.gmpls.lspEncodingType;
  msgbody->switching_type = lsp->common.LabelRequest_Para.data.gmpls.switchingType;
  msgbody->gpid = lsp->common.LabelRequest_Para.data.gmpls.gPid;
  msgbody->bandwidth = (lsp->common.GenericTSpec_Para)->R;
  
  msg = msg_new(MSG_APP_REQUEST_EVENT, msgbody, lsp->seqno,
        sizeof(struct msg_app2narb_request));
  XFREE(MTYPE_TMP, msgbody);
  
  /* Allocate new server-side connection structure */
  narb_apiserv = narb_apiserver_new (0);
  /* Add to active connection list */
  listnode_add (narb_apiserver_list, narb_apiserv);


  return msg;
}
#endif

/*******************************************************/
void
narb_vty_init ()
{
  struct thread_master *master_sav;

  /* Allocate initial top vector of commands. */
  cmdvec = vector_init (VECTOR_MIN_SIZE);

  /* Default host value settings. */
  host.name = NULL;
  host.password = vty_password? vty_password: NARB_VTY_DEFAULT_PASSWORD;
  host.enable = NULL;
  host.logfile = NULL;
  host.config = NULL;
  host.lines = -1;
  host.motd = dragon_motd;

  /* Install top nodes. */
  install_node (&narb_node, NULL);
  install_node (&auth_node, NULL);
  install_node (&view_node, NULL);
  install_node(&topology_node, NULL);
  install_node(&te_link_node, NULL);
  install_node(&lsp_node, NULL);
  
  install_element(VIEW_NODE, &config_exit_cmd);
  install_element(VIEW_NODE, &config_quit_cmd);
  install_element(VIEW_NODE, &config_write_terminal_cmd);
  install_element(VIEW_NODE, &config_write_file_cmd);
  install_element(VIEW_NODE, &config_write_cmd);
  install_element(VIEW_NODE, &narb_configure_cmd);
  install_element(VIEW_NODE, &narb_topology_cmd);

  install_element(VIEW_NODE, &dragon_edit_lsp_cmd);
  install_element(VIEW_NODE, &dragon_show_lsp_cmd);
  install_element(VIEW_NODE, &dragon_show_lsp_all_cmd);
  install_element(VIEW_NODE, &dragon_commit_lsp_sender_cmd);
  install_element(VIEW_NODE, &dragon_commit_lsp_default_cmd);  
  install_element(VIEW_NODE, &dragon_delete_lsp_cmd);

  install_default(LSP_NODE);
  install_element(LSP_NODE, &dragon_set_lsp_name_cmd);
  install_element(LSP_NODE, &dragon_set_lsp_ip_cmd);
  install_element(LSP_NODE, &dragon_set_lsp_sw_cmd);
  install_element(LSP_NODE, &dragon_set_lsp_dir_cmd);
  install_element(LSP_NODE, &dragon_set_label_set_cmd);

  install_element(NARB_NODE, &narb_set_ospfd_interdomain_cmd);
  install_element(NARB_NODE, &narb_set_ospfd_intradomain_cmd);
  install_element(NARB_NODE, &narb_set_peer_cmd);
  install_element(NARB_NODE, &narb_set_routing_mode_cmd);
  install_element(NARB_NODE, &narb_set_working_mode_cmd);
  install_element(NARB_NODE, &narb_show_module_status_cmd);
  install_element(NARB_NODE, &narb_configure_end_cmd);
  install_element(NARB_NODE, &narb_connect_ospfd_cmd);
  install_element(NARB_NODE, &narb_disconnect_ospfd_cmd);
  install_element(NARB_NODE, &narb_set_rce_cmd);
  install_element(NARB_NODE, &narb_delete_rce_cmd);
  install_element(NARB_NODE, &narb_show_rce_cmd);
  install_element(NARB_NODE, &narb_set_holdon_cmd);
  
  install_element(TOPOLOGY_NODE, &narb_show_topology_cmd);
  install_element(TOPOLOGY_NODE, &narb_use_topology_cmd);
  install_element(TOPOLOGY_NODE, &narb_topology_delete_cmd);
  install_element(TOPOLOGY_NODE, &narb_topology_originate_cmd);
  install_element(TOPOLOGY_NODE, &narb_topology_end_cmd);
  install_element(TOPOLOGY_NODE, &narb_set_topology_refresh_interval_cmd);
  install_element(TOPOLOGY_NODE, &narb_update_link_cmd);

  install_element(TOPOLOGY_NODE, &narb_test_originate_intradomain_topology_cmd);
  install_element(TOPOLOGY_NODE, &narb_test_delete_intradomain_topology_cmd);
  install_element(TOPOLOGY_NODE, &narb_test_delete_link_cmd);

  install_element(TE_LINK_NODE, &narb_set_link_cmd);
  install_element(TE_LINK_NODE, &narb_set_link_sw_cmd);
  install_element(TE_LINK_NODE, &narb_show_link_cmd);
  install_element(TE_LINK_NODE, &narb_update_link_cancel_cmd);
  install_element(TE_LINK_NODE, &narb_update_link_commit_cmd);
  
  /* For further configuration read, preserve current directory. */
  vty_save_cwd ();
  
  vtyvec = vector_init (VECTOR_MIN_SIZE);
  
  /* Initilize server thread vector. */
  Vvty_serv_thread = vector_init (VECTOR_MIN_SIZE);

  
  /* Initilize thread master for RSVP client.
    Since the dragon_master_init() function will revise the global variable 'master',
    which has already been initiated and in use, 'master_sav' is used to revert this
    revision. */
  master_sav = master;
  dragon_master_init();
  master = dmaster.master = master_sav;
  inet_aton("127.0.0.1", &(dmaster.module[MODULE_NARB_INTRA].ip_addr));
}

void
narb_vty_cleanup ()
{
  if (vty_cwd)
    {
      XFREE(MTYPE_TMP, vty_cwd);
      vty_cwd = NULL;
    }
}

