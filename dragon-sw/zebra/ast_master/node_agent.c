#include <zebra.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include "vty.h"
#include "ast_master_ext.h"

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

#define NODE_AGENT_RET 	"/usr/local/noded_ret.xml"
#define NODE_AGENT_RECV "/usr/local/noded_recv.xml"
#define BROKER_FILE	"/usr/local/etc/broker.xml"
#define MAXPENDING      12
#define NODED_TEMP_FILE "/tmp/noded_temp_file"

#ifdef RESOURCE_BROKER
struct node_tank {
  int number;
  int index;
  struct {
    char* name;
    char* ip;
    char* router_id;
    char *tunnel;
  } es[20];
};
static struct node_tank node_pool[NUM_NODE_TYPE+1];

int broker_init();
static int broker_accept(struct thread*);
static int broker_read_app_cfg();
static int broker_process();
#endif

extern char *status_type_details[];
struct thread_master *master; /* master = dmaster.master */
static int agent_accept(struct thread *);
static void noded_kill_process();
int dragon_node_pc_minion_proc(enum action_type, struct resource *);

static char* interface;
static char* loopback;

/* extern to other variables to use the minion-related lib functions */
extern int glob_minion;
extern char *glob_minion_ret_xml;
extern char *glob_minion_recv_xml;
extern struct res_mod dragon_node_pc_mod;

struct option ast_master_opts[] =
{
  { "daemon",     no_argument,       NULL, 'd'},
  { "help",       no_argument,       NULL, 'h'},
#ifdef RESOURCE_BROKER
  { "broker",	  no_argument, 	NULL, 'b'},
#endif
  { "config_file", required_argument, NULL, 'c'},
  { 0 }
};

static void
init_application_module()
{
  /* DEVELOPER: add your resource module in here
   * dragon_app serves as an example, please consult dragon_app.[ch]
   */
  init_dragon_module();
}

static void
noded_read_config(char* config_file) 
{
  char line[100];
  FILE *fp;
  char *ret, *token;
 
  interface = NULL;
  loopback = NULL;

  if (!config_file)
    return;
  fp = fopen(config_file, "r");
  if (!fp)
    return;

  while ((ret = fgets(line, 100, fp)) != NULL) {
    token = strtok(ret, " ");
    if (!token || strcmp(token, "set") != 0)
      continue;
    token = strtok(NULL, " ");
    if (!token)
      continue;

    if (strcmp(token, "interface") == 0) {
      token = strtok(NULL, " ");
      if (!token)
	continue;
      if (token[strlen(token)-1] == '\n') 
	token[strlen(token)-1] = '\0';
      interface = strdup(token);
    } else if (strcmp(token, "router_id") == 0) {
      token = strtok (NULL, " ");
      if (!token)
	continue;
      if (token[strlen(token)-1] == '\n')
	token[strlen(token)-1] = '\0';
      loopback = strdup(token);
    }
  } 
}

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
-b, --broker	 Serve as resource broker\n\
-c, --config_file   Set configuraiton file name\n\
\n", progname);
#else
      printf ("Usage : %s [OPTION...]\n\
NSF NODE_AGENT.\n\n\
-d, --daemon       Runs in daemon mode\n\
-h, --help       Display this help and exit\n\
-c, --config_file   Set configuraiton file name\n\
\n", progname);
#endif

  exit (status);
}

/* all variable for child processing */
static int child_app_complete;
static int node_child;
static struct sigaction node_app_complete_action;
static struct sigaction myKillAction;
static int child_clnt_sock;

struct pid_node {
  pid_t pid;
  char *ast_id;
  struct pid_node* next;
};

static struct pid_node* child_pids = NULL;

static pid_t node_child_pid = 1;
static char *child_command = NULL;

