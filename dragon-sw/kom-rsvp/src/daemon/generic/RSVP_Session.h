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
#ifndef _RSVP_Session_h_
#define _RSVP_Session_h_ 1

#include "RSVP_Lists.h"
#include "RSVP_ProtocolObjects.h"
#include "RSVP_Relationships.h"
#include "RSVP_GeneralMemoryMachine.h"

class Message;
class FlowDescriptor;
class LogicalInterfaceSet;
class LogicalInterface;
class OutISB;
class NARB_APIClient;
class SwitchCtrl_Session_SubnetUNI;

typedef Relationship1toMANY<Session,PSB,PSB_List,DoOnEmptyDelete1> RelationshipSession_PSB;
typedef Relationship1toMANY<Session,PHopSB,PHOP_List> RelationshipSession_PHopSB;

class Session :	public RelationshipSession_PSB,
								public RelationshipSession_PHopSB,
								public SESSION_Object {

	SessionHash::Iterator iterFromRSVP;

	FilterStyle style;
	uint32 rsbCount;

	//These two parametered are stored when processPATH() is called,
	//and will be used by ResvConf
	const LogicalInterface* outLif;
	NetAddress gw;
	
	//The following parameter is stored when processPATH() is called,
	//and will be used by Resv
	const LogicalInterface* inLif;

	//Uni- or Bi-dir
	bool biDir;

	//NARB client
	NARB_APIClient* narbClient;

	//Subnet UNI Session handles
	SwitchCtrl_Session_SubnetUNI* pSubnetUniSrc;
	SwitchCtrl_Session_SubnetUNI* pSubnetUniDest;

	PHopSB* findOrCreatePHopSB( Hop&, uint32 );

	void matchPSBsAndFiltersAndOutInterface( const FilterSpecList&, const LogicalInterface&, PSB_List& result, OutISB*& );
#if defined(USE_SCOPE_OBJECT)
	void matchPSBsAndSCOPEandOutInterface( const AddressList&, const LogicalInterface&, PSB_List& result, OutISB*& );
#endif
	inline ERROR_SPEC_Object::ErrorCode processRESV_FDesc( const FLOWSPEC_Object&, const FilterSpecList&, const Message& msg, Hop& ); 

public:
	Session( const SESSION_Object& );
	~Session();
	void deleteAll();

	const LogicalInterface* getOutLif() const { return outLif; }
	const LogicalInterface* getInLif() const { return inLif; }
	const NetAddress& getGateway() const { return gw; }
	void setIterFromRSVP( SessionHash::Iterator iter ) {
		iterFromRSVP = iter;
	}

	NARB_APIClient* getNarbClient() { return narbClient; }

	OutISB* findOutISB( const LogicalInterface&, const PSB& );

	bool shouldReroute( const EXPLICIT_ROUTE_Object* ero );
	//int countLocalRouteHops( const EXPLICIT_ROUTE_Object* ero );

	bool processERO(const Message& msg, Hop&, EXPLICIT_ROUTE_Object* explicitRoute, bool fromLocalAPI, RSVP_HOP_Object& dataInRsvpHop, RSVP_HOP_Object& dataOutRsvpHop, VLSRRoute& vLSRoute);
	void processPATH( const Message&, Hop&, uint8 );
	// for multicast sessions
	void processAsyncRoutingEvent( const NetAddress&, const LogicalInterface&, LogicalInterfaceSet& );
	// for unicast sessions
	void processAsyncRoutingEvent( const LogicalInterface&, const NetAddress& );
	void processPTEAR( const Message&, const PacketHeader&, const LogicalInterface& );
	void processPERR( Message&, const LogicalInterface& );
	void processRESV( const Message& msg, Hop& );
	void processRTEAR( const Message& msg, Hop& h ) { processRESV( msg, h ); }
	void processRERR( Message&, Hop& );

	FilterStyle getStyle() const { return style; }
	void decreaseRSB_Count() { rsbCount -= 1; }
	void increaseRSB_Count() { rsbCount += 1; }

#if defined(WITH_API)
	bool deregisterAPI( const NetAddress&, uint16 port );
	void registerAPI();
#endif
	SwitchCtrl_Session_SubnetUNI* getSubnetUniSrc() { return pSubnetUniSrc; }
	SwitchCtrl_Session_SubnetUNI* getSubnetUniDest() { return pSubnetUniDest; }

	DECLARE_MEMORY_MACHINE_IN_CLASS(Session)
};
DECLARE_MEMORY_MACHINE_OUT_CLASS(Session, sessionMemMachine)
    
#endif /* RSVP_Session_h_ */
