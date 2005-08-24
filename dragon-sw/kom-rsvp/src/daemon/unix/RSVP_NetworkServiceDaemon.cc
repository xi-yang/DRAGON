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
#if defined(Linux)
#include <features.h>                            // __USE_EXTERN_INLINES?
#ifndef __USE_EXTERN_INLINES
#define __USE_EXTERN_INLINES
#endif
#endif

#include "RSVP_NetworkServiceDaemon.h"
#include "RSVP.h"
#include "RSVP_BaseTimer.h"
#include "RSVP_Global.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_NetworkService.h"
#include "RSVP_IntServObjects.h"
#include "RSVP_RoutingService.h"
#include "RSVP_SignalHandling.h"
#include "RSVP_TimeValue.h"
#if defined(NEED_MULTICAST_TTL) || defined(NEED_UNICAST_TTL)
#include "RSVP_PacketHeader.h"
#endif

#if !defined(NS2)
#include "SystemCallCheck.h"
#endif

#include <sys/types.h>                           // needed for other includes
#if defined(SunOS)
#include <sys/sockio.h>                          // Solaris: ioctl commands
#include <sys/file.h>
#include <bsm/libbsm.h>                          // Solaris: Problems with netinet/ip.h
#elif defined(FreeBSD)
#include <netinet/in_systm.h>                    // FreeBSD 3.3: Problems with netinet/ip.h
#include <net/if_dl.h>                           // struct sockaddr_dl, etc.
#include <sys/uio.h>                             // struct iovec
#include <sys/param.h>                           // ALIGN for CMSG_FIRSTHDR (must be included before sys/socket.h)
#include <sys/sysctl.h>                          // sysctl
#elif defined(Linux)
#include <sys/uio.h>                             // struct iovec
#include <linux/types.h>                         // needed for linux/pkt_sched.h
#include <linux/pkt_sched.h>                     // TC_PRIO_CONTROL
#endif
#include <sys/socket.h>                          // socket, bind, sendto, recvf, connect
#include <netinet/ip.h>                          // IPTOS_PREC_INTERNETCONTROL
#include <sys/ioctl.h>                           // ioctl
#include <net/if.h>                              // interface structs
#include <sys/time.h>                            // FD_SET, etc.
#include <unistd.h>                              // select

// Linux < 2.2.14 header files seem broken
#if defined(NEED_IN_PKTINFO)
struct in_pktinfo {
	int		ipi_ifindex;
	struct in_addr	ipi_spec_dst;
	struct in_addr	ipi_addr;
};
#endif

InterfaceHandle NetworkServiceDaemon::rsrrSocket = -1;
bool NetworkServiceDaemon::rsrrReady = false;
InterfaceHandle NetworkServiceDaemon::routingSocket = -1;
bool NetworkServiceDaemon::routingReady = false;
const LogicalInterface* NetworkServiceDaemon::globalVirtualInterface = NULL;
const LogicalInterface** NetworkServiceDaemon::indexToInterfaceTable = NULL;
uint32 NetworkServiceDaemon::loopbackInterfaceIndex = 0;
uint32 NetworkServiceDaemon::packetDropsAtStart = 0;

// have one 'struct sockaddr_in' in static memory instead of reallocating it all the time
static struct sockaddr_in staticSendAddr;
class NetworkService_dummy {
public:
	NetworkService_dummy() {
		initMemoryWithZero( &staticSendAddr, sizeof(staticSendAddr) );
		staticSendAddr.sin_family = AF_INET;
		staticSendAddr.sin_port = 0;
		staticSendAddr.sin_addr.s_addr = 0;
#if defined(HAVE_SIN_LEN)
		staticSendAddr.sin_len = sizeof(staticSendAddr);
#endif
	}
};

static NetworkService_dummy dummy;

inline void NetworkServiceDaemon::set_fdMask( InterfaceHandleMask& fdmask ) {
	static const int count = sizeof(InterfaceHandleMask)/sizeof(int);
	int i = 0;
	for ( ; i < count; ++i ) {
		((int*)&fdmask)[i] = ((int*)&NetworkService::fdmask)[i];
	}
}

