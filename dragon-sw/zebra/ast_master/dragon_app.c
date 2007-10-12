/* File dragon_app.c
 * 
 * This file along with the corresponding .h (dragon_app.h) file defines all
 * dragon related resource functions.
 */
#include <zebra.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include "vty.h"
#include "libxml/xmlmemory.h"
#include "libxml/parser.h"
#include "libxml/tree.h"
#include "libxml/relaxng.h"
#include "ast_master_ext.h"
#include "buffer.h"
#include "log.h"
#include "local_id_cfg.h"
#include "dragon_app.h"

/*
struct res_mod {
  name;
  res_type;

  functions related to a resource:
  
  void *(*read_func)(struct application_cfg*, xmlNodePtr, int);
	read_func() will be used by all modules in ast: minions, 
	ast_master etc to read the resource from xmlNodePtr (res_details)
	RETURN:
		NULL: when failed to read
		the specific app_res pointer: when succeed

  int (*validate_func)(struct application_cfg*, void*, int);
	after reading the whole xml file, ast_master will call 
	validate_func on ALL resources, nodes first and then link;
	if there is any ptr within a resource that's pointing to another 
	resource, this is the func should do that
	RETURN: 
		0: no error 
		non-zero: error

  int (*process_setup) (struct application cfg* existing ast, 
			struct resource *the_existing_one,
			struct resource *the incoming_one,
			int agent) 
	Even though the whole struct resource is passing to the func,
	this function should only updates the void* res pointer and 
	xmlNodePtr; agent will differentiate ways to process.
		master - receving SETUP_RESP from minions
		minion - receving SETUP_REQ from minions

  int (*process_app) (struct application cfg* existing ast,
			struct resource *the_existing_one,
			struct resource *the incoming_one,
			int agent)
	used when
		master - receving app_complete
		minions - receving ast_complete

  int (*process_release) (struct application cfg* existing ast,
			struct resource *the_existing_one,
			struct resource *the incoming_one,
			int agent)
	used when 
		master - receving release_req
		minions - receving releaset_resp	
  

  int (*print_cli)(struct vty*, void*);
  void (*print_func)(FILE*, void*, int);
  void (*free_func) (void*);
};
*/
#define SQL_RESULT       	"/tmp/mysql.result"
#define XML_SERVICE_DEF_FILE    "/usr/local/ast_file/service_template.xml"

extern struct res_mods all_res_mod;
extern char *status_type_details[];
static struct dragon_link_profile *link_profile = NULL;

struct res_mod dragon_node_pc_mod = 
       {"dragon_node_pc", 
	res_node, 
	dragon_node_pc_read, 
	dragon_node_pc_validate,
	dragon_node_pc_process_resp,
	NULL,
	dragon_node_pc_print,
	dragon_node_pc_print_cli,
	dragon_node_pc_free,
        NULL};

struct res_mod dragon_link_mod =  
       {"dragon_link", 
	res_link, 
	dragon_link_read, 
	dragon_link_validate,
	dragon_link_process_resp,
	dragon_link_compose_req,
	dragon_link_print,
	dragon_link_print_cli,
	dragon_link_free,
	NULL};

char *link_stype_name[] =
  { "none",
    "uni",
    "non_uni",
    "vlsr_vlsr" };

char *link_status_name[] =
  { "did_not_commit",
    "commit",
    "error",
    "in-service",
    "release"};

struct string_syntex_check local_field =
{
	4,
	{{"lsp-id",	     "LSP-ID"},
	 {"port",	       "Untagged single port"},
	 {"tagged-group",       "Tagged group"},
	 {"group",	      "Untagged port group"}}
};

struct string_syntex_check bandwidth_field =
{
	23,
	{{"gige",       "GigE (1000.00 Mbps)"},
	 {"gige_f",     "Fast GigE (1250.00 Mbps)"},
	 {"2gige",      "2 GigE (2000.00 Mbps)"},
	 {"3gige",      "3 GigE (3000.00 Mbps)"},
	 {"4gige",      "4 GigE (4000.00 Mbps)"},
	 {"5gige",      "5 GigE (5000.00 Mbps)"},
	 {"6gige",      "6 GigE (6000.00 Mbps)"},
	 {"7gige",      "7 GigE (7000.00 Mbps)"},
	 {"8gige",      "8 GigE (8000.00 Mbps)"},
	 {"9gige",      "9 GigE (9000.00 Mbps)"},
	 {"eth100M",    "Ethernet (100.00 Mbps)"},
	 {"eth150M",    "Ethernet (150.00 Mbps)"},
	 {"eth200M",    "Ethernet (200.00 Mbps)"},
	 {"eth300M",    "Ethernet (300.00 Mbps)"},
	 {"eth400M",    "Ethernet (400.00 Mbps)"},
	 {"eth500M",    "Ethernet (500.00 Mbps)"},
	 {"eth600M",    "Ethernet (600.00 Mbps)"},
	 {"eth700M",    "Ethernet (700.00 Mbps)"},
	 {"eth800M",    "Ethernet (800.00 Mbps)"},
	 {"eth900M",    "Ethernet (900.00 Mbps)"},
	 {"10g",	"10GigE-LAN(10000.00 Mbps)"},
	 {"hdtv",       "HDTV/SMPTE-292 (1485.00 Mbps)"},
	 {"10g",	"10GigE-LAN(10000.00 Mbps)"},
	 {"hdtv",       "HDTV/SMPTE-292 (1485.00 Mbps)"},
	 {"oc48",       "OC-48/STM-16 (2488.32 Mbps)"}}
};

struct string_syntex_check swcap_field =
{
	4,
	{{"psc1",       "Packet-Switch Capable-1"},
	 {"l2sc",       "Layer-2 Switch Capable"},
	 {"lsc",	"Lambda Switch Capable"},
	 {"tdm",	"Time Division Multiplex Capable"}}
};

struct string_syntex_check encoding_field =
{
	4,
	{{"packet",     "Packet"},
	 {"ethernet",   "Ethernet"},
	 {"lambda",     "Lambda (photonic)"},
	 {"sdh",	"SONET/SDH"}}
};

struct string_syntex_check gpid_field =
{
	3,
	{{"ethernet",   "Ethernet"},
	 {"lambda",     "Lambda"},
	 {"sdh",	"SONET/SDH"}}
};