static int 
node_assign_ip(struct dragon_node_pc* node)
{
  int i;
  struct dragon_if_ip *ifp;
  struct adtlistnode* curnode;
  struct in_addr ip, broadcast, netmask;
  char *c;
  u_int8_t *byte;
  static char command[200];
#ifndef __FreeBSD__
  static char iface_name[50];
#if 0
  int ioctl_ret;
  struct sockaddr_in *sock;
  int sockfd = -1;
  struct ifreq if_info;
#endif
#endif

  if (!node) 
    return 1;
  if (!node->if_list)
    return 0;

  for (curnode = node->if_list->head;
	curnode;
	curnode = curnode->next) {
#ifndef __FreeBSD__
#if 0
    static char bcast[IP_MAXLEN+1];
#endif
#endif 

    ifp = (struct dragon_if_ip*)curnode->data;

#if 0
    bzero(bcast, IP_MAXLEN+1);
    if (sockfd == -1) 
      sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
#endif
    if (interface && ifp->iface == NULL) {
#ifndef __FreeBSD__
      if (ifp->vtag != 0) {
	sprintf(iface_name, "%s.%d", interface, ifp->vtag);
	ifp->iface = strdup(iface_name);

	sprintf(iface_name, "/sbin/vconfig add %s %d", interface, ifp->vtag);
	zlog_info("system(): %s", iface_name);
	system(iface_name);
      } else
#endif
	ifp->iface = strdup(interface);
    }

    if (!ifp->iface)
      continue;

    c = strstr(ifp->assign_ip, "/");
    if (!c) {
      ip.s_addr = inet_addr(ifp->assign_ip);
      netmask.s_addr = inet_addr("255.255.255.0");
      broadcast.s_addr = ip.s_addr | ~(netmask.s_addr);
    } else {
      *c = '\0';
      ip.s_addr = inet_addr(ifp->assign_ip);
      if (strlen(c+1) <= 2) {
        netmask.s_addr = 0xffffffff >> (32-atoi(c+1));
	byte = (u_int8_t*)&(netmask.s_addr);
	for (i = 0; i < 4; i++) {
          if (byte[i] != 0xff && byte[i] != 0x00) {
	    switch (atoi(c+1) % 8) {
	      case 1:
		byte[i] = 0x80;
		break;
	      case 2:
		byte[i] = 0xc0;
		break;
	      case 3:
		byte[i] = 0xe0;
		break;
	      case 4:
		byte[i] = 0xf0;
	        break;
	      case 5:
		byte[i] = 0xf8;
	        break;
	      case 6:
		byte[i] = 0xfc;
		break;
	      case 7:
		byte[i] = 0xfe;
		break;
	    }
	    break;
          }
        }
      } else 
        netmask.s_addr = inet_addr(c+1);
      broadcast.s_addr = ip.s_addr | ~(netmask.s_addr);
      *c = '/';
    }

    zlog_info("node_assing_ip(): iface: %s", ifp->iface);
    zlog_info("ip: %s", inet_ntoa(ip));
    zlog_info("netmask: %s", inet_ntoa(netmask));
    zlog_info("broadcast: %s", inet_ntoa(broadcast));

    sprintf(command, "/sbin/ifconfig %s %s", ifp->iface, inet_ntoa(ip));
    sprintf(command+strlen(command), " netmask %s", inet_ntoa(netmask));

    zlog_info("system(): %s", command);
    system(command); 
#if 0
    bzero(&if_info, sizeof(struct ifreq));
    strcpy(if_info.ifr_name, ifp->iface);
    bzero(&if_info.ifr_ifru.ifru_addr, sizeof(struct sockaddr));
    sock = (struct sockaddr_in*)&(if_info.ifr_ifru.ifru_addr);
    sock->sin_family = AF_INET;
    sock->sin_addr = ip;
    ioctl_ret = ioctl(sockfd, SIOCSIFADDR, &if_info);
    if (ioctl_ret == -1) {
      node->status = ast_failure;
      node->agent_message = strdup("ifconfig failure");
      zlog_err("ifconfig failed for %s", ifp->iface);
      close(sockfd);
      return 1;
    }

#ifndef __FreeBSD__ 
    sock = (struct sockaddr_in*)&(if_info.ifr_ifru.ifru_netmask); 
#else
    sock = (struct sockaddr_in*)&(if_info.ifr_ifru.ifru_addr);
#endif

    sock->sin_addr = netmask;
    ioctl_ret = ioctl(sockfd, SIOCSIFNETMASK, &if_info);
    if (ioctl_ret == -1) {
      node->status = ast_failure;
      node->agent_message = strdup("ifconfig failure");
      zlog_err("ifconfig failed for %s", ifp->iface);
      close(sockfd);
      return 1;
    }
     
    sock = (struct sockaddr_in*)&(if_info.ifr_ifru.ifru_broadaddr);
    sock->sin_addr = broadcast;
    ioctl_ret = ioctl(sockfd, SIOCSIFBRDADDR, &if_info);
    if (ioctl_ret == -1) {
      node->status = ast_failure;
      node->agent_message = strdup("ifconfig failure");
      zlog_err("ifconfig failed for %s", ifp->iface);
      close(sockfd);
      return 1;
    }
#endif
  }

#if 0
  if (sockfd != -1)
    close(sockfd);
#endif
  return 0;
}

