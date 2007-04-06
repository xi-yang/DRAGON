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
#include "tg_classes.h"
#include "RSVP_Set.h"
#include "aggreg.h"

#include <net/if.h>                              // interface structs
#include <sys/ioctl.h>                           // ioctl
#include <fcntl.h>                               // fcntl and flags

#if defined(FreeBSD)
#include <sys/sysctl.h>
#include <netinet/ip_var.h>
#include <netinet/udp_var.h>
#elif defined(SunOS)
#include <sys/sockio.h>                          // Solaris: ioctl commands
#endif

#if defined(ENABLE_ALTQ)
#include <fcntl.h>     
#include <sys/stat.h>  
#include <sys/param.h> 
#include <sys/linker.h>
#include <altq/altq_stamp.h>
#endif

#include <typeinfo>

TG::TrafficGenerator* traffgen;

namespace TG {

char Flow::buffer[65535]; 
char Sender::buffer[65535];
uint32 ActionSet::counter = 0;

// fraction of rate which is allocated as token bucket depth
static const uint32 tbDepthFactor = 5;

static const int sendStampOffset = 0;
static const int sendStampSize = sizeof(timeval);
static const int recvStampOffset = sendStampOffset + sendStampSize;
static const int recvStampSize = sizeof(timeval);
static const int seqNoOffset = recvStampOffset + recvStampSize;
static const int seqNoSize = sizeof(uint32);
static const int lengthOffset = seqNoOffset + seqNoSize;
static const int lengthSize = sizeof(uint16);
static const int minPacketLength = lengthOffset + lengthSize;

extern inline TimeValue getSenderStamp( const char* const buffer ) {
	return TimeValue( ntohl(*(sint32*)(buffer+sendStampOffset)), ntohl(*(sint32*)(buffer+sendStampOffset+4)) );
}
extern inline void setSenderStamp( const char* const buffer, const TimeValue& t ) {
	*(sint32*)(buffer+sendStampOffset) = htonl( t.tv_sec );
	*(sint32*)(buffer+sendStampOffset+4) = htonl( t.tv_usec );
}
extern inline TimeValue getReceiverStamp( const char* const buffer ) {
	return TimeValue( ntohl(*(sint32*)(buffer+recvStampOffset)), ntohl(*(sint32*)(buffer+recvStampOffset+4)) );
}
extern inline void setReceiverStamp( const char* const buffer, const TimeValue& t ) {
	*(sint32*)(buffer+recvStampOffset) = htonl( t.tv_sec );
	*(sint32*)(buffer+recvStampOffset+4) = htonl( t.tv_usec );
}
extern inline uint32 getSequenceNumber( const char* const buffer ) {
	return ntohl(*(uint32*)(buffer+seqNoOffset));
}
extern inline void setSequenceNumber( const char* const buffer, uint32 s ) {
	*(uint32*)(buffer+seqNoOffset) = htonl(s);
}
extern inline uint16 getPacketLength( const char* const buffer ) {
	return ntohs(*(uint16*)(buffer+lengthOffset));
}
extern inline void setPacketLength( const char* const buffer, uint16 l ) {
	*(uint16*)(buffer+lengthOffset) = htons(l);
}

struct Interface {
	uint32 address;
	String name;
	DECLARE_ORDER(Interface)
	Interface( uint32 a = 0, const String& n = "" ) : address(a), name(n) {}
};
IMPLEMENT_ORDER1(Interface,address)

typedef Set<Interface,Interface> InterfaceSet;

class Network {
	static InterfaceSet interfaceSet;
	static int stamp_fd;
	static int kernelmod_fileid;
	static int ip_drops_start;
	static int udp_drops_start;
public:
	static void init( uint32 );
	static void cleanup();
	static bool findInterface( uint32 addr );
#if defined(ENABLE_ALTQ)
	static bool findInterface( uint32 addr, struct stamp_interface& );
	static int getStampFD() { return stamp_fd; }
#endif
};

InterfaceSet Network::interfaceSet;
int Network::stamp_fd = -1;
int Network::kernelmod_fileid = -1;
int Network::ip_drops_start = 0;
int Network::udp_drops_start = 0;

template <class T>
void OnceTimer<T>::internalFire() {
	cancel();
	object.timeout();
}

template <class T>
void ManyTimer<T>::internalFire() {
	this->cancel();
	this->alarmTime = this->object.timeout();
	if ( this->alarmTime != TimeValue(0,0) ) this->start();
}

void PacketCounter::showSummaryInformation() {
	if ( packetCountRSVP || packetCountNoRSVP ) {
		ERROR(6)( LogStats, name, "counted", packetCountRSVP, "RSVP packets and", packetCountNoRSVP, "other packets" );
	}
}

void DelayCounter::showSummaryInformation() {
	if (PacketCounter::empty()) return;
	PacketCounter::showSummaryInformation();
	uint32 totalCount = 0;
	// TODO: change to long double for increased precision?
	TimeValueLong sumDelay, sumDelayPow2 = 0;
	for ( uint32 i = 0; i <= hashBinCount; i += 1 ) {
		totalCount += hashBins[i];
		if ( i != hashBinCount ) {
			sumDelay += (((2*i+1)*hashBinSize)/2) * hashBins[i];
			sumDelayPow2 += (((2*i+1)*hashBinSize)/2).pow2() * hashBins[i];
		}
		ERROR(6)( LogStats, name, "delay", (((2*i+1)*hashBinSize)/2), "packet count:", hashBins[i], totalCount );
	}
	if ( totalCount > 0 ) {
		TimeValue avgDelay = sumDelay/totalCount;
		TimeValue deviation = (sumDelayPow2/totalCount - avgDelay.pow2()).sqrt();
		ERROR(7)( LogStats, name, "summary - avg/std/max:", (PreciseTimeValue&)avgDelay, "/", (PreciseTimeValue&)deviation, "/", (PreciseTimeValue&)maxDelay );
		ERROR(3)( LogStats, name, "summary - excess packets:", hashBins[hashBinCount] );
	}
	if ( delayEstimator ) delayEstimator->showSummaryInformation();
}

void RateEstimator::storePacket( const TimeValue& currentTime, uint32 length ) {
	if ( slotEnd == TimeValue(0,0) ) slotEnd = currentTime;
	while ( currentTime > slotEnd + avgInterval ) {
		slotEnd += avgInterval;
		if ( avgRateList.back().full() ) avgRateList.push_back( PacketChunk() );
		avgRateList.back().addPacket( Packet( slotEnd, 0 ) );
	}
	if ( currentTime > slotEnd ) {
		if ( avgRateList.back().full() ) avgRateList.push_back( PacketChunk() );
		if ( delayEstimator ) {
			if ( packetCounter > 0 ) {
				avgRateList.back().addPacket( Packet( currentTime, byteCounter/packetCounter ) );
			}
		} else {
			avgRateList.back().addPacket( Packet( currentTime, byteCounter ) );
		}
		byteCounter = 0;
		packetCounter = 0;
		slotEnd += avgInterval;
	}
	byteCounter += length;
	packetCounter += 1;
}

void RateEstimator::showSummaryInformation() {
	uint32 lastRate = 0;
	uint32 c = 0;
	uint64 sumRate = 0;
	uint64 sumBytes = 0;
	String subject = "rate";
	String unit = "bits/sec";
	if  ( delayEstimator ) {
		subject = "delay";
		unit = "microseconds";
	}
	if ( !avgRateList.empty() && avgRateList.front().next() > 0 ) {
		PacketList::ConstIterator piter = avgRateList.begin();
		for ( ; piter != avgRateList.end(); ++piter ) {
			for ( uint32 j = 0; j < (*piter).next(); ++j ) {
				if ( delayEstimator ) {
					lastRate = (*piter)[j].data;
				} else {
					lastRate = (8LL * (*piter)[j].data * USECS_PER_SEC) / avgInterval.getUsec();
					sumBytes += (*piter)[j].data;
				}
				sumRate += lastRate;
				c += 1;
				ERROR(6)( LogStats, name, subject, "at", (DaytimeTimeValue&)(*piter)[j].packetTime, lastRate, unit );
			} // for all packets in chunk
		} // for all packet chunks
	} // if there are packets
	if ( c > 1 ) {
		c -= 1;
		sumRate -= lastRate;
		ERROR(5)( LogStats, name, subject, "summary -", sumRate/c, unit );
		if ( !delayEstimator ) {
			ERROR(4)( LogStats, name, "total transmit: ", sumBytes, "bytes" );
		}
	}
}

void Flow::getLocalAddressing() {
	struct sockaddr_in addr;
	RECVFROM_SIZE_T x = sizeof(addr);
	CHECK( getsockname( fd, (struct sockaddr*)&addr, &x ) );   
	localAddr = addr.sin_addr.s_addr;
	localPort = ntohs(addr.sin_port);
}

void Flow::getRemoteAddressing() {
	struct sockaddr_in addr;
	RECVFROM_SIZE_T x = sizeof(addr);
	CHECK( getpeername( fd, (struct sockaddr*)&addr, &x ) );   
	remoteAddr = addr.sin_addr.s_addr;
	remotePort = ntohs(addr.sin_port);
}

void Flow::bind() {
	LOG(2)( LogDebug, "bind:", *this );
	struct sockaddr_in addr; 
	initMemoryWithZero( &addr.sin_zero, sizeof(addr.sin_zero) );
	addr.sin_family = AF_INET;
	addr.sin_port = htons(localPort);
	addr.sin_addr.s_addr = localAddr;
#if defined(__FreeBSD__)
	addr.sin_len = sizeof( addr );
#endif
	if ( ::bind( fd, (struct sockaddr*)&addr, sizeof(addr) ) < 0 ) {
		ERROR(4)( Log::Error, *this, "bind reports error", errno, strerror(errno) );
		exit(1);
	}
}

void Flow::connect() {
	LOG(2)( LogDebug, "connect:", *this );
	if ( (ntohl(remoteAddr) >> 28) == 14 ) {
		const int on = 1;
		CHECK( setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on) )); 
		struct ip_mreq mreq;
		mreq.imr_multiaddr.s_addr = remoteAddr;
		mreq.imr_interface.s_addr = localAddr;
		CHECK( setsockopt( fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq) )); 
	} else {
		struct sockaddr_in addr;
		initMemoryWithZero( &addr.sin_zero, sizeof(addr.sin_zero) );
		addr.sin_family = AF_INET;
		addr.sin_port = htons(remotePort);
		addr.sin_addr.s_addr = remoteAddr;
#if defined(__FreeBSD__)
		addr.sin_len = sizeof( addr );
#endif
		if ( ::connect( fd, (struct sockaddr*)&addr, sizeof(addr) ) < 0 ) {
			ERROR(4)( Log::Error, *this, "connect reports error", errno, strerror(errno) );
			exit(1);
		}
	}
}