// this code is derived from W.R. Stevens: Unix Network Programming, 2nd Ed.
void NetworkServiceDaemon::buildInterfaceList( LogicalInterfaceList& lifList ) {
#if defined(REAL_NETWORK)
	FD_ZERO( &NetworkService::fdmask );

// get list of interfaces in system data structures
	int query_fd = CHECK( socket( AF_INET, SOCK_DGRAM, 0 ) );
	int trylength = 10;
	int lastlength = 0;
	struct ifconf iflist;
	for (;;) {
		iflist.ifc_len = trylength * sizeof(ifreq);
		iflist.ifc_req = new struct ifreq[trylength];
#if defined(FreeBSD)
		// try: getifaddrs
		CHECK( ioctl( query_fd, OSIOCGIFCONF, &iflist ));
#else
		CHECK( ioctl( query_fd, SIOCGIFCONF, &iflist ));
#endif
		if ( iflist.ifc_len == lastlength ) {
	break;
		}
		lastlength = iflist.ifc_len;
		trylength += 10;
		delete [] iflist.ifc_req;
	}
	uint32 interfaceCount = iflist.ifc_len/sizeof(ifreq);
	uint32 indexCount = interfaceCount;
	LOG(3)( Log::Config, "detected", interfaceCount, "interfaces" );

#if defined(Linux)
	struct ifreq query_if;
	initMemoryWithZero( &query_if, sizeof(query_if) );
	copyMemory( query_if.ifr_name, iflist.ifc_req[interfaceCount-1].ifr_name, IFNAMSIZ );
	CHECK( ioctl( query_fd, SIOCGIFINDEX, &query_if ) );
	indexCount = query_if.ifr_ifindex;
#endif
	
	indexToInterfaceTable = new const LogicalInterface*[indexCount + 1];
	initMemoryWithZero( indexToInterfaceTable, sizeof(LogicalInterface*) * (indexCount + 1) );

	uint32 xxx;
	for ( xxx = 0; xxx < interfaceCount; xxx += 1 ) {
		LOG(2)( Log::Config, "found interface", iflist.ifc_req[xxx].ifr_name );
		indexToInterfaceTable[xxx] = NULL;
	}
	
// get additional information per interface: address, MTU
// and create list of LogicalInterface objects
#if defined(FreeBSD)
	int indexOffset = 1;
#endif
	uint32 i;
	for ( i = 0; i < interfaceCount; i += 1 ) {
		struct ifreq query_if;
		initMemoryWithZero( &query_if, sizeof(query_if) );
		copyMemory( query_if.ifr_name, iflist.ifc_req[i].ifr_name, IFNAMSIZ );

#if defined(FreeBSD)
		if ( ioctl( query_fd, SIOCGIFADDR, &query_if ) < 0 ) {
	continue;
		}
		uint32 addr = ((sockaddr_in*)&query_if.ifr_addr)->sin_addr.s_addr;
#else
		uint32 addr = ((sockaddr_in*)&iflist.ifc_req[i].ifr_addr)->sin_addr.s_addr;
#endif

#if defined(FreeBSD) || defined(Linux)
		static uint32 lastAddr = 0;
		// ignore duplicate interfaces listed on FreeBSD or Linux
		if ( i > 0 && addr == lastAddr && !strncmp( iflist.ifc_req[i].ifr_name, iflist.ifc_req[i-1].ifr_name, IFNAMSIZ ) ) {
#if defined(FreeBSD)
			indexOffset -= 1;
#endif
	continue;
		}
		lastAddr = addr;
#endif

		CHECK( ioctl( query_fd, SIOCGIFMTU, &query_if ) );
#if defined(SunOS)
		uint32 mtu = query_if.ifr_metric;
#else
		uint32 mtu = query_if.ifr_mtu;
#endif

/*
		CHECK( ioctl( query_fd, SIOCGIFNETMASK, &query_if ) );
#if defined(Linux)
		uint32 mask = ((sockaddr_in*)&query_if.ifr_netmask)->sin_addr.s_addr;
#else
		uint32 mask = ((sockaddr_in*)&query_if.ifr_addr)->sin_addr.s_addr;
#endif
*/

		uint32 index = i;
#if defined(Linux)
		CHECK( ioctl( query_fd, SIOCGIFINDEX, &query_if ) );
		index = query_if.ifr_ifindex;
#elif defined(FreeBSD)
		index = i + indexOffset;
#endif /* SunOS -> use manual index counter */

		// ignore loopback interface
		if ( NetAddress(addr) == LogicalInterface::loopbackAddress ) {
			loopbackInterfaceIndex = index;
	continue;
		}

		LogicalInterface* netif = new LogicalInterface( iflist.ifc_req[i].ifr_name, addr, mtu, index );
		lifList.push_back( netif );
		indexToInterfaceTable[index] = netif;
		LOG(4)( Log::Config, "interface", iflist.ifc_req[i].ifr_name, "has system index", index );

		CHECK( ioctl( query_fd, SIOCGIFFLAGS, &query_if ) );
		if ( !(query_if.ifr_flags & IFF_UP) ) {
			netif->disable();
		} else if ( !(query_if.ifr_flags & IFF_ALLMULTI) ) {
			query_if.ifr_flags |= IFF_ALLMULTI;
//			CHECK( ioctl( query_fd, SIOCSIFFLAGS, &query_if ) );
		}

	}
	delete [] iflist.ifc_req;
	CHECK( close( query_fd ) );

	// create dedicated raw interface to read incoming RSVP packets
	globalVirtualInterface = new LogicalInterface( "recvIF", 0, 0, 4096 );
	const_cast<LogicalInterface*>(globalVirtualInterface)->init( sint32Infinite );
	// receive all RSVP packets on this socket, if not received on a vif
#if defined(FreeBSD)
	const int on = 1;
	CHECK( setsockopt( globalVirtualInterface->fd, IPPROTO_IP, IP_RECVIF, (char*)&on, sizeof(on) ));
	CHECK( setsockopt( globalVirtualInterface->fd, IPPROTO_IP, IP_RSVP_ON, (char*)NULL, 0 ));
#elif defined(Linux)
	const int on = 1;
	CHECK( setsockopt( globalVirtualInterface->fd, SOL_IP, IP_PKTINFO, (char*)&on, sizeof(on) ));
	CHECK( setsockopt( globalVirtualInterface->fd, SOL_IP, IP_ROUTER_ALERT, (char *)&on, sizeof(on) ));
#endif /* FreeBSD vs. Linux */
	NetworkService::initReceiveInterface( globalVirtualInterface->fd, true );
#if defined(FreeBSD)  
  // store dropping stats
	size_t len = sizeof(packetDropsAtStart);
	CHECK( sysctlbyname( "net.inet.ip.intr_queue_drops", &packetDropsAtStart, &len, NULL, 0) );
#endif
#endif /* REAL_NETWORK */
}

