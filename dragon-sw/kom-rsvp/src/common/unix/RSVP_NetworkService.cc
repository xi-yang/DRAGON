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
#include "RSVP_NetworkService.h"
#include "RSVP_PacketHeader.h"
#include "RSVP_Log.h"

#include "SystemCallCheck.h"

#include <sys/types.h>                           // needed for other includes
#include <sys/socket.h>                          // socket, bind, sendto, recvf
#include <sys/time.h>                            // FD_SET, etc.

InterfaceHandleMask NetworkService::fdmask;
int NetworkService::maxSelectFDs = 0;

void NetworkService::joinMCastGroupIP4( InterfaceHandle fd, const NetAddress& group ) {
#if defined(REAL_NETWORK)
	ip_mreq mreq;
	mreq.imr_multiaddr.s_addr = group.rawAddress();
	mreq.imr_interface.s_addr = INADDR_ANY;
	CHECK( setsockopt( fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq) ));
	LOG(2)( Log::Packet, "joined mcast group", group );
#endif
}

void NetworkService::leaveMCastGroupIP4( InterfaceHandle fd, const NetAddress& group ) {
#if defined(REAL_NETWORK)
	ip_mreq mreq;
	mreq.imr_multiaddr.s_addr = group.rawAddress();
	mreq.imr_interface.s_addr = INADDR_ANY;
	CHECK( setsockopt( fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq) ));
	LOG(2)( Log::Packet, "left mcast group", group ); 
#endif
}

// this function creates an encapsulated socket and sets its file descriptor in 'fdmask'
InterfaceHandle NetworkService::initInterfaceUDP( uint16& localPort ) {
	InterfaceHandle fd = CHECK( socket( AF_INET, SOCK_DGRAM, 0 ) );
	struct sockaddr_in if_addr;
	initMemoryWithZero( &if_addr, sizeof(if_addr) );
	if_addr.sin_family = AF_INET;
	if_addr.sin_port = htons(localPort);
	if_addr.sin_addr.s_addr = INADDR_ANY;
#if defined(HAVE_SIN_LEN)
	if_addr.sin_len = sizeof(if_addr);
#endif
	CHECK( bind( fd, (struct sockaddr*)&if_addr, sizeof(if_addr) ) );
	initMemoryWithZero( &if_addr, sizeof(if_addr) );
	GETSOCKNAME_SIZE_T arglen2 = sizeof(if_addr);
	CHECK( getsockname( fd, (struct sockaddr*)&if_addr, &arglen2 ) );
	localPort = ntohs( if_addr.sin_port );
	initReceiveInterface( fd );
	return fd;
}

void NetworkService::initReceiveInterface( InterfaceHandle fd, bool dedicatedRSVP ) {
	// increase receive buffer size
	int recvbufsize;
	GETSOCKOPT_SIZE_T arglen = sizeof(int);
	CHECK( getsockopt( fd, SOL_SOCKET, SO_RCVBUF, (char *)&recvbufsize, &arglen ) );
	if ( recvbufsize < sockbufsize ) {
		CHECK( setsockopt( fd, SOL_SOCKET, SO_RCVBUF, (char *)&sockbufsize, sizeof(sockbufsize) ));
	}
	// set file description for select
	FD_SET( fd, &fdmask );
	if ( fd >= maxSelectFDs ) maxSelectFDs = fd + 1;
}

bool NetworkService::waitForPacket( InterfaceHandle fd, bool setTimeout, TimeValue timeout ) {
	fd_set readfds;
	FD_ZERO( &readfds );
	FD_SET( fd, &readfds );
	int retval = 0;
	if ( setTimeout ) {
		retval = select( fd + 1, &readfds, NULL, NULL, &timeout );
	} else {
		retval = select( fd + 1, &readfds, NULL, NULL, NULL );
	}
	return (retval > 0) && FD_ISSET( fd, &readfds );
}

bool NetworkService::receivePacket( InterfaceHandle fd, INetworkBuffer&buffer ) {
	struct sockaddr_in fromAddr;
	initMemoryWithZero( &fromAddr, sizeof(fromAddr) );
	RECVFROM_SIZE_T fromLength = sizeof(fromAddr);
	int length = recvfrom( fd, (RECVFROM_BUF_T)buffer.getWriteBuffer(), buffer.getSize(), 0, (struct sockaddr*)&fromAddr, &fromLength );
	if (length < 0) {
#if defined(Linux)
		if (errno != EAGAIN && errno != ECONNREFUSED)
#else
		if (errno != EAGAIN )
#endif
		{
			LOG(3)( Log::Error, "ERROR receivePacket: errno is", errno, strerror(errno) );
		}
	} else {
		if ( reinterpret_cast<PacketHeader*>(buffer.getWriteBuffer())->getSrcAddress() == NetAddress(0) ) {
			reinterpret_cast<PacketHeader*>(buffer.getWriteBuffer())->setSrcAddress( fromAddr.sin_addr.s_addr );
		}
		buffer.setWriteLength( (uint16)length );
	}
	return (length > 0);
}

bool NetworkService::sendPacket( InterfaceHandle fd, const ONetworkBuffer& buffer, const NetAddress& destAddr, uint16 destPort, bool connected ) {
	struct sockaddr_in dest;
	int retval;
	if ( !connected ) {
	  initMemoryWithZero( &dest, sizeof(dest) );
		dest.sin_family = AF_INET;
		dest.sin_port = htons(destPort);
		dest.sin_addr.s_addr = destAddr.rawAddress();
#if defined(HAVE_SIN_LEN)
		dest.sin_len = sizeof(dest);
#endif
		LOG(4)( Log::Packet, "sending packet to", destAddr, "/", destPort );
		retval = sendto( fd, (SENDTO_BUF_T)buffer.getContents(), buffer.getUsedSize(), 0, (sockaddr*)&dest, sizeof(dest) );
	} else {
		LOG(1)( Log::Packet, "sending packet to fixed address" );
		retval = send( fd, (SENDTO_BUF_T)buffer.getContents(), buffer.getUsedSize(), 0 );
	}
	if ( retval < 0 ) {
		LOG(7)( Log::Error, "ERROR: couldn't send packet to", destAddr, "/", destPort, "errno is", errno, strerror(errno) );
		return false;
	}
	return true;
}

void NetworkService::shutdownInterface( InterfaceHandle fd ) {
	FD_CLR( fd, &fdmask );
	close(fd);
}
