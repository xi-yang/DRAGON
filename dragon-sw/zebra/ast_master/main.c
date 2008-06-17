#include <zebra.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#if !defined(__FreeBSD__) && !defined(__APPLE__)
#include <sys/sendfile.h>
#endif
#include "vty.h"
#include "ast_master_ext.h"
#include "ast_master.h"
#include "libxml/xmlmemory.h"
#include "libxml/parser.h"
#include "libxml/tree.h"
#include "libxml/relaxng.h"
#include "version.h"
#include "getopt.h"
#include "thread.h"
#include "prefix.h"
#include "linklist.h"
#include "if.h"
#include "vector.h"
#include "vty.h"
#include "command.h"
#include "filter.h"
#include "plist.h"
#include "stream.h"
#include "log.h"
#include "memory.h"
#include "buffer.h"
#include "dragon_app.h"

/* NEW STUFF */
extern struct res_mods all_res_mod;

#ifdef RESOURCE_BROKER
#define BROKER_SEND     "/usr/local/broker_send.xml"
#define BROKER_RECV     "/usr/local/broker_resp.xml"
#endif

#define AST_CLIENT_SEND_FILE 	"/usr/local/ast_client_sent.xml"
#define AST_CLIENT_RESULT_FILE 	"/usr/local/ast_client_result.xml"

#define AST_TEMP_FILE		"/usr/local/ast_master_temp.xml"

#define CLIENT_TIMEOUT	20

/* Configuration filename and directory. */
char config_current[] = MASTER_DEFAULT_CONFIG;
char config_default[] = SYSCONFDIR MASTER_DEFAULT_CONFIG;

struct thread_master *master; /* master = dmaster.master */
extern char *status_type_details[];
extern char *action_type_details[];
static int master_accept(struct thread *);
static int minion_callback(struct thread *);
static void clean_socket(struct application_cfg *);
static void init_socket(struct application_cfg *);
static void master_check_app_list();
static void handle_alarm();
extern int master_process_id(char*);
extern struct application_cfg* master_final_parser(char*, int);
extern int send_file_to_agent(char *, int, char *);

/* backward compatibility */
struct vtag_tank {
  int number;
  int vtags[20];
};

struct narb_tank {
  int number;
  struct {
    char* prefix;
    char* narb_ip;
  } narbs[5];
};

static struct vtag_tank vtag_pool;
static struct narb_tank narb_pool;
extern int master_recv_alarm;

/* structurs defined for the resource agency
 *
 */
struct resource_agent {
  char *add;
  int  port;
};
static struct resource_agent agency[5]; 

int master_process_release_req();

#define MAXPENDING      12

int send_task_to_minions(struct adtlist*, enum resource_type);
void integrate_result();

struct option ast_master_opts[] =
{
  { "daemon",     no_argument,       NULL, 'd'},
  { "help",	  no_argument,	     NULL, 'h'},
  { "config_file", required_argument, NULL, 'c'},
  { 0 }
};

/* Help information display. */
static void
usage (char *progname, int status)
{
  if (status != 0)
    printf("Try \"%s --help\" for more information.\n", progname);
  else 
      printf ("Usage : %s [OPTION...]\n\
NSF AST Master.\n\n\
-d, --daemon       Runs in daemon mode\n\
-c, --config_file   Set configuraiton file name\n\
-h, --help	 Display this help and exit\n\
-v, --validate	 Validate the xml file\n\
-f, --sendfile	 Send the xml file to ast_master server\n\
\n", progname); 
    
  exit (status);
}

void 
release_ast(char* ast_id)
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

static void 
init_socket(struct application_cfg *app_cfg)
{
  struct adtlistnode *curnode;
  struct resource *res;

  if (!app_cfg)
    return;

  if (app_cfg->node_list) {
    for (curnode = app_cfg->node_list->head;
  	curnode;
  	curnode = curnode->next) {
      res = (struct resource*)curnode->data;
      res->minion_sock = -1;
    }
  }

  if (app_cfg->link_list) {
    for (curnode = app_cfg->link_list->head;
	curnode;
	curnode = curnode->next) {
      res = (struct resource*)curnode->data;
      res->minion_sock = -1;
    }
  }
}

static void 
clean_socket(struct application_cfg *app_cfg)
{
  struct adtlistnode *curnode;
  struct resource *res;

  if (!app_cfg)
    return;

  if (app_cfg->node_list) {
    for (curnode = app_cfg->node_list->head;
  	curnode;
  	curnode = curnode->next) {
      res = (struct resource*)curnode->data;
  
      if (res->minion_sock != -1) {
	thread_remove_read(master, minion_callback, NULL, res->minion_sock);
	close(res->minion_sock);
//	zlog_info("SOCK: closing minion_sock %d", res->minion_sock);
	res->minion_sock = -1;
      }
    }
  }
  
  if (app_cfg->link_list) {
    for (curnode = app_cfg->link_list->head;
  	curnode;
  	curnode = curnode->next) {
      res = (struct resource*)curnode->data;
  
      if (res->minion_sock != -1) {
	thread_remove_read(master, minion_callback, NULL, res->minion_sock);
	close(res->minion_sock);
//	zlog_info("SOCK: closing minion_sock %d", res->minion_sock);
	res->minion_sock = -1;
      }
    }
  }
}

static void
init_application_module()
{
  /* DEVELOPER: add your resource module in here
   * dragon_app serves as an example, please consult dragon_app.[ch]
   */
  init_dragon_module();
}

void
set_res_fail(char* error_msg, struct resource *res)
{
  zlog_err(error_msg);
  if (res->agent_message) 
    free(res->agent_message); 
  res->agent_message = strdup(error_msg);

  res->status = ast_failure;
  glob_app_cfg->status = ast_failure;
}

void
print_old_final(char* path)
{
  FILE *fp;
  struct adtlistnode *curnode;

  if (!path)
    return;

  fp = fopen(path, "w+");
  if (!fp)
    return;

  fprintf(fp, "<topology ast_id=\"%s\" action=\"%s\">\n",
                glob_app_cfg->ast_id, action_type_details[glob_app_cfg->action])
;
  if (glob_app_cfg->status)
    fprintf(fp, "<status>%s</status>\n", status_type_details[glob_app_cfg->status]);
  if (glob_app_cfg->xml_file[0] != '\0')
    fprintf(fp, "<xml_file>%s</xml_file>\n", glob_app_cfg->xml_file);
  if (glob_app_cfg->details[0] != '\0')
    fprintf(fp, "<details>%s</details>\n", glob_app_cfg->details);
  if (glob_app_cfg->ast_ip.s_addr != -1)
    fprintf(fp, "<ast_ip>%s</ast_ip>\n", inet_ntoa(glob_app_cfg->ast_ip));

  if (glob_app_cfg->node_list) {
    for (curnode = glob_app_cfg->node_list->head;
	 curnode;
	 curnode = curnode->next) 
      dragon_node_pc_old_print(fp, (struct resource*)curnode->data, MASTER);
  }
  if (glob_app_cfg->link_list) {
    for (curnode = glob_app_cfg->link_list->head;
         curnode;
         curnode = curnode->next)
      dragon_link_old_print(fp, (struct resource*)curnode->data, MASTER);
  }
  fprintf(fp, "</topology>");
  fflush(fp);
  fclose(fp);
}