void Flow::rsvpUpcallDispatcher( const GenericUpcallParameter& upcallPara, Flowset* flowset ) {
	if ( upcallPara.generalInfo->infoType == UpcallParameter::RESV_EVENT ) {
		if ( upcallPara.resvEvent->flowDescriptor.filterSpecList.size() > 1 ) {
			ERROR(1)( Log::Error, "rsvpUpcallDispatcher: cannot dispatch RSVP RESV with multiple flow descriptors" );
		return;
		}
		uint16 x = upcallPara.resvEvent->flowDescriptor.filterSpecList.back().getLspId() - flowset->basePort;
		if ( x >= flowset->portRange ) {
			ERROR(6)( Log::Error, "rsvpUpcallDispatcher: ignoring RSVP RESV with port number", x+flowset->basePort, "exceeding range", flowset->basePort, "-", flowset->basePort+flowset->portRange-1 );
		return;
		}
		Flow::rsvpUpcall( upcallPara, flowset->flows[x] );
	} else {
		Flow::rsvpUpcall( upcallPara, flowset->flows[0] );
	}
}

void Flow::rsvpUpcall( const GenericUpcallParameter& upcallPara, Flow* This ) {
	LOGV(2)( LogOther, "received:", *upcallPara.generalInfo );
	uint16 portOffset;
	switch ( upcallPara.generalInfo->infoType ) {
	case UpcallParameter::PATH_EVENT: {
		if ( (*upcallPara.generalInfo->session)->getTunnelId() != This->localPort ) {
			ERROR(3)( Log::Error, "rsvpUpcall: ignoring RSVP PATH with wrong destination", (*upcallPara.generalInfo->session)->getDestAddress(), (*upcallPara.generalInfo->session)->getTunnelId() );
		return;
		}
		const TSpec& sentTSpec = upcallPara.pathEvent->sendTSpec;
		const ADSPEC_Object* adspec = upcallPara.pathEvent->adSpec;
		FLOWSPEC_Object* flowspec = NULL;
		if ( adspec && adspec->supportsGS() ) {
			if ( This->rspec.get_R() == 0 ) {
				if ( This->rspec.get_S() != 0 ) {
					// R=0, S!=0  ==>  S denotes delay
					double new_R = sentTSpec.calculateRate( adspec->getAdSpecGSParameters().getTotError(), This->rspec.get_S() );
					flowspec = new FLOWSPEC_Object( sentTSpec, RSpec( (ieee32float)new_R, 0 ) );
				} else {
					// R=0, S=0  ==> request CL service
					TSpec tspec( (ieee32float)This->wtpFactor * sentTSpec.get_r(),
						(ieee32float)This->wtpFactor * sentTSpec.get_b(),
						(ieee32float)This->wtpFactor * sentTSpec.get_p(),
						sentTSpec.get_m(), sentTSpec.get_M() );
					flowspec = new FLOWSPEC_Object( tspec );
				}
			} else {
				// R!=0  ==> use R and S as given
				flowspec = new FLOWSPEC_Object( sentTSpec, This->rspec );
			}
		} else {
			// G service not supported
			TSpec tspec( (ieee32float)This->wtpFactor * sentTSpec.get_r(),
				(ieee32float)This->wtpFactor * sentTSpec.get_b(),
				(ieee32float)This->wtpFactor * sentTSpec.get_p(),
				sentTSpec.get_m(), sentTSpec.get_M() );
			flowspec = new FLOWSPEC_Object( tspec );
		}
		FlowDescriptorList fdList;
		fdList.push_back( flowspec );
		fdList.back().filterSpecList.push_back( upcallPara.pathEvent->senderTemplate );
                                assert( This->rsvpAPI && This->rsvpSession );
		This->rsvpAPI->createReservation( This->rsvpSession, true, FF, fdList );
	}
		break;
	case UpcallParameter::RESV_EVENT:
		portOffset = (*upcallPara.generalInfo->session)->getTunnelId() - This->remotePort;
		if ( portOffset >= This->portrange ) {
			ERROR(7)( Log::Error, "rsvpUpcall: ignoring RSVP RESV exceeding port range", This->remotePort, "-", This->remotePort + This->portrange - 1, "for", (*upcallPara.generalInfo->session)->getDestAddress(), (*upcallPara.generalInfo->session)->getTunnelId() );
		return;
		}
		This->sendResvSuccess = true;
		if ( This->sender ) {
			LOG(2)( LogOther, "starting sender now:", *This->sender );
			This->sender->startSending();
		}
		break;
	case UpcallParameter::PATH_TEAR:
		portOffset = upcallPara.pathTear->senderTemplate.getLspId() - This->remotePort;
		if ( portOffset >= This->portrange ) {
			ERROR(7)( Log::Error, "rsvpUpcall: ignoring RSVP PTEAR exceeding port range", This->remotePort, "-", This->remotePort + This->portrange - 1, "from", upcallPara.pathTear->senderTemplate.getSrcAddress(), upcallPara.pathTear->senderTemplate.getLspId() );
		return;
		}
		if ( This->pstats[portOffset].recvResvSuccess ) {
			LOGV(1)( LogOther, "removed reservation" );
			This->pstats[portOffset].recvResvSuccess = false;
		}
		break;
	case UpcallParameter::RESV_CONFIRM:

		portOffset = upcallPara.resvConfirm->flowDescriptor.filterSpecList.front().getLspId() - This->remotePort;
		if ( portOffset >= This->portrange ) {
			ERROR(7)( Log::Error, "rsvpUpcall: ignoring RSVP CONFIRM exceeding port range", This->remotePort, "-", This->remotePort + This->portrange - 1, "from", upcallPara.resvConfirm->flowDescriptor.filterSpecList.front().getSrcAddress(), upcallPara.resvConfirm->flowDescriptor.filterSpecList.front().getLspId() );
		return;
		}
		This->pstats[portOffset].recvResvSuccess = true;
		break;
	default:
		break;
	}
}

