#include <zebra.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <net/if.h>
#include <sys/ioctl.h>
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

#define NODE_AGENT_RET 	"/usr/local/noded_ret.xml"
#define NODE_AGENT_RECV "/usr/local/noded_recv.xml"
#define NODE_AGENT_DIR	"/usr/local/node_agent"
#define RCVBUFSIZE	200
#define MAXPENDING      12
#define TIMEOUT_SECS	3

#ifdef RESOURCE_BROKER
static struct adtlist node_resource;
static struct adtlistnode *resource_index;
int broker_init(char*);
static int broker_accept(struct thread*);
#endif

extern char *status_type_details[];
struct thread_master *master; /* master = dmaster.master */
static int agent_accept(struct thread *);

struct option ast_master_opts[] =
{
  { "daemon",     no_argument,       NULL, 'd'},
  { "help",       no_argument,       NULL, 'h'},
#ifdef RESOURCE_BROKER
  { "file",	  required_argument, NULL, 'f'},
#endif
  { 0 }
};

static void
usage (char *progname, int status)
{
  if (status != 0)
    printf("Try \"%s --help\" for more information.\n", progname);
  else
#ifdef RESOURCE_BROKER 
      printf ("Usage : %s [OPTION...]\n\
NSF NODE_AGENT.\n\n\
-d, --daemon       Runs in daemon mode\n\
-h, --help       Display this help and exit\n\
-f, --file	 File contains resources for node\n\
\n", progname);
#else
      printf ("Usage : %s [OPTION...]\n\
NSF NODE_AGENT.\n\n\
-d, --daemon       Runs in daemon mode\n\
-h, --help       Display this help and exit\n\
\n", progname);
#endif

  exit (status);
}

/* all variable for child processing */
static int child_app_complete;
static int node_child;
static struct sigaction node_app_complete_action;
static pid_t node_child_pid;

int 
node_assign_ip(struct node_cfg* node)
{
  struct ifreq if_info;
  int sockfd = -1, i;
  struct sockaddr_in *sock;
  int ioctl_ret;
  struct vlsr *vlsr_cur;

  if (node->vlsr_total == 0)
    return 1;

  node->ast_status = AST_SUCCESS; 
  for (i = 0; i < node->vlsr_total; i++) {
    vlsr_cur = (struct vlsr*)node->vlsr_info[i];
    if (vlsr_cur->assign_ip[0] != '\0') {
      if (sockfd == -1) 
	sockfd = socket(AF_INET, SOCK_DGRAM, 0); 

      bzero(&if_info, sizeof(struct ifreq));
      strcpy(if_info.ifr_name, vlsr_cur->iface);
      bzero(&if_info.ifr_ifru.ifru_addr, sizeof(struct sockaddr));
      sock = (struct sockaddr_in*)&(if_info.ifr_ifru.ifru_addr);
      sock->sin_family = AF_INET;
      sock->sin_addr.s_addr = inet_addr(vlsr_cur->assign_ip);
      ioctl_ret = ioctl(sockfd, SIOCSIFADDR, &if_info);
      if (ioctl_ret == -1) {
	node->ast_status = AST_FAILURE;
	node->agent_message = strdup("ifconfig failure");
	zlog_err("ifconfig failed for %s", vlsr_cur->iface);
	close(sockfd);
	return 0;
      }
      
      sock->sin_addr.s_addr = inet_addr("255.255.255.192");
      ioctl_ret = ioctl(sockfd, SIOCSIFNETMASK, &if_info);
      if (ioctl_ret == -1) {
	node->ast_status = AST_FAILURE;
	node->agent_message = strdup("ifconfig failure");
	zlog_err("ifconfig failed for %s", vlsr_cur->iface);
	close(sockfd);
	return 0;
      }
    }
  }
  if (sockfd != -1)
    close(sockfd);

  return 1;
}

