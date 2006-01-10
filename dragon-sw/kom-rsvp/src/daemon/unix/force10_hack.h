#ifndef __FORCE10_HACK_H__
#define __FORCE10_HACK_H__

#include <sys/types.h>

extern "C" {
    void usage(void);
    int force10_hack(char* portName, char* vlanNum, char* action);
    int do_read(int fd, char *text1, char *text2, int show, int timeout);
    int do_write(int fd, char *text, int timeout);
    int is_force10_prompt(char *p, int len);
    void sigfunct(int signo);
    void sigpipe(int signo);
}

extern char  progname[100];
extern char  hostname[100];
extern pid_t pid;
extern int    got_alarm;
extern int    verbose;

/* program constants */
#define TELNET_EXEC     "/usr/bin/telnet"
#define TELNET_CLOSE    "Connection closed"
#define TELNET_PORT     "23"
#define TELNET_PROMPT   "telnet> "
#define LINELEN  256
#define FORCE10_PROMPT ((char*)-1)

#define SSH_EXEC     "/usr/bin/ssh"

/*CLI Session Types*/
enum CLISessionType {
	CLI_NONE = 0,
	CLI_TELNET = 1,
	CLI_SSH = 2
};

#endif
