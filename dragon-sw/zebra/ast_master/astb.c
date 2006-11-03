#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "getopt.h"
#include "ast_master.h"
#include "local_id_cfg.h"

#define ASTB_RESULT_FILE	"/tmp/astb_result.xml"
#define ASTB_SEND_FILE		"/tmp/astb_sent.xml"

struct thread_master *master; /* master = dmaster.master */

struct option astb_opts[] =
{
  { "input_file", 	required_argument, NULL, 'f'},
  { "help",       	no_argument,       NULL, 'h'},
  { "ast_id", 	required_argument, NULL, 'g'},
  { "type",		required_argument, NULL, 't'},
  { "signal",		required_argument, NULL, 's'},
  { 0 }
};

static void
usage (char *progname, int status)
{
  if (status != 0)
    printf("Try \"%s --help\" for more information.\n", progname);
  else {
    printf ("Usage : %s [OPTION...]\n\
NSF AST Builder.\n\n\
-i, --input file   	File to ast_master\n\
-h, --help	 	Display this help and exit\n\
-g, --ast_id  	ast_id\n\
-t, --resource_type	Resourse type, node or link\n\
-s, --signal	   	Signal sent to ast_master\n\
-v, --validate	   	File to be validated\n\
\n", progname);
    printf("To send file to ast_master, \"astb -f <file>\"\n");
    printf("To validate file to send to ast_master, \"astb -v <file>\"\n");
    printf("To send signal, \"astb -t node|type -s APP_COMPLETE -g <ast_id>\"\n");
  }

  exit (status);
}

void 
send_app_complete(int type)
{
  char file[100];
  struct stat fs;
  char* ast_id;
  struct adtlistnode *curnode;
  struct resource *node, *link;
  int sock, fd, total;
  struct sockaddr_in astServ;

  ast_id = glob_app_cfg->ast_id;
  glob_app_cfg->ast_id = NULL;

  sprintf(file, "%s/%s/setup_response.xml", 
		type == 1 ? NODE_AGENT_DIR:LINK_AGENT_DIR, ast_id);
  if (stat(file, &fs) == -1) {
    printf("Can't locate ast_id file for %s\n", type == 1 ? "node":"link");
    exit(0);
  }

  free(glob_app_cfg->ast_id);
  
  if ((glob_app_cfg = topo_xml_parser(file, ASTB)) == NULL) {
    printf("Error in ast_id file\n");
    exit(0);
  }

  if (glob_app_cfg->action == RELEASE_RESP) {
    printf("ast_id file indicate this the resource has been released\n");
    exit(0);
  }
 
  if (glob_app_cfg->ast_ip == NULL) {
    printf("ast_id file has no ast_ip specified; don't know how to contact the agent\n");
    exit(0);
  }
 
  if (type == 1) {
    if (glob_app_cfg->node_list == NULL) {
      printf("the node_agent file for ast_id contains no node\n");
      exit(0);
    }

    for (curnode = glob_app_cfg->node_list->head;
	 curnode;
	 curnode = curnode->next) {
      node = (struct resource*) curnode->data;
      node->status = AST_APP_COMPLETE;
    }
  } else {
    if (glob_app_cfg->link_list == NULL) {
      printf("the link_agent file for ast_id contains no link\n");
      exit(0);
    } 

    for (curnode = glob_app_cfg->link_list->head;
	 curnode;
	 curnode = curnode->next) {
      link = (struct resource*) curnode->data;
      link->status = AST_APP_COMPLETE;
    }
  }
  glob_app_cfg->status = AST_APP_COMPLETE;
  glob_app_cfg->action = APP_COMPLETE;

  sprintf(file, "%s/%s/final.xml",
	        type == 1 ? NODE_AGENT_DIR:LINK_AGENT_DIR, ast_id);
  print_final(file);

  printf("Sending APP_COMPLETE to ast_master at %s:%d\n",
	glob_app_cfg->ast_ip, MASTER_PORT);

  if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    printf("socket() failed\n");
    exit(1);
  }

  memset(&astServ, 0, sizeof(astServ));
  astServ.sin_family = AF_INET;
  astServ.sin_addr.s_addr = inet_addr(glob_app_cfg->ast_ip);
  astServ.sin_port = htons(MASTER_PORT);

  fd = open(file, O_RDONLY);

  if (fd == -1) {
    printf("open() failed; err = %s\n", strerror(errno));
    exit(1);
  }

  if (connect(sock, (struct sockaddr*)&astServ, sizeof(astServ)) < 0)
  {
    printf("connect() failed\n");
    exit(1);
  }

#ifdef __FreeBSD__
  total = sendfile(fd, sock, 0, 0, NULL, NULL, 0);
  printf("sendfile() returns %d\n", total);
#else
  if (fstat(fd, &fs) == -1) {
    printf("fstat() failed on %s\n", file);
    exit(1);
  }
  total = sendfile(sock, fd, 0, fs.st_size);
