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
#include "RSVP_IntServComponents.h"
#include "RSVP_Log.h"

ieee32float TSpec::calculateRate( const ErrorTerms& et, ieee32float_p Qd ) const {
	if ( !checkIntegrity() || !checkDelay( et, Qd ) ) {
		return 0;
	}
	ieee32float_p R;
	if ( p == r ) {
		R = r;
	} else {
		// see: Optimal Network Service Curves under Bandwidth-Delay Decoupling
		// by Jens Schmitt, IEE Proceedings Electronic Letters
		ieee32float_p dp = ieee32float_p(M+et.C)/ieee32float_p(p) + et.D;
		if ( dp > Qd ) {
			R = ieee32float_p(M+et.C)/ieee32float_p(Qd-et.D);
		} else {
			ieee32float_p helper1 = (b-M)/(ieee32float_p(p)-ieee32float_p(r));
			ieee32float_p helper2 = (Qd - et.D) / USECS_PER_SEC;
			R = ( ieee32float_p(p) * helper1 + M + et.C ) / ( helper1 + helper2 );
			if ( R < r ) {
				R = r;
			} else if ( R > p ) {
				R = (M + et.C) / helper2;
			}
		}
	}
	if ( !checkDelay( et, Qd, R ) ) {
		return 0;
	}
	return R;
}

ieee32float TSpec::calculateErrorRate( const ErrorTerms& et, ieee32float_p Qd ) {
	return (et.C * USECS_PER_SEC) / (Qd - et.D);
}

ieee32float TSpec::calculateDelay( const ErrorTerms& et, ieee32float_p R ) const {
	if ( !checkIntegrity() || !checkRate( R ) ) {
		return 0;
	}
	ieee32float_p helpQd = et.D + USECS_PER_SEC * (M + et.C) / R ;
	if ( R < p ) {
		return helpQd + USECS_PER_SEC*(b-M)*(p-R) / (R*(p-r));
	} else {
		return helpQd;
	}
	abort();
}

ieee32float TSpec::calculateErrorDelay( const ErrorTerms& et, ieee32float_p R ) {
	return et.D + USECS_PER_SEC * (et.C) / R ;
}

ieee32float TSpec::calculateBuffer( const ErrorTerms& et, ieee32float_p R ) const {
	if ( !checkIntegrity() || !checkRate( R ) ) {
		return 0;
	}
	ieee32float_p helper2 = et.C / R + et.D / USECS_PER_SEC;
	if ( p <= R ) {
		return helper2 * p + M;
	} else {
		ieee32float_p helper1 = (b-M)/(p-r);
		if ( helper1 < helper2 ) {
			return helper2 * r + b;
		} else {
			return helper1 * (p-R) + M + helper2 * R;
		}
	}
	abort();
}
