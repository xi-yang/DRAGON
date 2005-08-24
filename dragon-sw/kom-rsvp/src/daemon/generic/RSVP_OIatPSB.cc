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
#include "RSVP_OIatPSB.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_MPLS.h"
#include "RSVP_MessageProcessor.h"
#include "RSVP_OutISB.h"
#include "RSVP_PHopSB.h"
#include "RSVP_Session.h"

ostream& operator<< (ostream& os, const OIatPSB& o ) {
	os << "OIatPSB:" << *static_cast<const SENDER_Object*>(o.RelationshipOIatPSB_PSB::followRelationship()) << " -> " << RSVP_Global::rsvp->findInterfaceByLIH(o.LIH)->getName();
	return os;
}

OIatPSB::OIatPSB( PSB& psb, uint32 LIH ) : LIH(LIH), rsbCount(0),
	fHandle(NULL), refreshTimer(*this), lifetimeTimer(*this) {
	RelationshipOIatPSB_PSB::setRelationshipFull( this, &psb );
	LOG(2)( Log::SB, "creating:", *this );
	outLabel = NULL;
	outLabelRequested = false;
}

OIatPSB::~OIatPSB() {
	LOG(2)( Log::SB, "deleting OIatPSB:", *this );
	if ( RelationshipOIatPSB_OutISB::followRelationship() ) {
		removeOutISB();
	}
}

// PATH msg added routing from PSB to new OI -> WF and OutISB already existed
void OIatPSB::newOutISB( OutISB* oisb ) {
	// update relationship OutISB <-> this
	RelationshipOIatPSB_OutISB::setRelationshipFull( this, oisb );
	// style is WF => get current number of RSB's from OutISB
	rsbCount = oisb->getRSB_List().size();
	// no timeout is set, because either the RSBs time out or filters are refreshed
	oisb->addPSBtoRSBs( *RelationshipOIatPSB_PSB::followRelationship() );
	// prepare RESV refresh directed to phop
	RSVP_Global::messageProcessor->markForResvRefresh( *RelationshipOIatPSB_PSB::followRelationship() );
}

// PATH msg removed routing from PSB to OI where reservations existed
// PTEAR or PATH timeout removed PSB
// filter timed out
void OIatPSB::removeOutISB() {
	lifetimeTimer.cancel();
	OutISB* oisb = RelationshipOIatPSB_OutISB::followRelationship();
	TrafficControl& tc = oisb->getOI().getTC();
	if ( fHandle ) {
		tc.removeFilter( fHandle );
		tc.updateTC( *oisb, NULL, TrafficControl::ModifiedRSB );
		fHandle = NULL;
	}
	if ( outLabel ) {
		RSVP_Global::rsvp->getMPLS().deleteOutLabel( outLabel );
		outLabel = NULL;
	}
	// update all RSBs -> might lead to deletion of RSBs and potentially OutISB
	oisb->removePSBfromRSBs( *RelationshipOIatPSB_PSB::followRelationship() );
	if ( RelationshipOIatPSB_OutISB::followRelationship() ) {
		RelationshipOIatPSB_OutISB::clearRelationshipFull();
	}
	rsbCount = 0;
}

// RESV msg added reservation
// updateTC is called in Session::processRESV
bool OIatPSB::addRSB( OutISB* oisb ) {
	rsbCount += 1;
	LOG(3)( Log::SB, *this, "addRSB, count is", rsbCount );
	if ( rsbCount == 1 ) {
		if ( getSession().getStyle() != WF ) {
			oisb->getOI().getTC().addFilter( this );
		}
		RelationshipOIatPSB_OutISB::setRelationshipFull( this, oisb );
		return true;
	}
	return false;
}

// RTEAR msg or RSB timeout removed RSB, or
// RESV msg at API removed filter, or
// updateTC is called in ~RSB or Session::processRESV
void OIatPSB::removeRSB() {
	rsbCount -= 1;
	LOG(3)( Log::SB, *this, "removeRSB, count is", rsbCount );
	if ( rsbCount == 0 ) {
		lifetimeTimer.cancel();
		if ( RelationshipOIatPSB_OutISB::followRelationship() ) {
			if ( fHandle ) {
				RelationshipOIatPSB_OutISB::followRelationship()->getOI().getTC().removeFilter( fHandle );
				fHandle = NULL;
			}
			if ( outLabel ) {
				RSVP_Global::rsvp->getMPLS().deleteOutLabel( outLabel );
				outLabel = NULL;
			}
			RelationshipOIatPSB_OutISB::clearRelationshipFull();
			RSVP_Global::messageProcessor->markForResvRemove( *RelationshipOIatPSB_PSB::followRelationship() );
		}
	}
}

void OIatPSB::refresh() {
	RSVP_Global::messageProcessor->refreshOIatPSB( *this );
}

inline void MessageProcessor::refreshOIatPSB( OIatPSB& o ) {
	currentSession = &o.getSession();
	o.doRefresh();
}

inline void OIatPSB::doRefresh() {
	RelationshipOIatPSB_PSB::followRelationship()->sendRefresh( *RSVP_Global::rsvp->findInterfaceByLIH( LIH ) );
}

inline void OIatPSB::timeout() {
	RSVP_Global::messageProcessor->timeoutFilter( this );
}

inline void MessageProcessor::timeoutFilter( OIatPSB* psbOI ) {
	currentSession = &psbOI->getSession();
	psbOI->removeOutISB();
	markForResvRemove( *psbOI->RelationshipOIatPSB_PSB::followRelationship() ); 
	refreshReservations();
}