static int 
node_delete_ip(struct dragon_node_pc* node)
{
  struct dragon_if_ip *ifp;
  struct adtlistnode* curnode;
  static char iface_name[50];

  if (!node || !node->if_list)
    return 1;

  for (curnode = node->if_list->head;
	curnode;
	curnode = curnode->next) {

    ifp = (struct dragon_if_ip*)curnode->data;
    if (ifp->iface && ifp->vtag) {
#ifndef __FreeBSD__
      sprintf(iface_name, "/sbin/vconfig rem %s", ifp->iface);
      system(iface_name);
#endif
    } else if (ifp->iface) {
#ifndef __FreeBSD__
      sprintf(iface_name, "/sbin/ifconfig %s 0.0.0.0", ifp->iface);
#else
      sprintf(iface_name, "/sbin/ifconfig %s delete", ifp->iface);
#endif
      system(iface_name);
    }
  }

  return 0;
}

static void
handle_app_complete_child()
{
  struct pid_node *curr, *prev;

  /* call waitpid to collect the child; otherwise, it will become a zombie */
  if (child_pids != NULL) {
    for (prev = NULL, curr = child_pids;
	 curr;
	 prev = curr, curr = curr->next) 
      if (waitpid(curr->pid, NULL, WNOHANG) == curr->pid)
	break;

    if (curr) {
      zlog_info("handle_app_complete_child: %d", curr->pid);
      if (prev == NULL) {
	child_pids = child_pids->next;
	free(curr->ast_id);
	free(curr);
      } else {
	prev->next = curr->next;
	free(curr->ast_id);
	free(curr);
      }
    }
  }
}

