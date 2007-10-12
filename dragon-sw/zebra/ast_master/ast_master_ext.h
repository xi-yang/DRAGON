#ifndef _ZEBRA_AST_MASTER_EXT_H
#define _ZEBRA_AST_MASTER_EXT_H

#include "adt.h"
#include "buffer.h"
#include "vty.h"
#include "libxml/relaxng.h"
#include "ast_master_ext.h"

#define NODENAME_MAXLEN         30

enum resource_type {res_node=1, res_link};

enum action_type {
  action_unknown = 0,
  setup_req, setup_resp, ast_complete,
  app_complete, release_req, release_resp,
  query_req, query_resp, update_req, update_resp};

enum status_type {
  status_unknown = 0,
  ast_success, ast_failure, ast_pending,
  ast_ast_complete, ast_app_complete }; 

extern char* action_type_details[];

struct application_cfg {
  int xml_type;
  int clnt_sock;
  enum action_type org_action;
  enum action_type action;
  enum status_type status;
  u_int32_t flags;
  struct timeval start_time;
  
  /* counters */
  u_int8_t setup_ready;
  u_int8_t setup_sent;
  u_int8_t release_ready;
  u_int8_t complete_ready;
  u_int8_t total;
 
  char* ast_id;
  struct in_addr ast_ip;
  char xml_file[100];
  char details[200];
  struct adtlist *node_list;
  struct adtlist *link_list;
};

struct broker {
  struct in_addr ip;
  unsigned short port;
};

struct res_def {
  char *name;
  enum resource_type res_type;
  struct adtlist *broker_list;
  char *schema;  
  xmlRelaxNGPtr schema_ctxt;
  struct res_mod *mod; 
  struct in_addr agent_ip;
  unsigned short agent_port;
};
struct res_defs {
  struct adtlist node_list;
  struct adtlist link_list;
};

struct resource {
  enum resource_type res_type;  /* indicate this is link or node */
  struct res_def *subtype;	/* points to the subtype */
  struct in_addr ip;
  int minion_sock;
  char name[NODENAME_MAXLEN + 1];
  enum status_type status;
  u_int32_t flags;
  char *agent_message;

  /* specific info on the resource */
  void* res;
  xmlNodePtr  xml_node;
};

struct res_mod {
  const char* name;
  enum resource_type res_type;
  void *(*read_func)(struct application_cfg*, xmlNodePtr, int);
  int (*validate_func)(struct application_cfg*, struct resource*, int);
  int (*process_resp_func) (struct application_cfg *, struct resource *, struct resource *);
  int (*compose_req_func)(char*, struct application_cfg*, struct resource*);
  void (*print_func)(FILE*, void*, int);
  void (*print_cli_func)(struct vty*, void*);
  void (*free_func) (void*);
  int (*minion_proc_func)(enum action_type, struct resource*);
};
struct res_mods {
  int total;
  struct res_mod* res[10];
};

struct es_tank {
  int number;
  struct {
    struct in_addr ip;
    struct in_addr router_id;
    char* tunnel;
  } es[50];
};
struct es_tank es_pool;

#define AST_DIR		"/usr/local/ast"
#define NODE_AGENT_DIR	"/usr/local/node_agent"
#define LINK_AGENT_DIR	"/usr/local/link_agent"

#define MASTER_PORT			2619
#define DEFAULT_NODE_XML_PORT		2623
#define DEFAULT_LINK_XML_PORT		2618
#ifdef RESOURCE_BROKER
#define DEFAULT_NODE_BROKER_PORT        2624
#endif

/* types of agents in AST suite */
#define NODE_AGENT		1
#define LINK_AGENT		2
#define MASTER			3
#define ASTB			4
#define MINION			5

/* types of XML_FILES */
#define TOPO_XML		1
#define ID_XML			2
#define ID_QUERY_XML		3
#define CTRL_XML		4

/* buffer size */
#define RCVBUFSIZE              100
#define SENDBUFSIZE     	5000
#define TIMEOUT_SECS		3

#define IP_MAXLEN 		15 
#define LSP_NAME_LEN            13 

struct string_syntex_check {
   int number;
   struct {
	char* abbre;
	char* details;
   } ss[50];
};


#define REG_TXT_FIELD_LEN	20

