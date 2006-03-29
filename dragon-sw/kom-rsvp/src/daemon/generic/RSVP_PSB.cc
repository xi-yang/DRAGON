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
#include "RSVP_PSB.h"
#include "RSVP.h"
#include "RSVP_BlockSB.h"
#include "RSVP_IntServObjects.h"
#include "RSVP_Log.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_MPLS.h"
#include "RSVP_Message.h"
#include "RSVP_MessageProcessor.h"
#include "RSVP_OIatPSB.h"
#include "RSVP_PHopSB.h"
#include "RSVP_RSB.h"
#include "RSVP_Session.h"
#include "RSVP_OutISB.h"
#include "RSVP_TrafficControl.h"
#include "SwitchCtrl_Global.h"
//#include "SNMP_Session.h"
//#include "CLI_Session.h"

ostream& operator<< ( ostream& os, const PSB& psb ) {
	os << "PSB:" << (const SENDER_Object&)psb;
	if ( psb.RelationshipPSB_PHopSB::followRelationship() ) {
		os << " from " << *psb.RelationshipPSB_PHopSB::followRelationship();
	} else {
		os << " PHOP not yet set";
	}
	return os;
}

PSB::PSB( const SENDER_Object& senderTemplate )
	: SENDER_Object(senderTemplate), adSpec(NULL),
	gateway(LogicalInterface::noGatewayAddress), lifetimeTimer(*this),
	blockadeSB(NULL), forwardFlowspec(NULL) {
	if ( adSpec ) {
		adSpec->borrow();
	}
	LOG(2)( Log::SB, "creating", *this );
#if defined(REFRESH_REDUCTION) || defined(ONEPASS_RESERVATION)
	nextHop = NULL;
#endif
#if defined(REFRESH_REDUCTION)
	sendID = NULL;
	recvID = NULL;
#endif
#if defined(ONEPASS_RESERVATION)
	onepass = false;
	onepassAll = false;
	duplexObject = NULL;
	duplexPSB = NULL;
#endif
#if defined(WITH_API)
	localOnly = false;
#endif
	// fromAPI is set in Session::processPATH in any case
	inLabelRequested = false;
	inLabel = NULL;
	explicitRoute = NULL;
	labelSet = NULL;
	hasSuggestedLabel = false;
	hasUpstreamInLabel = hasUpstreamOutLabel = false;
	E_Police = false;
}

PSB::~PSB() {
	LOG(2)( Log::SB, "deleting", *this );
	updateRoutingInfo( LogicalInterfaceSet(), LogicalInterface::noGatewayAddress, true, false );
	if ( inLabel ) RSVP_Global::rsvp->getMPLS().deleteInLabel(*this, inLabel );
	if (hasUpstreamInLabel) {
		RSVP_Global::rsvp->getMPLS().deleteUpstreamInLabel(*this);
		hasUpstreamInLabel = false;
	}
	if (hasUpstreamOutLabel){ //Upstream out label is removed here since we did not create a new OIatPSB for upstream LSP
		RSVP_Global::rsvp->getMPLS().deleteUpstreamOutLabel(*this);
		hasUpstreamOutLabel = false;
	}
	if ( explicitRoute ) explicitRoute->destroy();
#if defined(REFRESH_REDUCTION)
	if ( recvID ) getPHopSB().getHop().clearRecvPSB( recvID->id );
#endif
	if ( adSpec ) adSpec->destroy();
	if ( blockadeSB ) delete blockadeSB;
	if ( forwardFlowspec ) forwardFlowspec->destroy();
	UnknownObjectList::ConstIterator uoIter = unknownObjectList.begin();
	for ( ; uoIter != unknownObjectList.end(); ++uoIter ) {
		(*uoIter)->destroy();
	}
#if defined(ONEPASS_RESERVATION)  
	if (duplexObject) delete duplexObject;
	if (duplexPSB) delete duplexPSB;
#endif
}

OutISB* PSB::getOutISB( uint32 i ) const {
	if ( getOIatPSB(i) ) {
		return getOIatPSB(i)->getOutISB();
	} else {
		return NULL;
	}
}

