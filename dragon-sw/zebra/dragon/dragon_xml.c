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
#include "linklist.h"
#include "memory.h"
#include "buffer.h"
#include "dragon/dragond.h"

#define DRAGON_XML_RESULT 	"/usr/local/dragon/dragon_ret.xml"
#define DRAGON_XML_RECV 	"/usr/local/dragon/dragon_recv.xml"
#define LINK_AGENT_DIR		"/usr/local/dragon/link_agent"
#define LSP_NAME_LEN 		13
#define XML_FILE_RECV_BUF	250
#define TIMEOUT_SECS		3

extern struct thread_master *master;
extern char *status_type_details[];
static struct vty* fake_vty = NULL;
static char* argv[7];

extern void set_lsp_default_para(struct lsp*);
extern void process_xml_query(FILE *, char*);

static int dragon_link_provision();
static int dragon_link_release();

static struct vty*
generate_fake_vty()
{
  struct vty* vty;

  vty = vty_new();
  vty->type = VTY_FILE;

  return vty;
}

int
dragon_process_setup_req()
{
  char path[105];
  char directory[80];

  glob_app_cfg.org_function = SETUP_REQ;
  glob_app_cfg.function = SETUP_RESP;
  zlog_info("Processing glob_ast_id: %s, SETUP_REQ", glob_app_cfg.glob_ast_id);

  strcpy(directory, LINK_AGENT_DIR);
  if (mkdir(directory, 0755) == -1 && errno != EEXIST) {
    zlog_err("Can't create diectory %s", directory);
    return 0;
  }

  sprintf(directory+strlen(directory), "/%s", glob_app_cfg.glob_ast_id);
  if (mkdir(directory, 0755) == -1) {
    if (errno == EEXIST) {
      zlog_err("<glob_ast_id> %s exists already", glob_app_cfg.glob_ast_id);
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
    glob_app_cfg.ast_status = AST_FAILURE;
  else
    glob_app_cfg.ast_status = AST_SUCCESS;

  glob_app_cfg.function = SETUP_RESP;
  print_xml_response(path, BRIEF_VERSION);
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

  glob_app_cfg.org_function = glob_app_cfg.function;
  glob_app_cfg.function = RELEASE_RESP;
  zlog_info("Processing glob_ast_id: %s, RELEASE_REQ", glob_app_cfg.glob_ast_id);

  sprintf(path, "%s/%s/final.xml", 
	  LINK_AGENT_DIR, glob_app_cfg.glob_ast_id);
  if (stat(path, &fs) == -1) {
    sprintf(glob_app_cfg.details, "Can't locate the glob_ast_id final file");
    return 0;
  }

  memcpy(&working_app_cfg, &glob_app_cfg, sizeof(struct application_cfg));
  memset(&glob_app_cfg, 0, sizeof(struct application_cfg));

  if (agent_final_parser(path) != 1) {
    memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "didn't parse the file for glob_ast_id successfully");
    glob_app_cfg.ast_status = AST_FAILURE;
    return 0;
  }

  /* before processing, set all link's ast_status = AST_FAILURE
   */
  if (glob_app_cfg.function == RELEASE_RESP) {
    free_application_cfg(&glob_app_cfg);
    memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "glob_ast_id has received RELEASE_REQ already");
    glob_app_cfg.ast_status = AST_FAILURE;
    return 0;
  }

  if (strcmp(glob_app_cfg.ast_ip, working_app_cfg.ast_ip) != 0)
    zlog_warn("ast_ip is %s in this RELEASE_REQ, but is %s in the original SETUP_REQ",
                working_app_cfg.ast_ip, glob_app_cfg.ast_ip);

  sprintf(path, "%s/%s/setup_response.xml", LINK_AGENT_DIR, glob_app_cfg.glob_ast_id);
  free_application_cfg(&glob_app_cfg);
  if (topo_xml_parser(path, BRIEF_VERSION) != 1) {
    memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "didn't parse the file for glob_ast_id successfully");
    glob_app_cfg.ast_status = AST_FAILURE;
    return 0;
  }

  glob_app_cfg.ast_ip = working_app_cfg.ast_ip;
  working_app_cfg.ast_ip = NULL;
  free_application_cfg(&working_app_cfg);
  glob_app_cfg.function = RELEASE_RESP;

  sprintf(path, "%s/%s/release_origianl.xml", 
	  LINK_AGENT_DIR, glob_app_cfg.glob_ast_id);

  if (rename(DRAGON_XML_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	   DRAGON_XML_RECV, path, errno, strerror(errno));

  sprintf(path, "%s/%s/release_response.xml", 
	  LINK_AGENT_DIR, glob_app_cfg.glob_ast_id);

  glob_app_cfg.function = RELEASE_RESP;
  if (dragon_link_release() == 0) 
    glob_app_cfg.ast_status = AST_FAILURE;
  else
    glob_app_cfg.ast_status = AST_SUCCESS;

  print_xml_response(path, BRIEF_VERSION);
  symlink(path, DRAGON_XML_RESULT);

  sprintf(path, "%s/%s/final.xml",
	  LINK_AGENT_DIR, glob_app_cfg.glob_ast_id);
  print_final(path);
 
  return 1;
}

