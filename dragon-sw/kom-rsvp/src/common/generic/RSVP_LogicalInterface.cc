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
#include "RSVP_LogicalInterface.h"
#include "RSVP_Global.h"
#include "RSVP_Log.h"
#include "RSVP_Message.h"
#include "RSVP_NetworkService.h"
#include "RSVP_PacketHeader.h"
#include "RSVP_TrafficControl.h"
#if defined(NS2)
#include "RSVP_Global.h"
#include "RSVP_Wrapper.h"
#include "RSVP_API.h"
#endif

#if defined(WITH_API) || defined(VIRT_NETWORK)
const uint32 LogicalInterfaceUDP::lossProbabilityScale = 1000000;
#endif

void LogicalInterface::Print( ostream& os ) const {
	os << name << "(" << getLIH() << ") ";
	os << getLocalAddress();
	os << " (" << refreshInterval << ")";
	os << " MTU: " << MTU;
	if ( mpls_enabled ) os << " MPLS";
	if ( trafficControl ) os << " " << *trafficControl;
}

LogicalInterface::LogicalInterface( const String& name, const NetAddress&
	addr, uint32 MTU, sint32 sysIndex ) :
	RSVP_HOP_Object(addr,0), disabled(false), fd(-1), name(name),
	MTU(MTU), sysIndex(sysIndex), refreshInterval(defaultRefresh), vif(-1),
	clonedTC(false), trafficControl(NULL), obuffer(maxPayloadLength) {
	maxUnfragmentMsgSize = MTU - PacketHeader::maxOutputSize();
	mpls_enabled = RSVP_Global::mplsDefault && (sysIndex != -1);
#if defined(REFRESH_REDUCTION)
	rapidRefreshInterval = TimeValue(0,0);
	maxIdCount = (maxUnfragmentMsgSize - (MESSAGE_ID_LIST_Object::minSize() + Message::headerSize())) / MESSAGE_ID_LIST_Object::idSize();
#endif
}

LogicalInterface::~LogicalInterface() {
	if ( fd >= 0 ) NetworkService::shutdownInterface( fd );
	if ( !clonedTC && trafficControl ) delete trafficControl;
}

bool LogicalInterface::parseBuffer( INetworkBuffer& buffer, PacketHeader& header,  Message& msg ) const {
	if ( buffer.getRemainingSize() >= Message::headerSize() ) {
		buffer >> msg;
	} else {
		return false;
	}
	LOG(1)( Log::Msg, "****************   new message received   ****************" );
	if ( msg.getStatus() == Message::Drop ) {
		ERROR(5)( Log::Error, name, "ERROR in MSG from", header.getSrcAddress(), ":", msg );
		ERROR(3)( Log::Error, "packet dump:", endl, buffer );
		return false;
	} else {
		LOG(3)( Log::Packet, "packet dump:", endl, buffer );
	}
	LOG(5)( Log::Msg, name, "received MSG from", header.getSrcAddress(), ":", msg );
	if ( msg.getTTL() == 0 ) {
		LOG(1)( Log::Msg, "discarding message, because TTL is 0" );
		return false;
	}
#if defined(WITH_API)
	if ( getLIH() == 0 ) return true;
#endif
	msg.decrementTTL();
#if !defined(NS2)
	header.decrementTTL();
#endif
	return true;
}

ONetworkBuffer* LogicalInterface::createOutgoingBuffer( const Message& msg, const NetAddress& dest ) const {
#if defined(REFRESH_REDUCTION)
	const_cast<Message&>(msg).setFlags( Message::RefreshReduction );
#endif
	PacketHeader header;
	bool routerAlert = false;
	header.setSrcAddress( getAddress() );
	header.setDestAddress( dest );
	header.setFurtherInfo( msg.getLength(), msg.getTTL(), routerAlert );
	ONetworkBuffer* obuf = new ONetworkBuffer(msg.getLength()+header.outputSize());
	*obuf << header;
	*obuf << msg;
	return obuf;
}

