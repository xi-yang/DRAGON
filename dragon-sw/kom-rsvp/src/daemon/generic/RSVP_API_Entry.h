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
#ifndef _RSVP_API_Entry_h_
#define _RSVP_API_Entry_h_ 1

#include "RSVP_ProtocolObjects.h"
#include "RSVP_Timer.h"

class API_EntryKey {
	friend class Less<API_EntryKey*>;
protected:
	SESSION_Object session;
	NetAddress address;
	uint16 port;
public:
	API_EntryKey( const SESSION_Object& session, const NetAddress& address, uint16 port )
	: session(session), address(address), port(port) {}
	const SESSION_Object& getSession() { return session; }
};

class API_Entry : public API_EntryKey {
	friend class API_Server;
	TimeoutTimer<API_Entry> timer;
	RefreshTimer<API_Entry> refreshTimer;
	friend inline bool operator< ( const API_Entry&, const API_Entry& );
	friend inline bool operator== ( const API_Entry&, const API_Entry& );
	friend class TimeoutTimer<API_Entry>;
	inline void timeout();                             // implemented in API_Server.cc
public:
	inline void refresh();                             // implemented in API_Server.cc
	API_Entry( const SESSION_Object& session, const NetAddress& addr, uint16 port, const TimeValue& timeout )
		: API_EntryKey(session,addr,port), timer(*this,multiplyTimeoutTime(timeout)),
		refreshTimer(*this,randomizeRefreshTime(timeout)) {}
	void restartTimeout() {
		timer.restart();
	}
	void restartTimeout( const TimeValue& timeout ) {
		timer.restart( multiplyTimeoutTime(timeout) );
		refreshTimer.restart( randomizeRefreshTime(timeout) );
	}
};

template <> struct Less<API_EntryKey*> {
	bool operator()( const API_EntryKey* e1, const API_EntryKey* e2 ) const {
		if ( e1->session != e2->session ) {
			return e1->session < e2->session;
		} else if ( e1->address != e2->address ) {
			return e1->address < e2->address;
		} else {
			return e1->port < e2->port;
		}
	}
};

#endif /* _RSVP_API_Entry_h_ */
