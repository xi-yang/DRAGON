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

/*
 * Copyright (c) 1994
 * Hewlett-Packard Company
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Hewlett-Packard Company makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 *
 * Copyright (c) 1996
 * Silicon Graphics Computer Systems, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */

#ifndef _RSVP_Set_h_
#define _RSVP_Set_h_ 1

#include "RSVP_SortedList.h"

template <class Value, class Key, class Compare = Less<Key> >
class Set : public SortedList<Value, Key, Compare> {
	typedef typename SortedList<Value, Key, Compare>::ListNode ListNode;
public:
	typedef typename SortedList<Value, Key, Compare>::Iterator Iterator;
	typedef typename SortedList<Value, Key, Compare>::ConstIterator ConstIterator;
private:
	// make these methods (more or less;-) inaccessible
	Iterator find_or_insert_sorted( const Value& elem );
	Iterator insert_sorted( const Value& elem );
public:
	Set() {}
	Set( const Value& elem ) : SortedList<Value, Key, Compare>( elem ) {}

	Set& operator=( const Set& l ) {
		SortedList<Value, Key, Compare>::operator=( l ); return *this;
	}

	Set create_union_with( const Set& s ) const {
		Set result = s;
		result.union_with( *this );
		return result;
	}

	void intersection_with( const Set& s ) {
		ListNode* iter1 = head(); ListNode* iter2 = s.head();
		while ( iter1 != tail() && iter2 != s.tail() ) {
			if ( comp( KEY_CAST iter1->data, KEY_CAST iter2->data ) ) {
				iter1 = erase_node( iter1 );
			} else if ( comp( KEY_CAST iter2->data, KEY_CAST iter1->data ) ) {
				iter2 = iter2->next;
			} else {
				iter1 = iter1->next; iter2 = iter2->next;
			}
		}
		erase_range ( iter1, tail() );
	}

	Set create_intersection_with( const Set& s ) const {
		Set result = s;
		result.intersection_with( *this );
		return result;
	}

	Set create_difference_from( const Set& s ) const {
		Set result;
		ListNode* iter1 = head(); ListNode* iter2 = s.head();
		while ( iter1 != tail() && iter2 != s.tail() ) {
			if ( comp( KEY_CAST iter1->data, KEY_CAST iter2->data ) ) {
				result.insert_elem( result.tail(), iter1->data );
				iter1 = iter1->next;
			} else if ( comp( KEY_CAST iter2->data, KEY_CAST iter1->data ) ) {
				iter2 = iter2->next;
			} else {
				iter1 = iter1->next; iter2 = iter2->next;
			}
		}
		result.insert_range( result.tail(), iter1, tail() );
		return result;
	}

	void replaceElements( const Set& s, Set& removed, Set& added ) {
		ListNode* iter1 = head(); ListNode* iter2 = s.head();
		while ( iter1 != tail() && iter2 != s.tail() ) {
			if ( comp( KEY_CAST iter1->data, KEY_CAST iter2->data ) ) {
				removed.insert_elem( removed.tail(), iter1->data );
				iter1 = erase_node( iter1 );
			} else if ( comp( KEY_CAST iter2->data, KEY_CAST iter1->data ) ) {
				added.insert_elem( added.tail(), iter2->data );
				insert_elem( iter1, iter2->data );
				iter2 = iter2->next;
			} else {
				iter1 = iter1->next; iter2 = iter2->next;
			}
		}
		removed.insert_range( removed.tail(), iter1, tail() );
		erase_range( iter1, tail() );
		insert_range( tail(), iter2, s.tail() );
		added.insert_range( added.tail(), iter2, s.tail() );
	}

#if !((__GNUC__ >= 2 && __GNUC_MINOR__ >= 8) || __GNUC__ >= 3)
// unnecessary, but without it, gcc 2.7.2 has given me 'internal compiler error'
	unsigned int size() const { return length; }
	bool empty() const { return length == 0; }
#endif

};

#endif /* _RSVP_Set_h_ */