bool PSB::updateSENDER_TSPEC_Object( const SENDER_TSPEC_Object& tspec ) {
	if (tspec.getService() == SENDER_TSPEC_Object::GMPLS_Sender_Tspec ){
		if ( senderTSpec.getService() != tspec.getService() || 
			static_cast<const TSpec&>(senderTSpec) != static_cast<const TSpec&>(tspec) ) {
			senderTSpec = tspec;
			LOG(2)( Log::SB, "TSpec changed:", static_cast<const TSpec&>(tspec) );
			return true;
		}
	}
	else if ( tspec.getService() == SENDER_TSPEC_Object::SonetSDH_Sender_Tspec ){
		if ( senderTSpec.getService() != tspec.getService() ||
			static_cast<const SONET_TSpec&>(senderTSpec) != static_cast<const SONET_TSpec&>(tspec) ) {
			senderTSpec = tspec;
			LOG(2)( Log::SB, "SONET_TSpec changed:", static_cast<const SONET_TSpec&>(tspec) );
			return true;
		}
	}
	return false;
}

void PSB::updateADSPEC_Object( const ADSPEC_Object* a, bool nonRsvp ) {
	if (adSpec) {
		adSpec->destroy();
		adSpec = NULL;
	}
	if (a) {
		adSpec = a->borrow();
		if ( nonRsvp ) {
			const_cast<ADSPEC_Object*>(adSpec)->setBreakBitCL(true);
			const_cast<ADSPEC_Object*>(adSpec)->setBreakBitGS(true);
		}
	}
}

bool PSB::calculateForwardFlowspec( bool B_Merge, const FLOWSPEC_Object* blockadeFlowspec ) {
	if ( !blockadeFlowspec && blockadeSB ) blockadeFlowspec = &blockadeSB->getFlowspec();
	bool forwardFlowspecGLB = false;
	if ( forwardFlowspec ) forwardFlowspec->destroy();
	forwardFlowspec = new FLOWSPEC_Object;
	FLOWSPEC_Object* flowspecGLB = new FLOWSPEC_Object;
#if defined(ONEPASS_RESERVATION)
	onepassAll = true;
#endif
	LogicalInterfaceSet::ConstIterator lifIter = outLifSet.begin();
	for ( ;lifIter != outLifSet.end(); ++lifIter ) {
		OutISB* oisb = getOIatPSB( (*lifIter)->getLIH() )->getOutISB();
		if ( oisb ) {
			const FLOWSPEC_Object& oisbFlowspec = oisb->getForwardFlowspec();
			if ( !blockadeFlowspec || *blockadeFlowspec > oisbFlowspec ) { 
				forwardFlowspec->LUB( oisbFlowspec );
			} else {
				RSVP_Global::messageProcessor->noteThatOutISBisBlockaded( *oisb );
				const RSB_List& rsbList = oisb->getRSB_List();
				RSB_List::ConstIterator rsbIter = rsbList.begin();
				for ( ; rsbIter != rsbList.end(); ++rsbIter ) {
					if ( *blockadeFlowspec > (*rsbIter)->getFLOWSPEC_Object() ) {
						forwardFlowspec->LUB( (*rsbIter)->getFLOWSPEC_Object() );
					} else if ( B_Merge ) {
						flowspecGLB->GLB( (*rsbIter)->getFLOWSPEC_Object() );
					}
				}
			}
#if defined(ONEPASS_RESERVATION)
			onepassAll = onepassAll && oisb->isOnepassAll();
#endif
		}
	}
	if ( *forwardFlowspec == *RSVP::zeroFlowspec ) {
		forwardFlowspec->destroy(); forwardFlowspec = NULL;
		if ( *flowspecGLB != *RSVP::zeroFlowspec ) {
			forwardFlowspec = flowspecGLB;
			forwardFlowspecGLB = true;
		} else {
#if defined(ONEPASS_RESERVATION)
			onepassAll = false;
#endif
			flowspecGLB->destroy();
		}
	} else {
		flowspecGLB->destroy();
	}
	LOG(4)( Log::Process, "PSB", *this, "calculated forward flowspec", (forwardFlowspec ? *forwardFlowspec : *RSVP::zeroFlowspec) );
	return forwardFlowspecGLB;
}

