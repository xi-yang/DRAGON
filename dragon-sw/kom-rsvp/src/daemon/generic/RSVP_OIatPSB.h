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
#ifndef _RSVP_OIatPSB_h_
#define _RSVP_OIatPSB_h_ 1

#include "RSVP.h"
#include "RSVP_InterfaceArray.h"
#include "RSVP_PSB.h"
#include "RSVP_Relationships.h"
#include "RSVP_GeneralMemoryMachine.h"
#include "RSVP_TrafficControl.h"
#include "RSVP_Timer.h"

class MPLS_OutLabel;

typedef RelationshipMANYto1<OIatPSB,OIatPSB_List,OutISB> RelationshipOIatPSB_OutISB;
typedef RelationshipMANYto1<OIatPSB,OIatPSB_Array,PSB> RelationshipOIatPSB_PSB;

class OIatPSB : public RelationshipOIatPSB_OutISB,
								public RelationshipOIatPSB_PSB {

	friend ostream& operator<< (ostream&, const OIatPSB& );

	uint32 LIH;
	uint32 rsbCount;
	TrafficControl::FHandle* fHandle;
	const MPLS_OutLabel* outLabel;
	bool outLabelRequested;
	uint8 outLabelRequestedType;
	RandomRefreshTimer<OIatPSB> refreshTimer;
	TimeoutTimer<OIatPSB> lifetimeTimer;
public:

	OIatPSB( PSB& psb, uint32 LIH );
	~OIatPSB();
	Session& getSession() { return RelationshipOIatPSB_PSB::followRelationship()->getSession(); }
	PSB& getPSB() { return *RelationshipOIatPSB_PSB::followRelationship(); }
	OutISB* getOutISB() { return RelationshipOIatPSB_OutISB::followRelationship(); }

	// PATH
	// called by PSB to indicate new routing to existing OutISB, only when WF
	void newOutISB( OutISB* );

	// PATH/PTEAR/PSB timeout, filter timeout
	// called by PSB to indicate remove routing to OutISB
	void removeOutISB();

	// RESV
	// called by RSB to indicate new filter from RSB at OutISB
	// return value indicates whether this is the first reservation for this PSB
	// used for refresh/confirm decision
	bool addRSB( OutISB* oisb );

	// RTEAR, RSB timeout
	// called by RSB to indicate removed filter from RSB
	void removeRSB();

	void refresh();
	inline void doRefresh();
	void setRefreshTime( const TimeValue& t ) {
		refreshTimer.restart(t);
	}
	void setTimeout( const TimeValue& timeout ) {
                    assert( RelationshipOIatPSB_OutISB::followRelationship() );
		if ( timeout > lifetimeTimer.getRemainingTime() ) {
			lifetimeTimer.restart( timeout );
		}
	}
	void restartTimeout() {
		lifetimeTimer.restart();
	}
	void setLocalRepairRefresh( const TimeValue& w ) {
		refreshTimer.restartOnce( w );
	}
	inline void timeout();
	uint32 getLIH() { return LIH; }
	void setFHandle( TrafficControl::FHandle* fH ) { fHandle = fH; }
	void clearFHandle() { fHandle = NULL; }
	TrafficControl::FHandle* getFHandle() const { return fHandle; }
	void setOutLabelRequested() { outLabelRequested = true; }
	void setOutLabelRequestedType(uint8 type) { outLabelRequestedType = type;}
	bool hasOutLabelRequested() const { return outLabelRequested; }
	uint8 getRequestedOutLabelType() const { return outLabelRequestedType; }
	void setOutLabel( const MPLS_OutLabel* l ) { outLabel = l; }
	const MPLS_OutLabel* getOutLabel() { return outLabel; }

	DECLARE_MEMORY_MACHINE_IN_CLASS(OIatPSB)
};
DECLARE_MEMORY_MACHINE_OUT_CLASS(OIatPSB, oiatpsbMemMachine)

inline OIatPSB_Array::ConstIterator OIatPSB_Array::insert_unique( OIatPSB* psbOI ) {
	return InterfaceArray<OIatPSB>::insert_unique( psbOI, psbOI->getLIH() );
}
inline OIatPSB_Array::ConstIterator OIatPSB_Array::erase_key( OIatPSB* psbOI ) {
	return InterfaceArray<OIatPSB>::erase_key( psbOI->getLIH() );
}
inline bool OIatPSB_Array::contains( OIatPSB* psbOI ) {
	return InterfaceArray<OIatPSB>::contains( psbOI->getLIH() );
}

#endif /* _RSVP_OIatPSB_h_ */