/* HELPER FUNCTIONS: TYPE DRAGON_NODE_PC */
int
autofill_es_info(struct resource* res)
{
  int i;
  struct dragon_node_pc *node;

  if (!res)
    return 1;

  if (strcmp(res->subtype->name, "dragon_node_pc") != 0) {
    zlog_err("autofill_es_info is only for dragon_node_pc");
    return 1;
  }

  if (res->res) {
    node = (struct dragon_node_pc *)res->res;
    if (node->router_id.s_addr != -1 && node->tunnel[0] != '\0') 
      return 0;
  }

  for (i = 0; i < es_pool.number; i++) {
    if (es_pool.es[i].ip.s_addr == res->ip.s_addr) {
      if (!res->res) {
	res->res = (void*)malloc(sizeof(struct dragon_node_pc));
	memset(res->res, 0, sizeof(struct dragon_node_pc));
	node = (struct dragon_node_pc *)res->res;
	node->router_id.s_addr = -1;
      }
      node = (struct dragon_node_pc *)res->res;

      if (node->router_id.s_addr == -1) 
	node->router_id.s_addr = es_pool.es[i].router_id.s_addr;
      if (node->tunnel[0] == '\0') 
	strncpy(node->tunnel, es_pool.es[i].tunnel, 9);

      return 0;
    }
  }

  return 1;
}

static int
lookup_assign_ip(struct resource *res)
{
  struct dragon_node_pc *src_node, *dest_node;
  struct dragon_endpoint *src_ep, *dest_ep;
  struct dragon_link *link;
  FILE *fp;
  char *f_ret, *token, line[100], addr[200];
  struct in_addr mask, ip;

  zlog_info("lookup_assign_ip() ... for link %s", res->name);

  link = (struct dragon_link *)res->res;
  if (link->stype == vlsr_vlsr)
    return 1;

  src_ep = link->src;
  dest_ep = link->dest;
  src_node = (struct dragon_node_pc*)src_ep->node->res;
  dest_node = (struct dragon_node_pc*)dest_ep->node->res;

  if (!src_ep->ifp) {
    src_ep->ifp = malloc(sizeof(struct dragon_if_ip));
    memset(src_ep->ifp, 0, sizeof(struct dragon_if_ip));
    if (!src_node->if_list) {
      src_node->if_list = malloc(sizeof(struct adtlist));
      memset(src_node->if_list, 0, sizeof(struct adtlist));
    }
    adtlist_add(src_node->if_list, src_ep->ifp);
    src_ep->ifp->vtag = atoi(link->vtag);
  } 
  if (!dest_ep->ifp) {
    dest_ep->ifp = malloc(sizeof(struct dragon_if_ip));
    memset(dest_ep->ifp, 0, sizeof(struct dragon_if_ip));
    if (!dest_node->if_list) {
      dest_node->if_list = malloc(sizeof(struct adtlist));
      memset(dest_node->if_list, 0, sizeof(struct adtlist));
    }
    adtlist_add(dest_node->if_list, dest_ep->ifp);
    dest_ep->ifp->vtag = atoi(link->vtag);
  }

  if (dest_ep->ifp->assign_ip && src_ep->ifp->assign_ip)
    return 0;

  system("mysql -hlocalhost -udragon -pflame -e \"use dragon; select * from data_plane_blocks where in_use='no' limit 1;\" > /tmp/mysql.result");

  /* error if 
   * SQL_RESULT doesn't start with slash_30
   * return file format:
   * slash_30	in_use 
   * 140.173.96.32   no
   *
   */
  fp = fopen(SQL_RESULT, "r");
  if (!fp) {
    return 1;
  }

  f_ret = fgets(line, 100, fp);
  if (!f_ret) {
    fclose(fp);
    zlog_err("lookup_assign_ip: sql server returns empty file");
    return 1;
  }

  if (strncmp(f_ret, "slash_30", 8) != 0) {
    fclose(fp);
    zlog_err("lookup_assign_ip: sql server returns error"); 
    return 1;
  }

  f_ret = fgets(line, 100, fp);
  if (!f_ret) {
    fclose(fp);
    zlog_err("lookup_assign_ip: sql server returns no slash_30");
    return 1;
  }

  token = strtok(f_ret, "\t");
  if (!token) { 
    fclose(fp);       
    zlog_err("lookup_assign_ip: sql server returns no slash_30");
    return 1;
  }
  zlog_info("for link %s, assign slash_30 %s", res->name, token);

  /* for src */
  mask.s_addr = inet_addr("0.0.0.1");
  ip.s_addr = inet_addr(token) | mask.s_addr;
  zlog_info("for src, assign %s", inet_ntoa(ip));
  sprintf(addr, "%s/30", inet_ntoa(ip));
  src_ep->ifp->assign_ip = strdup(addr);
 
  mask.s_addr = inet_addr("0.0.0.2");
  ip.s_addr = inet_addr(token) | mask.s_addr;
  zlog_info("for dest, assign %s", inet_ntoa(ip)); 
  sprintf(addr, "%s/30", inet_ntoa(ip));
  dest_ep->ifp->assign_ip = strdup(addr);

  sprintf(addr, "mysql -hlocalhost -udragon -pflame  -e \"use dragon; update data_plane_blocks set in_use='yes' where slash_30='%s';\"", token);

  system(addr);
  fclose(fp);

  return 0;
}

static int
cleanup_assign_ip(struct resource *res)
{
  char *c, addr[200];
  struct in_addr mask, ip, slash_30;
  struct dragon_endpoint *src_ep;
  struct dragon_link *link;

  link = (struct dragon_link*)res->res;
  if (link->stype == vlsr_vlsr)
    return 1;

  zlog_info("cleanup_assign_ip() ... for link %s", res->name);
  src_ep = link->src;
   
  if (!src_ep->ifp)
    return 0;
  if (!src_ep->ifp->assign_ip)
    return 0;
    
  c = strstr(src_ep->ifp->assign_ip, "/");
  if (!c)
    return 0;
  *c = '\0';
  ip.s_addr = inet_addr(src_ep->ifp->assign_ip);
  mask.s_addr = inet_addr("255.255.255.252");
  slash_30.s_addr = ip.s_addr & mask.s_addr;

  /* don't care if this addr is from SQL database or not, just remove it
   */
  sprintf(addr, "mysql -hlocalhost -udragon -pflame  -e \"use dragon; update data_plane_blocks set in_use='no' where slash_30='%s';\"", inet_ntoa(slash_30));
  system(addr);

  return 0;
}

static void
check_command(struct application_cfg *app_cfg, 
	      struct resource *res)
{
  struct resource *node_res;
  struct dragon_link *link;
  struct dragon_node_pc* node;
  struct dragon_endpoint *ep;
  char *command, *s, *d, *e, *f, *token;
  static char newcomm[300];
  int i, stop, change, nospace;

  zlog_info("check_command() ...");

  if (!res)
    return;
  link = (struct dragon_link *)res->res;
  if (!link)
    return;

