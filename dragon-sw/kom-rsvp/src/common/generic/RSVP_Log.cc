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
#include "RSVP_Log.h"
#include "RSVP_Global.h"
#include "RSVP_TimeValue.h"

#include <iostream>
#include <iomanip>
#include <fstream>

uint32 Log::loglevel = Log::Fatal;
ostream* Log::log = NULL;  
ostream* Log::stdlog = &cout;
ostream* Log::errlog = &cerr;
bool Log::virtualTime = false;

static struct __Logremove {
	~__Logremove() { Log::close(); }
} __logremove;

Log::debugOption Log::options[] = {
	{ (char*)"none", Log::None, 2 },
	{ (char*)"fatal", Log::Fatal, 1 },
	{ (char*)"error", Log::Error, 1 },
	{ (char*)"timer", Log::Timer, 2 },
	{ (char*)"ref", Log::Ref, 3 },
	{ (char*)"pc", Log::PC, 2 },
	{ (char*)"tc", Log::TC, 2 },
	{ (char*)"cbq", Log::CBQ, 2 },
	{ (char*)"msg", Log::Msg, 2 },
	{ (char*)"api", Log::API, 2 },
	{ (char*)"packet", Log::Packet, 3 },
	{ (char*)"session", Log::Session, 3 },
	{ (char*)"sb", Log::SB, 2 },
	{ (char*)"routing", Log::Routing, 2 },
	{ (char*)"rsrr", Log::RSRR,  2 },
	{ (char*)"config", Log::Config, 3 },
	{ (char*)"process", Log::Process, 2 },
	{ (char*)"parser", Log::Parser, 3 },
	{ (char*)"select", Log::Select, 3 },
	{ (char*)"scheduler", Log::Scheduler, 2 },
	{ (char*)"hfsc", Log::HFSC, 1 },
	{ (char*)"unused", Log::Unused1, 2 },
	{ (char*)"unused", Log::Unused2, 2 },
	{ (char*)"unused", Log::Unused3, 2 },
	{ (char*)"unused", Log::Unused4, 2 },
	{ (char*)"reduct", Log::Reduct, 3 },
	{ (char*)"mpls", Log::MPLS, 2 },
	{ (char*)"ns2", Log::NS, 1 },
	{ (char*)"short", Log::Short, 2 },
	{ (char*)"append", Log::Append, 2 },
	{ (char*)"all", Log::All, 2 }
};

void Log::parse( const String& s, bool disable ) {
	const char* begin = s.chars();
	while ( *begin != 0 ) {
		const char* end = strchr( begin, ',' );
		if ( !end ) {
			end = s.chars() + s.length() + 1;
		}
		uint32 i = 0;
		for ( ; i < sizeof(options)/sizeof(debugOption); ++i ) {
			if ( end-begin >= (int)options[i].nchars ) {
				if ( !strncmp( begin, options[i].name, options[i].nchars ) ) {
					if ( disable ) {
						loglevel &= ~options[i].level;
//						cerr << "disabling " << options[i].name << endl;
					} else {
						loglevel |= options[i].level;
//						cerr << "enabling " << options[i].name << endl;
					}
				}
			}
		}
		if ( (end - s.chars()) > (int)s.length() || *end == 0 ) {
	break;
		}
		begin = end + 1;
	}
}

void Log::close() {
	if (stdlog && stdlog != &cout && stdlog != &cerr ) {
		delete stdlog;
	}
	if (errlog && errlog != &cout && errlog != &cerr && errlog != stdlog) {
		delete errlog;
	}
	stdlog = NULL;
	errlog = NULL;
}

void Log::outInfo( ostream& os ) {
	static TimeValue currentTime;
#if defined(NS2)
	os << setw(4) << setfill('0') << getCurrentNodeNumber() << "|";
#endif
	if ( virtualTime ) {
		currentTime = RSVP_Global::getCurrentTime();
	} else {
		getCurrentSystemTime( currentTime );
	}
	os << (DaytimeTimeValue&)currentTime << " ";
}

void Log::init( uint32 loglevel, const String& filename, bool logErrorsInStdLog ) {
	close();
	Log::loglevel = loglevel;
	internalInit( filename, logErrorsInStdLog );
}

void Log::init( const String& enable, const String& disable, const String& filename, bool logErrorsInStdLog ) {
	close();
	parse( enable );
	parse( disable, true );
	internalInit( filename, logErrorsInStdLog );
}

void Log::internalInit( const String& filename, bool logErrorsInStdLog ) {
	if ( filename.empty() ) {
		stdlog = &cout;
	} else {
		stdlog = new ofstream( filename.chars(), ios::out |((loglevel & Log::Append) ? ios::app : ios::trunc) );
		if ( !stdlog || stdlog->bad() ) {
			cerr << "couldn't use stdlog " << filename << endl;
			cerr << "logging to stdout..." << endl;
			stdlog = &cout;
		} else {
                        logErrorsInStdLog = true;
		}

	}
	*stdlog << setiosflags(ios::fixed) << setprecision(3);
	if ( logErrorsInStdLog ) {
		errlog = stdlog;
	} else {
		errlog = &cerr;
		*errlog << setiosflags(ios::fixed) << setprecision(3);
	}
}

void Log::usage( ostream& os ) {
	os << options[0].name;
	uint32 i = 1;
	for ( ; i < sizeof(options)/sizeof(debugOption); ++i ) {
		os << "," << options[i].name;
	}
	os << endl;
}
