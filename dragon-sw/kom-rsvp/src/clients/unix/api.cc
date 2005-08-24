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
#include "RSVP_API.h"
#include "RSVP_API_Upcall.h"

#include "CommandParser.h"

#include <iostream>
#include <fstream>

#include "common.h"

static bool endFlag = false;
static void exitHandler( int ) {
	endFlag = true;
}

static void startHandler( int ) {
}

int main( int argc, char** argv ) {
	if ( argc < 2 ) {
		cerr << "usage: " << argv[0] << " config-file [command-file]" << endl;
		exit(1);
	}
	installExitHandler( exitHandler );
	struct sigaction sa;
	sigemptyset( &sa.sa_mask );
	sa.sa_handler = startHandler;
	sa.sa_flags = 0;
	CHECK( sigaction( SIGUSR1, &sa, NULL ) );
	Log::init( "all", "ref,packet,select" );
	RSVP_API api( argv[1] );
	istream* ifs = NULL;
	if ( argc > 2 ) {
		ifs = new ifstream( argv[2] );
		if ( ifs->fail() ) {
			delete ifs;
			ifs = NULL;
		}
	}
	if ( !ifs ) ifs = &cin;
	CommandParser cp( *ifs, api, endFlag );
	cp.readNextCommand();
	if ( ifs && ifs != &cin ) {
		// wait for start signal
		select( 0, NULL, NULL, NULL, NULL );
	}
	while ( !endFlag && cp.execNextCommand() );
	if ( ifs && ifs != &cin ) delete ifs;
}
