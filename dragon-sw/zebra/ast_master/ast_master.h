#ifndef _ZEBRA_AST_MASTER_H
#define _ZEBRA_AST_MASTER_H

#include "adt.h"
#include "buffer.h"

#define AST_DIR		"/usr/local/ast"
#define NODE_AGENT_DIR	"/usr/local/node_agent"
#define LINK_AGENT_DIR	"/usr/local/link_agent"

#define MASTER_PORT		2619
#define DRAGON_XML_PORT         2618
#ifdef RESOURCE_BROKER
#define NODE_BROKER_PORT        2624
#endif

/* types of agents in AST suite */
#define NODE_AGENT		1
#define LINK_AGENT		2
#define MASTER			3
#define ASTB			4

/* types of Resource in AST */
#define	NODE_RES		1
#define	LINK_RES		2

/* types of XML_FILES */
#define TOPO_XML		1
#define ID_XML			2
#define CTRL_XML		3

/* buffer size */
#define RCVBUFSIZE              100
#define SENDBUFSIZE     	5000
#define TIMEOUT_SECS		3

#define NODENAME_MAXLEN		30
#define IP_MAXLEN 		15 
#define NUM_LINK_TYPE		3
#define NUM_NODE_TYPE		3
#define NUM_LINK_STYPE		4	
#define NUM_FUNCTION_TYPE	8
#define NUM_STATUS_TYPE		5
#define NUM_NODE_STYPE		3
#define LSP_NAME_LEN            13 
#define NODE_AGENT_PORT 	2623

#define AST_UNKNOWN		0 
#define AST_SUCCESS		1
#define AST_FAILURE		2
#define AST_PENDING		3
#define AST_AST_COMPLETE	4
#define AST_APP_COMPLETE	5
 
/* defines for application_cfg.function */
#define SETUP_REQ		1
#define SETUP_RESP		2
#define AST_COMPLETE		3
#define RELEASE_REQ		4
#define RELEASE_RESP		5
#define APP_COMPLETE		6
#define QUERY_REQ		7
#define QUERY_RESP		8
#define INVALID_INCOMING_VALUE	9

struct string_syntex_check {
   int number;
   struct {
	char* abbre;
	char* details;
   } ss[50];
};

struct es_tank {
  int number;
  struct {
    char* ip;
    char* router_id; 
    char* tunnel;
  } es[20];
};
struct es_tank es_pool;

#define REG_TXT_FIELD_LEN	20

/* structures defined for the xml files are:
 * 1. struct application_cfg
 * 2. struct node_cfg
 * 3. struct link_cfg
 */
struct application_cfg {
  int xml_type;
  int clnt_sock;
  int org_action;
  int action;
  int status;
  u_int32_t flags;
  struct timeval start_time;
  
  /* counters */
  u_int8_t setup_ready;
  u_int8_t setup_sent;
  u_int8_t release_ready;
  u_int8_t complete_ready;
 
  char* ast_id;
  char* ast_ip;
  char details[200];
  struct adtlist *node_list;
  struct adtlist *link_list;
};

enum node_stype { PC = 1, correlator, computation_array };
enum link_stype { uni = 1, non_uni, vlsr_vlsr, vlsr_es };

struct if_ip {
  char *iface;
  char *assign_ip;
  int vtag;
}; 

struct node_cfg {
  enum node_stype stype;
  char ip[IP_MAXLEN+1];
  char router_id[IP_MAXLEN+1];
  char tunnel[10];
  char *command;
  struct adtlist *if_list;
  struct adtlist *link_list;
};

struct endpoint {
  struct resource *es;
  struct resource *vlsr;
  struct resource *proxy;
  struct if_ip *ifp;
  char local_id_type[REG_TXT_FIELD_LEN+1];
  int local_id;
};

struct link_cfg {
  enum link_stype stype;
  int service_type;
  char lsp_name[LSP_NAME_LEN+1];
  struct endpoint *src;
  struct endpoint *dest;
  struct resource *dragon;