void
print_final_client(char *path)
{
  if (!glob_app_cfg->old_xml)
    print_final(path, MASTER);
  else 
    print_old_final(path);
}

int
master_validate_msg_type(struct application_cfg *old_cfg, 
			 struct application_cfg *new_cfg)
{
  int new_action, old_action;

  switch (new_cfg->action) {
    case setup_resp:
      old_action = setup_req;
      new_action = setup_resp;
      break;
    case release_resp:
      old_action = release_req;
      new_action = release_resp;
      break;
    case app_complete:
      old_action = ast_complete;
      new_action = app_complete;
      break;
    default:
      return 0;
  }

  if (old_cfg->action == old_action)
    old_cfg->action = new_action;

  if (old_cfg->action != new_action) {
    zlog_err("Invalid %s received, currently in %s",
	        action_type_details[new_action], 
		action_type_details[old_cfg->action]);
    return 1;
  }
  return 0;
}

int
master_gen_update(struct application_cfg *old_cfg, 
		  struct application_cfg *new_cfg, 
		  struct resource *old_res, 
		  struct resource *new_res)
{
  u_int32_t flag;
  u_int8_t *counter;

  switch(old_cfg->action) {
    case setup_resp: 
      flag = FLAG_SETUP_RESP;
      counter = &old_cfg->setup_ready;
      break;
    case app_complete: 
      flag = FLAG_APP_COMPLETE; 
      counter = &old_cfg->complete_ready;
      break;
    case release_resp: 
      flag = FLAG_RELEASE_RESP; 
      counter = &old_cfg->release_ready;
      break;
    default: 
      zlog_err("Wrong type in master_gen_update");
      return 1;
  }

  if (IS_RES_ACTION(old_res, flag))
    zlog_warn("action has been recieved, so this is an UPDATE");
  else if (new_res->status != ast_pending) {
    (*counter)++;
    old_res->flags |= flag;
  }

//  new_res->status = ast_success;
  old_res->status = new_res->status;
  if (old_res->agent_message) {
    free(old_res->agent_message);
    old_res->agent_message = NULL;
  }

  if (new_res->agent_message) {
    old_res->agent_message = new_res->agent_message;
    new_res->agent_message = NULL;
  } else if (new_cfg->details[0] != '\0')
    old_res->agent_message = strdup(new_cfg->details);
  if (old_res->status == ast_failure)
    old_cfg->status = ast_failure;

  // clean up socket
  if (old_res->minion_sock != -1) {
    thread_remove_read(master, minion_callback, NULL, old_res->minion_sock);
    close(old_res->minion_sock);
//    zlog_info("SOCK: closing minion_sock %d", old_res->minion_sock);
    old_res->minion_sock = -1;
  }

  return 0;
}

int
master_res_update(struct application_cfg *old_cfg, 
		  struct resource *old_res, 
		  struct resource *new_res)
{
  struct res_mod *mod;
  mod = old_res->subtype->mod;

  if (mod &&  mod->process_resp_func) 
    return mod->process_resp_func(old_cfg, old_res, new_res);

  if (old_res->res && new_res->res) {
    if (mod && mod->free_func) 
      mod->free_func(old_res->res);
    else
      free(old_res->res);
      
    old_res->res = new_res->res;
    new_res->res = NULL;
  } else if (!old_res->res && !new_res->res) {
    if (old_res->xml_node)
      xmlFreeNode(old_res->xml_node);
    old_res->xml_node = new_res->xml_node;
    new_res->xml_node = NULL;
  }

  return 0; 
}

void
master_save_file(struct application_cfg *new_cfg, 
		 struct resource *res)
{
  static char newpath[300] = "";

  switch (new_cfg->action) {
    case setup_resp:
      sprintf(newpath, "%s/%s/setup_resp_%s.xml",
	        AST_DIR, glob_app_cfg->ast_id, res->name);
      break;
    case app_complete:
      sprintf(newpath, "%s/%s/app_comp_%s.xml",
	        AST_DIR, glob_app_cfg->ast_id, res->name);
      break;
    case release_resp:
      sprintf(newpath, "%s/%s/rel_resp_%s.xml",
	        AST_DIR, glob_app_cfg->ast_id, res->name);
      break;
    default:
      return;
  }

  rename(AST_XML_RECV, newpath);
}

int
master_list_update(struct application_cfg *old_cfg,
		   struct application_cfg *new_cfg,
		   struct adtlist *res_list)
{
  struct adtlistnode *curnode;
  struct resource *old_res, *new_res;
  int ret_val = 0;

  if (!old_cfg || !res_list)
    return 0;

  for (curnode = res_list->head;
	curnode;
	curnode = curnode->next) {
    new_res = (struct resource*)curnode->data;
    old_res = search_res_by_name(glob_app_cfg, new_res->res_type, new_res->name);
       
    if (!old_res) {
      zlog_warn("The received %s %s is not found in the ast_id file",
		(new_res->res_type==res_node)?"node":"link", new_res->name);
      glob_app_cfg = new_cfg;
      continue;
    }
 
    ret_val += master_gen_update(glob_app_cfg, new_cfg, old_res, new_res);
    ret_val += master_res_update(glob_app_cfg, old_res, new_res);
    master_save_file(new_cfg, new_res);
  }
 
  return ret_val;
}

int
master_process_resp()
{
  struct application_cfg *new_cfg;
  int ret_val = 0;

  new_cfg = glob_app_cfg;

  /* search in the list first */
  glob_app_cfg = retrieve_app_cfg(new_cfg->ast_id, MASTER);

  if (!glob_app_cfg) {
    sprintf(glob_app_cfg->details, "didn't parse the ast_id file successfully");
    glob_app_cfg->status = ast_failure;
    glob_app_cfg = new_cfg;
    return 1;
  }

  if (new_cfg->link_list == NULL &&
	new_cfg->node_list == NULL) {
    zlog_warn("The received SETUP_RESP message has no resources");
    glob_app_cfg = new_cfg;
    return 1;
  }

  /* make sure that the receiving type of message is correct */
  if (master_validate_msg_type(glob_app_cfg, new_cfg)) {
    zlog_warn("Mismatch on the msg_type");
    glob_app_cfg = new_cfg;
    return 1;
  }

  /* now has to distinguish where this request is coming from,
   * node_agent or link_agent (dragond)
   */
  new_cfg->status = ast_success;
  ret_val += master_list_update(glob_app_cfg, new_cfg, new_cfg->node_list);
  ret_val += master_list_update(glob_app_cfg, new_cfg, new_cfg->link_list);
  if (glob_app_cfg->action == setup_resp)
    gettimeofday(&(glob_app_cfg->start_time), NULL);

  free_application_cfg(new_cfg);
  integrate_result();

  return ret_val;  
}

