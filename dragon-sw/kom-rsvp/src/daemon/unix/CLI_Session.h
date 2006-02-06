/****************************************************************************

CLI Based Switch Control Module header file CLI_Session.h
Created by Xi Yang @ 01/17/2006
Extended from SNMP_Global.h by Aihua Guo and Xi Yang, 2004-2005
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#ifndef _CLI_SESSION_H_
#define _CLI_SESSION_H_

#include "SwitchCtrl_Global.h"
#include <signal.h>

//CLI Session Types
enum CLISessionType {
	CLI_NONE = 0,
	CLI_TELNET = 1,
	CLI_SSH = 2
};

extern char  progname[100];
extern char  hostname[100];
extern pid_t pid;
extern int    got_alarm;

#define LINELEN  256
#define SWITCH_PROMPT ((char*)-1) // a pointer == (-1), indicating that a switch prompt is expected.

#define TELNET_EXEC     "/usr/bin/telnet"
#define TELNET_PORT     "23"
#define TELNET_PROMPT   "telnet> "

#define SSH_EXEC     "/usr/bin/ssh"
#define SSH_PORT     "22"

class CLI_Session: public SwitchCtrl_Session
{
public:
	CLI_Session(): SwitchCtrl_Session() {  fdin = fdout = -1; }
	CLI_Session(const String& sName, const NetAddress& swAddr): SwitchCtrl_Session(sName, swAddr) 
		{ fdin = fdout = -1;  }
	virtual ~CLI_Session() { disconnectSwitch(); }

	virtual bool connectSwitch();
	virtual void disconnectSwitch();
	virtual bool refresh(); //to be called by RSVP_SREFRESH !!!

	bool engage();
	void disengage();
	void stop();

	///////////------QoS Functions ------/////////
	virtual bool policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0) { return false; }
	virtual bool limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0) { return false; }

protected:
	int fdin;
	int fdout;

	inline bool pipeAlive();
	int readShell(char *text1, char *text2, int verbose, int timeout);
	int writeShell(char *text, int timeout, bool echo_back = false);
	virtual bool isSwitchPrompt(char *p, int len);
	virtual bool preAction();
	virtual bool postAction();
};


extern "C" {
    void sigfunct(int signo);
    void sigpipe(int signo);
}

#define DIE_IF_NEGATIVE(X) if ((X)<0) return (false & postAction())
#define DIE_IF_EQUAL(X, Y) if(X==Y) return (false & postAction())

#endif //ifndef _CLI_SESSION_H_

