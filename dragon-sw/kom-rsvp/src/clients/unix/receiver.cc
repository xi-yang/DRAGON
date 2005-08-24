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
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>                        // htonl, htons, ntohl, ntohl
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include "RSVP_TimeValue.h"
#include "common.h"

#if defined(ENABLE_ALTQ)
#include <altq/altq_stamp.h>
#endif

#define USE_RSVP

#if defined(USE_RSVP)
#include "RSVP_API.h"
#include "RSVP_API_Upcall.h"
#endif

static bool end = false;
static long failedPackets = 0;
static unsigned short destPort = 0, sourcePort = 0;
static unsigned int sourceAddress = 0;
static unsigned int destAddress = INADDR_ANY;
static unsigned short packetSize = 65535;
static int socketType = SOCK_DGRAM;
static TimeValue averageInterval(1,0);

#if defined(USE_RSVP)
static FLOWSPEC_Object* flowspec;
static RSVP_API::SessionId rsvpSession = NULL;
#endif

struct Stats {
	unsigned long receivedBytes;
	unsigned long receivedBytesMAC;
	unsigned long receivedBytesRaw;
	unsigned long receivedPackets;
	Stats() : receivedBytes(0), receivedBytesMAC(0), receivedBytesRaw(0), receivedPackets(0) {}
};

static void setStats( Stats& s, int received ) {
	s.receivedBytes += received;
	s.receivedBytesMAC += (received + IP_HEADER_SIZE + UDP_HEADER_SIZE);
	s.receivedBytesRaw += (received + IP_HEADER_SIZE + UDP_HEADER_SIZE + ETHER_HEADER_SIZE);
	s.receivedPackets += 1;
}

static void killHandler( int ) {
	end = true;
}

void output1( const Stats& e, const Stats& s ) {
	cout << endl;
	cout << "port " << destPort << " received " << e.receivedPackets - s.receivedPackets << " packets" << endl;
	cout << "port " << destPort << " received " << e.receivedBytes - s.receivedBytes << " bytes ";
	cout << (socketType == SOCK_STREAM ? "TCP" : "UDP");
	cout << " payload" << endl;
	if ( socketType != SOCK_STREAM ) cout << "port " << destPort << " received " << e.receivedBytesMAC - s.receivedBytesMAC << " bytes MAC payload" << endl;
	if ( socketType != SOCK_STREAM ) cout << "port " << destPort << " received " << e.receivedBytesRaw - s.receivedBytesRaw << " raw bytes" << endl;
	cout << "port " << destPort << " failed " << failedPackets << " packets" << endl;
}

void output2( const Stats& e, const Stats& s, const TimeValue& duration ) {
	if ( e.receivedBytes-s.receivedBytes != 0 && duration != TimeValue(0,0) ) {
		cout << "port " << destPort << " received " << (unsigned int)(((e.receivedBytes-s.receivedBytes)*8)/duration.getFractionalValue()) << " bit/sec ";
		cout << (socketType == SOCK_STREAM ? "TCP" : "UDP");
		cout << " payload" << endl;
	}
	if ( duration != TimeValue(0,0) ) {
		if ( socketType != SOCK_STREAM ) cout << "port " << destPort << " received " << (unsigned int)(((e.receivedBytesMAC - s.receivedBytesMAC)*8)/duration.getFractionalValue()) << " bits/s MAC payload" << endl;
		if ( socketType != SOCK_STREAM ) cout << "port " << destPort << " received " << (unsigned int)(((e.receivedBytesRaw - s.receivedBytesRaw)*8)/duration.getFractionalValue()) << " bits/s raw data" << endl;
	}
	cout << endl;
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
	cout << "received: " << *upcallPara.generalInfo << endl;
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

int main( int argc, char** argv ) {

	Stats allPacketStats, partPacketStats, firstPacketStats;

	if ( argc < 4 ) {
		cerr << "usage: " << argv[0];
#if defined(ENABLE_ALTQ)
		cerr << " -i <iface>";
#endif
		cerr << " [tcp|rsvp] source-addr source-port dest-port [dest-addr]" << endl;
		cerr << "If dest-addr is multicast, then source-addr specifies the local interface" << endl;
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

	sourceAddress = getInetAddr( argv[1] );
	if ( !sourceAddress ) {
		cerr << "cannot resolve source: " << argv[1] << " receiving from any sender" << endl;
	}

	sourcePort = atoi( argv[2] );
	destPort = atoi( argv[3] );
	if ( argc > 4 ) {
		destAddress = getInetAddr( argv[4] );
		if ( !destAddress ) {
			cerr << "cannot resolve destination: " << argv[4] << endl;
			return 1;
		} else if ( (ntohl(destAddress) >> 28) == 14 && socketType == SOCK_STREAM ) {
			cerr << "can't use TCP and receive from multicast group " << argv[4] << endl;
			return 1;
		}
	}

	int old_fd = -1;
	int fd = CHECK( socket( AF_INET, socketType, 0 ) );

	if ( (ntohl(destAddress) >> 28) == 14 ) {
		const int on = 1;
		CHECK( setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on) ));
	}
	struct sockaddr_in if_addr;
	memset( &if_addr.sin_zero, 0, sizeof(if_addr.sin_zero) );
	if_addr.sin_family = AF_INET;
	if_addr.sin_port = htons(destPort);
	if_addr.sin_addr.s_addr = destAddress;
#if defined(__FreeBSD__)
	if_addr.sin_len = sizeof( if_addr );
#endif
 	CHECK( bind( fd, (struct sockaddr*)&if_addr, sizeof(if_addr) ) );
	if ( (ntohl(destAddress) >> 28) == 14 ) {
		ip_mreq mreq;
		mreq.imr_multiaddr.s_addr = destAddress;
		mreq.imr_interface.s_addr = sourceAddress;
		CHECK( setsockopt( fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq) ));
	} else if ( socketType == SOCK_STREAM ) {
		old_fd = fd;
		CHECK( listen( fd, 5 ) );
		RECVFROM_SIZE_T x;
		fd = CHECK( accept( old_fd, (struct sockaddr*)&if_addr, &x ) );
	} else {
		memset( &if_addr.sin_zero, 0, sizeof(if_addr.sin_zero) );
		if_addr.sin_family = AF_INET;
		if_addr.sin_port = htons(sourcePort);
		if_addr.sin_addr.s_addr = sourceAddress;
#if defined(__FreeBSD__)
		if_addr.sin_len = sizeof( if_addr );
#endif
		CHECK( connect( fd, (struct sockaddr*)&if_addr, sizeof(if_addr) ) );
	}

	int sockbuf = 65535;
	CHECK( setsockopt( fd, SOL_SOCKET, SO_RCVBUF, (char *)&sockbuf, sizeof(sockbuf) ) );

	cout << "starting port " << destPort << endl;

	struct sigaction sa;
	sigemptyset( &sa.sa_mask );
	sa.sa_handler = killHandler;
	CHECK( sigaction( SIGHUP, &sa, NULL ) );
	CHECK( sigaction( SIGINT, &sa, NULL ) );
	CHECK( sigaction( SIGQUIT, &sa, NULL ) );
	CHECK( sigaction( SIGTERM, &sa, NULL ) );