int
master_process_setup_req()
{
  static char directory[300];
  static char newpath[300];
  
  /* pre-processing */
  strcpy(directory, AST_DIR);
  if (mkdir(directory, 0755) == -1 && errno != EEXIST) {
   zlog_err("Can't create directory %s", directory);
   return 0;
  }

  sprintf(directory+strlen(directory), "/%s", glob_app_cfg->ast_id);
  if (mkdir(directory, 0755) == -1) {
    if (errno == EEXIST) {
      set_allres_fail("ast_id already exist");
      return 0; 
    } else {
      zlog_err("Can't create the directory: %s; error = %d(%s)",
		directory, errno, strerror(errno));
      return 0;
    }
  }
  sprintf(newpath, "%s/setup_original.xml", directory);
  if (rename(AST_XML_RECV, newpath) == -1) 
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	     AST_XML_RECV, newpath, errno, strerror(errno));

  app_cfg_pre_req();
  init_socket(glob_app_cfg);
  glob_app_cfg->flags |= FLAG_SETUP_REQ;
  gettimeofday(&(glob_app_cfg->start_time), NULL);
  if (glob_app_cfg->link_list && send_task_to_minions(glob_app_cfg->link_list, res_link))
    glob_app_cfg->status = ast_failure;
  else
    glob_app_cfg->status = ast_pending;

  integrate_result();

  return 1;
}

int
master_process_query_req()
{
  static char directory[300];
  static char newpath[300];
  
  glob_app_cfg->ast_id = generate_ast_id(ID_QUERY);

  strcpy(directory, AST_DIR);
  mkdir(directory, 0755);

  sprintf(directory+strlen(directory), "/%s", glob_app_cfg->ast_id);
  if (mkdir(directory, 0755) == -1) {
    if (errno == EEXIST) {
      if (remove(directory) == -1) {
	zlog_err("Can't remove the directory: %s", directory);
	return 0;
      }
    } else {
      zlog_err("Can't create the directory: %s; error = %d(%s)",
		directory, errno, strerror(errno));
      return 0;
    }
  }

  sprintf(newpath, "%s/%s/query_original.xml", AST_DIR, glob_app_cfg->ast_id);
  if (rename(AST_XML_RECV, newpath) == -1) 
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	     AST_XML_RECV, newpath, errno, strerror(errno));

  glob_app_cfg->status = ast_success;
  if (send_task_to_minions(glob_app_cfg->node_list, res_node))
    glob_app_cfg->status = ast_failure;
  if (send_task_to_minions(glob_app_cfg->link_list, res_link)) 
    glob_app_cfg->status = ast_failure;

  glob_app_cfg->action = query_resp;
  integrate_result();

  return 1;
}
 
