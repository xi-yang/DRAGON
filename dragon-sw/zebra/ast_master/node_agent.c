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
extern char * node_stype_name[];
struct thread_master *master; /* master = dmaster.master */
static int agent_accept(struct thread *);
static void noded_kill_process();

static char* interface;
static char* loopback;

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

struct pid_node {
  pid_t pid;
  struct pid_node* next;
};

static struct pid_node* child_pids = NULL;

static pid_t node_child_pid = 1;

static int 
node_assign_ip(struct resource* node)
{
  struct ifreq if_info;
  int sockfd = -1, i;
  struct sockaddr_in *sock;
  int ioctl_ret;
  struct if_ip *ifp;
  struct adtlistnode* curnode;
#ifndef __FreeBSD__
  static char iface_name[50];
#endif
  struct in_addr ip;
  struct in_addr broadcast;
  struct in_addr netmask;
  char *c;
  u_int8_t *byte;
  static char command[200];

  if (!node->res.n.if_list)
    return 1;

  node->status = AST_SUCCESS; 
  for (curnode = node->res.n.if_list->head;
	curnode;
	curnode = curnode->next) {
    static char bcast[IP_MAXLEN+1];

    ifp = (struct if_ip*)curnode->data;

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
      node->status = AST_FAILURE;
      node->agent_message = strdup("ifconfig failure");
      zlog_err("ifconfig failed for %s", ifp->iface);
      close(sockfd);
      return 0;
    }

#ifndef __FreeBSD__ 
    sock = (struct sockaddr_in*)&(if_info.ifr_ifru.ifru_netmask); 
#else
    sock = (struct sockaddr_in*)&(if_info.ifr_ifru.ifru_addr);
#endif

    sock->sin_addr = netmask;
    ioctl_ret = ioctl(sockfd, SIOCSIFNETMASK, &if_info);
    if (ioctl_ret == -1) {
      node->status = AST_FAILURE;
      node->agent_message = strdup("ifconfig failure");
      zlog_err("ifconfig failed for %s", ifp->iface);
      close(sockfd);
      return 0;
    }
     
    sock = (struct sockaddr_in*)&(if_info.ifr_ifru.ifru_broadaddr);
    sock->sin_addr = broadcast;
    ioctl_ret = ioctl(sockfd, SIOCSIFBRDADDR, &if_info);
    if (ioctl_ret == -1) {
      node->status = AST_FAILURE;
      node->agent_message = strdup("ifconfig failure");
      zlog_err("ifconfig failed for %s", ifp->iface);
      close(sockfd);
      return 0;
    }
#endif
  }

#if 0
  if (sockfd != -1)
    close(sockfd);
#endif

  return 1;
}

