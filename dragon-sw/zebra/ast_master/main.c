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

struct thread_master *master; /* master = dmaster.master */
extern char *status_type_details[];
extern char *action_type_details[];

extern int master_process_id(char*);
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
-i, --input file   Runs in standalone mode\n\
-h, --help	 Display this help and exit\n\
\n", progname); 
    
  exit (status);
}

int
master_process_setup_resp()
{
  char path[100];
  struct stat fs;
  struct application_cfg working_app_cfg;
  struct adtlistnode *work_node, *glob_node; 
  struct resource *glob_node_cfg;
  struct resource *work_link_cfg, *glob_link_cfg;
  int complete = 1;

  if (glob_app_cfg.ast_id == NULL) {
    sprintf(glob_app_cfg.details, "For SETUP_RESP, ast_id has to be set");
    return 0;
  }

  zlog_info("Processing ast_id: %s, action: SETUP_RESP", glob_app_cfg.ast_id);
  sprintf(path, "%s/%s/final.xml", AST_DIR, glob_app_cfg.ast_id);
  if (stat(path, &fs) == -1) {
    sprintf(glob_app_cfg.details, "Can't locate the ast_id final file"); 
    return 0;
  }

  memcpy (&working_app_cfg, &glob_app_cfg, sizeof(struct application_cfg));
  memset(&glob_app_cfg, 0, sizeof(struct application_cfg));

  if (master_final_parser(path, MASTER) != 1) {
    memcpy (&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "didn't parse the ast_id file successfully");
    glob_app_cfg.status = AST_FAILURE;
    return 0;
  }

  if (glob_app_cfg.action == RELEASE_RESP) {
    free_application_cfg(&glob_app_cfg);
    memcpy (&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "ast_id has received RELEASE_REQ already");
    glob_app_cfg.status = AST_FAILURE;
    return 0;
  }

  if (glob_app_cfg.action == AST_COMPLETE) {
    free_application_cfg(&glob_app_cfg);
    memcpy (&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "ast_id has send AST_COMPLETE already");
    glob_app_cfg.status = AST_FAILURE;
    return 0;
  }

  if (glob_app_cfg.action == APP_COMPLETE) {
    memcpy (&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "ast_id has send APP_COMPLETE already");
    glob_app_cfg.status = AST_FAILURE;
    return 0;
  }

  glob_app_cfg.status = AST_FAILURE;
  glob_app_cfg.action = SETUP_RESP;

  if (glob_app_cfg.link_list == NULL &&
	glob_app_cfg.node_list == NULL) {
    zlog_warn("The received SETUP_RESP message has no resources");
    free_application_cfg(&glob_app_cfg);
    free_application_cfg(&working_app_cfg);
    glob_app_cfg.org_action = SETUP_RESP;
    return 0;
  }

  /* now has to distinguish where this request is coming from,
   * node_agent or link_agent (dragond)
   */
  if (working_app_cfg.link_list == NULL) {
    /* error here; only link_agent will send async SETUP_RESP */
    zlog_err("Recived async SETUP_RESP from node_agent");
    free_application_cfg(&glob_app_cfg);
    free_application_cfg(&working_app_cfg);
    glob_app_cfg.org_action = SETUP_RESP;
    return 0;
  } else {
    /* this is from link_agent */
    for (work_node = working_app_cfg.link_list->head;
	 work_node;
	 work_node = work_node->next) {
      work_link_cfg = (struct resource*)work_node->data;
      zlog_info("SETUP_RESP msg from link_agent %s", work_link_cfg->res.l.dragon->name);

      for (glob_node = glob_app_cfg.link_list->head;
	   glob_node;
	   glob_node = glob_node->next) {
	glob_link_cfg = (struct resource*)glob_node->data;
	if (strcmp(work_link_cfg->res.l.lsp_name, glob_link_cfg->res.l.lsp_name) == 0) {
	  zlog_info("link %s return %s", work_link_cfg->name,
			status_type_details[work_link_cfg->status]);
	  glob_link_cfg->status = work_link_cfg->status;
	  break;
	}
      }
    }
  }

  /* working_app_cfg is the coming in app_cfg xml, the above code
   * has integrated this into final.xml
   */
  glob_app_cfg.org_action = SETUP_RESP;
  free_application_cfg(&working_app_cfg);

  for (glob_node = glob_app_cfg.node_list->head;
       glob_node && complete;
       glob_node = glob_node->next) {
    glob_node_cfg = (struct resource*)glob_node->data;

    if (glob_node_cfg->status != AST_SUCCESS)
      complete = 0;
  }

  if (glob_app_cfg.link_list != NULL) {
    for (glob_node = glob_app_cfg.link_list->head;
	glob_node && complete;
	glob_node = glob_node->next) {
      glob_link_cfg = (struct resource*)glob_node->data;
      if (glob_link_cfg->status != AST_SUCCESS)
	complete = 0;
    }
  }

  if (complete)
    glob_app_cfg.status = AST_SUCCESS;
  else
    glob_app_cfg.status = AST_FAILURE;

  /* anyway, now, it's time to save the whole file again
   */
  integrate_result();

  return 1;
}

