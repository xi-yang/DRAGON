/****************************************************************************

  KOM RSVP Engine (release version 3.0f)
  Copyright (C) 1999-2004 Martin Karsten

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
#include "RSVP_SignalHandling.h"

#include "SystemCallCheck.h"
#include "RSVP_TimeValue.h"

#include <signal.h>

SignalHandling::SigHandler SignalHandling::alarmHandler = SigHandler(0);
SignalHandling::SigHandler SignalHandling::exitHandler = SigHandler(0);
SignalHandling::SigHandler SignalHandling::userHandler = SigHandler(0);
SignalHandling::SigHandler SignalHandling::userHandler2 = SigHandler(0);
void* SignalHandling::clientData = (void*)0;
struct sigaction SignalHandling::sigAction;
bool SignalHandling::userSignal = false;

void SignalHandling::internal_SigHandler( int signum ) {
	switch (signum ) {
	case SIGHUP:
	case SIGINT:
	case SIGQUIT:
	case SIGTERM:
		userSignal = false;
		exitHandler( clientData );
		printSafe("caught exit signal, exiting...\n");
		break;
	case SIGPIPE:
		userSignal = false;
		exitHandler( clientData );
		printSafe("ignoring SIGPIPE signal\n");
		break;
	case SIGALRM:
		userSignal = false;
		alarmHandler( clientData );
		break;
	case SIGUSR1:
		userSignal = true;
		userHandler( clientData );
		break;
	case SIGUSR2:
		userSignal = true;
		userHandler2( clientData );
		break;
	}
}

void SignalHandling::wait( bool setTimeout, TimeValue& waitTime ) {
	if (setTimeout) {
		select( 0, NULL, NULL, NULL, &waitTime );
	} else {
		select( 0, NULL, NULL, NULL, NULL );
	}
}

void SignalHandling::install( SigHandler exitHandler, SigHandler alarmHandler, void* clientData ) {
	SignalHandling::exitHandler = exitHandler;
	SignalHandling::alarmHandler = alarmHandler;
	SignalHandling::clientData = clientData;
	memset( &sigAction, 0, sizeof(sigAction) );
	sigemptyset( &sigAction.sa_mask );
	if (exitHandler) {
		sigAction.sa_handler = internal_SigHandler;
#if !defined(__APPLE__)
//		sigAction.sa_flags = SA_RESETHAND;
#endif
		struct sigaction oldAction;
		CHECK( sigaction( SIGHUP, &sigAction, &oldAction ) );
		if ( oldAction.sa_handler != SIG_DFL && oldAction.sa_handler != SIG_IGN ) {
			cerr << "can't establish SIGHUP handling" << endl;
			CHECK( sigaction( SIGHUP, &oldAction, NULL ) );
		}
		CHECK( sigaction( SIGINT, &sigAction, &oldAction ) );
		if ( oldAction.sa_handler != SIG_DFL && oldAction.sa_handler != SIG_IGN ) {
			cerr << "can't establish SIGINT handling" << endl;
			CHECK( sigaction( SIGINT, &oldAction, NULL ) );
		}
		CHECK( sigaction( SIGQUIT, &sigAction, &oldAction ) );
		if ( oldAction.sa_handler != SIG_DFL && oldAction.sa_handler != SIG_IGN ) {
			cerr << "can't establish SIGQUIT handling" << endl;
			CHECK( sigaction( SIGQUIT, &oldAction, NULL ) );
		}
		CHECK( sigaction( SIGTERM, &sigAction, &oldAction ) );
		if ( oldAction.sa_handler != SIG_DFL && oldAction.sa_handler != SIG_IGN ) {
			cerr << "can't establish SIGTERM handling" << endl;
			CHECK( sigaction( SIGTERM, &oldAction, NULL ) );
		}
		sigAction.sa_flags = 0;
		CHECK( sigaction( SIGPIPE, &sigAction, &oldAction ) );
		if ( oldAction.sa_handler != SIG_DFL && oldAction.sa_handler != SIG_IGN ) {
			cerr << "can't establish SIGPIPE handling" << endl;
			CHECK( sigaction( SIGPIPE, &oldAction, NULL ) );
		}
	}
	if ( alarmHandler ) {
		sigAction.sa_handler = internal_SigHandler;
//		sigAction.sa_flags = SA_RESTART;
		CHECK( sigaction( SIGALRM, &sigAction, NULL ) );
	}
}

void SignalHandling::installUserSignal( SigHandler userHandler, SigHandler userHandler2 ) {
	SignalHandling::userHandler = userHandler;
	SignalHandling::userHandler2 = userHandler2;
	memset( &sigAction, 0, sizeof(sigAction) );
	sigemptyset( &sigAction.sa_mask );
	sigAction.sa_handler = internal_SigHandler;
	if ( userHandler ) {
		sigAction.sa_flags = SA_RESTART;
		CHECK( sigaction( SIGUSR1, &sigAction, NULL ) );
		if ( userHandler2 ) {
			sigAction.sa_flags = SA_RESTART;
			CHECK( sigaction( SIGUSR2, &sigAction, NULL ) );
		}
	}
}
