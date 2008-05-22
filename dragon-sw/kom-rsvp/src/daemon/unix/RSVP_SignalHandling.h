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
#ifndef _RSVP_SignalHandling_h_
#define _RSVP_SignalHandling_h_ 1

#include "RSVP_System.h"
#include "RSVP_TimeValue.h"

class SignalHandling {
public:
	typedef void (*SigHandler)( void* );
	static bool userSignal;

private:
	static SigHandler alarmHandler;
	static SigHandler exitHandler;
	static SigHandler userHandler;
	static SigHandler userHandler2;
	static void* clientData;
	static struct sigaction sigAction;
	static void internal_SigHandler(int);

public:
	static void install( SigHandler exitHandler = SigHandler(0),
	SigHandler alarmHandler = SigHandler(0), void* = (void*)0 );
	static void installUserSignal( SigHandler, SigHandler = SigHandler(0) );
	static void wait( bool, TimeValue& );
};

#endif /* _RSVP_SignalHandling_h_ */
