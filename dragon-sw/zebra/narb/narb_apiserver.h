/*narb_apiserver.h*/
#ifndef _NARB_APISERVER_H_
#define _NARB_APISERVER_H_

#define MTYPE_NARB_APISERVER MTYPE_TMP
#define MTYPE_ERO MTYPE_TMP

/*define the narb routing modes*/
enum narb_routing_mode {
  RT_MODE_ALL_STRICT_ONLY = 0x01,
  RT_MODE_MIXED_ALLOWED,
  RT_MODE_MIXED_PREFERRED,
  RT_MODE_ALL_LOOSE_ALLOWED
};
extern int flag_routing_mode;
extern int flag_holdon;
extern unsigned int NARB_API_SYNC_PORT;
extern list narb_apiserver_list;

/* data structure storing information for each instance of
    narb_apiserver */
struct narb_apiserver
{
  int apiserver_id;
  
  /* Socket connection for synchronous requests/replies*/
  int fd_sync;

  /* Storing app client address and port*/
  struct sockaddr_in peer_sync; 

  /* a list of type app_req_super_data */
  list app_req_list;

  /* Fifo buffer for outgoing messages */
  struct msg_fifo *out_sync_fifo;

  /* Read and write threads */
  struct thread *t_sync_read;
  struct thread *t_sync_write;

  /* Routing mode specified by Client. By default it inherits the value of the
  global variable flag_routing_mode. However, in some cases client may want 
  to change the default routing mode.*/
  int routing_mode;
};

/* internal data structure storing information for a app->narb routing request*/
struct app_req_super_data
{

   /* information extracted from the request message */
  u_int32_t app_seqnum;
  struct in_addr src;
  struct in_addr dest;
  float bandwidth;
  u_char encoding_type; /*refer to definitions in ospf_te.h*/
  u_char switching_type; /*refer to definitions in ospf_te.h*/
  
  /*state of the current request, values defined below*/
  u_char state;
#define APP_REQ_STATE_REQ 1
#define APP_REQ_STATE_ERO_VALID 2
#define APP_REQ_STATE_CONFIRMED 3

  /* state of ERO obtained from inter- and intra-doman OSPFd's
      the state values are defined below*/
  u_char ero_state;
#define ERO_NONE 0
#define ERO_INTER_ONLY 1
#define ERO_INTRA_ONLY 2
#define ERO_INTER_INTRA 3
#define ERO_ALL_STRICT 4
  
  /* inter- and intra-domain ERO's, two list of type ero_subobj
    generated from replies to CSPF requests*/
  list ero_inter;
  list ero_intra;

  /* pointing to the information for a recursive NARB */
  struct if_narb_info * rec_narb;
  int rec_narb_fd;
 
  /*the number of request retransmissions, [0, MAX_REQ_RETRAN] */
  int req_retran_counter;
#define MAX_REQ_RETRAN 3
};

/* data structure of APP->NARB request message */
struct msg_app2narb_request
{
  u_int16_t type;
  u_int16_t length;
  struct in_addr src;
  struct in_addr dest;
  u_int8_t  encoding_type;
  u_int8_t  switching_type;
  u_int16_t gpid;
  float bandwidth;
};

/* the internal data type defined for the node data of ERO list*/
struct ero_subobj
{
  struct in_addr addr;
  u_char hop_type;
  u_char prefix_len;
  u_char pad[2];
};

/* NARB event ID's, each corresponding to an event handler*/
enum narb_apiserver_event 
{
  NARB_APISERVER_ACCEPT = 1,
  NARB_APISERVER_SYNC_READ,
  NARB_APISERVER_SYNC_WRITE
};

/* definitions of NARB<->APP message types */
enum  narb_msg
{
  MSG_APP_REQUEST_EVENT = 0x02,
  MSG_APP_CONFIRM_EVENT = 0x03,
  MSG_APP_REMOVE_EVENT = 0x04,
  MSG_REPLY_ERO = 0x21,
  MSG_REPLY_ERROR = 0x22,
  MSG_REPLY_REMOVE_CONFIRM = 0x23,
  MSG_NARB_CSPF_REQUEST = 0xf1,
  MSG_NARB_CSPF_REPLY = 0xf2
};

/* each NARB<->APP contains a TLV in its message body*/
enum  narb_tlv_type
{
  TLV_TYPE_NARB_REQUEST = 0x02,
  TLV_TYPE_NARB_ERO = 0x03,
  TLV_TYPE_NARB_ERROR_CODE = 0x04
};

/* definitions of NARB error code as proccessing a request fails*/
enum  narb_error_code
{
  NARB_ERROR_NO_SRC = 1,
  NARB_ERROR_NO_DEST = 2,  
  NARB_ERROR_NO_ROUTE = 3,
  NARB_ERROR_INTERNAL = 4,
  NARB_ERROR_JUST_HOLDON = 6,
  NARB_ERROR_EXCEED_MAX_RETRAN = 7
};

