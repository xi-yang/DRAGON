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

#if defined(ENABLE_ALTQ)

#include "RSVP_SchedulerRate.h"
#include "RSVP_LogicalInterface.h"

#include <altq/altq.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

ADSPEC_Object*  SchedulerRate::init( const LogicalInterface& lif, uint32& serviceSupport ) {
	LOG(5)( Log::Scheduler, "Rate: enabling on interface", lif.getName(), "rate:", totalBw, "bit/s" );
	ifname = lif.getName();
	int altq_fd = CHECK( open(ALTQ_DEVICE, O_RDWR) );
	struct tbrreq req;
	strncpy( req.ifname, ifname.chars(), IFNAMSIZ );
	req.tb_prof.rate = (u_int)totalBw;
	req.tb_prof.depth = 1514*10;
	CHECK( ioctl( altq_fd, ALTQTBRSET, &req ) );
	CHECK( close( altq_fd ) );
	return SchedulerDummy::init( lif, serviceSupport );
}

SchedulerRate::~SchedulerRate() {
	int altq_fd = CHECK( open(ALTQ_DEVICE, O_RDWR) );
	struct tbrreq req;
	strncpy( req.ifname, ifname.chars(), IFNAMSIZ );
	req.tb_prof.rate = 0;
	req.tb_prof.depth = 0;
	CHECK( ioctl( altq_fd, ALTQTBRSET, &req ) );
	CHECK( close( altq_fd ) );
}

#endif /* ENABLE_ALTQ */