int 
node_process_setup_req()
{
  struct adtlistnode *curnode;
  struct node_cfg *node;
  char path[105];
  char directory[80];

  glob_app_cfg.function = SETUP_RESP;
  zlog_info("Processing glob_ast_id: %s, SETUP_REQ", glob_app_cfg.glob_ast_id);

  strcpy(directory, NODE_AGENT_DIR);
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
  if (rename(NODE_AGENT_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
           NODE_AGENT_RECV, path, errno, strerror(errno));

  sprintf(path, "%s/setup_response.xml", directory);
  glob_app_cfg.ast_status = AST_SUCCESS;
  for (curnode = glob_app_cfg.node_list->head;
       curnode;
	curnode = curnode->next) {
    node = (struct node_cfg*) curnode->data;
    if (!node_assign_ip(node)) {
      glob_app_cfg.ast_status = AST_FAILURE;
      break;
    }
  } 

  glob_app_cfg.function = SETUP_RESP;
  print_xml_response(path, BRIEF_VERSION);
  symlink(path, NODE_AGENT_RET);

  sprintf(path, "%s/final.xml", directory);
  print_final(path);

  return (glob_app_cfg.ast_status == AST_SUCCESS);
}

static void
handle_app_complete_child()
{
  /* call waitpid to collect the child; otherwise, it will become a zombie */
  waitpid(node_child_pid, NULL, 0); 
  zlog_info("handle_app_complete_child: APP_COMPLETE child has exited");
}

int
node_process_ast_complete()
{
  struct adtlistnode *curnode;
  struct node_cfg *node;
  char path[105];
  struct stat fs;
  struct application_cfg working_app_cfg;

  glob_app_cfg.org_function = glob_app_cfg.function;
  glob_app_cfg.function = APP_COMPLETE;
  zlog_info("Processing glob_ast_id: %s, AST_COMPLETE", glob_app_cfg.glob_ast_id);

  sprintf(path, "%s/%s/final.xml",
          NODE_AGENT_DIR, glob_app_cfg.glob_ast_id);
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

  sprintf(path, "%s/%s/setup_response.xml", NODE_AGENT_DIR, glob_app_cfg.glob_ast_id);
  free_application_cfg(&glob_app_cfg);
  if (topo_xml_parser(path, BRIEF_VERSION) != 1) {
    memcpy(&glob_app_cfg, &working_app_cfg, sizeof(struct application_cfg));
    sprintf(glob_app_cfg.details, "didn't parse the glob_ast_id file successfully");
    glob_app_cfg.ast_status = AST_FAILURE;
    return 0;
  }
  glob_app_cfg.function = working_app_cfg.function;
  glob_app_cfg.ast_ip = working_app_cfg.ast_ip;
  working_app_cfg.ast_ip = NULL;
  free_application_cfg(&working_app_cfg);

  sprintf(path, "%s/%s/ast_complete.xml",
          NODE_AGENT_DIR, glob_app_cfg.glob_ast_id);

  if (rename(NODE_AGENT_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
        NODE_AGENT_RECV, path, errno, strerror(errno));

  for (curnode = glob_app_cfg.node_list->head;
       curnode;
        curnode = curnode->next) {
    node = (struct node_cfg*) curnode->data;
    if (node->command) {
      node_child = 1;
      if ((node_child_pid = fork()) < 0) 
	zlog_err("Fork() failed; error = %d(%s)", errno, strerror(errno));
      else if (node_child_pid == 0) {

	/* Child process */
	zlog_info("Running command %s", node->command);
	system(node->command);
        child_app_complete = 1;
      } else {
	/* Parent process */
        /* setup facility to reap the Zombie from child process */
        node_app_complete_action.sa_handler = handle_app_complete_child;
        if (sigfillset(&node_app_complete_action.sa_mask) < 0) { 
          zlog_err("sigfillset() failed"); 
        } else { 
          node_app_complete_action.sa_flags = 0; 
      
          if (sigaction(SIGCHLD, &node_app_complete_action, 0) < 0)
            zlog_err("sigaction() failed");
        }
      if (waitpid(node_child_pid, NULL, WNOHANG) < 0)
        zlog_err("waitpid failed; error = %d(%s)", errno, strerror(errno));
	
      } 
    } else 
      zlog_info("Nothing needs to be executed");

    node->ast_status = AST_APP_COMPLETE;
  }
  glob_app_cfg.ast_status = AST_APP_COMPLETE;
  sprintf(path, "%s/%s/final.xml",
          NODE_AGENT_DIR, glob_app_cfg.glob_ast_id);
  print_final(path);
  symlink(path, NODE_AGENT_RET);

  return 1;
}

int 
node_process_release_req()
{
  struct adtlistnode *curnode;
  struct node_cfg *node;
  char path[105];
  struct stat fs;
  struct application_cfg working_app_cfg;

  glob_app_cfg.org_function = glob_app_cfg.function;
  glob_app_cfg.function = RELEASE_RESP;
  zlog_info("Processing glob_ast_id: %s, RELEASE_REQ", glob_app_cfg.glob_ast_id);

  sprintf(path, "%s/%s/final.xml",
          NODE_AGENT_DIR, glob_app_cfg.glob_ast_id);
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

  sprintf(path, "%s/%s/setup_response.xml", NODE_AGENT_DIR, glob_app_cfg.glob_ast_id);
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
          NODE_AGENT_DIR, glob_app_cfg.glob_ast_id);

  if (rename(NODE_AGENT_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
           NODE_AGENT_RECV, path, errno, strerror(errno));

  sprintf(path, "%s/%s/release_response.xml",
          NODE_AGENT_DIR, glob_app_cfg.glob_ast_id);

  for (curnode = glob_app_cfg.node_list->head;
       curnode;
        curnode = curnode->next) {
    node = (struct node_cfg*) curnode->data;
    node->ast_status = AST_SUCCESS;
  }
  
  glob_app_cfg.ast_status = AST_SUCCESS;
  glob_app_cfg.function = RELEASE_RESP;
  print_xml_response(path, BRIEF_VERSION);
  symlink(path, NODE_AGENT_RET);

  sprintf(path, "%s/%s/final.xml",
          NODE_AGENT_DIR, glob_app_cfg.glob_ast_id);
  print_final(path);

  return 1;
}

int
node_process_query_req()
{
  struct adtlistnode *curnode;
  struct node_cfg *node;
  char path[105];
  char directory[80];

  glob_app_cfg.org_function = glob_app_cfg.function;
  glob_app_cfg.function = QUERY_RESP;
  zlog_info("Processing glob_ast_id: %s, QUERY_REQ", glob_app_cfg.glob_ast_id);

  strcpy(directory, NODE_AGENT_DIR);
  if (mkdir(directory, 0755) == -1 && errno != EEXIST) {
    zlog_err("Can't create directory %s", NODE_AGENT_DIR);
    return 0;
  }

  sprintf(directory, "%s/%s", NODE_AGENT_DIR, glob_app_cfg.glob_ast_id);
  if (mkdir(directory, 0755) == -1) {
    zlog_err("Can't create directory %s", directory);
    return 0;
  }

  sprintf(path, "%s/%s/query_original.xml",
        NODE_AGENT_DIR, glob_app_cfg.glob_ast_id);

  if (rename(NODE_AGENT_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
           NODE_AGENT_RECV, path, errno, strerror(errno));

  sprintf(path, "%s/%s/query_response.xml",
        NODE_AGENT_DIR, glob_app_cfg.glob_ast_id);

  glob_app_cfg.function = QUERY_RESP;
  glob_app_cfg.ast_status = AST_SUCCESS;

  for (curnode = glob_app_cfg.node_list->head;
       curnode;
        curnode = curnode->next) {
    node = (struct node_cfg*) curnode->data;
    node->ast_status = AST_SUCCESS;
  }

  print_final(path);
  symlink(path, NODE_AGENT_RET);

  return 1;
}

int
noded_process_xml(char* input_file)
{
  char system_call[100];

  if (strcasecmp(input_file, NODE_AGENT_RECV) != 0) {
    sprintf(system_call, "cp %s %s", input_file, NODE_AGENT_RECV);
    system(system_call);
  }

  /* after all the preparation, parse the application xml file 
   */
  if (topo_xml_parser(input_file, FULL_VERSION) == 0) {
    zlog_err("noded_process_xml: topo_xml_parser() failed");
    return 0;
  }

  if (glob_app_cfg.function != SETUP_REQ &&
      glob_app_cfg.function != RELEASE_REQ &&
      glob_app_cfg.function != QUERY_REQ &&
      glob_app_cfg.function != AST_COMPLETE) {
    zlog_err("noded_process_xml: invalid <ast_func> in xml file");
    sprintf(glob_app_cfg.details, "invalid ast_func in xml file");
    return 0;
  }

  if (glob_app_cfg.function != SETUP_REQ &&
        glob_app_cfg.glob_ast_id == NULL) {
    zlog_err("noded_process_xml: <glob_ast_id> should be set in non-setup request case");
    sprintf(glob_app_cfg.details, "glob_ast_id should be set in non-setup request case");
    return 0;
  }

  if (glob_app_cfg.node_list == NULL) {
    zlog_err("noded_process_xml: should have at least one node in xml file");
    return 0;
  }

  if (glob_app_cfg.function == SETUP_REQ) {
    node_process_setup_req();
  } else if ( glob_app_cfg.function == AST_COMPLETE) {
    node_process_ast_complete();
  } else if (glob_app_cfg.function == RELEASE_REQ) {
    node_process_release_req();
  } else if (glob_app_cfg.function == QUERY_REQ) {
    node_process_query_req();
  }

  return 1;
}

static void
handleAlarm()
{
  /* don't need to do anything special for SIGALARM
   */
}

/* let ast_masterd to listen on a certain tcp port;
 */
int
main(int argc, char* argv[])
{
  struct sockaddr_in ServAddr;
  int servSock;
  char *p;
  char *progname;
  int daemon_mode = 0;
  struct thread thread;
  struct sigaction myAlarmAction;
#ifdef RESOURCE_BROKER
  char *resource_file = NULL;
#endif
  
  progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);

  while (1) {
    int opt;

#ifdef RESOURCE_BROKER
    opt = getopt_long (argc, argv, "dhf:", ast_master_opts, 0);
    if (argc > 4) {
      usage(progname, 1);
      exit(EXIT_FAILURE);
    }
#else
    opt = getopt_long (argc, argv, "dh", ast_master_opts, 0);
    if (argc > 2) {
      usage(progname, 1); 
      exit(EXIT_FAILURE);
    }
#endif

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
#ifdef RESOURCE_BROKER
      case 'f':
        resource_file = optarg;
	break;
#endif
      default:
        usage (progname, 1);
    }
  }

  /* Change to the daemon program. */
  if (daemon_mode)
    daemon (0, 0);

  master = thread_master_create();

  zlog_default = openzlog (progname, ZLOG_NOLOG, ZLOG_ASTB,
                           LOG_CONS|LOG_NDELAY|LOG_PID, LOG_DAEMON);
  zlog_set_file(zlog_default, ZLOG_FILE, "/var/log/node_agent.log");

  zlog_info("node_agent starts: pid = %d",  getpid());

  if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    zlog_err("socket() failed");
    exit(EXIT_FAILURE);
  }
  
  memset(&ServAddr, 0, sizeof(ServAddr));
  ServAddr.sin_family = AF_INET;
  ServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  ServAddr.sin_port = htons(NODE_AGENT_PORT);

  if (bind(servSock, (struct sockaddr*)&ServAddr, sizeof(ServAddr)) < 0) {
    zlog_err("bind() failed");
    exit(EXIT_FAILURE);
  }

  if (listen(servSock, MAXPENDING) < 0) {
    zlog_err("listen() failed");
    exit(EXIT_FAILURE);
  }

  thread_add_read(master, agent_accept, NULL, servSock);

#ifdef RESOURCE_BROKER
  if (resource_file != NULL) {
    if (broker_init(resource_file) == 1) {
      if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        zlog_err("socket() failed");
        exit(EXIT_FAILURE);
      }
      
      memset(&ServAddr, 0, sizeof(ServAddr));
      ServAddr.sin_family = AF_INET;
      ServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
      ServAddr.sin_port = htons(NODE_BROKER_PORT);
    
      if (bind(servSock, (struct sockaddr*)&ServAddr, sizeof(ServAddr)) < 0) {
        zlog_err("bind() failed");
        exit(EXIT_FAILURE);
      }
    
      if (listen(servSock, MAXPENDING) < 0) {
        zlog_err("listen() failed");
        exit(EXIT_FAILURE);
      }
   
      zlog_err("BROKER: this node_agent will also serve as resource broker"); 
      thread_add_read(master, broker_accept, NULL, servSock);
    } else 
      zlog_err("BROKER: can't read the resource file (%s)", resource_file);
  }