void PSB::updateRoutingInfo( const LogicalInterfaceSet& lifList,
	const NetAddress& gw, bool Path_Refresh_Needed, bool localRepair
#if defined(ONEPASS_RESERVATION)
	, bool o, uint32 timeout
#endif
	) {

	LogicalInterfaceSet removedLifs;
	LogicalInterfaceSet addedLifs;

	outLifSet.replaceElements( lifList, removedLifs, addedLifs );

#if defined(ONEPASS_RESERVATION)
	if ( o != onepass ) {
		Path_Refresh_Needed = true;
		onepass = o;
	}
#endif

	if ( !outLifSet.empty() ) {
		if ( gateway != gw ) {
#if defined(REFRESH_REDUCTION) || defined(ONEPASS_RESERVATION)
			nextHop = RSVP_Global::rsvp->findHop( *outLifSet.front(), gw );
#endif
#if defined(REFRESH_REDUCTION)
			if ( sendID ) {
				nextHop->clearSendState( sendID->id );
				sendID = NULL;
			}
#endif
			Path_Refresh_Needed = true;
			gateway = gw;
#if defined(REFRESH_REDUCTION)
		// if final node is directly attached to link, try to find hop
		// the same applies for an MPLS explicit routed next hop
		} else if ( gw == LogicalInterface::noGatewayAddress && !nextHop ) {
			if ( !getSession().getDestAddress().isMulticast()
#if defined(WITH_API)
				&& outLifSet.front() != RSVP::getApiLif()
#endif
			) {
				nextHop = RSVP_Global::rsvp->findHop( *outLifSet.front(), getSession().getDestAddress() );
			}
#endif
		}
	}

#if defined(REFRESH_REDUCTION)
	if ( Path_Refresh_Needed && nextHop && nextHop->isRefreshReductionCapable() ) {
		if ( sendID ) nextHop->clearSendState( sendID->id );
		if ( !outLifSet.empty() ) {
			sendID = nextHop->storeSendPSB( this );
		} else {
			sendID = NULL;
		}
	}
#endif

	LogicalInterfaceSet::ConstIterator iter = removedLifs.begin();
	for ( ; iter != removedLifs.end(); ++iter ) {
		delete getOIatPSB((*iter)->getLIH());
	}

	for ( iter = addedLifs.begin(); iter != addedLifs.end(); ++iter ) {
		uint32 LIH = (*iter)->getLIH();
		OIatPSB* oiatpsb = new OIatPSB( *this, LIH );

		if ( TTL > 0 ) {
#if defined(REFRESH_REDUCTION)
			if ( !sendID )
#endif
				oiatpsb->setRefreshTime( (*iter)->getRefreshInterval() );
		}

		// only for WF style or if PSB is localOnly, RSBs respectively OutISBs
		// can already exist at new interfaces => register with them and update TC
		if ( getSession().getStyle() == WF
#if defined(WITH_API)
			|| localOnly
#endif
		) {
			OutISB* oisb = getSession().findOutISB( **iter, *this );
			if ( oisb ) oiatpsb->newOutISB( oisb );
		} // if WF or localOnly

#if defined(ONEPASS_RESERVATION)
	}
	if ( onepass ) {
		if ( Path_Refresh_Needed ) {
			installDefaultReservations( timeout );
		} else {
			refreshDefaultReservations( timeout );
		}
	}
	for ( iter = addedLifs.begin(); iter != addedLifs.end(); ++iter ) {
		OIatPSB* oiatpsb = getOIatPSB( (*iter)->getLIH() );
#endif
	
		if ( TTL > 0 && !Path_Refresh_Needed ) {
			if ( localRepair ) {
				oiatpsb->setLocalRepairRefresh( TimerSystem::W );
			} else {
				sendRefresh( **iter );
			}
		}
	} // for added lifs

	if ( TTL > 0 && Path_Refresh_Needed ) {
		iter = outLifSet.begin();
		for (; iter != outLifSet.end(); ++iter ) {
			sendRefresh( **iter );
		}
	}
	LOG(4)( Log::SB, "PSB::updateRoutingInfo done, gateway is", gateway, ", new lif count:", outLifSet.size() );
}

