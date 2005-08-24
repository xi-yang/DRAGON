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
#ifndef _CommandParser_h_
#define _CommandParser_h_ 1

#include "RSVP_API.h"

#include <iostream>

union GenericUpcallParameter;
class DCPE;
class UCPE;
class RSVP_API_Wrapper;

class CommandParser {
#if defined(NS2)
	RSVP_API_Wrapper& wrapper;
#endif
	istream* is;
	String token;
	RSVP_API& api;
	RSVP_API::SessionId currentSession;
	InterfaceHandle dummyFD;
	bool gotPath;
	bool& endFlag;
	SENDER_Object waitpathSender;
	SENDER_Object lastPathSender;
	UCPE* ucpe;
	FLOWSPEC_Object* readFlowspec( bool );
	void sessionCommand();
	void senderCommand();
	void unsenderCommand();
#if defined(ONEPASS_RESERVATION)
	void senderResvCommand();
#endif
	void reserveCommand();
	void unreserveCommand();
	void closeCommand();
	void sleepCommand();
	void waitPathCommand();
	void ucpeCommand();
	static void upcall( const GenericUpcallParameter&, CommandParser* );
public:
#if !defined(NS2)
	CommandParser( istream& is, RSVP_API& api, bool& endFlag )
		: is(&is), api(api), dummyFD(-1), gotPath(0), endFlag(endFlag), ucpe(NULL) {}
#else
	CommandParser( RSVP_API_Wrapper& wrapper, RSVP_API& api, bool& endFlag )
		: wrapper(wrapper), is(NULL), api(api), dummyFD(-1), gotPath(0), endFlag(endFlag), ucpe(NULL) {}
	void setIS( istream& i ) { is = &i; i>>token; }
#endif
	bool readNextCommand();
	bool execNextCommand();
};

#endif /* _CommandParser_h_ */
