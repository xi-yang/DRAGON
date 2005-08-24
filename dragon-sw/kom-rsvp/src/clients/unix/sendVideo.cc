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
#include <sys/time.h>
#include <netinet/in.h>                        // htonl, htons, ntohl, ntohl
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <termios.h>
#include <fcntl.h>

#define USE_RSVP

#if defined(USE_RSVP)
#include "RSVP_API.h"
#else
#define USECS_PER_SEC	1000000
#endif

#include "common.h"

#define PRINT_TIME_INTERVAL 5

static bool end = false;
static unsigned short localPort = 0, remotePort = 0;
static unsigned int remoteAddress = 0;
static unsigned short packetSize = 1000;
static unsigned int rate = 10000000;
#if defined(USE_RSVP)
static TSpec tspec;
#endif
static void killHandler( int ) {
	end = true;
}

static void handleKeyboard() {
	char buffer = 0;
	read( STDIN_FILENO, &buffer, 1 );
#if defined(USE_RSVP)
	cout << "current TSpec: " << tspec << endl;
#endif
	switch (buffer) {
#if defined(USE_RSVP)
	case 'p':
	case 'b':
	case 'r': {
		cout << "new " << buffer << "? ";
		ieee32float floatvalue;
		cin >> floatvalue;
		switch (buffer) {
		case 'p':
			tspec.set_p( floatvalue );
			break;
		case 'b':
			tspec.set_b( floatvalue );
			break;
		case 'r':
			tspec.set_r( floatvalue );
			break;
		}
	}	break;
	case 'M':
	case 'm': {
		cout << "new " << buffer << "? ";
		sint32 intvalue;
		cin >> intvalue;
		switch (buffer) { 
		case 'M':
			tspec.set_M( intvalue );
			break;
		case 'm':
			tspec.set_m( intvalue );
			break;
		}
	}	break;
#endif
	case 'q':
	case 'Q':
		end = true;
	}
}