Flow::Flow( uint32 la, uint16 lp, uint32 ra, uint16 rp, uint8 p, uint16 pr )

	: fd(-1), localAddr(la), localPort(lp), remoteAddr(ra), remotePort(rp),
	proto(p), sender(NULL), rsvpAPI(NULL), rsvpSession(NULL), ownRSVP(false),
	flowset(NULL), failedPackets(0), sendResvSuccess(false), lastSentSeq(0),
	sendingRateEstimator(NULL), receivingRateEstimator(NULL),
	sendPacketCounter(NULL), delayCounter(NULL), sequencing(true),
	portrange(pr), pstats(new PortStats[portrange]), senderStamping(false),
	receiverStamping(false), kernelStamping(false), transportHeaderSize(0),
	headerOverhead(0) {
	for ( uint16 i = 0; i < portrange; i += 1 ) {
		pstats[i].port = remotePort + i;
	}
}
	
Flow::~Flow() {
	LOG(2)( LogDebug, "destructor:", *this );
	if ( ownRSVP && rsvpSession ) {
                                                             assert(rsvpAPI);
		rsvpAPI->releaseSession( rsvpSession );
	}
	if ( flowset ) delete [] flowset;
	if ( fd != -1 ) close(fd);
	delete [] pstats;
}

void Flow::cloneFrom( Flow& f ) {
	sender = f.sender;
	sendingRateEstimator = f.sendingRateEstimator;
	receivingRateEstimator = f.receivingRateEstimator;
	sendPacketCounter = f.sendPacketCounter;
	delayCounter = f.delayCounter;
	sequencing = f.sequencing;
	senderStamping = f.senderStamping;
	receiverStamping = f.receiverStamping;
}

void Flow::enableReceiving() {
                                                      assert( headerOverhead );
	int recvbufsize;
	GETSOCKOPT_SIZE_T arglen = sizeof(int);
	CHECK( getsockopt( fd, SOL_SOCKET, SO_RCVBUF, (char *)&recvbufsize, &arglen ) );
	if ( recvbufsize < 32768 ) {
		recvbufsize = 32768;
		CHECK( setsockopt( fd, SOL_SOCKET, SO_RCVBUF, (char *)&recvbufsize, sizeof(recvbufsize) ));
	}
}

RSVP_API::SessionId Flow::registerSenderRSVP( RSVP_API& api, Flowset* flowset ) {
                                              assert(!rsvpSession && !rsvpAPI);
	flowset = flowset;
	rsvpAPI = &api;
	rsvpSession = api.createSession( remoteAddr, proto, remotePort, (UpcallProcedure)rsvpUpcallDispatcher, flowset );
	return rsvpSession;
}

void Flow::unregisterRSVP() {
                                     assert(ownRSVP && rsvpAPI && rsvpSession);
	rsvpAPI->releaseSession( rsvpSession );
	rsvpSession.reset();
	sendResvSuccess = false;
	for ( uint16 i = 0; i < portrange; i += 1 ) {
		pstats[i].recvResvSuccess = false;
	}
	rsvpAPI = NULL;
}

void Flow::signalSenderRSVP( RSVP_API& api, const TSpec& tspec ) {
	if ( !rsvpAPI ) {
                                                          assert(!rsvpSession);
		rsvpAPI = &api;
		rsvpSession = api.createSession( remoteAddr, proto, remotePort, (UpcallProcedure)rsvpUpcall, this );
		ownRSVP = true;
	}
	sendResvSuccess = false;

	//api.createSender( rsvpSession, localAddr, localPort, tspec, 63 );
}

void Flow::signalReceiverRSVP( RSVP_API& api, const RSpec& r, double w ) {
	rspec = r;
	wtpFactor = ( w >= 0 && w <= 1 ) ? w : 1.0;
	if ( !rsvpAPI ) {
                                                          assert(!rsvpSession);
		rsvpAPI = &api;
		rsvpSession = api.createSession( localAddr, proto, localPort, (UpcallProcedure)rsvpUpcall, this );
		ownRSVP = true;
	}
	for ( uint16 i = 0; i < portrange; i += 1 ) {
		pstats[i].recvResvSuccess = false;
	}
}

void Flow::enableSenderTimestamping() {
	senderStamping = true;
#if defined(ENABLE_ALTQ)
	struct stamp_interface stamp_if;
	if ( Network::getStampFD() >= 0
		&& Network::findInterface( localAddr, stamp_if )
		&& proto != IPPROTO_TCP ) {
		LOG(2)( LogDebug, "enabling sender stamping for", *this );
		struct stamp_add_filter filter;
		initMemoryWithZero( &filter, sizeof(filter) );
		copyMemory( &filter.iface, &stamp_if, sizeof(stamp_if) );
		filter.filter.ff_flow.fi_src.s_addr = localAddr;
		filter.filter.ff_mask.mask_src.s_addr = 0xffffffff;
		filter.filter.ff_flow.fi_sport = htons( localPort );
		filter.filter.ff_flow.fi_dst.s_addr = remoteAddr;
		filter.filter.ff_mask.mask_dst.s_addr = 0xffffffff;
		if ( portrange > 1 ) {
			filter.filter.ff_flow.fi_dport = 0;
		} else {
			filter.filter.ff_flow.fi_dport = htons( remotePort );
		}
		filter.filter.ff_flow.fi_proto = proto;
		filter.filter.ff_flow.fi_family = AF_INET;   
		filter.filter.ff_flow.fi_len = sizeof(struct flowinfo_in);
		filter.stamp_offset = transportHeaderSize + sendStampOffset;
		CHECK( ioctl( Network::getStampFD(), STAMP_ADD_FILTER_OUT, &filter ) );
		kernelStamping = true;
		return;
	}
	ERROR(2)( Log::Error, *this, "no sender kernel stamping" );
#endif
	kernelStamping = false;
}

void Flow::enableReceiverTimestamping() {
	receiverStamping = true;
#if defined(ENABLE_ALTQ)
	struct stamp_interface stamp_if;
	if ( Network::getStampFD() >= 0
		&& Network::findInterface( localAddr, stamp_if )
		&& proto != IPPROTO_TCP ) {
		LOG(2)( LogDebug, "enabling receiver stamping for", *this );
		uint16 i = 0;
		for ( ; i < portrange; i += 1 ) {
			struct stamp_add_filter filter;
			initMemoryWithZero( &filter, sizeof(filter) );
			copyMemory( &filter.iface, &stamp_if, sizeof(stamp_if) );
			filter.filter.ff_flow.fi_src.s_addr = remoteAddr;
			filter.filter.ff_mask.mask_src.s_addr = 0xffffffff;
			filter.filter.ff_flow.fi_sport = htons( remotePort + i );
			filter.filter.ff_flow.fi_dst.s_addr = localAddr;
			filter.filter.ff_mask.mask_dst.s_addr = 0xffffffff;
			filter.filter.ff_flow.fi_dport = htons( localPort );
			filter.filter.ff_flow.fi_proto = proto;
			filter.filter.ff_flow.fi_family = AF_INET;   
			filter.filter.ff_flow.fi_len = sizeof(struct flowinfo_in);
			filter.stamp_offset = transportHeaderSize + recvStampOffset;
			CHECK( ioctl( Network::getStampFD(), STAMP_ADD_FILTER_IN, &filter ) );
		}
		kernelStamping = true;
		return;
	}
	ERROR(2)( Log::Error, *this, "no receiver kernel stamping" );
#endif
	kernelStamping = false;
}

bool Flow::send( const char* const buffer, uint16 length ) {
	if ( length < minPacketLength ) {
		ERROR(2)( Log::Error, "send: ignoring too small packet with length:", length );
	return true;
	}
	TimeValue now = getCurrentSystemTime();
	if ( senderStamping ) {
		if ( !kernelStamping ) {
			setSenderStamp( buffer, now );
		}
		if ( sequencing ) {
			lastSentSeq += 1;
			setSequenceNumber( buffer, lastSentSeq );
		}
	}
	int sent = ::send( fd, buffer, length, 0 );
	if ( sent > 0 ) {
		if ( sendingRateEstimator ) {
			sendingRateEstimator->storePacket( now, sent + headerOverhead );
		}
		if ( sendPacketCounter ) sendPacketCounter->storePacket( sendResvSuccess );
		return true;
	} else {
//		ERROR(3)( Log::Error, "send reports error", errno, strerror(errno) );
		failedPackets += 1;
		return false;
	}
}

