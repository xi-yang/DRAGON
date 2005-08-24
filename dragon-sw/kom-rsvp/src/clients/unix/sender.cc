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
#include <signal.h>
#include <stdlib.h>
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>                                                // INT_MAX

#include "RSVP_TimeValue.h"
#include "common.h"

#if defined(ENABLE_ALTQ)
#include <altq/altq_stamp.h>
#endif

#define USE_RSVP

#if defined(USE_RSVP)
#include "RSVP_API.h"
#include "RSVP_API_Upcall.h"
#else
#define USECS_PER_SEC 1000000
#endif

#if !defined(IPTOS_ECT)
#define IPTOS_CE  0x01
#define IPTOS_ECT 0x02
#endif

static bool endFlag = false;

static void killHandler( int ) {
	endFlag = true;
}

#if defined(USE_RSVP)
static void rsvpUpcall( const GenericUpcallParameter& upcallPara, RSVP_API* rsvpAPI ) {
	if ( upcallPara.generalInfo->infoType == UpcallParameter::RESV_EVENT ) {
		cout << "RESV received" << endl;
	}
}
#endif

int main( int argc, const char** argv ) {
	unsigned short localPort = 0, remotePort = 0;
	unsigned int remoteAddress = 0;
	unsigned short packetSize = 1000;
	TimeValue duration(1,0);
	int rate = 1000000;
	int rate_alt = 1000000;
	int rate_current = 0;
	TimeValue period(1,0);
	TimeValue period_alt(1,0);
	int socketType = SOCK_DGRAM;

	if ( argc < 5 ) {
		cerr << "usage: " << argv[0];
#if defined(ENABLE_ALTQ)
		cerr << " -i <iface>";
#endif
		cerr << " [tcp|rsvp] remote-addr remote-port local-port duration(in msec) [packet-size (def:1000 bytes)] [rate (def:1000000 bit/s)] [alt-rate (def:rate)] [period (def:1000 msec)] [alt-period (def:1000 msec)] [x = don't wait (def:wait)]" << endl;
		return 1;
	}

#if defined(ENABLE_ALTQ)
	String iface;
	if ( !strcmp( argv[1], "-i" ) ) {
		iface = argv[2];
		argv += 2;
		argc -= 2;
	}
#endif

	bool reserve = false;
	if ( !strcmp( argv[1], "tcp" ) ) {
		socketType = SOCK_STREAM;
		argv++;
		argc--;
	} else if ( !strcmp( argv[1], "rsvp" ) ) {
		reserve = true;
		argv++;
		argc--;
	}

	remoteAddress = getInetAddr( argv[1] );
	if ( !remoteAddress ) {
		cerr << "cannot find destination: " << argv[1] << endl;
		return 1;
	} else if ( (ntohl(remoteAddress) >> 28) == 14 && socketType == SOCK_STREAM ) {
	  cerr << "can't use TCP and send to multicast destination " << argv[1] << endl;
	  return 1;
	}

	remotePort = atoi( argv[2] );
	localPort = atoi( argv[3] );
	duration = TimeValue( atoi(argv[4])/MSECS_PER_SEC, (atoi(argv[4])%MSECS_PER_SEC) * USECS_PER_MSEC );
	if ( duration < TimeValue(1,0) ) {
		duration = TimeValue(1,0);
	}

	if ( argc > 5 ) {
		packetSize = atoi( argv[5] );
	}
	if ( argc > 6 ) {
		rate = atoi( argv[6] );
	}
	if ( argc > 7 ) {
		rate_alt = atoi( argv[7] );
	} else {
		rate_alt = rate;
	}
	if ( argc > 8 ) {
		period = TimeValue( atoi(argv[8])/MSECS_PER_SEC, (atoi(argv[8])%MSECS_PER_SEC) * USECS_PER_MSEC );
	}
	if ( argc > 9 ) {
		period_alt = TimeValue( atoi(argv[9])/MSECS_PER_SEC, (atoi(argv[9])%MSECS_PER_SEC) * USECS_PER_MSEC );
	}
	TimeValue timePerPacket = period;
	if ( rate > 8*packetSize ) {
		timePerPacket = TimeValue( 0, 1000000 / (rate / (8*packetSize)) );
	} else {
		cout << "!! rate " << rate << " bit/s is smaller than one packet. Adjusted to 0 !!" << endl;
		rate = 0;
	}
	TimeValue timePerPacket_alt = period_alt;
	if ( rate_alt > 8*packetSize ) {
		timePerPacket_alt = TimeValue( 0, 1000000 / (rate_alt  / (8*packetSize)) );
	} else {
		cout << "!! rate " << rate_alt << " bit/s is smaller than one packet. Adjusted to 0 !!" << endl;
		rate_alt = 0;
	}
	TimeValue timePerPacket_current = timePerPacket_alt;
	enum { Normal, Alt } currentPeriod = Alt;        // => begin with normal rate

#if defined(USE_RSVP)
	RSVP_API rsvpAPI("");
	RSVP_API::SessionId rsvpSession;
	if ( reserve ) {
		rsvpSession = rsvpAPI.createSession( remoteAddress, IPPROTO_UDP, remotePort, (UpcallProcedure)rsvpUpcall, &rsvpAPI );
		TSpec tspec;
		tspec.set_p( (rate*2.0)/8 );
		// burst size = 0.2 seconds of peak load
		tspec.set_b( (rate*2.0)/8 * 0.2 );
		// add UDP/IP header overhead to rate, then add 10% for smooth demo effect
		tspec.set_r( 1.1 * ((rate*1.0)/8 + (1 + rate/(8*packetSize)) * (UDP_HEADER_SIZE + IP_HEADER_SIZE)) );
		tspec.set_M( 1500 );
		tspec.set_m( packetSize );
		rsvpAPI.createSender( rsvpSession, localPort, tspec, 63 );
	}
#endif

	int fd = CHECK( socket( AF_INET, socketType, 0 ) );

// not needed if underlying TC modules (correctly) dismisses packets
// also not needed, if intermediate node is used which dismisses packets
//	CHECK( fcntl( fd, F_SETFL, (fcntl(fd, F_GETFL) | O_NONBLOCK )) );

	struct sockaddr_in if_addr;

	memset( &if_addr.sin_zero, 0, sizeof(if_addr.sin_zero) );
	if_addr.sin_family = AF_INET;
	if_addr.sin_port = htons(localPort);
	if_addr.sin_addr.s_addr = INADDR_ANY;
#if defined(__FreeBSD__)
	if_addr.sin_len = sizeof( if_addr );
#endif
	CHECK( bind( fd, (struct sockaddr*)&if_addr, sizeof(if_addr) ) );

	memset( &if_addr.sin_zero, 0, sizeof(if_addr.sin_zero) );
	if_addr.sin_family = AF_INET;
	if_addr.sin_port = htons(remotePort);
	if_addr.sin_addr.s_addr = (remoteAddress);
#if defined(__FreeBSD__)
	if_addr.sin_len = sizeof( if_addr );
#endif
	CHECK( connect( fd, (struct sockaddr*)&if_addr, sizeof(if_addr) ) );

	if ( remoteAddress && (ntohl(remoteAddress) >> 28) == 14 ) {
		char ttl = 15;
		CHECK( setsockopt( fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl) ) );
	}

