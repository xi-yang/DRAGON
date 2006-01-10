/* Created by DRAGON Team 2005-2006 
 *
 * Modifed from ftos_telnet_hack.c
 *
 * adds/removes untagged ports in VLANs on Force10 devices via telnet
 * 
 * Tue Apr 12 14:04:01 EDT 2005
 * chris tracy (chris@maxgigapop.net)
 * Modified by Jason Lu on Aug 1, 2005
 *
 * How it actually works
 * ---------------------
 * It starts a 'telnet' session to the Force10 device, issues the
 * commands to login as well as other commands necessary to configure
 * VLANs and then logs out. The information is passed to the parent
 * process through pipes (the parent process has a pipe through which
 * it can send commands to the 'stdin' of the 'telnet process, and
 * another pipe through which it receives the output from the 'stdout'
 * (and 'stderr') of the 'telnet' process).
 *
 * Building the utility
 * --------------------
 *
 * - Using FreeBSD or Linux:
 *   $ gcc -g -Wall ftos_telnet_hack.c -o ftos_telnet_hack 
 *
 * Using the utility
 * -----------------
 *
 *   $ ftos_telnet_hack [-vh] serverAddress portName vlanNumber action
 *
 *   where: serverAddress = IP address of Force10 device
 *          portName      = interface name (e.g. 'gi3/1')
 *          vlanNumber    = vlan number (e.g. '10')
 *          action        = keyword 'add' or 'remove'
 *
 * Limitations
 * -----------
 * There is practically no error checking with regard to what we
 * receive back from the Force10 OS.  If a particular configuration
 * command errors out, there is no way to know right now.  We do take
 * precautions against timeouts by using alarm(3).  To catch
 * configuration errors, we should look for the '% Error:' string when
 * in configure mode.  It might also be a good idea to catch password
 * problems as well.
 *
 * Examples
 * --------
 *
 *   Add port gi3/1 to VLAN 10:
 *   --------------------------
 * 
 * # ./ftos_telnet_hack -v 10.10.10.2 gi3/1 10 add   
 * 
 * Trying 10.10.10.2...
 * Connected to 10.10.10.2.
 * Escape character is '^]'.
 * Login: jason_lu
 * Password: 
 * hopi-e600>enable
 * hopi-e600#configure
 * hopi-e600(conf)#interface gi3/1
 * hopi-e600(conf-if-gi-3/1)#switchport
 * hopi-e600(conf-if-gi-3/1)#exit
 * hopi-e600(conf)#interface vlan 10
 * hopi-e600(conf-if-vl-10)#untagged gi3/1
 * hopi-e600(conf-if-vl-10)#exit
 * hopi-e600(conf)#exit
 * hopi-e600#show vlan id 10
 * 
 * Codes: * - Default VLAN, G - GVRP VLANs
 * 
 *     NUM    Status    Q Ports
 *     10     Inactive  U Gi 3/1
 * hopi-e600#exit
 * Connection closed
 * 
 *   Remove port gi3/1 from VLAN 10:
 *   -------------------------------
 *
 * # ./ftos_telnet_hack -v 10.10.10.2 gi3/1 10 remove
 * 
 * Trying 10.10.10.2...
 * Connected to 10.10.10.2.
 * Escape character is '^]'.
 * Login: jason_lu
 * Password: 
 * hopi-e600>enable
 * hopi-e600#configure
 * hopi-e600(conf)#interface gi3/1
 * hopi-e600(conf-if-gi-3/1)#no switchport
 * % Error: Gi 3/1 Port is part of a non-default VLAN.
 * hopi-e600(conf-if-gi-3/1)#exit
 * hopi-e600(conf)#interface vlan 10
 * hopi-e600(conf-if-vl-10)#no untagged gi3/1
 * hopi-e600(conf-if-vl-10)#exit
 * hopi-e600(conf)#exit
 * hopi-e600#show vlan id 10
 * 
 * Codes: * - Default VLAN, G - GVRP VLANs
 * 
 *     NUM    Status    Q Ports
 *     10     Inactive  
 * hopi-e600#exit
 * Connection closed
 *
 *   Example showing bogus interface name and VLAN number:
 *   -----------------------------------------------------
 *
 * # ./ftos_telnet_hack -v 10.10.10.2 gi3/12345 12345 remove
 * 
 * Trying 10.10.10.2...
 * Connected to 10.10.10.2.
 * Escape character is '^]'.
 * Login: jason_lu
 * Password: 
 * hopi-e600>enable
 * hopi-e600#configure
 * hopi-e600(conf)#interface gi3/12345
 *                              ^
 * % Error: Value out of range at "^" marker.
 * hopi-e600(conf)#no switchport
 *                    ^
 * % Error: Invalid input at "^" marker.
 * hopi-e600(conf)#exit
 * hopi-e600#interface vlan 12345
 *           ^
 * % Error: Invalid input at "^" marker.
 * hopi-e600#no untagged gi3/12345
 *              ^
 * % Error: Invalid input at "^" marker.
 * hopi-e600#exit
 * Connection closed by foreign host.
 * ftos_telnet_hack: connection to host '10.10.10.2' failed
 *  */

