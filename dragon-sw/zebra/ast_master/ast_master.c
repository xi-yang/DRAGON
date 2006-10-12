#include <zebra.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include "libxml/xmlmemory.h"
#include "libxml/parser.h"
#include "libxml/tree.h"
#include "ast_master.h"
#include "buffer.h"
#include "log.h"
#include "local_id_cfg.h"

char *node_stype_name[] =
  { "None",
    "PC",
    "correlator",
    "computation_array" };

char *link_type_name[] =
  { "None",
    "EtherPipeBasic",
    "EtherPipeUltra",
    "TDMBasic" };

char *status_type_details[] =
  { "AST_UNKNOWN", "AST_SUCCESS", "AST_FAILURE", "AST_PENDING",
    "AST_AST_COMPLETE", "AST_APP_COMPLETE" };

char *action_type_details[] =
  { "VOID", "SETUP_REQ", "SETUP_RESP", "AST_COMPLETE",
    "RELEASE_REQ", "RELEASE_RESP", "APP_COMPLETE", "QUERY_REQ",
    "QUERY_RESP"};

char *link_stype_name[] = 
  { "none",
    "uni",
    "non_uni",
    "vlsr_vlsr",
    "vlsr_es" };

struct string_syntex_check local_field =
{
	4,
	{{"lsp-id",		"LSP-ID"},
	 {"port",		"Untagged single port"},
	 {"tagged-group",       "Tagged group"},
	 {"group",		"Untagged port group"}}
};

struct string_syntex_check bandwidth_field = 
{
	22,   
 	{{"gige",	"GigE (1000.00 Mbps)"},
	 {"gige_f", 	"Fast GigE (1250.00 Mbps)"},
	 {"2gige",	"2 GigE (2000.00 Mbps)"},
	 {"3gige",      "3 GigE (3000.00 Mbps)"},
	 {"4gige", 	"4 GigE (4000.00 Mbps)"},
	 {"5gige",      "5 GigE (5000.00 Mbps)"},
	 {"6gige",      "6 GigE (6000.00 Mbps)"},
	 {"7gige",      "7 GigE (7000.00 Mbps)"},
	 {"8gige",      "8 GigE (8000.00 Mbps)"},
	 {"9gige",      "9 GigE (9000.00 Mbps)"},
	 {"eth100M",	"Ethernet (100.00 Mbps)"},
	 {"eth200M",    "Ethernet (200.00 Mbps)"},
	 {"eth300M",    "Ethernet (300.00 Mbps)"},
	 {"eth400M",    "Ethernet (400.00 Mbps)"},
	 {"eth500M",    "Ethernet (500.00 Mbps)"},
	 {"eth600M",    "Ethernet (600.00 Mbps)"},
	 {"eth700M",    "Ethernet (700.00 Mbps)"},
	 {"eth800M",    "Ethernet (800.00 Mbps)"},
	 {"eth900M",    "Ethernet (900.00 Mbps)"},
	 {"10g",	"10GigE-LAN(10000.00 Mbps)"},
	 {"hdtv", 	"HDTV/SMPTE-292 (1485.00 Mbps)"},
	 {"oc48",	"OC-48/STM-16 (2488.32 Mbps)"}}
};

struct string_syntex_check swcap_field =
{
	4,
	{{"psc1",	"Packet-Switch Capable-1"},
	 {"l2sc",	"Layer-2 Switch Capable"},
	 {"lsc",	"Lambda Switch Capable"},
	 {"tdm",	"Time Division Multiplex Capable"}}
};

struct string_syntex_check encoding_field =
{
	4,
	{{"packet",	"Packet"},
	 {"ethernet",	"Ethernet"},
	 {"lambda",	"Lambda (photonic)"},
	 {"sdh",	"SONET/SDH"}}
};

struct string_syntex_check gpid_field =
{
	3,
	{{"ethernet", 	"Ethernet"},
	 {"lambda",	"Lambda"},
	 {"sdh",	"SONET/SDH"}}
};

struct application_cfg* master_final_parser(char*, int);
int autofill_es_info(struct resource*);

void 
set_allres_fail(char* error_msg)
{
  struct adtlistnode *curnode;
  struct resource *myres;

  glob_app_cfg->status = AST_FAILURE;
  if (error_msg) {
    strcpy(glob_app_cfg->details, error_msg);
    zlog_err(error_msg);
  }
  if (glob_app_cfg->node_list) {
    for (curnode = glob_app_cfg->node_list->head;
         curnode;
         curnode = curnode->next) {
      myres = (struct resource*) curnode->data;
      myres->status = AST_FAILURE;
    }
  }
  if (glob_app_cfg->link_list) {
    for (curnode = glob_app_cfg->link_list->head;
         curnode;
         curnode = curnode->next) {
      myres = (struct resource*) curnode->data;
      myres->status = AST_FAILURE;
    }
  }
}

void
set_res_fail(char* error_msg, struct resource *res)
{
  zlog_err(error_msg);
  if (res->agent_message) {
    free(res->agent_message); 
    res->agent_message = strdup(error_msg);
  }
  res->status = AST_FAILURE;
  glob_app_cfg->status = AST_FAILURE;
}

struct application_cfg*
search_cfg_in_list(char* ast_id)
{
  struct adtlistnode *curnode;
  struct application_cfg *curcfg;

  for (curnode = app_list.head;
        curnode;
        curnode = curnode->next) {
    curcfg = (struct application_cfg*)curnode->data;

    if (strcmp(curcfg->ast_id, ast_id) == 0)
      return curcfg;
  }

  return NULL;
}

void
add_cfg_to_list()
{
  struct adtlistnode *curnode;
  struct application_cfg *curcfg;

  if (!glob_app_cfg || !glob_app_cfg->ast_id)
    return;

  zlog_info("Adding <ast_id>:%s into glob_list", glob_app_cfg->ast_id);

  curcfg = search_cfg_in_list(glob_app_cfg->ast_id);

  if (!curcfg)
    adtlist_add(&app_list, glob_app_cfg);
  else if (curcfg != glob_app_cfg) {

    zlog_warn("Finding another copy of <ast_id>:%s in app_list", glob_app_cfg->ast_id);

    for (curnode = app_list.head;
         curnode;
         curnode = curnode->next) {
      if (curcfg == curnode->data) {
        free_application_cfg(curcfg);
        curnode->data = glob_app_cfg;
      }
    }
  }
}

struct application_cfg* 
retrieve_app_cfg(char* ast_id, int agent)
{
  char path[100];
  struct application_cfg *ret = NULL;
  struct stat fs;

  if (!ast_id)
    return NULL;

  ret = search_cfg_in_list(ast_id);
  if (ret)
    return ret;

  if (agent == MASTER) 
    sprintf(path, "%s/%s/final.xml", AST_DIR, ast_id);
  else if (agent == NODE_AGENT)
    sprintf(path, "%s/%s/final.xml", NODE_AGENT_DIR, ast_id); 
  else if (agent == LINK_AGENT) 
    sprintf(path, "%s/%s/final.xml", LINK_AGENT_DIR, ast_id);
  else 
    return NULL;

  if (stat(path, &fs) == -1) 
    return NULL;

  if (agent == MASTER) 
    ret = master_final_parser(path, MASTER);       
  else 
    ret = agent_final_parser(path);

  if (agent == MASTER)  
    add_cfg_to_list();
  return ret;
}

char* generate_ast_id(int type)
{
  static char id[100];

  memset(id, 0, 100); 

  switch (type) {
    case ID_SETUP:
      gethostname(id, 100);
      sprintf(id+strlen(id), "_%d", (int)time(NULL));
      break;
    case ID_TEMP:
      sprintf(id, "tmp");
      gethostname(id+strlen(id), 100);
      sprintf(id+strlen(id), "_%d", (int)time(NULL));
      break;
    case ID_QUERY:
      sprintf(id, "query");
      sprintf(id+strlen(id), "_%d", (int)time(NULL));
      break;
    default:
      return NULL;
  }

  return strdup(id);
}

