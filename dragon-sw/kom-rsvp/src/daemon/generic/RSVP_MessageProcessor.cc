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
#include "RSVP_MessageProcessor.h"
#include "RSVP.h"
#include "RSVP_API_Server.h"
#include "RSVP_Log.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_Message.h"
#include "RSVP_MPLS.h"
#include "RSVP_NetworkServiceDaemon.h"
#include "RSVP_PHopSB.h"
#include "RSVP_PSB.h"
#include "RSVP_PolicyObjects.h"
#include "RSVP_ProtocolObjects.h"
#include "RSVP_RoutingService.h"
#include "RSVP_Session.h"
#include "RSVP_OutISB.h"
#include "SwitchCtrl_Global.h"
#include "SwitchCtrl_Session_SubnetUNI.h"
//#include "SNMP_Session.h"
//#include "CLI_Session.h"

// Xi2007 >>
ONetworkBuffer MessageEntry::obuffer(LogicalInterface::maxPayloadLength);
// Xi2007 <<

MessageProcessor::MessageProcessor() : ibuffer(LogicalInterface::maxPayloadLength),
	sendingHop(NULL), currentLif(NULL), incomingLif(NULL),
	currentSession(NULL), confirmOutISB(NULL), confirmMsg(NULL), resvMsg(NULL),
	B_Merge(true), fullRefresh(true) {
#if defined(USE_SCOPE_OBJECT)
	resvScope = NULL;
#endif
#if defined(ONEPASS_RESERVATION)
	onepassPSB = NULL;
#endif

	msgQueue = new MessageQueue;

//@@@@ Xi2008 >>
	for (int i = 0; i < NSIG_SNC_STABLE; i++)
		psbArrayWaitingForStableSNC[i] = NULL;
//@@@@ Xi2008 <<
}

MessageProcessor::~MessageProcessor() {
	// needed later, why though? ;-)

	if (msgQueue) {
		MessageQueue::Iterator msgIter = msgQueue->begin();
		for ( ; msgIter != msgQueue->end(); ++msgIter ) {
			if (*msgIter)
				delete (*msgIter);
		}
		delete msgQueue;
		msgQueue = NULL;
	}
}

//$$$$ Xi2008 for subnet control only >>
extern pid_t pid_verifySNCStateWorkingState;
//$$$$ Xi2008 <<

