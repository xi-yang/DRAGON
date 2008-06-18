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

static TimeValue pathTime;

static bool end = false;
static void exitHandler( int ) {
	end = true;
}
  
static void upcall( const GenericUpcallParameter& upcallPara ) {
	TimeValue resvTime;
	TimeValue setupTime;
	cout << "***** UPCALL *****" << endl;
	switch( upcallPara.generalInfo->infoType ) {
		case UpcallParameter::PATH_EVENT:
			cout << "PATH_EVENT: " << *upcallPara.pathEvent << endl;
			break;
		case UpcallParameter::RESV_EVENT:
			cout << "RESV_EVENT: " << *upcallPara.resvEvent << endl;
			getCurrentSystemTime(resvTime);
			setupTime = resvTime - pathTime;
			cout << "setup time was " << setupTime.tv_sec << ":"
					<< setw(6) << setupTime.tv_usec << " seconds" << endl;
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

/*
static void usage( const char* arg0 ) {
	//cerr << "usage: " << arg0 << " session-addr port [src-port] [rate in byte/s] [RSVPD-host] [RSVPD-port]" << endl;
	cerr << "usage: " << arg0 << " no-parameter" << endl;
	exit(1);
}
*/

int main( int argc, char** argv ) {
	//if ( argc < 7 ) {
	//	usage( argv[0] );
	//}
	struct sigaction sa;
	sigemptyset( &sa.sa_mask );
	sa.sa_handler = exitHandler;
	sa.sa_flags = 0;
	CHECK( sigaction( SIGHUP, &sa, NULL ) );
	CHECK( sigaction( SIGINT, &sa, NULL ) );
	CHECK( sigaction( SIGQUIT, &sa, NULL ) );  
	CHECK( sigaction( SIGTERM, &sa, NULL ) );

	/*String vlsrAddr = "130.85.189.3";
	struct _vlsr_route_{
		String vlsrEthSwAddr;
		uint32 vlsrSrcIfID;
		uint32 vlsrDstIfID;
	} vlsr_route[3] = {
		{"130.85.189.18", 3, 2},
		{"130.85.189.7", 2, 3},
		{"130.85.189.8", 3, 2}
	};*/

        uint8 prefix = 32;
        String srcAddr = "10.1.50.9";
        uint16 srcPort = 5000;
        /* uint32 srcIfID = 1; */

        String gsfcAddr1 = "10.1.50.10";
        String gsfcAddr2 = "10.1.32.10";
        /* uint32 gsfcIfID = 102; */

        String iWSSAddr1 = "10.1.32.9";
        String iWSSAddr2 = "10.1.32.5";

        String arlgAddr1 = "10.1.32.6";
        String arlgAddr2 = "10.1.50.6";
        /* uint32 arlgIfID = 9; */

        String dstAddr = "10.1.50.5";
        uint16 dstPort = 6000;
        /* uint32 dstIfID = 1; */
        uint32 rate = TSpec::R_Gig_E_OverFiber;

        String rsvpdHost = "127.0.0.1";
        uint16 rsvpdPort = 0;



        Log::init( "all", "ref,packet,select" );
        RSVP_API api( rsvpdPort, rsvpdHost ) ;
        getCurrentSystemTime(pathTime);
        RSVP_API::SessionId session = api.createSession( NetAddress(dstAddr), dstPort, ntohl(NetAddress(srcAddr).rawAddress()), (UpcallProcedure)upcall );

        ADSPEC_Object* ao = new ADSPEC_Object( 0, 100000000, 85000, 1500 );
        AdSpecCLParameters cl;
        cl.override.setHopCount(12);
        cl.override.setMinPathLatency(100);
        ao->addCL( cl );
        AdSpecGSParameters gs( 1500, 10000, 1500, 10000 );
        gs.override.setMinPathLatency(120);
        ao->addGS( gs );
        
        TSpec tb1(rate,rate,rate,100,1500);
        SENDER_TSPEC_Object stb(tb1);
       //***************************************
       // To support SONET/SDH circuit setup: 
       //		(a) Activate the following two commented lines. 
       //		(b) Comment the corresponding two lines above
       // 		(c) Input your own SONET/SDH TSpec parameters
       // SONET_TSpec sonet_tb1(1, 1, 1, 1, 1, 1, 1); 
       // SENDER_TSPEC_Object stb(sonet_tb1);
       //***************************************

       EXPLICIT_ROUTE_Object* ero = new EXPLICIT_ROUTE_Object();

        ero->pushBack(AbstractNode(false, NetAddress(srcAddr), prefix));
       ero->pushBack(AbstractNode(false, NetAddress(gsfcAddr1), (uint32)0x10A002));
       ero->pushBack(AbstractNode(false, NetAddress(gsfcAddr2), (uint8)32));
        //ero->pushBack(AbstractNode(false, NetAddress(iWSSAddr1), (uint32)2));
        ero->pushBack(AbstractNode(false, NetAddress(iWSSAddr2), (uint8)32));
        ero->pushBack(AbstractNode(false, NetAddress(arlgAddr1), (uint8)32));
        //ero->pushBack(AbstractNode(false, NetAddress(arlgAddr1), (uint32)0x10A002));
        ero->pushBack(AbstractNode(false, NetAddress(arlgAddr2), (uint32)0x10A002));
        //ero->pushBack(AbstractNode(false, NetAddress(arlgAddr2), (uint8)32));
        ero->pushBack(AbstractNode(false, NetAddress(dstAddr), prefix));

        LABEL_SET_Object* labelSet = new LABEL_SET_Object();
        labelSet->addSubChannel(193300);
        labelSet->addSubChannel(194700);
        labelSet->addSubChannel(193100);
        labelSet->addSubChannel(1089538);

	SESSION_ATTRIBUTE_Object* ssAttrib = new SESSION_ATTRIBUTE_Object(String("DRAGON"));
	UPSTREAM_LABEL_Object* upLabel = new UPSTREAM_LABEL_Object(161252);
       api.createSender( session, srcPort, stb, LABEL_REQUEST_Object(), ero, NULL, NULL, labelSet, ssAttrib, upLabel, 50, NULL, NULL );
	ao->destroy();
	ero->destroy();
	labelSet->destroy();
	delete ssAttrib;
	delete upLabel;
	int fd = api.getFileDesc();
	while (!end) {
		fd_set readfds;
		FD_ZERO( &readfds );
		FD_SET( fd, &readfds );
		int retval = select( fd + 1, &readfds, NULL, NULL, NULL );
		if ( retval > 0 && FD_ISSET( fd, &readfds )) {
			api.receiveAndProcess();
		}
	}  
	// not needed at program end
	// api.releaseSession( session );
}
