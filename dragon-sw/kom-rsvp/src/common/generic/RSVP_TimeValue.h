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
#ifndef _RSVP_TimeValue_h_
#define _RSVP_TimeValue_h_

#include "RSVP_BasicTypes.h"
#include "RSVP_Helper.h"
#include "RSVP_System.h"

#include <iostream>
#include <iomanip>

class TimeValue;
class TimeValueLong {
protected:
	sint64 tv_sec;
	sint32 tv_usec;
	friend class TimeValue;
	friend inline ostream& operator<< ( ostream&, const TimeValueLong& );
	void correct() {
		if ( tv_usec >= USECS_PER_SEC ) {
			tv_sec += 1; tv_usec -= USECS_PER_SEC;
		}
		if ( tv_usec < 0 && tv_sec > 0 ) {
			tv_sec -= 1; tv_usec += USECS_PER_SEC;
		} else if ( tv_sec < 0 && tv_usec > 0 ) {
			tv_sec += 1; tv_usec -= USECS_PER_SEC;
		}
	}
	static TimeValueLong fromUsec( sint64 usec ) {
		return TimeValueLong( usec / USECS_PER_SEC, usec % USECS_PER_SEC );
	}
public:
	TimeValueLong( sint32 sec = 0, sint32 usec = 0 )
		: tv_sec(sec), tv_usec(usec) {}
	sint64 getUsec() const {
		return ((sint64)tv_sec * (sint64)USECS_PER_SEC + tv_usec);
	}
	inline TimeValueLong& operator+= ( const TimeValue& t );
	inline TimeValueLong& operator-= ( const TimeValue& t );
	inline TimeValue operator* ( sint32 i ) const;
	inline TimeValue operator/ ( sint32 i ) const;
	inline TimeValue sqrt();
};

// timerep: tv_sec <-> seconds,  tv_usec <-> microseconds
class TimeValue : public timerep {
	friend inline ostream& operator<< ( ostream&, const TimeValue& );
	void correct() {
		if ( tv_usec >= USECS_PER_SEC ) {
			tv_sec += 1; tv_usec -= USECS_PER_SEC;
		}
		if ( tv_usec < 0 && tv_sec > 0 ) {
			tv_sec -= 1; tv_usec += USECS_PER_SEC;
		} else if ( tv_sec < 0 && tv_usec > 0 ) {
			tv_sec += 1; tv_usec -= USECS_PER_SEC;
		}
	}
	static TimeValue fromUsec( sint64 usec ) {
		return TimeValue( usec / sint64(USECS_PER_SEC), usec % sint64(USECS_PER_SEC) );
	}
public:
	TimeValue( sint32 sec = 0, sint32 usec = 0 ) {
		this->tv_sec = sec; this->tv_usec = usec;
	}
	TimeValue( const timerep& t ) : timerep(t) {}
	TimeValue( const TimeValueLong& t ) {
		this->tv_sec = t.tv_sec; this->tv_usec = t.tv_usec;
	}
	TimeValue& operator=( const TimeValueLong& t ) {
		this->tv_sec = t.tv_sec; this->tv_usec = t.tv_usec;
		return *this;
	}
	sint64 getUsec() const {
		return (sint64(tv_sec) * sint64(USECS_PER_SEC) + tv_usec);
	}
	TimeValue& operator+= ( const TimeValue& t ) {
		tv_sec += t.tv_sec;
		tv_usec += t.tv_usec;
		correct();
		return *this;
	}
	TimeValue& operator-= ( const TimeValue& t ) {
		tv_sec -= t.tv_sec;
		tv_usec -= t.tv_usec;
		correct();
		return *this;
	}
	TimeValue operator* ( sint32 i ) const {
		sint64 usec = getUsec() * sint64(i);
		return fromUsec( usec );
	}
	TimeValue operator/ ( sint32 i ) const {
		sint64 usec = getUsec() / sint64(i);
		return fromUsec( usec );
	}
	sint32 operator/ ( const TimeValue& t2 ) const {
		return getUsec() / t2.getUsec();
	}
	TimeValue operator% ( const TimeValue& t2 ) const {
		sint64 usec_rest = getUsec() % t2.getUsec();
		return fromUsec( usec_rest );
	}
	TimeValue multFloat( ieee32float_p p ) const {
		sint64 usec = (sint64)(getUsec() * p);
		return fromUsec( usec );
	}
	TimeValue pow2() {
		uint64 usec = ((uint64)getUsec() * (uint64)getUsec()) / USECS_PER_SEC;
		return fromUsec( usec );
	}
	TimeValue sqrt() {
		sint64 usec = (sint64)::sqrt(getUsec() * USECS_PER_SEC);
		return fromUsec( usec );
	}
	ieee32float_p getFractionalValue() const {
		ieee32float_p result = tv_usec;
		result = (result / USECS_PER_SEC) + tv_sec;
		return result;
	}
	void getFromFraction( ieee32float x ) {
		tv_sec = (sint32)x;
		tv_usec = (sint32)((x-tv_sec) * USECS_PER_SEC);
		if ( tv_sec < 0 ) tv_usec = -tv_usec;
	}
	DECLARE_ORDER(TimeValue)
	static uint16 size() { return 8; }
};
IMPLEMENT_ORDER2(TimeValue,tv_sec,tv_usec)
extern inline ostream& operator<< ( ostream& s, void (*func)(bool) ) {
	func(true);
	return s;
}
extern inline TimeValue operator+ ( const TimeValue& t1, const TimeValue& t2 ) {
	return TimeValue(t1) += t2;
}
extern inline TimeValue operator- ( const TimeValue& t1, const TimeValue& t2 ) {
	return TimeValue(t1) -= t2;
}
extern inline TimeValue operator* ( sint32 i, const TimeValue& t1 ) {
	return t1 * i;
}
extern inline ostream& operator<< ( ostream& os, const TimeValue& t ) {
	os << t.tv_sec << ".";
	os << setfill('0') << setw(3) << t.tv_usec/USECS_PER_MSEC;
	os << " sec";
	return os;
}
extern inline ONetworkBuffer& operator<<( ONetworkBuffer& buffer, const TimeValue& t ) {
	buffer << sint32(t.tv_sec) << sint32(t.tv_usec);
	return buffer;
}
extern inline INetworkBuffer& operator>>( INetworkBuffer& buffer, const TimeValue& t ) {
        sint32 *psec = (sint32*)&(t.tv_sec), *pusec = (sint32*)&(t.tv_usec);
	buffer >> *psec >> *pusec;
	return buffer;
}

