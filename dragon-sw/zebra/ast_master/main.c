#include <zebra.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#ifndef __FreeBSD__
#include <sys/sendfile.h>
#endif
#include "vty.h"
#include "ast_master_ext.h"
#include "ast_master.h"

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

#ifdef RESOURCE_BROKER
#define BROKER_SEND     "/tmp/broker_send.xml"
#define BROKER_RECV     "/tmp/broker_resp.xml"
#endif

#define CLIENT_TIMEOUT	60
#define SQL_RESULT	"/tmp/mysql.result"

/* Configuration filename and directory. */
char config_current[] = MASTER_DEFAULT_CONFIG;
char config_default[] = SYSCONFDIR MASTER_DEFAULT_CONFIG;

struct thread_master *master; /* master = dmaster.master */
extern char *status_type_details[];
extern char *action_type_details[];
extern char *node_stype_name[];

static int master_accept(struct thread *);
static int noded_callback(struct thread *);
static int dragon_callback(struct thread *);
static void clean_socket(struct application_cfg *);
static void init_socket(struct application_cfg *);
static int master_setup_req_to_node();
static void master_check_app_list();
static void handle_alarm();
static int master_lookup_assign_ip();
static int master_cleanup_assign_ip();
static void master_check_command();
extern int master_process_id(char*);
extern struct application_cfg* master_final_parser(char*, int);
extern int send_file_to_agent(char *, int, char *);

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
static int recv_alarm = 0;

/* structurs defined for the resource agency
 *
 */
struct resource_agent {
  char *add;
  int  port;
};
static struct resource_agent agency[NUM_NODE_TYPE+1]; 

int master_process_release_req();

#define MAXPENDING      12
#define AST_XML_RECV	"/usr/local/ast_master_recv.xml"

int send_task_to_link_agent();
int send_task_to_node_agent();
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
\n", progname); 
    
  exit (status);
}

static 
void clean_socket(struct application_cfg *app_cfg)
{
  struct adtlistnode *curnode;
  struct resource *node;

  zlog_info("clean_socket(): start");
  if (!app_cfg || !app_cfg->node_list)
    return;

  for (curnode = app_cfg->node_list->head;
	curnode;
	curnode = curnode->next) {
    node = (struct resource*)curnode->data;

    if (node->dragon_sock != -1) {
      thread_remove_read(master, dragon_callback, NULL, node->dragon_sock);
      close(node->dragon_sock);
      zlog_info("SOCK: closing dragon_sock %d", node->dragon_sock);
    }
    if (node->noded_sock != -1) {
      thread_remove_read(master, noded_callback, NULL, node->noded_sock);
      close(node->noded_sock);
      zlog_info("SOCK: closing noded_sock %d", node->noded_sock);
    }
  }
  zlog_info("clean_socket(): end");
}