int main( int argc, char** argv ) {
	if ( argc < 5 ) {
		cerr << "usage: " << argv[0] << " remote-addr remote-port local-port filename [packet-size (def:1000bytes)] [rate in bit/s]" << endl;
		return 1;
	}

	remoteAddress = getInetAddr( argv[1] );
	if ( !remoteAddress ) {
		cerr << "cannot find destination: " << argv[1] << endl;
		return 1;
	}

	remotePort = atoi( argv[2] );
	localPort = atoi( argv[3] );

	if ( argc > 5 ) {
		packetSize = atoi( argv[5] );
	}
	if ( argc > 6 ) {
		rate = atoi( argv[6] );
	}
	unsigned int usecPerPacket = USECS_PER_SEC / (rate / (8*packetSize));

	int fd = CHECK( socket( AF_INET, SOCK_DGRAM, 0 ) );

// not needed if underlying TC modules (correctly) dismisses packets
//	CHECK( fcntl( fd, F_SETFL, (fcntl(fd, F_GETFL) | O_NONBLOCK )) );

	struct sockaddr_in if_addr;
	memset( &if_addr.sin_zero, 0, sizeof(if_addr.sin_zero) );
	if_addr.sin_family = AF_INET;
	if_addr.sin_port = htons(localPort);
	if_addr.sin_addr.s_addr = INADDR_ANY;
#if defined(HAVE_SIN_LEN)
	if_addr.sin_len = sizeof( if_addr );
#endif
	CHECK( bind( fd, (struct sockaddr*)&if_addr, sizeof(if_addr) ) );

	memset( &if_addr.sin_zero, 0, sizeof(if_addr.sin_zero) );
	if_addr.sin_family = AF_INET;
	if_addr.sin_port = htons(remotePort);
	if_addr.sin_addr.s_addr = remoteAddress;
#if defined(HAVE_SIN_LEN)
	if_addr.sin_len = sizeof( if_addr );
#endif
	CHECK( connect( fd, (struct sockaddr*)&if_addr, sizeof(if_addr) ) );

	installExitHandler( killHandler );

	// make stdin unbuffered
	struct termios save_termios, new_termios;
	CHECK( tcgetattr( STDIN_FILENO, &save_termios ) );
	new_termios = save_termios;
	new_termios.c_lflag &= ~(ICANON | ECHO);
	CHECK( tcsetattr( STDIN_FILENO, TCSANOW, &new_termios ) );

#if defined(USE_RSVP)
	RSVP_API rsvpAPI("");
	RSVP_API::SessionId rsvpSession = rsvpAPI.createSession( remoteAddress, IPPROTO_UDP, remotePort );
	tspec.set_p( (rate*2.0)/8 );
	// burst size = 0.2 seconds of peak load
	tspec.set_b( (rate*2.0)/8 * 0.2 );
	// add UDP header overhead to rate, then add 10% for smooth demo effect
	tspec.set_r( 1.1 * ((rate*1.0)/8 + (1 + rate/(8*packetSize)) * UDP_HEADER_SIZE) );
	tspec.set_M( 1500 );
	tspec.set_m( packetSize );

	rsvpAPI.createSender( rsvpSession, localPort, tspec, 63 );
#endif

	int read_fd = CHECK( open( argv[4], O_RDONLY ) );
	cout << "sending 1 packet every " << usecPerPacket << " microseconds" << endl;

	char* buffer = new char[packetSize];
	unsigned long sentBytes = 0;
	unsigned long sentBytesMAC = 0;
	unsigned long sentPackets = 0;
	unsigned long failedPackets = 0;
	int nextPrint = PRINT_TIME_INTERVAL;
	timeval nextSend, startTime, endTime;
	timeval currentTime, waitTime;
	CHECK( gettimeofday( &nextSend, NULL ) );
	startTime.tv_sec = nextSend.tv_sec;
	startTime.tv_usec = nextSend.tv_usec;
	while ( !end ) {
		CHECK( gettimeofday( &currentTime, NULL ) );
		waitTime.tv_sec = nextSend.tv_sec - currentTime.tv_sec;
		waitTime.tv_usec = nextSend.tv_usec - currentTime.tv_usec;
		if ( waitTime.tv_sec > 0 ) {
			if ( waitTime.tv_usec < 0 ) {
				waitTime.tv_sec -= 1;
				waitTime.tv_usec += USECS_PER_SEC;
			}
		} else if ( waitTime.tv_sec < 0 ) {
			if ( waitTime.tv_usec > 0 ) {
				waitTime.tv_sec += 1;
				waitTime.tv_usec -= USECS_PER_SEC;
			}
		}
		if ( waitTime.tv_sec > 0 || (waitTime.tv_sec == 0 && waitTime.tv_usec > 10000) ) {
			static fd_set fdSet;
			FD_ZERO( &fdSet );
			FD_SET( STDIN_FILENO, &fdSet );
#if defined(USE_RSVP)
			FD_SET( rsvpAPI.getFileDesc(), &fdSet );
			int count = select( rsvpAPI.getFileDesc() + 1, &fdSet, NULL, NULL, &waitTime );
#else
			int count = select( STDIN_FILENO + 1, &fdSet, NULL, NULL, &waitTime );
#endif
			if ( count > 0 ) {
#if defined(USE_RSVP)
				if ( FD_ISSET( rsvpAPI.getFileDesc(), &fdSet ) ) {
					rsvpAPI.receiveAndProcess();
				}
#endif
				if ( FD_ISSET( STDIN_FILENO, &fdSet ) ) {
					handleKeyboard();
#if defined(USE_RSVP)
					rsvpAPI.createSender( rsvpSession, localPort, tspec, 63 );
#endif
				}
			}
		} else {
			int readBytes = CHECK( read( read_fd, buffer, packetSize ) );
			if ( readBytes < packetSize ) {
				end = true;
			}
			int sent = send( fd, buffer, readBytes, 0 );
			if ( sent > 0 ) {
				sentBytes += sent;
				sentBytesMAC += sent + IP_HEADER_SIZE + UDP_HEADER_SIZE;
				sentPackets += 1;
			} else {
				failedPackets += 1;
//				cerr << "p " << localPort << " errno " << errno << endl;
			}
			nextSend.tv_usec += usecPerPacket;
			if ( nextSend.tv_usec >= USECS_PER_SEC ) {
				nextSend.tv_sec += 1;
				nextSend.tv_usec -= USECS_PER_SEC;
			}
		}
		if ( currentTime.tv_sec - startTime.tv_sec > nextPrint ) {
//			cout << "sent " << nextPrint << " seconds of video" << endl;
			nextPrint += PRINT_TIME_INTERVAL;
		}
	}

	CHECK( tcsetattr( STDIN_FILENO, TCSANOW, &save_termios ) );

	CHECK( gettimeofday( &endTime, NULL ) );
	float duration = endTime.tv_sec - startTime.tv_sec;
	int usec = endTime.tv_usec - startTime.tv_usec;
	if ( usec < 0 ) {
		duration -= 1;
		usec += USECS_PER_SEC;
	} else if ( usec > USECS_PER_SEC ) {
		duration += 1;
		usec -= USECS_PER_SEC;
	}
	duration += (float)usec / USECS_PER_SEC;

	buffer[0] = 'E';
	CHECK( send( fd, buffer, 1, 0 ) );

#if defined(USE_RSVP)
	rsvpAPI.releaseSession( rsvpSession );
#endif

	cout << endl;
	cout << "port " << localPort << " sent " << sentPackets << " packets" << endl;
	cout << "port " << localPort << " sent " << sentBytes << " bytes UDP payload" << endl;
	cout << "port " << localPort << " sent " << sentBytesMAC << " bytes MAC payload" << endl;
	cout << "port " << localPort << " sent " << duration << " seconds" << endl;
	cout << "port " << localPort << " failed " << failedPackets << " packets" << endl;
	cout << "port " << localPort << " sent " << (unsigned int)((sentBytes * 8)/duration) << " bit/sec UDP payload" << endl;
	cout << "port " << localPort << " sent " << (unsigned int)((sentBytesMAC * 8)/duration) << " bit/sec MAC payload" << endl;
	return 0;

}
