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
#include "RSVP_BasicTypes.h"

void Buffer::dump( ostream& os, uint16 number ) const {
	const uint16 lineMultiplier = 16;
	uint16 i,j,k;
	os << setbase(16);
	for ( i = 0; i < (number - lineMultiplier); i += lineMultiplier ) {
		for ( j = 0; j < lineMultiplier; j += 2 ) {
			os << setw(2) << setfill('0') << (uint16)(buffer[i+j]);
			os << setw(2) << setfill('0') << (uint16)(buffer[i+j+1]) << " ";
		}
		os << endl;
	}
	for ( k = i; k < number; k += 2 ) {
		os << setw(2) << setfill('0') << (uint16)(buffer[k]);
		os << setw(2) << setfill('0') << (uint16)(buffer[k+1]) << " ";
	}
	os << setbase(10);
}

// this algorithm is taken from the ISI code, because RFC 2205 is rather
// vague about it
uint16 NetworkBuffer::calculateChecksumRSVP( uint16 length ) const {
	uint16* current = (uint16*)checkSumStart;
	uint32 checksum = 0;
	for ( ; length > 1; length -= 2, current += 1 ) {
		checksum += *current;
	}
	if ( length > 0 ) {
		checksum += *(uint8*)current;
	}
	checksum = (checksum & 0xFFFF) + (checksum >> 16);
	checksum = ~(checksum + (checksum >> 16)) & 0xFFFF;
	if (checksum == 0xFFFF) {
		checksum = 0;
	}
	return (uint16)checksum;
}
