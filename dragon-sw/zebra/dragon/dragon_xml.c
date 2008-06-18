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
#include "ast_master/dragon_app.h"
#include "ast_master/ast_master_ext.h"
#include "ast_master/local_id_cfg.h"
#include "linklist.h"
#include "memory.h"
#include "buffer.h"
#include "dragon/dragond.h"

#define DRAGON_XML_RESULT 	"/usr/local/dragon_ret.xml"
#define DRAGON_XML_RECV 	"/usr/local/dragon_recv.xml"
#define LSP_NAME_LEN 		13
#define XML_FILE_RECV_BUF	250
#define TIMEOUT_SECS		3

extern int dragon_set_local_id(struct cmd_element*, struct vty*, int, char**);
extern int dragon_set_local_id_group(struct cmd_element*, struct vty*, int, char**);
extern int dragon_delete_local_id(struct cmd_element*, struct vty*, int, char**);
extern int dragon_set_local_id_group_refresh(struct cmd_element*, struct vty*, int, char**);
extern int dragon_net_lsp_uni(struct cmd_element*, struct vty*, int, char**);
extern int dragon_delete_lsp(struct cmd_element*, struct vty*, int, char**);
extern int dragon_set_lsp_ip(struct cmd_element*, struct vty*, int, char**);
extern int dragon_set_lsp_sw(struct cmd_element*, struct vty*, int, char**);
extern int dragon_set_lsp_dir(struct cmd_element*, struct vty*, int, char**);
extern int dragon_set_lsp_vtag_default(struct cmd_element*, struct vty*, int, char**);
extern int dragon_set_lsp_vtag(struct cmd_element*, struct vty*, int, char**);
extern int dragon_commit_lsp_sender(struct cmd_element*, struct vty*, int, char**);
extern int dragon_set_lsp_uni(struct cmd_element*, struct vty*, int, char**);

struct dragon_callback {
  char *ast_id;
  char lsp_name[LSP_NAME_LEN+1];
  struct res_def *link_stype;
  struct res_def *node_stype;
  char link_name[NODENAME_MAXLEN+1];
  char link_agent[NODENAME_MAXLEN+1];
};
static struct adtlist pending_list;

extern struct thread_master *master;
extern char *status_type_details[];
extern char *action_type_details[];
extern char* link_stype_name[];
extern char* node_stype_name[];
extern char *local_id_name[];
extern char *id_action_name[];
extern list registered_local_ids;
static struct vty* fake_vty = NULL;
static char* argv[7];
static int FIN_accept(struct thread *);

extern void set_lsp_default_para(struct lsp*);
extern void process_xml_query(FILE *, char*);
extern struct local_id * search_local_id(u_int16_t, u_int16_t);
extern void print_id_response(char *, int);

static int dragon_link_provision(struct resource*);
static int dragon_link_release(struct resource*);
extern struct string_value_conversion conv_rsvp_event;

/* extern to other variables to use the minion-related lib functions */
extern int glob_minion;
extern char *glob_minion_ret_xml;
extern char *glob_minion_recv_xml;
extern struct res_mod dragon_link_mod;
int dragon_link_minion_proc(enum action_type, struct resource *);

static struct vty*
generate_fake_vty()
{
  struct vty* vty;

  vty = vty_new();
  vty->type = VTY_FILE;

  return vty;
}