void MessageProcessor::processMessage() {

#if defined(WITH_API)
	// fix hop information, if message is from API client -> avoid confusion with onepass messages
	if ( currentLif == RSVP_Global::rsvp->getApiLif() ) {

		//$$$$ DRAGON Monitoring >>
		if ( currentMessage.getMsgType() == Message::MonQuery) {
			assert(currentMessage.getDRAGON_EXT_INFO_Object() != NULL && currentMessage.getDRAGON_EXT_INFO_Object()->HasSubobj(DRAGON_EXT_SUBOBJ_MON_QUERY));
			processDragonMonQuery(const_cast<SESSION_Object&>(currentMessage.getSESSION_Object()), currentMessage.getDRAGON_EXT_INFO_Object()->getMonQuery());
			return;
		}
		//$$$$ DRAGON Monitoring <<

		const_cast<RSVP_HOP_Object&>(currentMessage.getRSVP_HOP_Object()).setAddress( currentHeader.getSrcAddress() );
	}
	//$$$$ Xi2007 >>
	else if ( currentLif == RSVP_Global::rsvp->getApiUniClientLif() ) {
		const_cast<RSVP_HOP_Object&>(currentMessage.getRSVP_HOP_Object()).setAddress( currentHeader.getSrcAddress() );
		const_cast<RSVP_HOP_Object&>(currentMessage.getRSVP_HOP_Object()).setLIH( RSVP_Global::rsvp->getApiUniClientLif()->getLIH() );
		SwitchCtrl_Session_SubnetUNI_List::Iterator it = SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList->begin();
		for (; it != SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList->end(); ++it)
			(*it)->receiveAndProcessMessage(currentMessage); //Only one of them should own the message...Polling and checking inside
		return;
	}
	//$$$$ Xi2007 <<
	if ( currentMessage.getMsgType() == Message::InitAPI || currentMessage.getMsgType() == Message::RemoveAPI ) {
		RSVP::getApiServer().processMessage( currentMessage, *this );
		return;
	}
	if ( currentMessage.getMsgType() == Message::AddLocalId || currentMessage.getMsgType() == Message::DeleteLocalId
            || currentMessage.getMsgType() == Message::RefreshLocalId) {
		LocalId *lid = currentMessage.getLocalIdObject();
		SwitchCtrl_Global::processLocalIdMessage(currentMessage.getMsgType(), *lid);
		delete lid;
		return;
	}
#endif

	if ( currentMessage.getMsgType() == Message::ResvConf ) {
		const LogicalInterface* destLif = NULL;
		NetAddress gateway = LogicalInterface::noGatewayAddress;
		Session* s = RSVP_Global::rsvp->findSession( currentMessage.getSESSION_Object(), false );
		if (s && s->getOutLif())
		{
			destLif = s->getOutLif();
			gateway = s->getGateway();
		}
		else{
			if ( RSVP_Global::rsvp->findInterfaceByAddress( currentHeader.getDestAddress() ) ) {
				destLif = RSVP_Global::rsvp->getApiServer().getApiLif();
			} else
				destLif = RSVP_Global::rsvp->getRoutingService().getUnicastRoute( currentHeader.getDestAddress(), gateway );
		}
		if ( !destLif) { //destLif may be the same as currentLif !!!
		//if ( !destLif || destLif == currentLif ) {
			LOG(2)( Log::Msg, "cannot forward confirmation message; no route to", currentHeader.getDestAddress() );
		} else if ( currentMessage.getTTL() > 0 ) {
			if (gateway!=LogicalInterface::noGatewayAddress)
				destLif->sendMessage( currentMessage, gateway);
			else
				destLif->sendMessage( currentMessage, currentHeader.getDestAddress());
		}
		return;
	}

	findSendingHop();

#if defined(REFRESH_REDUCTION)
	if ( currentMessage.getMsgType() == Message::Srefresh ) {
		sendingHop->processSrefresh( currentMessage );
	return;
	} else if ( currentMessage.getMsgType() == Message::Ack ) {
		if ( sendingHop ) {
			sendingHop->processAckMessage( currentMessage );
		}
	return;
	} else if ( currentMessage.hasMESSAGE_ID_Object() ) {
		//@@@@ DRAGON hack! ==> Force processing all PATH/RESV messages even if MESSAGE_ID is present.
		/*
		if ( sendingHop && sendingHop->checkMessageID( currentMessage ) ) {
			LOG(3)( Log::Msg, "ID", currentMessage.getMESSAGE_ID_Object().getID(), "recognized -> ignoring message" );
	return;
		}
		*/
		if ( currentMessage.getMsgType() != Message::Path && currentMessage.getMsgType() != Message::Resv ) {
			currentMessage.clearMESSAGE_ID_Object();
		}
	}
#endif

	LOG(2)( Log::Short, "received", (MessagePrintShort&)currentMessage );

	currentSession = RSVP_Global::rsvp->findSession( currentMessage.getSESSION_Object(),
		currentMessage.getMsgType() == Message::Path || currentMessage.getMsgType() == Message::PathResv );
	
#if defined(REFRESH_REDUCTION)
	if ( currentMessage.getMsgType() == Message::Srefresh ) {
		if (currentSession->getSubnetUniSrc()) {
			switch (currentSession->getSubnetUniSrc()->getUniState()) {
			case Message::PathErr:
			case Message::ResvErr:
				sendPathErrMessage( ERROR_SPEC_Object::Notify, ERROR_SPEC_Object::SubnetUNISessionFailed); //UNI ERROR 
				return;
				break;
			}
		}
		if (currentSession->getSubnetUniDest()) {
			switch (currentSession->getSubnetUniDest()->getUniState()) {
			case Message::PathErr:
			case Message::ResvErr:
				sendPathErrMessage( ERROR_SPEC_Object::Notify, ERROR_SPEC_Object::SubnetUNISessionFailed); //UNI ERROR 
				return;
				break;
			}
		}		
	}
#endif

	if ( currentSession == NULL ) {
		switch( currentMessage.getMsgType() ) {
		case Message::Path:
#if defined(ONEPASS_RESERVATION)
		case Message::PathResv:
#endif
			sendPathErrMessage( ERROR_SPEC_Object::ConflictingDestPorts, 0 );
			break;
		case Message::Resv:
			sendResvErrMessage( 0, ERROR_SPEC_Object::NoPathInformation, 0 );
			break;
		default:
			LOG(2)( Log::Msg, "ignoring message (no session found):", currentMessage.getSESSION_Object() );
			break;
		}
	return;
	}

	switch ( currentMessage.getMsgType() ) {
	case Message::Path:
#if defined(ONEPASS_RESERVATION)
	case Message::PathResv:
#endif
		if ( checkPathMessage() ) {
			if (currentMessage.getEXPLICIT_ROUTE_Object()!=NULL)
				currentMessage.updateEXPLICIT_ROUTE_Object_Length(currentMessage.getEXPLICIT_ROUTE_Object()->total_size());
			currentSession->processPATH( currentMessage, *sendingHop, currentHeader.getTTL() );
		}
		if ( currentSession->RelationshipSession_PHopSB::followRelationship().empty() ) {
			delete currentSession;
		} else {
			refreshReservations();
		}
		break;
	case Message::Resv: {
		if ( currentMessage.hasRESV_CONFIRM_Object() ) {
			prepareConfirmMsg();
		}
		fullRefresh = false;
		currentSession->processRESV( currentMessage, *sendingHop );
		refreshReservations();
		//$$$$ Xi2008 for subnet control only >>
		//This is a diverged child process, it comes here only for sending out delayed RESV messages
		if (pid_verifySNCStateWorkingState == 0) {
			exit(0);
		}
		//$$$$ Xi2008 <<
		finishAndSendConfirmMsg();
		fullRefresh = true;
	}	break;
	case Message::PathTear:
		if ( checkPathMessage() ) {
			currentSession->processPTEAR( currentMessage, currentHeader, *currentLif );
		}
		assert( phopRefreshList.empty() );
		break;
	case Message::PathErr:
		currentSession->processPERR( currentMessage, *currentLif );
		break;
	case Message::ResvTear:
		currentSession->processRTEAR( currentMessage, *sendingHop );
		refreshReservations();
		break;
	case Message::ResvErr:
		B_Merge = false;
		fullRefresh = false;
		currentSession->processRERR( currentMessage, *sendingHop );
		refreshReservations();
		//$$$$ Xi2008 for subnet control only>>
		//This is a diverged child process, it comes here only for sending out delayed RESV messages
		if (pid_verifySNCStateWorkingState == 0) {
			exit(0);
		}
		//$$$$ Xi2008 <<
		B_Merge = true;
		fullRefresh = true;
		break;
	//$$$$ DRAGON specific
	case Message::AddLocalId:
	case Message::DeleteLocalId:
	case Message::RefreshLocalId:
		break;
	default:
		LOG(2)( Log::Msg, "ignoring unknown message type:", currentMessage.getMsgType() );
		break;
	}
#if defined(ONEPASS_RESERVATION)
	if ( currentMessage.hasDUPLEX_Object() ) {
		if ( currentSession->getDestAddress().isMulticast() ) {
			LOG(2)( Log::Process, "ignoring duplex request for multicast session", currentSession->getDestAddress() );
		} else if ( onepassPSB ) {
			if ( onepassPSB->getNextHop() ) {
				currentMessage.switchDuplex( onepassPSB->getNextHop()->getAddress() );
			} else {
				currentMessage.switchDuplex();
			}
			processMessage();
		}
		onepassPSB = NULL;
	}
#endif
	// if no error has been generated -> send ACK, if ACK_Desired was set
}