  for (i = 0; i < 2; i++) {
    if (i == 0) {
      node_res = link->src->node;
      node = (struct dragon_node_pc*)node_res->res;
    } else {
      node_res = link->dest->node;
      node = (struct dragon_node_pc*)node_res->res;
    }

    if (!node->command) 
      continue;

    memset(newcomm, 0, 300);

    /* look for any "$<link_name>.src" "$<link_name>.dest"
     */
    command = node->command;
    change = stop = nospace = 0;
    token = strtok(command, " ");
    if (!token) {
      nospace = 1;
      token = command;
    } else 
      nospace = 0;
    do {
      if ((e = strstr(token, "$")) == NULL) {
	sprintf(newcomm+strlen(newcomm), "%s ", token);
	continue;
      } 
      if ((d = strstr(e, ".")) == NULL) {
	sprintf(newcomm+strlen(newcomm), "%s ", token);
	continue;
      }
      *d = '\0';

      /* now, d+1 should be either "src" or "dest" and 
       * token+1 should be the name of a link in this AST
       */
      if (strcmp(d+1, "src") != 0 && strcmp(d+1, "dest") != 0) 
	stop = 1;
      else if (strcmp(e+1, res->name) != 0)
        stop = 1;
      else { 
	if (strcmp(d+1, "src") == 0) {
	  ep = link->src; 
	  f = d+4;
	} else if (strcmp(d+1, "dest") == 0) {
	  ep = link->dest;
	  f = d+5;
	}
	if (!ep) 
	  stop = 1; 
	else if (!ep->ifp) 
	  stop = 1; 
	else if (!ep->ifp->assign_ip) 
	  stop = 1; 
      }
	
      if (stop) {
	*d = '.';
	sprintf(newcomm+strlen(newcomm), "%s ", token);
	continue;
      }

      s = strstr(ep->ifp->assign_ip, "/30");
      if (s)
	*s = '\0';
      change++;
      if (e == token) { 
	sprintf(newcomm+strlen(newcomm), "%s", ep->ifp->assign_ip);
	if (*f != '\0')
	  sprintf(newcomm+strlen(newcomm), "%s ", f);
	else
	  sprintf(newcomm+strlen(newcomm), " ");
      }
      else {
	*e = '\0';
	sprintf(newcomm+strlen(newcomm), "%s%s", token, ep->ifp->assign_ip);
	if (*f != '\0')
	  sprintf(newcomm+strlen(newcomm), "%s ", f);
	else
	  sprintf(newcomm+strlen(newcomm), " ");
      }

      if (s)
	*s = '/';
      *d = '.';
      stop = 0;
    } while (!nospace && (token = strtok(NULL, " ")) != NULL);

    newcomm[strlen(newcomm)-1] = '\0';
    if (change) 
      zlog_info("new command for %s: %s", node_res->name, newcomm);
    free(node->command);
    node->command = strdup(newcomm);
  }
}

/* HELPER FUNCTIONS: TYPE DRAGON_LINK */
struct dragon_link_profile*
get_profile_by_str(char* name)
{
  struct dragon_link_profile *cur;

  for (cur = link_profile;
	cur;
	cur = cur->next) {
    if (strcasecmp(cur->service_name, name) == 0)
      break;
  }
 
  return cur;
}

int
get_link_status_by_str(char* key)
{
  int i;

  for (i = 1; key && i <= delete; i++)
    if (strcasecmp(key, link_status_name[i]) == 0)
      return i;

  return 0;
}

/* HELPER FUNCTIONS: DRAGON_APPLICATION*/
static void
free_endpoint(struct dragon_if_ip* ifp)
{
  if (!ifp)
    return;

  if (ifp->iface)
    free(ifp->iface);

  free(ifp);
}

static void
print_endpoint(FILE* fp, 
	       struct dragon_endpoint* ep)
{
  if (!fp || !ep)
    return;

  if (ep->node)
    fprintf(fp, "\t\t\t<node>%s</node>\n", ep->node->name);
  if (ep->proxy)
    fprintf(fp, "\t\t\t<proxy>%s</proxy>\n", ep->proxy->name);
  if (ep->ifp) {
    if (ep->ifp->iface)
      fprintf(fp, "\t\t\t<iface>%s</iface>\n", ep->ifp->iface);
    if (ep->ifp->assign_ip)
      fprintf(fp, "\t\t\t<assign_ip>%s</assign_ip>\n", ep->ifp->assign_ip);
  }
  if (ep->local_id_type[0] != '\0')
    fprintf(fp, "\t\t\t<local_id>%c/%d</local_id>\n",
		ep->local_id_type[0], ep->local_id);
}

/* functions for resource type : dragon_node_pc
 */
void *
dragon_node_pc_read(struct application_cfg *app_cfg, 
		    xmlNodePtr xmlnode, 
		    int agent)
{
  struct dragon_node_pc *res;
  xmlNodePtr cur, link_ptr;
  struct dragon_if_ip *myifp;

  res = malloc(sizeof(struct dragon_node_pc));
  memset(res, 0, sizeof(struct dragon_node_pc));
  res->router_id.s_addr = -1;

  for (cur = xmlnode->xmlChildrenNode;
	cur;
	cur=cur->next) {

    if (strcasecmp((char*)cur->name, "router_id") == 0) 
      res->router_id.s_addr = inet_addr((char*)cur->children->content);
    else if (strcasecmp((char*)cur->name, "tunnel") == 0)
      strncpy(res->tunnel, (char*)cur->children->content, 10);
    else if (strcasecmp((char*)cur->name, "command") == 0)
      res->command = strdup((char*)cur->children->content);
    else if ((strcasecmp((char*)cur->name, "ifaces") == 0) && 
		(agent == NODE_AGENT || 
		(agent == MASTER && app_cfg->action == setup_resp))) {
      myifp = malloc(sizeof(struct dragon_if_ip));
      memset(myifp, 0, sizeof(struct dragon_if_ip));

      for (link_ptr = cur->xmlChildrenNode;
	    link_ptr;
	    link_ptr = link_ptr->next) {

	if (strcasecmp((char*)link_ptr->name, "iface") == 0)
	  myifp->iface = strdup((char*)link_ptr->children->content);
	else if (strcasecmp((char*)link_ptr->name, "assign_ip") == 0)
	  myifp->assign_ip = strdup((char*)link_ptr->children->content);
	else if (strcasecmp((char*)link_ptr->name, "vtag") == 0)
	  myifp->vtag = atoi((char*)link_ptr->children->content);
      }

      /* in face, if IP is not set, there is no need to play attention to
       * <iface> or link this interface to node
       */
      if (!myifp->assign_ip) {
	if (myifp->iface)
	  free(myifp->iface);
	free(myifp);
      } else {
	if (!res->if_list && agent != LINK_AGENT) {
	  res->if_list = malloc(sizeof(struct adtlist));
	  memset(res->if_list, 0, sizeof(struct adtlist));
	}
	adtlist_add(res->if_list, myifp);
      }
    }
  }

  return res;
} 