void NetworkServiceDaemon::cleanup() {
#if defined(REAL_NETWORK)
#if defined(FreeBSD)  
	uint32 packetDropsAtEnd;
	size_t len = sizeof(packetDropsAtEnd);
	CHECK( sysctlbyname( "net.inet.ip.intr_queue_drops", &packetDropsAtEnd, &len, NULL, 0) );
	if ( packetDropsAtEnd - packetDropsAtStart > 0 ) {
		ERROR(3)( Log::Error, "detected", packetDropsAtEnd - packetDropsAtStart, "IP packet drops" );
	}
#endif
	if (globalVirtualInterface) {
#if defined(FreeBSD)
		CHECK( setsockopt( globalVirtualInterface->fd, IPPROTO_IP, IP_RSVP_OFF, (char*)NULL, 0 ));
#endif
		FD_CLR( globalVirtualInterface->fd, &NetworkService::fdmask );
		close( globalVirtualInterface->fd );
		delete globalVirtualInterface;
		globalVirtualInterface = NULL;
	}
	if ( indexToInterfaceTable ) delete [] indexToInterfaceTable;
#endif
}

// the number for 'maxSelectFDs' is collected during various initialization
// routines from 'NetworkService[Daemon]'.
const LogicalInterface* NetworkServiceDaemon::queryInterfaces() {
	static SimpleList<const LogicalInterface*> readyList;
	while ( readyList.empty() && !(rsrrReady || routingReady ) ) {
		static InterfaceHandleMask readfds;
		static int fdCount;
		TimeValue zeroTime(0,0);
		set_fdMask( readfds );
		// first check for incoming packets, otherwise execute pending timers
		fdCount = select( NetworkService::maxSelectFDs, &readfds, NULL, NULL, &zeroTime );
		if ( fdCount == 0 ) {
			static TimeValue* waitTime;
			static TimeValue remainingTime;
			if ( RSVP_Global::currentTimerSystem->executeTimer(remainingTime) ) {
				waitTime = &remainingTime;
			} else {
				waitTime = (TimeValue*)0;
			}
#if defined(LOG_ON)
			if (!waitTime || *waitTime != TimeValue(0,0))
				LOG(2)( Log::Select, "NetworkService calling blocking select, timeout is", (waitTime ? *waitTime : TimeValue(0)) );
#endif
			set_fdMask( readfds );
			fdCount = select( NetworkService::maxSelectFDs, &readfds, NULL, NULL, waitTime );
		}
		if ( fdCount < 0 ) {
			if ( errno == EINTR ) {
				if ( SignalHandling::userSignal ) {
					SignalHandling::userSignal = false;
	continue;
				} else {
	return NULL;
				}
			} else {
				ERROR(3)( Log::Error, "select reports error", errno, strerror(errno) );
			}
		}

		// search those interfaces that have a ready file descriptor
#if defined(REAL_NETWORK)
		// check dedicated listen socket
		if ( globalVirtualInterface && FD_ISSET( globalVirtualInterface->fd, &readfds ) ) {
			readyList.push_back( globalVirtualInterface );
			fdCount -= 1;
		}
		// check routing sockets
		if ( rsrrSocket != -1 && FD_ISSET( rsrrSocket, &readfds ) ) {
			rsrrReady = true;
			fdCount -= 1;
		}
		if ( routingSocket != -1 && FD_ISSET( routingSocket, &readfds ) ) {
			routingReady = true;
			fdCount -= 1;
		}
#endif
		// check other interfaces, if necessary (vif or API or UDP interfaces)
		static uint32 i;
		for ( i = 0; fdCount > 0 && i < RSVP_Global::rsvp->getInterfaceCount(); ++i ) {
			const LogicalInterface* lif = RSVP_Global::rsvp->findInterfaceByLIH(i);
			if ( lif && !lif->isDisabled() && FD_ISSET( lif->fd, &readfds ) ) {
				readyList.push_back( RSVP_Global::rsvp->findInterfaceByLIH(i) );
				fdCount -= 1;
			}
		}
                                                        assert( fdCount == 0 );
	}
	if ( !readyList.empty() ) {
		static const LogicalInterface* lif;
		lif = readyList.front();
		readyList.pop_front();
		return lif;
	} else {
		return NULL;
	}
}