void MessageProcessor::forwardCurrentMessage() {
	if ( currentMessage.getTTL() > 1
		&& !RSVP_Global::rsvp->findInterfaceByAddress( currentHeader.getDestAddress() ) ) {
		LOG(2)( Log::Msg, "ignoring and forwarding message to", currentHeader.getDestAddress() );
		LogicalInterfaceSet lifList;
		RSVP_Global::rsvp->getRoutingService().getMulticastRoute( currentHeader.getSrcAddress(), currentHeader.getDestAddress(), lifList );
		LogicalInterfaceSet::ConstIterator lifIter = lifList.begin();
		for ( ; lifIter != lifList.end(); ++lifIter ) {
			if ( *lifIter != incomingLif ) {
				(*lifIter)->sendMessage( currentMessage, currentHeader.getDestAddress(), currentHeader.getSrcAddress() );
			} else {
				LOG(1)( Log::Msg, "won't forward message through incoming interface" );
			}
		}
	}
}

void MessageProcessor::refreshReservations() {
	PHOP_RefreshList::Iterator phopIter = phopRefreshList.begin();
	while ( phopIter != phopRefreshList.end() ) {
		PHopSB_Refresh& phopRefresh = *phopIter;

		if ( !phopRefresh.getPHopSB().getPSB_List().empty() ) {

			const LogicalInterface& inLif = phopRefresh.getPHopSB().getHop().getLogicalInterface();
			PSB_List::ConstIterator psbIter = phopRefresh.getPHopSB().getPSB_List().begin();
			resvMsg = new Message( Message::Resv, 63, *currentSession );
			resvPolicy = new POLICY_DATA_Object;
			bool forwardConfirm = false;

#if defined(USE_SCOPE_OBJECT)
			if ( currentSession->getStyle() == WF && phopRefresh.getPHopSB().getPSB_List().size() > 1 ) {
				resvScope = new SCOPE_Object;
			}
#endif

			resvMsg->setSTYLE_Object( currentSession->getStyle() );
			resvLength = resvMsg->getLength();

			LOG(2)( Log::Process, "calculating RESV for", phopRefresh.getPHopSB().getAddress() );

			if ( currentSession->getStyle() == FF ) {
				forwardConfirm = createResvMessageFF( phopRefresh );
			} else {
				forwardConfirm = createResvMessageShared( phopRefresh );
			}

			if ( resvLength == resvMsg->getLength() ) {
				delete resvMsg; resvMsg = NULL;
				if ( fullRefresh ) {
					LOG(2)( Log::Process, "stopping RESV for", phopRefresh.getPHopSB().getAddress() );
					phopRefresh.getPHopSB().stopResvRefresh();
				} else {
					LOG(2)( Log::Process, "no reservations found => no RESV needed for", phopRefresh.getPHopSB().getAddress() );
				}
		goto endloop;
			}

#if defined(USE_SCOPE_OBJECT)
			if ( resvScope ) {
				if ( resvScope->getAddressList().empty() ) {
					LOG(2)( Log::Process, "empty SCOPE => no RESV needed for", phopRefresh.getPHopSB().getAddress() );
					delete resvMsg; resvMsg = NULL;
		goto endloop;
				} else {
					resvMsg->setSCOPE_Object( *resvScope );
				}
			}
#endif

			LOG(2)( Log::Process, "creating RESV for PHOP", phopRefresh.getPHopSB().getAddress() );

			//$$$$ Comply with GMPLS RSVP-TE (Section 8.1.2 of RFC 3473)
			resvMsg->setRSVP_HOP_Object( (*psbIter)->getDataInRsvpHop() );

			resvMsg->setTIME_VALUES_Object( inLif.getRefreshInterval() );

			//$$$$ DRAGON specific
			if (currentMessage.getDRAGON_UNI_Object())
				resvMsg->setUNI_Object(*currentMessage.getDRAGON_UNI_Object());
			if (currentMessage.getDRAGON_EXT_INFO_Object())
				resvMsg->setDRAGON_EXT_INFO_Object(*currentMessage.getDRAGON_EXT_INFO_Object());
			
			//$$$$ Always return the suggested LABEL_Object in RESV messaage (important for VLSR--Movaz interop)
			if ( (*psbIter)->hasSUGGESTED_LABEL_Object() ) {
                            const SUGGESTED_LABEL_Object & suggestedLabelObject = (*psbIter)->getSUGGESTED_LABEL_Object();
                            const LABEL_Object labelObject(suggestedLabelObject.getLabel(), suggestedLabelObject.getLabelCType());
                            resvMsg->setLABEL_Object(labelObject);
			}
			//$$$$ end

			if ( forwardConfirm && currentMessage.hasRESV_CONFIRM_Object() ) {
				RESV_CONFIRM_Object confirm = currentMessage.getRESV_CONFIRM_Object();
#if defined(WITH_API)
				if ( confirm.getAddress() == NetAddress(0) ) {
					confirm.setAddress( inLif.getLocalAddress() );
				}
#endif
				resvMsg->setRESV_CONFIRM_Object( confirm );
			}

			if ( resvPolicy && resvPolicy->size() != POLICY_DATA_Object::emptySize() ) {
				resvMsg->addPOLICY_DATA_Object( *resvPolicy );
			}

			resvMsg->addUnknownObjects( currentMessage.getUnknownObjectList() );

endloop:
			if ( !phopRefresh.getRemovePSB_List().empty() ) {
				sendTearMessage( phopRefresh );
			}

			if ( resvMsg ) {
				phopRefresh.getPHopSB().startResvRefreshWithMessage( *resvMsg, fullRefresh );
				delete resvMsg;
			}

#if defined(USE_SCOPE_OBJECT)
			if ( resvScope ) {
				resvScope->destroy();
				resvScope = NULL;
			}
#endif

			if (resvPolicy) resvPolicy->destroy();
		}
		phopIter = phopRefreshList.erase( phopIter );
	}

	confirmOutISB = NULL;
}

