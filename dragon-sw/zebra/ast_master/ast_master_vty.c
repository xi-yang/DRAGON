#include <zebra.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "thread.h"
#include "libxml/xmlmemory.h"
#include "libxml/parser.h"
#include "libxml/tree.h"
#include "vty.h"
#include "log.h"
#include "command.h"
#include "linklist.h"
#include "memory.h"
#include "buffer.h"
#include "ast_master_ext.h"
#include "ast_master.h"

char master_prompt[100] = "ast_master# ";
extern struct host host;
extern struct adtlist app_list;
extern char *status_type_details[];
extern char *action_type_details[];
extern char *node_stype_name[];
extern char *link_stype_name[];

struct cmd_node master_node =
{
  LSP_NODE,
  master_prompt,
  1
};

int master_config_write(struct vty *vty)
{
  if (host.name)   
    vty_out (vty, "hostname %s%s", host.name, VTY_NEWLINE);
  if (host.password)
        vty_out (vty, "password %s%s%s", host.password, VTY_NEWLINE, VTY_NEWLINE);

  return 0;
}

void print_flags(struct vty *vty, u_int32_t flags)
{
  if (flags & FLAG_SETUP_REQ)
    vty_out(vty, "SETUP_REQ | ");
  if (flags & FLAG_SETUP_RESP)
    vty_out(vty, "SETUP_RESP | ");
  if (flags & FLAG_AST_COMPLETE)
    vty_out(vty, "AST_COMPLETE | ");
  if (flags & FLAG_APP_COMPLETE)
    vty_out(vty, "APP_COMPLETE | ");
  if (flags & FLAG_RELEASE_REQ)
    vty_out(vty, "RELEASE_REQ | ");
  if (flags & FLAG_RELEASE_RESP)
    vty_out(vty, "RELEASE_RESP | ");
  if (flags & FLAG_UNFIXED)
    vty_out(vty, "RES_UNFIXED"); 
}

static void release_ast(char* ast_id)
{
  FILE *fp;

  fp = fopen(AST_XML_RECV, "w+");
  fprintf(fp, "<topology ast_id=\"%s\" action=\"RELEASE_REQ\"></topology>",
		  ast_id);
  fflush(fp);
  fclose(fp);

  glob_app_cfg = topo_xml_parser(AST_XML_RECV, MASTER);
  if (!glob_app_cfg) {
    zlog_info("internal error");
    return;
  }
  glob_app_cfg->clnt_sock = -1;

  master_process_release_req();

}

DEFUN (master_release,
       master_release_cmd,
       "release ast NAME",
       "Release an existing AST\n"
       "AST\n"
       "ast_id\n")
{
  struct adtlistnode *curnode;
  struct application_cfg *curcfg;

  if (argc == 0) {
    if (app_list.count == 0) {
      vty_out(vty, "no active ast; nothing will be released%s", VTY_NEWLINE);
      return CMD_SUCCESS; 
    }
    for (curnode = app_list.head;
	 curnode;
	 curnode = curnode->next) {
      curcfg = (struct application_cfg*) curnode->data;
      vty_out(vty, "Releasing ... %s [%s]%s", curcfg->ast_id, curcfg->xml_file, VTY_NEWLINE);
      zlog_info("Releasing ... %s [%s]\n", curcfg->ast_id, curcfg->xml_file);
      release_ast(curcfg->ast_id);
    }
  } else {
    glob_app_cfg = retrieve_app_cfg(argv[0], MASTER);
    if (!glob_app_cfg) {
      vty_out(vty, "ast_id: %s is not found in the active or achieved AST list%s", argv[0], VTY_NEWLINE);
      return CMD_SUCCESS;
    }
  
    if (glob_app_cfg->action == RELEASE_RESP) {
      vty_out(vty, "ast_id: %s has received RELEASE_REQ already%s", argv[0], VTY_NEWLINE);
      if (glob_app_cfg != search_cfg_in_list(glob_app_cfg->ast_id))
        free_application_cfg(glob_app_cfg);
      return CMD_SUCCESS;
    }
    release_ast(glob_app_cfg->ast_id);
  }

  return CMD_SUCCESS;
}

DEFUN(master_show_es,
      master_show_es_cmd,
      "show es",
      "Show registered end system")
{
  int i;
 
  vty_out(vty, "\t\t\t** Registered End System(s) ** %s%s", VTY_NEWLINE, VTY_NEWLINE);
  vty_out(vty, "%-20s%-20s%-15s%s", "IP", "1st hop loopback", "tunnel to 1st hop", VTY_NEWLINE);
  vty_out(vty, "----------------------------------------------------------------- %s", VTY_NEWLINE);

  for (i=0; i < es_pool.number; i++)
    vty_out(vty, "%-20s%-20s%-15s%s", 
	es_pool.es[i].ip, es_pool.es[i].router_id, es_pool.es[i].tunnel, VTY_NEWLINE);

  return CMD_SUCCESS;
}
	
