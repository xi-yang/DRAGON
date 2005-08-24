/*narb_config.c*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "zebra.h"
#include "prefix.h"
#include "linklist.h"
#include "memory.h"
#include "thread.h"
#include "log.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_te.h"
#include "ospfd/ospf_opaque.h"
#include "ospfd/ospf_api.h"
#include "ospf_apiclient.h"
#include "narb_apiserver.h"

#include "narb_summary.h"
#include "narb_config.h"

extern char * vty_password;
extern char * narb_vty_addr;

/* Read up configuration file from config_file. */
int
narb_read_config (char *config_file,
		 char *config_current_dir,
		 char *config_default_dir,
		 struct narb_domain_info * p_domain_info)
{
  char *cwd;
  FILE *confp = NULL;
  char *fullpath = NULL;

  assert(p_domain_info);
  
  /* If -f flag specified. */
  if (config_file != NULL)
    {
      if (config_file[0] != '/')
        {
        	  cwd = getcwd (NULL, MAXPATHLEN);
        	  fullpath = XMALLOC (MTYPE_TMP, 
        			      strlen (cwd) + strlen (config_file) + 2);
        	  sprintf (fullpath, "%s/%s", cwd, config_file);
        }
      else
        fullpath = config_file;

      confp = fopen (fullpath, "r");

      if (confp == NULL)
       {
          fprintf (stderr, "can't open configuration file [%s]\n", 
            config_file);
          return -1;
        }
	}
  else /* config_file not specified in comand line*/
    {
      /* Relative path configuration file open. */
      if (config_current_dir)
        {
          confp = fopen (config_current_dir, "r");
          /* If there is no relative path exists, open system default file. */
        }

      if (confp == NULL)
      	 {
      	   confp = fopen (config_default_dir, "r");
      	   if (confp == NULL)
      	     {
      		    fprintf (stderr, "can't open configuration file [%s]\n",
      			   config_default_dir);
      		    return -1;
      		  }
           else
            {
              fullpath = config_default_dir;
            }
        }
      else
        {
          /* Rleative path configuration file. */
          cwd = getcwd (NULL, MAXPATHLEN);
          fullpath = XMALLOC (MTYPE_TMP, 
          	      strlen (cwd) + strlen (config_current_dir) + 2);
          sprintf (fullpath, "%s/%s", cwd, config_current_dir);
        }  
    }
  
  narb_config_from_file (confp, p_domain_info);

  /*
  if (fullpath)
    XFREE(MTYPE_TMP, fullpath);
  if (cwd)
    XFREE(MTYPE_TMP, cwd);
  */
 
  fclose (confp);
  return 0;

}

/* each opaque LSA from the same advertising router needs 
    a unique opaque_id*/
u_int32_t 
narb_ospf_opaque_id (void)
{
  static u_int32_t opaque_id = 0;
  opaque_id++;
  return opaque_id;
}

/* parsing the config file and create data structures to store the
    configuration. In particular the abstract topology (domain summary)
    is created from here.*/