inline bool MessageProcessor::checkCurrentLif() {
	/*const LogicalInterface* checkLif = NULL;
#if defined(WITH_API)
	if ( currentLif == RSVP_Global::rsvp->getApiLif() ) {
		checkLif = currentLif;
	} else
#endif
	if ( currentMessage.getMsgType() == Message::Resv || currentMessage.getMsgType() == Message::ResvTear ) {
		currentLif = RSVP_Global::rsvp->findInterfaceByAddress( currentHeader.getDestAddress() );
		checkLif = RSVP_Global::rsvp->findInterfaceByLIH( currentMessage.getRSVP_HOP_Object().getLIH() );
	} else if ( currentMessage.getMsgType() == Message::PathErr || currentMessage.getMsgType() == Message::ResvErr ) {
		checkLif = RSVP_Global::rsvp->findInterfaceByAddress( currentHeader.getDestAddress() );
	} else {
		checkLif = currentLif;
	}
	if ( currentLif == NULL ) {
		return false;
	}
	if ( currentLif != checkLif ) {
		LOG(6)( Log::Msg, "dest", currentHeader.getDestAddress(), "cannot determine applicable interface being", currentLif->getName(), "or", (checkLif ? checkLif->getName() : String("none")) );
		return false;
	}*/
	return true;
}

inline void MessageProcessor::findSendingHop() {
	if ( currentMessage.getMsgType() != Message::PathErr
		&& currentMessage.getMsgType() != Message::PathTear
		&& currentMessage.getMsgType() != Message::ResvConf ) {
#if !defined(REFRESH_REDUCTION)
		sendingHop = RSVP_Global::rsvp->findHop( *currentLif, currentMessage.getRSVP_HOP_Object().getAddress(), true );
#else
		sendingHop = RSVP_Global::rsvp->findHop( *currentLif, currentHeader.getSrcAddress(), true );

#if defined(WITH_API)
		if ( currentLif != RSVP_Global::rsvp->getApiLif() )
#endif
		sendingHop->setRefreshReductionCapable( currentMessage.getFlags() & Message::RefreshReduction );
#endif
	}
}

inline bool MessageProcessor::checkPathMessage() {
/*
	//We don't need to adjust src and dest address, do we?
	//since now out-of-band is possible
	
	if ( currentHeader.getSrcAddress() != currentMessage.getSENDER_TEMPLATE_Object().getSrcAddress() ) {
		LOG(4)( Log::Msg, "adjusting source address from", currentHeader.getSrcAddress(), "to", currentMessage.getSENDER_TEMPLATE_Object().getSrcAddress() );
		currentHeader.setSrcAddress( currentMessage.getSENDER_TEMPLATE_Object().getSrcAddress() );
	}
	if ( currentHeader.getDestAddress() != currentMessage.getSESSION_Object().getDestAddress() ) {
		LOG(4)( Log::Msg, "adjusting dest address from", currentHeader.getDestAddress(), "to", currentMessage.getSESSION_Object().getDestAddress() );
		currentHeader.setDestAddress( currentMessage.getSESSION_Object().getDestAddress() );
	}
*/
	return true;
}