bool PSB::addReservation( OutISB* oisb, const Hop& nhop ) {
	OIatPSB* oiatpsb = getOIatPSB( oisb->getOI().getLIH() );
       assert( oiatpsb );
	if ( oiatpsb->addRSB( oisb ) ) {
		if ( inLabelRequested && !inLabel ) inLabel = RSVP_Global::rsvp->getMPLS().setInLabel( *this );

              if (outLabel == 0 && vlsrt.size() > 0 && vlsrt.back().outPort != 0) //@@@@ hacked
              {
                     outLabel = vlsrt.back().outPort; //@@@@ hacked
              }
  
		if ( outLabel ) {
			if ( oiatpsb->getOutLabel() && oiatpsb->getOutLabel()->getLabel() ) {
				if ( oiatpsb->getOutLabel()->getLabel() == outLabel ) {
					outLabel = 0;
		goto noLabelChange;
				} else {
					RSVP_Global::rsvp->getMPLS().deleteOutLabel( oiatpsb->getOutLabel() );
				}
			}
			RSVP_Global::rsvp->getMPLS().handleOutLabel( *oiatpsb, outLabel, nhop );
		} else if ( oiatpsb->getOutLabel() && oiatpsb->getOutLabel()->getLabel() ) {
			// TODO: remove out label assignment here
		}
noLabelChange:
		if ( outLabel ) {
			if ( !inLabelRequested ) {
				RSVP_Global::rsvp->getMPLS().createIngressClassifier( getSession(), *this, *oiatpsb->getOutLabel() );
			} else {
				if (!RSVP_Global::rsvp->getMPLS().bindInAndOut( *this, *inLabel, *oiatpsb->getOutLabel() ))
					return false;
			}
		} else if ( inLabel ) {
			RSVP_Global::rsvp->getMPLS().createEgressBinding( *inLabel, getPHopSB().getHop().getLogicalInterface() );
		}
		else
			return  false;
	}
	else
		return false;
	//create upstream ingress / egress binding
	if (hasUpstreamInLabel && upstreamInLabel.getLabel() && hasUpstreamOutLabel && upstreamOutLabel.getLabel()){
		return (RSVP_Global::rsvp->getMPLS().bindUpstreamInAndOut(*this));
	}
	else if (hasUpstreamInLabel && upstreamInLabel.getLabel()){
		return (RSVP_Global::rsvp->getMPLS().createUpstreamIngressClassifier(getSession(), *this));
	}
	else if (hasUpstreamOutLabel && upstreamOutLabel.getLabel()){
		return (RSVP_Global::rsvp->getMPLS().createUpstreamEgressBinding(*this, getPHopSB().getHop().getLogicalInterface()));
	}
	else
		return true;
}

void PSB::removeReservation( uint32 LIH ) {
	getOIatPSB(LIH)->removeRSB();
}

void PSB::refreshReservation( uint32 LIH, uint32 timeout ) {
	if ( getSession().getStyle() == SE ) {
		getOIatPSB(LIH)->setTimeout(multiplyTimeoutTime(timeout));
	}
}

void PSB::refreshReservation( uint32 LIH ) {
	if ( getSession().getStyle() == SE ) {
		getOIatPSB(LIH)->restartTimeout();
	}
}

bool PSB::matchOI( const LogicalInterface& lif ) const {
	return getOIatPSB(lif.getLIH()) != NULL;
}

