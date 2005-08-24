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
#ifndef _RSVP_SortableHash_h_
#define _RSVP_SortableHash_h_ 1

#include "RSVP_SortableList.h"

template <class Key>
struct GetHash {
	unsigned int operator()( const Key& k, unsigned int hashCount ) const { return k.getHashValue(hashCount); }
};

template <class Value, class Key = Value, class Compare = Less<Key>, class HashValue = GetHash<Key>, unsigned int defaultSize = 1 >
class SortableHash {

public:
	typedef SortableList<Value,Key,Compare> HashBucket;

private:
	SortableHash& operator=( const SortableHash& );
	SortableHash( const SortableHash& );

protected:
	HashValue getHashValue;

	HashBucket* hash;
	unsigned int hashCount;
	unsigned int elemCount;

public:
	class ConstIterator : public HashBucket::ConstIterator {
		ConstIterator& operator++();
		ConstIterator& operator--();
		ConstIterator prev();
		ConstIterator next();
	public:
		ConstIterator() {}
		ConstIterator( const typename HashBucket::ConstIterator& iter ) : HashBucket::ConstIterator(iter) {}
	};
	class Iterator : public HashBucket::Iterator {
		Iterator& operator++();
		Iterator& operator--();
		Iterator prev();
		Iterator next();
	public:
		Iterator() {}
		Iterator( const typename HashBucket::Iterator& iter ) : HashBucket::Iterator(iter) {}
	};

	SortableHash( unsigned int hashCount = defaultSize )
		: hash(new HashBucket[hashCount]), hashCount(hashCount), elemCount(0) {}
	~SortableHash() { delete [] hash; }

	bool operator==( const SortableHash& h ) const {
		if ( hashCount == h.hashCount && elemCount == h.elemCount ) {
			unsigned int x = 0;
			for ( ; x < hashCount; x += 1 ) {
				typename HashBucket::ConstIterator iter1 = hash[x].begin();
				typename HashBucket::ConstIterator iter2 = h.hash[x].begin();
				for ( ; iter1 != hash[x].end() && iter2 = h.hash[x].end(); ++iter1, ++iter2 ) {
					if ( *iter1 != *iter2 ) return false;
				}
			}
			return true;
		}
		return false;
	}

	bool operator!=( const SortableHash& l ) const { return !operator==(h); }

	unsigned int size() const { return elemCount; }
	bool empty() const { return elemCount == 0; }

	Iterator lower_bound( const Key& elem ) const {
		return hash[getHashValue(elem,hashCount)].lower_bound( elem );
	}

	Iterator find( const Key& elem ) const {
		return hash[getHashValue(elem,hashCount)].find( elem );
	}

	bool contains( const Key& key ) const {
		return hash[getHashValue(elem,hashCount)].find(key) != hash[getHashValue(elem,hashCount)].end();
	}

	Iterator insert( ConstIterator pos, const Value& elem ) {
		elemCount += 1;
		return hash[getHashValue(elem,hashCount)].insert( pos, elem );
	}

	Iterator insert_sorted( const Value& elem ) {
		elemCount += 1;
		return hash[getHashValue(elem,hashCount)].insert_sorted( elem );
	}

	Iterator insert_unique( const Value& elem ) {
		unsigned int preCount = hash[getHashValue(elem,hashCount)].size();
		Iterator i = hash[getHashValue(elem,hashCount)].insert_unique( elem );
		elemCount += (hash[getHashValue(elem,hashCount)].size() - preCount);
		return i;
	}

	Iterator erase_key( const Key& elem ) {
		unsigned int preCount = hash[getHashValue(elem,hashCount)].size();
		Iterator i = hash[getHashValue(elem,hashCount)].erase_key( elem );
		elemCount -= (preCount - hash[getHashValue(elem,hashCount)].size());
		return i;
	}

	Iterator erase( ConstIterator pos ) {
		elemCount -= 1;
		return hash[getHashValue(*pos,hashCount)].erase( pos );
	}

	const HashBucket& operator[]( unsigned int x ) const {
		return hash[x];
	}

	const HashBucket& getHashBucket( const Key& elem ) const {
		return hash[getHashValue(elem,hashCount)];
	}

	unsigned int getHashCount() const { return hashCount; }

	void callForAll( typename HashBucket::CallForAllVoid call ) {
		unsigned int x = 0;
		for ( ; x < hashCount; x += 1 ) {
			typename HashBucket::Iterator i = hash[x].begin();
			for ( ; i != hash[x].end(); ++i ) {
				call( *i );
			}
		}
	}
};

#endif /* _RSVP_SortableHash_h_ */
