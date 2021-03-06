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
#include "RSVP.h"
#include "RSVP_LogicalInterface.h"
#include "RSVP_RoutingService.h"
#include "RSVP_List.h"

int main (int argc, char **argv) {
	if (argc != 2) {
		cerr << "usage: " << argv[0] << " dest-addr [mask]" << endl;
		return 1;
	}
	Log::init( "all", "ref,packet,select" );
	RSVP* rsvp = new RSVP( "" );
#if defined(Linux) && defined(REAL_NETWORK)
	rsvp->getRoutingService().addUnicastRoute( NetAddress("192.168.240.66"), *rsvp->findInterfaceByName( "eth0"), NetAddress(0), 500 );
	char jofel;
	cin >> jofel;
	rsvp->getRoutingService().delUnicastRoute( NetAddress("192.168.240.66"), *rsvp->findInterfaceByName( "eth0") );
#endif
	NetAddress gw;
	const LogicalInterface* lif = rsvp->getRoutingService().getUnicastRoute( NetAddress(argv[1]), gw );
	if (lif) cout << endl << "Outgoing interface:" << endl << *lif << endl << "gateway: " << gw << endl;
	delete rsvp;
	Log::close();
}
