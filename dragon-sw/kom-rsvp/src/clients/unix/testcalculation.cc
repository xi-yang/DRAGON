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
#include <iostream>
#include <fstream>

#include "RSVP_IntServComponents.h"

int main( int argc, char** argv ) {
	if ( argc < 2 ) {
		cerr << "usage: " << argv[0] << " datafile" << endl;
		exit(1);
	}
	ifstream infile( argv[1] );
																												assert(bool(infile));
	TSpec tb(0,0,0,0,0);
	infile >> tb;
	cout << tb << endl;
	ErrorTerms et;
	infile >> et;
	cout << et << endl;
	ieee32float delay;
	infile >> delay;
	ieee32float R = tb.calculateRate( et, delay );
	cout << "requested delay " << delay << " microsecs results in rate: " << R << " bytes/sec" << endl;
	cout << "requested delay " << delay << " microsecs results in buffer: " << tb.calculateBuffer( et, R ) << " bytes" << endl;
	infile >> R;
	cout << "rate " << R << " bytes/sec results in delay: " << tb.calculateDelay( et, R ) << " microsecs" << endl;
	cout << "rate " << R << " bytes/sec results in buffer: " << tb.calculateBuffer( et, R ) << " bytes" << endl;
	int hops;
	if (infile >> hops) {
		ErrorTerms singleHop;
		infile >> singleHop;
		int i;
		for ( i = 0; i < hops; i += 1 ) {
			ieee32float loc_delay = TSpec::calculateErrorDelay( singleHop, R );
			cout << "local delay: " << loc_delay << " microsecs" << endl;
			delay -= loc_delay;
			et -= singleHop;
			R = tb.calculateRate( et, delay );
			delay = tb.calculateDelay( et, R );
			cout << "error terms: " << et << " => R = " << R << " bytes/sec <=> res Qd = " << delay << " microsecs" << endl;
			delay = tb.calculateDelay( et, R );
			cout << "check: delay is " << delay << endl;
		}
	}
#if 0
	TSpec tb2(0,0,0,0,0);
	infile >> tb2;
	cout << endl;
	cout << tb2 << endl;
	R = tb2.calculateRate( et, delay );
	cout << "requested delay " << delay << " microsecs results in rate: " << R << " bytes/sec" << endl;
	cout << "requested delay " << delay << " microsecs results in buffer: " << tb2.calculateBuffer( et, R ) << " bytes" << endl;
	tb2 += tb;
	cout << endl;
	cout << tb2 << endl;
	R = tb2.calculateRate( et, delay );
	cout << "requested delay " << delay << " microsecs results in rate: " << R << " bytes/sec" << endl;
	cout << "requested delay " << delay << " microsecs results in buffer: " << tb2.calculateBuffer( et, R ) << " bytes" << endl;
#endif
}