void Flow::receive( uint16 length ) {
	for (;;) {
		int received = 0;
		uint16 portOffset = 0;
		if ( portrange > 1 ) {
			static struct sockaddr_in addr;
			RECVFROM_SIZE_T len = sizeof(addr);
			received = recvfrom( fd, buffer, length, 0, (struct sockaddr*)&addr, &len );
			portOffset = ntohs(addr.sin_port) - remotePort;
			if ( portOffset >= portrange ) {
				ERROR(7)( Log::Error, "receive: ignoring packet exceeding port range", remotePort, "-", remotePort + portrange - 1, "from", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port) );
	continue;
			}
		} else {
			received = recv( fd, buffer, length, 0 );
		}
		if ( received < minPacketLength ) {
			if ( received < 0 ) {
				if ( errno == EWOULDBLOCK ) {
	return;
				} else if ( errno == EINTR ) {
	continue;
				} else {
					ERROR(3)( Log::Error, "recv reports error", errno, strerror(errno) );
				}
			} else {
				ERROR(2)( Log::Error, "receive: ignoring too small packet with length:", received );
			}
	return;
		}
		PortStats& pstat = pstats[portOffset];
		TimeValue receiveTime = getCurrentSystemTime();
		if ( receiverStamping ) {
			if ( sequencing ) {
				uint32 seqNo = getSequenceNumber(buffer);
				if ( seqNo < pstat.lastReceivedSeq ) {
					if ( seqNo == 0 || seqNo < pstat.lastReceivedSeq / 2 ) {
						pstat.lastReceivedSeq = seqNo;
					} else {
						pstat.reorderedPackets += 1;
						pstat.lostPackets -= 1;
					}
				} else {
					if ( seqNo > pstat.lastReceivedSeq + 1 ) {
						pstat.lostPackets += (seqNo - (pstat.lastReceivedSeq+1));
					} else if ( seqNo == pstat.lastReceivedSeq ) {
						pstat.duplicatePackets += 1;
					}
					pstat.lastReceivedSeq = seqNo;
				}
			}
			TimeValue delay(0,0);
			if ( kernelStamping ) {
				delay = getReceiverStamp( buffer ) - getSenderStamp( buffer );
			} else {
				delay = receiveTime - getSenderStamp( buffer );
			}
			if ( delay > TimeValue(0,0) ) {
				if ( delayCounter ) delayCounter->storePacket( delay, pstat.recvResvSuccess );
			} else {
				pstat.delayPoisonedPackets += 1;
				if ( delayCounter ) delayCounter->PacketCounter::storePacket( pstat.recvResvSuccess );
			}
		}
		if ( receivingRateEstimator ) {
			if ( receiverStamping && kernelStamping ) {
				receivingRateEstimator->storePacket( getReceiverStamp(buffer), received + headerOverhead );
			} else {
				receivingRateEstimator->storePacket( receiveTime, received + headerOverhead );
			}
		}
	} // for (;;)
}

void Flow::showReceiveStats() {
	ERROR(2)( LogStats, *this, "receive problems:" );
	for ( uint16 i = 0; i < portrange; i += 1 ) {
		if ( pstats[i].lostPackets ) {
			ERROR(4)( LogStats, "port", pstats[i].port, "lost packets:", pstats[i].lostPackets );
		}
		if ( pstats[i].duplicatePackets ) {
			ERROR(4)( LogStats, "port", pstats[i].port, "duplicate packets:", pstats[i].duplicatePackets );
		}
		if ( pstats[i].reorderedPackets ) {
			ERROR(4)( LogStats, "port", pstats[i].port, "reordered packets:", pstats[i].reorderedPackets );
		}
		if ( pstats[i].delayPoisonedPackets ) {
			ERROR(4)( LogStats, "port", pstats[i].port, "delay-poisoned packets:", pstats[i].delayPoisonedPackets );
		}
	}
}

void Flow::showSendStats() {
	if ( failedPackets ) {
		ERROR(3)( LogStats, "flow failed to send", failedPackets, "packets" );
	}
	if ( sendResvSuccess ) {
		LOG(1)( LogActions, "flow removes RSVP reservation" );
	}
}

void Flow::resetSend() {
	failedPackets = 0;
	lastSentSeq = 0;
}

ostream& operator<< ( ostream& os, const Flow& f ) {
	os << String( inet_ntoa( *(in_addr*)&f.localAddr ) );
	os << "/" << f.localPort << " <- " << (uint32)f.proto << " -> ";
	os << String( inet_ntoa( *(in_addr*)&f.remoteAddr ) );
	os << "/" << f.remotePort;
	if ( f.portrange > 1 ) os << "-" << f.remotePort + f.portrange - 1;
	return os;
}

TcpFlow::TcpFlow( uint32 ra, uint16 rp, int f )
	: Flow(0,0,ra,rp,IPPROTO_TCP), connector(*this), connectTime(-1,-1) {
	setTransportHeaderSize( sizeof(tcphdr) );
	fd = f;
	getLocalAddressing();
	long arg = CHECK( fcntl( fd, F_GETFL ) ) | O_NONBLOCK;
	CHECK( fcntl( fd, F_SETFL, arg ) );
}

TcpFlow::TcpFlow( uint32 la, uint32 ra, uint16 rp, TimeValue ct )
	: Flow(la,0,ra,rp,IPPROTO_TCP), connector(*this), connectTime(ct) {
	setTransportHeaderSize( sizeof(tcphdr) );
	fd = CHECK( socket( AF_INET, SOCK_STREAM, 0 ) );
	bind();
	getLocalAddressing();
}

inline void TcpFlow::startConnection() {
	traffgen->removeFD( fd );
	if ( connectTime != TimeValue(-1,-1) ) {
		if ( connectTime != TimeValue(0,0) ) {
			connector.startRel( connectTime );
		} else {
			timeout();
		}
	}
}

inline void TcpFlow::timeout() {
	connect();
	getRemoteAddressing();
	long arg = CHECK( fcntl( fd, F_GETFL ) ) | O_NONBLOCK;
	CHECK( fcntl( fd, F_SETFL, arg ) );
	traffgen->addFlow( *this );
}

bool TcpFlow::send( const char* const buffer, uint16 length ) {
	if ( connector.isActive() ) return false;
	if ( length >= minPacketLength && senderStamping ) {
		setPacketLength( buffer, length );
	}
	return Flow::send( buffer, length );
}

void TcpFlow::receive( uint16 length ) {
	int received = recv( fd, buffer, minPacketLength, MSG_PEEK );
	if ( received >= minPacketLength ) {
		if ( receiverStamping ) {
			uint16 l = getPacketLength( buffer );
			if ( l < minPacketLength ) {
				ERROR(3)( Log::Error, "tcp recv finds length of", l, "in peeked buffer" );
			} else {
				length = l;
			}
		}
		Flow::receive( length );
	} else if ( received == 0 ) {
		LOG(3)( LogOther, "TCP", *this, "has been closed" );
		traffgen->removeFD( fd );
		close(fd);
		fd = -1;
	} else if ( received > 0 ) {
		ERROR(2)( Log::Error, "tcp ignoring too small packet with length:", received );
	} else if ( errno != EINTR ) {
		ERROR(3)( Log::Error, "tcp recv reports error", errno, strerror(errno) );
	}
}

void TcpServer::listen( uint32 num ) {
	LOG(2)( LogDebug, "listen:", *this );
	CHECK( ::listen( fd, num ) );
}

void TcpServer::accept() {
	LOG(2)( LogDebug, "accept:", *this );
	struct sockaddr_in addr;
	RECVFROM_SIZE_T x = sizeof(addr);
	int new_fd = CHECK( ::accept( fd, (struct sockaddr*)&addr, &x ) );
	TcpFlow* flow = new TcpFlow( addr.sin_addr.s_addr, ntohs(addr.sin_port), new_fd );
	flow->cloneFrom( *this );
	acceptedFlows += 1;
	traffgen->addFlow( *flow );
	traffgen->addFlowHousekeeping( *flow );
}