int 
dragon_node_pc_validate(struct application_cfg* app_cfg, 
			struct resource* res, int agent)
{
  struct dragon_node_pc *mynode;

  zlog_info("validating dragon_node_pc %s", res->name);

  if (app_cfg->action != setup_req || res->ip.s_addr == -1)
    return 0;

  if (agent != MASTER && agent != ASTB) {
    if (!res->res)
      return 1;
    mynode = (struct dragon_node_pc *)res->res;
  
    if (mynode->router_id.s_addr == -1)
      return 1;
  } else {

    /* even the node ip is defined; 
     * still need to figure out the tunnel and router_id */
    if ( agent == MASTER && autofill_es_info(res)) {
      if (mynode->router_id.s_addr == -1) {
	zlog_err("node (%s:%s) is not in our ES pool\n", res->name, inet_ntoa(res->ip)); 
	return 1;
      }
    }
  }

  return 0;
}

int
dragon_node_pc_process_resp(struct application_cfg *old_cfg,
			    struct resource *old_res,
			    struct resource *new_res)
{
  struct dragon_node_pc *old_node, *new_node;
  struct dragon_if_ip *old_ifp, *new_ifp;
  struct adtlistnode *curnode1, *curnode2;

  if (!old_res || !new_res)
    return 1;
  if (old_cfg->action != setup_resp) 
    return 0;

  old_node = (struct dragon_node_pc *)old_res->res;
  new_node = (struct dragon_node_pc *)new_res->res;

  if (old_node && new_node) {
    if (old_cfg->action == setup_resp) { 
      if (old_node->if_list && new_node->if_list)
	for (curnode1 = old_node->if_list->head, 
  	     curnode2 = new_node->if_list->head; 
	     curnode1 && curnode2; 
	     curnode1 = curnode1->next, curnode2 = curnode2->next) {
	  old_ifp = (struct dragon_if_ip*)curnode1->data;
	  new_ifp = (struct dragon_if_ip*)curnode2->data;

	  if (old_ifp->iface)
	    free(old_ifp->iface);
	  old_ifp->iface = new_ifp->iface;
	  new_ifp->iface = NULL;
	}
    }
  } else if (!old_node && new_node) {
    old_res->res = new_res->res;
    new_res->res = NULL;
  } else if (old_node && !new_node) 
    zlog_info("new update doesn't have any specific info on the resource");

  return 0;
}
 
int 
dragon_node_pc_compose_req(char* path, 
			   struct application_cfg* app_cfg, 
			   struct resource* res)
{
  if (!app_cfg || !app_cfg->node_list || !app_cfg->node_list->head || !res)
    return 2;

  return 1;
}

void 
dragon_node_pc_print(FILE* fp, 
		     void* res, 
		     int agent)
{
  struct dragon_node_pc *node;
  struct adtlistnode *curnode;
  struct dragon_if_ip* ifp;

  if (!fp || !res)
    return;

  node = (struct dragon_node_pc*)res;

  fprintf(fp, "\t<res_details>\n");
  if (node->router_id.s_addr != -1)
    fprintf(fp, "\t\t<router_id>%s</router_id>\n", inet_ntoa(node->router_id));
  if (node->tunnel[0] != '\0')
    fprintf(fp, "\t\t<tunnel>%s</tunnel>\n", node->tunnel);
  if (node->command)
    fprintf(fp, "\t\t<command>%s</command>\n", node->command);

  /* if this is sending to NODE_AGENT or MASTER */
  if (node->if_list) {
    for (curnode = node->if_list->head;
	 curnode;
	 curnode = curnode->next) {
      ifp = (struct dragon_if_ip*) curnode->data;

      fprintf(fp, "\t<ifaces>\n");
      if (ifp->iface)
	fprintf(fp, "\t\t<iface>%s</iface>\n", ifp->iface);
      if (ifp->assign_ip)
	fprintf(fp, "\t\t<assign_ip>%s</assign_ip>\n", ifp->assign_ip);
      if (ifp->vtag)
	fprintf(fp, "\t\t<vtag>%d</vtag>\n", ifp->vtag);
      fprintf(fp, "\t</ifaces>\n");
    }
  }
  fprintf(fp, "\t</res_details>\n");
}

void
dragon_node_pc_print_cli(struct vty *vty, 
			 void *res)
{
  struct dragon_node_pc *node;
  struct adtlistnode *curnode;
  struct dragon_if_ip *ifp;
  const char spacer[] = "	    ";

  if (!vty || !res)
    return;

  node = (struct dragon_node_pc*)res;

  vty_out(vty, "\tres_details:%s", VTY_NEWLINE);
  if (node->router_id.s_addr != -1)
    vty_out(vty, "%srouter_id: %s%s", spacer, inet_ntoa(node->router_id), VTY_NEWLINE);
  if (node->tunnel[0] != '\0')
    vty_out(vty, "%sTunnel: %s%s", spacer, node->tunnel, VTY_NEWLINE);
  if (node->command)
    vty_out(vty, "%sCommand: %s%s", spacer, node->command, VTY_NEWLINE);
  if (node->if_list) {
    vty_out(vty, "%sInterfaces:%s", spacer, VTY_NEWLINE);
    for (curnode = node->if_list->head;
	    curnode;
	    curnode = curnode->next) {
      ifp = (struct dragon_if_ip*) curnode->data;
      vty_out(vty,  "%s%s [%s]%s", spacer, 
		ifp->iface? ifp->iface:"TBA", 
		ifp->assign_ip, VTY_NEWLINE);
    }
  }
}

void
dragon_node_pc_free(void* res)
{
  struct dragon_node_pc *mynode;
  struct adtlistnode *curnode;
  struct dragon_if_ip *ifp;

  if (!res)
    return;

  mynode = (struct dragon_node_pc*)res;

  if (mynode->if_list) {
    for (curnode = mynode->if_list->head;
	 curnode;
	 curnode = curnode->next) {
      if (!mynode->link_list)  {
	ifp = (struct dragon_if_ip*) curnode->data;

	free(ifp->iface);
	ifp->iface = NULL;
	free(ifp->assign_ip);
	ifp->assign_ip = NULL;
	free(ifp);
      }
      curnode->data = NULL;
    }
  }
  adtlist_free(mynode->if_list);
  if (mynode->link_list) {
    /* since both node->link_list and link_list shares
     * the same ptr of data.  need to make sure
     * we won't free the data here
     */
    for (curnode = mynode->link_list->head;
	 curnode;
	 curnode = curnode->next) {
      curnode->data = NULL;
    }
    adtlist_free(mynode->link_list);
  }
  if (mynode->command)
    free(mynode->command);
  free(mynode);
}

