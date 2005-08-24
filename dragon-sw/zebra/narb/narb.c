/* Implementation of the NARB server  */
#include "zebra.h"
#include "prefix.h" 
#include "linklist.h"
#include "thread.h"
#include "log.h"
#include "getopt.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_te.h"
#include "ospfd/ospf_opaque.h"
#include "ospfd/ospf_api.h"

#include "ospf_apiclient.h"
#include "narb_apiserver.h"

#include "ospfd/ospf_dump.h" /* for ospf_lsa_header_dump */

#include "narb_config.h"
#include "narb_summary.h"
#include "narb_vty.h"
#include "narb_rceapi.h"
#include "dragond.h"

/* Remote OSPFd ports for sync channels.*/
#define OSPF_API_SYNC_PORT_INTER 2607
#define OSPF_API_SYNC_PORT_INTRA 2617

/* Configuration filename and directory. */
char narb_config_current[] = NARB_DEFAULT_CONFIG;
char narb_config_default[] = "/usr/local/etc/" NARB_DEFAULT_CONFIG;

char * narb_vty_addr = NULL;

  
/* Global variables */
/* Master thread */
struct dragon_master dmaster;
struct thread_master *master;
/*inter-domain instance of ospf_apiclient*/
struct ospf_apiclient *oclient_inter;
/*intra-domain instance of ospf_apiclient*/
struct ospf_apiclient *oclient_intra;

/* ---------------------------------------------------------
 * Main program 
 * ---------------------------------------------------------
 */

/* NARB options. */
struct option longopts[] = 
{
  { "daemon",      no_argument,       NULL, 'd'},
  { "config_file", required_argument, NULL, 'f'},
  { "port",        no_argument,       NULL, 'p'},
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
 Daemon which manages NARB.\n\n\
-d, --daemon       Runs in daemon mode\n\
-f, --config_file  Set configuration file name\n\
-p, --apiserver port  The port listen to client connection requests \n\
-v, --version      Print program version\n\
-h, --help         Display this help and exit\n",
      progname);
    }
  exit (status);
}

static void
print_version (char *progname)
{
  printf ("%s (NARB): version-0.1 ... developed by the DRAGON team.\n", progname);
}

/* SIGHUP handler. */
void 
sighup (int sig)
{
  zlog (NULL, LOG_INFO, "SIGHUP received");
}

/* SIGINT handler. */
void
sigint (int sig)
{
  zlog (NULL, LOG_INFO, "Terminating on signal");

  if (oclient_inter)
    ospf_apiclient_close(oclient_inter);
  if (oclient_intra)
    ospf_apiclient_close(oclient_intra);
  if (rce_api_sock > 0)
    close(rce_api_sock);
  
  narb_apiserver_free_all();

  narb_vty_cleanup();
  
  exit (0);
}

/* SIGUSR1 handler. */
void
sigusr1 (int sig)
{
  zlog_rotate (NULL);
}

/* Signal wrapper. */
RETSIGTYPE *
signal_set (int signo, void (*func)(int))
{
  int ret;
  struct sigaction sig;
  struct sigaction osig;

  sig.sa_handler = func;
  sigemptyset (&sig.sa_mask);
  sig.sa_flags = 0;
#ifdef SA_RESTART
  sig.sa_flags |= SA_RESTART;
#endif /* SA_RESTART */

  ret = sigaction (signo, &sig, &osig);

  if (ret < 0) 
    return (SIG_ERR);
  else
    return (osig.sa_handler);
}

/* Initialization of signal handles. */
void
signal_init ()
{
  signal_set (SIGHUP, sighup);
  signal_set (SIGINT, sigint);
  signal_set (SIGTERM, sigint);
  signal_set (SIGPIPE, SIG_IGN);
#ifdef SIGTSTP
  signal_set (SIGTSTP, SIG_IGN);
#endif
#ifdef SIGTTIN
  signal_set (SIGTTIN, SIG_IGN);
#endif
#ifdef SIGTTOU
  signal_set (SIGTTOU, SIG_IGN);
#endif
  signal_set (SIGUSR1, sigusr1);
}