#endif
  if (total < 0)
    printf("sendfile() failed; error = %d(%s)\n", errno, strerror(errno));
  else 
    printf("Message has been successfully sent to ast_master\n");
  close(fd);
  close(sock);

  exit(1);
}

int main(int argc, char* argv[])
{
  int sock;
  struct sockaddr_in astServ;
  int fd;
  char buffer[RCVBUFSIZE];
  int bytesRcvd, total;
  char *p, *progname, *input_file = NULL;
  struct stat file_stat;
  FILE *ret_file;
  int type = 0; /* 1 - node, 2 - link */
  int signal_mode = 0, validate = 0;

  progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);

  while (1) {
    int opt;
  
    opt = getopt_long (argc, argv, "f:t:g:s:v:h", astb_opts, 0);
    if (argc != 3 && argc != 7) {
      usage(progname, 0);
      exit(EXIT_FAILURE);
    }

    if (opt == EOF)
      break;

    switch (opt) {
      case 0:
	break;
      case 'f':
	input_file = optarg;
	break;
      case 'v':
	validate = 1;
	input_file = optarg;
	break;
      case 'h':
	usage(progname, 0);
	break;
      case 't':
	if (strcmp(optarg, "node") == 0) 
	  type = 1;
 	else if (strcmp(optarg, "link") == 0)
	  type = 2;
	else {
	  printf("You must specify \"node\" or \"link\" for resource type (-t)\n");
	  usage(progname, 0);
	}
	break;
      case 's':
	signal_mode = 1;
	if (strcmp(optarg, "APP_COMPLETE") == 0)
	  glob_app_cfg->action = APP_COMPLETE;
	else {
	  printf("You must specify \"APP_COMPLETE\" for signal (-s)\n");
	  usage(progname, 0);
	}
	break;
      case 'g':
	glob_app_cfg->ast_id = optarg;
	break;
      default:
	usage (progname, 0);
    }
  }

  if (signal_mode) {
    if (input_file) 
      usage (progname, 0);

    if (!type || !glob_app_cfg->ast_id) 
      usage (progname, 0);

    send_app_complete(type);
  }

  if (!input_file) 
    usage(progname, 0);

  if (stat(input_file, &file_stat) == -1) {
    printf("Can't locate %s\n", input_file);
    exit(1);
  }

  switch (xml_parser(input_file)) {

  case TOPO_XML:
  if ( (glob_app_cfg = topo_xml_parser(input_file, ASTB)) == NULL) {
    printf("Validation Failed\n");
    exit(1);
  }

  if (topo_validate_graph(ASTB, glob_app_cfg) != 1) {
    printf("Validation Failed\n");
    exit(1);
  }
  break;

  case ID_XML:

  if ((glob_app_cfg = id_xml_parser(input_file, ASTB)) == NULL) {
    printf("Validation Failed\n");
    exit(1);
  }

  break;
  }

  if (validate) {
    printf("File is validated\n");
    exit(1);
  }

  if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    printf("socket() failed\n");
    exit(1);
  }

  memset(&astServ, 0, sizeof(astServ));
  astServ.sin_family = AF_INET;
  astServ.sin_addr.s_addr = inet_addr("127.0.0.1");
  astServ.sin_port = htons(MASTER_PORT);

  fd = open(input_file, O_RDONLY);

  if (fd == -1) {
    printf("open() failed; err = %s\n", strerror(errno));
    exit(1);
  }

  if (connect(sock, (struct sockaddr*)&astServ, sizeof(astServ)) < 0)
  {
    printf("connect() failed\n");
    exit(1);
  }

#ifdef __FreeBSD__
  total = sendfile(fd, sock, 0, 0, NULL, NULL, 0);
  printf("sendfile() returns %d\n", total);
#else
  if (fstat(fd, &file_stat) == -1) {
    printf("fstat() failed on %s\n", input_file);
    exit(1);
  }
  total = sendfile(sock, fd, 0, file_stat.st_size);
#endif
  if (total < 0)
    printf("sendfile() failed; error = %d(%s)\n", errno, strerror(errno));
  else { 
    total = 0;
    while ((bytesRcvd = recv(sock, buffer, RCVBUFSIZE-1, 0))  > 0) {
  
      if (!total) {
	ret_file = fopen(ASTB_RESULT_FILE, "w");
	printf("Confirmation from ast_master\n");
	if (ret_file) 
  	printf("; result will be saved at %s temporarily\n", ASTB_RESULT_FILE);
      }
      total+=bytesRcvd;
      buffer[bytesRcvd] = '\0';
      printf("%s", buffer);
      if (ret_file)
	fprintf(ret_file, buffer);
    }
  }

  if (total == 0)
    printf("No confirmation from ast_master\n");   
  else {
    if (ret_file) {
      fflush(ret_file);
      fclose(ret_file);
    }
  }    
  close(sock);
  close(fd);
  exit(0);
}
