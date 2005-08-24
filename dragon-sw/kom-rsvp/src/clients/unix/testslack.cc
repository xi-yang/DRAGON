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
	cout << "token bucket: " << tb << endl;
	int numberHops;
	infile >> numberHops;
	cout << "# hops: " << numberHops << endl;
	ErrorTerms *et = new ErrorTerms[numberHops];
	ErrorTerms totET;
	int i;
	for ( i = 0; i < numberHops; i += 1 ) {
		infile >> et[i];
		totET += et[i];
	}
	cout << "total error terms: " << totET << endl;
	ieee32float delay;
	infile >> delay;
	ieee32float R = tb.calculateRate( totET, delay );
	cout << "requested delay " << delay << " microsecs results in rate: " << R << " bytes/sec" << endl;
	infile >> R;
	ieee32float S = delay - tb.calculateDelay( totET, R );
	if ( S < 0 ) {
		cout << "no slack available" << endl;
		return 0;
	}
	cout << "rate " << R << " bytes/sec results in slack: " << S << " microsecs" << endl;
	int slackRouter;
	infile >> slackRouter;
	for ( i = numberHops - 1; i >= 0; i -= 1 ) {
		if ( i == slackRouter ) {
			ErrorTerms dummy(0,0);
			cout << endl;
			ieee32float local_delay = tb.calculateDelay( totET, R );
			cout << "error delay,orig rate: " << TSpec::calculateErrorDelay( totET, R ) << " microsecs" << endl;
			cout << "queuing delay,orig rate: " << tb.calculateDelay( dummy, R ) << " microsecs" << endl;
			cout << "total delay,orig rate : " << local_delay << " microsecs" << endl;
			cout << endl;
			ieee32float newR = tb.calculateRate( totET, local_delay + S );
			cout << "hop " << i << ": rate adjustment to " << newR << " bytes/sec" << endl;
			cout << endl;
			cout << "error delay,new rate: " << TSpec::calculateErrorDelay( totET, newR ) << " microsecs" << endl;
			cout << "queuing delay,new rate: " << tb.calculateDelay( dummy, newR ) << " microsecs" << endl;
			cout << "total delay,new rate : " << tb.calculateDelay( totET, newR ) << " microsecs" << endl;
			cout << "RFC 2212, page 13: " << tb.calculateDelay( totET, newR ) << " <= " << tb.calculateDelay( totET, R ) + S << endl;
			cout << endl;
			ieee32float opt_delay = tb.calculateDelay( et[i], R );
			ieee32float altR = tb.calculateRate( et[i], opt_delay + S );
			cout << "alternative calculation: rate adjustment to " << altR << " bytes/sec" << endl;
			cout << "queuing delay,new rate: " << tb.calculateDelay( dummy, altR ) << " microsecs" << endl;
			cout << "local error delay,new rate: "  << TSpec::calculateErrorDelay( et[i], altR ) << " microsecs" << endl;
			cout << "error delay,old rate: "  << TSpec::calculateErrorDelay( totET - et[i], R ) << " microsecs" << endl;
			cout << "RFC 2212, page 13: " << TSpec::calculateErrorDelay( totET - et[i], R ) + tb.calculateDelay( et[i], altR ) << " <= " << tb.calculateDelay( totET, R ) + S << endl;
			cout << endl;
		}
		totET -= et[i];
	}
	delete [] et;
}