void
dragon_upcall_callback(int msg_type, struct lsp* lsp)
{
  struct adtlistnode *prev, *curr;
  struct dragon_callback *data;
  int status = status_unknown;
  FILE *fp;
  int sock;

  if (pending_list.count == 0 || !lsp)
    return;

  prev = NULL;
  for (curr = pending_list.head;
	curr;
	prev = curr, curr = curr->next) {
    data = (struct dragon_callback*) curr->data;
    if (strcmp(data->lsp_name, (lsp->common.SessionAttribute_Para)->sessionName) == 0) 
      break;
  }

  if (!curr) 
    return;
  /* re-organize the list */
  if (prev == NULL) 
    pending_list.head = curr->next;
  else 
    prev->next = curr->next;
  pending_list.count--;
  if (!pending_list.count)
    pending_list.tail = NULL;
  else if (curr == pending_list.tail)
    pending_list.tail = prev;
  free(curr);
    
  free_application_cfg(glob_app_cfg);
  glob_app_cfg = NULL;

  zlog_info("dragon_upcall_callback(): START");
  zlog_info("** msg_type: %d; lsp_name: %s", msg_type, data->lsp_name);
  unlink(DRAGON_XML_RESULT);

  glob_app_cfg = retrieve_app_cfg(data->ast_id, LINK_AGENT);
  if (!glob_app_cfg) {
    free(data->ast_id);
    free(data);
    return;
  }
 
  if (glob_app_cfg->action == release_resp) {
    free_application_cfg(glob_app_cfg);
    glob_app_cfg = NULL;
    zlog_err("final.xml is in RELEASE_RESP, no need to send anything to ast_master");
    free(data->ast_id);
    free(data);
    return;
  }

  fp = fopen(DRAGON_XML_RESULT, "w");
  fprintf(fp, "<topology ast_id=\"%s\" action=\"SETUP_RESP\">\n", glob_app_cfg->ast_id);

  /* if Path, Resv, ResvConf, return AST_SUCCESS to user
   * if PathErr, ResvErr, return AST_FAILURE to user
   */
  switch (msg_type) {
    case Path:
    case Resv:
    case ResvConf:
      status = ast_success;
      break; 
    case PathErr:
    case ResvErr:
      status = ast_failure;
      break;
  }

  fprintf(fp, "<status>%s</status>\n", 
		status == ast_success? "AST_SUCCESS":"AST_FAILURE");
  fprintf(fp, "<resource res_type=\"link\" subtype=\"%s\" name=\"%s\">\n", 
		data->link_stype->name, data->link_name);
  fprintf(fp, "\t<status>%s</status>\n", 
		status == ast_success? "AST_SUCCESS":"AST_FAILURE");
  fprintf(fp, "\t<res_details>\n");
  fprintf(fp, "\t<link_status>%s</link_status>\n",
		status == ast_success? "IN-SERVICE":"ERROR"); 
  fprintf(fp, "\t<lsp_name>%s</lsp_name>\n", data->lsp_name);
  fprintf(fp, "\t<te_params>\n");
  fprintf(fp, "\t\t<vtag>%d</vtag>\n", lsp->dragon.lspVtag);
  fprintf(fp, "\t</te_params>\n");
  fprintf(fp, "</res_details>\n</resource>\n</topology>\n");
  fflush(fp);
  fclose(fp);

  zlog_info("sending SETUP_RESP to ast_master at %s", inet_ntoa(glob_app_cfg->ast_ip));

  sock = send_file_to_agent(inet_ntoa(glob_app_cfg->ast_ip), MASTER_PORT, DRAGON_XML_RESULT);

  glob_app_cfg = NULL;
  
  /* FIONA */
  close(sock); 
  free(data->ast_id);
  free(data);
  zlog_info("dragon_upcall_callback(): DONE");
}