int
master_process_app_complete()
{
  char path[100], newpath[100];
  struct stat fs;
  struct application_cfg working_app_cfg;
  struct adtlistnode *work_node, *glob_node; 
  struct resource *work_node_cfg, *glob_node_cfg; 
  struct resource *work_link_cfg, *glob_link_cfg;
  int complete = 1;

  glob_app_cfg.org_action = glob_app_cfg.action;

  if (glob_app_cfg.ast_id == NULL) {
    sprintf(glob_app_cfg.details, "For APP_COMPLETE, ast_id has to be set");
    return 0;
  }

  zlog_info("Processing ast_id: %s, action: APP_COMPLETE", glob_app_cfg.ast_id);
  sprintf(path, "%s/%s/final.xml", AST_DIR, glob_app_cfg.ast_id);
  if (stat(path, &fs) == -1) {
    sprintf(glob_app_cfg.details, "Can't locate the ast_id final file"); 
    return 0;
  }

  memcpy (&working_app_cfg, &glob_app_cfg, sizeof(struct application_cfg));
  memset(&glob_app_cfg, 0, sizeof(struct application_cfg));

  if (master_final_parser(path, MASTER) != 1) {
    memcpy (&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "didn't parse the ast_id file successfully");
    glob_app_cfg.status = AST_FAILURE;
    return 0;
  }

  if (glob_app_cfg.action == RELEASE_RESP) {
    free_application_cfg(&glob_app_cfg);
    memcpy (&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "ast_id has received RELEASE_REQ already");
    glob_app_cfg.status = AST_FAILURE;
    return 0;
  }

  /* as long as there is one AST_APP_COMPLETE comes back, change
   * the overall ast_atatus to AST_APP_COMPLETE
   */
  glob_app_cfg.status = AST_APP_COMPLETE;
  glob_app_cfg.action = APP_COMPLETE;
  newpath[0] = '\0';

  if (glob_app_cfg.link_list == NULL &&
	glob_app_cfg.node_list == NULL) {
    zlog_warn("The received APP_COMPLETE message has no resources");
    free_application_cfg(&glob_app_cfg);
    free_application_cfg(&working_app_cfg);
    glob_app_cfg.org_action = APP_COMPLETE;
    return 0;
  }

  /* now has to distinguish where this request is coming from,
   * node_agent or link_agent (dragond)
   */
  if (working_app_cfg.link_list == NULL) {
    /* this is from node_agent */
    for (work_node = working_app_cfg.node_list->head;
	 work_node;
	 work_node = work_node->next) {
      work_node_cfg = (struct resource*)work_node->data;
      zlog_info("APP_COMPLETE msg from node agent %s", work_node_cfg->name);

      for (glob_node = glob_app_cfg.node_list->head;
	   glob_node;
	   glob_node = glob_node->next) {
	glob_node_cfg = (struct resource*)glob_node->data;
	
	if (strcmp(work_node_cfg->name, glob_node_cfg->name) == 0) { 
	  glob_node_cfg->status = AST_APP_COMPLETE;
	  sprintf(newpath, "%s/%s/app_complete_node_agent_%s.xml", 
		AST_DIR, glob_app_cfg.ast_id, glob_node_cfg->name);

	  break;
	}
      }
    }	
  } else {
    /* this is from link_agent */
    for (work_node = working_app_cfg.link_list->head;
	 work_node;
	 work_node = work_node->next) {
      work_link_cfg = (struct resource*)work_node->data;
      zlog_info("APP_COMPLETE msg from link_agent %s", work_link_cfg->res.l.dragon->name);

      for (glob_node = glob_app_cfg.link_list->head;
	   glob_node;
	   glob_node = glob_node->next) {
	glob_link_cfg = (struct resource*)glob_node->data;
	if (strcmp(work_link_cfg->name, glob_link_cfg->name) == 0) {
	  glob_link_cfg->status = AST_APP_COMPLETE;
	  sprintf(newpath, "%s/%s/app_complete_link_agent_%s.xml", 
		AST_DIR, glob_app_cfg.ast_id, work_link_cfg->res.l.dragon->name);
	  break;
	}
      }
    }
  }

  /* working_app_cfg is the coming in app_cfg xml, the above code
   * has integrated this into final.xml
   */
  free_application_cfg(&working_app_cfg);

  for (glob_node = glob_app_cfg.node_list->head;
       glob_node && complete;
       glob_node = glob_node->next) {
    glob_node_cfg = (struct resource*)glob_node->data;

    if (glob_node_cfg->status != AST_APP_COMPLETE)
      complete = 0;
  }

  if (glob_app_cfg.link_list != NULL) {
    for (glob_node = glob_app_cfg.link_list->head;
	glob_node && complete;
	glob_node = glob_node->next) {
      glob_link_cfg = (struct resource*)glob_node->data;
      if (glob_link_cfg->status != AST_APP_COMPLETE)
	complete = 0;
    }
  }

  /* anyway, now, it's time to save the whole file again
   */
  print_final(path);
  link(AST_XML_RECV, newpath);

  if (complete) {
    /* if APP_COMPLETE has been received from all, can call release it 
     */
    zlog_info("ast_master has received all APP_COMPLETE, now do RELEASE_REQ");
    glob_app_cfg.action = RELEASE_REQ;
    if (master_process_release_req() == 0) {
      return 0;
    }
  }

  return 1;
}

