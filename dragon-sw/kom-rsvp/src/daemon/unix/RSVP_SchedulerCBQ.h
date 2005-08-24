/****************************************************************************

  KOM RSVP Engine (release version 3.0f)
  Copyright (C) 1999-2004 Martin Karsten

  Note: All code to interface with Linux traffic control is based on code
  written by Aart van Halteren (a.t.vanhalteren@kpn.com)

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
#ifndef _SchedulerCBQ_h_
#define _SchedulerCBQ_h_ 1

#include "RSVP_Scheduler.h"

#if !defined(ENABLE_CBQ)

#define SchedulerCBQ SchedulerDummy

#else
#if defined(SunOS)
#define NULL_CLASS_HANDLE 0
#include "cbqio.h"
#elif defined(FreeBSD)
#include <net/if.h>
#include <altq/altq_cbq.h>
#else
#define NULL_CLASS_HANDLE 0
#include "LLInfoManager.h"
#endif


class SchedulerCBQ : public BaseScheduler {

#if defined(Linux)
	struct CBQ_Reservation : public Reservation {
		CBQ_Class* cbq_class_;
		CBQ_Reservation(): cbq_class_(new CBQ_Class)  {}
	};

	static RTNetlink::rtnl_handle* tc_handle_;
	static uint32 t2us_;
	static uint32 us2t_;
	static double tick_in_usec_;

	String ifname_;	
	LLInfoManager* ll_info_;

	uint32 root_qdisc_handle_; // the classid of the root CBQ qdisc
	CBQ_Class* root_class_; // all-link class
	CBQ_Class* rsvp_class_; // reserved class. rsvp_class_.classid should be 1:7FFE

	long tc_usec2tick(long usec) {return (long)(usec*tick_in_usec_);};
	long tc_tick2usec(long tick) {return (long)(tick/tick_in_usec_);};
	u_int tc_cbq_calc_offtime(u_int bndw, u_int rate, u_int avpkt,int ewma_log, u_int minburst);
	u_int tc_cbq_calc_maxidle(u_int bndw, u_int rate, u_int avpkt,int ewma_log, u_int maxburst);
	int tc_calc_rtable(u_int bps, uint32* rtab, int cell_log, u_int mtu, u_int mpu);
	int tc_add_class(CBQ_Class* cbq_class, struct rtattr *opt);
	int tc_kill_class(uint32 classid);
	bool add_filter_proto();

	static int dump_qdiscinfo(struct sockaddr_nl *who, struct nlmsghdr *n, void *arg);
	static int dump_classinfo(struct sockaddr_nl *who, struct nlmsghdr *n, void *arg);
	static String print_rate(uint32 rate);
	static String print_tc_classid(__u32 h);

#else

	struct CBQ_Reservation : public Reservation {
		u_long rhandle;
		CBQ_Reservation() : rhandle(NULL_CLASS_HANDLE) {}
	};
	static int cbqfd;
	struct cbq_interface cbqif;
	double nsPerByteTotal;

	u_long cbq_create_class( u_long parent_class, u_long borrow_class,
		u_int pri, u_int is_default, u_int is_ctl, u_int bandwidth, u_int maxdelay,
		u_int maxburst, u_int minburst, u_int av_pkt_size, u_int max_pkt_sz );

#endif

	struct CBQ_Filter : public Filter {
		u_long fhandle;
		CBQ_Filter( const SENDER_Object& sender, const SESSION_Object& session, Reservation& resv, u_long fhandle )
		: 	Filter(sender,session,resv), fhandle(fhandle) {}
	};

	ADSPEC_Object* adspec;
	uint32 flowCounter;
	uint32 flowAdspecCount;
	uint32 MTU;
	u_long root_handle, resv_handle, default_handle, ctl_handle;

	void updateADSPEC();
	bool setFilter( TrafficControl::RHandle* resv, const SESSION_Object&, const SENDER_Object&, CBQ_Filter& );
protected:
	virtual Reservation* internalMakeReservation( const RSB& );
public:
	SchedulerCBQ( ieee32float totalBw, uint32 latency ) : BaseScheduler( totalBw, latency ),
#if defined(Linux)
	ll_info_(NULL), root_class_(NULL), rsvp_class_(NULL),
#endif
	root_handle(NULL_CLASS_HANDLE), resv_handle(NULL_CLASS_HANDLE),
	default_handle(NULL_CLASS_HANDLE), ctl_handle(NULL_CLASS_HANDLE) {}
	virtual ~SchedulerCBQ();
	virtual ADSPEC_Object* init( const LogicalInterface&, uint32& );
	virtual bool modFlowspec( TrafficControl::RHandle*, const FLOWSPEC_Object&,
		uint8 pFlags, const FLOWSPEC_Object*&, const RSB&,
		TrafficControl::UpdateFlag, const FLOWSPEC_Object* replaceFlowspec );
	virtual void delFlowspec( const TrafficControl::RHandle* resv );
	virtual Filter* addFilter( TrafficControl::RHandle* resv, const SESSION_Object&, const SENDER_Object& );
	virtual void delFilter( const TrafficControl::FHandle* filter );
};

#endif

#endif /* _SchedulerCBQ_h_ */