int 
master_process_release_req()
{
  static char path[300];
  struct application_cfg *working_app_cfg;
  int file_mode = 0;

  if (glob_app_cfg->ast_id == NULL) {

    sprintf(glob_app_cfg->details, "ast_id is not set");
    if (glob_app_cfg->link_list == NULL &&
	glob_app_cfg->node_list == NULL) {
      zlog_err("no node or link in file; nothing to release");
      return 0;
    }

    glob_app_cfg->ast_id = generate_ast_id(ID_TEMP);
    file_mode = 1;
 
  } else if (strcasecmp(glob_app_cfg->ast_id, "all") == 0) {

    FILE *fp;
    struct adtlistnode *curnode;
    struct application_cfg *curcfg, *tempcfg = glob_app_cfg;
    
    glob_app_cfg = NULL;
    fp = fopen(AST_TEMP_FILE, "w+");
    fprintf(fp, "<topology action=\"release_resp\">\n");
    fprintf(fp, "<status>ast_success</status>\n");
    fprintf(fp, "<details>\n");
    zlog_info("release all request");
    if (app_list.count == 0) {
      zlog_info("no active ast; nothing will be released");
      fprintf(fp, "no active ast; nothing will be released");
    } else {

      for (curnode = app_list.head;
           curnode;
           curnode = curnode->next) {
        curcfg = (struct application_cfg*) curnode->data;
        zlog_info("Releasing ... %s [%s]", curcfg->ast_id, curcfg->xml_file);
        fprintf(fp, "Releaseing ... %s\n", curcfg->ast_id);
        release_ast(curcfg->ast_id);
      } 
    }
    fprintf(fp, "</details>\n</topology>\n");
    fflush(fp);
    fclose(fp);
    rename(AST_TEMP_FILE, AST_XML_RESULT);
    tempcfg->action = release_resp;
    tempcfg->status = ast_success;
    glob_app_cfg = tempcfg;
    return 0;

  } else {
    working_app_cfg = glob_app_cfg;
    
    /* search in the list first */
    glob_app_cfg = retrieve_app_cfg(working_app_cfg->ast_id, MASTER);

    if (!glob_app_cfg) {
      glob_app_cfg = working_app_cfg;
      sprintf(glob_app_cfg->details, "didn't parse the ast_id final file successfully");
      glob_app_cfg->status = ast_failure;
      return 0;
    }

    app_cfg_pre_req();

    if (!file_mode) {
      if (glob_app_cfg->action == release_resp ||
	  glob_app_cfg->action == release_req) {
	glob_app_cfg = working_app_cfg;
	set_allres_fail("ast_id has received release_req already");
	glob_app_cfg->status = ast_failure;
	return 0;
      }
    }
    glob_app_cfg->action = working_app_cfg->action;
  }

  if (file_mode) {
    char directory[300];

    strcpy(directory, AST_DIR);
    if (mkdir(directory, 0755) == -1 && errno != EEXIST) {
     zlog_err("Can't create directory %s", directory);
     return 0;
    }
  
    sprintf(directory+strlen(directory), "/%s", glob_app_cfg->ast_id);
    if (mkdir(directory, 0755) == -1) {
      if (errno == EEXIST) {
	if (remove(directory) == -1) {
	  zlog_err("Can't remove the directory: %s", directory);
	  return 0;
	}
      } else {
	zlog_err("Can't create the directory: %s; error = %d(%s)",
		  directory, errno, strerror(errno));
	return 0;
      }
    }
  }

  /* now, save the release_original first */
  sprintf(path, "%s/%s/release_original.xml", AST_DIR, glob_app_cfg->ast_id);
  if (rename(AST_XML_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	     AST_XML_RECV, path, errno, strerror(errno));

  app_cfg_pre_req();
  glob_app_cfg->flags |= FLAG_RELEASE_REQ;

  gettimeofday(&(glob_app_cfg->start_time), NULL);

  if (send_task_to_minions(glob_app_cfg->node_list, res_node)) 
    glob_app_cfg->status = ast_failure;
  if (send_task_to_minions(glob_app_cfg->link_list, res_link)) 
    glob_app_cfg->status = ast_failure;

  integrate_result();
  
  return 1;
}

int
master_process_topo(char* input_file)
{
  char system_call[300];

  if (strcasecmp(input_file, AST_XML_RECV) != 0) { 
    sprintf(system_call, "cp %s %s", input_file, AST_XML_RECV); 
    system(system_call);
  }

  /* after all the preparation, parse the application xml file 
   */ 
  if ((glob_app_cfg = old_topo_xml_parser(AST_XML_RECV, MASTER)) == NULL &&
      (glob_app_cfg = topo_xml_parser(AST_XML_RECV, MASTER)) == NULL) { 
    zlog_err("master_process_topo: topo_xml_parser() failed"); 
    return 0;
  }

  if (topo_validate_graph(MASTER, glob_app_cfg)) {
    sprintf(glob_app_cfg->details, "Failed at validation");
    zlog_err("master_process_topo: failed at validation");
    if (glob_app_cfg->action == setup_req)
      glob_app_cfg->action = setup_resp;
    else if (glob_app_cfg->action == release_req)
      glob_app_cfg->action = release_resp;

    return 0;
  }

#ifdef RESOURCE_BROKER
  if (glob_app_cfg->action == setup_req && master_locate_resource() == 0) {
    sprintf(glob_app_cfg->details, "Failed at locating unknown resource(s)");
    return 0;
  }
#endif

  if (glob_app_cfg->action == setup_req)
    glob_app_cfg->ast_id = generate_ast_id(ID_SETUP);

  zlog_info("Processing %s, %s",
		glob_app_cfg->ast_id,
		action_type_details[glob_app_cfg->action]);

  /* central point before ast_master calls any funcs to process xml file */
  if ( glob_app_cfg->action == setup_req) {
    glob_app_cfg->status = ast_success;
    master_process_setup_req();
  } else if ( glob_app_cfg->action == setup_resp)
    master_process_resp();
  else if (glob_app_cfg->action == release_req)
    master_process_release_req();
  else if ( glob_app_cfg->action == query_req)
    master_process_query_req();
  else if ( glob_app_cfg->action == app_complete) 
    master_process_resp();

  return 1;
}

int 
master_process_ctrl(char* filename)
{
  FILE *fp;

  fp = fopen(AST_XML_RESULT, "w+");
  
  fprintf(fp, "<ast_ctrl action=\"HELLO_RESP\">\n");
  fprintf(fp, "\t<msg>AST_MASTER, Version 1.0</msg>\n");
  fprintf(fp, "</ast_ctrl>\n");
 
  fflush(fp);
  fclose(fp);

  return 1;
}

void
process_client(char *str, char action)
{
  struct sockaddr_in astServ;
  int sock, fd, total;
  struct stat file_stat;
  FILE *fp;
  char *filename;

  unlink(AST_CLIENT_SEND_FILE);
  glob_app_cfg = NULL;
  zlog_default = openzlog ("astb", ZLOG_NOLOG, ZLOG_ASTB,
	                   LOG_CONS|LOG_NDELAY|LOG_PID, LOG_DAEMON);
  zlog_set_file(zlog_default, ZLOG_FILE, "/var/log/astb.log");

  if (action == 'r') {
    fp = fopen(AST_CLIENT_SEND_FILE, "w+");
    fprintf(fp, "<topology ast_id=\"%s\" action=\"release_req\"></topology>",
		str);
    fflush(fp);
    fclose(fp);
    filename = AST_CLIENT_SEND_FILE;

  } else if (action == 'v' || action == 'f') {

    if (stat(str, &file_stat) == -1) { 
      printf("Can't locate %s\n", str); 
      exit(1); 
    }

    switch (xml_parser(str)) {

    case TOPO_XML: 

      if ((glob_app_cfg = old_topo_xml_parser(str, ASTB)) == NULL &&
	  (glob_app_cfg = topo_xml_parser(str, ASTB)) == NULL) { 
	printf("Validation Failed\n"); 
	exit(1); 
      } 

      if (topo_validate_graph(ASTB, glob_app_cfg)) { 
	printf("Validation Failed\n"); 
	exit(1); 
      }
    
      if (action == 'v') { 
	printf("File passes valiation\n"); 
	exit (0); 
      }

      if (glob_app_cfg->action == setup_req) 
	strncpy(glob_app_cfg->xml_file, str, 100-1); 

      filename = str;
      /* rewrite the input file */
      if (glob_app_cfg) {
	if (glob_app_cfg->old_xml) 
	  print_old_final(AST_CLIENT_SEND_FILE);
	else 
	  print_final(AST_CLIENT_SEND_FILE, ASTB);
	filename = AST_CLIENT_SEND_FILE;
      }

      break;

#ifdef FIONA
    case ID_XML: 

      if ((glob_app_cfg = id_xml_parser(str, ASTB)) == NULL) { 
	printf("Validation Failed\n"); 
	exit(1); 
      } 
      break;

      if (action == 'v') {
	printf("File is validated\n"); 
	exit(1); 
      }
      break;
#endif

    case CTRL_XML:
      filename = str;
      break;

    default:
      printf("Invalid incoming file\n");
      exit(1);
    }
  }
      
  if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) { 
    printf("socket() failed\n"); 
    exit(1); 
  }

  memset(&astServ, 0, sizeof(astServ));
  astServ.sin_family = AF_INET;
  astServ.sin_addr.s_addr = inet_addr("127.0.0.1");
  astServ.sin_port = htons(MASTER_PORT);

  
  fd = open(filename, O_RDONLY);

  if (fd == -1) {
    printf("open() failed; err = %s\n", strerror(errno));
    exit(1);
  }

  if (connect(sock, (struct sockaddr*)&astServ, sizeof(astServ)) < 0)
  {
    printf("connect() failed\n");
    exit(1);
  }

#if defined(__APPLE__)
  total = sendfile(fd, sock, 0, 0, NULL, 0);
#elif defined( __FreeBSD__)
  total = sendfile(fd, sock, 0, 0, NULL, NULL, 0);
  printf("sendfile() returns %d\n", total);
#else
  if (fstat(fd, &file_stat) == -1) {
    printf("fstat() failed on %s\n", filename);
    exit(1);
  }
  total = sendfile(sock, fd, 0, file_stat.st_size);
#endif
  if (total < 0)
    printf("sendfile() failed; error = %d(%s)\n", errno, strerror(errno));
  else if (recv_file(sock, AST_CLIENT_RESULT_FILE, RCVBUFSIZE, 0, ASTB))
    printf("No confirmation from ast_master\n");

  close(sock);
  close(fd);
  exit(0);
}