TcpServer::TcpServer( uint32 la, uint16 lp ) : Flow(la,lp,0,0,IPPROTO_TCP),
	acceptedFlows(0) {
	setTransportHeaderSize( sizeof(tcphdr) );
	fd = CHECK( socket( AF_INET, SOCK_STREAM, 0 ) );
	bind();
	listen( 5 );
}

void TcpServer::receive( uint16 ) {
	accept();
}

void TcpServer::showReceiveStats() {
	ERROR(4)( LogStats, *this, "accepted", acceptedFlows, "connection(s)" );
}

UdpFlow::UdpFlow( uint32 la, uint16 lp, uint32 ra, uint16 rp, uint16 pr )
	: Flow(la,lp,ra,rp,IPPROTO_UDP,pr) {
	setTransportHeaderSize( sizeof(udphdr) );
	fd = CHECK( socket( AF_INET, SOCK_DGRAM, 0 ) );
	bind();
	if ( pr == 1 ) connect();
	long arg = CHECK( fcntl( fd, F_GETFL ) ) | O_NONBLOCK;
	CHECK( fcntl( fd, F_SETFL, arg ) );
}

void ActionObject::start() {
                                                            assert( !running );
	LOG(2)( LogActions, "starting", *this );
	running = true;
	traffgen->actionStarted();
}

void ActionObject::stop() {
                                                             assert( running );
	running = false;
	LOG(2)( LogActions, "stopped", *this );
	if ( endNotification ) endNotification(context);
	traffgen->actionFinished();
}

template <class T>
void TimeoutTimerRSVP<T>::internalFire() {
	this->cancel();
	this->object.rsvpTimeout();
}

void Sender::startSending() {
	nextPacketTime -= startTime;
	startTime = TrafficGenerator::getCurrentTime();
	nextPacketTime += startTime;
	rsvpTimer.cancel();
	rsvpWaiting = false;
	timer.startAbs( nextPacketTime );
}

void Sender::start() {
	ActionObject::start();
	startTime = TrafficGenerator::getCurrentTime();
	if ( rapi ) {
		TSpec x; if ( tspec == x ) calculateTSpec();
		if ( syncTime < TimeValue(0,0) ) {
			LOG(4)( LogActions, "signalling RSVP for", *this, "data start in", rsvpTime );
			startTime += rsvpTime;
		} else {
			LOG(3)( LogActions, "signalling RSVP for", *this, "data start synchronized" );
			flow.setSender(this);
			startTime = TimeValue(0,0);
			rsvpTimer.startRel( rsvpTime );
			rsvpWaiting = true;
		}
		flow.signalSenderRSVP( *rapi, tspec );
	}
	nextPacketTime = startTime;
	if ( getNextPacket( true ) ) {
		if ( startTime != TimeValue(0,0) ) timer.startAbs( nextPacketTime );
	} else {
		if ( startTime == TimeValue(0,0) ) flow.setSender(NULL);
		stop();
	}
}

void Sender::stop() {
	endTime = TrafficGenerator::getCurrentTime();
	if (rapi) flow.signalEndRSVP();
	timer.cancel();
	flow.showSendStats();
	flow.resetSend();
	ActionObject::stop();
}

void Sender::enableSignallingRSVP( RSVP_API& r, const TSpec& ts, const TimeValue& t, const TimeValue& s ) {
	this->rapi = &r;
	tspec = ts;
	rsvpTime = t;
	syncTime = s;
}

inline void Sender::rsvpTimeout() {
	if ( rsvpWaiting ) {
		LOG(2)( LogOther, "RSVP timeout:", *this );
		rsvpWaiting = false;
		if ( syncTime > TimeValue(0,0) ) {
			flow.signalEndRSVP();
			rsvpTimer.startRel( syncTime.multFloat( _exponent_() ) );
		} else {
			stop();
		}
	} else {
		rsvpTimer.startRel( rsvpTime );
		rsvpWaiting = true;
		LOG(3)( LogActions, "signalling RSVP (again) for", *this, "data synchronized" );
		flow.signalSenderRSVP( *rapi, tspec );
	}
}

inline TimeValue Sender::timeout() {
                                               assert(packetSize <= 65535);
	bool sendSuccess = flow.send( buffer, packetSize );
	if ( getNextPacket( sendSuccess ) ) {
		return nextPacketTime;
	} else {
		stop();
		return TimeValue(0,0);
	}
}

void Sender::Print( ostream& os ) const {
	os << "Sender " << flow;
}

bool TraceSender::getNextPacket( bool sendSuccess ) {
	if ( iter == packetList.end() ) {
		return false;
	}
	nextPacketTime = (*iter)[nextField].packetTime + startTime;
	packetSize = (*iter)[nextField].data;
	nextField += 1;
	if ( nextField >= (*iter).next() ) {
		++iter;
		nextField = 0;
	}
	return true;
}

TraceSender::TraceSender( Flow& f, const String& filename ) : Sender(f), nextField(0), filename(filename) {
	ifstream ifs( filename.chars() );
	if ( ifs.bad() ) {
		cerr << "ERROR: cannot access trace file " << filename << endl;
	return;
	}
	double t;
	uint16 ps;
	packetList.push_back( PacketChunk() );
	while ( ifs >> t >> ps ) {
		if ( packetList.back().full() ) {
			packetList.push_back( PacketChunk() );
		}
		packetList.back().addPacket( Packet( t, ps ) );
	}
	iter = packetList.end();
}

void TraceSender::start() {
	iter = packetList.begin();
	nextField = 0;
	Sender::start();
}

void TraceSender::stop() {
	iter = packetList.end();
	nextField = packetList.back().next();
	Sender::stop();
}

void TraceSender::calculateTSpec() {
	cerr << "ERROR: TraceSender cannot calculate tspec" << endl;
	return;
}

void TraceSender::Print( ostream& os ) const {
	os << "Trace (" << filename << ") ";
	Sender::Print(os);
}

bool CbrSender::getNextPacket( bool sendSuccess ) {
	nextPacketTime += interPacketTime;
	if ( inputFile ) {
		if ( !inputFile->read( buffer, packetLength ) ) {
			return false;
		}
	}
	if ( nextPacketTime - startTime > duration ) {
		return false;
	}
	return true;
}

void CbrSender::start() {
	packetSize = packetLength;
	Sender::start();
}

void CbrSender::stop() {
	packetSize = 0;
	Sender::stop();
}

void CbrSender::calculateTSpec() {
	uint16 rawPacketLength = packetLength + flow.getHeaderOverhead();
	tspec.set_r( rawPacketLength * TimeValue(1,0)/interPacketTime );
	tspec.set_b( rawPacketLength * TimeValue(1,0)/(tbDepthFactor*interPacketTime) );
	tspec.set_p( tspec.get_r() );
	tspec.set_m( rawPacketLength );
	tspec.set_M( rawPacketLength );
}

void CbrSender::Print( ostream& os ) const {
	os << "CBR (" << duration << ", " << TimeValue(1,0)/interPacketTime
		<< "pkts/sec a " << packetLength << "bytes) ";
	Sender::Print(os);
}

bool GreedySender::getNextPacket( bool sendSuccess ) {
	if ( !sendSuccess ) {
		nextPacketTime += interPacketTime;
	} else {
		nextPacketTime = TrafficGenerator::getCurrentTime();
	}
	if ( duration != TimeValue(0,0) && nextPacketTime - startTime > duration ) {
		return false;
	}
	if ( totalSize != 0 ) {
		if ( remainingSize <= packetLength ) {
			return false;
		} else {
			remainingSize -= packetLength;
		}
	}
	return true;
}

void GreedySender::start() {
	packetSize = packetLength;
	Sender::start();
}

void GreedySender::stop() {
	packetSize = 0;
	Sender::stop();
}

void GreedySender::calculateTSpec() {
	uint16 rawPacketLength = packetLength + flow.getHeaderOverhead();
	tspec.set_r( rawPacketLength * TimeValue(1,0)/interPacketTime );
	tspec.set_b( rawPacketLength * TimeValue(1,0)/(tbDepthFactor*interPacketTime) );
	tspec.set_p( tspec.get_r() );
	tspec.set_m( rawPacketLength );
	tspec.set_M( rawPacketLength );
}