int
master_process_setup_req()
{
  char directory[100];
  char newpath[105];
  
  glob_app_cfg.ast_id = generate_ast_id(ID_SETUP);
  glob_app_cfg.org_action = glob_app_cfg.action;
  zlog_info("Processing ast_id: %s, action: SETUP_REQ", glob_app_cfg.ast_id);

  strcpy(directory, AST_DIR);
  if (mkdir(directory, 0755) == -1 && errno != EEXIST) {
   zlog_err("Can't create directory %s", directory);
   return 0;
  }

  sprintf(directory+strlen(directory), "/%s", glob_app_cfg.ast_id);
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

  sprintf(newpath, "%s/setup_original.xml", directory);

  if (rename(AST_XML_RECV, newpath) == -1) 
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	     AST_XML_RECV, newpath, errno, strerror(errno));

  glob_app_cfg.status = AST_SUCCESS;
  if (send_task_to_node_agent() == 0) 
    glob_app_cfg.status = AST_FAILURE;

  if (send_task_to_link_agent() == 0) 
    glob_app_cfg.status = AST_FAILURE;

  glob_app_cfg.action = SETUP_RESP;
  integrate_result();

  return 1;
}

int
master_process_query_req()
{
  char directory[100];
  char newpath[105];
  
  glob_app_cfg.ast_id = generate_ast_id(ID_QUERY);
  zlog_info("Processing ast_id: %s, action: QUERY_REQ", glob_app_cfg.ast_id);
  glob_app_cfg.org_action = glob_app_cfg.action;

  strcpy(directory, AST_DIR);
  mkdir(directory, 0755);

  sprintf(directory+strlen(directory), "/%s", glob_app_cfg.ast_id);
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

  sprintf(newpath, "%s/%s/query_original.xml", AST_DIR, glob_app_cfg.ast_id);
  if (rename(AST_XML_RECV, newpath) == -1) 
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	     AST_XML_RECV, newpath, errno, strerror(errno));

  glob_app_cfg.status = AST_SUCCESS;

  if (send_task_to_node_agent() == 0)
    glob_app_cfg.status = AST_FAILURE;

  if (send_task_to_link_agent() == 0) 
    glob_app_cfg.status = AST_FAILURE;

  glob_app_cfg.action = QUERY_RESP;
  integrate_result();

  return 1;
}
 
