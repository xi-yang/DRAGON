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
#ifndef _RSVP_h_
#define _RSVP_h_ 1

#include "RSVP_BasicTypes.h"
#include "RSVP_Global.h"
#include "RSVP_Lists.h"
#include "RSVP_String.h"

class SESSION_Object;
class SCOPE_Object;
class FlowDescriptor;
class FLOWSPEC_Object;
class LogicalInterface;
class RoutingService;
class API_Server;
class OutISB;
class OIatPSB;
class MESSAGE_ID_Object;
class PSB;
class PHopSB;
class MPLS;

class RSVP {
	bool initOK;

#if defined(WITH_API)
	static API_Server* apiServer;
	static uint16 apiPort;
#endif

	SessionHash* sessionHash;

	const LogicalInterface** lifArray;
	HopList* hopListArray;
	uint32 interfaceCount;

	RoutingService* routing;
	bool endFlag;

	// statistical data only 
	uint32 currentSessionCount;
	uint32 maxSessionCount;
	uint32 currentReservationCount;

	MPLS* mpls;

#if defined(WITH_API)
	static void configureAPI( uint16 port ) {
		apiPort = port;
	}
#endif
  
	static void exitHandler( RSVP* );
	static void alarmHandler( RSVP* );
	static void statsHandler( RSVP* );
	static void statsHandlerReset( RSVP* );
	friend class ConfigFileReader;
#if defined(NS2)
	friend class RSVP_Wrapper;
	friend class RSVP_Daemon_Wrapper;
	RSVP_Wrapper* wrapper;
	void setWrapper( RSVP_Wrapper* w ) { wrapper = w; }
#endif

public:
	static const FLOWSPEC_Object* zeroFlowspec;

	RSVP( const String& confFileIface = "", LogicalInterfaceList tmpLifList = LogicalInterfaceList() );
	~RSVP();
	int main();
	bool properInit() { return initOK; }

	RoutingService& getRoutingService() { assert(routing); return *routing; }
	Session* findSession( const SESSION_Object& session, bool createIfNotFound = false );
	void removeSession( SessionHash::Iterator );

	uint32 getInterfaceCount() const { return interfaceCount; }
	const LogicalInterface* findInterfaceByName( const String& ) const;
	const LogicalInterface* findInterfaceByAddress( const NetAddress& ) const;
	const LogicalInterface* findInterfaceByLocalId( const uint32& lclId ) const;
	const LogicalInterface* findInterfaceByLIH( uint32 LIH ) const {
		if ( LIH < interfaceCount ) return lifArray[LIH];
		return NULL;
	}

	uint32 getLocalIdByIfName(char* name) const;

//Xi2007 >>
	uint32 addApiClientInterface(LogicalInterface* lifApiClient) { 
		lifArray[interfaceCount++] = lifApiClient; 
		return interfaceCount; 
	}
//Xi2007 <<

#if defined(NS2)
	const LogicalInterface* findInterfaceByOif( const String& ) const;
#endif

	Hop* findHop( const LogicalInterface&, const NetAddress&, bool create = false );
	void removeHop( HopKey& );
	MPLS& getMPLS() { return *mpls; }

#if defined(WITH_API)
	static API_Server& getApiServer() { assert(apiServer); return *apiServer; }
	static const LogicalInterface* getApiLif() { assert(apiServer); return RSVP_Global::rsvp->lifArray[0]; }
	static const LogicalInterface* getApiUniClientLif() { return findInterfaceByName(RSVP_Global::apiUniClientName); }
#endif

	// stats only
	inline void increaseSessionCount();
	inline void decreaseSessionCount();
	inline void increaseReservationCount();
	inline void decreaseReservationCount();
	inline void logStats();
	inline void resetStats();
};

#endif /* _RSVP_h_ */