#if defined(USE_RSVP)
	RSVP_API rsvpAPI("");
	flowspec = new FLOWSPEC_Object( TSpec(), RSpec( 0, 0 ) );
	if (reserve) {
		rsvpSession = rsvpAPI.createSession( NetAddress(destAddress), IPPROTO_UDP, destPort, (UpcallProcedure)rsvpUpcall, &rsvpAPI );
	}
#endif

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
		filter.filter.ff_flow.fi_dst.s_addr = destAddress;
		filter.filter.ff_flow.fi_src.s_addr = sourceAddress;
		filter.filter.ff_flow.fi_dport = htons( destPort );
		filter.filter.ff_flow.fi_sport = htons( sourcePort );
		filter.filter.ff_flow.fi_proto = IPPROTO_UDP;
		filter.filter.ff_flow.fi_family = AF_INET;
		filter.filter.ff_flow.fi_len = sizeof(struct flowinfo_in);
		filter.filter.ff_mask.mask_dst.s_addr = 0xffffffff;
		filter.filter.ff_mask.mask_src.s_addr = 0xffffffff;
		filter.stamp_offset = UDP_HEADER_SIZE + 8;
		CHECK( ioctl( stamp_fd, STAMP_ADD_FILTER_IN, &filter ) );
	}
	TimeValue maxDiff(0,0);
	TimeValue minDiff((1u<<31-1),999999);
	TimeValue avgDiff(0,0);
#endif

	char* buffer = new char[packetSize];
	end = false;
	TimeValue start(0,0), now, last;
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
			rsvpAPI.receiveAndProcess();
		}
#endif
		if ( FD_ISSET( fd, &readfds ) ) {
			int received = recv( fd, buffer, packetSize, 0 );
#if defined(ENABLE_ALTQ)
			if ( stamp_fd >= 0 ) {
				TimeValue sent( ntohl(*(unsigned int*)(buffer)), ntohl(*(unsigned int*)(buffer+4)) );
				TimeValue received( ntohl(*(unsigned int*)(buffer+8)), ntohl(*(unsigned int*)(buffer+12)) );
				TimeValue diff = received - sent;
//				cerr << "diff: " << diff.tv_sec << ":" << setw(6) << diff.tv_usec << endl;
				if ( diff > maxDiff ) maxDiff = diff;
				if ( diff < minDiff ) minDiff = diff;
				avgDiff += diff;
			}
#endif				
			if ( start == TimeValue(0,0) ) {
				CHECK( gettimeofday(&start,NULL) );
				last = start;
				setStats(firstPacketStats,received);
			}
			if ( !end ) CHECK( gettimeofday(&now,NULL) );
			if ( received > 0 ) {
				setStats(allPacketStats,received);
			} else if ( received == 0 ) {
				end = true;
			} else {
				failedPackets += 1;
			}
			TimeValue delta = now - last;
			if ( delta > averageInterval ) {
				output1( allPacketStats, partPacketStats );
				output2( allPacketStats, partPacketStats, delta );
				last = now;
				partPacketStats = allPacketStats;
			}
		}
	}
#if defined(ENABLE_ALTQ)
	if ( stamp_fd >= 0 && allPacketStats.receivedPackets ) {
		cout << endl;
		cout << "max diff: " << maxDiff.tv_sec << ":" << setw(6) << maxDiff.tv_usec << endl;
		cout << "min diff: " << minDiff.tv_sec << ":" << setw(6) << minDiff.tv_usec << endl;
		avgDiff = avgDiff/allPacketStats.receivedPackets;
		cout << "avg diff: " << avgDiff.tv_sec << ":" << setw(6) << avgDiff.tv_usec << endl;
	}
#endif

#if defined(USE_RSVP)
	if (reserve) rsvpAPI.releaseSession( rsvpSession );
	flowspec->destroy();
#endif

	output1( allPacketStats, Stats() );
	output2( allPacketStats, firstPacketStats, now - start );
	delete [] buffer;
	if ( (ntohl(destAddress) >> 28) != 14 ) CHECK( shutdown( fd, 2 ) );
	close( fd );
	if ( old_fd >= 0 ) {
		CHECK( shutdown( old_fd, 2 ) );
		close( old_fd );
	}
	return 0;

}