void NetworkServiceDaemon::registerRSRR_Handle( InterfaceHandle fd ) {
	rsrrSocket = fd;
	FD_SET( fd, &NetworkService::fdmask );
	if ( fd >= NetworkService::maxSelectFDs ) NetworkService::maxSelectFDs = fd + 1;
}

void NetworkServiceDaemon::deregisterRSRR_Handle( InterfaceHandle fd ) {
	rsrrSocket = -1;
	FD_CLR( fd, &NetworkService::fdmask );
}

void NetworkServiceDaemon::registerRouting_Handle( InterfaceHandle fd ) {
	routingSocket = fd;
	FD_SET( fd, &NetworkService::fdmask );
	if ( fd >= NetworkService::maxSelectFDs ) NetworkService::maxSelectFDs = fd + 1;
}

void NetworkServiceDaemon::deregisterRouting_Handle( InterfaceHandle fd ) {
	routingSocket = -1;
	FD_CLR( fd, &NetworkService::fdmask );
}

InterfaceHandle NetworkServiceDaemon::initRawInterfaceIP4( const NetAddress& addr ) {
#if defined(REAL_NETWORK)
	InterfaceHandle fd = CHECK( socket( AF_INET, SOCK_RAW, IPPROTO_RSVP ));
	const uint8 off = 0;
	setsockopt( fd, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&off, sizeof(off) );
	const int on = 1;
	CHECK( setsockopt( fd, SOL_SOCKET, SO_REUSEXXX, (char*)&on, sizeof(on) ));
	CHECK( setsockopt( fd, IPPROTO_IP, IP_HDRINCL, (char*)&on, sizeof(on) ));
#if defined(NEED_RA_SOCKOPT)
	// for Solaris, setting options in raw ip header doesn't work, see also class PacketHeader
	// therefore -> router alert always turned on outgoing packets!
	static char options[4] = {148, 4, 0, 0};
	CHECK( setsockopt( fd, IPPROTO_IP, IP_OPTIONS, options, sizeof(options) ));
#endif
#if defined(Linux)
//	int prio = TC_PRIO_CONTROL;
//	CHECK( setsockopt( fd, SOL_IP, SO_PRIORITY, (char*)&prio, sizeof(prio) ) );
#else
//	int tos = IPTOS_PREC_INTERNETCONTROL;
//	CHECK( setsockopt( fd, IPPROTO_IP, IP_TOS, (char*)&tos, sizeof(tos) ));
#endif
	return fd;
#else /* no REAL_NETWORK */
	return -1;
#endif
}