/* all commands for ast_master */
DEFUN (master_show_ast,
       master_show_ast_cmd,
       "show ast NAME",
       SHOW_STR
       "Show ast status\n")
{
  struct adtlistnode *curnode, *curnode1;
  struct application_cfg *curcfg;
  struct resource *res;
  struct endpoint *src, *dest;
  struct if_ip *ifp;

  if (argc > 0) {
    curcfg = retrieve_app_cfg(argv[0], MASTER);
    if (!curcfg) {
      vty_out(vty, "Can't find ast (%s) %s", argv[0], VTY_NEWLINE);
      return (CMD_SUCCESS);
    }
    vty_out(vty, "%sAST %s status %s", VTY_NEWLINE, curcfg->ast_id, VTY_NEWLINE);
    vty_out(vty, "----------------------------------------------%s", VTY_NEWLINE);
    vty_out(vty, "overall status: %s %s", status_type_details[curcfg->status], VTY_NEWLINE);
    vty_out(vty, "action status: %s %s", action_type_details[curcfg->action], VTY_NEWLINE);
    if (curcfg->flags) { 
      vty_out(vty, "flags: "); 
      print_flags(vty, curcfg->flags); 
      vty_out(vty, VTY_NEWLINE);
    }
    if (curcfg->xml_file[0] != '\0')
      vty_out(vty, "xml_file: %s", curcfg->xml_file);

    if (!curcfg->node_list || !curcfg->link_list) {
      if (curcfg != search_cfg_in_list(curcfg->ast_id)) 
	free_application_cfg(curcfg);
 
      vty_out(vty, "%sTotal number of nodes: 0%s", VTY_NEWLINE, VTY_NEWLINE); 
      vty_out(vty, "%sTotal number of links: 0%s", VTY_NEWLINE, VTY_NEWLINE);

      return CMD_SUCCESS;
    }

    vty_out(vty, "%sTotal number of nodes: %d %s", VTY_NEWLINE, curcfg->node_list->count, VTY_NEWLINE);
    
    for (curnode = curcfg->node_list->head;
	 curnode;
	 curnode = curnode->next) {
      res = (struct resource*) curnode->data;

      vty_out(vty, "    NODE (%s) %s:%s", res->name, node_stype_name[res->res.n.stype], VTY_NEWLINE);
      if (res->status) 
	vty_out(vty, "\tstatus: %s%s", status_type_details[res->status], VTY_NEWLINE);
      if (res->agent_message)
	vty_out(vty, "\tdetails: %s%s", res->agent_message, VTY_NEWLINE);
      if (res->flags) { 
	vty_out(vty, "\tflags: "); 
	print_flags(vty, res->flags); 
	vty_out(vty, VTY_NEWLINE);
      }
      if (res->res.n.router_id[0] != '\0') 
        vty_out(vty, "\tIP: %s\trouter_id: %s%s", res->res.n.ip, res->res.n.router_id, VTY_NEWLINE);
      else
	vty_out(vty, "\tIP: %s\trouter_id: None%s", res->res.n.ip, VTY_NEWLINE);
      if (res->res.n.tunnel[0] != '\0') 
	vty_out(vty, "\tTunnel: %s%s", res->res.n.tunnel, VTY_NEWLINE);
      vty_out(vty, "\tnoded_sock: %d",  res->noded_sock);
      vty_out(vty, "\tdragon_sock: %d%s", res->dragon_sock, VTY_NEWLINE);
      if (res->res.n.command)
 	vty_out(vty, "\tCommand: %s%s", res->res.n.command, VTY_NEWLINE);
      if (res->res.n.if_list) {
	vty_out(vty, "\tInterfaces:%s", VTY_NEWLINE);
	for (curnode1 = res->res.n.if_list->head;
		curnode1;
		curnode1 = curnode1->next) {
	  ifp = (struct if_ip*) curnode1->data;
	  vty_out(vty,  "\t    %s [%s]%s", 
		  ifp->iface? ifp->iface:"TBA", ifp->assign_ip, VTY_NEWLINE);
	}
      }
    }

    vty_out(vty, "%sTotal number of links: %d %s", VTY_NEWLINE, curcfg->link_list->count, VTY_NEWLINE);
    for (curnode = curcfg->link_list->head;
	 curnode;
	 curnode = curnode->next) {
      res = (struct resource*) curnode->data;

      vty_out(vty, "    LINK (%s) %s:%s", res->name, link_stype_name[res->res.l.stype], VTY_NEWLINE);
      vty_out(vty, "\tstatus: %s%s", status_type_details[res->status], VTY_NEWLINE);
      if (res->status == AST_FAILURE && res->agent_message)
	vty_out(vty, "\t    details: %s%s", res->agent_message, VTY_NEWLINE);
      if (res->flags) { 
	vty_out(vty, "\tflags: "); 
	print_flags(vty, res->flags); 
	vty_out(vty, VTY_NEWLINE);
      }
      if (res->res.l.lsp_name[0] != '\0')
	vty_out(vty, "\tlsp_name: %s%s", res->res.l.lsp_name, VTY_NEWLINE);
      if (res->res.l.vtag[0] != '\0')
	vty_out(vty, "\tvtag: %s%s", res->res.l.vtag, VTY_NEWLINE);
      vty_out(vty, "\tsrc:%s", VTY_NEWLINE);
      src = res->res.l.src;
      dest = res->res.l.dest; 
      if (src->es)
	vty_out(vty, "\t    es: %s%s", src->es->name, VTY_NEWLINE);
      if (src->vlsr)
        vty_out(vty, "\t    vlsr: %s%s", src->vlsr->name, VTY_NEWLINE);
      if (src->proxy)
        vty_out(vty, "\t    proxy: %s%s", src->proxy->name, VTY_NEWLINE);
      if (src->local_id_type[0] != '\0')
	vty_out(vty, "\t    local_id: %s/%d%s", src->local_id_type, src->local_id, VTY_NEWLINE);
      vty_out(vty, "\tdest:%s", VTY_NEWLINE);
      if (dest->es)
	vty_out(vty, "\t    es: %s%s", dest->es->name, VTY_NEWLINE);
      if (dest->vlsr)
        vty_out(vty, "\t    vlsr: %s%s", dest->vlsr->name, VTY_NEWLINE);
      if (dest->proxy)
        vty_out(vty, "\t    proxy: %s%s", dest->proxy->name, VTY_NEWLINE);
      if (dest->local_id_type[0] != '\0')
	vty_out(vty, "\t    local_id: %s/%d%s", dest->local_id_type, dest->local_id, VTY_NEWLINE);
    } 
  } else {

    if (app_list.count == 0) {
      vty_out(vty, "No active AST on the list %s", VTY_NEWLINE);
      return (CMD_SUCCESS);
    }

    vty_out(vty, "\t\t\t** ACTIVE AST session(s) ** %s%s", VTY_NEWLINE, VTY_NEWLINE);
    vty_out(vty, "%20s%15s%15s%15s%s", "state", "status", "# of nodes", "# of links", VTY_NEWLINE);
    vty_out(vty, "----------------------------------------------------------------- %s", VTY_NEWLINE);

    for (curnode = app_list.head;
	 curnode;
	 curnode = curnode->next) {
      curcfg = (struct application_cfg*) curnode->data;

      vty_out(vty, "%s [%s]%s%20s%15s%15d%15d%s%s", 
		curcfg->ast_id, 
		curcfg->xml_file,
		VTY_NEWLINE,
		action_type_details[curcfg->action],
		status_type_details[curcfg->status], 
		curcfg->node_list->count,
		curcfg->link_list->count,
		VTY_NEWLINE,
		VTY_NEWLINE);        

    }    
  }

  if (curcfg != search_cfg_in_list(curcfg->ast_id))
    free_application_cfg(curcfg);

  return CMD_SUCCESS;
}