int
dragon_process_query_req()
{
  char path[105];
  char directory[80];
  char buffer[400];
  struct node_cfg* mynode;
  FILE* fp;

  glob_app_cfg.org_function = glob_app_cfg.function;
  glob_app_cfg.function = QUERY_RESP;
  zlog_info("Processing glob_ast_id: %s, QUERY_REQ", glob_app_cfg.glob_ast_id);

  strcpy(directory, LINK_AGENT_DIR);
  if (mkdir(directory, 0755) == -1 && errno != EEXIST) {
    zlog_err("Can't create directory %s", LINK_AGENT_DIR);
    return 0;
  }

  sprintf(directory, "%s/%s", LINK_AGENT_DIR, glob_app_cfg.glob_ast_id);
  if (mkdir(directory, 0755) == -1) {
    zlog_err("Can't create directory %s", directory);
    return 0;
  }

  sprintf(path, "%s/%s/query_original.xml", 
	LINK_AGENT_DIR, glob_app_cfg.glob_ast_id);

  if (rename(DRAGON_XML_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	   DRAGON_XML_RECV, path, errno, strerror(errno));

  sprintf(path, "%s/%s/query_response.xml", 
	LINK_AGENT_DIR, glob_app_cfg.glob_ast_id);

  glob_app_cfg.function = QUERY_RESP;
  glob_app_cfg.ast_status = AST_SUCCESS;

  fp = fopen(path, "w+");
  if (fp == NULL) {
    zlog_err("Can't open file %s: error = %d(%s)",
	     path, errno, strerror(errno));
    return 0;
  }

  /* prepare ret_buf before sending into process_xml_query
   */
  fprintf(fp, "<topology>\n");
  fprintf(fp, "<ast_func>QUERY_RESP</ast_func>\n");
  fprintf(fp, "<glob_ast_id>%s</glob_ast_id>\n", glob_app_cfg.glob_ast_id);
  fprintf(fp, "<ast_status>AST_SUCCESS</ast_status>\n");
  mynode = (struct node_cfg*)(glob_app_cfg.node_list->head->data);
  mynode->ast_status = AST_SUCCESS;
  print_node(buffer, mynode);
  fprintf(fp, buffer);
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
  struct link_cfg *link;
  struct adtlistnode *curnode;

  glob_app_cfg.org_function = glob_app_cfg.function;
  glob_app_cfg.function = APP_COMPLETE;
  zlog_info("Processing glob_ast_id: %s, AST_COMPLETE", glob_app_cfg.glob_ast_id);

  sprintf(path, "%s/%s/final.xml",
	  LINK_AGENT_DIR, glob_app_cfg.glob_ast_id);
  if (stat(path, &fs) == -1) {
    sprintf(glob_app_cfg.details, "Can't locate the glob_ast_id final file");
    return 0;
  }

  memcpy(&working_app_cfg, &glob_app_cfg, sizeof(struct application_cfg));
  memset(&glob_app_cfg, 0, sizeof(struct application_cfg));

  if (agent_final_parser(path) != 1) {
    memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "didn't parse the glob_ast_id file successfully");
    glob_app_cfg.ast_status = AST_FAILURE;
    return 0;
  }

  if (glob_app_cfg.function == RELEASE_RESP) {
    free_application_cfg(&glob_app_cfg);
    memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "glob_ast_id has received RELEASE_REQ already");
    glob_app_cfg.ast_status = AST_FAILURE;
    return 0;
  }

  if (strcmp(glob_app_cfg.ast_ip, working_app_cfg.ast_ip) != 0)
    zlog_warn("ast_ip is %s in this AST_COMPLETE, but is %s in the original SETUP_REQ",
                working_app_cfg.ast_ip, glob_app_cfg.ast_ip);

  /* for link_agent, there is nothing to do for AST_COMPLETE;
   * 1. save the original final file (glob_app_cfg) as final.xml
   * 2. save the incoming file (working_app_cfg) as ast_complete.xml
   */
  sprintf(path, "%s/%s/setup_response.xml", LINK_AGENT_DIR, glob_app_cfg.glob_ast_id);
  free_application_cfg(&glob_app_cfg);
  if (topo_xml_parser(path, BRIEF_VERSION) != 1) {
    memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "didn't parse the glob_ast_id file successfully");
    glob_app_cfg.ast_status = AST_FAILURE;
    return 0;
  }

  glob_app_cfg.ast_ip = working_app_cfg.ast_ip;
  working_app_cfg.ast_ip = NULL;
  glob_app_cfg.function = working_app_cfg.function;
  free_application_cfg(&working_app_cfg);

  for (curnode = glob_app_cfg.link_list->head;
	curnode;
	curnode = curnode->next) {
    link = (struct link_cfg*) curnode->data;
    link->ast_status = AST_APP_COMPLETE;
  }
  glob_app_cfg.ast_status = AST_APP_COMPLETE;
  sprintf(path,  "%s/%s/final.xml", LINK_AGENT_DIR, glob_app_cfg.glob_ast_id);
  print_final(path);
  symlink(path, DRAGON_XML_RESULT);
  
  sprintf(path, "%s/%s/ast_complete.xml", 
	  LINK_AGENT_DIR, glob_app_cfg.glob_ast_id);

  if (rename(DRAGON_XML_RECV, path) == -1) 
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	DRAGON_XML_RECV, path, errno, strerror(errno));
 
  return 1;
}