#include "force10_hack.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

char  progname[100];
char  hostname[100];
pid_t pid;
int   got_alarm;
int   verbose;
bool pipe_broken = false;

/* show usage info */
void usage(void)
{
  fprintf(stderr, "usage: %s [-vh] serverAddress portName vlanNumber (add|remove)\n", progname);
  exit(0);
}

/* print error text */
void err_msg(const char *format, ...)
{
  va_list ap;

  /* show error message */
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fflush(stderr);
}

/* print error text and exit gracefully */
void err_exit(const char *format, ...)
{
  va_list ap;

  /* show error message */
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fflush(stderr);

  /* exit with error */
  exit(1);
}

/* try to terminate the telnet-child that we started */
void stop_cli(void)
{
  pid_t p;

  if ((pid != -1) && (pid != 0)) {
    if (kill(pid, 0) >= 0) {
      kill(pid, SIGTERM);
      kill(pid, SIGKILL);

      if (kill(pid, 0) >= 0) {
	for(p = wait(0); (p != -1) && (p != pid); p = wait(0)) ; /* zombies */
      }
    }
    pid = -1;
  }
}

/* our timeout procedure, used to abort malfunctioning connections */
void sigalrm(int signo)
{
  got_alarm = 1;
  err_msg("%s: timeout on connection to host '%s'\n", progname, hostname);
}

/* our timeout procedure, used to abort malfunctioning connections */
void sigpipe(int signo)
{
  pipe_broken = true;
  fprintf(stderr, ".... pipe broken!\n");
}

/* handle the reception of signals */
void sigfunct(int signo)
{
#if defined(SIGPIPE)
  if (signo != SIGPIPE)
#endif
  {
    err_msg("%s: received signal #%d during connection to host '%s' -- aborting\n", progname, signo, hostname);
  }

  force10_hack(NULL, NULL, "disengage");
  exit(1);
}

int pipe_alive(int fdi, int fdo)
{
  int n;
  if (fdi < 0 || fdo < 0)
    return 0;

  pipe_broken = false;
  if ((n = do_write(fdo, "\n", 2)) < 0 || pipe_broken) 
  	return 0;

  if ((n = do_read (fdi, FORCE10_PROMPT, NULL, 0, 3)) < 0  || pipe_broken) 
  	return 0;

  return 1;
}