ALIAS (master_release,
       master_release_all_cmd,
       "release ast all",
       SHOW_STR
       "all active AST\n")

ALIAS (master_show_ast,
       master_show_ast_all_cmd,
       "show ast",
       SHOW_STR
       "Show ALL ast status\n");

DEFUN (master_set_es,
	master_set_es_cmd,
	"set es A.B.C.D A.B.C.D Tunnel",
	"Set es parameters\n"
	"ES\n"
	"end system IP address, where A, B, C, and D are integers 0 to 255\n"
	"first hop vlsr loopback address, where A, B, C, and D are integers 0 to 255\n"
	"control channel to first hop vlsr"
	)
{
  int index = es_pool.number;

  es_pool.es[index].ip = strdup(argv[0]);
  es_pool.es[index].router_id = strdup(argv[1]);
  es_pool.es[index].tunnel = strdup(argv[2]); 

  es_pool.number++;
  return CMD_SUCCESS;
}

void
master_supp_vty_init()
{
  install_node(&master_node, NULL);
  
  install_element(VIEW_NODE, &master_show_ast_cmd);
  install_element(VIEW_NODE, &master_show_ast_all_cmd);
  install_element(VIEW_NODE, &master_release_cmd);
  install_element(VIEW_NODE, &master_release_all_cmd);
  install_element(VIEW_NODE, &master_show_es_cmd);

  install_element(VIEW_NODE, &master_set_es_cmd);
  install_element(CONFIG_NODE, &master_set_es_cmd);
}
