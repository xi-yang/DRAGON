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
#include <signal.h>

RSVP_API* theAPI = NULL;

static bool end = false;
static void exitHandler( int ) {
	end = true;
}

void makeReservation( const GenericUpcallParameter& upcallPara ) {
	const SENDER_TSPEC_Object& sentTSpec = upcallPara.pathEvent->sendTSpec;
	const ADSPEC_Object* adspec = upcallPara.pathEvent->adSpec;
	FLOWSPEC_Object* flowspec = NULL;
	ieee32float R = 0;

	if (sentTSpec.getService() == SENDER_TSPEC_Object::SonetSDH_Sender_Tspec)
		flowspec = new FLOWSPEC_Object((const SONET_TSpec&)sentTSpec);
	else{
		if ( adspec && adspec->supportsGS() ) {
			R = sentTSpec.calculateRate( adspec->getAdSpecGSParameters().getTotError(), 300000 );
		}
		if ( R != 0 ) {
			flowspec = new FLOWSPEC_Object( (const TSpec&)sentTSpec, RSpec( R, 0 ) );
		} else {
			flowspec = new FLOWSPEC_Object( (const TSpec&)sentTSpec );
		}
	}
	FlowDescriptorList fdList;
	fdList.push_back( flowspec );
	fdList.back().filterSpecList.push_back( upcallPara.pathEvent->senderTemplate );
	theAPI->createReservation( upcallPara.pathEvent->session, true, FF, fdList);
}

static void upcall( const GenericUpcallParameter& upcallPara ) {
	cout << "***** UPCALL *****" << endl;
	switch( upcallPara.generalInfo->infoType ) {
		case UpcallParameter::PATH_EVENT:
			cout << "PATH_EVENT: " << *upcallPara.pathEvent << endl;
			makeReservation( upcallPara );
			break;
		case UpcallParameter::RESV_EVENT:
			cout << "RESV_EVENT: " << *upcallPara.resvEvent << endl;
			break;
		case UpcallParameter::PATH_TEAR:
			cout << "PATH_TEAR: " << *upcallPara.pathTear << endl;
			break;
		case UpcallParameter::RESV_TEAR:
			cout << "RESV_TEAR: " << *upcallPara.resvTear << endl;
			break;
		case UpcallParameter::PATH_ERROR:
			cout << "PATH_ERROR: " << *upcallPara.pathError << endl;
			break;
		case UpcallParameter::RESV_ERROR:
			cout << "RESV_ERROR: " << *upcallPara.resvError << endl;
			break;
		case UpcallParameter::RESV_CONFIRM:
			cout << "RESV_CONFIRM: " << *upcallPara.resvConfirm << endl;
			break;
		default:
			cerr << "upcall with unknown info type" << endl;
			break;
	}
}

static void usage( const char* arg0 ) {
	//cerr << "usage: " << arg0 << " session-addr port [RSVPD-host] [RSVPD-port]" << endl;
	cerr << "usage: " << arg0 << " dest_control_addr dest_port" << endl;
	exit(1);
}

int main( int argc, char** argv ) {
	//if ( argc != 3 ) {
		//usage( argv[0] );
	//}
	struct sigaction sa;
	sa.sa_handler = exitHandler;
	sa.sa_flags = 0;
	CHECK( sigaction( SIGHUP, &sa, NULL ) );
	CHECK( sigaction( SIGINT, &sa, NULL ) );
	CHECK( sigaction( SIGQUIT, &sa, NULL ) );  
	CHECK( sigaction( SIGTERM, &sa, NULL ) );

        uint8 prefix = 32;
        String srcAddr = "1.1.1.5";
        uint16 srcPort = 2000;
        uint32 srcIfID = 1;

        String gsfcAddr1 = "10.1.50.10";
        String gsfcAddr2 = "10.1.32.10";
        uint32 gsfcIfID = 102;

        String iWSSAddr1 = "10.1.32.9";
        String iWSSAddr2 = "10.1.32.5";

        String arlgAddr1 = "10.1.32.6";
        String arlgAddr2 = "10.1.50.6";
        uint32 arlgIfID = 9;

        String dstAddr = "1.1.1.6";
        uint16 dstPort = 3000;
        uint32 dstIfID = 1;
        uint32 rate = TSpec::R_Gig_E_OverFiber;

        String rsvpdHost = "127.0.0.1";
        uint16 rsvpdPort = 0;

	
	Log::init( "all", "ref,packet,select" );
	RSVP_API api( rsvpdPort, rsvpdHost );
	theAPI = &api;
	InterfaceHandle fd = -1;
	if ( NetAddress(dstAddr).isMulticast() ) {
		fd = socket( AF_INET, SOCK_DGRAM, 0 );
		const int on = 1;
		CHECK( setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on) ));
		struct sockaddr_in if_addr;
		memset( &if_addr.sin_zero, 0, sizeof(if_addr.sin_zero) );
		if_addr.sin_family = AF_INET;
		if_addr.sin_port = htons(dstPort);
		if_addr.sin_addr.s_addr = NetAddress(dstAddr).rawAddress();
#if defined(__FreeBSD__)
		if_addr.sin_len = sizeof( if_addr );
#endif
	 	CHECK( bind( fd, (struct sockaddr*)&if_addr, sizeof(if_addr) ) );
		ip_mreq mreq;
		mreq.imr_multiaddr.s_addr = NetAddress(dstAddr).rawAddress();
		mreq.imr_interface.s_addr = INADDR_ANY;
		CHECK( setsockopt( fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq) ));
		cout << "joined mcast group " << dstAddr << endl;
	}
        RSVP_API::SessionId session = api.createSession( NetAddress(dstAddr), dstPort, NetAddress(srcAddr).rawAddress(), (UpcallProcedure)upcall );
	int fd_api = api.getFileDesc();
	while (!end) {
		fd_set readfds;
		FD_ZERO( &readfds );
		FD_SET( fd_api, &readfds );
		int retval = select( fd_api + 1, &readfds, NULL, NULL, NULL );
		if ( retval > 0 && FD_ISSET( fd_api, &readfds )) {
			api.receiveAndProcess();
		}
	}  
	api.releaseSession( session );
	if ( NetAddress(dstAddr).isMulticast() ) {
		ip_mreq mreq;
		mreq.imr_multiaddr.s_addr = NetAddress(dstAddr).rawAddress();
		mreq.imr_interface.s_addr = INADDR_ANY;
		CHECK( setsockopt( fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq) ));
		cout << "left mcast group " << dstAddr << endl;
		close( fd );
	}
}
