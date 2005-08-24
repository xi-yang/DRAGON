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
#include "RSVP_System.h"
#include "RSVP_BasicTypes.h"
#include "RSVP_List.h"
#include "RSVP_Log.h"
#include "RSVP_NetworkService.h"

#include <time.h>                                            // time
#include <stdio.h>                                           // vsprintf
#include <stdarg.h>                                          // va_list,etc.

const int NetworkService::sockbufsize = 65536;
static char printBuffer[512];

// ensures above constructors, as well
void initSystem() {
#if !defined(NS2)
	_seed(time(NULL)%getpid());
#endif
}

// address is returned in network format
// called only during init and logging -> no special action for reentrance
bool convertStringToAddress( const char* s, uint32& address, bool try_only ) {
	bool retval = false;
	if ( !s ) goto end;
	if ( !strcmp( s, "localhost" ) ) s = "127.0.0.1";
#if defined(HAVE_INET_ATON)
	// this is a hack, but 'struct in_addr' only has this one member
	retval = (inet_aton( s, (in_addr*)&address ) != 0);
#else
	address = inet_addr( s );
	retval = (address != ~0u);
	if ( !retval && !strcmp( s, "255.255.255.255" ) ) {
		retval = true;
	}
#endif
#if defined(REAL_NETWORK)
	if ( !retval ) {
		struct hostent* host = gethostbyname( s );
		if ( host && host->h_length == 4 ) {
			address = *(uint32*)(host->h_addr_list[0]);
			retval = true;
		}
	}
#endif
end:
	if (!retval && !try_only) {
		cerr << "FATAL ERROR: cannot convert \"" << (s ? s : "") << "\" to network address" << endl;
		abortProcess();
	}
#if defined(NS2)
	if (retval) address = ntohl(address);
#endif
	return retval;
}

// addr must be given in network format
// called only during init and logging -> no special action for reentrance
String convertAddressToString( const NetAddress& address ) {
	static struct in_addr ia;
#if defined(NS2)
	ia.s_addr = htonl(address.rawAddress());
#else
	ia.s_addr = address.rawAddress();
#endif
	return String( inet_ntoa(ia) );
}

void printSafe( const char* fmt, ... ) {
	va_list args;
	va_start( args, fmt );
	vsprintf( printBuffer, fmt, args );
	write( 2, printBuffer, strlen( printBuffer ) );
	va_end( args );
}

void abortProcess() {
	FATAL(1)( Log::Fatal, "internal error -> forced abort" );
	Log::close();
	abort();
}

timerep measureTimerResolution() {
	// measure time of 'gettimeofday'
	TimeValue start, end, dummy;
	getCurrentSystemTime( start );
	getCurrentSystemTime( dummy );
	getCurrentSystemTime( end );
	TimeValue duration_gettimeofday = (end - start)/2;

	// measure 10 selects and take smallest duration
	TimeValue duration(1024,0);
	uint32 i = 5;
	for ( ; i > 0; i -= 1 ) {
		uint32 j = 5;
		getCurrentSystemTime( start );
		for ( ; j > 0; j -= 1 ) {
			dummy = TimeValue(0,1);
			select( 0, NULL, NULL, NULL, &dummy );
		}
		getCurrentSystemTime( end );
		TimeValue duration_select = ( (end-start)/5-duration_gettimeofday );
		if ( duration_select < duration ) duration = duration_select;
	}
	ERROR(2)( Log::Error, "measured select duration:", (PreciseTimeValue&)duration );

	// try to estimate true select duration from measured numbers
	// search for next appropriate divisor for 1 second
	while ( TimeValue(1,0) % duration != 0 ) {
		duration.tv_usec += 1;
	}
	return duration;
}