void MessageProcessor::sendTearMessage( PHopSB_Refresh& phopRefresh ) {
	LOG(2)( Log::Process, "creating RTEAR for", phopRefresh.getPHopSB().getAddress() );
	const LogicalInterface& inLif = phopRefresh.getPHopSB().getHop().getLogicalInterface();
	Message tearMsg( Message::ResvTear, 63, *currentSession );

	PSB_List::ConstIterator psbIter = phopRefresh.getRemovePSB_List().begin();
	tearMsg.setRSVP_HOP_Object( (*psbIter)->getDataInRsvpHop() );

	//$$$$ DRAGON specific
	if ((*psbIter)->getDRAGON_UNI_Object())
		tearMsg.setUNI_Object(*(DRAGON_UNI_Object*)(*psbIter)->getDRAGON_UNI_Object());
	if ((*psbIter)->getDRAGON_EXT_INFO_Object())
		tearMsg.setDRAGON_EXT_INFO_Object(*(DRAGON_EXT_INFO_Object*)(*psbIter)->getDRAGON_EXT_INFO_Object());
	
	tearMsg.setSTYLE_Object( currentSession->getStyle() );
	bool sendTear = false;
	if ( currentSession->getStyle() == WF ) {
		if ( phopRefresh.getPHopSB().getForwardFlowspec() == *RSVP::zeroFlowspec ) {
			sendTear = true;
		}
	} else {
		PSB_List::ConstIterator psbIter = phopRefresh.getRemovePSB_List().begin();
		for ( ; psbIter != phopRefresh.getRemovePSB_List().end(); ++psbIter ) {
			if ( (*psbIter)->getForwardFlowspec() == NULL ) {
				sendTear = true;
				tearMsg.addFILTER_SPEC_Objects( FILTER_SPEC_Object(**psbIter) );
				delete *psbIter; //???
			}
		}
	}
	
	if ( sendTear ) {
		inLif.sendMessage( tearMsg, phopRefresh.getPHopSB().getAddress() );
	}
}

bool MessageProcessor::createResvMessageShared( PHopSB_Refresh& phopRefresh ) {
	assert( resvMsg->getSTYLE_Object().getStyle() != FF);
	const FLOWSPEC_Object* currentFlowspec = phopRefresh.getPHopSB().getForwardFlowspec().borrow();
	phopRefresh.calculateForwardFlowspec( B_Merge );
	const FLOWSPEC_Object& newFlowspec = phopRefresh.getPHopSB().getForwardFlowspec();
	FilterSpecList resvFilterList;

#if defined(ONEPASS_RESERVATION)
	if ( phopRefresh.getPHopSB().isOnepassAll() ) {
		assert( !confirmOutISB );
		currentFlowspec->destroy();
	return false;
	}
#endif

	PSB_List::ConstIterator psbIter, psbBegin, psbEnd;
	if ( fullRefresh ) {
		psbBegin = phopRefresh.getPHopSB().getPSB_List().begin();
		psbEnd = phopRefresh.getPHopSB().getPSB_List().end();
	} else {
		psbBegin = phopRefresh.getRefreshPSB_List().begin();
		psbEnd = phopRefresh.getRefreshPSB_List().end();
	}
	for ( psbIter = psbBegin; psbIter != psbEnd; ++psbIter ) {
		if ( (*psbIter)->getForwardFlowspec() != NULL ) {
			if ( currentSession->getStyle() != WF && (*psbIter)->getInLabel() ) {
				resvFilterList.push_back( FILTER_SPEC_Object( **psbIter, (*psbIter)->getInLabel()->getLabel() ) );
			} else
			resvFilterList.push_back( **psbIter );
#if defined(USE_SCOPE_OBJECT)
			if (resvScope) resvScope->addAddress( (*psbIter)->getSrcAddress() );
#endif
		}
	}

	if ( !resvFilterList.empty() ) {
		resvMsg->setFLOWSPEC_Object( newFlowspec );
		if ( resvMsg->getSTYLE_Object().getStyle() == SE ) {
			resvMsg->addFILTER_SPEC_Objects( resvFilterList );
		}
	}

	// decide about confirm
	bool forwardConfirm = true;
	if ( confirmOutISB ) {
		if (
#if defined(WITH_API)
			// 1) if any PSB from this phop is an API client, assume all PSBs are
			// 2) a PSB must exist, otherwise we wouldn't be here
			phopRefresh.getPHopSB().getPSB_List().front()->isFromAPI() ||
#endif
		( newFlowspec <= *currentFlowspec && currentSession->getStyle() != SE ) ) {
			addToConfirmMsg( confirmOutISB->getForwardFlowspec(), resvFilterList );
			forwardConfirm = false;
		}
	}
	currentFlowspec->destroy();
	return forwardConfirm;
}