#if defined(ENABLE_ALTQ)
	int stamp_fd = -1;
	if ( iface.length() > 0 ) {
		stamp_fd = open( "/dev/altq/stamp", O_RDWR );
	}
	if ( stamp_fd >= 0 ) {
		struct stamp_interface stamp_if;
		initMemoryWithZero( &stamp_if, sizeof(stamp_if) );
		strncpy( stamp_if.ifname, iface.chars(), IFNAMSIZ );
		CHECK( ioctl(stamp_fd, STAMP_IF_ATTACH, &stamp_if) );
		cout << "using interface " << iface << endl;
		CHECK( ioctl(stamp_fd, STAMP_ENABLE, &stamp_if) );
		struct stamp_add_filter filter;
		initMemoryWithZero( &filter, sizeof(filter) );
		copyMemory( &filter.iface, &stamp_if, sizeof(stamp_if) );
		filter.filter.ff_flow.fi_dst.s_addr = remoteAddress;
		struct sockaddr_in jofel;
		socklen_t x = sizeof(jofel);
		CHECK( getsockname( fd, (sockaddr*)&jofel, &x ) );
		filter.filter.ff_flow.fi_src.s_addr = jofel.sin_addr.s_addr;
		filter.filter.ff_flow.fi_dport = htons( remotePort );
		filter.filter.ff_flow.fi_sport = htons( localPort );
		filter.filter.ff_flow.fi_proto = IPPROTO_UDP;
		filter.filter.ff_flow.fi_family = AF_INET;
		filter.filter.ff_flow.fi_len = sizeof(struct flowinfo_in);
		filter.filter.ff_mask.mask_dst.s_addr = 0xffffffff;
		filter.filter.ff_mask.mask_src.s_addr = 0xffffffff;
		filter.stamp_offset = UDP_HEADER_SIZE + 0;
		CHECK( ioctl( stamp_fd, STAMP_ADD_FILTER_OUT, &filter ) );
	}