/* ASP Masterd main routine */
int
main(int argc, char* argv[])
{
  char *progname;
  char *p, action;
  char *vty_addr = NULL;
  int daemon_mode = 0;
  struct thread thread;
  struct sockaddr_in servAddr;
  int servSock;
  char *config_file = NULL;
  struct sigaction myAlarmAction;
  int client_mode = 0;
  char *client_str;
  
  progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);

  while (1) {
    int opt;

    opt = getopt_long (argc, argv, "v:f:r:dlc:hA:P:", ast_master_opts, 0);
    if (argc > 4) {
      usage(progname, 1); 
      exit(EXIT_FAILURE); 
    }
 
    if (opt == EOF) 
      break; 

    switch (opt) { 
      case 0: 
	break; 
      case 'd': 
	daemon_mode = 1; 
	break; 
      case 'h': 
	usage(progname, 0);
	break;
      case 'c':
	config_file = optarg;
	break;
      case 'f':
	client_str = optarg;
	action = 'f';
	client_mode = 1;
	break;
      case 'v':
	client_str = optarg;
	action = 'v';
	client_mode = 1;
	break;
      case 'r':
	client_str = optarg;
	client_mode = 1;
	action = 'r';
	break;
      default: 
	usage (progname, 1);
    }
  }

  if (!client_mode) { 
    zlog_default = openzlog (progname, ZLOG_NOLOG, ZLOG_ASTB,
			   LOG_CONS|LOG_NDELAY|LOG_PID, LOG_DAEMON);
    zlog_set_file(zlog_default, ZLOG_FILE, "/var/log/ast_master.log");
  }

  memset(&vtag_pool, 0, sizeof(struct vtag_tank));
  memset(&narb_pool, 0, sizeof(struct narb_tank));
  memset(&es_pool, 0, sizeof(struct es_tank));
  memset(&agency, 0, 3*sizeof(struct resource_agent));

  /* Change to the daemon program. */
  if (daemon_mode)
    daemon (0, 0);

  master = thread_master_create();

  init_application_module();

  /* parse all service_template.xml and build the default
   * struct for each service template for later use
   */
  if (init_resource()) {
    zlog_err("There is no resource defined in this ast_master instance; exit ...");
    exit(0);
  }

  init_schema("/usr/local/ast_file/xml_schema/setup_req.rng");
  glob_app_cfg = NULL;
  memset(&app_list, 0, sizeof(struct adtlist));

  if (client_mode)
    process_client(client_str, action);

  /* print banner */
  zlog_info("AST_MASTER starts: pid = %d", getpid());

  if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    zlog_err("socket() failed");
    exit(EXIT_FAILURE);
  }
      
  memset(&servAddr, 0, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servAddr.sin_port = htons(MASTER_PORT);
    
  if (bind(servSock, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
    zlog_err("bind() failed");
    exit(EXIT_FAILURE);
  }

  if (listen(servSock, MAXPENDING) < 0) {
    zlog_err("listen() failed");
    exit(EXIT_FAILURE);
  }

  myAlarmAction.sa_handler = handle_alarm;
  if (sigfillset(&myAlarmAction.sa_mask) < 0) {
    zlog_err("main: sigfillset() failed");
    return 0;
  }
  myAlarmAction.sa_flags = 0;
  
  if (sigaction(SIGALRM, &myAlarmAction, 0) < 0) {
    zlog_err("main: sigaction() failed");
    return 0;
  }

  cmd_init(1);
  vty_init();
  master_supp_vty_init();
  sort_node();

  /* Get configuration file. */
  vty_read_config (config_file, config_current, config_default);

  vty_serv_sock(vty_addr, 2612, "/tmp/.master_vty");

  thread_add_read(master, master_accept, NULL, servSock);
  while (thread_fetch (master, &thread))
    thread_call (&thread);

  return (EXIT_SUCCESS);
}

int
master_compose_req(struct application_cfg *app_cfg, 
		  	    char *path, 
			    struct resource* res)
{
  FILE *fp;
  int compose_req_value = 1;

  if (!res || !app_cfg || !path || strlen(path) == 0) 
    return 1;

  if (res->res && res->subtype->mod && res->subtype->mod->compose_req_func)
    compose_req_value = res->subtype->mod->compose_req_func(path, app_cfg, res);

  if (compose_req_value != 1)
    return compose_req_value;

  fp = fopen(path, "w+");
  if (!fp) {
    zlog_err("Can't open the file %s; error = %d(%s)", 
		path, errno, strerror(errno));
    return 1;
  }
  
  fprintf(fp, "<topology ast_id=\"%s\" action=\"%s\">\n", app_cfg->ast_id, action_type_details[app_cfg->action]);

  print_res(fp, res, MASTER);
  fprintf(fp, "</topology>");
  fflush(fp);
  fclose(fp);

  return 0;
}

int
master_compose_broker_request(struct resource *myres, char* path)
{
  FILE *send_file;

  if (!myres)
    return 0;
   
  if (!path || strlen(path) == 0)
    return 0;

  send_file = fopen(path, "w+");
  if (!send_file) 
    return 0;

  fprintf(send_file, "<topology>\n");
  fprintf(send_file, "<resource type=\"%s\" name=\"%s\">\n",
		"FIONA", myres->name);
  fprintf(send_file, "</resource>\n"); 
  fprintf(send_file, "</topology>");
  fflush(send_file);
  fclose(send_file);

  return 1;
}

int
send_task_to_minions(struct adtlist *res_list, 
		     enum resource_type res_type)
{
  struct adtlistnode *curnode;
  struct resource *res;
  int sock, ready = 0, ret_value = 0;
  u_int16_t flags;
  static char buffer[RCVBUFSIZE];
  static char send_buf[SENDBUFSIZE];
  static char directory[300];
  static char newpath[300];
  static char path_prefix[300];
  char *type = (res_type == res_node)?"node":"link";

  if (!res_list)
    return 1;

  zlog_info("send_task_to_minions %s ...", type);
  strcpy(directory, AST_DIR);
  sprintf(directory+strlen(directory), "/%s", glob_app_cfg->ast_id);
  
  if (glob_app_cfg->action == setup_req) {
    flags = FLAG_SETUP_REQ;
    sprintf(path_prefix, "%s/setup_", directory);
  } else if (glob_app_cfg->action == release_req) {
    flags = FLAG_RELEASE_REQ;
    sprintf(path_prefix, "%s/release_", directory);
  } else if (glob_app_cfg->action == query_req) {
    flags = FLAG_QUERY_REQ;
    sprintf(path_prefix, "%s/query_", directory);
  } else if (glob_app_cfg->action == ast_complete) {
    flags = FLAG_AST_COMPLETE;
    sprintf(newpath, "%s/ast_complete.xml", directory);
    print_final(newpath, MASTER);
  } else {
    zlog_err("send_task_to_minions: invalid action");
    return 0;
  }

  memset(buffer, 0, RCVBUFSIZE);
  memset(send_buf, 0, SENDBUFSIZE);

  for (curnode = res_list->head;
       curnode && !(ready && glob_app_cfg->action == setup_req);
       curnode = curnode->next) {
    res = (struct resource*)(curnode->data); 

    /* if for some reason, this req has already sent out, do nothing */
    if (IS_RES_ACTION(res, flags)) {
      if (glob_app_cfg->action == setup_req)
	glob_app_cfg->setup_sent++;
      continue;
    }

    if (glob_app_cfg->action == release_req ||
	glob_app_cfg->action == ast_complete) {
      if (!IS_SET_SETUP_REQ(res) && IS_SET_SETUP_REQ(glob_app_cfg)) {
        res->status = ast_success;
	ready++;
	continue;
      }
    }

    if (glob_app_cfg->action != ast_complete) {
      sprintf(newpath, "%srequest_%s_%s.xml", path_prefix, type, res->name);
      if (master_compose_req(glob_app_cfg, newpath, res)) {
	ready++; 
	ret_value++;
	set_res_fail("problem encountered to compose the request to minion", res);
	continue;
      }
    }

    zlog_info("sending request to %s (%s:%d)", 
		res->name, inet_ntoa(res->ip), res->subtype->agent_port);
    sock = send_file_to_agent(inet_ntoa(res->ip), res->subtype->agent_port, newpath);
    if (sock == -1)  {
      ready++;
      ret_value++;
      set_res_fail("problem encountered to connect to the minion", res);
      res->status = ast_failure;
      continue;
    } else {
      res->status = ast_pending,
      res->flags |= flags;
    }

// FIONA
/* Assumption here is that link_agent can have > 1 resource requests on 1 ast_id,
 * while node_agent will only have 1
 * when it's the case of setup_req, if we listen to a socket and resceive 1 resp,
 * default is to close that particular socket and then, the subsequent attempt using 
 * that socket will failed.
 * but b/c of the implementation of tcp socket, the link_agent end will not know that
 * the other side of the socket has been closed when trying to use it.
 * thus, when it's case of res_link, we don't add the socket to minion_callback if it's
 * setup_req
 */
    if ((glob_app_cfg->action != ast_complete && res_type == res_node) || 
 	(glob_app_cfg->action != setup_req && res_type == res_link)) { 
      zlog_info("SOCK: %d added for minion_callback", sock); 
      thread_add_read(master, minion_callback, NULL, sock); 
      if (res->minion_sock != -1) { 
	thread_remove_read(master, minion_callback, NULL, res->minion_sock); 
	close(res->minion_sock); 
//	zlog_info("SOCK: closing minion_sock %d", res->minion_sock); 
	res->minion_sock = -1;
      } 
      res->minion_sock = sock; 
    } else  
      close(sock); 

    if (glob_app_cfg->action == setup_req) 
      glob_app_cfg->setup_sent++;
  }

  if (glob_app_cfg->action == setup_req)
    glob_app_cfg->setup_ready += ready;
  else if (glob_app_cfg->action == release_req)
    glob_app_cfg->release_ready += ready;
  else if (glob_app_cfg->action == app_complete)
    glob_app_cfg->complete_ready += ready;
    
  return ret_value;
}

void 
integrate_result()
{
  static char directory[300];
  static char newpath[300];
  static char path_prefix[300];
  int sendtask_ret;

  /* first, save the cur cfg into final.xml */
  sprintf(directory, "%s/%s", AST_DIR, glob_app_cfg->ast_id);
  if (glob_app_cfg->action != query_resp) { 
    sprintf(newpath, "%s/final.xml", directory); 
    print_final(newpath, MASTER);
  }
  add_cfg_to_list();
  symlink(newpath, AST_XML_RESULT);

  switch (glob_app_cfg->action) {
    case setup_resp:
      sprintf(path_prefix, "%s/setup_", directory);
      zlog_info("setup_sent = %d; setup_ready = %d", glob_app_cfg->setup_sent, glob_app_cfg->setup_ready);

      if (glob_app_cfg->status == ast_failure)
	break;

      if (glob_app_cfg->setup_ready == adtlist_getcount(glob_app_cfg->link_list)) {

	print_final(newpath, MASTER); 
        glob_app_cfg->action = setup_req;
        sendtask_ret = send_task_to_minions(glob_app_cfg->node_list, res_node);
        glob_app_cfg->action = setup_resp; 
	if (sendtask_ret) 
	  glob_app_cfg->status = ast_failure; 
	if (glob_app_cfg->status == ast_pending)
	  return;

      } else if (glob_app_cfg->setup_ready < glob_app_cfg->setup_sent)
	return;

      break;

    case release_req:
      if (glob_app_cfg->release_ready == glob_app_cfg->total) {
	glob_app_cfg->action = release_resp;
	sprintf(path_prefix, "%s/release_", directory);
	zlog_info("No need to send any release_req");
      } else
	return;

      break;

    case release_resp:
      zlog_info("release_ready: %d, total: %d", 
		glob_app_cfg->release_ready, glob_app_cfg->total);
      if (glob_app_cfg->release_ready == glob_app_cfg->total) 
	sprintf(path_prefix, "%s/release_", directory);
      else
	return;
      break;
  
    case query_resp:
      sprintf(path_prefix, "%s/query_", directory);
      break;

    case app_complete:
      if (glob_app_cfg->total == glob_app_cfg->complete_ready) {
	zlog_info("All minions are done with this AST, resources are ready to be released");
	glob_app_cfg->status = ast_success;
	return;
      }
      break;

    case setup_req:
      sprintf(path_prefix, "%s/setup_", directory);

      if (!glob_app_cfg->link_list) {

        print_final(newpath, MASTER);
        sendtask_ret = send_task_to_minions(glob_app_cfg->node_list, res_node);
        if (sendtask_ret)
          glob_app_cfg->status = ast_failure;
        else 
          return;
      }

      break;
    default:
      return;
  }
  sprintf(newpath, "%sfinal.xml", path_prefix);
  print_final(newpath, MASTER);

  if (glob_app_cfg->status == ast_pending)
    glob_app_cfg->status = ast_success;
  /* send the result back to user */

  if (glob_app_cfg->clnt_sock != -1) {
    if (glob_app_cfg->action == setup_resp)
      glob_app_cfg->flags |= FLAG_SETUP_RESP;
    else 
      glob_app_cfg->flags |= FLAG_RELEASE_RESP;

    unlink(AST_XML_RESULT);
    print_final_client(AST_XML_RESULT);
    if (send_file_over_sock(glob_app_cfg->clnt_sock, AST_XML_RESULT))
      zlog_err("Failed to send the result back to client");
    close(glob_app_cfg->clnt_sock);
    glob_app_cfg->clnt_sock = -1;
  }

  /* see if need to send out AST_COMPLETE */
  if (glob_app_cfg->status == ast_success && 
      glob_app_cfg->action == setup_resp) {

    glob_app_cfg->action = ast_complete;
    zlog_info("Received success from ALL in setup_req; now, do ast_complete");

    send_task_to_minions(glob_app_cfg->node_list, res_node);
    send_task_to_minions(glob_app_cfg->link_list, res_link);
    glob_app_cfg->status = ast_pending;
  }

  if (glob_app_cfg->action == release_resp &&
      glob_app_cfg->release_ready == glob_app_cfg->total) {
    clean_socket(glob_app_cfg);
    del_cfg_from_list(glob_app_cfg);
    glob_app_cfg = NULL;
  }
}

/* let ast_masterd to listen on a certain tcp port;
 */
static int
master_accept(struct thread *thread)
{
  int servSock, clntSock;
  struct sockaddr_in clntAddr;
  unsigned int clntLen;
  struct stat sb;
  int fd;

  alarm(0);
  servSock = THREAD_FD(thread);

  clntLen = sizeof(clntAddr);
  unlink(AST_XML_RESULT);
  unlink(AST_XML_RECV);
    
  if ((clntSock = accept(servSock, (struct sockaddr*)&clntAddr, &clntLen)) < 0) {
    zlog_err("master_accept: accept() failed");
    exit(EXIT_FAILURE);
  }

  zlog_info("master_accept(): START; fd: %d", clntSock);
  if (recv_file(clntSock, AST_XML_RECV, 4000, TIMEOUT_SECS, MASTER)) {
    zlog_info("master_accept(): recv error");
    close(clntSock);
    thread_add_read(master, master_accept, NULL, servSock);
    return 1;
  }
    
  glob_app_cfg = NULL;
  zlog_info("Handling client %s ...", inet_ntoa(clntAddr.sin_addr));

  switch(xml_parser(AST_XML_RECV)) {

    case TOPO_XML:

      zlog_info("XML_TYPE: TOPO_XML");
      if (!master_process_topo(AST_XML_RECV)) {
	if (!glob_app_cfg)
	  print_error_response(AST_XML_RESULT);
	else
	  glob_app_cfg->status = ast_failure;
	  
	break;
      } else {
	/* supposedly at this point, AST_XML_RECV should exist */
	fd = open(AST_XML_RESULT, O_RDONLY);
	if (fd == -1)
	  print_final(AST_XML_RESULT, MASTER);
	close(fd);
      }

      if (glob_app_cfg && glob_app_cfg->details[0] != '\0')
	zlog_err(glob_app_cfg->details);

      /* if at this point, there is already error(s) in 
       * processing, no need to wait for ALL result back before
       * sending updates to user
       */
      if (glob_app_cfg) {
	if ((glob_app_cfg->action == setup_req ||
	  glob_app_cfg->action == release_req) &&
	  (glob_app_cfg->status == ast_success ||
	   glob_app_cfg->status == ast_pending)) {
	  glob_app_cfg->clnt_sock = clntSock;
	}
      }

      break;

    case ID_XML:

      zlog_info("XML_TYPE: ID_XML");
      master_process_id(AST_XML_RECV);
      free_application_cfg(glob_app_cfg);
      glob_app_cfg = NULL;

      break;

    case ID_QUERY_XML:

      zlog_info("XML_TYPE: ID_QUERY_XML");
      master_process_id(AST_XML_RECV);
      free_application_cfg(glob_app_cfg);
      glob_app_cfg = NULL;
     
      break;
    case CTRL_XML:
      zlog_info("XML_TYPE: CTRL_XML");
      master_process_ctrl(AST_XML_RECV);
   
      break;
   
    default:

      zlog_info("XML_TYPE: can't be determined");
  }

  /* Here, we will send something back no matter unless the 
   * clntSock has been saved in glob_app_cfg to be used later
   * if there is error in sending file out, it's ok because sometimes
   * the minions doesn't expect any reply from ast_master
   */
  if (!glob_app_cfg) {

    /* CASE:
     * we simply have no glob_app_cfg pointer; fundamental error
     */
    if (stat(AST_XML_RESULT, &sb) == -1)
      print_error_response(AST_XML_RESULT);

    send_file_over_sock(clntSock, AST_XML_RESULT);
    close(clntSock);
//    zlog_info("SOCK: closing clntSock %d", clntSock);
  } else if (strcasecmp(glob_app_cfg->ast_id, "all") == 0) {

    /* CASE: 
     * when user is calling to release ALL ast 
     */
    send_file_over_sock(clntSock, AST_XML_RESULT);
    close(clntSock);
    zlog_info("SOCK: closing clntSock %d", clntSock);
    glob_app_cfg->clnt_sock = -1;
    zlog_info("master_accept(): DONE");
    thread_add_read(master, master_accept, NULL, servSock);
    free_application_cfg(glob_app_cfg);

    return 1;
  } else if (glob_app_cfg->clnt_sock == -1 || 
		glob_app_cfg->status == ast_failure) {

    /* CASE:
     * when user haven't received any resp from minion on the req
     * but it already encounter error, so ast_failure
     */
    if (glob_app_cfg->action == setup_req) {
      glob_app_cfg->action = setup_resp;
      glob_app_cfg->flags |= FLAG_SETUP_RESP;
    } else if (glob_app_cfg->action == release_req) {
      glob_app_cfg->action = release_resp;
      glob_app_cfg->flags |= FLAG_RELEASE_RESP;
    } 
    unlink(AST_XML_RESULT); 
    print_final_client(AST_XML_RESULT);

    send_file_over_sock(clntSock, AST_XML_RESULT);
    close(clntSock);
    glob_app_cfg->clnt_sock = -1;
  } else if (glob_app_cfg->clnt_sock == -1 || 
	     clntSock != glob_app_cfg->clnt_sock) {

    /* CASE:
     * when clnt_sock has been saved to be used later
     */
    close(clntSock);
  }

  if (glob_app_cfg && glob_app_cfg != search_cfg_in_list(glob_app_cfg->ast_id)) 
    free_application_cfg(glob_app_cfg);
  glob_app_cfg = NULL;

  zlog_info("master_accept(): DONE; fd: %d", clntSock);

  thread_add_read(master, master_accept, NULL, servSock);

  master_check_app_list();
  return 1;
}

static int 
minion_callback(struct thread *thread)
{
  int servSock;
  int ret_value = 1;

  alarm(0);
  servSock = THREAD_FD(thread);
  unlink(AST_XML_RECV);

  zlog_info("minion_callback(): START; fd: %d", servSock);

  if (recv_file(servSock, AST_XML_RECV, RCVBUFSIZE, TIMEOUT_SECS, MASTER)) {
    ret_value = 0;
  } else {
      
    if ((glob_app_cfg = topo_xml_parser(AST_XML_RECV, MASTER)) == NULL) {
      zlog_err("received file is not parsed correctly, ignore ...");
      ret_value = 0;
    } else if (topo_validate_graph(MASTER, glob_app_cfg)) {
      zlog_err("received file is invalid, ignore ...");
      ret_value = 0;
    } else if (!glob_app_cfg->node_list && !glob_app_cfg->link_list) {
      zlog_err("received file has no resource in it, ignore ...");
      ret_value = 0;
    } else {
      zlog_info("Processing %s, %s",
		glob_app_cfg->ast_id, 
		action_type_details[glob_app_cfg->action]);

      if (glob_app_cfg->action == setup_resp ||
	  glob_app_cfg->action == app_complete ||
	  glob_app_cfg->action == release_resp ||
	  glob_app_cfg->action == query_resp) 
 	master_process_resp();
      else {
	zlog_err("Invalid action %s sent from node_agent",
		action_type_details[glob_app_cfg->action]);
	free(glob_app_cfg);
	glob_app_cfg = NULL;
	ret_value = 0;
      }
    }
  }

  close(servSock);
  master_check_app_list();
  zlog_info("minion_callback(): DONE; fd: %d", servSock);

  return ret_value;
}

#ifdef RESOURCE_BROKER
int 
master_locate_resource()
{
  struct adtlistnode *curnode;
  struct resource *myres, *newres;
  int sock;
  char buffer[501];
  int bytesRcvd;
  FILE *fp;
  struct application_cfg *working_app_cfg; 
  struct resource_agent *agent;

  unlink(BROKER_SEND);
  unlink(BROKER_RECV);

  if (!glob_app_cfg->node_list)
    return 0;

  for ( curnode = glob_app_cfg->node_list->head;
	curnode;
	curnode = curnode->next) {
    myres = (struct resource*)(curnode->data);

    if (!IS_RES_UNFIXED(myres))
      continue;

    zlog_info("Trying to locate node resource %s", myres->name);
    agent = &agency[myres->stype];
    if (!agent->add || !agent->port) {
      set_res_fail("Can't find the agency", myres);
      return 0;
    }

    if (!master_compose_broker_request(myres, BROKER_SEND)) {
      set_res_fail("Fail to compose broker request", myres);
      return 0;
    }
    sock = send_file_to_agent(agent->add, agent->port, BROKER_SEND);

    if (sock == -1) {
      set_res_fail("Error in connecting the broker", myres);
      return 0;
    }

    if ((bytesRcvd = recv(sock, buffer, 500, 0))  > 0) {
      buffer[bytesRcvd] = '\0';
      fp = fopen(BROKER_RECV, "w+");
      fprintf(fp, buffer);
      fflush(fp);
      fclose(fp);
      close(sock);
      if ((working_app_cfg = topo_xml_parser(BROKER_RECV, BRIEF_VERSION)) == NULL) { 
	set_res_fail("Broker resource can't be parsed successfully", myres);
	return 0;
      } else if (!working_app_cfg->node_list) {
	set_res_fail("Return file doesn't have any node", myres);
	return 0;
      }
     
      newres = (struct resource*) working_app_cfg->node_list->head->data; 
      if (newres->ip.s_addr == -1 ){
	 // newres->router_id[0] == '\0' ||
	 // newres->tunnel[0] == '\0') { 
	set_res_fail("Invalid response from resource broker", myres);
	return 0;
      }

      strncpy(myres->ip, newres->ip, IP_MAXLEN);
      // strncpy(myres->router_id, newres->router_id, IP_MAXLEN);
      // strncpy(myres->tunnel, newres->tunnel, 9);
      free(working_app_cfg);
    } else {
      set_res_fail("No response from resource broker", myres);
      close(sock);
      return 0;
    }
  }

  return 1;
}
#endif

static void
master_check_app_list()
{
  struct adtlistnode *curnode;
  struct application_cfg *app_cfg, *cur_cfg;
  char newpath[105];
  struct timeval curr_time;
  unsigned int next_alarm = 0;
  long time_escape;
  unsigned int client_timeout = 0;

  gettimeofday(&curr_time, NULL);

  if (adtlist_getcount(&app_list) == 0)
    return;

  for (curnode = app_list.head;
	curnode;
	curnode = curnode->next) {
    app_cfg = (struct application_cfg*) curnode->data;
    if (app_cfg->link_list)
      client_timeout = CLIENT_TIMEOUT + ((app_cfg->link_list->count)/10) * CLIENT_TIMEOUT;
    else
      client_timeout = CLIENT_TIMEOUT;

    time_escape = curr_time.tv_sec - app_cfg->start_time.tv_sec;
    if (time_escape < client_timeout) {
      if (next_alarm == 0)
	next_alarm = client_timeout - time_escape;
      else if ((client_timeout - time_escape) < next_alarm)
	next_alarm = client_timeout - time_escape; 
      continue; 
    }

    if (IS_SET_RELEASE_REQ(app_cfg) && !IS_SET_RELEASE_RESP(app_cfg)) {

      zlog_info("master_check_app_list(): sending release_resp for %s", app_cfg->ast_id);
      app_cfg->flags |= FLAG_RELEASE_RESP;
      app_cfg->action = release_resp;
      app_cfg->status = ast_failure;
      strcpy(app_cfg->details, "didn't receive all release_resp");

      sprintf(newpath, "%s/%s/release_final.xml", AST_DIR, app_cfg->ast_id);
      cur_cfg = glob_app_cfg;
      glob_app_cfg = app_cfg;
      print_final(newpath, MASTER);
      sprintf(newpath, "%s/%s/final.xml", AST_DIR, app_cfg->ast_id);
      print_final(newpath, MASTER);
      print_final_client(AST_XML_RESULT);

      glob_app_cfg = cur_cfg;

      if (send_file_over_sock(app_cfg->clnt_sock, AST_XML_RESULT))
	zlog_err("Failed to send the result back to client");
      close(app_cfg->clnt_sock);
      app_cfg->clnt_sock = -1;

      break;
    } else if (IS_SET_SETUP_REQ(app_cfg) && !IS_SET_SETUP_RESP(app_cfg)) {

      zlog_info("master_check_app_list(): sending setup_resp for %s", app_cfg->ast_id);
      app_cfg->flags |= FLAG_SETUP_RESP;
      app_cfg->action = setup_resp;
      strcpy(app_cfg->details, "didn't receive all setup_resp");

      sprintf(newpath, "%s/%s/setup_final.xml", AST_DIR, app_cfg->ast_id);
      glob_app_cfg = app_cfg;
      print_final(newpath, MASTER);
      sprintf(newpath, "%s/%s/final.xml", AST_DIR, app_cfg->ast_id);
      print_final(newpath, MASTER);
      print_final_client(AST_XML_RESULT);
      glob_app_cfg = NULL;

      if (send_file_over_sock(app_cfg->clnt_sock, AST_XML_RESULT))
	zlog_err("Failed to send the result back to client");
      close(app_cfg->clnt_sock);
      app_cfg->clnt_sock = -1;
    }
  }

  if (app_cfg->action == release_resp && app_cfg->clnt_sock == -1) 
    del_cfg_from_list(app_cfg);

/*
  if (next_alarm)
    zlog_info("alarm(): %d", next_alarm);
*/
  alarm(next_alarm);

  return;
}

static void
handle_alarm()
{
  zlog_info("Received: SIGALARM");
  if (!master_recv_alarm) 
    master_check_app_list();
}