int
get_status_by_str(char* key)
{
  int i;

  for (i = 0; key && i <= AST_APP_COMPLETE ; i++) 
    if (strcasecmp(key, status_type_details[i]) == 0) 
      return i;

  return 0;
}

int 
get_action_by_str(char* key)
{
  int i;

  for (i = 0; key && i <= NUM_FUNCTION_TYPE; i++) 
    if (strcasecmp(key, action_type_details[i]) == 0) 
      return i;

  return 0;
}

int 
get_node_stype_by_str(char* key)
{ 
  int i;

  for (i = 0; key && i <= NUM_NODE_TYPE; i++)
    if (strcasecmp(key, node_stype_name[i]) == 0)
      return i;

  return 0;
}

struct resource * 
search_node_by_name(struct application_cfg* app_cfg, char* str)
{
  struct adtlistnode *curnode;
  struct resource *res;

  if (app_cfg && app_cfg->node_list) {
    for (curnode = app_cfg->node_list->head;
	 curnode;
	 curnode = curnode->next) {
      res = (struct resource*) curnode->data;

      if (strcasecmp(res->name, str) == 0) 
	return res;
    }
  }

  return NULL;
}

void
free_id_cfg_res(struct id_cfg_res* ptr)
{
  struct adtlistnode* curnode;
  struct local_id_cfg *id;

  if (!ptr)
    return;

  if (ptr->cfg_list) {
    for (curnode = ptr->cfg_list->head;
         curnode;
         curnode = curnode->next) {
      id = (struct local_id_cfg*) curnode->data;

      if (id->mems)
        free(id->mems);
      if (id->msg)
        free(id->msg);
      free (id);
      curnode->data = NULL;
    }
  }
  adtlist_free(ptr->cfg_list);

  if (ptr->msg)
    free(ptr->msg);
  free(ptr);
}

void
free_application_cfg(struct application_cfg *app_cfg)
{
  struct adtlistnode *curnode, *curnode1, *if_node;
  struct resource *mynode, *mylink;
  
  if (!app_cfg)
    return;

  switch (app_cfg->xml_type) {

  case TOPO_XML:

  if (app_cfg->node_list) {
    for (curnode = app_cfg->node_list->head;
	 curnode;
	 curnode = curnode->next) {
      mynode = (struct resource*) curnode->data;
      if (mynode->res.n.link_list) {
	/* since both node->link_list and link_list shares 
	 * the same ptr of data.  need to make sure
	 * we won't free the data here
	 */
	for (curnode1 = mynode->res.n.link_list->head;
	     curnode1;
	     curnode1 = curnode1->next) {
	  curnode1->data = NULL;
	}
	adtlist_free(mynode->res.n.link_list);
      }
      if (mynode->agent_message)
	free(mynode->agent_message);
      if (mynode->res.n.command)
	free(mynode->res.n.command);
      if (mynode->res.n.if_list) {
	for (if_node = mynode->res.n.if_list->head;
	     if_node;
	     if_node = if_node->next) 
	  if_node->data = NULL;
      }
      adtlist_free(mynode->res.n.if_list);
      free(mynode);
      curnode->data = NULL;
    }
  }
  adtlist_free(app_cfg->node_list); 
  app_cfg->node_list = NULL;

  if (app_cfg->link_list) {
    for (curnode = app_cfg->link_list->head;
	 curnode;
	 curnode = curnode->next) {
      mylink = (struct resource*) curnode->data;
      if (mylink->agent_message) 
	free(mylink->agent_message);
      if (mylink->res.l.src) {
	if (mylink->res.l.src->ifp) {
	  free(mylink->res.l.src->ifp->iface);
	  free(mylink->res.l.src->ifp->assign_ip);
	  free(mylink->res.l.src->ifp);
	}
	free(mylink->res.l.src);
      }
      if (mylink->res.l.dest) {
	if (mylink->res.l.dest->ifp) {
	  free(mylink->res.l.dest->ifp->iface);
	  free(mylink->res.l.dest->ifp->assign_ip);
	  free(mylink->res.l.dest->ifp);
	}
	free(mylink->res.l.dest);
      }
      free(mylink);
      curnode->data = NULL;
    }
  }
  adtlist_free(app_cfg->link_list);
  app_cfg->link_list = NULL;

  if (app_cfg->ast_id) 
    free(app_cfg->ast_id);
  if (app_cfg->ast_ip)
    free(app_cfg->ast_ip);
  break;

  case ID_XML:
  
  if (app_cfg->node_list) {
    for (curnode = app_cfg->node_list->head;
	 curnode;
	 curnode = curnode->next) {
      free_id_cfg_res(curnode->data);
      curnode->data = NULL;
    }
  }
  adtlist_free(app_cfg->node_list);
  if (app_cfg->ast_id)
    free(app_cfg->ast_id);
    
  break;

  }

  free(app_cfg);
}

