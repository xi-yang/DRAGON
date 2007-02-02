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
#include "RSVP_Hop.h"
#include "RSVP_API_Server.h"
#include "RSVP_FilterSpecList.h"
#include "RSVP_MPLS.h"
#include "RSVP_Message.h"
#include "RSVP_OIatPSB.h"
#include "RSVP_PHopSB.h"
#include "RSVP_PSB.h"
#include "RSVP_RSB.h"
#include "RSVP_Session.h"
#include "CLI_Session.h"
#include "SwitchCtrl_Session_SubnetUNI.h"

#define CHECK_UNICAST_ROUTING_FOR_PATH_REFRESH
#if defined(CHECK_UNICAST_ROUTING_FOR_PATH_REFRESH)
#include "RSVP_RoutingService.h"
#endif

ostream& operator<< ( ostream& os, const Hop& h ) {
	os << "Hop:" << h.getAddress() << " via " << h.getLogicalInterface().getName();
	return os;
}

Hop::Hop( const LogicalInterface& lif, const NetAddress& addr )
	: HopKey(lif,addr), sbCount(0), timeoutTimer(*this), discovered(true)
#if defined(REFRESH_REDUCTION)
	, refreshReductCapable(false), refreshTimer(*this)
#endif
{
	LOG(2)( Log::SB, "creating", *this );
#if defined(REFRESH_REDUCTION)
#if defined(WITH_API)
	idRecv = NULL;
	idSend = NULL;
	if ( &lif == RSVP_Global::rsvp->getApiServer().getApiLif() ) return;
#endif
#if defined(NO_TIMERS) || defined(FIXED_TIMEOUTS)
	sendEpoch = 4711;
#else
	// TODO: store value to ensure that it is not reused at next start
	sendEpoch = drawRandomNumber( 16777215 );
#endif
	sendID = -1;
	idSend = new SendStorageID[RSVP_Global::idHashCountSend];
	idSendCount = 0;
	currentRefreshHash = 0;
	idRefreshPerMessageCount = 0;
	noOfRefreshMessages = 0;
	initMemoryWithZero( idSend, sizeof(SendStorageID) * RSVP_Global::idHashCountSend );
	idRecv = new ID_List[RSVP_Global::idHashCountRecv];
	idRecvCount = 0;
	currentRecvID = 0;
	currentRecvEpoch = 0;
	maxNackSize = getLogicalInterface().getMaxUnfragmentMsgSize() - MESSAGE_ID_NACK_Object::total_size();
#endif
	if ( addr && addr != LogicalInterface::loopbackAddress ) {
		mplsHopInfo = RSVP_Global::rsvp->getMPLS().createHopInfo( lif, addr );
	} else {
		mplsHopInfo = 0;
	}
}

Hop::~Hop() {
	LOG(2)( Log::SB, "deleting", *this );
#if defined(REFRESH_REDUCTION)
	if ( idSend ) delete [] idSend;
	if ( idRecv ) delete [] idRecv;
#endif
	if ( mplsHopInfo ) RSVP_Global::rsvp->getMPLS().removeHopInfo( mplsHopInfo );
}

void Hop::timeout() {
	ERROR(5)( Log::Error, "Hop timeout:", *this , "timeout", (DaytimeTimeValue&)timeoutTimer.getAlarmTime(), "fired" );
	RSVP_Global::rsvp->removeHop(*this);
}

#if defined(REFRESH_REDUCTION)
void Hop::sendMessageReliable( const Message& msg, const NetAddress& dest, const NetAddress& src, const NetAddress& gw ) {
	if ( !msg.hasMESSAGE_ID_Object() ) {
		increaseSendID();
		const_cast<Message&>(msg).setMESSAGE_ID_Object( MESSAGE_ID_Object( 0, sendEpoch, sendID ) );
	}
	if ( getLogicalInterface().getRapidRefreshInterval() != TimeValue(0,0) ) {
		const_cast<MESSAGE_ID_Object&>(msg.getMESSAGE_ID_Object()).setFlags( MESSAGE_ID_Object::ACK_Desired );
		ONetworkBuffer* buffer = getLogicalInterface().createOutgoingBuffer( msg, dest );
		getLogicalInterface().sendBuffer( *buffer, src, gw );
		delete buffer;
	}
}

