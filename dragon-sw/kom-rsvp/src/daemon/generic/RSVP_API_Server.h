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
#ifndef _RSVP_API_Server_h_
#define _RSVP_API_Server_h_ 1

#if defined(WITH_API)

#include "RSVP_API_Entry.h"
#include "RSVP_BasicTypes.h"
#include "RSVP_SortedList.h"

class SESSION_Object;
class Message;
class ONetworkBuffer;
class LogicalInterfaceAPI_Server;
class MessageProcessor;
class Hop;

typedef SortedList<API_Entry*,API_EntryKey*> ApiEntryList;

class API_Server {
	ApiEntryList* apiEntryList;
	inline const ApiEntryList& getApiList( const SESSION_Object& ) const;
	inline ApiEntryList& getApiList( const SESSION_Object& );
	LogicalInterfaceAPI_Server* apiLif;
	MessageProcessor* currentProcessor;
	uint16 currentPort;
	const NetAddress* currentAddress;
	void deregisterAPI( const ApiEntryList::ConstIterator );
	// statistics data
	uint32 clientCount;
public:
	API_Server( uint16 );
	~API_Server();
	inline void timeoutAPI( const SESSION_Object& session, const NetAddress&, uint16 );
	inline void requestRefreshAPI( const SESSION_Object& session, const NetAddress&, uint16 );
	void processMessage( const Message& msg, MessageProcessor& );
	void sendMessage( const Message& msg );
	const LogicalInterfaceAPI_Server* getApiLif() const { assert(apiLif); return apiLif; }
	bool findApiSession( const SESSION_Object& session ) const;
#if defined(RSVP_STATS)
	uint32 getNumberOfClients() { return clientCount; }
#endif
};

#endif /* WITH_API */

#endif /* _RSVP_API_Server_h_ */