void LogicalInterface::sendMessageInternal( const Message& msg, const NetAddress& dest, const NetAddress& src, const NetAddress& gw ) const {
                                                    assert( msg.getTTL() > 0 );
	if ( disabled ) return;
#if defined(REFRESH_REDUCTION)
#if defined(NS2)
	if (this != RSVP_API::apiLif)
#endif
	const_cast<Message&>(msg).setFlags( Message::RefreshReduction );
#endif
	LOG(5)( Log::Msg, name, "sends MSG to", dest, ":", msg );
	obuffer.init();
	PacketHeader header;
	bool routerAlert = false;
	header.setSrcAddress( src );
	header.setDestAddress( dest );
	header.setFurtherInfo( msg.getLength(), msg.getTTL(), routerAlert );
	obuffer << header;
	obuffer << msg;
	sendBuffer( obuffer, dest, gw );
}

void LogicalInterface::disable() {
	disabled = true;
	LOG(2)( Log::Config, "Disabling interface:", *this );
}

#if defined(WITH_API) || defined(VIRT_NETWORK)
void LogicalInterfaceUDP::Print( ostream& os ) const {
	LogicalInterface::Print( os );
	os << " [UDP:" << localPort;
	os << " <-> " << destAddr << ":";
	if ( destPortList.empty() ) {
		os << "0";
	} else {
		PortList::ConstIterator portIter = destPortList.begin();
		os << *portIter;
		++portIter;
		for (; portIter != destPortList.end(); ++portIter ) {
			os << "," << *portIter;
		}
	}
	os << "]";
	os << " loss:" << (ieee32float)(100*lossProb)/(ieee32float)lossProbabilityScale << "%";
}

void LogicalInterfaceUDP::configureUDP( uint16 localPort, const NetAddress& destAddr,
	const PortList& destPortList ) {
	this->destAddr = destAddr;
	this->localPort = localPort;
	this->destPortList = destPortList;
}

void LogicalInterfaceUDP::init( uint32 LIH ) {
	if ( disabled ) return;
#if defined(NS2)
                                                assert( RSVP_Global::wrapper );
	localPort = RSVP_Global::wrapper->getLocalPort();
#else
	fd = NetworkService::initInterfaceUDP( localPort );
#endif
	RSVP_HOP_Object::setLIH( LIH );
}

// only done at API client
void LogicalInterfaceUDP::initWithPort() {
	if ( disabled ) return;
#if defined(NS2)
                                                assert( RSVP_Global::wrapper );
	localPort = RSVP_Global::wrapper->getLocalPort();
#else
	fd = NetworkService::initInterfaceUDP( localPort );
#endif
	RSVP_HOP_Object::setLIH( localPort );
}

const LogicalInterface* LogicalInterfaceUDP::receiveBuffer( INetworkBuffer& buf, PacketHeader& header ) const {
	if ( disabled ) return NULL;
#if defined(NS2)
                                                assert( RSVP_Global::wrapper );
	RSVP_Global::wrapper->receivePacket( buf );
	buf >> header;
	return this;
#else
	if (NetworkService::receivePacket( fd, buf )) {
		buf >> header;
		return this;
	} else {
		header.init();
		return NULL;
	}
#endif
}

void LogicalInterfaceUDP::sendBuffer( const ONetworkBuffer& obuf, const NetAddress& dest, const NetAddress& ) const {
#if defined(NS2)
                                                assert( RSVP_Global::wrapper );
	RSVP_Global::wrapper->sendPacket( obuffer, RSVP_Global::wrapper->getNodeAddr(), tmpDestPort );
#else
	if ( tmpDestPort != 0 ) {
		if ( drawRandomNumber( lossProbabilityScale - 1 ) < lossProb ) {
			LOG(2)( Log::Msg, "simulating loss to", dest );
		} else {
			NetworkService::sendPacket( fd, obuf, (destAddr ? destAddr : dest), tmpDestPort );
		}
		tmpDestPort = 0;
	} else {
		PortList::ConstIterator portIter = destPortList.begin();
		for (; portIter != destPortList.end(); ++portIter ) {
			if ( drawRandomNumber( lossProbabilityScale ) < lossProb ) {
				LOG(2)( Log::Msg, "simulating loss to", dest );
			} else {
				NetworkService::sendPacket( fd, obuf, (destAddr ? destAddr : dest), *portIter );
			}
		}
	}
#endif
}
#endif /* defined(WITH_API) || defined(VIRT_NETWORK) */
