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
#ifndef _RSVP_PSB_h_
#define _RSVP_PSB_h_ 1

#include "RSVP_BasicTypes.h"
#include "RSVP_GeneralMemoryMachine.h"
#if defined(REFRESH_REDUCTION)
#include "RSVP_Hop.h"
#endif
#include "RSVP_InterfaceArray.h"
#include "RSVP_IntServObjects.h"
#include "RSVP_Lists.h"
#include "RSVP_LogicalInterfaceSet.h"
#include "RSVP_ProtocolObjects.h"
#include "RSVP_Relationships.h"
#include "RSVP_Timer.h"
#include "RSVP_RoutingService.h"

template <class T> class BlockSB;
class MPLS_InLabel;
class EXPLICIT_ROUTE_Object;

typedef RelationshipMANYto1<PSB,PSB_List,Session,DoOnEmptyDelete1> RelationshipPSB_Session;
typedef RelationshipMANYto1<PSB,PSB_List,PHopSB,DoOnEmptyDelete1> RelationshipPSB_PHopSB;
typedef Relationship1toMANY<PSB,OIatPSB,OIatPSB_Array> RelationshipPSB_OIatPSB;

class PSB :	public RelationshipPSB_Session,
						public RelationshipPSB_PHopSB,
						public RelationshipPSB_OIatPSB,
						public SENDER_Object {

	SENDER_TSPEC_Object   senderTSpec;
	const ADSPEC_Object*  adSpec;
	LogicalInterfaceSet   outLifSet;
	UnknownObjectList     unknownObjectList;

	NetAddress gateway;
	uint8 TTL;
	bool E_Police;

	TimeoutTimer<PSB> lifetimeTimer;

	BlockSB<PSB>* blockadeSB;
	FLOWSPEC_Object* forwardFlowspec;

#if defined(REFRESH_REDUCTION) || defined(ONEPASS_RESERVATION)
	Hop* nextHop;
#endif
#if defined(REFRESH_REDUCTION)
	Hop::SendStorageID* sendID;
	Hop::RecvStorageID* recvID;
#endif

#if defined(WITH_API)
	bool localOnly;
	bool fromAPI;
#endif

#if defined(ONEPASS_RESERVATION)
	bool onepass;
	bool onepassAll;
	DUPLEX_Object* duplexObject;
	PSB* duplexPSB;
#endif

	bool inLabelRequested;
	const MPLS_InLabel* inLabel;
	uint32 outLabel;
	LABEL_REQUEST_Object labelReqObject;
	SUGGESTED_LABEL_Object suggestedLabelObject;
	bool hasSuggestedLabel; 
	UPSTREAM_LABEL_Object upstreamOutLabel;
	bool hasUpstreamOutLabel; 
	UPSTREAM_LABEL_Object upstreamInLabel;
	bool hasUpstreamInLabel; 
	SESSION_ATTRIBUTE_Object sessionAttributeObject;
	bool hasSessionAttributeObject;
	EXPLICIT_ROUTE_Object* explicitRoute;
	UNI_Object* uni;
	LABEL_SET_Object* labelSet;
	VLSRRoute vlsrt;
	RSVP_HOP_Object dataInRsvpHop;
	RSVP_HOP_Object dataOutRsvpHop;

	friend ostream& operator<< ( ostream&, const PSB& );
	PSB(const PSB&);
	PSB& operator=( const PSB&);
public:
	PSB( const SENDER_Object& );
	~PSB();

	OutISB* getOutISB( uint32 i ) const;
	PHopSB& getPHopSB() const { return *const_cast<PHopSB*>(RelationshipPSB_PHopSB::followRelationship()); }
	Session& getSession() { return *RelationshipPSB_Session::followRelationship(); }
	OIatPSB* getOIatPSB( uint32 LIH ) const { return RelationshipPSB_OIatPSB::followRelationship()[LIH]; }

	bool updateSENDER_TSPEC_Object( const SENDER_TSPEC_Object& );
	void updateADSPEC_Object( const ADSPEC_Object*, bool nonRsvp );
	void updateUnknownObjectList( const UnknownObjectList& );
#if defined(ONEPASS_RESERVATION)
	void updateRoutingInfo( const LogicalInterfaceSet&, const NetAddress&, bool, bool, bool = false, uint32 = 0 );
#else
	void updateRoutingInfo( const LogicalInterfaceSet&, const NetAddress&, bool, bool );
#endif

	// return value indicates that this is the first reservation for this PSB
	// used for refresh/confirm decision
	bool addReservation( OutISB*, const Hop& );
	void removeReservation( uint32 );
	void refreshReservation( uint32, uint32 );
	void refreshReservation( uint32 );

	const SENDER_TSPEC_Object& getSENDER_TSPEC_Object() const { return senderTSpec; }
	const LogicalInterfaceSet& getOutLifSet() const { return outLifSet; }
	bool matchOI( const LogicalInterface& ) const;
	const NetAddress& getGateway() const { return gateway; }

	void setTimeoutTime( uint32 t ) {
		lifetimeTimer.restart( multiplyTimeoutTime(t) );
	}
	void restartTimeout() {
		lifetimeTimer.restart();
#if defined(ONEPASS_RESERVATION)
		if ( onepass ) refreshDefaultReservations( 0 );
#endif
	}
	void setTTL( uint8 TTL ) { this->TTL = TTL; }

	void setE_Police( bool b ) { E_Police = b; }
	bool getE_Police() const { return E_Police; }

	void sendRefresh( const LogicalInterface& outLif );
	void refreshVLSRbyLocalId(); //!!!! DRAGON Addition
	void sendVtagNotification(); //!!!! DRAGON Addition
	void sendTearMessage();
	inline void timeout();

	void setBlockade( const FLOWSPEC_Object&, uint32 timeout );
	inline void clearBlockade();

	// return value indicates whether the current flowspec is below blockaded
	bool calculateForwardFlowspec( bool, const FLOWSPEC_Object* = NULL );
	const FLOWSPEC_Object* getForwardFlowspec() const { return forwardFlowspec; }

	const MPLS_InLabel* getInLabel() const { return inLabel; }
	void setInLabelRequested() { inLabelRequested = true; }
	void setOutLabel( uint32 l ) { outLabel = l; }
	const uint32 getOutLabel() const { return outLabel; }
	void setDataChannelInfo(VLSRRoute v, RSVP_HOP_Object& in, RSVP_HOP_Object& out) { if (v.size() > 0) vlsrt = v; dataInRsvpHop = in; dataOutRsvpHop = out; }
	VLSRRoute& getVLSR_Route() { return vlsrt; }
	const RSVP_HOP_Object& getDataInRsvpHop() const { return dataInRsvpHop; }
	const RSVP_HOP_Object& getDataOutRsvpHop() const {return dataOutRsvpHop;}
	bool updateEXPLICIT_ROUTE_Object( EXPLICIT_ROUTE_Object* er );
	const EXPLICIT_ROUTE_Object* getEXPLICIT_ROUTE_Object() const { return explicitRoute; }
	bool updateLABEL_REQUEST_Object( LABEL_REQUEST_Object labelReq );
	const LABEL_REQUEST_Object&  getLABEL_REQUEST_Object() const { return labelReqObject; }
	bool updateSUGGESTED_LABEL_Object( SUGGESTED_LABEL_Object sLabel);
	bool hasSUGGESTED_LABEL_Object() { return hasSuggestedLabel; }
	const SUGGESTED_LABEL_Object&  getSUGGESTED_LABEL_Object() const { return suggestedLabelObject; }
	bool updateUPSTREAM_OUT_LABEL_Object( UPSTREAM_LABEL_Object uOutLabel);
	bool hasUPSTREAM_OUT_LABEL_Object() { return hasUpstreamOutLabel; }
	const UPSTREAM_LABEL_Object&  getUPSTREAM_OUT_LABEL_Object() const { return upstreamOutLabel; }
	bool updateUPSTREAM_IN_LABEL_Object( UPSTREAM_LABEL_Object uInLabel);
	bool updateUPSTREAM_IN_LABEL_Object( uint32 uInLabel);
	bool hasUPSTREAM_IN_LABEL_Object() { return hasUpstreamInLabel; }
	const UPSTREAM_LABEL_Object&  getUPSTREAM_IN_LABEL_Object() const { return upstreamInLabel; }
	bool updateSESSION_ATTRIBUTE_Object(SESSION_ATTRIBUTE_Object ssAttrib);
	bool hasSESSION_ATTRIBUTE_Object() { return hasSessionAttributeObject; }
	const SESSION_ATTRIBUTE_Object& getSESSION_ATTRIBUTE_Object() const { return sessionAttributeObject; }
	bool updateLABEL_SET_Object( LABEL_SET_Object* er );
	const LABEL_SET_Object* getLABEL_SET_Object() const { return labelSet; }
	bool updateUNI_Object( UNI_Object* uni );
	bool updateDRAGON_UNI_Object( DRAGON_UNI_Object* uni );
	bool updateGENERALIZED_UNI_Object( GENERALIZED_UNI_Object* uni );
	const UNI_Object* getUNI_Object() const { return uni; }
	const DRAGON_UNI_Object* getDRAGON_UNI_Object() const { return (const DRAGON_UNI_Object*)uni; }
	const GENERALIZED_UNI_Object* getGENERALIZED_UNI_Object() const { return (const GENERALIZED_UNI_Object*)uni; }

#if defined(REFRESH_REDUCTION) || defined(ONEPASS_RESERVATION)
	Hop* getNextHop() { return nextHop; }
#endif

#if defined(ONEPASS_RESERVATION)
	bool isOnepass() { return onepass; }
	bool isOnepassAll() { return onepassAll; }
	void refreshDefaultReservations( uint32 );
	inline void installDefaultReservations( uint32 );
	void setDuplexPSB( PSB* psb ) {
		if ( duplexPSB != psb ) {
			if ( duplexPSB ) delete duplexPSB;
			duplexPSB = psb;
		}
	}
	bool updateDuplexObject( const DUPLEX_Object& d ) {
		if ( duplexObject ) {
			bool retval = (d != *duplexObject);
			*duplexObject = d;
			return retval;
		} else {
			duplexObject = new DUPLEX_Object(d);
			return true;
		}
	}
	bool clearDuplexObject() {
		if ( duplexObject ) {
			delete duplexObject;
			duplexObject = NULL;
			return true;
		} else {
			return false;
		}
	}
#endif

#if defined(REFRESH_REDUCTION)
	Hop::SendStorageID* getSendID() { return sendID; }
	Hop::RecvStorageID* getRecvID() { return recvID; }
	void setRecvID( sint32 id );
#endif

#if defined(WITH_API)
	void setLocalOnly( bool b ) { localOnly = b; }
	bool isLocalOnly() const { return localOnly; }
	void setFromAPI( bool fa ) { fromAPI = fa; }
	bool isFromAPI() const { return fromAPI; }
	uint16 getAPI_Port() const;
	const NetAddress& getAPI_Address() const;
#endif

	DECLARE_MEMORY_MACHINE_IN_CLASS(PSB)
};
DECLARE_MEMORY_MACHINE_OUT_CLASS(PSB, psbMemMachine)

#endif /* _RSVP_PSB_h_ */
