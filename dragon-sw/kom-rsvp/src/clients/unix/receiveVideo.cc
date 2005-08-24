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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>                        // htonl, htons, ntohl, ntohl
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <termios.h>
#include <unistd.h>

#if defined(HAVE_PTHREAD_H)
#include <pthread.h>
#elif defined(HAVE_PTHREADS_H)
#include <pthreads.h>
#else
#error no thread support on this system
#endif

#define USE_RSVP

#if defined(USE_RSVP)
#include "RSVP_API.h"
#include "RSVP_API_Upcall.h"
#endif

#include "common.h"

static bool end = false;
unsigned short destPort = 0, sourcePort = 0;
unsigned int sourceAddress = INADDR_ANY;
unsigned int destAddress = INADDR_ANY;
unsigned short packetSize = 65535;
struct termios save_termios, new_termios;

#if defined(USE_RSVP)
static FLOWSPEC_Object* flowspec;
static RSVP_API::SessionId rsvpSession = NULL;
#endif

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void killHandler( int ) {
	end = true;
}

#if defined(USE_RSVP)
static void sendReservation( RSVP_API& rsvpAPI ) {
	if ( !rsvpSession ) return;
	FlowDescriptorList fdList;
	FlowDescriptor fDesc;
	fDesc.setFlowspec( flowspec );
	fdList.push_back( fDesc );
	cout << "RESERVING" << endl;
	rsvpAPI.createReservation( rsvpSession, false, WF, fdList );
}
#endif

#if defined(USE_RSVP)
static void rsvpUpcall( const GenericUpcallParameter& upcallPara, RSVP_API* rsvpAPI ) {
	if ( upcallPara.generalInfo->infoType == UpcallParameter::PATH_EVENT ) {
		const TSpec& sentTSpec = upcallPara.pathEvent->sendTSpec;
		flowspec->destroy();
		const ADSPEC_Object* adspec = upcallPara.pathEvent->adSpec;
		if ( adspec && adspec->supportsGS() ) {
			flowspec = new FLOWSPEC_Object( sentTSpec, RSpec( sentTSpec.calculateRate( adspec->getAdSpecGSParameters().getTotError(), 300000 ), 0 ) );
		} else {
			flowspec = new FLOWSPEC_Object( sentTSpec );
		}
		sendReservation( *rsvpAPI );
	}
}
#endif

void* handleKeyboard(void* x) {
	RSVP_API* rsvpAPI = (RSVP_API*)x;
	while ( !end ) {
		char buffer;
		read( STDIN_FILENO, &buffer, 1 );
		switch (buffer) {
		case 'q':
		case 'Q':
			end = true;
		break;
#if defined(USE_RSVP)
		case 'x':
		case 'X':
			if ( !rsvpSession ) {
				pthread_mutex_lock( &mutex );
				rsvpSession = rsvpAPI->createSession( NetAddress(destAddress), IPPROTO_UDP, destPort, (UpcallProcedure)rsvpUpcall, rsvpAPI );
				pthread_mutex_unlock( &mutex );
			}
		break;
		case 'p':
		case 'b':
		case 'r':
		case 'R': {
			CHECK( tcsetattr( STDIN_FILENO, TCSANOW, &save_termios ) );
			cout << "new " << buffer << "? ";
			ieee32float floatvalue;
			cin >> floatvalue;
			CHECK( tcsetattr( STDIN_FILENO, TCSANOW, &new_termios ) );
			switch (buffer) {
			case 'p':
				flowspec->set_p( floatvalue );
				break;
			case 'b':
				flowspec->set_b( floatvalue );
				break;
			case 'r':
				flowspec->set_r( floatvalue );
				break;
			case 'R':
				flowspec->set_R( floatvalue );
				break;
			}
			pthread_mutex_lock( &mutex );
			sendReservation( *rsvpAPI );
			pthread_mutex_unlock( &mutex );
		}	break;
		case 'M':
		case 'm':
		case 'S': {
			CHECK( tcsetattr( STDIN_FILENO, TCSANOW, &save_termios ) );
			cout << "new " << buffer << "? ";
			sint32 intvalue;
			cin >> intvalue;
			CHECK( tcsetattr( STDIN_FILENO, TCSANOW, &new_termios ) );
			switch (buffer) {
			case 'M':
				flowspec->set_M( intvalue );
				break;
			case 'm':
				flowspec->set_m( intvalue );
				break;
			case 'S':
				flowspec->set_S( intvalue );
				break;
			}
			pthread_mutex_lock( &mutex );
			sendReservation( *rsvpAPI );
			pthread_mutex_unlock( &mutex );
		}	break;
		case 'u': 
		case 'U': 
			cout << "UNRESERVING" << endl;
			pthread_mutex_lock( &mutex );
			if (rsvpSession) rsvpAPI->releaseSession( rsvpSession );
			rsvpSession = NULL;
			pthread_mutex_unlock( &mutex );
			break;
#endif
		}
	}
	return NULL;
}

