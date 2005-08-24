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
#ifndef _RSVP_SENDER_Object_h_
#define _RSVP_SENDER_Object_h_ 1

#include "RSVP_Lists.h"
#include "RSVP_BasicTypes.h"
#include "RSVP_Helper.h"
#include "RSVP_ObjectHeader.h"

class SENDER_Object {
protected:
	NetAddress srcAddress;
	uint16 srcPort;
	// implemented in RSVP_ProtocolObjects.cc
	friend ostream& operator<< ( ostream&, const SENDER_Object& );
	friend INetworkBuffer& operator>> ( INetworkBuffer&, SENDER_Object& );
	DECLARE_ORDER(SENDER_Object)
	static uint16 size() { return NetAddress::size() + 4; }
public:
	SENDER_Object( const NetAddress& srcAddress = 0, uint16 srcPort = 0 )
		: srcAddress(srcAddress), srcPort(srcPort) {}
	SENDER_Object( INetworkBuffer& buffer ) { buffer >> *this; }
	static uint16 total_size() { return size() + RSVP_ObjectHeader::size(); }
	const NetAddress& getSrcAddress() const { return srcAddress; }
	void setSrcAddress( const NetAddress& s ) { srcAddress = s; }
	const uint16& getLspId() const { return srcPort; }
	void setLspId( uint16 p ) { srcPort = p; }
};
IMPLEMENT_ORDER2(SENDER_Object,srcAddress,srcPort)

inline bool Less<SENDER_Object*>::operator()( const SENDER_Object* s1, const SENDER_Object* s2 ) const {
	return *s1 < *s2;
}

#endif /* _RSVP_SENDER_Object_h_ */