extern inline ostream& operator<< ( ostream& os, const TimeValueLong& t ) {
	os << TimeValue(t);
	return os;
}
inline TimeValueLong& TimeValueLong::operator+= ( const TimeValue& t ) {
	tv_sec += t.tv_sec;
	tv_usec += t.tv_usec;
	correct();
	return *this;
}
inline TimeValueLong& TimeValueLong::operator-= ( const TimeValue& t ) {
	tv_sec -= t.tv_sec;
	tv_usec -= t.tv_usec;
	correct();
	return *this;
}
inline TimeValue TimeValueLong::operator* ( sint32 i ) const {
	sint64 usec = getUsec() * i;
	return TimeValue( fromUsec( usec ) );
}
inline TimeValue TimeValueLong::operator/ ( sint32 i ) const {
	sint64 usec = getUsec() / i;
	return TimeValue( fromUsec( usec ) );
}
inline TimeValue TimeValueLong::sqrt() {
	sint64 usec = (sint64)::sqrt(getUsec());
	return TimeValue( fromUsec( usec ) );
}

class DaytimeTimeValue : public TimeValue {
public:
	friend inline ostream& operator<< ( ostream& os, const DaytimeTimeValue& t ) {
		uint32 hours, minutes;
		convertToLocalTime( t.tv_sec, hours, minutes );
		os << setfill('0') << setw(2) << hours << ":" << setw(2) << minutes << ":"
			<< setw(2) << (t.tv_sec % 60) << ".";
		os << setfill('0') << setw(3) << t.tv_usec/USECS_PER_MSEC;
		return os;
	}
};

class PreciseTimeValue : public TimeValue {
public:
	friend inline ostream& operator<< ( ostream& os, const PreciseTimeValue& t ) {
		os << t.tv_sec << ".";
		os << setfill('0') << setw(6) << t.tv_usec;
		os << " sec";
		return os;
	}
};

#endif /* _RSVP_TimeValue_h_ */
