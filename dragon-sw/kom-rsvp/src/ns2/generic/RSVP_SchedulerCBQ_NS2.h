#ifndef _SchedulerCBQ_NS2_h_
#define _SchedulerCBQ_NS2_h_ 1

#include "RSVP_Scheduler.h"

class SchedulerCBQ_NS2 : public BaseScheduler {

	String linkObj;

	struct CBQ_Reservation : public Reservation {
		String rhandle;
		CBQ_Reservation() {}
	};

	struct CBQ_Filter : public Filter {
		u_long fhandle;
		CBQ_Filter( const SENDER_Object& sender, const SESSION_Object& session, Reservation& resv, u_long fhandle )
		: 	Filter(sender,session,resv), fhandle(fhandle) {}
	};

	ADSPEC_Object* adspec;
	uint32 flowCounter;
	uint32 flowAdspecCount;
	uint32 MTU;

	String root_handle, resv_handle, default_handle, ctl_handle;

	void updateADSPEC();
	bool setFilter( TrafficControl::RHandle* resv, const SESSION_Object&, const SENDER_Object&, CBQ_Filter& );
protected:
	virtual Reservation* internalMakeReservation( const RSB& );
public:
	SchedulerCBQ_NS2( const String& linkObj, ieee32float totalBw, uint32 latency ) : BaseScheduler( totalBw, latency ),
	        linkObj(linkObj) {}
	virtual ~SchedulerCBQ_NS2();
	virtual ADSPEC_Object* init( const LogicalInterface&, uint32& );
	virtual bool modFlowspec( TrafficControl::RHandle*, const FLOWSPEC_Object&,
		uint8 pFlags, const FLOWSPEC_Object*&, const RSB&,
		TrafficControl::UpdateFlag, const FLOWSPEC_Object* replaceFlowspec );
	virtual void delFlowspec( const TrafficControl::RHandle* resv );
	virtual Filter* addFilter( TrafficControl::RHandle* resv, const SESSION_Object&, const SENDER_Object& );
	virtual void delFilter( const TrafficControl::FHandle* filter );
};

#endif /* _SchedulerCBQ_NS2_h_ */