void*
dragon_link_read(struct application_cfg* app_cfg, 
	         xmlNodePtr xmlnode, 
		 int agent)
{
  struct dragon_link *res;
  xmlNodePtr cur, node_ptr;
  struct dragon_endpoint *ep;
  struct dragon_node_pc *node_res;
  struct _xmlAttr* attr;
  int i;

  res = malloc(sizeof(struct dragon_link));
  memset(res, 0, sizeof(struct dragon_link));

  /* first extract the subtype for dragon_link */
  for ( attr = xmlnode->properties;
	attr;
	attr = attr->next) {
    if (strcasecmp((char*)attr->name, "dragon_type") == 0) {
      for (i = 1; i <= NUM_DRAGON_LINK_TYPE; i++ ) {
	if (strcasecmp((char*)attr->children->content, link_stype_name[i]) == 0) {
	  res->stype = i;
	  break;
	}
      }
    }
  }

  for (cur = xmlnode->xmlChildrenNode;
	cur;
	cur=cur->next) {
    if (strcasecmp((char*)cur->name, "src") == 0 ||
	strcasecmp((char*)cur->name, "dest") == 0) {

      ep = (struct dragon_endpoint*) malloc(sizeof(struct dragon_endpoint));
      memset(ep, 0, sizeof(struct dragon_endpoint));
      
      for (node_ptr = cur->xmlChildrenNode;
	   node_ptr;
	   node_ptr = node_ptr->next) {

	if (strcasecmp((char*)node_ptr->name, "node") == 0) {
	  ep->node = search_res_by_name(app_cfg, res_node, (char*)node_ptr->children->content);
	  if (!ep->node) {
	    if (ep->ifp) {
	      if (ep->ifp->iface) 
		free(ep->ifp->iface); 
	    } 
	    free(ep); 
	    ep = NULL;
	    break; 
	  } 
	} else if (strcasecmp((char*)node_ptr->name, "proxy") == 0)
	  ep->proxy = search_res_by_name(app_cfg, res_node, (char*)node_ptr->children->content);
	else if (strcasecmp((char*)node_ptr->name, "local_id") == 0) {
	  switch(node_ptr->children->content[0]) {
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
	      zlog_err("invalid local_id type: %s", (char*)node_ptr->children->content);
	  }

	  if (strlen(ep->local_id_type) != 0)
	    ep->local_id = atoi((char*)node_ptr->children->content+2);

	} else if (res->stype != vlsr_vlsr) {

	  if (agent != NODE_AGENT &&
		(strcasecmp((char*)node_ptr->name, "iface") == 0 || 
		strcasecmp((char*)node_ptr->name, "assign_ip") == 0 ||
		strcasecmp((char*)node_ptr->name, "vtag") == 0)) {
	    if (!ep->ifp) {
	      ep->ifp = (struct dragon_if_ip*) malloc (sizeof(struct dragon_if_ip));
	      memset(ep->ifp, 0, sizeof(struct dragon_if_ip));
	    }
	    
	    if (strcasecmp((char*)node_ptr->name, "iface") == 0) 
	      ep->ifp->iface = strdup((char*)node_ptr->children->content); 
	    else if (strcasecmp((char*)node_ptr->name, "assign_ip") == 0) 
	      ep->ifp->assign_ip = strdup((char*)node_ptr->children->content); 
	    else if (strcasecmp((char*)node_ptr->name, "vtag") == 0) 
	      ep->ifp->vtag = atoi((char*)node_ptr->children->content);
	  }
	}
      }

      if (!ep)
	continue;

      if (!ep->node) {
	if (ep->ifp && ep->ifp->iface) 
	    free(ep->ifp->iface);
	free(ep);
	continue;
      }

      if (strcasecmp((char*)cur->name, "src") == 0) 
	res->src = ep;
      else 
	res->dest = ep;

      if (ep->ifp && agent != LINK_AGENT) {
	/* put this if_ip into if_list in node
	 */
	if (ep->node && ep->node->res) { 
	  /* FIONA:
	   * should check the subtype of this node_res before casting
	   * to dragon_node_pc*
	   */
	  node_res = (struct dragon_node_pc*)ep->node->res; 
	  if (!node_res->if_list) { 
	    node_res->if_list = malloc(sizeof(struct adtlist));
	    memset(node_res->if_list, 0, sizeof(struct adtlist));
	  }
	  adtlist_add(node_res->if_list, ep->ifp);
	}
      }

    } else if (strcasecmp((char*)cur->name, "te_params") == 0) {

      if ((agent == MASTER || agent == ASTB) 
		&& app_cfg->action == setup_req) {

	for (attr = cur->properties;
	     attr;
	     attr = attr->next) {

	  if (strcasecmp((char*)attr->name, "profile") == 0) {

	    if (agent == ASTB) {
	      res->profile = malloc(sizeof(struct dragon_link_profile));
	      strcpy(res->profile->service_name, (char*)attr->children->content);
	    } else {
              res->profile = get_profile_by_str((char*)attr->children->content);
              if (!res->profile)
                zlog_err("service type (%s) unknown", (char*)attr->children->content);
              else {
                strcpy(res->bandwidth, res->profile->bandwidth);
                strcpy(res->swcap, res->profile->swcap);
                strcpy(res->encoding, res->profile->encoding);
                strcpy(res->gpid, res->profile->gpid);
              }
	    }
	  }
	}
      }

      for (node_ptr = cur->xmlChildrenNode;
	    node_ptr;
	    node_ptr = node_ptr->next) {

	if (strcasecmp((char*)node_ptr->name, "bandwidth") == 0)
	  strncpy(res->bandwidth, (char*)node_ptr->children->content, REG_TXT_FIELD_LEN);
	else if (strcasecmp((char*)node_ptr->name, "swcap") == 0)
	  strncpy(res->swcap, (char*)node_ptr->children->content, REG_TXT_FIELD_LEN);
	else if (strcasecmp((char*)node_ptr->name, "encoding") == 0)
	  strncpy(res->encoding, (char*)node_ptr->children->content, REG_TXT_FIELD_LEN);
	else if (strcasecmp((char*)node_ptr->name, "gpid") == 0)
	  strncpy(res->gpid, (char*)node_ptr->children->content, REG_TXT_FIELD_LEN);
	else if (strcasecmp((char*)node_ptr->name, "vtag") == 0) {
	  strncpy(res->vtag, (char*)node_ptr->children->content, REG_TXT_FIELD_LEN);
	  if (res->src && res->src->ifp)
	    res->src->ifp->vtag = atoi((char*)node_ptr->children->content);
	  if (res->dest && res->dest->ifp)
	    res->dest->ifp->vtag = atoi((char*)node_ptr->children->content);
	}
      }
    } else if (strcasecmp((char*)cur->name, "lsp_name") == 0 &&
	    app_cfg->action != setup_req )
      strncpy(res->lsp_name, (char*)cur->children->content, LSP_NAME_LEN);
    else if (strcasecmp((char*)cur->name, "dragon") == 0)
      res->dragon = search_res_by_name(app_cfg, res_node, (char*)cur->children->content);
    else if (strcasecmp((char*)cur->name, "link_status") == 0)
      res->l_status = get_link_status_by_str((char*)cur->children->content);
  }

  return res;
}

