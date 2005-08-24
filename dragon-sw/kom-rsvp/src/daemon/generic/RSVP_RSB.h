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
#ifndef _RSVP_RSB_h_
#define _RSVP_RSB_h_ 1

#include "RSVP_FilterSpecList.h"
#if defined(REFRESH_REDUCTION) || defined(WITH_API)
#include "RSVP_Hop.h"
#endif
#include "RSVP_Lists.h"
#include "RSVP_PSB.h"
#include "RSVP_Relationships.h"
#include "RSVP_GeneralMemoryMachine.h"
#include "RSVP_Timer.h"

class FLOWSPEC_Object;
class SCOPE_Object;
class OutISB;

typedef RelationshipToMANY<PSB,PSB_List>	Relationship_ToPSB;

class RSB_Contents : public Relationship_ToPSB {
protected:
	const FLOWSPEC_Object* flowspec;
	const SCOPE_Object*    scope;
public:
	inline RSB_Contents();
	~RSB_Contents();
	RSB_Contents* createBackup() const;
	bool updateContents( const FLOWSPEC_Object&, const SCOPE_Object* );
	const PSB_List& getPSB_List() const { return followRelationship(); }
	const FLOWSPEC_Object& getFLOWSPEC_Object() const {
		assert(flowspec); return *flowspec;
	}
	const SCOPE_Object* getSCOPE_Object() const { return scope; }
};

typedef RelationshipMANYto1<RSB,RSB_List,OutISB,DoOnEmptyDelete1> RelationshipRSB_OutISB;

class RSB_Key {
protected:
	Hop& nhop;
	DECLARE_ORDER(RSB_Key)
public:
	RSB_Key( Hop& nhop ) : nhop(nhop) {}
};
IMPLEMENT_ORDER1(RSB_Key,nhop)

inline bool Less<RSB_Key*>::operator()( const RSB_Key* r1, const RSB_Key* r2 ) const {
	return *r1 < *r2;
}

class RSB : public RelationshipRSB_OutISB, public RSB_Contents, public RSB_Key {
	TimeoutTimer<RSB> lifetimeTimer;
#if defined(REFRESH_REDUCTION)
	Hop::RecvStorageID* recvID;
#endif

	RSB( const RSB& );
	RSB& operator=( const RSB& );
	friend ostream& operator<< ( ostream&, const RSB& );
#if defined(WITH_API)
	uint16 apiPort;
#endif
public:
	RSB( Hop& );
	~RSB();
	const Hop& getNextHop() const { return nhop; }
	Hop& getNextHop() { return nhop; }
	Session& getSession() const {
                                             assert( !getPSB_List().empty() );
		return getPSB_List().front()->getSession();
	}

	void addPSB( PSB& psb ) { Relationship_ToPSB::setRelationshipHalf( &psb ); }
	// caller has to check whether RSB is to be deleted
	void removePSB( PSB& psb ) { Relationship_ToPSB::clearRelationshipHalf( &psb ); }
	void replaceWithBackup( const RSB_Contents&, uint32 );
	bool setFiltersAndTimeout( const PSB_List&, uint32, bool, PSB_List* = NULL, FilterSpecList* = NULL );
	void restartTimeout();
	bool teardownFilters( const FilterSpecList& );

	inline void timeout();
#if defined(ONEPASS_RESERVATION)
	bool isOnepass() { return getNextHop().getAddress() == NetAddress(0); }
#endif
#if defined(REFRESH_REDUCTION)
	Hop::RecvStorageID* getRecvID() { return recvID; }
	void setRecvID( sint32 id ) {
		if ( recvID ) {
			if ( recvID->id == id ) return;
			nhop.clearRecvRSB( recvID->id, this );
		}
		recvID = nhop.storeRecvRSB( this, id );
	}
#endif
#if defined(WITH_API)
	void setAPI_Port( uint16 ap ) { apiPort = ap; }
	uint16 getAPI_Port() const { return apiPort; }
	const NetAddress& getAPI_Address() const { return getNextHop().getAddress(); }
#endif

	DECLARE_MEMORY_MACHINE_IN_CLASS(RSB)
};
DECLARE_MEMORY_MACHINE_OUT_CLASS(RSB,rsbMemMachine)

#endif /* _RSVP_RSB_h_ */
