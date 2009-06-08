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
char* mon_host = "localhost";
extern int MON_APISERVER_PORT;

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

#define query_switch_lsplist(FD) mon_apiclient_send_query(FD, MON_API_MSGTYPE_LSPLIST, NULL)
#define query_lsp_info(FD, GRI) mon_apiclient_send_query(FD, MON_API_MSGTYPE_LSPINFO, GRI)
#define query_lsp_ero(FD, GRI) mon_apiclient_send_query(FD, MON_API_MSGTYPE_LSPERO, GRI)
#define query_lsp_nodelist(FD, GRI) mon_apiclient_send_query(FD, MON_API_MSGTYPE_NODELIST, GRI)
#define query_switch_info(FD) mon_apiclient_send_query(FD, MON_API_MSGTYPE_SWITCH, NULL)


int query_circuit_info (int fd, char* gri, struct in_addr dest)
{
  char body[4+MAX_MON_NAME_LEN+8];
  int rc = 0;
  u_int16_t bodylen = 0;
  struct mon_api_msg* msg;
  struct dragon_tlv_header* tlv;
  
  assert(fd > 0);

  if (!gri || dest.s_addr == 0)
    return -1;

  tlv = (struct dragon_tlv_header*)body;
  tlv->type = htons(MON_TLV_GRI);
  tlv->length = htons(MAX_MON_NAME_LEN);
  bodylen += sizeof(struct dragon_tlv_header);
  strncpy(body+bodylen, gri, MAX_MON_NAME_LEN+1);
  bodylen += MAX_MON_NAME_LEN;
  tlv = (struct dragon_tlv_header*)(body+bodylen);
  tlv->type = htons(MON_TLV_IPv4_ADDR);
  tlv->length = htons(sizeof(struct in_addr));
  bodylen += sizeof(struct dragon_tlv_header);
  memcpy(body+bodylen, &dest, sizeof(struct in_addr));
  bodylen += sizeof(struct in_addr);  
  msg = mon_api_msg_new(MON_API_MSGTYPE_CIRCUIT, MON_API_ACTION_RTRV, bodylen, get_ucid(), get_seqence_number(), 0, body);
  assert(msg);

  mon_api_msg_write(fd, msg);
  return rc;
}

int setup_lsp(int fd, char* gri, struct in_addr src_ip, u_int32_t src_lid, struct in_addr dest_ip, u_int32_t dest_lid, float bandwidth)
{
  char body[512];
  int rc = 0;
  u_int16_t bodylen = 0;
  struct mon_api_msg* msg;
  struct _LSPService_Request* lsp_req;
  struct dragon_tlv_header* tlv;
  
  assert(fd > 0);

  if (!gri || src_ip.s_addr == 0 || dest_ip.s_addr == 0 || src_lid == 0 || dest_lid == 0)
    return -1;

  tlv = (struct dragon_tlv_header*)body;
  tlv->type = htons(MON_TLV_GRI);
  tlv->length = htons(MAX_MON_NAME_LEN);
  bodylen += sizeof(struct dragon_tlv_header);
  strncpy(body+bodylen, gri, MAX_MON_NAME_LEN+1);
  bodylen += MAX_MON_NAME_LEN;

  tlv = (struct dragon_tlv_header*)(body+bodylen);
  tlv->type = htons(MON_TLV_LSP_REQUEST);
  tlv->length = htons(sizeof(struct _LSPService_Request));
  bodylen += sizeof(struct dragon_tlv_header);
  lsp_req = (struct _LSPService_Request*)(tlv+1);
  memset(lsp_req, 0, sizeof(struct _LSPService_Request));
  lsp_req->source.s_addr = src_ip.s_addr;
  lsp_req->src_lclid = src_lid;
  lsp_req->destination.s_addr = dest_ip.s_addr;
  lsp_req->dest_lclid = dest_lid;
  lsp_req->bandwidth = bandwidth*1000000/8;
  /*default testing values*/
  lsp_req->switching_type = 51;
  lsp_req->encoding_type = 2;
  lsp_req->gpid = 33;
  lsp_req->vlan_tag = ANY_VTAG;
  lsp_req->subnet_vtag_ingress = lsp_req->subnet_vtag_egress = 5000;
  bodylen += sizeof(struct _LSPService_Request);

  /*Optional ERO and SubnetERO TLVs for testing only*/
  /*TBD*/
  
  msg = mon_api_msg_new(MON_API_MSGTYPE_LSPPROV, MON_API_ACTION_INSERT, bodylen, get_ucid(), get_seqence_number(), 0, body);
  assert(msg);
  mon_api_msg_write(fd, msg);
  return rc;
}