void
narb_config_from_file(FILE *fp, struct narb_domain_info * p_domain_info)
{
  char buf[NARB_BUFSIZ];
  char blk_header[NARB_LINESIZ];
  char blk_body[NARB_BLKSIZ];
  char line[NARB_LINESIZ];

  int len;
  int config_code;
  char * next_blk = buf;
  char area[MAX_ADDR_LEN];

  char te_link_id_inter[MAX_ADDR_LEN];
  char narb_addr[MAX_ADDR_LEN];
  int narb_port;
  struct link_info* link;

  assert(p_domain_info);

  memset(p_domain_info, 0, sizeof(struct narb_domain_info));
  
  /* init p_domain_info */
  p_domain_info->router_ids = list_new();
  p_domain_info->te_links = list_new();
  p_domain_info->inter_domain_te_links = list_new();
  p_domain_info->if_narb_table = list_new();
  p_domain_info->svc_probes = list_new();
 
  /*reading everything into a big buffer*/
  buf[0] = 0;
  while (fgets(line, NARB_LINESIZ, fp))
    {
      if (line[0] == '!')
        continue;
      
      len = strlen(line);
      if (len)
        {
          line[len - 1] = ' '; /* change 'newline' into space */
          line[len] = 0;
          strcat (buf, line);
        }
    }
  len = sizeof (buf);
  if (len)
    buf[len - 1] = 0;

  /*parsing the big buffer into configuration blocks*/
  while ((config_code = read_config_blk(next_blk, blk_header,  blk_body, &next_blk)) !=CONFIG_END)
    {
      /* each block is identified by the config_code*/
      switch(config_code) 
        {
        case CONFIG_DOMAIN_ID:
          {
            char domain_id[MAX_ADDR_LEN];
            if (read_config_parameter(blk_body, "ip", "%s", domain_id))
              {
                inet_aton(domain_id, (struct in_addr*)(&p_domain_info->domain_id));
              }
            else if  (read_config_parameter(blk_body, "id", "%s", domain_id))
              {
                p_domain_info->domain_id = strtoul(domain_id, NULL, 10);
              }
            else
              {
                zlog_warn("read_config_parameter failed on domain-id");
              }
          }
          break;
        case  CONFIG_INTER_DOMAIN_ODPFD:
          {
            char address[MAX_ADDR_LEN];
            int port;
            char ori_if[MAX_ADDR_LEN];

            if (read_config_parameter(blk_body, "address", "%s", address))
              {
                strcpy(p_domain_info->ospfd_inter.addr, address);
              }
            else
              {
                zlog_warn("read_config_parameter failed on inter-domain-ospfd : address");
                strcpy(p_domain_info->ospfd_inter.addr, "127.0.0.1");
                zlog_warn("inter-domain-ospfd address has been set to 127.0.0.1 (localhost)");
              }

            if (read_config_parameter(blk_body, "localport", "%d", &port))
              {
                p_domain_info->ospfd_inter.localport = port;
              }
            else
              {
                p_domain_info->ospfd_inter.localport = NARB_OSPFD_LOCAL_PORT_INTER;
              }
            
            if (read_config_parameter(blk_body, "port", "%d", &port))
              {
                p_domain_info->ospfd_inter.port = port;
              }
            else
              {
                zlog_warn("read_config_parameter failed on inter-domain-ospfd : port");
                p_domain_info->ospfd_inter.port = DEFAULT_OSPFD_INTER_PORT;
                zlog_warn("inter-domain-ospfd port has been set to %d", DEFAULT_OSPFD_INTER_PORT);
              }

            if (read_config_parameter(blk_body, "originate-interface", "%s", ori_if))
              {
                inet_aton(ori_if, &p_domain_info->ospfd_inter.ori_if);
              }
            else
              {
                zlog_warn("read_config_parameter failed on inter-domain-ospfd : ori_if");
              }

            if (read_config_parameter(blk_body, "area", "%s", area))
              {
                inet_aton(area, &p_domain_info->ospfd_inter.area);
              }
            else
              {
                zlog_warn("read_config_parameter failed on inter-domain-ospfd : area");
              }
          }
          break;
        case  CONFIG_INTRA_DOMAIN_ODPFD:
          {
            char address[MAX_ADDR_LEN];
            int port;
            char ori_if[MAX_ADDR_LEN];

            if (read_config_parameter(blk_body, "address", "%s", address))
              {
                strcpy(p_domain_info->ospfd_intra.addr, address);
              }
            else
              {
                zlog_warn("read_config_parameter failed on intra-domain-ospfd : address");
                strcpy(p_domain_info->ospfd_intra.addr, "127.0.0.1");
                zlog_warn("intra-domain-ospfd address has been set to 127.0.0.1 (localhost)");
              }

            if (read_config_parameter(blk_body, "localport", "%d", &port))
              {
                p_domain_info->ospfd_intra.localport = port;
              }
            else
              {
                p_domain_info->ospfd_intra.localport = NARB_OSPFD_LOCAL_PORT_INTRA;
              }
            
            if (read_config_parameter(blk_body, "port", "%d", &port))
              {
                p_domain_info->ospfd_intra.port = port;
              }
            else
              {
                zlog_warn("read_config_parameter failed on intra-domain-ospfd : port");
                p_domain_info->ospfd_inter.port = DEFAULT_OSPFD_INTRA_PORT;
                zlog_warn("inter-domain-ospfd port has been set to %d", DEFAULT_OSPFD_INTRA_PORT);
              }

            if (read_config_parameter(blk_body, "originate-interface", "%s", ori_if))
              {
                inet_aton(ori_if, &p_domain_info->ospfd_intra.ori_if);
              }
            else
              {
                zlog_warn("read_config_parameter failed on intra-domain-ospfd : ori_if");
              }

            if (read_config_parameter(blk_body, "area", "%s", area))
              {
                inet_aton(area, &p_domain_info->ospfd_intra.area);
              }
            else
              {
                zlog_warn("read_config_parameter failed on intra-domain-ospfd : area");
              }
          }
          break;
        case  CONFIG_ROUTER:
          {
            char link_header[NARB_LINESIZ], link_body[NARB_BLKSIZ];
            char * link_blk, *p_str;
            int ret;
            char router_id[MAX_ADDR_LEN];
            int service_id;
            struct svc_probe *p_svc_probe;

            struct router_id_info * router = XMALLOC(MTYPE_TMP, sizeof(struct router_id_info));
            memset(router, 0, sizeof(struct router_id_info));

            if (read_config_parameter(blk_body, "id", "%s", router_id))
              {
                inet_aton(router_id, &router->id);
                router->adv_id = router->id;
              }
            else
              {
                XFREE(MTYPE_TMP, router);
                zlog_warn("read_config_parameter failed on router : id");
                continue;
              }
            listnode_add(p_domain_info->router_ids, router);

            link_blk = blk_body;
            if (read_config_parameter(blk_body, "type", "%d", &router->type) 
                 && (router->type == RT_TYPE_BORDER||router-> type == RT_TYPE_HOST))
              {
                link_blk = strstr(blk_body, "svc_probes");
                if (link_blk)
                  {
                    ret = read_config_blk(link_blk, link_header, link_body, &link_blk);
                    assert (ret == CONFIG_SVC_PROBE);

                    p_str = strtok(link_body, " \t,");
                    assert(p_str);
                    do {
                        p_svc_probe = XMALLOC(MTYPE_TMP, sizeof(struct svc_probe));
                        memset(p_svc_probe, 0, sizeof(struct svc_probe));
                        p_svc_probe->router = router;
                        service_id = atoi(p_str);
                        /* search for the service block in the services list*/
                        p_svc_probe->service = service_lookup_by_id(p_domain_info->services, service_id);
                        if (!p_svc_probe->service)
                          {
                            zlog_warn("service_lookup_by_id failed on service_id (%d)", service_id);
                            XFREE(MTYPE_TMP, p_svc_probe);
                          }
                        else
                          {
                            listnode_add(p_domain_info->svc_probes, p_svc_probe);
                          }
                      } while ((p_str = strtok(NULL, " \t,")) != NULL);
                  }
              }

            /*one or more te links advertising by the same router 
               are contained in the router block*/
            link_blk = strstr(blk_body, "link");
            while (link_blk && strstr(link_blk, "link"))
              {
                char link_id[MAX_ADDR_LEN];
                char loc_if[MAX_ADDR_LEN];
                char rem_if[MAX_ADDR_LEN];
                int link_type;
                int enc_type, sw_type;
                struct link_info *link;
                  
                ret = read_config_blk(link_blk, link_header, link_body, &link_blk);
                assert (ret == CONFIG_LINK);

                link = XMALLOC(MTYPE_TMP, sizeof(struct link_info));
                memset(link, 0, sizeof(struct link_info));

                /* reading madatory parameter*/
                if (read_config_parameter(link_body, "id", "%s", link_id))
                  {
                    inet_aton(link_id, &link->id);
                    inet_aton(router_id, &link->adv_id);
                  }
                else
                  {
                    XFREE(MTYPE_TMP, link);
                    zlog_warn("read_config_parameter failed on link : id");
                    continue;
                  }
                /* reading madatory parameter*/                
                if (read_config_parameter(link_body, "type", "%d", &link_type))
                  {
                    link->type = link_type;
                  }
                else
                  {
                    XFREE(MTYPE_TMP, link);
                    zlog_warn("read_config_parameter failed on link : type");
                    continue;
                  }
                /* reading madatory parameter*/                
                if (read_config_parameter(link_body, "local_if", "%s", loc_if))
                  {
                    inet_aton(loc_if, &link->loc_if);
                    SET_LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_LOC_IF);
                  }
                else
                  {
                    XFREE(MTYPE_TMP, link);
                    zlog_warn("read_config_parameter failed on link : LocIf");
                    continue;
                  }
                /* reading madatory parameter*/                
                if (read_config_parameter(link_body, "remote_if", "%s", rem_if))
                  {
                    inet_aton(rem_if, &link->rem_if);
                    SET_LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_REM_IF);
                  }
                else
                  {
                    XFREE(MTYPE_TMP, link);
                    zlog_warn("read_config_parameter failed on link : RemIf");
                    continue;
                  }

                /*reading optional parameter*/
                if (read_config_parameter(link_body, "max_bw", "%f", &link->max_bw))
                  {
                    SET_LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_MAX_BW);
                  }

                if (read_config_parameter(link_body, "max_rsv_bw", "%f", &link->max_rsv_bw))
                  {
                    SET_LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_MAX_RSV_BW);
                  }

                if (read_config_parameter(link_body, "unrsv_bw0", "%f", &link->unrsv_bw[0]))
                  {
                    SET_LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_UNRSV_BW);
                    read_config_parameter(link_body, "unrsv_bw1", "%f", &link->unrsv_bw[1]);
                    read_config_parameter(link_body, "unrsv_bw2", "%f", &link->unrsv_bw[2]);
                    read_config_parameter(link_body, "unrsv_bw3", "%f", &link->unrsv_bw[3]);
                    read_config_parameter(link_body, "unrsv_bw4", "%f", &link->unrsv_bw[4]);
                    read_config_parameter(link_body, "unrsv_bw5", "%f", &link->unrsv_bw[5]);
                    read_config_parameter(link_body, "unrsv_bw6", "%f", &link->unrsv_bw[6]);
                    read_config_parameter(link_body, "unrsv_bw7", "%f", &link->unrsv_bw[7]);
                    memcpy(link->ifswcap.max_lsp_bw_at_priority, link->unrsv_bw, 8*sizeof(float));

                  }

                if (read_config_parameter(link_body, "enc_type", "%d", &enc_type))
                  {
                    SET_LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_IFSW_CAP);
                    link->ifswcap.encoding = (char)enc_type;
                  }

                if (read_config_parameter(link_body, "sw_type", "%d", &sw_type))
                  {
                    SET_LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_IFSW_CAP);
                    link->ifswcap.switching_cap = (char)sw_type;
                  }

                if (read_config_parameter(link_body, "metric", "%d", &link->metric))
                  {
                    SET_LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_METRIC);
                  }

                if (read_config_resvs(link_body,  &link->resvs))
                  {
                    SET_LINK_PARA_FLAG(link->info_flag, LINK_PARA_FLAG_RESV);
                  }

                /*more optional parameters will be supported later on*/
                
                listnode_add(p_domain_info->te_links, link);
              }
          }
          break;
        case  CONFIG_INTER_DOMAIN_TE_LINK:
          {
            struct in_addr * link_inter = XMALLOC(MTYPE_TMP, sizeof(struct in_addr));

            if (read_config_parameter(blk_body, "id", "%s", te_link_id_inter))
              {
                inet_aton(te_link_id_inter, link_inter);
              }
            else
              {
                XFREE(MTYPE_TMP, link_inter);
                zlog_warn("read_config_parameter failed on inter-domain-te-link : id");
                continue;
              }
            listnode_add(p_domain_info->inter_domain_te_links, link_inter);

            /* read peering narb information that is associated with this inter-domain link */
            if (read_config_parameter(blk_body, "narb-peer", "%s", narb_addr))
              {
                if (!read_config_parameter(blk_body, "port", "%d", &narb_port))
                  {
                    narb_port = NARB_API_SYNC_PORT;
                  }

                link = narb_lookup_link_by_id(p_domain_info->te_links, link_inter);
                
                if (link)
                  if_narb_add (p_domain_info->if_narb_table, narb_addr, narb_port, link->rem_if);
                else
                  zlog_warn("Wrong inter-domain-te-link ID: %x", link_inter->s_addr);
              }
            else
              {
                zlog_warn("read_config_parameter failed on inter-domain-te-link : narb_addr");
              }

        }
        break;
      case  CONFIG_SERVICE:
        {
          struct service_info *p_service;
          p_service = XMALLOC(MTYPE_TMP, sizeof(struct service_info));
          memset(p_service, 0, sizeof(struct service_info));
          
          if (!p_domain_info->services)
            p_domain_info->services = list_new();
          
          if (read_config_parameter(blk_body, "id", "%d", &p_service->service_id))
            {
              if (!read_config_parameter(blk_body, "sw_type", "%d", &p_service->sw_type))
                zlog_warn("read_config_parameter failed on service : sw_type");
              if (!read_config_parameter(blk_body, "enc_type", "%d", &p_service->enc_type))
                zlog_warn("read_config_parameter failed on service : enc_type");
              if (!read_config_parameter(blk_body, "max_bw", "%f", &p_service->max_bw))
                zlog_warn("read_config_parameter failed on service : max_bw");
              listnode_add(p_domain_info->services, p_service);
            }
          else
            {
              XFREE(MTYPE_TMP, p_service);
              zlog_warn("read_config_parameter failed on service : id");
            }
        }
        break;
      case CONFIG_VTY:
        {
           char pass[20];
           char host[20];
           
           if (read_config_parameter(blk_body, "password", "%s", pass))
            {
              vty_password = XMALLOC(MTYPE_TMP, sizeof(pass));
              strcpy(vty_password, pass);
            }
           if (read_config_parameter(blk_body, "host", "%s", host))
            {
              narb_vty_addr = XMALLOC(MTYPE_TMP, sizeof(host));
              strcpy(narb_vty_addr, host);
            }
        }
        break;
      case  CONFIG_UNKNOWN:
      default:
         zlog_warn("Unknow configration block: %s {%s}", blk_header, blk_body);
      }
    }

}