/* magic incantations for manipulating VLANs in Force10 OS (FTOS) land */
int force10_hack(char* portName, char* vlanNum, char* action)
{
  assert(CLI_SESSION_TYPE);

  static int fdin = -1;
  static int fdout = -1;
  int fdpipe[2][2], fderr, err, n;
  char tagged_untagged[20];
  int i, level = 0;
    
  got_alarm = 0;

  if (strcmp(action, "refresh") == 0) {
      if (pipe_alive(fdin, fdout)) {
        if ((n = do_write(fdout, "\n", 5)) >= 0)
          n = do_read(fdin, FORCE10_PROMPT, NULL, 1, 10);
        if (n >=0)
            return 0;
      }
      fdin = fdout = -1;
      return -1;
  } else if (strcmp(action, "disengage") == 0) {
      if (pipe_alive(fdin, fdout)) {
        /* exit interface configuration mode */
        if ((n = do_write(fdout, "exit\n", 5)) >= 0)
          n = do_read(fdin, FORCE10_PROMPT, NULL, 1, 10);
      }
      if (fdin >= 0)
        close(fdin);
      if (fdout >= 0)
        close(fdout);
      fdin = fdout = -1;
      stop_cli();
      return 0;
  } else if (strcmp(action, "check") == 0) {
      if (pipe_alive(fdin, fdout))
	  return 0;
      fdin = fdout = -1;
      return -1;
  } else if (!pipe_alive(fdin, fdout)) {
      /* we need pipes to communicate between the programs */
      if (pipe(fdpipe[0]) < 0) {
        err_msg("%s: pipe failed: errno=%d\n", progname, errno);
        return -1;
      }
      if (pipe(fdpipe[1]) < 0) {
        err_msg("%s: pipe failed: errno=%d\n", progname, errno);
        close(fdpipe[0][0]);
        close(fdpipe[0][1]);
        return -1;
      }

      switch(pid = fork()) {
      case 0: /* child */
        /* child:stdin */
        close(0);
        if (dup(fdpipe[0][0]) < 0) {
          err_exit("%s: dup failed: errno=%d\n", progname, errno);
        }
      
        /* close first pipe */
        close(fdpipe[0][0]);
        close(fdpipe[0][1]);
      
        /* child:stdout */
        close(1);
        if (dup(fdpipe[1][1]) < 0) {
          err_exit("%s: dup failed: errno=%d\n", progname, errno);
        }
      
        /* child:stderr */
        if ((fderr = dup(2)) < 0) {
          err_exit("%s: dup failed: errno=%d\n", progname, errno);
        }
      
        close(2);
        if (dup(fdpipe[1][1]) < 0) {
          err = errno;
          dup(fderr);
          err_exit("%s: dup failed: errno=%d\n", progname, err);
        }
      
        /* close second pipe */
        close(fdpipe[1][0]);
        close(fdpipe[1][1]);
      
        /* exec CLI session application */
        if (CLI_SESSION_TYPE == CLI_TELNET) {
            execl(TELNET_EXEC, "telnet", hostname, TELNET_PORT, (char*)NULL);
          
            /* if we're still here the TELNET_EXEC could not be exec'd */
            err = errno;
            close(2);
            dup(fderr);
            err_exit("%s: execl(%s) failed: errno=%d\n", progname, TELNET_EXEC, err);
        } if (CLI_SESSION_TYPE == CLI_SSH) {
            execl(SSH_EXEC, "ssh", hostname, "-p", TELNET_PORT, "-l", CLI_USERNAME, (char*)NULL);
          
            /* if we're still here the SSH_EXEC could not be exec'd */
            err = errno;
            close(2);
            dup(fderr);
            err_exit("%s: execl(%s) failed: errno=%d\n", progname, SSH_EXEC, err);            
        }
        else {
            err_exit("invalid cli seesion execl: %s\n", progname);
        }
        break;

      case -1: /* error */
        err_exit("%s: fork failed: errno=%d\n", progname, errno);
        return -1;
      
      default: /* parent */
        /* close the childs end of the pipes */
        close(fdpipe[0][0]);
        close(fdpipe[1][1]);
        break;
      	}
      
      /* now communicate with the 'telnet' process */
      fdin = fdpipe[1][0];
      fdout = fdpipe[0][1];
    
      if (CLI_SESSION_TYPE == CLI_TELNET) {
          /* wait for login prompt */
          n = do_read(fdin, "Login: ", TELNET_PROMPT, 1, 15);
          if (n != 1) {
            if (got_alarm == 0)
              err_msg("%s: connection to host '%s' failed\n", progname, hostname);
            goto _telnet_dead;
          }
          
          /* send the telnet username and password */
          if ((n = do_write(fdout, CLI_USERNAME, 5)) < 0) goto _telnet_dead;
          if ((n = do_write(fdout, "\n", 5)) < 0) goto _telnet_dead;
          if ((n = do_read (fdin,  "Password: ", NULL, 1, 10)) < 0) goto _telnet_dead;
          if ((n = do_write(fdout, CLI_PASSWORD, 5)) < 0) goto _telnet_dead;
          if ((n = do_write(fdout, "\n", 5)) < 0) goto _telnet_dead;
          if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) goto _telnet_dead;
      } 
      else if (CLI_SESSION_TYPE == CLI_SSH) {
          if ((n = do_read (fdin,  "Password: ", "The authenticity", 1, 10)) < 0) goto _telnet_dead;
          if (n == 2) {
              if ((n = do_write(fdout, "yes", 5)) < 0) goto _telnet_dead;
              if ((n = do_write(fdout, "\n", 5)) < 0) goto _telnet_dead;
              if ((n = do_read(fdin, "Password: ", CLI_USERNAME, 1, 10) < 0) goto  _telnet_dead;
          }
          if ((n = do_write(fdout, CLI_PASSWORD, 5)) < 0) goto _telnet_dead;
          if ((n = do_write(fdout, "\n", 5)) < 0) goto _telnet_dead;
          if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) goto _telnet_dead;
      }
  }

  for(;;) {

    /* enter enable mode and send enable password */
   /* if ((n = do_write(fdout, "enable\n", 5)) < 0) break;
    if ((n = do_read (fdin,  "Password: ", NULL, 1, 10)) < 0) break;
    if ((n = do_write(fdout, ENABLE_PASSWORD, 5)) < 0) break;
    if ((n = do_write(fdout, "\n", 5)) < 0) break;
    if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
   */
    /* enter configuration mode */
    if ((n = do_write(fdout, "configure\n", 5)) < 0) break;
    if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
    level++;

    if (strstr(action, "add") != NULL) {
      /* when adding a port, do interface config then vlan config */

      /* enter interface configuration mode */
      if ((n = do_write(fdout, "interface ", 5)) < 0) break;
      if ((n = do_write(fdout, portName, 5)) < 0) break;
      if ((n = do_write(fdout, "\n", 5)) < 0) break;
      if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
      level++;

      /* XXX should we make the sysadmin do this or should the VLSR do it??? */ 
      /* switchport command sets an interface to layer-2 mode */
      if ((n = do_write(fdout, "no shutdown\n", 5)) < 0) break;
      if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
      if ((n = do_write(fdout, "switchport\n", 5)) < 0) break;
      if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
      
      /* exit interface configuration mode */
      if ((n = do_write(fdout, "exit\n", 5)) < 0) break;
      if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
      level--;

      /* enter vlan configuration mode */
      if ((n = do_write(fdout, "interface vlan ", 5)) < 0) break;
      if ((n = do_write(fdout, vlanNum, 5)) < 0) break;
      if ((n = do_write(fdout, "\n", 5)) < 0) break;
      if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
      level++;
      
      /* read parameter that specifies a port is tagged or untagged */
      if (strstr(action, "untagged") != NULL)
        strcpy(tagged_untagged, "untagged ");
      else
        strcpy(tagged_untagged, "tagged ");
      /* add specified port as an untagged/tagged member of the specified VLAN */
      if ((n = do_write(fdout, tagged_untagged, 5)) < 0) break;
      if ((n = do_write(fdout, portName, 5)) < 0) break;
      if ((n = do_write(fdout, "\n", 5)) < 0) break;
      if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
      
      /* exit vlan configuration mode */
      if ((n = do_write(fdout, "exit\n", 5)) < 0) break;
      if ((n = do_read(fdin, FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
      level--;
    }
    if (strstr(action, "remove") != NULL) {
      /* when removing a port, do vlan config then interface config */
      /* also, use the 'no' keyword in the commands */ 


      if (strstr(action, "shutdown") != NULL) {
          if ((n = do_write(fdout, "interface ", 5)) < 0) break;
          if ((n = do_write(fdout, portName, 5)) < 0) break;
          if ((n = do_write(fdout, "\n", 5)) < 0) break;
          if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
          level++;
          /* shutdown a port before it is removed and put back into VLAN 1*/
          if ((n = do_write(fdout, "shutdown\n", 5)) < 0) break;
          if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
          /* exit interface port configuration mode */
          if ((n = do_write(fdout, "exit\n", 5)) < 0) break;
          if ((n = do_read(fdin, FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
          level--;
      }

      /* enter vlan configuration mode */
      if ((n = do_write(fdout, "interface vlan ", 5)) < 0) break;
      if ((n = do_write(fdout, vlanNum, 5)) < 0) break;
      if ((n = do_write(fdout, "\n", 5)) < 0) break;
      if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
      level++;

      /* read parameter that specifies a port is tagged or untagged */
      if (strstr(action, "untagged") != NULL)
        strcpy(tagged_untagged, "no untagged ");
      else
        strcpy(tagged_untagged, "no tagged ");
      /* remove specified port as an untagged member/untagged of the specified VLAN */
      if ((n = do_write(fdout, tagged_untagged, 5)) < 0) break;
      if ((n = do_write(fdout, portName, 5)) < 0) break;
      if ((n = do_write(fdout, "\n", 5)) < 0) break;
      if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
      
      /* exit vlan configuration mode */
      if ((n = do_write(fdout, "exit\n", 5)) < 0) break;
      if ((n = do_read(fdin, FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
      level--;

      /* XXX should we make the sysadmin do this or should the VLSR do it??? */ 
      /* enter interface configuration mode */
      //if ((n = do_write(fdout, "interface ", 5)) < 0) break;
      //if ((n = do_write(fdout, portName, 5)) < 0) break;
      //if ((n = do_write(fdout, "\n", 5)) < 0) break;
      //if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
      // switchport command sets an interface to layer-2 mode
      //if ((n = do_write(fdout, "no switchport\n", 5)) < 0) break;
      //if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
      
      /* exit interface configuration mode */
      //if ((n = do_write(fdout, "exit\n", 5)) < 0) break;
      //if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
      //level--;
    }
    if (strstr(action, "rate police") != NULL) {
        float committed_rate;
        if(sscanf("%f", action+12, &committed_rate) < 1 || committed_rate == 0.0)
            break;

      /* enter interface/port configuration mode */
      if ((n = do_write(fdout, "interface ", 5)) < 0) break;
      if ((n = do_write(fdout, portName, 5)) < 0) break;
      if ((n = do_write(fdout, "\n", 5)) < 0) break;
      if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
      level++;

      if ((n = do_write(fdout, action, 5)) < 0) break;
      if ((n = do_write(fdout, "\n", 5)) < 0) break;
      if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;

      /* exit interface configuration mode */
      if ((n = do_write(fdout, "exit\n", 5)) < 0) break;
      if ((n = do_read(fdin, FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
      level--;
    }
    if (strstr(action, "rate limit") != NULL) {
        float committed_rate;
        if(sscanf("%f", action+11, &committed_rate) < 1 || committed_rate == 0.0)
            break;

      /* enter interface/port configuration mode */
      if ((n = do_write(fdout, "interface ", 5)) < 0) break;
      if ((n = do_write(fdout, portName, 5)) < 0) break;
      if ((n = do_write(fdout, "\n", 5)) < 0) break;
      if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
      level++;

      if ((n = do_write(fdout, action, 5)) < 0) break;
      if ((n = do_write(fdout, "\n", 5)) < 0) break;
      if ((n = do_read (fdin,  FORCE10_PROMPT, NULL, 1, 10)) < 0) break;      

      /* exit interface configuration mode */
      if ((n = do_write(fdout, "exit\n", 5)) < 0) break;
      if ((n = do_read(fdin, FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
      level--;
    }

    /* exit configuration mode */
    if ((n = do_write(fdout, "exit\n", 5)) < 0) break;
    if ((n = do_read(fdin, FORCE10_PROMPT, NULL, 1, 10)) < 0) break;
    level--;

    /* show the specified VLAN's configuration */
    if ((n = do_write(fdout, "show vlan id ", 5)) < 0) break;
    if ((n = do_write(fdout, vlanNum, 5)) < 0) break;
    if ((n = do_write(fdout, "\n", 5)) < 0) break;
    if ((n = do_read (fdin, FORCE10_PROMPT, NULL, 1, 10)) < 0) break;

    /* close telnet connection */
    //if ((n = do_write(fdout, "exit\n", 5)) < 0) break;
    //if ((n = do_read (fdin,  TELNET_CLOSE, NULL, 1, 20)) < 0) break;

    break;
  }

  printf("#DEBUG# done with level = %d!\n", level);

  /* return to root cli level */
  for (i = 0; i < level; i++) {
    if ((n = do_write(fdout, "exit\n", 5)) < 0)
      goto _telnet_dead;
    if ((n = do_read(fdin, FORCE10_PROMPT, NULL, 1, 5)) < 0)
      goto _telnet_dead;
  }

  if ((n = do_write(fdout, "end\n", 5)) < 0)  
      goto _telnet_dead;
  if ((n = do_read(fdin, FORCE10_PROMPT, NULL, 1, 5)) < 0)
      goto _telnet_dead;

  return 0;  

 _telnet_dead:

  printf("#DEBUG# telnet dead!\n");

  fdin = fdout = -1;
  return -1;
}

/* read information from 'telnet', stop reading upon errors, on a timeout */
/* and when the beginning of a line equals to 'text1' (and then return 1) */
/* or when the beginning of a line equals to 'text2' (and then return 2), */
/* if 'show' is non zero we display the information to the screen         */
/* (using 'stdout').                                                      */
int do_read(int fd, char *text1, char *text2, int show, int timeout)
{
  char line[LINELEN+1];
  int n, len1, len2, count = 0, m, err;

  /* setup alarm (so we won't hang forever upon problems) */
  signal(SIGALRM, sigalrm);
  alarm(timeout);
  
  len1 = (text1 != FORCE10_PROMPT) ? strlen(text1) : 0;
  len2 = ((text2 != FORCE10_PROMPT) && (text2 != NULL)) ? strlen(text2) : 0;

  /* start reading from 'telnet' */
  for(;;) {
    n = 0;
    for(;;) {
      if (n == LINELEN) {
	alarm(0); /* disable alarm */
	stop_cli();
	err_exit("%s: too long line!\n", progname);
      }
      m = read(fd, &line[n], 1);
      if (m != 1) {
	err = errno;
	alarm(0); /* disable alarm */
	return(-1);
      }
      if (verbose) fputc(0xff & (int)line[n], stdout);
      if (line[n] == '\r') continue;
      if (text1 == FORCE10_PROMPT) {
	if (is_force10_prompt(line, n+1)) {
	  /* we found the keyword we were searching for */
	  alarm(0); /* disable alarm */
	  return(1);
	}
      }
      else if (n >= len1-1) {
	if (strncmp(line, text1, len1) == 0) {
	  /* we found the keyword we were searching for */
	  alarm(0); /* disable alarm */
	  return(1);
	}
      }
      if (text2 == FORCE10_PROMPT) {
	if (is_force10_prompt(line, n+1)) {
	  /* we found the keyword we were searching for */
	  alarm(0); /* disable alarm */
	  return(2);
	}
      }
      else if ((text2 != NULL) && (n >= len2-1)) {
	if (strncmp(line, text2, len2) == 0) {
	  /* we found the keyword we were searching for */
	  alarm(0); /* disable alarm */
	  return(2);
	}
      }
      if (line[n] == '\n') break;
      n++;
    }
    if (show && !verbose) {
      line[++n] = '\0';
      if (++count > 1) {
	/* the very first line of information is the remains of  */
	/* a previously issued command and should be ignored.    */

	fputs(line, stdout);
	fflush(stdout);
      }
    }
  }
}

/* write a command to the 'telnet' process */
int do_write(int fd, char *text, int timeout)
{

  int err, len, n;

  if (verbose) printf("%s", text);

  /* setup alarm (so we won't hang forever upon problems) */
  signal(SIGALRM, sigalrm);
  alarm(timeout);

  len = strlen(text);
  n = write(fd, text, len);
  if (n != len) {
    err = errno;
    alarm(0); /* disable alarm */
    return(-1);
  }
  else {
    alarm(0); /* disable alarm */
    return(0);
  }
}

/* force10 CLI prompt ends in either > or # (ends in # when enabled) */
int is_force10_prompt(char *p, int len)
{
  int n, found = 0;

  for(n = 0; n < len; n++) {
    if (p[n] == '>' || p[n] =='#') {
      found = 1;
      break;
    }
    else if ((p[n] <= ' ') && (p[n] != '\r') && (p[n] != '\n')) break;
  }
  return found;
}