  /* te_params */
  char vtag[REG_TXT_FIELD_LEN+1];
  char bandwidth[REG_TXT_FIELD_LEN+1];
  char swcap[REG_TXT_FIELD_LEN+1];
  char encoding[REG_TXT_FIELD_LEN+1];
  char gpid[REG_TXT_FIELD_LEN+1];
};

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

#define IS_VTAG_ANY(X) \
	(X->res.l.stype == uni && X->res.l.src->local_id_type[0] == '\0' && \
	X->res.l.dest->local_id_type[0] == '\0' && \
	X->res.l.vtag[0] == '\0')

struct resource {
  int type;			/* indicate this is link or node */
  int noded_sock;		/* sock to connect to the minions */
  int dragon_sock;
  char name[NODENAME_MAXLEN + 1];
  int status;
  u_int32_t flags;
  char *agent_message;

  union {
    struct node_cfg n;
    struct link_cfg l;
  } res; 
};
struct application_cfg *glob_app_cfg;
struct adtlist app_list;

/* structures defined for link
 */
struct network_link {
  char service_name[REG_TXT_FIELD_LEN+1];
  char bandwidth[REG_TXT_FIELD_LEN+1];
  char swcap[REG_TXT_FIELD_LEN+1];
  char encoding[REG_TXT_FIELD_LEN+1];
  char gpid[REG_TXT_FIELD_LEN+1];
};

struct linkprofile {
  int count;
  struct network_link **elem;
} service;

enum link_type { EtherPipeBasic = 1, EtherPipeUltra, TDMBasic };

#define XML_SERVICE_DEF_FILE    "/usr/local/ast_file/service_template.xml"
#define XML_ETHERBASIC_FILE "/usr/local/ast_file/service_template/EtherPipeBasic.xml"
#define XML_ETHERULTRA_FILE "/usr/local/ast_file/service_template/EtherPipeUltra.xml"
#define XML_TDMBASIC_FILE "/usr/local/ast_file/service_template/TDMBasic.xml"
#define XML_NEW_FILE "/tmp/app_ready.xml"
#define XML_DRAGON_RETURN_FILE "/tmp/dragon_resp.xml"

#define AST_XML_RESULT  "/usr/local/ast_master_ret.xml"

int service_xml_parser(char*);
int master_locate_resource();
int master_validate_graph(int);
int master_send_task();

#define FULL_VERSION	1
#define BRIEF_VERSION	2

struct application_cfg* topo_xml_parser(char*, int);
struct application_cfg* retrieve_app_cfg(char*, int);
int topo_validate_graph(int, struct application_cfg*);
int xml_parser(char*);
int template_xml_parser();
void print_xml_response(char*, int);
void free_application_cfg(struct application_cfg*);
void print_node(FILE*, struct resource*);
void print_link(FILE*, struct resource*);
void print_endpoint(FILE*, struct endpoint*);
struct adtlist* dragon_query_result_parser(char*, struct node_cfg*);
int dragon_result_parser(char*, struct node_cfg *);
void print_final(char*);
void print_final_client(char*);
void print_error_response(char*);
struct application_cfg* agent_final_parser(char*);
void add_cfg_to_list();
void del_cfg_from_list(struct application_cfg*);
struct application_cfg* search_cfg_in_list(char*);
struct resource * search_node_by_name(struct application_cfg*, char*);
void set_allres_fail(char*);
void set_res_fail(char*, struct resource *);
void app_cfg_pre_req();
int get_node_stype_by_str(char*);
struct resource * search_link_by_name(struct application_cfg*, char*);

int send_file_over_sock(int, char*);
int send_file_to_agent(char*, int, char*);

#define ID_SETUP	1
#define ID_TEMP		2
#define ID_QUERY	3
char* generate_ast_id(int);

#endif /* _ZEBRA_AST_MASTER_H */