/* recognizing a configuration block*/
int 
blk_code (char *buf)
{
  if (strstr(buf, "domain-id"))
    return CONFIG_DOMAIN_ID;
  else if (strstr(buf, "inter-domain-ospfd"))
    return CONFIG_INTER_DOMAIN_ODPFD;
  else if (strstr(buf, "intra-domain-ospfd"))
    return CONFIG_INTRA_DOMAIN_ODPFD;
  else if (strstr(buf, "router"))
    return CONFIG_ROUTER;
  else if (strstr(buf, "inter-domain-te-link"))
    return CONFIG_INTER_DOMAIN_TE_LINK;
  else if (strstr(buf, "link"))
    return CONFIG_LINK;
  else if (strstr(buf, "service"))
    return CONFIG_SERVICE;
  else if (strstr(buf, "svc_probe"))
    return CONFIG_SVC_PROBE;
  else if (strstr(buf, "vty"))
    return CONFIG_VTY;
  else
    return CONFIG_UNKNOWN;
}

/* reading config block inside {}, note that the block may contain sub-level {} */
int
read_config_blk(char *buf, char * header, char * body, char ** next)
{
  int i = 0, j = 0, n = 0;
  int ret = CONFIG_UNKNOWN;

  while (buf[j++] !='{') 
    {
      if (!buf[j-1])
         return CONFIG_END;
    }
  
  if (j)
    {
      n++;
      i = j;
      strncpy(header, buf, i-1);
      header[i] = 0;
      ret = blk_code(header);

      while (n > 0)
        {
          if (!buf[j])
            return CONFIG_END;
   
          if (buf[j] == '{')
            n++;
          else if (buf[j] == '}')
            n--;

          j++;
        }
      
      strncpy(body, buf+i, j-i-1);
      body[j-i-1] = 0;
      *next = buf + j;
    }

  return ret;
}

