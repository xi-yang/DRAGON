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
#ifndef _RSVP_PolicyOptions_h_
#define _RSVP_PolicyOptions_h_ 1
          
#include "RSVP_BasicTypes.h"

class PolicyRefreshPeriod {
	uint32 refreshPeriod;
	friend ostream& operator<< ( ostream&, const PolicyRefreshPeriod& ); 
  friend ONetworkBuffer& operator<< ( ONetworkBuffer&, const PolicyRefreshPeriod& ); 
  friend INetworkBuffer& operator>> ( INetworkBuffer&, PolicyRefreshPeriod& );
	static uint16 size() { return 4; }
public:
	PolicyRefreshPeriod( uint32 refreshPeriod ) : refreshPeriod(refreshPeriod) {}
	PolicyRefreshPeriod( INetworkBuffer& buffer ) { buffer >> *this; }
	uint32 getRefreshPeriod() const { return refreshPeriod; }
	static uint16 total_size() { return size() + RSVP_ObjectHeader::size(); }
};	

extern inline INetworkBuffer& operator>> ( INetworkBuffer& buffer, PolicyRefreshPeriod& o ) {
	buffer >> o.refreshPeriod;
	return buffer;
}

extern inline ONetworkBuffer& operator<< ( ONetworkBuffer& buffer, const PolicyRefreshPeriod& o ) {
	buffer << RSVP_ObjectHeader(o.size(), RSVP_ObjectHeader::null, 1 );
	buffer << o.refreshPeriod;
	return buffer;
}

extern inline ostream& operator<< ( ostream& os, const PolicyRefreshPeriod& o ) {
	os << "PolicyRefresh: " << o.refreshPeriod;
	return os;
}

#endif /* _RSVP_PolicyOptions_h_ */