#endif

    myAlarmAction.sa_handler = handleAlarm;
    if (sigfillset(&myAlarmAction.sa_mask) < 0) {
      zlog_err("xml_accept: sigfillset() failed");
      return 0;
    }
    myAlarmAction.sa_flags = 0;

    if (sigaction(SIGALRM, &myAlarmAction, 0) < 0) {
      zlog_err("xml_accept: sigaction() failed");
      return 0;
    }

  memset(&glob_app_cfg, 0, sizeof(struct application_cfg));
  while (thread_fetch (master, &thread))
    thread_call (&thread);

  return 1;
}

static int
agent_accept(struct thread *thread)
{
  int servSock, clntSock;
  struct sockaddr_in clntAddr;
  struct sockaddr_in astAddr;
  unsigned int clntLen;
  int recvMsgSize;
  char buffer[RCVBUFSIZE];
  FILE* fp;
  int fd, total;
  static struct stat file_stat;

  servSock = THREAD_FD(thread);

  clntLen = sizeof(clntAddr);
  unlink(NODE_AGENT_RET);
  unlink(NODE_AGENT_RECV);
  child_app_complete = 0;
  node_child = 0;
   
  free_application_cfg(&glob_app_cfg);
  if ((clntSock = accept(servSock, (struct sockaddr*)&clntAddr, &clntLen)) < 0) 
    zlog_err("accept() failed; error = %d(%s)", errno, strerror(errno));
  else {
    zlog_info("Handling client %s ...", inet_ntoa(clntAddr.sin_addr));
    glob_app_cfg.ast_ip = strdup(inet_ntoa(clntAddr.sin_addr));

    fp = fopen(NODE_AGENT_RECV, "w");
    total = 0;
    while ((recvMsgSize = recv(clntSock, buffer, RCVBUFSIZE-1, 0)) > 0 ) {
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

      free_application_cfg(&glob_app_cfg);

      glob_app_cfg.ast_ip = strdup(inet_ntoa(clntAddr.sin_addr));

      if (noded_process_xml(NODE_AGENT_RECV) == 1) 
        glob_app_cfg.ast_status = AST_SUCCESS;
      else 
        glob_app_cfg.ast_status = AST_FAILURE;

      if (glob_app_cfg.function == APP_COMPLETE && node_child && !child_app_complete) {
	close(clntSock);
        thread_add_read(master, agent_accept, NULL, servSock);
	return 1;
      } 
    
      if (glob_app_cfg.function == APP_COMPLETE) {

        /* the clntSock should have been void */
        close(clntSock);
        if ((clntSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
          zlog_err("socket() failed");
        else {
          memset(&astAddr, 0, sizeof(astAddr));
          astAddr.sin_family = AF_INET;
          astAddr.sin_addr.s_addr = inet_addr(glob_app_cfg.ast_ip);
          astAddr.sin_port = htons(MASTER_PORT);

          if (connect(clntSock, (struct sockaddr*)&astAddr, sizeof(astAddr)) < 0) {
            zlog_err("connect() failed to ast_master");
            close(clntSock);
            clntSock = -1;
          }
        }
      }

      if (stat(NODE_AGENT_RET, &file_stat) == -1) {
        /* the result file hasn't been written
         */
        glob_app_cfg.ast_status = AST_FAILURE;
        print_error_response(NODE_AGENT_RET);
      }

      if (clntSock!= -1) {
	if (glob_app_cfg.function == APP_COMPLETE) 
	  zlog_info("Sending APP_COMPLETE for glob_ast_id: %s to ast_master at (%s:%d)",
			glob_app_cfg.glob_ast_id,
			glob_app_cfg.ast_ip, MASTER_PORT);
	else
          zlog_info("sending confirmation (%s) to ast_master at (%s:%d)",
                  status_type_details[glob_app_cfg.ast_status],
                  glob_app_cfg.ast_ip, MASTER_PORT);

        fd = open(NODE_AGENT_RET, O_RDONLY);
#ifdef __FreeBSD__
        total = sendfile(fd, clntSock, 0, 0, NULL, NULL, 0);
        zlog_info("sendfile() returns %d", total);
#else
        if (fstat(fd, &file_stat) == -1) {
          zlog_err("fstat() failed on %s", XML_NEW_FILE);
        }
        total = sendfile(clntSock, fd, 0, file_stat.st_size);
        zlog_info("file_size is %d and sendfile() returns %d",
                        (int)file_stat.st_size, total);
#endif
        if (total < 0)
          zlog_err("sendfile() failed; errno = %d (%s)\n",
                 errno, strerror(errno));

        close(fd);
        close(clntSock);
      }
      if (child_app_complete) 
	_exit(2);
    } else
      fclose(fp);

    zlog_info("DONE!");
  }

  thread_add_read(master, agent_accept, NULL, servSock);
  return 1;
}

#ifdef RESOURCE_BROKER
static int
broker_accept(struct thread *thread)
{
  int servSock, clntSock;
  struct sockaddr_in clntAddr;
  unsigned int clntLen;
  int recvMsgSize;
  char buffer[RCVBUFSIZE];
  FILE* fp;
  int fd, total;
  static struct stat file_stat;
  static char buf_ret[300];

  servSock = THREAD_FD(thread);

  clntLen = sizeof(clntAddr);
  unlink(NODE_AGENT_RET);
  unlink(NODE_AGENT_RECV);
  child_app_complete = 0;
  node_child = 0;
   
  if ((clntSock = accept(servSock, (struct sockaddr*)&clntAddr, &clntLen)) < 0) 
    zlog_err("accept() failed; error = %d(%s)", errno, strerror(errno));
  else {
    zlog_info("BROKER: Handling client %s ...", inet_ntoa(clntAddr.sin_addr));

    fp = fopen(NODE_AGENT_RECV, "w");
    total = 0;
    alarm(TIMEOUT_SECS);
    while ((recvMsgSize = recv(clntSock, buffer, RCVBUFSIZE-1, 0)) > 0 ) {
      if (errno == EINTR) {
        /* alarm went off
         */
        zlog_warn("xml_accept: dragon probably has not received all datas");
        zlog_warn("recvMsgSize = %d", recvMsgSize);
	total += recvMsgSize;
        break;
      }
      buffer[recvMsgSize]='\0';
      fprintf(fp, "%s", buffer);
      total += recvMsgSize;
      alarm(TIMEOUT_SECS);
    }
    alarm(0);
 
    zlog_info("broker_accept: total byte received = %d", total);
    if (total != 0) {
      struct node_cfg *node;

      fflush(fp);
      fclose(fp);

      /* basically, we will ignore this input file and just give back whatever we have
       */
      fp = fopen(NODE_AGENT_RET, "w+");
      fprintf(fp, "<topology>\n");
      if (resource_index == NULL) 
        resource_index = node_resource.head;
      node = (struct node_cfg *)resource_index->data;
       
      zlog_info("BROKER: returns node (ipadd = %s, teadd = %s)\n", node->ipadd, node->te_addr);   
      memset(buf_ret, 0, 300);
      print_node(buf_ret, node);
      fprintf(fp, buf_ret);
      fprintf(fp, "</topology>");
      fflush(fp);
      fclose(fp);
		
      fd = open(NODE_AGENT_RET, O_RDONLY);
#ifdef __FreeBSD__
      total = sendfile(fd, clntSock, 0, 0, NULL, NULL, 0);
      zlog_info("sendfile() returns %d", total);
#else
      if (fstat(fd, &file_stat) == -1) {
        zlog_err("fstat() failed on %s", XML_NEW_FILE);
      }
      total = sendfile(clntSock, fd, 0, file_stat.st_size);
      zlog_info("file_size is %d and sendfile() returns %d",
                      (int)file_stat.st_size, total);
#endif
      if (total < 0)
        zlog_err("sendfile() failed; errno = %d (%s)\n",
               errno, strerror(errno));

      close(fd);
    } else 
      fclose(fp);

    close(clntSock);  
    zlog_info("DONE!");
  }
  free_application_cfg(&glob_app_cfg);

  thread_add_read(master, broker_accept, NULL, servSock);
  return 1;
}

int 
broker_init(char* file)
{
  struct stat file_stat;
  struct adtlistnode *curnode;
  struct node_cfg *node;
   
  if (file == NULL) 
    return 0;

  if (stat(file, &file_stat) == -1) 
    return 0;

  if (topo_xml_parser(file, BRIEF_VERSION) == 0) 
    return 0;

  if (glob_app_cfg.node_list == NULL) 
    return 0;

  memset(&node_resource, 0, sizeof(struct adtlist));
  for (curnode = glob_app_cfg.node_list->head;
	curnode;
	curnode = curnode->next) {
    node = (struct node_cfg*) curnode->data;
    if (node->ipadd[0] != '\0' && 
	node->te_addr[0] != '\0') {
      adtlist_add(&node_resource, node);
      curnode->data = NULL;
    }
  }

  if (node_resource.head == NULL)
    return 0;

  resource_index = NULL; 
  return 1;
}  
#endif
