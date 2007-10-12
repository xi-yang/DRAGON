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
void release_ast(char*);

static void print_flags(struct vty *, u_int32_t);
int master_process_release_req();

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

void 
print_cli_res(struct vty *vty, struct resource *res)
{
  if (!vty || !res)
    return;

  vty_out(vty, "    %s (%s) TYPE (%s):%s", 
		(res->res_type == res_node) ? "NODE":"LINK",
		res->name, res->subtype->name, VTY_NEWLINE);
  if (res->ip.s_addr != -1)
    vty_out(vty, "\tIP: %s\t", inet_ntoa(res->ip));
  if (res->status) 
    vty_out(vty, "\tstatus: %s%s", status_type_details[res->status], VTY_NEWLINE);
  if (res->agent_message)
    vty_out(vty, "\tagent_message: %s%s", res->agent_message, VTY_NEWLINE);
  if (res->flags) { 
    vty_out(vty, "\tflags: "); 
    print_flags(vty, res->flags); 
    vty_out(vty, VTY_NEWLINE);
  }

  vty_out(vty, "\tminion_sock: %d%s",  res->minion_sock, VTY_NEWLINE);

  if (res->res && res->subtype->mod && res->subtype->mod->print_cli_func)
    res->subtype->mod->print_cli_func(vty, res->res);
}

void
print_cli_res_list(struct vty *vty, struct adtlist *list)
{
  struct adtlistnode *curnode;

  if (!vty || !list)
    return;

  for (curnode = list->head;
       curnode;
       curnode = curnode->next)
    print_cli_res(vty, (struct resource*)curnode->data);
}

static void 
print_flags(struct vty *vty, u_int32_t flags)
{
  if (flags & FLAG_SETUP_REQ)
    vty_out(vty, "setup_req | ");
  if (flags & FLAG_SETUP_RESP)
    vty_out(vty, "setup_resp | ");
  if (flags & FLAG_AST_COMPLETE)
    vty_out(vty, "ast_complete | ");
  if (flags & FLAG_APP_COMPLETE)
    vty_out(vty, "app_complete | ");
  if (flags & FLAG_RELEASE_REQ)
    vty_out(vty, "release_req | ");
  if (flags & FLAG_RELEASE_RESP)
    vty_out(vty, "release_resp | ");
  if (flags & FLAG_UNFIXED)
    vty_out(vty, "res_unfixed"); 
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
      zlog_info("Releasing ... %s [%s]", curcfg->ast_id, curcfg->xml_file);
      release_ast(curcfg->ast_id);
    }
  } else {
    glob_app_cfg = retrieve_app_cfg(argv[0], MASTER);
    if (!glob_app_cfg) {
      vty_out(vty, "ast_id: %s is not found in the active or achieved AST list%s", argv[0], VTY_NEWLINE);
      return CMD_SUCCESS;
    }
  
    if (glob_app_cfg->action == release_resp) {
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

  for (i=0; i < es_pool.number; i++) {
    vty_out(vty, "%-20s", inet_ntoa(es_pool.es[i].ip));
    vty_out(vty, "%-20s%-15s%s", 
	inet_ntoa(es_pool.es[i].router_id), es_pool.es[i].tunnel, VTY_NEWLINE);
  }

  return CMD_SUCCESS;
}
	
/* all commands for ast_master */
DEFUN (master_show_ast,
       master_show_ast_cmd,
       "show ast NAME",
       SHOW_STR
       "Show ast status\n")
{
  struct adtlistnode *curnode;
  struct application_cfg *curcfg;

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
    print_cli_res_list(vty, curcfg->node_list);

    vty_out(vty, "%sTotal number of links: %d %s", VTY_NEWLINE, curcfg->link_list->count, VTY_NEWLINE);
    print_cli_res_list(vty, curcfg->link_list); 
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

  es_pool.es[index].ip.s_addr = inet_addr(argv[0]);
  es_pool.es[index].router_id.s_addr = inet_addr(argv[1]);
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