void Hop::processSrefresh( const Message& msg ) {
       //static int switch_refresh_counter = 1;
       const SimpleList<sint32>& msgIdList = msg.getMESSAGE_ID_LIST_Object().getID_List();
	SimpleList<sint32>::ConstIterator msgIter = msgIdList.begin();
	Message* nackMsg = new Message( Message::Ack, 15 );
	for ( ; msgIter != msgIdList.end(); ++msgIter ) {
		// Search for matching state block(s). Either find a PSB and terminate
		// loop or find one or multiple RSBs, thus continue loop on RSB.
		bool foundRSB = false;
		ID_List& stateIdList = idRecv[recvHash(*msgIter)];
		ID_List::ConstIterator stateIter = stateIdList.begin();
		for ( ; stateIter != stateIdList.end(); ++stateIter ) {
			if ( (*stateIter).id == *msgIter ) {
				switch( (*stateIter).type ) {
				case RecvStorageID::Path:
				{
					LOG(4)( Log::Reduct, "found PSB for ID", *msgIter, "from", *this );
					if ( foundRSB ) {
						ERROR(5)( Log::Error, "already found RSB (and now PSB) for ID", *msgIter, "from", *this, "ignoring PSB" );
	goto nextMsgIter;
					}
#if defined(CHECK_UNICAST_ROUTING_FOR_PATH_REFRESH)
					static NetAddress gw(0);
					const LogicalInterface* lif;

					//@@@@ UNI related hacks
					DRAGON_UNI_Object* dragonUni = (DRAGON_UNI_Object*)(*stateIter).sb.psb->getDRAGON_UNI_Object();
					GENERALIZED_UNI_Object* generalizedUni = (GENERALIZED_UNI_Object*)(*stateIter).sb.psb->getGENERALIZED_UNI_Object();
#if defined(WITH_API)
					if ((*stateIter).sb.psb->getSession().getDestAddress() == RSVP_Global::rsvp->getRoutingService().getLoopbackAddress()) {
						if (dragonUni != NULL) {
							const String egressChanName = (const char*)dragonUni->getEgressCtrlChannel().name;
							if (egressChanName == "implicit")
								lif = RSVP_Global::rsvp->findInterfaceByLocalId((const uint32)dragonUni->getDestTNA().local_id);	
							else
								lif = RSVP_Global::rsvp->findInterfaceByName(egressChanName);
						}
						else { //non-UNI or Generalized UNI
							lif = RSVP_Global::rsvp->getApiLif();
						}
					}
					else
#endif
						if ( (*stateIter).sb.psb->getEXPLICIT_ROUTE_Object() )
					{
						EXPLICIT_ROUTE_Object* ero = const_cast<EXPLICIT_ROUTE_Object*>((*stateIter).sb.psb->getEXPLICIT_ROUTE_Object());
						if (ero->getAbstractNodeList().front().getType() == AbstractNode::IPv4)
							lif = RSVP_Global::rsvp->getRoutingService().findOutLifByOSPF(ero->getAbstractNodeList().front().getAddress(), 0, gw);
						else if (ero->getAbstractNodeList().front().getType() == AbstractNode::UNumIfID)
						{
							uint32 uNumIfID = ero->getAbstractNodeList().front().getInterfaceID();
							lif = RSVP_Global::rsvp->getRoutingService().findOutLifByOSPF(ero->getAbstractNodeList().front().getAddress(), uNumIfID, gw);
						}
						else
							lif = RSVP_Global::rsvp->getRoutingService().getUnicastRoute( (*stateIter).sb.psb->getSession().getDestAddress(), gw );
					}
					else if (generalizedUni) {
                                		lif = NULL;
						if (SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList) {
	                                		SwitchCtrl_Session_SubnetUNI_List::Iterator uniSessionIter = SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList->begin();
	                                		for ( ; uniSessionIter != SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList->end(); ++uniSessionIter) {
	                                			if ((*uniSessionIter)->isSessionOwner(msg)) {
	                                				lif = (*uniSessionIter)->getControlInterface(gw);
	                                			}
	                                		}
						}
 						if (!lif )
							lif = RSVP_Global::rsvp->getApiLif();
					}
					else if (dragonUni) {
						lif = RSVP_Global::rsvp->findInterfaceByName(String((const char*)dragonUni->getEgressCtrlChannel().name));
					}
					else {
						lif = RSVP_Global::rsvp->getRoutingService().getUnicastRoute( (*stateIter).sb.psb->getSession().getDestAddress(), gw );
					}
					//@@@@ UNI related hacks END

#if defined(WITH_API)
					if ( !lif
						&& ( RSVP_Global::rsvp->findInterfaceByAddress( (*stateIter).sb.psb->getSession().getDestAddress() )
							|| (*stateIter).sb.psb->getSession().getDestAddress().isMulticast() ) ) {
						lif = RSVP_Global::rsvp->getApiLif();
					}
#endif
					if ( !lif ) {
						ERROR(4)( Log::Error, "ERROR: cannot find outgoing interface for PSB", *(*stateIter).sb.psb, "from", *this );
		break;
					} else if ( !(*stateIter).sb.psb->matchOI( *lif ) ) {
						(*stateIter).sb.psb->updateRoutingInfo( lif, gw, false, true );
					}
#endif
					(*stateIter).sb.psb->restartTimeout();
	goto nextMsgIter;
				}
				case RecvStorageID::Resv:
					foundRSB = true;
					LOG(4)( Log::Reduct, "found RSB for ID", *msgIter, "from", *this );
					(*stateIter).sb.rsb->restartTimeout();
				}
			} // found matching ID
		}
		if ( !foundRSB ) {
			ERROR(4)( Log::Error, "no state found for ID", *msgIter, "from", *this );
			nackMsg->addMESSAGE_ID_NACK_Object( MESSAGE_ID_NACK_Object( 0, sendEpoch, *msgIter ) );
			if ( nackMsg->getLength() >= maxNackSize ) {
				LOG(1)( Log::Reduct, "splitting NACK message" );
				if (getLogicalInterface().getAddress() != LogicalInterface::noGatewayAddress){
					NetAddress peer;
					RSVP_Global::rsvp->getRoutingService().getPeerIPAddr(getLogicalInterface().getAddress(), peer);
					getLogicalInterface().sendMessage( *nackMsg, peer );
				}
				else
					getLogicalInterface().sendMessage( *nackMsg, getAddress() );
				delete nackMsg;
				nackMsg = new Message( Message::Ack, 15 );
			}
		}
nextMsgIter: ;
	}
	if ( nackMsg->getLength() != Message::headerSize() ) {
		if (getLogicalInterface().getAddress() != LogicalInterface::noGatewayAddress){
			NetAddress peer;
			RSVP_Global::rsvp->getRoutingService().getPeerIPAddr(getLogicalInterface().getAddress(), peer);
			getLogicalInterface().sendMessage( *nackMsg, peer );
		}
		else
			getLogicalInterface().sendMessage( *nackMsg, getAddress() );
	}
	delete nackMsg;
}