/* data structure of an IPv4 prefix type ERO sub-object*/
struct ipv4_prefix_subobj
{
  u_char l_and_type;
  u_char length;
  u_char addr[4];
  u_char prefix_len;
  u_char resvd;
};
/* macro to combine loose hop indicator and type into an octet*/
#define L_AND_TYPE(L, T) ((((u_char)L)<<7) |((u_char)T))
/* definitions of loose/strict hop indicator*/
#define ERO_TYPE_STRICT_HOP 0
#define ERO_TYPE_LOOSE_HOP 1

/* macro to generate a NARB->OSPF sequence number manually*/
#define NARB_OSPF_SEQ_NUM(X, Y) ((X & 0xffff0000) | (Y & 0x0000ffff))

/* structure of NARB->OSPFd CSPF request message (msg body only)*/
struct msg_narb_cspf_request 
{
  u_int32_t narb_apiserv_id;
  u_int32_t app_seqnum;
  struct in_addr area_id;
  struct msg_app2narb_request app_req_data;
}; 

/* structure of OSPFd->NARB CSPF reply message (msg body only)*/
struct msg_narb_cspf_reply
{
  u_int32_t narb_apiserv_id;
  u_int32_t app_seqnum;
  struct in_addr src;
  struct in_addr dest;
  struct te_tlv_header tlv;
};

/* definitions of OSPFd->NARB error code*/
enum msg_narb_cspf_error_code
{
  CSPF_ERROR_UNKNOWN_SRC = 1,
  CSPF_ERROR_UNKNOWN_DEST,
  CSPF_ERROR_NO_ROUTE
};

/* Declarations of narb_apiserver functioms */

extern int 
narb_apiserver_init(void);

extern unsigned short
narb_apiserver_getport (void);

extern int
narb_apiserver_accept (struct thread *thread);

extern struct narb_apiserver *
narb_apiserver_new (int fd_sync);

extern int
narb_apiserver_read (struct thread *thread);

extern int
narb_apiserver_sync_write (struct thread *thread);

extern void
narb_apiserver_event (enum narb_apiserver_event event, int fd,
               struct narb_apiserver *narb_apiserv);

extern int
narb_apiserver_handle_msg (struct narb_apiserver *apiserv, struct msg *msg);

extern int
narb_apiserver_handle_app_request_event (struct narb_apiserver *apiserv,
               struct msg *msg);

extern int
narb_apiserver_handle_app_remove_event (struct narb_apiserver *apiserv,
               struct msg *msg);

extern int
narb_apiserver_handle_app_confirm_event (struct narb_apiserver *apiserv,
               struct msg *msg);

extern void *
narb_apiserver_construct_ero_tlv (int *length,  list ero);

extern int
narb_apiserver_send_cspf_request (struct ospf_apiclient * oc, int narb_apiserv_id,
                u_int32_t app_seqnum, void *data);

extern void
narb_apiserver_ero_reply (struct narb_apiserver *apiserv, u_int32_t app_seqnum, list ero);

extern void
narb_apiserver_error_reply (struct narb_apiserver *apiserv, u_int32_t app_seqnum, u_int32_t errcode);

extern struct msg *
narb_new_msg_reply_ero (u_int32_t seqnr, void * ero_tlv, int length);

extern struct msg *
narb_new_msg_reply_error (u_int32_t seqnr, u_int16_t errorcode);

extern struct msg *
narb_new_msg_reply_remove_confirm (u_int32_t seqnr);

extern int
narb_cspf_req_write (struct thread *t);

extern struct narb_apiserver *
narb_apiserver_list_lookup (u_int32_t id);

extern void 
ero_free(list ero);

extern void
narb_apiserver_free (struct narb_apiserver *apiserv);

extern void
narb_apiserver_free_all (void);

extern void
narb_apiserver_msg_print (struct msg *msg);

extern const char *
narb_apiserver_typename (int msgtype);

extern list
narb_merge_ero (list ero_inter, list ero_intra);

extern int 
narb_oclient_handle_cspf_reply_inter (struct msg_narb_cspf_reply * msg_reply);

extern int 
narb_oclient_handle_cspf_reply_intra (struct msg_narb_cspf_reply * msg_reply);

extern int
narb_apiserver_recursive_routing (struct narb_apiserver *apiserv, 
                struct app_req_super_data *superdata, list ero);

extern int
narb_apiserver_handle_recursive_routing_reply (struct narb_apiserver *apiserv,
                  struct msg *msg, list ero);
  
struct app_req_super_data * 
app_req_list_lookup (list app_req_list, u_int32_t seqnum);

extern struct in_addr
narb_lookup_domain_gateway (list ero_inter, list inter_links, list all_links);

extern struct link_info * 
narb_lookup_link_by_if_addr (list links, struct in_addr *lcl_if, struct in_addr *rmt_if);

extern struct in_addr *
narb_lookup_router_by_if_addr (struct in_addr *lcl_if, struct in_addr *rmt_if);

extern struct ospf_lsa * 
narb_lookup_lsa_by_if_addr (list lsdb, struct in_addr *lcl_if, struct in_addr *rmt_if);

extern struct link_info * 
narb_lookup_link_by_id (list links, struct in_addr *id);

#endif
