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
#ifndef _RSVP_PHopSB_h_
#define _RSVP_PHopSB_h_ 1

#include "RSVP_BasicTypes.h"
#include "RSVP_FilterSpecList.h"
#include "RSVP_Hop.h"
#include "RSVP_Lists.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_ProtocolObjects.h"
#include "RSVP_Relationships.h"
#include "RSVP_Timer.h"
#include "RSVP_GeneralMemoryMachine.h"

class FLOWSPEC_Object;
class Message;
class Session;
template <class T> class BlockSB;

class PHopSBKey {
protected:
	Hop* phop;
	uint32 LIH;
	DECLARE_ORDER(PHopSBKey)
public:
	PHopSBKey( Hop& phop, uint32 LIH ) : phop(&phop), LIH(LIH) {
		this->phop->addSB();
	}
	~PHopSBKey() { phop->removeSB(); }
	const Hop& getHop() const { return *phop; }
	Hop& getHop() { return *phop; }
	uint32 getLIH() const { return LIH; }
	bool checkPHOP_Data( const Hop& phop, uint32 LIH ) {
		return this->phop == &phop && this->LIH == LIH;
	}
	uint32 getHashValue( uint32 hashCount ) const {
		return phop->getAddress().getHashValue( hashCount );
	}
};
IMPLEMENT_ORDER2(PHopSBKey,phop,LIH)

inline bool Less<PHopSBKey*>::operator()( const PHopSBKey* p1, const PHopSBKey* p2 ) const {
	return *p1 < *p2;
}

typedef RelationshipMANYto1<PHopSB,PHOP_List,Session> RelationshipPHopSB_Session;
typedef Relationship1toMANY<PHopSB,PSB,PSB_List,DoOnEmptyDelete1> RelationshipPHopSB_PSB;

class PHopSB :	public RelationshipPHopSB_Session,
								public RelationshipPHopSB_PSB,
								public PHopSBKey {

	ONetworkBuffer* refreshBuffer;
	bool fullRefreshNeeded;
	RandomRefreshTimer<PHopSB> refreshTimer;

	BlockSB<PHopSB>* blockadeSB;
	FLOWSPEC_Object* sharedForwardFlowspec;

#if defined(REFRESH_REDUCTION)
	Hop::SendStorageID* sendID;
#endif
#if defined(ONEPASS_RESERVATION)
	bool onepassAll;
#endif
	// return value indicates whether the current flowspec is below blockaded
	bool calculateForwardFlowspec( bool );
	friend class PHopSB_Refresh;
public:
	PHopSB( Hop& hop, uint32 LIH );
	~PHopSB();

	void startResvRefreshWithMessage( Message& m, bool );
	void stopResvRefresh();
	void refresh();
	inline void doRefresh();

	Session& getSession() { return *RelationshipPHopSB_Session::followRelationship(); }
	void matchPSBsAndFilters( const FilterSpecList&, PSB_List& result );
	const PSB_List& getPSB_List() const { return RelationshipPHopSB_PSB::followRelationship(); }

	void setBlockade( const FLOWSPEC_Object&, uint32 timeout );
	const FLOWSPEC_Object* getBlockadeFlowspec() const;
	inline void clearBlockade();

	const FLOWSPEC_Object& getForwardFlowspec() const {
		return *sharedForwardFlowspec;
	}
	const NetAddress& getAddress() const {
		return getHop().getAddress();
	}
#if defined(REFRESH_REDUCTION)
	Hop::SendStorageID* getSendID() { return sendID; }
#endif
#if defined(ONEPASS_RESERVATION)
	bool isOnepassAll() { return onepassAll; }
#endif

	DECLARE_MEMORY_MACHINE_IN_CLASS(PHopSB)
};
DECLARE_MEMORY_MACHINE_OUT_CLASS(PHopSB, phopMemMachine)

extern inline ostream& operator<< ( ostream& os, const PHopSB& p ) {
	os << "PHopSB:" << p.getHop().getAddress() << "[" << p.getLIH() << "] via " << p.getHop().getLogicalInterface().getName();
	return os;
}

class PHopSB_Refresh {
	PHopSB* phop;
	PSB_List psbRefreshList;
	PSB_List psbRemoveList;
public:
	PHopSB_Refresh() : phop(NULL){} 
	PHopSB_Refresh( PHopSB& phop ) : phop(&phop) {}
	PHopSB& getPHopSB() { assert(phop); return *phop; }
	const PHopSB& getPHopSB() const { assert(phop); return *phop; }
	void markForResvRefresh( PSB& );
	void markForResvRemove( PSB& );
	const PSB_List& getRefreshPSB_List() const { return psbRefreshList; }
	const PSB_List& getRemovePSB_List() const { return psbRemoveList; }  
	// return value indicates whether the current flowspec is below blockaded
	bool calculateForwardFlowspec( bool B_Merge ) {
		if ( !psbRefreshList.empty() || !psbRemoveList.empty() ) {
			return phop->calculateForwardFlowspec( B_Merge );
		}
		return false;
	}
	operator RSVP_HOP_Object() const { return RSVP_HOP_Object( phop->getHop().getAddress(), phop->getLIH() ); }
};	

#endif /* _RSVP_PHopSB_h_ */