void GreedySender::Print( ostream& os ) const {
	os << "Greedy (" << duration << ", " << remainingSize << " of " << totalSize << " bytes) ";
	Sender::Print(os);
}

bool ParetoSender::getNextPacket( bool sendSuccess ) {
	if ( nextPacketTime - startTime > duration ) {
		return false;
	}
	PacketTrace t = AG->GetNextPacket();
	nextPacketTime.getFromFraction(t.ByteStamp / capacity);
	nextPacketTime += startTime;
	packetSize = t.PacketSize;
	return true;
}

ParetoSender::ParetoSender( Flow& f, uint32 packetLength, uint32 packetRate,
	const TimeValue& d, uint32 sourceCount, double hurst, uint32 interPacketGap )
	: Sender(f), duration(d), AG(new Generator), load(0), capacity(125),
		avgPacketLength(0) {
	while ( capacity < packetLength * packetRate ) capacity *= 10;
	load = double(packetLength * packetRate) / capacity;
	for ( uint32 i = 0 ; i < sourceCount; i += 1 ) {
		AG->AddSource( new SourcePareto( i, 0, packetLength, interPacketGap, load/sourceCount, 3.0-2.0*hurst, 3.0-2.0*hurst ) );
		avgPacketLength += packetLength;
	}
	avgPacketLength /= sourceCount;
}  

ParetoSender::~ParetoSender() {
	Source* psrc = NULL;
	for ( psrc = AG->RemoveSource(); psrc; psrc = AG->RemoveSource() ) {
		delete psrc;
	}
	delete AG;
}

void ParetoSender::start() {
	AG->Reset();
	Sender::start();
}

void ParetoSender::stop() {
	Sender::stop();
}

void ParetoSender::calculateTSpec() {
	uint16 rawPacketLength = avgPacketLength + flow.getHeaderOverhead();
	uint32 rawRate = (uint32)(load*capacity / avgPacketLength) * rawPacketLength;
	tspec.set_r( rawRate );
	tspec.set_b( rawRate/tbDepthFactor );
	tspec.set_p( rawRate );
	tspec.set_m( rawPacketLength );
	tspec.set_M( rawPacketLength );
}

void ParetoSender::Print( ostream& os ) const {
	os << "Pareto (" << duration << ", avg:" << avgPacketLength << "bytes, " << setprecision(0) << (load*capacity*8) << " bit/s)";
	Sender::Print(os);
}

inline void SenderVariation::notify( SenderVariation *This ) {
	This->stop();
}

SenderVariation::SenderVariation( CbrSender& s, const TimeValue& period )
	: sender(s), timer(*this,period) {
	timer.cancel();
	packetRates.push_back( TimeValue(1,0) / s.getInterPacketTime() );
}

void SenderVariation::start() {
	ActionObject::start();
	sender.registerNotification( (Notification)notify, this );
	currentRate = packetRates.begin();
	timer.restart();
	sender.start();
}

void SenderVariation::stop() {
	timer.cancel();
	sender.registerNotification( NULL, NULL );
	if ( sender.isRunning() ) sender.stop();
	ActionObject::stop();
}

inline void SenderVariation::refresh() {
	++currentRate;
	if ( currentRate == packetRates.end() ) {
		currentRate = packetRates.begin();
	}
	LOG(5)( LogActions, "switching", sender, "to", *currentRate, "packets/sec" );
	sender.setInterPacketTime( TimeValue(1,0) / *currentRate );
}

void SenderVariation::addPacketRate( uint32 p ) {
	packetRates.push_back( p );
}

void SenderVariation::addPacketRates( const SimpleList<uint32>& rl ) {
	SimpleList<uint32>::ConstIterator iter = rl.begin();
	for ( ; iter != rl.end(); ++iter ) {
		packetRates.push_back( *iter );
	}
}

void SenderVariation::Print( ostream& os ) const {
	os << "SenderVariation ( " << timer.getPeriod() << ",";
	SimpleList<uint32>::ConstIterator iter = packetRates.begin();
	for ( ; iter != packetRates.end(); ++iter ) {
		os << " " << *iter;
	}
	os << ")" << " of " << sender;
}

inline void DelayAction::notify( DelayAction* This ) {
	This->action.registerNotification( NULL, NULL );
	This->ActionObject::stop();
}

DelayAction::DelayAction( ActionObject& o, const TimeValue& t )
	: action(o), delay(t), timer(*this) {}

void DelayAction::start() {
	ActionObject::start();
	timer.startRel( delay );
}

void DelayAction::stop() {
	timer.cancel();
	action.registerNotification( NULL, NULL );
	if ( action.isRunning() ) action.stop();
	ActionObject::stop();
}

inline void DelayAction::timeout() {
	action.registerNotification( (Notification)notify, this );
	action.start();
}

void DelayAction::Print( ostream& os ) const {
	os << "Delay (" << delay << "): " << action;
}

inline void RepeatAction::notify( RepeatAction* This ) {
	This->repeat();
}

void RepeatAction::start() {
	ActionObject::start();
	action.registerNotification( (Notification)notify, this );
	repeat();
}

void RepeatAction::stop() {
	action.registerNotification( NULL, NULL );
	if ( action.isRunning() ) action.stop();
	ActionObject::stop();
}

inline void RepeatAction::repeat() {
	if ( repeatCount > 0 ) {
		action.start();
		repeatCount -= 1;
	} else {
		action.registerNotification( NULL, NULL );
		ActionObject::stop();
	}
}

void RepeatAction::Print( ostream& os ) const {
	os << "Repeat " << repeatCount << " times: " << action;
}

inline void StopAction::notify( StopAction* This ) {
	This->action.registerNotification( NULL, NULL );
	This->actionSet.addAction( This->action );
	This->actionSet.removeStopAction( This->iter );
	delete This;
}

StopAction::StopAction( ActionSet& as, ActionObject& o, const TimeValue& d )
	: actionSet(as), action(o), timer(*this,d) {}

StopAction::StopAction( ActionSet& as, Sender& o )
	: actionSet(as), action(o), timer(*this) {
		action.registerNotification( (Notification)notify, this );
}

inline void StopAction::timeout() {
	action.registerNotification( NULL, NULL );
	action.stop();
	actionSet.addAction( action );
	actionSet.removeStopAction( iter );
	delete this;
}

ActionSet::ActionSet( uint32 cc, const TimeValue& a, const TimeValue& d, bool fa, bool fd )
	: number(++counter), createCount(cc), arrival(a), duration(d),
		fixedArrival(fa), fixedDuration(fd), timer(*this), created(0) {}

void ActionSet::start() {
	created = 0;
	ActionObject::start();
	nextStart = TrafficGenerator::getCurrentTime();
	timer.startNow();
}

void ActionSet::stop() {
	timer.cancel();
	StopActionList::Iterator iter = stopActions.begin();
	while ( iter != stopActions.end() ) {
		StopAction* s = *iter;
		++iter;
		s->timeout();
	}
}

void ActionSet::addAction( ActionObject& a ) {
	actions.push_back( &a );
}

inline void ActionSet::removeStopAction( SimpleList<StopAction*>::Iterator iter ) {
	stopActions.erase( iter );
	if ( stopActions.empty() && !timer.isActive() ) {
		LOG(2)( LogOther, *this, "no more running actions" );
		ActionObject::stop();
	}
}

inline TimeValue ActionSet::getArrival() {
	if ( fixedArrival ) return arrival;
	return arrival.multFloat( _exponent_() );
}

inline TimeValue ActionSet::getDuration() {
	if ( fixedDuration ) return duration;
	else {
		double x = _exponent_();
		if ( x < 0.10 ) x = 0.10;
		else if ( x > 4.0 ) x = 4.0;
		return duration.multFloat( x );
	}
}

