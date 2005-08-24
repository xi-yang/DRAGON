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
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>

#include <math.h>

#define USECS_PER_SEC		1000000

int main( int argc, char **argv ) {
	if ( argc != 3 ) {
		cerr << "usage: " << argv[0] << " <test-duration> <iterations>" << endl;
		return 1;
	}
	int iterations = atoi(argv[2]);
	int testwait = atoi(argv[1]);
	struct timeval start, end, wait;
	int duration_select, duration_gettimeofday;
	long long sum = 0;
	int i;

	for ( i = 0; i < iterations; i++ ) {
		wait.tv_sec = 0;
		wait.tv_usec = testwait;
		gettimeofday( &start, NULL );
		select( 0, NULL, NULL, NULL, &wait );
		gettimeofday( &end, NULL );
		duration_select = end.tv_usec - start.tv_usec;
		if ( end.tv_sec > start.tv_sec ) {
			duration_select += (end.tv_sec-start.tv_sec)*USECS_PER_SEC;
		}
//		cout << "duration_select: " << duration_select << endl;
		gettimeofday( &start, NULL );
		gettimeofday( &wait, NULL );
		gettimeofday( &end, NULL );
		duration_gettimeofday = end.tv_usec - start.tv_usec;
		if ( end.tv_sec > start.tv_sec ) {
			duration_gettimeofday += (end.tv_sec-start.tv_sec)*USECS_PER_SEC;
		}
		duration_gettimeofday /= 2;
//		cout << "duration_gettimeofday: " << duration_gettimeofday << endl;
		sum += (duration_select - duration_gettimeofday);
	}
	cout << "average duration for select: " << (sum/iterations) << " usec" << endl;

	gettimeofday( &start, NULL );
	for ( i = 0; i < iterations; i++ ) {
		wait.tv_sec = 0;
		wait.tv_usec = testwait;
		select( 0, NULL, NULL, NULL, &wait );
	}
	gettimeofday( &end, NULL );
	duration_select = end.tv_usec - start.tv_usec;
	if ( end.tv_sec > start.tv_sec ) {
		duration_select += (end.tv_sec-start.tv_sec)*USECS_PER_SEC;
	}
	cout << "total duration for " << iterations << " calls to select: " << duration_select << endl;
}