int
node_process_query_req()
{
  struct adtlistnode *curnode;
  struct resource *node;
  char path[105];
  char directory[80];

  glob_app_cfg->action = query_resp;
  zlog_info("Processing %s, QUERY_REQ", glob_app_cfg->ast_id);

  strcpy(directory, NODE_AGENT_DIR);
  if (mkdir(directory, 0755) == -1 && errno != EEXIST) {
    zlog_err("Can't create directory %s", NODE_AGENT_DIR);
    return 0;
  }

  sprintf(directory, "%s/%s", NODE_AGENT_DIR, glob_app_cfg->ast_id);
  if (mkdir(directory, 0755) == -1) {
    zlog_err("Can't create directory %s", directory);
    return 0;
  }

  sprintf(path, "%s/%s/query_original.xml",
        NODE_AGENT_DIR, glob_app_cfg->ast_id);

  if (rename(NODE_AGENT_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
           NODE_AGENT_RECV, path, errno, strerror(errno));

  sprintf(path, "%s/%s/query_response.xml",
        NODE_AGENT_DIR, glob_app_cfg->ast_id);

  glob_app_cfg->action = query_resp;
  glob_app_cfg->status = ast_success;

  for (curnode = glob_app_cfg->node_list->head;
       curnode;
        curnode = curnode->next) {
    node = (struct resource*) curnode->data;
    node->status = ast_success;
  }

  print_final(path, MINION);
  symlink(path, NODE_AGENT_RET);

  return 1;
}

static void
handleAlarm()
{
  /* don't need to do anything special for SIGALARM
   */
}

static void
handleKill()
{
  char command[500], line[10];
  struct pid_node *cur;
  FILE *fp;
  char* ret;

  zlog_warn("Program %d got killed or coredump", getpid());

  if (node_child_pid == 0) {
    /* this is a CHILD PROCESS */
    if (child_command) { 
   
      close(child_clnt_sock);
      if (!glob_app_cfg) 
        _exit(2); 
      else if (!glob_app_cfg->node_list) 
        _exit(2); 
	
      unlink(NODED_TEMP_FILE); 
      /* Kill the command that the forked node_agent runs 
       */
      sprintf(command, "ps -ax | grep \"%s\" | cut -c1-5 > %s", child_command, NODED_TEMP_FILE ); 
      system(command); 
      
      fp = fopen(NODED_TEMP_FILE, "r"); 
      if (!fp) 
        _exit(2); 
	
      while ((ret = fgets(line, 10, fp)) != NULL) { 
        ret[strlen(ret)-1] = '\0'; 
	kill(atoi(ret), SIGTERM); 
	waitpid(atoi(ret), NULL, 0);
      } 
      
      fclose(fp);
    }
    _exit(2);
  } else if (node_child_pid > 0) {

     for (cur = child_pids; cur; cur=cur->next) {
       kill(cur->pid, SIGTERM);
       zlog_warn("kill %d for %s", cur->pid, cur->ast_id);
     }
  }
  exit(1);
}

/* let ast_masterd to listen on a certain tcp port;
 */
int
main(int argc, char* argv[])
{
  struct sockaddr_in ServAddr;
  int servSock;
  char *p;
  char *progname, *config_file = NULL;
  int daemon_mode = 0;
  struct thread thread;
  struct sigaction myAlarmAction;
  
  progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);

  while (1) {
    int opt;

#ifdef RESOURCE_BROKER
    opt = getopt_long (argc, argv, "dhbc:", ast_master_opts, 0);
    if (argc > 6) {
      usage(progname, 1);
      exit(EXIT_FAILURE);
    }
#else
    opt = getopt_long (argc, argv, "dhc:", ast_master_opts, 0);
    if (argc > 4) {
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
      case 'b':
	broker_mode = 1;
	break;
#endif
      case 'c':
	config_file = optarg;
	break;
      default:
        usage (progname, 1);
    }
  }

  glob_minion = NODE_AGENT;
  glob_minion_ret_xml = NODE_AGENT_RET;
  glob_minion_recv_xml = NODE_AGENT_RECV; 
  dragon_node_pc_mod.minion_proc_func = dragon_node_pc_minion_proc;

  if (config_file) 
    noded_read_config(config_file); 
  else
    noded_read_config("/usr/local/dragon/etc/node_agent.conf");

  /* Change to the daemon program. */
  if (daemon_mode)
    daemon(0, 0);

  master = thread_master_create();

  init_application_module();
  if (init_resource()) {
    zlog_err("There is no resource defined in this ast_master instance; exit ...");
    exit(0);
  }

  init_schema("/usr/local/ast_file/xml_schema/setup_req.rng");

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
  ServAddr.sin_port = htons(DEFAULT_NODE_XML_PORT);

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
  if (broker_mode) {
    if (broker_init() == 1) {
      if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        zlog_err("socket() failed");
        exit(EXIT_FAILURE);
      }
      
      memset(&ServAddr, 0, sizeof(ServAddr));
      ServAddr.sin_family = AF_INET;
      ServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
      ServAddr.sin_port = htons(DEFAULT_NODE_BROKER_PORT);
    
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
      zlog_err("BROKER: failed to init");
  }
#endif

  myAlarmAction.sa_handler = handleAlarm;
  if (sigfillset(&myAlarmAction.sa_mask) < 0) {
    zlog_err("main: sigfillset() failed");
    return 0;
  }
  myAlarmAction.sa_flags = 0;
  if (sigaction(SIGALRM, &myAlarmAction, 0) < 0) {
    zlog_err("main: sigaction() failed");
    return 0;
  }

  myKillAction.sa_handler = handleKill;
  if (sigfillset(&myKillAction.sa_mask) < 0) {
    zlog_err("main: sigfillset() failed");
    return 0;
  }
  sigaction(SIGTERM, &myKillAction, 0);
  sigaction(SIGSEGV, &myKillAction, 0);

  glob_app_cfg = NULL;

  while (thread_fetch (master, &thread))
    thread_call (&thread);

  return 1;
}

