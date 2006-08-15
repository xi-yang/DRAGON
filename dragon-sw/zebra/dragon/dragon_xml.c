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
#include "ast_master/ast_master.h"
#include "ast_master/local_id_cfg.h"
#include "linklist.h"
#include "memory.h"
#include "buffer.h"
#include "dragon/dragond.h"

#define DRAGON_XML_RESULT 	"/usr/local/dragon_ret.xml"
#define DRAGON_XML_RECV 	"/usr/local/dragon_recv.xml"
#define LINK_AGENT_DIR		"/usr/local/link_agent"
#define LSP_NAME_LEN 		13
#define XML_FILE_RECV_BUF	250
#define TIMEOUT_SECS		3

struct dragon_callback {
  char *ast_id;
  char lsp_name[LSP_NAME_LEN+1];
  enum link_stype stype;
  enum node_stype node_stype;
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
static struct vty* fake_vty = NULL;
static char* argv[7];

extern void set_lsp_default_para(struct lsp*);
extern void process_xml_query(FILE *, char*);
extern struct local_id * search_local_id(u_int16_t, u_int16_t);

static int dragon_link_provision();
static int dragon_link_release();
extern struct string_value_conversion conv_rsvp_event;

static struct vty*
generate_fake_vty()
{
  struct vty* vty;

  vty = vty_new();
  vty->type = VTY_FILE;

  return vty;
}

void
dragon_upcall_callback(int msg_type, char *lsp_name)
{
  struct adtlistnode *prev, *curr, *next;
  struct dragon_callback *data;
  char path[105];
  struct stat file_stat;
  int status = AST_UNKNOWN;
  int xml_sock, fd;
  struct sockaddr_in astAddr;
  FILE *fp;

  if (pending_list.count == 0)
    return;

  prev = NULL;
  next = NULL;
  for (curr = pending_list.head;
	curr;
	prev = curr, curr = curr->next) {
    data = (struct dragon_callback*) curr->data;
    if (strcmp(data->lsp_name, lsp_name) == 0) 
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

  free_application_cfg(&glob_app_cfg);

  zlog_info("********* CALLBACK FOR LINK_AGNET msg_type: %d; lsp_name: %s", msg_type, lsp_name);
  unlink(DRAGON_XML_RESULT);

  /* now, read the file for ast_id */
  sprintf(path, "%s/%s/final.xml", 
          LINK_AGENT_DIR, data->ast_id);
  if (stat(path, &file_stat) == -1) {
    zlog_err("Can't locate the ast_id final file");
    free(data->ast_id);
    free(data);
    return;
  }
 
  /* check the status, if anything other than SETUP_RESP,
   * return
   */
  if (agent_final_parser(path) != 1) {
    zlog_err("Can't parse the final file successfully");
    free(data->ast_id);
    free(data);
    return;
  }
    
  if (glob_app_cfg.action != SETUP_RESP) {
    free_application_cfg(&glob_app_cfg);
    zlog_err("final.xml is not SETUP_RESP, no need to send anything to ast_master");
    free(data->ast_id);
    free(data);
    return;
  }

  fp = fopen(DRAGON_XML_RESULT, "w");
  fprintf(fp, "<topology ast_id=\"%s\" action=\"SETUP_RESP\">\n", glob_app_cfg.ast_id);

  /* if Path, Resv, ResvConf, return AST_SUCCESS to user
   * if PathErr, ResvErr, return AST_FAILURE to user
   */
  switch (msg_type) {
    case Path:
    case Resv:
    case ResvConf:
      status = AST_SUCCESS;
      break; 
    case PathErr:
    case ResvErr:
      status = AST_FAILURE;
      break;
  }
  fprintf(fp, "<status>%s</status>\n", 
		status == AST_SUCCESS? "AST_SUCCESS":"AST_FAILURE");
  fprintf(fp, "<resource name=\"%s\" type=\"%s\">\n", data->link_agent, node_stype_name[data->node_stype]);
  fprintf(fp, "</resource>\n");
  fprintf(fp, "<resource name=\"%s\" type=\"%s\">\n", data->link_name, link_stype_name[data->stype]);
  fprintf(fp, "\t<status>%s</status>\n", 
		status == AST_SUCCESS? "AST_SUCCESS":"AST_FAILURE");
  fprintf(fp, "\t<lsp_name>%s</lsp_name>\n", data->lsp_name);
  fprintf(fp, "\t<dragon>%s</dragon>\n", data->link_agent);
  fprintf(fp, "</resource>\n</topology>\n");
  fflush(fp);
  fclose(fp);

  zlog_info("sending SETUP_RESP to ast_master at %s", glob_app_cfg.ast_ip);
  if ((xml_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    zlog_err("socket() failed");
  else {
    memset(&astAddr, 0, sizeof(astAddr));
    astAddr.sin_family = AF_INET;
    astAddr.sin_addr.s_addr = inet_addr(glob_app_cfg.ast_ip);
    astAddr.sin_port = htons(MASTER_PORT);

    if (connect(xml_sock, (struct sockaddr*)&astAddr, sizeof(astAddr)) < 0) {
      zlog_err("connect() failed to ast_master");
      close(xml_sock);
      xml_sock = -1;
    }
  }

  if (xml_sock != -1) { 
    fd = open(DRAGON_XML_RESULT, O_RDONLY);
  #ifdef __FreeBSD__
    sendfile(fd, xml_sock, 0, 0, NULL, NULL, 0);
  #else
    if (fstat(fd, &file_stat) == -1) {
      zlog_err("fstat() failed on %s", XML_NEW_FILE);
    }
    sendfile(xml_sock, fd, 0, file_stat.st_size);
  #endif
    close(xml_sock);
    close(fd);
  }

  free_application_cfg(&glob_app_cfg);
  free(data->ast_id);
  free(data);
}

int 
dragon_process_id_cfg()
{
  struct adtlistnode *cur;
  struct id_cfg_res *res;
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

  zlog_info("dragon_process_id_cfg: start ...");

  if (!glob_app_cfg.node_list) {
    zlog_warn("dragon_process_id_cfg: nothing to do; no resource");
    return 1;
  }

  res = (struct id_cfg_res*) glob_app_cfg.node_list->head->data;

  /* loop through all the local_id_cfg in it */
  if (!res->cfg_list) {
    zlog_warn("dragon_process_id_cfg: no local_id to configure under resource %s", res->name);
    res->status = AST_SUCCESS;
    return 1;
  }

  for (cur = res->cfg_list->head;
	cur;
	cur = cur->next) {
    id = (struct local_id_cfg*) cur->data;

    zlog_info("working on id: %d, type: %s, action: %s",
	id->id, local_id_name[id->type], id_action_name[id->action]);
    id->status = AST_SUCCESS;

    /* depending on the types of the action, process the task accordingly 
     */
    switch (id->action) {

      case ID_CREATE:

	if (id->type == 3) {
	  sprintf(argv[0], "%d", id->id);
	  strcpy(argv[1], local_id_name[id->type]);
	  if (dragon_set_local_id(NULL, fake_vty, argc, &argv) != CMD_SUCCESS) {
	    id->status = AST_FAILURE;
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
	    if (dragon_set_local_id_group(NULL, fake_vty, argc, &argv) != CMD_SUCCESS) {
	      id->status = AST_FAILURE;
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

	if (dragon_delete_local_id(NULL, fake_vty, argc, &argv) != CMD_SUCCESS) {
	  id->status = AST_FAILURE;
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
	  id->status = AST_FAILURE;
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
	    if (dragon_set_local_id_group(NULL, fake_vty, argc, &argv) != CMD_SUCCESS) {
              id->status = AST_FAILURE;
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
	    if (dragon_set_local_id_group(NULL, fake_vty, argc, &argv) != CMD_SUCCESS) {
	      id->status = AST_FAILURE;
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
	if (dragon_set_local_id_group_refresh(NULL, fake_vty, argc, &argv) != CMD_SUCCESS) {
	  id->status = AST_FAILURE;
	  buffer_putc(fake_vty->obuf, '\0'); 
	  id->msg = buffer_getstr(fake_vty->obuf); 
	  buffer_reset(fake_vty->obuf);
	  ret_val = 0;
	}

	break;
    }
  }

  if (ret_val) {
    res->status = AST_SUCCESS;
    glob_app_cfg.status = AST_SUCCESS;
  } else {
    res->status = AST_FAILURE;
    glob_app_cfg.status = AST_FAILURE;
  }

  if (mkdir(LINK_AGENT_DIR, 0755) == -1 && errno != EEXIST) {
    zlog_err("Can't create diectory %s", LINK_AGENT_DIR);
    return 0;
  }

  sprintf(path, "%s/%s", LINK_AGENT_DIR, glob_app_cfg.ast_id);
  if (mkdir(path, 0755) == -1) {
    if (errno == EEXIST) 
      zlog_warn("<ast_id> %s exists already", glob_app_cfg.ast_id);
    else {
      zlog_err("Can't create the directory: %s; error = %d(%s)",
		path, errno, strerror(errno));
      return 0;
    }
  }

  sprintf(path, "%s/%s/org.xml", 
	  LINK_AGENT_DIR, glob_app_cfg.ast_id);
  if (rename(DRAGON_XML_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	DRAGON_XML_RECV, path, errno, strerror(errno));

  sprintf(path, "%s/%s/ret.xml",
	LINK_AGENT_DIR, glob_app_cfg.ast_id);

  symlink(path, DRAGON_XML_RESULT);
  print_id_response(path, LINK_AGENT);

  return ret_val;
}

int
dragon_process_setup_req()
{
  char path[105];
  char directory[80];

  glob_app_cfg.org_action = SETUP_REQ;
  glob_app_cfg.action = SETUP_RESP;
  zlog_info("Processing ast_id: %s, SETUP_REQ", glob_app_cfg.ast_id);

  strcpy(directory, LINK_AGENT_DIR);
  if (mkdir(directory, 0755) == -1 && errno != EEXIST) {
    zlog_err("Can't create diectory %s", directory);
    return 0;
  }

  sprintf(directory+strlen(directory), "/%s", glob_app_cfg.ast_id);
  if (mkdir(directory, 0755) == -1) {
    if (errno == EEXIST) {
      zlog_err("<ast_id> %s exists already", glob_app_cfg.ast_id);
      return 0;
    } else {
      zlog_err("Can't create the directory: %s; error = %d(%s)",
		directory, errno, strerror(errno));
      return 0;
    }
  }

  sprintf(path, "%s/setup_original.xml", directory);
  if (rename(DRAGON_XML_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	   DRAGON_XML_RECV, path, errno, strerror(errno));

  sprintf(path, "%s/setup_response.xml", directory);

  if (dragon_link_provision() == 0) 
    glob_app_cfg.status = AST_FAILURE;
  else
    glob_app_cfg.status = AST_PENDING;

  glob_app_cfg.action = SETUP_RESP;
  print_xml_response(path, LINK_AGENT);
  symlink(path, DRAGON_XML_RESULT);

  sprintf(path, "%s/final.xml", directory);
  print_final(path);

  return 1;
}

int
dragon_process_release_req()
{
  char path[105];
  struct stat fs;
  struct application_cfg working_app_cfg;

  glob_app_cfg.org_action = glob_app_cfg.action;
  glob_app_cfg.action = RELEASE_RESP;
  zlog_info("Processing ast_id: %s, RELEASE_REQ", glob_app_cfg.ast_id);

  sprintf(path, "%s/%s/final.xml", 
	  LINK_AGENT_DIR, glob_app_cfg.ast_id);
  if (stat(path, &fs) == -1) {
    sprintf(glob_app_cfg.details, "Can't locate the ast_id final file");
    return 0;
  }

  memcpy(&working_app_cfg, &glob_app_cfg, sizeof(struct application_cfg));
  memset(&glob_app_cfg, 0, sizeof(struct application_cfg));

  if (agent_final_parser(path) != 1) {
    memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "didn't parse the file for ast_id successfully");
    glob_app_cfg.status = AST_FAILURE;
    return 0;
  }

  /* before processing, set all link's status = AST_FAILURE
   */
  if (glob_app_cfg.action == RELEASE_RESP) {
    free_application_cfg(&glob_app_cfg);
    memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "ast_id has received RELEASE_REQ already");
    glob_app_cfg.status = AST_FAILURE;
    return 0;
  }

  if (strcmp(glob_app_cfg.ast_ip, working_app_cfg.ast_ip) != 0)
    zlog_warn("ast_ip is %s in this RELEASE_REQ, but is %s in the original SETUP_REQ",
                working_app_cfg.ast_ip, glob_app_cfg.ast_ip);

  sprintf(path, "%s/%s/setup_response.xml", LINK_AGENT_DIR, glob_app_cfg.ast_id);
  free_application_cfg(&glob_app_cfg);
  if (topo_xml_parser(path, LINK_AGENT) != 1) {
    memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "didn't parse the file for ast_id successfully");
    glob_app_cfg.status = AST_FAILURE;
    return 0;
  }

  glob_app_cfg.ast_ip = working_app_cfg.ast_ip;
  working_app_cfg.ast_ip = NULL;
  free_application_cfg(&working_app_cfg);
  glob_app_cfg.action = RELEASE_RESP;

  sprintf(path, "%s/%s/release_origianl.xml", 
	  LINK_AGENT_DIR, glob_app_cfg.ast_id);

  if (rename(DRAGON_XML_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	   DRAGON_XML_RECV, path, errno, strerror(errno));

  sprintf(path, "%s/%s/release_response.xml", 
	  LINK_AGENT_DIR, glob_app_cfg.ast_id);

  glob_app_cfg.action = RELEASE_RESP;
  if (dragon_link_release() == 0) 
    glob_app_cfg.status = AST_FAILURE;
  else
    glob_app_cfg.status = AST_SUCCESS;

  print_xml_response(path, LINK_AGENT);
  symlink(path, DRAGON_XML_RESULT);

  sprintf(path, "%s/%s/final.xml",
	  LINK_AGENT_DIR, glob_app_cfg.ast_id);
  print_final(path);
 
  return 1;
}

int
dragon_process_query_req()
{
  char path[105];
  char directory[80];
  struct resource *mynode;
  FILE* fp;

  glob_app_cfg.org_action = glob_app_cfg.action;
  glob_app_cfg.action = QUERY_RESP;
  zlog_info("Processing ast_id: %s, QUERY_REQ", glob_app_cfg.ast_id);

  strcpy(directory, LINK_AGENT_DIR);
  if (mkdir(directory, 0755) == -1 && errno != EEXIST) {
    zlog_err("Can't create directory %s", LINK_AGENT_DIR);
    return 0;
  }

  sprintf(directory, "%s/%s", LINK_AGENT_DIR, glob_app_cfg.ast_id);
  if (mkdir(directory, 0755) == -1) {
    zlog_err("Can't create directory %s", directory);
    return 0;
  }

  sprintf(path, "%s/%s/query_original.xml", 
	LINK_AGENT_DIR, glob_app_cfg.ast_id);

  if (rename(DRAGON_XML_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	   DRAGON_XML_RECV, path, errno, strerror(errno));

  sprintf(path, "%s/%s/query_response.xml", 
	LINK_AGENT_DIR, glob_app_cfg.ast_id);

  glob_app_cfg.action = QUERY_RESP;
  glob_app_cfg.status = AST_SUCCESS;

  fp = fopen(path, "w+");
  if (fp == NULL) {
    zlog_err("Can't open file %s: error = %d(%s)",
	     path, errno, strerror(errno));
    return 0;
  }

  /* prepare ret_buf before sending into process_xml_query
   */
  fprintf(fp, "<topology ast_id=\"%s\", action=\"%s\">\n", 
		glob_app_cfg.ast_id, action_type_details[glob_app_cfg.action]);
  fprintf(fp, "<status>AST_SUCCESS</status>\n");
  mynode = (struct resource*)(glob_app_cfg.node_list->head->data);
  mynode->status = AST_SUCCESS;
  print_node(fp, mynode);
  process_xml_query(fp, mynode->name);
  fprintf(fp, "</topology>");
  fflush(fp);
  fclose(fp);

  symlink(path, DRAGON_XML_RESULT);

  return 1;
}

int
dragon_process_ast_complete()
{
  char path[105];
  struct stat fs;
  struct application_cfg working_app_cfg;
  struct resource *link;
  struct adtlistnode *curnode;

  glob_app_cfg.org_action = glob_app_cfg.action;
  glob_app_cfg.action = APP_COMPLETE;
  zlog_info("Processing ast_id: %s, AST_COMPLETE", glob_app_cfg.ast_id);

  sprintf(path, "%s/%s/final.xml",
	  LINK_AGENT_DIR, glob_app_cfg.ast_id);
  if (stat(path, &fs) == -1) {
    sprintf(glob_app_cfg.details, "Can't locate the ast_id final file");
    return 0;
  }

  memcpy(&working_app_cfg, &glob_app_cfg, sizeof(struct application_cfg));
  memset(&glob_app_cfg, 0, sizeof(struct application_cfg));

  if (agent_final_parser(path) != 1) {
    memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "didn't parse the ast_id file successfully");
    glob_app_cfg.status = AST_FAILURE;
    return 0;
  }

  if (glob_app_cfg.action == RELEASE_RESP) {
    free_application_cfg(&glob_app_cfg);
    memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "ast_id has received RELEASE_REQ already");
    glob_app_cfg.status = AST_FAILURE;
    return 0;
  }

  if (strcmp(glob_app_cfg.ast_ip, working_app_cfg.ast_ip) != 0)
    zlog_warn("ast_ip is %s in this AST_COMPLETE, but is %s in the original SETUP_REQ",
                working_app_cfg.ast_ip, glob_app_cfg.ast_ip);

  /* for link_agent, there is nothing to do for AST_COMPLETE;
   * 1. save the original final file (glob_app_cfg) as final.xml
   * 2. save the incoming file (working_app_cfg) as ast_complete.xml
   */
  sprintf(path, "%s/%s/setup_response.xml", LINK_AGENT_DIR, glob_app_cfg.ast_id);
  free_application_cfg(&glob_app_cfg);
  if (topo_xml_parser(path, LINK_AGENT) != 1) {
    memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "didn't parse the ast_id file successfully");
    glob_app_cfg.status = AST_FAILURE;
    return 0;
  }

  glob_app_cfg.ast_ip = working_app_cfg.ast_ip;
  working_app_cfg.ast_ip = NULL;
  glob_app_cfg.action = working_app_cfg.action;
  free_application_cfg(&working_app_cfg);

  for (curnode = glob_app_cfg.link_list->head;
	curnode;
	curnode = curnode->next) {
    link = (struct resource*) curnode->data;
    link->status = AST_APP_COMPLETE;
  }
  glob_app_cfg.status = AST_APP_COMPLETE;
  sprintf(path,  "%s/%s/final.xml", LINK_AGENT_DIR, glob_app_cfg.ast_id);
  print_final(path);
  symlink(path, DRAGON_XML_RESULT);
  
  sprintf(path, "%s/%s/ast_complete.xml", 
	  LINK_AGENT_DIR, glob_app_cfg.ast_id);

  if (rename(DRAGON_XML_RECV, path) == -1) 
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	DRAGON_XML_RECV, path, errno, strerror(errno));
 
  return 1;
}

int
dragon_process_xml()
{
  int ret_value = 1;

  glob_app_cfg.xml_type = xml_parser(DRAGON_XML_RECV);
  
  switch(glob_app_cfg.xml_type) {

  case TOPO_XML:

  if (topo_xml_parser(DRAGON_XML_RECV, LINK_AGENT) != 1) { 
    zlog_err("dragon_process_xml: received xml file with error"); 
    vty_out(fake_vty, "ERROR: received xml file parse with error\n"); 
    return 0; 
  }
  
  if (topo_validate_graph(LINK_AGENT) == 0) {
    zlog_err("dragon_process_xml: topo_validate_graph() failed");
    return 0;
  }

  if (glob_app_cfg.action == SETUP_REQ) 
    ret_value = dragon_process_setup_req();
  else if (glob_app_cfg.action == RELEASE_REQ)
    ret_value = dragon_process_release_req();
  else if (glob_app_cfg.action == QUERY_REQ)
    ret_value = dragon_process_query_req();
  else if (glob_app_cfg.action == AST_COMPLETE)
    ret_value = dragon_process_ast_complete();

  break;

  case ID_XML:

  if (id_xml_parser(DRAGON_XML_RECV, LINK_AGENT) != 1) {
    zlog_err("dragon_process_xml: recieved xml file with error");
    vty_out(fake_vty, "ERROR: recieved xml file with error\n");
    return 0;
  }

  ret_value = dragon_process_id_cfg();
 
  break;

  default:

  zlog_err("dragon_process_xml: this is neither a <topology> nor <local_id_cfg> request");
  ret_value = 0;
  glob_app_cfg.status = AST_FAILURE;

  }

  if (ret_value) 
    glob_app_cfg.status = AST_SUCCESS;
  else
    glob_app_cfg.status = AST_FAILURE;

  return ret_value;
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
  memset(&glob_app_cfg, 0, sizeof(struct application_cfg));
  memset(&pending_list, 0, sizeof(struct adtlist));
}

static void
xml_module_reset()
{
  buffer_reset(fake_vty->obuf);
  free_application_cfg(&glob_app_cfg);
  unlink(DRAGON_XML_RESULT);
  unlink(DRAGON_XML_RECV);
}
  
static void
generate_lsp_name(char* name)
{
  int i;

  strcpy(name, "AST-");
  
  for (i = 4; i < LSP_NAME_LEN; i++) {
    name[i] = (random() % 10) + 48;
  }
  
  name[LSP_NAME_LEN] = '\0';
}

static struct lsp*
dragon_build_lsp(struct resource *link)
{
  struct lsp* lsp;
  char lsp_name[LSP_NAME_LEN];
  int argc;

  bzero(link->res.l.lsp_name, LSP_NAME_LEN+1);
  link->status = AST_FAILURE;
  generate_lsp_name(lsp_name);
  
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

  /* mirror what dragon_set_lsp_sw_cmd does
   */
  argc = 4;
  strcpy(argv[0], link->res.l.bandwidth);
  strcpy(argv[1], link->res.l.swcap);
  strcpy(argv[2], link->res.l.encoding);
  strcpy(argv[3], link->res.l.gpid);  
  if (dragon_set_lsp_sw (NULL, fake_vty, argc, &argv) != CMD_SUCCESS) {

    argc = 1;
    strcpy(argv[0], lsp_name);
    dragon_delete_lsp(NULL, fake_vty, argc, &argv);
    return NULL;
  }

  /* mirrow what dragon_set_lsp_uni does
   */
  if (link->res.l.stype == uni) {
    argc = 3;
    strcpy(argv[0], "client");
    strcpy(argv[1], "implicit");
    strcpy(argv[2], "implicit");
    if (dragon_set_lsp_uni (NULL, fake_vty, argc, &argv) != CMD_SUCCESS) {
      argc = 1;
      strcpy(argv[0], lsp_name);
      dragon_delete_lsp(NULL, fake_vty, argc, &argv);
      return NULL;
    }
  }
 
  /* mirror what dragon_set_lsp_ip does
   */
  argc = 6;

  switch (link->res.l.stype) {
    case uni:
    case non_uni:

      strcpy(argv[0], link->res.l.src->es->res.n.router_id);
      strcpy(argv[3], link->res.l.dest->es->res.n.router_id);
      break;

    case vlsr_vlsr:
 
      strcpy(argv[0], link->res.l.src->vlsr->res.n.router_id);
      strcpy(argv[3], link->res.l.dest->vlsr->res.n.router_id);
      break;

    case vlsr_es:

      if (link->res.l.src->vlsr && link->res.l.dest->es) {
	strcpy(argv[0], link->res.l.src->vlsr->res.n.router_id);
	strcpy(argv[3], link->res.l.dest->es->res.n.router_id);
      } else {
	strcpy(argv[0], link->res.l.src->es->res.n.router_id);
	strcpy(argv[3], link->res.l.dest->vlsr->res.n.router_id);
      }
      break;
  }

  if (link->res.l.src->local_id_type[0] == '\0') {
    strcpy(argv[1], "lsp-id");
    sprintf(argv[2], "%d", random() % 3000);
  } else { 
    strcpy(argv[1], link->res.l.src->local_id_type); 
    sprintf(argv[2], "%d", link->res.l.src->local_id);
  }

  if (link->res.l.dest->local_id_type[0] == '\0') {
    strcpy(argv[4], "tunnel-id");
    sprintf(argv[5], "%d", random() % 3000);
  } else {
    strcpy(argv[4], link->res.l.dest->local_id_type);
    sprintf(argv[5], "%d", link->res.l.dest->local_id);
  }

  zlog_info("parameters to dragon_set_lsp_ip:");
  zlog_info("%s | %s | %s | %s | %s | %s", argv[0], argv[1], argv[2], argv[3], argv[4], argv[5]);
  if (dragon_set_lsp_ip (NULL, fake_vty, argc, &argv) != CMD_SUCCESS) {
    
    argc = 1;
    strcpy(argv[0], lsp_name);
    dragon_delete_lsp(NULL, fake_vty, argc, &argv);
    return NULL;
  }

  if (link->res.l.vtag[0] != '\0') {
    argc = 1;
    strcpy(argv[0], link->res.l.vtag);

    if (dragon_set_lsp_vtag(NULL, fake_vty, argc, &argv) != CMD_SUCCESS) {
      dragon_delete_lsp(NULL, fake_vty, argc, &argv);
      return NULL;
    }
  }

  /* mirror what dragon_commit_lsp_sender does
   */
  argc = 1;
  strcpy(argv[0], lsp_name);
  if (dragon_commit_lsp_sender(NULL, fake_vty, argc, &argv) != CMD_SUCCESS) {
    dragon_delete_lsp(NULL, fake_vty, argc, &argv); 
    return NULL;
  }
 
  strcpy(link->res.l.lsp_name, lsp_name);
  link->status = AST_SUCCESS;

  return lsp;
}

void
dragon_release_lsp(struct resource *link)
{
  int argc;

  /* mirror what dragon_set_lsp_ip does 
   */
  argc = 1;
  strcpy(argv[0], link->res.l.lsp_name);
  if (dragon_delete_lsp (NULL, fake_vty, argc, &argv) != CMD_SUCCESS) 
    link->status = AST_FAILURE;
  else 
    link->status = AST_SUCCESS;
}


static int 
dragon_link_provision() 
{
  struct lsp *lsp = NULL;
  struct adtlistnode *curnode,*curnode1;
  struct resource *mynode, *mylink;
  int i, j, success;
 
  /* Now, loop through the task/link list and provision each link
   * it would be nice to dump this lsp into dmaster.dragon_lsp_table
   * with name as "XML-<8char>"
   * so that the show lsp in CLI can also show the link provisioned by 
   * AST
   */ 
  for ( i = 1, success = 0, curnode = glob_app_cfg.node_list->head; 
	curnode;
	i++, curnode = curnode->next) {
    mynode = (struct resource*)(curnode->data);
 
    if (mynode->res.n.link_list) { 
      for (j = 1, curnode1 = mynode->res.n.link_list->head; 
	   curnode1; 
	   j++, curnode1 = curnode1->next) { 
	mylink = (struct resource*)(curnode1->data); 
	buffer_reset(fake_vty->obuf);
	lsp = dragon_build_lsp(mylink); 
	if (!lsp) { 
	  switch (mylink->res.l.stype) {
	    case uni:
	    case non_uni:
	      zlog_err("ERROR: lsp is not set between ES(%s) and ES(%s)\n",
			mylink->res.l.src->es->name,
			mylink->res.l.dest->es->name);
	      break;

	    case vlsr_vlsr:
	      zlog_err("ERROR: lsp is not set between vlsr(%s) and vlsr(%s)\n", 
			mylink->res.l.src->vlsr->name, 
			mylink->res.l.dest->vlsr->name); 
	      break;

	    case vlsr_es:
	      if (mylink->res.l.src->vlsr && mylink->res.l.dest->es) 
	        zlog_err("ERROR: lsp is not set between vlsr(%s) and ES(%s)\n",
			mylink->res.l.src->vlsr->name,
			mylink->res.l.dest->es->name);
	      else
		zlog_err("ERROR: lsp is not set between ES(%s) and vlsr(%s)\n",
			mylink->res.l.src->es->name,
			mylink->res.l.dest->vlsr->name);
	      break;

	  }
	  mylink->status = AST_FAILURE;
    	  buffer_putc(fake_vty->obuf, '\0');
    	  mylink->agent_message = buffer_getstr(fake_vty->obuf);
	  continue;
	} else {
	  struct dragon_callback *data = malloc(sizeof(struct dragon_callback));
	  bzero(data, sizeof(struct dragon_callback));
          data->ast_id = strdup(glob_app_cfg.ast_id);
	  strncpy(data->lsp_name, mylink->res.l.lsp_name, LSP_NAME_LEN);
	  data->stype = mylink->res.l.stype;

	  switch (mylink->res.l.stype) {
	    case uni:
	    case non_uni:
	      strcpy(data->link_agent, mylink->res.l.src->es->name);
	      data->node_stype = mylink->res.l.src->es->res.n.stype;
	      zlog_info("lsp has been set between ES(%s) and ES(%s)\n", 
			mylink->res.l.src->es->name, 
			mylink->res.l.dest->es->name);

	      break;

	    case vlsr_vlsr:
	      strcpy(data->link_agent, mylink->res.l.src->vlsr->name);
	      data->node_stype = mylink->res.l.src->vlsr->res.n.stype;
	      zlog_info("lsp has been set between vlsr(%s) and vlsr(%s)\n",
			mylink->res.l.src->vlsr->name, 
			mylink->res.l.dest->vlsr->name);
	      break;

	    case vlsr_es:
	      if (mylink->res.l.src->vlsr && mylink->res.l.dest->es) {
		strcpy(data->link_agent, mylink->res.l.src->vlsr->name);
		data->node_stype = mylink->res.l.src->vlsr->res.n.stype;
		zlog_info("lsp has been set between vlsr(%s) and ES(%s)\n",
			mylink->res.l.src->vlsr->name,
			mylink->res.l.dest->es->name);
	      } else {
		strcpy(data->link_agent, mylink->res.l.src->es->name);
		data->node_stype = mylink->res.l.src->es->res.n.stype;
		zlog_info("lsp has been set between ES(%s) and vlsr(%s)\n",
			mylink->res.l.src->es->name,
			mylink->res.l.dest->vlsr->name);
	      }
	      break;

	  }
	  strncpy(data->link_name, mylink->name, NODENAME_MAXLEN);
	  adtlist_add(&pending_list, data);
	  mylink->status = AST_PENDING;
        }
	   
	success++;
      }
      break;
    }
  }

  if (success == adtlist_getcount(glob_app_cfg.link_list))
    glob_app_cfg.status = AST_SUCCESS;
  else
    glob_app_cfg.status = AST_FAILURE;
  
  return (success == adtlist_getcount(glob_app_cfg.link_list));
}

static int
dragon_link_release()
{
  struct adtlistnode *curnode,*curnode1;
  struct resource *mynode, *mylink;
  int i, j, success;
  
  /* Now, loop through the task/link list and provision each link
   * it would be nice to dump this lsp into dmaster.dragon_lsp_table
   * with name as "XML-<8char>"
   * so that the show lsp in CLI can also show the link provisioned by
   * AST
   */
  for ( i = 1, success = 0, curnode = glob_app_cfg.node_list->head;
	curnode;
	i++, curnode = curnode->next) {
    mynode = (struct resource*)(curnode->data);

    if (!mynode->res.n.link_list) {
      for (j = 1, curnode1 = mynode->res.n.link_list->head;
	   curnode1;
	   j++, curnode1 = curnode1->next) {
	mylink = (struct resource*)(curnode1->data);

	if (mylink->res.l.lsp_name[0] != '\0') 
	  dragon_release_lsp(mylink);
	if (mylink->status == AST_SUCCESS)
	  success++;
      }
      break;
    }
  }

  if (success == adtlist_getcount(glob_app_cfg.link_list))
    glob_app_cfg.status = AST_SUCCESS;
  else
    glob_app_cfg.status = AST_FAILURE;

  return (success == adtlist_getcount(glob_app_cfg.link_list));
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
  int xml_sock, accept_sock, total = 0;
  struct sockaddr_in clientAddr;
  struct sockaddr_in astAddr;
  int recvMsgSize, clientLen;
  static char buffer[XML_FILE_RECV_BUF];
  struct sigaction myAction;
  int fd;
  static struct stat file_stat;

  accept_sock = THREAD_FD (thread);
  xml_module_reset();
  clientLen = sizeof(clientAddr);
  xml_sock = accept (accept_sock, (struct sockaddr*)&clientAddr, &clientLen);

  if (xml_sock < 0) {
    zlog_err("xml_accept: accept failed() for xmlServSock");
  } else {
    FILE* fp;

    glob_app_cfg.ast_ip = strdup(inet_ntoa(clientAddr.sin_addr));
    zlog_info("xml_accept: Handling client %s ...", glob_app_cfg.ast_ip);
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
    
    fp = fopen(DRAGON_XML_RECV, "w");
 
    alarm(TIMEOUT_SECS);
    errno = 0;
    memset(buffer, 0, XML_FILE_RECV_BUF);
    while ((recvMsgSize = recv(xml_sock, buffer, XML_FILE_RECV_BUF-1, 0)) > 0) {
      if (errno == EINTR) {
	/* alarm went off
	 */
	zlog_warn("xml_accept: dragon probably has not received all datas");
        zlog_warn("recvMsgSize = %d", recvMsgSize);
	break;
      }
      buffer[recvMsgSize]='\0';
      fprintf(fp, "%s", buffer);
      total += recvMsgSize; 
      alarm(TIMEOUT_SECS);
    }
    alarm(0);

    zlog_info("xml_accept: total byte received = %d", total);
   
    if (total != 0) {
      fflush(fp);
      fclose(fp); 

      if (dragon_process_xml())
	glob_app_cfg.status = AST_SUCCESS;
      else
	glob_app_cfg.status = AST_FAILURE;

      if (glob_app_cfg.action == APP_COMPLETE) {

	/* the clntSock should have been void */
	close(xml_sock);
	if ((xml_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
	  zlog_err("socket() failed");
	else {
	  memset(&astAddr, 0, sizeof(astAddr));
	  astAddr.sin_family = AF_INET;
	  astAddr.sin_addr.s_addr = inet_addr(glob_app_cfg.ast_ip);
	  astAddr.sin_port = htons(MASTER_PORT);
  
	  if (connect(xml_sock, (struct sockaddr*)&astAddr, sizeof(astAddr)) < 0) {
	    zlog_err("connect() failed to ast_master");
	    close(xml_sock);
	    xml_sock = -1;
	  }
	}
      }

      if (stat(DRAGON_XML_RESULT, &file_stat) == -1) {
	/* the result file hasn't been written
	 */
	glob_app_cfg.status = AST_FAILURE;
	print_error_response(DRAGON_XML_RESULT);
      }

      if (xml_sock!= -1) {
	if (glob_app_cfg.action == APP_COMPLETE) 
	  zlog_info("Sending APP_COMPLETE for ast_id: %s to ast_master at (%s:%d)",
	 	glob_app_cfg.ast_id,
		glob_app_cfg.ast_ip, MASTER_PORT);
	else 
	  zlog_info("sending confirmation (%s) to ast_master at (%s:%d)",
                  status_type_details[glob_app_cfg.status],
                  glob_app_cfg.ast_ip, MASTER_PORT);

	fd = open(DRAGON_XML_RESULT, O_RDONLY);
#ifdef __FreeBSD__
	total = sendfile(fd, xml_sock, 0, 0, NULL, NULL, 0);
	zlog_info("sendfile() returns %d", total);
#else
	if (fstat(fd, &file_stat) == -1) {
  	  zlog_err("fstat() failed on %s", XML_NEW_FILE);
	}
	total = sendfile(xml_sock, fd, 0, file_stat.st_size);
	zlog_info("file_size is %d and sendfile() returns %d", 
			file_stat.st_size, total); 
#endif
	if (total < 0) 
    	  zlog_err("sendfile() failed; errno = %d (%s)\n",
		 errno, strerror(errno)); 

	close(fd);
	close(xml_sock);
      } 
    } else
      fclose(fp);

    zlog_info("DONE!");
  }

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

      ret = listen (sock, 3);
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
