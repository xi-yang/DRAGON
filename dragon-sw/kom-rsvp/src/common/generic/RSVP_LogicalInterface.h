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
#ifndef _RSVP_LogicalInterface_h_
#define _RSVP_LogicalInterface_h_ 1

#include "RSVP_BasicTypes.h"
#include "RSVP_Lists.h"
#include "RSVP_ProtocolObjects.h"
#include "RSVP_String.h"
#include "RSVP_TimeValue.h"

class Message;
class TrafficControl;
class Hop;

typedef SimpleList<uint32> PortList;

#if defined(WITH_API) || defined(VIRT_NETWORK)
#define VIRTUAL virtual
#else
#define VIRTUAL
#endif

class LogicalInterface : public RSVP_HOP_Object {
protected:
	bool disabled;
	InterfaceHandle fd;

	String name;
	uint32 MTU;
	sint32 sysIndex;
	TimeValue refreshInterval;
	uint32 maxUnfragmentMsgSize;
#if defined(REFRESH_REDUCTION)
	uint32 maxIdCount;
	TimeValue rapidRefreshInterval;
#endif

private:
	VifHandle vif;
	bool clonedTC;
	TrafficControl* trafficControl;

	bool mpls_enabled;
	uint32 localId;
#if defined(NS2)
	String oif;
#endif

	friend inline ostream& operator<< (ostream&, const LogicalInterface& );
	LogicalInterface& operator=(const LogicalInterface & i);   //prohibit copying

	friend class NetworkServiceDaemon;                 // access: fd
	friend class RSVP_API;                             // access: fd
	//hacked @@@@
	friend class RSVP;                             // access: fd
protected:
	mutable ONetworkBuffer obuffer;
	VIRTUAL void Print( ostream& ) const;
	void setLIH( uint32 LIH );
	void setAddress( const NetAddress& a );
public:
	// all below: defined in RSVP_Global.cc
	static const TimeValue defaultRefresh;
#if defined(REFRESH_REDUCTION)
	static const TimeValue defaultRapidRefresh;
#endif
	static const uint32 maxPayloadLength = 65000;
	static const NetAddress loopbackAddress;
	static const NetAddress noGatewayAddress;

	LogicalInterface( const String& name, const NetAddress& addr, uint32 MTU, sint32 sysIndex = -1 );
	VIRTUAL ~LogicalInterface();

	// implemented in RSVP_LogicalInterfaceDaemon.cc
	VIRTUAL void init( uint32 LIH );
	void configureTC( TrafficControl* );
	VIRTUAL const LogicalInterface* receiveBuffer( INetworkBuffer&, PacketHeader& ) const;
	bool parseBuffer( INetworkBuffer&, PacketHeader&, Message& ) const;
	ONetworkBuffer* createOutgoingBuffer( const Message&, const NetAddress& ) const;
	VIRTUAL void sendBuffer( const ONetworkBuffer&, const NetAddress&, const NetAddress& ) const;
	void sendMessageInternal( const Message& msg, const NetAddress& dest, const NetAddress& src, const NetAddress& gw = noGatewayAddress ) const;
	VIRTUAL void sendMessage( const Message& msg, const NetAddress& dest, const NetAddress& src, const NetAddress& gw = noGatewayAddress ) const {
		sendMessageInternal( msg, dest, src, gw );
	}
	void sendMessage( const Message& msg, const NetAddress& dest ) const {
		sendMessage( msg, dest, getAddress() );
	}

	void configureRefresh( const TimeValue& refreshInterval ) { this->refreshInterval = refreshInterval; }
	void setVif( VifHandle vif ) { this->vif = vif; }
	bool hasVif() const { return vif != -1; }
	void disable();
	void setMPLS( bool m ) { mpls_enabled = m; }
	bool hasEnabledMPLS() const { return mpls_enabled; }
	bool isDisabled() const { return disabled; }
	const String& getName() const { return name; }
	const NetAddress& getLocalAddress() const { return RSVP_HOP_Object::getAddress(); }
	uint32 getLIH() const { return RSVP_HOP_Object::getLIH(); }	
	uint32 getMTU() const { return MTU; }
	sint32 getSysIndex() const { return sysIndex; }
	const TimeValue& getRefreshInterval() const { return refreshInterval; }
	uint32 getMaxUnfragmentMsgSize() const { return maxUnfragmentMsgSize; }
	TrafficControl& getTC() const { assert(trafficControl); return *trafficControl; }
	void setLocalId (String& lclId);
	const uint32 getLocalId () { return localId; }
#if defined(REFRESH_REDUCTION)
	void setRapidRefreshInterval( uint32 msec ) { rapidRefreshInterval = TimeValue(msec/MSECS_PER_SEC,msec*USECS_PER_MSEC); }
	const TimeValue& getRapidRefreshInterval() const { return rapidRefreshInterval; }
	uint32 getMaxIdCount() const { return maxIdCount; }
#endif
#if defined(NS2)
	void setOif( const String& o ) { oif = o; }
	const String& getOif() const { return oif; }
#endif
};

extern inline ostream& operator<< (ostream& os, const LogicalInterface& i ) {
	i.Print(os); return os;
}

extern inline bool Less<LogicalInterface*>::operator()( const LogicalInterface* l1, const LogicalInterface* l2 ) const {
	return l1->getLIH() < l2->getLIH();
}

#if defined(WITH_API) || defined(VIRT_NETWORK)
class LogicalInterfaceUDP : public LogicalInterface {
	NetAddress destAddr;
	uint16 localPort;
	PortList destPortList;
	uint32 lossProb;
	mutable uint16 tmpDestPort;
protected:
	VIRTUAL void Print( ostream& os ) const;
public:
	static const uint32 lossProbabilityScale;
	LogicalInterfaceUDP( const String& name, const NetAddress& addr, uint32 MTU )
		: LogicalInterface(name, addr, MTU), destAddr(0), localPort(0), lossProb(0),
		tmpDestPort(0) {}
	void configureUDP( uint16, const NetAddress&, const PortList& );
	VIRTUAL void init( uint32 LIH );
	void initWithPort();
	VIRTUAL const LogicalInterface* receiveBuffer( INetworkBuffer&, PacketHeader& ) const;
	VIRTUAL void sendBuffer( const ONetworkBuffer&, const NetAddress& dest, const NetAddress& ) const;
	void setDestPort( uint16 d ) { tmpDestPort = d; }
	void setLossProbability( uint32 l ) {
		lossProb = (l >= lossProbabilityScale ? 0 : l);
	}
};
#endif

#if defined(WITH_API)
class LogicalInterfaceAPI_Server : public LogicalInterfaceUDP {
public:
	LogicalInterfaceAPI_Server( const String& name, const NetAddress& addr, uint32 MTU )
		: LogicalInterfaceUDP(name, addr, MTU) {}
	// implemented in RSVP_API_Server.cc
	VIRTUAL void sendMessage( const Message& msg, const NetAddress&, const NetAddress&, const NetAddress& = noGatewayAddress ) const;
};
#endif

#undef VIRTUAL

#endif /* _RSVP_LogicalInterface_h_ */