void PSB::sendRefresh( const LogicalInterface& outLif ) {
                                                             assert( TTL > 0 );
#if defined(WITH_API)
	if ( localOnly && &outLif != RSVP_Global::rsvp->getApiLif() )
		return;
#endif
	bool clearE_Police = !E_Police || outLif.getTC().doesPolicing();
	uint8 type = Message::Path;
#if defined(ONEPASS_RESERVATION)
	if ( getOutISB(outLif.getLIH()) && getOutISB(outLif.getLIH())->isOnepass() ) {
		type = Message::PathResv;
	}
#endif
	Message message( type, TTL, getSession(), clearE_Police );
#if defined(REFRESH_REDUCTION)
	if ( sendID != NULL ) {
                                    assert( sendID != NULL && nextHop != NULL );
		message.setMESSAGE_ID_Object( MESSAGE_ID_Object( 0, nextHop->getEpoch(), sendID->id ) );
	}
#endif
	if (( outLif.hasEnabledMPLS() && !getSession().getDestAddress().isMulticast() )  ||
            ( outLif == *RSVP_Global::rsvp->getApiLif() && vlsrt.size() > 0)) {//hacked @@@@
		message.setLABEL_REQUEST_Object( getLABEL_REQUEST_Object());
		getOIatPSB( outLif.getLIH() )->setOutLabelRequested();
		getOIatPSB( outLif.getLIH() )->setOutLabelRequestedType(getLABEL_REQUEST_Object().getRequestedLabelType());
	} else if (outLif == *(RSVP_Global::rsvp->getApiLif())){
		message.setLABEL_REQUEST_Object( getLABEL_REQUEST_Object());
	} else
		message.setLABEL_REQUEST_Object( getLABEL_REQUEST_Object());
	if ( explicitRoute && explicitRoute->getAbstractNodeList().size() >= 1 ) {
		message.setEXPLICIT_ROUTE_Object( *explicitRoute );
	}
	//We shall apply filtering rules to the labelSet object later. But for now, let's just simply copy it to outgoing PATH messages
	if (labelSet) message.setLABEL_SET_Object(*labelSet);
	if (hasSessionAttributeObject) message.setSESSION_ATTRIBUTE_Object(sessionAttributeObject);

	if (outLif == *(RSVP_Global::rsvp->getApiLif())){
		if (hasUpstreamOutLabel && upstreamOutLabel.getLabel()){
			message.setUPSTREAM_LABEL_Object(upstreamOutLabel);
		}
	}
	else{
		if (hasUpstreamInLabel && upstreamInLabel.getLabel()){
			message.setUPSTREAM_LABEL_Object(upstreamInLabel);
		}
	}
	message.setSENDER_TEMPLATE_Object( *this );
	message.setSENDER_TSPEC_Object( senderTSpec );
	message.setRSVP_HOP_Object(dataOutRsvpHop);
	message.setTIME_VALUES_Object( outLif.getRefreshInterval() );
#if defined(ONEPASS_RESERVATION)
	if ( getOutISB(outLif.getLIH()) && getOutISB(outLif.getLIH())->isOnepass() && duplexObject ) {
		message.setDUPLEX_Object( *duplexObject );
	}
#endif
	if ( adSpec ) {
		const ADSPEC_Object* newAdSpec;
		outLif.getTC().advertise( *adSpec, newAdSpec );
		message.setADSPEC_Object( *newAdSpec );
		newAdSpec->destroy();
	}
	message.addUnknownObjects( unknownObjectList );
	if ( explicitRoute ) 
		//Destination address field of the RSVP raw IP packet header must be the gateway
	    if (gateway!=LogicalInterface::noGatewayAddress)
		outLif.sendMessage( message, gateway);
           else
		outLif.sendMessage( message, explicitRoute->getAbstractNodeList().front().getAddress(), getSrcAddress(), gateway );
       else if (message->getDRAGON_UNI_Object() != NULL) //@@@@ hacked
	   	outLif.sendMessage( message, gateway );
	else
		outLif.sendMessage( message, getSession().getDestAddress(), getSrcAddress(), gateway );
}

