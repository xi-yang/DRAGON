#ifndef _ZEBRA_AST_MASTER_H
#define _ZEBRA_AST_MASTER_H

#define MASTER_DEFAULT_CONFIG "ast_master.conf"
#define AST_XML_RECV    "/usr/local/ast_master_recv.xml"

int master_config_write(struct vty*);
void master_supp_vty_init();

#endif /* _ZEBRA_AST_MASTER_H */
