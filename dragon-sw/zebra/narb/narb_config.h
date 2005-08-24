/*narb_config.h*/
#ifndef _NARB_CONFIG_H_
#define _NARB_CONFIG_H_

#define NARB_BUFSIZ 0x10000
#define NARB_BLKSIZ 0x400
#define NARB_LINESIZ 0x100

#define DEFAULT_OSPFD_INTER_PORT 2607
#define DEFAULT_OSPFD_INTRA_PORT 2617

/*definitions of codes for configuration blocks*/
enum config_code {
  CONFIG_END = 0,
  CONFIG_INTER_DOMAIN_ODPFD = 1,
  CONFIG_INTRA_DOMAIN_ODPFD,
  CONFIG_ROUTER,
  CONFIG_LINK,
  CONFIG_SERVICE,
  CONFIG_SVC_PROBE,
  CONFIG_INTER_DOMAIN_TE_LINK,
  CONFIG_VTY,
  CONFIG_DOMAIN_ID,
  CONFIG_UNKNOWN
};

#define NARB_DEFAULT_CONFIG "narb.conf"

/* declarations of narb_config functions*/

extern int
narb_read_config (char *config_file, char *config_current_dir, char *config_default_dir,
        struct narb_domain_info * p_domain_info); 

extern void
narb_config_from_file(FILE *fp, struct narb_domain_info * p_domain_info);

extern int
blk_code (char *buf);

extern int
read_config_blk(char *buf, char * header, char * body, char ** next);

extern int
read_config_parameter(char * buf, char * id, char * fmt, void * parameter);

extern u_int32_t 
narb_ospf_opaque_id (void);

extern void
if_narb_add (list table, char *addr, int port, struct in_addr if_addr);

extern struct service_info *
service_lookup_by_id(list services, int id);

#endif