#endif

	cout << "sending " << (rate > 8*packetSize ? 1 : 0) << " packet every " << timePerPacket.tv_usec << " microsecs during high period of " << period << endl;
	cout << "sending " << (rate_alt > 8*packetSize ? 1 : 0) << " packet every " << timePerPacket_alt.tv_usec << " microsecs during low period of " << period_alt << endl;

	struct sigaction sa;
	sigemptyset( &sa.sa_mask );
	sa.sa_handler = killHandler;
	CHECK( sigaction( SIGHUP, &sa, NULL ) );
	CHECK( sigaction( SIGINT, &sa, NULL ) );
	CHECK( sigaction( SIGQUIT, &sa, NULL ) );
	CHECK( sigaction( SIGTERM, &sa, NULL ) );

	if ( argc <= 10 ) {
		cout << "port " << localPort << ": send signal HUP, INT, QUIT or TERM to start (e.g. press CTRL-C)" << endl;
		sigsuspend( &sa.sa_mask );
	}

	cout << "starting port " << localPort << endl;

	char* buffer = new char[packetSize];
	unsigned long sentBytes = 0;
	unsigned long sentBytesMAC = 0;
	unsigned long sentBytesRaw = 0;
	unsigned long sentPackets = 0;
	unsigned long failedPackets = 0;
	TimeValue nextSend, endTime;
	CHECK( gettimeofday( &nextSend, NULL ) );
	endTime = nextSend + duration;
	TimeValue peakChangeTime = nextSend;
	TimeValue currentTime, waitTime;
	endFlag = false;
	while ( !endFlag ) {
		CHECK( gettimeofday( &currentTime, NULL ) );
		if ( currentTime > endTime ) {
	break;
		}
		if ( currentTime > peakChangeTime ) {
			if ( currentPeriod == Normal ) {
				timePerPacket_current = timePerPacket_alt;
				peakChangeTime += period_alt;
				cout << "changing rate to " << rate_alt << " bit/s" << endl;
				currentPeriod = Alt;
				rate_current = rate_alt;
			} else {
				timePerPacket_current = timePerPacket;
				peakChangeTime += period;
				cout << "changing rate to " << rate << " bit/s" << endl;
				currentPeriod = Normal;
				rate_current = rate;
			}
//			cout << "port " << localPort << " sent " << sentBytes << " bytes UDP payload so far" << endl;
	continue;
		}
		if ( nextSend < peakChangeTime ) {
			waitTime = nextSend - currentTime;
		} else {
			waitTime = peakChangeTime - currentTime;
		}
		if ( waitTime > TimeValue(0,0) ) {
			static fd_set fdSet;
			FD_ZERO( &fdSet );
#if defined(USE_RSVP)
			FD_SET( rsvpAPI.getFileDesc(), &fdSet );
			int count = select( rsvpAPI.getFileDesc() + 1, &fdSet, NULL, NULL, &waitTime );
#else
			int count = select( 0, NULL, NULL, NULL, &waitTime );
#endif
			if ( count > 0 ) {
#if defined(USE_RSVP)
				if ( FD_ISSET( rsvpAPI.getFileDesc(), &fdSet ) ) {
					rsvpAPI.receiveAndProcess();
				}
#endif
			}
		} else {
			if ( rate_current ) {
				int sent = send( fd, buffer, packetSize, 0 );
				if ( sent > 0 ) {
					sentBytes += sent;
					sentBytesMAC += sent + IP_HEADER_SIZE + UDP_HEADER_SIZE;
					sentBytesRaw += sent + IP_HEADER_SIZE + UDP_HEADER_SIZE + ETHER_HEADER_SIZE;
					sentPackets += 1;
				} else {
					failedPackets += 1;
//					cerr << "p " << localPort << " errno " << errno << endl;
				}
			}
			nextSend += timePerPacket_current;
		}
	}

#if defined(USE_RSVP)
	if (reserve) rsvpAPI.releaseSession( rsvpSession );
#endif

	cout << endl;
	cout << "port " << localPort << " sent total " << sentPackets << " packets" << endl;
	cout << "port " << localPort << " sent total " << sentBytes << " bytes ";
	cout << (socketType == SOCK_STREAM ? "TCP" : "UDP");
	cout << " payload" << endl;
	if ( socketType != SOCK_STREAM ) cout << "port " << localPort << " sent total " << sentBytesMAC << " bytes MAC payload" << endl;
	if ( socketType != SOCK_STREAM ) cout << "port " << localPort << " sent total " << sentBytesRaw << " raw bytes" << endl;
	cout << "port " << localPort << " failed " << failedPackets << " packets" << endl;
	cout << "port " << localPort << " sent " << (sentBytes * 8)/duration.tv_sec << " bit/sec ";
	cout << (socketType == SOCK_STREAM ? "TCP" : "UDP");
	cout << " payload" << endl;
	if ( socketType != SOCK_STREAM ) cout << "port " << localPort << " sent " << (sentBytesMAC * 8)/duration.tv_sec << " bit/sec MAC payload" << endl;
	if ( socketType != SOCK_STREAM ) cout << "port " << localPort << " sent " << (sentBytesRaw * 8)/duration.tv_sec << " bit/sec raw data" << endl;
	CHECK( shutdown( fd, 2 ) );
	close( fd );
	return 0;
}