int
dragon_process_xml()
{
  int ret_value = 1;

  if (topo_xml_parser(DRAGON_XML_RECV, BRIEF_VERSION) != 1) { 
    zlog_err("dragon_process_xml: received xml file parse with error"); 
    vty_out(fake_vty, "ERROR: received xml file parse with error\n"); 
    return 0; 
  }

  if (glob_app_cfg.function != SETUP_REQ &&
      glob_app_cfg.function != RELEASE_REQ &&
      glob_app_cfg.function != QUERY_REQ &&
      glob_app_cfg.function != AST_COMPLETE) {
    zlog_err("master_process_xml: invalid <ast_func> in xml file");
    sprintf(glob_app_cfg.details, "invalid ast_func in xml file");
    return 0;
  }

  if (glob_app_cfg.glob_ast_id == NULL) {
    zlog_err("dragon_process_xml: received xml file doesn't have glob_ast_id");
    return 0;
  }

  if (glob_app_cfg.function == SETUP_REQ) 
    ret_value = dragon_process_setup_req();
  else if (glob_app_cfg.function == RELEASE_REQ)
    ret_value = dragon_process_release_req();
  else if (glob_app_cfg.function == QUERY_REQ)
    ret_value = dragon_process_query_req();
  else if (glob_app_cfg.function == AST_COMPLETE)
    ret_value = dragon_process_ast_complete();

  return ret_value;
}

