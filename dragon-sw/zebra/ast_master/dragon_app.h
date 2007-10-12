#ifndef _AST_MASTER_DRAGON_APP_H
#define _AST_MASTER_DRAGON_APP_H

#include "ast_master_ext.h"

#define NUM_DRAGON_LINK_TYPE	3
#define DRAGON_XML_PORT         2618

enum link_stype { uni = 1, non_uni, vlsr_vlsr};
enum link_status { commit = 1, error, in_service, delete };

struct dragon_if_ip {
  char *iface;
  char *assign_ip;
  int vtag;
};

struct dragon_endpoint {
  struct resource *node;
  struct resource *proxy;
  struct dragon_if_ip *ifp;
  char local_id_type[REG_TXT_FIELD_LEN+1];
  int local_id;
};

struct dragon_node_pc {
  struct in_addr router_id;
  char tunnel[10];
  char *command;
  struct adtlist *if_list;
  struct adtlist *link_list;
};

struct dragon_link_profile {
  char service_name[REG_TXT_FIELD_LEN+1];
  char bandwidth[REG_TXT_FIELD_LEN+1];
  char swcap[REG_TXT_FIELD_LEN+1];
  char encoding[REG_TXT_FIELD_LEN+1];
  char gpid[REG_TXT_FIELD_LEN+1];
  struct dragon_link_profile *next;
};

struct dragon_link {
  enum link_stype stype;
  char lsp_name[LSP_NAME_LEN+1];
  struct dragon_endpoint *src;
  struct dragon_endpoint *dest;
  struct resource *dragon;
  enum link_status l_status;
  struct dragon_link_profile *profile; 

  /* te_params */
  char vtag[REG_TXT_FIELD_LEN+1];
  char bandwidth[REG_TXT_FIELD_LEN+1];
  char swcap[REG_TXT_FIELD_LEN+1];
  char encoding[REG_TXT_FIELD_LEN+1];
  char gpid[REG_TXT_FIELD_LEN+1];
};

/* functions for RESOURCE dragon_node_pc */
void* dragon_node_pc_read(struct application_cfg*, xmlNodePtr, int);
int dragon_node_pc_validate(struct application_cfg*, struct resource*, int);
int dragon_node_pc_process_resp(struct application_cfg *, struct resource *, struct resource *);
int dragon_node_pc_compose_req(char*, struct application_cfg*, struct resource*);
void dragon_node_pc_print(FILE*, void*, int);
void dragon_node_pc_print_cli(struct vty*, void*);
void dragon_node_pc_free(void*);

/* functions for RESOURCE dragon_link */
void* dragon_link_read(struct application_cfg*, xmlNodePtr, int);
int dragon_link_validate(struct application_cfg*, struct resource*, int);
int dragon_link_process_resp(struct application_cfg *, struct resource *, struct resource *);
int dragon_link_compose_req(char*, struct application_cfg*, struct resource*);
void dragon_link_print(FILE*, void*, int);
void dragon_link_print_cli(struct vty*, void*);
void dragon_link_free(void*);

/* functions for dragon app modele */
int init_dragon_module();
#endif /* _AST_MASTER_DRAGON_APP_H */
