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
#ifndef _SchedulerHFSC_h_
#define _SchedulerHFSC_h_ 1

#include "RSVP_Scheduler.h"

#if !defined(ENABLE_ALTQ)

#define SchedulerHFSC SchedulerDummy

#else

#include <net/if.h>
#include <altq/altq_hfsc.h>

extern "C" {
struct segment {
	LIST_ENTRY(segment) _next;
	double  x, y, d, m;
};

typedef LIST_HEAD(gen_sc, segment) gsc_head_t;
}

struct ServiceCurve : public service_curve {
	ServiceCurve() { m1 = 0; d = 0; m2 = 0; }
	ServiceCurve( const service_curve& s ) {
		m1 = s.m1; d = s.d; m2 = s.m2;
	}
	ServiceCurve& operator=( const service_curve& s ) {
		m1 = s.m1; d = s.d; m2 = s.m2; return *this;
	}
};

class SchedulerHFSC : public BaseScheduler {

	struct HFSC_Reservation : public Reservation {
		u_long rhandle;
		ServiceCurve sc;
		HFSC_Reservation() : rhandle(HFSC_NULLCLASS_HANDLE) {}
	};

	struct HFSC_Filter : public Filter {
		u_long fhandle;
		HFSC_Filter( const SENDER_Object& sender, const SESSION_Object& session, Reservation& resv, u_long fhandle )
		: Filter(sender,session,resv), fhandle(fhandle) {}
	};

	static int hfscfd;
	struct hfsc_interface hfscif;
	u_long resv_handle, default_handle, ctl_handle;
	struct service_curve realtimeSC;
	struct gen_sc totalGenSC;

	inline u_long createClass( u_long parent, uint32 m1, uint32 d = 0, uint32 m2 = 0, int qlimit = 50, int flags = 0 );
	inline void SchedulerHFSC::mapFlowspec2sc( const FLOWSPEC_Object& flowspec, service_curve& );

protected:
	virtual Reservation* internalMakeReservation( const RSB& );
public:
	SchedulerHFSC( ieee32float totalBw, uint32 latency ) : BaseScheduler( totalBw, latency ) {}
	virtual ~SchedulerHFSC();
	virtual ADSPEC_Object* init( const LogicalInterface&, uint32& );
	virtual bool modFlowspec( TrafficControl::RHandle*, const FLOWSPEC_Object&,
		uint8 pFlags, const FLOWSPEC_Object*&, const RSB&,
		TrafficControl::UpdateFlag, const FLOWSPEC_Object* replaceFlowspec );
	virtual void delFlowspec( const TrafficControl::RHandle* );
	virtual Filter* addFilter( TrafficControl::RHandle*, const SESSION_Object&, const SENDER_Object& );
	virtual void delFilter( const TrafficControl::FHandle* );
};

#endif /* ENABLE_ALTQ */

#endif /* _SchedulerHFSC_h_ */
