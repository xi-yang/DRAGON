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
#include "RSVP_OutISB.h"
#include "RSVP_OIatPSB.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_ProtocolObjects.h"
#include "RSVP_RSB.h"
#include "RSVP_Session.h"
#include "RSVP_TrafficControl.h"

OutISB::OutISB( const LogicalInterface& OI ) : OI(OI),
	effectiveFlowspec( RSVP::zeroFlowspec->borrow() ),
	forwardFlowspec( RSVP::zeroFlowspec->borrow() ),
	rHandle(NULL), fHandleWF(NULL) {
	LOG(2)( Log::SB, "creating OutISB at", OI.getName() );
#if defined(ONEPASS_RESERVATION)
	onepass = false;
	onepassAll = false;
#endif
}

OutISB::~OutISB() {
	LOG(2)( Log::SB, "deleting OutISB at", OI.getName() );
	effectiveFlowspec->destroy();
	forwardFlowspec->destroy();
}

void OutISB::addPSBtoRSBs( PSB& psb ) {
	RSB_List::Iterator iter = RelationshipOutISB_RSB::followRelationship().begin();
	for ( ; iter != RelationshipOutISB_RSB::followRelationship().end(); ++iter ) {
		(*iter)->addPSB( psb );
	}
}

void OutISB::removePSBfromRSBs( PSB& psb ) {
	bool quit = false;
	RSB_List::Iterator iter = RelationshipOutISB_RSB::followRelationship().begin();
	while ( !quit && iter != RelationshipOutISB_RSB::followRelationship().end() ) {
		RSB* rsb = *iter;
		++iter;
		rsb->removePSB( psb );
		if ( rsb->getPSB_List().empty() ) {
			// deleting the last RSB destroys this OutISB as well => quit immediately
			if ( RelationshipOutISB_RSB::followRelationship().size() == 1 ) {
				quit = true;
			}
			delete rsb;
		}
	}
}

#if defined(ONEPASS_RESERVATION)
void OutISB::updateOnepassFlags() {
	onepass = false;
	onepassAll = true;
	RSB_List::ConstIterator iter = getRSB_List().begin();
	for ( ; iter != getRSB_List().end(); ++iter ) {
		onepass = onepass || (*iter)->isOnepass();
		onepassAll = onepassAll && (*iter)->isOnepass();
	}
}
#endif
