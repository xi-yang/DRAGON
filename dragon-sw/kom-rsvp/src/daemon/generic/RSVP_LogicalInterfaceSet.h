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
#ifndef _RSVP_LogicalInterfaceSet_h_
#define _RSVP_LogicalInterfaceSet_h_ 1

#include "RSVP.h"
#include "RSVP_LogicalInterface.h"

class LogicalInterfaceSet {

public:
	class ConstIterator {
		const LogicalInterfaceSet* set;
		sint32 number;
	public:
		ConstIterator( const LogicalInterfaceSet& set, sint32 number = -1 )
			: set(&set), number(number) {}
		ConstIterator& operator++() {
			do ++number; while (number < (sint32)RSVP_Global::rsvp->getInterfaceCount() && !set->isSet(number));
			if (number == (sint32)RSVP_Global::rsvp->getInterfaceCount()) number = -1;
			return *this;
		}
		ConstIterator& operator--() {
			if (number == -1) number = (sint32)RSVP_Global::rsvp->getInterfaceCount();
			do --number; while (number >= 0 && !set->isSet(number));
			return *this;
		}
		bool operator==( const ConstIterator& i ) { assert(set == i.set); return number == i.number; }
		bool operator!=( const ConstIterator& i ) { assert(set == i.set); return number != i.number; }
		const LogicalInterface* operator*() const {
			return RSVP_Global::rsvp->findInterfaceByLIH(number);
		}
	};

private:
	uint32 mask[RSVP_Global::maxNumberOfInterfaces/32];
	uint32 elemCounter;
	friend class ConstIterator;

	void clearMask() {
		static uint32 count = RSVP_Global::rsvp->getInterfaceCount()/32;
		uint32 i = 0;
		for ( ; i <= count; ++i ) {
			mask[i] = 0;
		}
	}
	void copyMask( const uint32* const m ) {
		static uint32 count = RSVP_Global::rsvp->getInterfaceCount()/32;
		uint32 i = 0;
		for ( ; i <= count; ++i ) {
			mask[i] = m[i];
		}
	}

	bool isSet( uint32 number ) const {
		assert( number >= 0 && number < RSVP_Global::rsvp->getInterfaceCount() );
		return ( mask[number/32] & (1 << (number % 32)) );
	}
	sint32 set( uint32 number ) {
		assert( number >= 0 && number < RSVP_Global::rsvp->getInterfaceCount() );
		if ( !isSet(number) ) {
			elemCounter += 1;
			mask[number/32] |= (1 << (number % 32));
			return number;
		} else {
			return -1;
		}
	}
	void clear( uint32 number ) {
		assert( number >= 0 && number < RSVP_Global::rsvp->getInterfaceCount() );
		if ( isSet(number) ) {
			elemCounter -= 1;
			mask[number/32] &= ~(1 << (number % 32));
		}
	}

public:
	LogicalInterfaceSet() : elemCounter(0) {
		clearMask();
	}
	LogicalInterfaceSet( const LogicalInterface* lif ) : elemCounter(0) {
		clearMask();
		set( lif->getLIH() );
	}
	LogicalInterfaceSet( const LogicalInterfaceSet& ls ) : elemCounter(ls.elemCounter) {
		copyMask( ls.mask );
	}
	LogicalInterfaceSet& operator=( const LogicalInterfaceSet& ls ) {
		elemCounter = ls.elemCounter;
		copyMask( ls.mask );
		return *this;
	}
	~LogicalInterfaceSet() {}
	ConstIterator insert_unique( const LogicalInterface* lif ) {
		return ConstIterator( *this, set( lif->getLIH() ) );
	}
	ConstIterator erase_key( const LogicalInterface* lif ) {
		ConstIterator retval( *this, lif->getLIH() );
		++retval;
		clear( lif->getLIH() );
		return retval;
	}
	ConstIterator erase( const ConstIterator& iter ) {
		return erase_key( *iter );
	}
	ConstIterator begin() const {
		ConstIterator retval( *this );
		++retval;
		return retval;
	}
	ConstIterator end() const {
		return ConstIterator( *this );
	}
	void replaceElements( const LogicalInterfaceSet& s, LogicalInterfaceSet& removed, LogicalInterfaceSet& added ) {
		uint32 i = 0;
		for ( ; i < RSVP_Global::rsvp->getInterfaceCount(); ++i ) {
			if ( s.isSet(i) && !isSet(i) ) {
				added.set(i);
				set(i);
			} else if ( !s.isSet(i) && isSet(i) ) {
				removed.set(i);
				clear(i);
			}
		}
	}
	bool empty() const { return elemCounter == 0; }
	void clear() {
		clearMask();
		elemCounter = 0;
	}
	bool contains( const LogicalInterface* lif ) const {
		return isSet( lif->getLIH() );
	}
	void union_with( const LogicalInterfaceSet& s ) {
		uint32 i;
		for ( i = 0; i < ((RSVP_Global::rsvp->getInterfaceCount()/32)+1); ++i ) {
			mask[i] |= s.mask[i];
		}
	}
	void intersection_with( const LogicalInterfaceSet& s ) {
		uint32 i;
		for ( i = 0; i < ((RSVP_Global::rsvp->getInterfaceCount()/32)+1); ++i ) {
			mask[i] &= s.mask[i];
		}
	}
	const LogicalInterface* front() const {
		if (!elemCounter) return NULL;
		ConstIterator iter(*this);
		return *(++iter);
	}
	const LogicalInterface* back() const {
		if (!elemCounter) return NULL;
		ConstIterator iter(*this);
		return *(--iter);
	}
	uint32 size() { return elemCounter; }
};

#endif /* _RSVP_LogicalInterfaceSet_h_ */
