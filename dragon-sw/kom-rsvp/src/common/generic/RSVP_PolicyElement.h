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
#ifndef _RSVP_PolicyElement_h_
#define _RSVP_PolicyElement_h_ 1

#include "RSVP_BasicTypes.h"

class PolicyElement {
	friend class PolicyElementCompare;
public:
	enum P_Type {
		null = 0,
		UPSTREAM_CHARGING_POLICY = 65535
	};

protected:
	uint16 length;
	uint16 pType;
	friend inline ostream& operator<< ( ostream&, const PolicyElement& );
	friend inline ONetworkBuffer& operator<< ( ONetworkBuffer&, const PolicyElement& );
	friend inline INetworkBuffer& operator>> ( INetworkBuffer&, PolicyElement& );
public:
	PolicyElement( uint16 length = 0, uint16 pType = 0 )
		: length(size()), pType(pType) {}
	PolicyElement( const PolicyElement& pe ) : length(pe.length), pType(pe.pType) {}
	static uint16 size() { return 4; }
	uint16 getLength() const { return length; }
	const uint16 getPType() const { return pType; }
};

extern inline INetworkBuffer& operator>> ( INetworkBuffer& buf, PolicyElement& o ) {
	buf >> o.length >> o.pType;
	return buf;
}

extern inline ONetworkBuffer& operator<< ( ONetworkBuffer& buf, const PolicyElement& o ) {
	buf << o.length << o.pType;
	return buf;
}

extern inline ostream& operator<< ( ostream& os, const PolicyElement& o ) {
	os << " length:" << o.length << "pType: " << o.pType;
	return os;
}

class UnknownPolicyElement : public PolicyElement {
	Buffer content;
	friend ostream& operator<< ( ostream&, const UnknownPolicyElement& );
	friend istream& operator>> ( istream&, UnknownPolicyElement& ); 
	friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const UnknownPolicyElement& );
	friend INetworkBuffer& operator>> ( INetworkBuffer&, UnknownPolicyElement& );
public:
	UnknownPolicyElement( const PolicyElement& pe, INetworkBuffer& buffer )
	: PolicyElement(pe) { buffer >> *this; }
};

#endif /* _RSVP_PolicyElement_h_ */