static 
void init_socket(struct application_cfg *app_cfg)
{
  struct adtlistnode *curnode;
  struct resource *node;

  if (!app_cfg || !app_cfg->node_list)
    return;

  for (curnode = app_cfg->node_list->head;
	curnode;
	curnode = curnode->next) {
    node = (struct resource*)curnode->data;

    node->dragon_sock = -1;
    node->noded_sock = -1;
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

void
print_final_client(char *path)
{
  struct adtlistnode *curnode;
  struct resource *mynode, *mylink;
  int i;
  FILE *file;
  
  if (!path || !glob_app_cfg) {
    zlog_err("print_final_client: either file path or glob_app_cfg is NULL");
    return;
  }

  file = fopen(path, "w+");
  if (!file)
    return;

  if (glob_app_cfg->action != SETUP_RESP && glob_app_cfg->action != RELEASE_RESP) 
    zlog_warn("print_final_client: should only be called for SETUP_RESP or RELEASE_RESP");

  if (glob_app_cfg->action == SETUP_REQ)
    glob_app_cfg->action = SETUP_RESP;
  else if (glob_app_cfg->action == RELEASE_REQ)
    glob_app_cfg->action = RELEASE_RESP;

  fprintf(file, "<topology ast_id=\"%s\" action=\"%s\">\n",  glob_app_cfg->ast_id, action_type_details[glob_app_cfg->action]);

  fprintf(file, "<status>%s</status>\n", status_type_details[glob_app_cfg->status]);

  if (glob_app_cfg->details[0] != '\0') 
    fprintf(file, "<details>%s</details>\n", glob_app_cfg->details);
 
  if (glob_app_cfg->node_list) { 
    for ( i = 1, curnode = glob_app_cfg->node_list->head;
	  curnode;  
	  i++, curnode = curnode->next) {
      mynode = (struct resource*)(curnode->data);

      fprintf(file, "<resource type=\"%s\" name=\"%s\">\n",
		node_stype_name[mynode->res.n.stype], mynode->name); 
      if (mynode->res.n.stype != vlsr && mynode->status) 
        fprintf(file, "\t<status>%s</status>\n", status_type_details[mynode->status]);
      if (mynode->agent_message) 
	fprintf(file, "\t<agent_message>%s</agent_message>\n", mynode->agent_message);
      if (mynode->res.n.ip[0] != '\0') 
	fprintf(file, "\t<ip>%s</ip>\n", mynode->res.n.ip); 
      if (mynode->res.n.command) 
	fprintf(file, "\t<command>%s</command>\n", mynode->res.n.command);
      fprintf(file, "</resource>\n");
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

int
master_process_setup_resp()
{
  struct application_cfg *working_app_cfg;
  struct adtlistnode *curnode1, *curnode2; 
  struct if_ip *glob_ifp, *work_ifp;
  struct resource *work_res_cfg, *glob_res_cfg, *srcnode;
  char newpath[100];

  if (!glob_app_cfg->ast_id) {
    sprintf(glob_app_cfg->details, "For SETUP_RESP, ast_id has to be set");
    return 0;
  }

  working_app_cfg = glob_app_cfg;

  /* search in the list first */
  glob_app_cfg = retrieve_app_cfg(working_app_cfg->ast_id, MASTER);

  if (!glob_app_cfg) {
    glob_app_cfg = working_app_cfg;
    sprintf(glob_app_cfg->details, "didn't parse the ast_id file successfully");
    glob_app_cfg->status = AST_FAILURE;
    return 0;
  }

  if (glob_app_cfg->action == SETUP_REQ) 
    glob_app_cfg->action = SETUP_RESP;

  if (glob_app_cfg->action != SETUP_RESP) {
    zlog_err("Invalid SETUP_RESP received, currently in %s", 
		action_type_details[glob_app_cfg->action]);
    glob_app_cfg = working_app_cfg;
    return 0;
  }

  if (working_app_cfg->link_list == NULL &&
	working_app_cfg->node_list == NULL) {
    zlog_warn("The received SETUP_RESP message has no resources");
    glob_app_cfg = working_app_cfg;
    return 0;
  }

  /* now has to distinguish where this request is coming from,
   * node_agent or link_agent (dragond)
   */
  if (working_app_cfg->link_list == NULL) {

    work_res_cfg = (struct resource*)working_app_cfg->node_list->head->data;
    glob_res_cfg = search_node_by_name(glob_app_cfg, work_res_cfg->name);

    /* counter update */
    if (IS_SET_SETUP_RESP(glob_res_cfg)) 
	zlog_warn("SETUP_RESP has been recieved, so this is an UPDATE"); 
    else { 
	glob_app_cfg->setup_ready++; 
	glob_res_cfg->flags |= FLAG_SETUP_RESP; 
    }

    /* status update */ 
    glob_res_cfg->status = work_res_cfg->status;
    if (glob_res_cfg->agent_message) { 
      free(glob_res_cfg->agent_message); 
      glob_res_cfg->agent_message = NULL; 
    }

    if (work_res_cfg->agent_message) {
      glob_res_cfg->agent_message = work_res_cfg->agent_message;
      work_res_cfg->agent_message = NULL;
    } else if (working_app_cfg->details[0] != '\0')
      glob_res_cfg->agent_message = strdup(working_app_cfg->details);
    if (glob_res_cfg->status != AST_SUCCESS)
	glob_app_cfg->status = AST_FAILURE;

    /* parameters update */
    if (glob_res_cfg->res.n.if_list && work_res_cfg->res.n.if_list) 
      for (curnode1 = glob_res_cfg->res.n.if_list->head,
	  curnode2 = work_res_cfg->res.n.if_list->head;
	  curnode1 && curnode2;
	  curnode1 = curnode1->next, curnode2 = curnode2->next) {
	glob_ifp = (struct if_ip*)curnode1->data;
	work_ifp = (struct if_ip*)curnode2->data;

	if (glob_ifp->iface)
	  free(glob_ifp->iface);
	glob_ifp->iface = work_ifp->iface;
	work_ifp->iface = NULL;
    }
   
    sprintf(newpath, "%s/%s/setup_node_response_%s.xml",
	AST_DIR, glob_app_cfg->ast_id, glob_res_cfg->name);
    glob_res_cfg->noded_sock = -1;

    zlog_info("Node: %s %s", glob_res_cfg->name, status_type_details[glob_res_cfg->status]);

  } else {
    srcnode = (struct resource*)working_app_cfg->node_list->head->data;
    zlog_info("From link_agent %s", srcnode->name);

    glob_res_cfg = search_node_by_name(glob_app_cfg,  srcnode->name);
    if (!glob_res_cfg) 
      zlog_err("Can't find this link_agent %s in %s", srcnode->name, glob_app_cfg->ast_id);
    else {

      srcnode = glob_res_cfg;
      if (adtlist_getcount(glob_res_cfg->res.n.link_list) == 
	  adtlist_getcount(working_app_cfg->link_list)) {
	for (curnode1 = glob_res_cfg->res.n.link_list->head,
	     curnode2 = working_app_cfg->link_list->head;
	     curnode1 && curnode2;
	     curnode1 = curnode1->next, curnode2 = curnode2->next) {
	  glob_res_cfg = (struct resource*) curnode1->data;
	  work_res_cfg = (struct resource*) curnode2->data;
  
	  if (strcmp(glob_res_cfg->name, work_res_cfg->name) != 0) {
	    zlog_warn("Something wrong with the link ordering");
	    continue;
	  }
  
	  zlog_info("Link: %s, %s, LSP: %s vtag: %s", 
	  	work_res_cfg->name, 
	  	status_type_details[work_res_cfg->status],
	  	work_res_cfg->res.l.lsp_name,
		work_res_cfg->res.l.vtag);
  
	  /* counter update */
	  if (work_res_cfg->status != AST_PENDING) {
	    if (IS_SET_SETUP_RESP(glob_res_cfg)) 
	      zlog_warn("SETUP_RESP has been recieved, so this is an update");
	    else { 
	      glob_app_cfg->setup_ready++; 
	      glob_res_cfg->flags |= FLAG_SETUP_RESP; 
	    }
	  }
	
	  /* status update */ 
	  glob_res_cfg->status = work_res_cfg->status; 
	  glob_res_cfg->res.l.l_status = work_res_cfg->res.l.l_status;

	  if (glob_res_cfg->agent_message) { 
	    free(glob_res_cfg->agent_message); 
	    glob_res_cfg->agent_message = NULL; 
	  } 
	  if (work_res_cfg->agent_message) { 
	    glob_res_cfg->agent_message = work_res_cfg->agent_message; 
	    work_res_cfg->agent_message = NULL; 
	  } else if (working_app_cfg->details[0] != '\0') 
	    glob_res_cfg->agent_message = strdup(working_app_cfg->details);
	  if (glob_res_cfg->status != AST_SUCCESS)
	    glob_app_cfg->status = AST_FAILURE;
  
	  /* parameters update */
	  strcpy(glob_res_cfg->res.l.lsp_name, work_res_cfg->res.l.lsp_name);
	  if (strcmp(work_res_cfg->res.l.vtag, "65535") != 0) {
	    if (glob_res_cfg->res.l.vtag[0] == '\0' || strcmp(glob_res_cfg->res.l.vtag, "any") == 0)
	      strcpy(glob_res_cfg->res.l.vtag, work_res_cfg->res.l.vtag);
	  } 
	}
      } else {
        work_res_cfg = (struct resource*) working_app_cfg->link_list->head->data;

	for (curnode1 = glob_res_cfg->res.n.link_list->head;
	     curnode1;
	     curnode1 = curnode1->next) {
	  glob_res_cfg = (struct resource*) curnode1->data;
  
	  if (strcmp(glob_res_cfg->name, work_res_cfg->name) != 0) {
	    continue;
	  }
  
	  zlog_info("Link: %s, %s, LSP: %s vtag: %s", 
	  	work_res_cfg->name, 
	  	status_type_details[work_res_cfg->status],
	  	work_res_cfg->res.l.lsp_name,
		work_res_cfg->res.l.vtag);
  
	  /* counter update */
	  if (work_res_cfg->status != AST_PENDING) {
	    if (IS_SET_SETUP_RESP(glob_res_cfg)) 
	      zlog_warn("SETUP_RESP has been recieved, so this is an update");
	    else { 
	      glob_app_cfg->setup_ready++; 
	      glob_res_cfg->flags |= FLAG_SETUP_RESP; 
	    }
	  }
	
	  /* status update */ 
	  glob_res_cfg->status = work_res_cfg->status; 
	  if (glob_res_cfg->agent_message) { 
	    free(glob_res_cfg->agent_message); 
	    glob_res_cfg->agent_message = NULL; 
	  } 
	  if (work_res_cfg->agent_message) { 
	    glob_res_cfg->agent_message = work_res_cfg->agent_message; 
	    work_res_cfg->agent_message = NULL; 
	  } else if (working_app_cfg->details[0] != '\0') 
	    glob_res_cfg->agent_message = strdup(working_app_cfg->details);
	  if (glob_res_cfg->status != AST_SUCCESS)
	    glob_app_cfg->status = AST_FAILURE;
  
	  /* parameters update */
	  strcpy(glob_res_cfg->res.l.lsp_name, work_res_cfg->res.l.lsp_name);
	  if (glob_res_cfg->res.l.vtag[0] == '\0')
	    strcpy(glob_res_cfg->res.l.vtag, work_res_cfg->res.l.vtag);
	}
      }
    
      sprintf(newpath, "%s/%s/setup_response_%s.xml", 
		AST_DIR, glob_app_cfg->ast_id, srcnode->name); 
      srcnode->dragon_sock = -1;

    }
  }

  /* working_app_cfg is the coming in app_cfg xml, the above code
   * has integrated this into final.xml
   */
  free_application_cfg(working_app_cfg);
  if (rename(AST_XML_RECV, newpath) == -1) 
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	AST_XML_RECV, newpath, errno, strerror(errno));

  /* anyway, now, it's time to save the whole file again
   */
  integrate_result();

  return 1;
}

int
master_process_query_resp()
{
  return 1;
}

int
master_process_release_resp()
{
  struct application_cfg *working_app_cfg;
  struct adtlistnode *curnode1, *curnode2; 
  struct if_ip *glob_ifp, *work_ifp;
  struct resource *work_res_cfg, *glob_res_cfg, *srcnode;
  char newpath[100];

  if (glob_app_cfg->ast_id == NULL) {
    sprintf(glob_app_cfg->details, "For RELEASE_RESP, ast_id has to be set");
    return 0;
  }

  working_app_cfg = glob_app_cfg;

  /* search in the list first */
  glob_app_cfg = retrieve_app_cfg(working_app_cfg->ast_id, MASTER);

  if (!glob_app_cfg) {
    glob_app_cfg = working_app_cfg;
    sprintf(glob_app_cfg->details, "didn't parse the ast_id file successfully");
    glob_app_cfg->status = AST_FAILURE;
    return 0;
  }

  if (glob_app_cfg->action == RELEASE_REQ)
    glob_app_cfg->action = RELEASE_RESP;

  if (glob_app_cfg->action != RELEASE_RESP) {
    zlog_err("Invalid RELEASE_RESP received, %s is in %s", glob_app_cfg->ast_id,
		action_type_details[glob_app_cfg->action]);
    glob_app_cfg = working_app_cfg;
    return 0;
  }

  if (working_app_cfg->link_list == NULL &&
	working_app_cfg->node_list == NULL) {
    zlog_warn("The received RELEASE_RESP message has no resources");
    glob_app_cfg = working_app_cfg;
    return 0;
  }

  /* now has to distinguish where this request is coming from,
   * node_agent or link_agent (dragond)
   */
  if (working_app_cfg->link_list == NULL) {

    work_res_cfg = (struct resource*)working_app_cfg->node_list->head->data;
    glob_res_cfg = search_node_by_name(glob_app_cfg, work_res_cfg->name);

    /* counter update */
    if (IS_SET_RELEASE_RESP(glob_res_cfg)) 
	zlog_warn("RELEASE_RESP has been recieved, so this is an update"); 
    else { 
	glob_app_cfg->release_ready++; 
	glob_res_cfg->flags |= FLAG_RELEASE_RESP; 
    }

    /* status update */ 
    glob_res_cfg->status = work_res_cfg->status;
    if (glob_res_cfg->agent_message) { 
      free(glob_res_cfg->agent_message); 
      glob_res_cfg->agent_message = NULL; 
    }
    if (work_res_cfg->agent_message) {
      glob_res_cfg->agent_message = work_res_cfg->agent_message;
      work_res_cfg->agent_message = NULL;
    } else if (working_app_cfg->details[0] != '\0')
      glob_res_cfg->agent_message = strdup(working_app_cfg->details);
    if (glob_res_cfg->status != AST_SUCCESS)
	glob_app_cfg->status = AST_FAILURE;

    /* parameters update */
    if (glob_res_cfg->res.n.if_list && work_res_cfg->res.n.if_list) 
      for (curnode1 = glob_res_cfg->res.n.if_list->head,
	  curnode2 = work_res_cfg->res.n.if_list->head;
	  curnode1 && curnode2;
	  curnode1 = curnode1->next, curnode2 = curnode2->next) {
	glob_ifp = (struct if_ip*)curnode1->data;
	work_ifp = (struct if_ip*)curnode2->data;

	if (glob_ifp->iface)
	  free(glob_ifp->iface);
	glob_ifp->iface = work_ifp->iface;
	work_ifp->iface = NULL;
    }
   
    sprintf(newpath, "%s/%s/release_node_response_%s.xml",
	AST_DIR, glob_app_cfg->ast_id, glob_res_cfg->name);
    glob_res_cfg->noded_sock = -1;

    zlog_info("Node: %s, %s", glob_res_cfg->name, status_type_details[glob_res_cfg->status]);

  } else {
    srcnode = (struct resource*)working_app_cfg->node_list->head->data;
    zlog_info("From link_agent %s", srcnode->name);

    glob_res_cfg = search_node_by_name(glob_app_cfg,  srcnode->name);
    if (!glob_res_cfg) 
      zlog_err("Can't find this link_agent %s in %s", srcnode->name, glob_app_cfg->ast_id);
    else {
      for (curnode1 = glob_res_cfg->res.n.link_list->head,
	   curnode2 = working_app_cfg->link_list->head;
	   curnode1 && curnode2;
	   curnode1 = curnode1->next, curnode2 = curnode2->next) {
	glob_res_cfg = (struct resource*) curnode1->data;
	work_res_cfg = (struct resource*) curnode2->data;

	if (strcmp(glob_res_cfg->name, work_res_cfg->name) != 0) {
	  zlog_warn("Something wrong with the link ordering");
	  continue;
	}

	zlog_info("Link: %s, %s", work_res_cfg->name,
	                status_type_details[work_res_cfg->status]);

	/* counter update */
	if (IS_SET_RELEASE_RESP(glob_res_cfg)) 
	  zlog_warn("RELEASE_RESP has been recieved, so this is an update");
 	else {
	  glob_app_cfg->release_ready++;
	  glob_res_cfg->flags |= FLAG_RELEASE_RESP;
	}

	/* status update */ 
	glob_res_cfg->status = work_res_cfg->status; 
	if (glob_res_cfg->agent_message) { 
	  free(glob_res_cfg->agent_message); 
	  glob_res_cfg->agent_message = NULL; 
	} 
	if (work_res_cfg->agent_message) { 
	  glob_res_cfg->agent_message = work_res_cfg->agent_message; 
	  work_res_cfg->agent_message = NULL; 
	} else if (working_app_cfg->details[0] != '\0') 
	  glob_res_cfg->agent_message = strdup(working_app_cfg->details);
	if (glob_res_cfg->status != AST_SUCCESS)
	  glob_app_cfg->status = AST_FAILURE;

	/* parameters update */
	strcpy(glob_res_cfg->res.l.lsp_name, work_res_cfg->res.l.lsp_name);
      }
    
      sprintf(newpath, "%s/%s/release_response_%s.xml", 
		AST_DIR, glob_app_cfg->ast_id, srcnode->name); 
    }
    glob_res_cfg->dragon_sock = -1;
  }

  /* working_app_cfg is the coming in app_cfg xml, the above code
   * has integrated this into final.xml
   */
  free_application_cfg(working_app_cfg);
  if (rename(AST_XML_RECV, newpath) == -1) 
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	AST_XML_RECV, newpath, errno, strerror(errno));

  gettimeofday(&(glob_app_cfg->start_time), NULL);

  /* anyway, now, it's time to save the whole file again
   */
  integrate_result();

  return 1;
}

int
master_process_app_complete()
{
  struct application_cfg *working_app_cfg;
  struct adtlistnode *curnode1, *curnode2;
  struct if_ip *glob_ifp, *work_ifp;
  struct resource *work_res_cfg, *glob_res_cfg, *srcnode;
  char newpath[100];

  if (glob_app_cfg->ast_id == NULL) {
    sprintf(glob_app_cfg->details, "For APP_COMPLETE, ast_id has to be set");
    return 0;
  }

  working_app_cfg = glob_app_cfg;

  /* search in the list first */
  glob_app_cfg = retrieve_app_cfg(working_app_cfg->ast_id, MASTER);
  if (!glob_app_cfg) {
    glob_app_cfg = working_app_cfg;
    sprintf(glob_app_cfg->details, "didn't parse the ast_id file successfully");
    glob_app_cfg->status = AST_FAILURE;
    return 0;
  }

  if (glob_app_cfg->action == AST_COMPLETE)
    glob_app_cfg->action= APP_COMPLETE;

  if (glob_app_cfg->action != APP_COMPLETE) {
    zlog_err("Invalid APP_COMPLETE recieved, currently in %s",
		action_type_details[glob_app_cfg->action]);
    glob_app_cfg = working_app_cfg;
    return 0;
  }

  if (glob_app_cfg->link_list == NULL &&
	glob_app_cfg->node_list == NULL) {
    zlog_warn("The received APP_COMPLETE message has no resources");
    glob_app_cfg = working_app_cfg;
    return 0;
  }

  /* now has to distinguish where this request is coming from,
   * node_agent or link_agent (dragond)
   */
  if (working_app_cfg->link_list == NULL) {

    work_res_cfg = (struct resource*)working_app_cfg->node_list->head->data;
    glob_res_cfg = search_node_by_name(glob_app_cfg, work_res_cfg->name);

    /* counter update */
    if (IS_SET_APP_COMPLETE(glob_res_cfg))
        zlog_warn("APP_COMPLETE has been recieved");
    else {
        glob_app_cfg->complete_ready++;
        glob_res_cfg->flags |= FLAG_APP_COMPLETE;
    }

    /* status update */
    glob_res_cfg->status = work_res_cfg->status;
    if (glob_res_cfg->agent_message) {
      free(glob_res_cfg->agent_message);
      glob_res_cfg->agent_message = NULL;
    }
    if (work_res_cfg->agent_message) {
      glob_res_cfg->agent_message = work_res_cfg->agent_message;
      work_res_cfg->agent_message = NULL;
    } else if (working_app_cfg->details[0] != '\0')
      glob_res_cfg->agent_message = strdup(working_app_cfg->details);
    if (glob_res_cfg->status != AST_SUCCESS)
        glob_app_cfg->status = AST_FAILURE;

    sprintf(newpath, "%s/%s/app_complete_node_%s.xml",
	AST_DIR, glob_app_cfg->ast_id, glob_res_cfg->name);

    zlog_info("Node: %s", glob_res_cfg->name);

  } else {

    srcnode = (struct resource*)working_app_cfg->node_list->head->data;
    zlog_info("From link_agent %s", srcnode->name);

    glob_res_cfg = search_node_by_name(glob_app_cfg,  srcnode->name);
    if (!glob_res_cfg)
      zlog_err("Can't find this link_agent %s in %s", srcnode->name, glob_app_cfg->ast_id);
    else {
      for (curnode1 = glob_res_cfg->res.n.link_list->head,
           curnode2 = working_app_cfg->link_list->head;
           curnode1 && curnode2;
           curnode1 = curnode1->next, curnode2 = curnode2->next) {
        glob_res_cfg = (struct resource*) curnode1->data;
        work_res_cfg = (struct resource*) curnode2->data;

        if (strcmp(glob_res_cfg->name, work_res_cfg->name) != 0) {
          zlog_warn("Something wrong with the link ordering");
          continue;
        }

        zlog_info("Link: %s, %s, LSP: %s",
                work_res_cfg->name,
                status_type_details[work_res_cfg->status],
                work_res_cfg->res.l.lsp_name);

        /* counter update */
        if (IS_SET_APP_COMPLETE(glob_res_cfg))
          zlog_warn("APP_COMPLETE has been recieved, so this is an update");
        else {
          glob_app_cfg->complete_ready++;
          glob_res_cfg->flags |= FLAG_APP_COMPLETE;
        }

        /* status update */
        glob_res_cfg->status = work_res_cfg->status;
        if (glob_res_cfg->agent_message) {
          free(glob_res_cfg->agent_message);
          glob_res_cfg->agent_message = NULL;
        }
        if (work_res_cfg->agent_message) {
          glob_res_cfg->agent_message = work_res_cfg->agent_message;
          work_res_cfg->agent_message = NULL;
        } else if (working_app_cfg->details[0] != '\0')
          glob_res_cfg->agent_message = strdup(working_app_cfg->details);
        if (glob_res_cfg->status != AST_SUCCESS)
          glob_app_cfg->status = AST_FAILURE;
      }

      sprintf(newpath, "%s/%s/app_complete_link_%s.xml",
		AST_DIR, glob_app_cfg->ast_id, srcnode->name);

    }
  }

  /* working_app_cfg is the coming in app_cfg xml, the above code
   * has integrated this into final.xml
   */
  free_application_cfg(working_app_cfg);
  if (rename(AST_XML_RECV, newpath) == -1)
     zlog_err("Can't rename %s to %s; errno = %d(%s)",
	AST_XML_RECV, newpath, errno, strerror(errno));

  integrate_result();

  return 1;
}

int
master_process_setup_req()
{
  char directory[100];
  char newpath[105];
  
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
  if (send_task_to_link_agent() == 0)
    glob_app_cfg->status = AST_FAILURE;

  integrate_result();
  return 1;
}

int
master_process_query_req()
{
  char directory[100];
  char newpath[105];
  
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

  glob_app_cfg->status = AST_SUCCESS;
  if (send_task_to_node_agent() == 0)
    glob_app_cfg->status = AST_FAILURE;
  if (send_task_to_link_agent() == 0) 
    glob_app_cfg->status = AST_FAILURE;

  glob_app_cfg->action = QUERY_RESP;
  integrate_result();

  return 1;
}
 
int 
master_process_release_req()
{
  char path[100];
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
 
  } else {

    working_app_cfg = glob_app_cfg;
    
    /* search in the list first */
    glob_app_cfg = retrieve_app_cfg(working_app_cfg->ast_id, MASTER);

    if (!glob_app_cfg) {
      glob_app_cfg = working_app_cfg;
      sprintf(glob_app_cfg->details, "didn't parse the ast_id final file successfully");
      glob_app_cfg->status = AST_FAILURE;
      return 0;
    }

    app_cfg_pre_req();
    if (!file_mode) {
      
      if (glob_app_cfg->action == RELEASE_RESP ||
	  glob_app_cfg->action == RELEASE_REQ) {
	glob_app_cfg = working_app_cfg;
	set_allres_fail("ast_id has received RELEASE_REQ already");
	glob_app_cfg->status = AST_FAILURE;
	return 0;
      }
    }
    glob_app_cfg->action = working_app_cfg->action;
  }

  if (file_mode) {
    char directory[100];

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

  master_cleanup_assign_ip();
  /* now, save the release_original first */
  sprintf(path, "%s/%s/release_original.xml", AST_DIR, glob_app_cfg->ast_id);
  if (rename(AST_XML_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	     AST_XML_RECV, path, errno, strerror(errno));

  app_cfg_pre_req();
  glob_app_cfg->flags |= FLAG_RELEASE_REQ;

  gettimeofday(&(glob_app_cfg->start_time), NULL);

  if (send_task_to_node_agent() == 0) 
    glob_app_cfg->status = AST_FAILURE;

  if (send_task_to_link_agent() == 0) 
    glob_app_cfg->status = AST_FAILURE;

  integrate_result();
  
  return 1;
}

int
master_process_topo(char* input_file)
{
  char system_call[100];

  if (strcasecmp(input_file, AST_XML_RECV) != 0) { 
    sprintf(system_call, "cp %s %s", input_file, AST_XML_RECV); 
    system(system_call);
  }

  /* after all the preparation, parse the application xml file 
   */ 
  if ((glob_app_cfg = topo_xml_parser(AST_XML_RECV, MASTER)) == NULL) { 
    zlog_err("master_process_topo: topo_xml_parser() failed"); 
    return 0;
  }

  if (topo_validate_graph(MASTER, glob_app_cfg) == 0) {
    sprintf(glob_app_cfg->details, "Failed at validation");
    if (glob_app_cfg->action == SETUP_REQ)
      glob_app_cfg->action = SETUP_RESP;
    else if (glob_app_cfg->action == RELEASE_REQ)
      glob_app_cfg->action = RELEASE_RESP;

    return 0;
  }

  if (glob_app_cfg->action == SETUP_REQ && master_locate_resource() == 0) {
    sprintf(glob_app_cfg->details, "Failed at locating unknown resource(s)");
    return 0;
  }

  if (glob_app_cfg->action == SETUP_REQ)
    glob_app_cfg->ast_id = generate_ast_id(ID_SETUP);

  zlog_info("Processing ast_id: %s, action: %s",
		glob_app_cfg->ast_id,
		action_type_details[glob_app_cfg->action]);

  glob_app_cfg->status = AST_SUCCESS;
  if ( glob_app_cfg->action == SETUP_REQ)
    master_process_setup_req();
  else if ( glob_app_cfg->action == SETUP_RESP)
    master_process_setup_resp();
  else if (glob_app_cfg->action == RELEASE_REQ)
    master_process_release_req();
  else if ( glob_app_cfg->action == QUERY_REQ)
    master_process_query_req();
  else if ( glob_app_cfg->action == APP_COMPLETE) 
    master_process_app_complete();

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


/* ASP Masterd main routine */
int
main(int argc, char* argv[])
{
  char *progname;
  char *p;
  char *vty_addr = NULL;
  int daemon_mode = 0;
  struct thread thread;
  struct sockaddr_in servAddr;
  int servSock;
  char *config_file = NULL;
  struct sigaction myAlarmAction;

  progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);

  while (1) {
    int opt;

    opt = getopt_long (argc, argv, "dlc:hA:P:v", ast_master_opts, 0);
    if (argc > 3) {
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
      default: 
	usage (progname, 1);
    }
  }

  zlog_default = openzlog (progname, ZLOG_NOLOG, ZLOG_ASTB,
			   LOG_CONS|LOG_NDELAY|LOG_PID, LOG_DAEMON);
  zlog_set_file(zlog_default, ZLOG_FILE, "/var/log/ast_master.log");

  memset(&vtag_pool, 0, sizeof(struct vtag_tank));
  memset(&narb_pool, 0, sizeof(struct narb_tank));
  memset(&es_pool, 0, sizeof(struct es_tank));
  memset(&agency, 0, 3*sizeof(struct resource_agent));

  /* Change to the daemon program. */
  if (daemon_mode)
    daemon (0, 0);

  master = thread_master_create();


  /* print banner */
  zlog_info("AST_MASTER starts: pid = %d", getpid());

  /* parse all service_template.xml and build the default
   * struct for each service template for later use
   */
  if (template_xml_parser() == 0) 
    zlog_info("didn't have template files for link templates...");

  glob_app_cfg = NULL;
  memset(&app_list, 0, sizeof(struct adtlist));

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

void
set_alllink(struct adtlist *list, int ast_status, enum link_status link_status, u_int8_t flags, char* message)
{
  struct adtlistnode *curnode;
  struct resource *mylink;

  if (!list) 
    return;

  for (curnode = list->head;
	curnode;
	curnode = curnode->next) {
    mylink = (struct resource*) curnode->data; 
    if (ast_status != -1) 
      mylink->status = ast_status;
    if (link_status != -1)
      mylink->res.l.l_status = link_status;
    if (message) 
      mylink->agent_message = strdup(message);
    if (flags)
      mylink->flags |= flags;
  }
}

int
master_compose_link_request(struct application_cfg *app_cfg, 
		  	    char *path, 
			    struct resource* srcnode)
{
  struct adtlistnode *linknode;
  struct resource *link;
  FILE *send_file;

  if (!srcnode) 
    return 0;

  if (!path || strlen(path) == 0) 
    return 0;
  
  send_file = fopen(path, "w+");
  if (send_file == NULL) {
    zlog_err("Can't open the file %s; error = %d(%s)", 
		path, errno, strerror(errno));
    return 0;
  }
  
  fprintf(send_file, "<topology ast_id=\"%s\" action=\"%s\">\n", app_cfg->ast_id, action_type_details[app_cfg->action]);

  if (app_cfg->action == QUERY_REQ) {
    if (srcnode->agent_message) {
      free(srcnode->agent_message);
      srcnode->agent_message = NULL;
    }
    print_node(send_file, srcnode);
    fprintf(send_file, "</topology>");
    fflush(send_file);
    fclose(send_file);
    return 1;
  }

  if (app_cfg->action != SETUP_REQ) {
    fprintf(send_file, "</topology>");
    fflush(send_file);
    fclose(send_file);
    return 1;
  }

  /* now, it's the case for SETUP_RES 
   */
  print_node(send_file, srcnode);
  for (	linknode = srcnode->res.n.link_list->head;
	linknode;
	linknode = linknode->next) {
    link = (struct resource*)linknode->data;

    switch (link->res.l.stype) {

      case uni:
      case non_uni:
	if (link->res.l.src->proxy) 
	  print_node(send_file, link->res.l.src->proxy);
	print_node(send_file, link->res.l.dest->es);
	print_link(send_file, link);
 	break;	

      case vlsr_vlsr:
	print_node(send_file, link->res.l.dest->vlsr);
	print_link(send_file, link);
	break;

      case vlsr_es:
	if (link->res.l.src->vlsr && link->res.l.dest->es) {
	  print_node(send_file, link->res.l.dest->es);
	} else if (link->res.l.src->es && link->res.l.dest->vlsr) {
	  if (link->res.l.src->proxy)
	    print_node(send_file, link->res.l.src->es);
	  print_node(send_file, link->res.l.dest->vlsr);
	}
	print_link(send_file, link);
    } 
  }

  fprintf(send_file, "</topology>");
  fflush(send_file);
  fclose(send_file);

  return 1;
}

int
master_compose_node_request(struct application_cfg *app_cfg, 
			    char *path, 
			    struct resource* srcnode)
{
  FILE *send_file;

  if (srcnode == NULL) 
    return 0;

  if (path == NULL || strlen(path) == 0) 
    return 0;

  send_file = fopen(path, "w+");
  if (send_file == NULL) {
    zlog_err("Can't open the file %s; error = %d(%s)", 
		path, errno, strerror(errno));
    return 0;
  }

  fprintf(send_file, "<topology ast_id=\"%s\" action=\"%s\">\n", app_cfg->ast_id, action_type_details[app_cfg->action]);
  print_node(send_file, srcnode);

  fprintf(send_file, "</topology>");
  fflush(send_file);
  fclose(send_file);

  return 1;
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
		node_stype_name[myres->res.n.stype], myres->name);
  fprintf(send_file, "</resource>\n"); 
  fprintf(send_file, "</topology>");
  fflush(send_file);
  fclose(send_file);

  return 1;
}

int
send_task_to_link_agent()
{
  struct adtlistnode *curnode;
  struct resource *srcnode, *link;
  int sock, ready = 0;
  u_int16_t flags;
  static char buffer[RCVBUFSIZE];
  static char send_buf[SENDBUFSIZE];
  int ret_value = 1;
  char directory[80];
  char newpath[105];
  char path_prefix[100];

  zlog_info("send_task_to_link_agent ...");
  strcpy(directory, AST_DIR);
  sprintf(directory+strlen(directory), "/%s", glob_app_cfg->ast_id);
  
  if (glob_app_cfg->action == SETUP_REQ) {
    flags = FLAG_SETUP_REQ;
    sprintf(path_prefix, "%s/setup_", directory);
  } else if (glob_app_cfg->action == RELEASE_REQ) {
    flags = FLAG_RELEASE_REQ;
    sprintf(path_prefix, "%s/release_", directory);
  } else if (glob_app_cfg->action == QUERY_REQ) {
    flags = FLAG_QUERY_REQ;
    sprintf(path_prefix, "%s/query_", directory);
  } else if (glob_app_cfg->action == AST_COMPLETE) {
    flags = FLAG_AST_COMPLETE;
    sprintf(newpath, "%s/ast_complete.xml", directory);
    print_final(newpath);
  } else {
    zlog_err("send_task_to_link_agent: invalid action");
    return 0;
  }

  memset(buffer, 0, RCVBUFSIZE);
  memset(send_buf, 0, SENDBUFSIZE);
  for (curnode = glob_app_cfg->node_list->head;
       curnode;
       curnode = curnode->next) {
    srcnode = (struct resource*)(curnode->data); 

    if (!srcnode->res.n.link_list &&
	glob_app_cfg->action != QUERY_REQ) {
	
      /* No task need to be sent to this link_agent (dragond); skip 
       */
      continue;
    }

    if (glob_app_cfg->action == SETUP_REQ) {
      link = srcnode->res.n.link_list->head->data;
      if (IS_SET_SETUP_REQ(link))
	continue;
    }

    if (glob_app_cfg->action != AST_COMPLETE) {
      sprintf(newpath, "%srequest_%s.xml", path_prefix, srcnode->name);
      if (master_compose_link_request(glob_app_cfg, newpath, srcnode) == 0) {
	set_alllink(srcnode->res.n.link_list, AST_FAILURE, -1, 0, "Failed to compose the xml file to dragon");
	ready += adtlist_getcount(srcnode->res.n.link_list);
	ret_value = 0;
	continue;
      }
    }

    zlog_info("sending request to %s (%s:%d)", 
		srcnode->name, srcnode->res.n.ip, DRAGON_XML_PORT);
    sock = send_file_to_agent(srcnode->res.n.ip, DRAGON_XML_PORT, newpath);
    if (sock == -1)  {
      set_alllink(srcnode->res.n.link_list, AST_FAILURE, -1, flags, "Failed to connect to dragond");
      ready += adtlist_getcount(srcnode->res.n.link_list);
      ret_value = 0;

      if (glob_app_cfg->action == QUERY_REQ)
	continue;
 
      continue;
    } else {
      set_alllink(srcnode->res.n.link_list, AST_PENDING, commit, flags, NULL);
    }

    if (glob_app_cfg->action != AST_COMPLETE) {
      zlog_info("SOCK: %d added for dragon_callback", sock);
      thread_add_read(master, dragon_callback, NULL, sock);
      if (srcnode->dragon_sock != -1) {
	thread_remove_read(master, dragon_callback, NULL, srcnode->dragon_sock);
	close(srcnode->dragon_sock);
	zlog_info("SOCK: closing dragon_sock %d", srcnode->dragon_sock);
      }
      srcnode->dragon_sock = sock;
    } else {
      close(sock);
      srcnode->dragon_sock = -1;
    }

    if (glob_app_cfg->action == SETUP_REQ) {
      glob_app_cfg->setup_sent += adtlist_getcount(srcnode->res.n.link_list);
      break;
    }
  }

  if (glob_app_cfg->action == SETUP_REQ)   
    glob_app_cfg->setup_ready += ready;
  else if (glob_app_cfg->action == RELEASE_REQ)
    glob_app_cfg->release_ready += ready;
  else if (glob_app_cfg->action == APP_COMPLETE)
    glob_app_cfg->complete_ready += ready;
    
  return ret_value;
}

void 
integrate_result()
{
  char directory[80];
  char newpath[105];
  char path_prefix[100];

  /* first, save the cur cfg into final.xml */
  sprintf(directory, "%s/%s", AST_DIR, glob_app_cfg->ast_id);
  if (glob_app_cfg->action != QUERY_RESP) { 
    sprintf(newpath, "%s/final.xml", directory); 
    print_final(newpath);
  }
  add_cfg_to_list();
  symlink(newpath, AST_XML_RESULT);

  switch (glob_app_cfg->action) {
    case SETUP_RESP:
      sprintf(path_prefix, "%s/setup_", directory);
      if (glob_app_cfg->status != AST_SUCCESS)
	break;

      if (glob_app_cfg->setup_ready == adtlist_getcount(glob_app_cfg->link_list)) {

	if (glob_app_cfg->setup_ready == glob_app_cfg->total) {
	  /* all node resources are vlsr */
	  print_final(newpath);
	  break;
	}

	master_lookup_assign_ip(); 
	master_check_command(); 
	print_final(newpath); 
	if (master_setup_req_to_node() == 0) 
	  glob_app_cfg->status = AST_FAILURE; 
	else 
	  return;
      } else if (glob_app_cfg->setup_ready != glob_app_cfg->total ) {
	if (glob_app_cfg->setup_sent < adtlist_getcount(glob_app_cfg->link_list)
		&& glob_app_cfg->setup_ready == glob_app_cfg->setup_sent) {
	  glob_app_cfg->action = SETUP_REQ;
          if (send_task_to_link_agent() == 0) 
	    glob_app_cfg->status = AST_FAILURE;
	  glob_app_cfg->action = SETUP_RESP;

	  if (glob_app_cfg->status == AST_SUCCESS)
	    return;
	  
        } else 
	  return;
      }
      break;

    case RELEASE_RESP:
      if (glob_app_cfg->release_ready == glob_app_cfg->total) 
	sprintf(path_prefix, "%s/release_", directory);
      else
	return;
      break;
  
    case QUERY_RESP:
      sprintf(path_prefix, "%s/query_", directory);
      break;

    case APP_COMPLETE:
      if (glob_app_cfg->total == glob_app_cfg->complete_ready) {
        zlog_info("All minions are done with this AST, resources are ready to be released");
	glob_app_cfg->status = AST_SUCCESS;
        return;
      }
    default:
      return;
  }
  sprintf(newpath, "%sfinal.xml", path_prefix);
  print_final(newpath);

  /* send the result back to user */
  if (glob_app_cfg->clnt_sock != -1) {
    if (glob_app_cfg->action == SETUP_RESP) 
      glob_app_cfg->flags |= FLAG_SETUP_RESP;
    else 
      glob_app_cfg->flags |= FLAG_RELEASE_RESP;

    unlink(AST_XML_RESULT);
    print_final_client(AST_XML_RESULT);
    if (send_file_over_sock(glob_app_cfg->clnt_sock, AST_XML_RESULT) == 0)
      zlog_err("Failed to send the result back to client");
    close(glob_app_cfg->clnt_sock);
    glob_app_cfg->clnt_sock = -1;
  }

  /* see if need to send out AST_COMPLETE */
  if (glob_app_cfg->status == AST_SUCCESS && 
      glob_app_cfg->action == SETUP_RESP) {

    glob_app_cfg->action = AST_COMPLETE;
    zlog_info("Received AST_SUCCESS from ALL in SETUP_REQ; now, do AST_COMPLETE");

    send_task_to_node_agent();
    send_task_to_link_agent();
  }

  if (glob_app_cfg->action == RELEASE_RESP) {
    clean_socket(glob_app_cfg);
    del_cfg_from_list(glob_app_cfg);
    glob_app_cfg = NULL;
  }
}

int
send_task_to_node_agent()
{
  struct adtlistnode *curnode;
  struct resource* srcnode;
  int sock, ret_value = 1;
  static char buffer[RCVBUFSIZE];
  static char send_buf[SENDBUFSIZE];
  char directory[80];
  char newpath[105];
  char path_prefix[100];
  u_int16_t flags;
  int ready = 0;

  zlog_info("send_task_to_node_agent ...");  
  strcpy(directory, AST_DIR);
  sprintf(directory+strlen(directory), "/%s", glob_app_cfg->ast_id);

  if (glob_app_cfg->action == SETUP_REQ) {
    flags = FLAG_SETUP_REQ;
    sprintf(path_prefix, "%s/setup_node_", directory);
  } else if (glob_app_cfg->action == RELEASE_REQ) {
    flags = FLAG_RELEASE_REQ;
    sprintf(path_prefix, "%s/release_node_", directory);
  } else if (glob_app_cfg->action == QUERY_REQ) {
    flags = FLAG_QUERY_REQ;
    sprintf(path_prefix, "%s/query_node_", directory);
  } else if (glob_app_cfg->action == AST_COMPLETE) {
    flags = FLAG_AST_COMPLETE;
    sprintf(newpath, "%s/ast_complete.xml", directory);
    print_final(newpath);
  } else {
    zlog_err("send_task_to_node_agent: invalid action");
    return 0;
  }
   
  memset(buffer, 0, RCVBUFSIZE);
  memset(send_buf, 0, SENDBUFSIZE);

  for (curnode = glob_app_cfg->node_list->head;
	curnode;
	curnode = curnode->next) {
    srcnode = (struct resource*)(curnode->data);

    if (srcnode->res.n.stype == vlsr) {
      if (glob_app_cfg->action == APP_COMPLETE) 
	ready++;
      zlog_info("no need to send task to node_agent on vlsr (%s)", srcnode->name);
      continue;
    }

    if (glob_app_cfg->action == RELEASE_REQ) {
      if (!IS_SET_SETUP_REQ(srcnode)) {
	ready++;
	zlog_info("Skip node_agent (%s) for RELEASE_REQ, without sending SETUP_REQ to it", srcnode->name);
	if (srcnode->agent_message) 
	  free(srcnode->agent_message);
	srcnode->agent_message = strdup("Skip; as haven't sent SETUP_REQ to it");
	continue;
      } else if (!IS_SET_SETUP_RESP(srcnode)) {
	ready++;
	zlog_info("Skip node_agent (%s) for RELEASE_REQ, without receiving SETUP_RESP from it", srcnode->name);
	if (srcnode->agent_message) 
	  free(srcnode->agent_message);
	srcnode->agent_message = strdup("Skip; as received SETUP_RESP from it");
	continue;
      }
    }

    if (glob_app_cfg->action != AST_COMPLETE) {
      sprintf(newpath, "%srequest_%s.xml", path_prefix, srcnode->name);
      if (master_compose_node_request(glob_app_cfg, newpath, srcnode) == 0) {
	srcnode->status = AST_FAILURE;
	srcnode->agent_message = strdup("Failed to compose the message to node_agent");
	ret_value = 0;
	ready++;
	continue;
      }
    }

    zlog_info("sending request to %s (%s:%d)",
		srcnode->name, srcnode->res.n.ip, NODE_AGENT_PORT);
    sock = send_file_to_agent(srcnode->res.n.ip, NODE_AGENT_PORT, newpath);
    if (sock == -1) {
      srcnode->status = AST_FAILURE;
      srcnode->agent_message = strdup("Failed to connect to node_agent");
      ret_value = 0;
      ready++;
      continue;
    }

    srcnode->flags |= flags;

    if (glob_app_cfg->action == SETUP_REQ)
      glob_app_cfg->setup_sent++;

    if (glob_app_cfg->action != AST_COMPLETE) {
      zlog_info("SOCK: %d added for noded_callback", sock);
      thread_add_read(master, noded_callback, NULL, sock);
      if (srcnode->noded_sock != -1) {
        close(srcnode->noded_sock);
	zlog_info("SOCK: closing noded_sock %d", srcnode->noded_sock);
      }
      srcnode->noded_sock = sock;
    } else {
      shutdown(sock, SHUT_RDWR);
      srcnode->noded_sock = -1;
    }
  }

  if (glob_app_cfg->action == SETUP_REQ) 
    glob_app_cfg->setup_ready += ready;
  else if (glob_app_cfg->action == RELEASE_REQ)
    glob_app_cfg->release_ready += ready;
  else if (glob_app_cfg->action == APP_COMPLETE)
    glob_app_cfg->complete_ready += ready;

  return ret_value;
}

/* let ast_masterd to listen on a certain tcp port;
 */
static int
master_accept(struct thread *thread)
{
  int servSock, clntSock, fd;
  struct sockaddr_in clntAddr;
  unsigned int clntLen;
  int recvMsgSize, total = 0;
  char buffer[4001];
  FILE* fp;
  struct stat sb;

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
  fp = fopen(AST_XML_RECV, "w");
  recv_alarm = 1;
  alarm(TIMEOUT_SECS);
  total = 0;
  while ((recvMsgSize = recv(clntSock, buffer, 4000, 0)) > 0) {
    if (errno == EINTR) {
      /* alarm went off
       */
      buffer[recvMsgSize]='\0';
      fprintf(fp, "%s", buffer);
      break;
    }
    total+=recvMsgSize;
    buffer[recvMsgSize]='\0';
    fprintf(fp, "%s", buffer);
    alarm(TIMEOUT_SECS);
  }
  alarm(0);
  recv_alarm = 0;

  if (recvMsgSize < 0 && errno != EINTR) 
    zlog_err("master_server_init: recv() failed");

  /* dump the file to a well-known location */
  fflush(fp);
  fclose(fp);

  if (total == 0) {
    zlog_info("master_accept(): empty document recv");
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
	  glob_app_cfg->status = AST_FAILURE;
	  
	break;
      } else {
	/* supposedly at this point, AST_XML_RECV should exist */
	fd = open(AST_XML_RESULT, O_RDONLY);
	if (fd == -1)
	  print_final(AST_XML_RESULT);
	close(fd);
      }

      if (glob_app_cfg && glob_app_cfg->details[0] != '\0')
	zlog_err(glob_app_cfg->details);

      /* if at this point, there is already error(s) in 
       * processing, no need to wait for ALL result back before
       * sending updates to user
       */
      if (glob_app_cfg) {
	if ((glob_app_cfg->action == SETUP_REQ ||
	  glob_app_cfg->action == RELEASE_REQ) &&
	  glob_app_cfg->status == AST_SUCCESS) {
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
    if (stat(AST_XML_RESULT, &sb) == -1)
      print_error_response(AST_XML_RESULT);

    send_file_over_sock(clntSock, AST_XML_RESULT);
    close(clntSock);
    zlog_info("SOCK: closing clntSock %d", clntSock);
  } else if (glob_app_cfg->clnt_sock == -1 || 
		glob_app_cfg->status == AST_FAILURE) {
    unlink(AST_XML_RESULT); 
    print_final_client(AST_XML_RESULT);

    send_file_over_sock(clntSock, AST_XML_RESULT);
    close(clntSock);
    zlog_info("SOCK: closing clntSock %d", clntSock);
    glob_app_cfg->clnt_sock = -1;
  } else if (glob_app_cfg->clnt_sock == -1 || 
	     clntSock != glob_app_cfg->clnt_sock) {
    close(clntSock);
    zlog_info("SOCK: closing clntSock %d", clntSock);
  }

  if (glob_app_cfg && glob_app_cfg != search_cfg_in_list(glob_app_cfg->ast_id)) 
    free_application_cfg(glob_app_cfg);
  glob_app_cfg = NULL;

  zlog_info("master_accept(): DONE");

  thread_add_read(master, master_accept, NULL, servSock);

  master_check_app_list();
  return 1;
}

static int 
noded_callback(struct thread *thread)
{
  int servSock;
  int total;
  static char buffer[RCVBUFSIZE];
  static char ret_buf[SENDBUFSIZE];
  int bytesRcvd, ret_value = 1;
  FILE* ret_file = NULL;

  alarm(0);
  servSock = THREAD_FD(thread);
  unlink(AST_XML_RECV);

  zlog_info("noded_callback(): START; fd: %d", servSock);

  total = 0;
  memset(ret_buf, 0, SENDBUFSIZE);
  while ((bytesRcvd = recv(servSock, buffer, RCVBUFSIZE-1, 0)) >0 ) {
    if (!total) {  
      ret_file = fopen(AST_XML_RECV, "w");
      if (!ret_file) {
	zlog_err("noded_callback(): can't open %s", AST_XML_RECV);
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

    if ((glob_app_cfg = topo_xml_parser(AST_XML_RECV, MASTER)) == NULL) {
      zlog_err("received file is not parsed correctly, ignore ...");
      ret_value = 0;
    } else if (topo_validate_graph(MASTER, glob_app_cfg) == 0) {
      zlog_err("received file is invalid, ignore ...");
      ret_value = 0;
    } else if (!glob_app_cfg->node_list) {
      zlog_err("received file has no nodes in it, ignore ...");
      ret_value = 0;
    } else {
      zlog_info("Processing ast_id: %s, action: %s",
		glob_app_cfg->ast_id, 
		action_type_details[glob_app_cfg->action]);

      if (glob_app_cfg->action == SETUP_RESP) 
	master_process_setup_resp();
      else if (glob_app_cfg->action == APP_COMPLETE) 
	master_process_app_complete();
      else if (glob_app_cfg->action == RELEASE_RESP)
	master_process_release_resp();
      else if (glob_app_cfg->action == QUERY_RESP)
 	master_process_query_resp();
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
  zlog_info("SOCK: closing noded_callback fd %d", servSock);
  master_check_app_list();
  zlog_info("noded_callback(): DONE");

  return ret_value;
}

static int 
dragon_callback(struct thread *thread)
{
  int servSock, total;
  static char buffer[RCVBUFSIZE];
  static char ret_buf[SENDBUFSIZE];
  int bytesRcvd, ret_value = 1;
  FILE* ret_file = NULL;

  alarm(0);
  servSock = THREAD_FD(thread);

  unlink(AST_XML_RECV);

  zlog_info("dragon_callback(): START; fd: %d", servSock);

  total = 0;
  memset(ret_buf, 0, SENDBUFSIZE);
  while ((bytesRcvd = recv(servSock, buffer, RCVBUFSIZE-1, 0)) > 0) {
    if (!total) {  
      ret_file = fopen(AST_XML_RECV, "w");
      if (!ret_file) {
	zlog_err("dragon_callback(): can't open %s", AST_XML_RECV);
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

    if ((glob_app_cfg = topo_xml_parser(AST_XML_RECV, MASTER)) == NULL) {
      zlog_err("received file is not parsed correctly, ignore ...");
      ret_value = 0;
    } else if (topo_validate_graph(MASTER, glob_app_cfg) == 0) {
      zlog_err("received file is invalid, ignore ...");
      ret_value = 0;
    } else if (!glob_app_cfg->link_list) {
      zlog_err("received file has no link in it, ignore ...");
      ret_value = 0;
    } else {
      zlog_info("Processing ast_id: %s, action: %s",
		glob_app_cfg->ast_id, 
		action_type_details[glob_app_cfg->action]);

      if (glob_app_cfg->action == SETUP_RESP) 
	master_process_setup_resp();
      else if (glob_app_cfg->action == APP_COMPLETE) 
	master_process_app_complete();
      else if (glob_app_cfg->action == RELEASE_RESP)
	master_process_release_resp();
      else if (glob_app_cfg->action == QUERY_RESP)
 	master_process_query_resp();
      else {
	zlog_err("Invalid action %s sent from node_agent",
	        action_type_details[glob_app_cfg->action]);
	free_application_cfg(glob_app_cfg);
	glob_app_cfg = NULL;
	ret_value = 0;
      }
    }
  }

  zlog_info("SOCK: closing dragon_callback fd %d", servSock);
  close(servSock);
  master_check_app_list();
  zlog_info("dragon_callback(): END");

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
    agent = &agency[myres->res.n.stype];
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
      if (newres->res.n.ip == '\0' || 
	  newres->res.n.router_id[0] == '\0' ||
	  newres->res.n.tunnel[0] == '\0') { 
	set_res_fail("Invalid response from resource broker", myres);
	return 0;
      }

      strncpy(myres->res.n.ip, newres->res.n.ip, IP_MAXLEN);
      strncpy(myres->res.n.router_id, newres->res.n.router_id, IP_MAXLEN);
      strncpy(myres->res.n.tunnel, newres->res.n.tunnel, 9);
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

static int
master_setup_req_to_node()
{
  struct adtlistnode *curnode;
  struct resource *myres;
  struct endpoint *src, *dest;
  int ret_value = 1;

  /* change the action to SETUP_REQ temporarily
   */
  glob_app_cfg->action = SETUP_REQ;

  /* update if_ip vtag values
   */
  for (curnode = glob_app_cfg->link_list->head;
	curnode;
	curnode = curnode->next) {
    myres = (struct resource*) curnode->data;

    src = myres->res.l.src;
    dest = myres->res.l.dest;

    if (src->ifp)
      src->ifp->vtag = atoi(myres->res.l.vtag);
    if (dest->ifp) 
      dest->ifp->vtag = atoi(myres->res.l.vtag);
  }

  /* now, I can send this to the node_agent
   */
  if (send_task_to_node_agent() == 0) {
    glob_app_cfg->status = AST_FAILURE;
    ret_value = 0;
  }

  glob_app_cfg->action = SETUP_RESP;
  return ret_value;
}

static void
master_check_app_list()
{
  struct adtlistnode *curnode;
  struct application_cfg *app_cfg, *cur_cfg;
  char newpath[105];
  struct timeval curr_time;
  unsigned int next_alarm = 0;
  long time_escape;

  gettimeofday(&curr_time, NULL);

  if (adtlist_getcount(&app_list) == 0)
    return;

  zlog_info("master_check_app_list()");
  for (curnode = app_list.head;
	curnode;
	curnode = curnode->next) {
    app_cfg = (struct application_cfg*) curnode->data;

    if (IS_SET_RELEASE_REQ(app_cfg) && !IS_SET_RELEASE_RESP(app_cfg)) {

      time_escape = curr_time.tv_sec - app_cfg->start_time.tv_sec;
      if (time_escape < CLIENT_TIMEOUT) {
	if ((CLIENT_TIMEOUT - time_escape) > next_alarm)
	  next_alarm = CLIENT_TIMEOUT - time_escape;
	continue;
      }

      zlog_info("master_check_app_list(): sending RELEASE_RESP for %s", app_cfg->ast_id);
      del_cfg_from_list(app_cfg);
      app_cfg->flags |= FLAG_RELEASE_RESP;
      app_cfg->action = RELEASE_RESP;
      app_cfg->status = AST_FAILURE;
      strcpy(app_cfg->details, "didn't receive all RELEASE_RESP");

      sprintf(newpath, "%s/%s/release_final.xml", AST_DIR, app_cfg->ast_id);
      cur_cfg = glob_app_cfg;
      glob_app_cfg = app_cfg;
      print_final(newpath);
      sprintf(newpath, "%s/%s/final.xml", AST_DIR, app_cfg->ast_id);
      print_final(newpath);
      print_final_client(AST_XML_RESULT);

      glob_app_cfg = cur_cfg;

      if (send_file_over_sock(app_cfg->clnt_sock, AST_XML_RESULT) == 0)
	zlog_err("Failed to send the result back to client");
      close(app_cfg->clnt_sock);
      app_cfg->clnt_sock = -1;

    } else if (IS_SET_SETUP_REQ(app_cfg) && !IS_SET_SETUP_RESP(app_cfg)) {

      time_escape = curr_time.tv_sec - app_cfg->start_time.tv_sec;
      if (time_escape < CLIENT_TIMEOUT) {
	if ((CLIENT_TIMEOUT - time_escape) > next_alarm)
	  next_alarm = CLIENT_TIMEOUT - time_escape;
	continue;
      }

      zlog_info("master_check_app_list(): sending SETUP_REQ for %s", app_cfg->ast_id);
      app_cfg->flags |= FLAG_SETUP_RESP;
      app_cfg->action = SETUP_RESP;
      app_cfg->status = AST_FAILURE;
      strcpy(app_cfg->details, "didn't receive all SETUP_RESP");

      
      sprintf(newpath, "%s/%s/setup_final.xml", AST_DIR, app_cfg->ast_id);
      glob_app_cfg = app_cfg;
      print_final(newpath);
      sprintf(newpath, "%s/%s/final.xml", AST_DIR, app_cfg->ast_id);
      print_final(newpath);
      print_final_client(AST_XML_RESULT);
      glob_app_cfg = NULL;

      if (send_file_over_sock(app_cfg->clnt_sock, AST_XML_RESULT) == 0)
	zlog_err("Failed to send the result back to client");
      close(app_cfg->clnt_sock);
      app_cfg->clnt_sock = -1;
    }
  }

  alarm(next_alarm);
  return;
}

static void
handle_alarm()
{
  zlog_info("Received: SIGALARM");
  if (!recv_alarm) 
    master_check_app_list();
}

static int
master_lookup_assign_ip()
{
  struct adtlistnode *curnode;
  struct resource *res;
  struct endpoint *src, *dest;
  FILE *fp;
  char *f_ret, *token, line[100], addr[200];
  struct in_addr mask, ip;

  zlog_info("master_lookup_assign_ip() ...");
  if (!glob_app_cfg || !glob_app_cfg->link_list)
    return 0;

  for (curnode = glob_app_cfg->link_list->head;
	curnode;
	curnode = curnode->next) {
    res = (struct resource*)curnode->data;

    if (res->res.l.stype != uni && res->res.l.stype!= non_uni) {
      zlog_info(" ********* no need to look for ip for link %s", res->name);
      continue;
    }

    src = res->res.l.src; 
    dest = res->res.l.dest;

    if (!src->ifp) {
      src->ifp = malloc(sizeof(struct if_ip));
      memset(src->ifp, 0, sizeof(struct if_ip));
      if (!src->es->res.n.if_list) {
	src->es->res.n.if_list = malloc(sizeof(struct adtlist));
	memset(src->es->res.n.if_list, 0, sizeof(struct adtlist));
      }
      adtlist_add(src->es->res.n.if_list, src->ifp);
    }
    if (!dest->ifp) {
      dest->ifp = malloc(sizeof(struct if_ip));
      memset(dest->ifp, 0, sizeof(struct if_ip));
      if (!dest->es->res.n.if_list) {
        dest->es->res.n.if_list = malloc(sizeof(struct adtlist));
        memset(dest->es->res.n.if_list, 0, sizeof(struct adtlist));
      }
      adtlist_add(dest->es->res.n.if_list, dest->ifp);
    }

    if (dest->ifp->assign_ip && src->ifp->assign_ip)
      continue;

    system("mysql -hlocalhost -udragon -pflame -e \"use dragon; select * from data_plane_blocks where in_use='no' limit 1;\" > /tmp/mysql.result");

    /* error if 
     * SQL_RESULT doesn't start with slash_30
     * return file format:
     * slash_30        in_use 
     * 140.173.96.32   no
     *
     */
    fp = fopen(SQL_RESULT, "r");
    if (!fp) {
      continue;
    }

    f_ret = fgets(line, 100, fp);
    if (!f_ret) {
      fclose(fp);
      zlog_err("master_lookup_assign_ip: sql server returns empty file");
      return 0;
    }

    if (strncmp(f_ret, "slash_30", 8) != 0) {
      fclose(fp);
      zlog_err("master_lookup_assign_ip: sql server returns error"); 
      return 0;
    }

    f_ret = fgets(line, 100, fp);
    if (!f_ret) {
      fclose(fp);
      zlog_err("master_lookup_assign_ip: sql server returns no slash_30");
      return 0;
    }

    token = strtok(f_ret, "\t");
    if (!token) { 
      fclose(fp);       
      zlog_err("master_lookup_assign_ip: sql server returns no slash_30");
      return 0;
    }
    zlog_info("for link %s, assign slash_30 %s", res->name, token);

    /* for src */
    mask.s_addr = inet_addr("0.0.0.1");
    ip.s_addr = inet_addr(token) | mask.s_addr;
    zlog_info("for src, assign %s", inet_ntoa(ip));
    sprintf(addr, "%s/30", inet_ntoa(ip));
    src->ifp->assign_ip = strdup(addr);
 
    mask.s_addr = inet_addr("0.0.0.2");
    ip.s_addr = inet_addr(token) | mask.s_addr;
    zlog_info("for dest, assign %s", inet_ntoa(ip)); 
    sprintf(addr, "%s/30", inet_ntoa(ip));
    dest->ifp->assign_ip = strdup(addr);

    sprintf(addr, "mysql -hlocalhost -udragon -pflame  -e \"use dragon; update data_plane_blocks set in_use='yes' where slash_30='%s';\"", token);

    system(addr);
    fclose(fp);
  }

  return 1;
}

static int
master_cleanup_assign_ip()
{
  struct adtlistnode *curnode;
  struct resource *res;
  struct endpoint *src;
  char *c, addr[200];
  struct in_addr mask, ip, slash_30;

  zlog_info("master_cleanup_assign_ip() ...");
  if (!glob_app_cfg) 
    return 0;
  if (!glob_app_cfg->link_list)
    return 0;

  for (curnode = glob_app_cfg->link_list->head;
        curnode;
        curnode = curnode->next) {
    res = (struct resource*)curnode->data;   

    src = res->res.l.src;

    if (!src->ifp)
      continue;
    if (!src->ifp->assign_ip)
      continue;
    
    c = strstr(src->ifp->assign_ip, "/");
    if (!c)
      continue;
    *c = '\0';
    ip.s_addr = inet_addr(src->ifp->assign_ip);
    mask.s_addr = inet_addr("255.255.255.252");
    slash_30.s_addr = ip.s_addr & mask.s_addr;

    /* don't care if this addr is from SQL database or not, just remove it
     */
    sprintf(addr, "mysql -hlocalhost -udragon -pflame  -e \"use dragon; update data_plane_blocks set in_use='no' where slash_30='%s';\"", inet_ntoa(slash_30));
    system(addr);
  }
  
  return 1;
}

static void
master_check_command()
{
  struct adtlistnode *curnode;
  struct resource *res, *link;
  char *command, *s, *d, *e, *f, *token;
  static char newcomm[300];
  struct endpoint *ep;
  int stop, change, nospace;

  zlog_info("master_check_command() ...");

  if (!glob_app_cfg)
    return;

  if (!glob_app_cfg->node_list)
    return;

  memset(newcomm, 0, 300);
  for (curnode = glob_app_cfg->node_list->head;
	curnode;
	curnode = curnode->next) {
    res = (struct resource*) curnode->data;

    if (!res->res.n.command)
      continue;

    /* look for any "$<link_name>.src" "$<link_name>.dest"
     */
    command = res->res.n.command;
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
      else if ((link = search_link_by_name(glob_app_cfg, e+1)) == NULL) 
	stop = 1;
      else { 
	if (strcmp(d+1, "src") == 0) {
	  ep = link->res.l.src; 
	  f = d+4;
	} else if (strcmp(d+1, "dest") == 0) {
	  ep = link->res.l.dest;
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
      zlog_info("new command for %s: %s", res->name, newcomm);
    free(res->res.n.command);
    res->res.n.command = strdup(newcomm);
    memset(newcomm, 0, 300);
  } 
}
