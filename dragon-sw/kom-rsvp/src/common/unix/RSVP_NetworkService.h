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
#ifndef _RSVP_NetworkService_h_
#define _RSVP_NetworkService_h_ 1

#include "RSVP_BasicTypes.h"
#include "RSVP_TimeValue.h"

class NetworkService {
	static InterfaceHandleMask fdmask;
	static int maxSelectFDs;
	// defined in RSVP_System.cc
	static const int sockbufsize;
	friend class NetworkServiceDaemon;             // access: fdmask, maxSelectFDs
public:
	static InterfaceHandle initInterfaceUDP( uint16& );
	static void initReceiveInterface( InterfaceHandle, bool dedicatedRSVP = false );
	static bool waitForPacket( InterfaceHandle, bool, TimeValue = TimeValue(0,0) );
	static bool receivePacket( InterfaceHandle, INetworkBuffer& );
	static bool sendPacket( InterfaceHandle, const ONetworkBuffer&, const NetAddress&, uint16, bool = false );
	static void shutdownInterface( InterfaceHandle );
	static void joinMCastGroupIP4( InterfaceHandle, const NetAddress& );
	static void leaveMCastGroupIP4( InterfaceHandle, const NetAddress& );
};

#endif /* _RSVP_NetworkService_h_*/