bool MessageProcessor::createResvMessageFF( PHopSB_Refresh& phopRefresh ) {
                          assert( resvMsg->getSTYLE_Object().getStyle() == FF);
	PSB_List::ConstIterator psbIter, psbBegin, psbEnd;
	if ( fullRefresh ) {
		psbBegin = phopRefresh.getPHopSB().getPSB_List().begin();
		psbEnd = phopRefresh.getPHopSB().getPSB_List().end();
	} else {
		psbBegin = phopRefresh.getRefreshPSB_List().begin();
		psbEnd = phopRefresh.getRefreshPSB_List().end();
	}
	bool forwardConfirm = true;
	for ( psbIter = psbBegin; psbIter != psbEnd; ++psbIter ) {
		PSB* psb = *psbIter;
		const FLOWSPEC_Object* currentFlowspec = psb->getForwardFlowspec();
		if ( currentFlowspec ) currentFlowspec->borrow();
		psb->calculateForwardFlowspec( B_Merge );
		const FLOWSPEC_Object* newFlowspec = psb->getForwardFlowspec();
		if ( newFlowspec && (fullRefresh || !currentFlowspec || *newFlowspec != *currentFlowspec) ) {
#if defined(ONEPASS_RESERVATION)
			if ( !psb->isOnepassAll() )
#endif
			if ( currentSession->getStyle() != WF && psb->getInLabel() ) {
				addToMessage( *resvMsg, *newFlowspec, FILTER_SPEC_Object(*psb, psb->getInLabel()->getLabel() ) );
			} else
			addToMessage( *resvMsg, *newFlowspec, FILTER_SPEC_Object(*psb) );
		}
		if ( confirmOutISB ) {
			if (
#if defined(WITH_API)
			psb->isFromAPI() ||
#endif
			(newFlowspec && currentFlowspec && *newFlowspec <= *currentFlowspec) ) {
				addToConfirmMsg( confirmOutISB->getForwardFlowspec(), FILTER_SPEC_Object(*psb, psb->getOutLabel()) );
				forwardConfirm = false;
			}
		}
		if ( currentFlowspec ) currentFlowspec->destroy();
	} // for PSBs
	return forwardConfirm;
}

inline void MessageProcessor::addToMessage( Message& msg, const FLOWSPEC_Object& fspec, const FilterSpecList& filterList ) {
	if ( msg.getSTYLE_Object().getStyle() == FF ) {
		msg.addFLOWSPEC_Object( fspec );
	} else {
		msg.setFLOWSPEC_Object( fspec );
	}
	if ( msg.getSTYLE_Object().getStyle() != WF ) {
		msg.addFILTER_SPEC_Objects( filterList );
	}
}

inline void MessageProcessor::prepareConfirmMsg() {
	confirmMsg = new Message( Message::ResvConf, 63, *currentSession );
	confirmMsg->setSTYLE_Object( currentMessage.getSTYLE_Object() );
	confirmLength = confirmMsg->getLength();
}

inline void MessageProcessor::finishAndSendConfirmMsg() {
	if ( !confirmMsg ) return;
	if ( confirmLength != confirmMsg->getLength() ) {
		confirmMsg->setRESV_CONFIRM_Object( currentMessage.getRESV_CONFIRM_Object() );
		confirmMsg->setERROR_SPEC_Object( ERROR_SPEC_Object( currentLif->getLocalAddress(), 0,0,0 ) );
		if (currentLif->getAddress() != LogicalInterface::noGatewayAddress) {
			NetAddress peer;
			RSVP_Global::rsvp->getRoutingService().getPeerIPAddr(currentLif->getAddress(), peer);
			currentLif->sendMessage( *confirmMsg, peer);
		}
		else
			currentLif->sendMessage( *confirmMsg, currentMessage.getRESV_CONFIRM_Object().getAddress() );
	}
	delete confirmMsg;
	confirmMsg = NULL;
}

void MessageProcessor::readCurrentMessage( const LogicalInterface& cLif ) {
	MessageEntry *msgEntry = NULL;
	MessageQueue::Iterator msgIter;

	ibuffer.init();
	currentLif = cLif.receiveBuffer( ibuffer, currentHeader );
	if ( currentLif ) {
		LOG(2)( Log::Packet, "real incoming interface is", currentLif->getName() );
		currentMessage.init();
		if ( currentLif->parseBuffer( ibuffer, currentHeader, currentMessage ) ) {
			incomingLif = currentLif;
			if ( currentMessage.getStatus() == Message::Reject ) {
				LOG(5)( Log::Msg, currentLif->getName(), "rejects MSG from", currentHeader.getSrcAddress(), ":", currentMessage );
				currentMessage.revertToError( ERROR_SPEC_Object( currentLif->getLocalAddress(), 0, ERROR_SPEC_Object::UnknownObjectClass, 0 ) );
				if (currentLif->getAddress() != LogicalInterface::noGatewayAddress) {
					NetAddress peer;
					RSVP_Global::rsvp->getRoutingService().getPeerIPAddr(currentLif->getAddress(), peer);
					currentLif->sendMessage( currentMessage, peer );
				}
				else
					currentLif->sendMessage( currentMessage, currentHeader.getSrcAddress() );
			} else if ( checkCurrentLif() ) {
				//@@@@ Xi2007 >>
				bool processNow = true;
				if ( currentMessage.hasSESSION_Object() )
					currentSession = RSVP_Global::rsvp->findSession( currentMessage.getSESSION_Object(), false );
				else
					currentSession = NULL;
				//Only do this for Resv messages in main session
				if (currentMessage.getMsgType() == Message::Resv && currentSession && currentSession->getSubnetUniSrc()) {
					switch (currentSession->getSubnetUniSrc()->getUniState()) {
					case Message::Resv:
					case Message::ResvConf:
					case Message::PathErr:
					case Message::ResvErr:
					case Message::PathTear:
					case Message::ResvTear:
					case Message::InitAPI: //Inital state
						//Message OK for processing since UNI session has gone thru a whole cycle
						break;
					default:
						processNow = false;
						// search for currentSession in msgQueue to avoid enqueuing duplicate entries
						msgIter = msgQueue->begin();
						for (; msgIter != msgQueue->end(); ++msgIter) {
							if ((*msgIter)->getCurrentSession() == currentSession)
								break;
						}
						if ( msgIter != msgQueue->end() ) // The entry has already existed. --> same message received ...
							break;

						//otherwise enqueue current main-session Resv Message while UNI session is pending for return
						msgEntry = new MessageEntry;
						msgEntry->preserveMessage((LogicalInterface*)currentLif, currentSession, currentMessage);
						msgQueue->push_front(msgEntry);
						break;
					}
				}
				//@@@@ Xi2007 <<
				if (processNow) {
					processMessage();
				}
			} else if ( currentLif ) {
				if ( currentMessage.getMsgType() == Message::Resv ) {
					// LIH does not match interface -> no such PATH state available
					currentMessage.revertToError( ERROR_SPEC_Object( currentLif->getLocalAddress(), 0, ERROR_SPEC_Object::NoPathInformation, 0 ) );
					if (currentLif->getAddress() != LogicalInterface::noGatewayAddress) {
						NetAddress peer;
						RSVP_Global::rsvp->getRoutingService().getPeerIPAddr(currentLif->getAddress(), peer);
						currentLif->sendMessage( currentMessage, peer );
					}
					else
						currentLif->sendMessage( currentMessage, currentHeader.getSrcAddress() );
				} else {
					LOG(5)( Log::Msg, currentLif->getName(), "ignores MSG from", currentHeader.getSrcAddress(), ":", currentMessage );
				}
			} else {
				forwardCurrentMessage();
			}
			currentLif = NULL;
			incomingLif = NULL;
			sendingHop = NULL;
		}
	}
}

