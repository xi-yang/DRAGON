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
#include "RSVP_PHopSB.h"
#include "RSVP.h"
#include "RSVP_BlockSB.h"
#include "RSVP_IntServObjects.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_Message.h"
#include "RSVP_MessageProcessor.h"
#include "RSVP_PSB.h"
#include "RSVP_Session.h"
#include "RSVP_RoutingService.h"

PHopSB::PHopSB( Hop& hop, uint32 LIH ) : PHopSBKey( hop, LIH ),
	refreshBuffer(NULL), refreshTimer(*this), blockadeSB(NULL),
	sharedForwardFlowspec(new FLOWSPEC_Object) {
#if defined(REFRESH_REDUCTION)
	sendID = NULL;
#endif
#if defined(ONEPASS_RESERVATION)
	onepassAll = false;
#endif
	LOG(2)( Log::SB, "creating", *this );
}

PHopSB::~PHopSB() {
	LOG(2)( Log::SB, "deleting", *this );
	stopResvRefresh();
	if ( blockadeSB ) delete blockadeSB;
	sharedForwardFlowspec->destroy();
}

void PHopSB::startResvRefreshWithMessage( Message& m, bool fullRefresh ) {
	if ( !refreshBuffer ) {
		fullRefresh = true;
	}
#if defined(REFRESH_REDUCTION)
	if ( fullRefresh ) {
		if ( sendID ) {
			phop->clearSendState( sendID->id );
			sendID = NULL;
		}
		if ( phop->isRefreshReductionCapable() ) {
			sendID = phop->storeSendPHopSB( this );
		}
	}
	if ( sendID ) {
		m.setMESSAGE_ID_Object( MESSAGE_ID_Object( 0, phop->getEpoch(), sendID->id ) );
	}
#endif
	Session* s = RelationshipPHopSB_Session::followRelationship();
	NetAddress peer;
	RSVP_Global::rsvp->getRoutingService().getPeerIPAddr(s->getInLif()->getAddress(), peer);
	phop->getLogicalInterface().sendMessage( m, peer);
	m.clearRESV_CONFIRM_Object();
	if ( !fullRefresh ) {
		LOG(3)( Log::SB, "PHOP:", getAddress(), "-> partial refresh" );
	} else {
		LOG(3)( Log::SB, "PHOP:", getAddress(), "-> full refresh" );
		if ( refreshBuffer ) delete refreshBuffer;
		refreshBuffer = phop->getLogicalInterface().createOutgoingBuffer( m, getAddress() );
#if defined(REFRESH_REDUCTION)
		if ( !sendID )
#endif
			refreshTimer.restart( phop->getLogicalInterface().getRefreshInterval() );
	}
	fullRefreshNeeded = !fullRefresh;
}

void PHopSB::stopResvRefresh() {
	if (refreshBuffer) {
		delete refreshBuffer;
		refreshBuffer = NULL;
	}
#if defined(REFRESH_REDUCTION)
	if ( sendID ) {
		phop->clearSendState( sendID->id );
		sendID = NULL;
	}
#endif
	refreshTimer.cancel();
}

void PHopSB::refresh() {
	RSVP_Global::messageProcessor->refreshPHopSB( *this, RelationshipPHopSB_Session::followRelationship() );
}

inline void MessageProcessor::refreshPHopSB( PHopSB& p, Session* s ) {
	currentSession = s;
	p.doRefresh();
}

inline void PHopSB::doRefresh() {
	if ( fullRefreshNeeded ) {
		RSVP_Global::messageProcessor->internalResvRefresh( RelationshipPHopSB_Session::followRelationship(), *this );
	} else {
		LOG(3)( Log::SB, "PHOP:", getAddress(), "sending refresh message" );
		phop->getLogicalInterface().sendBuffer( *refreshBuffer, getAddress(), LogicalInterface::noGatewayAddress );
	}
}

void PHopSB::matchPSBsAndFilters( const FilterSpecList& filterList, PSB_List& result ) {
	FilterSpecList::ConstIterator filterIter = filterList.begin();
	PSB_List::Iterator psbIter = RelationshipPHopSB_PSB::followRelationship().begin();
	while ( filterIter != filterList.end() && psbIter != RelationshipPHopSB_PSB::followRelationship().end() ) {
		if ( **psbIter < *filterIter ) {
			++psbIter;
		} else if ( *filterIter < **psbIter ) {
			++filterIter;
		} else {
			result.push_back( *psbIter );
			++filterIter; ++psbIter;
		}
	}
}

void PHopSB::setBlockade( const FLOWSPEC_Object& f, uint32 timeout ) {
	if ( blockadeSB ) delete blockadeSB;
	blockadeSB = new BlockSB<PHopSB>( *this, f, timeout );
	LOG(4)( Log::SB, "set blockaded:", f, "at", *this );
}

inline void PHopSB::clearBlockade() {
                                                         assert( blockadeSB );
	LOG(4)( Log::SB, "clear blockaded:", blockadeSB->getFlowspec(), "at", *this );
	delete blockadeSB; blockadeSB = NULL;
}

bool PHopSB::calculateForwardFlowspec( bool B_Merge ) {
	sharedForwardFlowspec->destroy();
	sharedForwardFlowspec = new FLOWSPEC_Object;
	FLOWSPEC_Object* flowspecGLB = new FLOWSPEC_Object;
	const FLOWSPEC_Object* blockadeFlowspec = blockadeSB ? &blockadeSB->getFlowspec() : NULL;
#if defined(ONEPASS_RESERVATION)
	onepassAll = true;
#endif
	PSB_List::ConstIterator psbIter = getPSB_List().begin();
	for ( ; psbIter != getPSB_List().end(); ++psbIter ) {
		PSB* psb = *psbIter;
		if ( psb->calculateForwardFlowspec( B_Merge, blockadeFlowspec ) ) {
			if ( B_Merge && psb->getForwardFlowspec() ) {
				flowspecGLB->GLB( *psb->getForwardFlowspec() );
			}
		} else if ( psb->getForwardFlowspec() ) {
			sharedForwardFlowspec->LUB( *psb->getForwardFlowspec() );
		}
#if defined(ONEPASS_RESERVATION)
		onepassAll = onepassAll && psb->isOnepassAll();
#endif
	}
	if ( *sharedForwardFlowspec == *RSVP::zeroFlowspec && blockadeFlowspec
		&& *flowspecGLB != *RSVP::zeroFlowspec
		&& *flowspecGLB < *blockadeFlowspec ) {
		sharedForwardFlowspec->destroy();
		sharedForwardFlowspec = flowspecGLB;
	} else {
#if defined(ONEPASS_RESERVATION)
		onepassAll = false;
#endif
		flowspecGLB->destroy();
	}
	LOG(3)( Log::Process, *this, "calculated shared forward flowspec", *sharedForwardFlowspec );
	return (blockadeFlowspec != NULL);
}

void PHopSB_Refresh::markForResvRefresh( PSB& psb ) {
	LOG(3)( Log::Process, RSVP_HOP_Object(*this), "marked for refresh:", psb );
	psbRefreshList.insert_unique( &psb );
}

void PHopSB_Refresh::markForResvRemove( PSB& psb ) {
	LOG(3)( Log::Process, RSVP_HOP_Object(*this), "marked for remove:", psb );
	psbRemoveList.insert_unique( &psb );
}