inline TimeValue ActionSet::timeout() {
	if ( !actions.empty() ) {
		StopAction* s;
		ActionObject* a = actions.front();
		actions.pop_front();
		if ( typeid(*a) == typeid(CbrSender) ) {
			reinterpret_cast<CbrSender*>(a)->setDuration( getDuration() );
			s = new StopAction( *this, *reinterpret_cast<CbrSender*>(a) );
		} else if ( typeid(*a) == typeid(TraceSender) && duration == TimeValue(0,0) ) {
			s = new StopAction( *this, *reinterpret_cast<TraceSender*>(a) );
		} else if ( typeid(*a) == typeid(ParetoSender) ) {
			reinterpret_cast<ParetoSender*>(a)->setDuration( getDuration() );
			s = new StopAction( *this, *reinterpret_cast<ParetoSender*>(a) );
		} else {
			s = new StopAction( *this, *a, getDuration() );
		}
		created += 1;
		s->setIterator( stopActions.insert( stopActions.end(), s ) );
		s->start();
	} else {
		ERROR(1)( Log::Error, "ERROR: cannot find available action" );
	}
	if ( created >= createCount ) {
		LOG(2)( LogOther, *this, "finished creating all actions" );
		return TimeValue(0,0);
	} else {
		nextStart += getArrival();
		return nextStart;
	}
}

void ActionSet::Print( ostream& os ) const {
	os << "ActionSet(" << number << "): ";
	os << arrival << (fixedArrival ? "(f)" : "(e)") << "/";
	os << duration << (fixedDuration ? "(f)" : "(e)") << ", ";
	os << createCount << "/" << created;
}

