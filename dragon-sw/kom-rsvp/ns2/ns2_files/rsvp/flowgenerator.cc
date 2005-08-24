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
#include "flowgenerator.h"

#include <iostream>
#include <typeinfo>
#include "flags.h"

Housekeeping FlowReceiver::housekeeping;

static class UdpSourceClass : public TclClass {
public:
	UdpSourceClass() : TclClass("Agent/UDPSource") {}
	TclObject* create(int, const char*const*) {
		return (new UdpSource());
	}
} class_udpsource;

UdpSource::UdpSource() : Agent(PT_UDP), sendingRateEstimator(NULL)
{
	bind("packetSize_", &size_);
}

UdpSource::UdpSource(packet_t type) : Agent(type), sendingRateEstimator(NULL)
{
	bind("packetSize_", &size_);
}

void UdpSource::sendmsg(int nbytes, const char* flags)
{
	Packet *p;
	int n;

	if (size_)
		n = nbytes / size_;
	else
		printf("Error: UDP size = 0\n");

	if (nbytes == -1) {
		printf("Error:  sendmsg() for UDP should not be -1\n");
		return;
	}
	while (n-- > 0) {
		p = allocpkt();
//		hdr_cmn::access(p)->timestamp() = Scheduler::instance().clock();
		if (sendingRateEstimator) sendingRateEstimator->storePacket( getCurrentSystemTime(), size_ );
		target_->recv(p);
	}
	n = nbytes % size_;
	if (n > 0) {
		p = allocpkt();
		hdr_cmn::access(p)->size() = n;
//		hdr_cmn::access(p)->timestamp() = Scheduler::instance().clock();
		if (sendingRateEstimator) sendingRateEstimator->storePacket( getCurrentSystemTime(), n );
		target_->recv(p);
	}
	idle();
}

int UdpSource::command( int argc, const char*const* argv ) {
//	Tcl& tcl = Tcl::instance();
	if (argc == 4) {
		if (strcmp(argv[1], "estimator") == 0) {
			SummaryObject* so = FlowReceiver::housekeeping.getSummaryObject( argv[2] );
			if (!so) {
				TimeValue avgInterval;
				avgInterval.getFromFraction( atof(argv[3]) );
				sendingRateEstimator = new RateEstimator( avgInterval, argv[2] );
				FlowReceiver::housekeeping.addSummaryObject( *sendingRateEstimator );
			} else {
				if ( typeid(*so) != typeid(RateEstimator) ) {
					return( TCL_ERROR );
				}
				sendingRateEstimator = reinterpret_cast<RateEstimator*>(so);
			}
			return( TCL_OK );
		}
	}
	return(Agent::command(argc, argv));
}

static class UdpSinkClass : public TclClass {
public:
	UdpSinkClass() : TclClass("Agent/UDPSink") {}
	TclObject* create(int, const char*const*) {
		return (new UdpSink());
	}
} class_udpsink;

void UdpSink::recv(Packet* p, Handler*) {
	if (app_) {
		if ( typeid(*app_) == typeid(FlowReceiver) ) {
			reinterpret_cast<FlowReceiver*>(app_)->recv( p );
			return; //packet freed by FlowReceiver::recv().
		}
		app_->recv( hdr_cmn::access(p)->size() );
	}

	Packet::free(p);
}

static class FlowReceiverClass : public TclClass {
public:
	FlowReceiverClass() : TclClass("Application/FlowReceiver") { }
	TclObject* create(int, const char*const*) {
		return(new FlowReceiver());
	}
	virtual void bind();
	virtual int method(int ac, const char*const* av);
} class_flowreceiver;

void FlowReceiverClass::bind() {
	TclClass::bind();
	add_method("log-stats");
}

int FlowReceiverClass::method(int ac, const char*const* av) {
	int argc = ac - 2;
	const char*const* argv = av + 2;
	if (argc == 2) {
		if (strcmp(argv[1], "log-stats") == 0) {
			FlowReceiver::housekeeping.showStats();
			return TCL_OK;
		}
	}
	return TclClass::method(ac,av);
}

FlowReceiver::FlowReceiver(): delayCounter(NULL), receivingRateEstimator(NULL),
	ectRateEstimator(NULL), recvResvSuccess(false) {}

void FlowReceiver::storePacket( int nbytes, double delay, bool ect ) {
	if (receivingRateEstimator) {
		receivingRateEstimator->storePacket( getCurrentSystemTime(), nbytes );
	}
	if (ectRateEstimator && ect) {
		ectRateEstimator->storePacket( getCurrentSystemTime(), nbytes );
	}
	if (delayCounter) {
		TimeValue d;
		d.getFromFraction( delay );
		delayCounter->storePacket( d, recvResvSuccess );
	}
}
void FlowReceiver::recvBytes( int nbytes ) {
	storePacket( nbytes );
}

void FlowReceiver::recv( int nbytes ) {
	storePacket( nbytes );
}

void FlowReceiver::recv( Packet* p ) {
	double delay = Scheduler::instance().clock() - hdr_cmn::access(p)->timestamp();
	storePacket( hdr_cmn::access(p)->size(), delay, hdr_flags::access(p)->ect() );
	Packet::free(p);
}

int FlowReceiver::command( int argc, const char*const* argv ) {
	if (argc == 2) {
		if (strcmp(argv[1], "upcall-resv_conf") == 0) {
			recvResvSuccess = true;
			return( TCL_OK );
		}
		if (strcmp(argv[1], "upcall-path_tear") == 0) {
			recvResvSuccess = false;
			return( TCL_OK );
		}
	}
	if (argc == 4) {
		if (strcmp(argv[1], "estimator") == 0) {
			SummaryObject* so = housekeeping.getSummaryObject( argv[2] );
			if (!so) {
				TimeValue avgInterval;
				avgInterval.getFromFraction( atof(argv[3]) );
				receivingRateEstimator = new RateEstimator( avgInterval, argv[2] );
				housekeeping.addSummaryObject( *receivingRateEstimator );
			} else {
				if ( typeid(*so) != typeid(RateEstimator) ) {
					return( TCL_ERROR );
				}
				receivingRateEstimator = reinterpret_cast<RateEstimator*>(so);
			}
			return( TCL_OK );
		}
		if (strcmp(argv[1], "ect-estimator") == 0) {
			SummaryObject* so = housekeeping.getSummaryObject( argv[2] );
			if (!so) {
				TimeValue avgInterval;
				avgInterval.getFromFraction( atof(argv[3]) );
				ectRateEstimator = new RateEstimator( avgInterval, argv[2] );
				housekeeping.addSummaryObject( *ectRateEstimator );
			} else {
				if ( typeid(*so) != typeid(RateEstimator) ) {
					return( TCL_ERROR );
				}
				ectRateEstimator = reinterpret_cast<RateEstimator*>(so);
			}
			return( TCL_OK );
		}
	}
	if (argc == 5) {
		if (strcmp(argv[1], "delay-counter") == 0) {
			SummaryObject* so = housekeeping.getSummaryObject( argv[2] );
			if (!so) {
				TimeValue maxDelay;
				maxDelay.getFromFraction( atof(argv[3]) );
				int hashBinCount = atoi(argv[4]);
				delayCounter = new DelayCounter( maxDelay, hashBinCount, argv[2], TimeValue(1,0) );
				housekeeping.addSummaryObject( *delayCounter );
			} else {
				if ( typeid(*so) != typeid(DelayCounter) ) {
					return( TCL_ERROR );
				}
				delayCounter = reinterpret_cast<DelayCounter*>(so);
			}
			return( TCL_OK );
		}
	}
	return(Application::command(argc, argv));
}