void MessageProcessor::internalResvRefresh( Session* s, PHopSB& phopState ) {
                                            assert( phopRefreshList.empty() );
	phopRefreshList.push_back( PHopSB_Refresh(phopState) );
	currentSession = s;
	refreshReservations();
}

void MessageProcessor::resurrectResvRefresh( Session* s, PHopSB& phopState ) {
                                            assert( phopRefreshList.empty() );
	fullRefresh = true;
	phopRefreshList.push_back( PHopSB_Refresh(phopState) );
	currentSession = s;
	refreshReservations();
}

void MessageProcessor::processAsyncRoutingEvent( Session* s, const NetAddress& src,
	const LogicalInterface& inLif, LogicalInterfaceSet lifList ) {
                                   assert( s->getDestAddress().isMulticast() );
	currentSession = s;
	currentSession->processAsyncRoutingEvent( src, inLif, lifList );
	refreshReservations();
}

void MessageProcessor::processAsyncRoutingEvent( Session* s, const LogicalInterface& outLif, const NetAddress& gw ) {
                                  assert( !s->getDestAddress().isMulticast() );
	currentSession = s;
	currentSession->processAsyncRoutingEvent( outLif, gw );
	refreshReservations();
}

void MessageProcessor::markForResvRefresh( PSB& psb ) {
#if defined(ONEPASS_RESERVATION)
	if ( psb.isOnepassAll() ) return;
#endif
	// in case of a WF PATH message from a new incoming interface from which
	// other PATH messages are already received, the reservation does not need
	// to (?must not?) be refreshed.
	if ( currentSession && currentSession->getStyle() == WF
		&& currentMessage.getMsgType() == Message::Path ) {
		if ( psb.getPHopSB().getPSB_List().size() > 1 ) {
	return;
		}
	}
	PHOP_RefreshList::Iterator iter = phopRefreshList.insert_unique( PHopSB_Refresh( psb.getPHopSB() ) );
	(*iter).markForResvRefresh( psb );
}

void MessageProcessor::markForResvRemove( PSB& psb ) {
#if defined(ONEPASS_RESERVATION)
	if ( psb.isOnepass() ) return;
#endif
	if ( currentMessage.getMsgType() != Message::Resv ) {
		PHOP_RefreshList::Iterator iter = phopRefreshList.insert_unique( PHopSB_Refresh( psb.getPHopSB() ) );
		(*iter).markForResvRemove( psb );
	}
}

void MessageProcessor::addToConfirmMsg( const FLOWSPEC_Object& fspec, const FilterSpecList& filterList ) {
	if ( !confirmMsg ) return;
	addToMessage( *confirmMsg, fspec, filterList );
}

void MessageProcessor::sendResvErrMessage( uint8 errorFlags, uint8 errorCode, uint16 errorValue ) {
	FlowDescriptorList::ConstIterator flowdescIter = currentMessage.getFlowDescriptorList().begin();
	for ( ; flowdescIter != currentMessage.getFlowDescriptorList().end() ; ++flowdescIter ) {
		sendResvErrMessage( errorFlags, errorCode, errorValue, *flowdescIter );
	}
}