void Network::init( uint32 qlimit ) {
	interfaceSet.clear();
	stamp_fd = -1;
	kernelmod_fileid = 0;

#if defined(FreeBSD)
	// store dropping stats
	size_t len = sizeof(ip_drops_start);
	CHECK( sysctlbyname( "net.inet.ip.intr_queue_drops", &ip_drops_start, &len, NULL, 0) );
//	LOG(2)( LogOther, "ip queue drops before start: ", ip_drops_start );

	struct udpstat udpstat;
	len = sizeof(udpstat);
	CHECK( sysctlbyname("net.inet.udp.stats", &udpstat, &len, NULL, 0) );
//	LOG(2)( LogOther, "udp full socket drops before start: ", udpstat.udps_fullsock );
	udp_drops_start = udpstat.udps_fullsock;
#endif

	// find all network interfaces

	int query_fd = CHECK( socket( AF_INET, SOCK_DGRAM, 0 ) );
	int trylength = 10;
	int lastlength = 0;
	struct ifconf iflist; 
	for (;;) {
		iflist.ifc_len = trylength * sizeof(ifreq);
		iflist.ifc_req = new struct ifreq[trylength];
#if defined(FreeBSD)
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
	LOG(3)( LogOther, "detected", interfaceCount, "interfaces" );
	uint32 i = 0;
	for ( ; i < interfaceCount; i += 1 ) {
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
		interfaceSet.insert_unique( Interface(addr, query_if.ifr_name) );
	}
	LOG(3)( LogOther, "detected", interfaceSet.size(), "real interfaces" );
	delete [] iflist.ifc_req;

#if defined(ENABLE_ALTQ)
	// load, attach and enable ALTQ stamping on all interfaces	
	stamp_fd = open( STAMP_DEVICE, O_RDWR );
#if defined(HAVE_KLD)
	if ( stamp_fd < 0 ) {
		struct stat sbuf;
		if ( stat( "/modules/altq_stamp.ko", &sbuf ) >= 0 ) {
			kernelmod_fileid = kldload("/modules/altq_stamp.ko");
		}
		stamp_fd = open( STAMP_DEVICE, O_RDWR );
	}
#endif
	if ( stamp_fd < 0 ) {
		ERROR(1)( Log::Error, "cannot open " STAMP_DEVICE " -> no kernel-based stamping" );
	} else {
		InterfaceSet::Iterator iter = interfaceSet.begin();
		for ( ; iter != interfaceSet.end(); ++iter ) {
			LOG(4)( LogOther, "enabling kernel-based stamping on interface", (*iter).name, "  limit:", qlimit );
			struct stamp_interface stamp_if;
			initMemoryWithZero( &stamp_if, sizeof(stamp_if) );
			strncpy( stamp_if.ifname, (*iter).name.chars(), IFNAMSIZ );
			CHECK( ioctl(stamp_fd, STAMP_IF_ATTACH, &stamp_if) );
			CHECK( ioctl(stamp_fd, STAMP_ENABLE, &stamp_if) );
			struct stamp_config stamp_conf;
			copyMemory( &stamp_conf, &stamp_if, sizeof(stamp_if) );
			stamp_conf.stamp_limit = qlimit;
			CHECK( ioctl(stamp_fd, STAMP_CONFIG, &stamp_conf ) );
		}
	}
#endif /* ENABLE_ALTQ */
}

void Network::cleanup() {
#if defined(ENABLE_ALTQ)
	// disable, detach and unload ALTQ stamping on all interfaces
	if ( stamp_fd >= 0 ) {
		InterfaceSet::Iterator iter = interfaceSet.begin();
		for ( ; iter != interfaceSet.end(); ++iter ) {
			struct stamp_interface stamp_if;
			initMemoryWithZero( &stamp_if, sizeof(stamp_if) );
			strncpy( stamp_if.ifname, (*iter).name.chars(), IFNAMSIZ );
			CHECK( ioctl(stamp_fd, STAMP_DISABLE, &stamp_if) );
			CHECK( ioctl(stamp_fd, STAMP_IF_DETACH, &stamp_if) );
		}
		close( stamp_fd );
	}
#if defined(HAVE_KLD)
	if (kernelmod_fileid) kldunload(kernelmod_fileid);
#endif /* HAVE_KLD */
#endif /* ENABLE_ALTQ */

#if defined(FreeBSD)
	// show dropping stats
	int ip_drops_end;
	size_t len = sizeof(ip_drops_end);
	CHECK( sysctlbyname( "net.inet.ip.intr_queue_drops", &ip_drops_end, &len, NULL, 0) );
//	LOG(2)( LogOther, "ip queue drops after end: ", ip_drops_end );

	struct udpstat udpstat;
	len = sizeof(udpstat);
	CHECK( sysctlbyname("net.inet.udp.stats", &udpstat, &len, NULL, 0) );
//	LOG(2)( LogOther, "udp full socket drops after end: ", udpstat.udps_fullsock );
	
	ERROR(4)( LogStats, "IP queue drops:", ip_drops_end - ip_drops_start, "UDP socket drops:", udpstat.udps_fullsock - udp_drops_start );
#endif
}

#if defined(ENABLE_ALTQ)
bool Network::findInterface( uint32 addr, struct stamp_interface& stamp_if ) {
	InterfaceSet::Iterator iter = interfaceSet.find( Interface( addr, "" ) );
	if ( iter == interfaceSet.end() ) {
		ERROR(2)( Log::Error, "cannot find interface for local address:", inet_ntoa( *(in_addr*)&addr ) );
		return false;
	} else {
		strncpy( stamp_if.ifname, (*iter).name.chars(), IFNAMSIZ );
		return true;
	}
}
#endif

bool Network::findInterface( uint32 addr ) {
	InterfaceSet::Iterator iter = interfaceSet.find( Interface( addr, "" ) );
	return (iter != interfaceSet.end());
}

Housekeeping::~Housekeeping() {
	ActionList::Iterator aiter = actionList.begin();
	while ( aiter != actionList.end() ) {
		delete (*aiter);
		aiter = actionList.erase( aiter );
	}
	FlowList::Iterator fiter = flowList.begin();
	while ( fiter != flowList.end() ) {
		delete (*fiter);
		fiter = flowList.erase( fiter );
	}
	SummaryObjectList::Iterator siter = summaryObjectList.begin();
	while ( siter != summaryObjectList.end() ) {
		delete (*siter);
		siter = summaryObjectList.erase( siter );
	}
}

void Housekeeping::addAction( ActionObject& a ) {
	actionList.push_back( &a );
}

void Housekeeping::addFlow( Flow& f ) {
	flowList.push_back( &f );
	f.enableReceiving();
}

void Housekeeping::addSummaryObject( SummaryObject& so ) {
	summaryObjectList.push_back( &so );
}

SummaryObject* Housekeeping::getSummaryObject( const String& name ) {
	SummaryObjectList::Iterator siter = summaryObjectList.begin();
	for ( ; siter != summaryObjectList.end(); ++siter ) {
		if ( (*siter)->getName() == name ) {
	return *siter;
		}
	}
	return NULL;
}

void Housekeeping::checkForConnectionFlows() {
	FlowList::Iterator fiter = flowList.begin();
	for ( ; fiter != flowList.end(); ++fiter ) {
		if ( typeid(**fiter) == typeid(TcpFlow) ) {
			reinterpret_cast<TcpFlow*>(*fiter)->startConnection();
		}
	}
}

void Housekeeping::showStats() {
	uint32 totalPacketRecv = 0;
	uint32 totalPacketRecvRSVP = 0;
	SummaryObjectList::Iterator iter = summaryObjectList.begin();
	for ( ; iter != summaryObjectList.end(); ++iter ) {
		(*iter)->showSummaryInformation();
		if ( typeid(**iter) == typeid(DelayCounter) ) {
			totalPacketRecvRSVP += reinterpret_cast<DelayCounter*>(*iter)->getPacketCountRSVP();
			totalPacketRecv += reinterpret_cast<DelayCounter*>(*iter)->getPacketCountNoRSVP();
		}
	}
	if ( totalPacketRecv || totalPacketRecvRSVP ) {
		ERROR(5)( LogStats, "receive summary:", totalPacketRecvRSVP, "RSVP packets", totalPacketRecv, "other packets" );
	}
}

inline void TrafficGenerator::setMask( fd_set& fdMask ) {
	static const int count = sizeof(fdMasterMask)/sizeof(int);
	int i = 0;
	for ( ; i < count; ++i ) {
		((int*)&fdMask)[i] = ((int*)&fdMasterMask)[i];
	}
}

TrafficGenerator::TrafficGenerator( uint32 slotlen, uint32 maxPacketRate )
	: endFlag(false), min_fd(FD_SETSIZE), max_fd(0), rsvp_fd(-1),
	rsvpNotify(NULL), rapi(NULL), actionsRunning(0), starter(*this),
	randomSeed(0) {
	initMemoryWithZero( flowArray, sizeof(flowArray) );
	FD_ZERO( &fdMasterMask );
	RSVP_Global::init();
	housekeeping = new Housekeeping;
#if !defined(NS2)
	TimerSystem::totalPeriod = TimeValue( 600, 0 );
	TimerSystem::slotLength = TimeValue( 0, slotlen );
	TimerSystem::slotCount = 0;
	RSVP_Global::currentTimerSystem = new TimerSystem;
	if ( maxPacketRate == 0 ) maxPacketRate = 20000;
	Network::init( maxPacketRate / (TimeValue(1,0) / RSVP_Global::currentTimerSystem->timerResolution) );
#endif
}

TrafficGenerator::~TrafficGenerator() {
	starter.cancel();
	Network::cleanup();
	delete housekeeping;
	delete RSVP_Global::currentTimerSystem;
}

bool TrafficGenerator::findInterface( uint32 addr ) {
	return Network::findInterface( addr );
}

void TrafficGenerator::addFlow( Flow& f ) {
	if ( f.getFD() < 0 || f.getFD() >= (int)FD_SETSIZE ) {
		cerr << "FATAL ERROR: illegal file descriptor " << f.getFD() << endl;
		exit(1);
	}
	if ( f.getFD() >= max_fd ) max_fd = f.getFD() + 1;
	if ( f.getFD() < min_fd ) min_fd = f.getFD();
	FD_SET( f.getFD(), &fdMasterMask );
	flowArray[f.getFD()] = &f;
}

void TrafficGenerator::addAction( ActionObject& a ) {
	actionList.push_back( &a );
}

void TrafficGenerator::addActionHousekeeping( ActionObject& a ) {
	housekeeping->addAction( a );
}

void TrafficGenerator::addSummaryObject( SummaryObject& so ) {
	housekeeping->addSummaryObject( so );
}

SummaryObject* TrafficGenerator::getSummaryObject( const String& s ) {
	return housekeeping->getSummaryObject( s );
}

void TrafficGenerator::registerRSVP_API( RSVP_API& ra, Notification rn ) {
	rsvp_fd = ra.getFileDesc();
	FD_SET( rsvp_fd, &fdMasterMask );
	if (rsvp_fd >= max_fd) max_fd = rsvp_fd + 1;
	rsvpNotify = rn;
	rapi = &ra;
}

void TrafficGenerator::unregisterRSVP_API() {
	FD_CLR( rsvp_fd, &fdMasterMask );
	rsvp_fd = -1;
	rsvpNotify = NULL;
	rapi = NULL;
}

void TrafficGenerator::main( const TimeValue& startTime, uint32 waitSecs ) {
	if (randomSeed != 0) _seed(randomSeed);
	RSVP_Global::currentTimerSystem->start();
	if ( startTime == TimeValue(0,0) ) {
		if ( waitSecs == 0 ) {
			timeout();
		} else {
			starter.startRel( TimeValue( waitSecs, 0 ) );
		}
	} else {
		if ( startTime <= getCurrentTime() ) {
			cerr << "ERROR: start time " << (DaytimeTimeValue&)startTime << " has already passed now: " << (DaytimeTimeValue&)getCurrentTime() << endl;
	return;
		}
		starter.startAbs( startTime + TimeValue( waitSecs, 0 ) );
	}
	static TimeValue remainingTime;
	static TimeValue* waitTime;
	static fd_set fdMask;
	while ( !endFlag ) {
		setMask( fdMask );
		TimeValue zeroTime(0,0);
		int fdCount = select( max_fd, &fdMask, NULL, NULL, &zeroTime );
		if ( fdCount == 0 ) {
			if ( RSVP_Global::currentTimerSystem->executeTimer(remainingTime) ) {
				waitTime = &remainingTime;
			} else {
				waitTime = NULL;
			}
			setMask( fdMask );
			fdCount = select( max_fd, &fdMask, NULL, NULL, waitTime );
		}
		if ( fdCount < 0 ) {
			if ( errno == EINTR ) {
	continue;
			} else {
				ERROR(3)( Log::Error, "select reports error", errno, strerror(errno) );
			}
		} else {
			if ( (rsvp_fd != -1) && FD_ISSET( rsvp_fd, &fdMask ) ) {
                                        assert( rsvpNotify ); assert( rapi );
				rsvpNotify(rapi);
				FD_CLR( rsvp_fd, &fdMask );
			}
			int i = min_fd;
			for ( ; i < max_fd; i += 1 ) {
				if ( FD_ISSET( i, &fdMask ) ) {
                                                          assert(flowArray[i]);
					flowArray[i]->receive();
				}
			}
		}
	} // while
	TimeValue timerDrift = RSVP_Global::currentTimerSystem->checkDrift();
	LOG(1)( LogOther, "traffic generator finishing now..." );
	ActionList::Iterator aiter = actionList.begin();
	for ( ; aiter != actionList.end(); ++aiter ) {
		if ( (*aiter)->isRunning() ) (*aiter)->stop();
	}
	housekeeping->showStats();
	int i = min_fd;
	for ( ; i < max_fd; i += 1 ) {
		if ( flowArray[i] ) flowArray[i]->showReceiveStats();
	}
	FlowList::Iterator fiter = flowReportList.begin();
	for ( ; fiter != flowReportList.end(); ++fiter ) {
		(*fiter)->showReceiveStats();
	}
	if ( timerDrift != TimeValue(0,0) ) {
		cerr << "WARNING: estimated timer system drift: " << timerDrift << endl;
	}
}

inline void TrafficGenerator::timeout() {
	cerr << "starting all actions..." << endl;
	housekeeping->checkForConnectionFlows();
	ActionList::Iterator aiter = actionList.begin();
	for ( ; aiter != actionList.end(); ++aiter ) {
		(*aiter)->start();
	}
}

inline void TrafficGenerator::removeFD( int fd ) {
	FD_CLR( fd, &fdMasterMask );
	if ( flowArray[fd] != NULL ) {
		flowReportList.push_back( flowArray[fd] );
		flowArray[fd] = NULL;
	}
}

inline void TrafficGenerator::actionStarted() {
	actionsRunning += 1;
}

inline void TrafficGenerator::actionFinished() {
	actionsRunning -= 1;
	if ( actionsRunning == 0 ) {
		cerr << "All actions finished" << endl;
	}
}

inline const TimeValue& TrafficGenerator::getCurrentTime() {
	return RSVP_Global::currentTimerSystem->getCurrentTime();
}

} // namespace TG