int 
master_process_release_req()
{
  char path[100];
  struct stat fs;
  struct application_cfg working_app_cfg;
  int file_mode = 0;

  glob_app_cfg.org_action = glob_app_cfg.action;
  if (glob_app_cfg.ast_id == NULL) {

    sprintf(glob_app_cfg.details, "ast_id is not set");
    if (glob_app_cfg.link_list == NULL &&
	glob_app_cfg.node_list == NULL) {
      zlog_err("no node or link in file; nothing to release");
      return 0;
    }

    glob_app_cfg.ast_id = generate_ast_id(ID_TEMP);
    file_mode = 1;
 
  } else {

    zlog_info("Processing ast_id: %s, action: RELEASE_REQ", glob_app_cfg.ast_id);

    sprintf(path, "%s/%s/final.xml", AST_DIR, glob_app_cfg.ast_id);
    if (stat(path, &fs) == -1) {
      zlog_info("Can't locate the ast_id final file; use incoming file as final.xml"); 
      file_mode = 1;
    } else {
  
      memcpy(&working_app_cfg, &glob_app_cfg, sizeof(struct application_cfg));
      memset(&glob_app_cfg, 0, sizeof(struct application_cfg));
    
      if (master_final_parser(path, MASTER) != 1) {
        sprintf(glob_app_cfg.details, "didn't parsed the ast_id file successfully");
        memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
        glob_app_cfg.status = AST_FAILURE;
        return 0;
      }
    
      if (glob_app_cfg.action == RELEASE_RESP) {
        free_application_cfg(&glob_app_cfg);
        sprintf(glob_app_cfg.details, "ast_id has received RELEASE_REQ already");
        sprintf(glob_app_cfg.details+strlen(glob_app_cfg.details),
    		"the previous RELEASE_REQ returns %s", status_type_details[glob_app_cfg.status]);
        memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
        glob_app_cfg.status = AST_FAILURE;
        return 0;
      }
    
      glob_app_cfg.action = working_app_cfg.action;
      free_application_cfg(&working_app_cfg);
    }
  }

  if (file_mode) {
    char directory[100];

    strcpy(directory, AST_DIR);
    if (mkdir(directory, 0755) == -1 && errno != EEXIST) {
     zlog_err("Can't create directory %s", directory);
     return 0;
    }
  
    sprintf(directory+strlen(directory), "/%s", glob_app_cfg.ast_id);
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
  sprintf(path, "%s/%s/release_original.xml", AST_DIR, glob_app_cfg.ast_id);
  if (rename(AST_XML_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	     AST_XML_RECV, path, errno, strerror(errno));

  glob_app_cfg.status = AST_SUCCESS;
  if (send_task_to_node_agent() == 0) 
    glob_app_cfg.status = AST_FAILURE;

  if (send_task_to_link_agent() == 0) 
    glob_app_cfg.status = AST_FAILURE;

  glob_app_cfg.action = RELEASE_RESP;
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
  if (topo_xml_parser(AST_XML_RECV, MASTER) == 0) { 
    zlog_err("master_process_topo: topo_xml_parser() failed"); 
    return 0;
  }

  if (topo_validate_graph(MASTER) == 0) {
    sprintf(glob_app_cfg.details, "Failed at topo_validate_graph()");
    return 0;
  }

  if ( glob_app_cfg.action == SETUP_REQ)
    master_process_setup_req();
  else if (glob_app_cfg.action == RELEASE_REQ)
    master_process_release_req();
  else if ( glob_app_cfg.action == QUERY_REQ)
    master_process_query_req();
  else if ( glob_app_cfg.action == APP_COMPLETE) 
    master_process_app_complete();
  else if ( glob_app_cfg.action == SETUP_RESP)
    master_process_setup_resp();


  return 1;
}

/* let ast_masterd to listen on a certain tcp port;
 */
void
master_server_init()
{
  int servSock;
  int clntSock;
  struct sockaddr_in ServAddr;
  struct sockaddr_in clntAddr;
  unsigned int clntLen;
  int recvMsgSize;
  char buffer[4001];
  FILE* fp;
  int fd, total;
  static struct stat file_stat;
 
  if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    zlog_err("master_server_init: socket() failed");
    exit(EXIT_FAILURE);
  }

  memset(&ServAddr, 0, sizeof(ServAddr));
  ServAddr.sin_family = AF_INET;
  ServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  ServAddr.sin_port = htons(MASTER_PORT);

  if (bind(servSock, (struct sockaddr*)&ServAddr, sizeof(ServAddr)) < 0) {
    zlog_err("master_server_init: bind() failed");
    exit(EXIT_FAILURE);
  }

  if (listen(servSock, MAXPENDING) < 0) {
    zlog_err("master_server_init: listen() failed");
    exit(EXIT_FAILURE);
  }

  /* running a forever loop to accept connections */
  while (1) { 
    unlink(AST_XML_RESULT);
    unlink(AST_XML_RECV);
    clntLen = sizeof(clntAddr);
    
    if ((clntSock = accept(servSock, (struct sockaddr*)&clntAddr, &clntLen)) < 0) {
      zlog_err("master_server_init: accept() failed");
      exit(EXIT_FAILURE);
    }

    recvMsgSize = recv(clntSock, buffer, 4000, 0);
    if (recvMsgSize < 0) {
      zlog_err("master_server_init: recv() failed");
      exit(EXIT_FAILURE);
    }

    fp = fopen(AST_XML_RECV, "w");
    buffer[recvMsgSize]='\0';

    /* dump the file to a well-known location */
    fprintf(fp, "%s",  buffer);
    fflush(fp);
    fclose(fp);

    bzero(&glob_app_cfg, sizeof(struct application_cfg));

    zlog_info("Handling client %s ...", inet_ntoa(clntAddr.sin_addr));
    glob_app_cfg.xml_type = xml_parser(AST_XML_RECV);

    switch(glob_app_cfg.xml_type) {

      case TOPO_XML:

	zlog_info("XML_TYPE: TOPO_XML");
	master_process_topo(AST_XML_RECV);
	if (glob_app_cfg.details[0] != '\0')
	  zlog_err(glob_app_cfg.details);

	if (glob_app_cfg.org_action != APP_COMPLETE && 
	    glob_app_cfg.org_action != AST_COMPLETE && 
	    glob_app_cfg.org_action != SETUP_RESP) {
	  /* send result back to ast_master
	   */
	  if (stat(AST_XML_RESULT, &file_stat) == -1) {
	
	    /* build the xml result file */ 
	    glob_app_cfg.status = AST_FAILURE; 
	    print_error_response(AST_XML_RESULT); 
	  }
	}

	free_application_cfg(&glob_app_cfg);    
	break;

      case ID_XML:

	zlog_info("XML_TYPE: ID_XML");
	master_process_id(AST_XML_RECV);

	break;

      default:

	zlog_info("XML_TYPE: can't be determined");

    }

    /* returning to the client */
    fd = open(AST_XML_RESULT, O_RDONLY);
#ifdef __FreeBSD__
    total = sendfile(fd, clntSock, 0, 0, NULL, NULL, 0);
    zlog_info("sendfile() returns %d", total);
#else
    if (fstat(fd, &file_stat) == -1) {
      zlog_err("master_server_init: fstat() failed on %s", AST_XML_RESULT);
      exit(1);
    }
    total = sendfile(clntSock, fd, 0, file_stat.st_size);
#endif
    if (total < 0)
      zlog_err("master_server_init: sendfile() failed; error = %d(%s)\n", 
        		errno, strerror(errno)); 
    close(clntSock);
    zlog_info("DONE");
  }
}

/* ASP Masterd main routine */
int
main(int argc, char* argv[])
{
  char *progname;
  char *p;
  int daemon_mode = 0;

  progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);

  while (1) {
    int opt;

    opt = getopt_long (argc, argv, "dlf:hA:P:v", ast_master_opts, 0);
    if (argc > 2) {
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
      default: 
	usage (progname, 1);
    }
  }

  /* Change to the daemon program. */
  if (daemon_mode)
    daemon (0, 0);

  zlog_default = openzlog (progname, ZLOG_NOLOG, ZLOG_ASTB,
			   LOG_CONS|LOG_NDELAY|LOG_PID, LOG_DAEMON);
  zlog_set_file(zlog_default, ZLOG_FILE, "/var/log/ast_master.log");

  /* print banner */
  zlog_info("AST_MASTER starts: pid = %d", getpid());

  /* parse the service_def.xml where indicates where to locate
   * resource agency 
   */
  if (service_xml_parser(XML_SERVICE_DEF_FILE) == 0) 
    zlog_err("main: service_xml_parser() error");
  
  /* parse all service_template.xml and build the default
   * struct for each service template for later use
  if (master_template_parser() == 0) {
    zlog_err("master_template_parser(); exiting ...");
    exit(EXIT_FAILURE);
  }
  */

  memset(&glob_app_cfg, 0, sizeof(struct application_cfg));
  master_server_init();

/* temporarily here */

  return (EXIT_SUCCESS);
}