int
read_config_resvs(char * buf, list * resvs)
{
  char *str_resvs, *str;
  char resvs_buf[200];
  struct reservation *resv;
  int i;
  
  str_resvs = strstr(buf, "reservations");
  if (str_resvs == NULL)
    return 0;

  if (*resvs == NULL)
    *resvs = list_new();

  /* get manually defined reservations into list (for testing only) */
  while (*(str_resvs++) !='[') 
    ;
  i = 0;
  while (*str_resvs !=']') 
      resvs_buf[i++] = *(str_resvs++);
  resvs_buf[i] = 0;

  str = strtok(resvs_buf, " \t");
  while(str)
    {
        resv = XMALLOC(MTYPE_TMP, RESV_SIZE);
        memset(resv, 0, RESV_SIZE);
        i = sscanf (str, "%ld/%ld:%f", &resv->uptime, &resv->duration, &resv->bandwidth);
        assert (i == 3);
        resv->uptime += time(NULL);
        listnode_add(*resvs, resv);
        str = strtok(NULL, " \t");
    }
  return 1;
}

/* reading a parameter from a config block (char *buf). Each parameter is 
    specified in 'id = value' format. fmt describes it data type. The value is 
    returned in the buffer (void * parameter)*/
int
read_config_parameter(char * buf, char * id, char * fmt, void * parameter)
{
  char *str;
  
  str = strstr(buf, id);
  if (!str)
    return 0;

  /* return 1 if successful, otherwise 0 */
  return sscanf(str + strlen(id) + 1, fmt, parameter);
}

