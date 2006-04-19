/****************************************************************************

CLI Based Switch Control Module source file CLI_Session.cc
Created by Xi Yang @ 01/17/2006
Extended from SNMP_Global.cc by Aihua Guo and Xi Yang, 2004-2005
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include "CLI_Session.h"

/////////----Global varaibles and C functions-----///////////

char  progname[100];
char  hostname[100];
pid_t pid;
int   got_alarm;
bool pipe_broken = false;

// print error text 
void err_msg(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fflush(stderr);
}

// print error text and exit gracefully
void err_exit(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fflush(stderr);

  exit(1);
}


// our timeout procedure, used to abort malfunctioning connections 
void sigalrm(int signo)
{
  got_alarm = 1;
  err_msg("%s: timeout on connection to host '%s'\n", progname, hostname);
}

// our timeout procedure, used to abort malfunctioning connections 
void sigpipe(int signo)
{
  static int num_broken = 100;
  pipe_broken = true;
  err_msg(".... pipe broken!\n");
  if (--num_broken < 0)
  {
    err_msg("Too many 'pipe broken' errors! Aborting....\n");
    exit(1);
  }
}

// handle the reception of signals 
void sigfunct(int signo)
{
#if defined(SIGPIPE)
  if (signo != SIGPIPE)
#endif
  {
    err_msg("%s: received signal #%d during connection to host '%s' -- aborting\n", progname, signo, hostname);
  }

  exit(1);
}

/////////---------///////////

bool CLI_Session::connectSwitch()
{
    bool ret  = engage();
    return ret & SwitchCtrl_Session::connectSwitch();
}

void CLI_Session::disconnectSwitch() 
{
    disengage();
    SwitchCtrl_Session::disconnectSwitch();
}

bool CLI_Session::engage()
{
    int fdpipe[2][2], fderr, err, n;

    got_alarm = 0;
    strcpy(progname, "vlsr-ctrl-cli-session"); 
    strcpy(hostname, convertAddressToString(switchInetAddr).chars());

    // setup signals properly
    for(n = 1; n < NSIG; n++)
        if (n != 11)
            signal(n, sigfunct);
    signal(SIGCHLD, SIG_IGN);
#ifdef SIGPIPE
    signal(SIGPIPE, sigpipe);
#endif

    if (pipeAlive())
        return true;

    if (CLI_SESSION_TYPE == CLI_NONE)
        return false;

    // we need pipes to communicate between the programs 
    if (pipe(fdpipe[0]) < 0) {
        err_msg("%s: pipe failed: errno=%d\n", progname, errno);
        return false;
    }
    if (pipe(fdpipe[1]) < 0) {
        err_msg("%s: pipe failed: errno=%d\n", progname, errno);
        close(fdpipe[0][0]);
        close(fdpipe[0][1]);
        return false;
    }

    switch(pid = fork()) {
    case 0: // child 
        // child:stdin 
        close(0);
        if (dup(fdpipe[0][0]) < 0) {
         err_exit("%s: dup failed: errno=%d\n", progname, errno);
        }

        // close first pipe 
        close(fdpipe[0][0]);
        close(fdpipe[0][1]);

        // child:stdout 
        close(1);
        if (dup(fdpipe[1][1]) < 0) {
         err_exit("%s: dup failed: errno=%d\n", progname, errno);
        }

        // child:stderr 
        if ((fderr = dup(2)) < 0) {
         err_exit("%s: dup failed: errno=%d\n", progname, errno);
        }

        close(2);
        if (dup(fdpipe[1][1]) < 0) {
         err = errno;
         dup(fderr);
         err_exit("%s: dup failed: errno=%d\n", progname, err);
        }

        // close second pipe 
        close(fdpipe[1][0]);
        close(fdpipe[1][1]);

        // exec CLI session application 
        if (CLI_SESSION_TYPE == CLI_TELNET) {
           execl(TELNET_EXEC, "telnet", hostname, TELNET_PORT, (char*)NULL);
         
           // if we're still here the TELNET_EXEC could not be exec'd 
           err = errno;
           close(2);
           dup(fderr);
           err_exit("%s: execl(%s) failed: errno=%d\n", progname, TELNET_EXEC, err);
        } if (CLI_SESSION_TYPE == CLI_SSH) {
           char spawn_cmd[128];
           sprintf(spawn_cmd, "spawn ssh %s -l %s -p %s", hostname, CLI_USERNAME, SSH_PORT);
           execl("/usr/local/bin/expect", "expect", "-c", spawn_cmd, "-c", "interact", (char*)NULL);
         
           // if we're still here the SSH_EXEC could not be exec'd 
           err = errno;
           close(2);
           dup(fderr);
           err_exit("%s: execl(%s) failed: errno=%d\n", progname, SSH_EXEC, err);            
        }
        else {
           err_exit("invalid cli seesion execl: %s\n", progname);
        }
        break;

    case -1: // error 
        err_exit("%s: fork failed: errno=%d\n", progname, errno);
        return false;

    default: // parent 
        // close the childs end of the pipes 
        close(fdpipe[0][0]);
        close(fdpipe[1][1]);
        break;
    }

    // now communicate with the 'telnet' process 
    fdin = fdpipe[1][0];
    fdout = fdpipe[0][1];

    if (CLI_SESSION_TYPE == CLI_TELNET) {
     // wait for login prompt 
     n = readShell("Login: ", TELNET_PROMPT, 1, 15);
     if (n != 1) {
       if (got_alarm == 0)
         err_msg("%s: connection to host '%s' failed\n", progname, hostname);
       goto _telnet_dead;
     }
     
     // send the telnet username and password 
     if ((n = writeShell(CLI_USERNAME, 5)) < 0) goto _telnet_dead;
     if ((n = writeShell("\n", 5)) < 0) goto _telnet_dead;
     if ((n = readShell( "Password: ", NULL, 1, 10)) < 0) goto _telnet_dead;
     if ((n = writeShell(CLI_PASSWORD, 5)) < 0) goto _telnet_dead;
     if ((n = writeShell("\n", 5)) < 0) goto _telnet_dead;
     if ((n = readShell( SWITCH_PROMPT, NULL, 1, 10)) < 0) goto _telnet_dead;
    } 
    else if (CLI_SESSION_TYPE == CLI_SSH) {
     if ((n = readShell( "The authenticity", CLI_USERNAME, 1, 15)) < 0) {
       if (got_alarm == 0)
         err_msg("%s: connection to host '%s' failed\n", progname, hostname);
       goto _telnet_dead;
     }

     if (n == 1) {
         if ((n = writeShell("yes\n", 5)) < 0) goto _telnet_dead;
         if ((n = readShell(CLI_USERNAME, NULL, 1, 10)) < 0) goto
    _telnet_dead;
     }
     if ((n = writeShell(CLI_PASSWORD, 5)) < 0) goto _telnet_dead;
     if ((n = writeShell("\n", 5)) < 0) goto _telnet_dead;
     if ((n = readShell( SWITCH_PROMPT, NULL, 1, 10)) < 0) goto _telnet_dead;
    }

    return true;

 _telnet_dead:

    err_msg("#DEBUG# telnet dead!\n");
  
    fdin = fdout = -1;
    return false;
}

void CLI_Session::disengage()
{
    int n;
    if (pipeAlive()) {
        if ((n = writeShell("end\n", 5)) >= 0)
          n = readShell(SWITCH_PROMPT, NULL, 1, 10);
    }

    if (fdin >= 0)
        close(fdin);
    if (fdout >= 0)
        close(fdout);
    fdin = fdout = -1;
    stop();
}

void CLI_Session::stop()
{
    pid_t p;
  
    if ((pid != -1) && (pid != 0)) {
        if (kill(pid, 0) >= 0) {
            kill(pid, SIGTERM);
            kill(pid, SIGKILL);  
            if (kill(pid, 0) >= 0) {
                for(p = wait(0); (p != -1) && (p != pid); p = wait(0)) ; // zombies
            }
        }
        pid = -1;
    }
}
bool CLI_Session::refresh()
{
    int n;
    if (!pipeAlive()) {
        fdin = fdout = -1;
        return false;
    }
    DIE_IF_NEGATIVE(n = writeShell("\n", 5));
    DIE_IF_NEGATIVE(n = readShell(SWITCH_PROMPT, NULL, 1, 10));

    return true;
}

bool CLI_Session::pipeAlive()
{
  int n;
  if (fdin < 0 || fdout < 0)
    return false;

  pipe_broken = false;
  if ((n = writeShell("\n", 2)) < 0 || pipe_broken) 
  	return false;

  if ((n = readShell (SWITCH_PROMPT, NULL, 0, 3)) < 0  || pipe_broken) 
  	return false;

  return true;
}

// CLI prompt ends in either > or # (ends in # when enabled) 
bool CLI_Session::isSwitchPrompt(char *p, int len)
{
  int n, found = 0;

  for(n = 0; n < len; n++) {
    if (p[n] == '>' || p[n] =='#') {
      found = 1;
      break;
    }
    else if ((p[n] <= ' ') && (p[n] != '\r') && (p[n] != '\n')) break;
  }
  return (found == 1);
}

// read information from 'telnet', stop reading upon errors, on a timeout 
// and when the beginning of a line equals to 'text1' (and then return 1) 
// or when the beginning of a line equals to 'text2' (and then return 2), 
// if 'show' is non zero we display the information to the screen         
// (using 'stdout').                                                      
int CLI_Session::readShell(char *text1, char *text2, int verbose, int timeout)
{
  char line[LINELEN+1];
  int n, len1, len2, count = 0, m, err;

  if (fdin < 0)
    return (-1);

  // setup alarm (so we won't hang forever upon problems)
  signal(SIGALRM, sigalrm);
  alarm(timeout);
  
  len1 = (text1 != SWITCH_PROMPT) ? strlen(text1) : 0;
  len2 = ((text2 != SWITCH_PROMPT) && (text2 != NULL)) ? strlen(text2) : 0;

  // start reading from 'telnet'
  for(;;) {
    n = 0;
    for(;;) {
      if (n == LINELEN) {
	alarm(0); // disable alarm
	stop();
	err_exit("%s: too long line!\n", progname);
      }
      m = read(fdin, &line[n], 1);
      if (m != 1) {
	err = errno;
	alarm(0); // disable alarm
	return(-1);
      }
///////// debug info ////////
//      if (verbose) fputc(0xff & (int)line[n], stdout);
///////// debug info ////////
      if (line[n] == '\r') continue;
      if (text1 == SWITCH_PROMPT) {
	if (isSwitchPrompt(line, n+1)) {
	  // we found the keyword we were searching for
	  alarm(0); // disable alarm
	  return(1);
	}
      }
      else if (n >= len1-1) {
	if (strncmp(line, text1, len1) == 0) {
	  // we found the keyword we were searching for 
	  alarm(0); // disable alarm 
	  return(1);
	}
      }
      if (text2 == SWITCH_PROMPT) {
	if (isSwitchPrompt(line, n+1)) {
	  // we found the keyword we were searching for 
	  alarm(0); // disable alarm 
	  return(2);
	}
      }
      else if ((text2 != NULL) && (n >= len2-1)) {
	if (strncmp(line, text2, len2) == 0) {
	  // we found the keyword we were searching for 
	  alarm(0); // disable alarm 
	  return(2);
	}
      }
      if (line[n] == '\n') break;
      n++;
    }
///////// debug info ////////
//    if (!verbose) {
///////// debug info ////////
    if (verbose) {
      line[++n] = '\0';
      if (++count > 1) {
	// the very first line of information is the remains of  
	// a previously issued command and should be ignored.    
	fputs(line, stdout);
	fflush(stdout);
      }
    }
  }
}

// write a command to the 'telnet' process 
int CLI_Session::writeShell(char *text, int timeout, bool echo_back)
{
  int err, len, n;

  if (fdout < 0)
    return (-1);
 
  if (echo_back) {
    fprintf(stdout, text);
    fflush(stdout);
  }

  // setup alarm (so we won't hang forever upon problems) 
  signal(SIGALRM, sigalrm);
  alarm(timeout);

  len = strlen(text);
  n = write(fdout, text, len);
  if (n != len) {
    err = errno;
    alarm(0); // disable alarm 
    return(-1);
  }
  else {
    alarm(0); // disable alarm 
    return(0);
  }
}

bool CLI_Session::preAction()
{
    if (!active || vendor!=Force10E600 || !pipeAlive())
        return false;
    DIE_IF_NEGATIVE(writeShell("configure\n", 5));
    DIE_IF_NEGATIVE(readShell(SWITCH_PROMPT, NULL, 1, 10));
    return true;
}

bool CLI_Session::postAction()
{
    if (fdout < 0 || fdin < 0)
        return false;
    DIE_IF_NEGATIVE(writeShell("end\n", 5));
    readShell(SWITCH_PROMPT, NULL, 1, 10);
    return true;
}