int teardown_lsp(int fd, char* gri)
{
  char body[4+MAX_MON_NAME_LEN+8];
  int rc = 0;
  u_int16_t bodylen = 0;
  struct mon_api_msg* msg;
  struct dragon_tlv_header* tlv;

  if (!gri )
    return -1;

  tlv = (struct dragon_tlv_header*)body;
  tlv->type = htons(MON_TLV_GRI);
  tlv->length = htons(MAX_MON_NAME_LEN);
  bodylen += sizeof(struct dragon_tlv_header);
  strncpy(body+bodylen, gri, MAX_MON_NAME_LEN+1);
  bodylen += MAX_MON_NAME_LEN;

  msg = mon_api_msg_new(MON_API_MSGTYPE_LSPPROV, MON_API_ACTION_DELETE, bodylen, get_ucid(), get_seqence_number(), 0, body);
  assert(msg);
  mon_api_msg_write(fd, msg);
  return rc;
}

static char* ciena_logical_port2string(u_int32_t logical_port)
{
    static char buf[20];
    char shelf_alpha;
    int bay, shelf, slot, subslot, port;

    bay = (logical_port >> 24) + 1;
    shelf = ((logical_port >> 16)&0xff);
    slot = ((logical_port >> 12)&0x0f) + 1;
    subslot = ((logical_port >> 8)&0x0f) + 1;
    port = (logical_port&0xff) + 1;
    switch (shelf)
    {
    case 2:
        shelf_alpha = 'A';
        break;
    case 3:
        shelf_alpha = 'C';
        break;
    default:
        shelf_alpha = 'X';
        break;
    }
    sprintf(buf, "%d-%c-%d-%d-%d", bay, shelf_alpha, slot, subslot, port);
    return buf;
}
void display_subnet_circuit_info(struct _Subnet_Circuit_Info* circuit_info)
{
  printf("\t\t>>Subnet Edge --> ID: %d", circuit_info->subnet_id);
  printf(", Port: %s", ciena_logical_port2string(circuit_info->port));
  printf(", Ethernet Bandwidth: %.3f", circuit_info->ethernet_bw);
  printf(", First Timeslot: %x", circuit_info->first_timeslot);
  printf(", VCG: %s", circuit_info->vcg_name);
  printf(", EFLOW(s): %s -- %s", circuit_info->eflow_in_name,  circuit_info->eflow_out_name);
  if (strlen(circuit_info->snc_crs_name) > 0)
    printf(", SNC/CRS: %s", circuit_info->snc_crs_name);
  if (strlen(circuit_info->dtl_name) > 0)
    printf(", DTL: %s", circuit_info->dtl_name);
  printf("\n");
}

void display_ero_para(struct _EROAbstractNode_Para* hop)
{
  printf("\t\t   >>>ERO subobject (%s): ", hop->isLoose == 1 ? "loose":"strict");
  switch (hop->type)
    {
    case IPv4:
      printf("IPv4 -- %s\n", inet_ntoa(hop->data.ip4.addr));
      break;
    case UNumIfID:
      printf("UnNumbered -- rotuerID %s, interfaceID 0x%x\n", inet_ntoa(hop->data.uNumIfID.routerID), hop->data.uNumIfID.interfaceID);
      break;
    default:
      printf("Unsupported type %d\n", hop->type);
    }
}

