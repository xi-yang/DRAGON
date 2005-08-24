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
#ifndef _RSVP_List_h_
#define _RSVP_List_h_ 1

#include "RSVP_GeneralMemoryMachine.h"
#include "RSVP_Helper.h"

#ifdef DEBUG_TEMPLATES
#include <iostream>
#endif

#if defined(RSVP_MEMORY_MACHINE)
struct ListMemNode {
	void* prev;
	void* next;
	void* data;
	DECLARE_MEMORY_MACHINE_IN_CLASS(ListMemNode)
};
DECLARE_MEMORY_MACHINE_OUT_CLASS(ListMemNode,listMemMachine)
#endif

#if defined(LIST_MIMICKS_SORTED)
template <class Key>
struct Equal {
	bool operator()( const Key& k1, const Key& k2 ) const { return k1 == k2; }
};
#endif

#if defined(LIST_MIMICKS_SORTED)
template <class Value, class Key = Value, class CompareEqual = Equal<Key> >
#else
template <class Value>
#endif
class SimpleList {

#if defined(LIST_MIMICKS_SORTED)
	struct BoundEqual : public CompareEqual {
		const Key& b;
		BoundEqual( const Key& b ) : b(b) {}
		bool operator()( const Key& k ) const {
			return (*(CompareEqual*)(this))( k, b );
		}
	};
	ListNode* find_node( const Key& elem ) const {
		ListNode* iter = head();
		BoundEqual c(elem);
		while ( iter != tail() && !e( KEY_CAST iter->data ) )
			iter = iter->next;
		return iter;
	}
#endif

protected:
	struct ListNode;
	typedef ListNode* ListNodeP;

	struct ListNode {
		Value data;
		ListNode* next;
		ListNode* prev;
		ListNode() : next(this), prev(this) {}
		ListNode( const Value& data, const ListNodeP& next, const ListNodeP& prev )
		: data(data), next(next), prev(prev) {}
#if defined(RSVP_MEMORY_MACHINE)
		void* operator new( size_t s ) {
			if ( sizeof(Value) <= sizeof(void*) ) {
				return listMemMachine.alloc( s );
			} else {
				return ::operator new( s );
			}
		}
		void operator delete(void *pnt) {
			if ( sizeof(Value) <= sizeof(void*) ) {
				listMemMachine.dealloc( pnt );
			} else {
				::operator delete( pnt );
			}
		}
#endif
	};

public:
	class ConstIterator {
	protected:
		ListNode* node;
#ifdef DEBUG_TEMPLATES
#if ((__GNUC__ >= 2 && __GNUC_MINOR__ >= 8) || __GNUC__ >= 3)
		friend inline ostream& operator<< <Value> ( ostream&, const ConstIterator& );
#else
		friend inline ostream& operator<< ( ostream&, const ConstIterator& );
#endif
#endif
		friend class SimpleList;
	public:
		ConstIterator() : node(0) {}
		ConstIterator( ListNode* node ) : node(node) {}
		ConstIterator& operator++() { node = node->next; return *this; }
		ConstIterator& operator--() { node = node->prev; return *this; }
		ConstIterator prev() { return node->prev; }
		ConstIterator next() { return node->next; }
		const Value& operator*() const { return node->data; }
		bool operator== ( const ConstIterator& i ) const { return node == i.node; }
		bool operator!= ( const ConstIterator& i ) const { return node != i.node; }
		operator bool() const { return node != (ListNode*)0; }
		void reset() { node = (ListNode*)0; }
	};

	class Iterator : public ConstIterator {
	public:
		Iterator() {}
		Iterator( ListNode* node ) : ConstIterator(node) {}
		Iterator& operator++() { node = node->next; return *this; }
		Iterator& operator--() { node = node->prev; return *this; }
		Value& operator*() const { return node->data; }
	};

	typedef void (*CallForAllVoid)( Value );
protected:
	ListNode* endOfList;
	unsigned int length;

#ifdef DEBUG_TEMPLATES
#if (__GNUC__ >= 2) && (__GNUC_MINOR__ >= 8)
	friend inline ostream& operator<< <Value> ( ostream&, const SimpleList& );
#else
	friend inline ostream& operator<< ( ostream&, const SimpleList& );
#endif
#endif

	// insert new element before node, returns new node, node doesn't change
	ListNode* insert_elem( ListNode* node, const Value& elem ) {
		ListNode* tmp = new ListNode( elem, node, node->prev );
		node->prev->next = tmp;
		node->prev = tmp;
		length += 1;
		return tmp;
	}

	// erase current node, returns next node
	ListNode* erase_node( ListNode* node ) {
		if (node == tail()) return node;
		ListNode* oldPrev = node->prev;
		ListNode* oldNext = node->next;
		oldPrev->next = oldNext;
		oldNext->prev = oldPrev;
		delete node;
		length -= 1;
		return oldNext;
	}