static void
xml_module_init()
{
  int i;
  char* assign;

  fake_vty = generate_fake_vty();
  assign = malloc(7*20*sizeof(char));
  for (i = 0; i < 7; i++) 
    argv[i] = assign+(i*50);
  memset(&glob_app_cfg, 0, sizeof(struct application_cfg));
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
dragon_build_lsp(struct link_cfg *link)
{
  struct lsp* lsp;
  char lsp_name[LSP_NAME_LEN];
  int argc;

  memset(link->lsp_name, 0, LSP_NAME_LEN);
  link->ast_status = AST_FAILURE;
  generate_lsp_name(lsp_name);
  
  /* mirror what dragon_edit_lsp_cmd does
   */
  lsp = XMALLOC(MTYPE_OSPF_DRAGON, sizeof(struct lsp));
  memset(lsp, 0, sizeof(struct lsp));
  set_lsp_default_para(lsp);
  lsp->status = LSP_EDIT;
  strcpy((lsp->common.SessionAttribute_Para)->sessionName, lsp_name);
  (lsp->common.SessionAttribute_Para)->nameLength = strlen(lsp_name);
  fake_vty->index = lsp;
  listnode_add(dmaster.dragon_lsp_table, lsp);

  /* mirror what dragon_set_lsp_sw_cmd does
   */
  argc = 4;
  strcpy(argv[0], link->bandwidth);
  strcpy(argv[1], link->swcap);
  strcpy(argv[2], link->encoding);
  strcpy(argv[3], link->gpid);  
  if (dragon_set_lsp_sw (NULL, fake_vty, argc, &argv) != CMD_SUCCESS) {

    argc = 1;
    strcpy(argv[0], lsp_name);
    dragon_delete_lsp(NULL, fake_vty, argc, &argv);
    return NULL;
  }
 
  /* mirror what dragon_set_lsp_ip does 
   */
  /* mirror what dragon_set_lsp_ip does
   */
  argc = 6;
  if (link->src_local_id_type[0] == '\0') {
    strcpy(argv[0], link->src->te_addr);
    strcpy(argv[1], "lsp->id"); 
    sprintf(argv[2], "%d", random() % 3000); 
  } else {
    strcpy(argv[1], link->src_local_id_type);
    sprintf(argv[2], "%d", link->src_local_id);
    if (strcasecmp(link->src_local_id_type, "lsp-id") == 0) 
      strcpy(argv[0], link->src->te_addr);
    else
      strcpy(argv[0], link->src->ipadd);
  }
  if (link->dest_local_id_type[0] == '\0') {
    strcpy(argv[3], link->dest->te_addr);
    strcpy(argv[4], "tunnel-id");
    sprintf(argv[5], "%d", random() % 3000);
  } else {
    strcpy(argv[4], strcasecmp(link->dest_local_id_type, "lsp-id") == 0 ? 
		"tunnel-id":link->dest_local_id_type);
    sprintf(argv[5], "%d", link->dest_local_id);
    if (strcasecmp(link->dest_local_id_type, "lsp-id") == 0) 
      strcpy(argv[3], link->dest->te_addr);
    else 
      strcpy(argv[3], link->dest->ipadd);
  }

  if (dragon_set_lsp_ip (NULL, fake_vty, argc, &argv) != CMD_SUCCESS) {
    
    argc = 1;
    strcpy(argv[0], lsp_name);
    dragon_delete_lsp(NULL, fake_vty, argc, &argv);
    return NULL;
  }

  if (link->vtag[0] != '\0') {
    argc = 1;
    strcpy(argv[0], link->vtag);

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
 
  strcpy(link->lsp_name, lsp_name);
  link->ast_status = AST_SUCCESS;

  return lsp;
}

void
dragon_release_lsp(struct link_cfg *link)
{
  int argc;

  /* mirror what dragon_set_lsp_ip does 
   */
  argc = 1;
  strcpy(argv[0], link->lsp_name);
  if (dragon_delete_lsp (NULL, fake_vty, argc, &argv) != CMD_SUCCESS) 
    link->ast_status = AST_FAILURE;
  else 
    link->ast_status = AST_SUCCESS;
}


static int 
dragon_link_provision() 
{
  struct lsp *lsp = NULL;
  struct adtlistnode *curnode,*curnode1;
  struct node_cfg *mynode;
  struct link_cfg *mylink;
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
    mynode = (struct node_cfg*)(curnode->data);
 
    if (mynode->link_list != NULL) { 
      for (j = 1, curnode1 = mynode->link_list->head; 
	   curnode1; 
	   j++, curnode1 = curnode1->next) { 
	mylink = (struct link_cfg*)(curnode1->data); 
	lsp = dragon_build_lsp(mylink); 
	if (!lsp) { 
	  vty_out(fake_vty, "ERROR: lsp is not set between %s and %s\n", 
		  mylink->src->name, mylink->dest->name); 
	  mylink->ast_status = AST_FAILURE;
    	  buffer_putc(fake_vty->obuf, '\0');
    	  mylink->agent_message = buffer_getstr(fake_vty->obuf);
    	  buffer_reset(fake_vty->obuf);
	  continue;
	} 
	success++;
	vty_out(fake_vty, "SUCCESS: Lsp has been set between %s and %s\n", 
		mylink->src->name, mylink->dest->name); 
      }
      break;
    }
  }

  if (success == adtlist_getcount(glob_app_cfg.link_list))
    glob_app_cfg.ast_status = AST_SUCCESS;
  else
    glob_app_cfg.ast_status = AST_FAILURE;
  
  return (success == adtlist_getcount(glob_app_cfg.link_list));
}

static int
dragon_link_release()
{
  struct adtlistnode *curnode,*curnode1;
  struct node_cfg *mynode;
  struct link_cfg *mylink;
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
    mynode = (struct node_cfg*)(curnode->data);

    if (mynode->link_list != NULL) {
      for (j = 1, curnode1 = mynode->link_list->head;
	   curnode1;
	   j++, curnode1 = curnode1->next) {
	mylink = (struct link_cfg*)(curnode1->data);

	if (mylink->lsp_name[0] != '\0') 
	  dragon_release_lsp(mylink);
	if (mylink->ast_status == AST_SUCCESS)
	  success++;
      }
      break;
    }
  }

  if (success == adtlist_getcount(glob_app_cfg.link_list))
    glob_app_cfg.ast_status = AST_SUCCESS;
  else
    glob_app_cfg.ast_status = AST_FAILURE;

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
	glob_app_cfg.ast_status = AST_SUCCESS;
      else
	glob_app_cfg.ast_status = AST_FAILURE;

      if (glob_app_cfg.function == APP_COMPLETE) {

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
	glob_app_cfg.ast_status = AST_FAILURE;
	print_error_response(DRAGON_XML_RESULT);
      }

      if (xml_sock!= -1) {
	if (glob_app_cfg.function == APP_COMPLETE) 
	  zlog_info("Sending APP_COMPLETE for glob_ast_id: %s to ast_master at (%s:%d)",
	 	glob_app_cfg.glob_ast_id,
		glob_app_cfg.ast_ip, MASTER_PORT);
	else 
	  zlog_info("sending confirmation (%s) to ast_master at (%s:%d)",
                  status_type_details[glob_app_cfg.ast_status],
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
  req.ai_family = AF_UNSPEC;
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
      if (ainfo->ai_family != AF_INET
#ifdef HAVE_IPV6
	  && ainfo->ai_family != AF_INET6
#endif /* HAVE_IPV6 */
	  )
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
