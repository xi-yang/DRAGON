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
#include "RSVP_RSB.h"
#include "RSVP.h"
#include "RSVP_Global.h"
#include "RSVP_Hop.h"
#include "RSVP_IntServObjects.h"
#include "RSVP_Log.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_MessageProcessor.h"
#include "RSVP_OutISB.h"
#include "RSVP_Session.h"

inline void RSVP::increaseReservationCount() {
#if defined(RSVP_STATS)
	currentReservationCount += 1;
#endif
}

inline void RSVP::decreaseReservationCount() {
#if defined(RSVP_STATS)
	currentReservationCount -= 1;
#endif
}

ostream& operator<< ( ostream& os, const RSB& rsb ) {
	os << "RSB(nhop):" << rsb.getNextHop().getAddress();
	return os;
}

inline RSB_Contents::RSB_Contents() : flowspec(new FLOWSPEC_Object),
	scope(NULL) {}

RSB_Contents::~RSB_Contents() {
	flowspec->destroy();
	if ( scope ) scope->destroy();
}

bool RSB_Contents::updateContents( const FLOWSPEC_Object& f, const SCOPE_Object* s ) {
	bool result = false;
	if ( *flowspec != f ) {
		flowspec->destroy();
		flowspec = f.borrow();
		result = true;
	}
	if ( !pointerContentsEqual( scope, s ) ) {
		if ( scope ) scope->destroy();
		scope = s;
		if ( scope ) scope->borrow();
		result = true;
	}
	return result;
}

RSB_Contents* RSB_Contents::createBackup() const {
	RSB_Contents* rsb = new RSB_Contents;
	rsb->updateContents( *flowspec, scope );
	PSB_List::ConstIterator iter = Relationship_ToPSB::followRelationship().begin();
	for ( ; iter != Relationship_ToPSB::followRelationship().end(); ++iter ) {
		rsb->Relationship_ToPSB::followRelationship().push_back( *iter );
	}
	return rsb;
}

RSB::RSB( Hop& nhop ) : RSB_Key(nhop), lifetimeTimer(*this) {
	LOG(2)( Log::SB, "creating", *this );
	nhop.addSB();
	RSVP_Global::rsvp->increaseReservationCount();
#if defined(REFRESH_REDUCTION)
	recvID = NULL;
#endif
}

RSB::~RSB() {
	LOG(2)( Log::SB, "deleting", *this );
#if defined(REFRESH_REDUCTION)
	if ( recvID ) nhop.clearRecvRSB( recvID->id, this );
#endif
	OutISB* oisb = RelationshipRSB_OutISB::followRelationship();
            assert( oisb || Relationship_ToPSB::followRelationship().empty() );
	if (oisb) {
		setFiltersAndTimeout( PSB_List(), 0, true );
		nhop.getLogicalInterface().getTC().updateTC( *oisb, this, TrafficControl::RemovedRSB );
	}
	RSVP_Global::rsvp->decreaseReservationCount();
	nhop.removeSB();
}

void RSB::replaceWithBackup( const RSB_Contents& rsb, uint32 timeout ) {
	updateContents( rsb.getFLOWSPEC_Object(), rsb.getSCOPE_Object() );
	setFiltersAndTimeout( rsb.getPSB_List(), timeout, true );
}

bool RSB::setFiltersAndTimeout( const PSB_List& newList, uint32 timeout, bool remove,
	PSB_List* newFilterList, FilterSpecList* knownFilterList ) {

	uint32 LIH = nhop.getLIH();
	PSB_List::ConstIterator iter1 = Relationship_ToPSB::followRelationship().begin();
	PSB_List::ConstIterator iter2 = newList.begin();
	bool result = false;
	while ( iter1 != Relationship_ToPSB::followRelationship().end() && iter2 != newList.end() ) {
		if ( **iter1 < **iter2 ) {
			if (remove) {
				(*iter1)->removeReservation( LIH );
				iter1 = Relationship_ToPSB::followRelationship().erase( iter1 );
				result = true;
			} else {
				++iter1;
			}
		} else if ( **iter2 < **iter1 ) {
			Relationship_ToPSB::followRelationship().insert( iter1, *iter2 );
			if ( (*iter2)->addReservation( RelationshipRSB_OutISB::followRelationship(), getNextHop() ) ) {
				if ( newFilterList ) newFilterList->push_back( *iter2 );
			} else {
				if ( knownFilterList ) knownFilterList->push_back( **iter2 );
			}
			(*iter2)->refreshReservation( LIH, timeout );
			result = true;
			++iter2;
		} else {
			if ( knownFilterList ) knownFilterList->push_back( **iter2 );
			(*iter2)->refreshReservation( LIH, timeout );
			++iter2;
			++iter1;
		}
	}
	while ( iter2 != newList.end() ) {
		Relationship_ToPSB::followRelationship().insert( iter1, *iter2 );
		if ( (*iter2)->addReservation( RelationshipRSB_OutISB::followRelationship(), getNextHop() ) ) {
			if ( newFilterList ) newFilterList->push_back( *iter2 );
		} else {
			if ( knownFilterList ) knownFilterList->push_back( **iter2 );
		}
		(*iter2)->refreshReservation( LIH, timeout );
		result = true;
		++iter2;
	}
	if (remove) {
		while ( iter1 != Relationship_ToPSB::followRelationship().end() ) {
			(*iter1)->removeReservation( LIH );
			iter1 = Relationship_ToPSB::followRelationship().erase( iter1 );
			result = true;
		}
	}
	if ( !Relationship_ToPSB::followRelationship().empty() ) {
		lifetimeTimer.restart( multiplyTimeoutTime(timeout) );
	}
	return result;
}

void RSB::restartTimeout() {
	lifetimeTimer.restart();
	PSB_List::Iterator iter = Relationship_ToPSB::followRelationship().begin();
	for ( ; iter != Relationship_ToPSB::followRelationship().end(); ++iter ) {
		(*iter)->refreshReservation( nhop.getLIH() );
	}
}

bool RSB::teardownFilters( const FilterSpecList& removeList ) {
	PSB_List::Iterator iter1 = Relationship_ToPSB::followRelationship().begin();
	FilterSpecList::ConstIterator iter2 = removeList.begin();
	while ( iter2 != removeList.end() && iter1 != Relationship_ToPSB::followRelationship().end() ) {
		if ( *iter2 < **iter1 ) {
			LOG(2)( Log::SB, "RSB: filter for removal not found: ", *iter2 );
			++iter2;
		} else if ( **iter1 < *iter2 ) {
			LOG(2)( Log::SB, "RSB: filter for removal no match found: ", **iter1 );
			++iter1;
		} else {
			(*iter1)->removeReservation( nhop.getLIH() );
			iter1 = Relationship_ToPSB::followRelationship().erase( iter1 );
			++iter2;
		}
	}
	return Relationship_ToPSB::followRelationship().empty();
}

inline void RSB::timeout() {
	ERROR(5)( Log::Error, "RSB timeout:", getSession(), "timeout", (DaytimeTimeValue&)lifetimeTimer.getAlarmTime(), "fired" );
	RSVP_Global::messageProcessor->timeoutRSB( this );
}

inline void MessageProcessor::timeoutRSB( RSB* rsb ) {
	currentSession = &rsb->getSession();
                                         assert( !rsb->getPSB_List().empty() );
	currentSession->decreaseRSB_Count();
	delete rsb;
	refreshReservations();
}