int
dragon_link_validate(struct application_cfg* app_cfg, 
		     struct resource* res, 
		     int agent)
{
  struct resource *src_res, *dest_res;
  struct dragon_link *mylink;
  struct dragon_endpoint *src_ep, *dest_ep;
  struct dragon_node_pc *src_pc, *dest_pc;

  zlog_info("validating dragon_link %s", res->name);

  if (app_cfg->action == setup_req) {
    mylink = (struct dragon_link*) res->res;

    src_ep = mylink->src;
    dest_ep = mylink->dest;

    if (!src_ep || !dest_ep) {
      zlog_err("link (%s) should have src and dest defined", res->name);
      return 1;
    }

    src_res = src_ep->node;
    dest_res = dest_ep->node;

    if (!src_res || !dest_res) {
      zlog_err("link (%s) should have node in both src and dest", res->name);
      return 1;
    }
    src_pc = (struct dragon_node_pc*)src_res->res;
    dest_pc = (struct dragon_node_pc*)dest_res->res;

    switch (mylink->stype) {

      case non_uni:

	if (strcmp(src_res->subtype->name, "dragon_node_pc") != 0 ||
	    strcmp(src_res->subtype->name, "dragon_node_pc") != 0) {
	  zlog_err("for dragon_link stype: non_uni, src and dest should be dragon_node_pc");
	  return 1;
	}

	if (src_ep->local_id_type[0] == '\0' && dest_ep->local_id_type[0] == '\0') {
	  sprintf(src_ep->local_id_type, "lsp-id");
	  sprintf(dest_ep->local_id_type, "lsp-id");
	  src_ep->local_id = random()%65533 + 1;
	  dest_ep->local_id = random()%65533 + 1;;
	}

goto common_peer_mode_check;

	break;

      case uni:
	  
	if (strcmp(src_res->subtype->name, "dragon_node_pc") != 0 ||
	    strcmp(src_res->subtype->name, "dragon_node_pc") != 0) {
	  zlog_err("for dragon_link stype: non_uni, src and dest should be dragon_node_pc");
	  return 1;
	}

	if (src_ep->local_id_type[0] == 'l' ||
	    dest_ep->local_id_type[0] == 'l') {
	  zlog_err("for dragon_link stype: uni, both src and dest should have local_id_type for uni");
	  return 1;
	}

	if ((src_ep->local_id_type[0] == '\0' && dest_ep->local_id_type[0] == '\0') &&
	   mylink->vtag[0] != '\0') {
	  sprintf(src_ep->local_id_type, "tagged-group");
	  sprintf(dest_ep->local_id_type, "tagged-group");
	  src_ep->local_id = atoi(mylink->vtag);
	  dest_ep->local_id = atoi(mylink->vtag);
	}
common_peer_mode_check:

	if (src_ep->local_id_type[0] == '\0' && dest_ep->local_id_type[0] != '\0')
	  dest_ep->local_id_type[0] = '\0';
	else if (src_ep->local_id_type[0] != '\0' && dest_ep->local_id_type[0] == '\0')
	  src_ep->local_id_type[0] = '\0';

	if (src_ep->ifp && !dest_ep->ifp) {
	  free_endpoint(src_ep->ifp);
	  src_ep->ifp = NULL;
	} else if (!src_ep->ifp && dest_ep->ifp) {
	  free_endpoint(dest_ep->ifp);
	  dest_ep->ifp = NULL;
	} else if (src_ep->ifp && dest_ep->ifp) {
	  if (!src_ep->ifp->assign_ip && dest_ep->ifp->assign_ip) {
	    free(dest_ep->ifp->assign_ip);
	    dest_ep->ifp->assign_ip = NULL;
	  } else if (src_ep->ifp->assign_ip && !dest_ep->ifp->assign_ip) {
	    free(src_ep->ifp->assign_ip);
	    src_ep->ifp->assign_ip = NULL;
	  }
	}

	if (agent == MASTER) {
	  if (IS_RES_UNFIXED(src_res) || IS_RES_UNFIXED(dest_res))
	    break;

	  if (!src_pc || !dest_pc) 
	    return 1;

	  if (src_pc->router_id.s_addr == -1 || dest_pc->router_id.s_addr == -1) {
	    zlog_err("link (%s) should have <router_id> defined in its src and dest node", res->name);
	    return 1;
	  }
	  if (mylink->stype == uni) {
	    if (src_pc->tunnel[0] == '\0' && dest_pc->tunnel[0] == '\0') {
	      zlog_err("link (%s) should have <tunnel> defined in its src and dest node", res->name);
	      return 1;
	    }
	  }
	} else if (agent == LINK_AGENT) {
	  if (src_pc->router_id.s_addr == -1 || dest_pc->router_id.s_addr == -1) {
	    zlog_err("link (%s) should have <router_id> defined in its src and dest node", res->name);
	    return 1;
	  }
	  if (mylink->stype == uni) {
	    if (src_pc->tunnel[0] == '\0'  || dest_pc->tunnel[0] == '\0') {
	      zlog_err("link (%s) should have <tunnel> defined in its src and dest node", res->name);
	      return 1;
	    }
	  }
	} 
	break;

      case vlsr_vlsr:
	if (src_ep->local_id_type[0] == 'l' || dest_ep->local_id_type[0] == 'l' ||
	    src_ep->local_id_type[0] == '\0' || dest_ep->local_id_type[0] == '\0') {
	    zlog_err("for %s type vlsr_vlsr, <local_id> in <src> or <dest> has to be \"tagged-group\", \"group\", \"port\".\n", res->name);

	  return 1;
	}
	break;
 
      default:
	break;
    }
    res->ip.s_addr = src_res->ip.s_addr;
    /* add res to src_res link_list */
   
    if (agent != ASTB) {
     
      if (!src_pc->link_list) {
        src_pc->link_list = malloc(sizeof(struct adtlist));
        memset(src_pc->link_list, 0, sizeof(struct adtlist));
      }
      adtlist_add(src_pc->link_list, res);    

    } 

    if (IS_RES_UNFIXED(src_res))
      res->flags |= FLAG_UNFIXED;
  }

  return 0;
}

