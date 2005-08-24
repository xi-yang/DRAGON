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
#ifndef _RSVP_RefObject_h_
#define _RSVP_RefObject_h_ 1

#include "RSVP_Log.h"
#include "RSVP_System.h"

#define REF_OBJECT_METHODS(XXX) \
protected: \
	friend class RefObject< XXX >; \
	inline ~ XXX (); \
	XXX ( const XXX & ); \
	XXX & operator= ( const XXX & );

template <class T>
class RefObject {
	mutable unsigned int refCounter;
protected:
	~RefObject() {
	                                                    assert(refCounter == 1);
		LOG(2)( Log::Ref, "RefObject deleted: ", this );
	}
public:
	RefObject() : refCounter(1) {
		LOG(2)( Log::Ref, "RefObject created: ", this );
	}
	const T* borrow() const {
		LOG(3)( Log::Ref, "RefObject borrowed: refCounter is ", refCounter+1, this );
		refCounter += 1; return reinterpret_cast<const T*>(this);
	}
	T* borrow() {
		LOG(3)( Log::Ref, "RefObject borrowed: refCounter is ", refCounter+1, this );
		refCounter += 1; return reinterpret_cast<T*>(this);
	}
	void destroy() const {
		LOG(3)( Log::Ref, "RefObject destroyed: refCounter is ", refCounter, this );
		if (refCounter == 1) {
			delete reinterpret_cast<const T*>(this);
		} else {
			refCounter -= 1;
		}
	}
};

#endif /* _RSVP_RefObject_h_ */