void 
set_alllink_fail(struct adtlist *list, char* err_message)
{
  struct adtlistnode *curnode;
  struct resource *mylink;

  if (list == NULL) 
    return;
  for (curnode = list->head;
	curnode;
	curnode = curnode->next) {
    mylink = (struct resource*) curnode->data; 
    mylink->status = AST_FAILURE;
    if (err_message != NULL) 
      mylink->agent_message = strdup(err_message);
  }
}

void
process_link_setup_result(struct application_cfg *working_app_cfg, struct resource* srcnode)
{
  struct adtlistnode *curnode, *curnode1;
  struct resource *old_link, *new_link;

  if (glob_app_cfg.action != SETUP_RESP) {
    zlog_err("process_link_setup_result: message type is not SETUP_RESP");
    set_alllink_fail(srcnode->res.n.link_list, "returning message type is not SETUP_RESP");
    working_app_cfg->status = AST_FAILURE;
    return;
  }

  if (glob_app_cfg.link_list == NULL) {
    zlog_err("process_link_setup_result: no link in the link_agent setup response file");
    working_app_cfg->status = AST_FAILURE;
    return;
  }

  for (curnode = glob_app_cfg.link_list->head;
       curnode;
       curnode = curnode->next) {
    new_link = (struct resource*) curnode->data;

    for (curnode1 = srcnode->res.n.link_list->head;
	 curnode1;
	 curnode1 = curnode1->next) {
      old_link = (struct resource*) curnode1->data;

      if (strcasecmp(new_link->name, old_link->name) == 0) {
	strcpy(old_link->res.l.lsp_name, new_link->res.l.lsp_name);
	old_link->status = new_link->status;
        if (old_link->agent_message != NULL) {
          free(old_link->agent_message);
	  old_link->agent_message = NULL;
        }
        if (new_link->agent_message) {
          old_link->agent_message = new_link->agent_message;
          new_link->agent_message = NULL;
        } else if (glob_app_cfg.details[0] != '\0')
          old_link->agent_message = strdup(glob_app_cfg.details);
	break;
      }
    }
    if (curnode1 == NULL) 
      zlog_warn("unknown link %s in the respones file from link_agent",
		new_link->name);
  }

  if (glob_app_cfg.status != AST_SUCCESS)
    working_app_cfg->status = AST_FAILURE;
}

void
process_link_release_result(struct application_cfg *working_app_cfg, 
			    struct resource *srcnode)
{
  struct adtlistnode *curnode, *curnode1;
  struct resource *old_link, *new_link;

  if (glob_app_cfg.action != RELEASE_RESP) {
    zlog_err("process_link_release_result: message type is not RELEASE_RESP");
    set_alllink_fail(srcnode->res.n.link_list, "returning message type is not RELEASE_RESP");
    working_app_cfg->status = AST_FAILURE;
    return;
  }

  if (glob_app_cfg.link_list == NULL) {
    zlog_err("process_link_release_result: no link in the link_agent setup response file");
    set_alllink_fail(srcnode->res.n.link_list, (glob_app_cfg.details[0]=='\0') ?
		"no link in the link_agent setup response file" :
		glob_app_cfg.details);
    working_app_cfg->status = AST_FAILURE;
    return;
  }

  for (curnode = glob_app_cfg.link_list->head;
	curnode;
	curnode = curnode->next) {
    new_link = (struct resource*) curnode->data;

    for (curnode1 = srcnode->res.n.link_list->head;
	 curnode1; 
	 curnode1 = curnode1->next) { 
      old_link = (struct resource*) curnode1->data;

      if (strcasecmp(new_link->name, old_link->name) == 0) {
	strcpy(old_link->res.l.lsp_name, new_link->res.l.lsp_name);
	old_link->status = new_link->status; 
	if (old_link->agent_message != NULL) {
	  free(old_link->agent_message); 
	  old_link->agent_message = NULL;
        }
        if (new_link->agent_message) { 
	  old_link->agent_message = new_link->agent_message; 
	  new_link->agent_message = NULL; 
	} else if (glob_app_cfg.details[0] != '\0') 
	  old_link->agent_message = strdup(glob_app_cfg.details);
	break; 
      }
    }
    if (curnode1 == NULL) 
	zlog_warn("return message from %s contains an unknown link %s",
		  srcnode->name, new_link->name);
  }