void PSB::sendTearMessage() {
	if ( TTL < 1 ) return;
	Message msg( Message::PathTear, TTL, getSession() );
	msg.setSENDER_TEMPLATE_Object( *this );
	msg.setSENDER_TSPEC_Object(senderTSpec);
	LogicalInterfaceSet::ConstIterator lifIter = outLifSet.begin();
	for ( ;lifIter != outLifSet.end(); ++lifIter ) {
		msg.setRSVP_HOP_Object(dataOutRsvpHop);
		// unicast only -> no need to reset EXPLICIT_ROUTE_Object in message
		if ( explicitRoute ) {
 		    if (gateway!=LogicalInterface::noGatewayAddress)
			(*lifIter)->sendMessage( msg, gateway);
		    else
			(*lifIter)->sendMessage( msg, explicitRoute->getAbstractNodeList().front().getAddress(), getSrcAddress(), gateway );
		} 
	       else if (msg->getDRAGON_UNI_Object() != NULL) //@@@@ hacked
		   	(*lifIter)->sendMessage( msg, gateway );
		else
			(*lifIter)->sendMessage( msg, getSession().getDestAddress(), getSrcAddress(), gateway );
		msg.clearRSVP_HOP_Object(msg.getRSVP_HOP_Object());
	}
}

inline void PSB::timeout() {
	ERROR(5)( Log::Error, "PSB timeout:", getSession(), "timeout", (DaytimeTimeValue&)lifetimeTimer.getAlarmTime(), "fired" );
	RSVP_Global::messageProcessor->timeoutPSB( this );
}

inline void MessageProcessor::timeoutPSB( PSB* psb ) {
	currentSession = &psb->getSession();
	psb->sendTearMessage();
	delete psb;
                                             assert( phopRefreshList.empty() );
}

void PSB::setBlockade( const FLOWSPEC_Object& f, uint32 timeout ) {
	if ( blockadeSB ) delete blockadeSB;
	blockadeSB = new BlockSB<PSB>( *this, f, timeout );
	LOG(4)( Log::SB, "set blockaded:", f, "at", *this );
}

inline void PSB::clearBlockade() {
	LOG(4)( Log::SB, "clear blockaded:", blockadeSB->getFlowspec(), "at", *this );
	delete blockadeSB; blockadeSB = NULL;
}

void PSB::updateUnknownObjectList( const UnknownObjectList& uoList ) {
	UnknownObjectList::ConstIterator uoIter = unknownObjectList.begin();
	while ( uoIter != unknownObjectList.end() ) {
		(*uoIter)->destroy();
		uoIter = unknownObjectList.erase( uoIter );
	}
	uoIter = uoList.begin();
	for ( ; uoIter != uoList.end(); ++uoIter ) {
		unknownObjectList.push_back( (*uoIter)->borrow() );
	}
}

#if defined(ONEPASS_RESERVATION)
void PSB::refreshDefaultReservations( uint32 timeout ) {
	LogicalInterfaceSet::ConstIterator iter = outLifSet.begin();
	for (; iter != outLifSet.end(); ++iter ) {
		LOG(2)( Log::Process, "refreshing default reservations for", (*iter)->getName() );
		OutISB* oisb = getOutISB( (*iter)->getLIH() );
		if ( oisb ) {
			if ( timeout == 0 ) {
				oisb->getRSB_List().front()->restartTimeout();
			} else {
				oisb->getRSB_List().front()->setFiltersAndTimeout( this, timeout, false );
			}
		}
	}
	if ( duplexPSB ) duplexPSB->refreshDefaultReservations( timeout );
}