void NetworkServiceDaemon::initRawUnicastInterfaceIP4( InterfaceHandle fd, const NetAddress& addr ) {
#if defined(REAL_NETWORK)
	if ( addr ) {
		struct sockaddr_in if_addr;
		initMemoryWithZero( &if_addr, sizeof(if_addr) );
		if_addr.sin_family = AF_INET;
		if_addr.sin_port = 0;
		if_addr.sin_addr.s_addr = addr.rawAddress();
#if defined(HAVE_SIN_LEN)
		if_addr.sin_len = sizeof(if_addr);
#endif
		CHECK( bind( fd, (struct sockaddr*)&if_addr, sizeof(if_addr) ) );
		// when sending multicast packets on this socket bypass routing and use this interface
		setsockopt( fd, IPPROTO_IP, IP_MULTICAST_IF, (char*)(&if_addr.sin_addr), sizeof(if_addr.sin_addr) );
	}
#endif
}

void NetworkServiceDaemon::initRawMulticastInterfaceIP4( InterfaceHandle fd, const NetAddress& addr, VifHandle vifnum ) {
#if defined(REAL_NETWORK) && defined(FreeBSD)
	// receive RSVP packets from the specified vif on this socket
	CHECK( setsockopt( fd, IPPROTO_IP, IP_RSVP_VIF_ON, (char*)&vifnum, sizeof(vifnum) ) );
	NetworkService::initReceiveInterface( fd );
	// not to be used: packets are duplicated and the source address is set wrong
//	CHECK( setsockopt( fd, IPPROTO_IP, IP_MULTICAST_VIF, (char *)&vifnum, sizeof(vifnum) ) );
#endif
}

void NetworkServiceDaemon::sendRawPacketIP4( InterfaceHandle fd, const ONetworkBuffer& buffer, const NetAddress& destAddr, const NetAddress& gw ) {
#if defined(REAL_NETWORK)
	LOG(6)( Log::Packet, "sending raw packet to ", destAddr, "via", gw, "ip length:", buffer.getUsedSize() );
	if (destAddr.isMulticast()) {
#if defined(NEED_MULTICAST_TTL)
		uint8 hops = reinterpret_cast<const PacketHeader*>(buffer.getContents())->getTTL();
		CHECK( setsockopt( fd, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&hops, sizeof(hops) ) );
#endif
	} else {
#if defined(NEED_UNICAST_TTL)
		int hops = reinterpret_cast<const PacketHeader*>(buffer.getContents())->getTTL();
		CHECK( setsockopt( fd, IPPROTO_IP, IP_TTL, (char*)&hops, sizeof(hops) ) );
#endif
	}
	if ( gw == LogicalInterface::noGatewayAddress ) 
		staticSendAddr.sin_addr.s_addr = destAddr.rawAddress();
	else 
		staticSendAddr.sin_addr.s_addr = gw.rawAddress();
	int sendlen = sendto( fd, (SENDTO_BUF_T)buffer.getContents(), buffer.getUsedSize(), 0, (sockaddr*)&staticSendAddr, sizeof(staticSendAddr) );
	assert( sendlen == buffer.getUsedSize() || errno == EHOSTDOWN || errno == EHOSTUNREACH || errno == ENOBUFS );
/*#if !defined(FreeBSD)
	// direct send to gateway doesn't work on FreeBSD
	if ( gw != LogicalInterface::noGatewayAddress ) {
		staticSendAddr.sin_addr.s_addr = gw.rawAddress();
		int sendlen = sendto( fd, (SENDTO_BUF_T)buffer.getContents(), buffer.getUsedSize(), MSG_DONTROUTE, (sockaddr*)&staticSendAddr, sizeof(staticSendAddr) );
		assert( sendlen == buffer.getUsedSize() || errno == EHOSTDOWN || errno == EHOSTUNREACH || errno == ENOBUFS );
		return;
	}
#endif
	staticSendAddr.sin_addr.s_addr = destAddr.rawAddress();
	int sendlen = sendto( fd, (SENDTO_BUF_T)buffer.getContents(), buffer.getUsedSize(), 0, (sockaddr*)&staticSendAddr, sizeof(staticSendAddr) );
	assert( sendlen == buffer.getUsedSize() || errno == EHOSTDOWN || errno == EHOSTUNREACH || errno == ENOBUFS );
*/
#endif
}