int 
dragon_process_id_cfg()
{
  struct adtlistnode *cur;
  struct id_cfg_res *res;
  listnode node, node1;
  struct local_id_cfg *id;
  int i, argc, ret_val = 1;
  char path[105];

  /* for ID_MODIFY */
  u_int16_t type;
  u_int16_t tag;
  u_int16_t *iter_tag;
  struct local_id *lid = NULL;
  listnode node_inner;
  int found;
  u_int16_t * ptag = NULL;

  zlog_info("dragon_process_id_cfg: start ...");

  if (!glob_app_cfg->node_list) {
    zlog_warn("dragon_process_id_cfg: nothing to do; no resource");
    return 1;
  }

  res = (struct id_cfg_res*) glob_app_cfg->node_list->head->data;

  /* loop through all the local_id_cfg in it */
  if (glob_app_cfg->xml_type == ID_XML && !res->cfg_list) {
    zlog_warn("dragon_process_id_cfg: no local_id to configure under resource %s", res->name);
    res->status = ast_success;
    return 1;
  }

  switch (glob_app_cfg->xml_type) {
    case ID_XML:

    for (cur = res->cfg_list->head;
	  cur;
	  cur = cur->next) {
      id = (struct local_id_cfg*) cur->data;
  
      zlog_info("working on id: %d, type: %s, action: %s",
	  id->id, local_id_name[id->type], id_action_name[id->action]);
      id->status = ast_success;
  
      /* depending on the types of the action, process the task accordingly 
       */
      switch (id->action) {
  
	case ID_CREATE:
  
	  if (id->type == 3) {
	    sprintf(argv[0], "%d", id->id);
	    strcpy(argv[1], local_id_name[id->type]);
	    if (dragon_set_local_id(NULL, fake_vty, argc, (char**)&argv) != CMD_SUCCESS) {
	      id->status = ast_failure;
	      buffer_putc(fake_vty->obuf, '\0');
	      id->msg = buffer_getstr(fake_vty->obuf);
	      buffer_reset(fake_vty->obuf);
	      ret_val = 0;
	      break;
	    }
	   } else { 
  
	    argc = 4;
	    strcpy(argv[0], local_id_name[id->type]);
	    sprintf(argv[1], "%d", id->id);
	    strcpy(argv[2], "add");
  
	    for (i = 0; i < id->num_mem; i++) {
	      sprintf(argv[3], "%d", id->mems[i]);
	      if (dragon_set_local_id_group(NULL, fake_vty, argc, (char**)&argv) != CMD_SUCCESS) {
		id->status = ast_failure;
		buffer_putc(fake_vty->obuf, '\0');
		id->msg = buffer_getstr(fake_vty->obuf);
		buffer_reset(fake_vty->obuf);
		ret_val = 0;
		break;
	      }
	    }
	  }
	    
	  break;
  
	case ID_DELETE:
  
	  argc = 2;
	  strcpy(argv[0], local_id_name[id->type]);
	  sprintf(argv[1], "%d", id->id);
  
	  if (dragon_delete_local_id(NULL, fake_vty, argc, (char**)&argv) != CMD_SUCCESS) {
	    id->status = ast_failure;
	    buffer_putc(fake_vty->obuf, '\0');
	    id->msg = buffer_getstr(fake_vty->obuf);
	    buffer_reset(fake_vty->obuf);
	    ret_val = 0;
	  }
  
	  break;
  
	case ID_MODIFY:
  
	  if (id->type == 3) {
  
	    ret_val = 0;
	    break;
	  }
  
	  type = id->type == 1? LOCAL_ID_TYPE_GROUP:LOCAL_ID_TYPE_TAGGED_GROUP;
	  tag = id->id;
  
	  /* first make sure that this local_id exists in that type */
	  lid = search_local_id(tag, type);
  
	  if (!lid) {
	    id->status = ast_failure;
	    id->msg = strdup("requested local_id doesn't exist");
	    ret_val = 0;
	    break;
	  }
  
	  argc = 4;
	  strcpy(argv[0], local_id_name[id->type]);
	  sprintf(argv[1], "%d", id->id);
	  strcpy(argv[2], "add");
  
	  /* loop through the new list
	   * compare with the old, if not there, add */
	  for (i = 0; i < id->num_mem; i++) {
	    found = 0;
	    LIST_LOOP(lid->group, iter_tag, node_inner) {
	      if (*iter_tag == id->mems[i]) {
		found = 1;
		break;
	      }
	    }
  
	    if (!found) {
	      /* this member doesn't exist in the list yet, so add */
	      sprintf(argv[3], "%d", id->mems[i]);
	      if (dragon_set_local_id_group(NULL, fake_vty, argc, (char**)&argv) != CMD_SUCCESS) {
		id->status = ast_failure;
		buffer_putc(fake_vty->obuf, '\0');
		id->msg = buffer_getstr(fake_vty->obuf);
		buffer_reset(fake_vty->obuf);
		ret_val = 0;
	      }
	      found = 0;
	    }
	  }
  
	  strcpy(argv[2], "delete");
  
	  /* loop through the old list
	   * compare with the new, if not there, delete */
	  LIST_LOOP(lid->group, iter_tag, node_inner) {
	    for (i = 0, found = 0; i < id->num_mem; i++) {
	      if (*iter_tag == id->mems[i]) {
		found = 1;
		break;
	      }
	    }
	    
	    if (!found) {
	      /* this member doesn't exist in the new list, so need to delete */
	      sprintf(argv[3], "%d", *iter_tag);
	      if (dragon_set_local_id_group(NULL, fake_vty, argc, (char**)&argv) != CMD_SUCCESS) {
		id->status = ast_failure;
		buffer_putc(fake_vty->obuf, '\0');
		id->msg = buffer_getstr(fake_vty->obuf);
		buffer_reset(fake_vty->obuf);
		ret_val = 0;
	      }
	      found = 0;
	    }
	  }
  
	  /* after updating the local_id, need to do refresh 
	   */
	   argc = 2;
	  if (dragon_set_local_id_group_refresh(NULL, fake_vty, argc, (char**)&argv) != CMD_SUCCESS) {
	    id->status = ast_failure;
	    buffer_putc(fake_vty->obuf, '\0'); 
	    id->msg = buffer_getstr(fake_vty->obuf); 
	    buffer_reset(fake_vty->obuf);
	    ret_val = 0;
	  }
  
	  break;
      }
    }
  
    if (ret_val) {
      res->status = ast_success;
      glob_app_cfg->status = ast_success;
    } else {
      res->status = ast_failure;
      glob_app_cfg->status = ast_failure;
    }
  
    break;
  
    case ID_QUERY_XML:

    glob_app_cfg->status = ast_success;
    res->status = ast_success;

    if (registered_local_ids->count == 0)
      break;

    LIST_LOOP(registered_local_ids, lid, node) {
      id = (struct local_id_cfg*) malloc (sizeof(struct local_id_cfg));
      memset(id, 0, sizeof(struct local_id_cfg));
      
      id->id = lid->value;
      if (lid->type == LOCAL_ID_TYPE_GROUP)
	id->type = 1;
      else if (lid->type == LOCAL_ID_TYPE_TAGGED_GROUP)
	id->type = 2;
      else if (lid->type == LOCAL_ID_TYPE_PORT)
	id->type = 3;
      else {
	zlog_err("type error");
	free(id);
	continue;
      }

      if (lid->type == LOCAL_ID_TYPE_GROUP || 
	  lid->type == LOCAL_ID_TYPE_TAGGED_GROUP) {

	id->num_mem = lid->group->count;
	id->mems = (int*) malloc(sizeof(int)*id->num_mem);
	i = 0;
	LIST_LOOP(lid->group, ptag, node1) {
	  id->mems[i] = *ptag;
	  i++;
	}   
      }

      if (!res->cfg_list) {
	res->cfg_list = (struct adtlist*)malloc(sizeof(struct adtlist));
	memset(res->cfg_list, 0, sizeof(struct adtlist));
      }
      adtlist_add(res->cfg_list, id);
    }
    
    break;
  }

  if (mkdir(LINK_AGENT_DIR, 0755) == -1 && errno != EEXIST) {
    zlog_err("Can't create diectory %s", LINK_AGENT_DIR);
    return 0;
  }

  sprintf(path, "%s/%s", LINK_AGENT_DIR, glob_app_cfg->ast_id);
  if (mkdir(path, 0755) == -1) {
    if (errno == EEXIST) 
      zlog_warn("<ast_id> %s exists already", glob_app_cfg->ast_id);
    else {
      zlog_err("Can't create the directory: %s; error = %d(%s)",
		path, errno, strerror(errno));
      return 0;
    }
  }

  sprintf(path, "%s/%s/org.xml", 
	  LINK_AGENT_DIR, glob_app_cfg->ast_id);
  if (rename(DRAGON_XML_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	DRAGON_XML_RECV, path, errno, strerror(errno));

  sprintf(path, "%s/%s/ret.xml",
	LINK_AGENT_DIR, glob_app_cfg->ast_id);

  symlink(path, DRAGON_XML_RESULT);
  print_id_response(path, LINK_AGENT);

  return ret_val;
}

int
dragon_process_query_req()
{
  char path[105];
  char directory[80];
  struct resource *mynode;
  FILE* fp;

  glob_app_cfg->action = query_resp;
  zlog_info("Processing ast_id: %s, QUERY_REQ", glob_app_cfg->ast_id);

  strcpy(directory, LINK_AGENT_DIR);
  if (mkdir(directory, 0755) == -1 && errno != EEXIST) {
    zlog_err("Can't create directory %s", LINK_AGENT_DIR);
    return 0;
  }

  sprintf(directory, "%s/%s", LINK_AGENT_DIR, glob_app_cfg->ast_id);
  if (mkdir(directory, 0755) == -1) {
    zlog_err("Can't create directory %s", directory);
    return 0;
  }

  sprintf(path, "%s/%s/query_original.xml", 
	LINK_AGENT_DIR, glob_app_cfg->ast_id);

  if (rename(DRAGON_XML_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	   DRAGON_XML_RECV, path, errno, strerror(errno));

  sprintf(path, "%s/%s/query_response.xml", 
	LINK_AGENT_DIR, glob_app_cfg->ast_id);

  glob_app_cfg->action = query_resp;
  glob_app_cfg->status = ast_success;

  fp = fopen(path, "w+");
  if (fp == NULL) {
    zlog_err("Can't open file %s: error = %d(%s)",
	     path, errno, strerror(errno));
    return 0;
  }

  /* prepare ret_buf before sending into process_xml_query
   */
  fprintf(fp, "<topology ast_id=\"%s\", action=\"%s\">\n", 
		glob_app_cfg->ast_id, action_type_details[glob_app_cfg->action]);
  fprintf(fp, "<status>AST_SUCCESS</status>\n");
  mynode = (struct resource*)(glob_app_cfg->node_list->head->data);
  mynode->status = ast_success;
  print_res(fp, mynode, LINK_AGENT);
  process_xml_query(fp, mynode->name);
  fprintf(fp, "</topology>");
  fflush(fp);
  fclose(fp);

  symlink(path, DRAGON_XML_RESULT);

  return 1;
}

static void
xml_module_init()
{
  int i;
  char* assign;

  fake_vty = generate_fake_vty();
  assign = malloc(7*(REG_TXT_FIELD_LEN+1)*sizeof(char));
  for (i = 0; i < 7; i++) 
    argv[i] = assign+(i*REG_TXT_FIELD_LEN+1);
  free_application_cfg(glob_app_cfg);
  glob_app_cfg = NULL;
  memset(&app_list, 0, sizeof(struct adtlist));
  memset(&pending_list, 0, sizeof(struct adtlist));

  glob_minion = LINK_AGENT;
  glob_minion_ret_xml = DRAGON_XML_RESULT;
  glob_minion_recv_xml = DRAGON_XML_RECV;
  dragon_link_mod.minion_proc_func = dragon_link_minion_proc;
}

static void
xml_module_reset()
{
  buffer_reset(fake_vty->obuf);
  free_application_cfg(glob_app_cfg);
  glob_app_cfg = NULL;
  unlink(DRAGON_XML_RESULT);
  unlink(DRAGON_XML_RECV);
}
  
static void
generate_lsp_name(char* bandwidth, char* name)
{
  int i, s;

  if (strcasecmp(bandwidth, "gige") == 0 ||
	strcasecmp(bandwidth, "gige_f") == 0) {
    strcpy(name, "GIGE-");
    s = 5;
  } else {
    strcpy(name, "AST-");
    s = 4;
  }
  
  for (i = s; i < LSP_NAME_LEN; i++) {
    name[i] = (random() % 10) + 48;
  }
  
  name[LSP_NAME_LEN] = '\0';
}

static struct lsp*
dragon_build_lsp(struct resource *res)
{
  struct lsp* lsp;
  char lsp_name[LSP_NAME_LEN+1];
  int argc;
  struct dragon_link *link;
  struct dragon_endpoint *src_ep, *dest_ep;
  struct dragon_node_pc *src_node, *dest_node;

  link = (struct dragon_link *)res->res;

  bzero(link->lsp_name, LSP_NAME_LEN+1);
  res->status = ast_failure;
  generate_lsp_name(link->bandwidth, lsp_name);
  
  /* mirror what dragon_edit_lsp_cmd does
   */
  lsp = XMALLOC(MTYPE_OSPF_DRAGON, sizeof(struct lsp));
  bzero(lsp, sizeof(struct lsp));
  set_lsp_default_para(lsp);
  lsp->status = LSP_EDIT;
  strcpy((lsp->common.SessionAttribute_Para)->sessionName, lsp_name);
  (lsp->common.SessionAttribute_Para)->nameLength = strlen(lsp_name);
  fake_vty->index = lsp;
  listnode_add(dmaster.dragon_lsp_table, lsp);

  if (IS_VTAG_ANY(link))
    res->flags |= FLAG_UNFIXED;

  src_ep = link->src;
  dest_ep = link->dest;
  src_node = (struct dragon_node_pc*)src_ep->node->res;
  dest_node = (struct dragon_node_pc*)dest_ep->node->res;

  /* mirrow what dragon_set_lsp_uni does
   */
  if (link->stype == uni) {

    argc = 3;
    strcpy(argv[0], "client");
    if (IS_RES_UNFIXED(res) || 
	(src_node->tunnel[0] !='\0' &&
	 dest_node->tunnel[0] != '\0')) {
      strcpy(argv[1], src_node->tunnel);
      strcpy(argv[2], dest_node->tunnel);
    } 
    if (argv[1][0] == '\0')
      strcpy(argv[1], "implicit");
    if (argv[2][0] == '\0')
      strcpy(argv[2], "implicit");

    zlog_info("dragon_set_lsp_uni: %s | %s | %s", argv[0], argv[1], argv[2]);
    if (dragon_set_lsp_uni (NULL, fake_vty, argc, (char**)&argv) != CMD_SUCCESS) {
      argc = 1;
      strcpy(argv[0], lsp_name);
      dragon_delete_lsp(NULL, fake_vty, argc, (char**)&argv);
      return NULL;
    }
  }
 
  /* mirror what dragon_set_lsp_ip does
   */
  argc = 6;

  switch (link->stype) {
    case uni:
    case non_uni:

      strcpy(argv[0], inet_ntoa(src_node->router_id));
      strcpy(argv[3], inet_ntoa(dest_node->router_id));
      break;

    case vlsr_vlsr:
 
      strcpy(argv[0], inet_ntoa(src_node->router_id));
      strcpy(argv[3], inet_ntoa(dest_node->router_id));
      break;

  }

  if (IS_RES_UNFIXED(res)) {
    strcpy(argv[1], "tagged-group");
    strcpy(argv[2], "any");
    strcpy(argv[4], "tagged-group");
    strcpy(argv[5], "any");
  } else {
    strcpy(argv[1], src_ep->local_id_type);
    sprintf(argv[2], "%d", src_ep->local_id);
    if (dest_ep->local_id_type[0] == 'l')
      strcpy(argv[4], "tunnel-id");
    else 
      strcpy(argv[4], dest_ep->local_id_type);
    sprintf(argv[5], "%d", dest_ep->local_id);
  }

  zlog_info("dragon_set_lsp_ip: %s | %s | %s | %s | %s | %s", argv[0], argv[1], argv[2], argv[3], argv[4], argv[5]);
  if (dragon_set_lsp_ip (NULL, fake_vty, argc, (char**)&argv) != CMD_SUCCESS) {
    
    argc = 1;
    strcpy(argv[0], lsp_name);
    dragon_delete_lsp(NULL, fake_vty, argc, (char**)&argv);
    return NULL;
  }

  /* mirror what dragon_set_lsp_sw_cmd does
   */
  argc = 4;
  strcpy(argv[0], link->bandwidth);
  strcpy(argv[1], link->swcap);
  strcpy(argv[2], link->encoding);
  strcpy(argv[3], link->gpid);  
  zlog_info("dragon_set_lsp_sw: %s | %s | %s | %s", argv[0], argv[1], argv[2], argv[3]);

  if (dragon_set_lsp_sw (NULL, fake_vty, argc, (char**)&argv) != CMD_SUCCESS) {

    argc = 1;
    strcpy(argv[0], lsp_name);
    dragon_delete_lsp(NULL, fake_vty, argc, (char**)&argv);
    return NULL;
  }

  /* mirror what dragon_set_lsp_dir does
   */
  argc = 2;
  strcpy(argv[0], "bi");
  strcpy(argv[1], "161252");
  zlog_info("dragon_set_lsp_dir: %s | %s", argv[0], argv[1]);
  
  if (dragon_set_lsp_dir(NULL, fake_vty, argc, (char**)&argv) != CMD_SUCCESS) {
    
    argc = 1;
    strcpy(argv[0], lsp_name);
    dragon_delete_lsp(NULL, fake_vty, argc, (char**)&argv);
    return NULL;
  }

  if (link->vtag[0] != '\0') {
    argc = 1;
    strcpy(argv[0], link->vtag);

    if (strcmp(argv[0], "any") == 0) {
      zlog_info("dragon_set_lsp_vtag_default");
      dragon_set_lsp_vtag_default(NULL, fake_vty, 0, (char**)&argv);
    } else {
      zlog_info("dragon_set_lsp_vtag: %s", argv[0]);
      if (dragon_set_lsp_vtag(NULL, fake_vty, argc, (char**)&argv) != CMD_SUCCESS) {
 	argc = 1;
	strcpy(argv[0], lsp_name);
        dragon_delete_lsp(NULL, fake_vty, argc, (char**)&argv);
        return NULL;
      }
    }
  }

  /* mirror what dragon_commit_lsp_sender does
   */
  argc = 1;
  strcpy(argv[0], lsp_name);
  if (dragon_commit_lsp_sender(NULL, fake_vty, argc, (char**)&argv) != CMD_SUCCESS) {
    strcpy(argv[0], lsp_name);
    argc = 1;
    dragon_delete_lsp(NULL, fake_vty, argc, (char**)&argv); 
    return NULL;
  }
 
  strcpy(link->lsp_name, lsp_name);
  res->status = ast_success;

  return lsp;
}

static int 
dragon_link_provision(struct resource* link_res) 
{
  struct lsp *lsp = NULL;
  struct dragon_link *link;
  int ret_value = 0;
 
  /* it would be nice to dump this lsp into dmaster.dragon_lsp_table
   * with name as "XML-<8char>"
   * so that the show lsp in CLI can also show the link provisioned by 
   * AST
   */
  if (!link_res)
    return 1;
  if (!link_res->res)
    return 1;

  link = (struct dragon_link *)link_res->res;
 
  buffer_reset(fake_vty->obuf);
  lsp = dragon_build_lsp(link_res); 
  if (!lsp) {
    link->l_status = error; 
    zlog_err("ERROR: lsp is not set between %s and %s\n",
		link->src->node->name, link->dest->node->name);
    link_res->status = ast_failure;

    buffer_putc(fake_vty->obuf, '\0');
    link_res->agent_message = buffer_getstr(fake_vty->obuf);
    ret_value++;
  } else {
    struct dragon_callback *data = malloc(sizeof(struct dragon_callback));
    bzero(data, sizeof(struct dragon_callback));
    data->ast_id = strdup(glob_app_cfg->ast_id);
    strncpy(data->lsp_name, link->lsp_name, LSP_NAME_LEN);
    data->link_stype = link_res->subtype;

    zlog_info("lsp (%s) has been set between %s and %s\n",
		link->lsp_name, link->src->node->name,
		link->dest->node->name);
    strncpy(data->link_name, link_res->name, NODENAME_MAXLEN);
    adtlist_add(&pending_list, data);
    link_res->status = ast_pending;
    link->l_status = commit;
    link_res->status = ast_pending;
  } 

  return ret_value;
}

static int
dragon_link_release(struct resource *link_res)
{
  struct dragon_link *link;
  int argc;
 
  /* Now, loop through the task/link list and provision each link
   * it would be nice to dump this lsp into dmaster.dragon_lsp_table
   * with name as "XML-<8char>"
   * so that the show lsp in CLI can also show the link provisioned by
   * AST
   */
  if (!link_res->res)
    return 0;
  
  link = (struct dragon_link*)link_res->res;

  /* mirror what dragon_set_lsp_ip does 
   */
  if (link->lsp_name[0] != '\0') { 
    argc = 1;
    strcpy(argv[0], link->lsp_name);
    return (dragon_delete_lsp (NULL, fake_vty, argc, (char**)&argv) != CMD_SUCCESS);
  }

  return 0;
}

static void
handleAlarm()
{
  /* don't need to do anything special for SIGALARM
   */
}

int 
xml_accept(struct thread *thread)
{
  int xml_sock, accept_sock;
  struct sockaddr_in clientAddr;
  int clientLen;
  struct sigaction myAction;
  static struct stat file_stat;

  zlog_info("xml_accept(): START");
  accept_sock = THREAD_FD (thread);
  xml_module_reset();
  clientLen = sizeof(clientAddr);
  xml_sock = accept (accept_sock, (struct sockaddr*)&clientAddr, (socklen_t*)&clientLen);

  if (xml_sock < 0) {
    zlog_err("xml_accept: accept failed() for xmlServSock");
  } else {
    zlog_info("xml_accept: Handling client %s ...", inet_ntoa(clientAddr.sin_addr));
    unlink(DRAGON_XML_RECV);
    unlink(DRAGON_XML_RESULT);

#ifdef FIONA
    if (fcntl(xml_sock, O_NONBLOCK) < 0) 
      zlog_warn("xml_accept: Unable to set the xml socket to non-blocking status");
#endif

    zlog_info("xml_accept: accept succeed() got it");

    myAction.sa_handler = handleAlarm;
    if (sigfillset(&myAction.sa_mask) < 0) {
      zlog_err("xml_accept: sigfillset() failed");
      return 0;
    }
    myAction.sa_flags = 0;

    if (sigaction(SIGALRM, &myAction, 0) < 0) {
      zlog_err("xml_accept: sigaction() failed");
      return 0;
    }
    
    if (!recv_file(xml_sock, DRAGON_XML_RECV, XML_FILE_RECV_BUF, TIMEOUT_SECS, LINK_AGENT)) {

      minion_process_xml(clientAddr.sin_addr);

      if (!glob_app_cfg) {
	print_error_response(DRAGON_XML_RESULT);
	send_file_over_sock(xml_sock, DRAGON_XML_RESULT);
      } else {
        if (glob_app_cfg->action == setup_resp && 
		glob_app_cfg->status == ast_pending) {
	  thread_add_read(master, FIN_accept, NULL, xml_sock); 
	  thread_add_read(master, xml_accept, NULL, accept_sock); 
	  return 1; 
	} 

	if (glob_app_cfg->action == setup_resp) { 
	  close(xml_sock); 
	  xml_sock = -1; 
	} 

	if (stat(DRAGON_XML_RESULT, &file_stat) == -1) 
	  print_error_response(DRAGON_XML_RESULT);

	zlog_info("Sending %s to ast_master at %s", 
		action_type_details[glob_app_cfg->action], 
		inet_ntoa(glob_app_cfg->ast_ip));

        if (send_file_over_sock(xml_sock, DRAGON_XML_RESULT)) 
	  xml_sock = send_file_to_agent(inet_ntoa(glob_app_cfg->ast_ip), MASTER_PORT, DRAGON_XML_RESULT); 
      }
      if (xml_sock != -1) 
        close(xml_sock);
    }
  }

  zlog_info("xml_accept(): DONE");
  thread_add_read(master, xml_accept, NULL, accept_sock);
  return 1;
}

int
xml_serv_sock (const char *hostname, unsigned short port, char *path)
{
  int ret;
  struct addrinfo req;
  struct addrinfo *ainfo;
  struct addrinfo *ainfo_save;
  int sock;
  char port_str[BUFSIZ];

  struct thread *xml_thread;

  xml_module_init();

  memset (&req, 0, sizeof (struct addrinfo));
  req.ai_flags = AI_PASSIVE;
  req.ai_family = AF_INET;
  req.ai_socktype = SOCK_STREAM;
  sprintf (port_str, "%d", port);
  port_str[sizeof (port_str) - 1] = '\0';

  ret = getaddrinfo (hostname, port_str, &req, &ainfo);

  if (ret != 0)
    {
      fprintf (stderr, "getaddrinfo failed: %s\n", gai_strerror (ret));
      exit (1);
    }

  ainfo_save = ainfo;

  do
    {
      if (ainfo->ai_family != AF_INET)
	continue;

      sock = socket (ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
      if (sock < 0)
	continue;

      //sockopt_reuseaddr (sock);
      //sockopt_reuseport (sock);

      ret = bind (sock, ainfo->ai_addr, ainfo->ai_addrlen);
      if (ret < 0)
	{
	  close (sock); /* Avoid sd leak. */
	  continue;
	}

      ret = listen (sock, 10);
      if (ret < 0)
	{
	  close (sock); /* Avoid sd leak. */
	  continue;
	}

       xml_thread = thread_add_read (master, xml_accept, NULL, sock);
    }
  while ((ainfo = ainfo->ai_next) != NULL);

  freeaddrinfo (ainfo_save);

  return sock;
}

/* FIN_accept is used to handle any connection close on the other side */
static int
FIN_accept(struct thread *thread)
{
  int servSock, total;
  static char buffer[RCVBUFSIZE];
  static char ret_buf[SENDBUFSIZE];
  int bytesRcvd, ret_value = 1;
  FILE* ret_file = NULL;

  servSock = THREAD_FD(thread);

  zlog_info("FIN_accept(): START");

  total = 0;
  memset(ret_buf, 0, SENDBUFSIZE);
  while ((bytesRcvd = recv(servSock, buffer, RCVBUFSIZE-1, 0)) > 0) {
    if (!total) {
      ret_file = fopen("/usr/local/dragon_void.xml", "w");
      if (!ret_file) {
        ret_value = 0;
        continue;  
      }
    }
    total+=bytesRcvd;
    buffer[bytesRcvd] = '\0';
    sprintf(ret_buf+strlen(ret_buf), buffer);
    fprintf(ret_file, buffer);
  }

  if (total == 0) {
    ret_value = 0;
  } else {
    fflush(ret_file); 
    fclose(ret_file);

    zlog_err("SHOULD NOT RECEIVED ANYTHING HERE: look at /usr/local/dragon_void.xml");
  }

  close(servSock);
  zlog_info("FIN_accept(): END");
  return 1;
}

int
dragon_link_minion_proc(enum action_type action,
                        struct resource *res)
{
  if (!res->res)
    return 1;

  if (action == setup_req) 
    return (dragon_link_provision(res));
  else if (action == release_req) 
    return (dragon_link_release(res));

  return 0;
}