/**********MAIN FUNCTION***********/
int
main (int argc, char *argv[])
{
  char *p;
  int daemon_mode = 0;
  char *config_file = NULL;
  int vty_port = 0;
  
  char * progname;
  struct thread thread;
  int ret;

  /* Set umask before anything for security */
  umask (0027);
  
  /* get program name */
  progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);

  /* Only root user can run NARB server. */
  if (getuid () != 0)
    {
      errno = EPERM;
      perror (progname);
      exit (1);
    }

  /*Parsing command line args */
  while (1)
  {
    int opt;

    opt = getopt_long (argc, argv, "df:p:P:vhH:", longopts, 0);
    if (opt == EOF)
      break;

    switch (opt) 
      {
      case 0:
        break;
      case 'd':
	     daemon_mode = 1;
        break;
      case 'f':
        config_file = optarg;
	    break;
      case 'p':
        NARB_API_SYNC_PORT = atoi(optarg);
	    break;
      case 'P':
        vty_port = atoi(optarg);
	    break;
      case 'H':
        narb_vty_addr = optarg;
	    break;
      case 'v':
       print_version (progname);
       exit (0);
       break;
	  case 'h':
       usage (progname, 0);
       exit (0);
       break;
	 default:
       usage (progname, 1);
       break;
      }
  }      

  /* Initialization */
  signal_init ();
  master = thread_master_create ();
  narb_read_config (config_file, narb_config_current, narb_config_default, &narb_domain_info);

  /* initialize NARB-OSPF client to communicate to OSPFd_inter*/
  oclient_inter = ospf_apiclient_connect (narb_domain_info.ospfd_inter.addr,
                  NARB_OSPFD_LOCAL_PORT_INTER, OSPF_API_SYNC_PORT_INTER);
  if (!oclient_inter)
    {
      zlog_warn("ospf_apiclient_connect (inter) failed !");
      flag_holdon = 0;
      /*return -1;*/
    }
  
  /* initialize NARB-OSPF client to communicate to OSPFd_intra*/
  oclient_intra = ospf_apiclient_connect (narb_domain_info.ospfd_intra.addr,
                  NARB_OSPFD_LOCAL_PORT_INTRA, OSPF_API_SYNC_PORT_INTRA);
  if (!oclient_intra)
    {
      zlog_warn("ospf_apiclient_connect (intra) failed !");
      /*return -1;*/
    }

  /* initialize NARB server to accept app client connection requests */
  ret = narb_apiserver_init();

  if (ret < 0)
    {
      zlog_warn("narb_apiserver_init failed!");
      return ret;
    }

  narb_vty_init ();
  sort_node ();

  /* change to the daemon program. */
  if (daemon_mode)
    daemon (0, 0);

  /* Create VTY socket */
  vty_serv_sock (narb_vty_addr,
		 vty_port ? vty_port : NARB_VTY_PORT, NARB_VTYSH_PATH);

  if (oclient_inter)
    {
        /* register opaque type with OSPFd_inter for originating opaque LSA's*/
        ret = ospf_apiclient_register_opaque_type (oclient_inter, 10, 1);
        if (ret < 0)
          {
            zlog_warn("ospf_apiclient_register_opaque_type[10, 1] (inter) failed !");
          }
      /* originate LSA's to summary the domain topology. A timer thread is 
          started inside to refresh the domain-summary LSA's periodically.*/
      narb_originate_summary(oclient_inter);

      /* start a timer thread to fetch back oapque LSA's from the LSDB of 
          inter-domain OSPFd*/
      oclient_inter->t_sync_lsdb =
          thread_add_timer (master, ospf_narb_sync_lsdb, 
              oclient_inter, OSPF_NARB_SYNC_LSDB_INVERVAL);
    }

    /* pun loop for the threads */
    while (1)
      {
        thread_fetch (master, &thread);
        thread_call (&thread);
      }

  /* Never reached */
  return 0;
}