int
dragon_link_process_resp(struct application_cfg *old_cfg,
			 struct resource *old_res,
			 struct resource *new_res)
{
  struct dragon_link *old_link, *new_link;

  if (!old_res || !new_res)
    return 1;

  if (old_cfg->action != setup_resp)
    return 0;

  old_link = (struct dragon_link *)old_res->res;
  new_link = (struct dragon_link *)new_res->res;

  if (old_link && new_link) {
    zlog_info("Link: %s, %s, LSP: %s vtag: %s", new_res->name, 
  	    status_type_details[new_res->status], 
  	    new_link->lsp_name, new_link->vtag);
  
    /* parameters update */ 
    strcpy(old_link->lsp_name, new_link->lsp_name); 
    old_link->l_status = new_link->l_status;
    if (strcmp(new_link->vtag, "65535") != 0) { 
      if (old_link->vtag[0] == '\0' || strcmp(old_link->vtag, "any") == 0) 
	strcpy(old_link->vtag, new_link->vtag);
    }

    /* After getting the vtag, need to pass it to the if_ip and
     * run assign_ip */
    if (old_res->status == ast_success && lookup_assign_ip(old_res) == 0)
      check_command(old_cfg, old_res);

  } else if (!old_link && new_link) {
    old_res->res = new_res->res;
    new_res->res = NULL;
  } else if (old_link && !new_link) 
    zlog_info("new update doesn't have any specific info on the resource");

  return 0;
}

/* Return 
 *	0: no error
 *	1: compse_req will not take care of this type of action
 *	2: fundamental error
 */
int
dragon_link_compose_req(char* path, 
			struct application_cfg* app_cfg, 
			struct resource* res) 
{
  FILE *fp;
  struct dragon_link *link;
  struct dragon_node_pc *src_node;
  struct adtlistnode *curnode;
  struct resource *res2;

  if (!res || !app_cfg || !path || strlen(path) == 0)
    return 2;

  if (app_cfg->action != setup_req && 
      app_cfg->action != release_req &&
      app_cfg->action != ast_complete)
    return 1;

  fp = fopen(path, "w+");
  if (!fp) {
    zlog_err("Can't open the file %s; error = %d(%s)",
		path, errno, strerror(errno));
    return 2;
  }

  link = (struct dragon_link*)res->res;
  fprintf(fp, "<topology ast_id=\"%s\" action=\"%s\">\n", app_cfg->ast_id, action_type_details[app_cfg->action]);
  print_res_list(fp, app_cfg->node_list, MASTER);

  if (link->src->node && link->src->node->res) { 
    src_node = (struct dragon_node_pc*)link->src->node->res;
    if (src_node->link_list) {
      print_res_list(fp, src_node->link_list, MASTER);
      for (curnode = src_node->link_list->head;
	   curnode;
	   curnode = curnode->next) {
	res2 = (struct resource*)curnode->data;
	cleanup_assign_ip(res2);
      }
    }
  } else
    print_res(fp, res, MASTER);

  fprintf(fp, "</topology>\n");
  fflush(fp);
  fclose(fp); 

  return 0;
}

void
dragon_link_print(FILE* fp, 
		  void* res, 
		  int print_type)
{
  struct dragon_link *link;

  if (!fp || !res)
    return;
  link = (struct dragon_link *)res;
 
  fprintf(fp, "\t<res_details dragon_type=\"%s\">\n",
		link_stype_name[link->stype]);
  fprintf(fp, "\t\t<link_status>%s</link_status>\n", link_status_name[link->l_status]);
  if (link->lsp_name[0] != '\0')
    fprintf(fp, "\t\t<lsp_name>%s</lsp_name>\n", link->lsp_name);
  if (link->src) {
    fprintf(fp, "\t\t<src>\n");
    print_endpoint(fp, link->src);
    fprintf(fp, "\t\t</src>\n");
  }
  if (link->dest) {
    fprintf(fp, "\t\t<dest>\n");
    print_endpoint(fp, link->dest);
    fprintf(fp, "\t\t</dest>\n");
  }
  if (link->profile)
    fprintf(fp, "\t\t<te_params profile=\"%s\">\n", link->profile->service_name);
  else
    fprintf(fp, "\t\t<te_params>\n");
  if (link->bandwidth[0] != '\0')
    fprintf(fp, "\t\t\t<bandwidth>%s</bandwidth>\n", link->bandwidth);
  if (link->swcap[0] != '\0')
    fprintf(fp, "\t\t\t<swcap>%s</swcap>\n", link->swcap);
  if (link->encoding[0] != '\0')
    fprintf(fp, "\t\t\t<encoding>%s</encoding>\n", link->encoding);
  if (link->gpid[0] != '\0')
    fprintf(fp, "\t\t\t<gpid>%s</gpid>\n", link->gpid);
  if (link->vtag[0] != '\0')
    fprintf(fp, "\t\t\t<vtag>%s</vtag>\n", link->vtag);
  fprintf(fp, "\t\t</te_params>\n");
  fprintf(fp, "\t</res_details>\n");
}

