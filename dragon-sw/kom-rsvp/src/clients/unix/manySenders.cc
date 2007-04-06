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
#include "RSVP_API.h"
#include "RSVP_API_Upcall.h"

#include <iostream>
#include <fstream>
#include <termios.h>

#include "common.h"

struct ApiSession {
	RSVP_API::SessionId id;
	bool reservation;
public:
	ApiSession() : id(0), reservation(false) {}
};

uint32 maxSessions = 0;
uint32 sessionReport = 0;
static const uint32 sessionReportDivisor = 20;
ApiSession* sessions = NULL;
uint16 portnumOffset = 1;
uint16 nextCreateSession = 0;
uint16 nextDeleteSession = 0;
uint16 activeSessions = 0;
uint16 activeReservations = 0;
RSVP_API* api = NULL;
NetAddress sessionAddr;
TimeValue currentTime, startTime;

TSpec tb1((uint32)1000,(uint32)0,(uint32)1000,100,100);

static bool endFlag = false;
static bool outFlag = false;
static void exitHandler( int ) {
	endFlag = true;
}

static void upcall( const GenericUpcallParameter& upcallPara ) {
//	cout << "***** UPCALL *****" << endl;
	uint16 index = 0;
	switch( upcallPara.generalInfo->infoType ) {
		case UpcallParameter::PATH_EVENT:
//			cout << "PATH_EVENT: " << *upcallPara.pathEvent << endl;
			break;
		case UpcallParameter::RESV_EVENT:
//			cout << "RESV_EVENT: " << *upcallPara.resvEvent << endl;
			index = (*upcallPara.resvEvent->session)->getTunnelId() - portnumOffset;
			if ( sessions[index].id != (RSVP_API::SessionId)0 && !sessions[index].reservation ) {
				sessions[index].reservation = true;
				activeReservations += 1;
//				cerr << "current number of reservations:" << activeReservations << endl;
			}
			break;
		case UpcallParameter::PATH_TEAR:
//			cout << "PATH_TEAR: " << *upcallPara.pathTear << endl;
			break;
		case UpcallParameter::RESV_TEAR:
//			cout << "RESV_TEAR: " << *upcallPara.resvTear << endl;
			index = (*upcallPara.resvTear->session)->getTunnelId() - portnumOffset;
			if ( sessions[index].id != (RSVP_API::SessionId)0 && sessions[index].reservation ) {
				sessions[index].reservation = false;
				activeReservations -= 1;
//				cerr << "current number of reservations:" << activeReservations << endl;
			}
			break;
		case UpcallParameter::PATH_ERROR:
			cout << "PATH_ERROR: " << *upcallPara.pathError << endl;
			break;
		case UpcallParameter::RESV_ERROR:
			cout << "RESV_ERROR: " << *upcallPara.resvError << endl;
			break;
		case UpcallParameter::RESV_CONFIRM:
//			cout << "RESV_CONFIRM: " << *upcallPara.resvConfirm << endl;
			break;
		default:
			cerr << "upcall with unknown info type" << endl;
			break;
	}
}

void createSender() {
	if ( sessions[nextCreateSession].id != (RSVP_API::SessionId)0 ) return;
	sessions[nextCreateSession].id = api->createSession( sessionAddr, 17, portnumOffset + nextCreateSession, (UpcallProcedure)upcall );
	//api->createSender( sessions[nextCreateSession].id, portnumOffset + nextCreateSession, tb1, 50, NULL, NULL );
	nextCreateSession += 1;
	if ( nextCreateSession >= maxSessions ) {
		if ( !outFlag ) {
			cout << "reaching full load at " << (DaytimeTimeValue&)currentTime << endl;
			cout << "average session inter-arrival time ";
#if defined(REFRESH_REDUCTION)
			cout << (currentTime-startTime) / (nextCreateSession-1) << endl;
#else
			cout << (currentTime-startTime) / nextCreateSession << endl;
#endif
			outFlag = true;
		}
		nextCreateSession = 0;
	}
	if ( nextCreateSession >= sessionReport && !outFlag ) {
		cout << "created " << nextCreateSession << " sessions at " << (DaytimeTimeValue&)currentTime << endl;
		sessionReport += maxSessions / sessionReportDivisor;
	}
	activeSessions += 1;
}

void deleteSender() {
	if ( sessions[nextDeleteSession].id == (RSVP_API::SessionId)0 ) return;
	api->releaseSession( sessions[nextDeleteSession].id );
	sessions[nextDeleteSession].id = (RSVP_API::SessionId)0;
	if ( sessions[nextDeleteSession].reservation ) {
		sessions[nextDeleteSession].reservation = false;
		activeReservations -= 1;
//		cerr << "current number of reservations:" << activeReservations << endl;
	}
	nextDeleteSession += 1;
	if ( nextDeleteSession >= maxSessions ) {
		nextDeleteSession = 0;
	}
	activeSessions -= 1;
}