int 
send_file_over_sock(int sock, char* newpath)
{
  int fd, total;
#ifndef __FreeBSD__
  static struct stat file_stat;
#endif

  if (sock <= 0 || !newpath)
    return 0;
  
  fd = open(newpath, O_RDONLY);

  if (fd == -1) 
    return 0;

#ifdef __FreeBSD__
  total = sendfile(fd, sock, 0, 0, NULL, NULL, 0);
  if (total < 0) {
#else
  if (fstat(fd, &file_stat) == -1) {
    zlog_err("send_file_over_sock: fstat() failed on %s", newpath);
    close(sock);
    close(fd);
    return (0);
  }
  total = sendfile(sock, fd, 0, file_stat.st_size);
  if (total < 0) {
#endif
    zlog_err("send_file_over_sock: sendfile() failed; %d(%s)", errno, strerror(errno));
    close(sock);
    close(fd);
    return (0);
  }

  close(fd);
  return 1;
}

int
send_file_to_agent(char *ipadd, int port, char *newpath)
{
  int sock;
  struct sockaddr_in servAddr;
  int fd;
#ifndef __FreeBSD__
  static struct stat file_stat;
#endif

  if (!ipadd || !newpath) 
    return -1;

  if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    zlog_err("send_file_to_agent: socket() failed");
    return (-1);
  }

  memset(&servAddr, 0, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = inet_addr(ipadd);
  servAddr.sin_port = htons(port);

  if (connect(sock, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
    zlog_err("send_file_to_agent: connect() failed to %s port %d", ipadd, port);
    close(sock);
    return -1;
  }

  fd = open(newpath, O_RDONLY);

#ifdef __FreeBSD__
  if (sendfile(fd, sock, 0, 0, NULL, NULL, 0) < 0) {
#else
  if (fstat(fd, &file_stat) == -1) {
    zlog_err("send_file_to_agent: fstat() failed on %s", newpath);
    close(sock);
    close(fd);
    return (-1);
  }
  if (sendfile(sock, fd, 0, file_stat.st_size) < 0) {
#endif
    zlog_err("send_file_to_agent: sendfile() failed; %d(%s)", errno, strerror(errno));
    close(sock);
    close(fd);
    return (-1);
  }

  close(fd);
  return sock;
}

void
print_error_response(char *path)
{
  FILE *fp;

  if (!path)
    return;
  fp = fopen(path, "w+");
  if (!fp)
    return;

  if (!glob_app_cfg) {
    fprintf(fp, "<topology>\n");
    fprintf(fp, "<status>AST_FAILURE</status>\n");
    fprintf(fp, "<details>Incoming XML file can't be parsed successfully</details>\n");
    fprintf(fp, "</topology>\n");
    fflush(fp);
    fclose(fp);
    return;
  }

  fprintf(fp, "<topology ast_id=\"%s\" action=\"%s\">\n",
		glob_app_cfg->ast_id,
  		action_type_details[glob_app_cfg->action]);
  fprintf(fp, "<status>AST_FAILURE</status>\n");
  if (glob_app_cfg->details[0] != '\0')
    fprintf(fp, "<details>%s</details>\n", glob_app_cfg->details);
  fprintf(fp, "</topology>\n");
  fflush(fp);
  fclose(fp);
}

void
print_node(FILE* fp, struct resource* node)
{
  struct adtlistnode *curnode;
  struct if_ip *ifp;

  if (node == NULL)
    return;

  fprintf(fp, "<resource type=\"%s\" name=\"%s\">\n",
		node_stype_name[node->res.n.stype], node->name);
  fprintf(fp, "\t<status>%s</status>\n", status_type_details[node->status]);
  if (node->agent_message) 
    fprintf(fp, "\t<agent_message>%s</agent_message>\n", node->agent_message);
  if (node->res.n.ip[0] != '\0')
    fprintf(fp, "\t<ip>%s</ip>\n", node->res.n.ip);
  if (node->res.n.router_id[0] != '\0')
    fprintf(fp, "\t<router_id>%s</router_id>\n", node->res.n.router_id);
  if (node->res.n.tunnel[0] != '\0')
    fprintf(fp, "\t<tunnel>%s</tunnel>\n", node->res.n.tunnel);
  if (node->res.n.command) 
    fprintf(fp, "\t<command>%s</command>\n", node->res.n.command);

  /* FIONA */
  /* if this is sending to NODE_AGENT or MASTER */
  if (node->res.n.if_list) {
    for (curnode = node->res.n.if_list->head;
	 curnode;
	 curnode = curnode->next) {
      ifp = (struct if_ip*) curnode->data;

      fprintf(fp, "\t<ifaces>\n");
      if (ifp->iface) 
	fprintf(fp, "\t\t<iface>%s</iface>\n", ifp->iface);
      fprintf(fp, "\t\t<assign_ip>%s</assign_ip>\n", ifp->assign_ip);
      if (ifp->vtag)
	fprintf(fp, "\t\t<vtag>%d</vtag>\n", ifp->vtag);
      fprintf(fp, "\t</ifaces>\n");
    }
  } 
  fprintf(fp, "</resource>\n");
}

void
print_endpoint(FILE* fp, struct endpoint* ep)
{
  if (!fp || !ep)
    return;

  if (ep->es)
    fprintf(fp, "\t\t<es>%s</es>\n", ep->es->name);
  if (ep->vlsr)
    fprintf(fp, "\t\t<vlsr>%s</vlsr>\n", ep->vlsr->name);
  if (ep->proxy)
    fprintf(fp, "\t\t<proxy>%s</proxy>\n", ep->proxy->name);
  if (ep->ifp) {
    if (ep->ifp->iface) 
      fprintf(fp, "\t\t<iface>%s</iface>\n", ep->ifp->iface);
    fprintf(fp, "\t\t<assign_ip>%s</assign_ip>\n", ep->ifp->assign_ip);
  }
  if (ep->local_id_type[0] != '\0') 
    fprintf(fp, "\t\t<local_id>%c/%d</local_id>\n",
		ep->local_id_type[0], ep->local_id);
}

void 
print_link(FILE* fp, struct resource* link)
{
  if (link == NULL || fp == NULL) 
    return;

  fprintf(fp, "<resource type=\"%s\" name=\"%s\">\n",
		link_stype_name[link->res.l.stype], link->name);
  fprintf(fp, "\t<status>%s</status>\n", status_type_details[link->status]);
  if (link->agent_message) 
    fprintf(fp, "\t<agent_message>%s</agent_message>\n", link->agent_message);
  if (link->res.l.lsp_name[0] != '\0')
    fprintf(fp, "\t<lsp_name>%s</lsp_name>\n", link->res.l.lsp_name);
  if (link->res.l.src) {
    fprintf(fp, "\t<src>\n");
    print_endpoint(fp, link->res.l.src);
    fprintf(fp, "\t</src>\n");
  }
  if (link->res.l.dest) {
    fprintf(fp, "\t<dest>\n");
    print_endpoint(fp, link->res.l.dest);
    fprintf(fp, "\t</dest>\n");
  }
  fprintf(fp, "\t<te_params>\n");
  if (link->res.l.bandwidth[0] != '\0')
    fprintf(fp, "\t\t<bandwidth>%s</bandwidth>\n", link->res.l.bandwidth);
  if (link->res.l.swcap[0] != '\0') 
    fprintf(fp, "\t\t<swcap>%s</swcap>\n", link->res.l.swcap); 
  if (link->res.l.encoding[0] != '\0')
    fprintf(fp, "\t\t<encoding>%s</encoding>\n", link->res.l.encoding); 
  if (link->res.l.gpid[0] != '\0')
    fprintf(fp, "\t\t<gpid>%s</gpid>\n", link->res.l.gpid);
  if (link->res.l.vtag[0] != '\0') 
    fprintf(fp, "\t\t<vtag>%s</vtag>\n", link->res.l.vtag);
  fprintf(fp, "\t</te_params>\n");
  fprintf(fp, "</resource>\n");
}

void
print_link_brief(FILE* fp, struct resource* link)
{
  if (link == NULL || fp == NULL) 
    return;

  fprintf(fp, "<resource type=\"%s\" name=\"%s\">\n", 
		link_stype_name[link->res.l.stype], link->name);
  fprintf(fp, "\t<status>%s</status>\n", status_type_details[link->status]);
  if (link->agent_message) 
    fprintf(fp, "\t<agent_message>%s</agent_message>\n", link->agent_message);
  if (link->res.l.lsp_name[0] != '\0') 
    fprintf(fp, "\t<lsp_name>%s</lsp_name>\n", link->res.l.lsp_name);
  if (link->res.l.dragon)
    fprintf(fp, "\t<dragon>%s</dragon>\n", link->res.l.dragon->name);
  fprintf(fp, "</resource>\n");
}

void
print_xml_response(char* path, int agent)
{
  struct adtlistnode *curnode;
  struct resource *mynode, *mylink;
  FILE* fp;

  if (!path)
    return;

  fp = fopen(path, "w+");
  if (!fp)
    return;

  fprintf(fp, "<topology ast_id=\"%s\" action=\"%s\">\n", glob_app_cfg->ast_id, action_type_details[glob_app_cfg->action]);

  if (glob_app_cfg->ast_ip) 
    fprintf(fp, "<ast_ip>%s</ast_ip>\n", glob_app_cfg->ast_ip);

  fprintf(fp, "<status>%s</status>\n", status_type_details[glob_app_cfg->status]);
  if (glob_app_cfg->details[0] != '\0')
    fprintf(fp, "<details>%s</details>\n", glob_app_cfg->details);

  if (glob_app_cfg->node_list) {
    for ( curnode = glob_app_cfg->node_list->head;
  	curnode;  
  	curnode = curnode->next) {
      mynode = (struct resource*)(curnode->data);

      if (agent == NODE_AGENT) 
	print_node(fp, mynode);
      else if (mynode->res.n.link_list)
	print_node(fp, mynode);
    }
  }

  if (glob_app_cfg->link_list) {
    for ( curnode = glob_app_cfg->link_list->head;
  	curnode;
  	curnode = curnode->next) {
      mylink = (struct resource*)(curnode->data);
      print_link_brief(fp, mylink);
    }
  }

  fprintf(fp, "</topology>");
  fflush(fp);
  fclose(fp);
}

void
print_final(char *path)
{
  struct adtlistnode *curnode, *curnode1;
  struct resource *mynode, *mylink;
  int i;
  FILE *file;
  
  if (!path)
    return;

  file = fopen(path, "w+");
  if (!file)
    return;

  fprintf(file, "<topology ast_id=\"%s\" action=\"%s\">\n",  glob_app_cfg->ast_id, action_type_details[glob_app_cfg->action]);

  fprintf(file, "<status>%s</status>\n", status_type_details[glob_app_cfg->status]);

  if (glob_app_cfg->details[0] != '\0') 
    fprintf(file, "<details>%s</details>\n", glob_app_cfg->details);
 
  if (glob_app_cfg->ast_ip) 
    fprintf(file, "<ast_ip>%s</ast_ip>\n", glob_app_cfg->ast_ip);

  if (glob_app_cfg->node_list) { 
    for ( i = 1, curnode = glob_app_cfg->node_list->head;
	  curnode;  
	  i++, curnode = curnode->next) {
      mynode = (struct resource*)(curnode->data);

      print_node(file, mynode);
	
      if (glob_app_cfg->action == QUERY_RESP &&
	  mynode->res.n.link_list) {
	for (curnode1 = mynode->res.n.link_list->head;
	     curnode1;
	     curnode1 = curnode1->next) {
	  mylink = (struct resource*)curnode1->data;
	  print_link(file, mylink);
	}
      }
    }
  }

  if (glob_app_cfg->link_list) {
    for (curnode = glob_app_cfg->link_list->head;
	curnode;
	curnode = curnode->next) {
      mylink = (struct resource*)curnode->data;
      print_link(file, mylink); 
    }
  }

  fprintf(file, "</topology>");
  fflush(file);
  fclose(file);
}

char**
parse_values(char *values, int *num)
{
  char *test, **toreturn, *word;
  int i, j, pos;
 
  test = strdup(values); 

  for ( i = 0, word = strtok(test, "|");
	word;
	i++, word = strtok(NULL, "|")) 
     ;

  if (!i)
    return NULL;

  *num = i;
  toreturn = malloc(i*sizeof(char*));

  for (pos = 0, j = 0;
       j != i;
       pos += strlen(toreturn[j])+1, j++) 
    toreturn[j] = test+pos;
  
  return toreturn;
}

char**
parse_minmax(char *minmax)
{
  char *test, **toreturn, *word;

  test = strdup(minmax);
  word = strtok(minmax, "-");
  if (!word)
    return NULL;

  toreturn = malloc(2*sizeof(char*));

  toreturn[0] = word;
  toreturn[1] = test+strlen(word)+1;

  return toreturn;
}

xmlNodePtr
findxmlnode(xmlNodePtr cur, char* keyToFound)
{
  if (strcasecmp(cur->name, keyToFound) == 0)
    return cur;

  cur = cur->xmlChildrenNode;
  while (cur) {
    if (cur->xmlChildrenNode)  {
      if (strcasecmp(cur->name, keyToFound) == 0) 
	return cur;
      return findxmlnode(cur, keyToFound);
    } 
    cur = cur->next;
  }

  return NULL;
}

int
xml_parser(char* filename)
{
  xmlDocPtr doc;
  xmlNodePtr cur;

  doc = xmlParseFile(filename);

  if (doc == NULL) {
    zlog_err("xml_parser: Empty document");
    xmlFreeDoc(doc);
    return 0;
  }

  cur = xmlDocGetRootElement(doc);
  if (!cur) {
    zlog_err("xml_parser: Empty document");
    xmlFreeDoc(doc);
    return NULL;
  }

  if (findxmlnode(cur, "topology")) {
    xmlFreeDoc(doc);
    return TOPO_XML;
  }

  if (findxmlnode(cur, "local_id_cfg")) {
    xmlFreeDoc(doc);
    return ID_XML;
  }

  zlog_err("xml_parser: xml file is neither <topology> nor <local_id_cfg> type")
;
  xmlFreeDoc(doc);
  return 0;
}

int
template_xml_parser(char* filename, struct network_link* profile)
{
  xmlChar *key;
  xmlDocPtr doc;
  xmlNodePtr cur, service_ptr;
  char keyToFound[100];
  struct link_cfg *newlink;

  doc = xmlParseFile(filename);

  if (doc == NULL) {
    zlog_err("template_xml_parser: Document %s is not parsed successfully", filename);
    return 0;
  }

  cur = xmlDocGetRootElement(doc);

  if (cur == NULL) {
    zlog_err("template_xml_parser: Document %s is empty", filename);
    xmlFreeDoc(doc);
    return 0;
  }

  /* first locate "Service_Definition" 
   */
  strcpy(keyToFound, "Service_Definition");
  cur = findxmlnode(cur, keyToFound);

  if (!cur) {
    zlog_err("template_xml_parser: Document %s doesn't have <%s>", 
	      filename, keyToFound);
    return 0;
  }

  newlink = &(profile->default_cfg);

  for ( cur = cur->xmlChildrenNode; 
	cur; 
	cur = cur->next ) {
    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
    if (!key)
      continue;

    if (strcasecmp(cur->name, "Service_Name") == 0)
      profile->service_name = key;
    else if (strcasecmp(cur->name, "xmlns") == 0) 
      profile->xmlns = key;
    else if (strcasecmp(cur->name, "version") == 0) 
      profile->version = key;
    else if (strcasecmp(cur->name, "Service_Characteristics") == 0) {
      service_ptr = cur;
      service_ptr = service_ptr->xmlChildrenNode;

      while (service_ptr) { 
	key = xmlNodeListGetString(doc, service_ptr->xmlChildrenNode, 1);

	if (strcasecmp(service_ptr->name, "Bandwidth") == 0) {
	  profile->bandwidth_minmax = parse_minmax(key);
	  if (!(profile->bandwidth_minmax)) {
	    zlog_err("template_xml_parser: Bandwidth should be specified as \"<min>-<max>\", error ...");
	    xmlFreeDoc(doc);
	    return 0;
	  }
	} else if (strcasecmp(service_ptr->name, "Bandwidth_DEF") == 0) 
	  strncpy(newlink->bandwidth, key, REG_TXT_FIELD_LEN);
	else if (strcasecmp(service_ptr->name, "VLAN_Transport") == 0) {
	  profile->vlan_transport = parse_values(key, &(profile->vlan_transport_num));
	  if (!(profile->vlan_transport)) {
	    zlog_err("template_xml_parser: vlan_transport should be specified as \"<value1>|...|<valueN>, error ...");
	    xmlFreeDoc(doc);
	    return 0;
	  }
	}

	service_ptr = service_ptr->next;
      }
    } else if (strcasecmp(cur->name, "Default_Values") == 0) {
	service_ptr = cur;
	service_ptr = service_ptr->xmlChildrenNode;

	while (service_ptr) {
	  key = xmlNodeListGetString(doc, service_ptr->xmlChildrenNode, 1);
	
	  if (strcasecmp(service_ptr->name, "Bandwidth") == 0) 
	    strncpy(newlink->bandwidth, key, REG_TXT_FIELD_LEN);

	  service_ptr = service_ptr->next; 
	}
    }
  }

  xmlFreeDoc(doc);
  return 1;
}

int 
master_template_parser(int profile_id)
{
  char filename[100];

  /* if the linkprofile has already been parsed, simply return
   */
  if (linkprofile[profile_id])
    return 1;
  else
    linkprofile[profile_id] = malloc(sizeof(struct network_link));

  memset(linkprofile[profile_id], 0, sizeof(struct network_link));
 
  switch (profile_id) {
    case 1:
      strcpy(filename, XML_ETHERBASIC_FILE);
      break;
    case 2:
      strcpy(filename, XML_ETHERULTRA_FILE);
      break;
    case 3:
      strcpy(filename, XML_TDMBASIC_FILE);
      break;
    }   

  if (template_xml_parser(filename, linkprofile[profile_id]) == 0) 
    return 0;

  return 1;
}

#ifdef OLD_CODE
/* parse service xml resource broker location file
 */
int
service_xml_parser(char* filename)
{
  xmlChar* key;
  xmlDocPtr doc;
  xmlNodePtr cur, node_ptr;
  char keyToFound[100];
  struct resource_agent *newagent;
  
  doc = xmlParseFile(filename);

  if (doc == NULL) {
    zlog_err("service_xml_parser: Document is not parsed successfully");
    return (0);
  }

  cur = xmlDocGetRootElement(doc);
  
  if (cur == NULL) {
    zlog_err("service_xml_parser: service_xml_parser: document is empty");
    xmlFreeDoc(doc);
    return (0);
  }

  /* first locate "brokers_definition"
   */
  strcpy(keyToFound, "brokers_definition");
  cur = findxmlnode(cur, keyToFound);

  if (!cur) {
    zlog_err("service_xml_parser: can't locate <%s> in the document", keyToFound);
    xmlFreeDoc(doc);
    return (0);
  }

  /* parse all the <resource> node
   */
  for (cur = cur->xmlChildrenNode;
       cur;
       cur = cur->next) {
    if (strcasecmp(cur->name, "broker") == 0) {
      newagent = malloc(sizeof(struct resource_agent));
      memset(newagent, 0, sizeof(struct resource_agent));

      node_ptr = cur;
      node_ptr = node_ptr->xmlChildrenNode;
      while (node_ptr) {
	key = xmlNodeListGetString(doc, node_ptr->xmlChildrenNode, 1);

	if (strcasecmp(node_ptr->name, "name") == 0) 
	  newagent->resource_type = key;
	else if (strcasecmp(node_ptr->name, "ipadd") == 0) 
	  newagent->add = key;
	else if (strcasecmp(node_ptr->name, "port") == 0)
	  newagent->port = atoi(key);

	node_ptr = node_ptr->next;
      }

      adtlist_add(&glob_agency_list, newagent);
    }
  }

  xmlFreeDoc(doc);
  return (1);
}
#endif

static int 
establish_relationship(struct application_cfg* app_cfg)
{
  struct resource *mylink, *dragon;
  struct adtlistnode *curnode;
  struct endpoint *src, *dest;

  if (!app_cfg->link_list)
    return 1;

  for ( curnode = app_cfg->link_list->head;
	curnode;
	curnode = curnode->next) {
    mylink = (struct resource*) curnode->data;

    if (!mylink->res.l.src || !mylink->res.l.dest) 
      continue;

    switch (mylink->res.l.stype) {
      case uni:
      case non_uni:
	src = mylink->res.l.src;
	dest = mylink->res.l.dest;
	if (!src->es || !dest->es) {
	  zlog_err("link (%s) should have both src and dest es defined", mylink->name);
	  return 0;
	}

	if ((src->es->res.n.router_id[0] == '\0' && 
		!IS_RES_UNFIXED(src->es))||
	    (dest->es->res.n.router_id[0] == '\0' &&
		!IS_RES_UNFIXED(dest->es))) {
	  zlog_err("link (%s) should have <router_id> defined in its src and dest es", mylink->name);
	  return 0;
	}

	if (src->local_id_type[0] == '\0' && dest->local_id_type[0] != '\0')
	  dest->local_id_type[0] = '\0';
	else if (src->local_id_type[0] != '\0' || dest->local_id_type[0] == '\0') 
	  src->local_id_type[0] = '\0';

	if (src->local_id_type[0] == '\0' && dest->local_id_type[0] == '\0' &&
		mylink->res.l.vtag[0] != '\0') {
	  sprintf(src->local_id_type, "tagged-group");
	  sprintf(dest->local_id_type, "tagged-group");
          src->local_id = atoi(mylink->res.l.vtag);
	  dest->local_id = atoi(mylink->res.l.vtag);  
	}

	if (mylink->res.l.src->proxy) 
	  mylink->res.l.dragon = mylink->res.l.src->proxy;
	else 
	  mylink->res.l.dragon = mylink->res.l.src->es;

	dragon = mylink->res.l.dragon;
	if (!dragon->res.n.link_list) { 
	  dragon->res.n.link_list = malloc(sizeof(struct adtlist)); 
	  memset(dragon->res.n.link_list, 0, sizeof(struct adtlist)); 
	} 
	adtlist_add(dragon->res.n.link_list, mylink);

	break;

      case vlsr_vlsr:
	if (!mylink->res.l.src->vlsr || !mylink->res.l.dest->vlsr) {
	  zlog_err("link (%s) should have both src and dest vlsr defined", mylink->name);
	  return 0;
	}

	if (mylink->res.l.src->vlsr->res.n.router_id[0] == '\0' ||
	    mylink->res.l.dest->vlsr->res.n.router_id[0] == '\0') {
	  zlog_err("link (%s) should have <router_id> defined in vlsr", mylink->name);
	  return 0;
	}

	mylink->res.l.dragon = mylink->res.l.src->vlsr;
	mylink->res.l.src->proxy = NULL;
	mylink->res.l.src->es = NULL;
	mylink->res.l.dest->proxy = NULL;
	mylink->res.l.dest->es = NULL;

	dragon = mylink->res.l.dragon;
	if (!dragon->res.n.link_list) { 
	  dragon->res.n.link_list = malloc(sizeof(struct adtlist)); 
	  memset(dragon->res.n.link_list, 0, sizeof(struct adtlist)); 
	} 
	adtlist_add(dragon->res.n.link_list, mylink);

	if (mylink->res.l.src->ifp) {
	  free(mylink->res.l.src->ifp->iface);
	  free(mylink->res.l.src->ifp->assign_ip);
	  free(mylink->res.l.src->ifp);
	  mylink->res.l.src->ifp = NULL;
	}

	if (mylink->res.l.dest->ifp) {
	  free(mylink->res.l.dest->ifp->iface);
	  free(mylink->res.l.dest->ifp->assign_ip);
	  free(mylink->res.l.dest->ifp);
	  mylink->res.l.dest->ifp = NULL;
	}

	break;
	
      case vlsr_es:

	/* first, let's see if src:vlsr and dest:es will work
	 */
	if (mylink->res.l.src->vlsr && mylink->res.l.dest->es) {
	  if (mylink->res.l.src->vlsr->res.n.router_id[0] == '\0' ||
	      mylink->res.l.dest->es->res.n.router_id[0] == '\0') {
	    zlog_err("link (%s) should have both src vlsr router_id and dest es router_id defined", mylink->name);
	    return 0;
	  }

	  mylink->res.l.src->es = NULL;
	  mylink->res.l.src->proxy = NULL;
	  mylink->res.l.dest->vlsr = NULL;
	  mylink->res.l.dragon = mylink->res.l.src->vlsr;
	} else if (mylink->res.l.src->es && mylink->res.l.dest->vlsr) {
	  if (mylink->res.l.src->es->res.n.router_id[0] == '\0' ||
	      mylink->res.l.dest->vlsr->res.n.router_id[0] == '\0') {
	    zlog_err("link (%s) should have both src ES router_id and dest vlsr router_id defined", mylink->name);
	    return 0;
	  }

	  mylink->res.l.dest->es = NULL;
	  mylink->res.l.dest->proxy = NULL;
	  mylink->res.l.src->vlsr = NULL;
	  if (mylink->res.l.src->proxy)
	    mylink->res.l.dragon = mylink->res.l.src->proxy;
	  else
	    mylink->res.l.dragon = mylink->res.l.src->es;
	} else {
	  zlog_err("link (%s) should have (src:vlsr and dest:es) or (src:es and dest:vlsr) defined", mylink->name);
	  return 0;
	}

	dragon = mylink->res.l.dragon;
	if (!dragon->res.n.link_list) { 
	  dragon->res.n.link_list = malloc(sizeof(struct adtlist)); 
	  memset(dragon->res.n.link_list, 0, sizeof(struct adtlist)); 
	} 
	adtlist_add(dragon->res.n.link_list, mylink);

	break;
    }
  }
  
  return 1;
}

struct application_cfg*
agent_final_parser(char* filename)
{
  xmlChar *key;
  xmlDocPtr doc;
  xmlNodePtr cur;
  char keyToFound[100]; 
  struct _xmlAttr* attr;
  struct application_cfg *app_cfg;

  doc = xmlParseFile(filename);
  if (doc == NULL) {
    zlog_err("agent_final_parser: Document not parsed successfully.");
    return NULL;
  }

  cur = xmlDocGetRootElement(doc);
  if (cur == NULL) {
    zlog_err("agent_final_parser: Empty document");
    xmlFreeDoc(doc);
    return NULL;
  }

  /* first locate "topology" keyword
   */
  strcpy(keyToFound, "topology");
  cur = findxmlnode(cur, keyToFound);

  if (!cur) {
    zlog_err("agent_final_parser: Can't locate <%s> in the document", keyToFound);
    xmlFreeDoc(doc);
    return NULL;
  }

  app_cfg = (struct application_cfg*)malloc(sizeof(struct application_cfg));
  memset(app_cfg, 0, sizeof(struct application_cfg));

  /* parse the ast_id and action */
  for (attr = cur->properties;
	attr;
	attr = attr->next) {
    if (strcasecmp(attr->name, "action") == 0) {
      app_cfg->action = get_action_by_str(attr->children->content);
      if (app_cfg->action == 0) {
	xmlFreeDoc(doc);
	free(app_cfg);
	return NULL;
      }
    } else if (strcasecmp(attr->name, "ast_id") == 0) 
      app_cfg->ast_id = strdup(app_cfg->ast_id = strdup(attr->children->content));
  } 
	 
  for (cur = cur->xmlChildrenNode;
       cur;
       cur=cur->next) {

    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

    if (strcasecmp(cur->name, "status") == 0) 
      app_cfg->status = get_status_by_str(key);
    else if (strcasecmp(cur->name, "ast_ip") == 0) 
      app_cfg->ast_ip = strdup(key);

    if (strcasecmp(cur->name, "resource") != 0) 
      continue;
  }

  xmlFreeDoc(doc);
  return app_cfg;
}

struct application_cfg*
master_final_parser(char* filename, int agent)
{
  struct application_cfg* ret;

  ret = topo_xml_parser(filename, agent);
  establish_relationship(ret);

  return (ret);
}

  
/* parse xml and build internal representation;
 * parser_type:
 *	FULL_VERSION		1
 *	BRIEF_VERSION		2
 */
struct application_cfg *
topo_xml_parser(char* filename, int agent)
{
  xmlChar *key;
  xmlDocPtr doc;
  xmlNodePtr cur, topo_ptr, node_ptr, link_ptr, resource_ptr;
  char keyToFound[100]; 
  struct resource *myres, *myres2;
  struct adtlist *node_list, *link_list;
  struct adtlistnode *curnode;
  int i;
  struct _xmlAttr* attr;
  struct if_ip *myifp;
  struct endpoint *ep;
  struct application_cfg* app_cfg;

  node_list = NULL;
  link_list = NULL;

  doc = xmlParseFile(filename);
 
  if (doc == NULL) {
    zlog_err("topo_xml_parser: Document not parsed successfully.");
    return NULL;
  }

  cur = xmlDocGetRootElement(doc);

  if (cur == NULL) {
    zlog_err("topo_xml_parser: Empty document");
    xmlFreeDoc(doc);
    return NULL;
  }

  /* first locate "topology" keyword
   */
  strcpy(keyToFound, "topology");
  cur = findxmlnode(cur, keyToFound);

  if (!cur) {
    zlog_err("topo_xml_parser: Can't locate <%s> in the document", keyToFound);
    xmlFreeDoc(doc);
    return NULL;
  }
  topo_ptr = cur;
  app_cfg = (struct application_cfg*)malloc(sizeof(struct application_cfg));
  memset(app_cfg, 0, sizeof(struct application_cfg));

  /* parse the parameter inside topology tab 
   */
  for ( attr = topo_ptr->properties;
	attr;
	attr = attr->next) {
    if (strcasecmp(attr->name, "action") == 0) {
      app_cfg->action = get_action_by_str(attr->children->content);
      if (app_cfg->action == 0) {
	zlog_err("Invalid <topology action> found: %s", key);
	xmlFreeDoc(doc);
	free(app_cfg);
	return (NULL);
      }
    } else if (strcasecmp(attr->name, "ast_id") == 0) 
      app_cfg->ast_id = strdup(attr->children->content);
  }
      
  /* parse all the nodes 
   * since cur is now pointing to the <nodes> xmlnode, 
   * set it to point to the child level; then, go through all
   * the nodes in the child level and dump the extract nodes into 
   * node_list
   */
  for (cur = cur->xmlChildrenNode;
       cur;
       cur=cur->next) {
    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);

    if (strcasecmp(cur->name, "status") == 0) 
      app_cfg->status = get_status_by_str(key);
    else if (strcasecmp(cur->name, "ast_ip") == 0) 
      app_cfg->ast_ip = strdup(key);
    else if (strcasecmp(cur->name, "details") == 0) 
      strncpy(app_cfg->details, key, 200-1);
    
    if (strcasecmp(cur->name, "resource") != 0) 
      continue;

    /* First, we need to know what kind of resource this is:
     * link or node by looking at its <resource type> keyword
     */
    resource_ptr = cur;
 
    myres = (struct resource*) malloc(sizeof(struct resource));
    memset(myres, 0, sizeof(struct resource));
 
    /* parse the parameter inside topology tab 
     */
    for ( attr = resource_ptr->properties;
	  attr;
	  attr = attr->next) {
      if (strcasecmp(attr->name, "type") == 0) {
        for (i = 1;
	     i <= NUM_NODE_STYPE;
	     i++) {
	  if (strcasecmp(attr->children->content, node_stype_name[i]) == 0) {
	    myres->res.n.stype = i;
	    myres->type = NODE_RES;
	    break;
	  }
	}

        if (myres->type == 0) {
	  for (i = 1;
		i <= NUM_LINK_STYPE;
		i++ ) {
	    if (strcasecmp(attr->children->content, link_stype_name[i]) == 0) {
	      myres->res.l.stype = i;
	      myres->type = LINK_RES;
	      break;
	    }
	  }
	}

	if (myres->type == 0) {
	  zlog_err("Invalid <resource type> found: %s", attr->children->content);
	  free(myres);
	  myres = NULL;
	  break;
        }
      } else if (strcasecmp(attr->name, "name") == 0) 
	strncpy(myres->name, attr->children->content, NODENAME_MAXLEN);
    }

    if (!myres)
      continue;

    if (myres->name[0] == '\0') {
      zlog_err("RESource doesn't have a name; ignore ...");
      free(myres);
      myres = NULL;
      continue;
    }

    if (myres->type == NODE_RES) {

      /* first make sure that there is no conflict in the names 
       * with other NODE res
       */
      if (node_list) {
	for (curnode = node_list->head;
		curnode;
		curnode = curnode->next) {
	  myres2 = (struct resource*)curnode->data;
	  if (strcasecmp(myres2->name, myres->name) == 0) {
	    zlog_warn("found another node_res: %s; it will be ignore", myres->name);
	    free(myres);
	    myres = NULL;
	    break;
	  }
 	}
      }

      if (!myres)
	continue;

      for (node_ptr = resource_ptr->xmlChildrenNode;
	   node_ptr;
	   node_ptr = node_ptr->next) {
	key = xmlNodeListGetString(doc, node_ptr->xmlChildrenNode, 1); 

	if (!key)
	  continue;

	if (strcasecmp(node_ptr->name, "ip") == 0) 
	  strncpy(myres->res.n.ip, key, IP_MAXLEN); 
	else if (strcasecmp(node_ptr->name, "router_id") == 0)
	  strncpy(myres->res.n.router_id, key, IP_MAXLEN);
	else if (strcasecmp(node_ptr->name, "tunnel") == 0)
	  strncpy(myres->res.n.tunnel, key, 10); 
	else if (strcasecmp(node_ptr->name, "status") == 0) 
	  myres->status = get_status_by_str(key);
	else if (strcasecmp(node_ptr->name, "agent_message") == 0)
	  myres->agent_message = strdup(key);
	else if (strcasecmp(node_ptr->name, "command") == 0)
	  myres->res.n.command = strdup(key);
	else if (strcasecmp(node_ptr->name, "ifaces") == 0)  {
	  myifp = (struct if_ip*) malloc(sizeof(struct if_ip));
	  memset(myifp, 0, sizeof(struct if_ip));

	  for (link_ptr = node_ptr->xmlChildrenNode;
		link_ptr;
		link_ptr = link_ptr->next) {
	    key = xmlNodeListGetString(doc, link_ptr->xmlChildrenNode, 1);

	    if (strcasecmp(link_ptr->name, "iface") == 0) 
	      myifp->iface = strdup(key);
	    else if (strcasecmp(link_ptr->name, "assign_ip") == 0) 
	      myifp->assign_ip = strdup(key);
	    else if (strcasecmp(link_ptr->name, "vtag") == 0)
	      myifp->vtag = atoi(key);
	  }

	  /* in face, if IP is not set, there is no need to play attention to 
	   * <iface> or link this interface to node
	   */
	  if (!myifp->assign_ip) {
	    if (myifp->iface)
	      free(myifp->iface);
	    free(myifp->assign_ip);
	    free(myifp);
	  } else {
	    if (!myres->res.n.if_list && agent != LINK_AGENT) {
	      myres->res.n.if_list = malloc(sizeof(struct adtlist));
	      memset(myres->res.n.if_list, 0, sizeof(struct adtlist));
	    }
	    adtlist_add(myres->res.n.if_list, myifp);
	  }
	} 
      } 

      if (node_list == NULL) { 
	node_list = malloc(sizeof(struct adtlist)); 
	memset(node_list, 0, sizeof(struct adtlist)); 
	app_cfg->node_list = node_list;
      } 
      adtlist_add(node_list, myres); 
    } else {

      /* first make sure that there is no conflict in the names 
       * with other LINK res
       */
      if (link_list) {
	for (curnode = link_list->head;
		curnode;
		curnode = curnode->next) {
	  myres2 = (struct resource*)curnode->data;
	  if (strcasecmp(myres2->name, myres->name) == 0) {
	    zlog_warn("found another link_res: %s; it will be ignore", myres->name);
	    free(myres);
	    myres = NULL;
	    break;
	  }
 	}
      }

      if (!myres) 
	continue;

      /* remember, cur is still pointing to the XML node
       * that has "resource" as keyword
       */
      for (link_ptr= cur->xmlChildrenNode;
	   link_ptr;
	   link_ptr = link_ptr->next) { 
	key = xmlNodeListGetString(doc, link_ptr->xmlChildrenNode, 1); 

	if (!key)
	  continue;

	if (strcasecmp(link_ptr->name, "src") == 0 ||
	    strcasecmp(link_ptr->name, "dest") == 0) { 
     
	  ep = (struct endpoint*) malloc(sizeof(struct endpoint));
	  memset(ep, 0, sizeof(struct endpoint));

	  for (node_ptr = link_ptr->xmlChildrenNode;
	       node_ptr;
	       node_ptr = node_ptr->next) {
	    key = xmlNodeListGetString(doc, node_ptr->xmlChildrenNode, 1); 
	
	    if (strcasecmp(node_ptr->name, "es") == 0)
	      ep->es = search_node_by_name(app_cfg, key);
	    else if (strcasecmp(node_ptr->name, "vlsr") == 0)
	      ep->vlsr = search_node_by_name(app_cfg, key);
	    else if (strcasecmp(node_ptr->name, "proxy") == 0)
	      ep->proxy = search_node_by_name(app_cfg, key);
	    else if (strcasecmp(node_ptr->name, "local_id") == 0) {
	      switch(key[0]) {
		case 'l':
		  strcpy(ep->local_id_type, "lsp-id");
	 	  break;
		case 'p':
		  strcpy(ep->local_id_type, "port");
		  break;
		case 't':
		  strcpy(ep->local_id_type, "tagged-group");
		  break;
		case 'g':
		  strcpy(ep->local_id_type, "group");
		  break;
		default:
		  zlog_err("invalid local_id type: %s", key);
	      }

	      ep->local_id = atoi(key+2);
	    } else if (strcasecmp(node_ptr->name, "iface") == 0) {
	      if (!ep->ifp) {
		ep->ifp = (struct if_ip*) malloc (sizeof(struct if_ip));
		memset(ep->ifp, 0, sizeof(struct if_ip));
	      }
	      ep->ifp->iface = strdup(key);
	    } else if (strcasecmp(node_ptr->name, "assign_ip") == 0) {
	      if (!ep->ifp) {
		ep->ifp = (struct if_ip*) malloc (sizeof(struct if_ip));
		memset(ep->ifp, 0, sizeof(struct if_ip));
	      }
	      ep->ifp->assign_ip = strdup(key);
	    } else if (strcasecmp(node_ptr->name, "vtag") == 0) {
	      if (!ep->ifp) {
		ep->ifp = (struct if_ip*) malloc (sizeof(struct if_ip));
		memset(ep->ifp, 0, sizeof(struct if_ip));
	      }
	      ep->ifp->vtag = atoi(key);
	    }
	  }

	  if (!ep->es) {
	    if (ep->ifp) {
	      if (ep->ifp->iface)
		free(ep->ifp->iface);
	      if (ep->ifp->assign_ip);
		free(ep->ifp->assign_ip);
	    }
	    free(ep->es);
	    free(ep);
	    continue;
	  }

	  if (strcasecmp(link_ptr->name, "src") == 0) {
	    myres->res.l.src = ep;
	    if (ep->ifp && agent != LINK_AGENT) {
	      /* put this if_ip into if_list in es
	       */
	      if (!myres->res.l.src->es->res.n.if_list) {
		myres->res.l.src->es->res.n.if_list = malloc(sizeof(struct adtlist));
		memset(myres->res.l.src->es->res.n.if_list, 0, sizeof(struct adtlist));
	      }
	      adtlist_add(myres->res.l.src->es->res.n.if_list, ep->ifp);
	    }
	  } else {
	    myres->res.l.dest = ep;

	    if (ep->ifp && agent != LINK_AGENT) {
	      if (!myres->res.l.dest->es->res.n.if_list) {
	        myres->res.l.dest->es->res.n.if_list = malloc(sizeof(struct adtlist));
	        memset(myres->res.l.dest->es->res.n.if_list, 0, sizeof(struct adtlist));
	      }
	      adtlist_add(myres->res.l.dest->es->res.n.if_list, ep->ifp);
	    }
	  }
	} else if (strcasecmp(link_ptr->name, "te_params") == 0) {

	  for (node_ptr = link_ptr->xmlChildrenNode;
		node_ptr;
		node_ptr = node_ptr->next) {
	    key = xmlNodeListGetString(doc, node_ptr->xmlChildrenNode, 1);

	    if (strcasecmp(node_ptr->name, "bandwidth") == 0) 
	      strncpy(myres->res.l.bandwidth, key, REG_TXT_FIELD_LEN); 
	    else if (strcasecmp(node_ptr->name, "swcap") == 0) 
	      strncpy(myres->res.l.swcap, key, REG_TXT_FIELD_LEN); 
	    else if (strcasecmp(node_ptr->name, "encoding") == 0) 
	      strncpy(myres->res.l.encoding, key, REG_TXT_FIELD_LEN); 
	    else if (strcasecmp(node_ptr->name, "gpid") == 0) 
	      strncpy(myres->res.l.gpid, key, REG_TXT_FIELD_LEN); 
	    else if (strcasecmp(node_ptr->name, "vtag") == 0) {
	      strncpy(myres->res.l.vtag, key, REG_TXT_FIELD_LEN);
	    
	      if (myres->res.l.src && myres->res.l.src->ifp) 
		myres->res.l.src->ifp->vtag = atoi(key);
	      if (myres->res.l.dest && myres->res.l.dest->ifp)
		myres->res.l.dest->ifp->vtag = atoi(key);
	    }
	  }
	} else if (strcasecmp(link_ptr->name, "lsp_name") == 0 && 
		app_cfg->action != SETUP_REQ ) 
	  strncpy(myres->res.l.lsp_name, key, LSP_NAME_LEN); 
	else if (strcasecmp(link_ptr->name, "status") == 0) 
	  myres->status = get_status_by_str(key);
	else if (strcasecmp(link_ptr->name, "agent_message") == 0) 
	  myres->agent_message = strdup(key);
	else if (strcasecmp(link_ptr->name, "dragon") == 0)
	  myres->res.l.dragon = search_node_by_name(app_cfg, key);
      }

      if (link_list == NULL) { 
	link_list = malloc(sizeof(struct adtlist)); 
	memset(link_list, 0, sizeof(struct adtlist)); 
	app_cfg->link_list = link_list;
      } 

      adtlist_add(link_list, myres);    
    } 
  }

  xmlFreeDoc(doc);

  return app_cfg;
}


/* validate the graph
 */
int
topo_validate_graph(int agent, struct application_cfg *app_cfg)
{
  struct adtlistnode *curnode;
  struct resource *mynode, *mylink;
  int i;

  /* first check the validity of funciton and ast_id
   */
  switch (agent) {

    case NODE_AGENT:
    case LINK_AGENT:
      if (app_cfg->action == SETUP_RESP ||
	  app_cfg->action == APP_COMPLETE ||
	  app_cfg->action == RELEASE_RESP ||
	  app_cfg->action == QUERY_RESP) {
	zlog_err("Invalid action defined for this topology");
	return (0);
      }
      if (app_cfg->ast_id == NULL) {
        zlog_err("ast_id should be set for this topology");
	return (0);
      }
      break;
    
    case MASTER:
      if (app_cfg->action == AST_COMPLETE) {
	zlog_err("Invalid action defined for this topology");
	return (0);
      }
      if ((app_cfg->action != SETUP_REQ &&  
	   app_cfg->action != QUERY_REQ) && 
	   app_cfg->ast_id == NULL) {
	zlog_err("ast_id should be set for this topology");
	return (0);
      } 
      break;
    
    case ASTB:
      if (app_cfg->action == AST_COMPLETE || 
	  app_cfg->action == QUERY_RESP) {
    
	zlog_err("Invalid action defined for this topology"); 
	printf("Valide Values are:\n"); 
	printf("SETUP_REQ, RELEASE_REQ, APP_COMPLETE, QUERY_REQ\n"); 
	return 0; 
      }
      break;
  }

  /* now, validate the cfg according to the action type */
  switch (app_cfg->action) {
    case SETUP_REQ:
      if (!app_cfg->node_list) {
	zlog_err("No node defined in this topology");
	return 0;
      } else {
	for ( curnode = app_cfg->node_list->head;
	      curnode;
	      curnode = curnode->next) {
	  mynode = (struct resource*) curnode->data;
	  
	  if (mynode->res.n.ip[0] == '\0') {
	    if (agent == MASTER || agent == ASTB) {
	      zlog_info("node (%s) is undefined; need to contact reousrce broker later", mynode->name);
	      mynode->res.n.router_id[0] = '\0';
	      mynode->res.n.tunnel[0] = '\0';
	      mynode->flags |= FLAG_UNFIXED;
	    } else { 
	      zlog_err("node (%s) should have <ip> defined", mynode->name); 
	      return 0;
	    }
	  } else {

	    /* even the node ip is defined; still need to figure out the tunnel and router_id
	     */
	    if ( agent == MASTER && !autofill_es_info(mynode)) {
	      zlog_err("node (%s:%s) is not in our ES pool\n", mynode->name, mynode->res.n.ip);
	      return 0;
	    }
	  }
	}
      }

      if (agent != NODE_AGENT && agent != ASTB) {
	if (!app_cfg->link_list) {
	  zlog_err("No link defined in this topology"); 
	  return 0;
        } else { 
	  if (!establish_relationship(glob_app_cfg))
	    return 0;

	  for ( curnode = app_cfg->link_list->head; 
		curnode; 
		curnode = curnode->next) { 
	    mylink = (struct resource*) curnode->data;

	    if (!mylink->res.l.src || !mylink->res.l.dest) {
	      zlog_err("link (%s) should have src and dest defined", mylink->name);
	      return 0;
	    }

	    if (mylink->res.l.bandwidth[0] != '\0') { 
	      for (i = 0; i < bandwidth_field.number; i++) {
		if (strcasecmp(mylink->res.l.bandwidth, bandwidth_field.ss[i].abbre) == 0)
		  break;
	      }
	      if (i == bandwidth_field.number) {
		zlog_err("Invalid value for bandwidth: %s", mylink->res.l.bandwidth);
		if (agent==ASTB) {
		  printf("Valid values for bandwidth:\n"); 
		  for (i = 0; i < bandwidth_field.number; i++) 
		    printf("%s\t:%s\n", bandwidth_field.ss[i].abbre, bandwidth_field.ss[i].details);
		}
		return 0;
	      }
	    }
	    if (mylink->res.l.swcap[0] != '\0') {
	      for (i = 0; i < swcap_field.number; i++) {
		if (strcasecmp(mylink->res.l.swcap, swcap_field.ss[i].abbre) == 0)
		  break;
	      }
	      if (i == swcap_field.number) {
		zlog_err("Invalid value for swcap: %s", mylink->res.l.swcap);
		if (agent==ASTB) {
		  printf("Valid values for swcap:\n");
		  for (i = 0; i < swcap_field.number; i++)
		    printf("%s\t:%s\n", swcap_field.ss[i].abbre, swcap_field.ss[i].details);
		}
		return 0;
	      }
	    } 
	    if (mylink->res.l.gpid[0] != '\0') {
	      for (i = 0; i < gpid_field.number; i++) {
		if (strcasecmp(mylink->res.l.gpid, gpid_field.ss[i].abbre) == 0)
		  break;
	      }
	      if (i == gpid_field.number) {
		zlog_err("Invalid value for gpid: %s", mylink->res.l.gpid);
		if (agent==ASTB) {
		  printf("Valid values for gpid:\n");
		  for (i = 0; i < gpid_field.number; i++)
		    printf("%s\t:%s\n", gpid_field.ss[i].abbre, gpid_field.ss[i].details);
		}
		return 0;
	      }
	    } 
	    if (mylink->res.l.encoding[0]  != '\0') {
	      for (i = 0; i < encoding_field.number; i++) {
		if (strcasecmp(mylink->res.l.encoding, encoding_field.ss[i].abbre) == 0)
		  break;
	      }
	      if (i == encoding_field.number) {
		zlog_err("Invalid value for encoding: %s", mylink->res.l.encoding);
		if (agent==ASTB) {
		  printf("Valid values for encoding:\n");
		  for (i = 0; i < encoding_field.number; i++)
		    printf("%s\t:%s\n", encoding_field.ss[i].abbre, encoding_field.ss[i].details);
		}
		return 0;
	      }
	    }
	  }
	}
      }
	
    case RELEASE_REQ:
      /* for the case ast_id not set, needed to validate the file
       */
      break;

    case QUERY_REQ:

      break;
    case QUERY_RESP:

      break;
  }

  return 1;
}

void
app_cfg_pre_req()
{
  struct adtlistnode *curnode;
  struct resource *res_cfg;

  if (!glob_app_cfg)
    return;

  memset(glob_app_cfg->details, 0, 200);
  glob_app_cfg->status = AST_SUCCESS;
  
  if (glob_app_cfg->node_list)
    for (curnode = glob_app_cfg->node_list->head;
	 curnode;
	 curnode = curnode->next) {
      res_cfg = (struct resource*) curnode->data;

      res_cfg->status = AST_SUCCESS;
      if (res_cfg->agent_message) {
	free(res_cfg->agent_message);
	res_cfg->agent_message = NULL;
      }
    }
 
  if (glob_app_cfg->link_list)
    for (curnode = glob_app_cfg->link_list->head;
	 curnode;
	 curnode = curnode->next) {
      res_cfg = (struct resource*) curnode->data;

      res_cfg->status = AST_SUCCESS;
      if (res_cfg->agent_message) {
	free(res_cfg->agent_message);
	res_cfg->agent_message = NULL;
      }
    }
}

int
autofill_es_info(struct resource* res)
{
  int i;
  
  if (res->type != NODE_RES)
    return 0;

  for (i = 0; i < es_pool.number; i++) {
    if (strcmp(es_pool.es[i].ip, res->res.n.ip) == 0) {
      strncpy(res->res.n.router_id, es_pool.es[i].router_id, IP_MAXLEN);
      strncpy(res->res.n.tunnel, es_pool.es[i].tunnel, 9);
      return 1;
    }
  }
  return 0;
}