void msg_display(struct mon_api_msg* msg)
{
  char* gri;
  struct _Switch_Generic_Info* switch_info;
  struct _Ethernet_Circuit_Info* circuit_info_ethernet;
  struct _Subnet_Circuit_Info* circuit_info_subnet;
  struct _EROAbstractNode_Para* ero_para;
  struct _MON_LSP_Info* lsp_info;
  struct dragon_tlv_header* tlv;
  struct in_addr *addr;
  u_int32_t errcode;
  int bodylen, tlvlen, i;

  printf("\n  Response from monitroing API server:\n");
  printf("\t> Message type ");
  switch (msg->header.type)
    {
    case MON_API_MSGTYPE_SWITCH:
      printf("MON_API_MSGTYPE_SWITCH\n");
      break;
    case MON_API_MSGTYPE_CIRCUIT:
      printf("MON_API_MSGTYPE_CIRCUIT\n");
      if (ntohl(msg->header.options) & MON_SWITCH_OPTION_SUBNET)
          printf("\t\t>>EoS Subnet Connection - ");
      else
          printf("\t\t>>Ethernet VLAN - ");
      if (ntohl(msg->header.options) & MON_SWITCH_OPTION_SUBNET_TRANSIT)
          printf(" Transit Node");
      if (ntohl(msg->header.options) & MON_SWITCH_OPTION_SUBNET_SRC)
          printf(" w/ Source Edge");
      if (ntohl(msg->header.options) & MON_SWITCH_OPTION_SUBNET_DEST)
          printf(" w/ Destination Edge");
      printf("\n");
      break;
    case MON_API_MSGTYPE_LSPLIST: 
      printf("MON_API_MSGTYPE_LSPLIST\n");
      break;
    case MON_API_MSGTYPE_LSPERO: 
      printf("MON_API_MSGTYPE_LSPERO\n");
      break;
    case MON_API_MSGTYPE_LSPINFO: 
      printf("MON_API_MSGTYPE_LSPINFO\n");
      break;
    case MON_API_MSGTYPE_NODELIST:
      printf("MON_API_MSGTYPE_NODELIST\n");
      break;
    case MON_API_MSGTYPE_LSPPROV:
      printf("MON_API_MSGTYPE_LSPPROV\n");
      break;
    case MON_API_MSGTYPE_GENERAL:
      printf("MON_API_MSGTYPE_GENERAL\n");
      break;
    default:
      printf("UNKNOWN (%d)\n", msg->header.type);
    }

  printf("\t> Action ");
  switch (msg->header.action)
    {
    case MON_API_ACTION_DATA:
      printf("MON_API_ACTION_DATA\n");
      break;
    case MON_API_ACTION_ACK:
      printf("MON_API_ACTION_ACK\n");
      break;
    case MON_API_ACTION_ERROR:
      printf("MON_API_ACTION_ERROR\n");
      break;
    default:
      printf("UNKNOWN (%d)\n", msg->header.action);
    }
  
  bodylen = ntohs(msg->header.length);
  printf("\t> Body length %d\n", bodylen);
  tlv = (struct dragon_tlv_header*)msg->body;
  while (bodylen > 0)
    {
      switch (ntohs(tlv->type))
        {
        case MON_TLV_GRI:
          gri = (char*)(tlv + 1);
          printf("\t\t>>LSP: %s\n", gri);
          break;
        case MON_TLV_SWITCH_INFO:
            switch_info = (struct _Switch_Generic_Info*)(tlv+1);
            printf("\t\t>>Switch Info --> IP Address:");
            if (switch_info->switch_ip[0].s_addr != 0)
                printf(" Source %s", inet_ntoa(switch_info->switch_ip[0]));
            if (switch_info->switch_ip[1].s_addr != 0)
                printf(" Destination %s", inet_ntoa(switch_info->switch_ip[1]));
            printf(", Port: %d", switch_info->switch_port);
            printf(", Type: %d", switch_info->switch_type);
            printf(", Acess Type: %d\n ", switch_info->access_type);
          break;
        case MON_TLV_CIRCUIT_INFO:
            if (ntohs(tlv->length) == sizeof(struct _Ethernet_Circuit_Info))
              {
                circuit_info_ethernet = (struct _Ethernet_Circuit_Info*)(tlv+1);
                printf("\t\t>>VLAN Ingress --> VLAN: %d, Ports: ", circuit_info_ethernet->vlan_ingress);
                for (i = 0; i < circuit_info_ethernet->num_ports_ingress; i++)
                  {
                    printf(" %d", circuit_info_ethernet->ports_ingress[i]);
                  }
                printf("\n");
                printf("\t\t>>VLAN Egress --> VLAN: %d, Ports: ", circuit_info_ethernet->vlan_egress);
                for (i = 0; i < circuit_info_ethernet->num_ports_egress; i++)
                  {
                    printf(" %d", circuit_info_ethernet->ports_egress[i]);
                  }
                printf("\n");
              }
            else if (ntohs(tlv->length) % sizeof(struct _Subnet_Circuit_Info) == 0)
              {
                circuit_info_subnet = (struct _Subnet_Circuit_Info*)(tlv+1);
                display_subnet_circuit_info(circuit_info_subnet);                
                if (ntohs(tlv->length) == sizeof(struct _Subnet_Circuit_Info)*2)
                  {
                    circuit_info_subnet++;
                    display_subnet_circuit_info(circuit_info_subnet);
                  }
              }
            else 
              {
                printf("\t\t>>UNKOWN TLV_CIRCUIT_INFO content (length=%d)\n", ntohs(tlv->length));
              }
            
          break;
        case MON_TLV_NODE_LIST:
          printf("\t\t>>NodeList: ");
          for (i = 0, addr = (struct in_addr*)(tlv + 1); i < ntohs(tlv->length)/4; i++, addr++)
            {
              printf("-%s-", inet_ntoa(*addr));
            }
          printf("\n");
          break;
        case MON_TLV_LSP_ERO:
          printf("\t\t>>Explicit Route (Regular):\n");
          for (i = 0, ero_para = (struct _EROAbstractNode_Para*)(tlv + 1); i < ntohs(tlv->length)/sizeof(struct _EROAbstractNode_Para); i++, ero_para++)
            {
              display_ero_para(ero_para);
            }
          printf("\n");
          break;
        case MON_TLV_SUBNET_ERO:
          printf("\t\t>>Explicit Route (Subnet):\n");
          for (i = 0, ero_para = (struct _EROAbstractNode_Para*)(tlv + 1); i < ntohs(tlv->length)/sizeof(struct _EROAbstractNode_Para); i++, ero_para++)
            {
              display_ero_para(ero_para);
            }
          printf("\n");
          break;
        case MON_TLV_LSP_INFO:
        {
          char src_ip[20], dest_ip[20];
          lsp_info = (struct _MON_LSP_Info*)(tlv + 1);
          strcpy(src_ip, inet_ntoa(lsp_info->source)); 
          strcpy(dest_ip, inet_ntoa(lsp_info->destination)); 
          printf("\t\t>>LSP Info: src_ip=%s, lsp_id=%d, dest_ip=%s, tunnel_id=%d, status=0x%x -- created at %s\n", 
		  	src_ip,  lsp_info->lsp_id, dest_ip, lsp_info->tunnel_id, lsp_info->status, ctime((time_t*)&lsp_info->time_sec));
          if (lsp_info->status == LSP_ERROR)
          {
              struct _Error_Spec_Para *errspec = (struct _Error_Spec_Para *)((char*)tlv + sizeof(struct dragon_tlv_header) + ntohs(tlv->length));
              strcpy(src_ip, inet_ntoa(errspec->nodeAddress)); 
              u_int32_t xef = errspec->errFlags, xec = errspec->errCode, xev = errspec->errValue;
              printf("\t\t\t: Error Spec: err_node=%s, err_flag=0x%x, err_code=0x%x, err_value=0x%x\n", src_ip, xef, xec, xev);
          }
          break;
        }
        case MON_TLV_ERROR:
          errcode = *(u_int32_t*)(tlv + 1);
          printf("\t\t>>Error Code: 0x%x\n", errcode);
          break;
        default:
          printf("UNKNOWN TLV type %d\n", ntohs(tlv->type));          
        }
      tlvlen = sizeof(struct dragon_tlv_header) + ntohs(tlv->length);
      tlv = (struct dragon_tlv_header*)((char*)tlv + tlvlen);
      bodylen -= tlvlen;
    }
}

