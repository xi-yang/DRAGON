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
#ifndef _RSVP_RoutingService_h_
#define _RSVP_RoutingService_h_ 1

#include "RSVP_BasicTypes.h"
#include "RSVP_Lists.h"
#include "RSVP_LogicalInterfaceSet.h"

class RoutingEntry {
	NetAddress	dest;
	NetAddress	mask;
	NetAddress	gw;
	const LogicalInterface*	iface;
	RoutingEntry( const NetAddress& dest, const NetAddress& mask, const NetAddress& gw, const LogicalInterface* iface )
	: dest(dest), mask(mask), gw(gw), iface(iface) {}
	friend class RoutingService;
	friend class ConfigFileReader;
	friend ostream& operator<< (ostream&, const RoutingEntry& );
public:
	const LogicalInterface* getInterface() const { return iface; }
	const NetAddress& getGW() const { return gw; }
};

class RSRR;
class RoutingEntryList;

typedef struct _vlsr_route_{
	NetAddress switchID;
	uint32 inPort;
	uint32 outPort;
	uint32 vlanTag;
	float bandwidth;
}VLSR_Route;
typedef SimpleList<VLSR_Route> VLSRRoute;

class RoutingService {
	RSRR* rsrr;
	RoutingEntryList* rtList;
	pid_t mainPID;
	int ospf_socket;
#if defined(Linux)
	mutable uint32 queryCounter;
#else
	mutable sint32 queryCounter;
#endif
	void maskLength2IP (int masklen, NetAddress& netmask) const;
	void addRoute( const RoutingEntry& rte );
	bool getRoute( const NetAddress&, NetAddress& nexthop, NetAddress& remote_nexthop,  NetAddress& gateway ) const;
	void getVirtualRoute( const NetAddress&, LogicalInterfaceSet&, NetAddress& gateway ) const;
	bool sendRouteRequest( const NetAddress& dest ) const;
	const LogicalInterface* getRouteReply( NetAddress& dest, NetAddress& gateway, bool async = false ) const;
	friend class ConfigFileReader;
#if defined(Linux) && defined(REAL_NETWORK)
	void doRouteModification( bool add, const NetAddress&, const LogicalInterface* = NULL, const NetAddress& = 0, uint32 = 0 );
#endif	
public:
	enum OspfRsvpMessage {
		OspfResv = 2,
		OspfPathTear = 5,
		OspfResvTear = 6,
		GetExplicitRouteByOSPF = 128,	//Get explicit route from OSPF
		FindInterfaceByData = 129,		//Find control logical interface by data plane IP / interface ID
		FindDataByInterface = 130,  		//Find data plane IP / interface ID by control logical interface
		FindOutLifByOSPF = 131,			//Find outgoing control logical interface by next hop data plane IP / interface ID
		GetVLSRRoutebyOSPF = 132,		//Get VLSR route
		GetLoopbackAddress = 133, 		// Get its loopback address
		HoldVtagbyOSPF = 134, 		// Hold or release a VLAN Tag
		HoldBandwidthbyOSPF = 135, 		// Hold or release a portion of bandwidth
	};
	RoutingService();
	~RoutingService();
	const int getOspfSocket() const { return ospf_socket; }
	bool ospf_socket_init ();
	void getPeerIPAddr(const NetAddress& myAddr, NetAddress& peerAddr) const;
	void init( LogicalInterfaceList& tmpLifList );
	void init2();
	EXPLICIT_ROUTE_Object* getExplicitRouteByOSPF(const NetAddress& src, const NetAddress& dest, const SENDER_TSPEC_Object& sendTSpec, const LABEL_REQUEST_Object& labelReq);
	const LogicalInterface* findInterfaceByData( const NetAddress& ip, const uint32 ifID = 0);
	bool findDataByInterface(const LogicalInterface& lif, NetAddress& ip, uint32& ifID);
	const void notifyOSPF(uint8 msgType, const NetAddress& ctrlIfIP, ieee32float bw  );
	const LogicalInterface* findOutLifByOSPF( const NetAddress& , const uint32 , NetAddress& );
	const void getVLSRRoutebyOSPF(const NetAddress& inRtID, const NetAddress& outRtID, const uint32 inIfId, const uint32 outIfId, VLSR_Route& vlsr);
	const void holdBandwidthbyOSPF(u_int32_t port, float bw, bool hold = true);
	const void holdVtagbyOSPF(u_int32_t vtag, bool hold = true);
	NetAddress getLoopbackAddress();
	const LogicalInterface* getUnicastRoute( const NetAddress&, NetAddress& );
	const LogicalInterface* getMulticastRoute( const NetAddress&, const NetAddress&, LogicalInterfaceSet& );
	bool getAsyncMulticastRoutingEvent( NetAddress&, NetAddress&, const LogicalInterface*&, LogicalInterfaceSet& );
	const LogicalInterface*  getAsyncUnicastRoutingEvent( NetAddress&, NetAddress& );
	bool isMulticastRouter() { return rsrr != NULL; }
#if defined(Linux) && defined(REAL_NETWORK)
	void addUnicastRoute( const NetAddress& dest, const LogicalInterface* lif = NULL, const NetAddress& gw = 0, uint32 handle = 0 ) {
		doRouteModification( true, dest, lif, gw, handle );
	}
	void delUnicastRoute( const NetAddress& dest, const LogicalInterface* lif = NULL, const NetAddress& gw = 0 ) {
		doRouteModification( false, dest, lif, gw, 0 );
	}
#endif
};

#define CheckOspfSocket(function) \
	if (!ospf_socket){ \
		if (!ospf_socket_init()){ \
			LOG(1)( Log::Error, "checkOspfSocket: cannot connect to OSPFd..."); \
		} \
	} \
	CHECK(function); \
	if (CHECK_result < 0) { \
		CHECK(close(ospf_socket)); \
		ospf_socket = 0; \
	} \

#endif /* _RSVP_RoutingService_h_ */

