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
#ifndef rsvp_daemon_agent_h
#define rsvp_daemon_agent_h 1

#include "rsvp-agent.h"
#include "RSVP_System.h"

class RSVP_Daemon_Wrapper;

class RSVP_Daemon_Agent : public RSVP_Agent, public TimerHandler {

	RSVP_Daemon_Wrapper* rsvpDaemonWrapper;

	friend class RSVP_Agent_Class;
	static double defaultRefresh_;
	static double defaultRapidRefresh_;

	void registerLIFs();

protected:
	int command(int argc, const char*const* argv) {
		return (RSVP_Agent::command(argc, argv));
	}
public:
	RSVP_Daemon_Agent();
	virtual ~RSVP_Daemon_Agent();

	virtual void reset();
	virtual void recv(Packet* p, Handler* h);
	virtual void expire( Event* e );

	inline double getRefreshInterval()      { return(defaultRefresh_); }
	inline double getRapidRefreshInterval() { return(defaultRapidRefresh_); }
};

#endif
