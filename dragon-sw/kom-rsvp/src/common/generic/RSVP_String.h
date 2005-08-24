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
#ifndef _RSVP_String_h_
#define _RSVP_String_h_ 1

#include <iostream>
#include <iomanip>
#include <string.h>

#define String RSVP_String

static const unsigned int maxRSVP_StringLength = 256;

class RSVP_String {
	char rep[maxRSVP_StringLength];
	unsigned int len;
	friend inline ostream& operator<< (ostream&, const RSVP_String& );
	friend inline istream& operator>> (istream&, RSVP_String& );
	static unsigned int getLengthFromCharP(const char* s) {
		unsigned int l = strlen(s);
		if (l >= maxRSVP_StringLength) l = maxRSVP_StringLength-1;
		return l;
	}
public:
	RSVP_String() : len(0) { rep[0] = 0; }
	RSVP_String( const RSVP_String& s ) : len(s.len) {
		strncpy( rep, s.rep, len ); rep[len] = 0;
	}
	RSVP_String( const char* s ) {
		len = getLengthFromCharP(s);
		strncpy( rep, s, len ); rep[len] = 0;
	}
	RSVP_String( const char c ) {
		len = 1;
		rep[0] = c; rep[1] = 0;
	}
	~RSVP_String() {}
	RSVP_String& operator=( const RSVP_String& s ) {
		len = s.len;
		strncpy( rep, s.rep, len ); rep[len] = 0;
		return *this;
	}
	RSVP_String& operator=( const char* s ) {
		len = getLengthFromCharP(s);
		strncpy( rep, s, len ); rep[len] = 0;
		return *this;
	}
	RSVP_String& operator=( const char c ) {
		len = 1;
		rep[0] = c; rep[1] = 0;
		return *this;
	}
	RSVP_String& operator+=( const RSVP_String& s ) {
		strncpy( rep + len, s.rep, s.len );
		len += s.len;
		rep[len] = 0;
		return *this;
	}
	RSVP_String& operator+=( const char* s ) {
		unsigned int slen = getLengthFromCharP(s);
		strncpy( rep + len, s, slen );
		len += slen;
		rep[len] = 0;
		return *this;
	}
	RSVP_String operator+( const RSVP_String& s1 ) {
		RSVP_String s = *this;
		s += s1;
		return s;
	}
	const char* chars() const { return rep; }
	bool operator==( const RSVP_String& s ) const {
		if ( len == s.len ) return strncmp( rep, s.rep, len ) == 0;
		return false;
	}
	bool operator==( const char* s ) const {
		unsigned int slen = getLengthFromCharP(s);
		if ( len == slen ) return strncmp( rep, s, len ) == 0;
		return false;
	}
	bool leftequal( const RSVP_String& s ) const {
		return strncmp( rep, s.rep, s.len ) == 0;
	}
	bool leftequal( const char* s ) const {
		unsigned int slen = getLengthFromCharP(s);
		return strncmp( rep, s, slen ) == 0;
	}
	bool operator!=( const RSVP_String& s ) const { return !operator==(s); }
	bool operator!=( const char* s ) const { return !operator==(s); }
	bool operator<( const RSVP_String& s ) const {
		return strncmp( rep, s.rep, len ) < 0;
	}
	bool operator<( const char* s ) const { 
		unsigned int slen = getLengthFromCharP(s);
		int result = strncmp( rep, s, slen );
		if ( result == 0 ) return len < slen;
		return result < 0;
	}
	bool empty() const { return len == 0; }
	unsigned int length() const { return len; }
	char operator[](int i) const { return rep[i]; }
};

inline ostream& operator<< ( ostream& os, const RSVP_String& s ) {
	os << s.rep;
	return os;
}

inline istream& operator>> ( istream& is, RSVP_String& s ) {
	is >> setw(maxRSVP_StringLength) >> s.rep;
	s.len = strlen(s.rep);
	return is;
}

#endif /* _RSVP_String_h_ */
