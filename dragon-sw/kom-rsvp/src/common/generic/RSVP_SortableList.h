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
#ifndef _RSVP_SortableList_h_
#define _RSVP_SortableList_h_ 1

#include "RSVP_List.h"

#if ((__GNUC__ >= 2 && __GNUC_MINOR__ >= 8) || __GNUC__ >= 3)
#define KEY_CAST
#else
#define KEY_CAST	(Key&)
#endif

template <class Key>
struct Less {
	bool operator()( const Key& k1, const Key& k2 ) const { return k1 < k2; }
};

template <class Value, class Key = Value, class Compare = Less<Key> >
class SortableList : public SimpleList<Value> {

public:
	typedef typename SimpleList<Value>::ListNode ListNode;
	typedef typename SimpleList<Value>::Iterator Iterator;
	typedef typename SimpleList<Value>::ConstIterator ConstIterator;

private:
	struct BoundCompare : public Compare {
		const Key& b;
		BoundCompare( const Key& b ) : b(b) {}
		bool operator()( const Key& k ) const {
			return (*(Compare*)(this))( k, b );
		}
	};

protected:
	Compare comp;

	ListNode* lower_bound_node( const Key& elem ) const {
		ListNode* iter = head();
		BoundCompare c(elem);
		while ( iter != tail() && c( KEY_CAST iter->data ) )
			iter = iter->next;
		return iter;
	}

public:
	SortableList() {}
	SortableList( const Value& elem ) : SimpleList<Value>( elem ) {}

	SortableList& operator=( const SortableList& l ) {
		SimpleList<Value>::operator=( l ); return *this;
	}

	Iterator lower_bound( const Key& elem ) const {
		return lower_bound_node( elem );
	}

	Iterator find( const Key& elem ) const {
		ListNode* iter = lower_bound_node( elem );
		if ( iter != tail() && !comp( elem, KEY_CAST iter->data ) )
			return iter;
		else
			return tail();
	}

	Iterator find_or_insert_sorted( const Value& elem ) {
		ListNode* pos = lower_bound_node( KEY_CAST elem );
		if ( pos == tail() || comp( KEY_CAST elem, KEY_CAST pos->data ) )
			return insert_elem( pos, elem );
		else
			return pos;
	}

	bool contains( const Key& key ) const {
		return find(key) != end();
	}

	Iterator insert_sorted( const Value& elem ) {
		return insert_elem( lower_bound_node( KEY_CAST elem ), elem );
	}

	Iterator insert_unique( const Value& elem ) {
		Iterator iter;
		ListNode* pos = lower_bound_node( KEY_CAST elem );
		if ( pos == tail() || comp( KEY_CAST elem, KEY_CAST pos->data ) )
			iter = insert_elem( pos, elem );
		else
			iter = pos;
		return iter;
	}

	Iterator erase_key( const Key& elem ) {
		ListNode* iter = lower_bound_node( elem );
		if ( iter != tail() && !comp( elem, KEY_CAST iter->data ) )
			return erase_node( iter );
		else
			return tail();
	}

	void union_with( const SortableList& s ) {
		ListNode* iter1 = head(); ListNode* iter2 = s.head();
		while ( iter1 != tail() && iter2 != s.tail() ) {
			if ( comp( KEY_CAST iter1->data, KEY_CAST iter2->data ) ) {
				iter1 = iter1->next;
			} else if ( comp( KEY_CAST iter2->data, KEY_CAST iter1->data ) ) {
				insert_elem( iter1, iter2->data );
				iter2 = iter2->next;
			} else {
				iter1 = iter1->next; iter2 = iter2->next;
			}
		}
		insert_range( iter1, iter2, s.tail() );
	}

};

#endif /* _RSVP_SortableList_h_ */