  if (glob_app_cfg.status != AST_SUCCESS)
    working_app_cfg->status = AST_FAILURE;
}

void
process_link_query_result(struct application_cfg *working_app_cfg, 
			  struct resource *srcnode)
{
  if (glob_app_cfg.action != QUERY_RESP) {
    zlog_err("process_link_query_result: message type is not QUERY_RESP");
    set_alllink_fail(srcnode->res.n.link_list, "returning message type is not QUERY_RESP");
    working_app_cfg->status = AST_FAILURE;
    return;
  }

  zlog_info("return: %d link(s) for node %s", 
	adtlist_getcount(glob_app_cfg.link_list), srcnode->name);

  if (srcnode->agent_message != NULL) {
    free(srcnode->agent_message);
    srcnode->agent_message = NULL;
  }
  if (glob_app_cfg.details[0] != '\0') 
    srcnode->agent_message = strdup(glob_app_cfg.details);
  
  srcnode->res.n.link_list = glob_app_cfg.link_list;
  glob_app_cfg.link_list = NULL; /* so that it won't be freed */
}

int
master_compose_link_request(struct application_cfg app_cfg, 
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
  
  fprintf(send_file, "<topology ast_id=\"%s\" action=\"%s\">\n", app_cfg.ast_id, action_type_details[app_cfg.action]);

  if (app_cfg.action == QUERY_REQ) {
    print_node(send_file, srcnode);
    fprintf(send_file, "</topology>");
    fflush(send_file);
    fclose(send_file);
    return 1;
  }

  if (app_cfg.action != SETUP_REQ) {
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
	  print_node(send_file, link->res.l.src->es);
	print_node(send_file, link->res.l.dest->es);
	print_link(send_file, link);
 	break;	

      case vlsr_vlsr:
	print_node(send_file, link->res.l.dest->es);
	print_link(send_file, link);
	break;

      case vlsr_es:
	if (link->res.l.src->vlsr && link->res.l.dest->es) {
	  print_node(send_file, link->res.l.dest->es);
	} else if (link->res.l.src->es && link->res.l.dest->vlsr) {
	  if (link->res.l.src->proxy)
	    print_node(send_file, link->res.l.src->es);
	  print_node(send_file, link->res.l.dest->vlsr);
	  print_link(send_file, link);
	}
    } 
  }

  fprintf(send_file, "</topology>");
  fflush(send_file);
  fclose(send_file);

  return 1;
}

int
master_compose_node_request(struct application_cfg app_cfg, 
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

  fprintf(send_file, "<topology ast_id=\"%s\" action=\"%s\">\n", app_cfg.ast_id, action_type_details[app_cfg.action]);
  print_node(send_file, srcnode);

  fprintf(send_file, "</topology>");
  fflush(send_file);
  fclose(send_file);

  return 1;
}

