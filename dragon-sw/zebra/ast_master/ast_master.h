#ifndef _ZEBRA_AST_MASTER_H
#define _ZEBRA_AST_MASTER_H

#define MASTER_DEFAULT_CONFIG "ast_master.conf"

int master_config_write(struct vty*);
void master_supp_vty_init();

#endif /* _ZEBRA_AST_MASTER_H */
