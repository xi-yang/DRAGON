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
#include "tg_classes.h"
#include "RSVP_Log.h"

extern bool tg_parser_parse( const String& filename, RSVP_API* );

static void rsvpProcess( RSVP_API* api ) {
	api->receiveAndProcess();
}

static void killHandler( int ) {
	traffgen->finish();
}

static void usage( const char* program ) {
	cout << "usage: " << program << " [options] config-file" << endl;
	cout << endl;
	cout << "Option list:" << endl;
	cout << "-h, -?                      print this help" << endl;
	cout << "-o output file              write logging output into file" << endl;
	cout << "-p packetrate               max packet rate per sec" << endl;
	cout << "-t usecs                    timer resolution in usec" << endl;
	cout << "-s hhmmss                   absolute start time" << endl;
	cout << "-r seconds                  relative start time" << endl;
	cout << "-H host                     RSVPD host" << endl;
	cout << "-P port                     RSVPD port" << endl;
}

static TimeValue convertTimeAbs( const String& hhmmss ) {
	char hours[3], minutes[3], secs[3];
	hours[0] = hours[1] = '0'; hours[2] = 0;
	minutes[0] = minutes[1] = '0'; minutes[2] = 0;
	secs[0] = secs[1] = '0'; secs[2] = 0;
	uint32 lastpos = hhmmss.length();
	if (lastpos == 0) goto calc; lastpos--; secs[1] = hhmmss[lastpos];
	if (lastpos == 0) goto calc; lastpos--; secs[0] = hhmmss[lastpos];
	if (lastpos == 0) goto calc; lastpos--; minutes[1] = hhmmss[lastpos];
	if (lastpos == 0) goto calc; lastpos--; minutes[0] = hhmmss[lastpos];
	if (lastpos == 0) goto calc; lastpos--; hours[1] = hhmmss[lastpos];
	if (lastpos == 0) goto calc; lastpos--; hours[0] = hhmmss[lastpos];
calc:
	TimeValue t = getCurrentSystemTime();
	t.tv_usec = 0;
	struct tm* ltime = localtime( (time_t*)&t.tv_sec );
	uint32 add_secs = 0;
	ltime->tm_hour = atoi(hours);
	ltime->tm_min = atoi(minutes);
	ltime->tm_sec = atoi(secs);
	if ( ltime->tm_sec > 59 ) {
		ltime->tm_sec -= 60;
		ltime->tm_min += 1;
	}
	if ( ltime->tm_min > 59 ) {
		ltime->tm_min -= 60;
		ltime->tm_hour += 1;
	}
	if ( ltime->tm_hour > 23 ) {
		ltime->tm_hour = 0;
		add_secs = 24*60*60;
	}
	t.tv_sec = mktime( ltime );
	t.tv_sec += add_secs;
	return t;
}

int main( int argc, char* const argv[] ) {
	uint32 slotlen = 0;
	uint32 mpr = 0;
	TimeValue startTime(0,0);
	String absStart;
	uint32 waitSecs = 0;
	String rsvpHost = "127.0.0.1";
	uint16 rsvpPort = RSVP_Global::apiPort;
	const char* conffile = NULL;
	const char* logfile = "";
	const char* progname = argv[0];
	if ( argc > 1 && argv[1][0] != '-' ) {
		conffile = argv[1];
		argv++;
		argc--;
	}
	for (;;) {
		int option = getopt( argc, argv, "?ho:p:t:s:r:H:P:" );
		if (option == -1) {
	break;
		}
		switch(option) {
		case 'o':
			logfile = optarg;
			break;
		case 'p':
			mpr = atoi( optarg );
			break;
		case 't':
			slotlen = atoi( optarg );
			break;
		case 's':
			absStart = optarg;
			break;
		case 'r':
			waitSecs = atoi( optarg );
			break;
		case 'H':
			rsvpHost = optarg;
			break;
		case 'P':
			rsvpPort = atoi( optarg );
			break;
		default:
			usage( progname );
			return 1;
		}
	}

	if ( !conffile ) {
		if ( argc <= optind ) {
			usage( progname );
			return 1;
		} else {
			conffile = argv[optind];
		}
	}

	if ( logfile != "" ) {
		Log::init( Log::Fatal | Log::Error | LogWarning | LogStats | LogActions | LogOther, logfile, true );
	} else {
		Log::init( Log::Fatal | Log::Error | LogWarning | LogStats | LogActions | LogOther );
	}
	traffgen = new TG::TrafficGenerator( slotlen, mpr );

	struct sigaction sa;
	initMemoryWithZero( &sa, sizeof(sa) );
	sigemptyset( &sa.sa_mask );
	sa.sa_handler = killHandler;
	sa.sa_flags = SA_RESETHAND;
	CHECK( sigaction( SIGHUP, &sa, NULL ) );
	CHECK( sigaction( SIGINT, &sa, NULL ) );
	CHECK( sigaction( SIGQUIT, &sa, NULL ) );
	CHECK( sigaction( SIGTERM, &sa, NULL ) );
	CHECK( sigaction( SIGPIPE, &sa, NULL ) );

	RSVP_API rapi( rsvpPort, rsvpHost );
	traffgen->registerRSVP_API( rapi, (TG::Notification)rsvpProcess );
	cerr << "initializing..." << endl;
	if ( tg_parser_parse( conffile, &rapi ) ) {
		cerr << "starting traffic generator..." << endl;
		if ( absStart.length() > 0 ) {
			startTime = convertTimeAbs( absStart );
		}
		traffgen->main( startTime, waitSecs );
	}
	traffgen->unregisterRSVP_API();
	delete traffgen;
	Log::close();
	return 0;
}
