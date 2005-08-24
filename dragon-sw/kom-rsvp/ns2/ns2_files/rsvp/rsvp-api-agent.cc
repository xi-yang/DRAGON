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
#include "rsvp-api-agent.h"

#include "RSVP_API_Wrapper.h"

static class RSVP_API_Class : public TclClass {
public:
  RSVP_API_Class() : TclClass("Agent/RSVPApi") {}
  TclObject* create(int, const char*const*) {
    return (new RSVP_API_Agent());
  }
} class_RSVP_API;

RSVP_API_Agent::RSVP_API_Agent() {
	rsvpApiWrapper= new RSVP_API_Wrapper( this );
}

RSVP_API_Agent::~RSVP_API_Agent() {
	delete rsvpApiWrapper;
	rsvpApiWrapper = NULL;
}

int RSVP_API_Agent::command( int argc, const char*const* argv ) {
	if ( !strcmp(argv[1], "session")
		|| !strcmp(argv[1], "sender")
		|| !strcmp(argv[1], "unsender")
		|| !strcmp(argv[1], "senderresv")
		|| !strcmp(argv[1], "reserve")
		|| !strcmp(argv[1], "unreserve")
		|| !strcmp(argv[1], "close")
		|| !strcmp(argv[1], "waitpath")
		|| !strcmp(argv[1], "sleep")
		|| !strcmp(argv[1], "ucpe") ) {
		rsvpApiWrapper->acceptCommand( argc, argv );
		return (TCL_OK);
	} else {
		return (RSVP_Agent::command(argc, argv));
	}
}

// Called by NS2 just before simulation starts (via "simulator_ run").
void RSVP_API_Agent::reset() {
	rsvpApiWrapper->createAPI();
}

// informs wrapper when agent receives a packet
void RSVP_API_Agent::recv( Packet* p, Handler* h ) {
	currentPkt = p;
	rsvpApiWrapper->notifyPacketArrival( hdr_cmn::access(p)->iface() );
}

void RSVP_API_Agent::upcall( const char* upcallCommand ) {
	Tcl& tcl = Tcl::instance();
	tcl.eval( upcallCommand );
}