void
if_narb_add (list table, char *addr, int port, struct in_addr if_addr)
{
  listnode node, node_inner;
  struct if_narb_info * nodedata;
  struct in_addr *p_if;
    
  assert (table);

  p_if = XMALLOC(MTYPE_TMP, sizeof (struct in_addr));
  p_if->s_addr = if_addr.s_addr;

  node = listhead(table);
  while (node)
    {
        nodedata = getdata(node);
        if (strcmp(nodedata->addr, addr) == 0)
          {
            if (port > 0)
              nodedata->port = port;
            node_inner = listhead(nodedata->if_addr_list);
            while (node_inner)
              {
                if (((struct in_addr*)node_inner->data)->s_addr == p_if->s_addr)
                  break;
                nextnode(node_inner);
              }
            if (!node_inner)
              listnode_add(nodedata->if_addr_list, p_if);
            break;
          }
        nextnode(node);
    }
  
  if (!node)
    {
      nodedata = XMALLOC(MTYPE_TMP, sizeof (struct if_narb_info));
      strcpy(nodedata->addr, addr);
      nodedata->port = port;
      nodedata->if_addr_list = list_new();
      listnode_add(nodedata->if_addr_list, p_if);
      listnode_add(table, nodedata);
    }
}

struct service_info *
service_lookup_by_id(list services, int id)
{
  listnode node = listhead(services);
  struct service_info *nodedata;
  
  while (node)
    {
      nodedata = getdata(node);
      if (nodedata->service_id == id)
        return nodedata;
      nextnode(node);
    }
  
  return NULL;
}