static int 
node_delete_ip(struct resource* node)
{
  struct if_ip *ifp;
  struct adtlistnode* curnode;
  static char iface_name[50];

  if (!node->res.n.if_list)
    return 1;

  node->status = AST_SUCCESS; 

  for (curnode = node->res.n.if_list->head;
	curnode;
	curnode = curnode->next) {

    ifp = (struct if_ip*)curnode->data;
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

  return 1;
}

int 
node_process_setup_req()
{
  struct adtlistnode *curnode;
  struct resource *node;
  char path[105];
  char directory[80];

  glob_app_cfg->action = SETUP_RESP;
  zlog_info("Processing ast_id: %s, SETUP_REQ", glob_app_cfg->ast_id);

  strcpy(directory, NODE_AGENT_DIR);
  if (mkdir(directory, 0755) == -1 && errno != EEXIST) {
    set_allres_fail("Can't create NODE_AGENT dir");
    print_xml_response(NODE_AGENT_RET, NODE_AGENT);
    return 0;
  }

  sprintf(directory+strlen(directory), "/%s", glob_app_cfg->ast_id);
  if (mkdir(directory, 0755) == -1) {
    if (errno == EEXIST) {
      set_allres_fail("ast_id is already used");
      print_xml_response(NODE_AGENT_RET, NODE_AGENT);
      return 0;
    } else {
      set_allres_fail("Can't create NODE_AGENT dir with ast_id");
      print_xml_response(NODE_AGENT_RET, NODE_AGENT);
      return 0;
    }
  }

  sprintf(path, "%s/setup_original.xml", directory);
  if (rename(NODE_AGENT_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
           NODE_AGENT_RECV, path, errno, strerror(errno));
 
  sprintf(path, "%s/setup_response.xml", directory);
  glob_app_cfg->status = AST_SUCCESS;
  for (curnode = glob_app_cfg->node_list->head;
       curnode;
	curnode = curnode->next) {
    node = (struct resource*) curnode->data;
    if (!node_assign_ip(node)) {
      glob_app_cfg->status = AST_FAILURE;
      node->status = AST_FAILURE;
      break;
    } else
      node->status = AST_SUCCESS;
  } 

  glob_app_cfg->action = SETUP_RESP;
  print_xml_response(path, NODE_AGENT);
  symlink(path, NODE_AGENT_RET);

  sprintf(path, "%s/final.xml", directory);
  print_final(path);

  return (glob_app_cfg->status == AST_SUCCESS);
}

static void
handle_app_complete_child()
{
  struct pid_node *curr, *prev;

  /* call waitpid to collect the child; otherwise, it will become a zombie */
  if (child_pids != NULL) {
    for (prev = NULL, curr = child_pids;
	 curr;
	 prev = curr, curr = curr->next) {
      if (waitpid(curr->pid, NULL, WNOHANG) == curr->pid)
	break;
    }
    if (curr) {
      if (prev == NULL) {
	child_pids = child_pids->next;
	free(curr);
      } else {
	prev->next = curr->next;
	free(curr);
      }
    }
  }
  zlog_info("handle_app_complete_child: APP_COMPLETE child has exited");
}

int
node_process_ast_complete()
{
  struct adtlistnode *curnode;
  struct resource *node;
  char path[105];
  struct application_cfg *working_app_cfg;

  glob_app_cfg->action = APP_COMPLETE;
  zlog_info("Processing ast_id: %s, AST_COMPLETE", glob_app_cfg->ast_id);

  working_app_cfg = glob_app_cfg;

  glob_app_cfg = retrieve_app_cfg(working_app_cfg->ast_id, NODE_AGENT);
  if (!glob_app_cfg) {
    glob_app_cfg = working_app_cfg;
    set_allres_fail("can't retrieve informaton for this ast_id successfully");
    print_xml_response(NODE_AGENT_RET, NODE_AGENT);
    return 0;
  }

  if (glob_app_cfg->action == RELEASE_RESP) {
    free_application_cfg(glob_app_cfg);
    glob_app_cfg = working_app_cfg;
    set_allres_fail("ast_id has received RELEASE_REQ already");
    print_xml_response(NODE_AGENT_RET, NODE_AGENT);
    return 0;
  }

  if (strcmp(glob_app_cfg->ast_ip, working_app_cfg->ast_ip) != 0)
    zlog_warn("NEW ast_ip: %s, OLD ast_ip: %s",
                working_app_cfg->ast_ip, glob_app_cfg->ast_ip);

  sprintf(path, "%s/%s/setup_response.xml", NODE_AGENT_DIR, glob_app_cfg->ast_id);
  free_application_cfg(glob_app_cfg);
  if ((glob_app_cfg = topo_xml_parser(path, NODE_AGENT)) == NULL) {
    glob_app_cfg = working_app_cfg;
    set_allres_fail("didn't parse the ast_id file successfully");
    print_xml_response(NODE_AGENT_RET, NODE_AGENT);
    return 0;
  }
  glob_app_cfg->action = working_app_cfg->action;
  glob_app_cfg->ast_ip = working_app_cfg->ast_ip;
  working_app_cfg->ast_ip = NULL;
  free_application_cfg(working_app_cfg);
  working_app_cfg = NULL;

  sprintf(path, "%s/%s/ast_complete.xml",
          NODE_AGENT_DIR, glob_app_cfg->ast_id);

  if (rename(NODE_AGENT_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
        NODE_AGENT_RECV, path, errno, strerror(errno));

  for (curnode = glob_app_cfg->node_list->head;
       curnode;
        curnode = curnode->next) {
    node = (struct resource*) curnode->data;
    if (node->agent_message) {
      free(node->agent_message);
      node->agent_message = NULL;
    }
    if (node->res.n.command) {
      node_child = 1;
      if ((node_child_pid = fork()) < 0) 
	zlog_err("Fork() failed; error = %d(%s)", errno, strerror(errno));
      else if (node_child_pid == 0) {

	/* Child process */
	zlog_info("Running command \"%s\"", node->res.n.command);
	system(node->res.n.command);
        child_app_complete = 1;
      } else {
	/* Parent process */
        /* setup facility to reap the Zombie from child process */
        node_app_complete_action.sa_handler = handle_app_complete_child;
        if (child_pids == NULL) {
	  child_pids = malloc(sizeof(struct pid_node));
	  child_pids->pid = node_child_pid;
	  child_pids->next = NULL;
	} else {
	  struct pid_node* new_pids = malloc(sizeof(struct pid_node));
	  new_pids->pid = node_child_pid;
	  new_pids->next = child_pids;
	  child_pids = new_pids;
	}

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

    node->status = AST_APP_COMPLETE;
  }
  glob_app_cfg->status = AST_APP_COMPLETE;
  sprintf(path, "%s/%s/final.xml",
          NODE_AGENT_DIR, glob_app_cfg->ast_id);
  print_final(path);
  symlink(path, NODE_AGENT_RET);

  return 1;
}

int 
node_process_release_req()
{
  struct adtlistnode *curnode;
  struct resource *node;
  char path[105];
  struct application_cfg *working_app_cfg;
  int kill_p = 0;

  glob_app_cfg->action = RELEASE_RESP;
  zlog_info("Processing ast_id: %s, RELEASE_REQ", glob_app_cfg->ast_id);

  working_app_cfg = glob_app_cfg;
  
  if ((glob_app_cfg = retrieve_app_cfg(working_app_cfg->ast_id, NODE_AGENT)) == NULL) {
    glob_app_cfg = working_app_cfg;
    set_allres_fail("can't retrieve informaton for this ast_id successfully");
    print_xml_response(NODE_AGENT_RET, NODE_AGENT);
    return 0;
  }

  /* before processing, set all link's status = AST_FAILURE
   */
  if (glob_app_cfg->action == RELEASE_RESP ||
	glob_app_cfg->action == RELEASE_REQ) {
    free_application_cfg(glob_app_cfg);
    glob_app_cfg = working_app_cfg;
    set_allres_fail("ast_id has received RELEASE_REQ already");
    print_xml_response(NODE_AGENT_RET, NODE_AGENT);
    return 0;
  }

  if (glob_app_cfg->action == APP_COMPLETE || glob_app_cfg->action == AST_COMPLETE) 
    kill_p = 1;

  if (strcmp(glob_app_cfg->ast_ip, working_app_cfg->ast_ip) != 0) 
    zlog_warn("NEW ast_ip: %s in this RELEASE_REQ, OLD ast_ip: %s",
		working_app_cfg->ast_ip, glob_app_cfg->ast_ip);

  sprintf(path, "%s/%s/setup_response.xml", NODE_AGENT_DIR, glob_app_cfg->ast_id);
  free_application_cfg(glob_app_cfg);
  if ((glob_app_cfg = topo_xml_parser(path, NODE_AGENT)) == NULL) {
    glob_app_cfg = working_app_cfg;
    set_allres_fail("didn't parse the file for ast_id successfully");
    print_xml_response(NODE_AGENT_RET, NODE_AGENT);
    return 0;
  }

  if (kill_p)
    noded_kill_process();

  glob_app_cfg->action = RELEASE_RESP;
  glob_app_cfg->ast_ip = working_app_cfg->ast_ip;
  working_app_cfg->ast_ip = NULL;
  free_application_cfg(working_app_cfg);
  working_app_cfg = NULL;

  sprintf(path, "%s/%s/release_origianl.xml",
          NODE_AGENT_DIR, glob_app_cfg->ast_id);

  if (rename(NODE_AGENT_RECV, path) == -1)
    zlog_err("Can't rename %s to %s; errno = %d(%s)",
           NODE_AGENT_RECV, path, errno, strerror(errno));

  sprintf(path, "%s/%s/release_response.xml",
          NODE_AGENT_DIR, glob_app_cfg->ast_id);

  for (curnode = glob_app_cfg->node_list->head;
       curnode;
        curnode = curnode->next) {
    node = (struct resource*) curnode->data;
    node->status = AST_SUCCESS;
    if (node->agent_message) {
      free(node->agent_message);
      node->agent_message = NULL;
    }
    node_delete_ip(node);
  }
  
  glob_app_cfg->status = AST_SUCCESS;
  glob_app_cfg->action = RELEASE_RESP;
  print_xml_response(path, NODE_AGENT);
  symlink(path, NODE_AGENT_RET);

  sprintf(path, "%s/%s/final.xml",
          NODE_AGENT_DIR, glob_app_cfg->ast_id);
  print_final(path);

  return 1;
}

int
node_process_query_req()
{
  struct adtlistnode *curnode;
  struct resource *node;
  char path[105];
  char directory[80];

  glob_app_cfg->action = QUERY_RESP;
  zlog_info("Processing ast_id: %s, QUERY_REQ", glob_app_cfg->ast_id);

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

  glob_app_cfg->action = QUERY_RESP;
  glob_app_cfg->status = AST_SUCCESS;

  for (curnode = glob_app_cfg->node_list->head;
       curnode;
        curnode = curnode->next) {
    node = (struct resource*) curnode->data;
    node->status = AST_SUCCESS;
  }

  print_final(path);
  symlink(path, NODE_AGENT_RET);

  return 1;
}

int
noded_process_xml()
{
  if (glob_app_cfg->action != SETUP_REQ &&
      glob_app_cfg->action != RELEASE_REQ &&
      glob_app_cfg->action != QUERY_REQ &&
      glob_app_cfg->action != AST_COMPLETE) {
    zlog_err("noded_process_xml: invalid <action> in xml file");
    sprintf(glob_app_cfg->details, "invalid action in xml file");
    return 0;
  }

  if (glob_app_cfg->action != SETUP_REQ &&
        glob_app_cfg->ast_id == NULL) {
    set_allres_fail("ast_id should be set in non-setup request case");
    return 0;
  }

  if (glob_app_cfg->node_list == NULL) {
    set_allres_fail("noded_process_xml: should have at least one node in xml file");
    return 0;
  }

  if (glob_app_cfg->action == SETUP_REQ) {
    node_process_setup_req();
  } else if ( glob_app_cfg->action == AST_COMPLETE) {
    node_process_ast_complete();
  } else if (glob_app_cfg->action == RELEASE_REQ) {
    node_process_release_req();
  } else if (glob_app_cfg->action == QUERY_REQ) {
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
  char *progname, *config_file = NULL;
  int daemon_mode = 0;
  int broker_mode = 0;
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

  if (config_file) 
    noded_read_config(config_file); 
  else
    noded_read_config("/usr/local/dragon/etc/node_agent.conf");

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
  if (broker_mode) {
    if (broker_init() == 1) {
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
  int recvMsgSize;
  char buffer[RCVBUFSIZE];
  FILE* fp;
  int total;
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

    fp = fopen(NODE_AGENT_RECV, "w");
    total = 0;
    errno = 0;
    alarm(TIMEOUT_SECS);
    while ((recvMsgSize = recv(clntSock, buffer, RCVBUFSIZE-1, 0)) > 0 ) {
      if (errno == EINTR) {
        /* alarm went off
         */
        zlog_warn("agent_accept: dragon probably has not received all datas");
        zlog_warn("recvMsgSize = %d", recvMsgSize);
	buffer[recvMsgSize]='\0';
	fprintf(fp, "%s", buffer);
	total += recvMsgSize;
        break;
      }
      buffer[recvMsgSize]='\0';
      fprintf(fp, "%s", buffer);
      total += recvMsgSize;
      alarm(TIMEOUT_SECS);
    }
    alarm(0);
 
    zlog_info("agent_accept: total byte received = %d", total);
    if (total != 0) {
      fflush(fp);
      fclose(fp);

      glob_app_cfg = NULL;
      /* after all the preparation, parse the application xml file
       */
      if ((glob_app_cfg = topo_xml_parser(NODE_AGENT_RECV, FULL_VERSION)) == NULL) {
        zlog_err("agent_accept(): topo_xml_parser() failed");
	print_error_response(NODE_AGENT_RET);
      } else if (topo_validate_graph(NODE_AGENT, glob_app_cfg) == 0) {
        zlog_err("agent_accept(): topo_validate_graph() failed");
	print_error_response(NODE_AGENT_RET);
      } else {
	glob_app_cfg->ast_ip = strdup(inet_ntoa(clntAddr.sin_addr));
  
	if (noded_process_xml() == 0) 
	  glob_app_cfg->status = AST_FAILURE;
  
	if (glob_app_cfg->action == APP_COMPLETE && 
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
 
	if (glob_app_cfg->action == APP_COMPLETE) {
	  zlog_info("Sending APP_COMPLETE for ast_id: %s to ast_master at (%s:%d)", glob_app_cfg->ast_id, glob_app_cfg->ast_ip, MASTER_PORT);
	  close(clntSock);
	  clntSock = -1;
	} else 
	  zlog_info("sending confirmation (%s) to ast_master at (%s:%d)",
		      status_type_details[glob_app_cfg->status],
		      glob_app_cfg->ast_ip, MASTER_PORT); 
	
      }

     if (!send_file_over_sock(clntSock, NODE_AGENT_RET)) 
	clntSock = send_file_to_agent(glob_app_cfg->ast_ip, MASTER_PORT, NODE_AGENT_RET);

      free_application_cfg(glob_app_cfg);
      glob_app_cfg = NULL;
      close(clntSock);
      if (child_app_complete) 
	_exit(2);
    } else {
      fclose(fp);
      close(clntSock);
    }
  }

  zlog_info("agent_accept(): DONE");
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

      if (send_file_over_sock(clntSock, NODE_AGENT_RET) == 0) 
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
      ret_file = fopen("/usr/local/noded_void.xml", "w");
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

    zlog_err("SHOULD NOT RECEIVED ANYTHING HERE: look at /usr/local/noded_void.xml");
  }

  close(servSock);
  zlog_info("FIN_accept(): END");
  return 1;
}

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
    glob_app_cfg->status = AST_FAILURE;
    print_error_response(NODE_AGENT_RET);
    return 0;
  }

  for (curnode = glob_app_cfg->node_list->head;
	curnode;
	curnode = curnode->next) {
    res_cfg = (struct resource*) curnode->data;

    nodes = &node_pool[res_cfg->res.n.stype];
    if (nodes->number == 0) {
      zlog_err("Broker doesn't have type: %s", node_stype_name[res_cfg->res.n.stype]);
      res_cfg->agent_message = strdup("Broker doesn't have any of this type");
      res_cfg->status = AST_FAILURE;
      glob_app_cfg->status = AST_FAILURE;
    } else {
      if (nodes->index == nodes->number)
	nodes->index = 0;
      zlog_info("Broker is giving out: %s", nodes->es[nodes->index].name);
      strncpy(res_cfg->res.n.ip, nodes->es[nodes->index].ip, IP_MAXLEN);
      strncpy(res_cfg->res.n.router_id, nodes->es[nodes->index].router_id, IP_MAXLEN);
      strncpy(res_cfg->res.n.tunnel, nodes->es[nodes->index].tunnel, 9);
      nodes->index++;
      res_cfg->status = AST_SUCCESS;
      glob_app_cfg->status = AST_SUCCESS;
    }
  }

  print_broker_response(NODE_AGENT_RET);
  return 1;
}

static void
noded_kill_process()
{
  struct resource * res;
  char command[500], line[10];
  FILE *fp;
  char* ret;

  if (!glob_app_cfg)
    return;
  else if (!glob_app_cfg->node_list)
    return;

  unlink(NODED_TEMP_FILE);
  /* Kill the command that the forked node_agent runs
   */
  res = (struct resource*)glob_app_cfg->node_list->head->data;
  sprintf(command, "ps -ax | grep \"%s\" | cut -c1-5 > %s", res->res.n.command, NODED_TEMP_FILE );
  system(command);

  fp = fopen(NODED_TEMP_FILE, "r");
  if (!fp)
    return;

  while ((ret = fgets(line, 10, fp)) != NULL) {
    ret[strlen(ret)-1] = '\0';
    kill(atoi(ret), SIGTERM);
  }

  fclose(fp);
}
