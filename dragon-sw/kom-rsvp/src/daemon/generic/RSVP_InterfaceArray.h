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
#ifndef _RSVP_InterfaceArray_h_
#define _RSVP_InterfaceArray_h_ 1

#include "RSVP.h"
#include "RSVP_BasicTypes.h"
#include "RSVP_Global.h"

template <class T>
class InterfaceArray {
public:
        friend class ConstIterator;
        class ConstIterator {
	protected:
		friend class InterfaceArray;
		const InterfaceArray* set;
		int number;
	public:
		ConstIterator() : set(NULL), number(-1) {}
		ConstIterator( const InterfaceArray& set, int number = -1 )
			: set(&set), number(number) {}
		ConstIterator& operator++() {
			do ++number; while (number < (int)RSVP_Global::rsvp->getInterfaceCount() && !set->isSet(number));
			if (number == (int)RSVP_Global::rsvp->getInterfaceCount()) number = -1;
			return *this;
		}
		ConstIterator& operator--() {
			if (number == -1) number = (int)RSVP_Global::rsvp->getInterfaceCount();
			do --number; while (number >= 0 && !set->isSet(number));
			return *this;
		}
		bool operator==( const ConstIterator& i ) { assert(set == i.set); return number == i.number; }
		bool operator!=( const ConstIterator& i ) { assert(set == i.set); return number != i.number; }
		const T* operator*() const {
			assert(set&&set->field[number]); return set->field[number];
		}
	};

private:
	T** field;
	uint32 elemCounter;

	InterfaceArray( const InterfaceArray& i ) : field(NULL), elemCounter(i.elemCounter) {
		field = new T*[RSVP_Global::rsvp->getInterfaceCount()];
		copyMemory( field, i.field, sizeof(T*) * RSVP_Global::rsvp->getInterfaceCount() );
	}
	InterfaceArray& operator=( const InterfaceArray& i ) {
		field = new T*[RSVP_Global::rsvp->getInterfaceCount()];
		copyMemory( field, i.field, sizeof(T*) * RSVP_Global::rsvp->getInterfaceCount() );
		elemCounter = i.elemCounter;
		return *this;
	}

public:
	typedef void (*CallForAllVoid)( T* );
	InterfaceArray() : elemCounter(0) {
		field = new T*[RSVP_Global::rsvp->getInterfaceCount()];
		initMemoryWithZero( field, sizeof(T*) * RSVP_Global::rsvp->getInterfaceCount() );
	}
	~InterfaceArray() { delete [] field; }
	T*& operator[](int i) {
		assert( i >= 0 && i < (int)RSVP_Global::rsvp->getInterfaceCount() );
		return field[i];
	}
	T* const & operator[](int i) const {
		assert( i >= 0 && i < (int)RSVP_Global::rsvp->getInterfaceCount() );
		return field[i];
	}
	bool isSet( uint32 number ) const {
		assert( number >= 0 && number < RSVP_Global::rsvp->getInterfaceCount() );
		return ( field[number] != NULL );
	}
	ConstIterator InterfaceArray::insert_unique( T* elem, uint32 number ) {
		assert( number >= 0 && number < RSVP_Global::rsvp->getInterfaceCount() );
		if ( !isSet(number) ) {
			elemCounter += 1;
			field[number] = elem;
			return ConstIterator( *this, number );
		} else {
			return end();
		}
	}
	ConstIterator erase_key( uint32 number ) {
		assert( number >= 0 && number < RSVP_Global::rsvp->getInterfaceCount() );
		if ( isSet(number) ) {
			ConstIterator retval( *this, number );
			++retval;
			elemCounter -= 1;
			field[number] = NULL;
			return retval;
		} else {
			return end();
		}
	}
	bool contains( uint32 number ) const {
		return isSet( number );
	}
	ConstIterator erase( const ConstIterator& iter ) {
		return erase_key( iter.number );
	}
	ConstIterator begin() const {
		ConstIterator retval( *this );
		++retval;
		return retval;
	}
	ConstIterator end() const {
		return ConstIterator( *this );
	}
	bool empty() const { return elemCounter == 0; }
	T* front() {
		if (!elemCounter) return NULL;
		ConstIterator iter(*this);
		return *(++iter);
	}
	T* back() {
		if (!elemCounter) return NULL;
		ConstIterator iter(*this);
		return *(--iter);
	}
	uint32 size() const { return elemCounter; }

	void callForAll( CallForAllVoid call ) {
		uint32 x = 0;
		for ( ; x < RSVP_Global::rsvp->getInterfaceCount(); x += 1 ) {
			if ( field[x] != NULL ) call( field[x] );
		}
	}

};

class OIatPSB_Array : public InterfaceArray<OIatPSB> {
public:
	typedef InterfaceArray<OIatPSB>::ConstIterator ConstIterator;
	inline ConstIterator insert_unique( OIatPSB* );
	inline ConstIterator erase_key( OIatPSB* );
	inline bool contains( OIatPSB* );
};

#endif /* _RSVP_InterfaceArray_h_ */