const LogicalInterface* NetworkServiceDaemon::receiveRawPacketIP4( InterfaceHandle fd, INetworkBuffer&buffer ) {
	const LogicalInterface* realLif = NULL;
#if defined(REAL_NETWORK)
#if defined(FreeBSD) || defined(Linux)
	struct iovec iov;
	struct msghdr hdr;
	struct cmsghdr *chdr;
	char ctrlBuffer[128];
	hdr.msg_name = NULL;
	hdr.msg_namelen = 0;
	hdr.msg_control = (caddr_t)ctrlBuffer;
	hdr.msg_controllen = sizeof(ctrlBuffer);
	hdr.msg_iov = &iov;
	hdr.msg_iovlen = 1;
	iov.iov_base = (char*)buffer.getWriteBuffer();
	iov.iov_len = buffer.getSize();
	int length = CHECK( recvmsg( fd, &hdr, 0 ) );
	if ( fd != globalVirtualInterface->fd ) {
		buffer.setWriteLength( (uint16)length );
		return NULL;
	}
	if (hdr.msg_controllen != 0) {
		for ( chdr = CMSG_FIRSTHDR(&hdr); chdr != NULL; chdr = CMSG_NXTHDR(&hdr,chdr)) {
#if defined(FreeBSD)
			if (chdr->cmsg_len != 0 && chdr->cmsg_level == IPPROTO_IP && chdr->cmsg_type == IP_RECVIF) {
				struct sockaddr_dl* sdl = (sockaddr_dl*)CMSG_DATA(chdr);
				uint32 index = sdl->sdl_index;
#elif defined(Linux)
			if (chdr->cmsg_len != 0 && chdr->cmsg_level == SOL_IP && chdr->cmsg_type == IP_PKTINFO) {
				struct in_pktinfo* ipi = (in_pktinfo*)CMSG_DATA(chdr);
				uint32 index = ipi->ipi_ifindex;
#endif
				LOG(2)( Log::Msg, "interface system index is", index );
				realLif = indexToInterfaceTable[index];
		break;
			}
		}
	}
#else
	struct sockaddr_in fromAddr;
	RECVFROM_SIZE_T fromlen = sizeof(fromAddr);
	int length = CHECK( recvfrom( fd, (RECVFROM_BUF_T)buffer.getWriteBuffer(), buffer.getSize(), 0, (struct sockaddr*)&fromAddr, &fromlen ) );
	// KLUDGE: This is a hack to find the incoming interface. If it is
	// impossible to determine this directly, we are doing a reverse unicast
	// routing lookup for the packet's source address.
	// do that for PHOP in case of PATH/PTEAR? -> can't be done here
	NetAddress dummy;
	realLif = RSVP_Global::rsvp->getRoutingService().getUnicastRoute( NetAddress(fromAddr.sin_addr.s_addr), dummy );
#endif	
	if ( !realLif ) {
		ERROR(1)( Log::Error, "ERROR: unable to find real interface index" );
		buffer.setWriteLength( 0 );
	} else {
		buffer.setWriteLength( (uint16)length );
	}
#endif /* REAL_NETWORK */
	return realLif;
}
