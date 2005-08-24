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
#ifndef _RSVP_SortedList_h_
#define _RSVP_SortedList_h_ 1

#include "RSVP_SortableList.h"

template <class Value, class Key = Value, class Compare = Less<Key> >
class SortedList : public SortableList<Value,Key,Compare> {
	// remove R/W iterator
public:
	typedef typename SortableList<Value,Key,Compare>::ConstIterator ConstIterator;
	typedef typename SortableList<Value,Key,Compare>::ConstIterator Iterator;
private:
	// make these methods (more or less;-) inaccessible
	Iterator push_front( const Value& );
	Iterator push_back( const Value& );
	Iterator insert( Iterator, const Value& );
	Iterator insert( ConstIterator, ConstIterator, ConstIterator );
public:
	SortedList() {}
	SortedList( const Value& elem ) : SortableList<Value,Key,Compare>( elem ) {}
	SortedList& operator=( const SortedList& l ) {
		SortableList<Value,Key,Compare>::operator=( l ); return *this;
	}
	// the following declarations actually make the non-const ones invisible
	const Value& front() const {
		return SortableList<Value,Key,Compare>::front();
	}
	const Value& back() const {
		return SortableList<Value,Key,Compare>::back();
	}
};

#endif /* _RSVP_SortedList_h_ */
