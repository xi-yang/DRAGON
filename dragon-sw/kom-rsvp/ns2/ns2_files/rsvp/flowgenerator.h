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
#ifndef flowgenerator_h
#define flowgenerator_h 1

#include "agent.h"
#include "app.h"
#include "../RSVP_config.h"
#include "RSVP_System.h"
#include "tg_classes.h"

#if defined(bind)
#undef bind
#define bind bind
#endif

using TG::DelayCounter;
using TG::RateEstimator;
using TG::Housekeeping;
using TG::SummaryObject;

class UdpSource : public Agent {
	RateEstimator* sendingRateEstimator;

protected:
	virtual int command(int argc, const char*const* argv);
public:
	UdpSource();
	UdpSource(packet_t);
	virtual void sendmsg(int nbytes, const char *flags = 0);
};

class UdpSink : public Agent {
public:
	UdpSink(): Agent(PT_UDP) {};
	virtual void recv( Packet* p, Handler* h );
};

class FlowReceiver : public Application {

	DelayCounter*  delayCounter;
	RateEstimator* receivingRateEstimator;
	RateEstimator* ectRateEstimator;

	bool recvResvSuccess;

	friend class FlowReceiverClass;

public:
	static Housekeeping housekeeping;

protected:
	void storePacket( int nbytes, double delay=0.0, bool ect=false );
	virtual int command(int argc, const char*const* argv);
public:
	FlowReceiver();
	virtual void recv( int nbytes );
	virtual void recv( Packet* p );
	virtual void recvBytes( int nbytes );
};

#endif