void MessageProcessor::sendResvErrMessage( uint8 errorFlags, uint8 errorCode, uint16 errorValue, const FlowDescriptor& fd ) {
	if (Session::ospfRouterID.rawAddress() == 0)
		Session::ospfRouterID = RSVP_Global::rsvp->getRoutingService().getLoopbackAddress();

	assert( currentMessage.getMsgType() == Message::Resv );
	ERROR_SPEC_Object error( Session::ospfRouterID.rawAddress()==0?currentLif->getLocalAddress():Session::ospfRouterID, errorFlags, errorCode, errorValue );
	Message errorMsg( Message::ResvErr, 63, currentMessage.getSESSION_Object() );
	errorMsg.setERROR_SPEC_Object( error );
	if ( currentMessage.getSCOPE_Object() ) {
		errorMsg.setSCOPE_Object( *currentMessage.getSCOPE_Object() );
	}
	errorMsg.setSTYLE_Object( currentMessage.getSTYLE_Object() );
	addToMessage( errorMsg, *fd.getFlowspec(), fd.filterSpecList );
	errorMsg.setRSVP_HOP_Object( *currentLif );
	if (currentLif->getAddress() != LogicalInterface::noGatewayAddress) {
		NetAddress peer;
		RSVP_Global::rsvp->getRoutingService().getPeerIPAddr(currentLif->getAddress(), peer);
		currentLif->sendMessage( errorMsg, peer );
	}
	else
		currentLif->sendMessage( errorMsg, currentMessage.getRSVP_HOP_Object().getAddress() );
}

void MessageProcessor::sendPathErrMessage( uint8 errorCode, uint16 errorValue ) {
	if (Session::ospfRouterID.rawAddress() == 0)
		Session::ospfRouterID = RSVP_Global::rsvp->getRoutingService().getLoopbackAddress();

	assert( currentMessage.getMsgType() == Message::Path || currentMessage.getMsgType() == Message::PathResv );
	ERROR_SPEC_Object error( Session::ospfRouterID.rawAddress()==0?currentLif->getLocalAddress():Session::ospfRouterID, 0, errorCode, errorValue );
	NetAddress dest = currentMessage.getRSVP_HOP_Object().getAddress();
	currentMessage.revertToError( error );
#if defined(REFRESH_REDUCTION)
//		if ( sendingHop->isRefreshReductionCapable() ) {
//			currentMessage.setMESSAGE_ID_Object( sendingHop->getNextSendID() );
//		}
#endif
	if (currentLif->getAddress() != LogicalInterface::noGatewayAddress) {
		NetAddress peer;
		RSVP_Global::rsvp->getRoutingService().getPeerIPAddr(currentLif->getAddress(), peer);
		currentLif->sendMessage( currentMessage, peer );
	}
	else
		currentLif->sendMessage( currentMessage, dest );
}

#if defined(WITH_API)
void MessageProcessor::deregisterAPI( const SESSION_Object& session, const NetAddress& address, uint16 port ) {
	currentSession = RSVP_Global::rsvp->findSession( session );
	if ( currentSession ) {
		currentSession->deregisterAPI( address, port );
		refreshReservations();
	}
}

void MessageProcessor::registerAPI( const SESSION_Object& session ) {
	currentSession = RSVP_Global::rsvp->findSession( session );
	if ( currentSession ) currentSession->registerAPI();
}

// Xi2007 >>
bool MessageProcessor::queryEnqueuedMessages( ) {
	if ( !msgQueue)
		return false;

	MessageEntry* msgEntry;
	MessageQueue::Iterator msgIter = msgQueue->begin();
	for ( ; msgIter != msgQueue->end(); ++msgIter ) {
		msgEntry = *msgIter;
		if (msgEntry->getCurrentSession() && msgEntry->getCurrentSession()->getSubnetUniSrc() ) {
			switch (msgEntry->getCurrentSession()->getSubnetUniSrc()->getUniState()) {
			case Message::Resv:
			case Message::ResvConf:
			case Message::PathErr:
			case Message::PathTear:
			case Message::ResvTear:
				
				//Restore current message and MessageProcessor scene
				msgEntry->restoreMessage((LogicalInterface* &)currentLif, currentSession, currentMessage);
				//Dequeue messageEntry
				msgQueue->erase(msgIter);
				//MessageProcessor restored and ready for processing --> return true;
				return true;

				break;
			default:

				// do nothing

				break;
			}

		}
	}

	// no message restored, no processing after return
	return false;
}

// Xi2007 <<

// DRAGON Monitoring >>
void MessageProcessor::processDragonMonQuery(SESSION_Object& sessionObject, MON_Query_Subobject& monQuery)
{
	uint8 msgType = Message::MonReply;
	uint8 TTL = 1;
	Message replyMsg( msgType, TTL, sessionObject);
	DRAGON_EXT_INFO_Object* dragonExtInfo = new DRAGON_EXT_INFO_Object;
	MON_Reply_Subobject monReply;
	memset (&monReply, 0, sizeof(MON_Reply_Subobject));
       monReply.type = DRAGON_EXT_SUBOBJ_MON_REPLY;
	monReply.ucid = monQuery.ucid;
	monReply.seqnum = monQuery.seqnum;
       strncpy(monReply.gri, monQuery.gri, MAX_MON_NAME_LEN-1);

	//$$$$ retrieve switch/circuit information
	RSVP_Global::switchController->getMonitoringInfo(monQuery, monReply, sessionObject.getDestAddress().rawAddress());

	dragonExtInfo->SetMonReply(monReply);
	replyMsg.setDRAGON_EXT_INFO_Object(*dragonExtInfo);
	RSVP::getApiServer().sendMessage(replyMsg);
	dragonExtInfo->destroy();
}
// DRAGON Monitoring <<

#endif