inline void PSB::installDefaultReservations( uint32 timeout ) {
	FLOWSPEC_Object* flowspec = NULL;

	if (senderTSpec.getService() == SENDER_TSPEC_Object::GMPLS_Sender_Tspec)
		flowspec = new FLOWSPEC_Object( (const TSpec &)senderTSpec );
	else
		flowspec = new FLOWSPEC_Object( (const SONET_TSpec &)senderTSpec );
	LogicalInterfaceSet::ConstIterator iter = outLifSet.begin();
	for ( ; iter != outLifSet.end(); ++iter ) {
		LOG(2)( Log::Process, "installing default reservation for", (*iter)->getName() );
		TrafficControl::UpdateFlag uflag = TrafficControl::ModifiedRSB;
		OutISB* oisb = getOutISB( (*iter)->getLIH() );
		if ( !oisb ) {
			oisb = (*iter)->getTC().createOutISB( **iter );
		}
		RSB_Contents* oldRSB = NULL;
		RSB* currentRSB = NULL;
		if ( !oisb->getRSB_List().empty() ) {
			currentRSB = oisb->getRSB_List().front();
		}
		if ( currentRSB && currentRSB->isOnepass() ) {
			oldRSB = currentRSB->createBackup();
		} else {
			currentRSB = new RSB( *RSVP_Global::rsvp->findHop( **iter, NetAddress(0), true ) );
			oisb->RelationshipOutISB_RSB::setRelationshipFull( oisb, currentRSB, oisb->getRSB_List().begin() );
			getSession().increaseRSB_Count();
			uflag = TrafficControl::NewRSB;
		}
		bool filterChange = currentRSB->setFiltersAndTimeout( this, timeout, false );
		bool flowspecChange = currentRSB->updateContents( *flowspec, NULL );
		if ( filterChange || flowspecChange ) {
			TrafficControl::UpdateResult tcResult = oisb->getOI().getTC().updateTC( *oisb, currentRSB, uflag );
			if ( tcResult.error != ERROR_SPEC_Object::Confirmation ) {
				// TODO: send ERROR message
				if ( oldRSB ) {
					currentRSB->replaceWithBackup( *oldRSB, timeout );
					oldRSB = NULL;
				} else {
					getSession().decreaseRSB_Count();
					delete currentRSB;
				}
			}
		}
		if ( oldRSB ) delete oldRSB;
	}
	flowspec->destroy();
}
#endif

#if defined(REFRESH_REDUCTION)
void PSB::setRecvID( sint32 id ) {
	if ( recvID ) { 
		if ( recvID->id == id ) return;
		getPHopSB().getHop().clearRecvPSB( recvID->id );
	}
	recvID = getPHopSB().getHop().storeRecvPSB( this, id );
}
#endif

#if defined(WITH_API)
uint16 PSB::getAPI_Port() const {
	return getPHopSB().getLIH();
}

const NetAddress& PSB::getAPI_Address() const {
	return getPHopSB().getHop().getAddress();
}
#endif

bool PSB::updateEXPLICIT_ROUTE_Object( EXPLICIT_ROUTE_Object* er ) {
	if (explicitRoute) {
		if (!er || *er != *explicitRoute) {
			explicitRoute->destroy();
		} else {
			return false;
		}
	} else if (!er) {
		return false;
	}
	explicitRoute = er;
	return true;
}

bool PSB::updateLABEL_SET_Object( LABEL_SET_Object* ls ) {
	if (labelSet) {
		if (!ls || *ls != *labelSet) {
			labelSet->destroy();
		} else {
			return false;
		}
	} else if (!ls) {
		return false;
	}
	labelSet = ls;
	return true;
}

bool PSB::updateLABEL_REQUEST_Object( LABEL_REQUEST_Object lr ) {
	if (lr == labelReqObject)
		return false;
	labelReqObject = lr;
	return true;
}

bool PSB::updateSUGGESTED_LABEL_Object( SUGGESTED_LABEL_Object sl ) {
	if (sl == suggestedLabelObject)
		return false;
	suggestedLabelObject = sl;
	hasSuggestedLabel = true;
	return true;
}

bool PSB::updateUPSTREAM_OUT_LABEL_Object( UPSTREAM_LABEL_Object ul ) {
	if (ul == upstreamOutLabel)
		return false;
	upstreamOutLabel = ul;
	hasUpstreamOutLabel = true;
	return true;
}

bool PSB::updateUPSTREAM_IN_LABEL_Object( UPSTREAM_LABEL_Object ul ) {
	if (ul == upstreamInLabel)
		return false;
	upstreamInLabel = ul;
	hasUpstreamInLabel = true;
	return true;
}

bool PSB::updateUPSTREAM_IN_LABEL_Object( uint32 label) {
	if (label == upstreamInLabel.getLabel())
		return false;
	upstreamInLabel.setLabel(label);
	hasUpstreamInLabel = true;
	return true;
}

bool PSB::updateSESSION_ATTRIBUTE_Object( SESSION_ATTRIBUTE_Object sa ) {
	if (sa == sessionAttributeObject)
		return false;
	sessionAttributeObject= sa;
	hasSessionAttributeObject = true;
	return true;
}

