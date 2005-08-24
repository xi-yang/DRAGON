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
#ifndef _RSVP_OutISB_h_
#define _RSVP_OutISB_h_

#include "RSVP_FilterSpecList.h"
#include "RSVP_IntServObjects.h"
#include "RSVP_Lists.h"
#include "RSVP_Relationships.h"
#include "RSVP_GeneralMemoryMachine.h"
#include "RSVP_TrafficControl.h"

class LogicalInterface;
class FLOWSPEC_Object;
class OutISB;

typedef Relationship1toMANY<OutISB,RSB,RSB_List,DoOnEmptyDelete1> RelationshipOutISB_RSB;
typedef Relationship1toMANY<OutISB,OIatPSB,OIatPSB_List> RelationshipOutISB_OIatPSB;

class OutISB :	public RelationshipOutISB_RSB,
								public RelationshipOutISB_OIatPSB {

	const LogicalInterface& OI;
	const FLOWSPEC_Object* effectiveFlowspec;
	const FLOWSPEC_Object* forwardFlowspec;
	TrafficControl::RHandle* rHandle;
	TrafficControl::FHandle* fHandleWF;
#if defined(ONEPASS_RESERVATION)
	bool onepass;
	bool onepassAll;
#endif

public:
	OutISB( const LogicalInterface& );
	~OutISB();
	const LogicalInterface& getOI() const { return OI; }
	const FLOWSPEC_Object& getForwardFlowspec() const {
		assert(forwardFlowspec); return *forwardFlowspec;
	}
	void updateForwardFlowspec( const FLOWSPEC_Object& f ) {
		forwardFlowspec->destroy();	forwardFlowspec = f.borrow();
	}
	const FLOWSPEC_Object& getEffectiveFlowspec() const {
		assert(effectiveFlowspec); return *effectiveFlowspec;
	}
	void updateEffectiveFlowspec( const FLOWSPEC_Object& f ) {
		effectiveFlowspec->destroy(); effectiveFlowspec = f.borrow();
	}	
	RSB_List& getRSB_List() { return RelationshipOutISB_RSB::followRelationship(); }
	const RSB_List& getRSB_List() const { return RelationshipOutISB_RSB::followRelationship(); }
	void addPSBtoRSBs( PSB& );
	void removePSBfromRSBs( PSB& );
	void clearRHandle() { rHandle = NULL; }
	void setRHandle( TrafficControl::RHandle* rH ) { rHandle = rH; }
	TrafficControl::RHandle* getRHandle() const { return rHandle; }
	void clearFHandleWF() { fHandleWF = NULL; }
	void setFHandleWF( TrafficControl::FHandle* fH ) { fHandleWF = fH; }
	TrafficControl::FHandle* getFHandleWF() const { return fHandleWF; }
#if defined(ONEPASS_RESERVATION)
	bool isOnepass() { return onepass; }
	bool isOnepassAll() { return onepassAll; }
	void updateOnepassFlags();
#endif

	DECLARE_MEMORY_MACHINE_IN_CLASS(OutISB)
};
DECLARE_MEMORY_MACHINE_OUT_CLASS(OutISB, oisbMemMachine)

#endif /* _RSVP_OutISB_h_ */
