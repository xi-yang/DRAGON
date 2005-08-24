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
#ifndef _Scheduler_h_
#define _Scheduler_h_ 1

#include "RSVP_BasicTypes.h"
#include "RSVP_SortableList.h"
#include "RSVP_Relationships.h"
#include "RSVP_TrafficControl.h"

class SENDER_Object;
class SESSION_Object;
class FLOWSPEC_Object;
class ADSPEC_Object;

class BaseScheduler {
	friend inline ostream& operator<< ( ostream&, const BaseScheduler& );

protected:
	struct Reservation;
	struct Filter;

	typedef SimpleList<Filter*> FList;

	struct Filter : public TrafficControl::FHandle {
		const SENDER_Object& sender;
		const SESSION_Object& session;
		Reservation& resv;
		FList::Iterator iter;
		Filter( const SENDER_Object&, const SESSION_Object&, Reservation& );
		~Filter();
	};

	struct Reservation : public TrafficControl::RHandle {
  	ieee32float bandwidth;                        // in bit/s
  	FList filterList;
		Reservation() : bandwidth(0) {}
	};

	ieee32float totalBw;
	uint32 latency;
	ieee32float availBw;

	virtual Reservation* internalMakeReservation( const RSB& activeRSB ) {
		return new Reservation;
	}
	virtual void Print( ostream& ) const {}
	bool admitSimple( Reservation&, ieee32float, ieee32float );
	void releaseSimple( const Reservation& );

public:
	BaseScheduler( ieee32float totalBw, uint32 latency ) : totalBw(totalBw),
		latency(latency), availBw(0) {
	}
	virtual ~BaseScheduler() {}
	virtual ADSPEC_Object* init( const LogicalInterface&, uint32& serviceSupport ) = 0;

	Reservation* addFlowspec( const FLOWSPEC_Object& effFlowspec,
		uint8 pFlags, const FLOWSPEC_Object*& fwdFlowspec, const RSB& activeRSB,
		const FLOWSPEC_Object* replaceFlowspec ) {
		Reservation* rhandle = internalMakeReservation( activeRSB );
		if ( !modFlowspec( rhandle, effFlowspec, pFlags, fwdFlowspec, activeRSB, TrafficControl::NewRSB, replaceFlowspec ) ) {
			delete rhandle;
			return NULL;
		}
		return rhandle;
	}

	virtual bool modFlowspec( TrafficControl::RHandle*, const FLOWSPEC_Object& effFlowspec,
		uint8 pFlags, const FLOWSPEC_Object*& fwdFlowspec, const RSB& activeRSB,
		TrafficControl::UpdateFlag uflag, const FLOWSPEC_Object* replaceFlowspec ) = 0;

	virtual void delFlowspec( const TrafficControl::RHandle* ) = 0;

	virtual Filter* addFilter( TrafficControl::RHandle*, const SESSION_Object&, const SENDER_Object& ) = 0;
	virtual void delFilter( const TrafficControl::FHandle* ) = 0;

	virtual void prepareRedo( TrafficControl::RHandle* ) {}
	virtual void redoLastReservation( TrafficControl::RHandle* ) { assert(0); }

	virtual uint32 getCurrentLoad() {
		return (uint32) (((totalBw - availBw) / totalBw) * 1000000);
	}
};

class SchedulerDummy : public BaseScheduler {
public:
	SchedulerDummy( ieee32float totalBw, uint32 latency ) : BaseScheduler(totalBw, latency) {}
	virtual ADSPEC_Object* init( const LogicalInterface&, uint32& serviceSupport );

	virtual bool modFlowspec( TrafficControl::RHandle*, const FLOWSPEC_Object& effFlowspec,
		uint8 pFlags, const FLOWSPEC_Object*& fwdFlowspec, const RSB& activeRSB,
		TrafficControl::UpdateFlag uflag, const FLOWSPEC_Object* replaceFlowspec );

	virtual void delFlowspec( const TrafficControl::RHandle* );

	virtual Filter* addFilter( TrafficControl::RHandle*, const SESSION_Object&, const SENDER_Object& );
	virtual void delFilter( const TrafficControl::FHandle* );
};

extern inline ostream& operator<< ( ostream& os, const BaseScheduler& s ) {
	os << "bw: " << s.totalBw << " lat: " << s.latency;
	s.Print(os);
	return os;
}

#endif /* _Scheduler_h_ */