/* DRAGON MON_APIClient options. */
struct option longopts[] = 
{
  { "circuit",     required_argument,       NULL, 'c'},
  { "lspero",     required_argument,       NULL, 'e'},
  { "help",        no_argument,       NULL, 'h'},
  { "lsplist",     no_argument,       NULL, 'l'},
  { "nodelist",     required_argument,       NULL, 'n'},
  { "switch",     no_argument,       NULL, 's'},
  { "lspstatus",     required_argument,       NULL, 't'},
  { "version",     no_argument,       NULL, 'v'},
  { "host",     required_argument,       NULL, 'H'},
  { "port",     required_argument,       NULL, 'P'},
  { "setup",     required_argument,       NULL, 'S'},
  { "teardown",     required_argument,       NULL, 'T'},
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
-c, --circuit <gri,dest_ip>     Circuit information\n\
-e, --lspero <gri>     LSP ERO information\n\
-h, --help         Display this help and exit\n\
-l, --lsplist      Switch information\n\
-n, --nodelist      LSP node list\n\
-s, --switch      Switch information\n\    
-i, --lspinfo <gri>     LSP status\n\
-v, --version    Print program version\n\
-H, --host <name>     Host name\n\
-P, --port <number>  Port number\n\
-S, --setup <gri,src_ip:lid-type/value,dest_ip:lid-type/value,bw>  LSP setup\n\
-T, --teardown <gri>  LSP teardown\n\
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
  char buf[200];
  int sock;
  int ret = 0;
  int is_query_switch = 0;
  int is_query_circuit = 0;
  int is_query_lsplist = 0;
  int is_query_lspinfo = 0;
  int is_query_lspero = 0;
  int is_query_nodelist = 0;
  int is_lsp_setup = 0;
  int is_lsp_teardown = 0;
  char* gri = NULL;
  char* src = NULL;
  char* dest = NULL;
  struct in_addr src_ip;
  u_int32_t src_lid_type = 0;
  u_int32_t src_lid_value = 0;
  struct in_addr dest_ip;
  u_int32_t dest_lid_type = 0;
  u_int32_t dest_lid_value = 0;
  float bandwidth = 0;
  struct mon_api_msg* rmsg = NULL;
  src_ip.s_addr = dest_ip.s_addr = 0;