#define FLAG_SETUP_REQ		0x0001
#define FLAG_SETUP_RESP		0x0002
#define FLAG_AST_COMPLETE	0x0004
#define FLAG_APP_COMPLETE	0x0008
#define FLAG_RELEASE_REQ	0x0010
#define FLAG_RELEASE_RESP	0x0020
#define FLAG_QUERY_REQ		0x0040
#define FLAG_QUERY_RESP		0x0080
#define FLAG_UNFIXED		0x1000

#define IS_SET_SETUP_REQ(X)	((X->flags) & FLAG_SETUP_REQ)
#define IS_SET_SETUP_RESP(X)	((X->flags) & FLAG_SETUP_RESP)
#define IS_SET_AST_COMPLETE(X)	((X->flags) & FLAG_AST_COMPLETE)
#define IS_SET_APP_COMPLETE(X)	((X->flags) & FLAG_APP_COMPLETE)
#define IS_SET_RELEASE_REQ(X)	((X->flags) & FLAG_RELEASE_REQ)
#define IS_SET_RELEASE_RESP(X)	((X->flags) & FLAG_RELEASE_RESP)
#define IS_RES_UNFIXED(X)	((X->flags) & FLAG_UNFIXED)
#define IS_RES_ACTION(X, Y)	((X->flags) & Y)

#define IS_VTAG_ANY(X) \
	(X->stype == uni && X->src->local_id_type[0] == '\0' && \
	X->dest->local_id_type[0] == '\0' && \
	X->vtag[0] == '\0')

struct application_cfg *glob_app_cfg;
struct adtlist app_list;

#define XML_SERVICE_DEF_FILE    "/usr/local/ast_file/service_template.xml"
#define XML_ETHERBASIC_FILE "/usr/local/ast_file/service_template/EtherPipeBasic.xml"
#define XML_ETHERULTRA_FILE "/usr/local/ast_file/service_template/EtherPipeUltra.xml"
#define XML_TDMBASIC_FILE "/usr/local/ast_file/service_template/TDMBasic.xml"
#define XML_NEW_FILE "/tmp/app_ready.xml"
#define XML_DRAGON_RETURN_FILE "/tmp/dragon_resp.xml"

#define AST_XML_RESULT  "/usr/local/ast_master_ret.xml"

#define FULL_VERSION	1
#define BRIEF_VERSION	2

/* Functions related to overall topology configuraiton processing */
struct application_cfg* topo_xml_parser(char*, int);
struct application_cfg* retrieve_app_cfg(char*, int);
int topo_validate_graph(int, struct application_cfg*);
int xml_parser(char*);
int init_resource();

/* Functions for communicating to minions */

/* Functions related to resource processing with the topology configuraiton */
void print_xml_response(char*, int);
void free_res(struct resource*);
void free_res_list(struct adtlist*);
void free_application_cfg(struct application_cfg*);
void print_res(FILE*, struct resource*, int);
void print_res_list(FILE *fp, struct adtlist *res_list, int agent);

/* overall printing functions */
void print_final(char*, int);
void print_error_response(char*);

/* xml file transfer */
int send_file_over_sock(int, char*);
int send_file_to_agent(char*, int, char*);
int recv_file(int, char*, int, int, int);

/* overall searching functions */
struct resource * search_res_by_name(struct application_cfg*, enum resource_type, char*);
struct resource * search_link_by_name(struct application_cfg*, char*);
struct resource * search_node_by_name(struct application_cfg*, char*);

/* xml parsing functions */
xmlRelaxNGPtr build_RelaxNGPtr(char*);
xmlRelaxNGValidCtxtPtr build_RNGValidCtxt(char*);
xmlNodePtr findxmlnode(xmlNodePtr, char*);

/* misc functions */
xmlRelaxNGValidCtxtPtr init_schema();
struct application_cfg* agent_final_parser(char*);
void add_cfg_to_list();
void del_cfg_from_list(struct application_cfg*);
struct application_cfg* search_cfg_in_list(char*);
void set_allres_fail(char*);
void app_cfg_pre_req();

/* functions for minion */
int minion_process_xml(struct in_addr);

#define ID_SETUP	1
#define ID_TEMP		2
#define ID_QUERY	3
char* generate_ast_id(int);

#endif /* _ZEBRA_AST_MASTER_EXT_H */
