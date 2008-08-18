/****************************************************************************

DRAGON Monitoring API Server source file dragon_mon_apiserver.h
Created by Xi Yang @ 08/15/2008
To be incorporated into GNU Zebra  - DRAGON extension

****************************************************************************/
#include <zebra.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "linklist.h"
#include "memory.h"
#include "buffer.h"
#include "network.h"
#include "log.h"
#include "getopt.h"
#include "dragond.h"
#include "dragon_mon_apiserver.h"

/*not used here*/
struct dragon_master dmaster;
struct thread_master *master;

u_int32_t get_ucid()
{
  return (u_int32_t)getpid();
}

u_int32_t get_seqence_number()
{
  static u_int32_t seqnum = 1;
  return seqnum++;
}


int mon_apiclient_connect (char* host, int port)
{
    struct sockaddr_in addr;
    struct hostent *hp;
    int fd;
    int ret;
    int on = 1;
	
    hp = gethostbyname (host);
    if (!hp)
    {
        printf( "mon_apiclient_connect: no such host %s\n", host);
        exit(1);
    }

    fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        printf( "mon_apiclient_connect: socket(): %s\n", strerror (errno));
        exit(1);
    }
                                                                              
    ret = setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, (void *) &on, sizeof (on));
    if (ret < 0)
    {
        printf( "mon_apiclient_connect: SO_REUSEADDR failed\n");
        close (fd);
        exit(1);
    }
  
  #ifdef SO_REUSEPORT
    ret = setsockopt (fd, SOL_SOCKET, SO_REUSEPORT,
                      (void *) &on, sizeof (on));
    if (ret < 0)
    {
        printf( "mon_apiclient_connect: SO_REUSEADDR failed\n");
        close (fd);
        exit(1);
    }
  #endif /* SO_REUSEPORT */
  
    /* Prepare address structure for connect */
    memset (&addr, 0, sizeof (struct sockaddr_in));
    memcpy (&addr.sin_addr, hp->h_addr, hp->h_length);
    addr.sin_family = AF_INET;
    addr.sin_port = htons (port);
  
    ret = connect (fd, (struct sockaddr *) &addr, sizeof (struct sockaddr_in));
    if (ret < 0)
    {
        printf( "narbapi_connect: connect(): %s\n", strerror (errno));
        close (fd);
        exit(1);
    }
  
    return fd;
}

int mon_apiclient_send_query (int fd, u_int8_t type, char* gri)
{
  static char gri_tlv[MAX_MON_NAME_LEN+4];
  int rc = 0;
  u_int16_t bodylen = 0;
  struct mon_api_msg* msg;
  
  assert(fd > 0);

  if (gri)
    {
      struct dragon_tlv_header* tlv = (struct dragon_tlv_header*)gri_tlv;
      tlv->type = htons(MON_TLV_GRI);
      tlv->length = htons(MAX_MON_NAME_LEN);
      bodylen += sizeof(struct dragon_tlv_header);
      strncpy(gri_tlv+bodylen, gri, MAX_MON_NAME_LEN+1);
      bodylen += MAX_MON_NAME_LEN;
    }
  msg = mon_api_msg_new(type, MON_API_ACTION_RTRV, bodylen, get_ucid(), get_seqence_number(), 0, gri_tlv);
  assert(msg);

  mon_api_msg_write(fd, msg);
  return rc;
}

#define query_switch(FD) mon_apiclient_send_query(FD, MON_API_MSGTYPE_SWITCH, NULL)
#define query_circuit(FD, GRI) mon_apiclient_send_query(FD, MON_API_MSGTYPE_SWITCH, GRI)

/* DRAGON MON_APIClient options. */
struct option longopts[] = 
{
  { "switch",     no_argument,       NULL, 's'},
  { "circuit",     required_argument,       NULL, 'c'},
  { "help",        no_argument,       NULL, 'h'},
  { "version",     no_argument,       NULL, 'v'},
  { 0 }
};

/* Help information display. */
static void
usage (char *progname, int status)
{
  if (status != 0)
    fprintf (stderr, "Try `%s --help' for more information.\n", progname);
  else
    {    
      printf ("Usage : %s [OPTION...]\n\
NSF DRAGON gateway daemon.\n\n\
-s, --switch      Switch information\n\    
-c, --circuit      Switch information\n\
-h, --help         Display this help and exit\n\
-v, --version    Print program version\n\
\n", progname);
    }
  exit (status);
}


/* DRAGONd main routine. */
int
main (int argc, char **argv)
{
  char *p;
  char *progname;
  int sock;
  int is_query_switch = 0;
  int is_query_circuit = 0;
  char* gri = NULL;
  struct mon_api_msg* rmsg = NULL;
  int ret = 0;
  
  /* get program name */
  progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);

  while (1) 
    {
      int opt;

      opt = getopt_long (argc, argv, "sc:hv", longopts, 0);
    
      if (opt == EOF)
        break;

      switch (opt) 
        {
        case 0:
          break;
        case 'c':
          is_query_circuit = 1;
          gri = optarg;
          break;
        case 's':
          is_query_switch = 1;
          break;
        case 'h':
          usage (progname, 0);
          break;
	 case 'v':
          printf ("%s version %s\n", progname, DRAGON_VERSION);
          printf ("Copyright 2004-2008, the NSF DRAGON Project\n");
          printf ("Extended from GNU Zebra -- Copyright 1996-2001\n");
          exit (0);
          break;
        default:
          usage (progname, 1);
          break;
        }
    }

  sock =  mon_apiclient_connect("localhost", mon_apiserver_getport());
  if (sock < 0)
    {
      printf( "mon_apiclient_connect() failed\n");
      exit(2);
    }
  if (is_query_circuit && gri)
    {
      ret = query_circuit(sock, gri);
      if (ret != 0)
        {
          printf( "query_circuit() failed\n");
          exit(3);
        }
      rmsg = mon_api_msg_read(sock);
      if (rmsg == NULL)
        {
          printf( "query_circuit() failed\n");
          exit(4);
        }
      /* display switch info*/
    }
  else if (is_query_switch)
    {
      ret = query_switch(sock);
      if (ret != 0)
        {
          printf( "query_switch() failed\n");
          exit(3);
        }
      rmsg = mon_api_msg_read(sock);
      if (rmsg == NULL)
        {
          printf( "query_switch() failed\n");
          exit(4);
        }
      /* display circuit info*/
    }  

  return 0;
}