int main( int argc, char** argv ) {

	if ( argc < 4 ) {
		cerr << "usage: " << argv[0] << " source-addr source-port dest-port [dest-addr] [x]" << endl;
		cerr << "If dest-addr is multicast, then source-addr must specify local interface" << endl;
		return 1;
	}

#if defined (__DMALLOC_H__)
	dmalloc_shutdown();
#endif

	int childPid = fork();
	if ( childPid == 0 ) {
		execl( PATH_TO_MTVP, "mtvp", "udp:10220", "-aq2", "-ac0", NULL );
		exit(0);
	}

	sourceAddress = getInetAddr( argv[1] );
	sourcePort = atoi( argv[2] );
	destPort = atoi( argv[3] );
	if ( argc > 4 ) {
		destAddress = getInetAddr( argv[4] );
	}

	installExitHandler( killHandler );

#if defined(USE_RSVP)
	RSVP_API rsvpAPI("");
	flowspec = new FLOWSPEC_Object( TSpec(), RSpec( 0, 0 ) );
	if ( argc > 5 ) {
		rsvpSession = rsvpAPI.createSession( NetAddress(destAddress), IPPROTO_UDP, destPort, (UpcallProcedure)rsvpUpcall, &rsvpAPI );
	}
#endif

	int fd = CHECK( socket( AF_INET, SOCK_DGRAM, 0 ) );
	struct sockaddr_in if_addr;
	memset( &if_addr.sin_zero, 0, sizeof(if_addr.sin_zero) );
	if_addr.sin_family = AF_INET;
	if_addr.sin_port = htons(destPort);
	if_addr.sin_addr.s_addr = destAddress;
#if defined(HAVE_SIN_LEN)
	if_addr.sin_len = sizeof( if_addr );
#endif
	CHECK( bind( fd, (struct sockaddr*)&if_addr, sizeof(if_addr) ) );

	if ( destAddress && ((ntohl(destAddress) >> 28) == 14) ) {
		ip_mreq mreq;
		mreq.imr_multiaddr.s_addr = destAddress;
		mreq.imr_interface.s_addr = sourceAddress;
		CHECK( setsockopt( fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq) ));
	} else {
		memset( &if_addr.sin_zero, 0, sizeof(if_addr.sin_zero) );
		if_addr.sin_family = AF_INET;
		if_addr.sin_port = htons(sourcePort);
		if_addr.sin_addr.s_addr = sourceAddress;
#if defined(HAVE_SIN_LEN)
		if_addr.sin_len = sizeof( if_addr );
#endif
		CHECK( connect( fd, (struct sockaddr*)&if_addr, sizeof(if_addr) ) );
	}

	int send_fd = CHECK( socket( AF_INET, SOCK_DGRAM, 0 ) );
	memset( &if_addr.sin_zero, 0, sizeof(if_addr.sin_zero) );
	if_addr.sin_family = AF_INET;
	if_addr.sin_port = 0;
	if_addr.sin_addr.s_addr = INADDR_ANY;
#if defined(HAVE_SIN_LEN)
	if_addr.sin_len = sizeof( if_addr );
#endif
	CHECK( bind( send_fd, (struct sockaddr*)&if_addr, sizeof(if_addr) ) );

	memset( &if_addr.sin_zero, 0, sizeof(if_addr.sin_zero) );
	if_addr.sin_family = AF_INET;
	if_addr.sin_port = htons(10220);
	if_addr.sin_addr.s_addr = (getInetAddr("localhost"));
#if defined(HAVE_SIN_LEN)
	if_addr.sin_len = sizeof( if_addr );
#endif
	CHECK( connect( send_fd, (struct sockaddr*)&if_addr, sizeof(if_addr) ) );

	CHECK( tcgetattr( STDIN_FILENO, &save_termios ) );
	new_termios = save_termios;
	new_termios.c_lflag &= ~(ICANON | ECHO);
	CHECK( tcsetattr( STDIN_FILENO, TCSANOW, &new_termios ) );

	pthread_t thr;
	pthread_create( &thr, NULL, handleKeyboard, &rsvpAPI );

	cout << "starting port " << destPort << endl;
	char* buffer = new char[packetSize];
	unsigned long receivedBytes = 0;
	unsigned long receivedBytesMAC = 0;
	unsigned long receivedPackets = 0;
	unsigned long failedPackets = 0;
	int received = 0;
	while ( !end ) {
		static fd_set readfds;
		FD_ZERO( &readfds );
		FD_SET( fd, &readfds );
		FD_SET( STDIN_FILENO, &readfds );
#if defined(USE_RSVP)
		FD_SET( rsvpAPI.getFileDesc(), &readfds );
		int retval = select( max(fd,rsvpAPI.getFileDesc()) + 1, &readfds, NULL, NULL, NULL );
#else
		int retval = select( fd + 1, &readfds, NULL, NULL, NULL );
#endif

		if ( retval <= 0 ) {
	continue;
		}
#if defined(USE_RSVP)
		if ( FD_ISSET( rsvpAPI.getFileDesc(), &readfds ) ) {
			pthread_mutex_lock( &mutex );
			rsvpAPI.receiveAndProcess();
			pthread_mutex_unlock( &mutex );
		}
#endif
		if ( FD_ISSET( fd, &readfds ) ) {
			received = recv( fd, buffer, packetSize, 0 );
			if ( received == 1 && buffer[0] == 'E' ) {
				end = true;
				pthread_kill( thr, SIGINT );
			} else if ( received > 0 ) {
				// ignore any errors when sending to mtvp...
				send( send_fd, buffer, received, 0 );
				receivedBytes += received;
				receivedBytesMAC += received + IP_HEADER_SIZE + UDP_HEADER_SIZE;
				receivedPackets += 1;
			} else {
				failedPackets += 1;
			}
		}
	}

	pthread_join( thr, NULL );

#if defined(USE_RSVP)
	if (rsvpSession) rsvpAPI.releaseSession( rsvpSession );
	flowspec->destroy();
#endif

	cout << endl;
	cout << "port " << destPort << " received " << receivedPackets << " packets" << endl;
	cout << "port " << destPort << " received " << receivedBytes << " bytes UDP payload" << endl;
	cout << "port " << destPort << " received " << receivedBytesMAC << " bytes MAC payload" << endl;
	cout << "port " << destPort << " failed " << failedPackets << " packets" << endl;

	close(send_fd);
	CHECK( tcsetattr( STDIN_FILENO, TCSANOW, &save_termios ) );
	kill( childPid, SIGINT );
	return 0;
}
