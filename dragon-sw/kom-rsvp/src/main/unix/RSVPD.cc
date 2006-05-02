/****************************************************************************

  KOM RSVP Engine (release version 3.0f)
  Copyright (C) 2000, 2001 Martin Karsten

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  Contact:	Martin Karsten
		TU Darmstadt, FG KOM
		Merckstr. 25
		64283 Darmstadt
		Germany
		Martin.Karsten@KOM.tu-darmstadt.de

  Other copyrights might apply to parts of this package and are so
  noted when applicable. Please see file COPYRIGHT.other for details.

****************************************************************************/
#include "RSVP.h"
#include "RSVP_Log.h"
#include "SwitchCtrl_Global.h"
//#include "SNMP_Session.h"
//#include "CLI_Session.h"
#include <fcntl.h>
#include <sys/stat.h>

void usage( const char* program ) {
	cout << "usage: " << program << " [options]" << endl;
	cout << endl;
	cout << "Option list:" << endl;
	cout << "-h, -?                      print this help" << endl;
	cout << "-d			 	  runs RSVP in daemon mode" << endl;
	cout << "-c configfile               specifiy configuration file" << endl;
	cout << "-o output file              write logging output into file" << endl;
	cout << "-l loglevel,loglevel,...    enable given list of loglevels" << endl;
	cout << "-L loglevel,loglevel,...    start from 'all' and exclude listed loglevels" << endl;
	cout << "for loglevels, choose from:" << endl;
	Log::usage( cout );
	cout << endl;
}

/* Daemonize myself. */
int
daemon (int nochdir, int noclose)
{
  pid_t pid;

  pid = fork ();

  /* In case of fork is error. */
  if (pid < 0)
	{
	  perror ("fork");
	  return -1;
	}

  /* In case of this is parent process. */
  if (pid != 0)
	exit (0);

  /* Become session leader and get pid. */
  pid = setsid();

  if (pid < -1)
	{
	  perror ("setsid");
	  return -1;
	}

  /* Change directory to root. */
  if (! nochdir)
	chdir ("/");

  /* File descriptor close. */
  if (! noclose)
	{
	  int fd;

	  fd = open ("/dev/null", O_RDWR, 0);
	  if (fd != -1)
	{
	  dup2 (fd, STDIN_FILENO);
	  dup2 (fd, STDOUT_FILENO);
	  dup2 (fd, STDERR_FILENO);
	  if (fd > 2)
		close (fd);
	}
	}

  umask (0027);

  return 0;
}

int main( int argc, char** argv ) {
	const char* logstring_enable = "all";
	const char* logstring_disable = "ref,packet,select";
	const char* logfile = "";
	const char* configfile = "/usr/local/etc/RSVPD.conf";
	int daemonize = 0;
	for (;;) {
		int option = getopt( argc, argv, "?hdc:l:L:o:" );
		if ( option == -1 ) {
	break;
		}
		switch(option) {
		case 'c':
			configfile = optarg;
			break;
		case 'l':
			logstring_enable = optarg;
			logstring_disable = "";
			break;
		case 'L':
			logstring_enable = "all";
			logstring_disable = optarg;
			break;
		case 'o':
			logfile = optarg;
			break;
		case 'd':
			daemonize = 1;
			break;
		default:
			usage( argv[0] );
			return 0;
		}
	}
	if (daemonize)
		daemon(0, 0);
	
#if defined(REAL_NETWORK)
	if ( geteuid() != 0 ) {
		cerr << "you must be root to execute this program" << endl;
		return 1;
	}
	ostream *pidfile = new ofstream( PIDFILE );
	if ( pidfile->good() ) {
		*pidfile << getpid() << endl;
	}
	delete pidfile;
#endif
	Log::init( logstring_enable, logstring_disable, logfile );
	RSVP* rsvp = new RSVP( configfile);
	SwitchCtrl_Global* controller = &SwitchCtrl_Global::instance();
	RSVP_Global::switchController = controller;
	if ( rsvp->properInit() ) rsvp->main();
	delete rsvp;
	delete controller;
	Log::close();
#if defined(REAL_NETWORK)
	unlink( PIDFILE );
#endif
	return 0;
}
