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
#ifndef _RSVP_Relationships_h_
#define _RSVP_Relationships_h_ 1

#include <assert.h>

#define RELCHECK	assert

// methods need from container class for many-relationships:
// const-iterator with pre-operator ++
// insert_unique
// insert at iterator
// erase at iterator
// begin
// end

#if !((__GNUC__ >= 2 && __GNUC_MINOR__ >= 95) || __GNUC__ >= 3)
#error These classes only compile when using at least gcc 2.95
#endif

template <class Other>
class RelationshipTo1 {
protected:
	Other* other;
public:
	RelationshipTo1() : other(0) {}
	void setRelationshipHalf( Other* other ) { RELCHECK( !this->other );
		this->other = other;
	}
	void clearRelationshipHalf() { RELCHECK( this->other );
		other = (Other*)0;
	}
	Other* followRelationship() {
		return other;
	}
	const Other* followRelationship() const {
		return other;
	}
};

template <class This, class Other>
class Relationship1to1 : public RelationshipTo1<Other> {
	Relationship1to1( const Relationship1to1& );              // forbid copy
	Relationship1to1& operator=( const Relationship1to1& );   // forbid assignment
public:
	Relationship1to1() {}
	~Relationship1to1() {
		if (this->other) this->other->clearRelationshipHalf();
	}
	void setRelationshipFull( This* me, Other* other ) {
		other->Relationship1to1<Other,This>::setRelationshipHalf( me );
		setRelationshipHalf( other );
	}
	void clearRelationshipFull() {
		this->other->Relationship1to1<Other,This>::clearRelationshipHalf();
		this->clearRelationshipHalf();
	}
};

struct DoOnEmptyDelete1 {
	bool operator()() const { return true; }
};

struct DontOnEmptyDelete1 {
	bool operator()() const { return false; }
};

template <class This, class Other, class OtherContainer, class OnEmptyDelete1>
class Relationship1toMANY;

template <class This, class ThisContainer, class Other, class OnEmptyDelete1=DontOnEmptyDelete1>
class RelationshipMANYto1 : public RelationshipTo1<Other> {
	RelationshipMANYto1( const RelationshipMANYto1& );              // forbid copy
	RelationshipMANYto1& operator=( const RelationshipMANYto1& );   // forbid assignment
	typedef typename ThisContainer::ConstIterator ConstIterator;
	ConstIterator thisIter;
public:
	RelationshipMANYto1() {}
	~RelationshipMANYto1() {
		if (this->other) clearRelationshipFull();
	}
	void setRelationshipFull( This* me, Other* other ) {
		thisIter = other->Relationship1toMANY<Other,This,ThisContainer,OnEmptyDelete1>::setRelationshipHalf( me );
		setRelationshipHalf( other );
	}
	void setRelationshipFull( This* me, Other* other, const ConstIterator& position ) {
		thisIter = other->Relationship1toMANY<Other,This,ThisContainer,OnEmptyDelete1>::setRelationshipHalf( me, position );
		setRelationshipHalf( other );
	}
	void clearRelationshipFull() {
		if ( OnEmptyDelete1()() && this->other->Relationship1toMANY<Other,This,ThisContainer,OnEmptyDelete1>::followRelationship().size() <= 1 ) {
			delete this->other; 
			this->other = NULL;
		} else {
			this->other->Relationship1toMANY<Other,This,ThisContainer,OnEmptyDelete1>::clearRelationshipHalf( thisIter );
			this->clearRelationshipHalf();
		}
	}
	bool changeRelationshipFull( This* me, Other* other ) {
		if ( !this->other ) {
			setRelationshipFull( me, other );
		} else if ( this->other != other ) {
			clearRelationshipFull();
			setRelationshipFull( me, other );
		} else {
			return false;
		}
		return true;
	}
};

template <class Other, class OtherContainer>
class RelationshipToMANY {
public:
	typedef typename OtherContainer::ConstIterator ConstIterator;
protected:
	OtherContainer others;
public:
	RelationshipToMANY() {}
	RelationshipToMANY( unsigned int count ) : others(count) {}
	ConstIterator setRelationshipHalf( Other* other ) {
		return others.insert_unique( other );
	}
	ConstIterator setRelationshipHalf( Other* other, const ConstIterator& position ) {
		return others.insert( position, other );
	}
	void clearRelationshipHalf( Other* other ) {
		others.erase_key( other );
	}
	void clearRelationshipHalf( ConstIterator iter ) {
		others.erase( iter );
	}
	OtherContainer& followRelationship() {
		return others;
	}
	const OtherContainer& followRelationship() const {
		return others;
	}
};

template <class This, class Other, class OtherContainer, class OnEmptyDelete1=DontOnEmptyDelete1>
class Relationship1toMANY : public RelationshipToMANY<Other,OtherContainer> {
	typedef typename RelationshipToMANY<Other,OtherContainer>::ConstIterator ConstIterator;
	Relationship1toMANY( const Relationship1toMANY& );              // forbid copy
	Relationship1toMANY& operator=( const Relationship1toMANY& );   // forbid assignment
public:
	Relationship1toMANY() {}
	Relationship1toMANY( unsigned int count ) : RelationshipToMANY<Other,OtherContainer>(count) {}
	~Relationship1toMANY() {
		ConstIterator iter = this->others.begin();
		for ( ; iter != this->others.end(); ++iter ) {
			const_cast<Other*>(*iter)->RelationshipTo1<This>::clearRelationshipHalf();
		}
	}
	void setRelationshipFull( This* me, Other* other ) {
		other->RelationshipMANYto1<Other,OtherContainer,This,OnEmptyDelete1>::setRelationshipFull( other, me );
	}
	void setRelationshipFull( This* me, Other* other, const ConstIterator& position ) {
		other->RelationshipMANYto1<Other,OtherContainer,This,OnEmptyDelete1>::setRelationshipFull( other, me, position );
	}
	void clearRelationshipFull( Other* other ) {
		other->RelationshipMANYto1<Other,OtherContainer,This,OnEmptyDelete1>::clearRelationshipFull();
	}
};

#endif /* _RSVP_Relationships_h_ */