void
dragon_link_print_cli(struct vty *vty, 
		      void *res)
{
  struct dragon_link *link;
  struct dragon_endpoint *src, *dest;
  const char spacer[] = "	    ";

  if (!vty || !res)
    return;

  link = (struct dragon_link *)res;

  vty_out(vty, "\tres_details:%s", VTY_NEWLINE);
  if (link->lsp_name[0] != '\0')
    vty_out(vty, "%slsp_name: %s%s", spacer, link->lsp_name, VTY_NEWLINE);
  src = link->src;
  dest = link->dest;

  vty_out(vty, "%ssrc:%s", spacer, VTY_NEWLINE);
  if (src->node)
    vty_out(vty, "%s  node: %s%s", spacer, src->node->name, VTY_NEWLINE);
  if (src->local_id_type[0] != '\0')
    vty_out(vty, "%s  local_id: %s/%d%s", spacer, src->local_id_type, src->local_id, VTY_NEWLINE);
  if (src->ifp && src->ifp->assign_ip)
    vty_out(vty, "%s  assign_ip: %s%s", spacer, src->ifp->assign_ip, VTY_NEWLINE);

  vty_out(vty, "%sdest:%s", spacer, VTY_NEWLINE);
  if (dest->node)
    vty_out(vty, "%s  node: %s%s", spacer, dest->node->name, VTY_NEWLINE);
  if (dest->local_id_type[0] != '\0')
    vty_out(vty, "%s  local_id: %s/%d%s", spacer, dest->local_id_type, dest->local_id, VTY_NEWLINE);
  if (dest->ifp && dest->ifp->assign_ip)
    vty_out(vty, "%s  assign_ip: %s%s", spacer, dest->ifp->assign_ip, VTY_NEWLINE);

  vty_out(vty, "%ste_params:%s", spacer, VTY_NEWLINE);
  if (link->profile)
    vty_out(vty, "%s  profile: %s%s", spacer, link->profile->service_name, VTY_NEWLINE);
  if (link->bandwidth[0] != '\0')
    vty_out(vty, "%s  bandwidth: %s\t", spacer, link->bandwidth);
  if (link->swcap[0] != '\0')
    vty_out(vty, "swcap: %s%s", link->swcap, VTY_NEWLINE);
  if (link->encoding[0] != '\0')
    vty_out(vty, "%s  encoding: %s\t", spacer, link->encoding);
  if (link->gpid[0] != '\0')
    vty_out(vty, "gpid: %s%s", link->gpid, VTY_NEWLINE);
  if (link->vtag[0] != '\0')
    vty_out(vty, "%s  vtag: %s%s", spacer, link->vtag, VTY_NEWLINE);
}

void
dragon_link_free(void* res)
{
  struct dragon_link *mylink;
 
  if (!res)
    return;
  mylink = (struct dragon_link *)res;
 
  if (mylink->src) {
    if (mylink->src->ifp) {
      if (mylink->src->ifp->iface) 
	free(mylink->src->ifp->iface);
      free(mylink->src->ifp);
    }
    free(mylink->src);
  }
  if (mylink->dest) {
    if (mylink->dest->ifp) {
      free(mylink->dest->ifp->iface);
      free(mylink->dest->ifp);
    }
    free(mylink->dest);
  }
  free(mylink);
}

void
dragon_link_profile_read()
{
  struct dragon_link_profile *link;
  xmlChar *key;
  xmlDocPtr doc;
  xmlNodePtr cur, i_ptr;
  struct _xmlAttr* attr;
  int i, field = 0, err;

  link_profile = NULL;
  doc = xmlParseFile(XML_SERVICE_DEF_FILE);
  if (doc == NULL) 
    return;

  cur = xmlDocGetRootElement(doc);

  if (cur == NULL) {
    xmlFreeDoc(doc);
    return;
  }
  
  cur = findxmlnode(cur, "service_template");
  if (!cur) {
    xmlFreeDoc(doc);
    return;
  }
  
  for (err = 0, cur = cur->xmlChildrenNode;
	cur;
	err = 0, cur = cur->next) {

    if (strcasecmp(cur->name, "service") == 0) {
      link = (struct dragon_link_profile*)malloc(sizeof(struct dragon_link_profile));
      memset(link, 0, sizeof(struct dragon_link_profile));

      for (attr = cur->properties;
	   attr;
	   attr = attr->next) {
	if (strcasecmp(attr->name, "type") == 0) 
	  strncpy(link->service_name, attr->children->content, REG_TXT_FIELD_LEN);
      }

      if (link->service_name[0] == '\0')
	err = 1;

      for (field = 0, i_ptr = cur->xmlChildrenNode;
	   !err && i_ptr;
	   i_ptr = i_ptr->next) {
	key = xmlNodeListGetString(doc, i_ptr->xmlChildrenNode, 1);

        if (!key)
	  continue;

	if (strcasecmp(i_ptr->name, "bandwidth") == 0) {
	  strncpy(link->bandwidth, key, REG_TXT_FIELD_LEN);
	  if (link->bandwidth[0] != '\0') {
	    for (i = 0; i < bandwidth_field.number; i++) {
	      if (strcasecmp(link->bandwidth, bandwidth_field.ss[i].abbre) == 0)
	        break;
	    }
	    if (i == bandwidth_field.number) {
	      zlog_err("Invalid value for bandwidth: %s", link->bandwidth);
	      err = 1;
	    } else
	      field++;
	  }
	} else if (strcasecmp(i_ptr->name, "swcap") == 0) {
	  strncpy(link->swcap, key, REG_TXT_FIELD_LEN);
	  for (i = 0; i < swcap_field.number; i++) {
	    if (strcasecmp(link->swcap, swcap_field.ss[i].abbre) == 0)
	      break;
	  }
	  if (i == swcap_field.number) {
	    zlog_err("Invalid value for swcap: %s", link->swcap);
	    err = 1;
	  } else
	    field++;
	} else if (strcasecmp(i_ptr->name, "encoding") == 0) {
	  strncpy(link->encoding, key, REG_TXT_FIELD_LEN);
	  for (i = 0; i < encoding_field.number; i++) {
	    if (strcasecmp(link->encoding, encoding_field.ss[i].abbre) == 0)
	      break;
	  }
	  if (i == encoding_field.number) {
	    zlog_err("Invalid value for encoding: %s", link->encoding);
	    err = 1;
	  } else
	    field++;
	} else if (strcasecmp(i_ptr->name, "gpid") == 0) {
	  strncpy(link->gpid, key, REG_TXT_FIELD_LEN);
	  for (i = 0; i < gpid_field.number; i++) {
	    if (strcasecmp(link->gpid, gpid_field.ss[i].abbre) == 0)
	      break;
	  }
	  if (i == gpid_field.number) {
	    zlog_err("Invalid value for gpid: %s", link->gpid);
	    err = 1;
	  } else
	    field++;
	}
      }

      if (!err && field == 4) {
        link->next = link_profile;
	link_profile= link; 
      } else 
	free(link);
    }
  }

  xmlFreeDoc(doc);
}

/* Add dragon related functions into rmodule[]
 * so that ast_master/minions will know how to deal with these resource
 * 
 * When building custom agency for minions, i.e. (dragond and node_agent for 
 * DRAGON app), users can include this file in compilation
 */
int 
init_dragon_module()
{
  if (all_res_mod.total + 2 > 10) {
    zlog_info("Failed to add the dragon application module; too many resource in this instance");
    return 1;
  }

  dragon_link_profile_read();

  all_res_mod.res[all_res_mod.total] = &dragon_node_pc_mod;
  all_res_mod.res[all_res_mod.total+1] = &dragon_link_mod;
  all_res_mod.total+=2;
    
  return 0; 
}

