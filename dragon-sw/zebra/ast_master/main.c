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
extern char *function_type_details[];

#define MAXPENDING      12
#define AST_XML_RESULT	"/usr/local/ast_master_ret.xml"
#define AST_XML_RECV	"/usr/local/ast_master_recv.xml"
#define RCVBUFSIZE      100
#define SENDBUFSIZE     5000

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
send_file_to_agent(char *ipadd, int port, char *newpath)
{
  int sock;
  struct sockaddr_in servAddr;
  int fd;
#ifndef __FreeBSD__
  static struct stat file_stat;
#endif

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
    zlog_err("xml_accept: fstat() failed on %s", newpath);
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

int
master_process_setup_resp()
{
  char path[100];
  struct stat fs;
  struct application_cfg working_app_cfg;
  struct adtlistnode *work_node, *glob_node; 
  struct node_cfg *glob_node_cfg;
  struct link_cfg *work_link_cfg, *glob_link_cfg;
  int complete = 1;

  if (glob_app_cfg.glob_ast_id == NULL) {
    sprintf(glob_app_cfg.details, "For SETUP_RESP, glob_ast_id has to be set");
    return 0;
  }

  zlog_info("Processing glob_ast_id: %s, ast_func: SETUP_RESP", glob_app_cfg.glob_ast_id);
  sprintf(path, "%s/%s/final.xml", AST_DIR, glob_app_cfg.glob_ast_id);
  if (stat(path, &fs) == -1) {
    sprintf(glob_app_cfg.details, "Can't locate the glob_ast_id final file"); 
    return 0;
  }

  memcpy (&working_app_cfg, &glob_app_cfg, sizeof(struct application_cfg));
  memset(&glob_app_cfg, 0, sizeof(struct application_cfg));

  if (topo_xml_parser(path, BRIEF_VERSION) != 1) {
    memcpy (&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "didn't parse the glob_ast_id file successfully");
    glob_app_cfg.ast_status = AST_FAILURE;
    return 0;
  }

  if (glob_app_cfg.function == RELEASE_RESP) {
    free_application_cfg(&glob_app_cfg);
    memcpy (&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "glob_ast_id has received RELEASE_REQ already");
    glob_app_cfg.ast_status = AST_FAILURE;
    return 0;
  }

  if (glob_app_cfg.function == AST_COMPLETE) {
    free_application_cfg(&glob_app_cfg);
    memcpy (&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "glob_ast_id has send AST_COMPLETE already");
    glob_app_cfg.ast_status = AST_FAILURE;
    return 0;
  }

  if (glob_app_cfg.function == APP_COMPLETE) {
    memcpy (&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "glob_ast_id has send APP_COMPLETE already");
    glob_app_cfg.ast_status = AST_FAILURE;
    return 0;
  }

  glob_app_cfg.ast_status = AST_FAILURE;
  glob_app_cfg.function = SETUP_RESP;

  if (glob_app_cfg.link_list == NULL &&
	glob_app_cfg.node_list == NULL) {
    zlog_warn("The received SETUP_RESP message has no resources");
    free_application_cfg(&glob_app_cfg);
    free_application_cfg(&working_app_cfg);
    glob_app_cfg.org_function = SETUP_RESP;
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
    glob_app_cfg.org_function = SETUP_RESP;
    return 0;
  } else {
    /* this is from link_agent */
    for (work_node = working_app_cfg.link_list->head;
	 work_node;
	 work_node = work_node->next) {
      work_link_cfg = (struct link_cfg*)work_node->data;
      zlog_info("SETUP_RESP msg from link_agent %s", work_link_cfg->src->name);

      for (glob_node = glob_app_cfg.link_list->head;
	   glob_node;
	   glob_node = glob_node->next) {
	glob_link_cfg = (struct link_cfg*)glob_node->data;
	if (strcmp(work_link_cfg->lsp_name, glob_link_cfg->lsp_name) == 0) {
	  zlog_info("link %s return %s", work_link_cfg->name,
			status_type_details[work_link_cfg->ast_status]);
	  glob_link_cfg->ast_status = work_link_cfg->ast_status;
	  break;
	}
      }
    }
  }

  /* working_app_cfg is the coming in app_cfg xml, the above code
   * has integrated this into final.xml
   */
  glob_app_cfg.org_function = SETUP_RESP;
  free_application_cfg(&working_app_cfg);

  for (glob_node = glob_app_cfg.node_list->head;
       glob_node && complete;
       glob_node = glob_node->next) {
    glob_node_cfg = (struct node_cfg*)glob_node->data;

    if (glob_node_cfg->ast_status != AST_SUCCESS)
      complete = 0;
  }

  if (glob_app_cfg.link_list != NULL) {
    for (glob_node = glob_app_cfg.link_list->head;
	glob_node && complete;
	glob_node = glob_node->next) {
      glob_link_cfg = (struct link_cfg*)glob_node->data;
      if (glob_link_cfg->ast_status != AST_SUCCESS)
	complete = 0;
    }
  }

  if (complete)
    glob_app_cfg.ast_status = AST_SUCCESS;
  else
    glob_app_cfg.ast_status = AST_FAILURE;

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
  struct node_cfg *work_node_cfg, *glob_node_cfg; 
  struct link_cfg *work_link_cfg, *glob_link_cfg;
  int complete = 1;

  glob_app_cfg.org_function = glob_app_cfg.function;

  if (glob_app_cfg.glob_ast_id == NULL) {
    sprintf(glob_app_cfg.details, "For APP_COMPLETE, glob_ast_id has to be set");
    return 0;
  }

  zlog_info("Processing glob_ast_id: %s, ast_func: APP_COMPLETE", glob_app_cfg.glob_ast_id);
  sprintf(path, "%s/%s/final.xml", AST_DIR, glob_app_cfg.glob_ast_id);
  if (stat(path, &fs) == -1) {
    sprintf(glob_app_cfg.details, "Can't locate the glob_ast_id final file"); 
    return 0;
  }

  memcpy (&working_app_cfg, &glob_app_cfg, sizeof(struct application_cfg));
  memset(&glob_app_cfg, 0, sizeof(struct application_cfg));

  if (topo_xml_parser(path, BRIEF_VERSION) != 1) {
    memcpy (&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "didn't parse the glob_ast_id file successfully");
    glob_app_cfg.ast_status = AST_FAILURE;
    return 0;
  }

  if (glob_app_cfg.function == RELEASE_RESP) {
    free_application_cfg(&glob_app_cfg);
    memcpy (&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "glob_ast_id has received RELEASE_REQ already");
    glob_app_cfg.ast_status = AST_FAILURE;
    return 0;
  }

  /* as long as there is one AST_APP_COMPLETE comes back, change
   * the overall ast_atatus to AST_APP_COMPLETE
   */
  glob_app_cfg.ast_status = AST_APP_COMPLETE;
  glob_app_cfg.function = APP_COMPLETE;
  newpath[0] = '\0';

  if (glob_app_cfg.link_list == NULL &&
	glob_app_cfg.node_list == NULL) {
    zlog_warn("The received APP_COMPLETE message has no resources");
    free_application_cfg(&glob_app_cfg);
    free_application_cfg(&working_app_cfg);
    glob_app_cfg.org_function = APP_COMPLETE;
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
      work_node_cfg = (struct node_cfg*)work_node->data;
      zlog_info("APP_COMPLETE msg from node agent %s", work_node_cfg->name);

      for (glob_node = glob_app_cfg.node_list->head;
	   glob_node;
	   glob_node = glob_node->next) {
	glob_node_cfg = (struct node_cfg*)glob_node->data;
	
	if (strcmp(work_node_cfg->name, glob_node_cfg->name) == 0) { 
	  glob_node_cfg->ast_status = AST_APP_COMPLETE;
	  sprintf(newpath, "%s/%s/app_complete_node_agent_%s.xml", 
		AST_DIR, glob_app_cfg.glob_ast_id, glob_node_cfg->name);

	  break;
	}
      }
    }	
  } else {
    /* this is from link_agent */
    for (work_node = working_app_cfg.link_list->head;
	 work_node;
	 work_node = work_node->next) {
      work_link_cfg = (struct link_cfg*)work_node->data;
      zlog_info("APP_COMPLETE msg from link_agent %s", work_link_cfg->src->name);

      for (glob_node = glob_app_cfg.link_list->head;
	   glob_node;
	   glob_node = glob_node->next) {
	glob_link_cfg = (struct link_cfg*)glob_node->data;
	if (strcmp(work_link_cfg->name, glob_link_cfg->name) == 0) {
	  glob_link_cfg->ast_status = AST_APP_COMPLETE;
	  sprintf(newpath, "%s/%s/app_complete_link_agent_%s.xml", 
		AST_DIR, glob_app_cfg.glob_ast_id, work_link_cfg->src->name);
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
    glob_node_cfg = (struct node_cfg*)glob_node->data;

    if (glob_node_cfg->ast_status != AST_APP_COMPLETE)
      complete = 0;
  }

  if (glob_app_cfg.link_list != NULL) {
    for (glob_node = glob_app_cfg.link_list->head;
	glob_node && complete;
	glob_node = glob_node->next) {
      glob_link_cfg = (struct link_cfg*)glob_node->data;
      if (glob_link_cfg->ast_status != AST_APP_COMPLETE)
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
    glob_app_cfg.function = RELEASE_REQ;
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
  
  glob_app_cfg.glob_ast_id = generate_ast_id(ID_SETUP);
  glob_app_cfg.org_function = glob_app_cfg.function;
  zlog_info("Processing glob_ast_id: %s, ast_func: SETUP_REQ", glob_app_cfg.glob_ast_id);

  strcpy(directory, AST_DIR);
  if (mkdir(directory, 0755) == -1 && errno != EEXIST) {
   zlog_err("Can't create directory %s", directory);
   return 0;
  }

  sprintf(directory+strlen(directory), "/%s", glob_app_cfg.glob_ast_id);
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

  glob_app_cfg.ast_status = AST_SUCCESS;
  if (send_task_to_node_agent() == 0) 
    glob_app_cfg.ast_status = AST_FAILURE;

  if (send_task_to_link_agent() == 0) 
    glob_app_cfg.ast_status = AST_FAILURE;

  glob_app_cfg.function = SETUP_RESP;
  integrate_result();

  return 1;
}

int
master_process_query_req()
{
  char directory[100];
  char newpath[105];
  
  glob_app_cfg.glob_ast_id = generate_ast_id(ID_QUERY);
  zlog_info("Processing glob_ast_id: %s, ast_func: QUERY_REQ", glob_app_cfg.glob_ast_id);
  glob_app_cfg.org_function = glob_app_cfg.function;

  strcpy(directory, AST_DIR);
  mkdir(directory, 0755);

  sprintf(directory+strlen(directory), "/%s", glob_app_cfg.glob_ast_id);
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

  sprintf(newpath, "%s/%s/query_original.xml", AST_DIR, glob_app_cfg.glob_ast_id);
  if (rename(AST_XML_RECV, newpath) == -1) 
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	     AST_XML_RECV, newpath, errno, strerror(errno));

  glob_app_cfg.ast_status = AST_SUCCESS;

  if (send_task_to_node_agent() == 0)
    glob_app_cfg.ast_status = AST_FAILURE;

  if (send_task_to_link_agent() == 0) 
    glob_app_cfg.ast_status = AST_FAILURE;

  glob_app_cfg.function = QUERY_RESP;
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

  glob_app_cfg.org_function = glob_app_cfg.function;
  if (glob_app_cfg.glob_ast_id == NULL) {

    sprintf(glob_app_cfg.details, "glob_ast_id is not set");
    if (glob_app_cfg.link_list == NULL &&
	glob_app_cfg.node_list == NULL) {
      zlog_err("no node or link in file; nothing to release");
      return 0;
    }

    glob_app_cfg.glob_ast_id = generate_ast_id(ID_TEMP);
    file_mode = 1;
 
  } else {

    zlog_info("Processing glob_ast_id: %s, ast_func: RELEASE_REQ", glob_app_cfg.glob_ast_id);

    sprintf(path, "%s/%s/final.xml", AST_DIR, glob_app_cfg.glob_ast_id);
    if (stat(path, &fs) == -1) {
      zlog_info("Can't locate the glob_ast_id final file; use incoming file as final.xml"); 
      file_mode = 1;
    } else {
  
      memcpy(&working_app_cfg, &glob_app_cfg, sizeof(struct application_cfg));
      memset(&glob_app_cfg, 0, sizeof(struct application_cfg));
    
      if (topo_xml_parser(path, BRIEF_VERSION) != 1) {
        sprintf(glob_app_cfg.details, "didn't parsed the glob_ast_id file successfully");
        memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
        glob_app_cfg.ast_status = AST_FAILURE;
        return 0;
      }
    
      if (glob_app_cfg.function == RELEASE_RESP) {
        free_application_cfg(&glob_app_cfg);
        sprintf(glob_app_cfg.details, "glob_ast_id has received RELEASE_REQ already");
        sprintf(glob_app_cfg.details+strlen(glob_app_cfg.details),
    		"the previous RELEASE_REQ returns %s", status_type_details[glob_app_cfg.ast_status]);
        memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
        glob_app_cfg.ast_status = AST_FAILURE;
        return 0;
      }
    
      glob_app_cfg.function = working_app_cfg.function;
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
  
    sprintf(directory+strlen(directory), "/%s", glob_app_cfg.glob_ast_id);
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
  sprintf(path, "%s/%s/release_original.xml", AST_DIR, glob_app_cfg.glob_ast_id);
  if (rename(AST_XML_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
	     AST_XML_RECV, path, errno, strerror(errno));

  glob_app_cfg.ast_status = AST_SUCCESS;
  if (send_task_to_node_agent() == 0) 
    glob_app_cfg.ast_status = AST_FAILURE;

  if (send_task_to_link_agent() == 0) 
    glob_app_cfg.ast_status = AST_FAILURE;

  glob_app_cfg.function = RELEASE_RESP;
  integrate_result();
  
  return 1;
}

int
master_process_xml(char* input_file)
{
  char system_call[100];

  if (strcasecmp(input_file, AST_XML_RECV) != 0) { 
    sprintf(system_call, "cp %s %s", input_file, AST_XML_RECV); 
    system(system_call);
  }

  /* after all the preparation, parse the application xml file 
   */ 
  if (topo_xml_parser(AST_XML_RECV, FULL_VERSION) == 0) { 
    zlog_err("master_process_xml: topo_xml_parser() failed"); 
    return 0;
  }

  if (glob_app_cfg.function != SETUP_REQ && 
      glob_app_cfg.function != RELEASE_REQ &&
      glob_app_cfg.function != QUERY_REQ &&
      glob_app_cfg.function != APP_COMPLETE &&
      glob_app_cfg.function != SETUP_RESP ) { 
    sprintf(glob_app_cfg.details, "invalid ast_func in xml file");
    return 0;
  }

  if (glob_app_cfg.function != SETUP_REQ &&
	glob_app_cfg.function != QUERY_REQ &&
	glob_app_cfg.glob_ast_id == NULL) {
    sprintf(glob_app_cfg.details, "glob_ast_id should be set in non-setup request case");
    return 0;
  }

  /* after parsing the xml file, we also need to call
   * resource_agency to get more details for different resource
   * i.e. location
   */
#ifdef RESOURCE_BROKER 
  if (glob_app_cfg.function == SETUP_REQ && master_locate_resource() == 0) {
    zlog_err("master_process_xml: master_locate_resource() failed");
    sprintf(glob_app_cfg.details, "master_locate_resource() failed");
    return 0;
  }
#endif

  /* build task list
   */
  if (master_validate_graph(0) == 0) {
    zlog_err("master_process_xml: master_validate_graph() failed");
    sprintf(glob_app_cfg.details, "master_validate_graph() failed");
    return 0;
  }

  if ( glob_app_cfg.function == SETUP_REQ)
    master_process_setup_req();
  else if (glob_app_cfg.function == RELEASE_REQ)
    master_process_release_req();
  else if ( glob_app_cfg.function == QUERY_REQ)
    master_process_query_req();
  else if ( glob_app_cfg.function == APP_COMPLETE) 
    master_process_app_complete();
  else if ( glob_app_cfg.function == SETUP_RESP)
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
    master_process_xml(AST_XML_RECV);
    if (glob_app_cfg.details[0] != '\0')
      zlog_err(glob_app_cfg.details);

    if (glob_app_cfg.org_function != APP_COMPLETE && 
	glob_app_cfg.org_function != AST_COMPLETE &&
	glob_app_cfg.org_function != SETUP_RESP) {
      /* send result back to ast_master
       */
      if (stat(AST_XML_RESULT, &file_stat) == -1) {
	/* build the xml result file */
	glob_app_cfg.ast_status = AST_FAILURE;
	print_error_response(AST_XML_RESULT); 
      }
   
      fd = open(AST_XML_RESULT, O_RDONLY);
#ifdef __FreeBSD__
      total = sendfile(fd, clntSock, 0, 0, NULL, NULL, 0);
      zlog_info("sendfile() returns %d", total);
#else
      if (fstat(fd, &file_stat) == -1) {
	zlog_err("xml_accept: fstat() failed on %s", AST_XML_RESULT);
	exit(1);
      }
      total = sendfile(clntSock, fd, 0, file_stat.st_size);
#endif
      if (total < 0)
	zlog_err("xml_accept: sendfile() failed; error = %d(%s)\n",
  		errno, strerror(errno));
      zlog_info("DONE");
    }

    free_application_cfg(&glob_app_cfg);    
    close(clntSock);
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
  struct link_cfg *mylink;

  if (list == NULL) 
    return;
  for (curnode = list->head;
	curnode;
	curnode = curnode->next) {
    mylink = (struct link_cfg*) curnode->data; 
    mylink->ast_status = AST_FAILURE;
    if (err_message != NULL) 
      mylink->agent_message = strdup(err_message);
  }
}

void
process_link_setup_result(struct application_cfg *working_app_cfg, struct node_cfg *srcnode)
{
  struct adtlistnode *curnode, *curnode1;
  struct link_cfg *old_link, *new_link;

  if (glob_app_cfg.function != SETUP_RESP) {
    zlog_err("process_link_setup_result: message type is not SETUP_RESP");
    set_alllink_fail(srcnode->link_list, "returning message type is not SETUP_RESP");
    working_app_cfg->ast_status = AST_FAILURE;
    return;
  }

  if (glob_app_cfg.link_list == NULL) {
    zlog_err("process_link_setup_result: no link in the link_agent setup response file");
    working_app_cfg->ast_status = AST_FAILURE;
    return;
  }

  for (curnode = glob_app_cfg.link_list->head;
       curnode;
       curnode = curnode->next) {
    new_link = (struct link_cfg*) curnode->data;

    for (curnode1 = srcnode->link_list->head;
	 curnode1;
	 curnode1 = curnode1->next) {
      old_link = (struct link_cfg*) curnode1->data;

      if (strcasecmp(new_link->dest->name, old_link->dest->name) == 0) {
	strcpy(old_link->lsp_name, new_link->lsp_name);
	old_link->ast_status = new_link->ast_status;
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

  if (glob_app_cfg.ast_status != AST_SUCCESS)
    working_app_cfg->ast_status = AST_FAILURE;
}

void
process_link_release_result(struct application_cfg *working_app_cfg, struct node_cfg *srcnode)
{
  struct adtlistnode *curnode, *curnode1;
  struct link_cfg *old_link, *new_link;

  if (glob_app_cfg.function != RELEASE_RESP) {
    zlog_err("process_link_release_result: message type is not RELEASE_RESP");
    set_alllink_fail(srcnode->link_list, "returning message type is not RELEASE_RESP");
    working_app_cfg->ast_status = AST_FAILURE;
    return;
  }

  if (glob_app_cfg.link_list == NULL) {
    zlog_err("process_link_release_result: no link in the link_agent setup response file");
    set_alllink_fail(srcnode->link_list, (glob_app_cfg.details[0]=='\0') ?
		"no link in the link_agent setup response file" :
		glob_app_cfg.details);
    working_app_cfg->ast_status = AST_FAILURE;
    return;
  }

  for (curnode = glob_app_cfg.link_list->head;
	curnode;
	curnode = curnode->next) {
    new_link = (struct link_cfg*) curnode->data;

    for (curnode1 = srcnode->link_list->head;
	 curnode1; 
	 curnode1 = curnode1->next) { 
      old_link = (struct link_cfg*) curnode1->data;

      if (strcasecmp(new_link->dest->name, old_link->dest->name) == 0) {
	strcpy(old_link->lsp_name, new_link->lsp_name);
	old_link->ast_status = new_link->ast_status; 
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

  if (glob_app_cfg.ast_status != AST_SUCCESS)
    working_app_cfg->ast_status = AST_FAILURE;
}

void
process_link_query_result(struct application_cfg *working_app_cfg, struct node_cfg *srcnode)
{
  if (glob_app_cfg.function != QUERY_RESP) {
    zlog_err("process_link_query_result: message type is not QUERY_RESP");
    set_alllink_fail(srcnode->link_list, "returning message type is not QUERY_RESP");
    working_app_cfg->ast_status = AST_FAILURE;
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
  
  srcnode->link_list = glob_app_cfg.link_list;
  glob_app_cfg.link_list = NULL; /* so that it won't be freed */
}

int
master_compose_link_request(struct application_cfg app_cfg, char *path, struct node_cfg* srcnode)
{
  struct adtlistnode *linknode;
  struct link_cfg *link;
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
  
  fprintf(send_file, "<topology>\n");
  fprintf(send_file, "<glob_ast_id>%s</glob_ast_id>\n", app_cfg.glob_ast_id); 
  fprintf(send_file, "<ast_func>%s</ast_func>\n", function_type_details[app_cfg.function]);

  if (app_cfg.function == QUERY_REQ) {
    print_node(send_file, srcnode);
    fprintf(send_file, "</topology>");
    fflush(send_file);
    fclose(send_file);
    return 1;
  }

  if (app_cfg.function != SETUP_REQ) {
    fprintf(send_file, "</topology>");
    fflush(send_file);
    fclose(send_file);
    return 1;
  }

  print_node(send_file, srcnode);
  for (	linknode = srcnode->link_list->head;
	linknode;
	linknode = linknode->next) {
      
    link = (struct link_cfg*)linknode->data;
   
    print_node(send_file, link->dest);
    print_link(send_file, link);
  }

  fprintf(send_file, "</topology>");
  fflush(send_file);
  fclose(send_file);

  return 1;
}

int
master_compose_node_request(struct application_cfg app_cfg, char *path, struct node_cfg* srcnode)
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

  fprintf(send_file, "<topology>\n");
  fprintf(send_file, "<glob_ast_id>%s</glob_ast_id>\n", app_cfg.glob_ast_id); 
  fprintf(send_file, "<ast_func>%s</ast_func>\n", function_type_details[app_cfg.function]);
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
  struct link_cfg *link;
  struct node_cfg *srcnode;
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
  sprintf(directory+strlen(directory), "/%s", glob_app_cfg.glob_ast_id);
  
  if (glob_app_cfg.function == SETUP_REQ) 
    sprintf(path_prefix, "%s/setup_", directory);
  else if (glob_app_cfg.function == RELEASE_REQ)
    sprintf(path_prefix, "%s/release_", directory);
  else if (glob_app_cfg.function == QUERY_REQ)
    sprintf(path_prefix, "%s/query_", directory);
       
  memcpy (&working_app_cfg, &glob_app_cfg, sizeof(struct application_cfg));
  memset(&glob_app_cfg, 0, sizeof(struct application_cfg));
  memset(buffer, 0, RCVBUFSIZE);
  memset(send_buf, 0, SENDBUFSIZE);
  for (curnode = working_app_cfg.node_list->head;
       curnode;
       curnode = curnode->next) {
    srcnode = (struct node_cfg*)(curnode->data); 

   if (srcnode->link_list == NULL &&
	working_app_cfg.function != QUERY_REQ) {
	
      /* No task need to be sent to this link_agent (dragond); skip 
       */
      continue;
    }

    sprintf(newpath, "%srequest_%s.xml", path_prefix, srcnode->name);
    if (master_compose_link_request(working_app_cfg, newpath, srcnode) == 0) {
      ret_value = 0;
      continue;
    }

    zlog_info("sending request to %s (%s:%d)", srcnode->name, srcnode->ipadd, DRAGON_XML_PORT);
    sock = send_file_to_agent(srcnode->ipadd, DRAGON_XML_PORT, newpath);

    if (sock == -1)  {
      srcnode->ast_status = AST_FAILURE;
      ret_value = 0;

      if (working_app_cfg.function == QUERY_REQ)
	continue;

      for (linknode = srcnode->link_list->head;
	   linknode;
	   linknode = linknode->next) {
	
	link = (struct link_cfg*)linknode->data;
	link->ast_status = AST_FAILURE;
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
      srcnode->ast_status = AST_UNKNOWN;
    } else {

      fflush(ret_file);
      fclose(ret_file);

      if (topo_xml_parser(newpath, BRIEF_VERSION) == 0) {
	zlog_err("%s is not parsed correctly", newpath);
	srcnode->ast_status = AST_UNKNOWN;
	ret_value = 0;
      } else if (master_validate_graph(0) == 0) {
	zlog_err("%s failed at validation", newpath);
	srcnode->ast_status = AST_UNKNOWN;
	ret_value = 0;
      } else {
	if (working_app_cfg.function == SETUP_REQ)
	  process_link_setup_result(&working_app_cfg, srcnode);
	else if (working_app_cfg.function == RELEASE_REQ)
	  process_link_release_result(&working_app_cfg, srcnode);
	else if (working_app_cfg.function == QUERY_REQ)
	  process_link_query_result(&working_app_cfg, srcnode);
      }

      zlog_info("%s returns %s", srcnode->name, 
			status_type_details[glob_app_cfg.ast_status]);	
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
  struct node_cfg *srcnode;
  struct link_cfg *mylink;
  int sock, success;
  char directory[80];
  char newpath[105];
  char path_prefix[100];

  sprintf(directory, "%s/%s", AST_DIR, glob_app_cfg.glob_ast_id);
  if (glob_app_cfg.function == SETUP_RESP)
    sprintf(path_prefix, "%s/setup_", directory);
  else if (glob_app_cfg.function == RELEASE_RESP)
    sprintf(path_prefix, "%s/release_", directory);
  else if (glob_app_cfg.function == QUERY_RESP)
    sprintf(path_prefix, "%s/query_", directory);

  sprintf(newpath, "%sfinal.xml", path_prefix);
  print_final(newpath);

  if (glob_app_cfg.ast_status == AST_SUCCESS && 
      glob_app_cfg.function == SETUP_RESP) {
    char path[100];

    glob_app_cfg.function = AST_COMPLETE;
    /* create ast_complete.xml for record */
    sprintf(path, "%s/%s/ast_complete.xml", AST_DIR, glob_app_cfg.glob_ast_id);
    print_final(path);

    zlog_info("Receiving AST_SUCCESS from all resouce in SETUP_REQ; now, do AST_COMPLETE");
    /* sending to node first */
    for (curnode = glob_app_cfg.node_list->head, success = 0;
	 curnode; 
	 curnode = curnode->next) {
      srcnode = (struct node_cfg*) curnode->data;

#ifdef FIONA
      if (srcnode->agent_port == 0) 
	continue;
#endif

      /* AST_COMPLETE is a special case here;
       * all other message, we would wait for the return message, but
       * this one we won't
       */
      zlog_info("sending request to node_agent at %s (%s:%d)", 
		srcnode->name, srcnode->ipadd, NODE_AGENT_PORT);
      sock = send_file_to_agent(srcnode->ipadd, NODE_AGENT_PORT, path);
      if (sock == -1) {
	srcnode->ast_status = AST_FAILURE;
	srcnode->agent_message = strdup("Failed to connect to node_agent for AST_COMPETE msg");
      } else {
	srcnode->ast_status = AST_SUCCESS;
	success++;
	close(sock);
      }

      if (srcnode->link_list != NULL) {
       
	zlog_info("sending request to link_agent at %s (%s:%d)", 
		  srcnode->name, srcnode->ipadd, DRAGON_XML_PORT);
	sock = send_file_to_agent(srcnode->ipadd, DRAGON_XML_PORT, path);
	for (linknode = srcnode->link_list->head; 
	     linknode; 
	     linknode = linknode->next) { 
	  mylink = (struct link_cfg*) linknode->data; 
	  if (sock == -1) { 
	    mylink->ast_status = AST_FAILURE; 
	    mylink->agent_message = strdup("Failed to connect to link_agent for AST_COMPLETE msg"); 
	  } else {
	    mylink->ast_status = AST_SUCCESS; 
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
      glob_app_cfg.ast_status = AST_SUCCESS;
    } else {
      zlog_info("AST_COMPLETE msg failes at some resources");
      glob_app_cfg.ast_status = AST_FAILURE;
    }
  }

  /* "final.xml" is the latest file on this <glob_ast_id>
   */
  if (glob_app_cfg.function != QUERY_RESP) { 
    sprintf(newpath, "%s/final.xml", directory); 
    print_final(newpath);
  }
 
  symlink(newpath, AST_XML_RESULT);
}

int
send_task_to_node_agent()
{
  struct adtlistnode *curnode;
  struct node_cfg* srcnode, *newnode;
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
  sprintf(directory+strlen(directory), "/%s", glob_app_cfg.glob_ast_id);

  if (glob_app_cfg.function == SETUP_REQ)
    sprintf(path_prefix, "%s/setup_node_", directory);
  else if (glob_app_cfg.function == RELEASE_REQ)
    sprintf(path_prefix, "%s/release_node_", directory);
  else if (glob_app_cfg.function == QUERY_REQ)
    sprintf(path_prefix, "%s/query_node_", directory);

  memcpy(&working_app_cfg, &glob_app_cfg, sizeof(struct application_cfg));
  memset(&glob_app_cfg, 0, sizeof(struct application_cfg));
  memset(buffer, 0, RCVBUFSIZE);
  memset(send_buf, 0, SENDBUFSIZE);

  for (curnode = working_app_cfg.node_list->head;
	curnode;
	curnode = curnode->next) {
    srcnode = (struct node_cfg*)(curnode->data);

    sprintf(newpath, "%srequest_%s.xml", path_prefix, srcnode->name);
    if (master_compose_node_request(working_app_cfg, newpath, srcnode) == 0) {
      ret_value = 0;
      continue;
    }

    zlog_info("sending request to %s (%s:%d)",
		srcnode->name, srcnode->ipadd, NODE_AGENT_PORT);
    sock = send_file_to_agent(srcnode->ipadd, NODE_AGENT_PORT, newpath);
    if (sock == -1) {
      srcnode->ast_status = AST_FAILURE;
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
      srcnode->ast_status = AST_UNKNOWN;
      ret_value = 0;
    } else {

      fflush(ret_file);
      fclose(ret_file);

      if (topo_xml_parser(newpath, BRIEF_VERSION) == 0) {
	zlog_err("%s is not parsed correctly", newpath);
	srcnode->ast_status = AST_UNKNOWN;
	ret_value = 0;
      } else if (master_validate_graph(0) == 0) {
	zlog_err("%s failed at validation", newpath);
	srcnode->ast_status = AST_UNKNOWN;
	ret_value = 0;
      } else {
	srcnode->ast_status = glob_app_cfg.ast_status;
        if (glob_app_cfg.details[0] != '\0')
	  srcnode->agent_message = strdup(glob_app_cfg.details);
        else if (glob_app_cfg.node_list != NULL) {
 	  newnode = (struct node_cfg*)glob_app_cfg.node_list->head->data;
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
	                status_type_details[glob_app_cfg.ast_status]);
      free_application_cfg(&glob_app_cfg);
    }
    close(sock);
  }

  memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
  return ret_value;
}