int
send_task_to_link_agent()
{
  struct adtlistnode *curnode, *linknode;
  struct resource *link;
  struct resource *srcnode;
  int sock;
  static char buffer[RCVBUFSIZE];
  static char send_buf[SENDBUFSIZE];
  static char ret_buf[SENDBUFSIZE];
  int bytesRcvd, total, ret_value = 1;
  FILE* ret_file = NULL;
  char directory[80];
  char newpath[105];
  char path_prefix[100];
  struct application_cfg working_app_cfg;

  zlog_info("send_task_to_link_agent ...");
  strcpy(directory, AST_DIR);
  sprintf(directory+strlen(directory), "/%s", glob_app_cfg.ast_id);
  
  if (glob_app_cfg.action == SETUP_REQ) 
    sprintf(path_prefix, "%s/setup_", directory);
  else if (glob_app_cfg.action == RELEASE_REQ)
    sprintf(path_prefix, "%s/release_", directory);
  else if (glob_app_cfg.action == QUERY_REQ)
    sprintf(path_prefix, "%s/query_", directory);
       
  memcpy (&working_app_cfg, &glob_app_cfg, sizeof(struct application_cfg));
  memset(&glob_app_cfg, 0, sizeof(struct application_cfg));
  memset(buffer, 0, RCVBUFSIZE);
  memset(send_buf, 0, SENDBUFSIZE);
  for (curnode = working_app_cfg.node_list->head;
       curnode;
       curnode = curnode->next) {
    srcnode = (struct resource*)(curnode->data); 

   if (!srcnode->res.n.link_list &&
	working_app_cfg.action != QUERY_REQ) {
	
      /* No task need to be sent to this link_agent (dragond); skip 
       */
      continue;
    }

    sprintf(newpath, "%srequest_%s.xml", path_prefix, srcnode->name);
    if (master_compose_link_request(working_app_cfg, newpath, srcnode) == 0) {
      ret_value = 0;
      continue;
    }

    zlog_info("sending request to %s (%s:%d)", srcnode->name, srcnode->res.n.ip, DRAGON_XML_PORT);
    sock = send_file_to_agent(srcnode->res.n.ip, DRAGON_XML_PORT, newpath);

    if (sock == -1)  {
      srcnode->status = AST_FAILURE;
      ret_value = 0;

      if (working_app_cfg.action == QUERY_REQ)
	continue;

      for (linknode = srcnode->res.n.link_list->head;
	   linknode;
	   linknode = linknode->next) {
	
	link = (struct resource*)linknode->data;
	link->status = AST_FAILURE;
	link->agent_message = strdup("Failed to connect to dragond");
      }

      continue;
    }

    total = 0;
    memset(ret_buf, 0, SENDBUFSIZE);
    while ((bytesRcvd = recv(sock, buffer, RCVBUFSIZE-1, 0))  > 0) {
       
      if (!total) { 
	zlog_info("Received confirmation from %s", srcnode->name); 
	sprintf(newpath, "%sresponse_%s.xml", path_prefix, srcnode->name);
	ret_file = fopen(newpath, "w");
	if (!ret_file) {
	  zlog_err("send_task_to_link_agent: can't open %s; error = %d(%s)", 
			newpath, errno, strerror(errno));
	  continue;
	}
      } 
      total+=bytesRcvd;
      buffer[bytesRcvd] = '\0';
      sprintf(ret_buf+strlen(ret_buf), buffer);
      fprintf(ret_file, buffer);
    }

    if (total == 0) {
      zlog_err("No confirmation from %s", srcnode->name);
      ret_value = 0;
      srcnode->status = AST_UNKNOWN;
    } else {

      fflush(ret_file);
      fclose(ret_file);

      if (topo_xml_parser(newpath, MASTER) == 0) {
	zlog_err("%s is not parsed correctly", newpath);
	srcnode->status = AST_UNKNOWN;
	ret_value = 0;
      } else if (topo_validate_graph(MASTER) == 0) {
	zlog_err("%s failed at validation", newpath);
	srcnode->status = AST_UNKNOWN;
	ret_value = 0;
      } else {
	if (working_app_cfg.action == SETUP_REQ)
	  process_link_setup_result(&working_app_cfg, srcnode);
	else if (working_app_cfg.action == RELEASE_REQ)
	  process_link_release_result(&working_app_cfg, srcnode);
	else if (working_app_cfg.action == QUERY_REQ)
	  process_link_query_result(&working_app_cfg, srcnode);
      }

      zlog_info("%s returns %s", srcnode->name, 
			status_type_details[glob_app_cfg.status]);	
      free_application_cfg(&glob_app_cfg);
    } 

    close(sock);
  }

  memcpy (&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
  return ret_value;
}

void 
integrate_result()
{
  struct adtlistnode *curnode, *linknode;
  struct resource *srcnode;
  struct resource *mylink;
  int sock, success;
  char directory[80];
  char newpath[105];
  char path_prefix[100];

  sprintf(directory, "%s/%s", AST_DIR, glob_app_cfg.ast_id);
  if (glob_app_cfg.action == SETUP_RESP)
    sprintf(path_prefix, "%s/setup_", directory);
  else if (glob_app_cfg.action == RELEASE_RESP)
    sprintf(path_prefix, "%s/release_", directory);
  else if (glob_app_cfg.action == QUERY_RESP)
    sprintf(path_prefix, "%s/query_", directory);

  sprintf(newpath, "%sfinal.xml", path_prefix);
  print_final(newpath);

  if (glob_app_cfg.status == AST_SUCCESS && 
      glob_app_cfg.action == SETUP_RESP) {
    char path[100];

    glob_app_cfg.action = AST_COMPLETE;
    /* create ast_complete.xml for record */
    sprintf(path, "%s/%s/ast_complete.xml", AST_DIR, glob_app_cfg.ast_id);
    print_final(path);

    zlog_info("Receiving AST_SUCCESS from all resouce in SETUP_REQ; now, do AST_COMPLETE");
    /* sending to node first */
    for (curnode = glob_app_cfg.node_list->head, success = 0;
	 curnode; 
	 curnode = curnode->next) {
      srcnode = (struct resource*) curnode->data;

#ifdef FIONA
      if (srcnode->agent_port == 0) 
	continue;
#endif

      /* AST_COMPLETE is a special case here;
       * all other message, we would wait for the return message, but
       * this one we won't
       */
      zlog_info("sending request to node_agent at %s (%s:%d)", 
		srcnode->name, srcnode->res.n.ip, NODE_AGENT_PORT);
      sock = send_file_to_agent(srcnode->res.n.ip, NODE_AGENT_PORT, path);
      if (sock == -1) {
	srcnode->status = AST_FAILURE;
	srcnode->agent_message = strdup("Failed to connect to node_agent for AST_COMPETE msg");
      } else {
	srcnode->status = AST_SUCCESS;
	success++;
	close(sock);
      }

      if (srcnode->res.n.link_list != NULL) {
       
	zlog_info("sending request to link_agent at %s (%s:%d)", 
		  srcnode->name, srcnode->res.n.ip, DRAGON_XML_PORT);
	sock = send_file_to_agent(srcnode->res.n.ip, DRAGON_XML_PORT, path);
	for (linknode = srcnode->res.n.link_list->head; 
	     linknode; 
	     linknode = linknode->next) { 
	  mylink = (struct resource*) linknode->data; 
	  if (sock == -1) { 
	    mylink->status = AST_FAILURE; 
	    mylink->agent_message = strdup("Failed to connect to link_agent for AST_COMPLETE msg"); 
	  } else {
	    mylink->status = AST_SUCCESS; 
	    success++; 
	  }
	}
        if (sock != -1)
	  close(sock);
      }
    }

    if (success == adtlist_getcount(glob_app_cfg.node_list) +
		   adtlist_getcount(glob_app_cfg.link_list)) {
      zlog_info("AST_COMPLETE msg has been sent to all resources");
      glob_app_cfg.status = AST_SUCCESS;
    } else {
      zlog_info("AST_COMPLETE msg failes at some resources");
      glob_app_cfg.status = AST_FAILURE;
    }
  }

  /* "final.xml" is the latest file on this <ast_id>
   */
  if (glob_app_cfg.action != QUERY_RESP) { 
    sprintf(newpath, "%s/final.xml", directory); 
    print_final(newpath);
  }
 
  symlink(newpath, AST_XML_RESULT);
}

int
send_task_to_node_agent()
{
  struct adtlistnode *curnode;
  struct resource* srcnode, *newnode;
  int sock;
  static char buffer[RCVBUFSIZE];
  static char send_buf[SENDBUFSIZE];
  static char ret_buf[SENDBUFSIZE];
  int bytesRcvd, total, ret_value = 1;
  FILE* ret_file = NULL;
  char directory[80];
  char newpath[105];
  char path_prefix[100];
  struct application_cfg working_app_cfg;

  zlog_info("send_task_to_node_agent ...");  
  strcpy(directory, AST_DIR);
  sprintf(directory+strlen(directory), "/%s", glob_app_cfg.ast_id);

  if (glob_app_cfg.action == SETUP_REQ)
    sprintf(path_prefix, "%s/setup_node_", directory);
  else if (glob_app_cfg.action == RELEASE_REQ)
    sprintf(path_prefix, "%s/release_node_", directory);
  else if (glob_app_cfg.action == QUERY_REQ)
    sprintf(path_prefix, "%s/query_node_", directory);

  memcpy(&working_app_cfg, &glob_app_cfg, sizeof(struct application_cfg));
  memset(&glob_app_cfg, 0, sizeof(struct application_cfg));
  memset(buffer, 0, RCVBUFSIZE);
  memset(send_buf, 0, SENDBUFSIZE);

  for (curnode = working_app_cfg.node_list->head;
	curnode;
	curnode = curnode->next) {
    srcnode = (struct resource*)(curnode->data);

    sprintf(newpath, "%srequest_%s.xml", path_prefix, srcnode->name);
    if (master_compose_node_request(working_app_cfg, newpath, srcnode) == 0) {
      ret_value = 0;
      continue;
    }

    zlog_info("sending request to %s (%s:%d)",
		srcnode->name, srcnode->res.n.ip, NODE_AGENT_PORT);
    sock = send_file_to_agent(srcnode->res.n.ip, NODE_AGENT_PORT, newpath);
    if (sock == -1) {
      srcnode->status = AST_FAILURE;
      srcnode->agent_message = strdup("Failed to connect to node_agent");
      ret_value = 0;
      continue;
    }

    total = 0;
    memset(ret_buf, 0, SENDBUFSIZE);
    while ((bytesRcvd = recv(sock, buffer, RCVBUFSIZE-1, 0))  > 0) {

      if (!total) {
	zlog_info("Received confirmation from %s", srcnode->name);
	sprintf(newpath, "%sresponse_%s.xml", path_prefix, srcnode->name);
	ret_file = fopen(newpath, "w");
	if (!ret_file) {
	  zlog_err("send_task_to_link_agent: can't open %s; error = %d(%s)",
	                newpath, errno, strerror(errno));
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
      zlog_err("No confirmation from %s", srcnode->name);
      srcnode->status = AST_UNKNOWN;
      ret_value = 0;
    } else {

      fflush(ret_file);
      fclose(ret_file);

      if (topo_xml_parser(newpath, MASTER) == 0) {
	zlog_err("%s is not parsed correctly", newpath);
	srcnode->status = AST_UNKNOWN;
	ret_value = 0;
      } else if (topo_validate_graph(MASTER) == 0) {
	zlog_err("%s failed at validation", newpath);
	srcnode->status = AST_UNKNOWN;
	ret_value = 0;
      } else {
	srcnode->status = glob_app_cfg.status;
        if (glob_app_cfg.details[0] != '\0')
	  srcnode->agent_message = strdup(glob_app_cfg.details);
        else if (glob_app_cfg.node_list != NULL) {
 	  newnode = (struct resource*)glob_app_cfg.node_list->head->data;
          if (srcnode->agent_message) {
	    free(srcnode->agent_message);
	    srcnode->agent_message = NULL;
	  }
	  if (newnode->agent_message != NULL) {
	    srcnode->agent_message = newnode->agent_message;
	    newnode->agent_message = NULL;
	  } else if (glob_app_cfg.details[0] != '\0') 
	    srcnode->agent_message = strdup(glob_app_cfg.details);
	}
      }

      zlog_info("%s returns %s", srcnode->name,
	                status_type_details[glob_app_cfg.status]);
      free_application_cfg(&glob_app_cfg);
    }
    close(sock);
  }

  memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
  return ret_value;
}
