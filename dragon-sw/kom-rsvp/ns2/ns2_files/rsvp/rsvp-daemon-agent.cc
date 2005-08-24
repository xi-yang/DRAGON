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
#include "../RSVP_config.h"

#include "rsvp-daemon-agent.h"
#include "RSVP_Daemon_Wrapper.h"

#include "RSVP_Global.h"
#include "RSVP_LogicalInterface.h"

double RSVP_Daemon_Agent::defaultRefresh_ = 30.0;
double RSVP_Daemon_Agent::defaultRapidRefresh_ = 0.5;

RSVP_Daemon_Agent::RSVP_Daemon_Agent() {
	rsvpDaemonWrapper= new RSVP_Daemon_Wrapper( this );
}

RSVP_Daemon_Agent::~RSVP_Daemon_Agent() {
	delete rsvpDaemonWrapper;
}

// Called by NS2 just before simulation starts (via "simulator_ run").
// By that time all links should be set up, so we can add the node's interfaces to the global mapping list
void RSVP_Daemon_Agent::reset() {
	registerLIFs();
	rsvpDaemonWrapper->createRSVPModule();
}

void RSVP_Daemon_Agent::registerLIFs() {
	Tcl& tcl = Tcl::instance();

	// get a string containing all iifs of this agent's node
	tcl.evalf( "[%s set node_] get-all-iifs", name() );
	char* iifList = tcl.result();

	// now use a copy of the string to extract the iifs (don't use original, because strtok() inserts \0!)
	char* iifList_copy= new char[ strlen( iifList ) ];
	strcpy( iifList_copy, iifList );

	String oif;
	String link;
	String linkObj;
	String linkType;
	ieee32float totalBw;
	uint32 latency;
	char* iif= strtok( iifList_copy, " " );
	while( iif ) {
		// this is ugly, but the link only exists in OTcl...
		tcl.evalf( "[%s set node_] iif2oif %s", name(), iif );
		oif = tcl.result();
		tcl.evalf( "[%s set node_] oif2link %s", name(), oif.chars() );
		linkObj = tcl.result();
		tcl.evalf( "%s info class", linkObj.chars() );
		linkType = tcl.result();
		tcl.evalf( "%s bw", linkObj.chars() );
		totalBw = convertStringToFloat( tcl.result() );
		tcl.evalf( "%s delay", linkObj.chars() );
		latency = static_cast<uint32>( convertStringToFloat( tcl.result() ) * 1000.0 );

		rsvpDaemonWrapper->registerInterface( getLocalAddr(), atoi(iif), oif, linkObj, linkType, totalBw, latency );
		iif = strtok( NULL, " " );
	}

	delete iifList_copy;
}


// informs wrapper when agent receives a packet
void RSVP_Daemon_Agent::recv(Packet* p, Handler* h) {
	currentPkt = p;
	rsvpDaemonWrapper->notifyPacketArrival( hdr_cmn::access(p)->iface() );
}

void RSVP_Daemon_Agent::expire( Event* e ) {
	rsvpDaemonWrapper->fireTimer();
}


// used to create C++ objects from Tcl, registers "Agent/RSVP"
static class RSVP_Agent_Class : public TclClass {
public:
  RSVP_Agent_Class() : TclClass("Agent/RSVP") {}
  TclObject* create(int, const char*const*) {
    return (new RSVP_Daemon_Agent());
  }
  virtual void bind();
  virtual int method(int ac, const char*const* av);
} class_RSVP_Agent;

void RSVP_Agent_Class::bind() {
	TclClass::bind();
	add_method("defaultRefresh");
	add_method("defaultRapidRefresh");
	add_method("logfile");
}

class InstVar {
public:
	static double time_atof(const char* s);
};

int RSVP_Agent_Class::method(int ac, const char*const* av) {
	int argc = ac - 2;
	const char*const* argv = av + 2;
	if (argc == 3) {
		if (strcmp(argv[1], "defaultRefresh") == 0) {
			RSVP_Daemon_Agent::defaultRefresh_ = InstVar::time_atof(argv[2]);
			return TCL_OK;
		} else if (strcmp(argv[1], "defaultRapidRefresh") == 0) {
			RSVP_Daemon_Agent::defaultRapidRefresh_ = InstVar::time_atof(argv[2]);
			return TCL_OK;
		}
	} else if (argc == 5) {
		if (strcmp(argv[1], "logfile") == 0 ) {
			Log::init( argv[2], argv[3], argv[4], true );
			return TCL_OK;
		}
	}
	return TclClass::method(ac,av);
}