  /* get program name */
  progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);

  while (1) 
    {
      int opt;

      opt = getopt_long (argc, argv, "c:e:hln:si:vH:P:S:T:", longopts, 0);
    
      if (opt == EOF)
        break;

      switch (opt) 
        {
        case 0:
          break;
        case 'c':
          is_query_circuit = 1;
          gri = optarg;
          p = strstr(optarg, ",");
          if (!p)
            {
              printf ("# Wrong arguments: -c takes two arguments <GRI,Dest_IPv4>. Note they are separated by comma.\n");
            }
          *p = 0; p++;
          inet_aton(p, &dest_ip);
          break;
        case 'e':
          is_query_lspero = 1;
          gri = optarg;
          break;
        case 'h':
          usage (progname, 0);
          break;
        case 'l':
          is_query_lsplist = 1;
          break;
        case 'n':
          is_query_nodelist = 1;
          gri = optarg;
          break;
        case 's':
          is_query_switch = 1;
          break;
        case 'i':
          is_query_lspinfo = 1;
          gri = optarg;
          break;
	 case 'v':
          printf ("%s version %s\n", progname, DRAGON_VERSION);
          printf ("Copyright 2004-2008, the NSF DRAGON Project\n");
          printf ("Extended from GNU Zebra -- Copyright 1996-2001\n");
          exit (0);
          break;
        case 'H':
          mon_host = optarg;
          break;
        case 'P':
          sscanf(optarg, "%d", &MON_APISERVER_PORT);
          break;
        case 'S':
          gri = buf; src = buf+100; dest = buf +150;
          p = optarg;
          while (*p) { if (*p == ','||*p == ':') *p = ' ';  p++; }
          if (sscanf(optarg,"%s %s %d/%d %s %d/%d %f", gri, src, &src_lid_type, &src_lid_value, dest, &dest_lid_type, &dest_lid_value, &bandwidth) != 8)
            {
              printf ("# Wrong parameters for  -S (--setup) option\n");
              usage(progname, 1);
              exit(1);
            }
          if (!inet_aton(src, &src_ip))
            {
              printf ("# Invalid source IP %s\n", src);
              exit(1);
            }
          if (!inet_aton(dest, &dest_ip))
            {
              printf ("# Invalid source IP %s\n", src);
              exit(1);
            }
          is_lsp_setup = 1;
          break;
        case 'T':
          gri = optarg;
          is_lsp_teardown = 1;
          break;
        default:
          usage (progname, 1);
          exit(2);
        }
    }

  sock =  mon_apiclient_connect(mon_host, mon_apiserver_getport());
  if (sock < 0)
    {
      printf( "mon_apiclient_connect() failed\n");
      exit(2);
    }
  if (is_query_circuit && gri)
    {
      ret = query_circuit_info(sock, gri, dest_ip);
      if (ret != 0)
        {
          printf( "query_circuit() failed\n");
          exit(3);
        }
      rmsg = mon_api_msg_read(sock);
      if (rmsg == NULL)
        {
          printf( "query_circuit_info() failed\n");
          exit(4);
        }
    }
  else if (is_query_switch)
    {
      ret = query_switch_info(sock);
      if (ret != 0)
        {
          printf( "query_switch_info() failed\n");
          exit(3);
        }
      rmsg = mon_api_msg_read(sock);
      if (rmsg == NULL)
        {
          printf( "query_switch_info() failed\n");
          exit(4);
        }
    }  
  else if (is_query_lsplist)
    {
      ret = query_switch_lsplist(sock);
      if (ret != 0)
        {
          printf( "query_switch_lsplist() failed\n");
          exit(3);
        }
      rmsg = mon_api_msg_read(sock);
      if (rmsg == NULL)
        {
          printf( "query_switch_lsplist() failed\n");
          exit(4);
        }
    }
  else if (is_query_nodelist)
    {
      ret = query_lsp_nodelist(sock, gri);
      if (ret != 0)
        {
          printf( "query_lsp_nodelist() failed\n");
          exit(3);
        }
      rmsg = mon_api_msg_read(sock);
      if (rmsg == NULL)
        {
          printf( "query_lsp_nodelist() failed\n");
          exit(4);
        }
    }
  else if (is_query_lspero)
    {
      ret = query_lsp_ero(sock, gri);
      if (ret != 0)
        {
          printf( "query_lsp_ero() failed\n");
          exit(3);
        }
      rmsg = mon_api_msg_read(sock);
      if (rmsg == NULL)
        {
          printf( "query_lsp_ero() failed\n");
          exit(4);
        }
    }
  else if (is_query_lspinfo)
    {
      ret = query_lsp_info(sock, gri);
      if (ret != 0)
        {
          printf( "query_lsp_info() failed\n");
          exit(3);
        }
      rmsg = mon_api_msg_read(sock);
      if (rmsg == NULL)
        {
          printf( "query_lsp_info() failed\n");
          exit(4);
        }
    }
  else if (is_lsp_setup)
    {
      ret = setup_lsp(sock, gri, src_ip, (src_lid_type << 16)|(src_lid_value&0xffff), dest_ip, (dest_lid_type << 16)|(dest_lid_value&0xffff), bandwidth);
      if (ret != 0)
        {
          printf( "setup_lsp() failed\n");
          exit(3);
        }
      rmsg = mon_api_msg_read(sock);
      if (rmsg == NULL)
        {
          printf( "setup_lsp() failed\n");
          exit(4);
        }        
    }
  else if (is_lsp_teardown)
    {
      ret = teardown_lsp(sock, gri);
      if (ret != 0)
        {
          printf( "teardown_lsp() failed\n");
          exit(3);
        }
      rmsg = mon_api_msg_read(sock);
      if (rmsg == NULL)
        {
          printf( "teardown_lsp() failed\n");
          exit(4);
        }
 }

  msg_display(rmsg);

  return 0;
}


/************* Compile without libRSVP *************/

void* zInitRsvpApiInstance() {return NULL;}
void zInitRsvpPathRequest(void* api, struct _sessionParameters* para, u_int8_t isSender) {}
void zTearRsvpPathRequest(void *api, struct _sessionParameters* para) {}
void zTearRsvpResvRequest(void* api, struct _sessionParameters* para) {}
int zGetApiFileDesc(void *api) {return 0;}
void zApiReceiveAndProcess(void *api, zUpcall upcall) {}
void zInitRsvpResvRequest(void* api, struct _rsvp_upcall_parameter* upcallPara) {}
void zAddLocalId(void* api, u_int16_t type, u_int16_t value, u_int16_t tag) {}
void zDeleteLocalId(void* api, u_int16_t type, u_int16_t value, u_int16_t tag) {}
void zRefreshLocalId(void* api, u_int16_t type, u_int16_t value, u_int16_t tag) {}
void zMonitoringQuery(void* api, u_int32_t ucid, u_int32_t seqnum, char* gri, u_int32_t destAddrIp, u_int16_t tunnelId, u_int32_t extTunnelId) {}

/************* Compile without libRSVP *************/