inline TimeValue calcNextRandomTime( uint32 maxIn_usec ) {
#if defined(FIXED_TIMEOUTS)
	uint32 nextTime_usec = maxIn_usec / 2;
#else
	uint32 nextTime_usec = drawRandomNumber( maxIn_usec );
#endif	
	return TimeValue( nextTime_usec / USECS_PER_SEC, nextTime_usec % USECS_PER_SEC );
}

int main( int argc, char** argv ) {
	if ( argc < 5 ) {
		cerr << "usage: " << argv[0] << " <session-addr> <portnum-offset> <max-sessions> <max-create-delay in msec> [log]" << endl;
		exit(1);
	}

	installExitHandler( exitHandler );

	sessionAddr = NetAddress(argv[1]);
	portnumOffset = convertStringToInt( argv[2] );
	maxSessions = convertStringToInt( argv[3] );
	if ( maxSessions > sessionReportDivisor ) {
		// report periodically about creation progress
		sessionReport = maxSessions / sessionReportDivisor;
	} else {
		// don't report
		sessionReport = maxSessions + 1;
	}
	uint32 maxCreateDelay = (uint32)(convertStringToFloat( argv[4] ) * 1000);
	sessions = new ApiSession[maxSessions+1];

	Log::init();
	if ( argc > 5 ) {
		Log::init( "all", "ref,packet,select" );
	}
	api = new RSVP_API( "", maxSessions );

	getCurrentSystemTime( currentTime );

	TimeValue nextCreateTime = currentTime + calcNextRandomTime( maxCreateDelay );
	TimeValue nextDeleteTime = currentTime + calcNextRandomTime( maxCreateDelay );
	bool deleteSessions = false;

	struct termios save_termios, new_termios;
	CHECK( tcgetattr( STDIN_FILENO, &save_termios ) );
	new_termios = save_termios;
	new_termios.c_lflag &= ~(ICANON | ECHO);
	CHECK( tcsetattr( STDIN_FILENO, TCSANOW, &new_termios ) );
	fd_set fdSet;
	bool pause = false;

	cout << "starting at " << (DaytimeTimeValue&)currentTime << endl;
	startTime = currentTime;

#if defined(REFRESH_REDUCTION)
	// create one sender and wait for keystroke, such that dynamic node
	// detection does not influence these experiments
	createSender();
	cout << "created first sender, press any key to continue" << endl;
	pause = true;
	bool start = true;
#endif

	while ( !endFlag ) {
		FD_ZERO( &fdSet );
		FD_SET( STDIN_FILENO, &fdSet );
		TimeValue zeroTime(0,0);
		if ( select( STDIN_FILENO + 1, &fdSet, NULL, NULL, &zeroTime ) ) {
			char buffer;
			read( STDIN_FILENO, &buffer, 1 );
			if ( buffer == 'P' || buffer == 'p' ) {
				cout << "Pausing. Press any key to continue." << endl;
				pause = true;
			} else if ( buffer == 'Q' || buffer == 'q' ) {
				cout << "bye bye..." << endl;
				endFlag = true;
			} else {
				cout << "Creating/deleting sessions. Press 'p' for pausing." << endl;
#if defined(REFRESH_REDUCTION)
				if (start) {
					start = false;
					cout << "real start at " << (DaytimeTimeValue&)currentTime << endl;
					startTime = currentTime;
				}
#endif
				pause = false;
			}
		}
		getCurrentSystemTime( currentTime );
		if ( !deleteSessions || nextCreateTime <= nextDeleteTime ) {
			api->sleep( nextCreateTime - currentTime, endFlag );
			nextCreateTime += calcNextRandomTime( maxCreateDelay );
			if ( !pause ) createSender();
		} else {
			api->sleep( nextDeleteTime - currentTime, endFlag );
			nextDeleteTime += calcNextRandomTime( maxCreateDelay );
			if ( !pause ) deleteSender();
		}
		if ( activeSessions >= maxSessions - 5 ) {
			deleteSessions = true;
		} else if ( activeSessions < maxSessions - 10 ) {
			deleteSessions = false;
		}
//		cerr << "current number of active sessions:" << activeSessions << endl;
		static bool out13 = true;
		if ( out13 && (currentTime - startTime > TimeValue(780,0)) ) {
			out13 = false;
			cout << "running for 13 minutes now: " << (DaytimeTimeValue&)currentTime << endl;
		}
	}
	endFlag = false;

	cout << endl;
	uint32 i = 0, xxx = 1;
	uint32 sessionReport = (maxSessions > sessionReportDivisor) ? maxSessions/sessionReportDivisor : maxSessions + 1;
	for ( ; i < maxSessions && !endFlag; ++i, ++xxx ) {
		if ( sessions[i].id != (RSVP_API::SessionId)0 ) api->releaseSession( sessions[i].id );  
		if ( i >= sessionReport ) {
			cout << "removed " << sessionReport << " sessions" << endl;
			sessionReport += maxSessions/sessionReportDivisor;
		}
		// slow down session removal: otherwise, the local RSVPD gets in trouble
		if ( xxx == 10 ) { usleep(10000); xxx = 1; }
	}

	CHECK( tcsetattr( STDIN_FILENO, TCSANOW, &save_termios ) );

	delete api;
	delete [] sessions;
}