void Hop::processAckMessage( const Message& msg ) {
	LOG(1)( Log::Error, "processing Ack message -> only NACKs are implemented" );
	const MESSAGE_ID_NACK_List& nackList = msg.getMESSAGE_ID_NACK_List();
	MESSAGE_ID_NACK_List::ConstIterator nackIter = nackList.begin();
	for ( ; nackIter != nackList.end(); ++nackIter ) {
		SendStorageID& state = idSend[sendHash((*nackIter).getID())];
		if ( state.sb.sendPSB != NULL && state.id == (*nackIter).getID() ) {
			switch (state.type) {
			case SendStorageID::Path:
				if ( state.sb.sendPSB->getOIatPSB( getLIH() ) ) {
					state.sb.sendPSB->getOIatPSB( getLIH() )->refresh();
				}
			break;
			case SendStorageID::Resv:
				state.sb.sendPHopSB->refresh();
			break;
			}
		} else {
			LOG(4)( Log::Error, "no refresh state found for ID", *nackIter, "for", *this );
		}
	}
}

bool Hop::checkMessageID( const Message& msg ) {
	sint32 id = msg.getMESSAGE_ID_Object().getID();
	sint32 state_id = 0;
	if ( msg.getMESSAGE_ID_Object().getEpoch() != currentRecvEpoch ) {
		currentRecvEpoch = msg.getMESSAGE_ID_Object().getEpoch();
		return false;
	}
	switch ( msg.getMsgType() ) {
	case Message::Path:
#if defined(ONEPASS_RESERVATION)
	case Message::PathResv:
#endif
	case Message::PathTear:
	case Message::PathErr:
		if ( currentRecvID == id ) {
			PSB *psb = getRecvPSB(id);
			if ( psb && *psb == msg.getSENDER_TEMPLATE_Object() ) {
	return true;
			}
		} else if ( currentRecvID - id > 0 ) {
			if ( getRecvPSB(id) != NULL ) {
				state_id = getRecvPSB(id)->getRecvID()->id;
	goto check_state_id;
			}
		}
	break;
	case Message::Resv:
	case Message::ResvTear:
	case Message::ResvErr:
		if ( currentRecvID - id >= 0 ) {
			if ( getRecvRSB(id) != NULL ) {
				state_id = getRecvRSB(id)->getRecvID()->id;
	goto check_state_id;
			}
		}
	break;
	}
	currentRecvID = id;
	return false;
check_state_id:
	return ( id - state_id > 0 );
}

void Hop::refresh() {
                                                     assert( idSendCount > 0 );
	LOG(4)( Log::Reduct, "creating Srefresh message, looking for", idRefreshPerMessageCount, "IDs at", *this );
	Message message( sendEpoch );
	uint32 idStored = 0;
	while ( idStored < idRefreshPerMessageCount ) {
		if ( idSend[currentRefreshHash].sb.sendPSB != NULL ) {
			LOG(2)( Log::Reduct, "adding ID", idSend[currentRefreshHash].id );
			message.add_ID_for_MESSAGE_ID_LIST( idSend[currentRefreshHash].id );
			idStored += 1;
		}
		currentRefreshHash += 1;
		if ( currentRefreshHash >= RSVP_Global::idHashCountSend ) currentRefreshHash = 0;
	}
	if (getLogicalInterface().getAddress() != LogicalInterface::noGatewayAddress){
		NetAddress peer;
		RSVP_Global::rsvp->getRoutingService().getPeerIPAddr(getLogicalInterface().getAddress(), peer);
		getLogicalInterface().sendMessage( message, peer );
	}
	else
		getLogicalInterface().sendMessage( message, getAddress() );
}
#endif
