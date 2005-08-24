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
#include "RSVP_NetworkService.h"

#include <iostream>
#include <fstream>

#include "common.h"

static bool endFlag = true;

static void exitHandler( int ) {
	if (endFlag) exit(0);
	endFlag = true;
}

RSVP_API* api = NULL;

void makeReservation( const GenericUpcallParameter& upcallPara ) {
	FLOWSPEC_Object* flowspec = new FLOWSPEC_Object( upcallPara.pathEvent->sendTSpec, RSpec( upcallPara.pathEvent->sendTSpec.get_r(), 0 ) );
	FlowDescriptorList fdList;
	fdList.push_back( flowspec );
	fdList.back().filterSpecList.push_back( upcallPara.pathEvent->senderTemplate );
	api->createReservation( upcallPara.pathEvent->session, false, FF, fdList );
}

static void upcall( const GenericUpcallParameter& upcallPara ) {
//	cout << "***** UPCALL *****" << endl;
	switch( upcallPara.generalInfo->infoType ) {
		case UpcallParameter::PATH_EVENT:
//			cout << "PATH_EVENT: " << *upcallPara.pathEvent << endl;
			makeReservation( upcallPara );
			break;
		case UpcallParameter::RESV_EVENT:
//			cout << "RESV_EVENT: " << *upcallPara.resvEvent << endl;
			break;
		case UpcallParameter::PATH_TEAR:
//			cout << "PATH_TEAR: " << *upcallPara.pathTear << endl;
			break;
		case UpcallParameter::RESV_TEAR:
//			cout << "RESV_TEAR: " << *upcallPara.resvTear << endl;
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

int main( int argc, char** argv ) {
	if ( argc < 4 ) {
		cerr << "usage: " << argv[0] << " <session-addr> <portnum-offset> <max-sessions> [log]" << endl;
		exit(1);
	}
	NetAddress sessionAddr( argv[1] );
	uint16 portnumOffset = convertStringToInt( argv[2] );
	uint32 maxSessions = convertStringToInt( argv[3] );
	RSVP_API::SessionId* sessions = new RSVP_API::SessionId[maxSessions+1];
	initMemoryWithZero( sessions, sizeof( RSVP_API::SessionId[maxSessions+1] ) );

	Log::init();
	if ( argc > 4 ) {
		Log::init( "all", "ref,packet,select" );
	}
	api = new RSVP_API( "", maxSessions );

	installExitHandler( exitHandler );

	int fd = -1;
	if ( sessionAddr.isMulticast() ) {
		fd = socket( AF_INET, SOCK_DGRAM, 0 );
		NetworkService::joinMCastGroupIP4( fd, sessionAddr );
	}

	static const uint32 sessionReportDivisor = 20;
	uint32 sessionReport = (maxSessions > sessionReportDivisor) ? maxSessions/sessionReportDivisor : maxSessions + 1;
	uint32 i = 0, xxx = 1;
	for ( ; i < maxSessions; ++i, ++xxx ) {
		sessions[i] = api->createSession( sessionAddr, 17, portnumOffset + i, (UpcallProcedure)upcall );
		if ( i >= sessionReport ) {
			cout << "registered " << sessionReport << " sessions" << endl;
			sessionReport += maxSessions/sessionReportDivisor;
		}
		// slow down session ramp up: otherwise, the local RSVPD gets in trouble
		if ( xxx == 25 ) {
			usleep(1000);
			xxx = 1;
		}
	}
	endFlag = false;
	cout << "all receiving sessions registered" << endl;

	while (!endFlag) {
		if (NetworkService::waitForPacket( api->getFileDesc(), false )) {
			api->receiveAndProcess();
		}
	}
	endFlag = false;

	cout << endl;
	sessionReport = (maxSessions > sessionReportDivisor) ? maxSessions/sessionReportDivisor : maxSessions + 1;
	for ( i = 0, xxx = 1; i < maxSessions && !endFlag; ++i, ++xxx ) {
		if ( sessions[i] != (RSVP_API::SessionId)0 ) api->releaseSession( sessions[i] );
		if ( i >= sessionReport ) {
			cout << "removed " << sessionReport << " sessions" << endl;
			sessionReport += maxSessions/sessionReportDivisor;
		}
		// slow down session removal: otherwise, the local RSVPD gets in trouble
		if ( xxx == 10 ) { usleep(10000); xxx = 1; }
	}
	if ( sessionAddr.isMulticast() ) {
		NetworkService::leaveMCastGroupIP4( fd, sessionAddr );
		close( fd );
	}

	delete api;
	delete [] sessions;
}
