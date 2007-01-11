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
#ifndef _RSVP_NetworkServiceDaemon_h_
#define _RSVP_NetworkServiceDaemon_h_ 1

#include "RSVP_BasicTypes.h"
#include "RSVP_Lists.h"

class NetworkServiceDaemon {

	// interface detection and query
	static uint32	loopbackInterfaceIndex;
	static const LogicalInterface* globalVirtualInterface;
	static const LogicalInterface** indexToInterfaceTable;
	static uint32 packetDropsAtStart;
	static inline void set_fdMask( InterfaceHandleMask& fdmask );
	static void buildInterfaceList( LogicalInterfaceList& );
	static const LogicalInterface* queryInterfaces();
	static void cleanup();
	static uint32 getLoopbackInterfaceIndex() { return loopbackInterfaceIndex; }
	static const LogicalInterface* getInterfaceBySystemIndex( uint16 index ) {
		return indexToInterfaceTable[index];
	}

	// multicast routing
	static InterfaceHandle rsrrSocket;
	static bool rsrrReady;
	static void registerRSRR_Handle( InterfaceHandle );
	static void deregisterRSRR_Handle( InterfaceHandle );
	static bool queryAndClearAsyncMulticastRouting() {
		bool retval = rsrrReady; rsrrReady = false; return retval;
	}

	// unicast routing
	static InterfaceHandle routingSocket;
	static bool routingReady;
	static void registerRouting_Handle( InterfaceHandle );
	static void deregisterRouting_Handle( InterfaceHandle );
	static bool queryAndClearAsyncUnicastRouting() {
		bool retval = routingReady; routingReady = false; return retval;
	}

	static void registerApiClient_Handle( InterfaceHandle);
	static void deregisterApiClient_Handle( InterfaceHandle);

	friend class RSVP;                                  // access: buildInterfaceList,queryAndClearAsyncRouting,queryInterfaces,cleanup
	friend class RSRR;                                  // access: registerRSRR_Handle, deregisterRSRR_Handle
	friend class RoutingService;                        // access: registerRouting_Handle, deregisterRouting_Handle, getInterfaceBySystemIndex
public:
	// interface configuration
	static InterfaceHandle initRawInterfaceIP4( const NetAddress& );
	static void initRawUnicastInterfaceIP4( InterfaceHandle, const NetAddress& );
	static void initRawMulticastInterfaceIP4( InterfaceHandle, const NetAddress&, VifHandle );
	// packet handling
	static const LogicalInterface* receiveRawPacketIP4( InterfaceHandle, INetworkBuffer& );
	static void sendRawPacketIP4( InterfaceHandle, const ONetworkBuffer&, const NetAddress&, const NetAddress& );
};

#endif /* _RSVP_NetworkServiceDaemon_h_*/