	// insert a range of elements before node, node doesn't change
	void insert_range( ListNode* node, ListNode* first, ListNode* last ) {
		while ( first != last ) {
			insert_elem( node, first->data );
			first = first->next;
		}
	}

	// erases a range of nodes, returns next node
	ListNode* erase_range( ListNode* first, ListNode* last ) {
		while ( first != last ) {
			first = erase_node( first );
		}
		return first;
	}	

	ListNode* head() const { return endOfList->next; }
	ListNode* tail() const { return endOfList; }

public:

	SimpleList() : endOfList(new ListNode), length(0) {}
	SimpleList( const SimpleList& l ) : endOfList(new ListNode), length(0) {
		insert_range( tail(), l.head(), l.tail() );
	}
	SimpleList( const Value& elem ) : endOfList(new ListNode), length(0) {
		insert_elem( tail(), elem );
	}
	~SimpleList() {
		clear();
		delete endOfList;
	}
	
	SimpleList& operator=( const SimpleList& l ) {
		if ( this != &l ) {
			clear();
			insert_range( tail(), l.head(), l.tail() );
		}
		return *this;
	}

	bool operator==( const SimpleList& l ) const {
		ListNode* iter1 = head(); ListNode* iter2 = l.head();
		while ( iter1 != tail() && iter2 != l.tail() ) {
			if ( iter1->data != iter2->data )
		return false;
			iter1 = iter1->next; iter2 = iter2->next;
		}
		return true;
	}
	bool operator!=( const SimpleList& l ) const { return !operator==(l); }
	
	void clear() {
		erase_range( head(), tail() );
	}
	Iterator push_front( const Value& elem ) {
		return insert_elem( head(), elem );
	}
	void pop_front() {
		erase_node( head() );
	}
	Iterator push_back( const Value& elem ) {
		return insert_elem( tail(), elem );
	}
	void pop_back() {
		erase_node( tail()->prev );
	}

	Value& front() { return head()->data; }
	Value& back() { return tail()->prev->data; }
	const Value& front() const { return head()->data; }
	const Value& back() const { return tail()->prev->data; }

	Iterator begin() { return Iterator(head()); }
	Iterator end() { return Iterator(tail()); }
	ConstIterator begin() const { return ConstIterator(head()); }
	ConstIterator end() const { return ConstIterator(tail()); }

	Iterator insert( ConstIterator pos, const Value& elem ) {
		return insert_elem( pos.node, elem );
	}
	Iterator erase( ConstIterator pos ) {
		return erase_node( pos.node );
	}
	void insert( ConstIterator pos, ConstIterator first, ConstIterator last ) {
		insert_range( pos.node, first.node, last.node );
	}
	Iterator erase( ConstIterator first, ConstIterator last ) {
		return erase_range( first.node, last.node );
	}
	unsigned int size() const { return length; }
	bool empty() const { return length == 0; }

	void callForAll( CallForAllVoid call ) {
		ListNode* iter = head();
		for ( ; iter != tail(); iter = iter->next ) {
			call( iter->data );
		}
	}

#if defined(LIST_MIMICKS_SORTED)
	Iterator lower_bound( const Key& elem ) const {
		return find_node( elem );
	}

	Iterator find( const Key& elem ) const {
		return find_node( elem );
	}

	bool contains( const Key& key ) const {
		return find_node(key) != tail();
	}

	Iterator insert_sorted( const Value& elem ) {
		return insert_elem( tail(), elem );
	}

	Iterator erase_key( const Key& elem ) {
		ListNode* iter = find_node( key );
		if ( iter != tail() )
			return erase_node( iter );
		else
			return tail();
	}
#endif
};

#ifdef DEBUG_TEMPLATES
template <class Value>
inline ostream& operator<< ( ostream& os, const SimpleList<Value>::ListNode& l ) {
	os << "node: " << &l << " data: " << l.data;
	os << " next: " << l.next << " prev: " << l.prev;
	return os;
}

template <class Value>
inline ostream& operator<< ( ostream& os, const SimpleList<Value>::Iterator& i ) {
	os << "Iterator node: " << node << " : " << *node << endl;
	return os;
}

template <class Value>
inline ostream& operator<< ( ostream& os, const SimpleList<Value>::ConstIterator& i ) {
	os << "Iterator node: " << node << " : " << *node << endl;
	return os;
}

template <class Value>
inline ostream& operator<< ( ostream& os, const SimpleList<Value>& l ) {
	SimpleList<Value>::ListNode* tmp = l.head();
	for (;;) {
		os << *tmp;
		if ( tmp->next == l.tail() )
	break;
		os << endl;
		tmp = tmp->next;
	}
	return os;
}
#endif

#endif /* _RSVP_List_h_ */