static int
agent_accept(struct thread *thread)
{
  int servSock, clntSock;
  struct sockaddr_in clntAddr;
  unsigned int clntLen;
  struct stat file_stat;

  zlog_info("agent_accept(): START");
  servSock = THREAD_FD(thread);

  clntLen = sizeof(clntAddr);
  unlink(NODE_AGENT_RET);
  unlink(NODE_AGENT_RECV);
  child_app_complete = 0;
  node_child = 0;
   
  if ((clntSock = accept(servSock, (struct sockaddr*)&clntAddr, &clntLen)) < 0) 
    zlog_err("accept() failed; error = %d(%s)", errno, strerror(errno));
  else {
    zlog_info("Handling client %s ...", inet_ntoa(clntAddr.sin_addr));
    child_clnt_sock = clntSock;

    if (!recv_file(clntSock, NODE_AGENT_RECV, RCVBUFSIZE, TIMEOUT_SECS, NODE_AGENT)) {

      glob_app_cfg = NULL;
      /* after all the preparation, parse the application xml file
       */
      if (minion_process_xml(clntAddr.sin_addr)) {
	if (!glob_app_cfg)  
	  print_error_response(NODE_AGENT_RET);
	else  
	  glob_app_cfg->status = ast_failure; 
      }

      if (glob_app_cfg) {
        if (glob_app_cfg->action == app_complete && 
                      node_child && !child_app_complete) { 
          close(clntSock); 
          free_application_cfg(glob_app_cfg); 
          glob_app_cfg = NULL; 
          thread_add_read(master, agent_accept, NULL, servSock); 
          return 1;
        } 
        
        if (stat(NODE_AGENT_RET, &file_stat) == -1) 
          /* the result file hasn't been written 
           */ 
          print_error_response(NODE_AGENT_RET); 

        if (glob_app_cfg->action == app_complete) { 
          zlog_info("Sending app_complete for %s to ast_master at (%s:%d)", glob_app_cfg->ast_id, inet_ntoa(glob_app_cfg->ast_ip), MASTER_PORT); 
          close(clntSock); 
          clntSock = -1; 
        } else 
          zlog_info("sending confirmation (%s) to ast_master at (%s:%d)", 
                     status_type_details[glob_app_cfg->status], 
                     inet_ntoa(glob_app_cfg->ast_ip), MASTER_PORT); 

      }

      if (send_file_over_sock(clntSock, NODE_AGENT_RET)) 
	clntSock = send_file_to_agent(inet_ntoa(glob_app_cfg->ast_ip), MASTER_PORT, NODE_AGENT_RET);

      if (glob_app_cfg) { 
        free_application_cfg(glob_app_cfg); 
	glob_app_cfg = NULL; 
	close(clntSock); 
	if (child_app_complete) 
	  _exit(2); 
      } else 
        close(clntSock);
    }
  }

  zlog_info("agent_accept(): DONE");
  thread_add_read(master, agent_accept, NULL, servSock);
  return 0;
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
  int total;

  servSock = THREAD_FD(thread);

  clntLen = sizeof(clntAddr);
  unlink(NODE_AGENT_RET);
  unlink(NODE_AGENT_RECV);
  child_app_complete = 0;
  node_child = 0;
  
  zlog_info("BROKER: START"); 
  if ((clntSock = accept(servSock, (struct sockaddr*)&clntAddr, &clntLen)) < 0) 
    zlog_err("accept() failed; error = %d(%s)", errno, strerror(errno));
  else {
    zlog_info("BROKER: Handling client %s ...", inet_ntoa(clntAddr.sin_addr));

    fp = fopen(NODE_AGENT_RECV, "w");
    total = 0;
    errno = 0;
    alarm(TIMEOUT_SECS);
    while ((recvMsgSize = recv(clntSock, buffer, RCVBUFSIZE-1, 0)) > 0 ) {
      if (errno == EINTR) {
        /* alarm went off
         */
        zlog_warn("broker_accept: probably has not received all data");
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
      fflush(fp);
      fclose(fp);

      glob_app_cfg = topo_xml_parser(NODE_AGENT_RECV, NODE_AGENT);

      if (!glob_app_cfg) {
	zlog_err("Incoming file failed at parsing");
        print_error_response(NODE_AGENT_RET);
      } else 
	broker_process();

      if (send_file_over_sock(clntSock, NODE_AGENT_RET)) 
        zlog_err("Failed to return result to user");

      close(clntSock);
    } else 
      fclose(fp);
    close(clntSock);  
    zlog_info("BROKER: DONE!");
  }
  free_application_cfg(glob_app_cfg);
  glob_app_cfg = NULL;

  thread_add_read(master, broker_accept, NULL, servSock);
  return 1;
}

int 
broker_init()
{
  int ret_value = 1;

  memset(&node_pool, 0, (NUM_NODE_TYPE+1)*sizeof(struct node_tank));

  if ((glob_app_cfg = topo_xml_parser(BROKER_FILE, NODE_AGENT)) == NULL) 
    return 0;

  if (glob_app_cfg->node_list == NULL) 
    ret_value = 0;

  if (ret_value && !broker_read_app_cfg())
    ret_value = 0;

  free_application_cfg(glob_app_cfg);
  glob_app_cfg = NULL;

  return 1;
}  
#endif

#ifdef RESOURCE_BROKER
static int 
broker_read_app_cfg()
{
  struct adtlistnode *curnode;
  struct resource *res_cfg;
  struct node_tank *nodes;

  for (curnode = glob_app_cfg->node_list->head;
	curnode;
	curnode = curnode->next) {
    res_cfg = (struct resource*) curnode->data;
 
    nodes = &node_pool[res_cfg->res.n.stype];
    nodes->es[nodes->number].ip = strdup(res_cfg->res.n.ip);
    nodes->es[nodes->number].router_id = strdup(res_cfg->res.n.router_id);
    nodes->es[nodes->number].tunnel = strdup(res_cfg->res.n.tunnel);
    nodes->es[nodes->number].name = strdup(res_cfg->name);
    nodes->number++;
  }
     
  return 1;
}

static void
print_broker_response(char* path)
{
  struct adtlistnode *curnode;
  struct resource *mynode;
  FILE* fp;

  if (!path)
    return;

  fp = fopen(path, "w+");
  if (!fp)
    return;
  
  fprintf(fp, "<topology>\n");

  fprintf(fp, "<status>%s</status>\n", status_type_details[glob_app_cfg->status]);
  if (glob_app_cfg->details[0] != '\0')
    fprintf(fp, "<details>%s</details>\n", glob_app_cfg->details);

  if (glob_app_cfg->node_list) {
    for ( curnode = glob_app_cfg->node_list->head;
        curnode;
        curnode = curnode->next) {
      mynode = (struct resource*)(curnode->data);

      print_node(fp, mynode);
    }
  } 

  fprintf(fp, "</topology>");
  fflush(fp);
  fclose(fp);
}

static int
broker_process()
{
  struct adtlistnode *curnode;
  struct resource *res_cfg;
  struct node_tank *nodes;

  if (!glob_app_cfg->node_list) {
    zlog_err("No nodes in the incoming file");
    strcpy(glob_app_cfg->details, "No nodes in the incoming file");
    glob_app_cfg->status = ast_failure;
    print_error_response(NODE_AGENT_RET);
    return 0;
  }

  for (curnode = glob_app_cfg->node_list->head;
	curnode;
	curnode = curnode->next) {
    res_cfg = (struct resource*) curnode->data;

    nodes = &node_pool[res_cfg->res.n.stype];
    if (nodes->number == 0) {
      zlog_err("Broker doesn't have type: %s","FIONA");
      res_cfg->agent_message = strdup("Broker doesn't have any of this type");
      res_cfg->status = ast_failure;
      glob_app_cfg->status = ast_failure;
    } else {
      if (nodes->index == nodes->number)
	nodes->index = 0;
      zlog_info("Broker is giving out: %s", nodes->es[nodes->index].name);
      strncpy(res_cfg->res.n.ip, nodes->es[nodes->index].ip, IP_MAXLEN);
      strncpy(res_cfg->res.n.router_id, nodes->es[nodes->index].router_id, IP_MAXLEN);
      strncpy(res_cfg->res.n.tunnel, nodes->es[nodes->index].tunnel, 9);
      nodes->index++;
      res_cfg->status = ast_success;
      glob_app_cfg->status = ast_success;
    }
  }

  print_broker_response(NODE_AGENT_RET);
  return 1;
}
#endif

static void
noded_kill_process()
{
  struct pid_node *cur, *prev, *temp;

  if (!glob_app_cfg)
    return;
  else if (!glob_app_cfg->node_list)
    return;

  for (prev = NULL, cur = child_pids; 
  	cur;
	){
    if (strcmp(cur->ast_id, glob_app_cfg->ast_id) == 0) {
      kill(cur->pid, SIGTERM);
      waitpid(cur->pid, NULL, 0);

      if (prev == NULL)
        child_pids = cur->next;
      else
        prev->next = cur->next;

      if (cur->next) {
        temp = cur;
        cur = cur->next;
	free(temp->ast_id);
	free(temp);
      } else {
	free(cur->ast_id);
        free(cur);
	break;
      }
    } else {
      prev = cur;
      cur = cur->next;
    }
  }
}

int
dragon_node_pc_minion_proc(enum action_type action,
			   struct resource *res)
{
  struct dragon_node_pc *node;
  struct pid_node* new_pids;

  if (!res->res)
    return 1;

  node = (struct dragon_node_pc*)res->res;
  if (action == setup_req) {
     return node_assign_ip(node);

  } else if (action == ast_complete) {
    if (node->command) {
      node_child = 1;
      if ((node_child_pid = fork()) < 0)
        zlog_err("Fork() failed; error = %d(%s)", errno, strerror(errno));
      else if (node_child_pid == 0) {

        /* Child process */

        zlog_info("Running command \"%s\"", node->command);
	child_command = strdup(node->command);
        system(node->command);
        child_app_complete = 1;
      } else {
        /* Parent process */
        /* setup facility to reap the Zombie from child process */
        node_app_complete_action.sa_handler = handle_app_complete_child;

	new_pids = malloc(sizeof(struct pid_node));
	new_pids->pid = node_child_pid;
	new_pids->ast_id = strdup(glob_app_cfg->ast_id);
	new_pids->next = child_pids;
	child_pids = new_pids;

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
  } else if (action == release_req) {

    noded_kill_process();
    node_delete_ip(node);
  }

  return 0;
}
