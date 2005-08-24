/************** narb_vty.h ***********/
#ifndef _NARB_VTY_H_
#define _NARB_VTY_H_

/* VTY port number. */
#define NARB_VTY_PORT          2626
/* VTY shell path. */
#define NARB_VTYSH_PATH        "/tmp/.narb"
/* default VTY password */
#define NARB_VTY_DEFAULT_PASSWORD "dragon"

extern void
narb_vty_init ();

extern void
narb_vty_cleanup ();
#endif

